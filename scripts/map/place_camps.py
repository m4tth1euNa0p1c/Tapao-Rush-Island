"""Candidate spots for the fishermen camps (T-082, ACampSpawner): flat dry land with room for a camp of about 9 m by 6 m.

Usage:
  python scripts/map/place_camps.py raraka [--spacing 120] [--seed 21]

A spot is kept when the ground stands MIN_HEIGHT_M above the mean sea level and stays within FLAT_M over a disc of
FLAT_RADIUS_M, the land is dry (humidity channel of the masks low) over that disc, and it lies away from the
villages (their zone plus VILLAGE_MARGIN_M), the real village of Motutapu and the Marae des Perles. Vegetation is
allowed: the camp clears its zone in game. Thinned to SPACING_M apart (Poisson disk); the spawner draws a few each
game near the PlayerStarts.
Output: docs/reference/map/<ile>/collectibles/camp_spots.json (UE cm, Z = ground of the heightmap).
"""
import argparse
import json

import numpy as np
from PIL import Image

from place_totems import ROOT, poisson_thin

GRID_M = 6.0
MIN_HEIGHT_M = 1.0
FLAT_RADIUS_M = 9.0
FLAT_M = 0.6
WET_MAX = 40
VILLAGE_MARGIN_M = 60.0
MOTUTAPU_CLEAR_M = 300.0
MARAE = (-6677.86, -10132.64)   # m, UE frame (scripts/props/marae_layout.py)
MARAE_CLEAR_M = 80.0
SPACING_M = 120.0
FRAGMENT_VILLAGE_CLEAR_CM = 12000.0   # no camp on the village sites of a fragment (cm)
VILLAGE_INDEX_MARGIN_CM = 4000.0      # beyond the zone of each generated village or hamlet (villages/fragments/index.json)


