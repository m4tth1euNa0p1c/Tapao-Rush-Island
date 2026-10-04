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
2. **`build_fragment.py raraka FRG_Raraka_01`** (T-083). Il découpe le terrain dans la boîte du fragment, marge comprise, à partir des classes et des hauteurs de `build_terrain.py`. Il retire la terre exclue (remplacée par le profil platier → lagon ou océan), creuse les hoa synthétiques et fait descendre la marge en pente douce. Il recalcule les masques (sable et sable mouillé sur les nouvelles rives) et sort des tuiles Nanite, avec `georef.json` (décalage nul : même repère que Raraka). Détail plus bas, section « Terrain des fragments (T-083) ».
3. **`build_horizon.py raraka FRG_Raraka_01`** (T-084). Il produit la silhouette du reste de l'atoll vu depuis le fragment.
4. **Contenu** (T-085, T-086). On réutilise les générateurs existants : cocotiers, végétation, villages, marae, loot, totems. Ils sont **filtrés sur le polygone du fragment et recentrés**. On ajoute mouillages, crique et départs.
5. **Import** (T-089). Commandlet si l'éditeur est fermé, MCP sinon. Résultat : `L_Frag_Raraka_01` avec `LI_ExpeditionCore`.

## Terrain des fragments (T-083, lot L3, 2026-10-04)
Commandes (depuis la racine du dépôt) :

```
python scripts/map/fragments/build_fragment.py raraka FRG_Raraka_01 [FRG_Raraka_NN ...] [--all]
       [--check] [--anchors | --force-anchors] [--no-mesh] [--data-root <checkout LFS>]
python scripts/map/fragments/check_fragments.py raraka [--no-terrain]   # contrôles T-082 + terrain + galion
```

Paramètres : sections `terrain` et `galleon` de `rules.json`. Données lues comme T-082 (pointeurs LFS → arbre principal ou `--data-root`) : `terrain/heightmap_16.png` + `georef.json` (hauteurs de `build_terrain.py`, pente du fond ≤ 7° de T-068), atlas IRD, trait de côte, `T_Raraka_Masks.png`. `build_terrain.py` n'est pas modifié : on réutilise `classify_atlas`, `rasterize_coastline` (via `fraglib`), `smoothstep`, `grid_mesh` et `write_obj` (conversion d'axes calibrée).

**Repère** (décision de la session principale) : un fragment garde les **coordonnées monde de `Lvl_Raraka`**, sans recentrage. Le décalage vaut 0 ; villages, camps, marae, arrivées et grille d'inondation se reprennent tels quels dans `L_Frag_*`. `georef.json` du fragment donne sa boîte et l'emprise du terrain en cm monde.

