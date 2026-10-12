"""Sites of the French forts on the atoll (owner 2026-10-08: "les forts français sur la map de Raraka Tuamotu et pas sur
une île personnalisée, générés procéduralement"; AFortSpawner, DA_FortSites).

Usage:
  python scripts/map/place_forts.py raraka [--per-fragment 3] [--seed 1767]

A site is the centre of the land fort (LI_FortFrench_Land: ramparts 66 x 58 m, moat bank 81 x 73 m): the ground stands
min_height_m above the mean sea level, within flat_m from its lowest to its highest point over a disc of flat_radius_m,
dry (humidity channel of the masks), inside the land of a fragment, away from its arrival, its villages and hamlets,
Motutapu and the Marae des Perles. The fort's gate (local +Y) faces the nearest shore. Thinned SPACING_M apart, the
flattest PER_FRAGMENT kept per fragment (LEVELS: strict, then relaxed for a fragment left without one);
UFortSiteSubsystem draws one when the fragment is played.
Output: docs/reference/map/<ile>/collectibles/fort_sites.json (UE cm, Z = ground of the heightmap, yaw in degrees).
"""
import argparse
import json
import math

import numpy as np
from PIL import Image

from place_camps import point_in
from place_totems import ROOT, poisson_thin

GRID_M = 8.0
WET_MAX = 60
SPACING_M = 160.0
# strict first; a fragment left without a site takes the relaxed level (smaller esplanade, more relief, nearer things)
LEVELS = [
    {"name": "strict", "min_height_m": 0.6, "flat_radius_m": 45.0, "flat_m": 1.8, "village_margin_cm": 6000.0,
     "fragment_village_clear_cm": 15000.0, "arrival_clear_cm": 15000.0},
    {"name": "relaxed", "min_height_m": 0.4, "flat_radius_m": 38.0, "flat_m": 3.0, "village_margin_cm": 3000.0,
     "fragment_village_clear_cm": 9000.0, "arrival_clear_cm": 9000.0},
]
MOTUTAPU_CLEAR_M = 350.0
MARAE = (-6677.86, -10132.64)        # m, UE frame (scripts/props/marae_layout.py)
MARAE_CLEAR_M = 140.0
SHORE_SEARCH_M = 600.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("ile")
    ap.add_argument("--per-fragment", type=int, default=3)
    ap.add_argument("--seed", type=int, default=1767)
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

    def height_at(x, y):
        u = np.clip((np.asarray(x) + ex / 2) / ex, 0, 0.999999)
        v = np.clip((np.asarray(y) + ey / 2) / ey, 0, 0.999999)
        return hmin + heights[(v * H).astype(int), (u * W).astype(int)] / 65535.0 * (hmax - hmin)

    def wet_at(x, y):
        u = np.clip((np.asarray(x) + ex / 2) / ex, 0, 0.999999)
        v = np.clip((np.asarray(y) + ey / 2) / ey, 0, 0.999999)
        return masks[(v * MH).astype(int), (u * MW).astype(int), 3]

    xs = np.arange(-ex / 2 + GRID_M / 2, ex / 2, GRID_M)
    ys = np.arange(-ey / 2 + GRID_M / 2, ey / 2, GRID_M)
    grid_x, grid_y = np.meshgrid(xs, ys)
    grid_x, grid_y = grid_x.ravel(), grid_y.ravel()
    villages = json.loads((base / "villages/villages.json").read_text(encoding="utf-8"))
    index_path = base / "villages" / "fragments" / "index.json"
    index = json.loads(index_path.read_text(encoding="utf-8")).get("fragments", []) if index_path.exists() else []
    mot = georef["points_ue"]["village_motutapu"]
    stats = {}

    def candidates(level):
        """Land points flat, dry and away from the villages for this level: x, y (m), ground (m), relief (m)"""
        h0 = height_at(grid_x, grid_y)
        ok = (h0 >= level["min_height_m"]) & (wet_at(grid_x, grid_y) <= WET_MAX)
        gx, gy, h0 = grid_x[ok], grid_y[ok], h0[ok]
        lo, hi, dry = h0.copy(), h0.copy(), np.ones(len(gx), dtype=bool)
        radius = level["flat_radius_m"]
        for ang in np.linspace(0, 2 * np.pi, 16, endpoint=False):
            for r in (radius / 3, 2 * radius / 3, radius):
                px, py = gx + np.cos(ang) * r, gy + np.sin(ang) * r
                h = height_at(px, py)
                lo, hi = np.minimum(lo, h), np.maximum(hi, h)
                dry &= wet_at(px, py) <= WET_MAX
        keep = (hi - lo <= level["flat_m"]) & (lo >= level["min_height_m"]) & dry
        flat = (hi - lo)[keep]
        gx, gy, h0 = gx[keep], gy[keep], h0[keep]
        keep = np.ones(len(gx), dtype=bool)
        for village in villages.get("villages", []):
            c = np.array(village.get("center_cm", [0, 0])[:2], float) / 100.0
            r = (village.get("zone", {}).get("radius_cm", 9000.0) + level["village_margin_cm"]) / 100.0
            keep &= np.hypot(gx - c[0], gy - c[1]) > r
        for frag in index:
            for v in frag.get("villages", []):
                c = np.array(v["center_cm"][:2], float) / 100.0
                r = (v.get("radius_cm", 9000.0) + level["village_margin_cm"]) / 100.0
                keep &= np.hypot(gx - c[0], gy - c[1]) > r
        keep &= np.hypot(gx - mot["x_cm"] / 100.0, gy - mot["y_cm"] / 100.0) > MOTUTAPU_CLEAR_M
        keep &= np.hypot(gx - MARAE[0], gy - MARAE[1]) > MARAE_CLEAR_M
        return gx[keep], gy[keep], h0[keep], flat[keep]

    def gate_yaw(x, y, start_m):
        """Yaw (deg) turning the fort's gate (local +Y) toward the nearest shore, and that shore's distance (m)"""
        best = None
        for k in range(36):
            a = 2 * math.pi * k / 36
            for d in np.arange(start_m, SHORE_SEARCH_M, 8.0):
                px, py = x + math.cos(a) * d, y + math.sin(a) * d
                if float(height_at(px, py)) < 0.2:
                    if best is None or d < best[0]:
                        best = (d, a)
                    break
        if best is None:
            return 0.0, None
        dx, dy = math.cos(best[1]), math.sin(best[1])
        return math.degrees(math.atan2(-dx, dy)), round(best[0])

    by_fragment = {}
    frag_stats = {}
    fragments = json.loads((base / "fragments" / "fragments.json").read_text(encoding="utf-8")).get("fragments", [])
    for level in LEVELS:
        gx, gy, h0, flat = candidates(level)
        stats[level["name"] + "_candidates"] = len(gx)
        for frag in fragments:
            if by_fragment.get(frag["id"]):
                continue
            polys = [p["points"] for p in frag.get("land_polygons_cm", []) if not p.get("hole")]
            sites_v = [v["center_cm"] for v in frag.get("sites", {}).get("villages", [])]
            arr = (frag.get("arrival") or {}).get("location_cm")
            idx = []
            for i in range(len(gx)):
                p = (gx[i] * 100.0, gy[i] * 100.0)
                if not any(point_in(p, poly) for poly in polys):
                    continue
                if any(math.hypot(p[0] - v[0], p[1] - v[1]) <= level["fragment_village_clear_cm"] for v in sites_v):
                    continue
                if arr and math.hypot(p[0] - arr[0], p[1] - arr[1]) <= level["arrival_clear_cm"]:
                    continue
                idx.append(i)
            if not idx:
                by_fragment[frag["id"]] = []
                frag_stats[frag["id"]] = [0, None]
                continue
            cand = np.stack([gx[idx], gy[idx]], axis=1)
            kept = [idx[k] for k in poisson_thin(cand, SPACING_M, rng)]
            kept.sort(key=lambda i: flat[i])
            sites = []
            for i in kept[: args.per_fragment]:
                yaw, shore = gate_yaw(float(gx[i]), float(gy[i]), level["flat_radius_m"])
                sites.append([round(float(gx[i] * 100.0)), round(float(gy[i] * 100.0)), round(float(h0[i] * 100.0)), round(yaw, 1),
                              shore, round(float(flat[i]), 2), level["name"]])
            by_fragment[frag["id"]] = sites
            frag_stats[frag["id"]] = [len(sites), level["name"]]
    stats["fragments"] = frag_stats
    out = base / "collectibles"
    out.mkdir(exist_ok=True)
    (out / "fort_sites.json").write_text(json.dumps({
        "island": args.ile, "frame": "UE cm: X east, Y south, Z = ground of the heightmap; yaw deg (gate = local +Y toward the shore)",
        "columns": ["x_cm", "y_cm", "ground_z_cm", "yaw_deg", "shore_m", "relief_m", "level"],
        "params": {"grid_m": GRID_M, "wet_max": WET_MAX, "spacing_m": SPACING_M, "levels": LEVELS,
                   "per_fragment": args.per_fragment, "seed": args.seed},
        "stats": stats, "by_fragment": by_fragment}, indent=1), encoding="utf-8")
    print(json.dumps(stats))


if __name__ == "__main__":
    main()