def point_in(pt, poly):
    """Even-odd test of a point in a polygon (UE cm)"""
    x, y = pt
    inside = False
    for i in range(len(poly)):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % len(poly)]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1) + x1:
            inside = not inside
    return inside


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ile")
    ap.add_argument("--spacing", type=float, default=SPACING_M)
    ap.add_argument("--seed", type=int, default=21)
    args = ap.parse_args()
    rng = np.random.default_rng(args.seed)

    base = ROOT / "docs/reference/map" / args.ile
    georef = json.loads((base / "terrain/georef.json").read_text(encoding="utf-8"))
    masks = np.asarray(Image.open(base / "terrain/T_Raraka_Masks.png").convert("RGBA"))
    heights = np.asarray(Image.open(base / "terrain/heightmap_16.png"), dtype=np.float64)
    villages = json.loads((base / "villages/villages.json").read_text(encoding="utf-8"))
    ex, ey = georef["extent_m"]
    hmin, hmax = georef["heightmap_16"]["min_m"], georef["heightmap_16"]["max_m"]
    H, W = heights.shape
    MH, MW = masks.shape[:2]

    def height_at(x, y):
        u = np.clip((x + ex / 2) / ex, 0, 0.999999)
        v = np.clip((y + ey / 2) / ey, 0, 0.999999)
        return hmin + heights[(v * H).astype(int), (u * W).astype(int)] / 65535.0 * (hmax - hmin)

    def wet_at(x, y):
        u = np.clip((x + ex / 2) / ex, 0, 0.999999)
        v = np.clip((y + ey / 2) / ey, 0, 0.999999)
        return masks[(v * MH).astype(int), (u * MW).astype(int), 3]

    xs = np.arange(-ex / 2 + GRID_M / 2, ex / 2, GRID_M)
    ys = np.arange(-ey / 2 + GRID_M / 2, ey / 2, GRID_M)
    gx, gy = np.meshgrid(xs, ys)
    gx, gy = gx.ravel(), gy.ravel()
    h0 = height_at(gx, gy)
    ok = (h0 >= MIN_HEIGHT_M) & (wet_at(gx, gy) <= WET_MAX)
    gx, gy, h0 = gx[ok], gy[ok], h0[ok]
    stats = {"land_points": int(ok.sum())}

    # flat and dry over the whole disc of the camp
    lo, hi, dry = h0.copy(), h0.copy(), np.ones(len(gx), dtype=bool)
    for ang in np.linspace(0, 2 * np.pi, 12, endpoint=False):
        for r in (FLAT_RADIUS_M * 0.5, FLAT_RADIUS_M):
            px, py = gx + np.cos(ang) * r, gy + np.sin(ang) * r
            h = height_at(px, py)
            lo, hi = np.minimum(lo, h), np.maximum(hi, h)
            dry &= wet_at(px, py) <= WET_MAX
    keep = (hi - lo <= FLAT_M) & (lo >= MIN_HEIGHT_M - 0.3) & dry
    gx, gy, h0 = gx[keep], gy[keep], h0[keep]
    stats["flat_dry"] = len(gx)

    keep = np.ones(len(gx), dtype=bool)
    for village in villages.get("villages", []):
        c = np.array(village.get("center_cm", [0, 0])[:2], float) / 100.0
        radius = village.get("zone", {}).get("radius_cm", 9000.0) / 100.0 + VILLAGE_MARGIN_M
        keep &= np.hypot(gx - c[0], gy - c[1]) > radius
    mot = georef["points_ue"]["village_motutapu"]
    keep &= np.hypot(gx - mot["x_cm"] / 100.0, gy - mot["y_cm"] / 100.0) > MOTUTAPU_CLEAR_M
    keep &= np.hypot(gx - MARAE[0], gy - MARAE[1]) > MARAE_CLEAR_M
    gx, gy, h0 = gx[keep], gy[keep], h0[keep]
    stats["after_exclusions"] = len(gx)

    cand = np.stack([gx, gy], axis=1)
    kept = poisson_thin(cand, args.spacing, rng)
    spots = [[round(float(gx[i] * 100.0)), round(float(gy[i] * 100.0)), round(float(h0[i] * 100.0))] for i in kept]
    stats["spots"] = len(spots)

    # spots of each fragment of the expedition (T-094: one fragment is played per game): inside its land, away
    # from its village sites (fragments.json sites.villages, one pacifiable zone each)
    # generated villages and hamlets of the fragments (monk-island-78, AVillageSpawner): their zone plus a margin;
    # the marae built at runtime take their spots among these too, so they keep off the villages as well
    index_path = base / "villages" / "fragments" / "index.json"
    built = []
    if index_path.exists():
        for frag in json.loads(index_path.read_text(encoding="utf-8")).get("fragments", []):
            for v in frag.get("villages", []):
                built.append((v["center_cm"][0], v["center_cm"][1], v.get("radius_cm", 9000.0) + VILLAGE_INDEX_MARGIN_CM))
        before = len(spots)
        spots = [s for s in spots if all(np.hypot(s[0] - x, s[1] - y) > r for x, y, r in built)]
        stats["villages_index"] = {"villages": len(built), "spots_removed": before - len(spots)}
        stats["spots"] = len(spots)

    by_fragment = {}
    frag_path = base / "fragments" / "fragments.json"
    if frag_path.exists():
        fragments = json.loads(frag_path.read_text(encoding="utf-8")).get("fragments", [])
        for frag in fragments:
            polys = [p["points"] for p in frag.get("land_polygons_cm", []) if not p.get("hole")]
            villages = [v["center_cm"] for v in frag.get("sites", {}).get("villages", [])]
            keep_idx = [i for i, s in enumerate(spots)
                        if any(point_in(s[:2], poly) for poly in polys)
                        and all(np.hypot(s[0] - v[0], s[1] - v[1]) > FRAGMENT_VILLAGE_CLEAR_CM for v in villages)]
            by_fragment[frag["id"]] = keep_idx
        stats["fragments"] = {k: len(v) for k, v in by_fragment.items()}
    out = base / "collectibles"
    out.mkdir(exist_ok=True)
    (out / "camp_spots.json").write_text(json.dumps({
        "island": args.ile, "frame": "UE cm: X east, Y south, Z = ground of the heightmap",
        "params": {"grid_m": GRID_M, "min_height_m": MIN_HEIGHT_M, "flat_radius_m": FLAT_RADIUS_M, "flat_m": FLAT_M,
                   "wet_max": WET_MAX, "village_margin_m": VILLAGE_MARGIN_M, "motutapu_clear_m": MOTUTAPU_CLEAR_M,
                   "marae_clear_m": MARAE_CLEAR_M, "spacing_m": args.spacing, "seed": args.seed},
        "stats": stats, "spots": spots, "by_fragment": by_fragment}, separators=(",", ":")), encoding="utf-8")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
