"""Shared helpers of the fragment pipeline (ADR-004): data loading, UE frame, ring axis, raster <-> polygons.

Used by find_fragments.py and check_fragments.py. Reuses the functions of scripts/map/build_terrain.py
(coastline rasterisation, reef atlas classes, Web Mercator) instead of duplicating them.

Data files: a git worktree without LFS only holds pointer files for the PNG and geojson data. Every input is
resolved in order: --data-root, this checkout, then the main checkout of the repository (git common dir), and the
first real file (not an LFS pointer) wins. The georef.json is always read next to the heightmap it describes.
"""
import importlib.util
import json
import math
import subprocess
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi
from scipy.spatial import cKDTree

Image.MAX_IMAGE_PIXELS = None
ROOT = Path(__file__).resolve().parents[3]
RULES_PATH = Path(__file__).resolve().parent / "rules.json"


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


bt = _load("build_terrain", ROOT / "scripts" / "map" / "build_terrain.py")


def load_rules(path=None):
    return json.loads(Path(path or RULES_PATH).read_text(encoding="utf-8"))


# ---------------------------------------------------------------------------------------------- data roots

def main_checkout():
    try:
        out = subprocess.run(["git", "-C", str(ROOT), "rev-parse", "--path-format=absolute", "--git-common-dir"],
                             capture_output=True, text=True, check=True).stdout.strip()
        return Path(out).parent
    except (OSError, subprocess.CalledProcessError):
        return None


def is_lfs_pointer(path):
    try:
        if path.stat().st_size > 1024:
            return False
        return path.read_bytes().startswith(b"version https://git-lfs")
    except OSError:
        return True


class DataRoots:
    def __init__(self, data_root=None):
        roots = [Path(data_root)] if data_root else []
        roots.append(ROOT)
        main = main_checkout()
        if main and main.resolve() != ROOT.resolve():
            roots.append(main)
        self.roots = roots
        self.used = {}

    def find(self, rel):
        for r in self.roots:
            p = r / rel
            if p.exists() and not is_lfs_pointer(p):
                self.used[str(rel).replace("\\", "/")] = str(r)
                return p
        raise FileNotFoundError(f"{rel}: absent or LFS pointer in {', '.join(str(r) for r in self.roots)} "
                                f"(pass --data-root to a checkout with the LFS data)")


# ---------------------------------------------------------------------------------------------- atoll

