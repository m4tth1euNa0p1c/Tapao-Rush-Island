# Format `.tmk` — grille de terrain Tidemark (schéma 1)

Un `.tmk` est un **dossier** (par convention `<Nom>.tmk/`) qui contient un `manifest.json` et un fichier binaire par couche. Il décrit la grille de hauteurs qu'un `UTidemarkTerrainAsset` embarque, dans un format lisible sans Unreal : `Tools/tmk.py` (numpy seulement) le lit et l'écrit, `Tidemark::Format::SaveTmk` / `LoadTmk` (module `TidemarkRuntime`, `Grid/TidemarkFormat.h`) font de même dans le moteur.

```
Island.tmk/
  manifest.json      en-tête, couches, SHA-256, crédits
  elevation_cm.bin   int16 little-endian, SizeX * SizeY valeurs
  flags.bin          uint8, SizeX * SizeY valeurs
```

## Grille
- Cellules carrées de `cell_size_cm` cm. `size = [SizeX, SizeY]`.
- Les couches sont **ligne par ligne** (row-major) : l'indice de la cellule `(x, y)` est `y * SizeX + x`, `x` varie le plus vite.
- `origin_cm = [X, Y]` est la position monde (cm, plan XY du niveau) du coin inférieur gauche de la cellule `(0, 0)`. La valeur d'une cellule est portée par son **centre** : `centre(x, y) = origin + (x + 0,5, y + 0,5) * cell_size_cm`. L'échantillonnage bilinéaire interpole entre centres et se borne aux bords.
- `crs` : identifiant de projection optionnel (`"EPSG:32706"` par exemple) pour l'export vers un SIG ; `null` quand la grille n'est qu'en coordonnées monde.
- Budget : `SizeX * SizeY <= 5 592 405` cellules (16 Mio à 3 octets par cellule). Les lecteurs refusent au-delà.

## Couches (schéma 1)
| Nom | Type | Fichier | Contenu |
|---|---|---|---|
| `elevation_cm` | `int16` LE | `elevation_cm.bin` | altitude du sol en cm, de −32 768 à +32 767 (±327 m) |
| `flags` | `uint8` | `flags.bin` | masque de bits : `1` OpenSea (surface tenue au niveau de la mer, alimente la crue), `2` Border (bord de grille, même traitement), `4` Forbidden (refusé par le lint, ignoré par le solveur) |

Réservations du schéma 2, une couche chacune, même disposition : `roughness` (uint8, Manning n × 1000), `veg_cover` (uint8, 0–255 = 0–100 %), `soil` (uint8, énumération), `wind_exposure` (8 × uint8 par secteur). Leur ajout incrémente `schema_version` ; un lecteur refuse un schéma qu'il ne connaît pas.

## `manifest.json`
```json
{
  "schema_version": 1,
  "generator": "Tidemark/tmk.py",
  "origin_cm": [0.0, 0.0],
  "cell_size_cm": 400.0,
  "size": [512, 512],
  "crs": null,
  "layers": [
    {"name": "elevation_cm", "type": "int16", "file": "elevation_cm.bin", "sha256": "<64 hex>"},
    {"name": "flags",        "type": "uint8", "file": "flags.bin",        "sha256": "<64 hex>"}
  ],
  "source_hash": "2cc618ee4cf82873",
  "credits": [
    {"producer": "…", "license": "…", "url": "…", "date": "YYYY-MM-DD"}
  ]
}
```
- `layers[].sha256` : SHA-256 du fichier de la couche, en minuscules. Vérifié à la lecture ; toute différence est une erreur.
- `layers[].file` : nom de fichier simple, sans séparateur ni `..` (le lecteur refuse tout chemin qui sortirait du dossier).
- `source_hash` : FNV-1a 64 bits (16 hex) du contenu de la grille, identique à `FTidemarkGrid::ComputeSourceHash()` et à `tmk.source_hash()`. Ordre fixe, octets little-endian explicites : `origin.X` (double), `origin.Y` (double), `cell_size_cm` (float), `SizeX` (int32), `SizeY` (int32), puis chaque `elevation_cm` (int16) et chaque octet de `flags`. C'est le « hash de grille » qui, avec la seed, l'échéancier et le journal, rend un scénario reproductible. Vérifié à la lecture quand il est présent.
- `credits[]` : **obligatoire, au moins une entrée, les quatre champs `producer`, `license`, `url`, `date` non vides** (conventions §7). `SaveTmk` et `tmk.save` refusent d'écrire sans ; `scripts/tidemark/check_credits.py` refuse en CI tout manifest incomplet. Un relief estimé se déclare comme tel (`producer: "profils estimés — Tidemark"`).

## Outil Python
```
python3 -I monke_island/Plugins/Tidemark/Tools/tmk.py info Island.tmk
```
Affiche dimensions, emprise, min / médiane / max, comptes des drapeaux, histogramme ASCII des altitudes, `source_hash` vérifié et crédits. Dans un script : `grid, credits = tmk.load(dir)` (`grid["elevation_cm"]` est un `np.int16` de forme `(SizeY, SizeX)`), `tmk.save(dir, elevation_cm, flags, origin_cm, cell_size_cm, credits, crs=None)`. Tests : `python3 -I monke_island/Plugins/Tidemark/Tools/tests/test_tmk.py`.

## Garanties
- Aucune dépendance à l'endianness de la machine : les écrivains sérialisent octet par octet, les lecteurs désérialisent de même.
- Un dossier corrompu (manifest absent ou malformé, schéma inconnu, taille de couche fausse, SHA-256 ou `source_hash` différent) est refusé avec un message explicite ; la grille de sortie est vide.
- Mention à reprendre dans tout rapport produit à partir d'une grille : « Simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation. »
