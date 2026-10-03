"""Telecharge les couches open data Te Fenua (ArcGIS FeatureServer) sur une emprise, en GeoJSON.

Usage: python scripts/map/fetch_tefenua.py <out_dir> <xmin> <ymin> <xmax> <ymax>
Licence des donnees : Licence Ouverte Etalab 2.0 (DAF / Pays de Polynesie francaise).
"""
import json
import sys
import urllib.parse
import urllib.request
from pathlib import Path

BASE = "https://services.arcgis.com/L4H4rU26VyhEF588/arcgis/rest/services"
LAYERS = {
    "parcelles": ("CAD_parcelles_open", 0),
    "rivage": ("Ligne_de_rivage_Polynésie_française", 0),
    "cocoteraies": ("DAG_-_Inventaire_des_cocoteraies_de_Polynésie_Française_vue", 0),
    "batiments": ("Bâtiment_BD_TOPO_Edification", 17),
    "occupation_sol": ("Occupation_du_sol_BD_TOPO", 18),
    "toponymes": ("od_toponyme_topo", 0),
    "ortho_emprises": ("orthomosaique_BD_Raster_diffusion", 0),
}


def query(service, layer, bbox, offset):
    params = {
        "where": "1=1",
        "geometry": ",".join(map(str, bbox)),
        "geometryType": "esriGeometryEnvelope",
        "inSR": 4326,
        "spatialRel": "esriSpatialRelIntersects",
        "outFields": "*",
        "outSR": 4326,
        "resultOffset": offset,
        "resultRecordCount": 1000,
        "f": "geojson",
    }
    url = f"{BASE}/{urllib.parse.quote(service)}/FeatureServer/{layer}/query"
    data = urllib.parse.urlencode(params).encode()
    with urllib.request.urlopen(url, data=data, timeout=120) as r:
        return json.load(r)


def main():
    out = Path(sys.argv[1])
    bbox = [float(v) for v in sys.argv[2:6]]
    out.mkdir(parents=True, exist_ok=True)
    for name, (service, layer) in LAYERS.items():
        feats, offset = [], 0
        while True:
            page = query(service, layer, bbox, offset)
            if "error" in page:
                print(f"{name}: ERREUR {page['error']}")
                break
            batch = page.get("features", [])
            feats += batch
            if len(batch) < 1000:
                break
            offset += len(batch)
        (out / f"{name}.geojson").write_text(
            json.dumps({"type": "FeatureCollection", "features": feats}, ensure_ascii=False), encoding="utf-8"
        )
        print(f"{name}: {len(feats)} entites")


if __name__ == "__main__":
    main()
