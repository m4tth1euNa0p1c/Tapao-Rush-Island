"""Coconut palm (Cocos nucifera) generator.

All palms are built in object space with +Z up and the lean toward +X, base at the origin.
Fronds are real geometry (rachis tube + two rows of folded leaflets) so the crown has volume
from every angle and stays opaque (Nanite-friendly in Unreal).

Material slots: 0 bark, 1 leaf, 2 leaf-base fibre, 3 nut, 4 seedling leaf.
"""
import math

import numpy as np

from .geo import (GOLDEN, UP, MeshBuilder, add_strips, add_tube, align_z, integrate_path, leaf_profile, norm,
                  perp_basis, polyline_length, rot_axis, spline, stamp, uv_sphere)

BARK, LEAF, FIBER, NUTM, EOPHYLL = range(5)

_SPH = uv_sphere(12, 9)


def nut_template():
    """Coconut fruit: ovoid, slightly trigonal, pointed at the free end. Long axis = +Z."""
    co, q, uv = _SPH
    co = co.copy()
    ph = np.arctan2(co[:, 1], co[:, 0])
    r = 1 + 0.07 * np.cos(3 * ph) * (1 - np.abs(co[:, 2]))
    co[:, :2] *= r[:, None]
    co[:, 2] = np.where(co[:, 2] > 0, co[:, 2] * 1.18, co[:, 2] * 0.95)  # pointed tip
    co *= np.array([0.105, 0.105, 0.125])
    return co, q, uv


NUT_MESH = nut_template()


def nut_color(rng, maturity):
    """maturity 0 green -> 0.5 yellow-orange -> 1 brown dry."""
    green = np.array([0.30, 0.46, 0.10]) ** 2.2  # sRGB -> linear
    yellow = np.array([0.72, 0.56, 0.14]) ** 2.2
    brown = np.array([0.36, 0.27, 0.17]) ** 2.2
    m = np.clip(maturity + rng.normal(0, 0.08), 0, 1)
    c = green + (yellow - green) * min(1, m * 2) if m < 0.5 else yellow + (brown - yellow) * (m - 0.5) * 2
    dry = max(0.0, (m - 0.55) / 0.45)
    return np.array([*(c * rng.uniform(0.9, 1.1)), dry], np.float32)


# ---------------------------------------------------------------- frond

