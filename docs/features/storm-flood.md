# Tempête et montée des eaux

Lot L5 (nuit, danger). Tâche T-044. Cahier §5 : « La tempête finale dépend du début de submersion. » Le niveau d'eau reste un paramètre global.

## Une seule horloge : celle de l'expédition (T-098, lot L3)
Recette visée : cahier §12 n°13 à 18, surtout n°14 (« Zone validée à 75 s : échéance de montée à 240 s ; seconde zone = 360 s »).

- **Avec une expédition dans la map** (GameState `AExpeditionGameState` doté de `DA_ExpeditionRules`), l'eau et la tempête finale suivent l'horloge d'expédition (§6.1, T-053) :
  - la mer commence à monter à l'**échéance** de l'horloge : 120 s après le départ, +120 s par zone pacifiée avant le début de submersion ;
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

## Inondation (`AFloodSimulationActor`, `Source/monke_island/Flood/`)
- Eau peu profonde sur une grille posée sur le terrain, modèle des tuyaux virtuels (Mei, Decaudin, Hu 2007). L'eau descend les pentes, remplit les creux, et une crête la retient tant qu'elle n'est pas submergée.
- Grille : 512 × 512 cellules de 6 m, soit 3 km centrés sur l'acteur (village de Motutapu). Pas de 0,05 s. Calcul en `ParallelFor`.
- Le terrain est relevé en jeu par des tracés verticaux, sans compter la végétation ni les plans d'eau. Aucune donnée précalculée : la simulation suit toute modification du level design, et le terrain n'est jamais déformé.
- Les cellules de mer ouverte (plus de 1,5 m sous le niveau de la mer) et le bord de la grille suivent le niveau de la mer.
- Rendu : un maillage procédural par parcelle de 32 × 32 cellules, avec le matériau `M_Water_Lagoon`. Les parcelles sont reconstruites à tour de rôle toutes les 0,2 s.
- La surface locale alimente `UIslandWaterSubsystem::GetWaterSurfaceAt` et `GetWaterDepthAt` : le joueur patauge, nage et se noie avec l'inondation, et la barque flotte dessus (monk-island-aa).
- Réseau : chaque machine fait tourner les mêmes pas déterministes depuis l'horloge répliquée : `ClockState` et `Phase` de l'expédition, sinon l'horloge jour/nuit. Le serveur fait foi pour le gameplay.

## Débogage (hors Shipping, serveur)
`monke.SetTime 285`, `monke.FloodNow`, `monke.SetHour 19`, `monke.TimeSpeed 5`. Ces commandes ne valent que pour le repli jour/nuit. Sur l'horloge d'expédition, `monke.FloodNow` journalise un avertissement et ne déplace pas l'eau : Expedition/Core n'a pas encore de commande de saut.

## Limites connues
- Le plan de mer global ne monte pas : hors de la grille (au-delà de 1,5 km), l'océan reste au niveau 0, d'où une marche d'eau au bord de la grille. Le brouillard de tempête la masque en partie.
- Pas encore de pluie ni de sons de tempête. Seuls les cocotiers bougent avec le vent, pas les buissons.

## Critères d'acceptation
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
