"""
Fort francais : outils de batiments (murs de moellons enduits perces, encadrements de pierre de taille, chaines
d'angle, volets, barreaux, planchers, plafonds, toits de tuiles, corniches, cheminees, mobilier simple).

Repere d'un batiment : centre de l'emprise, faces S (-Y, facade), E (+X), N (+Y), W (-X) ; Z 0 = sol exterieur.
Ouvertures : dict(face, at, w, zb, zt, kind in {door, window, slit, arch}, bars, shutters, leaf) ; `at` = X (faces S/N)
ou Y (faces E/W) du centre, en coordonnees du batiment.
"""
import math
import random

from mathutils import Matrix, Vector

import fort_geo as G

V = G.V
TAU = 2 * math.pi
RUB = (0.0, 0.0, 0.0)
REND = (0.0, 1.0, 0.0)
BAL = (1.0, 0.0, 0.0)
ASH = (0.0, 0.0, 0.0)
BR = (0.0, 0.0, 1.0)


def _face_line(face, W, D, t):
    hw, hd = W / 2, D / 2
    if face == "S":
        return (-hw, -hd), (hw, -hd)
    if face == "N":
        return (hw, hd), (-hw, hd)
    if face == "E":
        return (hw, -hd + t), (hw, hd - t)
    return (-hw, hd - t), (-hw, -hd + t)


def _t_of(face, at, W, D, t):
    if face == "S":
        return at + W / 2
    if face == "N":
        return W / 2 - at
    if face == "E":
        return at + D / 2 - t
    return D / 2 - t - at


def walls(fb, M, W, D, z0, z1, t, openings, rng, render=REND, frames=True, quoin=True, frame_mat="ashlar",
          wood="tamanu", iron="iron", ucx=None, inner_render=None, shutters_open=165.0):
    """Quatre murs (moellons enduits), percements encadres de pierre de taille, chaines d'angle. Retourne les
    boites UCX des trumeaux."""
    ucx = [] if ucx is None else ucx
    for face in ("S", "E", "N", "W"):
        a, b = _face_line(face, W, D, t)
        ops = [o for o in openings if o["face"] == face]
        cut = []
        for o in ops:
            tc = _t_of(face, o["at"], W, D, t)
            cut.append((tc - o["w"] / 2, tc + o["w"] / 2, o["zb"], o["zt"]))
        o_, u, n, L = G.seg_frame(a, b)
        fb.blend = render
        n0 = fb.mark()
        G.wall_openings(fb, a, b, z0, z1, t, cut, M["rubble"], inward=1)
        if inner_render is not None:
            fb.paint(n0, lambda f, n=n: (0.0, inner_render, 0.0) if f.normal.dot(n) < -0.5 else None)
        # boites UCX : trumeaux (entre ouvertures) pleine hauteur
        ts = sorted(cut)
        prev = 0.0
        for c0, c1, _zb, _zt in ts + [(L, L, 0, 0)]:
            if c0 - prev > 0.05:
                ucx.append([V(*(o_ + u * tt - n * dd)[:2], zz) for tt in (prev, c0) for dd in (0, t) for zz in (z0, z1)])
            prev = c1
        for o, (c0, c1, zb, zt) in zip(ops, cut):
            M4 = G.frame_M4(o_ + u * ((c0 + c1) / 2), u, n)          # X le long du mur, Y vers l'exterieur
            kind = o.get("kind", "window")
            if kind == "arch":
                arch_infill(fb, M, M4, o["w"], o["zt"] - o["w"] / 2, o["zt"], t, render, frame_mat)
            if frames:
                frame(fb, M, M4, o, t, rng, frame_mat)
            kind = o.get("kind", "window")
            if o.get("bars"):
                fb.blend = RUB
                xs = []
                k = max(2, int(o["w"] / 0.12))
                for i in range(1, k):
                    xs.append(-o["w"] / 2 + o["w"] * i / k)
                for x in xs:
                    p0 = M4 @ V(x, -t * 0.35, zb - 0.02)
                    p1 = M4 @ V(x, -t * 0.35, zt + 0.02)
                    fb.cyl(p0, p1, 0.013, M[iron], segs=6)
                for zz in (zb + (zt - zb) * 0.35, zb + (zt - zb) * 0.75):
                    fb.box(M4 @ V(0, -t * 0.35, zz), (o["w"] + 0.04, 0.012, 0.035), M[iron],
                           R=M4.to_3x3().normalized(), bevel=0.0)
            if o.get("shutters") and kind == "window":
                shutters(fb, M, M4, o, rng, wood, iron, shutters_open)
            if kind == "door" and o.get("leaf", True):
                door_leaf(fb, M, M4, o, t, rng, wood, iron, open_deg=o.get("open", 95))
    # chaines d'angle
    if quoin:
        for (cx, cy), d1, d2 in (((W / 2, -D / 2), (1, 0), (0, 1)), ((W / 2, D / 2), (0, 1), (-1, 0)),
                                 ((-W / 2, D / 2), (-1, 0), (0, -1)), ((-W / 2, -D / 2), (0, -1), (1, 0))):
            corner_quoins(fb, M, (cx, cy), d1, d2, z0 + 0.05, z1 - 0.05, rng, frame_mat)
    return ucx


