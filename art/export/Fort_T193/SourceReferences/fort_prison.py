"""
Fort francais : prison (lot 'prison').

SM_Fort_Prison (3 cachots grilles 1,8 x 3,4 sous 2,31 le long d'un couloir de 1,2 m, soupiraux barreaudes, terrasse
de dalles a parapet bas), SM_Fort_CellDoor (porte-grille, pivot aux gonds), SM_Fort_StuddedDoor (porte cloutee a
judas), SM_Fort_Cage_Ground, SM_Fort_Cage_Hanging (pivot = anneau de suspension), SM_Fort_Gibbet (potence de
tamanu + chaine), SM_Fort_WallShackles.
"""
import math
import random

from mathutils import Matrix, Vector

import fort_bldg as BL
import fort_geo as G
import fort_kit as FK
import fort_mat as FM
import fort_rampart as RP

V = G.V
TAU = 2 * math.pi
REND, RUB, ASH, BR = RP.REND, RP.RUB, RP.ASH, RP.BR


def grille(fb, M, a, b, z0, z1, pitch=0.12, r=0.015, rails=None, skip=None, mat="iron"):
    """Grille de fer forge de a a b (plan), barreaux ronds + traverses plates ; skip = (t0, t1, zt) : porte."""
    a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
    d = b - a
    L = d.length
    u = d.normalized()
    n = max(1, round(L / pitch))
    fb.blend = RUB
    ang = math.atan2(u.y, u.x)
    for i in range(n + 1):
        t = L * i / n
        zz0 = z0
        if skip and skip[0] - 0.01 < t < skip[1] + 0.01:
            zz0 = skip[2]
            if zz0 >= z1 - 0.05:
                continue
        p = a + u * t
        fb.cyl(V(p.x, p.y, zz0), V(p.x, p.y, z1), r, M[mat], segs=6)
    rails = rails if rails is not None else (z0 + 0.12, z0 + 1.0, z1 - 0.12)
    for z in rails:
        segs = [(0.0, L)]
        if skip and z < skip[2]:
            segs = [(0.0, skip[0] - 0.02), (skip[1] + 0.02, L)]
        for t0, t1 in segs:
            if t1 - t0 < 0.03:
                continue
            c = a + u * ((t0 + t1) / 2)
            fb.box((c.x, c.y, z), (t1 - t0, 0.012, 0.05), M[mat], R=Matrix.Rotation(ang, 3, "Z"), bevel=0.0)
    if skip:
        c = a + u * ((skip[0] + skip[1]) / 2)
        fb.box((c.x, c.y, skip[2]), (skip[1] - skip[0] + 0.06, 0.03, 0.06), M[mat], R=Matrix.Rotation(ang, 3, "Z"),
               bevel=0.0)


