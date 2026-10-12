# Feature : Garnison du fort français (beans soldats)

- **Statut** : à vérifier (T-213, T-217, T-219). Code compilé et testé (monke_island.Fort.Garrison, 10 tests ; suite complète 526/526). Garnison posée dans L_Test_Flat et vérifiée en jeu (combat), uniformes texturés, tir et rechargement sur les clips du mousquet. L_Test_Flat est cuisinée dans le build livrable du 2026-10-07 (console Inser : `open /Game/MonkeIsland/Maps/Test/L_Test_Flat`). Restent : LOD des beans français, et le fort dans la campagne (tiré au hasard dès le jour 6, T-214).
- **Owner** : lead
- **Date** : 2026-10-07
- **Cahier / lots** : L5 (PvE, campagne jour 6). T-204 (bean français) et T-205 (forts et garnison).

## Demande (mots du propriétaire)
> « Récupérer les modèles de beans français de Blender sur OneDrive, ensuite les 7, les mettre dans le fort français ; ils se baladent et patrouillent dans le fort ; ils attaquent le player ; ils ne peuvent pas sortir du fort. »
> « Ajoute-les au fort français de la map Test Flat. »

## Ce qui est en place
**Modèles**
- Source : maquette `OneDrive\maquette-blender\maquettes\bean-francais` (2e PC), en lecture seule. Copie dans `art/export/Characters/BeanFR/`.
- 7 uniformes, sur le squelette du bean joueur (`SK_Bean_Body_Skeleton`), dans `/Game/MonkeIsland/Characters/BeanFR/` :

  | Uniforme | Rôle |
  |---|---|
  | CFM_Fusilier | fusilier |
  | Fusilier_1767 | fusilier |
  | Fusilier_Bicorne | fusilier |
  | Chasseur_Shako | tirailleur |
  | Grenadier | grenadier |
  | Sergent | sergent |
  | Officier_Marine | officier |

- 27 instances de `M_Bean_Master` (couleur, rugosité et métal repris du glTF de la maquette).
- 4 clips : `A_BeanFR_Idle_Guard`, `Patrol`, `Bayonet_Thrust` et `Sabre_Combo`. Les autres clips viennent de la bibliothèque du bean.
- `SM_Bean_Bayonet` (pivot à la bouche).
- Script d'import : `scripts/character/import_bean_fr_ue.py` (action `beanfrimport`).