class Atoll:
    """Grid of the reference mosaic (one pixel = ground_m_per_px), in the UE frame of terrain/georef.json:
    x = (col + 0.5) * m - cx (east), y = (row + 0.5) * m - cz (south), metres; cm = x 100."""

    def __init__(self, island, data_root=None, need=("land", "cls", "height", "masks", "coco", "bldg")):
        self.island = island
        self.rel = Path("docs") / "reference" / "map" / island
        self.D = DataRoots(data_root)
        hm_path = self.D.find(self.rel / "terrain" / "heightmap_16.png") if "height" in need else None
        geo_path = hm_path.parent / "georef.json" if hm_path else self.D.find(self.rel / "terrain" / "georef.json")
        self.georef = json.loads(geo_path.read_text(encoding="utf-8"))
        self.D.used[str(self.rel / "terrain" / "georef.json").replace("\\", "/")] = str(geo_path.parents[5])
        sg = self.georef["source_grid"]
        self.meta = {"merc_bounds": sg["merc_bounds"], "merc_px": sg["merc_px"]}
        self.W, self.H = sg["width"], sg["height"]
        self.m = float(self.georef["ground_m_per_px"])
        self.cx, self.cz = self.W * self.m / 2.0, self.H * self.m / 2.0
        shape = (self.H, self.W)
        self.land, rings = bt.rasterize_coastline(self.D.find(self.rel / "rivage.geojson"), self.meta, shape)
        self.cls = None
        if "cls" in need:
            atlas = np.asarray(Image.open(self.D.find(self.rel / "atlas_recif_z15.png")).convert("RGB")).astype(np.float32)
            self.cls = bt.classify_atlas(atlas)
            del atlas
            self.cls[self.land] = bt.LAND
            self.cls[(self.cls == bt.LAND) & ~self.land] = bt.REEF_FLAT
        self.h = None
        if hm_path is not None:
            raw = np.asarray(Image.open(hm_path)).astype(np.float32)
            hm = self.georef["heightmap_16"]
            self.h = (hm["min_m"] + raw / 65535.0 * (hm["max_m"] - hm["min_m"])).astype(np.float32)
            del raw
        self.sand = self.veg = None
        if "masks" in need:
            im = Image.open(self.D.find(self.rel / "terrain" / "T_Raraka_Masks.png".replace("Raraka", island.capitalize())))
            r, g = im.split()[:2]
            self.veg = np.asarray(r.resize((self.W, self.H), Image.BILINEAR))
            self.sand = np.asarray(g.resize((self.W, self.H), Image.BILINEAR))
        self.coco = None
        if "coco" in need:
            self.coco = self.rasterize_geojson(self.D.find(self.rel / "cocoteraies.geojson")) & self.land
        self.buildings = []
        self.bldg = None
        if "bldg" in need:
            self.bldg, self.buildings = self.load_buildings(self.D.find(self.rel / "batiments.geojson"))

    # ---------------------------------------------------------------- frame
    def px_to_m(self, cols, rows):
        return (np.asarray(cols) + 0.5) * self.m - self.cx, (np.asarray(rows) + 0.5) * self.m - self.cz

    def m_to_px(self, x, y):
        """Continuous pixel coordinates (col, row) of UE metres (pixel centres at .5)."""
        return (np.asarray(x) + self.cx) / self.m, (np.asarray(y) + self.cz) / self.m

    def sample(self, img, x, y):
        c, r = self.m_to_px(x, y)
        c = np.clip(np.floor(c).astype(int), 0, self.W - 1)
        r = np.clip(np.floor(r).astype(int), 0, self.H - 1)
        return img[r, c]

    def lonlat_to_px(self, lon, lat):
        mb, mp = self.meta["merc_bounds"], self.meta["merc_px"]
        mx, my = bt.merc(np.asarray(lon, float), np.asarray(lat, float))
        return (mx - mb[0]) / mp, (mb[3] - my) / mp

    def rasterize_geojson(self, path):
        img = Image.new("1", (self.W, self.H), 0)
        d = ImageDraw.Draw(img)
        for f in json.loads(Path(path).read_text(encoding="utf-8"))["features"]:
            for ring in _polygon_rings(f["geometry"]):
                pts = np.asarray(ring, float)
                c, r = self.lonlat_to_px(pts[:, 0], pts[:, 1])
                d.polygon(list(zip(c - 0.5, r - 0.5)), fill=1, outline=1)
        return np.asarray(img, bool)

    def load_buildings(self, path):
        img = Image.new("1", (self.W, self.H), 0)
        d = ImageDraw.Draw(img)
        out = []
        for f in json.loads(Path(path).read_text(encoding="utf-8"))["features"]:
            rings = _polygon_rings(f["geometry"])
            if not rings:
                continue
            pts = np.asarray(rings[0], float)
            c, r = self.lonlat_to_px(pts[:, 0], pts[:, 1])
            d.polygon(list(zip(c - 0.5, r - 0.5)), fill=1, outline=1)
            x, y = c * self.m - self.cx, r * self.m - self.cz
            area = 0.5 * abs(np.dot(x, np.roll(y, 1)) - np.dot(y, np.roll(x, 1)))
            p = f["properties"]
            out.append({"xy": np.array([x.mean(), y.mean()]), "poly": np.c_[x, y], "area_m2": float(area),
                        "material": (p.get("materiau") or "").strip(), "category": p.get("categorie")})
        return np.asarray(img, bool), out


def _polygon_rings(geom):
    t = geom["type"]
    if t == "Polygon":
        return [r for r in geom["coordinates"]]
    if t == "MultiPolygon":
        return [r for poly in geom["coordinates"] for r in poly]
    return []


# ---------------------------------------------------------------------------------------------- building sectors

