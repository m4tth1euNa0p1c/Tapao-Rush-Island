"""Horizon of a fragment (T-084, ADR-004 §4): low-poly silhouette of the rest of the atoll ring seen from it.

Usage: python scripts/map/fragments/build_horizon.py raraka FRG_Raraka_01 [FRG_...] [--all]
                                                     [--data-root <checkout with LFS data>] [--rules ...] [--no-preview]

For each fragment of docs/reference/map/<ile>/fragments/fragments.json:
  - every motu outside the playable box of the fragment (box_cm, i.e. its land + 1.5 km of sea) and outside a small
    extra margin, from the real coastline (rivage.geojson), simplified with a tolerance that grows with the distance;
  - a low sand band (plateau + skirt that sinks under the water) and a canopy ribbon of 15-25 m on the vegetation
    (mask R of T_<Ile>_Masks.png + cocoteraies.geojson), taller on the coconut groves, with crowns along its edge;
  - near the cut (the box edge), the canopy tapers down: a cut motu ends as a low sand spit, never as a wall;
  - far motu (> lod.liseret_m) are reduced to a coarse liseret; earth curvature is baked in (UE is flat);
  - UV0: U = vertical layer (sand bottom 0 -> canopy top 1), V = pre-computed haze (distance), for ONE material
    (M_Horizon_Atoll, recipe in docs/features/fragments-tuamotu.md, section Horizon).
Every parameter comes from horizon_rules.json.

Outputs:
  docs/reference/map/raw/<ile>_fragments/<FRG>/SM_<FRG>_Horizon.obj   (same frame and axes as build_terrain.py:
      write_obj is reused; Raraka frame, origin = centre of the mosaic, not recentred on the fragment)
  docs/reference/map/<ile>/fragments/<FRG>/horizon_top.png, horizon_pano.png, horizon.json (budget, checks)
Exit code 1 when a check fails: a triangle touching the playable box, a vertex closer than clip.min_land_dist_m to
the land of the fragment, or more than budget.max_tris triangles.
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

bt = fl.bt
HORIZON_RULES = Path(__file__).resolve().parent / "horizon_rules.json"
MATERIAL = "M_Horizon_Atoll"


# ---------------------------------------------------------------------------------------------- small helpers

def smoothstep(e0, e1, x):
    t = np.clip((np.asarray(x, float) - e0) / max(e1 - e0, 1e-9), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def hash01(x, y, salt=0):
    """Deterministic pseudo-random in [0, 1) from world coordinates (m), stable between runs and fragments."""
    xi = np.floor(np.asarray(x, float) * 2.0).astype(np.int64)
    yi = np.floor(np.asarray(y, float) * 2.0).astype(np.int64)
    h = (xi * 73856093) ^ (yi * 19349663) ^ (salt * 83492791)
    h = (h ^ (h >> 13)) * 1274126177
    h = h ^ (h >> 16)
    return (h & 0xFFFFFF).astype(np.float64) / float(0x1000000)


def value_noise(x, y, wavelength, salt=0):
    """Smooth 2D value noise in [-1, 1] (bilinear between lattice hashes)."""
    gx, gy = np.asarray(x, float) / wavelength, np.asarray(y, float) / wavelength
    x0, y0 = np.floor(gx), np.floor(gy)
    fx, fy = smoothstep(0, 1, gx - x0), smoothstep(0, 1, gy - y0)

    def h(i, j):
        return hash01((x0 + i) * 0.5, (y0 + j) * 0.5, salt) * 2 - 1
    a = h(0, 0) * (1 - fx) + h(1, 0) * fx
    b = h(0, 1) * (1 - fx) + h(1, 1) * fx
    return a * (1 - fy) + b * fy


def signed_area(P):
    x, y = P[:, 0], P[:, 1]
    return 0.5 * (np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def vertex_normals(P):
    """Outward unit normals of a CCW ring (x east, y north): average of the two adjacent edge normals."""
    e_next = np.roll(P, -1, axis=0) - P
    e_prev = P - np.roll(P, 1, axis=0)
    n1 = np.c_[e_next[:, 1], -e_next[:, 0]]
    n0 = np.c_[e_prev[:, 1], -e_prev[:, 0]]
    n1 /= np.maximum(np.linalg.norm(n1, axis=1, keepdims=True), 1e-9)
    n0 /= np.maximum(np.linalg.norm(n0, axis=1, keepdims=True), 1e-9)
    n = n0 + n1
    L = np.linalg.norm(n, axis=1, keepdims=True)
    n = np.where(L > 1e-6, n / np.maximum(L, 1e-9), n1)
    # miter length, capped: keeps the offset distance on straight runs without spikes at sharp corners
    cosh = np.clip((n * n1).sum(1), 0.5, 1.0)
    return n / cosh[:, None]


# ---------------------------------------------------------------------------------------------- polygons

def dp_adaptive(pts, tol_fn):
    """Douglas-Peucker on an open polyline with a per-segment tolerance tol_fn(midpoint) (keeps both ends)."""
    keep = np.zeros(len(pts), bool)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        i, j = stack.pop()
        if j <= i + 1:
            continue
        a, b = pts[i], pts[j]
        ab = b - a
        L = math.hypot(*ab)
        seg = pts[i + 1:j]
        if L < 1e-9:
            d = np.hypot(*(seg - a).T)
        else:
            d = np.abs(ab[0] * (seg[:, 1] - a[1]) - ab[1] * (seg[:, 0] - a[0])) / L
        k = int(np.argmax(d))
        if d[k] > tol_fn((a + b) * 0.5):
            keep[i + 1 + k] = True
            stack += [(i, i + 1 + k), (i + 1 + k, j)]
    return pts[keep]


def simplify_closed(ring, tol_fn):
    if len(ring) < 8:
        return ring
    far = int(np.argmax(np.hypot(*(ring - ring[0]).T)))
    a = dp_adaptive(ring[:far + 1], tol_fn)
    b = dp_adaptive(np.vstack([ring[far:], ring[:1]]), tol_fn)
    out = np.vstack([a[:-1], b[:-1]])
    return out if len(out) >= 3 else ring


def subdivide_closed(ring, spacing_fn):
    """Insert points so that no edge is longer than spacing_fn(midpoint)."""
    out = []
    nxt = np.roll(ring, -1, axis=0)
    for a, b in zip(ring, nxt):
        L = math.hypot(*(b - a))
        n = max(1, int(math.ceil(L / max(spacing_fn((a + b) * 0.5), 1.0))))
        t = np.arange(n)[:, None] / n
        out.append(a + t * (b - a))
    return np.vstack(out)


def earcut(P):
    """Ear clipping of a simple CCW polygon (n, 2) -> (n-2, 3) vertex indices. Tolerant: when no ear is found
    (self-touching ring after simplification), the most convex vertex is clipped anyway."""
    n = len(P)
    if n < 3:
        return np.zeros((0, 3), int)
    prev = np.r_[n - 1, np.arange(n - 1)]
    nxt = np.r_[np.arange(1, n), 0]
    alive = np.ones(n, bool)
    tris = []
    remaining = n

    def cross(i):
        a, b, c = P[prev[i]], P[i], P[nxt[i]]
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])

    def is_ear(i):
        if cross(i) <= 1e-9:
            return False
        a, b, c = P[prev[i]], P[i], P[nxt[i]]
        idx = np.nonzero(alive)[0]
        idx = idx[(idx != prev[i]) & (idx != i) & (idx != nxt[i])]
        if not len(idx):
            return True
        Q = P[idx]
        # only reflex or any vertex inside the triangle blocks the ear
        d1 = (b[0] - a[0]) * (Q[:, 1] - a[1]) - (b[1] - a[1]) * (Q[:, 0] - a[0])
        d2 = (c[0] - b[0]) * (Q[:, 1] - b[1]) - (c[1] - b[1]) * (Q[:, 0] - b[0])
        d3 = (a[0] - c[0]) * (Q[:, 1] - c[1]) - (a[1] - c[1]) * (Q[:, 0] - c[0])
        inside = (d1 >= -1e-9) & (d2 >= -1e-9) & (d3 >= -1e-9)
        return not inside.any()

    i = 0
    stall = 0
    while remaining > 3:
        if is_ear(i):
            tris.append((prev[i], i, nxt[i]))
            alive[i] = False
            nxt[prev[i]], prev[nxt[i]] = nxt[i], prev[i]
            remaining -= 1
            stall = 0
            i = nxt[i]
            continue
        i = nxt[i]
        stall += 1
        if stall > remaining:
            idx = np.nonzero(alive)[0]
            j = idx[int(np.argmax([cross(k) for k in idx]))]
            tris.append((prev[j], j, nxt[j]))
            alive[j] = False
            nxt[prev[j]], prev[nxt[j]] = nxt[j], prev[j]
            remaining -= 1
            stall = 0
            i = nxt[j]
    idx = np.nonzero(alive)[0]
    j = idx[0]
    tris.append((prev[j], j, nxt[j]))
    return np.asarray(tris, int)


def mask_rings_m(A, mask):
    """Outer rings of a boolean grid mask as CCW polygons in (east, north) metres, one per connected component
    (holes are filled before), with the component area in ha."""
    lab, n = ndi.label(mask)
    out = []
    for k, sl in enumerate(ndi.find_objects(lab), start=1):
        if sl is None:
            continue
        sub = ndi.binary_fill_holes(lab[sl] == k)
        area_ha = float(sub.sum()) * A.m * A.m / 1e4
        rings = fl.mask_to_rings(sub)
        if not rings:
            continue
        ring = max(rings, key=lambda r: abs(fl.ring_area(r)))
        east = (ring[:, 0] + sl[1].start) * A.m - A.cx
        north = -((ring[:, 1] + sl[0].start) * A.m - A.cz)
        P = np.c_[east, north]
        if signed_area(P) < 0:
            P = P[::-1]
        out.append((P, area_ha))
    return out


# ---------------------------------------------------------------------------------------------- mesh builder

class Mesh:
    def __init__(self):
        self.pos, self.uv, self.tri = [], [], []
        self.n = 0
        self.parts = {"sand": 0, "canopy": 0}

    def add(self, pos, uv, tri, part):
        self.pos.append(pos)
        self.uv.append(uv)
        self.tri.append(tri + self.n)
        self.n += len(pos)
        self.parts[part] += len(tri)

    def arrays(self):
        if not self.pos:
            return np.zeros((0, 3)), np.zeros((0, 2)), np.zeros((0, 3), int)
        return np.vstack(self.pos), np.vstack(self.uv), np.vstack(self.tri)


def ring_band(n_ring):
    """Triangles of a wall between ring 0 (bottom, indices 0..n-1) and ring 1 (top, n..2n-1), both CCW seen from
    above: faces point outward (counter-clockwise seen from outside)."""
    i = np.arange(n_ring)
    j = (i + 1) % n_ring
    b0, b1, t0, t1 = i, j, i + n_ring, j + n_ring
    return np.r_[np.c_[b0, b1, t1], np.c_[b0, t1, t0]]


class HorizonBuilder:
    def __init__(self, A, rules, fr):
        self.A, self.R, self.fr = A, rules, fr
        self.center = np.array([fr["center_cm"][0], -fr["center_cm"][1]]) / 100.0     # (east, north) m
        L = rules["lod"]
        self.lod_d = np.asarray(L["dist_m"], float)

    def dist(self, P):
        return np.linalg.norm(np.atleast_2d(P) - self.center, axis=1)

    def lod(self, key, P):
        return np.interp(self.dist(P), self.lod_d, np.asarray(self.R["lod"][key], float))

    def drop(self, P):
        C = self.R["curvature"]
        if not C["enabled"]:
            return np.zeros(len(np.atleast_2d(P)))
        d = self.dist(P)
        return d * d / (2.0 * C["refraction_k"] * C["earth_radius_m"])

    def haze_v(self, P):
        U = self.R["uv"]
        return np.clip((self.dist(P) - U["haze_near_m"]) / (U["haze_far_m"] - U["haze_near_m"]), 0.0, 1.0)

    # ------------------------------------------------------------------ masks
    def build_masks(self):
        A, R = self.A, self.R
        m = A.m
        b = np.asarray(self.fr["box_cm"], float) / 100.0                # x east, y south (m)
        mg = R["clip"]["margin_m"]
        c0, r0 = A.m_to_px(b[0] - mg, b[1] - mg)
        c1, r1 = A.m_to_px(b[2] + mg, b[3] + mg)
        clip = np.zeros((A.H, A.W), bool)
        clip[max(int(math.floor(r0)), 0):min(int(math.ceil(r1)), A.H),
             max(int(math.floor(c0)), 0):min(int(math.ceil(c1)), A.W)] = True
        self.clip = clip
        sand_s = R["sand"]["smooth_m"] / m
        land = ndi.gaussian_filter(A.land.astype(np.float32), sand_s) > 0.5 if sand_s > 0 else A.land.copy()
        land &= ~clip
        self.land = land
        self.d_clip = ndi.distance_transform_edt(~clip) * m                # m to the clip rectangle
        C = R["canopy"]
        veg = (A.veg.astype(np.float32) / 255.0 > C["veg_threshold"]) | A.coco
        d_shore = ndi.distance_transform_edt(land) * m
        canopy = veg & (d_shore > C["beach_m"]) & (self.d_clip > R["clip"]["taper_min_m"])
        cs = C["smooth_m"] / m
        canopy = (ndi.gaussian_filter(canopy.astype(np.float32), cs) > 0.5) & land
        self.canopy = canopy
        self.canopy_edt = ndi.distance_transform_edt(canopy) * m
        self.coco_blend = ndi.gaussian_filter(A.coco.astype(np.float32), C["coco_blur_m"] / m)
        self.coco_blend = np.clip(self.coco_blend * 2.0, 0.0, 1.0)

    def sample(self, img, P):
        return self.A.sample(img, P[:, 0], -P[:, 1])

    # ------------------------------------------------------------------ parts
    def rings(self, mask, min_area_key, extra_min_ha=0.0):
        out = []
        for P, area in mask_rings_m(self.A, mask):
            dmin = float(self.dist(P).min())
            min_ha = max(float(np.interp(dmin, self.lod_d, self.R["lod"][min_area_key])), extra_min_ha)
            if area < min_ha:
                continue
            Q = simplify_closed(P, lambda p: float(self.lod("tol_m", p)[0]))
            if len(Q) < 3 or abs(signed_area(Q)) < 1.0:
                continue
            if signed_area(Q) < 0:
                Q = Q[::-1]
            out.append((Q, area))
        return out

    def sand_parts(self, mesh):
        S, U = self.R["sand"], self.R["uv"]
        count = 0
        for P, _ in self.rings(self.land, "min_area_ha"):
            n = len(P)
            skirt = P + vertex_normals(P) * S["skirt_out_m"]
            z_bot = S["bottom_m"] - self.drop(skirt)
            z_top = S["top_m"] - self.drop(P)
            pos = np.vstack([np.c_[skirt, z_bot], np.c_[P, z_top]])
            v = np.r_[self.haze_v(skirt), self.haze_v(P)]
            uv = np.c_[np.r_[np.zeros(n), np.full(n, U["sand_top_u"])], v]
            tri = np.vstack([ring_band(n), earcut(P) + n])
            mesh.add(pos, uv, tri, "sand")
            count += 1
        return count

    def canopy_height(self, P):
        C, L = self.R["canopy"], self.R["lod"]
        d = self.dist(P)
        base = C["thicket_m"] + (C["coco_m"] - C["thicket_m"]) * self.sample(self.coco_blend, P)
        h = base + C["noise_amp_m"] * value_noise(P[:, 0], P[:, 1], C["noise_wavelength_m"], salt=1)
        amp = np.interp(d, self.lod_d, np.asarray(L["crown_amp_m"], float))
        r = hash01(P[:, 0], P[:, 1], salt=2)
        crown = np.where(np.arange(len(P)) % 2 == 0, 1.0, -1.0) * (0.5 + 0.5 * r)
        h += amp * crown
        emerg = hash01(P[:, 0], P[:, 1], salt=3) < C["emergent_prob"] * (d < L["liseret_m"])
        h += np.where(emerg, C["emergent_m"], 0.0)
        h = np.where(d > L["liseret_m"], h * C["liseret_scale"], h)
        taper = smoothstep(self.R["clip"]["taper_min_m"], self.R["clip"]["taper_m"], self.sample(self.d_clip, P))
        base_z = C["base_m"]
        return base_z + (h - base_z) * taper

    def canopy_parts(self, mesh):
        C, U = self.R["canopy"], self.R["uv"]
        count = 0
        for P, _ in self.rings(self.canopy, "min_area_ha", C["min_area_ha"]):
            Q = subdivide_closed(P, lambda p: float(self.lod("crown_spacing_m", p)[0]))
            n = len(Q)
            nrm = vertex_normals(Q)
            # inset of the top ring, limited by the local thickness of the canopy (no fold on narrow strips)
            thick = np.zeros(n)
            for k in (3.0, 6.0, 10.0, 15.0, 22.0):
                thick = np.maximum(thick, self.sample(self.canopy_edt, Q - nrm * k))
            inset = np.minimum(C["top_inset_m"], 0.45 * thick)
            top = Q - nrm * inset[:, None]
            z_bot = C["base_m"] - self.drop(Q)
            z_top = self.canopy_height(Q) - self.drop(top)
            pos = np.vstack([np.c_[Q, z_bot], np.c_[top, z_top]])
            v = np.r_[self.haze_v(Q), self.haze_v(top)]
            uv = np.c_[np.r_[np.full(n, U["canopy_base_u"]), np.ones(n)], v]
            tri = np.vstack([ring_band(n), earcut(Q) + n])
            mesh.add(pos, uv, tri, "canopy")
            count += 1
        return count


# ---------------------------------------------------------------------------------------------- checks

def triangles_hit_box(pos, tri, box):
    """Separating axis test of 2D triangles (plan view) against an axis-aligned rectangle (xmin, ymin, xmax, ymax)
    in (east, north) metres. Returns a boolean per triangle (True = overlaps)."""
    T = pos[tri][:, :, :2]
    xmin, ymin, xmax, ymax = box
    hit = ~((T[:, :, 0].max(1) < xmin) | (T[:, :, 0].min(1) > xmax) |
            (T[:, :, 1].max(1) < ymin) | (T[:, :, 1].min(1) > ymax))
    corners = np.array([[xmin, ymin], [xmax, ymin], [xmax, ymax], [xmin, ymax]])
    for e in range(3):
        a, b = T[:, e], T[:, (e + 1) % 3]
        nrm = np.c_[-(b[:, 1] - a[:, 1]), b[:, 0] - a[:, 0]]
        pt = (T * nrm[:, None]).sum(2)                         # triangle projected
        pc = corners @ nrm.T                                   # (4, n) box projected
        sep = (pt.max(1) < pc.min(0)) | (pt.min(1) > pc.max(0))
        hit &= ~sep
    return hit


def run_checks(A, fr, rules, pos, tri):
    box = np.asarray(fr["box_cm"], float) / 100.0
    box_en = (box[0], -box[3], box[2], -box[1])                # (east, north) rectangle
    hits = int(triangles_hit_box(pos, tri, box_en).sum()) if len(tri) else 0
    K = fl.polygons_to_mask(A, fr["land_polygons_cm"])
    edge = K & ~ndi.binary_erosion(K)
    er, ec = np.nonzero(edge)
    ex, ey = A.px_to_m(ec, er)
    tree = cKDTree(np.c_[ex, -ey])
    # vertices and triangle centroids (a triangle is convex: its closest point to a far set lies on its border,
    # the vertices bound it from below within the edge length, reported separately)
    dmin_v = float(tree.query(pos[:, :2])[0].min()) if len(pos) else float("inf")
    cen = pos[tri].mean(1)[:, :2] if len(tri) else np.zeros((0, 2))
    dmin_c = float(tree.query(cen)[0].min()) if len(cen) else float("inf")
    edge_len = float(np.linalg.norm(pos[tri][:, [1, 2, 0], :2] - pos[tri][:, :, :2], axis=2).max()) if len(tri) else 0
    tris = len(tri)
    if tris:
        P = pos[tri]
        nz = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
        nz = nz[:, 2] / np.maximum(np.linalg.norm(nz, axis=1), 1e-9)
        down = int((nz < -0.05).sum())
    else:
        down = 0
    B = rules["budget"]
    lim = rules["clip"]["min_land_dist_m"]
    ok = hits == 0 and dmin_v >= lim and dmin_c >= lim and tris <= B["max_tris"]
    return {"ok": bool(ok), "triangles_in_box": hits, "min_vertex_dist_to_fragment_land_m": round(dmin_v, 1),
            "min_centroid_dist_to_fragment_land_m": round(dmin_c, 1), "max_edge_m": round(edge_len, 1),
            "required_min_dist_m": lim, "triangles": tris, "budget_warn": B["warn_tris"], "budget_max": B["max_tris"],
            "within_budget": tris <= B["warn_tris"],
            "_facing_down": "triangles tournes vers le bas (dessus de canopee replie par le retrait du sommet ou la "
                            "decimation) : invisibles depuis le sol, simple trou vu d'avion ; informatif",
            "facing_down": down}


# ---------------------------------------------------------------------------------------------- previews

SKY_TOP, SKY_LOW = np.array([70, 125, 200]), np.array([190, 215, 235])
SEA = np.array([25, 90, 140])
HAZE = np.array([185, 210, 230])
SAND = np.array([232, 220, 185])
CANOPY_LOW, CANOPY_TOP = np.array([34, 58, 34]), np.array([62, 98, 52])
SUN = np.array([0.35, -0.55, 0.76])     # (east, north, up), sun high in the north-west (16 S)
SUN = SUN / np.linalg.norm(SUN)


def tri_colors(pos, uv, tri, haze_dist):
    """Flat colour per triangle: U gradient (sand -> canopy), Lambert with a sky ambient, haze by distance."""
    u = uv[tri][:, :, 0].mean(1)
    sand_t = np.clip(u / 0.2, 0, 1)[:, None]
    can_t = np.clip((u - 0.3) / 0.7, 0, 1)[:, None]
    base = np.where(u[:, None] < 0.25, SAND * (0.75 + 0.25 * sand_t), CANOPY_LOW + (CANOPY_TOP - CANOPY_LOW) * can_t)
    P = pos[tri]
    nrm = np.cross(P[:, 1] - P[:, 0], P[:, 2] - P[:, 0])
    nrm /= np.maximum(np.linalg.norm(nrm, axis=1, keepdims=True), 1e-9)
    lit = 0.55 + 0.45 * np.clip(nrm @ SUN, 0, 1)
    col = base * lit[:, None]
    f = (1 - np.exp(-np.asarray(haze_dist) / 9000.0))[:, None]
    col = col * (1 - f) + HAZE * f
    return np.clip(col, 0, 255).astype(np.uint8), nrm


def preview_top(path, A, fr, rules, pos, uv, tri, center, title):
    W = rules["preview"]["top_px"]
    s = W / (A.W * A.m)
    H = int(A.H * A.m * s)
    img = Image.new("RGB", (W, H), tuple(int(c) for c in SEA))
    d = ImageDraw.Draw(img)
    small = Image.fromarray((A.land * 255).astype(np.uint8)).resize((W, H), Image.BILINEAR)
    grey = Image.new("RGB", (W, H), (120, 120, 120))
    img.paste(grey, mask=small)

    def px(e, n):
        return (np.asarray(e) + A.cx) * s, (-np.asarray(n) + A.cz) * s
    if len(tri):
        cols, _ = tri_colors(pos, uv, tri, np.zeros(len(tri)))
        order = np.argsort(pos[tri][:, :, 2].mean(1))
        X, Y = px(pos[:, 0], pos[:, 1])
        for k in order:
            t = tri[k]
            d.polygon([(X[t[0]], Y[t[0]]), (X[t[1]], Y[t[1]]), (X[t[2]], Y[t[2]])], fill=tuple(int(c) for c in cols[k]))
    K = fl.polygons_to_mask(A, fr["land_polygons_cm"])
    km = Image.fromarray((K * 255).astype(np.uint8)).resize((W, H), Image.BILINEAR)
    img.paste(Image.new("RGB", (W, H), (240, 140, 40)), mask=km)
    b = np.asarray(fr["box_cm"], float) / 100.0
    x0, y0 = px(b[0], -b[1])
    x1, y1 = px(b[2], -b[3])
    d.rectangle([x0, y0, x1, y1], outline=(230, 40, 40), width=2)
    cx, cy = px(*center)
    for r, colr in ((rules["lod"]["dist_m"][1], (255, 255, 255)), (rules["lod"]["liseret_m"], (255, 230, 120))):
        rr = r * s
        d.ellipse([cx - rr, cy - rr, cx + rr, cy + rr], outline=colr, width=1)
    f = fl.font(18)
    d.text((10, 10), title, fill=(255, 255, 255), font=f)
    d.text((10, 34), "orange : terre du fragment ; rouge : boite jouable ; gris : terre reelle ; "
                     f"cercles : {rules['lod']['dist_m'][1] / 1000:.0f} km et liseret {rules['lod']['liseret_m'] / 1000:.0f} km",
           fill=(255, 255, 255), font=fl.font(14))
    img.save(path)


def render_pano(pos, uv, tri, center, eye_z, ppd, el_range, exaggeration=1.0):
    """Cylindrical 360 deg panorama (azimuth 0 = north, clockwise), painter's algorithm."""
    W = int(round(360 * ppd))
    el0, el1 = el_range
    H = int(round((el1 - el0) * ppd * exaggeration))
    sky = np.zeros((H, W, 3))
    horizon_row = el1 * ppd * exaggeration
    rows = np.arange(H)[:, None]
    t = np.clip(rows / max(horizon_row, 1), 0, 1)
    sky[:] = (SKY_TOP * (1 - t) + SKY_LOW * t)[:, None, :]
    sky[int(horizon_row):] = SEA
    img = Image.fromarray(sky.astype(np.uint8))
    if not len(tri):
        return img
    d = ImageDraw.Draw(img)
    rel = pos[:, :2] - center
    dist = np.maximum(np.linalg.norm(rel, axis=1), 1.0)
    az = np.degrees(np.arctan2(rel[:, 0], rel[:, 1])) % 360.0
    el = np.degrees(np.arctan2(pos[:, 2] - eye_z, dist))
    X = az * ppd
    Y = (el1 - el) * ppd * exaggeration
    tdist = dist[tri].mean(1)
    cols, nrm = tri_colors(pos, uv, tri, tdist)
    # back faces (seen from the eye) are skipped like the one-sided material would
    view = pos[tri].mean(1) - np.r_[center, eye_z]
    front = (nrm * view).sum(1) < 0
    for k in np.argsort(-tdist):
        if not front[k]:
            continue
        t3 = tri[k]
        xs = X[t3].copy()
        if xs.max() - xs.min() > W / 2:                     # crosses azimuth 0
            xs = np.where(xs < W / 2, xs + W, xs)
        poly = list(zip(xs, Y[t3]))
        c = tuple(int(v) for v in cols[k])
        d.polygon(poly, fill=c)
        if xs.max() > W:
            d.polygon([(x - W, y) for x, y in poly], fill=c)
    return img


