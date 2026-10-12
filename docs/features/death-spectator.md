# Feature : Mort, cause de la mort et spectateur

- **Statut** : review. Le code est écrit et compilé fichier par fichier (UBT `-SingleFile`, éditeur ouvert ; détail au §8). Le code généré par UHT compile aussi. La spec `monke_island.Death` est écrite et compilée, mais **pas encore exécutée**. **Rien n'a été testé en PIE**, ni en solo ni en hôte + client. `DA_DeathRules` n'existe pas encore : l'action éditeur `deathux` le crée une fois l'éditeur recompilé. En attendant, le jeu utilise les mêmes valeurs par défaut de la classe.
- **Owner** : `ue-ui-developer`
- **Date** : 2026-10-10
- **Cahier des charges** : tâche T-135, issue de l'audit T-117 (rapports 09 et 28). Lots concernés : L3 pour la mort définitive et la fin d'expédition (T-104, ADR-006), L5 pour le HUD et les écrans.
  - §4 : « Les morts deviennent spectateurs, sans influencer l'expédition. »
  - §19 : « Mort : cause courte, liste de coéquipiers observables, retour au hub après résolution ; aucun contrôle de monde par le spectateur. »
  - §21 : `WBP_Spectator`. Il est fait ici en Slate, comme tous les écrans TAPAO.
  - Recette visée : §12 n°4 (mort : spectateur), §12 n°7 (à l'échéance, un joueur à terre meurt) et RUN07 (équipage entier mort).

## 1. Intention
L'expédition dure 10 minutes. Un joueur qui meurt à la première minute ne doit pas être chassé de la partie.

- **Mort d'un joueur** :
  - il voit **« TU ES MORT »** et **ce qui l'a tué**, par exemple « Écrasé par Tupa Nui » ou « Noyé » ;
  - la caméra **suit ensuite un coéquipier vivant** ;
  - le clic gauche et le clic droit changent le coéquipier suivi.
- **Fin de l'expédition par la mort** : rien ne s'affiche tant qu'un coéquipier vit. Quand **toute l'équipe** est morte, c'est en campagne **« JOUR N PERDU »** : la campagne continue, le galion repart vers l'île suivante (T-173, `fin-de-jour-contrats.md`). L'écran **NAUFRAGÉ** ne reste que sur une carte sans campagne.
- **En solo**, mourir fait perdre le jour (T-173 ; avant : NAUFRAGÉ).

## 2. État trouvé (2026-10-10)
| Sujet | Avant T-135 | Preuve |
|---|---|---|
| Équipe entière morte (serveur) | **Déjà fait** par T-104 et T-142 :<br>- recompte à chaque mort et à chaque départ d'un joueur ;<br>- un joueur encore en chargement compte comme vivant ;<br>- spec `IsWipe` existante. | `AExpeditionGameState::HandlePlayerDeath`, `HandleLogout` et `CheckTeamWiped` (`ExpeditionGameState.cpp`, l. 372–462) ; `ExpeditionClockSpec.cpp`, l. 210–216 |
| Écran NAUFRAGÉ | S'affichait 1,5 s après la mort du **joueur local**, même si des coéquipiers vivaient. Il apparaissait aussi pour un mort dont l'équipage avait survécu au jour. | `UTapaoUISubsystem::PollGame` (ancienne version) |
| Cause de la mort | Absente. Chute et noyade passaient `nullptr`, et seul l'acteur tueur était répliqué. Les causes de T-129 (`EDeathCause`, sur la branche `dev/T-129`) ne sont **pas fusionnées**. | `IslanderHealthComponent.cpp` ; `git merge-base` |
| Spectateur | Absent. La caméra restait sur le cadavre, entrées coupées. | `AIslanderCharacter::MulticastDeathRagdoll` |

## 3. Règles
1. **Mort** (serveur, `UIslanderHealthComponent`) :
   - le coup fatal est enregistré dans `FDeathRecord`, répliqué avec la mort ;
   - le record contient :
     - la cause donnée par l'appelant : chute, noyade (`UIslanderMovementComponent`) ou déluge (`AExpeditionGameState`, fin de submersion) ;
     - sinon `Player` si le coup vient d'un joueur ;
     - la classe du pion derrière le coup : l'instigateur, le causeur lui-même, ou le pion qui a tiré ou tient le causeur ;
     - la classe du causeur et le type de dégâts ;
     - le `PlayerState` du joueur qui a frappé.
   - On garde les **classes** et non les acteurs : une noix de coco ou un projectile peut avoir disparu avant que le client lise la mort.
2. **Texte de la cause** (`UDeathRules::DescribeDeath`, fonction pure), dans cet ordre :
   1. Coup d'un joueur : « Tué par {nom} », ou « Victime de sa propre maladresse » si c'est le mort lui-même.
   2. Texte de la cause : chute, noyade, déluge, noix de coco, guerrier, requin.
   3. Texte de la classe du causeur (une arme ou un projectile est plus précis), puis de la classe du tueur.
      - L'entrée retenue est la classe parente **la plus proche**, donc un Blueprint passe avant sa classe native.
      - À distance égale, une entrée liée au type de dégâts passe avant une entrée générale.
   4. Sinon : « Mort dans des circonstances obscures ».
3. **Écran du joueur local** (`UDeathSpectatorComponent::DecideScreen`, fonction pure) :
   - vivant : rien ;
   - **équipe entière morte**, selon le serveur (`IsTeamWiped`) : l'écran de mort s'efface (`EDeathScreen::Wreck`) ; en campagne « JOUR N PERDU » prend l'écran (`UTapaoDayUISubsystem`, T-173), sinon **NAUFRAGÉ** après `GameOverDelay` (1,5 s) ;
   - jour terminé par d'autres (survivants partis) : rien, la scène de départ et le bilan du jour prennent l'écran ;
   - sinon, si un coéquipier est suivi : barre de spectateur ;
   - sinon : « TU ES MORT » et la cause.
4. **Spectateur** (serveur, `UDeathSpectatorComponent` sur le PlayerController) :
   - au bout de `DeathScreenSeconds`, le serveur dirige la vue du mort vers le pion d'un coéquipier vivant, par ordre de `PlayerId` (`SetViewTarget`, que le moteur transmet au client) ;
   - clic gauche : équipier précédent ; clic droit : suivant (`ServerCycle`) ;
   - si le coéquipier suivi meurt, la caméra reste sur lui `FollowedDeathSeconds`, puis passe au suivant ;
   - s'il quitte la partie, elle passe au suivant tout de suite ;
   - s'il ne reste personne de vivant, la vue reste où elle est : c'est NAUFRAGÉ.
   - **Aucun contrôle du monde** : seule la vue change.
   - Rien n'est suivi :
     - sans expédition (cartes de test) ;
     - sans mort définitive (`bPermadeath`) : le joueur réapparaît ;
     - une fois l'expédition terminée : scène de départ ou NAUFRAGÉ.
5. **Caméra de suivi** (côté client, `Amonke_islandCameraManager::UpdateViewTarget` → `ApplyFollowView`) :
   - derrière le coéquipier, légèrement au-dessus, dans la direction où il regarde (sa rotation de vue, que le moteur réplique au spectateur), avec une rotation lissée ;
   - une sonde la tient hors des murs et des rochers ;
   - **jamais d'écran noir** : avant le suivi, la vue reste sur son propre corps.

## 4. Valeurs (`DA_DeathRules`, `/Game/MonkeIsland/UI/Tapao`, `UDeathRules`)
Le dossier est cuisiné avec les écrans TAPAO. Sans l'asset, ce sont les valeurs par défaut de la classe.

| Paramètre | Défaut | Rôle |
|---|---|---|
| `DeathScreenSeconds` | 4 s | durée de « TU ES MORT » avant le suivi d'un coéquipier |
| `SkipDeathScreenAfter` | 1,5 s | un clic ne peut sauter l'écran de mort qu'après ce délai, pour que la cause reste lisible |
| `FollowedDeathSeconds` | 2,5 s | temps passé sur un coéquipier suivi qui vient de mourir |
| `MinCycleInterval` | 0,25 s | limite de débit des changements demandés, vérifiée par le serveur |
| `RefreshSeconds` | 0,5 s | période du minuteur serveur, actif seulement tant que le joueur est mort |
| `FollowDistance` / `FollowHeight` / `FollowPitch` | 380 cm / 60 cm / −15° | position de la caméra de suivi |
| `FollowProbeRadius` / `FollowRotationSpeed` / `FollowFieldOfView` | 12 cm / 6 / 85° | sonde anti-murs, lissage, champ |
| `CauseTexts` | chute, noyade, déluge, noix de coco, guerrier, requin | textes par cause |
| `SourceTexts` | Tupa Nui (`GiantCrab`), requin, guerriers de l'île (`TikiCreature`, `TikiWarrior`, Guerrier Coco, Gardien Crabe, Veilleur des Cendres), soldats français (`FortSoldier` et les 7 uniformes `BP_BeanFR_*` : fusilier, grenadier, sergent, officier de marine, chasseur), noix de coco, douves (`FortMoat`), déluge | textes par classe, en chemins souples (rien n'est chargé pour eux) |
| `PlayerKillText` / `TeammateName` / `SelfKillText` / `UnknownText` | « Tué par {Killer} » / « un équipier » / « Victime de sa propre maladresse » / « Mort dans des circonstances obscures » | coups de joueurs, cas inconnu |