def build_prison(coll, p=None):
    p = {**FK.mod("prison"), **(p or {})}
    M = FM.mats()
    rng = random.Random(181)
    fb = G.FB("Prison")
    W, D = p["footprint_m"]
    t = p["walls_m"]
    cz = p["ceiling_z"]
    hx, hy = W / 2 - t, D / 2 - t
    cx0, cx1 = p["cells"]["x_local"]
    dx = p["entrance"]["x_local"]
    ycs = [p["sockets"][f"Cell_{k}"][1] for k in (1, 2, 3)]
    roof_z = cz + 0.4
    ops = [dict(face="S", at=dx, w=1.0, zb=0.0, zt=1.8, kind="door", leaf=False)]
    for yc in ycs:
        ops.append(dict(face="E", at=yc, w=0.4, zb=1.8, zt=2.05, kind="window", bars=True))
    ops.append(dict(face="W", at=0.0, w=0.3, zb=1.7, zt=2.05, kind="window", bars=True))
    BL.floor_slab(fb, M, [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2)], 0.02)
    ucx = BL.walls(fb, M, W, D, 0.0, roof_z + 0.5, t, ops, rng)
    # dalle de terrasse (plafond enduit) + doubleaux, parapet bas + dalles
    fb.blend = REND
    fb.box((0, 0, (cz + roof_z) / 2), (2 * hx, 2 * hy, roof_z - cz), M["rubble"], bevel=0.0)
    fb.blend = ASH
    for y in (-hy + 0.05, -0.95, 0.95, hy - 0.05):
        fb.box((0, y, cz - 0.08), (2 * hx, 0.22, 0.16), M["ashlar"], bevel=0.015)
    G.slab_paving(fb, [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)], roof_z, M["ashlar"], rng, size=(0.4, 0.7),
                  blend=BR)
    fb.blend = ASH
    for (x0, y0), (x1, y1) in (((-W / 2, -D / 2), (W / 2, -D / 2)), ((W / 2, -D / 2), (W / 2, D / 2)),
                               ((W / 2, D / 2), (-W / 2, D / 2)), ((-W / 2, D / 2), (-W / 2, -D / 2))):
        o, u, n, L = G.seg_frame((x0, y0), (x1, y1))
        tt = 0.0
        while tt < L - 0.05:
            ln = min(rng.uniform(0.5, 0.8), L - tt)
            c = o + u * (tt + ln / 2) - n * t / 2
            fb.box((c.x, c.y, roof_z + 0.54), (ln - 0.01, t + 0.06, 0.08), M["ashlar"],
                   R=Matrix.Rotation(math.atan2(u.y, u.x), 3, "Z"), bevel=0.012)
            tt += ln
    # cloisons entre cachots (moellons enduits)
    fb.blend = REND
    for y in (-0.95, 0.95):
        fb.box(((cx0 + hx) / 2, y, cz / 2), (hx - cx0, 0.2, cz), M["rubble"], bevel=0.0)
    # fronts de grille + portes (les vantaux sont des SM a part) ; sol de dalles
    G.slab_paving(fb, [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)], -0.03, M["ashlar"], rng, size=(0.35, 0.6),
                  th=0.05, blend=BR)
    bounds = [(-hy, -1.05), (-0.85, 0.85), (1.05, hy)]
    for (y0, y1), yc in zip(bounds, ycs):
        t0 = (yc - 0.5) - y0
        grille(fb, M, (cx0, y0), (cx0, y1), 0.0, cz, skip=(t0, t0 + 1.0, 1.8))
        # mobilier : planche-lit, seau, paille
        fb.blend = RUB
        fb.wood((hx - 0.35, yc, 0.33), (0.6, min(1.7, y1 - y0 - 0.1), 0.05), M["tamanu"], bevel=0.006,
                long_u=False)
        for yy in (yc - 0.6, yc + 0.6):
            fb.wood((hx - 0.35, yy, 0.155), (0.5, 0.06, 0.31), M["tamanu"], bevel=0.004)
        from fort_rampart import _revolve
        tmp = _revolve([(0.0, 0.0), (0.12, 0.0), (0.14, 0.26), (0.13, 0.26)], 10)
        tmp.transform(Matrix.Translation((hx - 0.25, y0 + 0.25, 0.0)))
        fb.add_bm(tmp, M["tamanu"], smooth=True)
        c = V(hx - 0.9, yc + rng.uniform(-0.3, 0.3), 0.0)
        fb.sphere(c, 0.4, M["jute"], u=14, v=7, scale=(1.0, 1.3, 0.1))
    # couloir : crochet a cles, lanterne murale (socket)
    kx, ky, kz = p["sockets"]["KeyHook"]
    fb.blend = RUB
    fb.cyl(V(kx - 0.05, ky, kz), V(kx + 0.04, ky, kz), 0.008, M["iron"], segs=5)
    ring = [V(kx + 0.04, ky + 0.035 * math.cos(TAU * i / 10), kz - 0.07 + 0.035 * math.sin(TAU * i / 10))
            for i in range(10)]
    fb.tube(ring, 0.006, M["iron"], segs=4, closed=True)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx]
    for (y0, y1), yc in zip(bounds, ycs):
        for ya, yb in ((y0, yc - 0.5), (yc + 0.5, y1)):
            if yb - ya > 0.03:
                ucx.append(("grille", None, G.rbox_pts((cx0, (ya + yb) / 2, cz / 2), (0.05, yb - ya, cz))))
        ucx.append(("imposte", None, G.rbox_pts((cx0, yc, (1.8 + cz) / 2), (0.05, 1.0, cz - 1.8))))
    for y in (-0.95, 0.95):
        ucx.append(("cloison", None, G.rbox_pts(((cx0 + hx) / 2, y, cz / 2), (hx - cx0, 0.2, cz))))
    ucx.append(("plafond", None, G.rbox_pts((0, 0, (cz + roof_z) / 2), (W, D, roof_z - cz))))
    socks = FK.sockets_of(p["sockets"])
    for k, yc in enumerate(ycs, 1):
        socks[f"CellDoorHinge_{k}"] = ("root", V(cx0, yc - 0.5, 0))
        socks[f"Shackles_{k}"] = ("root", V(hx, yc + 0.55, 0))
    socks["DoorHinge"] = ("root", V(dx - 0.5, -D / 2 + 0.12, 0))
    rot = {f"CellDoorHinge_{k}": (0, 0, 90) for k in (1, 2, 3)}
    rot.update({f"Shackles_{k}": (0, 0, 90) for k in (1, 2, 3)})
    return FK.res([ob], socks, ucx, p, socket_rot=rot, info={"cells": 3, "corridor_m": 1.2, "clear_m": cz})