def corner_quoins(fb, M, C, d1, d2, z0, z1, rng, mat="ashlar", depth=0.22, proud=0.02):
    d1 = Vector((d1[0], d1[1], 0))
    d2 = Vector((d2[0], d2[1], 0))
    n1 = Vector((d1.y, -d1.x, 0))
    n2 = Vector((d2.y, -d2.x, 0))
    C = Vector((C[0], C[1], 0))
    fb.blend = ASH
    z, k = z0, 0
    while z < z1 - 0.1:
        h = min(rng.uniform(0.24, 0.3), z1 - z)
        for face, (dd, nn) in enumerate(((-d1, n1), (d2, n2))):
            ln = (0.5 if (k + face) % 2 == 0 else 0.3) * rng.uniform(0.92, 1.08)
            q = [C - dd * proud + nn * proud, C + dd * ln + nn * proud, C + dd * ln - nn * depth,
                 C - dd * proud - nn * depth]
            fb.loft([p + V(0, 0, z + 0.005) for p in q], [p + V(0, 0, z + h - 0.005) for p in q], M[mat],
                    bevel=0.01)
        z += h
        k += 1


def frame(fb, M, M4, o, t, rng, mat="ashlar"):
    """Encadrement de pierre de taille (jambages harpes, linteau, appui saillant) sur la face exterieure."""
    w, zb, zt = o["w"], o["zb"], o["zt"]
    kind = o.get("kind", "window")
    R3 = M4.to_3x3().normalized()
    fb.blend = ASH
    fw, fd = 0.16, 0.06
    y = fd / 2 - 0.02
    if kind != "arch":
        # linteau (ou plate-bande clavee)
        fb.box(M4 @ V(0, y, zt + 0.11), (w + 2 * fw + 0.04, fd + 0.02, 0.22), M[mat], R=R3, bevel=0.01)
    # jambages : blocs alternes
    ztop_j = zt - w / 2 if kind == "arch" else zt
    for sx in (-1, 1):
        z, k = (zb if kind not in ("door",) else 0.0), 0
        while z < ztop_j - 0.04:
            h = min(rng.uniform(0.26, 0.34), ztop_j - z)
            ww = fw + (0.07 if k % 2 == 0 else 0.0)
            x = sx * (w / 2 + ww / 2)
            fb.box(M4 @ V(x, y, z + h / 2), (ww - 0.006, fd, h - 0.008), M[mat], R=R3, bevel=0.008)
            z += h
            k += 1
    if kind in ("window", "arch"):
        fb.box(M4 @ V(0, y + 0.03, zb - 0.045), (w + 2 * fw + 0.08, fd + 0.08, 0.09), M[mat], R=R3, bevel=0.012)
        # tableau (embrasure interieure) en enduit : fond de baie sombre = epaisseur du mur deja ouverte
    if kind == "door":
        # seuil de pierre
        fb.blend = BR
        fb.box(M4 @ V(0, -t / 2 + 0.02, o.get("sill", 0.0) - 0.03), (w + 0.1, t + 0.12, 0.08), M[mat], R=R3,
               bevel=0.01)


