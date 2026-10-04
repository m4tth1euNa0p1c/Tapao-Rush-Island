# T-131 — Intégration sermon, arrimage et sauvegarde

Session `codex-78`, 2026-10-04. Worktree `.worktrees/T-131`, branche `dev/T-131`.
Ce dossier consigne des preuves et des commandes de reprise, pas un second chantier.

## Périmètre livré

Création depuis `main` à `383d923`, puis fusions séparées, sans conflit :

| Branche | Pointe intégrée | Contenu |
|---|---|---|
| `dev/T-054` | `d1a9035` | Zone, drapeau, sermon, règles et specs |
| `dev/T-055` | `01ecf50` | Arrimage, registre de cargaison, ressources et specs |
| `dev/T-057` | `1a8e1b1` | Sauvegarde transactionnelle, copies de secours, reprise et specs |

Les 27 fichiers de ces branches sont conservés. Aucun Blueprint, DataAsset,
`.umap`, plugin ou `Build.cs` n'a été créé ou modifié. Les branchements de carte,
les entrées/RPC joueur, l'installation des composants sur la barque et les objets,
et le raccord du menu à `SG_Run` restent des travaux d'intégration ultérieurs.
`SG_Campaign` reste le format historique de T-057 ; il ne prétend pas implémenter
la sauvegarde en manches de l'ADR-005. L'ADR-006, acceptée et versionnée pendant
la session, est reprise avec main ; aucun comportement jours/périodes n'est ajouté ici.

## Corrections d'intégration

- `CompleteSermon` appelle désormais `AExpeditionGameState::AddZoneBonus` avant
  d'annoncer `Pacified`. L'événement de réussite observe donc un bonus déjà acquis.
- Le serveur lie la zone à l'expédition active, refuse une autre expédition,
  un identifiant vide, l'absence d'horloge et un départ trop tardif. La phase de
  submersion interrompt le sermon ; un contrôle d'échéance couvre aussi le cas
  où le timer de phase n'a pas encore été exécuté. Un refus du bonus ne pacifie pas.
- Le défaut `UExpeditionRules.InitialFloodDelay` est porté à **540 s**, conformément
  au cahier v1.4 et à l'asset modifié par T-126. Bonus **120 s**, sermon **20 s**,
  montée finale **60 s** conservés. Les tests historiques de calcul gardent un
  préréglage personnalisé explicite à 120 s ; deux nouvelles specs vérifient le Standard.
- Huit nouveaux cas de sermon couvrent le raccord au vrai GameState, le bonus
  unique, les refus et la propagation de phase. Les tests font avancer ensemble
  l'horloge du monde et celle du sermon.
- Les specs Save utilisent un `UGameInstance` comme outer de `USaveSubsystem` :
  le moteur impose `Within=GameInstance`, le package transitoire précédent était invalide.

Le premier build révélait trois prérequis manquants du **main versionné** :
`AIslanderCharacter::SetForceThirdPerson`,
`UIslanderMovementComponent::bExhaustionKillsInWater` et
`EAnchorageRole::Galleon`. Le commit `22f95f4` reprend uniquement les fragments
nécessaires déjà présents dans l'arbre principal non commité (caméra aux rames,
noyade, rôle du mouillage). Les autres modifications joueur, ragdoll et eau ne sont
pas reprises. Le rôle Galleon a ensuite été versionné dans main (`eea515e`) et
intégré par `eb1d46c`, sans conflit. **Aucun original dans main n'a été écrit.**

Auto-relecture : ARC-03 (paramètres de gameplay), ARC-09 (specs ajoutées), MEM-02/03
(référence faible au GameState, délégué retiré en EndPlay), SEC-01 (décision serveur),
ERR-05/09 (aucune réussite sur refus, chemins de refus couverts par le code de test).
Ce contrôle de source n'est ni une revue réseau indépendante ni une preuve d'exécution.

## Preuves et limites

| Contrôle | Résultat |
|---|---|
| Fusions séquentielles | Sans conflit |
| Build jeu initial | Échec : trois prérequis ci-dessus, `01-game-initial-failed.log` |
| Build jeu après raccord | Réussi le 04/10 à 05:39, `02-game.log` (81,23 s) |
| Build jeu final, commit `eb1d46c` | Réussi le 04/10 à 05:49, `03-game-final.log` (55,30 s), après toutes les corrections de tests |
| Build éditeur | Non lancé : éditeur partagé PID 37272 ouvert par une autre session |
| Tests headless | Non exécutés ; aucun ancien rapport réutilisé |
| PIE, build packagé, réseau | Non testés |

