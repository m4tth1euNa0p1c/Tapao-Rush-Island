# SPEC-002 — Pipeline de données ouvertes (`scripts/envgrid/`)

- **Statut** : brouillon · **Lot** : V1.1 avancé, lancé en parallèle de S3 parce qu'il ne dépend pas d'Unreal · **Tickets** : TM-42 (découpé ci-dessous) · **Rôles** : `tidemark-terrain-programmer`, `tidemark-qa`, `tidemark-release-engineer` (licences).

## 1. Intention
Permettre à un utilisateur de produire, **sous sa responsabilité et sur sa machine**, une grille `.tmk` d'une emprise réelle à partir de sources ouvertes, avec les crédits exigés. Le plugin ne contient aucune donnée ; ce script est libre (MIT), hors Fab. Première emprise : un atoll des Tuamotu, **sans toponyme**, pour l'île de démo « inspirée de ».

## 2. Contrat public
- Commande : `python -I scripts/envgrid/build_envgrid.py --bbox <lonmin latmin lonmax latmax> --cell-m 6 --out <dir.tmk> [--name <id fictif>] [--coast <geojson Etalab>] [--no-bathy]`.
- Entrées : variables d'environnement `CDSE_CLIENT_ID`, `CDSE_CLIENT_SECRET` (OAuth2 Copernicus Data Space Ecosystem) ou `CDSE_S3_ACCESS_KEY`/`CDSE_S3_SECRET_KEY` ; `EARTHDATA_TOKEN` (ICESat-2, optionnel). **Le script refuse de démarrer sans les clés requises et n'écrit jamais leur valeur** (ni log, ni manifest, ni message d'erreur).
- Sources, dans l'ordre : Copernicus DEM GLO-30 (altitude), ICESat-2 ATL08 (calage vertical, optionnel), Sentinel-2 L2A (bathymétrie dérivée, optionnel, ≤ −20 m), ESA WorldCover 10 m (couverture, réservé V2), trait de côte Te Fenua (GeoJSON fourni par l'utilisateur, Etalab 2.0).
- Sortie : dossier `.tmk` conforme à `Plugins/Tidemark/Docs/TMK.md` (`manifest.json`, `elevation_cm.bin`, `flags.bin`), `credits[]` complet par source (producteur, licence, URL, date de téléchargement), champ `crs` renseigné (UTM 6S/7S), et `REPORT.md` (sources, emprise, stats, incertitude verticale estimée, mention non désactivable).
- Dépendances Python : `numpy`, `rasterio`, `pyproj`, `shapely`, `requests` (dans `scripts/envgrid/requirements.txt`). Pas de GDAL en ligne de commande requis.

## 3. Règles
- Jamais Maxar, DAG, raster IRD, Google 3D Tiles ; OSM exclu des grilles redistribuées tant que l'ODbL n'est pas tranchée.
- Crédits : Copernicus DEM → « © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH 2014-2018 provided under COPERNICUS by the European Union and ESA; all rights reserved » + déni de responsabilité ; WorldCover → « © ESA WorldCover project 2021 / Contains modified Copernicus Sentinel data (2021) processed by ESA WorldCover consortium », CC BY 4.0 ; ICESat-2 → NASA NSIDC, domaine public ; Te Fenua → « Gouvernement de la Polynésie française – Te Fenua, Licence Ouverte 2.0, <date> ». À vérifier à la source au premier téléchargement et consigner dans `REPORT.md`.
- Le relief d'un atoll est **sous le bruit du DEM** (2 à 4 m d'erreur pour 2 à 4 m de relief) : le script calcule et affiche une incertitude verticale, et `REPORT.md` porte « relief estimé, plausible, jamais une prévision ». Sans ICESat-2, la terre est **calée par profil** (comme `build_terrain.py` de Tapao, +0,5 à +3,7 m) contraint par la côte Etalab : documenté comme tel.
- Aucun toponyme habité dans la sortie ; `--name` est un identifiant fictif.
- Reproductible : même emprise, mêmes sources (versions datées), même sortie bit à bit ; `source_hash` identique.

## 4. Valeurs tunables (dans `scripts/envgrid/config.json`, pas en dur)
`cell_m` 6 · `land_min_cm` 50 · `land_max_cm` 370 · `shore_slope_max_deg` 7 · `open_sea_depth_cm` 150 · `lagoon_depth_cm` −800 à −3000 · `ocean_depth_cm` −30000 · `coast_match_min` 0.95.

## 5. Critères d'acceptation
1. Sans clés : exit 2, message « CDSE credentials missing », aucune requête réseau.
2. Avec clés : téléchargement Copernicus DEM pour la bbox, cache local (`.envgrid-cache/`, ignoré par Git), aucun secret dans les logs (`grep` du token sur la sortie → vide).
3. `.tmk` produit lisible par `Plugins/Tidemark/Tools/tmk.py info` et par `Tidemark::Format::LoadTmk` (même `source_hash`).
4. Terre de la grille conforme à ≥ 95 % à la côte Etalab fournie (IoU des masques terre).
5. Altitude médiane des motu entre 1 et 4 m après calage ; incertitude verticale rapportée.
6. `credits[]` complets, acceptés par `scripts/tidemark/check_credits.py` ; `REPORT.md` contient la mention non désactivable.
7. Deux exécutions → mêmes octets.
8. Aucune chaîne `Maxar`, `DigitalGlobe`, `DAG`, `IRD`, `Google` dans la sortie.

## 6. Preuve exigée
`testé hors moteur` : `python -m unittest scripts/tests/test_envgrid.py` (fixtures locales, réseau simulé) + une exécution réelle sur l'emprise de démo avec `REPORT.md` joint ; puis `testé éditeur` quand `tidemark.ImportTmk` charge la grille sur le PC.

## 7. Plan
| Tâche | Contenu | Rôle | Fichiers |
|---|---|---|---|
| TM-42a | squelette, config, lecture des clés depuis l'environnement, refus sans clés, cache, journalisation sans secret | terrain | `scripts/envgrid/build_envgrid.py`, `config.json`, `requirements.txt`, `.env.example` |
| TM-42b | client CDSE (OAuth2 / S3), téléchargement Copernicus DEM par bbox, reprojection UTM, rééchantillonnage `cell_m` | terrain | `scripts/envgrid/sources/copernicus_dem.py` |
| TM-42c | côte Etalab : masque terre, contrainte du relief, calage par profil ou ICESat-2 | terrain | `scripts/envgrid/sources/coastline.py`, `sources/icesat2.py` |
| TM-42d | écriture `.tmk` via `tmk.py`, `credits[]`, `REPORT.md`, incertitude | terrain | `scripts/envgrid/writer.py` |
| TM-42e | tests (réseau simulé, IoU, reproductibilité, secrets absents) et job CI `envgrid` | qa | `scripts/tests/test_envgrid.py`, `.github/workflows/tidemark.yml` |
| TM-42f | vérification des textes de licence à la source, `REPORT.md` de l'emprise de démo | release | `docs/licenses-data.md` |

## 8. Écarts et décisions
- 2026-10-24 : lancé avant la V1.1 prévue par le cahier, parce que le propriétaire dispose des clés et que le travail ne touche pas Unreal ; le plugin reste sans donnée embarquée.