def shutters(fb, M, M4, o, rng, wood="tamanu", iron="iron", open_deg=100):
    """Volets persiennes ouverts contre le mur."""
    w, zb, zt = o["w"], o["zb"], o["zt"]
    h = zt - zb - 0.02
    fb.blend = RUB
    for sx in (-1, 1):
        a = math.radians(open_deg) * sx * rng.uniform(0.85, 1.0)
        hinge = M4 @ V(sx * (w / 2 + 0.17), 0.07, 0)
        R = M4.to_3x3().normalized() @ Matrix.Rotation(-a, 3, "Z")
        lw = w / 2
        c = hinge + R @ V(-sx * lw / 2, 0.0, 0) + V(0, 0, zb + 0.01 + h / 2)
        fb.wood(c, (lw, 0.035, h), M[wood], R=R, bevel=0.005, long_u=False)
        for k in range(int(h / 0.12)):
            zz = zb + 0.08 + k * 0.12
            if zz > zb + h - 0.05:
                break
            cc = hinge + R @ V(-sx * lw / 2, 0.022, 0) + V(0, 0, zz)
            fb.wood(cc, (lw - 0.06, 0.012, 0.05), M[wood], R=R @ Matrix.Rotation(0.5, 3, "X"), bevel=0.0)


def door_leaf(fb, M, M4, o, t, rng, wood="tamanu", iron="iron", open_deg=95):
    """Vantail de planches ouvert vers l'interieur (penture de fer)."""
    w, zt = o["w"], o["zt"]
    zb = o.get("sill", 0.0)
    h = zt - zb - 0.02
    a = math.radians(open_deg)
    hinge = M4 @ V(-w / 2 + 0.04, -t + 0.12, 0)
    R = M4.to_3x3().normalized() @ Matrix.Rotation(-a, 3, "Z")
    fb.blend = RUB
    n = max(3, int(w / 0.2))
    for k in range(n):
        pw = (w - 0.06) / n
        c = hinge + R @ V(pw * (k + 0.5), 0, 0) + V(0, 0, zb + h / 2)
        fb.wood(c, (pw - 0.006, 0.045, h), M[wood], R=R, bevel=0.004, long_u=False)
    for zz in (0.25, h - 0.25):
        c = hinge + R @ V((w - 0.06) / 2, -0.035, 0) + V(0, 0, zb + zz)
        fb.box(c, (w - 0.12, 0.012, 0.05), M[iron], R=R, bevel=0.0)


def floor_slab(fb, M, poly, z, mat="ashlar", blend=BR, th=0.08):
    fb.blend = blend
    fb.prism(poly, z - th, z, M[mat])


def tiled_floor(fb, M, x0, x1, y0, y1, z, rng, mat="brick", tile=0.3, blend=RUB):
    """Carreaux de terre cuite (dessus a z)."""
    fb.blend = blend
    fb.box(((x0 + x1) / 2, (y0 + y1) / 2, z - 0.05), (x1 - x0, y1 - y0, 0.09), M[mat], bevel=0.0)


def plank_floor(fb, M, x0, x1, y0, y1, z, rng, mat="tamanu", w=0.22, th=0.05, along_x=True):
    fb.blend = RUB
    if along_x:
        y = y0
        while y < y1 - 0.02:
            ww = min(w * rng.uniform(0.9, 1.1), y1 - y)
            fb.wood(((x0 + x1) / 2, y + ww / 2, z - th / 2 + rng.uniform(-0.003, 0.003)), (x1 - x0, ww - 0.006, th),
                    M[mat], bevel=0.004)
            y += ww
    else:
        x = x0
        while x < x1 - 0.02:
            ww = min(w * rng.uniform(0.9, 1.1), x1 - x)
            fb.wood((x + ww / 2, (y0 + y1) / 2, z - th / 2), (ww - 0.006, y1 - y0, th), M[mat], bevel=0.004,
                    long_u=False)
            x += ww


