"""Candidate spots for the common loot scattered on the ground of the island (T-076, ALootScatterSpawner).

Usage:
  python scripts/map/place_loot.py raraka [--spacing 9] [--seed 13]

A spot is a point of land: the ground stands at least MIN_HEIGHT_M above the mean sea level and is dry (humidity
channel of the masks low), on sand or under the vegetation (loot lies everywhere people go: beaches, palm groves,
under the bushes, around the villages but not inside their buildings, which the level trace rejects). Out of the
real village of Motutapu. Thinned to SPACING_M apart (Poisson disk). In game the spawner keeps the spots near a
PlayerStart (DA_LootRules MaxDistanceFromStart), apart (MinSpacing), out of the quiet zones (the marae) and checks
the ground on the level.

Inputs (docs/reference/map/<ile>/): terrain/georef.json, terrain/T_Raraka_Masks.png, terrain/heightmap_16.png.
Output: collectibles/loot_spots.json (UE cm, Z = ground of the heightmap) and collectibles/preview_loot.png.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

from place_totems import ROOT, poisson_thin

GRID_M = 4.0              # sampling step of the candidates (m)
MIN_HEIGHT_M = 0.7        # above the wet beach (m above mean sea level)
WET_MAX = 60              # humidity channel of the masks (0-255): dry land only
MOTUTAPU_CLEAR_M = 120.0  # the core of the real village stays untouched (m around its centre); the PlayerStart beach, 119 m away, keeps its loot
SPACING_M = 9.0           # spots at least this far apart (m)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ile")
    ap.add_argument("--spacing", type=float, default=SPACING_M)
    ap.add_argument("--seed", type=int, default=13)
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)

    base = ROOT / "docs/reference/map" / args.ile
    georef = json.loads((base / "terrain/georef.json").read_text(encoding="utf-8"))
    masks = np.asarray(Image.open(base / "terrain/T_Raraka_Masks.png").convert("RGBA"))
    heights = np.asarray(Image.open(base / "terrain/heightmap_16.png"), dtype=np.float64)
    ex, ey = georef["extent_m"]
    hmin, hmax = georef["heightmap_16"]["min_m"], georef["heightmap_16"]["max_m"]
    H, W = heights.shape
    MH, MW = masks.shape[:2]

    xs = np.arange(-ex / 2 + GRID_M / 2, ex / 2, GRID_M)
    ys = np.arange(-ey / 2 + GRID_M / 2, ey / 2, GRID_M)
    gx, gy = np.meshgrid(xs, ys)
    gx = gx + rng.uniform(-GRID_M / 2, GRID_M / 2, gx.shape)
    gy = gy + rng.uniform(-GRID_M / 2, GRID_M / 2, gy.shape)
    u = np.clip((gx + ex / 2) / ex, 0, 0.999999)
    v = np.clip((gy + ey / 2) / ey, 0, 0.999999)
    height = hmin + heights[(v * H).astype(int), (u * W).astype(int)] / 65535.0 * (hmax - hmin)
    wet = masks[(v * MH).astype(int), (u * MW).astype(int), 3]
    ok = (height >= MIN_HEIGHT_M) & (wet <= WET_MAX)
    cand = np.stack([gx[ok], gy[ok]], axis=1)
    cand_h = height[ok]
    stats = {"land_points": int(ok.sum())}

    mot = georef["points_ue"]["village_motutapu"]
    keep = np.hypot(cand[:, 0] - mot["x_cm"] / 100.0, cand[:, 1] - mot["y_cm"] / 100.0) > MOTUTAPU_CLEAR_M
    cand, cand_h = cand[keep], cand_h[keep]
    stats["after_motutapu"] = len(cand)

    kept = poisson_thin(cand, args.spacing, rng)
    spots = [[round(float(cand[i, 0] * 100.0)), round(float(cand[i, 1] * 100.0)), round(float(cand_h[i] * 100.0))] for i in kept]
    stats["spots"] = len(spots)

    out = base / "collectibles"
    out.mkdir(exist_ok=True)
    (out / "loot_spots.json").write_text(json.dumps({
        "island": args.ile,
        "frame": "UE cm: X east, Y south, Z = ground of the heightmap (mean sea level 0)",
        "params": {"grid_m": GRID_M, "min_height_m": MIN_HEIGHT_M, "wet_max": WET_MAX,
                   "motutapu_clear_m": MOTUTAPU_CLEAR_M, "spacing_m": args.spacing, "seed": args.seed},
        "stats": stats,
        "spots": spots}, separators=(",", ":")), encoding="utf-8")

    scale = 2048 / max(ex, ey)
    img = Image.new("RGB", (int(ex * scale), int(ey * scale)), (20, 40, 70))
    land = np.asarray(Image.fromarray((heights > (0.5 - hmin) / (hmax - hmin) * 65535).astype(np.uint8) * 255).resize(img.size)) > 0
    arr = np.asarray(img).copy()
    arr[land] = (110, 120, 90)
    img = Image.fromarray(arr)
    draw = ImageDraw.Draw(img)
    for x, y, _ in spots[::4]:
        px, py = (x / 100.0 + ex / 2) * scale, (y / 100.0 + ey / 2) * scale
        draw.point((px, py), fill=(240, 200, 60))
    img.save(out / "preview_loot.png")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
