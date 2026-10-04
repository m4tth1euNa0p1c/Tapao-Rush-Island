"""Check docs/reference/map/<ile>/fragments/fragments.json against the real coastline (ADR-004, acceptance 1).

Usage: python scripts/map/fragments/check_fragments.py raraka [--data-root <checkout with LFS data>] [--rules ...] [--file ...]
Exit code 0 when every fragment passes, 1 otherwise (each failure is printed).

For each fragment, independently of find_fragments.py (only rivage.geojson, the atlas, the heightmap and
batiments.geojson are read again):
  - identifiers FRG_<Ile>_NN, unique; fragments do not share land;
  - the kept land (polygons, even-odd) lies on real land;
  - no boundary crosses the land: a kept pixel touching excluded land is only allowed inside a declared synthetic
    cut (hoa band), and excluded land stays >= natural_min_water_m away (>= the hoa minimum next to a synthetic cut);
  - 3 village sites (on kept land, spacing respected) and 1 marae site (on kept land, far enough from the villages);
  - no kept land within motutapu_buffer_m of the buildings of Motutapu;
  - the 1.5 km box contains the land;
  - the arrival of the boat: z = 0, 200-350 m from the kept shore, far from the other land, enough water on the
    straight line to its beach (no land, reef flat nor pinnacle before the final approach), bow toward the beach.
Terrain of the fragment (recomputed in memory by build_fragment.py, T-083; skipped with --no-terrain):
  - the kept land is intact (height > 0 exactly on the polygon), no excluded land is left in the box;
  - slope <= rules.terrain.max_slope_deg on the hoa and the new shores; synthetic hoa 40-80 m wide, 1.5-3 m deep in
    the middle, with a beach on the side of the fragment;
  - floor of the margin <= edge_max_floor_m at the edge of the box;
  - a navigable ring (>= boat_min_depth_m of water) goes round the kept land, the arrival is on it;
  - galleon mooring (ADR-005): >= 4 m of water on its 40 m disk, no pinnacle, inside the box, behind the arrival
    on the bow axis.
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path

import numpy as np
from scipy import ndimage as ndi
from scipy.spatial import cKDTree

sys.path.insert(0, str(Path(__file__).resolve().parent))
import fraglib as fl  # noqa: E402
import build_fragment as bf  # noqa: E402


def check_fragment(A, fr, R, mot_tree, errors, warn):
    C, Ar = R["cuts"], R["arrival"]
    m = A.m
    fid = fr["id"]

    def fail(msg):
        errors.append(f"{fid}: {msg}")
    K = fl.polygons_to_mask(A, fr["land_polygons_cm"])
    n = int(K.sum())
    if n == 0:
        fail("aucune terre gardee")
        return K
    off = K & ~A.land
    if off.sum() > 0.01 * n:
        fail(f"{int(off.sum())} pixels gardes hors de la terre reelle (> 1 %)")
    kr, kc = np.nonzero(K)
    pad = int(400 / m)
    r0, r1 = max(kr.min() - pad, 0), min(kr.max() + pad + 1, A.H)
    c0, c1 = max(kc.min() - pad, 0), min(kc.max() + pad + 1, A.W)
    Kw = K[r0:r1, c0:c1]
    Ew = A.land[r0:r1, c0:c1] & ~Kw
    syn = [c for c in fr["cuts"] if c["type"] == "synthetique"]
    er, ec = np.nonzero(Ew)
    ex, ey = A.px_to_m(ec + c0, er + r0)
    E = np.c_[ex, ey]
    in_band = np.zeros(len(E), bool)
    near_syn = np.zeros(len(E), bool)
    for c in syn:
        a, b = (np.array(q) / 100.0 for q in c["line_cm"])
        d = fl.seg_points_dist(E, a, b)
        in_band |= d <= c["hoa_width_m"] / 2 + 25.0
        near_syn |= d <= 250.0
    # 1. kept pixels touching excluded land: only inside a declared synthetic cut
    touch = Kw & ndi.binary_dilation(Ew, structure=np.ones((3, 3), bool))
    tr, tc = np.nonzero(touch)
    if len(tr):
        tx, ty = A.px_to_m(tc + c0, tr + r0)
        T = np.c_[tx, ty]
        ok = np.zeros(len(T), bool)
        for c in syn:
            a, b = (np.array(q) / 100.0 for q in c["line_cm"])
            ok |= fl.seg_points_dist(T, a, b) <= c["hoa_width_m"] / 2 + 25.0
        if (~ok).any():
            fail(f"la frontiere traverse la terre hors coupe synthetique ({int((~ok).sum())} pixels, "
                 f"ex. {fl_cm(T[~ok][0])} cm)")
    # 2. water width between kept land and excluded land
    dK = ndi.distance_transform_edt(~Kw) * m
    d_e = dK[er, ec]
    need = np.where(near_syn, C["synthetic_hoa_range_m"][0], C["natural_min_water_m"]) - 1.5 * m
    bad = ~in_band & (d_e < need)
    if bad.any():
        k = int(np.argmin(np.where(bad, d_e, np.inf)))
        fail(f"terre exclue trop proche ({d_e[k]:.0f} m < {need[k] + 1.5 * m:.0f} m) en {fl_cm(E[k])} cm")
    # 3. sites
    V, M = R["village"], R["marae"]
    vil = fr["sites"]["villages"]
    vxy = [np.array(v["center_cm"]) / 100.0 for v in vil]
    if len(vil) < V["count"]:
        fail(f"{len(vil)} site(s) de village au lieu de {V['count']}")
    for v, p in zip(vil, vxy):
        if not K[tuple(int(q) for q in reversed(np.floor(A.m_to_px(p[0], p[1]))))]:
            fail(f"site de village {v['id']} hors de la terre gardee")
        if v.get("houses_estimate", 0) < V["min_houses"]:
            fail(f"site de village {v['id']} : moins de {V['min_houses']} minka")
    for i in range(len(vxy)):
        for j in range(i + 1, len(vxy)):
            if np.linalg.norm(vxy[i] - vxy[j]) < V["spacing_m"] - 1:
                fail(f"villages {vil[i]['id']} et {vil[j]['id']} a moins de {V['spacing_m']:.0f} m")
    mar = fr["sites"]["marae"]
    if not mar:
        fail("aucun site de marae")
    else:
        p = np.array(mar[0]["center_cm"]) / 100.0
        if not K[tuple(int(q) for q in reversed(np.floor(A.m_to_px(p[0], p[1]))))]:
            fail("site de marae hors de la terre gardee")
        if any(np.linalg.norm(p - q) < M["min_village_dist_m"] - 1 for q in vxy):
            fail(f"marae a moins de {M['min_village_dist_m']:.0f} m d'un village")
    # 4. Motutapu
    kx, ky = A.px_to_m(kc, kr)
    d, _ = mot_tree.query(np.c_[kx, ky])
    if d.min() < R["exclusion"]["motutapu_buffer_m"]:
        fail(f"terre gardee a {d.min():.0f} m du bati de Motutapu")
    # 5. box
    bb, box = fr["bbox_cm"], fr["box_cm"]
    mg = R["segment"]["box_margin_m"] * 100
    if not (box[0] <= bb[0] - mg + 1 and box[1] <= bb[1] - mg + 1 and box[2] >= bb[2] + mg - 1 and box[3] >= bb[3] + mg - 1):
        fail("la boite ne couvre pas la terre + la marge")
    if kx.min() * 100 < bb[0] - m * 100 or kx.max() * 100 > bb[2] + m * 100:
        fail("bbox_cm ne correspond pas a la terre gardee")
    # 6. arrival
    arr = fr.get("arrival")
    if not arr:
        fail("pas de point d'arrivee de la barque")
        return K
    p = np.array(arr["location_cm"][:2], float) / 100.0
    t = np.array(arr["target_cm"], float) / 100.0
    if abs(arr["location_cm"][2]) > 1e-6:
        fail("arrivee : z different de 0")
    edge = A.land[kr, kc]
    ktree = cKDTree(np.c_[kx[edge], ky[edge]])
    dk, _ = ktree.query(p)
    lo, hi = Ar["shore_dist_m"]
    if not (lo - 2 * m <= dk <= hi + 2 * m):
        fail(f"arrivee a {dk:.0f} m du rivage (attendu {lo:.0f}-{hi:.0f} m)")
    dt, _ = ktree.query(t)
    if dt > 2 * m:
        fail(f"cible de l'arrivee a {dt:.0f} m de la terre du fragment")
    other = A.land & ~K
    q0, q1 = max(r0 - pad, 0), min(r1 + pad, A.H)
    p0, p1 = max(c0 - pad, 0), min(c1 + pad, A.W)
    orr, occ = np.nonzero(other[q0:q1, p0:p1])
    if len(orr):
        ox, oy = A.px_to_m(occ + p0, orr + q0)
        do, _ = cKDTree(np.c_[ox, oy]).query(p)
        if do < Ar["other_land_min_m"] - 2 * m:
            fail(f"arrivee a {do:.0f} m d'un autre motu (min {Ar['other_land_min_m']:.0f} m)")
    v = t - p
    yaw = math.degrees(math.atan2(v[1], v[0]))
    if abs((arr["yaw_deg"] - yaw + 180) % 360 - 180) > 1.0:
        fail(f"arrivee : proue a {arr['yaw_deg']} deg au lieu de {yaw:.1f} deg vers la plage")
    fa = float(arr.get("final_approach_m", 1e9))
    if fa > max(Ar["final_approach_m"]):
        fail(f"arrivee : approche finale de {fa} m (max {max(Ar['final_approach_m'])} m)")
    ok, why, _ = fl.arrival_line_ok(A, p, t, fa, Ar["min_depth_m"])
    if not ok:
        fail(f"arrivee : {why}")
    if not fr["sites"].get("extraction"):
        warn.append(f"{fid}: ni crique ni plage d'arrivee cote lagon pour l'extraction")
    if arr.get("side") != "lagon":
        warn.append(f"{fid}: arrivee cote ocean (pas de place cote lagon)")
    return K


def fl_cm(p):
    return [round(float(p[0]) * 100), round(float(p[1]) * 100)]


def run(island, data_root=None, rules=None, A=None, path=None, terrain=True):
    R = fl.load_rules(rules)
    path = Path(path) if path else fl.ROOT / "docs" / "reference" / "map" / island / "fragments" / "fragments.json"
    if not path.exists():
        print(f"[check] {path} absent")
        return 1
    doc = json.loads(path.read_text(encoding="utf-8"))
    if A is None:
        A = fl.Atoll(island, data_root, need=("land", "cls", "height", "bldg"))
    errors, warn = [], []
    E = R["exclusion"]
    mot_pt = np.array(E["motutapu_point_cm"]) / 100.0
    near = int(np.argmin([np.linalg.norm(b["xy"] - mot_pt) for b in A.buildings]))
    sec = next(g for g in fl.building_sectors(A.buildings, E["sector_link_m"]) if near in g)
    mot_tree = cKDTree(np.vstack([A.buildings[i]["poly"] for i in sec]))
    pat = re.compile(rf"^FRG_{island.capitalize()}_\d{{2,}}$")
    ids = [f["id"] for f in doc["fragments"]]
    if len(set(ids)) != len(ids):
        errors.append("identifiants en double")
    union = np.zeros(A.land.shape, np.uint8)
    for fr in doc["fragments"]:
        if not pat.match(fr["id"]):
            errors.append(f"{fr['id']}: identifiant invalide")
        K = check_fragment(A, fr, R, mot_tree, errors, warn)
        union += K.astype(np.uint8)
        if terrain:
            T = bf.FragmentTerrain(A, fr, R)
            terr_errors, met = bf.check_terrain(A, fr, R, T)
            errors.extend(f"{fr['id']}: terrain : {e}" for e in terr_errors)
            ga = fr.get("galleon_anchor") or {}
            print(f"[check] {fr['id']} terrain : pente max nouvelles rives {met['max_slope_new_shores_deg']} deg, bord "
                  f"{met['edge_max_floor_m']} m, galion {met.get('galleon_depth_m')} m de fond a "
                  f"{ga.get('beach_dist_m')} m de la plage")
    if (union > 1).any():
        errors.append(f"des fragments partagent de la terre ({int((union > 1).sum())} pixels)")
    if doc.get("prototype") and doc["prototype"]["id"] not in ids:
        errors.append("le prototype designe n'existe pas")
    for w in warn:
        print(f"[check] avertissement {w}")
    for e in errors:
        print(f"[check] ECHEC {e}")
    print(f"[check] {len(doc['fragments'])} fragments, {len(errors)} echec(s) -> {'OK' if not errors else 'ECHEC'}")
    return 0 if not errors and doc["fragments"] else 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("island")
    ap.add_argument("--data-root", default=None)
    ap.add_argument("--rules", default=None)
    ap.add_argument("--file", default=None, help="fragments.json to check (default: the one of the island)")
    ap.add_argument("--no-terrain", action="store_true", help="skip the terrain checks of build_fragment.py")
    a = ap.parse_args()
    sys.exit(run(a.island, a.data_root, a.rules, path=a.file, terrain=not a.no_terrain))


if __name__ == "__main__":
    main()