def frond(mb, rng, origin, az, pitch, length, age, *, dead=False, spear=False, lying=False,
          n_side=84, bend=None, leaflet_scale=1.0, width_scale=1.0):
    """One pinnate frond. age 0 (young) .. 1 (old). Leaflets droop with gravity."""
    h = np.array([math.cos(az), math.sin(az), 0.0])
    side = np.array([-math.sin(az), math.cos(az), 0.0])
    S = 24
    t = np.linspace(0, 1, S + 1)
    if bend is None:
        bend = 0.30 + 1.15 * age + rng.normal(0, 0.08)
    if spear:
        bend = 0.05
    ang = pitch - bend * t ** 1.6
    dirs = np.cos(ang)[:, None] * h + np.sin(ang)[:, None] * UP
    dirs = norm(dirs + side * rng.normal(0, 0.05) * t[:, None])
    pos = integrate_path(origin, dirs, length / S)
    pos[:, 2] = np.maximum(pos[:, 2], 0.025)  # low crowns: old fronds rest on the ground, never below it

    # rachis / petiole (leaf material, yellowish)
    r = 0.040 * (1 - t) ** 0.85 + 0.005
    r[:3] *= [1.9, 1.45, 1.15]
    rcol = np.array([1.75, 1.45, 0.75, 0.25 + 0.5 * age], np.float32)
    if dead:
        rcol = np.array([1.0, 1.0, 1.0, 1.0], np.float32)
    add_tube(mb, pos, r, 6, LEAF, rcol, 1.0, length)

    # leaflets
    t0 = 0.22 if not lying else 0.18
    n = n_side
    sides = np.repeat([-1.0, 1.0], n)
    ts = t0 + (1 - t0) * (np.tile(np.arange(n), 2) + rng.uniform(0, 0.85, 2 * n)) / n
    keep = rng.random(2 * n) > (0.02 + 0.10 * age + (0.25 if dead else 0) + (0.2 if lying else 0))
    ts, sides = ts[keep], sides[keep]
    N = len(ts)
    tt = (ts - t0) / (1 - t0)
    P = np.stack([np.interp(ts, t, pos[:, k]) for k in range(3)], -1)
    Tr = norm(np.stack([np.interp(ts, t, np.gradient(pos[:, k])) for k in range(3)], -1))
    Lb = norm(np.cross(Tr, UP))
    Nf = norm(np.cross(Lb, Tr))
    roll = rng.normal(0, 0.12)
    Lb, Nf = Lb * math.cos(roll) + Nf * math.sin(roll), -Lb * math.sin(roll) + Nf * math.cos(roll)

    if spear:
        phi = np.full(N, 1.35)
        nu = np.full(N, 0.15)
    elif dead:
        phi = rng.uniform(0.6, 1.1, N)
        nu = rng.uniform(-0.9, -0.4, N)
    elif lying:
        phi = rng.uniform(0.35, 0.9, N)
        nu = rng.uniform(-0.05, 0.12, N)
    else:
        phi = 0.48 - 0.12 * tt + rng.normal(0, 0.05, N)
        nu = (0.36 - 0.28 * age) * (1 - 0.4 * tt) + rng.normal(0, 0.05, N)
    D0 = norm(sides[:, None] * Lb * np.cos(phi)[:, None] + Tr * np.sin(phi)[:, None]
              + Nf * (np.tan(nu) * np.cos(phi))[:, None])

    Lmax = 0.95 * (length / 5.0) ** 0.8 * leaflet_scale
    prof = np.sin(np.pi * (0.10 + 0.84 * tt)) ** 0.65
    Ll = Lmax * prof * rng.uniform(0.9, 1.08, N)
    torn = rng.random(N) < (0.08 + 0.25 * age + (0.3 if dead else 0))
    Ll = np.where(torn, Ll * rng.uniform(0.35, 0.8, N), Ll)
    if spear:
        Ll *= 0.75
    if dead:
        droop = rng.uniform(0.7, 1.2, N)
    elif lying:
        droop = np.zeros(N)
    elif spear:
        droop = np.full(N, 0.03)
    else:
        droop = (0.28 + 0.6 * age + 0.25 * tt) * rng.uniform(0.75, 1.25, N)

    Sx = 4
    x = np.linspace(0, 1, Sx + 1)
    C = (P[:, None, :] + D0[:, None, :] * (x[None, :] * Ll[:, None])[..., None]
         - UP * (droop * Ll)[:, None, None] * (x ** 2)[None, :, None])
    C[..., 2] = np.maximum(C[..., 2], 0.012 + 0.01 * rng.random((N, 1)))
    wmax = 0.025 * (Ll / 0.9) ** 0.3 * width_scale
    if spear or dead:
        wmax = wmax * 0.55
    W = wmax[:, None] * leaf_profile("leaflet", x)[None, :]
    F = W * (0.95 if (spear or dead) else 0.32)
    U = np.broadcast_to(Nf[:, None, :], C.shape)

    # color: tint (rgb) + dryness (a)
    yellow = np.clip((age - 0.55) / 0.45, 0, 1)
    tint = np.array([1.08 + 0.45 * yellow, 1.05 + 0.16 * yellow, 0.82 - 0.3 * yellow])
    if spear or age < 0.1:
        tint = np.array([1.22, 1.14, 0.80])
    lt = tint[None, :] * (1 + 0.07 * rng.normal(0, 1, (N, 1)))
    frac = 0.04 + 0.32 * age ** 2
    dry = np.clip((x[None, :] - (1 - frac)) / frac, 0, 1) * (0.45 + 0.55 * age)
    dry = np.maximum(dry, (rng.random((N, 1)) < 0.03 + 0.1 * age) * 0.85)
    if dead or lying:
        dry = np.clip(0.85 + 0.15 * rng.random((N, 1)) + 0 * x[None, :], 0, 1)
    col = np.concatenate([np.broadcast_to(lt[:, None, :], (N, Sx + 1, 3)), dry[..., None]
                          * np.ones((N, Sx + 1, 1))], -1)
    add_strips(mb, C, U, W, F, col, LEAF)
    return pos


