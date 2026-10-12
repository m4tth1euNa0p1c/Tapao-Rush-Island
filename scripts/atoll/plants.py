"""Atoll strand vegetation: pandanus (fara), naupaka (Scaevola taccada), tahinu (Heliotropium
foertherianum), miki-miki (Pemphis acidula), fern (Phymatosorus), grass tufts, beach morning
glory (Ipomoea pes-caprae).

Object space: +Z up, base at origin. Material slots: 0 bark/stem, 1 leaf, 2 fruit/flower.
"""
import math

import numpy as np

from .geo import (GOLDEN, UP, MeshBuilder, Noise, add_strips, add_tube, align_z, icosphere, integrate_path,
                  leaf_profile, norm, perp_basis, rot_axis, spline, stamp, uv_sphere, wind_phase)

BARK, LEAF, EXTRA = 0, 1, 2
_BERRY = uv_sphere(8, 6)


def leaves(mb, bases, dirs, normals, lengths, half_w, shape, S=4, droop=0.1, fold=-0.12, cols=None, mat=LEAF,
           wind=None):
    """Batch of simple leaves (strips) with a width profile, gravity droop and a V fold."""
    N = len(bases)
    if N == 0:
        return
    x = np.linspace(0, 1, S + 1)
    droop = np.broadcast_to(np.asarray(droop, np.float64), (N,))
    C = (bases[:, None, :] + dirs[:, None, :] * (x[None, :] * lengths[:, None])[..., None]
         - UP * (droop * lengths)[:, None, None] * (x ** 2)[None, :, None])
    W = half_w[:, None] * leaf_profile(shape, x)[None, :]
    add_strips(mb, C, np.broadcast_to(normals[:, None, :], C.shape), W, W * fold, cols, mat, wind=wind)


def face_up(dirs, fallback):
    n = UP - dirs * (dirs @ UP)[:, None]
    bad = np.linalg.norm(n, axis=1) < 0.15
    n[bad] = fallback[bad]
    return norm(n)


def rosette(rng, tip, D, n, length, ratio, tilt=(0.35, 1.2), spread=0.12, up_bias=0.35, droop=0.1):
    E1, E2 = perp_basis(D)
    j = np.arange(n)
    a = j / max(1, n - 1)
    psi = j * GOLDEN + rng.uniform(0, 6.28)
    theta = tilt[0] + (tilt[1] - tilt[0]) * a + rng.normal(0, 0.08, n)
    radial = np.cos(psi)[:, None] * E1 + np.sin(psi)[:, None] * E2
    dirs = norm(D * np.cos(theta)[:, None] + radial * np.sin(theta)[:, None] + UP * up_bias)
    bases = tip - D * (spread * a)[:, None]
    normals = face_up(dirs, radial)
    lens = length * rng.uniform(0.75, 1.15, n) * (0.55 + 0.45 * np.minimum(1, a * 4))
    return bases, dirs, normals, lens, lens * ratio * 0.5, np.full(n, droop) * rng.uniform(0.6, 1.4, n)


def leaf_cluster(rng, tip, D, n, length, ratio, radius):
    """Small leaves scattered in a ball around a twig end (dense small-leaved shrubs)."""
    offs = norm(rng.normal(0, 1, (n, 3)) + D * 0.6) * (rng.uniform(0.15, 1.0, (n, 1)) ** 0.5) * radius
    bases = tip + offs
    dirs = norm(offs / radius + rng.normal(0, 0.5, (n, 3)) + UP * 0.35)
    E1, _ = perp_basis(dirs)
    normals = face_up(dirs, E1)
    lens = rng.uniform(*length, n)
    return bases, dirs, normals, lens, lens * ratio * 0.5, np.zeros(n)