def build_cell_door(coll, p=None):
    p = {**FK.mod("cell_door"), **(p or {})}
    M = FM.mats()
    fb = G.FB("CellDoor")
    w, th, h = p["size_m"]
    fb.blend = RUB
    for z in (0.04, 0.95, h - 0.04):
        fb.box((w / 2, 0, z), (w, 0.012, 0.05), M["iron"], bevel=0.0)
    for x in (0.02, w - 0.02):
        fb.box((x, 0, h / 2), (0.04, 0.02, h), M["iron"], bevel=0.0)
    n = int(w / 0.12)
    for i in range(1, n):
        x = w * i / n
        fb.cyl(V(x, 0, 0.02), V(x, 0, h - 0.02), 0.015, M["iron"], segs=6)
    for z in (0.3, h - 0.3):
        fb.cyl(V(0, 0, z - 0.07), V(0, 0, z + 0.07), 0.025, M["iron"], segs=8)
    # moraillon + cadenas
    fb.box((w - 0.03, -0.03, 1.0), (0.12, 0.012, 0.04), M["iron"], bevel=0.0)
    fb.box((w + 0.02, -0.06, 0.93), (0.07, 0.03, 0.08), M["iron"], bevel=0.008)
    arc = [V(w + 0.02 + 0.025 * math.cos(math.pi * k / 6), -0.06, 0.97 + 0.03 * math.sin(math.pi * k / 6))
           for k in range(7)]
    fb.tube(arc, 0.006, M["iron"], segs=4)
    ob = fb.finish(coll)
    ucx = [("porte", None, G.rbox_pts((w / 2, 0, h / 2), (w, 0.06, h)))]
    return FK.res([ob], {"Lock": ("root", V(w, -0.05, 1.0))}, ucx, p, info={"anim": p.get("anim")})


def build_studded_door(coll, p=None):
    p = {**FK.mod("studded_door"), **(p or {})}
    M = FM.mats()
    rng = random.Random(191)
    fb = G.FB("StuddedDoor")
    w, th, h = p["size_m"]
    fb.blend = RUB
    n = 5
    for k in range(n):
        x = w * (k + 0.5) / n
        if k == 2:
            for z0, z1 in ((0.0, 1.35), (1.6, h)):
                fb.wood((x, 0, (z0 + z1) / 2), (w / n - 0.006, th, z1 - z0), M["oak"], bevel=0.004, long_u=False)
        else:
            fb.wood((x, 0, h / 2), (w / n - 0.006, th, h), M["oak"], bevel=0.004, long_u=False)
    # judas grille
    for k in range(3):
        fb.cyl(V(w / 2 - 0.06 + 0.06 * k, -th / 2, 1.35), V(w / 2 - 0.06 + 0.06 * k, -th / 2, 1.6), 0.008, M["iron"],
               segs=5)
    pts = [V(0.1 + 0.16 * i, -th / 2 - 0.002, 0.12 + 0.17 * j) for i in range(6) for j in range(10)
           if not (0.3 < 0.1 + 0.16 * i < 0.7 and 1.25 < 0.12 + 0.17 * j < 1.7)]
    G.studs(fb, pts, 0.014, M["iron"], (0, -1, 0))
    for z in (0.3, h - 0.3):
        fb.box((w * 0.4, -th / 2 - 0.008, z), (w * 0.8, 0.012, 0.07), M["iron"], bevel=0.0)
        fb.cyl(V(0, 0, z - 0.07), V(0, 0, z + 0.07), 0.025, M["iron"], segs=8)
    fb.box((w - 0.18, -th / 2 - 0.025, 1.0), (0.25, 0.03, 0.05), M["iron"], bevel=0.005)
    fb.box((w - 0.3, -th / 2 - 0.015, 1.0), (0.04, 0.03, 0.12), M["iron"], bevel=0.0)
    ob = fb.finish(coll)
    ucx = [("porte", None, G.rbox_pts((w / 2, 0, h / 2), (w, th, h)))]
    return FK.res([ob], {"Bolt": ("root", V(w - 0.18, -th / 2 - 0.03, 1.0))}, ucx, p, info={"anim": p.get("anim")})