def building_sectors(buildings, link_m):
    """Single linkage of the building centroids (link_m): one sector = a group of buildings."""
    n = len(buildings)
    parent = list(range(n))

    def find(i):
        while parent[i] != i:
            parent[i] = parent[parent[i]]
            i = parent[i]
        return i
    P = np.array([b["xy"] for b in buildings])
    for i, j in cKDTree(P).query_pairs(link_m):
        parent[find(i)] = find(j)
    groups = {}
    for i in range(n):
        groups.setdefault(find(i), []).append(i)
    return sorted(groups.values(), key=lambda g: -len(g))


# ---------------------------------------------------------------------------------------------- ring axis

class Ring:
    """Axis of the atoll ring: for each angular sector around the lagoon centre, the median radius of the rim
    pixels (land + reef flat), smoothed and closed. s = arc length along the axis (m), origin in the middle of the
    longest stretch without land; lateral offset along the normal of the axis, > 0 toward the ocean. A line of
    constant s is the normal of the axis at s (nearest-point projection)."""

    def __init__(self, A, rules, origin_s=None):
        rr = rules["ring"]
        rim = A.land | (A.cls == bt.REEF_FLAT)
        lag_r, lag_c = np.nonzero(A.cls[::4, ::4] == bt.LAGOON)
        lx, ly = A.px_to_m(lag_c * 4, lag_r * 4)
        self.center = np.array([lx.mean(), ly.mean()])
        r, c = np.nonzero(rim[::2, ::2])
        x, y = A.px_to_m(c * 2, r * 2)
        dx, dy = x - self.center[0], y - self.center[1]
        n = rr["angle_bins"]
        b = (((np.arctan2(dy, dx) + math.pi) / (2 * math.pi)) * n).astype(int) % n
        rad = np.hypot(dx, dy)
        med = np.array(ndi.median(rad, labels=b + 1, index=np.arange(1, n + 1)), float)
        ok = np.isfinite(med) & (np.bincount(b, minlength=n) > 0)
        idx = np.arange(n)
        med = np.interp(idx, idx[ok], med[ok], period=n)
        med = ndi.gaussian_filter1d(med, rr["smooth_bins"], mode="wrap")
        ang = (idx + 0.5) / n * 2 * math.pi - math.pi
        pts = self.center + np.c_[med * np.cos(ang), med * np.sin(ang)]
        pts = np.vstack([pts, pts[:1]])
        seg = np.linalg.norm(np.diff(pts, axis=0), axis=1)
        s = np.r_[0.0, np.cumsum(seg)]
        self.L = float(s[-1])
        step = rr["sample_m"]
        ss = np.arange(0.0, self.L, step)
        self.raw_pts = np.c_[np.interp(ss, s, pts[:, 0]), np.interp(ss, s, pts[:, 1])]
        self.raw_s = ss
        self.step = step
        tg = np.roll(self.raw_pts, -1, axis=0) - np.roll(self.raw_pts, 1, axis=0)
        tg /= np.linalg.norm(tg, axis=1, keepdims=True)
        nr = np.c_[tg[:, 1], -tg[:, 0]]
        nr *= np.sign(((self.raw_pts - self.center) * nr).sum(1))[:, None]   # outward
        self.raw_t, self.raw_n = tg, nr
        self.tree = cKDTree(self.raw_pts)
        self.shift = 0.0
        if origin_s is None:
            lr, lc = np.nonzero(A.land)
            sl, _ = self.project(*A.px_to_m(lc, lr))
            origin_s = self.longest_gap_middle(sl)
        self.shift = float(origin_s)

    def longest_gap_middle(self, s_values):
        v = np.sort(np.mod(s_values, self.L))
        gaps = np.diff(np.r_[v, v[0] + self.L])
        k = int(np.argmax(gaps))
        return float((v[k] + gaps[k] / 2.0) % self.L)

    def project(self, x, y):
        """(s, lateral) of points in UE metres. s in [0, L) from the origin."""
        P = np.c_[np.ravel(x), np.ravel(y)]
        _, k = self.tree.query(P)
        q = self.raw_pts[k]
        lat = ((P - q) * self.raw_n[k]).sum(1)
        return np.mod(self.raw_s[k] - self.shift, self.L), lat

    def at(self, s):
        """Point, unit tangent and outward normal of the axis at abscissa s (m, from the origin)."""
        s = np.mod(np.asarray(s, float) + self.shift, self.L)
        k = (s / self.step).astype(int) % len(self.raw_pts)
        return self.raw_pts[k], self.raw_t[k], self.raw_n[k]

    def polyline(self, every_m=50.0):
        s = np.arange(0.0, self.L, every_m)
        p, _, _ = self.at(s)
        return s, p