def cornice(fb, M, W, D, z, mat="ashlar", proj=0.14):
    fb.blend = ASH
    prof = [(-0.1, -0.02), (0.03, -0.02), (0.05, 0.03), (proj * 0.7, 0.07), (proj, 0.12), (proj, 0.16), (-0.1, 0.16)]
    hw, hd = W / 2, D / 2
    path = [(-hw, -hd, z), (hw, -hd, z), (hw, hd, z), (-hw, hd, z)]
    # chemin anti-horaire : exterieur a droite (d > 0)
    fb.sweep([V(*p) for p in path], prof, M[mat], closed=True)


def roof(fb, M, W, D, z_eave, pitch, rng, hip=True, overhang=0.35, mat="tiles", wood="tamanu", ang=0.0, cx=0.0,
         cy=0.0, rafters=True):
    """Toit de tuiles + chevrons apparents sous l'egout + bandeau. Retourne (z faitage, points UCX)."""
    fb.blend = RUB
    zr, pts = G.hip_roof(fb, cx, cy, W, D, z_eave, pitch, overhang, M[mat], th=0.08, hip=hip, ang=ang)
    if rafters:
        R = Matrix.Rotation(ang, 3, "Z")
        tpc = math.tan(math.radians(pitch))
        for sy in (-1, 1):
            n = int(W / 0.55)
            for k in range(n + 1):
                x = -W / 2 + W * k / n
                a = V(x, sy * (D / 2 - 0.25), z_eave + 0.25 * tpc - 0.1)
                b = V(x, sy * (D / 2 + overhang - 0.05), z_eave - (overhang - 0.05) * tpc - 0.1)
                fb.tube([V(cx, cy, 0) + R @ a, V(cx, cy, 0) + R @ b], 0.035, M[wood], segs=4)
    return zr, pts


def chimney(fb, M, x, y, z0, z1, rng, mat="brick", w=0.6, d=0.45, ang=0.0):
    fb.blend = RUB
    R = Matrix.Rotation(ang, 3, "Z")
    fb.box((x, y, (z0 + z1) / 2), (w, d, z1 - z0), M[mat], R=R, bevel=0.01)
    fb.box((x, y, z1 + 0.05), (w + 0.1, d + 0.1, 0.1), M[mat], R=R, bevel=0.01)
    fb.box((x, y, z1 + 0.16), (w - 0.1, d - 0.1, 0.12), M[mat], R=R, bevel=0.01)


def table(fb, M, c, size, rng, mat="tamanu", ang=0.0):
    x, y, z = c
    w, d, h = size
    R = Matrix.Rotation(ang, 3, "Z")
    o = V(x, y, z)
    fb.blend = RUB
    fb.wood(o + V(0, 0, h - 0.025), (w, d, 0.05), M[mat], R=R, bevel=0.006)
    for sx in (-1, 1):
        for sy in (-1, 1):
            fb.wood(o + R @ V(sx * (w / 2 - 0.06), sy * (d / 2 - 0.06), 0) + V(0, 0, (h - 0.05) / 2),
                    (0.06, 0.06, h - 0.05), M[mat], R=R, bevel=0.005, long_u=False)
    fb.wood(o + V(0, 0, 0.15), (w - 0.15, 0.04, 0.04), M[mat], R=R, bevel=0.0)


def bench(fb, M, c, length, rng, mat="tamanu", ang=0.0, h=0.28):
    x, y, z = c
    R = Matrix.Rotation(ang, 3, "Z")
    o = V(x, y, z)
    fb.blend = RUB
    fb.wood(o + V(0, 0, h - 0.025), (length, 0.24, 0.05), M[mat], R=R, bevel=0.005)
    for sx in (-1, 1):
        fb.wood(o + R @ V(sx * (length / 2 - 0.12), 0, 0) + V(0, 0, (h - 0.05) / 2), (0.05, 0.22, h - 0.05),
                M[mat], R=R, bevel=0.004, long_u=False)


