---
name: map-geodata
description: Récupère les données géographiques réelles d'une île de Polynésie (parcelles cadastrales, trait de côte, bâti, cocoteraies, occupation du sol, toponymes, imagerie satellite Te Fenua, OSM) pour modéliser la map. À utiliser pour une nouvelle île / zone ou pour rafraîchir les données de référence.
argument-hint: "<ile> <xmin> <ymin> <xmax> <ymax>"
---

# Données géographiques réelles

Arguments : $ARGUMENTS (emprise en degrés WGS84 : lon min, lat min, lon max, lat max)

## Sources vérifiées
Base ArcGIS Te Fenua : `https://services.arcgis.com/L4H4rU26VyhEF588/arcgis/rest/services`

| Donnée | Service / couche | Notes |
|--------|------------------|-------|
| Parcelles cadastrales (DAF) | `CAD_parcelles_open/FeatureServer/0` | section, numéro, surface ; **pas** de noms de terre ni de propriétaires |
| Trait de côte | `Ligne_de_rivage_Polynésie_française/FeatureServer/0` | MultiLineString : un contour fermé par motu |
| Bâti BD TOPO | `Bâtiment_BD_TOPO_Edification/FeatureServer/17` | |
| Cocoteraies (DAG) | `DAG_-_Inventaire_des_cocoteraies_de_Polynésie_Française_vue/FeatureServer/0` | polygones |
| Occupation du sol | `Occupation_du_sol_BD_TOPO/FeatureServer/18` | |
| Toponymes | `od_toponyme_topo/FeatureServer/0` | champ `nom` |
| Emprises ortho | `orthomosaique_BD_Raster_diffusion/FeatureServer/0` | date, résolution, crédits de chaque mosaïque |
| Imagerie satellite | WMTS `https://www.tefenua.gov.pf/api/wmts`, couche `TEFENUA:IMAGE_PUBLIQUE`, TileMatrixSet `EPSG:900913` | tuiles 256 px PNG |
| Atlas des récifs (IRD 2005) | WMTS couche `TEFENUA:std_atlasrecif` | raster uniquement (le service vectoriel `reef_2014` ne couvre pas Raraka) |
| OSM | Overpass `https://overpass-api.de/api/interpreter` | POI du village ; noms locaux souvent absents |

Liste complète des couches WMTS : `https://www.tefenua.gov.pf/api/wmts?SERVICE=WMTS&REQUEST=GetCapabilities`.

## Commandes
```bash
python scripts/map/fetch_tefenua.py docs/reference/map/<ile> <xmin> <ymin> <xmax> <ymax>
python scripts/map/fetch_imagery.py docs/reference/map/<ile>/raw_sat_z15.png 15 <xmin> <ymin> <xmax> <ymax>
```
Les tuiles sont mises en cache dans `docs/reference/map/raw/tiles/` (ignoré par git). z15 ≈ 4,8 m/px Mercator, soit ≈ 4,6 m au sol à 16° S ; z17 ≈ 1,15 m au sol, à réserver aux zones de détail (village).

## Après récupération
1. Fiche `docs/reference/map/<ile>/README.md` : emprise, sources, dates, nombres d'entités, crédits.
2. Aperçu réduit de la mosaïque (`preview_sat.jpg`) : **le regarder** (nuages, scènes juxtaposées, zones nodata blanches hors emprise ortho).

## Licences
- Données Te Fenua / DAF : Licence Ouverte Etalab 2.0, citer « Te Fenua / DAF – Polynésie française ».
- Imagerie : © Maxar / DigitalGlobe via Te Fenua → **référence de modélisation uniquement**, ne pas l'embarquer telle quelle dans le jeu livré.
- OSM : ODbL, citer « © OpenStreetMap contributors ».

## Pièges connus
- Nominatim ne connaît pas tous les lieux ; la base cadastrale n'a pas de noms de terre (lieux-dits).
- Overpass peut renvoyer une page HTML d'erreur (surcharge) : réessayer ou changer de miroir, et lire le JSON en UTF-8.
- La mosaïque juxtapose plusieurs scènes : la couleur absolue n'est pas comparable d'une zone à l'autre.
