# Raraka (Tuamotu) — fiche de données

Atoll de l'ouest des Tuamotu, commune de Fakarava, commune associée de Kauehi. Village unique : **Motutapu** (nord-ouest), à côté de la **passe Manureva**.

## Emprise
- WGS84 : lon −145.022 → −144.765, lat −16.293 → −16.072 (mosaïques z15 : 6400 × 5632 px, 4,59 m/px au sol).
- Repère UE (`terrain/georef.json`) : origine au centre de l'emprise, X = est, Y = sud, Z = haut (cm), Z = 0 au niveau moyen de la mer.
  - Village de Motutapu ≈ (−653 335, −1 025 282) cm ; passe Manureva ≈ (−698 236, −1 011 930) cm.

## Données (récupérées le 2026-10-02)
| Fichier | Contenu | Source / licence |
|---------|---------|------------------|
| `parcelles.geojson` | 620 parcelles, 38 sections (RA → SV) | DAF / Te Fenua, Etalab 2.0 |
| `rivage.geojson` | trait de côte : 349 contours de motu (10,0 km² de terre) | DAF / Te Fenua, Etalab 2.0 |
| `batiments.geojson` | 119 bâtiments BD TOPO | Te Fenua, Etalab 2.0 |
| `cocoteraies.geojson` | 92 polygones de cocoteraies | DAG / Te Fenua, Etalab 2.0 |
| `occupation_sol.geojson`, `toponymes.geojson`, `ortho_emprises.geojson` | occupation du sol, toponymes (passe Manureva, Motutapu), emprises des orthos | Te Fenua, Etalab 2.0 |
| `raw_sat_z15.png` | mosaïque satellite (ortho 2018, 50 cm, réduite à z15) | © Maxar / DigitalGlobe via Te Fenua — **référence uniquement, ne pas embarquer** |
| `atlas_recif_z15.png` | atlas géomorphologique des récifs, même grille | Andréfouët et al., *Atlas des récifs coralliens de Polynésie française*, IRD 2005, via Te Fenua |

Classes de l'atlas identifiées sur Raraka (couleur de rendu → classe) : (93,193,217) lagon profond · (250,228,165) pente interne · (244,208,124) et (170,202,119) platier récifal · (182,132,30) tombant externe · rouge = pinacles · bleu saturé = passe.

