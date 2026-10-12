# Feature : Forts français générés sur l'atoll de Raraka

- **Statut** : à vérifier. Preuves au 2026-10-08 :
  - **compilé** ;
  - **suite complète** : 547 tests verts, dont 7 `monke_island.Fort.Site` ;
  - **testé éditeur** : PIE sur `Lvl_Raraka`, fort dans le fragment 05, 7 soldats dans l'enceinte, trésor intact, captures ;
  - **testé build** : v1.4 empaquetée, lancée sur `Lvl_Raraka?Island=FRG_Raraka_07` deux fois. Le fort sort chaque fois sur un emplacement différent, avec sa garnison de 7. Le temple est posé à l'écart, et le navmesh du fort se construit sans limite de tuiles.
  - Reste à faire : une partie complète par le propriétaire, et le multijoueur (hôte et client).
- **Owner** : lead
- **Date** : 2026-10-08
- **Cahier / lots** : L5 (campagne, PvE). Suite de T-205 / T-214 : le fort ne vient plus d'une île à part.

## Demande (mots du propriétaire)
> « Fais en sorte que les forts français spawn sur la map de Raraka Tuamotu et pas sur une île personnalisée, fais apparaître le temple artefact, les crabes, les forts dès le jour 1. »
> « Crée une map et génère procéduralement. »

Option choisie : « Forts générés sur Raraka ». Le jeu calcule une carte des emplacements possibles sur les vraies îles de l'atoll, puis génère les forts à chaque expédition, comme les villages. L'île du fort dans le lagon est supprimée.

## Ce qui est en place
**La carte des emplacements** (`scripts/map/place_forts.py`)
- Sortie : `docs/reference/map/raraka/collectibles/fort_sites.json`, avec une image de contrôle `fort_sites.png` (`scripts/map/fort_sites_map.py`).
- Un emplacement est le centre du fort. Il doit remplir toutes ces conditions :
  - sur la terre d'un fragment ;
  - à 0,6 m au moins au-dessus de la mer ;
  - sec (canal humidité des masques) ;
  - plat : 1,8 m de relief au plus sur un disque de 45 m ;
  - loin des villages et hameaux (zone + 60 m), de leur centre dans le fragment (150 m), de l'arrivée de la barque (150 m), de Motutapu (350 m) et du Marae des Perles (140 m).
- Les emplacements sont éclaircis à 160 m les uns des autres. On garde les 3 plus plats par fragment.
- Un fragment sans emplacement passe par un niveau souple :
  - 1,0 à 3 m de relief sur un disque de 38 m ;
  - 90 m des villages et de l'arrivée.
- Résultat : 52 emplacements, au moins un dans chacun des 20 fragments. Les fragments 12 et 19 passent par le niveau souple.
- La porte du fort (axe +Y local) est tournée vers la côte la plus proche.

**Le fort terrestre** (`/Game/MonkeIsland/Maps/Fragments/LI_FortFrench_Land`, script `scripts/editor/fort_land_ue.py`, étape `clean`)
- Copie de `LI_FortFrench` sans son île : 167 acteurs retirés. Il s'agit :
  - du terrain lointain et du lagon ;
  - du port et du ponton, avec ses caisses et sa lanterne ;
  - de la végétation et des volumes PCG ;
  - du panneau et du navmesh.
- 240 acteurs gardés : remparts, bâtiments, tour, porte, prison, trésor, garnison, lumières.
- Le **plateau** du fort (`SM_Fort_TerrainNear`) est gardé. Il porte la cour (165 cm), les douves en eau (115 cm) et la salle du trésor enterrée (sol à 66 cm). Le fort se pose dessus comme sur une butte.
- Côté porte, le plateau finit par un quai de 3 m (là où était le ponton). Une rampe de sable (`FortLand_GateRamp`, pente de 13°) descend du glacis jusqu'au sol de l'atoll. Les tracés le confirment : son dessus est à la hauteur prévue.

