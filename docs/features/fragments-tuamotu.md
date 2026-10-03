# Fragments des Tuamotu : îles d'expédition découpées par la mer

| Champ | Valeur |
|---|---|
| Lots | L3 : pipeline + `L_Frag_Raraka_01` jouable · L4 : choix au hub · L8 : fragments 2 à N |
| Décision | `docs/adr/ADR-004-fragments-tuamotu.md` |
| Tâches | T-081 → T-092 (feature `fragments`) |
| Scripts | `scripts/map/fragments/` (`find_fragments.py`, `check_fragments.py`, `rules.json`) ; esquisse `scripts/map/fragment_candidates.py` |
| Données | `docs/reference/map/raraka/fragments/` (`fragments.json`, aperçus, georef de chaque fragment) |
| Cible UE | `/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_NN`, `/Game/MonkeIsland/Environment/Fragments/<FRG>/`, `LI_ExpeditionCore` |

## Intention
Au lieu de lâcher le joueur sur les 29 km de l'atoll, chaque expédition se joue sur un **morceau** de Raraka : quelques motu, un village par zone de pacification, un marae, de la cocoteraie, du fourré, des plages, un platier, des hoa et le lagon. Le morceau est **entouré d'eau** et le reste de l'atoll se devine à l'horizon. On ne doit jamais voir une coupure : ni motu tranché, ni mur, ni bord de terrain.

## Ce que disent les données (T-082, 2026-10-03)
Commandes (depuis la racine du dépôt) :

```
python scripts/map/fragments/find_fragments.py raraka [--seed 7] [--reset] [--regen FRG_Raraka_NN]
python scripts/map/fragments/check_fragments.py raraka        # code de sortie 1 en cas d'échec
```

Données lues : si le checkout n'a que des pointeurs LFS pour les PNG et les geojson, le script les lit dans l'arbre principal, ou dans le dossier passé par `--data-root`.

Paramètres : `scripts/map/fragments/rules.json`. Aucune valeur n'est en dur.

Sorties, dans `docs/reference/map/raraka/fragments/` :
- `fragments.json` : la mémoire ;
- `preview_fragments.png` : l'atoll entier ;
- `preview_FRG_Raraka_NN.png` : une vignette par fragment.

**Méthode**
- **Axe de l'anneau.** On prend le rayon médian de la terre et du platier par secteur angulaire autour du centre du lagon, puis on le lisse. L'axe mesure 71,7 km. Chaque point reçoit une abscisse `s` le long de l'anneau. L'origine est au milieu du plus long tronçon sans terre.
- **Coupes naturelles.** Les terres séparées par moins de 60 m d'eau forment un même groupe : on en compte 102. Deux groupes sont donc toujours séparés par au moins 60 m d'eau (hoa, lagon ou océan).
- **Coupes synthétiques.** Dans les grands groupes, on cherche les cous du profil de terre le long de `s`, c'est-à-dire les endroits où un hoa de 60 m retire le moins de terre. On en retient 62.
  - Un cou n'est gardé que si ses deux rives restent à au moins 40 m l'une de l'autre.
  - Plusieurs de ces coupes ne font qu'élargir un hoa existant de moins de 60 m : elles ne creusent que 25 à 50 m de terre.
- **Segments.** Un segment est une suite de morceaux consécutifs le long de l'anneau :
  - 1 à 2,5 km de long ;
  - 15 à 80 ha de terre ;
  - au plus 600 m d'eau entre deux morceaux (fragments archipel).

  Les 164 morceaux donnent 233 segments jouables.
- **Sites de village.** On reprend les critères de `place_villages.py`, évalués par une approximation raster rapide :
  - le centre est à plus de 24 m de l'eau ;
  - la route est à plus de 12 m de l'eau ;
  - au moins 4 minka tiennent sur les lots à 15, 37, 59 et 81 m, chacune à 10 m de l'eau et avec 0,7 m de dénivelé au plus ;
  - l'axe du motu est estimé par tenseur de structure.

  Il faut 3 sites, espacés de 350 m.
