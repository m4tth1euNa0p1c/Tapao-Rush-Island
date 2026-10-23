---
name: tidemark-terrain-import
description: Bake un terrain vers une grille Tidemark (UTidemarkTerrainAsset et fichier .tmk) depuis un Landscape, une heightmap (R16, PNG 16 bits, EXR) ou des meshes de collision (rayons par lots, une fois), puis vérifie la grille (dimensions, plages, mer ouverte, hash des sources, crédits) et mesure le temps d'import. À utiliser pour « importe ce terrain », « bake la grille de L_Frag_Raraka_01 », « la grille est fausse ».
argument-hint: "<landscape <Map> | heightmap <fichier> --min-cm <a> --max-cm <b> | collision <Map> [--bounds] | verify <asset|.tmk>>"
---

# Import de terrain Tidemark

Arguments : `$ARGUMENTS`. Rôle : `tidemark-terrain-programmer`. Spéc du format : `Plugins/Tidemark/Docs/TMK.md`. Conventions §4 et §7.

## Avant
- Une opération dans l'éditeur tient la ressource exclusive : `python scripts/editor_lock.py acquire --owner <session> --reason "tidemark import"` et `owner=editor` dans la note du chantier.
- Pas de données interdites en entrée (Maxar, DAG, IRD sans licence, Google). Si la source est réelle : `credits[]` prêt (producteur, licence, URL, date).

## `landscape <Map>`
1. Ouvrir la map (MCP `unreal`, `EditorToolset`), trouver l'`ALandscape`, lire ses composants via `FLandscapeHeightSource`.
2. Pas de grille : `UTidemarkRules.GridCellCm` (600 par défaut) ; bornes = bounds du Landscape + marge `OpenSeaMarginCm`.
3. `ElevationCm` = hauteur du Landscape en cm monde ; `Flags.OpenSea` = cellules sous `SeaLevelCm − OpenSeaDepthCm` ; `Border` = bord.
4. Asset `UTidemarkTerrainAsset` sauvé sous `/Tidemark/Terrains/TA_<Map>` (ou `/Game/<Projet>/Tidemark/` chez un utilisateur), `SourceHash` = SHA-256 des données de hauteur, `Credits` = ceux de la map.
5. Chronométrer : objectif < 60 s pour 2 km.

## `heightmap <fichier> --min-cm --max-cm`
R16 brut (largeur × hauteur déduites ou passées), PNG 16 bits, EXR float. Valeur normalisée → `MinCm + v × (MaxCm − MinCm)`. Objectif < 5 s. Même asset et même vérification.

## `collision <Map> [--bounds]`
Rayons par lots asynchrones sur un canal dédié (`ECC_GameTraceChannel` réservé par le plugin, ou `ECC_WorldStatic` avec filtre), du `TraceTopCm` au `TraceBottomCm` des règles, **une fois**, en ignorant les ISM de végétation et les surfaces d'eau taguées ; « plus haut sol sous le niveau max ». C'est ce que `FloodSimulationActor::ReadGroundBatch` faisait à chaque chargement : ici le résultat est sauvé dans l'asset. Objectif : `L_Frag_Raraka_01` (14 tuiles Nanite) en < 2 min.

## `verify <asset|.tmk>`
1. Dimensions et pas cohérents ; `ElevationCm` sans `INT16_MIN/MAX` sauf bord ; part de cellules `OpenSea` plausible (> 5 % pour une île).
2. Histogramme d'altitude (ASCII), min/max, médiane de la terre.
3. `SourceHash` recalculé = stocké (sinon `GridStale`).
4. `credits[]` complet (4 champs par entrée) ; sinon échec.
5. Pour l'île de démo : terre de la grille conforme à ≥ 95 % à la côte Etalab (`scripts/tidemark/demo_island.py --check`).
6. Export/réimport `.tmk` via `Tools/tmk.py` → même hash.
Rapport : temps, taille (Mo), histogramme, verdicts, niveau de preuve `testé éditeur`.

## Pièges
- Le pas de la grille de simulation (600 cm) et celui d'une grille fine (400 cm) : garder un multiple entier.
- Un Landscape avec `ZScale` non standard : lire la transformation du composant, pas la valeur brute.
- Surplombs et grottes : le mode collision prend le plus haut sol ; documenter.
- Libérer le verrou éditeur à la fin.
