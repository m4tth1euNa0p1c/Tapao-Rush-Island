# Feature : Exécution de l'équipage (quota manqué au jour jalon)

**Lot** : L4 (boucle de campagne) · **Statut** : en place, testé éditeur (PIE) · **Code** : `Source/monke_island/Execution/`, `Run/RunContract`, `Expedition/Core/ExpeditionGameState`, `Expedition/Core/DayTransitionSubsystem` · **Données** : `/Game/MonkeIsland/Galleon/Execution/DA_ExecutionRules`, `DA_RunRules.bExecuteOnFailedContract`

## Demande (mots du propriétaire, 2026-10-07)
> « et si quota pas atteint au jours jalon cls alors animation des beans mit sur une planche et soldat francais les fusil »

Le jour jalon est le dernier jour d'un contrat : le quota y est vérifié (jours 3, 6 et 10 de la campagne de dix jours, paliers de `DA_RunRules`).

## Règle
- Au dernier jour d'un contrat, le quota peut ne pas être atteint. Si la règle `DA_RunRules.bExecuteOnFailedContract` est active (par défaut), le calculateur rend le verdict `ERunDayVerdict::Executed` et la partie passe en `ERunOutcome::Defeat`.
- Il n'y a ni perte de trésor ni retour d'une zone : la campagne est perdue.
- Si la règle est inactive, on retrouve l'ancien comportement (`ContractFailed` : la moitié du trésor perdue, une zone en arrière).
- Une équipe entièrement morte ce jour-là donne aussi `Executed`. La partie est perdue, mais il n'y a pas de scène, faute de survivants. Depuis T-173 il n'y a pas non plus de voyage : « JOUR N PERDU » au centre, puis l'écran de fin « CONTRAT ÉCHOUÉ » (« L'équipage est déclaré traître, à titre posthume. ») `LostCampaignScreenSeconds` (8 s) après (`fin-de-jour-contrats.md`).
- Au jour suivant, `URunSubsystem::BeginDay` voit `Defeat` et lance une nouvelle partie. C'est ce que font RECOMMENCER et JOUER.

## Déroulé (serveur autoritaire, chaque machine voit la scène)
| Temps depuis la fin du jour | Ce qui se passe | Valeur (`DA_ExecutionRules`) |
|---|---|---|
| 0 → 3,5 s | Vue du joueur sur le pont, titre « Quota non atteint » au centre. Pas de bilan du jour par-dessus (T-173) : l'avis du Bureau vient sur l'écran de fin. | `TitleSeconds` |
| 3,2 → 4 s | Fondu au noir | `FadeSeconds` |
| 4 s | Serveur : les joueurs vivants sont mis sur la planche, le dos au peloton et face à la mer. Quatre soldats français arrivent sur le pont, en ligne, face à la planche. Client : la vue passe à la caméra de la scène, en fondu depuis le noir. Le roulement de tambour commence. | `PlaceAtSeconds` |
| 7 s | Le peloton épaule | `AimAtSeconds` |
| 10 s (+0 à 0,3 s par soldat) | Salve. Chaque balle tue. Les corps tombent de la planche vers la mer (ragdoll lancé). Le tambour s'arrête. | `FireAtSeconds`, `VolleySpreadSeconds`, `PushSpeed`, `PushUpSpeed` |
| 11,8 → 15,5 s | Titre « La campagne s'achève ici » | — |
| 15,5 s | Noir, puis écran de fin, ruban « CONTRAT ÉCHOUÉ » (T-173 ; avant : « EXÉCUTÉS ») : « Avis de déchéance : zone », « x / y couronnes : il manque z couronnes. », « L'équipage est déclaré traître et remis au peloton français. », « La campagne s'achève au jour N. », les statistiques, RECOMMENCER (hôte) et MENU PRINCIPAL | `EndAtSeconds` |

Il n'y a pas de voyage vers l'île suivante. Le HUD du joueur est masqué pendant toute la scène (`UDayTransitionSubsystem::IsScenePlaying`).

## Mise en scène (repère de la coque du galion, cm : X en travers, Y le long, Z depuis la flottaison)
Les relevés du pont ont été faits en PIE le 2026-10-07 :
- le pont principal est à 162 cm, le haut du bastingage à 240 cm, le flanc à 330 cm ;
- le grand mât est vers (0, 175), avec sa voile vers y = 200 ;
- l'étal du marchand occupe x 76 → 252, y −215 → −41 ;
- l'escalier du gaillard d'avant occupe x ±150 → 225, y 450 → 550.

La bande y ≈ 330 est dégagée sur toute la largeur. La scène y est placée :
- **planche** : de (240, 330, 248), posée sur le bastingage, sur 480 × 45 × 8 cm vers +X, au-dessus de la mer. C'est le cube de 1 m du moteur étiré, avec le matériau `MI_Camp_Planks`. Les condamnés se placent à 260 cm de son départ puis tous les 80 cm ;
- **peloton** : `BP_BeanFR_Grenadier`, `Fusilier_Bicorne`, `Fusilier_1767`, `Sergent`, en ligne à x = −110, centrés sur y = 310 et espacés de 70 cm. Leurs lignes de tir passent à côté du mât ;
- **caméra** : au large, devant les condamnés, glisse de (1300, 170, 450) à (1170, 190, 420) en visant (180, 320, 255), avec un champ de 48°. On voit le bean sur la planche de face, la mer derrière, et le peloton derrière lui au-dessus du bastingage.

Les soldats du peloton sont des `AFortSoldier` en mode peloton (`StartFiringSquad`) :
- ils n'ont pas de garnison, ne perçoivent rien et ne décident rien ;
- ils épaulent et tirent aux heures données ;
- leur balle frappe toujours (`DA_FortSoldierRules.SquadDamage`) et donne la vitesse de chute au ragdoll du joueur.

## Outils
- `monke.Execution.Start` (hors Shipping, serveur) : fin du jour immédiate avec l'exécution, quel que soit l'état de la partie.
- Pour le chemin réel : `monke.Run.Day 3` sans rien rapporter, puis `monke.EndDeluge` à bord.
- Captures : `scripts/editor/garrison_pie_check.py`, étapes `cmd monke.Execution.Start; burst N s` et `execprobe` (relevé du pont).
- Réglages sans recompiler : action `exectune` de la session éditeur, qui écrit dans `DA_ExecutionRules`.

## Preuves
- Compilé ; tests 536/536 dont `monke_island.Execution.Layout` (4 cas) et `monke_island.Run.Contract.Failure` (verdict `Executed`, règle par défaut active).
- Testé éditeur (PIE `L_Frag_Raraka_01`, 2026-10-07). Journal :
  - « Execution: 1 of the crew on the plank, 4 soldier(s) on deck » ;
  - « the squad fires at BP_IslanderBean_C_0 » ×4 ;
  - « BP_IslanderBean_C_0 died ».
- Captures en rafale : titre, scène, salve, chute, titre de fin, écran « EXÉCUTÉS ».
- Le bouton MENU PRINCIPAL ramène à `L_MainMenu`.

## Écarts et points à valider
- Les soldats français exécutent un équipage britannique pour un quota manqué. C'est la demande telle quelle ; le récit (qui commande l'exécution) reste à écrire.
- Le multijoueur n'a pas été testé (seulement en solo). Chaque client tourne son propre pawn vers la mer : il possède sa rotation.
- Une seule planche ; au-delà de 4 joueurs, les places s'arrêtent au bout de la planche.