- **Site de marae.** Sable (masque G ≥ 0,85), dénivelé < 0,6 m sur 23 m, à plus de 150 m des villages, côté lagon de préférence.
- **Criques.** Eau côté lagon entourée de terre : au moins 36 % de terre dans une fenêtre de 120 m.
- **Mouillages.** Au moins 0,6 m d'eau côté lagon, au plus près du rivage.
- **Note d'un segment.**
  - Elle compte la capacité des villages, le marae, les criques, les mouillages, la part de cocoteraie (DAG 2023) et la part de fourré (masque R > G).
  - Pénalités : chaque coupe synthétique, la terre creusée, l'écart à la taille cible (40 ha, 1,6 km), chaque groupe de motu en plus (passage à gué).
  - Bonus : une arrivée de barque côté lagon avec une approche courte.
- **Sélection.** Méthode gloutonne par note, sans recouvrement. Un segment sans arrivée de barque valide est écarté (5 cas).
- **Exclusions.**

  | Secteur | Bâti | Décision |
  |---|---|---|
  | S1 Motutapu | 100 bâtiments | aucune terre gardée à moins de 500 m du bâti (5 morceaux interdits) |
  | S2 | 9 cabanes légères, 359 m² au sol | exclusion locale de 50 m autour du bâti : ni village ni marae, la terre reste jouable |
  | S3 | 5 cabanes légères, 117 m² au sol | idem |
  | S4 | 3 cabanes légères, 107 m² au sol | idem |
  | S5 | 2 cabanes légères, 116 m² au sol | idem |

  S2 à S5 sont des cabanes isolées de la BD TOPO, à 15–20 km de Motutapu. Ce sont probablement des campements de coprah, pas des villages. Je n'ai pas revu les orthos une par une.
- **Arrivée de la barque** (`arrival`). C'est une demande du propriétaire : le jeu commence dans la barque, sur l'eau.
  - On lance des rayons depuis chaque point de plage du fragment, en éventail tous les 10°.
  - Le point d'arrivée est à 200–350 m du rivage du fragment et à au moins 300 m des autres motu.
  - Sur la ligne droite jusqu'à la plage : ni terre ni pinacle. Au-delà de l'**approche finale**, il faut au moins 1,5 m d'eau et pas de platier.
  - L'approche finale est la plus courte possible parmi 35, 60, 90 et 120 m. La valeur retenue est écrite dans `arrival.final_approach_m`.
  - La plage visée est toujours à plus de 110 m d'un hoa synthétique, car ce hoa n'existe pas encore.
  - La proue (`yaw_deg`, de +X vers +Y) est orientée vers la plage, et z = 0.
- **Mémoire.** Un fragment écrit est gelé : une relance sans `--reset` ne le renumérote pas et ne le déplace pas. Vérifié : la relance donne un fichier identique octet pour octet, et `--regen FRG_Raraka_05` rend le même fragment sous le même identifiant. Un identifiant retiré n'est jamais réutilisé.

**Résultat : 20 fragments, contrôle OK** (`check_fragments.py raraka`, 0 échec). Seul avertissement : 9 arrivées sont côté océan, faute de place côté lagon. Le contrôle a aussi été testé sur une copie corrompue (coupe synthétique non déclarée, village manquant, arrivée déplacée) : il sort en échec (code 1).