def leaf_colors(rng, n, var=0.08, yellow=0.04, dry_frac=0.0):
    tint = 1 + var * rng.normal(0, 1, (n, 1)) * np.ones((1, 3))
    y = rng.random(n) < yellow
    tint[y] = tint[y] * np.array([1.55, 1.25, 0.45])
    dry = np.where(y, 0.25, 0.0) + (rng.random(n) < dry_frac) * 0.9
    return np.concatenate([tint, np.clip(dry, 0, 1)[:, None]], 1).astype(np.float32)


# ---------------------------------------------------------------- broadleaf shrubs

SHRUBS = {
    "naupaka": dict(H=(1.4, 2.4), Wd=(2.6, 3.8), tips=(230, 300), main=(7, 11), env="mound", trunk_h=None,
                    stem_r=(0.035, 0.009), leaves=(11, 16), leaf_len=(0.13, 0.22), ratio=0.42, shape="spatulate",
                    tilt=(0.3, 1.15), droop=0.08, fold=-0.15, var=0.08, yellow=0.04, flowers=0.3, twist=0.05,
                    leaf_S=4, up_bias=0.45, spread=0.10, inner=0.5),
    "tahinu": dict(H=(2.8, 4.0), Wd=(4.2, 6.0), tips=(320, 420), main=(4, 6), env="dome", trunk_h=(0.3, 0.7),
                   stem_r=(0.12, 0.011), leaves=(13, 18), leaf_len=(0.15, 0.24), ratio=0.36, shape="obovate",
                   tilt=(0.35, 1.25), droop=0.12, fold=-0.1, var=0.06, yellow=0.03, flowers=0.2, twist=0.32,
                   leaf_S=4, up_bias=0.3, spread=0.10, inner=0.4),
    "mikimiki": dict(H=(1.6, 2.6), Wd=(2.6, 4.0), tips=(300, 380), main=(6, 9), env="windswept", trunk_h=None,
                     stem_r=(0.055, 0.006), leaves=(30, 40), leaf_len=(0.03, 0.045), ratio=0.45, shape="elliptic",
                     tilt=(0.6, 1.35), droop=0.0, fold=-0.1, var=0.08, yellow=0.02, flowers=0.0, twist=0.35,
                     leaf_S=2, up_bias=0.2, spread=0.22, inner=0.3, groups=(4, 8), cards=(5, 8),
                     card_size=(0.30, 0.48)),
}


def _wiggle(rng, pts, amp):
    pts = np.array(pts, np.float64)
    if amp > 0 and len(pts) > 2:
        seg = np.linalg.norm(pts[-1] - pts[0])
        pts[1:-1] += rng.normal(0, amp * seg * 0.18, (len(pts) - 2, 3))
    return pts


