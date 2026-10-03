"""Motu terrain: cross-section of a Tuamotu islet with real relief.

X axis: ocean (-) -> lagoon (+). Y axis: along the motu.
ocean drop-off | algal ridge | reef flat (feo, pools) | rubble storm ridge (+3 m) | grove interior
(hummocks) | berm | lagoon beach | lagoon terrace with coral heads | lagoon drop-off.
"""
import math

import numpy as np

from .geo import MeshBuilder, Noise, norm, smoothstep

SEA = 0.0


class Motu:
    def __init__(self, seed=7):
        self.nz = Noise(seed)
        self.nh = Noise(seed + 101)
        rng = np.random.default_rng(seed)
        # lagoon coral heads ("pâtés"): (along-y, offset from waterline, radius, top height)
        n = 26
        self.heads = np.stack([rng.uniform(-260, 260, n), rng.uniform(10, 34, n), rng.uniform(1.2, 4.0, n),
                               rng.uniform(-1.1, -0.35, n)], 1)
        self.palms_xy = np.zeros((0, 2))

    # ---- shore lines (functions of y, meters)
    def xr(self, y):
        return -58 + 7 * (self.nz.fbm1(np.asarray(y) / 45 + 10) - 0.5)

    def xc(self, y):
        return -26 + 7 * (self.nz.fbm1(np.asarray(y) / 38 + 30) - 0.5)

    def xl(self, y):
        return 44 + 12 * (self.nz.fbm1(np.asarray(y) / 55 + 50) - 0.5)

    def xv(self, y):
        return self.xl(y) - 11.5 - 6 * (self.nz.fbm1(np.asarray(y) / 22 + 70) - 0.5)

    def xo(self, y):
        return self.xc(y) + 5 + 5 * (self.nz.fbm1(np.asarray(y) / 26 + 90) - 0.5)

    # ---- height
    def _profile(self, x, y):
        xr, xc, xl, xv = self.xr(y), self.xc(y), self.xl(y), self.xv(y)
        # (x breakpoint, height, easing into this breakpoint)
        bp = [(xr - 60, -25.0, "lin"), (xr - 5, -3.0, "smooth"), (xr, -0.45, "smooth"), (xr + 4, -0.10, "smooth"),
              (xc - 12, -0.22, "lin"), (xc - 7, 0.30, "smooth"), (xc, 3.1, "smooth"), (xc + 9, 2.3, "smooth"),
              (xv - 1, 1.6, "lin"), (xv + 3, 1.85, "smooth"), (xl, 0.0, "pow"), (xl + 22, -1.5, "lin"),
              (xl + 36, -10.0, "smooth"), (xl + 150, -17.0, "smooth"), (xl + 2000, -18.0, "lin")]
        X = np.stack([np.broadcast_to(b[0], x.shape) for b in bp], -1)
        Hh = np.array([b[1] for b in bp])
        k = np.clip((x[..., None] >= X).sum(-1) - 1, 0, len(bp) - 2)
        x0 = np.take_along_axis(X, k[..., None], -1)[..., 0]
        x1 = np.take_along_axis(X, (k + 1)[..., None], -1)[..., 0]
        u = np.clip((x - x0) / np.maximum(x1 - x0, 1e-6), 0, 1)
        kind = np.array([{"lin": 0, "smooth": 1, "pow": 2}[b[2]] for b in bp])[k + 1]
        e = np.where(kind == 1, u * u * (3 - 2 * u), np.where(kind == 2, 1 - (1 - u) ** 1.35, u))
        h = Hh[k] + (Hh[k + 1] - Hh[k]) * e
        return h, dict(xr=xr, xc=xc, xl=xl, xv=xv, xo=self.xo(y))

    def height(self, x, y, with_zones=False):
        x, y = np.broadcast_arrays(np.asarray(x, np.float64), np.asarray(y, np.float64))
        h, z = self._profile(x, y)
        nz = self.nh
        interior = smoothstep(z["xc"] + 6, z["xc"] + 12, x) * (1 - smoothstep(z["xv"] - 2, z["xv"] + 2, x))
        h = h + interior * (0.38 * (nz.fbm2(x / 16, y / 16) - 0.5) * 2 + 0.07 * (nz.fbm2(x / 3.5, y / 3.5) - 0.5) * 2)
        ridge = smoothstep(z["xc"] - 9, z["xc"] - 6, x) * (1 - smoothstep(z["xc"] + 6, z["xc"] + 10, x))
        h = h + ridge * 0.22 * (nz.fbm2(x / 4, y / 4) - 0.5) * 2
        flat = smoothstep(z["xr"] + 2, z["xr"] + 6, x) * (1 - smoothstep(z["xc"] - 13, z["xc"] - 9, x))
        h = h + flat * (0.16 * (nz.fbm2(x / 7, y / 7, 3) - 0.5) * 2 + 0.04 * (nz.fbm2(x * 1.3, y * 1.3, 2) - 0.5))
        beach = smoothstep(z["xv"], z["xv"] + 3, x) * (1 - smoothstep(z["xl"] + 18, z["xl"] + 24, x))
        h = h + beach * 0.025 * (nz.fbm2(x * 0.9, y * 0.35, 2) - 0.5) * 2
        heads = np.zeros_like(h)
        for hy, off, rad, top in self.heads:
            hx = self.xl(hy) + off
            d2 = ((x - hx) ** 2 + (y - hy) ** 2) / rad ** 2
            bump = np.exp(-d2 ** 1.5) * (1 + 0.25 * (nz.fbm2(x * 0.8, y * 0.8, 2) - 0.5))
            lift = np.maximum(0, top - h) * bump
            h = h + lift
            heads = np.maximum(heads, bump * (lift > 0.05))
        if with_zones:
            z["heads"] = heads
            return h, z
        return h

    def normal(self, x, y, e=0.25):
        hx = (self.height(x + e, y) - self.height(x - e, y)) / (2 * e)
        hy = (self.height(x, y + e) - self.height(x, y - e)) / (2 * e)
        return norm(np.stack([-hx, -hy, np.ones_like(hx)], -1))

    # ---- material masks
    def masks(self, x, y, h, z):
        nz = self.nz
        n1 = nz.fbm2(x / 6, y / 6, 3)
        n2 = nz.fbm2(x / 2.2 + 40, y / 2.2, 3)
        sand_dry = smoothstep(z["xv"] - 4 + 4 * n1, z["xv"] + 0.5 + 4 * n1, x) * smoothstep(0.08, 0.4, h)
        wrack = (np.exp(-((h - 1.2) / 0.14) ** 2) * (x > z["xv"]) * smoothstep(0.35, 0.65, n2)
                 + np.exp(-((h - 0.55) / 0.2) ** 2) * (x < z["xc"]) * 0.7 * smoothstep(0.4, 0.6, n2))
        soil = (smoothstep(z["xo"] - 3 + 3 * n1, z["xo"] + 2 + 3 * n1, x)
                * (1 - smoothstep(z["xv"] - 3 + 3 * n1, z["xv"] + 0.5 + 3 * n1, x)))
        litter = np.zeros_like(x)
        if len(self.palms_xy):
            for px, py in self.palms_xy:
                d2 = (x - px) ** 2 + (y - py) ** 2
                litter = np.maximum(litter, np.exp(-d2 / 3.2 ** 2))
        litter = np.clip(litter * (0.6 + 0.8 * n2) + soil * smoothstep(0.62, 0.75, n1) * 0.8, 0, 1) * soil
        rubble = (smoothstep(z["xc"] - 13 + 2 * n2, z["xc"] - 10 + 2 * n2, x)
                  * (1 - smoothstep(z["xo"] - 1 + 3 * n1, z["xo"] + 4 + 3 * n1, x)))
        rubble = np.maximum(rubble, soil * smoothstep(0.55, 0.68, nz.fbm2(x / 9 + 9, y / 9, 3)) * 0.8)
        rubble = np.maximum(rubble, z["heads"] * 0.6)
        feo = 1 - smoothstep(z["xc"] - 13.5 + 2 * n1, z["xc"] - 11 + 2 * n1, x)
        feo = np.maximum(feo, z["heads"])
        wet = 1 - smoothstep(z["xc"] - 10, z["xc"] - 7, x)
        algae = feo * smoothstep(0.45, 0.65, nz.fbm2(x / 3 + 3, y / 3, 3)) * (h < 0.1)
        A = np.stack([sand_dry, np.clip(wrack, 0, 1), soil, litter], -1)
        B = np.stack([rubble, feo, wet, algae], -1)
        return A.astype(np.float32), B.astype(np.float32)