**Méthode**, sur la fenêtre de la boîte (grille source, 4,59 m par pixel) :
1. Hauteurs et classes de `build_terrain.py`.
2. **Terre exclue** (tout motu hors du polygone gardé) → platier à −0,4 m.
3. **Fin du platier** : au-delà de 300 m de la terre gardée, le fond ne remonte jamais au-dessus d'une rampe qui part du platier et descend à 3° jusqu'à −30 m. Le platier qui continuait vers les motu exclus s'arrête donc en pente douce ; l'océan, plus profond, reste tel quel. Les pinacles du lagon sont gardés, sauf à moins de 300 m du bord. Les taches de l'atlas classées « pinacle » le long de la couronne (anticrénelage) ne le sont pas.
4. **Hoa synthétique** : un chenal le long de la ligne de coupe, −2 m au centre, qui remonte à 4,8° jusqu'à la ligne d'eau, à `hoa_width_m / 2` (30 m) de la ligne. De part et d'autre de la nouvelle ligne d'eau, le sol suit 4,8° : la terre gardée y gagne une plage. L'autre rive est de la terre exclue : elle devient du platier à −0,4 m (sa plage existe dans la map du fragment voisin).
5. **Terre gardée = exactement le polygone** de `fragments.json` : hauteur > 0 dessus, < 0 partout ailleurs. Les pixels qui bordent une nouvelle ligne d'eau valent ±0,08 m.
6. **Masques** (même format RGBA que `T_Raraka_Masks` : R végétation, G sable, B récif, A humidité), rééchantillonnés depuis ceux de l'atoll. Sur la terre exclue et le fond abaissé : récif sur le platier peu profond, sable au-delà ; sable au fond du hoa. Sur les nouvelles rives : sable sur 15 m et sable mouillé sur 4 m.
7. **Mouillage du galion** (ADR-005) : `galleon_anchor` {`location_cm`, `yaw_deg`, `depth_m`, `center_depth_m`, `radius_m`, `behind_arrival_m`, `beach_dist_m`}. On part du point d'arrivée de la barque et on recule vers le large le long de l'axe de la proue, par pas de 5 m, à partir de 50 m pour que le galion ne recouvre pas la barque. On retient le premier point où **tout le disque de 40 m** a au moins 4 m d'eau, sans pinacle à moins de 60 m du centre et à plus de 140 m du bord de la boîte. Le cap du galion est celui de la barque, proue vers l'île. `depth_m` est le point le moins profond du disque. Le mouillage est calculé une fois puis gelé avec le fragment : `find_fragments.py` ne le calcule que s'il manque, et `--force-anchors` le recalcule.

**Contrôles** (`build_fragment.py --check`, et `check_fragments.py` par défaut) :
- terre gardée intacte (hauteur > 0 exactement sur le polygone) et aucune terre exclue dans la boîte ;
- pente ≤ 7° sur chaque triangle du maillage, sur les hoa et les nouvelles rives (pinacles exclus) ;
- hoa de 40 à 80 m à la ligne d'eau, fond de −1,5 à −3 m au centre, plage côté fragment ;
- fond ≤ −10 m sur tout le bord de la boîte ;
- un **anneau navigable** d'au moins 1,2 m d'eau fait le tour de la terre : les hauts-fonds qui contiennent la terre gardée ne touchent pas le bord. L'arrivée de la barque et le galion sont sur l'eau navigable reliée au large ;
- galion : au moins 4 m sur tout le disque, pas de pinacle, dans la boîte, derrière l'arrivée sur l'axe de la proue.

**Sorties**
- `docs/reference/map/raw/raraka_fragments/<FRG>/` (régénérable, ignoré par git) :
  - `SM_Frag_Raraka_NN_Terrain_<x>_<y>.obj` : tuiles de 256 × 256 pixels, maillage au pas de 4,59 m (deux fois plus fin que `Lvl_Raraka`). Les tuiles entièrement sous −80 m sont omises ;
  - `SM_Frag_Raraka_NN_Water.obj` : plan d'eau à z = 0, boîte + 15 km ;
  - `T_Frag_Raraka_NN_Masks.png` : 2048², UV0 sur l'emprise du terrain ;
  - `heightmap_16.png` : bornes dans `georef.json`.
- `docs/reference/map/raraka/fragments/<FRG>/` :
  - `georef.json` : repère, boîte, fenêtre source, tuiles, eau, masques, règles, mouillage du galion, résultats des contrôles ;
  - aperçus `preview_relief.png`, `preview_classes.png`, `preview_masks.png`, `preview_ombrage.png`. On y voit la limite des hauts-fonds (anneau de la barque), l'arrivée et le galion.
- Les vignettes `preview_FRG_Raraka_NN.png` montrent aussi le galion (cercle orange).

**Résultats du 2026-10-04 : les 20 fragments sont construits, 0 échec** (`build_fragment.py raraka --all` puis `check_fragments.py raraka` : 0 échec, en 31 s). Le fond vaut −30 m sur tout le bord de la boîte, partout. Il y a 12 à 20 tuiles et 1,4 à 2,0 M triangles par fragment.