def shrub(kind, seed):
    p = SHRUBS[kind]
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    H = rng.uniform(*p["H"])
    Wd = rng.uniform(*p["Wd"])
    Dp = Wd * rng.uniform(0.75, 1.0)
    nt = int(rng.uniform(*p["tips"]))

    phi = rng.uniform(0, 2 * math.pi, nt)
    if p["env"] == "dome":
        th = np.arccos(rng.uniform(math.cos(1.65), 1, nt))
        center, radii = np.array([0, 0, H * 0.5]), np.array([Wd / 2, Dp / 2, H * 0.5])
    else:
        th = np.arccos(rng.uniform(math.cos(1.38), 1, nt))
        center, radii = np.zeros(3), np.array([Wd / 2, Dp / 2, H])
    rf = rng.uniform(p["inner"], 1.0, nt) ** 0.5
    dirv = np.stack([np.sin(th) * np.cos(phi), np.sin(th) * np.sin(phi), np.cos(th)], -1)
    rf *= 0.72 + 0.56 * Noise(seed).fbm3(dirv * 1.6 + 3.0, 3)  # lumpy, irregular canopy
    tips = center + dirv * radii * rf[:, None]
    if p["env"] == "windswept":  # sheared by the trade wind: low on the windward (-X) side
        xn = np.clip(tips[:, 0] / (Wd / 2), -1, 1)
        tips[:, 2] *= 0.45 + 0.55 * (xn + 1) / 2
        tips[:, 0] += 0.15 * Wd * (tips[:, 2] / H)
    tips[:, 2] = np.maximum(tips[:, 2], 0.12)

    k = int(rng.integers(*p["main"]))
    a_ang = np.linspace(0, 2 * math.pi, k, endpoint=False) + rng.uniform(0, 6.28)
    owner = np.argmin(np.abs(((np.arctan2(tips[:, 1], tips[:, 0])[:, None] - a_ang[None, :]) + math.pi)
                             % (2 * math.pi) - math.pi), axis=1)
    r0, r1 = p["stem_r"]
    bcol = np.array([1, 1, 1, 0], np.float32)
    if p["trunk_h"]:
        th_ = rng.uniform(*p["trunk_h"])
        tpts = _wiggle(rng, [[0, 0, 0], [0.05, 0.02, th_ * 0.5], [0.1, -0.03, th_]], p["twist"])
        add_tube(mb, spline(tpts, 8), np.linspace(r0 * 1.15, r0 * 0.85, 8), 10, BARK, bcol, 1.0, 1.0)
        fork = tpts[-1]
    all_b, all_d, all_n, all_l, all_w, all_dr, all_wind = [], [], [], [], [], [], []
    cards = []
    crown_c = np.array([0.0, 0.0, H * 0.3])
    for ai in range(k):
        mine = tips[owner == ai]
        if len(mine) == 0:
            continue
        out = np.array([math.cos(a_ang[ai]), math.sin(a_ang[ai]), 0.0])
        if p["trunk_h"]:
            base = fork
            A = fork + out * Wd * 0.26 + UP * (H - fork[2]) * 0.25
            rr = r0 * 0.7
        else:
            base = np.array([*(rng.normal(0, 0.08, 2)), 0.0])
            A = base + out * radii[0] * 0.3 + UP * H * 0.3
            rr = r0
        if p["env"] == "windswept":
            A[2] *= 0.6 + 0.4 * (out[0] + 1) / 2
        mpts = _wiggle(rng, [base, base + (A - base) * 0.5 + out * 0.08 * H, A], p["twist"])
        add_tube(mb, spline(mpts, 8), np.linspace(rr, rr * 0.6, 8), 7, BARK, bcol, 0.5, 1.0)
        order = np.argsort(np.arctan2(mine[:, 1] - A[1], mine[:, 0] - A[0]))
        groups = np.array_split(mine[order], int(rng.integers(*p.get("groups", (2, 5)))))
        for g in groups:
            if len(g) == 0:
                continue
            node = A * 0.45 + g.mean(0) * 0.55 + rng.normal(0, 0.03 * H, 3)
            spts = _wiggle(rng, [A, (A + node) / 2 + UP * 0.03 * H, node], p["twist"])
            # wind: a secondary branch sways from its fork, its twigs and leaves with it (one phase per twig)
            mb.wind_group = (A, 2.5 * np.linalg.norm(node - A) + 0.3, wind_phase(*node))
            add_tube(mb, spline(spts, 6), np.linspace(rr * 0.55, rr * 0.35, 6), 6, BARK, bcol, 0.4, 1.0)
            for tip in g:
                seg = np.linalg.norm(tip - node)
                tw = _wiggle(rng, [node, node + (tip - node) * 0.5 + UP * 0.12 * seg, tip], p["twist"])
                C = spline(tw, 5)
                twig_wind = (node, seg + 0.35, wind_phase(*tip))
                mb.wind_group = twig_wind
                add_tube(mb, C, np.linspace(max(r1, rr * 0.3), r1 * 0.7, 5), 4, BARK, bcol, 0.2, 1.0)
                D = norm(C[-1] - C[-2])
                if p.get("cards"):
                    cards.append(_cards(rng, tip, D, crown_c, int(rng.integers(*p["cards"])), p["card_size"])
                                 + (twig_wind,))
                    mb.wind_group = None
                    continue
                n_l = int(rng.integers(*p["leaves"]))
                if p.get("cluster"):
                    b, d, nrm, ln, hw, dr = leaf_cluster(rng, tip, D, n_l, p["leaf_len"], p["ratio"], p["cluster"])
                else:
                    b, d, nrm, ln, hw, dr = rosette(rng, tip, D, n_l, rng.uniform(*p["leaf_len"]), p["ratio"],
                                                    p["tilt"], p["spread"] * (H / 2), p["up_bias"],
                                                    p["droop"])
                all_wind.append((np.repeat(node[None], len(b), 0), np.full(len(b), seg + 0.35),
                                 np.full(len(b), twig_wind[2])))
                all_b.append(b)
                all_d.append(d)
                all_n.append(nrm)
                all_l.append(ln)
                all_w.append(hw)
                all_dr.append(dr)
                if p["flowers"] and rng.random() < p["flowers"]:
                    _flowers(mb, rng, kind, tip, D)
                mb.wind_group = None
    if cards:
        C = np.concatenate([c[0] for c in cards])
        W = np.concatenate([c[2] for c in cards])
        tint = 1 + p["var"] * rng.normal(0, 1, (len(C), 1)) * np.ones((1, 3))
        cols = np.concatenate([tint, np.zeros((len(C), 1))], 1).astype(np.float32)
        wind = (np.concatenate([np.repeat(c[3][0][None], len(c[0]), 0) for c in cards]),
                np.concatenate([np.full(len(c[0]), c[3][1]) for c in cards]),
                np.concatenate([np.full(len(c[0]), c[3][2]) for c in cards]))
        add_strips(mb, C, np.concatenate([c[1] for c in cards]), W, 0.06 * W, cols, LEAF, wind=wind)
        return mb
    B = np.concatenate(all_b)
    leaves(mb, B, np.concatenate(all_d), np.concatenate(all_n), np.concatenate(all_l), np.concatenate(all_w),
           p["shape"], p["leaf_S"], np.concatenate(all_dr), p["fold"], leaf_colors(rng, len(B), p["var"], p["yellow"]),
           wind=tuple(np.concatenate([w[k] for w in all_wind]) for k in range(3)))
    return mb