| Id | Type | ha | km | Groupes | Coupes naturelles (eau, m) | Coupes synthétiques (terre, m) | Villages (minka est.) | Marae | Criques | Mouill. | Arrivée | Coco / fourré % | Note |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **FRG_Raraka_01** | naturel | 52 | 2,21 | 1 | 2 (230, 115) | 0 | 3 (12/12/12) | 3 | 6 | 3 | lagon 274 m, appr. 90 m | 22 / 26 | 44,1 |
| FRG_Raraka_02 | archipel | 21 | 1,80 | 4 | 2 (188, 76) | 0 | 3 (12/12/12) | 2 | 7 | 3 | lagon 275 m, appr. 120 m | 45 / 26 | 44,4 |
| FRG_Raraka_03 | naturel | 42 | 1,85 | 1 | 2 (113, 302) | 0 | 3 (12/12/12) | 3 | 8 | 3 | océan 275 m, appr. 120 m | 11 / 33 | 40,6 |
| FRG_Raraka_04 | motu scindé | 47 | 1,65 | 1 | 1 (188) | 1 (221) | 3 (12/12/12) | 3 | 6 | 3 | lagon 292 m, appr. 90 m | 32 / 32 | 39,8 |
| FRG_Raraka_05 | naturel | 24 | 1,70 | 2 | 2 (83, 106) | 0 | 3 (12/12/12) | 2 | 5 | 2 | océan 275 m, appr. 35 m | 0 / 34 | 36,2 |
| FRG_Raraka_06 | motu scindé | 32 | 1,87 | 1 | 1 (106) | 1 (182) | 3 (12/12/12) | 3 | 12 | 3 | lagon 275 m, appr. 60 m | 9 / 29 | 35,9 |
| FRG_Raraka_07 | motu scindé | 43 | 1,86 | 1 | 1 (184) | 1 (50) | 3 (12/12/12) | 3 | 8 | 0 | océan 275 m, appr. 35 m | 26 / 34 | 35,9 |
| FRG_Raraka_08 | motu scindé | 27 | 1,73 | 1 | 1 (245) | 1 (25) | 3 (12/12/12) | 3 | 7 | 3 | lagon 201 m, appr. 90 m | 0 / 31 | 35,2 |
| FRG_Raraka_09 | motu scindé | 28 | 1,79 | 1 | 1 (167) | 1 (112) | 3 (12/12/12) | 3 | 10 | 3 | lagon 274 m, appr. 90 m | 0 / 25 | 34,0 |
| FRG_Raraka_10 | archipel | 19 | 1,80 | 4 | 2 (106, 66) | 0 | 3 (12/12/12) | 1 | 2 | 0 | océan 275 m, appr. 90 m | 8 / 27 | 32,5 |
| FRG_Raraka_11 | motu scindé | 39 | 1,85 | 1 | 0 | 2 (214, 157) | 3 (12/12/12) | 3 | 8 | 3 | lagon 275 m, appr. 35 m | 22 / 48 | 31,5 |
| FRG_Raraka_12 | naturel | 17 | 1,72 | 2 | 2 (619, 67) | 0 | 3 (12/12/11) | 1 | 7 | 0 | océan 296 m, appr. 120 m | 0 / 26 | 30,8 |
| FRG_Raraka_13 | motu scindé | 51 | 1,58 | 1 | 0 | 2 (246, 250) | 3 (12/12/12) | 3 | 4 | 3 | lagon 274 m, appr. 90 m | 57 / 18 | 29,6 |
| FRG_Raraka_14 | motu scindé | 45 | 1,58 | 1 | 0 | 2 (250, 229) | 3 (12/12/12) | 3 | 3 | 3 | lagon 275 m, appr. 90 m | 20 / 24 | 29,0 |
| FRG_Raraka_15 | motu scindé | 49 | 1,66 | 1 | 0 | 2 (240, 243) | 3 (12/12/12) | 3 | 3 | 3 | lagon 276 m, appr. 90 m | 20 / 25 | 28,4 |
| FRG_Raraka_16 | motu scindé | 32 | 1,57 | 1 | 0 | 2 (213, 242) | 3 (12/12/12) | 3 | 4 | 3 | océan 275 m, appr. 90 m | 43 / 40 | 28,0 |
| FRG_Raraka_17 | motu scindé | 35 | 1,39 | 1 | 0 | 2 (237, 213) | 3 (12/12/12) | 3 | 2 | 3 | océan 275 m, appr. 120 m | 24 / 39 | 25,6 |
| FRG_Raraka_18 | motu scindé | 33 | 1,27 | 1 | 0 | 2 (50, 260) | 3 (12/12/12) | 3 | 4 | 0 | océan 275 m, appr. 35 m | 11 / 32 | 23,1 |
| FRG_Raraka_19 | archipel | 17 | 2,31 | 11 | 2 (106, 113) | 0 | 3 (12/12/9) | 1 | 2 | 1 | océan 275 m, appr. 120 m | 0 / 30 | 22,7 |
| FRG_Raraka_20 | motu scindé | 71 | 2,04 | 1 | 0 | 2 (260, 237) | 3 (12/12/12) | 3 | 5 | 2 | océan 275 m, appr. 35 m | 52 / 37 | 21,9 |