def preview_pano(path, rules, pos, uv, tri, center, title):
    P = rules["preview"]
    ppd, el = P["pano_px_per_deg"], P["pano_rows_deg"]
    eye = P["eye_m"]
    full = render_pano(pos, uv, tri, center, eye, ppd, el)
    W, H = full.size
    q = W // 4
    exag = render_pano(pos, uv, tri, center, eye, ppd / 4.0, el, P["pano_exaggeration"])
    pad = 26
    out = Image.new("RGB", (q, pad + 4 * (H + pad) + exag.size[1] * 0 + pad + exag.size[1] + pad), (20, 20, 24))
    d = ImageDraw.Draw(out)
    f = fl.font(15)
    d.text((8, 5), title, fill=(255, 255, 255), font=f)
    y = pad
    for i, name in enumerate(("N -> E (0-90 deg)", "E -> S (90-180 deg)", "S -> O (180-270 deg)", "O -> N (270-360 deg)")):
        out.paste(full.crop((i * q, 0, (i + 1) * q, H)), (0, y))
        d.text((8, y + 2), f"echelle vraie, oeil {eye} m : {name}", fill=(255, 255, 255), font=fl.font(12))
        y += H + pad
    d.text((8, y - pad + 6), f"360 deg, hauteur exageree x{P['pano_exaggeration']:.0f} (lecture seulement)",
           fill=(255, 255, 255), font=fl.font(13))
    out.paste(exag.resize((q, exag.size[1])), (0, y))
    out.save(path)


