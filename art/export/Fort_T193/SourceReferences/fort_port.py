"""
Fort francais : port (lot 'port').

SM_Port_Quay_6m (quai de pierres de lest, couronnement de pierre de taille, anneaux), SM_Port_QuayStairs (escalier
parallele au parement jusqu'a l'eau), SM_Port_Jetty_6m (jetee maconnee 4 m), SM_Port_Pier_6m (ponton sur pieux de
cocotier), SM_Port_PierHead (musoir en T, debarcadere a marches, poteau de lanterne, potence a poulie),
SM_Port_Bollard (vieux canon enfonce, culasse en haut), SM_Port_Crane (grue a potence, fleche = enfant _Jib),
SM_Port_Warehouse (magasin portuaire, portail, lucarne et poulie de grenier).

Repere des modules de quai : Z 0 = dessus du quai (monde -0,33) ; lagon a Z local -1,32 ; fond a -2,64 au parement.
"""
import math
import random

import bmesh
from mathutils import Matrix, Vector

import fort_bldg as BL
import fort_geo as G
import fort_kit as FK
import fort_mat as FM
import fort_rampart as RP

V = G.V
TAU = 2 * math.pi
REND, RUB, ASH, BR, BAL = RP.REND, RP.RUB, RP.ASH, RP.BR, RP.BAL
WL = -1.32          # niveau du lagon en repere de quai
FOOT = -2.64        # fond au pied du parement
BAT = 0.1


def _quay_face(fb, M, a, b, rng, z0=FOOT, cope=True, cope_w=0.42, face=False):
    """Couronnement de pierre de taille (et parement talute 1/10 si face=True) de a vers b (exterieur a droite)."""
    o, u, n, L = G.seg_frame(a, b)
    fb.blend = BAL
    fb.base = WL
    off = (0 - z0) * BAT
    q0 = [o + n * off, o + u * L + n * off]
    q1 = [o, o + u * L]
    if face:
        fb.poly_bm([V(q0[0].x, q0[0].y, z0), V(q0[1].x, q0[1].y, z0), V(q1[1].x, q1[1].y, -0.2),
                    V(q1[0].x, q1[0].y, -0.2)], [(0, 1, 2, 3)], M["rubble"], recalc=False)
    if cope:
        fb.blend = ASH
        t = 0.0
        while t < L - 0.02:
            ln = min(rng.uniform(0.55, 0.9), L - t)
            c = o + u * (t + ln / 2) - n * (cope_w / 2 - 0.06)
            fb.box((c.x, c.y, -0.11), (ln - 0.008, cope_w, 0.22), M["ashlar"],
                   R=Matrix.Rotation(math.atan2(u.y, u.x), 3, "Z"), bevel=0.015)
            t += ln
    fb.base = 0.0


def _ring(fb, M, c, normal):
    n = Vector(normal).normalized()
    c = Vector(c)
    u = n.orthogonal().normalized()
    fb.cyl(c - n * 0.05, c + n * 0.03, 0.022, M["iron"], segs=8)
    pts = [c + n * 0.04 + V(0, 0, -0.07) + (u * math.cos(TAU * i / 12) * 0.06 + V(0, 0, 1) * math.sin(TAU * i / 12) * 0.06)
           for i in range(12)]
    fb.tube(pts, 0.012, M["iron"], segs=5, closed=True)


def build_quay(coll, p=None):
    p = {**FK.mod("quay_6m"), **(p or {})}
    M = FM.mats()
    rng = random.Random(401)
    fb = G.FB("Quay")
    W, D = p["size_m"]
    hx = W / 2
    off = -FOOT * BAT
    fb.blend = BAL
    fb.base = WL
    sec = [(-off, FOOT), (0.0, -0.2), (D, -0.2), (D, FOOT)]
    fb.loft([V(-hx, y, z) for y, z in sec], [V(hx, y, z) for y, z in sec], M["rubble"])
    fb.base = 0.0
    _quay_face(fb, M, (-hx, 0), (hx, 0), rng)
    G.slab_paving(fb, [(-hx, 0.36), (hx, 0.36), (hx, D), (-hx, D)], -0.2, M["ashlar"], rng, size=(0.45, 0.85), th=0.2,
                  blend=BR)
    fb.blend = RUB
    for x in (-1.5, 1.5):
        _ring(fb, M, (x, -0.05, -0.4), (0, -1, 0))
    ob = fb.finish(coll)
    ucx = [("quai", None, [V(x, y, z) for x in (-hx, hx) for y, z in sec[:1] + [(0, 0), (D, 0)] + sec[3:]])]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p)