## Terrain généré (`terrain/`, script `scripts/map/build_terrain.py raraka`)
- Surfaces : lagon 336,7 km² (Wikipédia : 342) · platier 36,1 km² · pente interne 18,3 km² · terre 10,0 km² · tombant 9,7 km² · pinacles 0,97 km² · passe 0,42 km².
- Hauteurs **estimées** (pas de bathymétrie mesurée) : motu +0,5 → +3,7 m · platier −0,4 m · pente interne jusqu'à −7,5 m · lagon −8 → −30 m · pinacles jusqu'à −1,2 m · passe −14 m · océan jusqu'à −300 m.
- `T_Raraka_Masks.png` (4096², UV0 sur l'emprise) : R végétation/litière, G sable, B récif/feo, A humidité.
- `heightmap_16.png` + min/max dans `georef.json` ; `parcelles_ue.json` : index des parcelles (section, numéro, surface, centre en cm UE).
- Meshes régénérables (ignorés par git) : `docs/reference/map/raw/raraka_terrain/` (51 tuiles OBJ, plan d'eau, calque des parcelles).

## Végétation (`vegetation/`, script `scripts/map/place_palms.py raraka`)
- **43 057 cocotiers** placés d'après les données, avec la surface exacte des tuiles du terrain (écart < 5 mm), puis enfoncés de 8 cm :
  - 22 431 en cocoteraies, soit 100 pieds/ha sur les 223 ha des 92 polygones ;
  - 6 706 juvéniles de régénération ;
  - 13 920 cocotiers naturels hors cocoteraies.
- Logique, d'après les attributs de l'inventaire DAG 2023 :
  - `disposition` REGULIERE : quinconce de 10 m aligné sur le grand axe de la parcelle, 7 % de manquants ; IRREGULIERE : semis de Poisson de 8 m.
  - `recouvrement` : 50–75 % → environ 107 pieds/ha ; 25–50 % → environ 70/ha.
  - `age` JEUNE : jeunes et juvéniles.
  - `etat` ARBUSTIVE (sous-bois non entretenu) : 35 % de régénération entre les troncs, contre 6 % sous un sous-bois herbacé.
  - `emergents` : part de grands vieux cocotiers (droits, +10 à +22 %).
  - `forme` DECOUPEE : clairières.
- Lisière (< 12 m du rivage) : cocotiers penchés ou en sabre orientés vers l'eau, et quelques cocotiers couchés par les cyclones.
- Exclusions : plage active (< 3 m du rivage), bâtiments (< 4 m), sol sous +0,5 m ; densité divisée par 2 près du village (< 25 m du bâti).
- Hors cocoteraies : lancer de fléchettes, 30/ha en bande littorale (4–15 m) et 20/ha à l'intérieur.
- Sorties : `palms_ue.json` (instances par variante, cm UE, cap = direction d'inclinaison), `preview_palms.png` (carte de contrôle).
- Vers Unreal : `scripts/map/palms_to_datatable.py raraka` produit `raw/raraka_vegetation/DT_Raraka_Palms.csv`. Le CSV utilise la structure GameplayTagTableRow : `Tag` = chemin du mesh, `DevComment` = `x,y,z|pitch,yaw,roll|échelle`.
- Les 13 formes de cocotier sont générées par `scripts/atoll/palms.py` et exportées par `scripts/atoll/export_ue.py` :
  - FBX dans `art/export/Vegetation/`, en cm ;
  - vertex color : RGB = teinte/2, A = sécheresse ;
  - collisions `UCX_` sur les troncs seulement.

### Arbustes, fourrés, couvre-sol, accessoires (`scripts/map/place_vegetation.py raraka`)
- Environ **773 000 instances** en 5 groupes :

  | Groupe | Instances | Contenu |
  |--------|-----------|---------|
  | `Shrubs` | ≈ 87 k | bois durs : miki-miki, tahinu, pandanus |
  | `Thicket` | ≈ 348 k | naupaka et cocotiers juvéniles |
  | `Trees` | ≈ 13,5 k | cocotiers adultes ajoutés |
  | `GroundCover` | ≈ 220 k | couvre-sol |
  | `Props` | ≈ 104 k | accessoires |

  Règles du catalogue `docs/reference/assets/catalogue_atoll.md`, même repère et même surface que les cocotiers (`Frame` de `place_palms.py`).
- **Nature dense dans les zones de terre** : 3,1 km² où le canal R (litière brune, zones sombres du satellite) dépasse le canal G (sable) du masque terrain.
  - Fourré continu de vieux naupaka : semis de 2,4 m, échelle 1,1 à 1,6, soit 3,3 à 4,8 m de large et 2,1 à 3,7 m de haut.
    - Environ 85 % du sol est couvert (simulation de couverture) : un adulte de 1,80 m y disparaît.
    - Environ 10 % de clairières.
    - Le village reste dégagé : rien à moins de 12 m d'un bâtiment.
  - En dessous : cocotiers juvéniles (semis de 5 m) et tapis de fougères (3,5 m).
  - Cocotiers adultes ajoutés (semis de 11 m, à 5 m au moins des troncs existants) : leurs troncs bloquent.
  - Grands pandanus hors cocoteraies, toujours présents.
- **Sable** : densité inchangée. Bosquets de naupaka et de jeunes pandanus et tapis de fougères à densité normale dans l'intérieur végétalisé hors zones de terre.
- **Physique, selon la dureté** (`is_soft`) :
  - `Hard` (collision) : troncs de cocotier, pandanus, tahinu, miki-miki (bois de fer), cocos, blocs de corail, bois flotté ;
  - `Soft` (traversable, sans collision) : naupaka, cocotiers juvéniles, cocos germés, fougères, herbes, ipomée, palmes sèches, gravats, coquillages. On peut entrer dans les fourrés et s'y cacher.
- **Lisières** :
  - haie de naupaka en haut de plage côté lagon (4–9 m de l'eau) ;
  - miki-miki en deux rangs sur la crête côté océan (3–9 m), naupaka derrière ;
  - tahinu isolés ;
  - ipomée et herbe sèche sur le sable du haut de plage.
- **Sous les cocotiers** (lus dans `palms_ue.json`) :
  - cocos (bruns surtout) ;
  - palmes sèches, base au tronc, orientées vers l'extérieur ;
  - cocos germés.
- **Littoral** :
  - blocs de corail sur la crête océan et sur le platier immergé (jusqu'à 120 m du rivage) ;
  - gravats sur la crête de tempête ;
  - bois flotté et coquillages sur la laisse de mer.
- **Circulation** :
  - environ 3 000 **passages** sont ouverts à travers les haies de lisière, un tous les 20–40 m, perpendiculaires au rivage, de la plage à 16 m dans l'intérieur ; le miki-miki bloque ;
  - aucun buisson bloquant à moins de 10 m d'un bâtiment.
- **Pose** : arbres et buissons verticaux (enfoncés de 5 à 10 cm) ; couvre-sol et accessoires alignés sur la pente (pitch et roll calculés sur la pente tournée de −yaw).
- Sorties :
  - `vegetation/vegetation_ue.json` :
    - `groups.<groupe>.<SM_Nom>` = `[x, y, z, pitch, yaw, roll, échelle]` (cm, degrés) ;
    - `meshes` = chemins UE ;
    - `physics` = Hard / Soft par mesh ;
    - `chunks` = liste des blocs (nom, physique, nombre, emprise en cm) ;
  - `preview_vegetation.png` et `preview_vegetation_village.png` (emprise réelle des meshes) ;
  - `raw/raraka_vegetation/DT_Raraka_{Hard,Soft}_NN.csv` : un bloc spatial par fichier, découpé en k-d à la médiane, 25 000 lignes au plus (`MAX_CHUNK`), au format GameplayTagTableRow comme les cocotiers. Aujourd'hui : 8 blocs Hard de 18 381 lignes et 32 blocs Soft de 19 559 lignes.
- Densité réglable : `--density 0.5` divise par deux, `--density 1.5` augmente de 50 %.

## Villages japonais (`villages/`, script `scripts/map/place_villages.py raraka`)
- Ce sont les 3 zones de pacification de la première île (cahier §6.2) : V01 Kura-mura, V02 Mori-mura et V03 Takara-mura, de 12 minka chacun. Les PNJ y apparaissent. Fiche : `docs/features/villages-japonais.md`.
- Ils sont générés par chunks de 256 m, là où la terre est large, plate, loin de l'eau et à plus de 250 m du bâti réel. Motutapu reste intact.
- Chaque village s'adapte à la cocoteraie en place : les parcelles glissent pour éviter les troncs.
- Mémoire : `villages/villages.json` (chunks évalués, villages, objets, spawns, zones, formes de dégagement). Un village connu est conservé ; `--regen Vxx` le reconstruit.
- Végétation dégagée sous les villages : `--apply-clearing`, à partir des copies `raw/raraka_vegetation/_orig/`. Les DataTables touchées sont listées dans `villages/clearing_report.json`.
- Vers Unreal : `raw/raraka_villages/DT_Raraka_Village_Vxx.csv`, au format GameplayTagTableRow comme la végétation. Chaque CSV donne une DataTable, et chaque DataTable un acteur PCG.

## Dans Unreal
- Niveau `/Game/MonkeIsland/Maps/Lvl_Raraka` (dupliqué du template Basic : soleil, SkyAtmosphere, nuages volumétriques, brouillard).
- 51 tuiles `SM_Raraka_Terrain_<x>_<y>` (Nanite, fallback 100 %, collision complexe) dans l'outliner `Raraka/Terrain`.
- `SM_Raraka_Water` (plan d'eau à Z = 0, sans collision) avec `M_Water_Lagoon` ; terrain avec `M_Terrain_Atoll`.
- `Cadastre_Parcelles` (outliner `Raraka/Cadastre`) : rubans jaunes, visibles dans l'éditeur, masqués en jeu.
- PlayerStart sur la plage côté lagon, au sud du village.
- `PCG_Raraka_Palms` (outliner `Raraka/Vegetation`) : graphe `/Game/MonkeIsland/Environment/PCG/PCG_Raraka_Palms`.
  - Chaîne : « Charger la table de données » (`DT_Raraka_Palms`, 43 057 lignes) → « Analyser la chaîne » (DevComment → `$Transform`, Tag → `Mesh`) → générateur de maillage statique (sélection par attribut).
  - Résultat : 13 ISM Nanite (`/Game/MonkeIsland/Environment/Vegetation/Palms/Meshes/SM_CoconutPalm_*`, préservation de surface).
  - Matériaux : `M_Foliage_Leaf` (Two Sided Foliage, teinte et sécheresse par vertex color), `M_Vegetation_Bark`, `M_Vegetation_VertexColor`, instanciés en `MI_CoconutPalm_*`.
  - Collision : convexes des troncs seulement (`CTF_UseSimpleAsComplex`).
- Pour mettre à jour les cocotiers :
  1. relancer les deux scripts (`place_palms.py` puis `palms_to_datatable.py`) ;
  2. supprimer, puis réimporter `DT_Raraka_Palms` avec la structure GameplayTagTableRow ;
  3. relancer l'exécution du volume PCG.
- Végétation : `PCG_Raraka_Palms` et 40 acteurs-blocs `PCG_Raraka_{Hard,Soft}_NN` (outliner `Raraka/Vegetation/Hard` et `Raraka/Vegetation/Soft`). Le volume de chaque bloc couvre son emprise.
  - Deux graphes, choisis selon la physique :
    - `PCG_Raraka_VegHard` : collision Custom, QueryAndPhysics, WorldStatic, Block sur tous les canaux sauf Camera = Ignore (la caméra ne saute pas derrière chaque tronc), `bCanEverAffectNavigation = false` ;
    - `PCG_Raraka_VegSoft` : NoCollision.
  - Le palmier `PCG_Raraka_Palms` a le même réglage que `VegHard`.
  - La DataTable est un **paramètre de graphe** (`DataTable`, SoftObjectPath) :
    - le nœud « Obtenir un paramètre graphique » (`GetDataTable`) alimente la broche `DataTable` du nœud « Charger la table de données » ;
    - chaque acteur porte son `DT_Raraka_{Hard,Soft}_NN` (`/Game/MonkeIsland/Environment/Vegetation/Data/Chunks/`).
  - PCG crée ses ISM en `NoCollision` par défaut, même si les meshes ont des `UCX_`, d'où le réglage du `TemplateDescriptor` du sélecteur. Via MCP, passer `CollisionResponses.ResponseToChannels` en entier, en plus de `ResponseArray` ; sinon la collision reste inopérante dans la session.
  - Vérifié par traces dans l'éditeur le 2026-10-03 :
    - troncs bloquants à environ 28 cm de l'axe ;
    - cocos d'environ 11 cm de rayon, que le joueur enjambe ;
    - blocs de corail et bois flotté bloquants ;
    - juvéniles, gravats, palmes, coquillages et fougères traversables.
  - **Coût de génération** : il croît comme le carré du nombre de points par acteur, T ≈ 0,0156 × (N/1000)² s. Par exemple 285 s pour 135 k points dans un seul acteur, mais 5 à 8 s pour un bloc de 19 k. Les 40 blocs se génèrent en environ 4 min, par lots de 7 à 8 ; c'est pourquoi tout est découpé en blocs.
  - **Régénération manuelle uniquement** : `Regenerate PCG Volume In Editor` est désactivé sur tous les acteurs. Modifier un graphe ou déplacer un volume ne relance plus rien. Pour régénérer : bouton Generate du composant, ou `PCGToolset.ExecuteGraphInstance`, en prévenant les autres sessions.
  - Pour mettre à jour :
    1. relancer `place_vegetation.py` ;
    2. supprimer puis réimporter les `DT_Raraka_*_NN` ;
    3. si le nombre de blocs change, créer ou supprimer les acteurs (`SpawnGraphInstance` avec `jsonParams={"DataTable": …}`) ;
    4. exécuter chaque bloc.
- Les meshes FBX s'importent avec `VertexColorImportOption = Replace`. Le MCP ne l'expose pas : régler le CDO `Default__FbxStaticMeshImportData` avant l'import.

## Ce qui reste (voir `/chantier`)
Masques plus fins au village · eau (répétition de la normal map, écume sur la crête récifale) · World Partition.
