"""Calque des parcelles cadastrales pour Unreal : rubans fins draperes sur le terrain + index des parcelles.

Usage: python scripts/map/build_parcels.py <ile> [--width 0.5] [--lift 0.2]

Entrees : docs/reference/map/<ile>/parcelles.geojson, terrain/georef.json, terrain/heightmap_16.png
Sorties : docs/reference/map/raw/<ile>_terrain/SM_<Ile>_Parcelles.obj (meme repere que le terrain)
          docs/reference/map/<ile>/terrain/parcelles_ue.json (identifiant, section, surface, centre en cm UE)
Les parcelles open data ne contiennent ni noms de terre ni proprietaires.
"""
import argparse
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_terrain import ROOT, merc, write_obj  # noqa: E402

Image.MAX_IMAGE_PIXELS = None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("island")
    parser.add_argument("--width", type=float, default=0.5, help="largeur du ruban (m)")
    parser.add_argument("--lift", type=float, default=0.2, help="hauteur au-dessus du sol (m)")
    parser.add_argument("--step", type=float, default=4.0, help="densification des segments (m)")
    args = parser.parse_args()

    ref = ROOT / "docs" / "reference" / "map" / args.island
    georef = json.loads((ref / "terrain" / "georef.json").read_text(encoding="utf-8"))
    grid = georef["source_grid"]
    W, H, px = grid["width"], grid["height"], grid["merc_px"]
    xmin, _, _, ymax = grid["merc_bounds"]
    m_px = georef["ground_m_per_px"]
    cx, cz = W * m_px / 2, H * m_px / 2
    hm = georef["heightmap_16"]
    heights = np.asarray(Image.open(ref / "terrain" / hm["file"])).astype(np.float32)
    heights = hm["min_m"] + (hm["max_m"] - hm["min_m"]) * heights / 65535.0

    def to_local(lon, lat):
        mx, my = merc(np.asarray(lon), np.asarray(lat))
        col, row = (mx - xmin) / px, (ymax - my) / px
        return col * m_px - cx, row * m_px - cz, col, row          # est (m), sud (m), colonne, ligne

    def height_at(col, row):
        c = np.clip(col, 0, W - 1.001)
        r = np.clip(row, 0, H - 1.001)
        c0, r0 = c.astype(int), r.astype(int)
        fc, fr = c - c0, r - r0
        top = heights[r0, c0] * (1 - fc) + heights[r0, c0 + 1] * fc
        bottom = heights[r0 + 1, c0] * (1 - fc) + heights[r0 + 1, c0 + 1] * fc
        return top * (1 - fr) + bottom * fr

    data = json.loads((ref / "parcelles.geojson").read_text(encoding="utf-8"))
    positions, indices, index = [], [], []
    half = args.width / 2
    for feat in data["features"]:
        geom, props = feat["geometry"], feat["properties"]
        polys = geom["coordinates"] if geom["type"] == "MultiPolygon" else [geom["coordinates"]]
        centroid = []
        for poly in polys:
            for ring in poly:
                pts = np.asarray(ring, dtype=np.float64)
                east, south, _, _ = to_local(pts[:, 0], pts[:, 1])
                centroid.append(np.stack([east, south], -1))
                for i in range(len(pts) - 1):
                    p0, p1 = np.array([east[i], south[i]]), np.array([east[i + 1], south[i + 1]])
                    seg = p1 - p0
                    length = float(np.hypot(*seg))
                    if length < 1e-3:
                        continue
                    n = max(1, int(np.ceil(length / args.step)))
                    t = np.linspace(0, 1, n + 1)
                    line = p0[None] + seg[None] * t[:, None]
                    perp = np.array([-seg[1], seg[0]]) / length * half
                    cols = (line[:, 0] + cx) / m_px
                    rows = (line[:, 1] + cz) / m_px
                    z = np.maximum(height_at(cols, rows), 0.0) + args.lift
                    left, right = line + perp, line - perp
                    base = len(positions)
                    for k in range(n + 1):
                        positions.append([left[k, 0] * 100, -left[k, 1] * 100, z[k] * 100])
                        positions.append([right[k, 0] * 100, -right[k, 1] * 100, z[k] * 100])
                    for k in range(n):
                        a = base + 2 * k
                        indices += [a, a + 2, a + 1, a + 1, a + 2, a + 3]
        if centroid:
            c = np.concatenate(centroid).mean(0)
            index.append({"id": props.get("iu_parcelle") or props.get("Identifiant_complet"),
                          "section": props.get("section"), "numero": props.get("numero"),
                          "surface_m2": props.get("surface_m2"),
                          "center_ue_cm": {"x": round(float(c[0]) * 100, 1), "y": round(float(c[1]) * 100, 1)}})

    pos = np.asarray(positions, dtype=np.float64)
    idx = np.asarray(indices, dtype=np.int64)
    name = f"SM_{args.island.capitalize()}_Parcelles"
    out_mesh = ROOT / "docs" / "reference" / "map" / "raw" / f"{args.island}_terrain" / f"{name}.obj"
    write_obj(out_mesh, pos, np.zeros((len(pos), 2)), idx, name, "M_Parcelles")
    (ref / "terrain" / "parcelles_ue.json").write_text(json.dumps(index, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"{len(index)} parcelles, {len(pos)} sommets, {len(idx) // 3} triangles -> {out_mesh}")


if __name__ == "__main__":
    main()