def build_quay_stairs(coll, p=None):
    p = {**FK.mod("quay_stairs"), **(p or {})}
    M = FM.mats()
    rng = random.Random(402)
    fb = G.FB("QuayStairs")
    W, D = p["size_m"]
    hx = W / 2
    n, rise, tread, sw = p["risers"], p["rise_m"], p["tread_m"], p["width_m"]
    off = -FOOT * BAT
    x_top = hx - 0.4
    fb.blend = BAL
    fb.base = WL
    sec = [(sw, FOOT), (sw, -0.2), (D, -0.2), (D, FOOT)]
    fb.loft([V(-hx, y, z) for y, z in sec], [V(hx, y, z) for y, z in sec], M["rubble"])
    # bande de l'escalier : massif en gradins le long de X (descend vers -X)
    steps = []
    for k in range(1, n + 1):
        x1 = x_top - (k - 1) * tread
        x0 = x_top - k * tread if k < n else -hx
        steps.append((x0, x1, -k * rise))
    blocks = [(x_top, hx, 0.0)] + steps
    for x0, x1, zt in blocks:
        q0 = [V(x0, -off, FOOT), V(x1, -off, FOOT), V(x1, sw, FOOT), V(x0, sw, FOOT)]
        zz = zt - (0.06 if zt < 0 else 0.2)
        q1 = [V(x0, -(0 - zz) * BAT, zz), V(x1, -(0 - zz) * BAT, zz), V(x1, sw, zz), V(x0, sw, zz)]
        fb.loft(q0, q1, M["rubble"])
    fb.base = 0.0
    fb.blend = BR
    for x0, x1, zt in steps:
        fb.box(((x0 + x1) / 2 - 0.01, sw / 2 - 0.02, zt - 0.03), (x1 - x0 + 0.03, sw - 0.02, 0.06), M["ashlar"],
               bevel=0.012)
    # couronnements : face (bout haut) et arete du mur cote terre
    _quay_face(fb, M, (x_top, 0), (hx, 0), rng)
    fb.blend = ASH
    t = -hx
    while t < x_top - 0.02:
        ln = min(rng.uniform(0.55, 0.9), x_top - t)
        fb.box((t + ln / 2, sw + 0.15, -0.11), (ln - 0.008, 0.42, 0.22), M["ashlar"], bevel=0.015)
        t += ln
    G.slab_paving(fb, [(-hx, sw + 0.36), (hx, sw + 0.36), (hx, D), (-hx, D)], -0.2, M["ashlar"], rng,
                  size=(0.45, 0.85), th=0.2, blend=BR)
    G.slab_paving(fb, [(x_top, 0.36), (hx, 0.36), (hx, sw + 0.36), (x_top, sw + 0.36)], -0.2, M["ashlar"], rng,
                  size=(0.35, 0.6), th=0.2, blend=BR)
    fb.blend = RUB
    _ring(fb, M, (-hx + 0.6, sw - 0.02, -0.5), (0, -1, 0))
    ob = fb.finish(coll)
    ucx = [("terre", None, [V(x, y, z) for x in (-hx, hx) for y, z in sec]),
           ("marches", None, [V(x, y, z) for y in (0, sw) for x, z in ((x_top, 0.0), (x_top - n * tread, -n * rise),
                                                                       (x_top, -n * rise))]),
           ("palier", None, G.rbox_pts(((-hx + x_top - n * tread) / 2, sw / 2, (-n * rise + FOOT) / 2),
                                       (x_top - n * tread + hx, sw, -n * rise - FOOT)))]
    socks = FK.sockets_of(p["sockets"])
    return FK.res([ob], socks, ucx, p, info={"bottom_z_local": -n * rise})