def axis_coords():
    """Rectilinear grid: 0.5 m in the play area, growing toward the horizon (watertight)."""
    fine_x = np.arange(-90, 90.001, 0.5)
    ox = -90 - np.cumsum(np.full(32, 5.0))
    lx = 90 + np.cumsum(np.full(32, 5.0))
    xs = np.concatenate([ox[::-1], fine_x, lx])
    fine_y = np.arange(-110, 110.001, 0.5)
    steps = 0.5 * 1.045 ** np.arange(110)
    far = 110 + np.cumsum(steps)
    ys = np.concatenate([-far[::-1], fine_y, far])
    return xs, ys


def build_terrain(motu):
    """Return (MeshBuilder, extent) for the terrain grid with MaskA/MaskB in extra arrays."""
    xs, ys = axis_coords()
    X, Y = np.meshgrid(xs, ys)
    H, Z = motu.height(X, Y, with_zones=True)
    A, B = motu.masks(X, Y, H, Z)
    mb = MeshBuilder()
    co = np.stack([X, Y, H], -1).reshape(-1, 3)
    uv = np.stack([X, Y], -1).reshape(-1, 2)
    base = mb.verts(co, uv, None)
    ny, nx = X.shape
    idx = base + np.arange(nx * ny).reshape(ny, nx)
    q = np.stack([idx[:-1, :-1], idx[:-1, 1:], idx[1:, 1:], idx[1:, :-1]], -1).reshape(-1, 4)
    mb.faces(q, 0)
    return mb, A.reshape(-1, 4), B.reshape(-1, 4), (xs[0], xs[-1], ys[0], ys[-1])