`GameOverDelay` (1,5 s) reste le réglage existant de `UTapaoUISubsystem` (`DefaultGame.ini`).

Les textes sont en `LOCTEXT` / `FText` de DataAsset, donc localisables. Le lexique public évite « tiki » (T-139, décision du propriétaire en attente) : on écrit « guerrier de l'île » et les noms des personnages.

## 5. Écrans (Slate TAPAO)
- **`STapaoDeath`** (`UI/Tapao`), géré par `UTapaoDeathUISubsystem`, à l'ordre Z 600. Il ne prend aucune entrée.
  - **Phase « mort »** :
    - voile noir à 40 % : l'île reste visible ;
    - « TU ES MORT » en titre, puis la cause ;
    - si un coéquipier vit : « Clic gauche ou clic droit : suivre un équipier » et « En vie : A · B ».
  - **Phase « spectateur »** : une barre en bas, au-dessus de la barre rapide, des munitions et de la barre de boss :
    - « Tu suis **Nom** » (le nom est écrit, pas seulement coloré) ;
    - les clics ;
    - « Tu es mort : cause » ;
    - « En vie : … · retour à bord au prochain jour ».
  - Textes de 20 px et plus à 1080p, crème sur fond sombre.
- **Clics** : un processeur d'entrées Slate ne consomme les clics gauche et droit (et ← →, LB et RB) que dans ces conditions :
  - l'écran de mort est affiché ;
  - aucun menu n'est ouvert (pause, question, paramètres, inventaire) ;
  - la fenêtre du jeu a le focus, ce qui compte avec plusieurs fenêtres PIE.