# ---------------------------------------------------------------- trunk, roots, crown

def path_from_angles(start, angles, azims, length, n=48):
    """Trunk centerline from tilt-from-vertical angles (n,) and azimuths (n,) along the arc."""
    ds = length / (n - 1)
    d = np.stack([np.sin(angles) * np.cos(azims), np.sin(angles) * np.sin(azims), np.cos(angles)], -1)
    return integrate_path(start, d, ds)


def trunk(mb, rng, C, r_base=0.17, r_top=0.13, bole=0.13, col=None, K=14):
    L = polyline_length(C)
    s = np.concatenate([[0.0], np.cumsum(np.linalg.norm(np.diff(C, axis=0), axis=1))])
    t = np.clip(s / L, 0, 1)
    r = r_top + (r_base - r_top) * (1 - t) ** 1.4 + bole * np.exp(-s / 0.45)
    r *= 1 + 0.035 * np.sin(s * 2 * math.pi / rng.uniform(1.4, 2.2) + rng.uniform(0, 6))
    add_tube(mb, C, r, K, BARK, col if col is not None else np.array([1, 1, 1, 0], np.float32),
             1.0, 1.0, cap=True)
    if not hasattr(mb, "trunks"):
        mb.trunks = []
    mb.trunks.append((np.asarray(C), np.asarray(r)))  # axis + radii, used for the UCX collision hulls
    return r


def roots(mb, rng, base, r0, n=34, plate_dir=None):
    """Adventitious root mass at the bole (orange-brown), or a lifted root plate if plate_dir."""
    for _ in range(n):
        az = rng.uniform(0, 2 * math.pi)
        out = np.array([math.cos(az), math.sin(az), 0.0])
        if plate_dir is None:
            z0 = rng.uniform(0.01, 0.32)
            start = base + out * r0 * 0.8 + UP * z0
            dn = rng.uniform(0.35, 0.95)
            d = out * math.cos(dn) - UP * math.sin(dn)
            ln = rng.uniform(0.25, 0.7)
        else:
            a = rng.uniform(0.25, 1.35)
            e1, e2 = perp_basis(plate_dir)
            b = rng.uniform(0, 2 * math.pi)
            d = norm(plate_dir * math.cos(a) + (e1 * math.cos(b) + e2 * math.sin(b)) * math.sin(a))
            start = base + d * r0 * 0.6
            ln = rng.uniform(0.35, 1.0)
        mid = start + d * ln * 0.5 + rng.normal(0, 0.04, 3)
        C = spline([start, mid, start + d * ln], 6)
        R = np.linspace(rng.uniform(0.010, 0.016), 0.005, 6)
        add_tube(mb, C, R, 5, BARK, np.array([0.95, 0.55, 0.35, 0.0], np.float32), 0.3, 0.6)


