# Tempête et montée des eaux

Lot L5 (nuit, danger). Tâche T-044. Cahier §5 : « La tempête finale dépend du début de submersion. » Le niveau d'eau reste un paramètre global.

## Une seule horloge : celle de l'expédition (T-098, lot L3)
Recette visée : cahier §12 n°13 à 18, surtout n°14 (« Zone validée à 75 s : échéance de montée à 240 s ; seconde zone = 360 s »).

- **Avec une expédition dans la map** (GameState `AExpeditionGameState` doté de `DA_ExpeditionRules`), l'eau et la tempête finale suivent l'horloge d'expédition (§6.1, T-053) :
  - la mer commence à monter à l'**échéance** de l'horloge : `DA_ExpeditionRules.InitialFloodDelay`, soit 540 s après le départ depuis le 2026-10-04 (déluge à 9 min, fin à 10 min, cahier v1.4 ; 120 s au cahier v1.3, 300 s dans le DataAsset jusque-là), +120 s par zone pacifiée avant le début de submersion ;
  - la montée dure `DA_ExpeditionRules.SubmersionDuration` (60 s), avec la hauteur `MaxWaterRiseCm` (650 cm) et la courbe `WaterRiseExponent` (1,6), comme `AExpeditionGameState::GetWaterRise` ;
  - aucun bonus après le début de submersion : l'échéance est figée ;
  - la fin de la submersion (`OnSubmersionEnded`, phase `Resolved`, puis l'extraction automatique) est le seul signal de fin ; l'acteur d'inondation ne décide rien et garde la mer au maximum ;
  - l'eau ne redescend jamais : la cible de la mer ne fait que monter (`FloodSchedule::HoldRise`). Un client attend la phase `Submerging` répliquée avant de faire monter l'eau : un bonus reçu en retard ne la fait pas monter puis redescendre.
- **Tempête finale** : le vent ultra violent (`DeadlineStrength`) se lève au début de submersion, ou `DA_WindRules.ExpeditionStormLeadSeconds` secondes avant (0 par défaut), jamais avant le départ. Un bonus accordé pendant cette avance repousse l'échéance, et le vent avec elle. Une fois la submersion commencée, il ne s'arrête plus. Les rafales comptent alors depuis le départ de l'expédition. `DeadlineDay` est ignoré.
- **Sans expédition dans la map**, repli sur l'horloge jour/nuit (chronologie ci-dessous : `DA_FloodRules.StartSeconds`, `RiseSeconds` et `DA_WindRules.DeadlineDay`).
- **Bascule par DataAsset** : `DA_FloodRules.ClockMode` et `DA_WindRules.ClockMode` (`EHazardClockMode`) :
  - `Auto` (défaut) : l'expédition si la map en a une, sinon le repli jour/nuit ;
  - `ExpeditionOnly` : sans expédition, l'aléa reste éteint (avertissement) ;
  - `DayNightOnly` : toujours l'horloge jour/nuit (maps de test).
- Le choix est fait une fois par acteur, dès que le GameState est connu sur la machine (un client attend sa réplication), puis il est journalisé (« follows the expedition clock » ou « follows the day / night clock »). Les deux horloges ne pilotent donc jamais le même aléa en même temps. Code : `Flood/HazardClock.*`, `Flood/FloodSchedule.*`, `WindMath::ExpeditionStormSeconds`.
- **Condition sur `Lvl_Raraka`** : `BP_IslandGameMode`, enfant de `Amonke_islandGameMode`, ne déclare pas encore de GameState d'expédition. Tant que T-095 n'a pas réglé `GameStateClass = BP_ExpeditionGameState` (avec `Rules = DA_ExpeditionRules`), la map reste sur le repli jour/nuit.

Chronologie de repli (map sans expédition), demandée par l'utilisateur le 2026-10-03, en temps de jeu écoulé sur l'horloge jour/nuit :

| Temps | Événement | Réglage |
|---|---|---|
| 1:20 | le vent se lève, violent au bout de 20 s | `DA_WindRules` `StartSeconds` 80, `RampSeconds` 20 |
| 3:00 | crépuscule | `DA_DayNightRules` clé 180 s → 18 h 15 |
| 3:40 | nuit | `DA_DayNightRules` clé 220 s → 19 h 30 |
| 4:50 | la mer monte de 6,5 m en 45 s | `DA_FloodRules` `StartSeconds` 290, `RiseCm` 650, `RiseSeconds` 45 |
| vers 5:50 | plus pied nulle part (motu ≤ 3,7 m) | |

## Vent (`AWindStormActor`, `Source/monke_island/Storm/`)
- Personnages : dérive de 2,2 m/s par vent moyen, plus forte en rafale. Elle passe par un mouvement racine additif, prédit côté client. Une simple force de 260 N était annulée par le freinage au sol, d'environ 1 500 N (constaté en PIE le 2026-10-03).
- Objets physiques et barque : pression de 375 Pa par vent moyen (traînée à 25 m/s), proportionnelle au carré du vent et multipliée par la surface exposée, déduite des bornes de l'objet. Accélération plafonnée à 25 m/s². Une noix de coco s'envole, une bûche de 45 kg résiste.
- Rafales : deux octaves de bruit de Perlin, avec une variation de direction de ±25°. Le vent souffle vers l'ouest, comme les alizés.
- C'est une fonction pure de l'horloge répliquée. Le serveur applique les forces ; chaque client les applique aussi à son propre personnage (prédiction du mouvement).
- Il pilote le vent moteur (`UWindDirectionalSourceComponent`, pour le feuillage et le tissu) et le facteur tempête du cycle jour/nuit : soleil ×0,25, brouillard ×4, exposition −0,7 EV.

## Vent dans les cocotiers (`MF_PalmWind`, `MPC_Wind`)
- `AWindStormActor` écrit à chaque image la force (`WindStrength`, rafales comprises) et la direction (`WindDirection`) du vent dans `MPC_Wind`. Référence : `DA_WindRules.MaterialParameters`.
- `MF_PalmWind` (un nœud HLSL) déplace les sommets (WPO) :
  - le tronc plie dans le sens du vent, en proportion du carré de la hauteur ;
  - il se balance dans l'axe du vent et sur les côtés, avec une phase propre à chaque arbre ;
  - les palmes frémissent vite, d'autant plus qu'elles sont loin du tronc.
- Réglages par instance de matériau :
  - `PalmWindBend` : 170 cm de flexion au sommet sous un vent de force 1 ;
  - `PalmWindFlutter` : 40 cm pour les folioles, 20 cm pour la base des palmes, 0 pour l'écorce ;
  - `PalmWindHeight` : 12 m.
- Les masters `M_Foliage_Leaf`, `M_Vegetation_Bark` et `M_Vegetation_VertexColor` ont une branche WPO derrière l'interrupteur statique `PalmWind`, éteint par défaut. Seules les 5 instances `MI_CoconutPalm_*` l'allument : les buissons (773 000 instances) ne paient rien.
- Coût à mesurer : WPO sur 43 000 cocotiers Nanite, avec invalidation des ombres virtuelles. Piste si nécessaire : distance de désactivation du WPO sur les ISM PCG.

## Noix de coco qui tombent (`ACoconutFallDirector`, `AFallingCoconutActor`, `Source/monke_island/Hazards/`)
- Le serveur choisit les cocotiers autour de chaque joueur, dans un rayon de 25 m. Ce sont les ISM placés par PCG, reconnus au chargement par leur nom de mesh et leur hauteur (6 m au moins) : 49 322 cocotiers sur Raraka, relevé PIE du 2026-10-03.
- Fréquence : une noix toutes les 7 s par temps calme, toutes les 1,2 s sous la tempête, plus souvent encore en rafale. L'intervalle est tiré au hasard autour de la moyenne.
- Choix de la noix : 50 % visées (une couronne à moins de 6 m lâche sa noix en tir balistique sur la tête, en tenant compte du temps de chute et du déplacement du joueur), 40 % depuis le cocotier le plus proche, le reste au hasard. Pendant la chute, la noix est abritée du vent (tag `WindSheltered`, retiré au premier contact) : une noix dense dérive peu, et le vent faussait la visée de plusieurs dizaines de mètres.
- PIE du 2026-10-03, 50 s en tempête : 3 KO (chocs de 10 à 16 m/s), bean en ragdoll avec le chapeau tombé, santé de 100 à 55.
- Choc : à 7,5 m/s ou plus (une chute d'environ 3 m), le joueur est assommé. Il perd 15 PV, passe en ragdoll via `AIslanderCharacter::Knockout` (ajouté par monk-island-4f), puis se relève tout seul ; son chapeau tombe. Une noix n'assomme qu'une fois.
- Au plus 40 noix restent à terre, chacune pendant 90 s. Le vent les pousse comme tout objet libre.
- Débogage : `monke.DropCoconut`, une noix visée sur le premier joueur.

## L'eau monte sur le fragment joué (T-103, lot L3)
Recette visée : cahier §6.1 (la submersion couvre progressivement les terres accessibles), §12 n°16 (l'eau ne baisse jamais), demande T-101 (après la submersion, +10 cm/s jusqu'à +15 m). Demande du propriétaire (2026-10-04) : « l'eau monte uniquement dans la zone où je suis, pas à la zone de spawn ».

- **Cause du « la mer ne monte pas »** : la grille d'`AFloodSimulationActor` faisait 3 km et restait centrée sur la position fixe de l'acteur dans `Lvl_Raraka`. Or le jeu démarre dans la barque devant un fragment tiré au hasard, à plus de 10 km de là (T-093/T-095). L'eau montait donc ailleurs.
- **Ce qui change** : au début de l'expédition, le serveur pose la grille sur le fragment joué, puis réplique son centre (`GridCenter`). Toutes les machines lisent ainsi le même sol.
  - Centre : la barque d'arrivée (`UBoatStartSubsystem::GetArrivalBoat`), avancée vers l'île le long de sa proue de `ArrivalToGridCenterCm` (600 m). La proue vise la plage, à 200–350 m. Sans barque d'arrivée, on prend le premier pion.
  - Taille : `ExpeditionGridCells` × `ExpeditionCellSizeCm` = 320 × 8 m = 2,56 km. Les fragments font au plus 2 km de terre (`fragments.json`) ; la barque reste à environ 680 m du bord arrière.
  - Montée : 650 cm en 60 s, puis +10 cm/s jusqu'à +1 500 cm, soit +21,5 m (`DA_ExpeditionRules`, T-101). Elle ne descend jamais (`FloodSchedule::HoldRise`). Un client attend la phase `Submerging` répliquée.
  - La surface de la grille alimente `UIslandWaterSubsystem::SetSurfaceProvider` : flottaison de la barque, pataugeage, nage, noyade, requins, tiki et bruits de pas la suivent.
  - La zone de départ et le reste de la map restent au niveau de la mer : `DA_FloodRules.bRaiseGlobalSea = false`. `UFloodSeaRiseSubsystem` sait monter la mer globale (`SetSeaLevel`, par pas de `SeaLevelStepCm`), mais seulement si cette case est cochée.
- Réglages (`DA_FloodRules`) :
  - catégorie *Expedition sea* : `bLocalGridInExpedition` (oui), `ExpeditionGridCells` (320), `ExpeditionCellSizeCm` (800), `ArrivalToGridCenterCm` (60 000), `bRaiseGlobalSea` (non), `SeaLevelStepCm` (2), `FrameStatsLeadSeconds` (10) ;
  - catégorie *Render* : `MeshBudgetMs` (1 ms), `FlatChunkToleranceCm` (3 cm).
- Journal : `flood grid laid on the fragment played, from the arrival boat of FRG_…`, `flood grid of 320 x 320 cells of 8.0 m…`, `SeaRise: the water starts rising`, `SeaRise: the water reached its top`.

### Mer hors de la grille et acteur unique (T-141, lot L3, revue REC, constats 3 et 6)
- **Constat 3 : marche d'eau au bord de la grille.** Hors de la grille, `GetFloodSurfaceAt` renvoyait false, et l'eau retombait au niveau de la map. Le bord arrière se trouve à environ 680 m derrière l'arrivée : une barque ou le galion qui partait au large tombait de plusieurs mètres.
- **Correction** : `FFloodOutsideSea` (`Flood/FloodOutsideSea.h`), un calcul pur et déterministe, posé une fois le sol lu, en mode expédition seulement.
  - Hors de la grille, la mer prend la même montée que la mer ouverte de la grille : `niveau de base + HeldRiseCm`.
  - Montée pleine sur toute la grille et sur la terre du fragment élargie de `OutsideFullRiseMarginCm` (1,5 km). La terre, ce sont les cellules dont le sol dépasse le niveau de la mer, lues par les mêmes tracés sur toutes les machines.
  - Au-delà, fondu progressif (smoothstep) jusqu'à aucune montée, sur `OutsideFadeCm` (500 m). Encore au-delà, `GetFloodSurfaceAt` renvoie false et la mer de la map reste intacte. La zone de spawn et le reste de `Lvl_Raraka`, à plus de 10 km, ne sont jamais inondés (demande du propriétaire).
  - Dans la grille, une bande de `EdgeBlendCm` (48 m) le long du bord fond la surface de la grille dans cette mer : pas de marche, ni visible ni ressentie. Une cellule sèche de la bande ne passe sous l'eau que si la mer du dehors dépasse son sol.
  - Rendu : un « tablier » (`FloodApron`, maillage procédural en treillis de `ApronCellCm`, 128 m) part exactement du bord des parcelles. Il est construit une fois en unités de montée, puis mis à l'échelle en Z par la montée à chaque image (une mise à jour de transform). Il reste caché sous `ApronMinRiseCm`, et il est enfoncé de `ApronSinkCm` pour que son bord ne scintille pas contre la mer de la map.
  - Le galion (ancre à 330–410 m de la plage) suit `GetFloodSurfaceAt` et monte avec l'eau, dans la grille comme dans la marge.
  - Réglages `DA_FloodRules`, catégorie *Expedition sea|Outside the grid* : `bRaiseOutsideGrid` (oui), `OutsideFullRiseMarginCm` (150 000), `OutsideFadeCm` (50 000), `EdgeBlendCm` (4 800), `ApronCellCm` (12 800). Catégorie *Render* : `ApronMinRiseCm` (1), `ApronSinkCm` (2).
  - Journal : `the sea of the fragment rises outside the grid up to X km past its border…` et `apron of N x M vertices…`.
- **Constat 6 : plusieurs `AFloodSimulationActor` dans un monde** (par exemple un dans la map et un dans `LI_ExpeditionCore`). Au `BeginPlay`, un seul est gardé, et les autres sont désactivés avec un `Warning` : tick, parcelles, tablier, fournisseur de surface et délégués. La règle (`FFloodActorRank`) ne dépend pas de l'ordre des `BeginPlay` et donne le même résultat sur toutes les machines :
  1. l'acteur du niveau persistant d'abord ;
  2. puis le plus petit nom d'acteur ;
  3. puis le plus petit paquet de niveau, sans préfixe PIE.

  Un niveau chargé plus tard qui apporte un acteur mieux classé désactive l'ancien.
- Tests : `monke_island.Flood.Outside` (pas de marche au bord, fondu monotone et doux, rien au-delà de la portée ni à 10 km, galion couvert, rang des doublons).

### Lag à l'immersion : cause, correction, mesures
- **Cause probable.** La grille 512 × 512 était hors du fragment, presque toute en mer ouverte. Une fois l'eau montée, ses 256 parcelles de 32 × 32 cellules étaient toutes mouillées et dessinées, double face (environ 8 000 triangles par parcelle). Elles étaient reconstruites à tour de rôle, 24 par image, en boucle toutes les 0,2 s. Chaque `CreateMeshSection` refait les données de rendu et crée un `UBodySetup` neuf (cuisson asynchrone même sans collision), donc des déchets pour le ramasse-miettes. S'ajoutaient 1 à 4 pas de simulation par image.
- **Correction.**
  1. Grille plus petite : 102 400 cellules au lieu de 262 144 (×0,39), et 100 parcelles au lieu de 256.
  2. Parcelles plates : une parcelle entièrement sous l'eau, à `FlatChunkToleranceCm` près du niveau de la mer (mer ouverte, terre noyée), devient un seul quadrilatère. Il est créé une fois, puis simplement déplacé avec la mer, sans reconstruction. Seules les parcelles de rivage, où l'eau avance, sont reconstruites.
  3. Budget : la reconstruction s'arrête après `MeshBudgetMs` (1 ms) par image et reprend à l'image suivante (24 parcelles au plus).
- **Mesure hors moteur** du 2026-10-04 : même algorithme de pas, en C++ autonome, en série, 200 pas, sur la machine de dev pendant un build. Ancienne grille 512² : 1,36 ms par pas en moyenne (min 0,81, max 3,6). Nouvelle grille 320² : 0,51 ms (min 0,31, max 1,6). Le moteur répartit le calcul sur plusieurs cœurs (`ParallelFor`). Cette mesure ne couvre pas la reconstruction des parcelles, qui demande l'éditeur.
- **Mesure en jeu (à faire, éditeur requis).** `UFloodSeaRiseSubsystem` journalise les temps d'image réels (`FApp::GetDeltaTime`) en min / moyen / max sur trois fenêtres : les 10 s avant la submersion, la submersion, puis la montée qui suit (lignes `SeaRise frame times, …`). En complément : `stat unit`, ou Insights avec les marqueurs `AFloodSimulationActor::Simulate`, `AFloodSimulationActor::BuildChunks` et `UFloodSeaRiseSubsystem::Tick`. Le test `monke_island.Flood.SeaLevel` donne le coût d'un pas dans le moteur pour les deux tailles (lignes « Legacy » et « Fragment »).
- Il n'existe pas de commande pour avancer l'horloge d'expédition (`Expedition/Core` n'en a pas). Pour atteindre la submersion, il faut attendre 540 s (9 min) avec `DA_ExpeditionRules` ; baisser `InitialFloodDelay` dans le DataAsset le temps d'un test.

## Inondation locale (`AFloodSimulationActor`, `Source/monke_island/Flood/`)
- Expédition : posée sur le fragment joué (voir ci-dessus), de 320 × 8 m. Les points suivants décrivent la grille du repli jour/nuit, centrée sur l'acteur.
- Eau peu profonde sur une grille posée sur le terrain, modèle des tuyaux virtuels (Mei, Decaudin, Hu 2007). L'eau descend les pentes, remplit les creux, et une crête la retient tant qu'elle n'est pas submergée.
- Grille : 512 × 512 cellules de 6 m, soit 3 km centrés sur l'acteur (village de Motutapu). Pas de 0,05 s. Calcul en `ParallelFor`.
- Le terrain est relevé en jeu par des tracés verticaux, sans compter la végétation ni les plans d'eau. Aucune donnée précalculée : la simulation suit toute modification du level design, et le terrain n'est jamais déformé.
- Les cellules de mer ouverte (plus de 1,5 m sous le niveau de la mer) et le bord de la grille suivent le niveau de la mer.
- Rendu : un maillage procédural par parcelle de 32 × 32 cellules, avec le matériau `M_Water_Lagoon`. Les parcelles sont reconstruites à tour de rôle toutes les 0,2 s, dans la limite de `MeshBudgetMs` par image. Une parcelle entièrement noyée au niveau de la mer est un seul quadrilatère, déplacé avec la mer (T-103).
- La surface locale alimente `UIslandWaterSubsystem::GetWaterSurfaceAt` et `GetWaterDepthAt` : le joueur patauge, nage et se noie avec l'inondation, et la barque flotte dessus (monk-island-aa).
- Réseau : chaque machine fait tourner les mêmes pas déterministes depuis l'horloge répliquée : `ClockState` et `Phase` de l'expédition, sinon l'horloge jour/nuit. Le serveur fait foi pour le gameplay.

## Débogage (hors Shipping, serveur)
`monke.SetTime 285`, `monke.FloodNow`, `monke.SetHour 19`, `monke.TimeSpeed 5`. Ces commandes ne valent que pour le repli jour/nuit. Sur l'horloge d'expédition, `monke.FloodNow` journalise un avertissement et ne déplace pas l'eau : Expedition/Core n'a pas encore de commande de saut.

## Limites connues
- Le plan de mer global ne monte pas (choix du propriétaire, `bRaiseGlobalSea` décoché). Depuis T-141, la mer du fragment monte aussi hors de la grille, sur la marge, puis s'estompe (voir plus haut). La marche n'existe plus qu'en repli jour/nuit (sans expédition), où la mer hors grille n'est pas posée.
- Le tablier reprend `M_Water_Lagoon`. Son rendu (raccord avec les parcelles, scintillement au bord du fondu, vagues de Gerstner du plan de mer) n'a pas encore été vu en PIE.
- Le centre est fixé une fois, au départ. Une expédition qui change de fragment en cours de partie ne déplace pas la grille.
- Avec `bRaiseGlobalSea` coché : `SM_Raraka_Water` a le profil `BlockAllDynamic` (QueryAndPhysics). S'il a une géométrie de collision, chaque pas de 2 cm la déplace dans la scène physique.
- Pas encore de pluie ni de sons de tempête. Seuls les cocotiers bougent avec le vent, pas les buissons.

## Critères d'acceptation
- [ ] T-103 : `monke_island.Flood.SeaLevel` passe (niveau = base + montée, jamais en baisse, poursuite jusqu'au plafond, réglages par défaut, mesure d'un pas). Compilé en cible jeu le 2026-10-04 ; tests et PIE pas encore lancés.
- [ ] T-103 : en PIE sur `Lvl_Raraka`, l'eau monte sur le fragment de départ, la barque flotte plus haut et le pion nage puis se noie ; la zone de spawn reste sèche. Relever les lignes `SeaRise frame times` avant et pendant la submersion.
- [ ] T-141 : `monke_island.Flood.Outside` passe. En PIE, une barque qui sort de la grille vers le large ne tombe pas, le galion monte avec l'eau, aucune marche n'est visible au bord, et le spawn reste sec. Avec deux `AFloodSimulationActor`, un `Warning` apparaît et une seule simulation tourne.
- [x] Les tests `monke_island.Flood.Grid` passent (volume conservé, écoulement vers le bas, crête qui retient l'eau, bas avant haut, chronologie du vent et de la mer).
- [x] PIE du 2026-10-03, 7 min en temps réel : tempête visible dès 1:50 (soleil de 6 à 1,5), nuit noire à 3:55, la mer commence à monter à 4:50 (journal : « the sea starts rising »).
- [x] Le joueur nage (HUD « Nage ») 45 s après le début de la montée, puis se noie à 75 s (journal : « BP_IslanderBean_C_0 died »). Le joueur réapparu se noie aussi : l'île est entièrement sous l'eau.
- [x] Le vent fait dériver le joueur (PIE 2 : environ 2,3 m/s vers l'ouest ; un joueur immobile est poussé à la mer et s'y noie à 3:41).
- [x] Les objets restent sous la vitesse du vent (PIE 3, après correctif sur la vitesse relative à l'air : 1,7 à 5 m/s ; avant, une noix de coco était éjectée à 1 km/s).
- [ ] Bug à analyser : les objets poussés vers le lagon traversent le sol et tombent à plusieurs km sous la carte, alors que le terrain est en `CTF_UseComplexAsSimple` (PIE 3).
- [ ] La nuit reste lisible (DA réglé : lune 0,2, exposition −0,6 EV, EV100 minimal −3 ; capture à refaire).
- [ ] Coût de la simulation mesuré (`stat game`) sous le budget de `docs/conventions/performance.md`.
- [x] T-098 compilé (cible jeu) : `AFloodSimulationActor` et `AWindStormActor` suivent `AExpeditionGameState` quand la map en a un ; les specs `monke_island.Flood.ExpeditionClock` et `monke_island.Storm.ExpeditionDeadline` sont écrites.
- [ ] T-098 : lancer ces specs (éditeur fermé).
- [ ] T-098 : PIE sur `Lvl_Raraka` après T-095. Le journal doit afficher « follows the expedition clock », puis la mer doit monter à 2:00 sans zone, à 4:00 avec une zone pacifiée à 1:15, et le compteur HUD doit donner la même échéance.
- [ ] T-098 : PIE hôte + client, même échéance et même début de montée des deux côtés.