| Id | Arrivée | Galion : fond min / centre (m) | Galion : recul depuis l'arrivée / distance à la plage (m) | Terre exclue retirée (ha) | Pente max nouvelles rives | Hoa (largeur à la ligne d'eau, m) |
|---|---|---|---|---|---|---|
| **FRG_Raraka_01** | lagon | 7,9 / 8,4 | 50 / 324 | 28,1 | 3,0° | — |
| FRG_Raraka_02 | lagon | 4,0 / 8,1 | 55 / 351 | 61,5 | 3,1° | — |
| FRG_Raraka_03 | océan | 18,4 / 23,4 | 50 / 334 | 10,2 | 3,1° | — |
| FRG_Raraka_04 | lagon | 4,1 / 7,2 | 105 / 401 | 73,1 | 5,6° | 53 |
| FRG_Raraka_05 | océan | 28,3 / 33,1 | 50 / 332 | 34,3 | 3,3° | — |
| FRG_Raraka_06 | lagon | 9,5 / 10,1 | 50 / 328 | 52,6 | 6,8° | 60 |
| FRG_Raraka_07 | océan | 30,8 / 35,3 | 50 / 328 | 51,6 | 6,8° | 50 |
| FRG_Raraka_08 | lagon | 4,4 / 8,2 | 205 / 413 | 43,3 | 6,8° | 62 |
| FRG_Raraka_09 | lagon | 5,2 / 6,0 | 50 / 332 | 83,4 | 6,8° | 48 |
| FRG_Raraka_10 | océan | 13,5 / 18,4 | 50 / 326 | 35,3 | 3,2° | — |
| FRG_Raraka_11 | lagon | 10,2 / 10,9 | 50 / 340 | 67,4 | 6,8° | 58, 49 |
| FRG_Raraka_12 | océan | 15,4 / 20,2 | 50 / 350 | 27,3 | 3,5° | — |
| FRG_Raraka_13 | lagon | 4,5 / 5,3 | 50 / 330 | 90,2 | 5,7° | 57, 51 |
| FRG_Raraka_14 | lagon | 8,2 / 8,5 | 50 / 326 | 125,7 | 5,6° | 62, 50 |
| FRG_Raraka_15 | lagon | 5,0 / 7,5 | 50 / 330 | 107,6 | 6,4° | 58, 51 |
| FRG_Raraka_16 | océan | 24,6 / 29,2 | 50 / 338 | 108,1 | 6,3° | 61, 51 |
| FRG_Raraka_17 | océan | 17,0 / 21,7 | 50 / 336 | 102,9 | 6,8° | 57, 50 |
| FRG_Raraka_18 | océan | 31,3 / 36,0 | 50 / 328 | 94,4 | 6,8° | 62, 51 |
| FRG_Raraka_19 | océan | 14,3 / 19,2 | 50 / 328 | 30,7 | 3,2° | — |
| FRG_Raraka_20 | océan | 31,9 / 36,6 | 50 / 332 | 86,6 | 6,8° | 61, 51 |

- Les contrôles ont été testés à rebours. Sur une copie corrompue (galion posé sur la plage, galion absent), `check_fragments.py` sort en échec. Avec des règles dégradées (rampe à 0,2°, pente des rives à 9°), `build_fragment.py --check` signale le bord à −0,4 m, l'anneau de barque fermé et une pente de 10,4° sur le hoa (code 1).
- Tous les hoa ont −2 m au centre. Quand la coupe élargit un hoa naturel peu profond, le centre mesure de −1,8 à −2 m.
- Le galion mouille presque partout 50 m derrière l'arrivée, soit le recul minimal. Seuls FRG_04 (105 m) et FRG_08 (205 m) doivent reculer davantage pour trouver 4 m sur tout le disque.

![FRG_Raraka_01, relief](../reference/map/raraka/fragments/FRG_Raraka_01/preview_relief.png)