def _cards(rng, tip, D, center, n, size):
    """n crossed leafy-twig alpha cards growing outward from a twig end."""
    out = norm(tip - center)
    v = norm(out[None] * 0.6 + D[None] * 0.4 + rng.normal(0, 0.45, (n, 3)) + UP * 0.25)
    E1, E2 = perp_basis(v)
    a = rng.uniform(0, 2 * math.pi, n)[:, None]
    nrm = E1 * np.cos(a) + E2 * np.sin(a)
    sz = rng.uniform(*size, n)
    base = tip + rng.normal(0, 0.05, (n, 3)) - v * (sz * 0.25)[:, None]
    C = np.stack([base, base + v * sz[:, None]], 1)
    U = np.stack([nrm, nrm], 1)
    W = np.stack([sz / 2, sz / 2], 1)
    return C, U, W


def _flowers(mb, rng, kind, tip, D):
    E1, E2 = perp_basis(D)
    if kind == "naupaka":  # white half-flowers and white berries in the leaf axils
        for _ in range(int(rng.integers(1, 3))):
            a = rng.uniform(0, 6.28)
            radial = E1 * math.cos(a) + E2 * math.sin(a)
            c = tip - D * rng.uniform(0.02, 0.05) + radial * 0.03 + UP * 0.01
            if rng.random() < 0.5:
                n = 5
                ang = np.linspace(-1.3, 1.3, n)
                dirs = norm(np.array([radial * math.cos(t) + UP * math.sin(t) * 0.6 + D * 0.3 for t in ang]))
                leaves(mb, np.repeat(c[None], n, 0), dirs, face_up(dirs, np.repeat(radial[None], n, 0)),
                       np.full(n, 0.013), np.full(n, 0.0045), "obovate", 2, 0.0, 0.0,
                       np.tile([0.85, 0.84, 0.78, 0.0], (n, 1)).astype(np.float32), EXTRA)
            else:
                for _b in range(int(rng.integers(2, 5))):
                    stamp(mb, _BERRY[0] * 0.006, _BERRY[1], np.eye(3), c + rng.normal(0, 0.008, 3), _BERRY[2],
                          np.array([0.92, 0.92, 0.88, 0.0], np.float32), EXTRA)
    else:  # tahinu: small white scorpioid cymes
        c = tip + D * 0.03 + UP * 0.02
        for _b in range(8):
            stamp(mb, _BERRY[0] * 0.0045, _BERRY[1], np.eye(3), c + rng.normal(0, 0.018, 3), _BERRY[2],
                  np.array([0.95, 0.95, 0.9, 0.0], np.float32), EXTRA)


