# Temple reliquaire (T-179, T-180, T-187)

| Champ | Valeur |
|---|---|
| Lot | L3 (monde, point d'intérêt) |
| Demande | Propriétaire, 2026-10-06 : au moins un temple par île jouée, au milieu de l'île entre les marae et les villages, apparu en jeu, arbres et buissons dégagés s'il le faut. |
| Maquette | `OneDrive\maquette-blender\maquettes\temple-reliquaire` (22 modules, 62 placements) et `artefacts-polynesiens` (6 artefacts légendaires), importés par Codex (T-179) dans `/Game/MonkeIsland/Environment/TempleReliquary/` et `/Game/MonkeIsland/Items/PolynesianArtifacts/` |
| Données | `DA_TempleReliquaryRules` (`UMaraeRules`) : modules, artefacts, dégagement, placement. Créé par `scripts/map/testflat_sites_ue.py` (T-187) et réglé par `scripts/map/temple_spawn_setup_ue.py` (T-180). |
| Code | `AMaraeSite` le construit (comme le marae des perles) ; `ACampSpawner::SpawnTemples` le place ; règles pures dans `Marae/SitePlacementMath.h` |

## Le temple
- Les 62 placements de la maquette sont repris dans le repère des FBX des modules (x, -y, z ; cap = -rot_z) :
  - parvis, autel aux reliques, ahu ;
  - portique, fare ia manaha, fata ;
  - murets, palissade, tiki, pierres levées, débris, torches.
- Sur les socles de l'autel, quatre artefacts légendaires portables (`APhysicsPropActor`) : `BP_Artifact_HeartOfTheReef` (6 000, 18 kg), `FishGodFragment` (4 500), `EclipseStone` (3 500), `CrownOfTheLastAriki` (2 200). Les deux perles (2 cm) sont laissées de côté.
- `AMaraeSite` construit les pièces sur chaque machine et les artefacts sur le serveur (répliqués). Il dégage la végétation sur 19 m et pose une zone calme de 19 m : pas de tiki, de totem, de camp ni de butin.

## Placement dans un fragment (`ACampSpawner`, serveur)
1. Les candidats sont les emplacements de camp du fragment (`DA_CampRules.FragmentSpots`, `scripts/map/place_camps.py`). Ce sont des terres sûres, déjà écartées de tout village ou hameau (rayon de zone + 40 m). Les marae y sont tirés d'abord.
2. Le temple prend l'emplacement libre le plus proche du **milieu des terres du fragment**, c'est-à-dire de la moyenne des emplacements (`bCentral`). Il doit être :
   - à 120 m au moins des marae (`MinDistanceFromSites`) ;
   - à 150 m au moins des joueurs au départ (`MinDistanceFromPlayers`) : on le trouve en explorant ;
   - hors des zones calmes ;
   - sur un sol de terre au-dessus de la plage (`MinGroundAboveSea` 80 cm) et plat (`MaxGroundDelta` 1,5 m). Le sol est testé par 9 tirs, au centre et à 14 m autour (`GroundCheckRadius`), à travers la végétation.
3. Sa porte (axe +Y du site) regarde les joueurs à l'arrivée (`bFacePlayers`).
4. **Un temple au moins par fragment** : si aucun emplacement ne passe la première passe, la seconde prend l'emplacement le plus central encore libre à 60 m des marae, sans tester le sol.
5. Journal : « reliquary temple at …, spot N of M by distance to the middle of the land », puis « N fishermen camps, N marae and N reliquary temples ».

## Saisir un artefact (T-203)
Un joueur qui saisit un artefact posé par le temple (F maintenu) fait tourner la météo en 10 s : vent violent, pluie, ciel sombre. Le temple est le propriétaire de ses artefacts (`AMaraeSite::SpawnActors`) : les mêmes artefacts dans les sanctuaires des villages ne déclenchent rien. Voir `storm-flood.md`, section « Météo qui tourne », et `DA_WeatherRules`.

## État
- **T-187** (fait, review) : temple et marae des perles posés dans deux zones éloignées de l'île Tuamotu de L_Test_Flat. Simulation du 2026-10-06 : 62 pièces, 4 artefacts, 203 plantes dégagées.
- **T-180** (en cours) : C++ compilé (cible jeu) ; specs `monke_island.Marae.Placement`. Restent :
  - le build éditeur ;
  - le réglage de `LI_ExpeditionCore` ;
  - un PIE sur deux fragments avec captures.
  - `Lvl_Raraka` (110 Mo) garde son spawner sans temple tant qu'on ne l'ouvre pas.
- À équilibrer (game design) : un temple porte 16 200 de valeur d'artefacts. Les sanctuaires des villages (T-189) en portent un de plus chacun. Les quotas et la taxe (cahier §7) sont à revoir avec ces valeurs.
