"""Geometry helpers: fast numpy mesh builder, leaf strips, tubes, splines, noise, rotations."""
import math

import numpy as np

UP = np.array([0.0, 0.0, 1.0])
GOLDEN = 2.399963


def norm(v, axis=-1):
    v = np.asarray(v, np.float64)
    n = np.linalg.norm(v, axis=axis, keepdims=True)
    return v / np.maximum(n, 1e-12)


class MeshBuilder:
    """Accumulates per-vertex position / UV / color and polygons, then builds one bpy mesh.

    Colors go to a POINT color attribute "Col": RGB = tint multiplier, A = dryness (0..1).

    Wind (baked for the foliage wind material MF_FoliageWind): every vertex gets
    branch weight, flutter weight and phase; the bend weight is computed at build from the height.
    Set mb.wind_group = (anchor, length, phase) while adding a swaying part (frond, twig): its
    vertices get branch = (distance to anchor / length) ** 1.5. Strips (leaves, leaflets, blades)
    flutter along their length.
    """

    def __init__(self):
        self.co, self.uv, self.col = [], [], []
        self.wind = []
        self.wind_group = None
        self.polys = []
        self.n = 0

    def verts(self, co, uv=None, col=None, wind=None):
        co = np.asarray(co, np.float32).reshape(-1, 3)
        m = len(co)
        w = np.zeros((m, 3), np.float32)
        if wind is not None:
            w = np.asarray(wind, np.float32).reshape(-1, 3)
        elif self.wind_group is not None:
            anchor, length, phase = self.wind_group
            d = np.linalg.norm(co - np.asarray(anchor, np.float32), axis=1) / max(float(length), 1e-3)
            w[:, 0] = np.clip(d, 0, 1) ** 1.5
            w[:, 2] = phase
        self.wind.append(w)
        uv = np.zeros((m, 2), np.float32) if uv is None else np.asarray(uv, np.float32).reshape(-1, 2)
        if col is None:
            col = np.tile(np.array([1, 1, 1, 0], np.float32), (m, 1))
        else:
            col = np.asarray(col, np.float32)
            col = np.tile(col, (m, 1)) if col.ndim == 1 else col.reshape(-1, 4)
        self.co.append(co)
        self.uv.append(uv)
        self.col.append(col)
        base = self.n
        self.n += m
        return base

    def faces(self, idx, mat=0):
        idx = np.asarray(idx, np.int64)
        if len(idx):
            self.polys.append((idx, np.full(len(idx), mat, np.int32)))

    def build(self, name, materials=(), smooth=True):
        import bpy

        co = np.concatenate(self.co)
        uv = np.concatenate(self.uv)
        col = np.concatenate(self.col)
        loops = np.concatenate([p[0].reshape(-1) for p in self.polys]).astype(np.int32)
        sizes = np.concatenate([np.full(len(p[0]), p[0].shape[1]) for p in self.polys])
        mats = np.concatenate([p[1] for p in self.polys])
        starts = np.concatenate([[0], np.cumsum(sizes)[:-1]]).astype(np.int32)
        me = bpy.data.meshes.new(name)
        me.vertices.add(len(co))
        me.vertices.foreach_set("co", co.ravel())
        me.loops.add(len(loops))
        me.loops.foreach_set("vertex_index", loops)
        me.polygons.add(len(starts))
        me.polygons.foreach_set("loop_start", starts)
        me.polygons.foreach_set("material_index", mats)
        uvl = me.uv_layers.new(name="UVMap")
        uvl.data.foreach_set("uv", uv[loops].ravel())
        wind = np.concatenate(self.wind)
        if self.co:  # always: the vegetation materials read UV2/UV3 (zeros = still), a missing channel would be UV0
            # UV1 = copy of UV0 (spare for lightmaps), UV2 = (bend, branch), UV3 = (flutter, phase).
            # FBX -> UE flips V (v' = 1 - v): V is stored flipped so Unreal reads the value itself.
            z = co[:, 2] - co[:, 2].min()
            bend = np.clip(z / max(float(z.max()), 1e-3), 0, 1) ** 2
            if not wind.any():
                bend[:] = 0.0  # rigid object (nut, loose leaf): no wind of its own
            me.uv_layers.new(name="UVLight").data.foreach_set("uv", uv[loops].ravel())
            w0 = np.stack([bend, 1.0 - wind[:, 0]], 1).astype(np.float32)
            w1 = np.stack([wind[:, 1], 1.0 - wind[:, 2]], 1).astype(np.float32)
            me.uv_layers.new(name="UVWind0").data.foreach_set("uv", w0[loops].ravel())
            me.uv_layers.new(name="UVWind1").data.foreach_set("uv", w1[loops].ravel())
        ca = me.color_attributes.new("Col", "FLOAT_COLOR", "POINT")
        ca.data.foreach_set("color", col.ravel())
        me.update(calc_edges=True)
        me.validate(clean_customdata=False)
        if smooth:
            me.shade_smooth()
        for m in materials:
            me.materials.append(m)
        return me