- **NAUFRAGÉ** (`UTapaoUISubsystem`) : seulement si l'équipe entière est morte, et depuis T-173 seulement sur une carte sans campagne (en campagne : « JOUR N PERDU », voir `fin-de-jour-contrats.md`). L'écran porte en plus la ligne « Tu es mort : cause ». « RECOMMENCER » reste réservé à l'hôte, et l'écran n'apparaît plus tant qu'un coéquipier vit.

## 6. Réseau
- **Serveur autoritaire** :
  - la mort et sa cause sont enregistrées par le serveur ;
  - l'équipe entière morte est décidée par le serveur (T-104) ;
  - le coéquipier suivi est choisi par le serveur.
  - Le client ne fait qu'afficher et demander.
- **Réplication** :
  - `FDeathRecord` sur le composant de santé, répliqué à tous ;
  - `FollowedPlayer` et `ObservablePlayers` (au plus 3 entrées) en `COND_OwnerOnly` sur le composant du PlayerController.
- **`ServerCycle(int8)`** :
  - Reliable, `WithValidation` : une direction autre que −1 ou 1 est impossible pour notre client ;
  - refusé si le joueur est vivant, avant `SkipDeathScreenAfter`, plus vite que `MinCycleInterval`, ou une fois l'expédition finie (SEC-02, SEC-03).
- **Pertinence** :
  - la vue serveur du mort est le pion suivi : il reste pertinent pour ce client même au-delà de 150 m ;
  - `Amonke_islandPlayerController::GetPlayerViewPoint` renvoie, côté serveur, les yeux du coéquipier suivi : ce qui l'entoure (mobs, objets) reste répliqué au mort, qui n'envoie plus de position de caméra ;
  - si le client ne connaît pas encore le pion, le moteur redemande la cible de vue (`ServerVerifyViewTarget`) jusqu'à ce qu'il le connaisse.

## 7. Critères d'acceptation (recette proposée)
- [ ] Solo :
  - mort par chute, noyade, `monke.EndDeluge` hors du galion, ou `monke.Death.Kill 0 Flood` dans la console ;
  - en campagne (T-173) : « JOUR N PERDU » au centre, « Jour suivant dans N s », JOUR SUIVANT actif ; sur une carte sans campagne : NAUFRAGÉ 1,5 s après, avec « Tu es mort : … », et « RECOMMENCER » actif.
