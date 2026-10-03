"""Candidate spots for the totems scattered on the sand of the island: rare collectible items, never in a bush.

Usage:
  python scripts/map/place_totems.py raraka [--spacing 60] [--seed 11]

A spot is a point of dry sand: the sand channel of the terrain masks is high and the vegetation/litter channel low,
the ground stands at least MIN_HEIGHT_M above the mean sea level (not the wet active beach), no bush or young palm
grows within BUSH_CLEAR_M and no trunk within TRUNK_CLEAR_M (vegetation_ue.json and palms_ue.json, before the
village clearing: more conservative), out of the villages (villages.json zones) and away from the real village of
Motutapu. The spots are thinned to SPACING_M apart (Poisson disk).
In game, AScatterSpawner draws a few of them each game (Count, MinSpacing of its DA_TotemScatter): the totems stay
rare and spread over the whole atoll.

Inputs (docs/reference/map/<ile>/): terrain/georef.json, terrain/T_Raraka_Masks.png, terrain/heightmap_16.png,
  vegetation/vegetation_ue.json, vegetation/palms_ue.json, villages/villages.json.
Outputs: collectibles/totem_spots.json (UE cm, Z = ground of the heightmap) and collectibles/preview_totems.png.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy.spatial import cKDTree

ROOT = Path(__file__).resolve().parents[2]

GRID_M = 8.0              # sampling step of the candidates (m)
MIN_HEIGHT_M = 0.8        # dry sand only: the active beach is lower (m above mean sea level)
SAND_MIN = 140            # sand channel of the masks (0-255)
LITTER_MAX = 80           # vegetation / litter channel of the masks (0-255)
BUSH_CLEAR_M = 3.0        # no bush (shrubs, thickets) closer than this (m)
TRUNK_CLEAR_M = 1.5       # no palm trunk or tree closer than this (m)
VILLAGE_MARGIN_M = 30.0   # beyond the pacification zone of a village (m)
MOTUTAPU_CLEAR_M = 300.0  # the real village stays untouched (m around its centre)
SPACING_M = 60.0          # spots at least this far apart (m)


def load(ile):
    base = ROOT / "docs/reference/map" / ile
    georef = json.loads((base / "terrain/georef.json").read_text(encoding="utf-8"))
    masks = np.asarray(Image.open(base / "terrain/T_Raraka_Masks.png").convert("RGBA"))
    heights = np.asarray(Image.open(base / "terrain/heightmap_16.png"), dtype=np.float64)
    veg = json.loads((base / "vegetation/vegetation_ue.json").read_text(encoding="utf-8"))
    palms = json.loads((base / "vegetation/palms_ue.json").read_text(encoding="utf-8"))
    villages_path = base / "villages/villages.json"
    villages = json.loads(villages_path.read_text(encoding="utf-8")) if villages_path.exists() else {}
    return base, georef, masks, heights, veg, palms, villages


def instances(groups, names):
    pts = []
    for group in names:
        for rows in groups.get(group, {}).values():
            pts.extend((r[0], r[1]) for r in rows)
    return np.array(pts, dtype=np.float64).reshape(-1, 2) / 100.0


def palm_points(palms):
    pts = []

    def walk(node):
        if isinstance(node, dict):
            for value in node.values():
                walk(value)
        elif isinstance(node, list):
            if node and all(isinstance(x, (int, float)) for x in node[:3]) and len(node) >= 3:
                pts.append((node[0], node[1]))
            else:
                for value in node:
                    walk(value)
    walk(palms.get("variants", palms))
    return np.array(pts, dtype=np.float64).reshape(-1, 2) / 100.0


def poisson_thin(points, spacing, rng):
    order = rng.permutation(len(points))
    kept = []
    tree_pts = []
    cell = spacing / np.sqrt(2.0)
    grid = {}
    for i in order:
        p = points[i]
        cx, cy = int(p[0] // cell), int(p[1] // cell)
        ok = True
        for dx in range(-2, 3):
            for dy in range(-2, 3):
                j = grid.get((cx + dx, cy + dy))
                if j is not None and np.hypot(*(tree_pts[j] - p)) < spacing:
                    ok = False
                    break
            if not ok:
                break
        if ok:
            grid[(cx, cy)] = len(tree_pts)
            tree_pts.append(p)
            kept.append(i)
    return np.array(kept, dtype=int)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ile")
    ap.add_argument("--spacing", type=float, default=SPACING_M)
    ap.add_argument("--seed", type=int, default=11)
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)

    base, georef, masks, heights, veg, palms, villages = load(args.ile)
    ex, ey = georef["extent_m"]
    hmin, hmax = georef["heightmap_16"]["min_m"], georef["heightmap_16"]["max_m"]
    H, W = heights.shape
    MH, MW = masks.shape[:2]

    # candidates on a grid over the extent (UE frame: origin at the centre, X east, Y south, metres here)
    xs = np.arange(-ex / 2 + GRID_M / 2, ex / 2, GRID_M)
    ys = np.arange(-ey / 2 + GRID_M / 2, ey / 2, GRID_M)
    gx, gy = np.meshgrid(xs, ys)
    u = (gx + ex / 2) / ex
    v = (gy + ey / 2) / ey
    hpx = np.clip((u * W).astype(int), 0, W - 1), np.clip((v * H).astype(int), 0, H - 1)
    mpx = np.clip((u * MW).astype(int), 0, MW - 1), np.clip((v * MH).astype(int), 0, MH - 1)
    height = hmin + heights[hpx[1], hpx[0]] / 65535.0 * (hmax - hmin)
    sand = masks[mpx[1], mpx[0], 1]
    litter = masks[mpx[1], mpx[0], 0]
    ok = (height >= MIN_HEIGHT_M) & (sand >= SAND_MIN) & (litter <= LITTER_MAX)
    cand = np.stack([gx[ok], gy[ok]], axis=1)
    cand_h = height[ok]
    stats = {"sand_points": int(ok.sum())}

    # no bush, no trunk around
    bushes = instances(veg["groups"], ["Shrubs", "Thicket"])
    trunks = np.concatenate([instances(veg["groups"], ["Trees"]), palm_points(palms)])
    for pts, clear, key in ((bushes, BUSH_CLEAR_M, "after_bushes"), (trunks, TRUNK_CLEAR_M, "after_trunks")):
        if len(pts):
            d, _ = cKDTree(pts).query(cand, k=1)
            keep = d >= clear
            cand, cand_h = cand[keep], cand_h[keep]
        stats[key] = len(cand)

    # out of the villages and of Motutapu
    keep = np.ones(len(cand), dtype=bool)
    for village in villages.get("villages", []):
        center = np.array(village.get("center_cm", village.get("zone", {}).get("center_cm", [0, 0]))[:2], float) / 100.0
        radius = village.get("zone", {}).get("radius_cm", 9000.0) / 100.0 + VILLAGE_MARGIN_M
        keep &= np.hypot(*(cand - center).T) > radius
    mot = georef["points_ue"]["village_motutapu"]
    keep &= np.hypot(cand[:, 0] - mot["x_cm"] / 100.0, cand[:, 1] - mot["y_cm"] / 100.0) > MOTUTAPU_CLEAR_M
    cand, cand_h = cand[keep], cand_h[keep]
    stats["after_villages"] = len(cand)

    kept = poisson_thin(cand, args.spacing, rng)
    spots = [[round(float(cand[i, 0] * 100.0), 1), round(float(cand[i, 1] * 100.0), 1), round(float(cand_h[i] * 100.0), 1)] for i in kept]
    stats["spots"] = len(spots)

    out = base / "collectibles"
    out.mkdir(exist_ok=True)
    (out / "totem_spots.json").write_text(json.dumps({
        "island": args.ile,
        "frame": "UE cm: X east, Y south, Z = ground of the heightmap (mean sea level 0)",
        "params": {"grid_m": GRID_M, "min_height_m": MIN_HEIGHT_M, "sand_min": SAND_MIN, "litter_max": LITTER_MAX,
                   "bush_clear_m": BUSH_CLEAR_M, "trunk_clear_m": TRUNK_CLEAR_M, "village_margin_m": VILLAGE_MARGIN_M,
                   "motutapu_clear_m": MOTUTAPU_CLEAR_M, "spacing_m": args.spacing, "seed": args.seed},
        "stats": stats,
        "spots": spots}, indent=0), encoding="utf-8")

    # control map: land in grey, spots in red
    scale = 2048 / max(ex, ey)
    img = Image.new("RGB", (int(ex * scale), int(ey * scale)), (20, 40, 70))
    small = np.asarray(Image.fromarray(masks[..., 1]).resize(img.size)) > SAND_MIN
    land = np.asarray(Image.fromarray((heights > (0.5 - hmin) / (hmax - hmin) * 65535).astype(np.uint8) * 255).resize(img.size)) > 0
    arr = np.asarray(img).copy()
    arr[land] = (110, 120, 90)
    arr[land & small] = (220, 205, 150)
    img = Image.fromarray(arr)
    draw = ImageDraw.Draw(img)
    for x, y, _ in spots:
        px, py = (x / 100.0 + ex / 2) * scale, (y / 100.0 + ey / 2) * scale
        draw.ellipse([px - 2, py - 2, px + 2, py + 2], fill=(220, 30, 30))
    img.save(out / "preview_totems.png")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