def add_mask_attrs(me, A, B):
    for name, arr in (("MaskA", A), ("MaskB", B)):
        ca = me.color_attributes.new(name, "FLOAT_COLOR", "POINT")
        ca.data.foreach_set("color", arr.ravel())


def ribbon(motu, xfun, y0, y1, step, width, offset, zfun=None, v_scale=10.0):
    """Ribbon mesh following a shoreline x=xfun(y). u across (0..1), v along y/v_scale."""
    ys = np.arange(y0, y1 + 1e-6, step)
    nu = 9
    u = np.linspace(0, 1, nu)
    xc = xfun(ys)
    X = xc[:, None] + offset + u[None, :] * width
    Y = np.broadcast_to(ys[:, None], X.shape)
    Z = np.zeros_like(X) if zfun is None else zfun(u)[None, :] * np.ones_like(X)
    mb = MeshBuilder()
    uv = np.stack([np.broadcast_to(u, X.shape), Y / v_scale], -1).reshape(-1, 2)
    base = mb.verts(np.stack([X, Y, Z], -1).reshape(-1, 3), uv)
    idx = base + np.arange(X.size).reshape(X.shape)
    q = np.stack([idx[:-1, :-1], idx[:-1, 1:], idx[1:, 1:], idx[1:, :-1]], -1).reshape(-1, 4)
    mb.faces(q, 0)
    return mb


def plane(x0, x1, y0, y1, z):
    mb = MeshBuilder()
    co = np.array([[x0, y0, z], [x1, y0, z], [x1, y1, z], [x0, y1, z]])
    base = mb.verts(co, co[:, :2] / 10.0)
    mb.faces(np.array([[base, base + 1, base + 2, base + 3]]), 0)
    return mb


def far_motu(seed, x0=2400.0, width=160.0, y_extent=4000.0):
    """Opposite rim of the atoll across the lagoon: low sand strip + shrub mass (palms scattered separately)."""
    rng = np.random.default_rng(seed)
    nz = Noise(seed)
    ys = np.arange(-y_extent, y_extent + 1, 20.0)
    w = width * (0.6 + 0.8 * nz.fbm1(ys / 400))
    mb = MeshBuilder()
    xs = np.linspace(0, 1, 6)
    X = x0 + xs[None, :] * w[:, None]
    Hh = np.array([-0.5, 1.2, 2.2, 2.2, 1.2, -0.5])
    Z = np.broadcast_to(Hh, X.shape)
    Y = np.broadcast_to(ys[:, None], X.shape)
    base = mb.verts(np.stack([X, Y, Z], -1).reshape(-1, 3), np.stack([X, Y], -1).reshape(-1, 2) / 3.0)
    idx = base + np.arange(X.size).reshape(X.shape)
    mb.faces(np.stack([idx[:-1, :-1], idx[:-1, 1:], idx[1:, 1:], idx[1:, :-1]], -1).reshape(-1, 4), 0)
    # foliage mass: a lumpy half-tube on top
    fm = MeshBuilder()
    K = 8
    ang = np.linspace(0, math.pi, K)
    cx = x0 + w * 0.5
    r = w * 0.32 * (0.7 + 0.6 * nz.fbm1(ys / 90 + 5))
    hgt = 5.0 * (0.6 + 0.8 * nz.fbm1(ys / 60 + 11))
    FX = cx[:, None] - np.cos(ang)[None, :] * r[:, None]
    FZ = 1.5 + np.sin(ang)[None, :] * hgt[:, None]
    FY = np.broadcast_to(ys[:, None], FX.shape)
    b2 = fm.verts(np.stack([FX, FY, FZ], -1).reshape(-1, 3), None)
    i2 = b2 + np.arange(FX.size).reshape(FX.shape)
    fm.faces(np.stack([i2[:-1, :-1], i2[:-1, 1:], i2[1:, 1:], i2[1:, :-1]], -1).reshape(-1, 4), 0)
    py = rng.uniform(-y_extent, y_extent, 900)
    px = x0 + rng.uniform(0.15, 0.85, 900) * np.interp(py, ys, w)
    return mb, fm, np.stack([px, py], 1)