def build_cage_ground(coll, p=None):
    p = {**FK.mod("cage_ground"), **(p or {})}
    M = FM.mats()
    rng = random.Random(201)
    fb = G.FB("CageGround")
    ox, oy, oz = p["outer_m"]
    dw, dh = p["door_m"]
    hx, hy = ox / 2, oy / 2
    zf = 0.14
    fb.blend = RUB
    for x in (-hx + 0.15, hx - 0.15):
        fb.wood((x, 0, 0.04), (0.12, oy + 0.3, 0.08), M["tamanu"], bevel=0.01, long_u=False)
    k = 0
    y = -hy
    while y < hy - 0.01:
        fb.wood((0, y + 0.1, zf - 0.025), (ox, 0.19, 0.05), M["tamanu"], bevel=0.005)
        y += 0.2
        k += 1
    for sx in (-1, 1):
        for sy in (-1, 1):
            fb.wood((sx * (hx - 0.05), sy * (hy - 0.05), oz / 2), (0.1, 0.1, oz), M["tamanu"], bevel=0.008,
                    long_u=False)
    for z in (zf + 0.03, oz - 0.05):
        for sy in (-1, 1):
            fb.wood((0, sy * (hy - 0.05), z), (ox, 0.1, 0.1), M["tamanu"], bevel=0.008)
        for sx in (-1, 1):
            fb.wood((sx * (hx - 0.05), 0, z), (0.1, oy, 0.1), M["tamanu"], bevel=0.008, long_u=False)
    for x in (-0.3, 0.3):
        fb.wood((x, 0, oz - 0.05), (0.08, oy, 0.08), M["tamanu"], bevel=0.006, long_u=False)
    # barreaux sur 4 cotes (porte-trappe sur -Y)
    z0, z1 = zf + 0.08, oz - 0.1
    for (a, b) in (((hx - 0.05, -hy + 0.05), (hx - 0.05, hy - 0.05)), ((hx - 0.05, hy - 0.05), (-hx + 0.05, hy - 0.05)),
                   ((-hx + 0.05, hy - 0.05), (-hx + 0.05, -hy + 0.05))):
        grille(fb, M, a, b, z0, z1, pitch=0.13, r=0.014, rails=(z0 + 0.6,))
    grille(fb, M, (-hx + 0.05, -hy + 0.05), (hx - 0.05, -hy + 0.05), z0, z1, pitch=0.13, r=0.014,
           rails=(z0 + 0.6,), skip=(hx - dw / 2 - 0.05, hx + dw / 2 - 0.05, z0 + dh))
    # porte-trappe (fermee, cadenas)
    for i in range(7):
        x = -dw / 2 + dw * i / 6
        fb.cyl(V(x, -hy + 0.02, z0), V(x, -hy + 0.02, z0 + dh - 0.04), 0.013, M["iron"], segs=6)
    for z in (z0 + 0.04, z0 + dh / 2, z0 + dh - 0.05):
        fb.box((0, -hy + 0.02, z), (dw, 0.012, 0.05), M["iron"], bevel=0.0)
    fb.box((dw / 2 + 0.03, -hy - 0.01, z0 + dh / 2), (0.07, 0.03, 0.08), M["iron"], bevel=0.008)
    for i in range(int(ox / 0.15)):
        x = -hx + 0.1 + i * 0.15
        fb.cyl(V(x, -hy + 0.05, oz - 0.05), V(x, hy - 0.05, oz - 0.05), 0.012, M["iron"], segs=5)
    ob = fb.finish(coll)
    ucx = [("fond", None, G.rbox_pts((0, 0, zf / 2), (ox, oy, zf)))]
    for c, s in (((hx - 0.05, 0, oz / 2), (0.1, oy, oz)), ((-hx + 0.05, 0, oz / 2), (0.1, oy, oz)),
                 ((0, hy - 0.05, oz / 2), (ox, 0.1, oz)), ((0, -hy + 0.05, oz / 2), (ox, 0.1, oz)),
                 ((0, 0, oz - 0.05), (ox, oy, 0.1))):
        ucx.append(("cote", None, G.rbox_pts(c, s)))
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"clear_m": [ox - 0.2, oy - 0.2, z1 - zf]})