def build_jetty(coll, p=None):
    p = {**FK.mod("jetty_6m"), **(p or {})}
    M = FM.mats()
    rng = random.Random(403)
    fb = G.FB("Jetty")
    W, L = p["size_m"]
    hx, hy = W / 2, L / 2
    off = -FOOT * BAT
    fb.blend = BAL
    fb.base = WL
    fb.loft([V(-hx - off, -hy, FOOT), V(hx + off, -hy, FOOT), V(hx, -hy, -0.2), V(-hx, -hy, -0.2)],
            [V(-hx - off, hy, FOOT), V(hx + off, hy, FOOT), V(hx, hy, -0.2), V(-hx, hy, -0.2)], M["rubble"])
    fb.base = 0.0
    _quay_face(fb, M, (hx, hy), (hx, -hy), rng)
    _quay_face(fb, M, (-hx, -hy), (-hx, hy), rng)
    G.slab_paving(fb, [(-hx + 0.36, -hy), (hx - 0.36, -hy), (hx - 0.36, hy), (-hx + 0.36, hy)], -0.2, M["ashlar"], rng,
                  size=(0.45, 0.85), th=0.2, blend=BR, angle=math.pi / 2)
    fb.blend = RUB
    for sx in (-1, 1):
        _ring(fb, M, (sx * (hx + 0.05), 1.2, -0.4), (sx, 0, 0))
    ob = fb.finish(coll)
    ucx = [("jetee", None, [V(x, y, z) for y in (-hy, hy) for x, z in ((-hx - off, FOOT), (hx + off, FOOT),
                                                                      (hx, 0.0), (-hx, 0.0))])]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p)


def _piles(fb, M, xs, ys, z_top, z_bot, rng, r=0.125, mat="coconut"):
    fb.blend = RUB
    fb.base = WL
    out = []
    for x in xs:
        for y in ys:
            lean = V(rng.uniform(-0.03, 0.03), rng.uniform(-0.03, 0.03), 0)
            fb.tube([V(x, y, z_bot) + lean, V(x, y, (z_top + z_bot) / 2) + lean * 0.5, V(x, y, z_top)],
                    [r * 1.05, r, r * 0.95], M[mat], segs=10, rng=rng, wobble=0.04)
            out.append(V(x, y, 0))
    fb.base = 0.0
    return out


def _deck(fb, M, x0, x1, y0, y1, z, rng, mat="oak_light", across=True):
    fb.blend = RUB
    if across:
        y = y0
        while y < y1 - 0.02:
            w = min(rng.uniform(0.2, 0.25), y1 - y)
            if rng.random() > 0.015:
                fb.wood((rng.uniform(-0.02, 0.02) + (x0 + x1) / 2, y + w / 2, z - 0.03 + rng.uniform(-0.006, 0.004)),
                        (x1 - x0 + rng.uniform(-0.06, 0.04), w - 0.016, 0.06), M[mat],
                        R=Matrix.Rotation(rng.uniform(-0.01, 0.01), 3, "Z"), bevel=0.006, long_u=True)
            y += w
    else:
        x = x0
        while x < x1 - 0.02:
            w = min(rng.uniform(0.2, 0.25), x1 - x)
            fb.wood((x + w / 2, (y0 + y1) / 2 + rng.uniform(-0.02, 0.02), z - 0.03), (w - 0.016, y1 - y0, 0.06),
                    M[mat], bevel=0.006, long_u=False)
            x += w