def add_strips(mb, C, U, W, F=None, col=None, mat=0, v=None, wind=None):
    """Leaf / blade ribbons with a folded midrib (3 vertices across).

    C (N,S1,3) centerlines, U (N,S1,3) wanted surface normal, W (N,S1) half widths,
    F (N,S1) midrib offset along the normal (+ ridge up, - channel), col (N,4) or (N,S1,4).
    UV: u across (0, .5, 1), v along (0 base .. 1 tip).
    Wind: flutter = v; wind = (anchor (N,3), length (N,), phase (N,)) per strip, else mb.wind_group.
    """
    C = np.asarray(C, np.float64)
    N, S1, _ = C.shape
    T = norm(np.gradient(C, axis=1))
    U = np.broadcast_to(np.asarray(U, np.float64), C.shape)
    B = np.cross(U, T)
    bad = np.linalg.norm(B, axis=-1) < 1e-6
    if bad.any():
        alt = np.cross(np.array([1.0, 0.0, 0.0]), T)
        B = np.where(bad[..., None], alt, B)
    B = norm(B)
    Un = norm(np.cross(T, B))
    W = np.asarray(W, np.float64)
    F = np.zeros((N, S1)) if F is None else np.asarray(F, np.float64)
    L = C - B * W[..., None]
    M = C + Un * F[..., None]
    R = C + B * W[..., None]
    V = np.stack([L, M, R], axis=2)
    if v is None:
        v = np.broadcast_to(np.linspace(0, 1, S1), (N, S1))
    uv = np.zeros((N, S1, 3, 2), np.float32)
    uv[..., 0] = np.array([0.0, 0.5, 1.0])
    uv[..., 1] = np.asarray(v)[..., None]
    c = None
    if col is not None:
        col = np.asarray(col, np.float32)
        if col.ndim == 2:
            col = np.broadcast_to(col[:, None, :], (N, S1, 4))
        c = np.broadcast_to(col[:, :, None, :], (N, S1, 3, 4)).reshape(-1, 4)
    wv = np.zeros((N, S1, 3, 3), np.float32)
    wv[..., 1] = np.asarray(v, np.float32)[..., None]
    if wind is not None:
        anchor, length, phase = (np.asarray(a, np.float64) for a in wind)
        d = np.linalg.norm(V - anchor[:, None, None, :], axis=-1) / np.maximum(length, 1e-3)[:, None, None]
        wv[..., 0] = np.clip(d, 0, 1) ** 1.5
        wv[..., 2] = phase[:, None, None]
    elif mb.wind_group is not None:
        anchor, length, phase = mb.wind_group
        d = np.linalg.norm(V - np.asarray(anchor), axis=-1) / max(float(length), 1e-3)
        wv[..., 0] = np.clip(d, 0, 1) ** 1.5
        wv[..., 2] = phase
    base = mb.verts(V.reshape(-1, 3), uv.reshape(-1, 2), c, wv.reshape(-1, 3))
    idx = base + np.arange(N * S1 * 3).reshape(N, S1, 3)
    q1 = np.stack([idx[:, :-1, 0], idx[:, 1:, 0], idx[:, 1:, 1], idx[:, :-1, 1]], -1)
    q2 = np.stack([idx[:, :-1, 1], idx[:, 1:, 1], idx[:, 1:, 2], idx[:, :-1, 2]], -1)
    mb.faces(np.stack([q1, q2], 2).reshape(-1, 4), mat)


