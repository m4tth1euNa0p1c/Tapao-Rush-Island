"""Build the terrain of an expedition fragment: a small map cut by the sea from the real atoll (ADR-004, Pipeline 2).

Usage (from the repository root):
  python scripts/map/fragments/build_fragment.py raraka FRG_Raraka_01 [FRG_Raraka_02 ...] [--all]
         [--check] [--anchors] [--force-anchors] [--no-mesh] [--data-root <checkout with the LFS data>] [--rules ...]

  --check          only recompute the terrain in memory and run the terrain checks (exit code 1 on failure)
  --anchors        write the galleon mooring (ADR-005) of the fragments that have none into fragments.json
  --force-anchors  recompute every galleon mooring of the selected fragments (identifiers are never touched)
  --no-mesh        previews, georef and checks only (no OBJ, no mask texture)

Inputs: fragments.json (find_fragments.py, T-082) and the terrain of build_terrain.py (heightmap_16.png + georef.json,
reef atlas classes, coastline), read through fraglib (LFS-aware data roots). Parameters: rules.json, sections
"terrain" and "galleon" (no value in the code).

Method, inside the box of the fragment (kept land + box_margin_m = 1.5 km):
  1. Heights = build_terrain.py heights (estimated bathymetry), classes = reef atlas of build_terrain.py.
  2. Excluded land (every motu outside the fragment) becomes reef flat (excluded_land_floor_m).
  3. Beyond rim_keep_m from the kept land, the floor never rises above a ramp that starts at the reef flat depth and
     goes down at rim_fall_deg to far_floor_m: the reef flat that ran on toward the excluded motu stops with a gentle
     slope, and the edge of the box is deep (lagoon -30 m, the ocean keeps its own deeper floor). Lagoon pinnacles
     are kept, except near the edge of the box.
  4. Synthetic hoa (cuts of type "synthetique"): a channel along the cut line, hoa_center_depth_m deep in the
     middle, rising at shore_slope_deg to the waterline at hoa_width_m / 2 from the line (width 40-80 m). On both
     sides of the new waterline the ground follows shore_slope_deg (the kept land gets a beach); the far side of the
     hoa is excluded land, hence reef flat.
  5. Kept land: exactly the polygon of fragments.json (height > 0 there and only there).
  6. Masks (same RGBA layout as T_<Ile>_Masks: R vegetation, G sand, B reef, A wetness) resampled from the atoll
     masks, then sand + wet sand on the new shores (beaches of the hoa), reef / sand on the former land.
  7. Galleon mooring (ADR-005): from the boat arrival, back toward the open sea along the bow axis.
Frame: the fragment keeps the world frame of Lvl_Raraka (terrain/georef.json: origin = centre of the extent of the
atoll, X = east, Y = south, Z = up, cm). The offset of a fragment is zero, so the villages, camps, arrivals and the
flood grid placed in Lvl_Raraka are reused as they are in L_Frag_*.
Outputs:
  docs/reference/map/raw/<ile>_fragments/<FRG>/      OBJ tiles (Nanite), water plane, mask texture, heightmap 16 bits
  docs/reference/map/<ile>/fragments/<FRG>/          georef.json, previews (relief, classes, masks, shading)
"""
import argparse
import json
import math
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fraglib as fl  # noqa: E402
from fraglib import bt  # noqa: E402

T0 = time.time()


def log(msg):
    print(f"[build_fragment {time.time() - T0:6.1f}s] {msg}", flush=True)


def edt(mask, m):
    return (ndi.distance_transform_edt(mask) * m).astype(np.float32)


def cm(p):
    return [round(float(p[0]) * 100, 1), round(float(p[1]) * 100, 1)]


# ---------------------------------------------------------------------------------------------- terrain

def fragment_window(A, fr):
    """Pixel window (r0, r1, c0, c1) of the box of the fragment, clipped to the atoll grid."""
    x0, y0, x1, y1 = (v / 100.0 for v in fr["box_cm"])
    ca, ra = A.m_to_px(x0, y0)
    cb, rb = A.m_to_px(x1, y1)
    want = (int(math.floor(ra)), int(math.ceil(rb)), int(math.floor(ca)), int(math.ceil(cb)))
    win = (max(want[0], 0), min(want[1], A.H), max(want[2], 0), min(want[3], A.W))
    return win, win != want