**Limites (à revoir en recette T-089, critères 2 et 3)**
- **La bathymétrie reste estimée** (`build_terrain.py`, pas de relevé SHOM ou IRD). La rampe de marge, le platier laissé à la place des motu exclus et les hoa synthétiques sont des constructions de jeu, pas des données.
- Les profondeurs du galion en découlent. Le fond minimal de 4 m (FRG_02, 04, 08, 13) n'est garanti que sur cette estimation.
- **Pas de vérification en jeu** : aucun import Unreal, ni PIE, ni tour en barque n'a été fait (c'est T-089). Les critères 2 et 3 ne sont vérifiés que par contrôle automatique et aperçus.
- **Atlas IRD en escalier** : côté océan, le bord du platier et du tombant suit les pixels de l'atlas (4,6 m). C'est déjà le cas dans `Lvl_Raraka`. On le voit sur l'ombrage, sous l'eau.
- **Bout d'un motu scindé** : vu d'avion, il est rectiligne, puisque la ligne de coupe est droite. La plage et la pente douce l'adoucissent au sol.
- **Rive opposée d'un hoa** : dans la map du fragment, c'est un haut-fond de platier (−0,4 m), pas une plage émergée. La terre de l'autre côté appartient au fragment voisin ou à l'horizon (T-084).
- **Bord du terrain** : il s'arrête à la boîte, à −30 m ou plus profond, sans jupe. Sous l'eau, on pourrait voir le vide au-delà de 1,5 km : à vérifier avec le brouillard d'eau en T-089.
- **Taille des OBJ** : 1,4 à 2 M triangles et environ 100 Mo d'OBJ par fragment. Un pas de 2 pixels (`mesh_step_px`) diviserait ce volume par 4, au prix d'une pente moins fine aux plages.
- Relancer `find_fragments.py` recalcule les `stats` globales de `fragments.json` (segments jouables, rejets) en comptant les fragments gelés comme pris. Cette différence existait avant T-083 ; les fragments, eux, restent identiques.

## Horizon (T-084 pipeline, T-089 import, lot L3)
Le reste de l'anneau reste visible au loin : silhouettes de sable et de canopée, sans gameplay. Statut : pipeline exécuté sur les 20 fragments, **aucun asset UE créé** (import et matériau : T-089).

### Maillage : `build_horizon.py`
- **Commande** : `python scripts/map/fragments/build_horizon.py raraka FRG_Raraka_01` (ou `--all`). Durée : environ 10 s par fragment. Paramètres : `scripts/map/fragments/horizon_rules.json`, chacun documenté.
- **Contenu** : tous les motu réels (`rivage.geojson`) hors de la boîte jouable (`box_cm`) + 60 m. Chaque motu donne :
  - une **bande de sable** à +1,4 m, avec une jupe qui plonge à -1,5 m, 10 m vers le large ;
  - un **ruban de canopée** de 15,5 m (fourré, forêt) à 22,5 m (cocoteraies, `cocoteraies.geojson`), posé sur la végétation (masque R de `T_Raraka_Masks` + cocoteraies, à plus de 14 m du rivage). Le ruban est irrégulier : bruit lent ±1,5 m, couronnes en dents de scie tous les 16 à 90 m selon la distance, palmiers émergents de +3 m. Le sommet est rentré de 7 m, ce qui donne une masse arrondie et non un mur.
- **Coupe** : le motu voisin coupé par la boîte perd sa canopée sur 500 m. Il finit en flèche de sable basse, jamais en motu tranché.
- **LOD** selon la distance au centre du fragment, par interpolation sur 1,5, 4, 9 et 20 km :
  - tolérance du contour de 2,5 à 20 m ;
  - îlots gardés à partir de 0,03 à 1 ha ;
  - au-delà de 9 km, **liseré** : contour grossier, canopée à 85 % sans palmiers émergents.