def build_pier(coll, p=None):
    p = {**FK.mod("pier_6m"), **(p or {})}
    M = FM.mats()
    rng = random.Random(404)
    fb = G.FB("Pier", world_uv=False)
    W, L = p["size_m"]
    hx, hy = W / 2, L / 2
    zb = p["piles_to_z_scene"] + 0.33
    xs = (-1.05, 0.0, 1.05)
    ys = (-2.0, 0.0, 2.0)
    _piles(fb, M, xs, ys, -0.36, zb, rng)
    fb.blend = RUB
    for y in ys:
        fb.wood((0, y, -0.38), (W + 0.4, 0.2, 0.2), M["tamanu"], bevel=0.01)
        for sx in (-1, 1):
            G.beam(fb, (sx * 1.05, y, -0.5), (sx * 0.0, y, -1.7), (0.1, 0.1), M["tamanu"])
    for x in xs:
        fb.wood((x, 0, -0.18), (0.14, L, 0.2), M["tamanu"], bevel=0.01, long_u=False)
    for x in (-1.05, 1.05):
        G.beam(fb, (x, -2.0, -0.5), (x, 0.0, -2.0), (0.08, 0.08), M["tamanu"])
        G.beam(fb, (x, 0.0, -0.5), (x, 2.0, -2.0), (0.08, 0.08), M["tamanu"])
    _deck(fb, M, -hx, hx, -hy, hy, 0.0, rng)
    for x, y in ((-1.05, -2.0), (1.05, 2.0)):
        for k in range(3):
            ring = [V(x + 0.14 * math.cos(TAU * i / 10), y + 0.14 * math.sin(TAU * i / 10), -0.7 - 0.03 * k)
                    for i in range(10)]
            fb.tube(ring, 0.018, M["rope"], segs=5, closed=True)
    ob = fb.finish(coll)
    ucx = [("tablier", None, G.rbox_pts((0, 0, -0.15), (W, L, 0.3)))]
    for x in xs:
        for y in ys:
            ucx.append(("pieu", None, [V(x + 0.13 * math.cos(TAU * k / 6), y + 0.13 * math.sin(TAU * k / 6), z)
                                       for k in range(6) for z in (zb, -0.3)]))
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p)