# ---------------------------------------------------------------------------------------------- polygons

def mask_to_rings(mask):
    """Closed rings (pixel corner coordinates (col, row)) of the boundary edges of a boolean mask, land on the right
    when walking (screen coordinates). Filled with the even-odd rule they give back the mask exactly."""
    M = np.pad(mask.astype(bool), 1)
    nxt = {}
    # horizontal boundaries between row r-1 and r (padded), at y = r - 1 in original corner coords
    a, b = M[:-1, :], M[1:, :]
    rr, cc = np.nonzero(a & ~b)            # land above, water below: edge goes -x
    for r, c in zip(rr, cc):
        y, x = r, c - 1
        nxt.setdefault((x + 1, y), []).append((x, y))
    rr, cc = np.nonzero(~a & b)            # water above, land below: edge goes +x
    for r, c in zip(rr, cc):
        y, x = r, c - 1
        nxt.setdefault((x, y), []).append((x + 1, y))
    a, b = M[:, :-1], M[:, 1:]
    rr, cc = np.nonzero(a & ~b)            # land left, water right: edge goes +y
    for r, c in zip(rr, cc):
        x, y = c, r - 1
        nxt.setdefault((x, y), []).append((x, y + 1))
    rr, cc = np.nonzero(~a & b)            # water left, land right: edge goes -y
    for r, c in zip(rr, cc):
        x, y = c, r - 1
        nxt.setdefault((x, y + 1), []).append((x, y))
    rings = []
    while nxt:
        start = next(iter(nxt))
        ring = [start]
        cur = start
        while True:
            outs = nxt[cur]
            n = outs.pop()
            if not outs:
                del nxt[cur]
            if n == start:
                break
            ring.append(n)
            cur = n
        rings.append(np.array(ring, float))
    return rings


def _dp(pts, tol):
    """Douglas-Peucker on an open polyline (keeps both ends)."""
    keep = np.zeros(len(pts), bool)
    keep[0] = keep[-1] = True
    stack = [(0, len(pts) - 1)]
    while stack:
        i, j = stack.pop()
        if j <= i + 1:
            continue
        a, b = pts[i], pts[j]
        ab = b - a
        L = np.hypot(*ab)
        seg = pts[i + 1:j]
        if L < 1e-9:
            d = np.hypot(*(seg - a).T)
        else:
            d = np.abs(ab[0] * (seg[:, 1] - a[1]) - ab[1] * (seg[:, 0] - a[0])) / L
        k = int(np.argmax(d))
        if d[k] > tol:
            keep[i + 1 + k] = True
            stack += [(i, i + 1 + k), (i + 1 + k, j)]
    return pts[keep]


def simplify_ring(ring, tol):
    if len(ring) < 8:
        return ring
    far = int(np.argmax(np.hypot(*(ring - ring[0]).T)))
    a = _dp(np.vstack([ring[:far + 1]]), tol)
    b = _dp(np.vstack([ring[far:], ring[:1]]), tol)
    out = np.vstack([a[:-1], b[:-1]])
    return out if len(out) >= 3 else ring


def ring_area(ring):
    x, y = ring[:, 0], ring[:, 1]
    return 0.5 * (np.dot(x, np.roll(y, -1)) - np.dot(y, np.roll(x, -1)))


