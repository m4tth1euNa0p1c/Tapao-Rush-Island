"""Map of the French fort sites on the atoll (owner 2026-10-08: "crée une map"): the land of Raraka from the heightmap,
the villages, the arrival of each fragment and the fort sites of docs/reference/map/<ile>/collectibles/fort_sites.json
(squares turned to the fort's yaw, the gate's side marked), labelled by fragment.

Usage: python scripts/map/fort_sites_map.py raraka  ->  docs/reference/map/<ile>/collectibles/fort_sites.png
"""
import json
import math
import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

ROOT = Path(__file__).resolve().parents[2]
SIZE = 2400
FORT_HALF_M = 41.0      # the moat bank of the land fort, 81 x 73 m


def main():
    ile = sys.argv[1] if len(sys.argv) > 1 else "raraka"
    base = ROOT / "docs/reference/map" / ile
    georef = json.loads((base / "terrain/georef.json").read_text(encoding="utf-8"))
    heights = np.asarray(Image.open(base / "terrain/heightmap_16.png"), dtype=np.float64)
    ex, ey = georef["extent_m"]
    hmin, hmax = georef["heightmap_16"]["min_m"], georef["heightmap_16"]["max_m"]
    metres = hmin + heights / 65535.0 * (hmax - hmin)
    scale = SIZE / max(ex, ey)
    w, h = int(ex * scale), int(ey * scale)
    land = np.asarray(Image.fromarray((metres > 0.2).astype(np.uint8) * 255).resize((w, h), Image.BILINEAR)) > 127
    img = np.zeros((h, w, 3), dtype=np.uint8)
    img[...] = (32, 92, 128)
    img[land] = (214, 200, 150)
    canvas = Image.fromarray(img)
    draw = ImageDraw.Draw(canvas)

    def px(x_cm, y_cm):
        return ((x_cm / 100.0 + ex / 2) * scale, (y_cm / 100.0 + ey / 2) * scale)

    index = base / "villages" / "fragments" / "index.json"
    if index.exists():
        for frag in json.loads(index.read_text(encoding="utf-8")).get("fragments", []):
            for v in frag.get("villages", []):
                x, y = px(*v["center_cm"][:2])
                r = max(3.0, v.get("radius_cm", 9000.0) / 100.0 * scale)
                draw.ellipse([x - r, y - r, x + r, y + r], outline=(60, 120, 60), width=2)
    fragments = json.loads((base / "fragments" / "fragments.json").read_text(encoding="utf-8")).get("fragments", [])
    for frag in fragments:
        arr = (frag.get("arrival") or {}).get("location_cm")
        if arr:
            x, y = px(arr[0], arr[1])
            draw.ellipse([x - 5, y - 5, x + 5, y + 5], fill=(40, 160, 255))
    sites = json.loads((base / "collectibles" / "fort_sites.json").read_text(encoding="utf-8"))["by_fragment"]
    half = max(FORT_HALF_M * scale, 5.0)
    for fragment_id, spots in sites.items():
        for i, (x_cm, y_cm, _z, yaw, *_rest) in enumerate(spots):
            x, y = px(x_cm, y_cm)
            a = math.radians(yaw)
            corners = []
            for cx, cy in ((-1, -1), (1, -1), (1, 1), (-1, 1)):
                corners.append((x + (cx * math.cos(a) - cy * math.sin(a)) * half, y + (cx * math.sin(a) + cy * math.cos(a)) * half))
            draw.polygon(corners, fill=(200, 40, 40) if i == 0 else (240, 120, 60), outline=(90, 0, 0))
            gx, gy = x - math.sin(a) * half * 2.2, y + math.cos(a) * half * 2.2
            draw.line([x, y, gx, gy], fill=(90, 0, 0), width=2)
        if spots:
            x, y = px(spots[0][0], spots[0][1])
            draw.text((x + half + 4, y - 6), fragment_id.replace("FRG_Raraka_", "F"), fill=(20, 20, 20))
    draw.text((12, 12), "Raraka - forts francais (rouge: 1er site, orange: autres; trait: porte), arrivees (bleu), villages (vert)",
              fill=(255, 255, 255))
    out = base / "collectibles" / "fort_sites.png"
    canvas.save(out)
    print(out)


if __name__ == "__main__":
    main()