def chest(fb, M, c, size, rng, wood="oak", iron="iron", ang=0.0, lid=0.0):
    x, y, z = c
    w, d, h = size
    R = Matrix.Rotation(ang, 3, "Z")
    o = V(x, y, z)
    fb.blend = RUB
    fb.wood(o + V(0, 0, h * 0.4), (w, d, h * 0.8), M[wood], R=R, bevel=0.008)
    fb.wood(o + V(0, 0, h * 0.9), (w + 0.02, d + 0.02, h * 0.2), M[wood], R=R, bevel=0.012)
    for sx in (-0.35, 0.35):
        fb.box(o + R @ V(sx * w, 0, 0) + V(0, 0, h * 0.5), (0.03, d + 0.025, h + 0.01), M[iron], R=R, bevel=0.0)


def lantern(fb, M, c, rng, h=0.4, iron="iron", lamp="lamp", bracket=None):
    """Lanterne de fer et corne (corps octogonal emissif) ; bracket = (point mural, direction) : potence."""
    c = Vector(c)
    r = 0.11
    fb.blend = RUB
    # corps de corne (emissif) + montants + chapeau
    fb.tube([c + V(0, 0, -h * 0.32), c + V(0, 0, h * 0.18)], [r * 0.82, r * 0.82], M[lamp], segs=8)
    for k in range(4):
        a = TAU * k / 4 + 0.785
        p = c + V(r * math.cos(a), r * math.sin(a), 0)
        fb.cyl(p + V(0, 0, -h * 0.36), p + V(0, 0, h * 0.2), 0.009, M[iron], segs=4)
    fb.tube([c + V(0, 0, -h * 0.42), c + V(0, 0, -h * 0.34)], [r * 1.05, r * 1.05], M[iron], segs=8)
    fb.tube([c + V(0, 0, h * 0.2), c + V(0, 0, h * 0.34), c + V(0, 0, h * 0.42)], [r * 1.1, r * 0.55, 0.02],
            M[iron], segs=8)
    ring = [c + V(0.03 * math.cos(TAU * i / 8), 0, h * 0.47 + 0.03 * math.sin(TAU * i / 8)) for i in range(8)]
    fb.tube(ring, 0.006, M[iron], segs=4, closed=True)
    if bracket:
        wp, d = Vector(bracket[0]), Vector(bracket[1]).normalized()
        top = c + V(0, 0, h * 0.5 + 0.03)
        knee = Vector((top.x, top.y, top.z + 0.08))
        fb.tube([wp + V(0, 0, 0.12), knee, top], 0.012, M[iron], segs=6)
        fb.tube([wp - V(0, 0, 0.12), wp + d * ((top - wp).length * 0.55) + V(0, 0, 0.02)], 0.009, M[iron], segs=5)
        fb.box(wp, (0.08, 0.02, 0.3), M[iron], R=Matrix.Rotation(math.atan2(d.y, d.x) + math.pi / 2, 3, "Z"),
               bevel=0.0)