# ---------------------------------------------------------------- pandanus

def pandanus(seed, young=False):
    """Pandanus tectorius: stilt roots, candelabra branching, spiral tufts of long strap leaves."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    bcol = np.array([1, 1, 1, 0], np.float32)
    h1 = rng.uniform(1.3, 2.0) if young else rng.uniform(2.4, 3.4)
    lean = rng.uniform(0.0, 0.22)
    la = rng.uniform(0, 6.28)
    lv = np.array([math.cos(la), math.sin(la), 0.0])
    tp = [[0, 0, 0], lv * lean * h1 * 0.3 + UP * h1 * 0.45, lv * lean * h1 + UP * h1]
    C = spline(tp, 14)
    r_tr = 0.10 if young else 0.14
    add_tube(mb, C, np.linspace(r_tr, r_tr * 0.82, 14), 12, BARK, bcol, 1.0, 1.0)
    mb.trunks = [(C, np.linspace(r_tr, r_tr * 0.82, 14))]  # axis + radii, used for the UCX collision hulls
    # prop (stilt) roots
    nr = int(rng.integers(3, 6)) if young else int(rng.integers(8, 14))
    for _ in range(nr):
        z0 = rng.uniform(0.25, 0.7 if young else 1.8) ** 1.0
        i = int(np.clip(z0 / h1 * 13, 0, 13))
        az = rng.uniform(0, 6.28)
        out = np.array([math.cos(az), math.sin(az), 0.0])
        a = C[i] + out * r_tr * 0.8
        g = np.array([a[0], a[1], -0.1]) + out * rng.uniform(0.45, 1.2) * (0.4 + z0 / 1.8)
        mid = (a + g) / 2 + out * 0.08 + UP * 0.05
        add_tube(mb, spline([a, mid, g], 8), np.linspace(rng.uniform(0.035, 0.055), 0.03, 8), 7, BARK,
                 np.array([1.08, 1.0, 0.9, 0.0], np.float32), 0.5, 1.0)
    tufts = []

    def branch(P, D, depth, r):
        if depth >= (0 if young else 3) or (depth > 1 and rng.random() < 0.15):
            tufts.append((P, D))
            return
        kb = 2 if rng.random() < 0.75 else 3
        base = rng.uniform(0, 6.28)
        E1, E2 = perp_basis(D)
        for j in range(kb):
            psi = base + j * 2 * math.pi / kb + rng.normal(0, 0.3)
            th = rng.uniform(0.6, 1.05)
            Dn = norm(D * math.cos(th) + (E1 * math.cos(psi) + E2 * math.sin(psi)) * math.sin(th) + UP * 0.1)
            Lb = rng.uniform(1.2, 2.2) * 0.72 ** depth
            pts = [P, P + Dn * Lb * 0.5, P + Dn * Lb + UP * 0.18 * Lb]
            Cb = spline(pts, 8)
            add_tube(mb, Cb, np.linspace(r, r * 0.7, 8), 9, BARK, bcol, 0.8, 1.0)
            branch(Cb[-1], norm(Cb[-1] - Cb[-3]), depth + 1, r * 0.7)

    branch(C[-1], norm(C[-1] - C[-3]), 0, r_tr * 0.8)
    Sx = 10
    x = np.linspace(0, 1, Sx + 1)
    Cs, Us, Ws, cols, winds = [], [], [], [], []
    for P, D in tufts:
        n = int(rng.integers(44, 64))
        winds.append((np.repeat(np.asarray(P)[None], n, 0), np.full(n, 1.6), np.full(n, wind_phase(*P))))
        E1, E2 = perp_basis(D)
        j = np.arange(n)
        a = j / (n - 1)
        psi = j * (2 * math.pi / 3 + 0.075) + rng.uniform(0, 6.28)
        radial = np.cos(psi)[:, None] * E1 + np.sin(psi)[:, None] * E2
        th = 0.12 + 1.45 * a ** 0.8 + rng.normal(0, 0.06, n)
        dl = norm(D * np.cos(th)[:, None] + radial * np.sin(th)[:, None])
        nrm = norm(-radial * np.cos(th)[:, None] + D * np.sin(th)[:, None])
        Ll = rng.uniform(0.9, 1.45, n) * (0.55 + 0.45 * np.minimum(1, a * 3))
        dead = a > 0.88
        dro = (0.15 + 1.1 * a ** 1.3) * rng.uniform(0.8, 1.2, n)
        dro[dead] = rng.uniform(1.6, 2.2, dead.sum())
        base = P - D * (0.28 * a)[:, None]
        Cl = (base[:, None, :] + dl[:, None, :] * (x[None, :] * Ll[:, None])[..., None]
              - UP * (dro * Ll)[:, None, None] * (x ** 2)[None, :, None])
        Cs.append(Cl)
        Us.append(np.broadcast_to(nrm[:, None, :], Cl.shape))
        Ws.append(rng.uniform(0.032, 0.042, n)[:, None] * leaf_profile("strap", x)[None, :])
        tint = 1 + 0.08 * rng.normal(0, 1, (n, 1)) * np.ones((1, 3))
        frac = 0.08 + 0.35 * a
        dry = np.clip((x[None, :] - (1 - frac[:, None])) / frac[:, None], 0, 1) * (0.3 + 0.7 * a[:, None])
        dry[dead] = 1.0
        cols.append(np.concatenate([np.broadcast_to(tint[:, None, :], (n, Sx + 1, 3)), dry[..., None]], -1))
        if not young and rng.random() < 0.35:  # hanging syncarp fruit
            s = P - D * 0.3
            e = s - UP * 0.35 + rng.normal(0, 0.05, 3)
            add_tube(mb, [s, e], np.array([0.02, 0.016]), 6, BARK, bcol, 0.3, 0.5)
            co, q, uv = _BERRY
            ph = np.arctan2(co[:, 1], co[:, 0])
            th_ = np.arccos(np.clip(co[:, 2], -1, 1))
            bump = 1 + 0.07 * np.abs(np.sin(6 * ph) * np.sin(5 * th_))
            m = rng.random()
            colf = np.array([0.35, 0.45, 0.12]) ** 2.2 * (1 - m) + np.array([0.88, 0.45, 0.10]) ** 2.2 * m
            stamp(mb, co * bump[:, None] * np.array([0.10, 0.10, 0.12]), q, np.eye(3), e - UP * 0.11, uv,
                  np.array([*colf, 0.2], np.float32), EXTRA)
    Ccat = np.concatenate(Cs)
    W = np.concatenate(Ws)
    add_strips(mb, Ccat, np.concatenate(Us), W, -0.35 * W, np.concatenate(cols).astype(np.float32), LEAF,
               wind=tuple(np.concatenate([w[k] for w in winds]) for k in range(3)))
    return mb


# ---------------------------------------------------------------- ground cover

def fern(seed):
    """Phymatosorus clump: arching pinnatifid fronds."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    nf = int(rng.integers(10, 17))
    B, Dd, Nn, Ll, Hw = [], [], [], [], []
    for _ in range(nf):
        o = np.array([*rng.normal(0, 0.12, 2), 0.0])
        az = rng.uniform(0, 6.28)
        h = np.array([math.cos(az), math.sin(az), 0.0])
        pitch = rng.uniform(0.55, 1.25)
        stipe, blade = rng.uniform(0.12, 0.25), rng.uniform(0.28, 0.5)
        total = stipe + blade
        S = 12
        t = np.linspace(0, 1, S + 1)
        ang = pitch - rng.uniform(0.7, 1.2) * t ** 1.5
        pos = integrate_path(o, norm(np.cos(ang)[:, None] * h + np.sin(ang)[:, None] * UP), total / S)
        add_tube(mb, pos, np.linspace(0.004, 0.0015, S + 1), 4, LEAF, np.array([0.95, 0.8, 0.6, 0.25], np.float32),
                 0.2, total)
        T = norm(np.gradient(pos, axis=0))
        nl = int(rng.integers(4, 8))
        tk = stipe / total + (blade / total) * np.linspace(0.06, 0.8, nl)
        for kk, tq in enumerate(tk):
            P = np.array([np.interp(tq, t, pos[:, c]) for c in range(3)])
            Tq = norm(np.array([np.interp(tq, t, T[:, c]) for c in range(3)]))
            Lb = norm(np.cross(Tq, UP))
            Nf = norm(np.cross(Lb, Tq))
            for sd in (-1, 1):
                d = norm(sd * Lb * math.cos(0.95) + Tq * math.sin(0.95) + Nf * 0.12)
                B.append(P)
                Dd.append(d)
                Nn.append(Nf)
                Ll.append(blade * 0.42 * (1 - 0.55 * kk / nl) * rng.uniform(0.85, 1.1))
                Hw.append(rng.uniform(0.012, 0.016))
        B.append(pos[-1])
        Dd.append(T[-1])
        Nn.append(norm(np.cross(np.cross(T[-1], UP), T[-1])))
        Ll.append(blade * 0.3)
        Hw.append(0.014)
    B = np.array(B)
    leaves(mb, B, np.array(Dd), norm(np.array(Nn)), np.array(Ll), np.array(Hw), "lanceolate", 4, 0.15, 0.1,
           leaf_colors(rng, len(B), 0.07, 0.03, 0.03))
    return mb