class FragmentTerrain:
    """Heights and masks of one fragment on the window of its box (source grid, A.m metres per pixel)."""

    def __init__(self, A, fr, R):
        Tr = R["terrain"]
        m = A.m
        self.A, self.fr, self.R, self.m = A, fr, R, m
        (r0, r1, c0, c1), self.clipped = fragment_window(A, fr)
        self.win = (r0, r1, c0, c1)
        win = self.win
        self.K = fl.polygons_to_mask(A, fr["land_polygons_cm"], win)
        self.land0 = A.land[r0:r1, c0:c1]
        self.E = self.land0 & ~self.K
        self.h0 = A.h[r0:r1, c0:c1].astype(np.float32)
        self.cls = A.cls[r0:r1, c0:c1]
        H, W = self.h0.shape
        self.shape = (H, W)
        xs, ys = A.px_to_m(np.arange(c0, c1), np.arange(r0, r1))
        self.xs, self.ys = xs, ys
        K, E = self.K, self.E
        water = ~K
        s = math.tan(math.radians(Tr["shore_slope_deg"]))
        floor_rf = Tr["excluded_land_floor_m"]

        # distances between pixel centres; the pixels on each side of the waterline get +/- shore_edge_m, then the
        # ground follows the slope s (an offset of half a pixel would double the step across a diagonal staircase)
        self.D_K = edt(~K, m)                                   # water: distance to the kept land (centres)
        psi = self.D_K - m                                     # water: 0 next to the kept shore
        phi = edt(K, m) - m                                    # kept land: 0 next to the water
        d_toE = edt(~E, m) - m                                 # kept land: 0 next to the excluded land
        e = Tr["shore_edge_m"]
        self.d_pin = edt(self.cls != bt.PINNACLE, m)
        rr = np.arange(H)[:, None]
        cc = np.arange(W)[None, :]
        self.d_edge = (np.minimum(np.minimum(rr, H - 1 - rr), np.minimum(cc, W - 1 - cc)) * m).astype(np.float32)

        h = self.h0.copy()
        # 2. excluded land -> reef flat
        h[E] = floor_rf
        # 3. the rim stops with a gentle slope beyond rim_keep_m of the kept land
        tf = math.tan(math.radians(Tr["rim_fall_deg"]))
        g = np.where(self.D_K > Tr["rim_keep_m"],
                     np.maximum(floor_rf - tf * (self.D_K - Tr["rim_keep_m"]), Tr["far_floor_m"]), np.inf)
        # lagoon pinnacles are kept; the specks of the atlas classified as pinnacles along the rim (anti-aliasing
        # around excluded motu and reef flat) are not
        d_rim0 = edt(~(self.land0 | (self.cls == bt.REEF_FLAT)), m)
        protect = (1.0 - bt.smoothstep(0.0, Tr["pinnacle_keep_m"], self.d_pin)) * \
            bt.smoothstep(0.0, Tr["edge_fade_m"], self.d_edge) * \
            bt.smoothstep(0.5 * Tr["pinnacle_min_rim_m"], Tr["pinnacle_min_rim_m"], d_rim0)
        hw = np.minimum(h, g)
        h = np.where(water, hw + protect * (h - hw), h).astype(np.float32)
        # 4. synthetic hoa
        X = np.broadcast_to(xs[None, :], (H, W)).ravel()
        Y = np.broadcast_to(ys[:, None], (H, W)).ravel()
        P = np.c_[X, Y]
        self.Z = np.zeros((H, W), bool)
        self.hoa_core = np.zeros((H, W), bool)
        self.hoas = []
        depth = Tr["hoa_center_depth_m"]
        for c in fr["cuts"]:
            if c["type"] != "synthetique":
                continue
            a, b = (np.array(q, float) / 100.0 for q in c["line_cm"])
            Wh = float(c["hoa_width_m"])
            dseg = fl.seg_points_dist(P, a, b).reshape(H, W).astype(np.float32)
            T = np.maximum(-depth, -e + s * (dseg - Wh / 2.0))
            h = np.where(water, np.minimum(h, T), h)
            self.Z |= dseg <= Wh / 2.0 + Tr["hoa_zone_extra_m"]
            self.hoa_core |= (dseg <= Wh / 2.0) & water
            self.hoas.append({"cut": c, "a": a, "b": b, "width_m": Wh, "dseg": dseg})
        # new shores: gentle on both sides of the waterline
        h = np.where(water & self.Z, np.maximum(h, -e - s * psi), h)
        h = np.where(K, np.minimum(h, e + s * d_toE), h)
        h = np.where(K & self.Z, np.minimum(h, e + s * phi), h)
        # 5. kept land = exactly the polygon
        h = np.where(K, np.maximum(h, Tr["land_min_m"]), np.minimum(h, Tr["water_max_m"])).astype(np.float32)
        self.h = h
        self.d_toE = d_toE
        self.modified = np.abs(h - self.h0) > 0.01
        self._masks = None

    # ---------------------------------------------------------------- masks
    def masks(self):
        """(H, W, 4) float masks R vegetation, G sand, B reef, A wetness, on the window grid."""
        if self._masks is not None:
            return self._masks
        A, Tr = self.A, self.R["terrain"]
        src = load_masks(A)
        r0, r1, c0, c1 = self.win
        S = src.shape[0]
        rows = (np.arange(r0, r1) + 0.5) / A.H * S - 0.5
        cols = (np.arange(c0, c1) + 0.5) / A.W * src.shape[1] - 0.5
        RR, CC = np.meshgrid(rows, cols, indexing="ij")
        M = np.stack([ndi.map_coordinates(src[..., k], [RR, CC], order=1, mode="nearest") for k in range(4)], -1)
        M = np.clip(M / 255.0, 0, 1).astype(np.float32)
        K, h = self.K, self.h
        water = ~K
        # former land and lowered floor: reef on the shallow reef flat, sand deeper; hoa: sandy bottom
        changed = water & (self.modified | self.E)
        reef = (bt.smoothstep(-2.5, -0.3, h) * 0.8).astype(np.float32)
        reef = np.where(self.hoa_core, 0.15, reef)
        M[..., 0] = np.where(water, 0.0, M[..., 0])
        M[..., 2] = np.where(changed, reef, M[..., 2])
        M[..., 1] = np.where(changed, 1.0 - reef, M[..., 1])
        M[..., 3] = np.where(water, 1.0, M[..., 3])
        # beaches of the new shores (kept land next to the hoa): sand, and wet sand at the waterline
        bw = (1.0 - bt.smoothstep(Tr["beach_m"] * 0.5, Tr["beach_m"], self.d_toE)) * K
        M[..., 1] = np.maximum(M[..., 1], bw)
        M[..., 0] *= 1.0 - bw
        M[..., 2] *= 1.0 - bw
        wet = (1.0 - bt.smoothstep(0.0, Tr["wet_m"], self.d_toE)) * K
        M[..., 3] = np.maximum(M[..., 3], wet)
        tot = M[..., :3].sum(-1, keepdims=True) + 1e-6
        M[..., :3] /= tot
        self._masks = M
        return M

    # ---------------------------------------------------------------- frame helpers
    def to_px(self, x, y):
        """Continuous window pixel coordinates (col, row) of UE metres."""
        c, r = self.A.m_to_px(x, y)
        return c - self.win[2], r - self.win[0]

    def sample_disk(self, img, x, y, radius):
        c, r = self.to_px(x, y)
        H, W = self.shape
        rad = radius / self.m
        r_lo, r_hi = int(math.floor(r - rad)), int(math.ceil(r + rad))
        c_lo, c_hi = int(math.floor(c - rad)), int(math.ceil(c + rad))
        if r_lo < 0 or c_lo < 0 or r_hi >= H or c_hi >= W:
            return None
        RR, CC = np.mgrid[r_lo:r_hi + 1, c_lo:c_hi + 1]
        inside = np.hypot(RR + 0.5 - r, CC + 0.5 - c) <= rad
        return img[RR[inside], CC[inside]]


