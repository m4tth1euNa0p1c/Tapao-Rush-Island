"""
Fort francais : porte (lot 'porte').

SM_Fort_GateHouse (12 m de courtine, passage vote 2,2 x 2,64, frontispice a pilastres, fronton et armes de France en
bas-relief, feuillure du pont-levis, rainures de herse, chemin de ronde continu), SM_Fort_GateDoor_L / _R (+ guichet
enfant _Wicket), SM_Fort_Portcullis, SM_Fort_Drawbridge, SM_Fort_BridgeFixed, SM_Fort_GuardHouse.
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
ZF, ZM, WALK, TOP = RP.ZF, RP.ZM, RP.WALK, RP.TOP
REND, RUB, ASH, BR = RP.REND, RP.RUB, RP.ASH, RP.BR


def _arch(half=1.1, spring=1.98, crown=2.64):
    rise = crown - spring
    R = (half * half + rise * rise) / (2 * rise)
    return R, crown - R


def arch_z(x, half=1.1, spring=1.98, crown=2.64):
    R, zc = _arch(half, spring, crown)
    return zc + math.sqrt(max(R * R - x * x, 0.0))


# ----------------------------------------------------------------------------
# Armes de France (bas-relief generique : ecu a 3 fleurs de lys sous couronne fermee simplifiee)

def fleur_de_lys(fb, M, c, s, y_front, mat="ashlar", relief=0.035):
    """Fleur de lys en relief sur un plan vertical (normale -Y), centre c (x, z), hauteur ~s."""
    cx, cz = c
    y0 = y_front
    y1 = y_front - relief

    def prism2d(poly):
        k = len(poly)
        verts = [V(cx + x * s, y0 + 0.005, cz + z * s) for x, z in poly] + [V(cx + x * s, y1, cz + z * s)
                                                                            for x, z in poly]
        faces = [tuple(range(k)), tuple(range(k, 2 * k))[::-1]]
        for i in range(k):
            j = (i + 1) % k
            faces.append((i, j, k + j, k + i))
        fb.poly_bm(verts, faces, M[mat])
    # petale central (fer de lance)
    prism2d([(0.0, 0.5), (0.09, 0.28), (0.06, 0.05), (0.0, -0.02), (-0.06, 0.05), (-0.09, 0.28)])
    # petales lateraux recourbes
    for sx in (-1, 1):
        pts = []
        for k in range(9):
            a = math.pi * (0.95 - 0.95 * k / 8)
            pts.append((sx * (0.22 + 0.16 * math.cos(a)), 0.12 + 0.2 * math.sin(a)))
        inner = []
        for k in range(9):
            a = math.pi * (0.95 - 0.95 * k / 8)
            inner.append((sx * (0.22 + 0.09 * math.cos(a)), 0.12 + 0.12 * math.sin(a)))
        poly = pts + inner[::-1]
        if sx < 0:
            poly = poly[::-1]
        prism2d(poly)
    # traverse
    prism2d([(-0.2, 0.0), (0.2, 0.0), (0.2, -0.07), (-0.2, -0.07)])
    # pied trifolie
    prism2d([(-0.05, -0.07), (0.05, -0.07), (0.07, -0.25), (0.0, -0.3), (-0.07, -0.25)])
    for sx in (-1, 1):
        prism2d([(sx * 0.05, -0.1), (sx * 0.2, -0.16), (sx * 0.22, -0.24), (sx * 0.12, -0.24), (sx * 0.04, -0.17)][::sx])


def arms_of_france(fb, M, cx, cz, y_face, scale=1.0, mat="ashlar"):
    """Cartouche : ecu (0,8 x 0,95) a trois fleurs de lys (2 et 1) sous une couronne fermee simplifiee."""
    s = scale
    fb.blend = ASH
    # ecu : contour arrondi en bas
    pts = [(-0.4, 0.45), (0.4, 0.45)]
    for k in range(13):
        a = -math.pi * k / 12
        pts.append((0.4 * math.cos(a), -0.1 + 0.45 * math.sin(a) * (1.0 if abs(math.cos(a)) < 0.99 else 1.0)))
    poly = [(x * s, z * s) for x, z in [(-0.4, 0.45), (0.4, 0.45), (0.4, -0.05)] +
            [(0.4 * math.cos(-math.pi * k / 12), -0.05 + 0.5 * math.sin(-math.pi * k / 12)) for k in range(1, 12)] +
            [(-0.4, -0.05)]]
    k = len(poly)
    y0, y1 = y_face - 0.05, y_face + 0.01
    verts = [V(cx + x, y0, cz + z) for x, z in poly] + [V(cx + x, y1, cz + z) for x, z in poly]
    faces = [tuple(range(k))[::-1], tuple(range(k, 2 * k))]
    for i in range(k):
        j = (i + 1) % k
        faces.append((i, j, k + j, k + i))
    fb.poly_bm(verts, faces, M[mat])
    # bordure de l'ecu
    border = [V(cx + x * 1.04, y0 - 0.012, cz + z * 1.04 + 0.002) for x, z in poly]
    fb.tube(border + [border[0]], 0.022, M[mat], segs=6)
    for fx, fz in ((-0.17, 0.2), (0.17, 0.2), (0.0, -0.18)):
        fleur_de_lys(fb, M, (cx + fx * s, cz + fz * s), 0.32 * s, y0, mat)
    # couronne fermee : bandeau, 5 fleurons, 2 arceaux, globe et croix
    zc = cz + 0.5 * s
    fb.box((cx, y0 + 0.0, zc + 0.05 * s), (0.78 * s, 0.1, 0.1 * s), M[mat], bevel=0.015)
    for i in range(5):
        x = (-0.32 + 0.16 * i) * s
        fb.sphere((cx + x, y0 - 0.02, zc + 0.15 * s), 0.045 * s, M[mat], u=8, v=5)
    arc = [V(cx + 0.36 * s * math.cos(math.pi * k / 10), y0 - 0.02, zc + 0.1 * s + 0.26 * s * math.sin(math.pi * k / 10))
           for k in range(11)]
    fb.tube(arc, 0.025 * s, M[mat], segs=6)
    fb.sphere((cx, y0 - 0.02, zc + 0.42 * s), 0.05 * s, M[mat], u=8, v=5)
    fb.box((cx, y0 - 0.02, zc + 0.52 * s), (0.03 * s, 0.04, 0.14 * s), M[mat], bevel=0.0)
    fb.box((cx, y0 - 0.02, zc + 0.54 * s), (0.1 * s, 0.04, 0.03 * s), M[mat], bevel=0.0)
    # lambrequins / palmes stylisees de part et d'autre
    for sx in (-1, 1):
        palm = [V(cx + sx * (0.45 + 0.18 * math.sin(math.pi * k / 8)) * s, y0, cz + (0.4 - 0.9 * k / 8) * s)
                for k in range(9)]
        fb.tube(palm, [0.03 * s * (1 - 0.6 * k / 8) for k in range(9)], M[mat], segs=6)


# ----------------------------------------------------------------------------
# Porterie

def build_gatehouse(coll, p=None):
    p = {**FK.mod("gatehouse"), **(p or {})}
    M = FM.mats()
    rng = random.Random(61)
    fb = G.FB("GateHouse")
    hx = p["size_m"][0] / 2
    pw = p["passage"]["width_m"] / 2
    spring, crown = p["passage"]["springing_z"], p["passage"]["crown_z"]
    yin, yfi = 4.2, 4.497
    fw = p["frontispice"]["width_m"] / 2
    yf = -0.52                                    # face du frontispice (aplomb du pied de l'escarpe a Z 0)
    yr = -0.27                                    # fond de la feuillure du pont-levis
    rw, rd, rh = p["drawbridge_recess"]["size_m"]
    rw /= 2
    top_f = p["frontispice"]["top_z"]
    ent = 4.40
    sec = [(-0.715, ZF), (0.0, ZM), (yin, ZM), (yfi, 0.0), (yfi, ZF)]
    # massifs lateraux (meme section que la courtine), passage traversant
    for x0, x1 in ((-hx, -pw), (pw, hx)):
        n0 = fb.mark()
        fb.blend = REND
        fb.loft([V(x0, y, z) for y, z in sec], [V(x1, y, z) for y, z in sec], M["rubble"])
        fb.set_base(n0, lambda f: 0.0 if (f.normal.y > 0.3 or abs(f.normal.x) > 0.7) else ZF)
    # sous le seuil (escarpe jusqu'au fosse)
    fb.blend = REND
    fb.base = ZF
    sec2 = [(-0.715, ZF), (-0.495, 0.0), (yfi, 0.0), (yfi, ZF)]
    fb.loft([V(-pw, y, z) for y, z in sec2], [V(pw, y, z) for y, z in sec2], M["rubble"])
    fb.base = 0.0
    # voute du passage (massif au-dessus de l'arc) jusqu'au dessus du massif
    BL.arch_wall(fb, M, -pw, pw, spring, crown, ZM, 0.0, yin, mat="rubble")
    BL.voussoirs(fb, M, -pw, pw, spring, crown, yin + 0.005, yin - 0.3, n=13, rng=rng)
    # plinthe et cordon (hors frontispice)
    RP.plinth(fb, M, [(-hx, 0), (hx, 0)])
    RP.cordon(fb, M, [(-hx, 0), (-fw, 0)], rng=rng)
    RP.cordon(fb, M, [(fw, 0), (hx, 0)], rng=rng)
    # frontispice : couche avant (feuillure), couche arriere (arc), mur haut
    fb.blend = ASH
    fb.base = ZF
    for x0, x1 in ((-fw, -rw), (rw, fw)):
        G.block(fb, [(x0, yf), (x1, yf), (x1, yr), (x0, yr)], 0.0, rh, M["ashlar"])
    for x0, x1 in ((-fw, -pw), (pw, fw)):
        G.block(fb, [(x0, yr), (x1, yr), (x1, 0.0), (x0, 0.0)], 0.0, rh, M["ashlar"])
    BL.arch_wall(fb, M, -pw, pw, spring, crown, rh, yr, 0.0, mat="ashlar")
    BL.voussoirs(fb, M, -pw, pw, spring, crown, yr - 0.005, yr + 0.3, n=13, rng=rng)
    G.block(fb, [(-fw, yf), (fw, yf), (fw, 0.0), (-fw, 0.0)], rh, ent, M["ashlar"])
    # appareil de pierre de taille : joints creux (bandes) sur la face
    for z in [0.4 * k for k in range(1, 11)] + [3.3, 3.7, 4.1]:
        if z < ent - 0.1:
            for x0, x1 in ((-fw, -rw), (rw, fw)) if z < rh else ((-fw, fw),):
                fb.box(((x0 + x1) / 2, yf - 0.004, z), (x1 - x0 - 0.02, 0.012, 0.012), M["ashlar"], bevel=0.0)
    # pilastres toscans
    for sx in (-1, 1):
        x = sx * (fw - 0.4)
        fb.box((x, yf - 0.07, ent / 2), (0.5, 0.14, ent - 0.3), M["ashlar"], bevel=0.015)
        fb.box((x, yf - 0.1, 0.15), (0.62, 0.2, 0.3), M["ashlar"], bevel=0.02)
        fb.box((x, yf - 0.1, ent - 0.12), (0.62, 0.2, 0.12), M["ashlar"], bevel=0.02)
        fb.box((x, yf - 0.08, ent - 0.24), (0.56, 0.16, 0.1), M["ashlar"], bevel=0.02)
    # entablement + fronton triangulaire
    fb.box((0, yf - 0.06, ent + 0.12), (2 * fw + 0.3, 0.24, 0.24), M["ashlar"], bevel=0.02)
    fb.box((0, yf - 0.13, ent + 0.3), (2 * fw + 0.45, 0.32, 0.12), M["ashlar"], bevel=0.02)
    zp0, zp1 = ent + 0.36, top_f
    tri = [(-fw - 0.1, zp0), (fw + 0.1, zp0), (0.0, zp1)]
    verts = [V(x, yf - 0.02, z) for x, z in tri] + [V(x, 0.3, z) for x, z in tri]
    fb.poly_bm(verts, [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)], M["ashlar"])
    for sx in (-1, 1):
        a = V(sx * (fw + 0.22), yf - 0.1, zp0 + 0.02)
        b = V(0.0, yf - 0.1, zp1 + 0.08)
        d = (b - a)
        L = d.length
        c = (a + b) / 2
        R = Matrix.Rotation(math.atan2(d.z, abs(d.x)) * (-1 if sx < 0 else 1), 3, "Y")
        fb.box(c, (L, 0.3, 0.12), M["ashlar"], R=R, bevel=0.02)
    fb.sphere((0, yf - 0.05, zp1 + 0.24), 0.12, M["ashlar"], u=10, v=6)
    for sx in (-1, 1):
        fb.box((sx * (fw + 0.1), yf - 0.05, zp0 + 0.22), (0.22, 0.22, 0.3), M["ashlar"], bevel=0.02)
        fb.sphere((sx * (fw + 0.1), yf - 0.05, zp0 + 0.45), 0.11, M["ashlar"], u=10, v=6)
    # cartouche : armes de France (sans devise ni date)
    arms_of_france(fb, M, 0.0, (rh + ent) / 2 - 0.02, yf - 0.02, scale=1.0)
    fb.base = 0.0
    # chemin de ronde : parapet a creneaux hors frontispice, muret derriere le frontispice
    ucx_par = RP.parapet(fb, M, [(-hx, 0), (-fw, 0)], [[("c", 1.0)]], rng=rng)
    ucx_par += RP.parapet(fb, M, [(fw, 0), (hx, 0)], [[("c", 2.0)]], rng=rng)
    fb.blend = REND
    fb.base = WALK
    G.block(fb, [(-fw, 0.0), (fw, 0.0), (fw, 0.45), (-fw, 0.45)], ent, ent + 0.2, M["rubble"])
    G.block(fb, [(-fw, 0.0), (fw, 0.0), (fw, 1.2), (-fw, 1.2)], ZM, WALK + 0.66, M["rubble"], round_top=0.08)
    fb.base = 0.0
    G.slab_paving(fb, [(-hx, 1.2), (hx, 1.2), (hx, 3.75), (-hx, 3.75)], ZM, M["ashlar"], rng, blend=BR)
    ucx = []
    ucx.append(("guard", None, RP.guard_wall(fb, M, (-hx, yin), (hx, yin), ZM, rng, inward=-1)))
    RP.gargoyle(fb, M, (-3.5, yin - 0.05), (0, 1), WALK - 0.02)
    # passage : pave de beachrock + caniveau central, rainures de herse, gonds
    G.slab_paving(fb, [(-pw, yf + 0.02), (pw, yf + 0.02), (pw, yfi + 0.2), (-pw, yfi + 0.2)], -0.06, M["ashlar"],
                  rng, size=(0.3, 0.55), th=0.06, blend=BR, holes=lambda c: abs(c.x) < 0.12)
    fb.blend = BR
    fb.box((0, (yf + yfi) / 2 + 0.1, -0.05), (0.2, yfi - yf + 0.2, 0.04), M["ashlar"], bevel=0.005)
    gy = p["portcullis_groove"]["y_local"]
    gw = p["portcullis_groove"]["width_m"]
    fb.blend = ASH
    for sx in (-1, 1):
        for dy in (-1, 1):
            fb.box((sx * (pw - 0.04), gy + dy * (gw / 2 + 0.07), spring / 2), (0.08, 0.14, spring), M["ashlar"],
                   bevel=0.01)
    hy = p["doors"]["y_local"]
    fb.blend = RUB
    for sx in (-1, 1):
        for z in (0.35, 1.25, 1.95):
            fb.cyl(V(sx * (pw + 0.02), hy, z - 0.06), V(sx * (pw + 0.02), hy, z + 0.06), 0.03, M["iron"], segs=8)
            fb.box((sx * (pw + 0.05), hy, z), (0.1, 0.03, 0.04), M["iron"], bevel=0.0)
    ob = fb.finish(coll)
    ucx += [("massif_w", None, [V(x, y, z) for x in (-hx, -pw) for y, z in sec]),
            ("massif_e", None, [V(x, y, z) for x in (pw, hx) for y, z in sec]),
            ("voute", None, [V(x, y, z) for x in (-pw, pw) for y in (yf, yin) for z in (spring, ZM)]),
            ("seuil", None, [V(x, y, z) for x in (-pw, pw) for y, z in sec2]),
            ("frontispice", None, [V(x, y, z) for x in (-fw, fw) for y in (yf, 0.0) for z in (rh, top_f)]),
            ("pile_w", None, [V(x, y, z) for x in (-fw, -pw) for y in (yf, 0.0) for z in (0, rh)]),
            ("pile_e", None, [V(x, y, z) for x in (pw, fw) for y in (yf, 0.0) for z in (0, rh)]),
            ("muret", None, [V(x, y, z) for x in (-fw, fw) for y in (0.0, 1.2) for z in (ZM, WALK + 0.66)])]
    ucx += [("parapet", None, q) for q in ucx_par]
    socks = FK.sockets_of(p["sockets"])
    rot = {"Door_L": (0, 0, 0), "Door_R": (0, 0, 0)}
    return FK.res([ob], socks, ucx, p, socket_rot=rot,
                  info={"passage_m": [2 * pw, crown], "frontispice_top_z": top_f})


# ----------------------------------------------------------------------------
# Vantaux

def _leaf(fb, M, x_from, x_to, h_fn, th, rng, front=-1, studs=True, plank_w=0.18, gap=0.006, wood="oak",
          x_skip=None):
    """Vantail de planches verticales (dessus suivant h_fn(x)), doublage horizontal au dos, clous a tete diamant
    sur la face avant (y = front * th / 2), pentures."""
    xs = sorted((x_from, x_to))
    n = max(2, round((xs[1] - xs[0]) / plank_w))
    pw = (xs[1] - xs[0]) / n
    fb.blend = RUB
    for k in range(n):
        x0 = xs[0] + k * pw + gap / 2
        x1 = x0 + pw - gap
        zb_ = 0.0
        if x_skip and x_skip[0] < (x0 + x1) / 2 < x_skip[1]:
            zb_ = x_skip[2]
        top = [(x0 + (x1 - x0) * i / 3, h_fn(x0 + (x1 - x0) * i / 3)) for i in range(4)]
        poly = [(x0, zb_), (x1, zb_)] + [(x, z) for x, z in top[::-1]]
        k2 = len(poly)
        y0, y1 = front * th / 2, 0.0
        verts = [V(x, y0, z + rng.uniform(-0.002, 0.002) * 0) for x, z in poly] + [V(x, y1, z) for x, z in poly]
        faces = [tuple(range(k2)), tuple(range(k2, 2 * k2))[::-1]]
        for i in range(k2):
            j = (i + 1) % k2
            faces.append((i, j, k2 + j, k2 + i))
        fb.force_local = True
        fb.poly_bm(verts, faces, M[wood], world=False)
        fb.force_local = False
    # doublage horizontal (dos)
    for z in (0.25, 1.0, 1.75):
        if z < h_fn((xs[0] + xs[1]) / 2) - 0.2:
            fb.wood(((xs[0] + xs[1]) / 2, -front * th * 0.25, z), (xs[1] - xs[0] - 0.02, th / 2, 0.14), M[wood],
                    bevel=0.006)
    return xs


def _studs_grid(fb, M, xs, h_fn, y, pitch=0.18, r=0.016, mat="iron", z0=0.12, skip=None):
    pts = []
    x = xs[0] + 0.09
    while x < xs[1] - 0.05:
        z = z0
        while z < h_fn(x) - 0.1:
            if skip and skip[0] - 0.02 < x < skip[1] + 0.02 and z < skip[2] + 0.04:
                z += pitch
                continue
            pts.append(V(x, y, z))
            z += pitch
        x += pitch
    G.studs(fb, pts, r, M[mat], (0, -1 if y < 0 else 1, 0))
    return len(pts)


def _strap(fb, M, x_hinge, x_end, z, y, mat="iron", w=0.07):
    xa, xb = sorted((x_hinge, x_end))
    fb.box(((xa + xb) / 2, y, z), (xb - xa, 0.012, w), M[mat], bevel=0.0)
    # extremite en fleur (penture)
    xe = x_end
    fb.box((xe, y, z), (0.09, 0.012, 0.12), M[mat], R=Matrix.Rotation(math.radians(45), 3, "Y"), bevel=0.0)
    fb.cyl(V(x_hinge, y, z - 0.06), V(x_hinge, y, z + 0.06), 0.028, M[mat], segs=8)


def build_gate_door(coll, p=None, side="L"):
    key = "gate_door_l" if side == "L" else "gate_door_r"
    p = {**FK.mod(key), **(p or {})}
    gp = FK.mod("gatehouse")
    M = FM.mats()
    rng = random.Random(71 if side == "L" else 72)
    fb = G.FB("GateDoor_" + side)
    w, th, h = p["size_m"]
    sgn = 1 if side == "L" else -1                 # le vantail s'etend de la charniere (x=0) vers le centre
    pw = gp["passage"]["width_m"] / 2
    sp, cr = gp["passage"]["springing_z"], gp["passage"]["crown_z"]

    def h_fn(x):
        xg = -pw + x if side == "L" else pw + x    # x local -> x de la porterie
        return arch_z(xg, pw, sp, cr) - 0.03
    wk = None
    if side == "R":
        ww, wh = p.get("wicket_m", gp["doors"]["wicket"]["size_m"])
        wk = (-0.15 - ww, -0.15, wh + 0.01)
    xs = _leaf(fb, M, 0.0, sgn * (w - 0.004), h_fn, th, rng, x_skip=wk)
    _studs_grid(fb, M, xs, h_fn, -th / 2 - 0.002, skip=wk)
    for z in ((0.35, 1.25, 1.95) if side == "L" else (1.95,)):
        _strap(fb, M, 0.0, sgn * (w - 0.2), z, -th / 2 - 0.008)
    if side == "R":
        for z in (0.35, 1.25):
            _strap(fb, M, 0.0, -0.13, z, -th / 2 - 0.008)
    children = []
    socks = FK.sockets_of(p.get("sockets", {}))
    if side == "L":
        # barre de fermeture (dos) et gache
        fb.wood((sgn * 0.75, th / 2 + 0.05, 1.1), (0.7, 0.09, 0.12), M["oak"], bevel=0.01)
        for x in (0.4, 0.95):
            fb.box((sgn * x, th / 2 + 0.03, 1.1), (0.04, 0.06, 0.18), M["iron"], bevel=0.0)
    ob = fb.finish(coll)
    if side == "R":
        ww, wh = gp["doors"]["wicket"]["size_m"]
        fw_ = G.FB("Wicket")
        x0, x1 = wk[0], wk[1]
        wxs = _leaf(fw_, M, x0 + 0.004, x1 - 0.004, lambda x: wh, th, rng)
        _studs_grid(fw_, M, wxs, lambda x: wh, -th / 2 - 0.002)
        for z in (0.3, 1.25):
            _strap(fw_, M, x1 - 0.02, x0 + 0.15, z, -th / 2 - 0.008)
        fw_.cyl(V(x0 + 0.1, -th / 2 - 0.05, 0.95), V(x0 + 0.1, -th / 2 - 0.05, 0.95), 0.02, M["iron"])
        ring = [V(x0 + 0.12 + 0.05 * math.cos(TAU * i / 10), -th / 2 - 0.03, 0.95 + 0.05 * math.sin(TAU * i / 10))
                for i in range(10)]
        fw_.tube(ring, 0.008, M["iron"], segs=5, closed=True)
        wob = fw_.finish(coll)
        children.append(("Wicket", [wob], V(x1, 0, 0)))
        socks["Wicket_Hinge"] = ("root", V(x1, 0, 0))
    ucx = [("leaf", None, [V(x, y, z) for x in (0, sgn * w) for y in (-th / 2, th / 2) for z in (0, h_fn(0))])]
    return FK.res([ob], socks, ucx, p, children=children, info={"anim": p.get("anim")})


def build_gate_door_l(coll, p=None):
    return build_gate_door(coll, p, "L")


def build_gate_door_r(coll, p=None):
    return build_gate_door(coll, p, "R")


# ----------------------------------------------------------------------------
# Herse, pont-levis, pont dormant

def build_portcullis(coll, p=None):
    p = {**FK.mod("portcullis"), **(p or {})}
    M = FM.mats()
    fb = G.FB("Portcullis")
    W, th, H = p["size_m"]
    pitch = 0.15
    nx = int(W / pitch)
    fb.blend = RUB
    for i in range(nx + 1):
        x = -W / 2 + W * i / nx
        fb.wood((x, 0, H / 2 + 0.05), (0.07, th, H - 0.1), M["oak"], bevel=0.006, long_u=False)
        tip = [V(x - 0.035, -0.035, 0.12), V(x + 0.035, -0.035, 0.12), V(x + 0.035, 0.035, 0.12),
               V(x - 0.035, 0.035, 0.12), V(x, 0, -0.05)]
        fb.poly_bm(tip, [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (3, 2, 1, 0)], M["iron"], world=False)
    nz = int((H - 0.2) / pitch)
    for k in range(nz + 1):
        z = 0.2 + (H - 0.3) * k / nz
        fb.wood((0, 0.04, z), (W + 0.04, 0.05, 0.06), M["oak"], bevel=0.005)
        if k % 3 == 0:
            fb.box((0, -0.042, z), (W + 0.04, 0.01, 0.05), M["iron"], bevel=0.0)
    ob = fb.finish(coll)
    ucx = [("grille", None, G.rbox_pts((0, 0, H / 2), (W, th, H)))]
    return FK.res([ob], {"Lift": ("root", V(0, 0, H))}, ucx, p, info={"mesh_m": pitch})


def build_drawbridge(coll, p=None):
    p = {**FK.mod("drawbridge"), **(p or {})}
    M = FM.mats()
    rng = random.Random(81)
    fb = G.FB("Drawbridge")
    W, L, th = p["deck_m"]
    fb.blend = RUB
    # madriers en travers + longerons + bandes de fer de rive
    y = 0.0
    while y > -L + 0.01:
        w = min(rng.uniform(0.2, 0.26), y + L)
        fb.wood((rng.uniform(-0.01, 0.01), y - w / 2, -0.035), (W - rng.uniform(0.0, 0.04), w - 0.008, 0.07),
                M["oak"], bevel=0.008)
        y -= w
    for x in (-W / 2 + 0.15, -0.35, 0.35, W / 2 - 0.15):
        fb.wood((x, -L / 2, -0.07 - 0.035), (0.14, L, 0.07), M["oak"], bevel=0.008, long_u=False)
    for sx in (-1, 1):
        fb.box((sx * W / 2, -L / 2, -0.06), (0.012, L, 0.12), M["iron"], bevel=0.0)
    fb.box((0, -L + 0.01, -0.06), (W, 0.012, 0.12), M["iron"], bevel=0.0)
    # fleches (2 x 4,0 m au-dessus de la porte, z 3,1) et chaines vers le bout du tablier
    za = 3.1
    for sx in (-1, 1):
        x = sx * 1.35
        fb.wood((x, -0.7, za), (0.16, 4.0, 0.2), M["oak"], bevel=0.01, long_u=False)
        fb.box((x, 1.2, za - 0.12), (0.3, 0.3, 0.3), M["iron"], bevel=0.02)          # contrepoids
        fb.cyl(V(x - 0.12, 0, za), V(x + 0.12, 0, za), 0.04, M["iron"], segs=8)
        top = V(x, -L + 0.1, za - 0.1)
        bot = V(x, -L + 0.1, 0.02)
        n = int((top - bot).length / 0.1)
        for k in range(n):
            c = bot.lerp(top, (k + 0.5) / n)
            rot = (k % 2) * math.pi / 2
            ring = [c + Matrix.Rotation(rot, 3, "Z") @ V(0.022 * math.cos(TAU * i / 6), 0, 0.045 * math.sin(TAU * i / 6))
                    for i in range(6)]
            fb.tube(ring, 0.008, M["iron"], segs=3, closed=True)
        fb.box(V(x, -L + 0.1, 0.0), (0.06, 0.06, 0.03), M["iron"], bevel=0.0)
    ob = fb.finish(coll)
    ucx = [("tablier", None, G.rbox_pts((0, -L / 2, -0.07), (W, L, 0.14)))]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"arms_m": 4.0})


def build_bridge_fixed(coll, p=None):
    p = {**FK.mod("bridge_fixed"), **(p or {})}
    M = FM.mats()
    rng = random.Random(82)
    fb = G.FB("BridgeFixed")
    W, L, th = p["deck_m"]
    pier_w, pier_d = p["pier"]["size_m"]
    z0 = p["pier"]["from_z"]
    # pile de blocs de corail (sous l'about cote pont-levis)
    fb.blend = (1.0, 0.0, 0.0)
    fb.base = z0
    fb.loft([V(-pier_w / 2 - 0.12, -pier_d / 2 - 0.12, z0), V(pier_w / 2 + 0.12, -pier_d / 2 - 0.12, z0),
             V(pier_w / 2 + 0.12, pier_d / 2, z0), V(-pier_w / 2 - 0.12, pier_d / 2, z0)],
            [V(-pier_w / 2, -pier_d / 2, -th), V(pier_w / 2, -pier_d / 2, -th), V(pier_w / 2, pier_d / 2, -th),
             V(-pier_w / 2, pier_d / 2, -th)], M["rubble"])
    fb.blend = ASH
    fb.box((0, 0, -th - 0.06), (pier_w + 0.08, pier_d + 0.08, 0.12), M["ashlar"], bevel=0.015)
    fb.base = 0.0
    # culee cote contrescarpe
    fb.blend = (1.0, 0.0, 0.0)
    fb.base = z0
    fb.loft([V(-pier_w / 2, -L - 0.3, z0), V(pier_w / 2, -L - 0.3, z0), V(pier_w / 2, -L + 0.25, z0),
             V(-pier_w / 2, -L + 0.25, z0)],
            [V(-pier_w / 2, -L - 0.3, -th), V(pier_w / 2, -L - 0.3, -th), V(pier_w / 2, -L + 0.25, -th),
             V(-pier_w / 2, -L + 0.25, -th)], M["rubble"])
    fb.base = 0.0
    # poutres + platelage
    fb.blend = RUB
    for x in (-0.9, -0.3, 0.3, 0.9):
        fb.wood((x, -L / 2, -th - 0.1), (0.2, L + 0.5, 0.22), M["oak"], bevel=0.01, long_u=False)
    y = 0.0
    while y > -L:
        w = min(rng.uniform(0.2, 0.26), y + L)
        fb.wood((rng.uniform(-0.015, 0.015), y - w / 2, -th / 2 + 0.01), (W, w - 0.01, th - 0.02), M["oak"],
                bevel=0.008)
        y -= w
    # garde-corps (poteaux + lisses + contrefiches)
    for sx in (-1, 1):
        x = sx * (W / 2 - 0.06)
        ys = [-0.15 - k * (L - 0.3) / 3 for k in range(4)]
        for yy in ys:
            fb.wood((x, yy, 0.33), (0.1, 0.1, 0.66), M["oak"], bevel=0.008, long_u=False)
            fb.wood((x + sx * 0.18, yy, 0.12), (0.06, 0.06, 0.5), M["oak"], bevel=0.005, long_u=False,
                    R=Matrix.Rotation(-sx * 0.6, 3, "Y"))
        fb.wood((x, -L / 2, 0.68), (0.1, L - 0.2, 0.08), M["oak"], bevel=0.008, long_u=False)
        fb.wood((x, -L / 2, 0.38), (0.06, L - 0.2, 0.06), M["oak"], bevel=0.005, long_u=False)
    ob = fb.finish(coll)
    ucx = [("tablier", None, G.rbox_pts((0, -L / 2, -th / 2), (W, L, th))),
           ("pile", None, G.rbox_pts((0, 0, (z0 - th) / 2), (pier_w, pier_d, -z0 - th))),
           ("rail_g", None, G.rbox_pts((-W / 2 + 0.06, -L / 2, 0.35), (0.12, L, 0.7))),
           ("rail_d", None, G.rbox_pts((W / 2 - 0.06, -L / 2, 0.35), (0.12, L, 0.7)))]
    return FK.res([ob], {"DrawbridgeEnd": ("root", V(0, 0, 0)), "Counterscarp": ("root", V(0, -L, 0))}, ucx, p)


# ----------------------------------------------------------------------------
# Corps de garde

def build_guardhouse(coll, p=None):
    p = {**FK.mod("guardhouse"), **(p or {})}
    M = FM.mats()
    rng = random.Random(91)
    fb = G.FB("GuardHouse")
    W, D = p["footprint_m"]
    fz, wt = p["floor_z"], p["wall_top_z"]
    t = 0.5
    dw, dh = p["door"]["size_m"]
    ww, wh = p["windows"]["size_m"]
    sill = p["windows"]["sill_m"] + fz
    ops = [dict(face="S", at=0.0, w=dw, zb=0.0, zt=fz + dh, kind="door", sill=fz, open=100),
           dict(face="S", at=-2.3, w=ww, zb=sill, zt=sill + wh, kind="window", bars=True),
           dict(face="S", at=2.3, w=ww, zb=sill, zt=sill + wh, kind="window", bars=True),
           dict(face="E", at=0.0, w=ww, zb=sill, zt=sill + wh, kind="window", bars=True)]
    # soubassement / sol interieur
    BL.floor_slab(fb, M, [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2)], fz)
    G.slab_paving(fb, [(-W / 2 + t, -D / 2 + t), (W / 2 - t, -D / 2 + t), (W / 2 - t, D / 2 - t),
                       (-W / 2 + t, D / 2 - t)], fz - 0.04, M["ashlar"], rng, size=(0.35, 0.6), th=0.04, blend=BR)
    ucx = BL.walls(fb, M, W, D, 0.0, wt, t, ops, rng)
    BL.cornice(fb, M, W, D, wt - 0.16)
    zr, rp = BL.roof(fb, M, W + 0.1, D + 0.1, wt, 35, rng, hip=True)
    # plafond de planches (interieur)
    BL.plank_floor(fb, M, -W / 2 + t, W / 2 - t, -D / 2 + t, D / 2 - t, wt + 0.0, rng, th=0.04)
    # mobilier : bat-flanc, ratelier, table + bancs, ecritoire
    x0 = -W / 2 + t
    fb.blend = RUB
    bz = fz + 0.33
    fb.wood((0.0, D / 2 - t - 0.7, bz + 0.03), (5.0, 1.4, 0.06), M["tamanu"], bevel=0.008, long_u=True)
    fb.wood((0.0, D / 2 - t - 1.38, bz - 0.12), (5.0, 0.06, 0.36), M["tamanu"], bevel=0.006)
    for x in (-2.3, -0.8, 0.8, 2.3):
        fb.wood((x, D / 2 - t - 1.3, fz + 0.165), (0.1, 0.1, 0.33), M["tamanu"], bevel=0.006, long_u=False)
    rx, ry = p["sockets"]["MusketRack"][:2]
    fb.wood((rx + 0.12, ry - 0.15, fz + 1.4), (0.08, 1.2, 0.1), M["tamanu"], bevel=0.008, long_u=False)
    fb.wood((rx + 0.12, ry - 0.15, fz + 0.25), (0.12, 1.2, 0.08), M["tamanu"], bevel=0.008, long_u=False)
    for k in range(8):
        yy = ry - 0.7 + k * 0.15
        fb.tube([V(rx + 0.2, yy, fz + 0.2), V(rx + 0.16, yy, fz + 1.55)], [0.025, 0.014], M["iron"], segs=6)
    BL.table(fb, M, (1.2, -0.6, fz), (1.2, 0.7, 0.66), rng)
    BL.bench(fb, M, (1.2, -1.2, fz), 1.2, rng)
    BL.bench(fb, M, (1.2, 0.0, fz), 1.2, rng)
    BL.table(fb, M, (2.6, 0.9, fz), (0.6, 0.45, 0.85), rng)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx]
    ucx += [("toit", None, rp), ("batflanc", None, G.rbox_pts((0, D / 2 - t - 0.7, bz / 2 + 0.03), (5.0, 1.4, bz + 0.06))),
            ("table", None, G.rbox_pts((1.2, -0.6, fz + 0.33), (1.2, 0.7, 0.66)))]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"interior_m": [W - 2 * t, D - 2 * t],
                                                                   "ridge_z": round(zr, 3)})


MODULES = {
    "gatehouse": ("SM_Fort_GateHouse", build_gatehouse),
    "gate_door_l": ("SM_Fort_GateDoor_L", build_gate_door_l),
    "gate_door_r": ("SM_Fort_GateDoor_R", build_gate_door_r),
    "portcullis": ("SM_Fort_Portcullis", build_portcullis),
    "drawbridge": ("SM_Fort_Drawbridge", build_drawbridge),
    "bridge_fixed": ("SM_Fort_BridgeFixed", build_bridge_fixed),
    "guardhouse": ("SM_Fort_GuardHouse", build_guardhouse),
}