def frames(C, ref=(1.0, 0.0, 0.0)):
    """Parallel-transport frames (T, N, B) along a polyline."""
    C = np.asarray(C, np.float64)
    T = norm(np.gradient(C, axis=0))
    n = len(C)
    Nn = np.zeros_like(C)
    r = np.asarray(ref, np.float64)
    if abs(np.dot(T[0], r)) > 0.9:
        r = np.array([0.0, 1.0, 0.0]) if abs(T[0][1]) < 0.9 else np.array([0.0, 0.0, 1.0])
    Nn[0] = norm(r - T[0] * np.dot(r, T[0]))
    for i in range(1, n):
        v = Nn[i - 1] - T[i] * np.dot(Nn[i - 1], T[i])
        ln = np.linalg.norm(v)
        Nn[i] = v / ln if ln > 1e-9 else Nn[i - 1]
    B = np.cross(T, Nn)
    return T, Nn, B


def add_tube(mb, C, R, K=8, mat=0, col=None, u_tiles=1.0, v_len=1.0, cap=False, ref=(1.0, 0.0, 0.0)):
    """Tube along polyline C (S1,3) with radii R (S1,). Seam duplicated for clean UVs."""
    C = np.asarray(C, np.float64)
    R = np.broadcast_to(np.asarray(R, np.float64), (len(C),))
    T, Nn, B = frames(C, ref)
    ang = np.linspace(0, 2 * math.pi, K + 1)
    ring = np.cos(ang)[None, :, None] * Nn[:, None, :] + np.sin(ang)[None, :, None] * B[:, None, :]
    V = C[:, None, :] + R[:, None, None] * ring
    S1 = len(C)
    arc = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(C, axis=0), axis=1))])
    uv = np.zeros((S1, K + 1, 2), np.float32)
    uv[..., 0] = (ang / (2 * math.pi) * u_tiles)[None, :]
    uv[..., 1] = (arc / v_len)[:, None]
    c = None
    if col is not None:
        col = np.asarray(col, np.float32)
        c = np.tile(col, (S1 * (K + 1), 1)) if col.ndim == 1 else np.repeat(col, K + 1, axis=0)
    base = mb.verts(V.reshape(-1, 3), uv.reshape(-1, 2), c)
    idx = base + np.arange(S1 * (K + 1)).reshape(S1, K + 1)
    q = np.stack([idx[:-1, :-1], idx[:-1, 1:], idx[1:, 1:], idx[1:, :-1]], -1).reshape(-1, 4)
    mb.faces(q, mat)
    if cap:
        cb = mb.verts(C[-1:] + T[-1:] * R[-1] * 0.3, np.array([[0.5, arc[-1] / v_len]]),
                      None if col is None else (col if col.ndim == 1 else col[-1]))
        ring_i = idx[-1, :-1]
        tri = np.stack([ring_i, np.roll(ring_i, -1), np.full(K, cb)], -1)
        mb.faces(tri, mat)
    return T, Nn, B


def spline(P, n):
    """Catmull-Rom through control points, resampled to n points evenly by arc length."""
    P = np.asarray(P, np.float64)
    if len(P) == 2:
        return np.linspace(P[0], P[1], n)
    E = np.vstack([2 * P[0] - P[1], P, 2 * P[-1] - P[-2]])
    out = []
    t = np.linspace(0, 1, 24, endpoint=False)[:, None]
    for i in range(1, len(E) - 2):
        p0, p1, p2, p3 = E[i - 1], E[i], E[i + 1], E[i + 2]
        out.append(0.5 * (2 * p1 + (p2 - p0) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t
                          + (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    out.append(E[-2][None])
    return resample(np.vstack(out), n)


def resample(poly, n):
    poly = np.asarray(poly, np.float64)
    s = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(poly, axis=0), axis=1))])
    t = np.linspace(0, s[-1], n)
    return np.stack([np.interp(t, s, poly[:, k]) for k in range(3)], axis=1)


def integrate_path(start, dirs, ds):
    """Positions from unit directions (n,3) with constant step ds."""
    steps = np.vstack([np.zeros((1, 3)), np.cumsum(dirs[:-1] * ds, axis=0)])
    return np.asarray(start, np.float64) + steps