def grass(seed, n=(24, 40), height=(0.12, 0.38), dry=0.25):
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    nb = int(rng.integers(*n))
    rr = rng.uniform(0, 0.06, nb)
    az = rng.uniform(0, 6.28, nb)
    h = np.stack([np.cos(az), np.sin(az), np.zeros(nb)], -1)
    base = h * rr[:, None]
    tilt = 0.05 + 0.55 * (rr / 0.06) + rng.uniform(0, 0.15, nb)
    D = norm(h * np.sin(tilt)[:, None] + UP * np.cos(tilt)[:, None])
    side = np.cross(UP, h)
    nrm = norm(np.cross(D, side))
    L = rng.uniform(*height, nb)
    dro = tilt * 0.6 + rng.uniform(0, 0.25, nb)
    s = rng.random(nb)
    tint = np.stack([1 + 0.25 * s, 1 + 0.1 * s, 1 - 0.2 * s], -1) * rng.uniform(0.85, 1.1, (nb, 1))
    cols = np.concatenate([tint, rng.uniform(0, dry, (nb, 1))], 1).astype(np.float32)
    leaves(mb, base, D, nrm, L, rng.uniform(0.002, 0.0035, nb), "strap", 4, dro, -0.4, cols)
    return mb


def ipomoea(seed):
    """Beach morning glory patch: runners creeping on the sand, bilobed leaves, pink flowers."""
    rng = np.random.default_rng(seed)
    mb = MeshBuilder()
    B, Dd, Nn, Ll, Hw = [], [], [], [], []
    for _ in range(int(rng.integers(5, 10))):
        head = rng.uniform(0, 6.28)
        p = np.array([*rng.normal(0, 0.1, 2), 0.008])
        pts = [p.copy()]
        for _s in range(int(rng.uniform(1.2, 3.5) / 0.1)):
            head += rng.normal(0, 0.18)
            p = p + np.array([math.cos(head), math.sin(head), 0.0]) * 0.1
            pts.append(p.copy())
        pts = np.array(pts)
        add_tube(mb, pts, np.full(len(pts), 0.004), 4, LEAF, np.array([1.3, 0.85, 0.8, 0.15], np.float32), 0.2,
                 1.0)
        for i in range(1, len(pts) - 1):
            if rng.random() < 0.3:
                continue
            node = pts[i]
            sd = rng.choice([-1, 1])
            tdir = norm(pts[i + 1] - pts[i - 1])
            lat = np.cross(UP, tdir) * sd
            pe = norm(lat * 0.5 + UP * 0.85)
            pl = rng.uniform(0.05, 0.10)
            add_tube(mb, [node, node + pe * pl], np.array([0.0025, 0.002]), 3, LEAF,
                     np.array([1.2, 1.0, 0.8, 0.1], np.float32), 0.1, 0.2)
            el = rng.uniform(0.25, 0.75)
            dl = norm(np.cross(UP, np.array([0, 0, 1.0])) + lat * math.cos(el) + UP * math.sin(el) + tdir * 0.2)
            B.append(node + pe * pl)
            Dd.append(dl)
            Nn.append(norm(UP - dl * dl[2]))
            ln = rng.uniform(0.045, 0.075)
            Ll.append(ln)
            Hw.append(ln * 0.48)
            if rng.random() < 0.12:  # funnel flower
                fc = node + pe * pl * 0.6 + UP * 0.03
                ang = np.linspace(0, 2 * math.pi, 7)[:-1]
                fd = norm(np.stack([np.cos(ang), np.sin(ang), np.full(6, 0.9)], -1))
                leaves(mb, np.repeat(fc[None], 6, 0), fd, face_up(fd, fd), np.full(6, 0.03), np.full(6, 0.014),
                       "round", 2, 0.0, -0.2, np.tile([0.64, 0.07, 0.34, 0.0], (6, 1)).astype(np.float32), EXTRA)
    B = np.array(B)
    leaves(mb, B, np.array(Dd), norm(np.array(Nn)), np.array(Ll), np.array(Hw), "round", 4, 0.05, -0.45,
           leaf_colors(rng, len(B), 0.07, 0.05))
    return mb


