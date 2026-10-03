# Villages japonais : les zones de pacification de Raraka

| Champ | Valeur |
|---|---|
| Lot | L3, zones pacifiables (cahier §6.2) : la première île en compte trois. Chaque village est une zone ; les PNJ y apparaissent. |
| Tâche | T-064 |
| Kit (Blender) | `scripts/props/village_japonais.py` (minka : `scripts/props/minka/`, objets : `scripts/props/village/props.py`) → `art/blender/props/village_japonais.blend`, exports `art/export/Props/Village/` |
| Générateur | `scripts/map/place_villages.py` (mémoire : `docs/reference/map/raraka/villages/villages.json`) |
| Dégagement de la végétation | `scripts/map/village_clearing.py`, branché sur `place_vegetation.py` et `palms_to_datatable.py` |
| Cible UE | `/Game/MonkeIsland/Environment/Village/` (`Meshes`, `Materials`, `Textures`, `Data`), graphe `/Game/MonkeIsland/Environment/PCG/PCG_Raraka_Village`, outliner `Raraka/Villages` |

## Le kit
Pas d'arbres, de haies ni de rizières : sur l'île, les villages vivent dans la cocoteraie et les fourrés déjà en place.
- `SM_Minka` : la minka entière en un seul mesh (37,7 k triangles), porte d'entrée ouverte à 65 %, mobilier intérieur. Elle a 67 collisions `UCX` et un socle de terre enterré de 0,8 m sous le corps et l'engawa. Sur un sol irrégulier, la maison est posée sur le point le plus haut de son emprise ; le socle comble l'écart.
- `SM_Village_*` : `Kura` (entrepôt), `Well` (puits), `NoticeBoard` (panneau), `FireTower` (tour de guet), `Torii`, `Shrine` (sanctuaire), `StoneLantern` (lanterne de pierre), `BambooFence` (clôture de 2 m).
  - De 96 à 1 056 triangles chacun.
  - Socles et poteaux descendent de 0,3 à 0,6 m sous le pivot.
  - Collisions `UCX` en boîtes. Le torii n'a que ses poteaux : on passe dessous.
- Matériaux : 41 matériaux Blender, tous sur des textures CC0 (Poly Haven, crédits dans `art/SOURCES.md`).
  - `python scripts/props/pack_ue_textures.py` en tire 120 textures Unreal dans `art/export/Props/Village/Textures/` : couleur (sRGB), normale (DirectX) et ORM (occlusion, rugosité, métal).
  - Limites : 1024 px pour les grandes surfaces, 512 px pour les petits objets, 8 px pour les couleurs unies.
  - `ue_materials.json` associe chaque emplacement de matériau à son MI.