def crown(mb, rng, top, axis, n_green=28, n_dead=3, length=(4.6, 5.6), bunches=6, n_side=84,
          leaflet_scale=1.0, bulb=1.0, width_scale=1.0):
    axis = norm(axis)
    # fibrous leaf-base bulb (petiole sheaths)
    bp = [top - axis * 0.55 * bulb, top - axis * 0.1 * bulb, top + axis * 0.45 * bulb, top + axis * 0.8 * bulb]
    C = spline(bp, 9)
    R = np.array([0.17, 0.22, 0.26, 0.27, 0.25, 0.21, 0.16, 0.10, 0.04]) * bulb
    add_tube(mb, C, R, 12, FIBER, np.array([0.9, 0.85, 0.8, 0.0], np.float32), 1.0, 0.8, cap=True)

    az0 = rng.uniform(0, 2 * math.pi)
    ax_pitch = math.asin(np.clip(axis[2], -1, 1))
    ax_az = math.atan2(axis[1], axis[0])
    for i in range(n_green):
        age = i / max(1, n_green - 1)
        spear = i == 0
        az = az0 + i * GOLDEN + rng.normal(0, 0.1)
        if spear:
            pitch, az = ax_pitch, ax_az
        else:
            pitch = 1.22 - 1.70 * age ** 0.9 + rng.normal(0, 0.08)
        L = rng.uniform(*length) * (0.55 if spear else 0.8 if i < 3 else 1.0)
        h = np.array([math.cos(az), math.sin(az), 0.0])
        attach = top + axis * (0.62 - 0.85 * age) * bulb + h * (0.06 + 0.12 * age) * bulb
        frond(mb, rng, attach, az, pitch, L, age, spear=spear, n_side=n_side, leaflet_scale=leaflet_scale,
              width_scale=width_scale)
    for j in range(n_dead):
        az = rng.uniform(0, 2 * math.pi)
        h = np.array([math.cos(az), math.sin(az), 0.0])
        attach = top - axis * (0.45 + 0.12 * j) * bulb + h * 0.16 * bulb
        frond(mb, rng, attach, az, rng.uniform(-1.45, -1.2), rng.uniform(3.8, 4.8), 1.0, dead=True,
              bend=0.15, n_side=int(n_side * 0.8), width_scale=width_scale)
    for b in range(bunches):
        az = az0 + (b + 0.5) * 2 * math.pi / bunches + rng.normal(0, 0.25)
        h = np.array([math.cos(az), math.sin(az), 0.0])
        base = top - axis * (0.05 + 0.35 * b / max(1, bunches)) * bulb + h * 0.2 * bulb
        d = norm(h * 0.8 - UP * 0.6)
        ln = rng.uniform(0.35, 0.65)
        end = base + d * ln
        add_tube(mb, spline([base, base + d * ln * 0.5 + UP * 0.05, end], 5), np.linspace(0.03, 0.018, 5), 6,
                 FIBER, np.array([1.1, 0.95, 0.6, 0.2], np.float32), 0.5, 0.5)
        m = b / max(1, bunches - 1)
        cnt = int(rng.integers(4, 12))
        offs = rng.normal(0, 0.15, (cnt, 3))
        offs[:, 2] = -np.abs(offs[:, 2]) - 0.08
        offs += d * 0.1
        for _ in range(6):  # relax overlaps
            dd = offs[:, None, :] - offs[None, :, :]
            dist = np.linalg.norm(dd, axis=-1) + np.eye(cnt)
            push = np.clip(0.22 - dist, 0, None)[..., None] * dd / dist[..., None]
            offs += push.sum(1) * 0.5
        for o in offs:
            M = align_z(norm(o + d * 0.05)) @ rot_axis(UP, rng.uniform(0, 6.28))
            stamp(mb, NUT_MESH[0] * rng.uniform(0.9, 1.1), NUT_MESH[1], M, end + o, NUT_MESH[2], nut_color(rng, m), NUTM)
    return top


# ---------------------------------------------------------------- variants

def _angles(n, fn):
    u = np.linspace(0, 1, n)
    return u, fn(u)