# ---------------------------------------------------------------- leaves carried by the wind

WIND_LEAVES = {
    # name: (shape, length m, half width ratio, curl, tint rgb, dryness)
    "PalmLeaflet": ("leaflet", 0.30, 0.09, 0.10, (1.12, 1.06, 0.80), 0.25),
    "Naupaka": ("spatulate", 0.16, 0.24, 0.18, (1.0, 1.0, 1.0), 0.0),
    "Dry": ("obovate", 0.13, 0.30, 0.35, (1.0, 1.0, 1.0), 0.9),
}


def wind_leaf(kind):
    """One loose leaf for the flying-leaves effect (AWindLeavesActor), centred on its middle, a little curled.

    No wind channels: the effect moves the whole leaf, the foliage wind must not shake it again.
    """
    shape, length, ratio, curl, tint, dry = WIND_LEAVES[kind]
    mb = MeshBuilder()
    S = 6
    x = np.linspace(0, 1, S + 1)
    C = np.zeros((1, S + 1, 3))
    C[0, :, 0] = (x - 0.5) * length
    C[0, :, 2] = curl * length * (x - 0.5) ** 2  # curled up at both ends
    W = (length * ratio) * leaf_profile(shape, x)[None, :]
    col = np.array([[*tint, dry]], np.float32)
    add_strips(mb, C, np.broadcast_to(UP, C.shape), W, -0.18 * W, col, LEAF)
    mb.wind = [np.zeros_like(w) for w in mb.wind]
    return mb