```
blender --background --factory-startup --python scripts/props/village_japonais.py -- --render --export
python scripts/props/pack_ue_textures.py
```
- Depuis le 2026-10-03, Blender ne tourne plus sur ce PC (CLAUDE.md). On relance l'export sur le 2e PC, puis on récupère les fichiers dans `C:\Users\matth\OneDrive\maquette-blender`.
- Piège corrigé dans `export_group`, qui sert aussi à la minka : l'objet exporté doit porter exactement le nom du mesh. Sinon Blender l'appelle `<nom>.001`, Unreal ne rattache plus ses `UCX_<nom>_NN` et génère une collision automatique (une enveloppe autour de tout l'objet, torii compris).

## Le générateur
```
python scripts/map/place_villages.py raraka [--count 3] [--seed 7] [--reset] [--regen V02]
       [--min-dist 250] [--max-dist 2000] [--spacing 350] [--max-houses 12] [--reclear] [--apply-clearing]
```
1. **Chunks** : l'île est découpée en carrés de 256 m. Dans chaque chunk, les meilleurs centres possibles doivent remplir ces conditions :
   - terre à plus de 24 m de l'eau ;
   - à plus de 250 m des bâtiments réels (Motutapu reste intact) ;
   - à plus de 60 m des acteurs du niveau ;
   - entre 250 m et 2 km du PlayerStart.

   Le générateur essaie un village complet sur chacun, puis note le chunk. Une maison vaut +10 et un sanctuaire +12. Chaque tronc à couper retire 0,6, et chaque mètre au-delà de 600 m du départ retire 0,015.
2. **Choix** : les meilleurs chunks sont retenus, à au moins 350 m les uns des autres.
3. **Plan d'un village**, adapté à son site :
   - l'axe suit le grand axe du motu ;
   - la route en terre suit la crête du motu, à 12 m de l'eau au moins, avec un virage limité à 10° par pas et un écart de 30° au plus par rapport à l'axe ;
   - la place est du côté le plus profond : puits, panneau, tour de guet, lanternes, et au centre le drapeau de la zone ;
   - derrière la place, l'allée mène au torii puis au sanctuaire ; s'il n'y a pas la place, le sanctuaire va au bout de la route ;
   - jusqu'à 16 parcelles bordent la route, 4 de chaque côté et dans chaque sens. Chaque minka fait face à la route, avec une clôture, une lanterne au portail et un kura derrière. Le kura est sous chaque maison au village des vivres, sous une sur deux ailleurs.
4. **Adaptation au terrain** : chaque parcelle peut glisser de ±6 m le long de la route et reculer de 4 m. Elle prend la position qui coupe le moins de troncs. Une parcelle est abandonnée dans ces cas :
   - trop près de l'eau ;
   - pente trop forte (écart de hauteur supérieur à 0,7 m pour une minka) ;
   - trop près d'un bâtiment réel ou d'un acteur ;
   - chevauchement avec une autre construction.

   Le village prend donc la taille que son site permet ; il faut au moins 4 maisons.
5. **Thèmes** (§6.2, « chaque zone ouvre un intérêt concret ») :
   - **Takara-mura** (richesses et crique d'extraction) : le village le plus proche d'une crique du lagon ;
   - **Mori-mura** (bois et passage court) : le site le plus boisé ;
   - **Kura-mura** (nourriture et abri) : un kura par maison.
6. **Ce qui gêne est retiré**, le reste demeure :
   - on retire tout ce qui est sous les constructions (avant-toits compris), sur la place, sur la route et sur l'allée ;
   - on retire aussi ce qui les toucherait, avec une marge qui dépend de la plante (`village_clearing.MARGIN_CM`) : autour d'une maison, 3 m pour un buisson ou un cocotier, 3,5 m pour un jeune cocotier, 5 m pour un cocotier couché, 1 m pour une fougère ; autour d'une clôture, 2,4 m pour un buisson, ce qui garde les cours avant nettes ;
   - tous les autres cocotiers et buissons restent en place, autour des maisons.

### Mémoire (`villages.json`)
- Le fichier garde, pour chaque chunk évalué, son score et ce qu'il peut accueillir.
- Pour chaque village, il garde :
  - l'identifiant, le nom, le thème, le chunk et la graine ;
  - le centre, l'axe et la zone (centre, rayon, drapeau) ;
  - les objets (asset, position en cm UE, cap) ;
  - les points de spawn et la route ;
  - les formes de dégagement.
- Un nouveau lancement garde les villages connus tels quels. Il ne remplit que les chunks libres jusqu'à `--count`. `--regen V02` reconstruit un village, `--reset` repart de zéro, `--reclear` recalcule le dégagement des villages connus sans rien déplacer.

### Dégagement de la végétation
- `--apply-clearing` réécrit les CSV de végétation sans les instances situées sous les villages. Les blocs k-d gardent leurs limites : seules les DataTables touchées changent.
- Les copies complètes restent dans `raw/raraka_vegetation/_orig/`. Supprimer les villages rend la végétation d'origine.
- `place_vegetation.py` et `palms_to_datatable.py` appliquent le même dégagement : une nouvelle génération de la végétation laisse les villages dégagés.
- Le rapport `villages/clearing_report.json` donne les DataTables à réimporter. Au 2026-10-03, avec les marges par plante, 1 201 instances sont retirées : Palms 114, Hard_00 79, Soft_00 828, Soft_05 180. La première version, avec une marge fixe de 1,5 m, laissait les naupaka (jusqu'à 4,8 m de large) toucher les murs.

## Résultat sur Raraka (graine 7)
| Id | Village | Thème | Chunk | Distance au départ | Maisons | Kura | Objets | Spawns | Retirés (troncs) |
|---|---|---|---|---|---|---|---|---|---|
| V01 | Kura-mura | Nourriture et abri | 34_9 | 918 m | 12 | 12 | 118 | 36 | 322 (16) |
| V02 | Mori-mura | Bois et passage court | 28_12 | 935 m | 12 | 6 | 111 | 36 | 275 (19) |
| V03 | Takara-mura | Richesses et crique d'extraction | 33_9 | 556 m | 12 | 6 | 112 | 36 | 604 (32) |

V01 et V03 sont sur le motu au nord-est de Motutapu, V02 sur le motu au sud-ouest, de l'autre côté de la passe. Carte de contrôle : `docs/reference/map/raraka/villages/preview_villages.png`.

## Points de spawn
Ils sont rangés par village dans `villages.json` (`spawns`) et repris dans Unreal en TargetPoints tagués.
- Tags communs : `NpcSpawn`, `Village.<Id>`.
- Le rôle s'ajoute en tag :
  - `villager` : devant le doma et l'engawa de chaque maison ;
  - `elder` : sur la place ;
  - `priest` : au sanctuaire ;
  - `guardian` : menaces désignées de la zone (§6.2), aux bouts de la route et en lisière.
- Le centre et le rayon de la zone sont marqués par un `TriggerSphere` tagué `PacificationZone`, en attendant `BP_PacificationZone` (T-054).

## Dans Unreal (fait le 2026-10-03)
- **Kit** :
  - `/Game/MonkeIsland/Environment/Village/` : 9 meshes Nanite, 40 MI de `M_Prop_Atlas`, 120 textures.
  - Collisions `UCX` : 67 pour la minka ; 3 pour le puits, 4 pour la tour, 2 poteaux pour le torii.
- **Placement** :
  - `PCG_Raraka_Village` : copie de `PCG_Raraka_VegHard`, avec `bUseDefaultCollision` (BlockAll + UCX, bloque aussi la caméra).
  - 3 acteurs `PCG_Raraka_Village_Vxx` (outliner `Raraka/Villages/<Id>_<Nom>`), en régénération manuelle.
  - Chacun lit son `DT_Raraka_Village_Vxx` (`/Game/MonkeIsland/Environment/Village/Data/`).
- **Marqueurs** :
  - `PacificationZone_Vxx` : TriggerSphere au rayon de la zone, tags `PacificationZone`, `Village.Vxx`, `Theme.<thème>`.
  - `ZoneFlag_Vxx`.
  - 108 `NpcSpawn_*` (TargetPoints, rangés dans `.../Spawns`).
  - Tous portent le tag `VillageMarker`.
- **Végétation** :
  - DataTables Palms, Hard_00, Soft_00 et Soft_05 synchronisées sur place (lignes retirées et rendues, sans suppression d'asset), puis blocs régénérés.
  - Au 2026-10-03, 1 201 instances sont retirées avec les marges par plante (`village_clearing.MARGIN_CM`).
- **PNJ et mobs** (`TikiDirector`, voir `docs/features/characters-tiki.md`) :
  - les villageois n'apparaissent que sur les spawns des villages ;
  - les gardiens (rôle `guardian`) bondissent des buissons proches.

Piège : supprimer puis réimporter une DataTable encore référencée par un acteur PCG l'efface du disque mais la laisse en mémoire, et l'import échoue (« already exists »). Il faut la mettre à jour sur place : `remove_rows`, puis `add_rows` + `set_rows` avec les lignes calculées hors ligne.

## Vie du village (T-064, demande du propriétaire)
- **Points d'intérêt** (`place_villages.py --extras`) : 33 par village, posés en TargetPoints `VillagePOI` dans `Raraka/Villages/<Id>/POI`.
  - Puits : `work`. Panneau : `read`. Tour de guet : `watch`. Sanctuaire : `pray` ×2.
  - Devant chaque minka, hors de la clôture, côté route : `work` au portail, `rest` face à la route devant l'engawa.
  - Place : 4 points `chat` autour du drapeau.
  - Les villageois avancent en ligne droite, sans NavMesh. Le générateur ne garde donc que les points atteignables en ligne droite depuis la route ou la place, en testant les emprises au sol (`poi_reachable`, marge 0,45 m). Il écarte ainsi les cours fermées par la clôture et les portes de kura derrière les maisons.
  - Amélioration prévue : des points de passage (portail, côté de la maison) pour entrer dans les cours.
  - Vérifié en PIE le 2026-10-03 (journal « <villageois> : chat/read/rest for N s at point P ») : un villageois bavarde sur la place, un autre lit le panneau puis se repose devant une maison.
- **Lumières** (`VillageLight`, `Raraka/Villages/<Id>/Lights`), 29 à 30 par village :
  - une lampe au-dessus de l'irori de chaque maison (25 cd, 6,5 m, avec ombres) ;
  - une lueur dans chaque lanterne de pierre (6 cd, 4,5 m, sans ombres).
  - Le papier de l'andon et les braises de l'irori sont émissifs (`M_Village_Glow`, MI `LanternGlow` et `Ember`).
- **Ambiance** (`AVillageAmbienceActor`, un par village, `DA_VillageAmbience`) : cosmétique, calculée sur chaque machine, non répliquée, coupée sur serveur dédié.
  - Les lampes suivent la lumière du jour : intérieur à 35 % le jour, lanternes éteintes, plein feu la nuit.
  - 14 papillons (`SM_Village_Butterfly`, texture procédurale façon monarque) volettent le jour, 24 lucioles s'allument la nuit.
  - Ils ne bougent que si une caméra locale est à moins de 180 m.
  - Assets : `python scripts/props/village_ambience_assets.py` (OBJ et PNG écrits sans Blender).

## Reste à faire
- [ ] `BP_PacificationZone` (T-054) à la place des marqueurs de zone : drapeau sur `ZoneFlag_Vxx`, menaces désignées = gardiens du village.
- [ ] Mesurer le coût des 12 villageois et 9 gardiens, qui tickent en continu, avant la gate PROD.
- [ ] Chemin de terre visible : la route n'est aujourd'hui qu'un dégagement de la végétation ; il faudra un decal ou une couche du matériau de terrain.
- [ ] Distances : V01 et V02 sont à plus de 900 m du départ. Ce sera à juger en jeu avec la barque (§6.3), en réglant `--max-dist`.
- [ ] Villages des fragments (T-102) : PIE hôte + client (le client doit construire les mêmes villages par `OnRep_ActiveFragment`), partie lancée depuis `L_Boot`, coût serveur des 36 à 66 villageois d'un fragment.
- [ ] Les villages V01-V03 de Motutapu (acteurs PCG et marqueurs de `Lvl_Raraka`) ne servent plus que hors fragment : à retirer ou à garder pour le hub, à décider.

## Villages des fragments (T-102)
Depuis T-094/T-095, une partie se joue dans **un fragment tiré au hasard** (`FRG_Raraka_NN`, départ en barque à son arrivée). Les trois villages de Motutapu (V01-V03) sont hors de tous les fragments : le joueur n'en voyait aucun. Chaque fragment a désormais un village sur chacun de ses trois sites (`sites.villages` Z1-Z3 de `docs/reference/map/raraka/fragments/fragments.json`), un par zone pacifiable (`ZoneCount = 3`).
- **Génération** (hors ligne) : `python scripts/map/fragment_villages.py raraka [--fragment FRG_Raraka_19] [--regen]`.
  - Il reprend le plan de `place_villages.py` (route, place, sanctuaire, parcelles, points d'intérêt, lumières, portes, formes de dégagement).
  - Chaque site est essayé depuis son centre et depuis 4 points à 5 m, avec l'axe du site et celui du motu. Il garde le meilleur plan dont la place reste à **20 m au plus du centre du site**. Les camps de monk-island-19 restent à 120 m des centres de site.
  - Les marae du fragment (`sites.marae`) sont tenus libres sur 22 m.
  - Thèmes dans chaque fragment : richesses côté lagon au plus près du rivage, bois là où la cocoteraie est la plus dense, vivres pour le troisième. Les noms sont fictifs, tirés du thème et du numéro du fragment (Kogane-mura, Take-mura, Uo-mura…).
  - Un village de zone remplit toutes les parcelles que son site permet, jusqu'à 16 (`ZONE_MAX_HOUSES`) : l'utilisateur trouvait qu'il n'y avait pas assez de villages.
  - **Hameaux** (`kind: hameau`, ids `Fxx_H1..3`, noms en `-sato`) : jusqu'à 3 par fragment, 4 à 6 maisons, sur la terre large qui reste à 220 m au moins des autres villages et à 120 m des sites de marae. Ils ne sont pas des zones pacifiables (`bZone = false`) : pas de drapeau, pas de gardiens, 3 à 6 villageois (`villager_count`).
  - Résultat au 2026-10-04 : 60 villages de zone (927 maisons, 15,5 en moyenne) et 44 hameaux (254 maisons). Places des villages de zone à 20 m au plus de leur site.
  - Les camps de monk-island-19 (`scripts/map/place_camps.py`) excluent tout village de l'index, hameaux compris (rayon de zone + 40 m), et ses marae prennent des emplacements de camp.
- **Mémoire** (suivie) : `docs/reference/map/raraka/villages/fragments/<FRG>.json`, même format que `villages.json`, plus `fragment`, `site` et `arrival_cm`. `index.json` résume les centres, les rayons, les thèmes et les noms. Aperçus : `preview_<FRG>.png` (le point cyan est l'arrivée de la barque).
- **Rien n'est posé dans la carte**, à part un acteur `BP_VillageSpawner` (`AVillageSpawner`, dossier `Raraka/Villages`).
  - Le serveur choisit le fragment une fois, dans cet ordre :
    1. l'option `Island=` de l'URL quand elle y est (partie lancée depuis `L_Boot`) ;
    2. sinon, le fragment de l'arrivée qu'a prise la barque de départ (`UBoatStartSubsystem::GetArrivalFragmentId`) : c'est le cas d'un PIE ouvert directement sur `Lvl_Raraka`, où le fragment annoncé par le GameState peut différer de celui de la barque ;
    3. sinon, le fragment dont l'arrivée est la plus proche d'un joueur, à 1 km au plus (`ArrivalMatchRadius`).
  - Le choix est répliqué (`ActiveFragment`), et chaque machine construit les mêmes villages :
    - maisons et objets en instanced meshes avec leurs `UCX` (BlockAll, comme `PCG_Raraka_Village`) ;
    - lumières et `AVillageAmbienceActor`, sauf sur serveur dédié ;
    - végétation dégagée au lancement sous les villages, avec les formes et les marges par plante de `village_clearing.py` (tables dans `DA_VillageRules`), seulement sur les meshes de `/Game/MonkeIsland/Environment/Vegetation/` et `/Props/`. Les instances sont **déplacées de 1 km sous terre** (`HiddenDepth`), pas supprimées : leurs indices ne bougent pas, car `ACoconutFallDirector` compte les noix par indice d'instance (même technique que `FSiteClearing` des camps). `TikiDirector::CacheBushes` ignore les buissons sous `BushMinZ` (-100 m).
  - Sans joueur près d'une arrivée au bout de 20 s, aucun village de fragment n'est construit : les villages de Motutapu restent.
- **PNJ** : `ATikiDirector` remplace les villages des marqueurs par ceux du fragment (`OnVillagesBuilt`) : spawns, points d'intérêt, portes, gardiens. Il relit les buissons, puisque la végétation sous les villages a disparu.
- **Unreal** :
  - `DA_VillageRules` et `DA_Villages_FRG_Raraka_01..20` (`/Game/MonkeIsland/Environment/Village/Data/`), `BP_VillageSpawner` (`.../Village/Blueprints/`) ;
  - remplis par `python scripts/map/fragment_villages_ue.py raraka [--place]` via le MCP. Les points ne passent pas par la conversation : 2 Mo pour les 60 villages.
- **Tests** : `monke_island.Village.Fragments` (choix du fragment, formes de dégagement, plantes et marges, portails).
- **Vérifié en PIE le 2026-10-04** (compilé + testé éditeur) :
  - FRG_Raraka_01 (PIE de monk-island-19) : 6 villages, 623 instances, 166 lumières, 2 233 plantes dégagées en 29 ms, avec 2 marae et 8 camps de 19 ;
  - FRG_Raraka_16 : 4 villages, 495 instances, 128 lumières, 3 208 plantes en 30 ms ; TikiDirector remplace les villages des marqueurs, gardiens sortis autour des 3 zones, pas autour du hameau. Capture : `docs/production/captures/T-102/pie_FRG_Raraka_16_Kashi-mura.png`.
  - Non testé : multijoueur (réplication de `ActiveFragment` vers un client), partie depuis `L_Boot`.

## Portes des maisons (T-077)
- `place_villages.py --extras` calcule pour chaque minka la porte du doma : locale (0,9 ; 0,6), devant la façade, à gauche de l'engawa. Il calcule aussi le portail de sa palissade : locale (0,35 ; 5,7), dehors.
- Il les garde dans `villages.json` (`doors` : nom, maison, position, cap vers le portail, distance en cm).
- Dans `Lvl_Raraka`, 36 TargetPoints `Door_Vxx_PorteNN` (dossier `Raraka/Villages/<village>/Doors`) portent les tags `VillageDoor`, `Village.Vxx`, `Area.maison_NN` et `Gate.513`. Leur avant regarde le portail.
- Quand un villageois est tué, les autres rentrent chez eux par là et s'y cachent (`docs/features/characters-tiki.md`).
