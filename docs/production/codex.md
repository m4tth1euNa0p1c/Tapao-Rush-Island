# Codex dans monke_island

Analyse et outillage T-121, 2026-10-04. Ce guide ne remplace ni le chantier ni le
cahier des charges. Les nombres ci-dessous sont un instantané, pas une mesure
automatiquement tenue à jour.

## Ce qui est déjà là

Crownfall Expeditions est un prototype coopératif d'extraction maritime sous
Unreal Engine 5.8. Le module `monke_island` contient 144 `.cpp` et 158 `.h`,
organisés par domaines. Les Blueprints/DataAssets complètent ce code dans
`Content/MonkeIsland`. Les scripts Python couvrent cartographie Raraka, fragments,
végétation, modèles et imports ; PowerShell pilote UBT, Automation et packaging.

L'organisation Claude existe déjà : 29 rôles, 35 skills, un chantier et 18
worktrees secondaires. Avant T-121 : 120 tâches, 22 `fait`, 75 `review`,
18 `a-faire`, 5 `en-cours`. L'arbre partagé contenait 234 entrées Git modifiées et
34 non suivies (certains répertoires sont regroupés). Ces chiffres ne mesurent pas
le taux de livraison : `review` contient beaucoup de recettes encore à réaliser.

L'audit `docs/audit/2026-10-04-commercial/` comporte 59 rapports numérotés sur les
100 angles annoncés. Il apporte des pistes à vérifier ; plusieurs constats ont
déjà évolué. Exemple : un rapport Automation de 309 tests et une configuration
de packaging sont maintenant présents. T-120 porte déjà le build investisseurs :
sa dernière note le place en `review`, avec packaging Development et lancement
déclarés réussis ; contrôle visuel et recette complète restent ouverts. Ces preuves
de T-120 n'ont pas été reproduites par T-121.

## Travail prioritaire, vérifié dans le code

1. **Raccorder la partie en manches — T-111 à T-114.**
   `Galleon/GalleonSpawner.cpp::TryAnchor` attend la barque puis replace les joueurs.
   `Expedition/Core/ExpeditionGameState.cpp::BeginPlay` démarre encore l'expédition.
   `Run/` contient règles, types, calculateur et tests ; les classes d'orchestration
   `URunSubsystem`, `URunStateComponent`, `ARunGameMode` et la barre/vote prévus par
   ADR-005 ne sont pas présentes dans les sources examinées. Les calculs de vente
   et résolution existent, mais les appels trouvés sont dans les tests.
2. **Sauvegarder une vraie partie — T-115.**
   `UI/Tapao/TapaoSaveGame.h` conserve surtout des compteurs, un fragment et l'or.
   `UTapaoUISubsystem::OnContinue` appelle `TravelToPlay`. Le manifeste de cale,
   `RunId`/`RoundId` et le marqueur `ExpeditionInProgress` restent à intégrer.
   Traiter aussi sauvegarde illisible, version et reprise sans perte de valeur.
3. **Prouver le parcours complet — T-116/T-120.**
   Recette RUN01–RUN14, puis hôte/client et build hors éditeur : départ, retour,
   vente, mort d'équipe, manche suivante et reprise. Les specs de calcul ne
   vérifient ni voyages ni physique réseau. Coordonner T-120 au lieu de lancer
   un second packaging.

Ces constats portent sur le texte source. Aucun asset binaire n'a été inspecté
dans l'éditeur pendant T-121 ; aucun build ni test Unreal n'a été relancé.

## Outils disponibles

Python 3.11 minimum, sans dépendance tierce pour `scripts/monke.py`.
Toutes ses commandes sont en lecture seule. `doctor` effectue uniquement un test
TCP local du port 8000, sans appel MCP. Les sorties `--json` sont destinées aux agents.

| Commande depuis la racine | Utilité |
|---|---|
| `python scripts/monke.py status` | Chantier, changements Git résumés, code, worktrees et agents |
| `python scripts/monke.py task T-113` | Tâche complète, références T-NNN et tâches de même feature |
| `python scripts/monke.py doctor` | Fichiers requis, outils, moteur, port Unreal et validation TOML des agents |
| `python scripts/monke.py report` | Contrôle du rapport Automation existant, défaut suite `monke_island` |
| `python scripts/monke.py report --filter monke_island.Run --min-tests 9` | Contrôle d'une suite précise et d'un effectif minimum attendu |
| `python scripts/monke.py --root .worktrees/T-113 status --json` | Diagnostic d'un autre arbre (s'il existe) |
| `python -m unittest discover -s scripts/tests -p "test_monke.py" -v` | Tests de l'outillage, fichiers temporaires seulement |

`report` accepte aussi un chemin `index.json`, `--max-age-hours` (défaut 24 h)
et `--since` (ISO 8601 avec fuseau). Il refuse un rapport absent/malformé, zéro
test sélectionné, une suite trop courte, un échec, un test inachevé/inconnu,
des erreurs dans les entrées, des événements illisibles, des doublons ou des
compteurs incohérents avec les états individuels (y compris hors du filtre).
Les avertissements restent visibles mais ne font pas échouer une réussite.
Les compteurs globaux sont contrôlés ; un échec dans une autre suite ne fait
pas échouer la suite expressément sélectionnée.

