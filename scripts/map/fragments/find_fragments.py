"""Find the expedition fragments of a real atoll: pieces of the ring cut only by the sea (ADR-004).

Usage (from the repository root):
  python scripts/map/fragments/find_fragments.py raraka [--seed 7] [--reset] [--regen FRG_Raraka_03 ...]
         [--data-root <checkout with the LFS data>] [--rules <rules.json>] [--check]

Method (parameters in scripts/map/fragments/rules.json, never in the code):
  1. Ring axis: median radius of the rim (land of rivage.geojson + reef flat of the IRD atlas, classes of
     build_terrain.py) per angular sector around the lagoon centre, smoothed; every point gets its abscissa s
     along the ring (origin in the middle of the longest stretch without land).
  2. Cut points:
     - natural: land pieces closer than natural_min_water_m (60 m) form one group; two groups are always separated
       by at least 60 m of water (hoa, lagoon, ocean): a fragment boundary between groups is a natural cut;
     - synthetic: in big groups, the necks of the land profile along s (least land removed by a hoa of
       synthetic_hoa_m) become candidate cuts (future hoa of 40-80 m dug by build_fragment.py, T-083), kept only
       when the two sides end up at least 40 m apart.
  3. Segments: consecutive pieces along the ring, 1-2.5 km long and 15-80 ha of land, internal water gaps of at
     most max_internal_gap_m (archipelago fragments: chains of islets joined by hoa where one wades).
  4. Scores: village sites (criteria of place_villages.py: centre > 24 m from the water, road > 12 m, room for at
     least 4 minka 10 m from the water on the plots of LOT_S), marae site (criteria of the Marae des Perles:
     sand, drop < 0.6 m over 23 m), lagoon coves (extraction), moorings, share of coconut grove (cocoteraies.geojson)
     and thicket (masks R > G), synthetic cuts and land removed (penalties), size close to the target.
     Not playable without 3 village sites (350 m apart), 1 marae site and a boat arrival (step 7).
     FRG_<Ile>_01 = best score among the fragments of rules.prototype (one group of motu, no synthetic hoa,
     arrival on the lagoon side); the others are numbered by score.
  5. Exclusions: the real village of Motutapu (no kept land within 500 m of the buildings of its sector); the other
     built sectors (isolated huts, BD TOPO) only get a local exclusion of 50 m (no village or marae site).
  6. Memory (docs/reference/map/<ile>/fragments/fragments.json): a fragment in memory is frozen, never renumbered
     nor moved; a new run only adds fragments on free land. --regen <id> rebuilds one (same id, the new segment that
     overlaps it most), --reset forgets everything. --seed only breaks ties between equal scores.
  7. Arrival of the boat (the game starts in the boat, on the water, in front of the fragment): lagoon side first,
     200-350 m from the shore, at least 1.5 m of water along the straight line to a beach or cove of the fragment
     (except the final approach), no reef flat nor pinnacle on the way, far from the other motu; bow toward the beach.
  8. Galleon mooring (ADR-005, T-083): a fragment without galleon_anchor gets one, computed on the terrain of the
     fragment by build_fragment.py (back from the arrival along the bow axis to >= 4 m of water on a 40 m disk).
Outputs: fragments.json, preview_fragments.png (whole atoll), preview_<FRG>.png (one per fragment), then the check
of check_fragments.py (non-zero exit code on failure).
Frame: UE cm of terrain/georef.json (origin = centre of the extent, X = east, Y = south, Z = up).
Sources: Te Fenua / DAF (Etalab 2.0), atlas IRD 2005; depths and heights are the estimates of build_terrain.py.
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
from scipy.spatial import cKDTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fraglib as fl  # noqa: E402
from fraglib import bt  # noqa: E402

VERSION = 1
T0 = time.time()


def log(msg):
    print(f"[fragments {time.time() - T0:6.1f}s] {msg}", flush=True)


def edt(mask, m):
    return (ndi.distance_transform_edt(mask) * m).astype(np.float32)


def cm(p):
    return [round(float(p[0]) * 100, 1), round(float(p[1]) * 100, 1)]


def sdist(a, b, L):
    d = np.abs(np.mod(np.asarray(a) - np.asarray(b), L))
    return np.minimum(d, L - d)


# ---------------------------------------------------------------------------------------------- fields

class Fields:
    def __init__(self, A, R):
        m = A.m
        self.A, self.R = A, R
        self.d_in = edt(A.land, m)              # land: distance to the water
        self.d_out = edt(~A.land, m)            # water: distance to the land
        lag = np.isin(A.cls, [bt.LAGOON, bt.INNER_SLOPE, bt.PINNACLE, bt.PASS])
        oce = np.isin(A.cls, [bt.OCEAN, bt.OUTER_SLOPE])
        d_lag = edt(~lag, m)
        d_oce = edt(~oce, m)
        self.lagoon_side = d_lag < d_oce
        del d_lag, d_oce
        self.thicket = A.land & (A.veg.astype(np.int16) > A.sand.astype(np.int16))
        lr, lc = np.nonzero(A.land)
        self.lr, self.lc = lr, lc
        self.lx, self.ly = A.px_to_m(lc, lr)
        self.land_tree = cKDTree(np.c_[self.lx, self.ly])
        self.land_index = np.full(A.land.shape, -1, np.int32)
        self.land_index[lr, lc] = np.arange(len(lr))


# ---------------------------------------------------------------------------------------------- exclusions

def exclusions(A, F, R):
    E = R["exclusion"]
    sectors = fl.building_sectors(A.buildings, E["sector_link_m"])
    mot_pt = np.array(E["motutapu_point_cm"]) / 100.0
    near = int(np.argmin([np.linalg.norm(b["xy"] - mot_pt) for b in A.buildings]))
    report, mot_pts, iso_pts = [], [], []
    for k, g in enumerate(sectors):
        B = [A.buildings[i] for i in g]
        P = np.vstack([b["poly"] for b in B])
        c = np.mean([b["xy"] for b in B], axis=0)
        is_mot = near in g
        mats = sorted({b["material"] or "?" for b in B})
        ext = P.max(0) - P.min(0)
        foot = sum(b["area_m2"] for b in B)
        if is_mot:
            decision = (f"village reel de Motutapu : aucune terre gardee a moins de {E['motutapu_buffer_m']:.0f} m "
                        f"du bati (ADR-004 point 5, E-03 d'ADR-001)")
            mot_pts.append(P)
        else:
            decision = (f"{len(B)} cabane(s) isolee(s) ({foot:.0f} m2 au sol, materiaux {', '.join(mats)}, "
                        f"emprise {ext[0]:.0f} x {ext[1]:.0f} m, a {np.linalg.norm(c - mot_pt) / 1000:.1f} km de Motutapu) : "
                        f"campement de coprah, pas un village ; exclusion locale de {E['isolated_buffer_m']:.0f} m "
                        f"autour du bati (aucun site de village ni de marae), la terre reste utilisable")
            iso_pts.append(P)
        report.append({"sector": f"S{k + 1}", "motutapu": is_mot, "buildings": len(B), "footprint_m2": round(foot),
                       "center_cm": cm(c), "extent_m": [round(float(ext[0])), round(float(ext[1]))],
                       "materials": mats, "decision": decision})
    mot_tree = cKDTree(np.vstack(mot_pts))
    iso_tree = cKDTree(np.vstack(iso_pts)) if iso_pts else None
    d_mot, _ = mot_tree.query(np.c_[F.lx, F.ly], distance_upper_bound=E["motutapu_buffer_m"] + 1)
    forbidden_land = d_mot < E["motutapu_buffer_m"]

    def d_iso(P):
        if iso_tree is None:
            return np.full(len(P), 1e9)
        d, _ = iso_tree.query(np.asarray(P).reshape(-1, 2), distance_upper_bound=1000.0)
        return d
    return report, forbidden_land, d_iso, mot_tree


# ---------------------------------------------------------------------------------------------- groups, necks, pieces

def groups_and_pieces(A, F, ring, R, forbidden_land):
    C = R["cuts"]
    m = A.m
    lab, n = ndi.label(F.d_out <= C["natural_min_water_m"] / 2.0)
    gid = lab[F.lr, F.lc] - 1
    s_land, lat_land = ring.project(F.lx, F.ly)
    L = ring.L
    s_u = s_land.copy()
    order = np.argsort(gid, kind="stable")
    bounds = np.searchsorted(gid[order], np.arange(n + 1))
    groups, necks, pieces = [], [], []
    hoa, binm = C["synthetic_hoa_m"], C["profile_bin_m"]
    for g in range(n):
        idx = order[bounds[g]:bounds[g + 1]]
        if not len(idx):
            continue
        s = s_land[idx]
        if s.max() - s.min() > L / 2:
            s = np.where(s < L / 2, s + L, s)
            s_u[idx] = s
        area_ha = len(idx) * m * m / 1e4
        s0, s1 = float(s.min()), float(s.max())
        grp = {"id": len(groups), "label": g, "idx": idx, "area_ha": area_ha, "s0": s0, "s1": s1, "necks": []}
        groups.append(grp)
        cut_s = []
        if area_ha >= C["neck_min_group_ha"] or (s1 - s0) >= C["neck_min_group_km"] * 1000:
            nb = int((s1 - s0) / binm) + 1
            b = np.minimum(((s - s0) / binm).astype(int), nb - 1)
            counts = np.bincount(b, minlength=nb).astype(float) * m * m
            wb = max(1, int(round(hoa / binm)))
            removed = np.convolve(counts, np.ones(wb), "same")
            win = max(3, int(C["neck_window_m"] / binm) | 1)
            is_min = removed <= ndi.minimum_filter1d(removed, win, mode="nearest") + 1e-6
            pos = s0 + (np.arange(nb) + 0.5) * binm
            ok = is_min & (removed / hoa <= C["neck_max_land_width_m"]) & \
                (pos >= s0 + C["neck_min_piece_m"]) & (pos <= s1 - C["neck_min_piece_m"])
            cand = []
            k = 0
            okl = list(ok)
            while k < nb:                                   # flat runs of minima -> their middle
                if okl[k]:
                    e = k
                    while e + 1 < nb and okl[e + 1] and abs(removed[e + 1] - removed[k]) < 1e-6:
                        e += 1
                    mid = (k + e) // 2
                    cand.append((removed[mid], pos[mid]))
                    k = e + 1
                else:
                    k += 1
            cand.sort()
            for rem, sc in cand:
                if any(abs(sc - c2) < C["neck_min_piece_m"] for c2 in cut_s):
                    continue
                band = idx[np.abs(s - sc) < hoa / 2]
                before = idx[(s < sc - hoa / 2) & (s > sc - 400)]
                after = idx[(s > sc + hoa / 2) & (s < sc + 400)]
                if not len(before) or not len(after):
                    continue
                gap, _ = cKDTree(np.c_[F.lx[before], F.ly[before]]).query(np.c_[F.lx[after], F.ly[after]])
                gap = float(gap.min()) - m                   # pixel centres -> edges
                if gap < C["synthetic_hoa_range_m"][0]:
                    continue
                cut_s.append(sc)
                lat = ring.project(F.lx[band], F.ly[band])[1] if len(band) else np.array([0.0])
                p, t, o = ring.at(np.array([sc]))
                a = p[0] + o[0] * (lat.min() - 20.0)
                bpt = p[0] + o[0] * (lat.max() + 20.0)
                necks.append({"id": len(necks), "group": grp["id"], "s": float(sc), "band": band,
                              "removed_ha": round(len(band) * m * m / 1e4, 3),
                              "land_width_m": round(len(band) * m * m / hoa, 1), "gap_m": round(gap, 1),
                              "line": (a, bpt), "center": p[0]})
        cut_s.sort()
        grp["necks"] = [nk["id"] for nk in sorted((nk for nk in necks if nk["group"] == grp["id"]), key=lambda q: q["s"])]
        edges = [-np.inf] + cut_s + [np.inf]
        for k in range(len(edges) - 1):
            sel = (s >= edges[k]) & (s < edges[k + 1])
            pidx = idx[sel]
            if not len(pidx):
                continue
            ps = s[sel]
            pieces.append({"id": len(pieces), "group": grp["id"], "rank": k, "idx": pidx,
                           "s0": float(ps.min()), "s1": float(ps.max()), "area_ha": len(pidx) * m * m / 1e4,
                           "neck_before": grp["necks"][k - 1] if k > 0 else None,
                           "neck_after": grp["necks"][k] if k < len(grp["necks"]) else None,
                           "forbidden": bool(forbidden_land[pidx].any()),
                           "coco": int(A.coco[F.lr[pidx], F.lc[pidx]].sum()),
                           "thicket": int(F.thicket[F.lr[pidx], F.lc[pidx]].sum()),
                           "villages": [], "marae": [], "coves": [], "moorings": []})
        grp["pieces"] = [p["id"] for p in pieces if p["group"] == grp["id"]]
    piece_of_land = np.full(len(F.lr), -1, np.int32)
    for p in pieces:
        piece_of_land[p["idx"]] = p["id"]
    return groups, necks, pieces, s_u, piece_of_land


# ---------------------------------------------------------------------------------------------- sites

def village_sites(A, F, R, d_iso, piece_of_land, group_of_piece):
    V = R["village"]
    m = A.m
    step = V["candidate_step_px"]
    cand = A.land & (F.d_in >= V["site_min_shore_m"])
    rr, cc = np.nonzero(cand)
    sel = (rr % step == 0) & (cc % step == 0)
    rr, cc = rr[sel], cc[sel]
    cx, cy = A.px_to_m(cc, rr)
    ok = d_iso(np.c_[cx, cy]) >= R["exclusion"]["isolated_buffer_m"]
    rr, cc, cx, cy = rr[ok], cc[ok], cx[ok], cy[ok]
    # motu axis: structure tensor of the land on a 2x grid (dominant gradient = across the motu)
    land2 = A.land[::2, ::2].astype(np.float32)
    Ls = ndi.gaussian_filter(land2, 1.5)
    gy, gx = np.gradient(Ls)
    sig = V["axis_sigma_m"] / (2 * m)
    a, b, c = (ndi.gaussian_filter(gx * gx, sig), ndi.gaussian_filter(gx * gy, sig), ndi.gaussian_filter(gy * gy, sig))
    r2, c2 = np.minimum(rr // 2, land2.shape[0] - 1), np.minimum(cc // 2, land2.shape[1] - 1)
    th = 0.5 * np.arctan2(2 * b[r2, c2], a[r2, c2] - c[r2, c2])
    del a, b, c, gx, gy, Ls
    nrm = np.c_[np.cos(th), np.sin(th)]          # across the motu (x east, y south)
    tan = np.c_[-np.sin(th), np.cos(th)]          # along the motu: road axis
    P = np.c_[cx, cy]
    own = F.land_index[rr, cc]
    gown = group_of_piece[piece_of_land[own]]

    def land_ok(Q, min_shore):
        x, y = Q[..., 0], Q[..., 1]
        col, row = A.m_to_px(x, y)
        col = np.clip(np.floor(col).astype(int), 0, A.W - 1)
        row = np.clip(np.floor(row).astype(int), 0, A.H - 1)
        li = F.land_index[row, col]
        same = (li >= 0) & (group_of_piece[np.where(li >= 0, piece_of_land[li], 0)] == gown.reshape(
            (-1,) + (1,) * (x.ndim - 1)))
        return same & (F.d_in[row, col] >= min_shore), A.h[row, col]

    # road reach along +/- tan (straight approximation of trace_road, conservative)
    reach = {}
    us = np.arange(6.0, max(V["lot_s_m"]) + 12.0, 6.0)
    for sgn in (1.0, -1.0):
        Q = P[:, None, :] + sgn * us[None, :, None] * tan[:, None, :]
        good, _ = land_ok(Q, V["road_min_shore_m"])
        first_bad = np.where(good.all(1), len(us), np.argmin(good, axis=1))
        reach[sgn] = np.where(first_bad > 0, us[np.maximum(first_bad - 1, 0)], 0.0)
    houses = np.zeros(len(P), int)
    f0, f1 = V["facade_m"] - 1.2, V["facade_m"] + V["house_depth_m"]
    for s_lot in V["lot_s_m"]:
        for sgn in (1.0, -1.0):
            road_ok = reach[sgn] >= s_lot + 6.0
            for side in (1.0, -1.0):
                offs = [(s_lot - 5.1, f0), (s_lot + 5.1, f0), (s_lot - 5.1, f1), (s_lot + 5.1, f1)]
                Q = np.stack([P + sgn * u * tan + side * v * nrm for u, v in offs], 1)
                good, hz = land_ok(Q, V["house_min_shore_m"])
                fits = good.all(1) & ((hz.max(1) - hz.min(1)) <= V["house_max_drop_m"])
                fits &= d_iso(Q.reshape(-1, 2)).reshape(len(P), 4).min(1) >= R["exclusion"]["isolated_buffer_m"]
                houses += (fits & road_ok).astype(int)
    houses = np.minimum(houses, V["max_houses"])
    good = houses >= V["min_houses"]
    out = []
    for i in np.nonzero(good)[0]:
        out.append({"xy": P[i], "axis": tan[i], "houses": int(houses[i]), "shore_m": float(F.d_in[rr[i], cc[i]]),
                    "piece": int(piece_of_land[own[i]])})
    return out


def marae_sites(A, F, R, d_iso, piece_of_land):
    M = R["marae"]
    m = A.m
    rad = M["diameter_m"] / 2 / m
    k = int(math.ceil(rad))
    yy, xx = np.mgrid[-k:k + 1, -k:k + 1]
    disc = (xx * xx + yy * yy) <= rad * rad + 0.25
    rows, cols = np.nonzero(A.land)
    r0, r1, c0, c1 = rows.min() - k, rows.max() + k + 1, cols.min() - k, cols.max() + k + 1
    h = A.h[r0:r1, c0:c1]
    drop = ndi.maximum_filter(h, footprint=disc) - ndi.minimum_filter(h, footprint=disc)
    sand = ndi.uniform_filter(A.sand[r0:r1, c0:c1].astype(np.float32) / 255.0, size=2 * k + 1)
    cand = A.land[r0:r1, c0:c1] & (F.d_in[r0:r1, c0:c1] >= M["min_shore_m"]) & (drop < M["max_drop_m"]) & \
        (sand >= M["min_sand"])
    rr, cc = np.nonzero(cand)
    sel = (rr % 2 == 0) & (cc % 2 == 0)
    rr, cc = rr[sel] + r0, cc[sel] + c0
    x, y = A.px_to_m(cc, rr)
    ok = d_iso(np.c_[x, y]) >= R["exclusion"]["isolated_buffer_m"] + M["diameter_m"] / 2
    out = []
    for i in np.nonzero(ok)[0]:
        r, c = rr[i], cc[i]
        out.append({"xy": np.array([x[i], y[i]]), "drop_m": float(drop[r - r0, c - c0]),
                    "sand": float(sand[r - r0, c - c0]), "lagoon": bool(F.lagoon_side[r, c]),
                    "piece": int(piece_of_land[F.land_index[r, c]])})
    return out


def coves_and_moorings(A, F, R, piece_of_land):
    Cv, Mo = R["coves"], R["moorings"]
    m = A.m
    size = int(2 * Cv["radius_m"] / m) | 1
    rows, cols = np.nonzero(F.d_out <= max(Cv["max_shore_dist_m"], Mo["shore_band_m"][1]) + 5)
    r0, r1, c0, c1 = rows.min(), rows.max() + 1, cols.min(), cols.max() + 1
    land = A.land[r0:r1, c0:c1]
    frac = ndi.uniform_filter(land.astype(np.float32), size=size)
    d_out = F.d_out[r0:r1, c0:c1]
    lag = F.lagoon_side[r0:r1, c0:c1]
    cove = ~land & lag & (d_out >= Cv["min_shore_dist_m"]) & (d_out <= Cv["max_shore_dist_m"]) &         (frac >= Cv["min_land_fraction"])
    lab, n = ndi.label(cove)
    coves = []
    if n:
        cnt = np.bincount(lab.ravel(), minlength=n + 1)
        com = ndi.center_of_mass(cove, lab, np.arange(1, n + 1))
        for k in range(1, n + 1):
            area_ha = cnt[k] * m * m / 1e4
            if area_ha < Cv["min_area_ha"]:
                continue
            r, c = com[k - 1]
            x, y = A.px_to_m(c + c0, r + r0)
            d, li = F.land_tree.query([x, y])
            coves.append({"xy": np.array([float(x), float(y)]), "area_ha": round(float(area_ha), 2),
                          "shore_m": round(float(d), 1), "piece": int(piece_of_land[li])})
    band = Mo["shore_band_m"]
    moor = ~land & lag & (d_out >= band[0]) & (d_out <= band[1]) & (A.h[r0:r1, c0:c1] <= Mo["max_floor_m"])
    rr, cc = np.nonzero(moor)
    sel = (rr % 3 == 0) & (cc % 3 == 0)
    rr, cc = rr[sel] + r0, cc[sel] + c0
    x, y = A.px_to_m(cc, rr)
    _, li = F.land_tree.query(np.c_[x, y])
    moorings = [{"xy": np.array([x[i], y[i]]), "floor_m": float(A.h[rr[i], cc[i]]), "piece": int(piece_of_land[li[i]]),
                 "shore_m": float(F.d_out[rr[i], cc[i]])} for i in range(len(rr))]
    return coves, moorings


# ---------------------------------------------------------------------------------------------- segments

def pick_spread(items, key, count, spacing):
    out = []
    for it in sorted(items, key=key):
        if all(np.linalg.norm(it["xy"] - o["xy"]) >= spacing for o in out):
            out.append(it)
            if len(out) >= count:
                break
    return out


def evaluate(run, pieces, groups, necks, ring, R, rng):
    """Validity, sites and score of a run of pieces. Returns a dict or None (with the reason in run['why'])."""
    S, V, M, Sc = R["segment"], R["village"], R["marae"], R["score"]
    L = ring.L
    pcs = [pieces[i] for i in run["pieces"]]
    land = sum(p["area_ha"] for p in pcs)
    # each group must be used as a contiguous range of its pieces: cuts at the range ends
    cuts, removed = [], 0.0
    by_group = {}
    for p in pcs:
        by_group.setdefault(p["group"], []).append(p["rank"])
    for g, ranks in by_group.items():
        ranks.sort()
        if ranks != list(range(ranks[0], ranks[-1] + 1)):
            return None, "pieces non contigues d'un meme groupe"
        gp = groups[g]["pieces"]
        first, last = pieces[gp[ranks[0]]], pieces[gp[ranks[-1]]]
        for nk in (first["neck_before"], last["neck_after"]):
            if nk is not None:
                cuts.append(necks[nk])
                removed += necks[nk]["removed_ha"]
    land_kept = land - removed / 2.0     # each hoa takes about half its band from the kept side
    if not (S["land_ha"][0] <= land_kept <= S["land_ha"][1]):
        return None, "surface"
    span = run["span"]
    if not (S["ring_km"][0] * 1000 <= span <= S["ring_km"][1] * 1000):
        return None, "longueur"

    def far_from_cuts(it, clear):
        if not cuts:
            return True
        s_it, _ = ring.project([it["xy"][0]], [it["xy"][1]])
        return all(sdist(s_it[0], nk["s"], L) >= clear for nk in cuts)
    vill = [v for p in pcs for v in p["villages"] if far_from_cuts(v, V["cut_clearance_m"])]
    jitter = {id(v): rng.uniform(0, 1e-3) for v in vill}
    chosen = pick_spread(vill, lambda v: (-v["houses"], -v["shore_m"] - jitter[id(v)]), V["count"], V["spacing_m"])
    if len(chosen) < V["count"]:
        return None, "sites de village"
    mar = [q for p in pcs for q in p["marae"] if far_from_cuts(q, M["cut_clearance_m"]) and
           all(np.linalg.norm(q["xy"] - v["xy"]) >= M["min_village_dist_m"] for v in chosen)]
    if not mar:
        return None, "site de marae"
    mar_best = pick_spread(mar, lambda q: (not q["lagoon"], q["drop_m"], -q["sand"]), 3, 150.0)
    coves = sorted([c for p in pcs for c in p["coves"]], key=lambda c: -c["area_ha"])
    moor_all = [q for p in pcs for q in p["moorings"]]
    moor = pick_spread(moor_all, lambda q: q["shore_m"], R["moorings"]["count"], R["moorings"]["spacing_m"])
    n_groups = len(by_group)
    coco = sum(p["coco"] for p in pcs) / max(1, sum(len(p["idx"]) for p in pcs))
    thick = sum(p["thicket"] for p in pcs) / max(1, sum(len(p["idx"]) for p in pcs))
    archipel = n_groups >= S["archipel_min_groups"]
    terms = {
        "villages": round(Sc["village_capacity"] * sum(v["houses"] for v in chosen), 2),
        "marae": round(Sc["marae_sites"] * min(len(mar_best), 2), 2),
        "criques": round(Sc["coves"] * min(len(coves), 2), 2),
        "mouillages": round(Sc["moorings"] * (len(moor) >= R["moorings"]["count"]), 2),
        "cocoteraie": round(Sc["coconut"] * min(coco / Sc["coconut_target"], 1.0), 2),
        "fourre": round(Sc["thicket"] * min(thick / Sc["thicket_target"], 1.0), 2),
        "coupes_synthetiques": round(-Sc["synthetic_cut"] * len(cuts), 2),
        "terre_creusee": round(-Sc["removed_ha"] * removed, 2),
        "taille": round(-Sc["size_ha"] * abs(land_kept - Sc["target_ha"]) - Sc["size_km"] * abs(span / 1000 - Sc["target_km"]), 2),
        "groupes": round(-Sc["extra_group"] * (n_groups - 1), 2),
    }
    return {"pieces": run["pieces"], "groups": n_groups, "land_ha": land_kept, "span_m": span, "s0": run["s0"],
            "s1": run["s1"], "cuts": cuts, "removed_ha": removed, "villages": chosen, "marae": mar_best,
            "coves": coves, "moorings": moor, "coco": coco, "thicket": thick, "archipel": archipel,
            "terms": terms, "score": round(sum(terms.values()), 2)}, None


def enumerate_runs(pieces, groups, necks, ring, R, taken, rng):
    S = R["segment"]
    L = ring.L
    order = sorted(range(len(pieces)), key=lambda i: ((pieces[i]["s0"] + pieces[i]["s1"]) / 2) % L)
    n = len(order)
    results, reasons = [], {}
    for a in range(n):
        first = pieces[order[a]]
        if first["forbidden"] or first["id"] in taken:
            continue
        base = ((first["s0"] + first["s1"]) / 2) % L - (first["s0"] + first["s1"]) / 2
        s0, s1 = first["s0"] + base, first["s1"] + base
        run = [first["id"]]
        for k in range(n):
            if k:
                q = pieces[order[(a + k) % n]]
                if q["forbidden"] or q["id"] in taken:
                    break
                c = (q["s0"] + q["s1"]) / 2
                off = ((c % L) - c) + (L if (a + k) >= n else 0.0)
                q0, q1 = q["s0"] + off, q["s1"] + off
                if q0 - s1 > S["max_internal_gap_m"]:
                    break
                s0, s1 = min(s0, q0), max(s1, q1)
                run.append(q["id"])
            if s1 - s0 > S["ring_km"][1] * 1000:
                break
            if sum(pieces[i]["area_ha"] for i in run) > S["land_ha"][1] * 1.3:
                break
            res, why = evaluate({"pieces": list(run), "span": s1 - s0, "s0": s0 % L, "s1": s1 % L},
                                pieces, groups, necks, ring, R, rng)
            if res:
                results.append(res)
            else:
                reasons[why] = reasons.get(why, 0) + 1
    return results, reasons


# ---------------------------------------------------------------------------------------------- arrival

def find_arrival(A, F, R, kept, frag, cuts=()):
    """Boat arrival point in front of the fragment (see module doc). Rays are cast from every beach point of the
    fragment in a fan of directions; the arrival is a point of a ray 200-350 m from the fragment shore, with no land
    nor pinnacle before it and at least min_depth_m of water (no reef flat) beyond the final approach of the beach.
    The shortest final approach that works is kept (lagoon side first)."""
    Ar = R["arrival"]
    m = A.m
    kr, kc = np.nonzero(kept)
    edge = F.d_in[kr, kc] <= 1.5 * m
    er, ec = kr[edge], kc[edge]
    ex, ey = A.px_to_m(ec, er)
    ktree = cKDTree(np.c_[ex, ey])
    step = Ar["shore_step_px"]
    sel = (er % step == 0) & (ec % step == 0)
    for nk in cuts:          # the beach must exist today: not on the future hoa (T-083) nor right next to it
        sel &= fl.seg_points_dist(np.c_[ex, ey], *nk["line"]) > R["cuts"]["synthetic_hoa_m"] / 2 + 80.0
    S = np.c_[ex[sel], ey[sel]]
    if not len(S):
        return None
    lo, hi = Ar["shore_dist_m"]
    pad = int((hi + Ar["other_land_min_m"] + 100) / m)
    r0, r1 = max(kr.min() - pad, 0), min(kr.max() + pad + 1, A.H)
    c0, c1 = max(kc.min() - pad, 0), min(kc.max() + pad + 1, A.W)
    orr, occ = np.nonzero(A.land[r0:r1, c0:c1] & ~kept[r0:r1, c0:c1])
    otree = cKDTree(np.c_[A.px_to_m(occ + c0, orr + r0)]) if len(orr) else None
    ang = np.linspace(0, 2 * math.pi, Ar["directions"], endpoint=False)
    D = np.c_[np.cos(ang), np.sin(ang)]
    u = np.arange(Ar["sample_m"], hi + 40.0, Ar["sample_m"])
    Q = S[:, None, None, :] + D[None, :, None, :] * u[None, None, :, None]
    col, row = A.m_to_px(Q[..., 0], Q[..., 1])
    col = np.clip(np.floor(col).astype(int), 0, A.W - 1)
    row = np.clip(np.floor(row).astype(int), 0, A.H - 1)
    h, cls = A.h[row, col], A.cls[row, col]
    blocked = (A.land[row, col] & (u > 2 * m)) | (cls == bt.PINNACLE)
    blocked_cum = np.cumsum(blocked, axis=2) > 0
    shallow = (h > -Ar["min_depth_m"]) | (cls == bt.REEF_FLAT)
    far = u >= lo
    cand = far[None, None, :] & ~blocked_cum
    ci = np.nonzero(cand)
    P = Q[ci]
    dk, _ = ktree.query(P)
    ok = (dk >= lo) & (dk <= hi)
    if otree is not None:
        do, _ = otree.query(P)
        ok &= do >= Ar["other_land_min_m"]
    ci = tuple(a[ok] for a in ci)
    P, dk = P[ok], dk[ok]
    if not len(P):
        return None
    lag = F.lagoon_side[row[ci], col[ci]]
    cove_xy = np.array([c["xy"] for c in frag["coves"]]) if frag["coves"] else np.zeros((0, 2))
    tgt = S[ci[0]]
    in_cove = np.zeros(len(P), bool)
    for c in cove_xy:
        in_cove |= np.linalg.norm(tgt - c, axis=1) <= 80.0
    for fa in Ar["final_approach_m"]:
        # shallow water allowed only within fa of the beach (u <= fa), up to the arrival point
        bad = shallow & (u > fa)[None, None, :]
        bad_cum = np.cumsum(bad, axis=2) > 0
        good = ~bad_cum[ci]
        if not good.any():
            continue
        for side in (True, False):
            g = good & (lag == side)
            if not g.any():
                continue
            score = in_cove * 1.0 - np.abs(dk - 275.0) / 200.0 + 0.02 * np.minimum(-h[ci], 10.0)
            order = np.argsort(-np.where(g, score, -np.inf))[:int(g.sum())]
            k = None
            for kk in order[:400]:           # same test as check_fragments.py, on the rounded coordinates
                pr, tr = np.round(P[kk] * 1000) / 1000, np.round(tgt[kk] * 1000) / 1000
                ok, _, depth = fl.arrival_line_ok(A, pr, tr, fa, Ar["min_depth_m"])
                if ok:
                    k = int(kk)
                    break
            if k is None:
                continue
            p, t = pr, tr
            v = t - p
            return {"location_cm": [round(float(p[0]) * 100, 1), round(float(p[1]) * 100, 1), 0.0],
                    "yaw_deg": round(math.degrees(math.atan2(v[1], v[0])), 1),
                    "target_cm": cm(t), "target": "crique" if in_cove[k] else "plage",
                    "side": "lagon" if side else "ocean", "shore_dist_m": round(float(dk[k]), 1),
                    "line_m": round(float(np.linalg.norm(v)), 1),
                    "min_depth_m": round(depth, 2),
                    "final_approach_m": fa}
    return None


# ---------------------------------------------------------------------------------------------- output

def build_fragment(A, F, ring, R, res, pieces, s_u, fid, seed):
    kept = np.zeros(A.land.shape, bool)
    for i in res["pieces"]:
        p = pieces[i]
        kept[F.lr[p["idx"]], F.lc[p["idx"]]] = True
    for nk in res["cuts"]:
        kept[F.lr[nk["band"]], F.lc[nk["band"]]] = False
    # drop specks left by the hoa bands (< 3 px), only next to a synthetic cut (real islets stay)
    if res["cuts"]:
        lab, n = ndi.label(kept)
        cnt = np.bincount(lab.ravel())
        small = (cnt < 3)
        small[0] = False
        sr, sc_ = np.nonzero(small[lab])
        if len(sr):
            sx, sy = A.px_to_m(sc_, sr)
            near = np.zeros(len(sr), bool)
            for nk in res["cuts"]:
                near |= fl.seg_points_dist(np.c_[sx, sy], *nk["line"]) <= R["cuts"]["synthetic_hoa_m"] / 2 + 30
            kept[sr[near], sc_[near]] = False
    polys = fl.mask_to_polygons_cm(A, kept)
    kr, kc = np.nonzero(kept)
    kx, ky = A.px_to_m(kc, kr)
    land_ha = len(kr) * A.m * A.m / 1e4
    bb = [float(kx.min() - A.m / 2), float(ky.min() - A.m / 2), float(kx.max() + A.m / 2), float(ky.max() + A.m / 2)]
    mg = R["segment"]["box_margin_m"]
    cuts = []
    kept_tree = cKDTree(np.c_[kx, ky])
    other = A.land & ~kept
    # natural cuts at both ends of the segment along the ring (lateral sides are open sea)
    L = ring.L
    for end, sign in (("debut", -1.0), ("fin", 1.0)):
        if any(abs(sdist(nk["s"], res["s0"] if sign < 0 else res["s1"], L)) < 300 for nk in res["cuts"]):
            continue
        s_end = res["s0"] if sign < 0 else res["s1"]
        orr, occ = np.nonzero(other)
        ox, oy = A.px_to_m(occ, orr)
        so, _ = ring.project(ox, oy)
        rel = np.mod((so - s_end) * sign, L)
        near = rel <= 2500.0
        if not near.any():
            cuts.append({"type": "naturel", "end": end, "water_width_m": None, "note": "pas de terre a moins de 2,5 km"})
            continue
        d, k = kept_tree.query(np.c_[ox[near], oy[near]])
        j = int(np.argmin(d))
        a = np.array([ox[near][j], oy[near][j]])
        b = np.array([kx[k[j]], ky[k[j]]])
        mid = (a + b) / 2
        sm, _ = ring.project([mid[0]], [mid[1]])
        pp, tt, oo = ring.at(sm)
        cuts.append({"type": "naturel", "end": end, "water_width_m": round(float(d[j]) - A.m, 1),
                     "position_cm": cm(mid), "s_m": round(float(sm[0]), 1),
                     "line_cm": [cm(pp[0] - oo[0] * 300), cm(pp[0] + oo[0] * 300)],
                     "nearest_kept_cm": cm(b), "nearest_other_cm": cm(a)})
    for nk in res["cuts"]:
        a, b = nk["line"]
        cuts.append({"type": "synthetique", "s_m": round(nk["s"], 1), "position_cm": cm(nk["center"]),
                     "line_cm": [cm(a), cm(b)], "hoa_width_m": R["cuts"]["synthetic_hoa_m"],
                     "land_width_m": nk["land_width_m"], "removed_ha": nk["removed_ha"], "gap_after_cut_m": nk["gap_m"]})
    sand_share = float((A.sand[kr, kc] > A.veg[kr, kc]).mean())
    kind = "archipel" if res["archipel"] else ("motu_scinde" if res["cuts"] else "naturel")
    frag = {
        "id": fid, "frozen": True, "review": "a-revoir", "kind": kind, "display_name": "",
        "seed": seed, "land_ha": round(land_ha, 2), "ring_km": round(res["span_m"] / 1000, 3),
        "ring_s_m": [round(res["s0"], 1), round(res["s1"], 1)], "groups": res["groups"],
        "center_cm": cm([kx.mean(), ky.mean()]),
        "bbox_cm": [round(v * 100, 1) for v in bb],
        "box_cm": [round((bb[0] - mg) * 100, 1), round((bb[1] - mg) * 100, 1), round((bb[2] + mg) * 100, 1),
                   round((bb[3] + mg) * 100, 1)],
        "land_polygons_cm": polys,
        "cuts": cuts,
        "sites": {
            "villages": [{"id": f"Z{k + 1}", "center_cm": cm(v["xy"]), "axis": [round(float(v["axis"][0]), 3),
                                                                             round(float(v["axis"][1]), 3)],
                          "houses_estimate": v["houses"], "shore_m": round(v["shore_m"], 1)}
                         for k, v in enumerate(res["villages"])],
            "marae": [{"center_cm": cm(q["xy"]), "drop_m": round(q["drop_m"], 2), "sand": round(q["sand"], 2),
                       "lagoon_side": q["lagoon"]} for q in res["marae"]],
            "coves": [{"center_cm": cm(c["xy"]), "area_ha": c["area_ha"], "shore_m": c["shore_m"]} for c in res["coves"]],
            "extraction_cove": (cm(res["coves"][0]["xy"]) if res["coves"] else None),
            "moorings": [{"location_cm": cm(q["xy"]), "floor_m": round(q["floor_m"], 2), "shore_m": round(q["shore_m"], 1)}
                         for q in res["moorings"]],
        },
        "shares": {"cocoteraie": round(res["coco"], 3), "fourre": round(res["thicket"], 3), "sable": round(sand_share, 3)},
        "scores": dict(res["terms"], total=res["score"]),
    }
    frag["arrival"] = find_arrival(A, F, R, kept, {"coves": res["coves"]}, res["cuts"])
    ex = frag["sites"]["extraction_cove"]
    if ex is not None:
        frag["sites"]["extraction"] = {"kind": "crique", "center_cm": ex}
    elif frag["arrival"] and frag["arrival"]["side"] == "lagon":
        frag["sites"]["extraction"] = {"kind": "plage_arrivee", "center_cm": frag["arrival"]["target_cm"],
                                       "note": "pas de crique detectee : la plage d'arrivee cote lagon sert d'extraction"}
    else:
        frag["sites"]["extraction"] = None
    return frag, kept


# ---------------------------------------------------------------------------------------------- previews

PALETTE = [(255, 120, 60), (80, 220, 120), (90, 170, 255), (240, 220, 70), (230, 90, 200), (60, 230, 230),
           (250, 150, 150), (170, 120, 255), (150, 230, 90), (255, 180, 40)]


def class_colors(A, win=None):
    r0, r1, c0, c1 = win or (0, A.H, 0, A.W)
    cls = A.cls[r0:r1, c0:c1]
    col = np.zeros(cls.shape + (3,), np.uint8)
    col[:] = (16, 42, 92)
    col[np.isin(cls, [bt.LAGOON, bt.PASS])] = (36, 110, 160)
    col[cls == bt.INNER_SLOPE] = (70, 150, 185)
    col[cls == bt.REEF_FLAT] = (120, 175, 190)
    col[cls == bt.OUTER_SLOPE] = (30, 70, 120)
    col[cls == bt.PINNACLE] = (150, 120, 110)
    col[A.land[r0:r1, c0:c1]] = (120, 120, 112)
    return col


def preview_global(path, A, ring, doc, kept_masks, sectors, mot_tree, R):
    S = 4
    col = class_colors(A)
    for k, (fid, kept) in enumerate(kept_masks.items()):
        col[kept] = PALETTE[k % len(PALETTE)]
    mot = np.zeros(A.land.shape, bool)
    lr, lc = np.nonzero(A.land)
    lx, ly = A.px_to_m(lc, lr)
    d, _ = mot_tree.query(np.c_[lx, ly], distance_upper_bound=R["exclusion"]["motutapu_buffer_m"] + 1)
    mot[lr[d < R["exclusion"]["motutapu_buffer_m"]], lc[d < R["exclusion"]["motutapu_buffer_m"]]] = True
    col[mot] = (210, 40, 40)
    img = Image.fromarray(col).resize((A.W // S, A.H // S), Image.NEAREST)
    dr = ImageDraw.Draw(img)

    def px(p):
        c, r = A.m_to_px(p[0], p[1])
        return (float(c) / S, float(r) / S)
    _, pts = ring.polyline(100.0)
    dr.line([px(p) for p in pts] + [px(pts[0])], fill=(250, 230, 120), width=1)
    f_big, f_small = fl.font(30), fl.font(16)
    for k, fr in enumerate(doc["fragments"]):
        c = np.array(fr["center_cm"]) / 100.0
        _, _, o = ring.at(ring.project([c[0]], [c[1]])[0])
        lp = c + o[0] * 1500
        dr.line([px(c), px(lp)], fill=(255, 255, 255), width=2)
        dr.text(px(lp), fr["id"].split("_")[-1], fill=(255, 255, 255), font=f_big, anchor="mm")
        for cut in fr["cuts"]:
            if "line_cm" not in cut:
                continue
            a, b = (np.array(q) / 100.0 for q in cut["line_cm"])
            dr.line([px(a), px(b)], fill=(255, 40, 40) if cut["type"] == "synthetique" else (0, 255, 255), width=3)
        if fr.get("arrival"):
            p = np.array(fr["arrival"]["location_cm"][:2]) / 100.0
            q = px(p)
            dr.ellipse([q[0] - 4, q[1] - 4, q[0] + 4, q[1] + 4], fill=(255, 255, 255), outline=(0, 0, 0))
    for s in sectors:
        q = px(np.array(s["center_cm"]) / 100.0)
        dr.rectangle([q[0] - 5, q[1] - 5, q[0] + 5, q[1] + 5], outline=(255, 60, 60) if s["motutapu"] else (255, 170, 0), width=2)
    lines = [f"Fragments de {doc['island'].capitalize()} ({len(doc['fragments'])}) - couleur = terre gardee, numero = FRG_NN, "
             "point blanc = arrivee de la barque",
             "rouge plein = secteur de Motutapu exclu (500 m)  carre orange = cabanes isolees (exclusion locale 50 m)",
             "trait rouge = coupe synthetique (hoa a creuser)  trait cyan = coupe naturelle (eau >= 60 m)  jaune = axe de l'anneau"]
    for i, t in enumerate(lines):
        dr.text((12, img.size[1] - 70 + 20 * i), t, fill=(255, 255, 255), font=f_small)
    img.save(path)


def preview_fragment(path, A, F, fr, kept, R):
    pad = int(450 / A.m)
    kr, kc = np.nonzero(kept)
    arr = fr.get("arrival")
    r0, r1, c0, c1 = kr.min() - pad, kr.max() + pad, kc.min() - pad, kc.max() + pad
    if arr:
        ac, ar_ = A.m_to_px(arr["location_cm"][0] / 100, arr["location_cm"][1] / 100)
        r0, r1 = min(r0, int(ar_) - 60), max(r1, int(ar_) + 60)
        c0, c1 = min(c0, int(ac) - 60), max(c1, int(ac) + 60)
    r0, c0 = max(r0, 0), max(c0, 0)
    r1, c1 = min(r1, A.H), min(c1, A.W)
    col = class_colors(A, (r0, r1, c0, c1))
    k = kept[r0:r1, c0:c1]
    sand = A.sand[r0:r1, c0:c1] >= A.veg[r0:r1, c0:c1]
    col[k] = (225, 210, 150)
    col[k & ~sand] = (95, 150, 70)
    col[k & A.coco[r0:r1, c0:c1]] = (40, 125, 50)
    col[k & F.thicket[r0:r1, c0:c1]] = (120, 125, 45)
    removed = A.land[r0:r1, c0:c1] & ~k & (F.d_in[r0:r1, c0:c1] >= 0)
    col[removed] = (95, 95, 95)
    H0, W0 = col.shape[:2]
    sc = min(1100 / max(H0, W0), 3.0)
    img = Image.fromarray(col).resize((int(W0 * sc), int(H0 * sc)), Image.NEAREST)
    dr = ImageDraw.Draw(img)

    def px(p):
        c, r = A.m_to_px(p[0], p[1])
        return ((float(c) - c0) * sc, (float(r) - r0) * sc)
    m_px = sc / A.m
    for cut in fr["cuts"]:
        if "line_cm" not in cut:
            continue
        a, b = (np.array(q) / 100.0 for q in cut["line_cm"])
        if cut["type"] == "synthetique":
            dr.line([px(a), px(b)], fill=(255, 40, 40), width=max(3, int(cut["hoa_width_m"] * m_px)))
        else:
            dr.line([px(a), px(b)], fill=(0, 255, 255), width=3)
            dr.text(px(a), f"{cut['water_width_m']:.0f} m d'eau", fill=(0, 255, 255), font=fl.font(14))
    f = fl.font(16)
    for v in fr["sites"]["villages"]:
        c = np.array(v["center_cm"]) / 100.0
        q = px(c)
        rr = 85 * m_px
        ax = np.array(v["axis"])
        dr.line([px(c - ax * 81), px(c + ax * 81)], fill=(255, 255, 255), width=2)
        dr.ellipse([q[0] - rr, q[1] - rr, q[0] + rr, q[1] + rr], outline=(255, 255, 255), width=2)
        dr.text((q[0], q[1] - rr - 10), f"{v['id']} ~{v['houses_estimate']} minka", fill=(255, 255, 255), font=f, anchor="mm")
    for j, q in enumerate(fr["sites"]["marae"]):
        p = px(np.array(q["center_cm"]) / 100.0)
        rr = max(4, 11.5 * m_px)
        dr.rectangle([p[0] - rr, p[1] - rr, p[0] + rr, p[1] + rr], outline=(255, 60, 220), width=3 if j == 0 else 1)
        if j == 0:
            dr.text((p[0] + rr + 4, p[1]), "marae", fill=(255, 60, 220), font=f, anchor="lm")
    for c in fr["sites"]["coves"]:
        p = px(np.array(c["center_cm"]) / 100.0)
        dr.ellipse([p[0] - 7, p[1] - 7, p[0] + 7, p[1] + 7], outline=(0, 255, 255), width=2)
    ex = fr["sites"].get("extraction")
    if ex:
        p = px(np.array(ex["center_cm"]) / 100.0)
        dr.text((p[0] + 10, p[1] + 14), "extraction", fill=(0, 255, 255), font=f, anchor="lm")
    for q in fr["sites"]["moorings"]:
        p = px(np.array(q["location_cm"]) / 100.0)
        dr.ellipse([p[0] - 5, p[1] - 5, p[0] + 5, p[1] + 5], fill=(30, 60, 255), outline=(255, 255, 255))
    if arr:
        p0 = np.array(arr["location_cm"][:2]) / 100.0
        t = np.array(arr["target_cm"]) / 100.0
        dr.line([px(p0), px(t)], fill=(255, 255, 255), width=1)
        yaw = math.radians(arr["yaw_deg"])
        fwd = np.array([math.cos(yaw), math.sin(yaw)])
        side = np.array([-fwd[1], fwd[0]])
        tri = [p0 + fwd * 30, p0 - fwd * 15 + side * 12, p0 - fwd * 15 - side * 12]
        dr.polygon([px(q) for q in tri], fill=(255, 255, 255), outline=(0, 0, 0))
        dr.text(px(p0 - fwd * 40), "arrivee barque", fill=(255, 255, 255), font=f, anchor="mm")
    ga = fr.get("galleon_anchor")
    if ga:
        q = px(np.array(ga["location_cm"][:2]) / 100.0)
        rr = max(4, ga["radius_m"] * m_px)
        dr.ellipse([q[0] - rr, q[1] - rr, q[0] + rr, q[1] + rr], outline=(255, 140, 0), width=3)
        dr.text((q[0], q[1] + rr + 10), f"galion ({ga['depth_m']:.1f} m)", fill=(255, 140, 0), font=f, anchor="mm")
    # scale bar 500 m
    x0, y0 = 20, img.size[1] - 30
    dr.line([(x0, y0), (x0 + 500 * m_px, y0)], fill=(255, 255, 255), width=4)
    dr.text((x0, y0 - 18), "500 m", fill=(255, 255, 255), font=f)
    n_nat = sum(1 for c in fr["cuts"] if c["type"] == "naturel")
    n_syn = sum(1 for c in fr["cuts"] if c["type"] == "synthetique")
    head = (f"{fr['id']} ({fr['kind']}) : {fr['land_ha']:.0f} ha, {fr['ring_km']:.2f} km d'anneau, {fr['groups']} groupe(s), "
            f"coupes {n_nat} nat. / {n_syn} synth., note {fr['scores']['total']:.1f}")
    leg = ["terre gardee : sable / vert = vegetation / vert fonce = cocoteraie / olive = fourre ; gris = terre exclue",
           "rouge = hoa synthetique a creuser ; cyan = coupe naturelle ; cercles blancs = zones de village (85 m) ;",
           "carre rose = marae (gras = retenu) ; cercles cyan = criques ; points bleus = mouillages ; fleche = arrivee barque ;",
           "cercle orange = mouillage du galion (ADR-005, disque de 40 m)"]
    dr.rectangle([0, 0, img.size[0], 100], fill=(0, 0, 0))
    dr.text((8, 4), head, fill=(255, 255, 255), font=fl.font(15))
    for i, t in enumerate(leg):
        dr.text((8, 26 + 18 * i), t, fill=(200, 200, 200), font=fl.font(13))
    img.save(path)


def dump_doc(doc):
    """Readable JSON (indent 1) with the long point lists (polygons, ring axis) on one line each."""
    compact = {}

    def mark(v):
        key = f"@@{len(compact)}@@"
        compact[key] = json.dumps(v, separators=(",", ":"))
        return key
    d = json.loads(json.dumps(doc))
    d["ring"]["polyline_cm"] = mark(d["ring"]["polyline_cm"])
    for fr in d["fragments"]:
        for ring in fr["land_polygons_cm"]:
            ring["points"] = mark(ring["points"])
    text = json.dumps(d, indent=1, ensure_ascii=False)
    for key, val in compact.items():
        text = text.replace(f'"{key}"', val)
    return text


# ---------------------------------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("island")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--reset", action="store_true", help="forget the fragments in memory")
    ap.add_argument("--regen", nargs="*", default=[], help="fragment ids to rebuild (same id)")
    ap.add_argument("--data-root", default=None, help="checkout holding the LFS data (default: auto)")
    ap.add_argument("--rules", default=None)
    ap.add_argument("--check", action="store_true", help="only run the check of fragments.json")
    args = ap.parse_args()
    R = fl.load_rules(args.rules)
    out_dir = fl.ROOT / "docs" / "reference" / "map" / args.island / "fragments"
    mem_path = out_dir / "fragments.json"
    import check_fragments as cf
    if args.check:
        sys.exit(cf.run(args.island, args.data_root, args.rules))
    out_dir.mkdir(parents=True, exist_ok=True)
    memory = json.loads(mem_path.read_text(encoding="utf-8")) if mem_path.exists() and not args.reset else None
    rng = np.random.default_rng(args.seed)

    A = fl.Atoll(args.island, args.data_root)
    log(f"grille {A.W}x{A.H}, {A.m:.3f} m/px, terre {A.land.sum() * A.m * A.m / 1e6:.2f} km2 ; donnees : {A.D.used}")
    F = Fields(A, R)
    ring = fl.Ring(A, R, origin_s=(memory or {}).get("ring", {}).get("origin_raw_s_m"))
    log(f"axe de l'anneau : {ring.L / 1000:.1f} km, centre du lagon {cm(ring.center)} cm")
    sectors, forbidden_land, d_iso, mot_tree = exclusions(A, F, R)
    for s in sectors:
        log(f"secteur {s['sector']} : {s['decision']}")
    groups, necks, pieces, s_u, piece_of_land = groups_and_pieces(A, F, ring, R, forbidden_land)
    group_of_piece = np.array([p["group"] for p in pieces] + [-1], np.int32)
    log(f"{len(groups)} groupes de motu (eau < {R['cuts']['natural_min_water_m']:.0f} m), {len(necks)} cous candidats, "
        f"{len(pieces)} morceaux ({sum(p['forbidden'] for p in pieces)} dans le secteur de Motutapu)")
    vs = village_sites(A, F, R, d_iso, piece_of_land, group_of_piece)
    ms = marae_sites(A, F, R, d_iso, piece_of_land)
    coves, moorings = coves_and_moorings(A, F, R, piece_of_land)
    for lst, key in ((vs, "villages"), (ms, "marae"), (coves, "coves"), (moorings, "moorings")):
        for it in lst:
            if it["piece"] >= 0:
                pieces[it["piece"]][key].append(it)
    log(f"sites : {len(vs)} centres de village possibles, {len(ms)} sites de marae, {len(coves)} criques, "
        f"{len(moorings)} points de mouillage")

    keep = [f for f in (memory or {}).get("fragments", []) if f["id"] not in args.regen]
    old_regen = {f["id"]: f for f in (memory or {}).get("fragments", []) if f["id"] in args.regen}
    taken = set()
    kept_masks = {}
    for f in keep:
        mk = fl.polygons_to_mask(A, f["land_polygons_cm"])
        kept_masks[f["id"]] = mk
        hit = piece_of_land[F.land_index[mk & A.land]]
        taken.update(int(i) for i in np.unique(hit) if i >= 0)
    runs, reasons = enumerate_runs(pieces, groups, necks, ring, R, taken, rng)
    log(f"{len(runs)} segments jouables ; rejets : {reasons}")
    runs.sort(key=lambda r: (-r["score"], r["s0"]))
    # greedy, disjoint; a segment without a valid boat arrival is not playable (the game starts in the boat)
    chosen, used, no_arrival = [], set(taken), 0
    for r in runs:
        if not used.isdisjoint(r["pieces"]):
            continue
        fr, kept = build_fragment(A, F, ring, R, r, pieces, s_u, "?", args.seed)
        if fr["arrival"] is None:
            no_arrival += 1
            continue
        chosen.append((r, fr, kept))
        used.update(r["pieces"])
    reasons["pas d'arrivee de barque (segment ecarte a la selection)"] = no_arrival
    Sc = R["score"]
    for r, fr, _ in chosen:          # the arrival joins the score: the game starts in the boat
        a = fr["arrival"]
        fr["scores"]["arrivee"] = round(Sc["arrival_lagoon"] * (a["side"] == "lagon")
                                        - Sc["arrival_approach_per_m"] * a["final_approach_m"], 2)
        fr["scores"]["total"] = round(sum(v for k, v in fr["scores"].items() if k != "total"), 2)
    chosen.sort(key=lambda c: -c[1]["scores"]["total"])
    P = R["prototype"]

    def proto_ok(fr):
        return (fr["groups"] <= P["max_groups"] and fr["arrival"]["side"] == P["arrival_side"] and
                sum(c["type"] == "synthetique" for c in fr["cuts"]) <= P["max_synthetic_cuts"])
    proto_k = next((k for k, c in enumerate(chosen) if proto_ok(c[1])), None)
    if proto_k is not None and not keep:          # numbering only happens at the creation of the catalogue
        chosen.insert(0, chosen.pop(proto_k))
    log(f"{len(chosen)} fragments retenus, {no_arrival} segment(s) ecarte(s) faute d'arrivee de barque")
    island_cap = args.island.capitalize()
    nums = [int(f["id"].rsplit("_", 1)[1]) for f in keep] + [int(i.rsplit("_", 1)[1]) for i in old_regen] + \
        [int(i.rsplit("_", 1)[1]) for i in (memory or {}).get("retired_ids", [])]
    nxt = max(nums + [0]) + 1
    # --regen: the new segment that overlaps the old fragment most gets its id back
    ids = {}
    for rid, old in old_regen.items():
        om = fl.polygons_to_mask(A, old["land_polygons_cm"])
        hit = set(int(i) for i in np.unique(piece_of_land[F.land_index[om & A.land]]) if i >= 0)
        best = max(((len(hit & set(r["pieces"])), k) for k, (r, _, _) in enumerate(chosen) if k not in ids.values()),
                   default=(0, None))
        if best[0] > 0:
            ids[rid] = best[1]
        else:
            log(f"--regen {rid} : aucun segment ne recouvre l'ancien, identifiant retire")
    retired = list((memory or {}).get("retired_ids", [])) + [rid for rid in old_regen if rid not in ids]
    by_k = {k: rid for rid, k in ids.items()}
    new_frags = []
    for k, (r, fr, kept) in enumerate(chosen):
        fid = by_k.get(k)
        if fid is None:
            fid = f"FRG_{island_cap}_{nxt:02d}"
            nxt += 1
        fr["id"] = fid
        kept_masks[fid] = kept
        new_frags.append(fr)
        log(f"{fid} : {fr['kind']}, {fr['land_ha']:.1f} ha, {fr['ring_km']:.2f} km, note {fr['scores']['total']}, "
            f"arrivee {fr['arrival']['side']} a {fr['arrival']['shore_dist_m']:.0f} m, approche {fr['arrival']['final_approach_m']:.0f} m")
    frags = sorted(keep + new_frags, key=lambda f: f["id"])
    import build_fragment as bfr
    for fr in frags:                 # galleon mooring (ADR-005): computed once, then frozen with the fragment
        if not fr.get("galleon_anchor"):
            fr["galleon_anchor"] = bfr.galleon_anchor(A, fr, R, bfr.FragmentTerrain(A, fr, R))
    s_poly, p_poly = ring.polyline(50.0)
    doc = {
        "version": VERSION, "island": args.island,
        "generator": "scripts/map/fragments/find_fragments.py",
        "frame": "UE cm de terrain/georef.json : origine = centre de l'emprise, X = est, Y = sud ; polygones en regle "
                 "pair-impair ; yaw en degres depuis +X vers +Y",
        "sources": ["Trait de cote, batiments BD TOPO, cocoteraies DAG 2023 : Te Fenua / DAF, Licence Ouverte Etalab 2.0",
                    "Atlas des recifs coralliens de Polynesie francaise, Andrefouet et al., IRD 2005, via Te Fenua",
                    "Hauteurs et profondeurs : estimations de scripts/map/build_terrain.py (pas de bathymetrie mesuree)"],
        "rules": R, "seed": args.seed,
        "ring": {"length_m": round(ring.L, 1), "origin_raw_s_m": round(ring.shift, 1), "center_cm": cm(ring.center),
                 "polyline_cm": [cm(p) for p in p_poly],
                 "method": "rayon median terre + platier par secteur angulaire autour du centre du lagon, lisse ; "
                           "s croissant dans le sens horaire vu du dessus"},
        "built_sectors": sectors,
        "stats": {"groups": len(groups), "necks": len(necks), "pieces": len(pieces), "village_sites": len(vs),
                  "marae_sites": len(ms), "coves": len(coves), "playable_segments": len(runs), "rejections": reasons},
        "prototype": None,
        "retired_ids": retired,
        "fragments": frags,
    }
    if memory and memory.get("prototype") and any(f["id"] == memory["prototype"]["id"] for f in frags):
        doc["prototype"] = memory["prototype"]
    elif new_frags:
        cands = [f for f in frags if proto_ok(f)] or frags
        proto = max(cands, key=lambda f: f["scores"]["total"])
        doc["prototype"] = {"id": proto["id"], "rule": "rules.prototype",
                            "why": ("meilleure note parmi les fragments d'un seul groupe de motu (zones reliees a pied), "
                                    "sans hoa synthetique, arrivee cote lagon ; voir docs/features/fragments-tuamotu.md")}
    mem_path.write_text(dump_doc(doc), encoding="utf-8")
    log(f"memoire : {mem_path.relative_to(fl.ROOT)} ({len(frags)} fragments, {len(new_frags)} nouveaux)")
    preview_global(out_dir / "preview_fragments.png", A, ring, doc, kept_masks, sectors, mot_tree, R)
    for old in out_dir.glob(f"preview_FRG_{island_cap}_*.png"):      # previews of fragments no longer in memory
        if old.stem[len("preview_"):] not in kept_masks:
            old.unlink()
    for fr in frags:
        preview_fragment(out_dir / f"preview_{fr['id']}.png", A, F, fr, kept_masks[fr["id"]], R)
    log("apercus ecrits")
    del F
    sys.exit(cf.run(args.island, args.data_root, args.rules, A=A))


if __name__ == "__main__":
    main()