def build_cage_hanging(coll, p=None):
    p = {**FK.mod("cage_hanging"), **(p or {})}
    M = FM.mats()
    fb = G.FB("CageHanging")
    r = p["inner_d_m"] / 2 + 0.02
    H = p["outer_h_m"]
    zb = -H
    zt = -0.28
    fb.blend = RUB
    n = 10
    for i in range(n):
        a = TAU * i / n
        u = V(math.cos(a), math.sin(a), 0)
        pts = [V(0, 0, zb + 0.02) + u * (r * 0.6), V(0, 0, zb + 0.04) + u * r, V(0, 0, zt) + u * r]
        for k in range(1, 6):
            aa = math.pi / 2 * k / 5
            pts.append(V(0, 0, zt + 0.22 * math.sin(aa)) + u * (r * math.cos(aa)))
        fb.tube(pts, 0.012, M["iron"], segs=4)
    for z in (zb + 0.05, zb + 0.6, zb + 1.1, zt):
        ring = [V(r * math.cos(TAU * i / 20), r * math.sin(TAU * i / 20), z) for i in range(20)]
        fb.tube(ring, 0.014, M["iron"], segs=4, closed=True)
    fb.tube([V(0, 0, zb), V(0, 0, zb + 0.01)], [r * 0.95, r * 0.95], M["iron"], segs=16)
    fb.cyl(V(0, 0, zt + 0.2), V(0, 0, -0.06), 0.02, M["iron"], segs=6)
    ring = [V(0.05 * math.cos(TAU * i / 10), 0, -0.04 + 0.05 * math.sin(TAU * i / 10)) for i in range(10)]
    fb.tube(ring, 0.012, M["iron"], segs=5, closed=True)
    # porte (charnieres + cadenas) sur -Y
    fb.box((0.0, -r - 0.015, zb + 0.75), (0.08, 0.03, 0.08), M["iron"], bevel=0.008)
    ob = fb.finish(coll)
    ucx = [("cage", None, [V((r + 0.02) * math.cos(TAU * k / 8), (r + 0.02) * math.sin(TAU * k / 8), z)
                           for k in range(8) for z in (zb, zt + 0.22)])]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"inner_m": [p["inner_d_m"], p["inner_h_m"]]})