Codes de sortie : **0** résultat exploitable, **1** contrôle échoué, **2** entrée
invalide ou donnée indisponible. `doctor` distingue erreurs bloquantes et warnings
(éditeur arrêté, moteur absent) : lire ces derniers avant un build.
`--engine` ou `UE_ROOT` change le moteur inspecté par `doctor` seulement ; les
anciens scripts de compilation gardent leur propre chemin moteur.

Pour associer le rapport à une nouvelle exécution autorisée :

```powershell
$testStartedAt = (Get-Date).ToUniversalTime().ToString('o')
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 test -Filter monke_island.Run
if ($LASTEXITCODE -ne 0) { throw 'Le processus Automation a échoué' }
python scripts/monke.py report --filter monke_island.Run --min-tests 9 --since $testStartedAt
if ($LASTEXITCODE -ne 0) { throw 'Le rapport Automation ne valide pas cette exécution' }
```

Le contrôle d'âge porte sur la date du fichier ; copier un vieux rapport peut la
changer. Conserver aussi la commande, le commit et l'état Git, les logs et une
copie du rapport dans les preuves de la tâche. Ce script n'atteste pas à lui seul
la version des binaires ni l'exhaustivité de la suite. Le minimum doit être ajusté
au périmètre réellement demandé, pas utilisé comme un total immuable.

## Agents Codex

Six profils `.codex/agents/*.toml` ; ils héritent du modèle et des droits de la
session. Le champ `developer_instructions` définit leur mission. Aucun agent
ne tourne en permanence. Le parent fournit tâche, worktree, fichiers autorisés,
critères et propriétaire des ressources ; il sérialise les mises à jour du board.

| Profil Codex | Mission | Rôle à utiliser dans chantier.py |
|---|---|---|
| `monke-producer` | Analyse, dépendances, preuves, découpage | `ue-producer` ou `lead` |
| `monke-gameplay` | C++, expédition, barque, galion, économie | `ue-gameplay-programmer` / `ue-systems-programmer` |
| `monke-network-reviewer` | Revue RPC, autorité, réplication, voyages | `ue-network-engineer` |
| `monke-world` | Fragments, imports, assets, intégration | `ue-editor-integrator` |
| `monke-qa` | Rapports, recette solo/réseau/build | `ue-qa-tester` |
| `monke-toolsmith` | Scripts et diagnostics | `ue-build-engineer` |

Exemple : « Utilise monke-producer pour préparer T-113, puis monke-network-reviewer
pour relire le contrat de voyage. Lecture seule, pas de build. » Si la session ne
propose pas les noms personnalisés, le parent lit leur TOML et transmet les
instructions à l'outil de délégation disponible. Leur chargement natif n'est pas
garanti dans une session déjà ouverte ; rouvrir le projet si nécessaire.

Le skill personnel `$monke-island` est installé dans
`C:/Users/matth/.codex/skills/monke-island/SKILL.md`. Il renvoie à ce guide et au
`AGENTS.md` du dépôt/worktree. Il n'est pas versionné dans ce dépôt.

Les nouveaux worktrees légers incluront `.codex/` et `AGENTS.md` une fois ces
fichiers versionnés. Les worktrees déjà ouverts ne sont pas modifiés : leurs
règles sparse et leur révision restent inchangées. Ne pas y copier aveuglément
des fichiers non commités de l'arbre principal.

Le MCP Unreal est configuré dans `.codex/config.toml`, serveur optionnel à
`http://127.0.0.1:8000/mcp`. L'éditeur doit déjà être disponible et son propriétaire
coordonné. Blender reste sur le second PC ; aucune connexion Blender automatique
n'est ajoutée ici. La configuration ne remplace pas la découverte des vrais outils.

Format vérifié dans la [documentation officielle des sous-agents Codex](https://learn.chatgpt.com/docs/agent-configuration/subagents)
et la [configuration MCP officielle](https://learn.chatgpt.com/docs/extend/mcp?surface=cli).

## Vérifications de T-121

- 14 tests Python de l'outillage réussis (rapports invalides/incomplets/anciens,
  filtres, compteurs, chemins Git, lecture du chantier sans écriture). Relecture
  indépendante : deux faux positifs corrigés et couverts (résumé contradictoire
  avec filtre, événements malformés). Les reproductions initiales ont été revérifiées
  par le relecteur après correction : rejet confirmé, codes 1 et 2 respectivement.
- Six profils TOML valides ; skill validé par `quick_validate.py`.
- Configuration Unreal reconnue par `codex mcp get unreal --json` ; la connexion
  au serveur reste distincte de ce contrôle de configuration.
- Rapport Unreal existant du 2026-10-04 à 01:43:35 UTC lu : 306 réussites sans
  avertissement + 3 avec avertissements, 0 échec, 0 non exécuté, 0 en cours.
  Les trois warnings appartiennent à Economy. Ce résultat n'est pas une relance.
- Port Unreal 8000 fermé au diagnostic initial. Aucun import, lancement/arrêt
  d'éditeur, compilation Unreal, packaging ou commit réalisé par cette tâche.