**Le générateur** (`Source/monke_island/Fort/`)
- `UFortSiteSubsystem` (serveur), chemin de `DA_FortSiteRules` dans `DefaultGame.ini` :
  - dans le fragment que joue le monde (option `Island=` de l'URL), il tire `FortsPerExpedition` emplacements au début de la partie ;
  - il le fait avant que les camps, les marae, le temple et les tikis regardent la terre ;
  - en PIE ouvert directement sur la carte, il prend le fragment le plus proche des joueurs ;
  - le fort s'ouvre au jour `DA_RunRules.Campaign.FortsFromDay` (**3** depuis le 2026-10-09, 1 avant) ; hors campagne il est toujours là ;
  - le nombre de forts suit la courbe de difficulté du jour (`DA_RunRules.DifficultyByDay.FortsPerExpedition` : 1 les jours 3-5, 2 les jours 6-8, 3 les jours 9-10 ; `DA_FortSiteRules.FortsPerExpedition` quand le jour n'en fixe pas), dans la limite des emplacements du fragment ;
  - les cartes sans fragment (menus, Test Flat) n'en ont pas.
- **Hauteur** : le sol est mesuré au centre et sur 3 anneaux (15, 30, 45 m ; 25 points, végétation ignorée). Le quantile 0,9 de ces hauteurs se place à `TopGroundLocalCm` (50 cm) dans le repère du fort. Ainsi le sol de l'atoll passe sous le trésor et sous l'eau des douves, et la cour reste un peu au-dessus.
- `AFortSite` (répliqué, toujours pertinent, tag `TikiQuietZone`) :
  - sur chaque machine, il charge `LI_FortFrench_Land` en level instance dynamique, au même nom que sur le serveur, nom nécessaire pour la réplication des soldats, du butin et des prisonniers ;
  - il dégage la végétation dans un rayon de 65 m (instances descendues, index gardés) ;
  - il ferme 70 m aux tikis, camps, marae et temples.
- `FortSiteMath` : tirage des emplacements, quantile, sondes, repère du fort, nom du niveau (tests `monke_island.Fort.Site`).
- Valeurs dans `DA_FortSiteRules` (`/Game/MonkeIsland/Environment/FortFrench/Data`) : niveau, forts par expédition, hauteur, anneaux, quantile, rayons, graine, emplacements par fragment (étape `rules` du script, depuis `fort_sites.json`).

**Le reste de la demande**
- `FRG_Raraka_21` (l'île du fort dans le lagon) sort de `DA_IslandCatalog` (20 fragments). Son level instance est retiré de `L_Frag_Raraka_01`.
- Temple, crabes géants et forts dès le jour 1 : `DA_RunRules.Campaign` (TemplesFromDay, GiantCrabsFromDay, FortsFromDay = 1), valeurs par défaut du C++ et tests mis à jour.
- **Le temple n'apparaissait pas** sur les fragments 02 à 20 (carte `Lvl_Raraka`) : leur `CampSpawner` n'avait pas de `TempleRules` (T-180 l'avait laissé pour plus tard). Il a maintenant `DA_TempleReliquaryRules`. En PIE : « reliquary temple at … », avec la Perle rouge de Raraka sur le socle.

## Vérifié en éditeur (2026-10-08)
- PIE sur `Lvl_Raraka` (jour de campagne 2 de la sauvegarde de l'éditeur) :
  - « Forts: FRG_Raraka_05 site 1 of 2 … ground 200 cm (25 of 25 probes hit) » ;
  - « fort LI_FortFrench_Land_FRG_Raraka_05_0_… 1360 vegetation instances cleared » ;
  - 7 soldats, tous dans l'enceinte ;
  - le temple et 12 marae sont posés à côté.
- Captures (`Saved/Screenshots/WindowsEditor/HighresScreenshot00120-00122`) : le portail et la rampe, la vue aérienne (douves, bastions, bâtiments, lagon), la salle du trésor (coffres et gemmes, sans terrain qui la traverse).

## Navigation des soldats
- `Lvl_Raraka` n'a pas de navmesh. Le volume de navigation du fort (`FortFR_NavBounds`) fait créer au serveur un navmesh à la volée, limité au fort, une seule fois au chargement.
- Ce chargement a lieu dans le noir avant l'arrivée. Coût : environ 100 ms sur une image, surtout pour lire la collision du terrain (281 600 triangles par tuile).
- Aucune reconstruction ensuite : rien en 6 minutes de jeu dans le build.
- Les remparts, la cour, la voûte et le pont empilent plus de 3 couches par tuile. Le défaut du moteur (3) donnait « 60 tile limit reached » et un navmesh incomplet. `DefaultEngine.ini` passe donc `AverageLayersPerTile` à 8.
- Les clients ne construisent rien : la navigation reste côté serveur.
- **Carte avec son propre navmesh statique** (`L_Test_Flat`, T-319, 2026-10-11) : le moteur ne construit jamais le volume d'un fort chargé en jeu, donc le second fort de la carte de test n'a pas de navmesh. Sans chemin, un soldat marche tout droit (`DirectStep`). Il butait contre un mur pendant des minutes (« is stuck and failed to move » 60 fois par seconde). Désormais, un mur juste devant (deux traces, taille et poitrine : une rampe ou une marche n'est pas un mur) ou l'absence d'avancée (`DirectMinProgressCm` en `DirectProgressSeconds`) arrête la marche : il prend son poste, puis en essaie un autre. Essais écartés :
  - navmesh dynamique : reconstruction permanente, environ 5 ms par image, à cause des objets physiques de l'arène ;
  - construction unique forcée au chargement du fort (`MarkRequiresInitialRebuild` + `RebuildAll`) : le navmesh statique est redimensionné, et en PIE il n'avait plus aucun chemin, même dans le premier fort.

## Limites connues
- **Ordre de génération en PIE direct** : le fort peut arriver après les marae, car il attend le joueur. Le jeu normal (URL avec `Island=`) le pose en premier.
- **Rampe** : un pavé de sable de base, fonctionnel. Un modèle de rampe ou d'escalier serait plus joli.
- **Crabe géant** : `UGiantCrabEmergenceSubsystem` ne fait plus sortir de crabe dans l'enceinte d'un fort (`AFortGarrisonZone::FindAt`). Le glacis, hors des murs, reste possible.
- **Non testé** : le build empaqueté (à faire à la livraison) et le multijoueur, avec le chargement du fort chez le client.

## Jour 3 et difficulté par jour (2026-10-09, T-252) — code écrit, non compilé
> « ajoute au 3e jour les forts français sur Raraka et les temples artefacts ; mets "tout devient plus dur" »
- **Jour 3** : `DA_RunRules.Campaign.FortsFromDay` = 3 (et `AllArtifactsFromDay` = 3 : les 3 temples à artefacts arrivent avec les forts). Jours 1-2 : pas de fort, un seul temple.
- **Garnison du jour** (`AFortGarrisonZone`, serveur) : `GarrisonSettleSeconds` (1 s) après le début du niveau du fort, la zone compte sa garnison et l'amène à `SoldiersPerFort` du jour (7 le jour 3, 8 les jours 4-5, 9 le jour 6, 10 le jour 7, 11 les jours 8-9, 12 le jour 10 ; 0 = telle que placée) : les soldats manquants sont des clones des soldats placés (classe BP, `Rules`, `Kind`, `Skin`, `HatMesh`, `HeadMesh`, à tour de rôle) posés aux postes de patrouille suivants (`FortGarrisonMath::SpawnPost`), `SpawnLiftCm` au-dessus ; un surplus est retiré (les derniers vus). Chaque soldat pris dans la garnison reçoit `MobHealthScale` fois sa santé (`UIslanderHealthComponent::ScaleMaxHealth`, après son `BeginPlay`, d'où l'attente de `HasActorBegunPlay`). Journal : « garrison of 7, the day wants 9: 2 soldier(s) added at the posts (health x1.35, hit chance x1.20) ».
- **Précision des soldats** : `AFortGarrisonZone::GetHitChanceScale()` (lu au début depuis `URunSubsystem::DifficultyOf`). **À brancher par le lead** dans `AFortSoldier::Shoot` (`Fort/FortSoldier.cpp`, lignes 1021-1023, fichier réservé) : après le calcul de `Chance`, `Chance = FMath::Min(1.0f, Chance * (Garrison.IsValid() ? Garrison->GetHitChanceScale() : URunSubsystem::DifficultyOf(this).SoldierHitChanceScale));` (inclure `Run/RunSubsystem.h`). Sans ce branchement, la précision reste celle du DA.
- **Dégâts des soldats sur les joueurs** : × `MobDamageScale` du jour, côté `UIslanderHealthComponent` du joueur (rien à faire dans `FortSoldier`).
- **Nombre de forts** : voir ci-dessus (`FortsPerExpedition` du jour). Les fragments 12 et 19 n'ont qu'un emplacement : un seul fort, journal « day N asks K forts, FRG_Raraka_12 has 1 site(s) ».
- **Tests écrits** : `monke_island.Fort.Garrison` (« Garrison of the day »), `monke_island.Run.Campaign`. Non exécutés : à compiler et lancer par le lead.
- Détail de la courbe et des autres valeurs : `campagne-dix-jours.md`, « Difficulté par jour ».

## Suite du 2026-10-08 (soir)
- **Douves mortelles** (« quand je tombe dans les douves du fort français je meurs dans l'eau ») :
  - `AFortMoat` est posé dans `LI_FortFrench_Land` et près du fort de `L_Test_Flat`. Il est relié aux murs (`FortFR_Garrison`) et à l'eau (`Fort_T198_SM_Fort_MoatWater`).
  - Un joueur dont les pieds restent 40 cm sous la surface, dans la bande de 8 m le long des murs, se noie au bout de 0,6 s.
  - Le pont, le glacis, la cour et le sable de l'atoll autour ne tuent pas.
  - La noyade passe aussi l'invincibilité de la carte de test.
  - Vérifié en PIE : « fell into the water of the moat (63 cm under the surface) », puis « died (killer: FortMoat_0) ».
  - Test `monke_island.Creatures.HitZone` (cas des douves).
- **Fort de la section Raraka de Test Flat** (« ajoute le fort français dans la map, section réservée à Raraka, et la génération du fort ») :
  - `DA_FortSiteRules` reçoit `FRG_TestFlat`, avec `TestMap = L_Test_Flat` et un emplacement sur l'île des générateurs : (-145 m, -115 m), sol plat, à 43 m des villages et des marae, portail tourné vers le départ du joueur.
  - `UFortSiteSubsystem` le construit au lancement de la carte quand celle-ci ne joue aucun fragment du catalogue.
  - Vérifié en PIE : fort construit, 7 soldats dans l'enceinte, 1 039 plantes dégagées (`HighresScreenshot00123`).
  - L'ancien fort-vitrine sur son île, à l'ouest (-660 m), reste en place.

## Corps qui volaient après la mort (2026-10-10, T-277) — statut : compilé, test PIE partiel
Demande du propriétaire : « fix les corps des soldats français voler après mort ».
- **Causes** : (1) le crabe géant lance un soldat **puis** lui inflige les dégâts du lancer : le soldat mourait en l'air, et la mort coupait son mouvement (`DisableMovement`) : le corps restait suspendu ; (2) les coups du crabe (recul) et le souffle des grenades appelaient `LaunchCharacter` sur des corps déjà morts, qui repartaient en vol.
- **Correctifs** (`Fort/FortSoldier.*`) : un soldat mort ignore `LaunchCharacter` ; un soldat tué en l'air tombe sous la gravité (vitesse horizontale amortie) et ne s'immobilise qu'en touchant le sol (`Landed`) ; souffle de l'explosion du corps ramené de 700 à 380 cm/s (`DA_FortSoldierRules.ExplodeImpulse`).
- **Preuve** : compilé, build complet et suite verte ; en PIE, les soldats tués dans l'arène reposent au sol ; le cas exact (lancer du crabe puis mort en l'air) n'a pas pu être reproduit en plein air (le test de lancer a été fait à l'intérieur du fort, sur un étage).

## Soldats qui disparaissent, gardes des bâtiments, navmesh des forts (2026-10-11, T-332, L5) — statut : compilé (fichiers seuls), non testé
Demande du propriétaire : « fix le fort lorsqu'il apparaît sur raraka, les soldats bug encore lorsque je leur tire dessus à la mitrailleuse il dépop, je veux plus de soldats dans les bâtiments ».

**Soldats qui « dépop » sous la mitrailleuse** (vu dans la fenêtre client d'une PIE hôte + client, `L_Test_Flat`, 2026-10-11 20:20)
- La mitrailleuse arrache une jambe en une balle (34 × `LimbDamageScale` = 48 > `LimbHealthLeg` 30) : presque chaque victime passe en reptation (capsule de 55 à 30 cm, maillage remonté de 25 cm par `ApplyStance`), puis meurt en `CrawlDeath`.
- **Cause** : chez un client, le soldat est un *simulated proxy* ; le lissage réseau du mouvement (`UCharacterMovementComponent::SmoothClientPosition_UpdateVisuals`) remet le maillage à l'offset mémorisé par le personnage (`BaseTranslationOffset`, celui du soldat debout) à chaque déplacement envoyé par le serveur. Sur la capsule basse, le corps rampant puis le cadavre étaient dessinés **25 cm sous le sol**. L'hôte et le jeu solo n'ont pas ce lissage : le défaut ne se voit que chez un client.
- **Correctif** : `ApplyStance` met à jour cet offset (`CacheInitialMeshOffset`, comme l'accroupissement du moteur) et part de l'offset mémorisé pour la position debout.
- **Défaut voisin corrigé** : quand la balle qui tue arrache aussi la première jambe, le corps (mouvement coupé à la mort) passait en reptation et flottait 25 cm au-dessus du sol (vu à 18:15:44 : `dead … at thigh_r (Plain)` puis `thigh_r torn off`). Le corps garde désormais la posture de sa mort (`FFortSoldierActionRep::bDiedCrawling`, répliqué).
- **Prudence** : un corps couché dépasse d'environ 1 m la boîte de rendu d'un soldat debout (échelle 1,8). `DA_FortSoldierRules.LyingBoundsScale` (2,6 ; 0 = inchangé) l'agrandit pour un soldat qui rampe ou qui est mort, sur chaque machine. Hypothèse non mesurée (disparition en bord d'écran) ; coût : LOD un peu plus fins pour les corps.

**Gardes des bâtiments** (« plus de soldats dans les bâtiments »)
- `AFortGarrisonZone.BuildingPosts` (`FFortBuildingPost` : position et cap dans le repère de la zone, bâtiment) : postes **à l'intérieur** de la caserne, du corps de garde, du logis du gouverneur (rez, étage, galerie), du couloir de la prison, de la tour de guet, de la chapelle, de la cuisine, de la salle des coffres et de l'enclos de la poudrière. Ils sont trouvés par `scripts/editor/fort_building_posts_ue.py` (sol praticable à la hauteur de la maquette, toit, place pour une capsule de 45 cm, dans l'enceinte, 1,5 m d'écart, de préférence en vue de la porte).
- Après la garnison du jour (cour et remparts, `DA_RunRules`), la zone poste `DA_FortSoldierRules.BuildingGuardsByDay` gardes (jours 1-3 : 4 ; 4-6 : 5 ; 7-10 : 6 ; `BuildingGuardsWithoutRun` 4 sans campagne), un par poste au plus, répartis d'abord un par bâtiment (bâtiment de départ tiré au sort), uniformes des soldats placés, le fort plafonné à `MaxSoldiersPerFort` (18). Journal : « day 3 wants 4 guard(s) in the buildings: 4 posted on N post(s) (barracks 1, …) ».
- Un garde tient son bâtiment : debout à son poste, face à la porte (vers l'extérieur sur une galerie ou une terrasse) ; il ne s'éloigne pas à plus de `BuildingHoldRadiusCm` (6 m) de son poste ; il ne répond à l'alarme du fort que si l'ennemi est à moins de `BuildingAlarmRadiusCm` (15 m) ; il chasse ce qu'il voit, entend ou ce qui le blesse, puis retourne à son poste.
- Effectif : jour 3, 7 → 11 soldats par fort ; jour 7, 10 → 16 ; jour 10, 12 → 18 (plafond). Trois forts au jour 10 : 54 soldats au plus contre 36. Coût par soldat non mesuré : personnage complet (mouvement, IA, squelette de 84 à 97 k triangles en LOD0, pose évaluée à chaque image même hors champ, `AlwaysTickPoseAndRefreshBones`), 30 mises à jour réseau par seconde à moins de 150 m. Les gardes immobiles ne changent presque rien à répliquer. À mesurer (`stat game`, `stat anim`) au jour 10 dans un fort.
- `GoToNextPost` cherche un poste libre dans le registre de la garnison au lieu de parcourir tous les soldats du monde.

**Fort qui apparaît sur Raraka** (journal du build v2.0 joué le 2026-10-11 vers 19 h, `Lvl_Raraka`, fragments 16 et 19)
- À chaque apparition du fort : `LogNavigation: Warning: NavData RegistrationFailed_DataPendingKill (RecastNavMesh-Default)`, puis 50 « is stuck and failed to move » de soldats du fort en 2 min. Le build v1.4 (2026-10-08) écrivait au même moment « SpawnMissingNavigationData … Spawning and rebuilding at runtime » : le navmesh du fort se construisait.
- Cause probable : `LI_FortFrench_Land.umap` (enregistré le 2026-10-08 à 19:22) contient un `RecastNavMesh-Default` (chaîne trouvée dans le paquet ; `fort_land_ue.py` l'avait retiré). Le moteur jette un navmesh enregistré dans un niveau chargé en flux (`ARecastNavMesh::CheckToDiscardSubLevelNavData`) et aucun navmesh n'est construit : sans chemin, les soldats marchent tout droit dans les murs.
- Correctif : étape `navdata` du script éditeur (le navmesh enregistré dans le niveau du fort est supprimé, le volume `FortFR_NavBounds` reste, vérification après l'enregistrement). Ne jamais réenregistrer `LI_FortFrench_Land` avec un `RecastNavMesh`.
- Autres constats : sur `L_Test_Flat`, les 25 sondes du sol ratent toujours (« 0 of 25 probes hit ») ; la hauteur de repli (Z = 0 de l'emplacement) tombe juste sur cette carte. Le niveau du fort arrive environ 3 s après le chargement de la carte (flux asynchrone) : le terrain dégagé se voit avant le fort.

**Tests** : `monke_island.Fort.Garrison` (gardes du jour, plafond, répartition, alarme, maintien, disque, valeurs par défaut), `monke_island.Fort.SoldierGore` (posture du mort, hauteur du maillage), `monke_island.Fort.Corpse` (boîte d'un corps couché). Écrits et compilés en fichier seul, non exécutés.