def build_gibbet(coll, p=None):
    p = {**FK.mod("gibbet"), **(p or {})}
    M = FM.mats()
    fb = G.FB("Gibbet")
    H, A = p["post_h_m"], p["arm_m"]
    hx, hy, hz = p["sockets"]["Hang"]
    cb = p["cage_bottom_z"]
    ring_z = cb + FK.mod("cage_hanging")["outer_h_m"]
    fb.blend = ASH
    fb.box((0, 0, 0.15), (0.7, 0.7, 0.3), M["ashlar"], bevel=0.03)
    fb.blend = RUB
    fb.wood((0, 0, H / 2), (0.22, 0.22, H), M["tamanu"], bevel=0.015, long_u=False)
    fb.wood((A / 2 - 0.05, 0, H - 0.1), (A + 0.1, 0.18, 0.18), M["tamanu"], bevel=0.012)
    fb.wood((0.42, 0, H - 0.52), (0.12, 0.12, 0.95), M["tamanu"], R=Matrix.Rotation(math.radians(-45), 3, "Y"),
            bevel=0.008, long_u=False)
    for d in ((1, 0), (-1, 0), (0, 1), (0, -1)):
        G.beam(fb, (d[0] * 0.55, d[1] * 0.55, 0.3), (d[0] * 0.1, d[1] * 0.1, 0.95), (0.1, 0.1), M["tamanu"])
    fb.box((hx, 0, H - 0.02), (0.06, 0.2, 0.02), M["iron"], bevel=0.0)
    top, bot = V(hx, hy, hz), V(hx, hy, ring_z + 0.02)
    n = max(2, int((top - bot).length / 0.09))
    for k in range(n):
        c = bot.lerp(top, (k + 0.5) / n)
        rot = (k % 2) * math.pi / 2
        ring = [c + Matrix.Rotation(rot, 3, "Z") @ V(0.018 * math.cos(TAU * i / 6), 0, 0.04 * math.sin(TAU * i / 6))
                for i in range(6)]
        fb.tube(ring, 0.007, M["iron"], segs=3, closed=True)
    ob = fb.finish(coll)
    ucx = [("poteau", None, G.rbox_pts((0, 0, H / 2), (0.3, 0.3, H))),
           ("bras", None, G.rbox_pts((A / 2, 0, H - 0.1), (A, 0.2, 0.2)))]
    socks = FK.sockets_of(p["sockets"])
    socks["CageRing"] = ("root", V(hx, hy, ring_z))
    return FK.res([ob], socks, ucx, p, info={"cage_ring_z": round(ring_z, 3)})


def build_wall_shackles(coll, p=None):
    p = {**FK.mod("wall_shackles"), **(p or {})}
    M = FM.mats()
    fb = G.FB("WallShackles")
    w, d, h = p["size_m"]
    fb.blend = RUB
    fb.box((0, -0.01, h - 0.05), (0.12, 0.02, 0.12), M["iron"], bevel=0.005)
    ring = [V(0.045 * math.cos(TAU * i / 10), -0.03, h - 0.12 + 0.045 * math.sin(TAU * i / 10)) for i in range(10)]
    fb.tube(ring, 0.01, M["iron"], segs=5, closed=True)
    for sx in (-1, 1):
        top = V(0, -0.03, h - 0.17)
        end = V(sx * 0.2, -0.08, 0.05)
        mid = (top + end) / 2 + V(0, -0.03, -0.1)
        pts = [top.lerp(mid, k / 6) for k in range(7)] + [mid.lerp(end, k / 6) for k in range(1, 7)]
        for k in range(len(pts) - 1):
            c = (pts[k] + pts[k + 1]) / 2
            rot = (k % 2) * math.pi / 2
            dd = (pts[k + 1] - pts[k]).normalized()
            R = dd.to_track_quat("Z", "Y").to_matrix() @ Matrix.Rotation(rot, 3, "Z")
            ring = [c + R @ V(0.015 * math.cos(TAU * i / 6), 0, 0.035 * math.sin(TAU * i / 6)) for i in range(6)]
            fb.tube(ring, 0.006, M["iron"], segs=3, closed=True)
        cuff = [end + V(0.06 * math.cos(TAU * i / 14 + 0.6), 0.06 * math.sin(TAU * i / 14 + 0.6), 0.0)
                for i in range(12)]
        fb.tube(cuff, [0.012] * 12, M["iron"], segs=4)
    ob = fb.finish(coll)
    return FK.res([ob], {"Wall": ("root", V(0, 0, h - 0.12))}, [], p)


MODULES = {
    "prison": ("SM_Fort_Prison", build_prison),
    "cell_door": ("SM_Fort_CellDoor", build_cell_door),
    "studded_door": ("SM_Fort_StuddedDoor", build_studded_door),
    "cage_ground": ("SM_Fort_Cage_Ground", build_cage_ground),
    "cage_hanging": ("SM_Fort_Cage_Hanging", build_cage_hanging),
    "gibbet": ("SM_Fort_Gibbet", build_gibbet),
    "wall_shackles": ("SM_Fort_WallShackles", build_wall_shackles),
}