_MASKS = {}


def load_masks(A):
    key = id(A)
    if key not in _MASKS:
        p = A.D.find(A.rel / "terrain" / f"T_{A.island.capitalize()}_Masks.png")
        _MASKS[key] = np.asarray(Image.open(p).convert("RGBA")).astype(np.float32)
    return _MASKS[key]


# ---------------------------------------------------------------------------------------------- galleon (ADR-005)

def galleon_anchor(A, fr, R, T):
    """Mooring of the galleon: first point behind the arrival (toward the open sea, along the bow axis) with at
    least min_depth_m of water on the whole disk of radius_m, no pinnacle nearby, inside the box."""
    G = R["galleon"]
    arr = fr.get("arrival")
    if not arr:
        return None
    p = np.array(arr["location_cm"][:2], float) / 100.0
    yaw = math.radians(arr["yaw_deg"])
    fwd = np.array([math.cos(yaw), math.sin(yaw)])
    target = np.array(arr["target_cm"], float) / 100.0
    rad = G["radius_m"]
    t = G["start_offset_m"]
    while True:
        q = p - fwd * t
        c, r = T.to_px(q[0], q[1])
        H, W = T.shape
        d_edge = min(c, r, W - c, H - r) * T.m
        if d_edge < rad + G["edge_clearance_m"]:
            return None
        hs = T.sample_disk(T.h, q[0], q[1], rad)
        dp = T.sample_disk(T.d_pin, q[0], q[1], rad)
        if hs is not None and hs.max() <= -G["min_depth_m"] and dp.min() >= G["pinnacle_clearance_m"]:
            hc = float(T.h[int(r), int(c)])
            return {"location_cm": [round(float(q[0]) * 100, 1), round(float(q[1]) * 100, 1), 0.0],
                    "yaw_deg": arr["yaw_deg"], "depth_m": round(float(-hs.max()), 2),
                    "center_depth_m": round(-hc, 2), "radius_m": rad,
                    "behind_arrival_m": round(float(t), 1),
                    "beach_dist_m": round(float(np.linalg.norm(q - target)), 1),
                    "side": arr.get("side"),
                    "method": "rules.galleon : recul depuis l'arrivee le long de l'axe de la proue ; profondeur = "
                              "point le moins profond du disque (terrain du fragment, bathymetrie estimee)"}
        t += G["step_m"]


# ---------------------------------------------------------------------------------------------- checks

def triangle_slopes_deg(h, m):
    """Slopes (deg) of the two triangles of every grid cell, as meshed by build_terrain.grid_mesh:
    (A, C, B) and (B, C, D) with A = (i, j), B = (i, j+1), C = (i+1, j), D = (i+1, j+1)."""
    a, b, c, d = h[:-1, :-1], h[:-1, 1:], h[1:, :-1], h[1:, 1:]
    s1 = np.hypot(b - a, c - a) / m
    s2 = np.hypot(d - c, d - b) / m
    return np.degrees(np.arctan(s1)), np.degrees(np.arctan(s2))