def build_pier_head(coll, p=None):
    p = {**FK.mod("pier_head"), **(p or {})}
    M = FM.mats()
    rng = random.Random(405)
    fb = G.FB("PierHead", world_uv=False)
    W, D = p["size_m"]
    hx, hy = W / 2, D / 2
    zb = -4.29
    xs = (-3.6, -1.8, 0.0, 1.8, 3.6)
    ys = (-1.6, 0.0, 1.6)
    _piles(fb, M, xs, ys, -0.36, zb, rng, r=0.14)
    fb.blend = RUB
    for y in ys:
        fb.wood((0, y, -0.38), (W + 0.3, 0.22, 0.22), M["tamanu"], bevel=0.01)
    for x in xs:
        fb.wood((x, 0, -0.17), (0.16, D, 0.2), M["tamanu"], bevel=0.01, long_u=False)
    _deck(fb, M, -hx, hx, -hy, hy, 0.0, rng, across=False)
    for x in xs:
        for y in (-1.6, 1.6):
            G.beam(fb, (x, y, -0.5), (x, 0.0, -2.0), (0.09, 0.09), M["tamanu"])
    # debarcadere : 8 marches de bois vers le sud (cote est), limons sur pieux
    n = p["landing"]["risers"]
    rise = 0.165
    tread = 0.28
    bx0, bx1 = 2.6, 3.8
    for k in range(1, n + 1):
        y = -hy - (k - 0.5) * tread
        fb.wood(((bx0 + bx1) / 2, y, -k * rise - 0.025), (bx1 - bx0 - 0.1, tread + 0.02, 0.05), M["oak_light"],
                bevel=0.005)
    for x in (bx0 + 0.05, bx1 - 0.05):
        G.beam(fb, (x, -hy + 0.05, -0.1), (x, -hy - n * tread, -n * rise - 0.15), (0.07, 0.22), M["tamanu"])
    _piles(fb, M, (bx0 + 0.05, bx1 - 0.05), (-hy - n * tread + 0.1,), -n * rise - 0.2, zb, rng, r=0.11)
    # poteau de lanterne (potence) au milieu du bord sud
    lx, ly, _ = p["sockets"]["Lantern"]
    fb.wood((lx, ly, 1.1), (0.14, 0.14, 2.3), M["tamanu"], bevel=0.01, long_u=False)
    fb.wood((lx, ly, 0.08), (0.34, 0.34, 0.16), M["tamanu"], bevel=0.01)
    # potence a poulie de charge (cote ouest)
    px, py = -3.2, -1.5
    fb.wood((px, py, 1.3), (0.16, 0.16, 2.6), M["tamanu"], bevel=0.01, long_u=False)
    G.beam(fb, (px, py, 2.5), (px, py - 1.2, 2.5), (0.12, 0.14), M["tamanu"])
    G.beam(fb, (px, py, 1.7), (px, py - 0.8, 2.45), (0.08, 0.08), M["tamanu"])
    fb.wood((px, py - 1.1, 2.3), (0.08, 0.14, 0.2), M["tamanu"], bevel=0.01)
    fb.tube([V(px, py - 1.12, 2.22), V(px, py - 1.12, 0.6)], 0.012, M["rope"], segs=5)
    fb.tube([V(px, py - 1.06, 2.22), V(px + 0.05, py - 0.3, 1.2), V(px + 0.1, py - 0.08, 0.9)], 0.012, M["rope"], segs=5)
    coil = [V(px + 0.5 + 0.18 * math.cos(TAU * i / 14), py + 0.4 + 0.18 * math.sin(TAU * i / 14), 0.03 + 0.012 * (i // 14))
            for i in range(42)]
    fb.tube(coil, 0.014, M["rope"], segs=5)
    ob = fb.finish(coll)
    ucx = [("tablier", None, G.rbox_pts((0, 0, -0.15), (W, D, 0.3))),
           ("marches", None, [V(x, y, z) for x in (bx0, bx1) for y, z in ((-hy, 0.0), (-hy - n * tread, -n * rise),
                                                                          (-hy, -n * rise))]),
           ("poteau", None, G.rbox_pts((lx, ly, 1.15), (0.16, 0.16, 2.3)))]
    socks = FK.sockets_of(p["sockets"])
    socks["LanternMount"] = ("root", V(lx, ly - 0.08, 2.05))
    return FK.res([ob], socks, ucx, p, socket_rot={"LanternMount": (0, 0, 180)})


def build_bollard(coll, p=None):
    p = {**FK.mod("bollard"), **(p or {})}
    M = FM.mats()
    fb = G.FB("Bollard", world_uv=False)
    h, d = p["height_m"], p["d_m"]
    r = d / 2
    from fort_rampart import _revolve
    prof = [(0.0, h + 0.06), (0.03, h + 0.065), (0.045, h + 0.045), (0.03, h + 0.02), (0.035, h), (r * 0.7, h - 0.01),
            (r * 0.95, h - 0.04), (r, h - 0.08), (r * 1.04, h - 0.12), (r * 1.04, h - 0.15), (r * 0.98, h - 0.17),
            (r * 0.93, 0.05), (r * 1.1, 0.02), (r * 1.1, -0.1)]
    fb.add_bm(_revolve(prof, 18), M["cast"], smooth=True)
    ob = fb.finish(coll)
    return FK.res([ob], {"Rope": ("root", V(0, 0, h * 0.6))},
                  [("bitte", None, [V(r * 1.1 * math.cos(TAU * k / 8), r * 1.1 * math.sin(TAU * k / 8), z)
                                    for k in range(8) for z in (0, h)])], p, info={"variant": "cannon"})


def build_crane(coll, p=None):
    p = {**FK.mod("crane"), **(p or {})}
    M = FM.mats()
    rng = random.Random(406)
    fb = G.FB("Crane")
    H, J = p["mast_h_m"], p["jib_m"]
    bx, by = p["base_m"]
    fb.blend = ASH
    fb.box((0, 0, 0.15), (bx, by, 0.3), M["ashlar"], bevel=0.03)
    fb.blend = RUB
    fb.cyl(V(0, 0, 0.3), V(0, 0, 0.42), 0.28, M["iron"], segs=16)
    for sx, sy in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        G.beam(fb, (sx * 0.7, sy * 0.7, 0.35), (sx * 0.2, sy * 0.2, 1.2), (0.12, 0.12), M["tamanu"])
    base = fb.finish(coll)
    # partie tournante (enfant _Jib) : mat, fleche, contrefiche, treuil, cordage, crochet, filet
    fj = G.FB("CraneJib", world_uv=False)
    fj.blend = RUB
    fj.wood((0, 0, 0.42 + H / 2), (0.26, 0.26, H), M["tamanu"], bevel=0.015, long_u=False)
    zj = 0.42 + H - 0.3
    fj.wood((0, -J / 2 + 0.3, zj), (0.2, J + 0.6, 0.24), M["tamanu"], bevel=0.012, long_u=False)
    G.beam(fj, (0, -0.1, 0.42 + H * 0.45), (0, -J * 0.6, zj - 0.1), (0.14, 0.14), M["tamanu"])
    for z in (0.42 + H * 0.3, zj - 0.5):
        fj.box((0, 0, z), (0.3, 0.3, 0.06), M["iron"], bevel=0.0)
    # treuil a deux manivelles
    wx, wy, wz = p["sockets"]["Winch"]
    for sx in (-1, 1):
        fj.wood((sx * 0.35, wy, wz / 2 + 0.2), (0.1, 0.1, wz + 0.3), M["tamanu"], bevel=0.008, long_u=False)
        G.beam(fj, (sx * 0.35, wy + 0.3, 0.42), (sx * 0.35, 0.0, 0.9), (0.08, 0.08), M["tamanu"])
        crank = [V(sx * 0.45, wy, wz), V(sx * 0.5, wy, wz), V(sx * 0.5, wy + 0.25 * math.cos(0.6), wz + 0.25 * math.sin(0.6)),
                 V(sx * 0.62, wy + 0.25 * math.cos(0.6), wz + 0.25 * math.sin(0.6))]
        fj.tube(crank, 0.016, M["iron"], segs=6)
    fj.tube([V(-0.3, wy, wz), V(0.3, wy, wz)], 0.12, M["tamanu"], segs=12)
    for k in range(5):
        x = -0.24 + 0.12 * k
        ring = [V(x, wy + 0.125 * math.cos(TAU * i / 12), wz + 0.125 * math.sin(TAU * i / 12)) for i in range(12)]
        fj.tube(ring, 0.014, M["rope"], segs=5, closed=True)
    hx, hy, hz = p["sockets"]["Hook"]
    fj.tube([V(0, wy - 0.05, wz + 0.12), V(0, 0.18, zj + 0.1), V(0, -J + 0.1, zj + 0.1), V(0, hy, hz + 0.25)], 0.014,
            M["rope"], segs=5)
    fj.tube([V(0, hy, hz + 0.25), V(0, hy, hz - 1.1)], 0.014, M["rope"], segs=5)
    for y in (0.0, -J + 0.1):
        fj.cyl(V(-0.12, y, zj + 0.1), V(0.12, y, zj + 0.1), 0.09, M["tamanu"], segs=12)
    fj.box(V(0, hy, hz - 1.15), (0.08, 0.12, 0.16), M["tamanu"], bevel=0.01)
    hook = [V(0, hy, hz - 1.22), V(0, hy, hz - 1.35), V(0.05, hy, hz - 1.43), V(0.1, hy, hz - 1.38),
            V(0.09, hy, hz - 1.32)]
    fj.tube(hook, 0.014, M["iron"], segs=6)
    # filet de charge (mailles de corde) avec une caisse
    net_c = V(0, hy, hz - 1.95)
    for k in range(8):
        a = TAU * k / 8
        pts = [V(0, hy, hz - 1.4)] + [net_c + V(0.32 * math.cos(a) * math.sin(math.pi * t / 6),
                                                0.32 * math.sin(a) * math.sin(math.pi * t / 6),
                                                0.45 * math.cos(math.pi * t / 6)) for t in range(1, 7)]
        fj.tube(pts, 0.008, M["rope"], segs=4)
    for t in (2, 3, 4, 5):
        rr = 0.32 * math.sin(math.pi * t / 6)
        ring = [net_c + V(rr * math.cos(TAU * i / 12), rr * math.sin(TAU * i / 12), 0.45 * math.cos(math.pi * t / 6))
                for i in range(12)]
        fj.tube(ring, 0.007, M["rope"], segs=4, closed=True)
    fj.wood(net_c + V(0, 0, -0.1), (0.42, 0.34, 0.34), M["tamanu"], bevel=0.01)
    jib = fj.finish(coll)
    ucx = [("socle", None, G.rbox_pts((0, 0, 0.15), (bx, by, 0.3))),
           ("mat", None, G.rbox_pts((0, 0, 0.42 + H / 2), (0.3, 0.3, H))),
           ("fleche", None, G.rbox_pts((0, -J / 2 + 0.3, zj), (0.25, J + 0.6, 0.3)))]
    return FK.res([base], FK.sockets_of(p["sockets"]), ucx, p, children=[("Jib", [jib], V(0, 0, 0))],
                  info={"anim": p.get("anim")})


def build_warehouse(coll, p=None):
    p = {**FK.mod("warehouse"), **(p or {})}
    M = FM.mats()
    rng = random.Random(407)
    fb = G.FB("Warehouse")
    W, D = p["footprint_m"]
    fz, eave = p["floor_z"], p["eave_z"]
    t = 0.5
    dw, dh = p["door_m"]
    pitch = 40
    ops = [dict(face="S", at=0.0, w=dw, zb=0.0, zt=fz + dh, kind="door", sill=fz, leaf=False),
           dict(face="S", at=-3.4, w=0.6, zb=fz + 1.1, zt=fz + 1.7, kind="window", bars=True),
           dict(face="S", at=3.4, w=0.6, zb=fz + 1.1, zt=fz + 1.7, kind="window", bars=True),
           dict(face="E", at=0.0, w=1.0, zb=0.0, zt=fz + 1.8, kind="door", sill=fz, leaf=True, open=80)]
    BL.floor_slab(fb, M, [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2)], fz)
    hi_x, hi_y = W / 2 - t, D / 2 - t
    BL.plank_floor(fb, M, -hi_x, hi_x, -hi_y, hi_y, fz + 0.02, rng, mat="tamanu", w=0.26)
    ucx = BL.walls(fb, M, W, D, 0.0, eave, t, ops, rng, iron="tamanu")
    G.gable_fill(fb, 0, 0, W, D, eave, pitch, t, M["rubble"])
    zr, rp = BL.roof(fb, M, W + 0.5, D, eave, pitch, rng, hip=False, overhang=0.4, mat="shingles")
    # grand portail : 2 vantaux ouverts a 100 deg
    fb.blend = RUB
    for sx in (-1, 1):
        hinge = V(sx * dw / 2, -D / 2 + t - 0.05, 0)
        R = Matrix.Rotation(sx * math.radians(100), 3, "Z")
        for k in range(5):
            c = hinge + R @ V(-sx * (dw / 2) * (k + 0.5) / 5, 0, 0) + V(0, 0, fz + dh / 2)
            fb.wood(c, (dw / 2 / 5 - 0.006, 0.05, dh - 0.02), M["tamanu"], R=R, bevel=0.004, long_u=False)
        for z in (0.4, dh - 0.3):
            c = hinge + R @ V(-sx * dw / 4, 0.035, 0) + V(0, 0, fz + z)
            fb.wood(c, (dw / 2 - 0.05, 0.03, 0.12), M["tamanu"], R=R, bevel=0.004)
    # lucarne de grenier (porte 1,0 x 1,2) + poutre a poulie
    lw, lh = p["loft_door_m"]
    yf = -D / 2
    zl0 = eave + 0.05
    fb.blend = REND
    fb.box((-lw / 2 - 0.35, yf + 0.25, (zl0 + zl0 + lh + 0.3) / 2 - 0.05), (0.7, 0.5, lh + 0.5), M["rubble"], bevel=0.0)
    fb.box((lw / 2 + 0.35, yf + 0.25, (zl0 + zl0 + lh + 0.3) / 2 - 0.05), (0.7, 0.5, lh + 0.5), M["rubble"], bevel=0.0)
    tri = [(-lw / 2 - 0.7, zl0 + lh + 0.2), (lw / 2 + 0.7, zl0 + lh + 0.2), (0.0, zl0 + lh + 0.95)]
    vv = [V(x, yf, z) for x, z in tri] + [V(x, yf + 0.5, z) for x, z in tri]
    fb.poly_bm(vv, [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)], M["rubble"])
    fb.box((0, yf + 0.25, zl0 + lh + 0.1), (lw + 0.2, 0.5, 0.2), M["rubble"], bevel=0.0)
    for sx in (-1, 1):
        a = V(sx * (lw / 2 + 0.8), yf - 0.15, zl0 + lh + 0.15)
        b = V(0, yf - 0.15, zl0 + lh + 1.0)
        G.beam(fb, a, b, (0.5, 0.06), M["shingles"], wood=False)
    fb.blend = RUB
    hx_, hy_, hz_ = p["sockets"]["Hoist"]
    G.beam(fb, (0, yf + 1.2, hz_ + 0.05), (0, hy_ - 0.3, hz_ + 0.05), (0.16, 0.18), M["tamanu"])
    fb.wood((0, hy_ - 0.1, hz_ - 0.15), (0.08, 0.16, 0.22), M["tamanu"], bevel=0.01)
    fb.tube([V(0, hy_ - 0.1, hz_ - 0.25), V(0, hy_ - 0.1, 0.9)], 0.012, M["jute"], segs=5)
    fb.wood((0.55, yf - 0.1, zl0 + lh / 2), (0.05, 0.9, lh - 0.05), M["tamanu"], R=Matrix.Rotation(-1.4, 3, "Z"),
            bevel=0.004)
    # marchandises : tonneaux, caisses, sacs, cordages, balance
    from fort_rampart import _revolve
    for i in range(6):
        for j in range(2):
            x, y = -hi_x + 0.4 + i * 0.5, hi_y - 0.35 - j * 0.5
            prof = [(0.0, fz), (0.18, fz), (0.21, fz + 0.3), (0.18, fz + 0.6), (0.0, fz + 0.6)]
            tmp = _revolve(prof, 12)
            tmp.transform(Matrix.Translation((x, y, 0)))
            fb.add_bm(tmp, M["tamanu"], smooth=True)
            if j == 0 and i < 4:
                tmp = _revolve([(0.0, fz + 0.6), (0.18, fz + 0.6), (0.21, fz + 0.9), (0.18, fz + 1.2), (0.0, fz + 1.2)], 12)
                tmp.transform(Matrix.Translation((x, y, 0)))
                fb.add_bm(tmp, M["tamanu"], smooth=True)
    for k in range(5):
        x, y = hi_x - 0.5 - (k % 3) * 0.65, hi_y - 0.35 - (k // 3) * 0.5
        fb.wood((x, y, fz + 0.2 + 0.4 * (k >= 3) * 0), (0.6, 0.42, 0.4), M["tamanu"], bevel=0.01)
    for k in range(7):
        c = V(hi_x - 0.4 - rng.uniform(0, 1.6), -hi_y + 0.5 + rng.uniform(0, 1.2), fz + 0.15)
        fb.sphere(c, 0.25, M["jute"], u=10, v=6, scale=(1.3, 0.85, 0.6))
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx] + [("toit", None, rp)]
    ucx += [("tonneaux", None, G.rbox_pts((-hi_x + 1.65, hi_y - 0.6, fz + 0.6), (3.0, 1.0, 1.2))),
            ("caisses", None, G.rbox_pts((hi_x - 1.15, hi_y - 0.6, fz + 0.2), (1.9, 1.0, 0.4)))]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"ridge_z": round(zr, 3)})


MODULES = {
    "quay_6m": ("SM_Port_Quay_6m", build_quay),
    "quay_stairs": ("SM_Port_QuayStairs", build_quay_stairs),
    "jetty_6m": ("SM_Port_Jetty_6m", build_jetty),
    "pier_6m": ("SM_Port_Pier_6m", build_pier),
    "pier_head": ("SM_Port_PierHead", build_pier_head),
    "bollard": ("SM_Port_Bollard", build_bollard),
    "crane": ("SM_Port_Crane", build_crane),
    "warehouse": ("SM_Port_Warehouse", build_warehouse),
}