def polyline_length(C):
    return float(np.linalg.norm(np.diff(C, axis=0), axis=1).sum())


def perp_basis(D):
    """Two unit vectors perpendicular to D (vectorized over leading dims)."""
    D = norm(D)
    a = np.where(np.abs(D[..., 2:3]) < 0.9, np.array([0.0, 0.0, 1.0]), np.array([1.0, 0.0, 0.0]))
    E1 = norm(np.cross(D, a))
    E2 = np.cross(D, E1)
    return E1, E2


def rot_axis(axis, ang):
    axis = norm(axis)
    x, y, z = axis
    c, s = math.cos(ang), math.sin(ang)
    C = 1 - c
    return np.array([[c + x * x * C, x * y * C - z * s, x * z * C + y * s],
                     [y * x * C + z * s, c + y * y * C, y * z * C - x * s],
                     [z * x * C - y * s, z * y * C + x * s, c + z * z * C]])


def rot_z(a):
    return rot_axis((0.0, 0.0, 1.0), a)


def align_z(n):
    """Rotation matrix taking +Z to unit vector n (minimal rotation)."""
    n = norm(n)
    v = np.cross([0.0, 0.0, 1.0], n)
    s = np.linalg.norm(v)
    c = n[2]
    if s < 1e-9:
        return np.eye(3) if c > 0 else np.diag([1.0, -1.0, -1.0])
    return rot_axis(v / s, math.atan2(s, c))


# ---------------------------------------------------------------- primitives / templates

def uv_sphere(nu=12, nv=8):
    """Unit lat-long sphere: (co (V,3), quads (F,4), uv (V,2)); seam and poles duplicated."""
    th = np.linspace(0, math.pi, nv + 1)
    ph = np.linspace(0, 2 * math.pi, nu + 1)
    TH, PH = np.meshgrid(th, ph, indexing="ij")
    co = np.stack([np.sin(TH) * np.cos(PH), np.sin(TH) * np.sin(PH), -np.cos(TH)], -1).reshape(-1, 3)
    uv = np.stack([PH / (2 * math.pi), TH / math.pi], -1).reshape(-1, 2)
    idx = np.arange((nv + 1) * (nu + 1)).reshape(nv + 1, nu + 1)
    q = np.stack([idx[:-1, :-1], idx[:-1, 1:], idx[1:, 1:], idx[1:, :-1]], -1).reshape(-1, 4)
    return co, q, uv


def icosphere(subdiv=2):
    """Unit icosphere: (co (V,3), tris (F,3))."""
    t = (1 + 5 ** 0.5) / 2
    V = [(-1, t, 0), (1, t, 0), (-1, -t, 0), (1, -t, 0), (0, -1, t), (0, 1, t), (0, -1, -t), (0, 1, -t),
         (t, 0, -1), (t, 0, 1), (-t, 0, -1), (-t, 0, 1)]
    V = [tuple(np.array(v) / np.linalg.norm(v)) for v in V]
    Fc = [(0, 11, 5), (0, 5, 1), (0, 1, 7), (0, 7, 10), (0, 10, 11), (1, 5, 9), (5, 11, 4), (11, 10, 2),
          (10, 7, 6), (7, 1, 8), (3, 9, 4), (3, 4, 2), (3, 2, 6), (3, 6, 8), (3, 8, 9), (4, 9, 5),
          (2, 4, 11), (6, 2, 10), (8, 6, 7), (9, 8, 1)]
    for _ in range(subdiv):
        cache = {}

        def mid(a, b):
            key = (min(a, b), max(a, b))
            if key not in cache:
                m = (np.array(V[a]) + np.array(V[b])) / 2
                V.append(tuple(m / np.linalg.norm(m)))
                cache[key] = len(V) - 1
            return cache[key]

        nf = []
        for a, b, c in Fc:
            ab, bc, ca = mid(a, b), mid(b, c), mid(c, a)
            nf += [(a, ab, ca), (b, bc, ab), (c, ca, bc), (ab, bc, ca)]
        Fc = nf
    return np.array(V, np.float64), np.array(Fc, np.int64)


def stamp(mb, co, faces, M, t, uv=None, col=None, mat=0):
    """Add a transformed copy of a template mesh."""
    V = np.asarray(co) @ np.asarray(M).T + np.asarray(t)
    base = mb.verts(V, uv, col)
    mb.faces(np.asarray(faces) + base, mat)
    return base