def arch_points(x0, x1, z_spring, z_crown, n=10):
    """Arc segmentaire de x0 a x1 (points (x, z) de gauche a droite)."""
    hw = (x1 - x0) / 2
    xc = (x0 + x1) / 2
    rise = z_crown - z_spring
    if rise < 1e-4:
        return [(x0, z_spring), (x1, z_spring)]
    R = (hw * hw + rise * rise) / (2 * rise)
    zc = z_crown - R
    a0 = math.atan2(z_spring - zc, -hw)
    a1 = math.atan2(z_spring - zc, hw)
    return [(xc + R * math.cos(a0 + (a1 - a0) * k / n), zc + R * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]


def arch_wall(fb, M, x0, x1, z_spring, z_crown, z_top, y0, y1, mat="rubble", n=12, margin=0.0):
    """Massif au-dessus d'un arc (tympan / voute) : contour (x, z) extrude entre y0 et y1."""
    pts = arch_points(x0, x1, z_spring, z_crown, n)
    poly = [(x0 - margin, z_spring)] + pts + [(x1 + margin, z_spring), (x1 + margin, z_top), (x0 - margin, z_top)]
    poly = [poly[0]] + pts[1:-1] + [(x1 + margin, z_spring), (x1 + margin, z_top), (x0 - margin, z_top)] \
        if margin == 0.0 else poly
    k = len(poly)
    verts = [V(x, y0, z) for x, z in poly] + [V(x, y1, z) for x, z in poly]
    faces = [tuple(range(k))[::-1], tuple(range(k, 2 * k))]
    for i in range(k):
        j = (i + 1) % k
        faces.append((i, j, k + j, k + i))
    fb.poly_bm(verts, faces, M[mat])


def voussoirs(fb, M, x0, x1, z_spring, z_crown, y0, y1, n=11, depth=0.32, rng=None, mat="ashlar", key=0.05):
    """Claveaux d'un arc segmentaire (face exterieure entre y0 et y1), cle saillante."""
    hw = (x1 - x0) / 2
    xc = (x0 + x1) / 2
    rise = z_crown - z_spring
    R = (hw * hw + rise * rise) / (2 * rise)
    zc = z_crown - R
    a0 = math.atan2(z_spring - zc, -hw)
    a1 = math.atan2(z_spring - zc, hw)
    fb.blend = ASH
    for k in range(n):
        aa = a0 + (a1 - a0) * (k + 0.03) / n
        ab = a0 + (a1 - a0) * (k + 0.97) / n
        d = depth + (key if k == n // 2 else 0.0)
        e = 0.03 if k == n // 2 else 0.0
        q = [(xc + R * math.cos(aa), zc + R * math.sin(aa)), (xc + R * math.cos(ab), zc + R * math.sin(ab)),
             (xc + (R + d) * math.cos(ab), zc + (R + d) * math.sin(ab)),
             (xc + (R + d) * math.cos(aa), zc + (R + d) * math.sin(aa))]
        verts = [V(x, y0 - e, z) for x, z in q] + [V(x, y1, z) for x, z in q]
        faces = [(3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        fb.poly_bm(verts, faces, M[mat])


def arch_infill(fb, M, M4, w, z_spring, z_crown, t, render=REND, frame_mat="ashlar", n=10):
    """Tete en plein cintre d'une baie rectangulaire (remplit les ecoincons) + claveaux exterieurs."""
    pts = arch_points(-w / 2, w / 2, z_spring, z_crown, n)
    fb.blend = render
    for side in (-1, 1):
        half = [p for p in pts if (p[0] <= 1e-6 if side < 0 else p[0] >= -1e-6)]
        if side < 0:
            poly = [(-w / 2, z_spring)] + half[1:] + [(0.0, z_crown + 0.001), (-w / 2, z_crown + 0.001)]
        else:
            poly = half[:-1] + [(w / 2, z_spring), (w / 2, z_crown + 0.001), (0.0, z_crown + 0.001)]
        k = len(poly)
        verts = [M4 @ V(x, 0.0, z) for x, z in poly] + [M4 @ V(x, -t, z) for x, z in poly]
        faces = [tuple(range(k)), tuple(range(k, 2 * k))[::-1]]
        for i in range(k):
            j = (i + 1) % k
            faces.append((i, j, k + j, k + i))
        fb.poly_bm(verts, faces, M["rubble"])
    fb.blend = ASH
    m = 7
    R = w / 2
    for i in range(m):
        a0 = math.pi * (1 - (i + 0.04) / m)
        a1 = math.pi * (1 - (i + 0.96) / m)
        d = 0.18 + (0.04 if i == m // 2 else 0.0)
        q = [(R * math.cos(a0), z_spring + R * math.sin(a0)), (R * math.cos(a1), z_spring + R * math.sin(a1)),
             ((R + d) * math.cos(a1), z_spring + (R + d) * math.sin(a1)),
             ((R + d) * math.cos(a0), z_spring + (R + d) * math.sin(a0))]
        verts = [M4 @ V(x, 0.03, z) for x, z in q] + [M4 @ V(x, -0.08, z) for x, z in q]
        faces = [(3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)]
        fb.poly_bm(verts, faces, M[frame_mat])