**Code** (`Source/monke_island/Fort/`)
- `AFortSoldier` : serveur autoritaire. Il patrouille de poste en poste, avec un arrêt de 4 à 10 s à chaque poste.
  - **Repérage** : il voit à 26 m, dans un cône de ±75°, et remarque à 4,5 m tout autour. Les murs et les buissons cachent le joueur.
  - **Alerte** : il prévient la garnison dans un rayon de 45 m.
  - **Mousquet** : il vise 0,7 à 1,3 s, tire (35 dégâts), puis recharge (clip de 6,6 s).
    - Il a 6 coups au total.
    - Il touche à 55 % jusqu'à 5 m et à 15 % à 15 m.
  - **Corps à corps** : il passe à l'arme blanche si le joueur est à moins de 1,4 m ou s'il n'a plus de coups.
    - Fusiliers et grenadier : baïonnette.
    - Tirailleur, sergent, officier, ou plus de coups : sabre (dégainé, puis coups tirés au sort).
  - **Déplacements** : chemins sur le maillage de navigation, marche droite à défaut.
  - **Animation** : `UBeanLibraryAnimInstance` (locomotion, clip de l'état, haut du corps pendant la course).
  - **Tir** : flamme, fumée et détonation reprises du mousquet du joueur.
- `AFortGarrisonZone` : le contour des murs du fort (tracé de la maquette), les postes de patrouille, l'alerte.
  - Les soldats qui démarrent dedans forment la garnison.
  - Leurs chemins et leurs buts restent à l'intérieur.
  - Si un soldat se retrouve dehors, le serveur le remet sur la dernière place où il se tenait à l'intérieur (contrôle 10 fois par seconde).
- `UFortSoldierRules` (`DA_FortSoldierRules`) : toutes les valeurs (vue, vitesses, coups, dégâts, portées, fenêtres de coup du manifeste, prises d'armes, effets, rôles).

**Rôles**

| Rôle | PV | Dégâts | Arme blanche |
|---|---|---|---|
| Fusilier | 100 | ×1 | baïonnette |
| Tirailleur | 100 | ×1 | sabre |
| Grenadier | 130 | ×1,2 | baïonnette |
| Sergent | 130 | ×1,1 | sabre |
| Officier | 200 | ×1,25 | sabre |

**Dans L_Test_Flat** (dossier `FortFrench/Garrison`, script `scripts/editor/fort_garrison_setup_ue.py`, actions `garrisonsetup` puis `garrisonnav`)
- `FortFR_Garrison` : 17 postes reliés par des chemins complets à la porte.
  - Cour : porte, citerne, logis, caserne, cuisine, chapelle, prison, corps de garde, coins et magasin.
  - Chemin de ronde : haut des rampes ouest et est, et tronçons nord et ouest.
  - 3 tronçons isolés du chemin de ronde ont été retirés.
- `FortFR_NavBounds` : navigation du fort, copie des bornes de l'arène.
- 7 soldats `FortFR_<Uniforme>` placés à leurs postes de départ :

  | Poste | Uniforme |
  |---|---|
  | porte | CFM |
  | corps de garde | 1767 |
  | rampe ouest | Bicorne |
  | rampe est | Chasseur |
  | citerne | Grenadier |
  | caserne | Sergent |
  | logis | Officier |

## Vérifications (2026-10-07)
- **Tests** : `monke_island.Fort.Garrison` (10) et `monke_island.Run` (28), tous réussis après compilation.
- **Partie lancée devant la porte, joueur invulnérable** :
  - l'officier repère le joueur à 26 m et alerte 6 soldats ;
  - tirs au mousquet (1 touché sur 9 premiers tirs, chances de 24 à 55 % selon la distance) ;
  - rechargements, coups de baïonnette (25), poursuite.
  - Le propriétaire a pris la main dans cette partie et a tué 6 soldats sur 7 au mousquet.
- **Défauts vus et corrigés** :
  - Uniformes en damier gris : les matériaux posés pendant la construction du maillage avaient été perdus. Ils ont été reposés avec l'outil MCP des maillages squelettiques et relus.
  - Tir et rechargement (demande du propriétaire) : le mousquet restait figé dans la main droite. Les soldats jouent maintenant les clips du mousquet du joueur en troisième personne (`DA_MusketRules`, `UFortSoldierRules.MusketPoses`), avec la trace du mousquet image par image (la main droite le lâche pendant le rechargement). La visée épaule l'arme et la tient jusqu'à la détente.

## Prisonniers, or de la chapelle, butin (demandes du 2026-10-07)
- **Prisonniers** : 4 villageois tiki (`SK_Villager`, sans IA, boucles Cower / Shake_No / Look_Around), sans collision (les barreaux les tiennent), tournés vers la cour.
  - 2 dans la grande cage au sol, 1 dans la seconde, 1 dans la cage suspendue au gibet.
  - Acteurs `FortFR_Prisoner_*` (script `scripts/props/fort_chapel_cages_ue.py`, action `fortcages`).
- **Or de la chapelle** : copies du trésor du fort posées sur l'autel, 5 300 $ (`FortFR_ChapelGold_*`, même script) :
  - une petite pile de lingots (3 000) ;
  - 3 lingots (500 chacun) ;
  - 2 piles de pièces (400 chacune).
- **Butin des bâtiments** (T-217) : 36 objets communs posés au lancement sur 306 emplacements dans les pièces, d'une valeur de 5 à 120 $ (`DA_FortLoot`, `FortFR_Loot`).

## Position de tir et rechargement (demande du 2026-10-07 : « fix la position de tir », « fix animation rechargement »)
Les soldats reprenaient les pistes du mousquet du joueur (`DA_MusketRules.FireMusketTrack`, `ReloadMusketTrack`), placées dans le repère de la main. Résultat : au tir, le mousquet pointait à 90° sur le côté pendant que le corps faisait face à la cible ; au rechargement, il partait dans tous les sens. Désormais (`AFortSoldier::UpdateMusketGrip`), le mousquet prend trois poses, avec un fondu de 0,25 s entre elles :
- **en main** : la prise `MusketInHand`, comme avant (patrouille, poste, course) ;
- **épaulé**, pendant la visée et le tir : la crosse dans la main droite, le canon pointé sur la cible. Le cap suit le soldat, qui fait face à la cible. Le site est l'inclinaison vers la poitrine de la cible, calculée par le serveur et répliquée dans `FFortSoldierActionRep.AimPitch`. Le canon pointe donc vers le joueur sur toutes les machines ;
- **debout**, pendant le rechargement : le mousquet est dressé (75° dans le repère du soldat) et tenu par le canon dans la main gauche, la crosse vers le sol. La main droite charge.

Réglages dans `DA_FortSoldierRules`, catégorie « Musket|Poses » : `FireYawOffsetDeg` (corps tourné par rapport à la cible, 0), `AimMusketOffset`, `ReloadHoldBone` (`hand_l`), `ReloadHoldPoint` (40 cm le long du canon), `ReloadMusketRotation`, `MusketPoseBlendSeconds`.

Vérifié en PIE dans la cour du fort de `L_Test_Flat` (rafale de 40 captures, 2026-10-07) : les soldats visent avec le canon vers le joueur et rechargent le mousquet debout à leur gauche.

Mode peloton (`StartFiringSquad`) pour l'exécution de l'équipage : voir `docs/features/execution-equipage.md`.

## Hostilité générale et soldats sans garnison (T-240, demande du 2026-10-08)
Code écrit, à compiler et tester par le lead. `AFortSoldier::IsEnemy` ne vise plus « un joueur vivant » mais tout vivant d'un autre camp (`Creatures/Hostility.h` : joueurs, tikis, crabe géant) ; `Sense` prend l'ennemi vu le plus proche dans le registre `UHostilitySubsystem`, les coups de baïonnette et de sabre frappent tout ennemi dans le cône, l'alarme de garnison passe n'importe quelle cible. Sans fort (`JoinGarrison` jamais appelé), le soldat garde son poste, sent, poursuit et combat ; posé ou créé dans une `AArenaZone`, il reçoit une laisse (`SetLeash`) qui borne ses buts, ses pas droits et ses fentes au cercle. Détails et recette : `docs/features/crabe-geant.md`, section « Arène ».

## Écarts et points à valider
- Le propriétaire avait d'abord demandé « un gun 6 coups » et 8 soldats par fort (T-205). La maquette arme les soldats d'un mousquet et d'un sabre, et en fournit 7. On a donc 6 coups de mousquet et 7 soldats dans le fort de test.
- Dégâts du mousquet : 35. Le manifeste suggère 45, je l'ai baissé pour 7 tireurs. Valeur à régler en jeu.
- Prises d'armes :
  - le mousquet en main reprend celle du joueur ;
  - le mousquet dans le dos, le fourreau et le sabre rengainé reprennent les sockets de la maquette, convertis ;
  - à vérifier sur captures.
- LOD des uniformes : pas encore générés (84 000 à 97 000 triangles chacun).

## Suite
- La campagne : fort tiré au hasard dès le jour 6, sans garnison avant (T-214).
- Les forts des fragments (T-205).