def mask_to_polygons_cm(A, mask, tol_m=0.05):
    """Rings of a full-grid mask in UE cm: [{"hole": bool, "points": [[x, y], ...]}], even-odd fill.
    Pixel-exact: only collinear corners are dropped (a looser simplification loses corner pixels, and the check
    then sees kept land touching excluded land)."""
    rows, cols = np.nonzero(mask)
    if not len(rows):
        return []
    r0, r1, c0, c1 = rows.min(), rows.max() + 1, cols.min(), cols.max() + 1
    out = []
    for ring in mask_to_rings(mask[r0:r1, c0:c1]):
        ring = simplify_ring(ring, tol_m / A.m)
        x = (ring[:, 0] + c0) * A.m - A.cx
        y = (ring[:, 1] + r0) * A.m - A.cz
        area = ring_area(np.c_[x, y])
        # screen coords (y down) walked with the land on the right: an outer ring has a positive signed area here
        out.append({"hole": bool(area < 0), "points": [[round(float(a) * 100, 1), round(float(b) * 100, 1)]
                                                       for a, b in zip(x, y)]})
    return out


def polygons_to_mask(A, polygons, window=None):
    """Even-odd rasterisation of rings in UE cm on the grid (or on window = (r0, r1, c0, c1))."""
    r0, r1, c0, c1 = window or (0, A.H, 0, A.W)
    h, w = r1 - r0, c1 - c0
    rows_l, starts_l = [], []
    for ring in polygons:
        P = np.asarray(ring["points"], float) / 100.0
        c, r = A.m_to_px(P[:, 0], P[:, 1])            # pixel corner coordinates (centres at .5)
        c, r = c - c0, r - r0
        xa, ya, xb, yb = c, r, np.roll(c, -1), np.roll(r, -1)
        for x1, y1, x2, y2 in zip(xa, ya, xb, yb):
            if y1 == y2:
                continue
            lo, hi = min(y1, y2), max(y1, y2)
            rr = np.arange(max(int(math.ceil(lo - 0.5)), 0), min(int(math.ceil(hi - 0.5)), h))
            if not len(rr):
                continue
            yc = rr + 0.5
            xc = x1 + (yc - y1) * (x2 - x1) / (y2 - y1)
            rows_l.append(rr)
            starts_l.append(np.clip(np.ceil(xc - 0.5).astype(int), 0, w))
    out = np.zeros((h, w + 1), np.int32)
    if rows_l:
        np.add.at(out, (np.concatenate(rows_l), np.concatenate(starts_l)), 1)
    return (np.cumsum(out, axis=1)[:, :w] % 2).astype(bool)


def seg_points_dist(P, a, b):
    """Distance of points P (n, 2) to the segment a-b."""
    P = np.asarray(P, float).reshape(-1, 2)
    a, b = np.asarray(a, float), np.asarray(b, float)
    ab = b - a
    t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
    return np.linalg.norm(P - (a + t[:, None] * ab), axis=1)


def font(size):
    from PIL import ImageFont
    for name in ("arial.ttf", "DejaVuSans.ttf", "segoeui.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def arrival_line_ok(A, p, t, final_approach_m, min_depth_m, step_m=1.0):
    """Straight line from the arrival p to the beach t (UE m): no land (except at the beach) nor pinnacle, and
    beyond the final approach at least min_depth_m of water and no reef flat. Returns (ok, reason, min depth)."""
    v = np.asarray(t, float) - np.asarray(p, float)
    dist = float(np.linalg.norm(v))
    u = np.linspace(0.0, 1.0, max(2, int(dist / step_m)) + 1)
    Q = np.asarray(p, float)[None] + u[:, None] * v[None]
    col, row = A.m_to_px(Q[:, 0], Q[:, 1])
    col = np.clip(np.floor(col).astype(int), 0, A.W - 1)
    row = np.clip(np.floor(row).astype(int), 0, A.H - 1)
    to_go = dist * (1 - u)
    h, cls, land = A.h[row, col], A.cls[row, col], A.land[row, col]
    open_w = to_go > final_approach_m
    if land[to_go > 2 * A.m].any():
        return False, "la ligne vers la plage traverse de la terre", None
    if (cls == bt.PINNACLE).any():
        return False, "pinacle sur la ligne vers la plage", None
    if not open_w.any():
        return False, "ligne plus courte que l'approche finale", None
    depth = float(-h[open_w].max())
    if depth < min_depth_m:
        return False, f"moins de {min_depth_m} m d'eau avant l'approche finale (fond max {-depth:.2f} m)", depth
    if (cls[open_w] == bt.REEF_FLAT).any():
        return False, "platier sur la ligne avant l'approche finale", depth
    return True, "", depth