# ---------------------------------------------------------------------------------------------- main

def build_one(A, fr, rules, out_raw, out_doc, preview=True):
    t0 = time.time()
    hb = HorizonBuilder(A, rules, fr)
    hb.build_masks()
    mesh = Mesh()
    n_sand = hb.sand_parts(mesh)
    n_can = hb.canopy_parts(mesh)
    pos, uv, tri = mesh.arrays()
    checks = run_checks(A, fr, rules, pos, tri)
    fid = fr["id"]
    name = f"SM_{fid}_Horizon"
    out_raw.mkdir(parents=True, exist_ok=True)
    out_doc.mkdir(parents=True, exist_ok=True)
    obj = out_raw / f"{name}.obj"
    pos_cm = np.c_[pos[:, 0] * 100.0, pos[:, 1] * 100.0, pos[:, 2] * 100.0]
    bt.write_obj(obj, pos_cm, uv, tri.ravel(), name, MATERIAL)
    dist = hb.dist(pos[:, :2]) if len(pos) else np.zeros(0)
    tdist = hb.dist(pos[tri].mean(1)[:, :2]) if len(tri) else np.zeros(0)
    L = rules["lod"]
    bands = [0.0] + list(L["dist_m"][1:3]) + [1e9]
    report = {
        "fragment": fid, "generator": "scripts/map/fragments/build_horizon.py", "rules": "horizon_rules.json",
        "obj": str(obj.relative_to(fl.ROOT)).replace("\\", "/"), "mesh": name, "material": MATERIAL,
        "frame": "UE cm de terrain/georef.json (repere de Raraka, pas recentre sur le fragment) ; OBJ ecrit par "
                 "build_terrain.write_obj (est, nord, haut) -> UE (x, -y, z)",
        "fragment_center_cm": fr["center_cm"],
        "vertices": int(len(pos)), "triangles": int(len(tri)),
        "triangles_by_part": mesh.parts, "sand_islets": n_sand, "canopy_patches": n_can,
        "triangles_by_distance_km": {f"{bands[i] / 1000:.0f}-{bands[i + 1] / 1000:.0f}" if bands[i + 1] < 1e8
                                     else f">{bands[i] / 1000:.0f}": int(((tdist >= bands[i]) & (tdist < bands[i + 1])).sum())
                                     for i in range(len(bands) - 1)},
        "distance_m": {"min": round(float(dist.min()), 1) if len(dist) else None,
                       "max": round(float(dist.max()), 1) if len(dist) else None},
        "height_m": {"min": round(float(pos[:, 2].min()), 2) if len(pos) else None,
                     "max": round(float(pos[:, 2].max()), 2) if len(pos) else None},
        "bounds_cm": ([[round(float(v), 1) for v in pos_cm.min(0)], [round(float(v), 1) for v in pos_cm.max(0)]]
                      if len(pos) else None),
        "checks": checks,
        "sources": ["Trait de cote et cocoteraies : Te Fenua / DAF, Licence Ouverte Etalab 2.0",
                    "Masques de vegetation : build_terrain.py (atlas IRD 2005 + trait de cote)"],
        "seconds": round(time.time() - t0, 1),
    }
    if preview:
        title = f"{fid} : horizon {len(tri)} triangles (sable {mesh.parts['sand']}, canopee {mesh.parts['canopy']})"
        preview_top(out_doc / "horizon_top.png", A, fr, rules, pos, uv, tri, hb.center, title)
        preview_pano(out_doc / "horizon_pano.png", rules, pos, uv, tri, hb.center,
                     f"{fid} : vue rasante depuis le centre du fragment (le fragment lui-meme n'est pas dessine)")
        report["previews"] = [str((out_doc / n).relative_to(fl.ROOT)).replace("\\", "/")
                              for n in ("horizon_top.png", "horizon_pano.png")]
    report["seconds"] = round(time.time() - t0, 1)
    (out_doc / "horizon.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    return report


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("island")
    ap.add_argument("fragments", nargs="*", help="FRG_<Ile>_NN (default with --all: every fragment)")
    ap.add_argument("--all", action="store_true")
    ap.add_argument("--data-root", default=None, help="checkout holding the LFS data (default: auto)")
    ap.add_argument("--rules", default=None, help="horizon rules (default: horizon_rules.json)")
    ap.add_argument("--no-preview", action="store_true")
    args = ap.parse_args()
    rules = json.loads(Path(args.rules or HORIZON_RULES).read_text(encoding="utf-8"))
    A = fl.Atoll(args.island, args.data_root, need=("masks", "coco"))
    rel = A.rel / "fragments" / "fragments.json"
    cat = json.loads((fl.ROOT / rel).read_text(encoding="utf-8"))
    by_id = {f["id"]: f for f in cat["fragments"]}
    ids = list(by_id) if args.all else args.fragments
    if not ids:
        ap.error("give fragment ids or --all")
    unknown = [i for i in ids if i not in by_id]
    if unknown:
        ap.error(f"unknown fragments: {', '.join(unknown)}")
    failed = 0
    summary = []
    for fid in ids:
        out_raw = fl.ROOT / "docs" / "reference" / "map" / "raw" / f"{args.island}_fragments" / fid
        out_doc = fl.ROOT / A.rel / "fragments" / fid
        rep = build_one(A, by_id[fid], rules, out_raw, out_doc, preview=not args.no_preview)
        c = rep["checks"]
        flag = "OK" if c["ok"] else "ECHEC"
        warn = "" if c["within_budget"] else f" (au-dessus de l'alerte {c['budget_warn']})"
        print(f"{fid}: {flag} {rep['triangles']} triangles{warn}, sable {rep['triangles_by_part']['sand']}, "
              f"canopee {rep['triangles_by_part']['canopy']}, dist min sommet {c['min_vertex_dist_to_fragment_land_m']} m, "
              f"triangles dans la boite {c['triangles_in_box']}, {rep['seconds']} s")
        summary.append((fid, rep["triangles"], c["ok"]))
        failed += not c["ok"]
    if len(ids) > 1:
        tris = [t for _, t, _ in summary]
        print(f"{len(ids)} fragments, {failed} en echec ; triangles min {min(tris)}, max {max(tris)}, "
              f"moyenne {int(np.mean(tris))}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