- [ ] Carte de test sans expédition (`L_Test_Flat`) : « TU ES MORT » et la cause jusqu'à la réapparition, sans NAUFRAGÉ.
- [ ] Hôte + client, le client meurt :
  - le client voit « TU ES MORT » et la cause, pas NAUFRAGÉ ;
  - au bout de 4 s, la caméra du client passe derrière l'hôte, avec la barre « Tu suis … » ;
  - l'hôte continue sans changement ;
  - le journal serveur montre `Death: … follows …`.
- [ ] Hôte + client, l'hôte meurt ensuite : « JOUR N PERDU » sur les deux machines (T-173 ; NAUFRAGÉ sur une carte sans campagne, chacun avec sa propre cause) ; JOUR SUIVANT (ou RECOMMENCER) seulement sur l'hôte.
- [ ] Hôte mort le premier : il suit le client ; le client s'éloigne de plus de 150 m et les mobs autour de lui restent visibles pour l'hôte mort. Sur un listen server, tout est local à l'hôte : refaire avec le client mort qui suit l'hôte au loin pour vérifier la pertinence.
- [ ] Le jour finit avec des survivants (galion) : le mort voit la scène de départ, ni NAUFRAGÉ ni écran de mort.
- [ ] Trois joueurs ou plus :
  - le clic gauche et le clic droit changent le coéquipier suivi (et pas plus vite que 0,25 s) ;
  - si le suivi meurt, la caméra reste 2,5 s sur lui, puis passe au suivant.
- [ ] Aucun écran noir à aucun moment. La pause (Échap) garde ses clics pendant le spectateur.
- [ ] Spec `monke_island.Death` verte : écran (NAUFRAGÉ seulement à l'équipe entière morte), choix du suivi, record, causes, caméra.

## 8. Preuves et statut honnête
- **Compilé** (2026-10-10, éditeur ouvert, UBT `-SingleFile`, en série) :
  - fichiers, chacun seul, sans erreur :
    - `DeathRules.cpp`, `DeathSpectatorComponent.cpp`, `DeathSpec.cpp` ;
    - `IslanderHealthComponent.cpp`, `IslanderMovementComponent.cpp` ;
    - `ExpeditionGameState.cpp` ;
    - `monke_islandPlayerController.cpp`, `monke_islandCameraManager.cpp` ;
    - `STapaoDeath.cpp`, `TapaoDeathUISubsystem.cpp`, `TapaoUISubsystem.cpp` ;
    - `FortMoat.cpp`, `ExpeditionAudioSubsystem.cpp` (appelants de la santé) ;
  - le code généré par UHT des en-têtes nouveaux ou modifiés a été compilé avec `cl.exe` et les options du module, dans un dossier temporaire : 0 erreur, 0 avertissement.
- **Non fait** :
  - build éditeur complet (verrou du lead) ;
  - exécution de la spec ;
  - PIE solo ;
  - PIE hôte + client ;
  - build packagé ;
  - deux machines.
- **Action éditeur `deathux`** (`scripts/editor/death_ux_setup_ue.py`), **à lancer après le build** :
  - elle crée `DA_DeathRules` ;
  - elle vérifie que `BP_FirstPersonPlayerController` a bien le composant `DeathSpectator` ;
  - résultat dans `Saved/T135/death-ux-setup.json`.
  - Elle n'a pas été lancée : sur l'éditeur actuel, les classes n'existent pas encore.

## 9. Limites connues et suite
- **T-129** (branche `dev/T-129`, non fusionnée) définit aussi `Core/DeathCause.h` et une autre signature de `ApplyHealthDamage`.
  - L'enum de T-135 reprend ses six premières valeurs, dans le même ordre, et ajoute `Flood` et `Player`.
  - À la fusion : garder la version de T-135 et y ajouter `UMonkeDamageType` (T-129), dont les types de dégâts (tiki, requin) rempliront `Cause`.
- **Rôle du mort** (audit 28) : rappel sur le pont, fantôme… à trancher par ADR. Ici, le mort ne fait que regarder.
- **Corps disparu** : un pion mort détruit (chute hors du monde) n'est compté ni mort ni vivant par T-104. C'est un écart existant, hors de cette tâche.
- **Audio du mort** : il entend autour du coéquipier suivi, sans filtre « sourd ».
- **`Hazards/FallingCoconutActor.cpp`** ne compile pas seul : il manque `Engine/World.h`. C'est un défaut existant, hors périmètre ; il compile dans le build normal.