![fragments](../reference/map/raraka/fragments/preview_fragments.png)

**Prototype proposé : `FRG_Raraka_01`.** C'est un motu de l'est de l'atoll, centré vers (1 224 336, 203 359) cm UE, à environ 19 km de Motutapu. La règle `prototype` de `rules.json` le désigne : il a la meilleure note parmi les fragments qui remplissent trois conditions.
- **Un seul groupe de motu.** On va d'une zone à l'autre à pied, en pataugeant au plus dans un hoa de moins de 60 m.
- **Aucun hoa synthétique.** Les deux frontières sont naturelles (230 m et 115 m d'eau) : T-083 n'aura rien à creuser pour ce premier fragment.
- **Arrivée de la barque côté lagon,** à 274 m de la plage, avec au moins 1,5 m d'eau jusqu'à 90 m du rivage.

Ce qu'il contient :
- 52 ha sur 2,2 km ;
- 3 zones de village bien séparées sur sa longueur (Z1, Z2, Z3) ;
- 3 sites de marae ; le meilleur a 0,01 m de dénivelé et 96 % de sable, côté lagon ;
- 6 criques côté lagon pour l'extraction et 3 mouillages ;
- 22 % de cocoteraie et 26 % de fourré.

L'archipel `FRG_Raraka_02` a une note à peine supérieure (44,4 contre 44,1). Mais ses villages occupent des îlots de 200 m, séparés par 400 m de platier à traverser à gué. C'est un bon second fragment (variété, L8), mais trop risqué pour un premier prototype.

![FRG_Raraka_01](../reference/map/raraka/fragments/preview_FRG_Raraka_01.png)

**Limites connues (à revoir en recette)**
- **La bathymétrie est estimée** par `build_terrain.py` ; il n'y a pas de relevé.
  - Côté lagon, le platier fait environ 150 m de large en médiane, à −0,4 m.
  - Une ligne « ≥ 1,5 m d'eau jusqu'à la plage » n'existe donc nulle part : la barque traverse 35 à 120 m de platier en fin d'approche.
  - 9 arrivées sur 20 sont côté océan.
- **La capacité des villages est approximative** (route droite, coins de la maison). Elle plafonne à 12 minka presque partout. Seul `place_villages.py` (T-085) dira le vrai nombre de maisons, en tenant compte des cocotiers et des acteurs déjà placés.
- **Le cou d'un motu est mesuré le long de l'axe de l'anneau.** La ligne de coupe suit la normale de l'axe, pas forcément la direction la plus étroite du motu. La largeur réellement creusée dépendra du hoa construit par T-083 (plage, pente ≤ 7°).
- **Les lignes de coupe naturelles sont indicatives** (normale de l'axe, ±300 m). La largeur d'eau, elle, est mesurée entre les terres.
- **Les noms affichés (`display_name`) restent vides.** Ils seront fictifs (String Table, T-087).
- L'esquisse `scripts/map/fragment_candidates.py` (groupes à moins de 40 m) est remplacée par ce pipeline.

## Règles d'un fragment (valeurs dans `DA_FragmentRules` et dans la config du pipeline, jamais en dur)
- **Taille jouable** : de 15 à 80 ha de terre, 1 à 2,5 km le long de l'anneau. C'est la cible pour une expédition de 2 à 8 min avec 3 zones (§6.1).
- **Contenu minimal** :
  - 3 sites de village d'au moins 4 maisons, un par zone de pacification (`place_villages.py`) ;
  - 1 site de marae (`marae_layout.py`) ;
  - 3 mouillages, dont un accessible pendant les 60 s finales (§6.4) ;
  - 1 crique d'extraction côté lagon ;
  - un départ pour l'équipe et la barque.
- **Frontière** :
  - elle ne coupe jamais la terre ;
  - elle passe par de l'eau large d'au moins 60 m (hoa naturel, lagon ou océan) ;
  - sinon, par un hoa synthétique creusé au point le plus étroit du motu : 40 à 80 m de large, -1,5 à -3 m au centre, une plage de chaque côté, une pente d'au plus 7° ;
  - le platier qui continue vers les motu exclus s'arrête par une pente douce, jamais par une marche.
- **Marge d'eau** : environ 1,5 km de mer autour de la terre. Le fond rejoint le lagon profond (-8 à -30 m) ou l'océan. Le plan d'eau dépasse l'horizon.
- **Horizon** : au-delà de la marge, le reste de l'anneau est montré par un maillage très simplifié (sable et bande de canopée), avec un seul matériau, sans collision ni ombre dynamique.
- **Fiction** : les noms des fragments sont fictifs, sans toponyme réel ; Motutapu en est exclu.

## Pipeline (hors ligne, reproductible)
1. **`find_fragments.py raraka`** (T-082). Il calcule l'axe de l'anneau (squelette terre + platier) et repère les points de coupe :
   - coupes naturelles, là où l'eau est large ;
   - cous étroits des grands motu.

   Il note ensuite chaque segment (taille, sites de village et de marae, criques, cocoteraie et fourré) et écrit `fragments.json`, avec mémoire et identifiants stables comme `villages.json`, ainsi que des aperçus. Les fragments retenus sont **gelés**.
2. **`build_fragment.py raraka FRG_Raraka_01`** (T-083). Il découpe le terrain dans la boîte du fragment, marge comprise, à partir des classes et des hauteurs de `build_terrain.py`. Il retire la terre exclue (remplacée par le profil platier → lagon ou océan), creuse les hoa synthétiques et fait descendre la marge en pente douce. Il recalcule les masques (sable et sable mouillé sur les nouvelles rives) et sort des tuiles Nanite, avec `georef.json` pour le décalage par rapport au repère de Raraka.
3. **`build_horizon.py raraka FRG_Raraka_01`** (T-084). Il produit la silhouette du reste de l'atoll vu depuis le fragment.
4. **Contenu** (T-085, T-086). On réutilise les générateurs existants : cocotiers, végétation, villages, marae, loot, totems. Ils sont **filtrés sur le polygone du fragment et recentrés**. On ajoute mouillages, crique et départs.
5. **Import** (T-089). Commandlet si l'éditeur est fermé, MCP sinon. Résultat : `L_Frag_Raraka_01` avec `LI_ExpeditionCore`.

## Côté jeu (C++, T-087 / T-088)
- `UExpeditionIslandDefinition` (`UPrimaryDataAsset`, type d'asset primaire `ExpeditionIsland`). Il contient :
  - la map en `TSoftObjectPtr<UWorld>`, l'identifiant `FRG_*` et le nom affiché (String Table) ;
  - le nombre de zones et la liste des mouillages ;
  - la taille, la difficulté indicative, les seeds de variantes et les crédits des données.
- `DA_IslandCatalog` : la liste des fragments proposés au hub (L4, tableau de contrats §7).
- Voyage : du hub au fragment par `ServerTravel` en seamless travel, puis retour au hub au bilan. Les joueurs, leur PlayerState et leurs cosmétiques sont conservés ; aucun loot de l'île précédente (recette n°9).
- `LI_ExpeditionCore` contient le plan d'eau tagué `IslandWaterSurface`, le ciel et le jour/nuit, la tempête et le vent, l'horloge d'expédition et le directeur d'événements. Le niveau d'eau reste un paramètre global (`UIslandWaterSubsystem`).
- Pas de World Partition par défaut (environ 3 × 3 km marge comprise). On ne l'active qu'après mesure (T-091).

### Design technique T-087 (lot L3)
- **Dossier** `Source/monke_island/Expedition/Islands/` : les îles sont des données du domaine Expédition (cahier §9 « Data Assets : îles »), à côté de `Expedition/Core/`. `World/` reste pour les systèmes du monde (eau, ciel). Dépendance à sens unique : `Core/` (GameState) inclut `Islands/` dans son `.cpp` ; `Islands/` n'inclut ni `Core/`, ni UI, ni autre domaine (ARC-01).
- **Classes** :
  - `UExpeditionIslandDefinition` (`UPrimaryDataAsset`, type primaire `ExpeditionIsland`) : `FragmentId` (`FRG_Raraka_01`, aussi nom du `FPrimaryAssetId`, donc stable même si l'asset est renommé), `DisplayName` (FText, String Table), `Map` (`TSoftObjectPtr<UWorld>`), `ZoneCount`, `Anchorages` (`FExpeditionAnchorage` : id, nom, rôle §6.4 départ / ressources / repli, disponible pendant la submersion, transform dans le repère de la map), `SizeHectares`, `IndicativeDifficulty`, `VariantSeeds`, `DataCredits`, `PreviewImage` (soft, bundle `Preview`). `FragmentId` et `Map` sont `AssetRegistrySearchable` : on retrouve le fragment d'une map sans rien charger.
  - Mouillages en **données** (id + transform), pas en référence d'acteur : la définition est lisible au hub sans charger la map ; les futurs `BP_MooringPoint` porteront le même `AnchorageId`.
  - `UIslandCatalog` (`UPrimaryDataAsset`, type `IslandCatalog`, asset `DA_IslandCatalog`) : liste de `FPrimaryAssetId` de définitions + `UFragmentRules` (`DA_FragmentRules`, petit, référence dure).
  - `UFragmentRules` (`UDataAsset`) : seuils runtime seulement — délai maximal de chargement (60 s, §16), mouillages minimum (3), mouillage de submersion exigé. Les valeurs du pipeline Python restent dans sa config.
  - `ExpeditionIslands::` (fonctions pures, testées) : `ValidateDefinition`, `ValidateCatalog`, `BuildTravelURL`, `ParseFragmentOption`.
  - `UExpeditionIslandSubsystem` (`UGameInstanceSubsystem`, toutes machines) : charge catalogue puis définitions **en asynchrone** via l'Asset Manager (`LoadPrimaryAssets`, branches succès / échec / annulation, délai maximal), cache des définitions, **sélection serveur** (`SelectFragment`, refusée sur un client ou hors catalogue), URL de voyage pour T-088 (`<map>?Island=FRG_…`, option partagée avec T-093), et `ResolveFragmentIdForWorld` (option d'URL, puis sélection, puis registre d'assets par la map ouverte : couvre le PIE lancé directement sur `L_Frag_Raraka_01`).
- **Erreurs** : `EFragmentError` + `GetFragmentErrorText` (textes LOCTEXT, à basculer en String Table `ST_Fragments`) ; jamais de `check` sur une donnée d'asset ; une définition invalide est écartée du catalogue avec un `Warning`, un catalogue vide rend `CatalogEmpty`. Journal `LogMonkeIslands`.
- **Réseau** : le choix appartient au serveur (hub, L4). Seul `FragmentId` est répliqué, dans `AExpeditionGameState` (fixé par le serveur au `BeginPlay`, avant le départ de l'horloge) ; chaque client charge la définition localement par son id (nom affiché, crédits). Aucun RPC ajouté ici : la requête « choisir ce fragment » passera par le PlayerController (L4) et appellera `SelectFragment` côté serveur.
- **Mémoire** : définitions légères (map et aperçu en souple), gardées par le cache du subsystem et l'Asset Manager ; pas de `LoadSynchronous`. Callbacks liés faiblement, timers et handles annulés au `Deinitialize`.
- **Config** : `DefaultGame.ini`, `+PrimaryAssetTypesToScan` `ExpeditionIsland` et `IslandCatalog` dans `/Game/MonkeIsland/Data/Islands`, `CookRule=AlwaysCook` (la map suit par sa référence souple ; à vérifier au packaging, T-088).
- **Reste** : T-088 (voyage seamless, GameMode qui appelle `GetTravelURL`, `ZoneCount` vers les règles d'horloge), T-089 (assets `DA_Island_Raraka_01`, `DA_IslandCatalog`, `DA_FragmentRules`, `LI_ExpeditionCore`).

### Île aléatoire au lancement (T-094, lot L3, en attendant le hub L4)
- **Code** (`Expedition/Islands/`) :
  - `ExpeditionIslands::PickIndex` / `PickFragmentId` (purs) : tirage seedé, jamais deux fois de suite le même fragment sauf catalogue d'un seul, `CatalogEmpty` sur catalogue vide ;
  - `UExpeditionIslandSubsystem::PickRandomIsland(Seed)` (serveur) : tire dans le catalogue chargé, sélectionne, mémorise le dernier tiré dans le GameInstance ;
  - `TravelToSelectedIsland()` : solo = voyage client comme `OpenLevel`, hôte = `ServerTravel` absolu avec `?listen` conservé, client = refus `NoAuthority` ;
  - `UExpeditionIslandDefinition::FindAnchorageByRole(Start)` : point d'arrivée de la barque ;
  - `AExpeditionBootGameMode` : au `BeginPlay`, charge le catalogue, tire (graine horloge, ou `FixedSeed` si `bUseFixedSeed`), voyage au tick suivant. Pas de pion sur la carte de boot. Échec : log `Error`, message à l'écran (hors Shipping) et voyage vers `FallbackMap`, pour ne jamais rester bloqué sur une carte vide.
- **Contrat avec T-093** (barque, `Boats/`) : URL `<Map>?Island=<FragmentId>`. Le serveur lit l'option `Island` (`World->URL`, ou `ExpeditionIslands::ParseFragmentOption`). Il prend la définition déjà chargée (`UExpeditionIslandSubsystem::FindLoadedDefinition`, ou `LoadDefinition` si besoin), puis le mouillage `Start` (`FindAnchorageByRole`), et y fait apparaître la barque avec les joueurs assis. `AExpeditionGameState::GetFragmentId()` donne le même id à tous les clients.
- **À créer en éditeur par T-095** (aucun asset n'existe à ce stade) :
  1. `/Game/MonkeIsland/Data/Islands/DA_FragmentRules` (`UFragmentRules`, valeurs par défaut : 60 s, 3 mouillages, mouillage de submersion exigé).
  2. Des `DA_Island_Raraka_01`, `_02`, `_03` intérimaires (`UExpeditionIslandDefinition`) dans le même dossier :
     - `FragmentId` `FRG_Raraka_01`, `02`, `03` ;
     - `Map` = `Lvl_Raraka` pour les trois ;
     - `ZoneCount` 3, un `DisplayName` fictif, `DataCredits` (Te Fenua / DAF, Licence Ouverte Etalab 2.0 ; atlas IRD) ;
     - 3 mouillages : `Start`, `Resources`, `Fallback`, ce dernier avec `bAvailableDuringSubmersion`. Le `Start` de chacun est placé sur l'eau devant une plage **différente** de `Lvl_Raraka`, hors Motutapu, pour que les arrivées varient.

     Sans ces 3 mouillages, la définition est écartée du catalogue (`InvalidAnchorages` dans le log).
  3. `DA_IslandCatalog` (`UIslandCatalog`, nom exact attendu par le code) : `Fragments` = `ExpeditionIsland:FRG_Raraka_01`, `02`, `03` ; `Rules` = `DA_FragmentRules`.
  4. La carte vide `/Game/MonkeIsland/Maps/L_Boot`, avec un `BP_BootGameMode` (enfant de `AExpeditionBootGameMode`, `FallbackMap` = `Lvl_Raraka`) en GameMode Override des World Settings.
  5. Une fois `L_Boot` en place, et seulement alors : `DefaultEngine.ini`, `[/Script/EngineSettings.GameMapsSettings]`, `GameDefaultMap=/Game/MonkeIsland/Maps/L_Boot.L_Boot`. `EditorStartupMap` ne change pas.
  6. Vérifier : l'éditeur relancé après la fusion (nouveaux types dans l'Asset Manager), *Data Validation* propre sur les 5 assets, `Audit Assets` montre `ExpeditionIsland:FRG_Raraka_0N`. Ensuite, deux PIE successifs depuis `L_Boot` arrivent sur deux fragments différents ; le log `LogMonkeIslands` donne la graine et le fragment tiré.
- **Limite connue** : tant que les trois définitions partagent `Lvl_Raraka`, un PIE lancé directement sur `Lvl_Raraka` (sans `?Island=`) ne sait pas quel fragment il joue : le registre renvoie le premier trouvé. Passer par `L_Boot`.

## Départ en barque : ce qui est attendu (T-093 → T-094, T-095)
Le départ en barque est implémenté dans `Source/monke_island/Boats/` (spec : `boat-rowing.md`, section « Départ en barque »). Il reste à le brancher :
- **T-082** : `fragments.json` donne, pour chaque fragment, au moins un point d'arrivée sur l'eau. Ce point est à quelques centaines de mètres d'une plage, sur au moins 1,2 m d'eau, avec un cap vers l'île : x, y et lacet.
- **T-095** : place dans `Lvl_Raraka` un acteur `AExpeditionArrivalPoint` par point de `fragments.json`, avec `FragmentId = FRG_*` et la flèche (+X) vers l'île. Crée `DA_BoatStartRules` et le renseigne dans `DefaultGame.ini`. Vérifie en PIE, en solo puis en hôte + client, avec `?Island=` et sans.
- **T-094** : ouvre la map avec `?Island=<FragmentId>`. Une fois la définition d'île fusionnée, il peut aussi injecter le mouillage `Start` par `UBoatStartSubsystem::SetArrivalOverride(Transform, FragmentId)`, côté serveur, avant la première frame du monde (`OnWorldBeginPlay` d'un subsystem d'Expedition/). L'override passe avant les acteurs.

## Critères d'acceptation
1. Aucune frontière de fragment ne traverse la terre. Un test automatique sur `fragments.json` vérifie l'intersection entre frontière et terre, hors hoa synthétique.
2. Huit captures prises depuis la côte et depuis la barque, en faisant le tour : aucun bord, mur, motu tranché ni trou de terrain visible.
3. La barque fait le tour complet du fragment sans s'échouer sur une marche ni sortir du monde.
4. Les 3 villages, le marae et les 3 mouillages sont atteignables à pied, à la nage ou en barque. Le PlayerStart et la barque sont valides.
5. Le passage hub → fragment → hub fonctionne en solo, puis en hôte + client (PIE à 2), avec les mêmes valeurs des deux côtés.
6. Mémoire et temps de chargement mesurés et comparés à `Lvl_Raraka`, avec le budget de `docs/conventions/performance.md` et `memory.md`.
7. Données citées (Etalab 2.0, IRD) dans les crédits. Aucune imagerie Maxar embarquée.

## Hors périmètre
Plusieurs fragments en streaming dans une même partie ; génération de terrain au lancement ; autres atolls (plus tard, même pipeline) ; éditeur d'îles E1.