def build(kind, seed, **kw):
    """Return a MeshBuilder for one palm form."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    n = 48
    az = np.full(n, 0.0)
    if kind == "straight":
        H = kw.get("height", 18.0)
        u, a = _angles(n, lambda u: 0.03 + 0.05 * u + 0.02 * np.sin(u * 5 + seed))
        az = az + rng.normal(0, 0.02) + 0.3 * u
        C = path_from_angles([0, 0, 0], a, az, H)
    elif kind == "leaning":
        H = kw.get("height", 16.0)
        lean = kw.get("lean", 0.42)
        u, a = _angles(n, lambda u: lean - 0.22 * np.clip((u - 0.55) / 0.45, 0, 1) ** 1.5)
        C = path_from_angles([0, 0, 0], a, az + 0.15 * u, H)
    elif kind == "sabre":  # beach palm: strong lean at the base, rising to vertical
        H = kw.get("height", 11.0)
        a0 = kw.get("lean", 1.05)
        u, a = _angles(n, lambda u: a0 * (1 - u ** 0.9) + 0.06)
        C = path_from_angles([0, 0, 0], a, az + 0.1 * np.sin(u * 3), H)
    elif kind == "scurve":
        H = kw.get("height", 14.0)
        u, a = _angles(n, lambda u: 0.32 * np.sin(2 * math.pi * u * 0.85 + 0.35))
        C = path_from_angles([0, 0, 0], np.abs(a), np.where(a >= 0, 0.0, math.pi) + 0.2 * u, H)
    elif kind == "fallen":  # knocked down by a cyclone, still alive: crown turned up
        Lf = kw.get("lying", 6.0)
        Lup = kw.get("rise", 6.5)
        H = Lf + 2.2 + Lup
        u, a = _angles(n, lambda u: np.where(u * H < Lf, 1.53,
                                             1.53 - 1.47 * np.clip((u * H - Lf) / 2.2, 0, 1) ** 0.8))
        C = path_from_angles([0, 0, 0.2], a, az + 0.05 * np.sin(u * 7), H)
    elif kind == "young":
        H = kw.get("height", 2.5)
        u, a = _angles(n, lambda u: 0.05 + 0.04 * u)
        C = path_from_angles([0, 0, 0], a, az, H)
    elif kind == "snag":
        H = kw.get("height", 9.0)
        u, a = _angles(n, lambda u: 0.06 + 0.08 * u)
        C = path_from_angles([0, 0, 0], a, az, H)
    elif kind in ("juvenile", "sprout"):
        C = None
    else:
        raise ValueError(kind)

    if kind == "snag":
        bleach = np.array([1.12, 1.10, 1.06, 0.0], np.float32)
        trunk(mb, rng, C, 0.17, 0.11, 0.12, bleach)
        roots(mb, rng, C[0], 0.3, 26)
        # jagged broken top: splinters
        top = C[-1]
        for _ in range(7):
            azs = rng.uniform(0, 2 * math.pi)
            o = np.array([math.cos(azs), math.sin(azs), 0.0]) * 0.07
            add_tube(mb, [top + o - UP * 0.1, top + o * 1.2 + UP * rng.uniform(0.1, 0.45)],
                     np.array([0.035, 0.004]), 4, BARK, bleach, 0.2, 0.5)
        return mb

    if kind == "juvenile":
        small = kw.get("small", False)
        top = np.array([0.0, 0.0, 0.12])
        crown(mb, rng, top, np.array([0.05, 0.0, 1.0]), n_green=8 if small else 11, n_dead=1 if small else 2,
              length=(1.5, 2.3) if small else (2.6, 3.8), bunches=0, n_side=46 if small else 60,
              leaflet_scale=0.75, bulb=0.45 if small else 0.6)
        if not small:  # the seed nut, still visible at the base
            M = align_z(norm(np.array([1.0, 0.2, 0.15])))
            stamp(mb, NUT_MESH[0] * 1.25, NUT_MESH[1], M, np.array([0.18, 0.05, 0.04]), NUT_MESH[2],
                  np.array([0.075, 0.042, 0.018, 1.0], np.float32), NUTM)
        return mb

    if kind == "sprout":
        n_leaves = kw.get("leaves", 3)
        M = align_z(norm(np.array([1.0, 0.0, 0.08])))
        stamp(mb, NUT_MESH[0] * 1.4, NUT_MESH[1], M, np.array([0.0, 0.0, 0.085]), NUT_MESH[2],
              np.array([0.075, 0.042, 0.018, 1.0], np.float32), NUTM)
        base = np.array([0.15, 0.0, 0.14])
        add_tube(mb, [base - UP * 0.03, base + UP * 0.10], np.array([0.022, 0.015]), 8, FIBER,
                 np.array([1.1, 0.95, 0.75, 0.2], np.float32), 1.0, 0.3)
        N = n_leaves
        Sx = 8
        x = np.linspace(0, 1, Sx + 1)
        C, U, W, cols = [], [], [], []
        for j in range(N):
            azl = j * GOLDEN + rng.uniform(-0.3, 0.3)
            tilt = 0.15 + 0.22 * j + rng.uniform(0, 0.1)
            e = np.array([math.cos(azl), math.sin(azl), 0.0])
            D = norm(UP * math.cos(tilt) + e * math.sin(tilt))
            Ll = rng.uniform(0.45, 0.95) * (0.65 + 0.35 * j / max(1, N - 1))
            c = base + UP * 0.08 + D[None, :] * (x * Ll)[:, None] - UP * 0.25 * Ll * x[:, None] ** 2 * math.sin(tilt) * 2
            C.append(c)
            U.append(np.broadcast_to(norm(np.cross(D, np.cross(UP, e) + 1e-6)), c.shape))
            W.append(0.075 * (0.8 + 0.4 * j / max(1, N)) * leaf_profile("lanceolate", x))
            cols.append([1.0, 1.0, 1.0, 0.0])
        add_strips(mb, np.array(C), np.array(U), np.array(W), np.array(W) * 0.15, np.array(cols, np.float32),
                   EOPHYLL)
        return mb

    # standard palms with a trunk and crown
    r_base, r_top, bole = {"young": (0.24, 0.20, 0.18), "fallen": (0.17, 0.14, 0.10)}.get(kind, (0.17, 0.135, 0.13))
    trunk(mb, rng, C, r_base, r_top, bole)
    if kind == "fallen":
        roots(mb, rng, C[0], 0.25, 40, plate_dir=norm(np.array([-1.0, 0.0, 0.25])))
    else:
        roots(mb, rng, C[0], r_base + bole, 34)
    T_top = norm(C[-1] - C[-3])
    if kind == "young":
        crown(mb, rng, C[-1], T_top, n_green=24, n_dead=2, length=(4.8, 6.0), bunches=2, n_side=78)
    else:
        crown(mb, rng, C[-1], T_top, n_green=kw.get("fronds", 28), n_dead=kw.get("dead", 3),
              length=kw.get("frond_len", (4.6, 5.6)), bunches=kw.get("bunches", 6), n_side=kw.get("n_side", 84))
    return mb


VARIANTS = {
    "Straight_A": dict(kind="straight", seed=11, height=18.5),
    "Straight_B": dict(kind="straight", seed=12, height=15.0, fronds=24, bunches=4),
    "Leaning_A": dict(kind="leaning", seed=21, height=16.0, lean=0.45),
    "Leaning_B": dict(kind="leaning", seed=22, height=12.5, lean=0.30, bunches=5),
    "Sabre_A": dict(kind="sabre", seed=31, height=12.0, lean=1.05),
    "Sabre_B": dict(kind="sabre", seed=32, height=8.5, lean=1.25, fronds=24),
    "SCurve_A": dict(kind="scurve", seed=41, height=14.5),
    "Fallen_A": dict(kind="fallen", seed=51, lying=6.0, rise=6.0, bunches=3),
    "Young_A": dict(kind="young", seed=61, height=1.6),
    "Young_B": dict(kind="young", seed=62, height=3.6),
    "Juvenile_A": dict(kind="juvenile", seed=71),
    "Juvenile_B": dict(kind="juvenile", seed=72, small=True),
    "Sprout_A": dict(kind="sprout", seed=81, leaves=2),
    "Sprout_B": dict(kind="sprout", seed=82, leaves=4),
    "DeadSnag_A": dict(kind="snag", seed=91, height=9.0),
}

LABELS_FR = {
    "Straight_A": "Cocotier droit", "Straight_B": "Cocotier droit (bas)", "Leaning_A": "Penché",
    "Leaning_B": "Penché léger", "Sabre_A": "Sabre (bord de lagon)", "Sabre_B": "Sabre bas",
    "SCurve_A": "Tronc en S", "Fallen_A": "Couché redressé", "Young_A": "Jeune tronc court",
    "Young_B": "Jeune", "Juvenile_A": "Juvénile sans tronc", "Juvenile_B": "Juvénile petit",
    "Sprout_A": "Coco germé", "Sprout_B": "Coco germé (4 f.)", "DeadSnag_A": "Chicot mort",
}


def lod_palm(seed, height=17.0):
    """Light far-distance palm (~10k verts): same materials and look, far fewer and wider leaflets."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    lean = rng.uniform(0.02, 0.35)
    u = np.linspace(0, 1, 16)
    C = path_from_angles([0, 0, 0], lean * (1 - 0.5 * u), np.zeros(16), height, 16)
    add_tube(mb, C, np.linspace(0.19, 0.13, 16), 7, BARK, None, 1.0, 1.0, cap=True)
    crown(mb, rng, C[-1], norm(C[-1] - C[-3]), n_green=16, n_dead=1, length=(4.4, 5.4), bunches=2, n_side=22,
          width_scale=2.2, bulb=0.9)
    return mb