Les 77 cas attendus des trois branches se répartissent en **30 Sermon + 24 Cargo
+ 23 Save**. Le décompte statique de tous les `It`/`LatentIt` des fichiers `*Spec.cpp`
du worktree est **364**, à confronter à la liste réellement découverte par Automation.
Un cas écrit et compilé n'est pas un cas exécuté.

`manifest.json` rattache les sources, le binaire jeu et les journaux à leurs SHA-256,
au commit de code et aux trois branches ancêtres. `git diff --check` est passé.
Le commit qui archive ce dossier ne change pas les sources compilées.

L'éditeur PID 37272, démarré à 05:15, n'appartient pas à T-131. Sa fermeture par
sa session propriétaire a été demandée, sans fermeture ni arrêt de PIE par Codex.
Les builds T-142 puis T-129 ont également été observés ; T-131 attend leur fin
avant toute nouvelle compilation. Le verrou utilisé est celui de la racine
principale, propriétaire `codex-78`, jamais un verrou indépendant du worktree.
Dernier constat à 05:51 : après restitution de notre verrou, `monk-island-aa`
a réservé la ressource jusqu'à 06:15 pour build éditeur, headless et relance
(T-142/T-140/T-141). T-131 n'utilise pas cette fenêtre détenue par une autre session.

## Reprise de la validation

Depuis la racine principale, après fermeture par le propriétaire de l'éditeur :

1. Relire `git status`, `python scripts/monke.py task T-131` et le statut du verrou.
   Vérifier l'absence d'UBT, UnrealEditor-Cmd, cook et compilateurs actifs.
2. Annoncer la fenêtre dans T-131 avec `chantier.py`, acquérir le verrou principal
   `editor_lock.py acquire --owner codex-78 --reason "REC T-131" --minutes 30`,
   définir `$env:MONKE_LOCK_OWNER='codex-78'`.
3. Lancer `python scripts/worktree.py build T-131`, puis
   `python scripts/worktree.py build T-131 --editor`, **séquentiellement**.
4. Exécuter les filtres ci-dessous dans le worktree, un processus à la fois.
   Enregistrer avant chaque exécution la date ISO avec fuseau, la commande,
   `git rev-parse HEAD`, l'état Git et le journal. Le worktree étant sans assets,
   utiliser la carte moteur `/Engine/Maps/Entry`, sans importer de contenu.

| Filtre | Minimum attendu |
|---|---:|
| `monke_island.Expedition.Sermon` | 30 |
| `monke_island.Cargo` | 24 |
| `monke_island.Save` | 23 |
| `monke_island.` | 364 |

La ligne headless reprend les arguments de `scripts/ue.ps1 test`, avec la carte
moteur explicite et un dossier de rapport distinct pour chaque filtre :

```powershell
$startT131 = (Get-Date).ToUniversalTime().ToString('o')
$projectT131 = (Resolve-Path '.worktrees/T-131/monke_island/monke_island.uproject').Path
$reportT131 = Join-Path (Split-Path $projectT131) 'Saved/Automation/T131-Sermon'
& 'C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe' `
    $projectT131 '/Engine/Maps/Entry' `
    '-ExecCmds=Automation RunTests monke_island.Expedition.Sermon; Quit' `
    -unattended -nullrhi -nosplash -nosound -NoP4 -stdout -FullStdOutLogOutput `
    "-ReportExportPath=$reportT131"
if ($LASTEXITCODE -ne 0) { throw 'Échec du processus Automation' }
python scripts/monke.py report "$reportT131/index.json" `
    --filter monke_island.Expedition.Sermon --min-tests 30 --since $startT131
if ($LASTEXITCODE -ne 0) { throw 'Rapport non valide pour ce run' }
```

Adapter filtre, dossier et minimum pour les trois autres passages. Archiver les
`index.json` et journaux avant tout nouveau run. Rendre le verrou dans un `finally`.
Cette procédure est préparée, **elle n'a pas été exécutée en headless pendant T-131**.

## Report dans main : arrêt en review

Quatre fichiers du report sont encore modifiés par d'autres sessions dans main :

- `monke_island/Source/monke_island/Player/IslanderCharacter.cpp`
- `monke_island/Source/monke_island/Player/IslanderCharacter.h`
- `monke_island/Source/monke_island/Player/IslanderMovementComponent.cpp`
- `monke_island/Source/monke_island/Player/IslanderMovementComponent.h`

Ne pas appliquer de checkout, reset ou stash sur ces fichiers. Leurs propriétaires
doivent d'abord versionner leur travail. Puis fusionner ce main dans `dev/T-131`,
conserver les comportements joueurs les plus récents, relancer les validations,
et seulement alors exécuter `python scripts/worktree.py merge T-131` depuis main.
Le build et les tests INT du nouvel état principal restent requis après le report.
T-131 reste **review**, jamais `fait` ; aucun report dans main n'a été tenté.