def check_terrain(A, fr, R, T, anchor="memory"):
    """Terrain checks of a fragment. Returns (errors, metrics)."""
    Tr, G = R["terrain"], R["galleon"]
    m = T.m
    errors, met = [], {}
    h, K = T.h, T.K
    # 1. kept land intact, no excluded land left
    land_new = h > 0
    met["kept_land_px"] = int(K.sum())
    lost = int((K & ~land_new).sum())
    extra = int((land_new & ~K).sum())
    if lost:
        errors.append(f"terre gardee alteree : {lost} pixels du polygone sous l'eau")
    if extra:
        errors.append(f"{extra} pixels de terre hors du polygone garde (terre exclue restante)")
    full_K = fl.polygons_to_mask(A, fr["land_polygons_cm"])
    if int(full_K.sum()) != int(K.sum()):
        errors.append("la boite ne contient pas toute la terre gardee")
    met["excluded_land_removed_ha"] = round(float(T.E.sum()) * m * m / 1e4, 2)
    # 2. slopes on the hoa and the new shores
    t1, t2 = triangle_slopes_deg(h, m)
    region = (T.Z | (K & (T.d_toE <= Tr["beach_m"] * 2)) | ndi.binary_dilation(T.E, iterations=2))
    region &= T.d_pin > Tr["pinnacle_keep_m"] + 10
    reg = region & (T.modified | K)
    cell = reg[:-1, :-1] & reg[:-1, 1:] & reg[1:, :-1] & reg[1:, 1:]
    smax = float(max(t1[cell].max(initial=0.0), t2[cell].max(initial=0.0)))
    met["max_slope_new_shores_deg"] = round(smax, 2)
    if smax > Tr["max_slope_deg"] + 1e-3:
        errors.append(f"pente de {smax:.1f} deg sur un hoa ou une nouvelle rive (max {Tr['max_slope_deg']} deg)")
    ramp = (T.modified & ~K & ~region) & (T.d_pin > Tr["pinnacle_keep_m"] + 10)
    cell = ramp[:-1, :-1] & ramp[:-1, 1:] & ramp[1:, :-1] & ramp[1:, 1:]
    met["max_slope_lowered_floor_deg"] = round(float(max(t1[cell].max(initial=0.0), t2[cell].max(initial=0.0))), 2)
    # 3. edge of the box
    border = np.r_[h[0, :], h[-1, :], h[:, 0], h[:, -1]]
    met["edge_max_floor_m"] = round(float(border.max()), 2)
    if border.max() > Tr["edge_max_floor_m"]:
        errors.append(f"fond a {border.max():.1f} m au bord de la boite (attendu <= {Tr['edge_max_floor_m']} m)")
    if T.clipped:
        errors.append("la boite depasse la grille de l'atoll")
    # 4. hoa
    sand = T.masks()[..., 1]
    hoa_out = []
    lo, hi = Tr["hoa_depth_range_m"]
    wlo, whi = R["cuts"]["synthetic_hoa_range_m"]
    for ho in T.hoas:
        # centre of the hoa: the line, where the floor was shallower than the hoa (dug land or reef flat of a narrow
        # natural hoa that the cut widens); beyond, the line reaches the natural lagoon or ocean floor
        band = (ho["dseg"] <= 3.0) & ~K & (T.h0 >= -Tr["hoa_center_depth_m"])
        if not band.any():
            errors.append(f"hoa {ho['cut']['position_cm']} : aucun pixel creuse sur la ligne")
            continue
        dmin, dmax = float(-h[band].max()), float(-h[band].min())
        kd = ho["dseg"][K & (ho["dseg"] <= ho["width_m"])]
        width = 2.0 * (float(kd.min()) - m / 2.0) if len(kd) else float("nan")
        beach = K & (T.d_toE <= 10.0) & (ho["dseg"] <= ho["width_m"])
        sand_beach = float(sand[beach].mean()) if beach.any() else 0.0
        hoa_out.append({"position_cm": ho["cut"]["position_cm"], "center_depth_m": [round(dmin, 2), round(dmax, 2)],
                        "width_m": round(width, 1), "beach_sand": round(sand_beach, 2)})
        if dmin < lo - 1e-3 or dmax > hi + 1e-3:
            errors.append(f"hoa {ho['cut']['position_cm']} : fond de {dmin:.2f} a {dmax:.2f} m au centre "
                          f"(attendu {lo} a {hi} m)")
        if not (wlo <= width <= whi):
            errors.append(f"hoa {ho['cut']['position_cm']} : largeur {width:.0f} m (attendu {wlo:.0f}-{whi:.0f} m)")
        if sand_beach < 0.5:
            errors.append(f"hoa {ho['cut']['position_cm']} : pas de plage cote fragment (sable {sand_beach:.2f})")
    met["hoa"] = hoa_out
    # 5. navigable ring around the kept land
    nav = h <= -Tr["boat_min_depth_m"]
    lab, n = ndi.label(~nav)
    kl = np.unique(lab[K])
    kl = kl[kl > 0]
    edge_labels = np.unique(np.r_[lab[0, :], lab[-1, :], lab[:, 0], lab[:, -1]])
    touching = np.intersect1d(kl, edge_labels)
    if len(touching):
        errors.append("pas d'anneau navigable : les hauts-fonds de la terre gardee touchent le bord de la boite")
    nlab, _ = ndi.label(nav)
    sea = np.unique(np.r_[nlab[0, :], nlab[-1, :], nlab[:, 0], nlab[:, -1]])
    sea = sea[sea > 0]
    obstacle = np.isin(lab, kl)
    ring_w = edt(~obstacle, m)
    met["boat_ring"] = {"min_depth_m": Tr["boat_min_depth_m"],
                        "shallows_ha": round(float(obstacle.sum()) * m * m / 1e4, 1),
                        "ring_max_offshore_m": round(float(ring_w[nav & np.isin(nlab, sea) &
                                                                  (ndi.binary_dilation(obstacle))].max(initial=0.0)), 1)}
    arr = fr.get("arrival")
    if arr:
        c, r = T.to_px(arr["location_cm"][0] / 100.0, arr["location_cm"][1] / 100.0)
        if not (0 <= int(r) < T.shape[0] and 0 <= int(c) < T.shape[1]) or nlab[int(r), int(c)] not in sea:
            errors.append("l'arrivee de la barque n'est pas sur l'eau navigable reliee au large")
    # 6. galleon mooring
    ga = fr.get("galleon_anchor") if anchor == "memory" else anchor
    if not ga:
        errors.append("pas de mouillage du galion (galleon_anchor) : lancer build_fragment.py --anchors")
    else:
        q = np.array(ga["location_cm"][:2], float) / 100.0
        hs = T.sample_disk(T.h, q[0], q[1], G["radius_m"])
        dp = T.sample_disk(T.d_pin, q[0], q[1], G["radius_m"])
        if hs is None:
            errors.append("galion : disque hors de la boite")
        else:
            depth = float(-hs.max())
            met["galleon_depth_m"] = round(depth, 2)
            if depth < G["min_depth_m"] - 1e-3:
                errors.append(f"galion : {depth:.1f} m de fond sur le disque de {G['radius_m']:.0f} m "
                              f"(min {G['min_depth_m']} m)")
            if dp.min() < G["pinnacle_clearance_m"]:
                errors.append("galion : pinacle trop proche")
        c, r = T.to_px(q[0], q[1])
        d_edge = min(c, r, T.shape[1] - c, T.shape[0] - r) * m
        if d_edge < G["radius_m"] + G["edge_clearance_m"] - m:
            errors.append(f"galion a {d_edge:.0f} m du bord de la boite")
        if abs(ga["location_cm"][2]) > 1e-6:
            errors.append("galion : z different de 0")
        if arr:
            p = np.array(arr["location_cm"][:2], float) / 100.0
            yaw = math.radians(arr["yaw_deg"])
            fwd = np.array([math.cos(yaw), math.sin(yaw)])
            v = q - p
            if float(v @ fwd) > -G["start_offset_m"] + 1e-3 or abs(float(v[0] * fwd[1] - v[1] * fwd[0])) > 1.0:
                errors.append("galion : pas derriere l'arrivee sur l'axe de la proue")
            if nlab[int(r), int(c)] not in sea:
                errors.append("galion : pas sur l'eau navigable reliee au large")
    return errors, met