# ---------------------------------------------------------------- noise

class Noise:
    """Value noise / fBm in 1D, 2D, 3D (vectorized, deterministic per seed)."""

    def __init__(self, seed, size=256):
        rng = np.random.default_rng(seed)
        self.size = size
        self.g2 = rng.random((size, size))
        self.g3 = rng.random((32, 32, 32))

    @staticmethod
    def _s(f):
        return f * f * (3 - 2 * f)

    def n2(self, x, y):
        x = np.asarray(x, np.float64)
        y = np.asarray(y, np.float64)
        s = self.size
        xi = np.floor(x).astype(np.int64)
        yi = np.floor(y).astype(np.int64)
        fx = self._s(x - xi)
        fy = self._s(y - yi)
        g = self.g2
        a = g[yi % s, xi % s]
        b = g[yi % s, (xi + 1) % s]
        c = g[(yi + 1) % s, xi % s]
        d = g[(yi + 1) % s, (xi + 1) % s]
        return (a + (b - a) * fx) * (1 - fy) + (c + (d - c) * fx) * fy

    def fbm2(self, x, y, octaves=4, gain=0.5):
        tot, amp, nrm = 0.0, 1.0, 0.0
        for o in range(octaves):
            f = 2.0 ** o
            tot = tot + amp * self.n2(x * f + 17.1 * o, y * f - 9.3 * o)
            nrm += amp
            amp *= gain
        return tot / nrm  # ~[0,1], mean 0.5

    def fbm1(self, x, octaves=4):
        return self.fbm2(x, np.full_like(np.asarray(x, np.float64), 3.7), octaves)

    def n3(self, p):
        p = np.asarray(p, np.float64)
        s = 32
        i = np.floor(p).astype(np.int64)
        f = self._s(p - i)
        g = self.g3
        out = 0.0
        for dx in (0, 1):
            for dy in (0, 1):
                for dz in (0, 1):
                    w = ((f[..., 0] if dx else 1 - f[..., 0]) * (f[..., 1] if dy else 1 - f[..., 1])
                         * (f[..., 2] if dz else 1 - f[..., 2]))
                    out = out + w * g[(i[..., 0] + dx) % s, (i[..., 1] + dy) % s, (i[..., 2] + dz) % s]
        return out

    def fbm3(self, p, octaves=4, gain=0.5):
        tot, amp, nrm = 0.0, 1.0, 0.0
        for o in range(octaves):
            tot = tot + amp * self.n3(np.asarray(p) * (2.0 ** o) + 5.3 * o)
            nrm += amp
            amp *= gain
        return tot / nrm


def wind_phase(*keys):
    """Deterministic phase in [0, 1) from floats (does not consume the generators' random streams)."""
    x = 0.0
    for i, k in enumerate(np.ravel(keys)):
        x += float(k) * (12.9898 + 31.7 * i)
    return float((math.sin(x) * 43758.5453) % 1.0)


def smoothstep(e0, e1, x):
    t = np.clip((np.asarray(x, np.float64) - e0) / (e1 - e0), 0, 1)
    return t * t * (3 - 2 * t)


def leaf_profile(kind, x):
    """Half-width profile (max 1) along a leaf, x in [0,1] base->tip."""
    x = np.asarray(x, np.float64)
    if kind == "spatulate":
        w = x ** 1.1 * (1 - x) ** 0.28
    elif kind == "obovate":
        w = x ** 0.85 * (1 - x) ** 0.42
    elif kind == "elliptic":
        w = np.sin(np.pi * x) ** 0.85
    elif kind == "lanceolate":
        w = np.minimum(1, x / 0.15) ** 0.5 * (1 - x) ** 0.9
    elif kind == "strap":
        w = np.minimum(1, x / 0.05) ** 0.5 * (1 - x) ** 0.55
    elif kind == "round":
        w = x ** 0.45 * (1 - x) ** 0.42
    elif kind == "leaflet":
        w = np.minimum(1, x / 0.12) ** 0.5 * (1 - x) ** 0.8
    else:
        raise ValueError(kind)
    m = w.max() if w.size else 1.0
    return w / (m if m > 0 else 1.0)