- **Courbure terrestre** : elle est incluse. UE est plat, donc chaque sommet est abaissé de d²/(2kR), avec la réfraction standard k = 7/6. Cela donne 1,7 m à 5 km, 6,7 m à 10 km et 27 m à 20 km. L'autre rive du lagon (20 à 25 km) ne montre plus que des cimes, comme en vrai. Effet de bord : le bas des motu lointains est sous z = 0, donc il faut un plan d'eau opaque à cette distance.
- **Sorties** :
  - `docs/reference/map/raw/raraka_fragments/<FRG>/SM_<FRG>_Horizon.obj` : non versionné, régénérable. Écrit par `build_terrain.write_obj`, donc dans le même repère que les tuiles de Raraka (cm, (est, nord, haut) → UE (x, -y, z)), non recentré.
  - `docs/reference/map/raraka/fragments/<FRG>/horizon_top.png` (vue de dessus), `horizon_pano.png` (vue rasante à 2 m depuis le centre du fragment : 4 × 90° à l'échelle vraie, plus 360° exagéré × 8) et `horizon.json` (budget, contrôles, emprise).
- **UV0** (un seul matériau, ni vertex color ni texture) :
  - **U = étage vertical** : 0 = bas de la jupe sous l'eau, 0,2 = haut du sable, 0,3 = pied de la canopée, 1 = cime ;
  - **V = brume précalculée** : 0 à 1,5 km, 1 à 20 km du centre du fragment, linéaire.
- **Contrôles** (le script sort en code 1 s'ils échouent) :
  - aucun triangle ne touche la boîte jouable (test des axes séparateurs en plan) ;
  - aucun sommet ni centre de triangle à moins de 1,5 km de la terre du fragment ;
  - au plus `max_tris` triangles.

  Résultat, sur les 20 fragments : 0 triangle dans une boîte, distance minimale de 1 557 m (FRG_18) à 2 333 m (FRG_03).
- **Budget** : alerte à 60 000 triangles, échec à 90 000. Mesuré : 8 845 à 13 277 triangles (moyenne 11 984), environ 45 % sable et 55 % canopée ; un seul draw, sans ombre. Les triangles de dessus repliés (`facing_down`, au plus 145, soit environ 1 %) se voient seulement d'avion.

### Recette `M_Horizon_Atoll` (à créer en T-089)
Master dans `/Game/MonkeIsland/Materials/M_Horizon_Atoll`, instance `MI_Horizon_Raraka` dans `/Game/MonkeIsland/Environment/Fragments/Materials/`.
- **Modèle** : **Unlit** de préférence (aucun coût Lumen ; la couleur ne suit pas le soleil, donc `Brightness` et `NightDim` l'assombrissent avec l'heure). En repli, *Default Lit* sans texture (Roughness 1, Specular 0) si l'unlit tranche trop avec le ciel ; Substrate : un seul Slab, même logique. Opaque, One-sided (les faces sont orientées vers l'extérieur ; si l'import les inverse, corriger l'import, pas passer en Two-Sided).
- **Couleur selon U** (`TexCoord[0].R`) :
  - `SandColor` (sable corallien sec vu de loin, sRGB ~ #D9CDA8) de U = 0 à `SandTopU` (0,2) ;
  - puis `CanopyLowColor` (pied, sous les couronnes, ~ #22381F) → `CanopyTopColor` (cimes éclairées, ~ #45633A) de `CanopyBaseU` (0,3) à 1, par `SmoothStep`.
- **Bas fondu dans l'eau** : sous `WaterFadeHeight` (0,4 m au-dessus du niveau d'eau), la couleur tend vers `WaterTint` (bleu du lagon au loin). Calcul : `saturate((AbsoluteWorldPosition.z - WaterLevel) / WaterFadeHeight)`. `WaterLevel` : scalaire à 0 par défaut. La montée de l'eau ne touche que la grille locale du fragment (T-103) : l'horizon reste au niveau de la mer. Si un jour la mer globale monte, brancher un Material Parameter Collection écrit par `UIslandWaterSubsystem` (à créer, il n'existe pas aujourd'hui).
- **Brume selon la distance** : `HazeAmount = 1 - exp(-max(0, PixelDepth - HazeStart) / HazeDistance)`, mélangée vers `HazeColor`.
  - Valeurs de départ : `HazeStart` 1 500 m, `HazeDistance` 9 000 m, `HazeColor` ~ #B9D2E6 ; `HazeColor` peut être relié à la couleur de l'horizon du SkyAtmosphere par MPC.
  - `V` donne la même information précalculée (repli si l'on évite `PixelDepth`).
  - Un `ExponentialHeightFog` léger s'ajoute déjà sur un matériau lit. Sur un unlit, appliquer le brouillard (*Apply Fogging*, réglage du matériau) et réduire la brume du matériau pour ne pas compter deux fois.
- **Paramètres exposés** (chacun avec sa description) : `SandColor`, `CanopyLowColor`, `CanopyTopColor`, `SandTopU`, `CanopyBaseU`, `WaterTint`, `WaterFadeHeight`, `HazeColor`, `HazeStart`, `HazeDistance`, `Brightness` (0,6 à 1 selon l'heure) et `NightDim` (§8 : à la nuit, les silhouettes deviennent sombres sur le ciel, sans émission propre).
- **Coût visé** : moins de 60 instructions pixel, aucun échantillonnage de texture. À vérifier avec `ue-performance-engineer`.

### Réglages de l'acteur (T-089)
- **Import** : `SM_FRG_Raraka_NN_Horizon` dans `/Game/MonkeIsland/Environment/Fragments/<FRG>/`.
  - Échelle 1 (OBJ en cm), normales lisses calculées, **pas de collision** (UCX absent, collision auto retirée, *Collision Complexity* sans simple ni complexe) ;
  - Nanite au choix : un seul maillage ≈ 12 k triangles, culling sans enjeu.
- **Placement** : un `StaticMeshActor` à l'origine du repère de Raraka, décalé de l'offset du fragment si `build_fragment.py` (T-083) recentre la map (`fragment_center_cm` est dans `horizon.json`).
- **Composant** :
  - `Cast Shadow` désactivé, `Affect Distance Field Lighting` et `Affect Dynamic Indirect Lighting` désactivés ;
  - `Collision Presets` = `NoCollision`, `Can Ever Affect Navigation` désactivé ;
  - `Visible in Ray Tracing` désactivé, `Receives Decals` désactivé.
- **Distance** : `Min Draw Distance` 0 (le maillage commence à 1,5 km). Pas de `Max Draw Distance` : l'horizon doit rester visible. HLOD inutile, puisque c'est déjà le niveau de détail le plus bas.
- **Validation** : `CaptureViewport` avant / après, depuis la plage et depuis la barque à l'arrivée, puis en vue aérienne. Comparer à `horizon_pano.png`. Une capture de nuit (§8).

### Limites
- La végétation vient du masque de l'atlas, une classification approximative : quelques motu réellement boisés apparaissent en sable seul (arc sud-ouest).
- La courbure suppose le joueur près du centre du fragment. À 1 km du centre, l'erreur est d'environ 1,4 m à 10 km.
- L'aperçu panoramique est un rendu logiciel par ordre de profondeur, sans l'eau ni le fragment : il sert à contrôler la forme, pas le rendu final.

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

## Intégration T-095 (lot L3, 2026-10-03, preuve `testé éditeur`)
**Créé** (script reproductible `scripts/editor/setup_fragments.py --part assets|arrivals`, exécuté par le `ProgrammaticToolset` du MCP, idempotent) :
- `Lvl_Raraka` : 20 `AExpeditionArrivalPoint` `ARR_FRG_Raraka_NN`, dossier `Expedition/Arrivals`, position et lacet de `arrival` (z = 0). Aucun autre acteur n'a été modifié.
- `/Game/MonkeIsland/Data/Islands/` :
  - `DA_FragmentRules` (valeurs par défaut) ;
  - `DA_Island_Raraka_01` à `_20` :
    - `Map` = `Lvl_Raraka`, `ZoneCount` 3, `SizeHectares` et `VariantSeeds` issus du json, `DataCredits` Te Fenua / DAF Etalab 2.0 + IRD ;
    - `DisplayName` = référence `ST_Islands` / `Island.FRG_Raraka_NN.Name` de 01 à 08, « Fragment NN » provisoire ensuite ;
    - mouillages `ANC_Start` (l'arrivée), `ANC_Resources` (1er mouillage, sinon une crique), `ANC_Fallback` (crique d'extraction, `bAvailableDuringSubmersion`) ;
  - `DA_IslandCatalog` (les 20 + `DA_FragmentRules`).
- `/Game/MonkeIsland/UI/Text/ST_Islands` : import de `docs/narrative/ST_Islands.csv`, 70 clés. Le chemin diffère de `islands.md` (`/Localization/`).
- Départ, horloge et compteur :
  - `Boat/Data/DA_BoatStartRules` (valeurs par défaut), `Expedition/Data/DA_ExpeditionRules` (120 / 120 / 3 / 60) ;
  - `Expedition/Blueprints/BP_ExpeditionGameState` (`Rules`, `bStartOnBeginPlay`), branché comme `GameStateClass` de `BP_IslandGameMode` ;
  - `UI/DA_HudTimerRules` : `SoundClass` = `SC_Effects`, sons vides faute de bip adapté.
- Démarrage : `Maps/L_Boot` (gabarit vidé) + `Expedition/Blueprints/BP_BootGameMode` (`FallbackMap` = `Lvl_Raraka`) en GameMode Override.
- Config : `DefaultGame.ini` (`StartRulesAsset`, section `FloodTimerSubsystem`) ; `DefaultEngine.ini` `GameDefaultMap` = `L_Boot`.

**Vérifié en PIE (éditeur)** :
- Depuis `L_Boot`, en solo :
  - catalogue chargé (20 fragments), tirage FRG_Raraka_19, ouverture de `Lvl_Raraka?Island=FRG_Raraka_19` ;
  - le GameState joue FRG_Raraka_19 ; barque au point, bean **aux rames** (siège 0), proue vers la plage ;
  - compteur « Montée des eaux dans m:ss » sur l'horloge d'expédition (échéance 120 s, capturé à 1:41 et 1:23) ; eau et vent « follow the expedition clock ».
  - Relance : FRG_Raraka_20.
  - Captures : `docs/production/captures/T-095/`.
- PIE à 2 directement sur `Lvl_Raraka` : l'hôte aux rames ; le client embarque **debout** dans la barque, car `DA_BoatRules.bOnlyRowerSits` = vrai : seul le rameur s'assoit. Même compteur des deux côtés.

**Non vérifié ou défauts** :
- ZQSD n'a pas été piloté (pas d'entrée clavier par le MCP) : seul « at the oars » est prouvé.
- **PIE à 2 depuis `L_Boot` : le client est perdu.** `AExpeditionBootGameMode` voyage 0,3 s après le chargement du catalogue, avant la connexion du client. Le `ServerTravel` non seamless de l'hôte ferme alors la connexion (« HostClosedConnection »), et le client retombe seul sur `Lvl_Raraka?closed` avec un autre fragment. À corriger en C++ : attendre les joueurs attendus, ou ne pas tirer en listen server avant le hub L4.
- Le joueur debout se tient sur `StandingSpots[0]` (−40, 0, 100), juste devant la caméra du rameur, et lui bouche la vue (capture 03).
- PIE lancé directement sur `Lvl_Raraka` sans `?Island=` : le GameState annonce FRG_Raraka_01 (premier trouvé), alors que la barque tire un point au hasard (limite connue).
- « Jamais deux fois de suite » ne tient que dans une même session de jeu : à chaque PIE ou relance, `previous None`.
- Les définitions créées vides s'enregistrent d'abord sous leur nom d'asset : une nouvelle analyse de l'Asset Manager a été nécessaire. Le script ne règle pas ce point : après une création, relancer l'éditeur ou modifier un réglage de l'Asset Manager.
- `Lvl_Raraka` reste une map intérimaire : pas encore de fragments découpés (T-083 à T-089).

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
