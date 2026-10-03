"""Telecharge une mosaique satellite Te Fenua (WMTS IMAGE_PUBLIQUE, EPSG:900913) sur une emprise.

Usage: python scripts/map/fetch_imagery.py <out_png> <zoom> <xmin> <ymin> <xmax> <ymax> [layer]
(layer par defaut : TEFENUA:IMAGE_PUBLIQUE ; ex. TEFENUA:std_atlasrecif pour l'atlas des recifs IRD)
Ecrit <out_png> + <out_png>.json (emprise exacte en lon/lat et en metres Web Mercator).
Tuiles mises en cache dans docs/reference/map/raw/tiles/<zoom>/.
Imagerie : (c) Maxar / DigitalGlobe via Te Fenua - usage reference uniquement.
"""
import io
import json
import math
import sys
import time
import urllib.request
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
CACHE = ROOT / "docs" / "reference" / "map" / "raw" / "tiles"
URL = ("https://www.tefenua.gov.pf/api/wmts?SERVICE=WMTS&REQUEST=GetTile&VERSION=1.0.0"
       "&LAYER={layer}&STYLE=&TILEMATRIXSET=EPSG:900913"
       "&TILEMATRIX=EPSG:900913:{z}&TILEROW={y}&TILECOL={x}&FORMAT=image/png8")
TILE = 256
DEFAULT_LAYER = "TEFENUA:IMAGE_PUBLIQUE"
R = 6378137.0


def lonlat_to_tile(lon, lat, z):
    n = 2 ** z
    x = (lon + 180.0) / 360.0 * n
    y = (1.0 - math.asinh(math.tan(math.radians(lat))) / math.pi) / 2.0 * n
    return x, y


def tile_to_merc(x, y, z):
    """Coin haut-gauche d'une tuile en metres Web Mercator."""
    n = 2 ** z
    span = 2 * math.pi * R
    return x / n * span - span / 2, span / 2 - y / n * span


def fetch(z, x, y, layer=DEFAULT_LAYER):
    # cache historique de l'imagerie dans tiles/<z>/, autres couches dans tiles/<couche>/<z>/
    base = CACHE if layer == DEFAULT_LAYER else CACHE / layer.split(":")[-1]
    path = base / str(z) / f"{x}_{y}.png"
    if path.exists():
        return Image.open(path).convert("RGB")
    path.parent.mkdir(parents=True, exist_ok=True)
    for attempt in range(4):
        try:
            req = urllib.request.Request(URL.format(z=z, x=x, y=y, layer=layer), headers={"User-Agent": "monke-island-mapping/1.0"})
            with urllib.request.urlopen(req, timeout=60) as r:
                data = r.read()
            path.write_bytes(data)
            return Image.open(io.BytesIO(data)).convert("RGB")
        except Exception as e:  # noqa: BLE001 - retry any network error
            if attempt == 3:
                print(f"tuile {z}/{x}/{y} echec: {e}")
                return Image.new("RGB", (TILE, TILE))
            time.sleep(2 * (attempt + 1))


def main():
    out = Path(sys.argv[1])
    z = int(sys.argv[2])
    xmin, ymin, xmax, ymax = map(float, sys.argv[3:7])
    layer = sys.argv[7] if len(sys.argv) > 7 else DEFAULT_LAYER
    x0, y0 = (int(v) for v in lonlat_to_tile(xmin, ymax, z))
    x1, y1 = (int(v) for v in lonlat_to_tile(xmax, ymin, z))
    cols, rows = x1 - x0 + 1, y1 - y0 + 1
    print(f"z{z}: {cols}x{rows} = {cols * rows} tuiles -> {cols * TILE}x{rows * TILE} px")
    mosaic = Image.new("RGB", (cols * TILE, rows * TILE))
    jobs = [(x, y) for y in range(y0, y1 + 1) for x in range(x0, x1 + 1)]
    with ThreadPoolExecutor(max_workers=6) as pool:
        for (x, y), img in zip(jobs, pool.map(lambda j: fetch(z, *j, layer), jobs)):
            mosaic.paste(img, ((x - x0) * TILE, (y - y0) * TILE))
    out.parent.mkdir(parents=True, exist_ok=True)
    mosaic.save(out)
    mx0, my0 = tile_to_merc(x0, y0, z)
    mx1, my1 = tile_to_merc(x1 + 1, y1 + 1, z)
    meta = {
        "zoom": z, "width": mosaic.width, "height": mosaic.height,
        "merc_bounds": [mx0, my1, mx1, my0],
        "merc_px": (mx1 - mx0) / mosaic.width,
        "layer": layer,
        "credits": "(c) Maxar/DigitalGlobe via Te Fenua (tefenua.gov.pf)" if layer == DEFAULT_LAYER
                   else "Atlas des recifs coralliens de Polynesie francaise (IRD, 2005) via Te Fenua",
    }
    Path(str(out) + ".json").write_text(json.dumps(meta, indent=2))
    print(json.dumps(meta, indent=2))


if __name__ == "__main__":
    main()