# ---------------------------------------------------------------------------------------------- outputs

def write_meshes(T, fid, mesh_dir, island_cap, R):
    Tr = R["terrain"]
    s = int(Tr["mesh_step_px"])
    H, W = T.shape
    H2, W2 = H // s, W // s
    hg = T.h[:H2 * s, :W2 * s].reshape(H2, s, W2, s).mean((1, 3))
    m = T.m
    spacing = s * m
    east0 = float(T.xs[0]) + (s - 1) / 2.0 * m          # centre of the first block (pixel centres)
    south0 = float(T.ys[0]) + (s - 1) / 2.0 * m
    for old in list(mesh_dir.glob("*.obj")):
        old.unlink()
    base = f"SM_Frag_{island_cap}_{fid.rsplit('_', 1)[1]}"
    tiles = []
    tp = max(2, int(Tr["tile_px"]) // s)
    nty, ntx = math.ceil((H2 - 1) / tp), math.ceil((W2 - 1) / tp)
    for ty in range(nty):
        for tx in range(ntx):
            r0, c0 = ty * tp, tx * tp
            r1, c1 = min(r0 + tp, H2 - 1), min(c0 + tp, W2 - 1)
            if r1 <= r0 or c1 <= c0:
                continue
            block = hg[r0:r1 + 1, c0:c1 + 1]
            if block.max() < Tr["skip_tile_below_m"]:
                continue
            pos, uv, idx = bt.grid_mesh(block, east0 + c0 * spacing, south0 + r0 * spacing, spacing,
                                        ((c0 * s + s / 2) / W, (r0 * s + s / 2) / H), (s / W, s / H))
            name = f"{base}_Terrain_{tx}_{ty}"
            bt.write_obj(mesh_dir / f"{name}.obj", pos, uv, idx, name, "M_Terrain")
            tiles.append({"name": name, "file": rel(mesh_dir / f"{name}.obj"), "vertices": int(len(pos)),
                          "triangles": int(len(idx) // 3), "max_height_m": round(float(block.max()), 2),
                          "min_height_m": round(float(block.min()), 2)})
    # water plane (Z = 0, the water level stays a global parameter: the actor is moved in UE)
    x0, y0, x1, y1 = (v / 100.0 for v in T.fr["box_cm"])
    side = max(x1 - x0, y1 - y0) + 2 * Tr["water_plane_margin_m"]
    n = int(Tr["water_plane_cells"])
    cxm, cym = (x0 + x1) / 2.0, (y0 + y1) / 2.0
    pos, uv, idx = bt.grid_mesh(np.zeros((n + 1, n + 1), np.float32), cxm - side / 2, cym - side / 2, side / n,
                                (0.0, 0.0), (1.0 / n, 1.0 / n))
    wname = f"{base}_Water"
    bt.write_obj(mesh_dir / f"{wname}.obj", pos, uv, idx, wname, "M_Water")
    water = {"name": wname, "file": rel(mesh_dir / f"{wname}.obj"), "center_cm": cm([cxm, cym]),
             "size_m": round(side, 1), "z_cm": 0.0}
    return {"step_px": s, "spacing_m": spacing, "tiles": tiles, "water": water,
            "first_vertex_cm": cm([east0, south0])}


def write_masks(T, path, size):
    M = T.masks()
    chans = [Image.fromarray((np.clip(M[..., k], 0, 1) * 255).astype(np.uint8), "L").resize((size, size), Image.BILINEAR)
             for k in range(4)]
    Image.merge("RGBA", chans).save(path)


def rel(p):
    try:
        return str(Path(p).resolve().relative_to(fl.ROOT.resolve())).replace("\\", "/")
    except ValueError:
        return str(p).replace("\\", "/")


# ---------------------------------------------------------------------------------------------- previews

def shade_of(h, m):
    gy, gx = np.gradient(h, m)
    return np.clip(0.55 + 0.45 * (-gx * 0.7 - gy * 0.7) / np.sqrt(1 + gx ** 2 + gy ** 2), 0, 1)


def relief_rgb(h, m):
    ss = bt.smoothstep
    shade = shade_of(h, m)
    depth_col = np.stack([ss(-40, 0, h) * 0.4, ss(-40, 0, h) * 0.9, 0.35 + 0.65 * ss(-300, -2, h)], -1)
    land_col = np.stack([0.55 + 0.1 * ss(0, 4, h), 0.5 + 0.2 * ss(0, 4, h), np.full_like(h, 0.3)], -1)
    col = np.where((h > 0)[..., None], land_col, depth_col) * shade[..., None]
    return (np.clip(col, 0, 1) * 255).astype(np.uint8)


def classes_rgb(T):
    """Classes after the cut: kept land, reef atlas classes of the water, former land (reef flat), lowered floor,
    hoa channel, beach of the new shores."""
    cls = T.cls
    col = bt.CLASS_COLORS[cls].copy()
    col[T.K] = bt.CLASS_COLORS[bt.LAND]
    lowered = ~T.K & (T.h < T.h0 - 1.0) & ~T.E
    col[lowered] = (150, 110, 200)
    col[T.E] = (250, 160, 60)
    col[T.hoa_core] = (240, 40, 40)
    beach = T.K & (T.d_toE <= T.R["terrain"]["beach_m"])
    col[beach] = (255, 120, 200)
    return col


def overlay(img, T, fr, sc, title, legend):
    dr = ImageDraw.Draw(img)
    f = fl.font(15)

    def px(p):
        c, r = T.to_px(p[0], p[1])
        return (float(c) * sc, float(r) * sc)
    m_px = sc / T.m
    # navigable ring: outline of the shallows (< boat_min_depth_m) around the kept land
    nav = T.h <= -T.R["terrain"]["boat_min_depth_m"]
    lab, _ = ndi.label(~nav)
    kl = np.unique(lab[T.K])
    obstacle = np.isin(lab, kl[kl > 0])
    edge = obstacle & ~ndi.binary_erosion(obstacle)
    er, ec = np.nonzero(edge)
    for r, c in zip(er[::2], ec[::2]):
        x, y = c * sc, r * sc
        dr.point((x, y), fill=(255, 255, 0))
    for c in fr["cuts"]:
        if c["type"] == "synthetique":
            a, b = (np.array(q) / 100.0 for q in c["line_cm"])
            dr.line([px(a), px(b)], fill=(255, 40, 40), width=2)
    arr = fr.get("arrival")
    if arr:
        p0 = np.array(arr["location_cm"][:2]) / 100.0
        t = np.array(arr["target_cm"]) / 100.0
        dr.line([px(p0), px(t)], fill=(255, 255, 255), width=1)
        yaw = math.radians(arr["yaw_deg"])
        fwd = np.array([math.cos(yaw), math.sin(yaw)])
        side = np.array([-fwd[1], fwd[0]])
        tri = [p0 + fwd * 30, p0 - fwd * 15 + side * 12, p0 - fwd * 15 - side * 12]
        dr.polygon([px(q) for q in tri], fill=(255, 255, 255), outline=(0, 0, 0))
        dr.text(px(p0 + side * 60), "barque", fill=(255, 255, 255), font=f, anchor="mm")
    ga = fr.get("galleon_anchor")
    if ga:
        q = np.array(ga["location_cm"][:2]) / 100.0
        c = px(q)
        rr = ga["radius_m"] * m_px
        dr.ellipse([c[0] - rr, c[1] - rr, c[0] + rr, c[1] + rr], outline=(255, 140, 0), width=3)
        dr.text((c[0], c[1] + rr + 12), f"galion {ga['depth_m']:.1f} m", fill=(255, 140, 0), font=f, anchor="mm")
    x0, y0 = 20, img.size[1] - 30
    dr.line([(x0, y0), (x0 + 500 * m_px, y0)], fill=(255, 255, 255), width=4)
    dr.text((x0, y0 - 18), "500 m", fill=(255, 255, 255), font=f)
    hh = 26 + 18 * len(legend)
    dr.rectangle([0, 0, img.size[0], hh], fill=(0, 0, 0))
    dr.text((8, 4), title, fill=(255, 255, 255), font=fl.font(15))
    for i, t in enumerate(legend):
        dr.text((8, 24 + 18 * i), t, fill=(200, 200, 200), font=fl.font(13))
    return img


def write_previews(T, fr, out_dir, metrics):
    H, W = T.shape
    sc = min(1200 / max(H, W), 2.0)
    size = (int(W * sc), int(H * sc))
    fid = fr["id"]
    head = (f"{fid} : boite {W * T.m / 1000:.2f} x {H * T.m / 1000:.2f} km, terre {fr['land_ha']:.0f} ha, "
            f"pente max nouvelles rives {metrics['max_slope_new_shores_deg']:.1f} deg, bord {metrics['edge_max_floor_m']:.0f} m")
    common = ["jaune = limite des hauts-fonds (< 1,2 m) : la barque fait le tour au-dela ; rouge = hoa synthetique ;",
              "fleche = arrivee de la barque ; cercle orange = mouillage du galion (40 m, >= 4 m de fond)"]
    img = Image.fromarray(relief_rgb(T.h, T.m)).resize(size, Image.BILINEAR)
    overlay(img, T, fr, sc, head, ["relief et bathymetrie estimee (bleu clair = peu profond, fonce = profond)"] + common)
    img.save(out_dir / "preview_relief.png")
    img = Image.fromarray(classes_rgb(T)).resize(size, Image.NEAREST)
    overlay(img, T, fr, sc, head, ["classes : vert = terre gardee ; rose = plage des nouvelles rives ; orange = terre exclue "
                                   "devenue platier ;", "violet = fond abaisse de plus de 1 m (rampe vers -30 m) ; rouge plein = chenal du hoa ; "
                                   "reste = atlas IRD (creme = pente interne)"] + common)
    img.save(out_dir / "preview_classes.png")
    M = T.masks()
    img = Image.fromarray((np.clip(M[..., :3], 0, 1) * 255).astype(np.uint8)).resize(size, Image.BILINEAR)
    overlay(img, T, fr, sc, head, ["masques : R = vegetation, G = sable, B = recif (A = humidite, non affiche)"] + common)
    img.save(out_dir / "preview_masks.png")
    sh = shade_of(T.h * 4.0, T.m)
    g = (np.clip(sh, 0, 1) * 255).astype(np.uint8)
    img = Image.fromarray(np.stack([g, g, g], -1)).resize(size, Image.BILINEAR)
    overlay(img, T, fr, sc, head, ["ombrage (relief x4) : une marche ou un mur se verrait comme un trait net"] + common)
    img.save(out_dir / "preview_ombrage.png")


def write_heightmap(T, path):
    hmin, hmax = float(T.h.min()), float(T.h.max())
    Image.fromarray(((T.h - hmin) / max(hmax - hmin, 1e-6) * 65535).astype(np.uint16)).save(path)
    return {"file": rel(path), "min_m": hmin, "max_m": hmax,
            "note": "valeur = min_m + pixel / 65535 * (max_m - min_m) ; meme grille que la fenetre source"}


def georef_of(A, T, fr, R, mesh, masks_file, heightmap, metrics, errors):
    r0, r1, c0, c1 = T.win
    x0 = (c0 * A.m - A.cx) * 100
    y0 = (r0 * A.m - A.cz) * 100
    x1 = (c1 * A.m - A.cx) * 100
    y1 = (r1 * A.m - A.cz) * 100
    Tr = R["terrain"]
    return {
        "island": A.island, "fragment": fr["id"], "generator": "scripts/map/fragments/build_fragment.py",
        "frame": "repere monde de Lvl_Raraka (terrain/georef.json) : origine = centre de l'emprise de l'atoll, X = est, "
                 "Y = sud, Z = haut (cm), Z = 0 niveau moyen de la mer. Pas de recentrage : les donnees de placement "
                 "de Lvl_Raraka (villages, camps, marae, arrivees, grille d'inondation) se reutilisent telles quelles.",
        "offset_cm": [0.0, 0.0, 0.0],
        "origin": "celle de Lvl_Raraka",
        "box_cm": fr["box_cm"],
        "terrain_extent_cm": [round(x0, 1), round(y0, 1), round(x1, 1), round(y1, 1)],
        "source_window_px": {"row0": r0, "row1": r1, "col0": c0, "col1": c1, "ground_m_per_px": A.m,
                             "note": "fenetre de la grille de terrain/georef.json (bords de pixels)"},
        "mesh": mesh,
        "masks_texture": {"file": masks_file, "size": Tr["masks_size"],
                          "uv0": "UV0 = ((x - terrain_extent_cm[0]) / largeur, (y - terrain_extent_cm[1]) / hauteur) sur "
                                 "l'emprise du terrain du fragment",
                          "channels": {"R": "vegetation / litiere", "G": "sable", "B": "recif / debris coralliens",
                                       "A": "humidite (1 = immerge)"}},
        "heightmap_16": heightmap,
        "rules_terrain": Tr,
        "galleon_anchor": fr.get("galleon_anchor"),
        "arrival": fr.get("arrival"),
        "checks": {"errors": errors, "metrics": metrics},
        "estimates": "hauteurs des motu et profondeurs du lagon estimees par build_terrain.py (pas de releve) ; la "
                     "rampe de marge, le platier des motu exclus et les hoa synthetiques sont des constructions de jeu",
        "sources": ["Trait de cote : Te Fenua / DAF (Licence Ouverte Etalab 2.0)",
                    "Atlas des recifs coralliens de Polynesie francaise, Andrefouet et al., IRD 2005, via Te Fenua",
                    "Hauts-fonds : contraste local de l'imagerie (c) Maxar via Te Fenua (derive, image non embarquee)"],
    }


# ---------------------------------------------------------------------------------------------- memory

def memory_path(island):
    return fl.ROOT / "docs" / "reference" / "map" / island / "fragments" / "fragments.json"


def write_memory(island, doc, R):
    import find_fragments as ff
    doc["rules"] = R
    memory_path(island).write_text(ff.dump_doc(doc), encoding="utf-8")


# ---------------------------------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("island")
    ap.add_argument("ids", nargs="*")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--anchors", action="store_true")
    ap.add_argument("--force-anchors", action="store_true")
    ap.add_argument("--no-mesh", action="store_true")
    ap.add_argument("--data-root", default=None)
    ap.add_argument("--rules", default=None)
    a = ap.parse_args()
    R = fl.load_rules(a.rules)
    doc = json.loads(memory_path(a.island).read_text(encoding="utf-8"))
    frags = doc["fragments"]
    sel = [f for f in frags if a.all or f["id"] in a.ids]
    missing = set(a.ids) - {f["id"] for f in frags}
    if missing or not sel:
        print(f"fragments inconnus ou aucun choisi : {sorted(missing)}")
        sys.exit(2)
    A = fl.Atoll(a.island, a.data_root, need=("land", "cls", "height"))
    log(f"grille {A.W}x{A.H}, {A.m:.3f} m/px ; donnees : {A.D.used}")
    island_cap = a.island.capitalize()
    failed = 0
    changed_memory = False
    summary = []
    for fr in sel:
        fid = fr["id"]
        T = FragmentTerrain(A, fr, R)
        if a.force_anchors or (a.anchors and not fr.get("galleon_anchor")):
            ga = galleon_anchor(A, fr, R, T)
            fr["galleon_anchor"] = ga
            changed_memory = True
            log(f"{fid} : mouillage du galion " + (f"{ga['depth_m']} m de fond, {ga['behind_arrival_m']} m derriere "
                                                   f"l'arrivee, {ga['beach_dist_m']} m de la plage" if ga else "INTROUVABLE"))
        errors, met = check_terrain(A, fr, R, T)
        for e in errors:
            print(f"[check] ECHEC {fid}: {e}")
        failed += bool(errors)
        summary.append((fid, met, errors))
        log(f"{fid} : fenetre {T.shape[1]}x{T.shape[0]} px, pente max nouvelles rives {met['max_slope_new_shores_deg']} deg, "
            f"fond abaisse {met['max_slope_lowered_floor_deg']} deg, bord {met['edge_max_floor_m']} m, "
            f"terre exclue retiree {met['excluded_land_removed_ha']} ha, hoa {met['hoa']} -> "
            f"{'OK' if not errors else 'ECHEC'}")
        if a.check:
            continue
        out_dir = memory_path(a.island).parent / fid
        out_dir.mkdir(parents=True, exist_ok=True)
        raw_dir = fl.ROOT / "docs" / "reference" / "map" / "raw" / f"{a.island}_fragments" / fid
        raw_dir.mkdir(parents=True, exist_ok=True)
        mesh, masks_file, hm = None, None, None
        if not a.no_mesh:
            mesh = write_meshes(T, fid, raw_dir, island_cap, R)
            mname = f"T_Frag_{island_cap}_{fid.rsplit('_', 1)[1]}_Masks.png"
            write_masks(T, raw_dir / mname, int(R["terrain"]["masks_size"]))
            masks_file = rel(raw_dir / mname)
            hm = write_heightmap(T, raw_dir / "heightmap_16.png")
            log(f"{fid} : {len(mesh['tiles'])} tuiles, {sum(t['triangles'] for t in mesh['tiles'])} triangles -> {rel(raw_dir)}")
        else:
            old = out_dir / "georef.json"
            if old.exists():
                prev = json.loads(old.read_text(encoding="utf-8"))
                mesh, masks_file, hm = prev.get("mesh"), prev["masks_texture"]["file"], prev.get("heightmap_16")
        write_previews(T, fr, out_dir, met)
        geo = georef_of(A, T, fr, R, mesh, masks_file, hm, met, errors)
        (out_dir / "georef.json").write_text(json.dumps(geo, indent=1, ensure_ascii=False), encoding="utf-8")
    if changed_memory:
        write_memory(a.island, doc, R)
        log(f"memoire mise a jour : {rel(memory_path(a.island))}")
    print(f"[check] {len(sel)} fragment(s), {failed} en echec -> {'OK' if not failed else 'ECHEC'}")
    sys.exit(1 if failed else 0)


if __name__ == "__main__":
    main()
