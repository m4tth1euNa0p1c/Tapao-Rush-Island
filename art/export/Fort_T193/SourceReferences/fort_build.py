"""
Fort francais : batiments interieurs (lots 'batiments' et 'poudriere_chapelle').

SM_Fort_GovernorHouse (logis a galerie, 2 niveaux praticables), SM_Fort_Barracks (caserne, 2 chambrees de lits a deux
etages), SM_Fort_Kitchen (cuisine-boulangerie : foyer, four a pain de briques), SM_Fort_Cistern (citerne a margelle et
potence), SM_Fort_PowderMagazine (magasin a poudre voute, contreforts, toit de dalles, enclos), SM_Fort_Chapel
(chapelle a clocher-mur). Murs de moellons enduits a la chaux, encadrements et chaines en pierre de taille,
tuiles canal, bois de tamanu. Maximum 5 materiaux par batiment.
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
REND, RUB, ASH, BR, BAL = RP.REND, RP.RUB, RP.ASH, RP.BR, RP.BAL


def _box_ucx(c, s):
    return G.rbox_pts(c, s)


def chair(fb, M, c, rng, ang=0.0, mat="tamanu"):
    x, y, z = c
    R = Matrix.Rotation(ang, 3, "Z")
    o = V(x, y, z)
    fb.blend = RUB
    fb.wood(o + V(0, 0, 0.3), (0.38, 0.38, 0.04), M[mat], R=R, bevel=0.005)
    for sx in (-1, 1):
        for sy in (-1, 1):
            hgt = 0.82 if sy > 0 else 0.3
            fb.wood(o + R @ V(sx * 0.16, sy * 0.16, 0) + V(0, 0, hgt / 2), (0.04, 0.04, hgt), M[mat], R=R,
                    bevel=0.003, long_u=False)
    fb.wood(o + R @ V(0, 0.16, 0) + V(0, 0, 0.66), (0.36, 0.03, 0.22), M[mat], R=R, bevel=0.004)


def bed(fb, M, c, rng, L=1.9, w=1.1, posts=True, mat="tamanu", cloth="tamanu", ang=0.0):
    x, y, z = c
    R = Matrix.Rotation(ang, 3, "Z")
    o = V(x, y, z)
    fb.blend = RUB
    fb.wood(o + V(0, 0, 0.3), (w, L, 0.12), M[mat], R=R, bevel=0.01)
    fb.wood(o + V(0, 0, 0.42), (w - 0.06, L - 0.06, 0.14), M[cloth], R=R, bevel=0.04)
    for sx in (-1, 1):
        for sy in (-1, 1):
            h = 1.9 if posts else (0.7 if sy > 0 else 0.45)
            fb.wood(o + R @ V(sx * (w / 2 - 0.04), sy * (L / 2 - 0.04), 0) + V(0, 0, h / 2), (0.07, 0.07, h),
                    M[mat], R=R, bevel=0.006, long_u=False)
    if posts:
        for sy in (-1, 1):
            fb.wood(o + R @ V(0, sy * (L / 2 - 0.04), 0) + V(0, 0, 1.88), (w, 0.06, 0.08), M[mat], R=R, bevel=0.005)
        for sx in (-1, 1):
            fb.wood(o + R @ V(sx * (w / 2 - 0.04), 0, 0) + V(0, 0, 1.88), (0.06, L, 0.08), M[mat], R=R, bevel=0.005)
    fb.wood(o + R @ V(0, L / 2 - 0.04, 0) + V(0, 0, 0.75), (w, 0.05, 0.5), M[mat], R=R, bevel=0.006)


def bunk(fb, M, c, rng, L=1.3, w=0.6, mat="tamanu", straw="jute", ang=0.0):
    x, y, z = c
    R = Matrix.Rotation(ang, 3, "Z")
    o = V(x, y, z)
    fb.blend = RUB
    for sx in (-1, 1):
        for sy in (-1, 1):
            fb.wood(o + R @ V(sx * (L / 2 - 0.03), sy * (w / 2 - 0.03), 0) + V(0, 0, 0.85), (0.06, 0.06, 1.7),
                    M[mat], R=R, bevel=0.005, long_u=False)
    for zz in (0.33, 1.16):
        fb.wood(o + V(0, 0, zz - 0.03), (L, w, 0.05), M[mat], R=R, bevel=0.005)
        fb.box(o + V(rng.uniform(-0.02, 0.02), 0, zz + 0.04), (L - 0.12, w - 0.1, 0.09), M[straw], R=R, bevel=0.03)
        for sy in (-1, 1):
            fb.wood(o + R @ V(0, sy * (w / 2 - 0.02), 0) + V(0, 0, zz + 0.06), (L, 0.04, 0.12), M[mat], R=R,
                    bevel=0.004)


# ----------------------------------------------------------------------------
# Logis du gouverneur

def build_governor_house(coll, p=None):
    p = {**FK.mod("governor_house"), **(p or {})}
    M = FM.mats()
    rng = random.Random(121)
    fb = G.FB("GovernorHouse")
    W, D = p["footprint_m"]
    g0, g1 = p["gallery_m"]["y_local"]
    pl = p["plinth"]["height_m"]
    l0, l1 = p["levels_z_local"]
    eave = p["eave_z"]
    t = 0.5
    dw, dh = p["openings"]["main_door_m"]
    ow, oh = p["openings"]["doors_m"]
    ww, wh = p["openings"]["windows_m"]
    ws = p["openings"]["window_sill_m"]
    # soubassement (pierre de taille) sous le corps et la galerie, marches au centre
    fb.blend = ASH
    fb.base = 0.0
    fb.box((0, (g0 + D / 2) / 2, pl / 2 - 0.01), (W + 0.12, D / 2 - g0 + 0.12, pl), M["ashlar"], bevel=0.02)
    fb.blend = BR
    G.slab_paving(fb, [(-W / 2, g0), (W / 2, g0), (W / 2, -D / 2), (-W / 2, -D / 2)], pl - 0.05, M["ashlar"], rng,
                  size=(0.45, 0.75), th=0.05, blend=BR)
    for k in range(3):
        zt = pl - k * 0.165
        y0 = g0 - (k + 1) * 0.28
        fb.box((0, y0 + 0.14 + (0.28 * k) / 2, zt - 0.0825 - 0.0), (2.4 + 0.3 * k, 0.28 * (k + 1) + 0.02, 0.165),
               M["ashlar"], bevel=0.01)
    # murs : 2 niveaux d'ouvertures
    up = l1
    ops = [dict(face="S", at=0.0, w=dw, zb=pl, zt=pl + dh, kind="door", sill=pl, open=100)]
    for x in (-5.5, -3.0, 3.0, 5.5):
        ops.append(dict(face="S", at=x, w=ww, zb=pl + ws, zt=pl + ws + wh, kind="window", shutters=True))
    for x in (-3.0, 0.0, 3.0):
        ops.append(dict(face="S", at=x, w=ow, zb=up, zt=up + oh, kind="door", sill=up, open=95))
    for x in (-5.5, 5.5):
        ops.append(dict(face="S", at=x, w=ww, zb=up + ws, zt=up + ws + wh, kind="window", shutters=True))
    for x in (-5.0, -2.0, 2.0, 5.0):
        for z in (pl, up):
            ops.append(dict(face="N", at=x, w=ww, zb=z + ws, zt=z + ws + wh, kind="window", shutters=True))
    for face in ("E", "W"):
        for z in (pl, up):
            ops.append(dict(face=face, at=0.0, w=ww, zb=z + ws, zt=z + ws + wh, kind="window", shutters=True))
    ucx = BL.walls(fb, M, W, D, pl, eave, t, ops, rng, iron="tamanu")
    BL.cornice(fb, M, W, D, eave - 0.16)
    # interieur : carreaux de terre cuite au rez, plancher de tamanu a l'etage (tremie d'escalier), cloisons
    hi_x, hi_y = W / 2 - t, D / 2 - t
    BL.tiled_floor(fb, M, -hi_x, hi_x, -hi_y, hi_y, pl + 0.02, rng, mat="brick")
    st_x0, st_x1 = 0.35, 1.35
    st_y0 = -hi_y + 0.3
    n_st = 16
    hole = (st_x0, st_x1, st_y0 + 2.2, st_y0 + n_st * 0.28 + 0.1)
    for r in _rects_minus(-hi_x, hi_x, -hi_y, hi_y, hole):
        BL.plank_floor(fb, M, r[0], r[1], r[2], r[3], up, rng, mat="tamanu", w=0.24)
    fb.blend = RUB
    for x in [-6.0 + 1.0 * k for k in range(13)]:
        if st_x0 - 0.1 < x < st_x1 + 0.1:
            continue
        fb.wood((x, 0, up - 0.16), (0.14, 2 * hi_y, 0.2), M["tamanu"], bevel=0.008, long_u=False)
    BL.plank_floor(fb, M, -hi_x, hi_x, -hi_y, hi_y, eave + 0.04, rng, mat="tamanu", w=0.26, th=0.04)
    for xp in (-1.5, 1.5):
        for z0, z1 in ((pl, up - 0.25), (up, eave)):
            fb.blend = REND
            for y0, y1 in ((-hi_y, -0.5), (0.5, hi_y)):
                fb.box((xp, (y0 + y1) / 2, (z0 + z1) / 2), (0.2, y1 - y0, z1 - z0), M["rubble"], bevel=0.0)
            fb.box((xp, 0, (z0 + 1.8 + z1) / 2), (0.2, 1.0, z1 - z0 - 1.8), M["rubble"], bevel=0.0)
    # escalier droit 16 marches x 1,0 (le long de la cloison est du vestibule)
    fb.blend = RUB
    for k in range(n_st):
        y = st_y0 + k * 0.28
        fb.wood(((st_x0 + st_x1) / 2, y + 0.14, pl + (k + 1) * 0.165 - 0.025), (1.0, 0.3, 0.05), M["tamanu"],
                bevel=0.005)
    ang = math.atan2(n_st * 0.165, n_st * 0.28)
    for x in (st_x0 + 0.03, st_x1 - 0.03):
        fb.wood((x, st_y0 + n_st * 0.14, pl + n_st * 0.0825 - 0.1), (0.05, n_st * 0.28 / math.cos(ang) + 0.2, 0.25),
                M["tamanu"], R=Matrix.Rotation(ang, 3, "X"), bevel=0.005, long_u=False)
    rail = [V(st_x0 + 0.03, st_y0 + 0.1, pl + 0.95), V(st_x0 + 0.03, st_y0 + n_st * 0.28, up + 0.9)]
    fb.tube(rail, 0.025, M["tamanu"], segs=6)
    for k in range(0, n_st, 3):
        y = st_y0 + k * 0.28 + 0.14
        fb.cyl(V(st_x0 + 0.03, y, pl + (k + 1) * 0.165), V(st_x0 + 0.03, y, pl + (k + 1) * 0.165 + 0.9), 0.015,
               M["tamanu"], segs=6)
    # mobilier : salle (table + 6 chaises, buffet), cabinet (bureau, coffre-fort, ratelier), chambres
    BL.table(fb, M, (-4.0, 0.0, pl), (2.0, 0.9, 0.7), rng)
    for k in range(3):
        chair(fb, M, (-4.7 + 0.7 * k, -0.75, pl), rng, ang=math.pi)
        chair(fb, M, (-4.7 + 0.7 * k, 0.75, pl), rng)
    fb.blend = RUB
    fb.wood((-4.0, hi_y - 0.3, pl + 0.5), (1.6, 0.5, 1.0), M["tamanu"], bevel=0.01)
    fb.wood((-4.0, hi_y - 0.3, pl + 1.02), (1.7, 0.56, 0.05), M["tamanu"], bevel=0.008)
    dx, dy = p["sockets"]["Desk"][:2]
    BL.table(fb, M, (dx, dy - 0.4, pl), (1.3, 0.7, 0.72), rng)
    chair(fb, M, (dx, dy - 1.05, pl), rng, ang=math.pi)
    sx_, sy_ = p["sockets"]["Strongbox"][:2]
    BL.chest(fb, M, (sx_, sy_ - 0.2, pl), (0.55, 0.4, 0.45), rng, wood="tamanu", iron="tamanu")
    fb.blend = RUB
    fb.wood((hi_x - 0.06, -1.5, pl + 1.3), (0.06, 1.2, 0.1), M["tamanu"], bevel=0.006)
    BL.chest(fb, M, (-4.8, 1.8, up), (0.9, 0.5, 0.5), rng, wood="tamanu", iron="tamanu")
    bed(fb, M, (-4.5, 1.3, up), rng, posts=True)
    bed(fb, M, (4.5, 1.3, up), rng, posts=False)
    BL.table(fb, M, (3.0, 2.3, up), (0.8, 0.5, 0.75), rng)
    # galerie : poteaux de tamanu sur 2 niveaux, balcon a balustrade, plafond
    fb.blend = RUB
    xs = [-W / 2 + 0.1 + k * (W - 0.2) / 7 for k in range(8)]
    for x in xs:
        fb.wood((x, g0 + 0.12, (pl + eave) / 2), (0.18, 0.18, eave - pl), M["tamanu"], bevel=0.015, long_u=False)
        fb.wood((x, g0 + 0.12, pl + 0.08), (0.28, 0.28, 0.16), M["ashlar"], bevel=0.02, long_u=False)
        for zz in (up - 0.2, eave - 0.2):
            fb.wood((x, g0 + 0.32, zz), (0.12, 0.45, 0.12), M["tamanu"],
                    R=Matrix.Rotation(-0.7, 3, "X"), bevel=0.006, long_u=False)
    fb.wood((0, g0 + 0.12, eave - 0.06), (W, 0.2, 0.12), M["tamanu"], bevel=0.01)
    fb.wood((0, g0 + 0.12, up - 0.1), (W, 0.2, 0.2), M["tamanu"], bevel=0.01)
    BL.plank_floor(fb, M, -W / 2, W / 2, g0, -D / 2, up, rng, mat="tamanu", w=0.22, along_x=False)
    for y in (g0 + 0.5, g0 + 1.2):
        fb.wood((0, y, up - 0.14), (W, 0.12, 0.16), M["tamanu"], bevel=0.006)
    # balustrade (main courante 0,66 + balustres)
    zb = up + 0.66
    fb.wood((0, g0 + 0.05, zb), (W - 0.1, 0.1, 0.06), M["tamanu"], bevel=0.008)
    fb.wood((0, g0 + 0.05, up + 0.06), (W - 0.1, 0.1, 0.05), M["tamanu"], bevel=0.008)
    nb_ = int((W - 0.2) / 0.16)
    for k in range(nb_):
        x = -W / 2 + 0.12 + k * (W - 0.24) / (nb_ - 1)
        if any(abs(x - xp) < 0.12 for xp in xs):
            continue
        fb.wood((x, g0 + 0.05, up + 0.36), (0.04, 0.04, 0.56), M["tamanu"], bevel=0.0, long_u=False)
    for sx in (-1, 1):
        fb.wood((sx * (W / 2 - 0.05), (g0 - D / 2) / 2, zb), (0.1, -D / 2 - g0, 0.06), M["tamanu"], bevel=0.008,
                long_u=False)
    # toit a croupes sur le corps + la galerie, souche de cheminee en briques
    zr, rp = BL.roof(fb, M, W, D / 2 - g0, eave, p["roof"]["pitch_deg"], rng, hip=True, overhang=0.45,
                     cy=(g0 + D / 2) / 2)
    BL.chimney(fb, M, -4.0, 1.6, eave - 0.2, zr + 0.4, rng, mat="brick", w=0.7, d=0.55)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx]
    ucx += [("socle", None, _box_ucx((0, (g0 + D / 2) / 2, pl / 2), (W, D / 2 - g0, pl))),
            ("etage", None, _box_ucx((-3.5, 0, up - 0.05), (6.0, 2 * hi_y, 0.1))),
            ("etage_e", None, _box_ucx((4.0, 0, up - 0.05), (5.0, 2 * hi_y, 0.1))),
            ("balcon", None, _box_ucx((0, (g0 - D / 2) / 2, up - 0.05), (W, -D / 2 - g0, 0.1))),
            ("garde_corps", None, _box_ucx((0, g0 + 0.05, up + 0.35), (W, 0.12, 0.7))),
            ("escalier", None, [V(x, y, z) for x in (st_x0, st_x1) for y, z in ((st_y0, pl), (st_y0 + n_st * 0.28, pl),
                                                                               (st_y0 + n_st * 0.28, up))]),
            ("toit", None, rp)]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p,
                  info={"ridge_z": round(zr, 3), "clear_height_m": round(up - pl - 0.05, 3)})


def _rects_minus(x0, x1, y0, y1, hole):
    hx0, hx1, hy0, hy1 = hole
    out = [(x0, hx0, y0, y1), (hx1, x1, y0, y1), (hx0, hx1, y0, hy0), (hx0, hx1, hy1, y1)]
    return [r for r in out if r[1] - r[0] > 0.05 and r[3] - r[2] > 0.05]


# ----------------------------------------------------------------------------
# Caserne

def build_barracks(coll, p=None):
    p = {**FK.mod("barracks"), **(p or {})}
    M = FM.mats()
    rng = random.Random(131)
    fb = G.FB("Barracks")
    W, D = p["footprint_m"]
    fz, eave = p["floor_z"], p["eave_z"]
    t = 0.5
    dw, dh = p["openings"]["door_m"]
    ww, wh = p["openings"]["window_m"]
    ws = p["openings"]["window_sill_m"] + fz
    ops = [dict(face="S", at=x, w=dw, zb=0.0, zt=fz + dh, kind="door", sill=fz, open=100) for x in (-4.0, 4.0)]
    ops += [dict(face="S", at=x, w=ww, zb=ws, zt=ws + wh, kind="window", shutters=True) for x in (-6.5, 0.0, 6.5)]
    ops += [dict(face="N", at=x, w=ww, zb=ws, zt=ws + wh, kind="window", shutters=True) for x in (-5.0, 5.0)]
    ops += [dict(face="E", at=0.0, w=ww, zb=ws, zt=ws + wh, kind="window", shutters=True)]
    BL.floor_slab(fb, M, [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2)], fz)
    hi_x, hi_y = W / 2 - t, D / 2 - t
    G.slab_paving(fb, [(-hi_x, -hi_y), (hi_x, -hi_y), (hi_x, hi_y), (-hi_x, hi_y)], fz - 0.04, M["ashlar"], rng,
                  size=(0.4, 0.7), th=0.04, blend=BR)
    ucx = BL.walls(fb, M, W, D, 0.0, eave, t, ops, rng, iron="tamanu")
    pitch = p["roof"]["pitch_deg"]
    G.gable_fill(fb, 0, 0, W, D, eave, pitch, t, M["rubble"])
    fb.blend = ASH
    zr, rp = BL.roof(fb, M, W + 0.5, D, eave, pitch, rng, hip=False, overhang=0.4)
    # cloison + plafond
    fb.blend = REND
    for y0, y1 in ((-hi_y, -1.0), (0.0, hi_y)):
        fb.box((0, (y0 + y1) / 2, (fz + eave) / 2), (0.2, y1 - y0, eave - fz), M["rubble"], bevel=0.0)
    fb.box((0, -0.5, (fz + 1.8 + eave) / 2), (0.2, 1.0, eave - fz - 1.8), M["rubble"], bevel=0.0)
    BL.plank_floor(fb, M, -hi_x, hi_x, -hi_y, hi_y, eave + 0.04, rng, mat="tamanu", w=0.26, th=0.04)
    fb.blend = RUB
    for x in [-7.0 + 1.0 * k for k in range(15)]:
        fb.wood((x, 0, eave - 0.08), (0.12, 2 * hi_y, 0.16), M["tamanu"], bevel=0.006, long_u=False)
    # lits a deux etages le long du mur nord, coffres, table + bancs, ratelier
    for side in (-1, 1):
        for k in range(5):
            x = side * (0.85 + k * 1.38)
            bunk(fb, M, (x, hi_y - 0.32, fz), rng)
            BL.chest(fb, M, (x, hi_y - 0.85, fz), (0.55, 0.32, 0.3), rng, wood="tamanu", iron="tamanu")
        BL.table(fb, M, (side * 3.6, -0.9, fz), (1.8, 0.7, 0.66), rng)
        BL.bench(fb, M, (side * 3.6, -1.5, fz), 1.8, rng)
    fb.blend = RUB
    rx, ry = p["sockets"]["WeaponRack"][:2]
    fb.wood((rx + 0.14, ry - 0.5, fz + 1.35), (0.06, 1.2, 0.1), M["tamanu"], bevel=0.006, long_u=False)
    fb.wood((rx + 0.14, ry - 0.5, fz + 0.2), (0.12, 1.2, 0.08), M["tamanu"], bevel=0.006, long_u=False)
    for k in range(7):
        y = ry - 1.0 + k * 0.16
        fb.tube([V(rx + 0.2, y, fz + 0.16), V(rx + 0.16, y, fz + 1.5)], [0.022, 0.013], M["tamanu"], segs=5)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx] + [("toit", None, rp)]
    ucx += [("lits_" + str(s), None, _box_ucx((s * 3.6, hi_y - 0.32, fz + 0.85), (7.0, 0.6, 1.7))) for s in (-1, 1)]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"ridge_z": round(zr, 3), "bunks": 20})


# ----------------------------------------------------------------------------
# Cuisine-boulangerie

def build_kitchen(coll, p=None):
    p = {**FK.mod("kitchen"), **(p or {})}
    M = FM.mats()
    rng = random.Random(141)
    fb = G.FB("Kitchen")
    W, D = p["footprint_m"]
    fz, eave = p["floor_z"], p["eave_z"]
    t = 0.5
    dw, dh = p["door_m"]
    ops = [dict(face="S", at=0.0, w=dw, zb=0.0, zt=fz + dh, kind="door", sill=fz, open=100),
           dict(face="S", at=-1.9, w=0.7, zb=fz + 0.9, zt=fz + 1.7, kind="window", bars=True),
           dict(face="E", at=0.0, w=0.7, zb=fz + 0.9, zt=fz + 1.7, kind="window", bars=True)]
    BL.floor_slab(fb, M, [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2)], fz, mat="brick",
                  blend=RUB)
    ucx = BL.walls(fb, M, W, D, 0.0, eave, t, ops, rng, frame_mat="brick")
    pitch = 35
    G.gable_fill(fb, 0, 0, W, D, eave, pitch, t, M["rubble"])
    zr, rp = BL.roof(fb, M, W + 0.5, D, eave, pitch, rng, hip=False, overhang=0.35)
    # foyer ouvert (briques) + hotte + souche
    hx, hy, _hz = p["sockets"]["Fire"]
    hw, hd, mz = p["hearth"]["width_m"], p["hearth"]["depth_m"], p["hearth"]["mantel_z"]
    yb = D / 2 - t
    fb.blend = RUB
    fb.box((hx, yb - 0.05, (fz + eave) / 2), (hw + 0.6, 0.1, eave - fz), M["brick"], bevel=0.0)
    for sx in (-1, 1):
        fb.box((hx + sx * (hw / 2 + 0.15), yb - hd / 2, fz + mz / 2), (0.3, hd, mz), M["brick"], bevel=0.01)
    fb.box((hx, yb - hd / 2, fz + 0.1), (hw, hd, 0.2), M["brick"], bevel=0.01)
    hood = [V(hx - hw / 2 - 0.3, yb - hd, fz + mz), V(hx + hw / 2 + 0.3, yb - hd, fz + mz), V(hx + hw / 2 + 0.3, yb, fz + mz),
            V(hx - hw / 2 - 0.3, yb, fz + mz)]
    top = [V(hx - 0.4, yb - 0.4, eave), V(hx + 0.4, yb - 0.4, eave), V(hx + 0.4, yb, eave), V(hx - 0.4, yb, eave)]
    fb.loft(hood, top, M["brick"])
    fb.box((hx, yb - hd - 0.04, fz + mz + 0.05), (hw + 0.7, 0.12, 0.12), M["tamanu"], bevel=0.01)
    BL.chimney(fb, M, hx, yb - 0.2, eave - 0.3, p["chimney_top_z"] - 0.25, rng, mat="brick", w=0.8, d=0.6)
    # cremaillere + chaudron, buches
    fb.cyl(V(hx, yb - 0.25, fz + mz + 0.3), V(hx, yb - 0.25, fz + 0.75), 0.012, M["iron"], segs=5)
    pot = [(0.0, fz + 0.32), (0.16, fz + 0.34), (0.22, fz + 0.45), (0.21, fz + 0.6), (0.19, fz + 0.63)]
    from fort_rampart import _revolve
    tmp = _revolve(pot, 14)
    tmp.transform(Matrix.Translation((hx, yb - 0.25, 0)))
    fb.add_bm(tmp, M["iron"], smooth=True)
    for k in range(4):
        a = rng.uniform(-0.4, 0.4)
        fb.tube([V(hx - 0.4, yb - 0.3 + k * 0.08, fz + 0.24), V(hx + 0.4, yb - 0.3 + k * 0.08 + a * 0.2, fz + 0.26)],
                0.045, M["tamanu"], segs=6)
    # four a pain : socle + coupole de briques, bouche 0,45 x 0,33
    ox, oy = 1.35, D / 2 - t - 0.85
    od = p["oven"]["dome_d_m"]
    fb.box((ox, oy, fz + 0.35), (od + 0.3, od + 0.2, 0.7), M["brick"], bevel=0.01)
    prof = [(od / 2 * math.cos(math.pi / 2 * k / 6), fz + 0.7 + od / 2 * 0.8 * math.sin(math.pi / 2 * k / 6))
            for k in range(7)]
    tmp = _revolve([(od / 2, fz + 0.7)] + prof[1:], 16)
    tmp.transform(Matrix.Translation((ox, oy, 0)))
    fb.add_bm(tmp, M["brick"], smooth=True)
    fb.box((ox, oy - od / 2 - 0.02, fz + 0.7 + 0.17), (0.6, 0.12, 0.42), M["brick"], bevel=0.01)
    fb.box((ox, oy - od / 2 - 0.09, fz + 0.7 + 0.15), (0.45, 0.04, 0.33), M["iron"], bevel=0.0)
    BL.table(fb, M, (-0.3, -0.8, fz), (1.6, 0.7, 0.8), rng)
    fb.blend = RUB
    fb.wood((-W / 2 + t + 0.25, 0.0, fz + 1.4), (0.35, 2.4, 0.04), M["tamanu"], bevel=0.004)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx] + [("toit", None, rp)]
    ucx += [("four", None, _box_ucx((ox, oy, fz + 0.75), (od + 0.3, od + 0.2, 1.5))),
            ("table", None, _box_ucx((-0.3, -0.8, fz + 0.4), (1.6, 0.7, 0.8)))]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"ridge_z": round(zr, 3)})


# ----------------------------------------------------------------------------
# Citerne

def build_cistern(coll, p=None):
    p = {**FK.mod("cistern"), **(p or {})}
    M = FM.mats()
    rng = random.Random(151)
    fb = G.FB("Cistern")
    S = p["footprint_m"][0]
    ch = p["curb_height_m"]
    rd = p["well_d_m"] / 2
    from fort_rampart import _revolve
    fb.blend = BR
    G.slab_paving(fb, [(-S / 2, -S / 2), (S / 2, -S / 2), (S / 2, S / 2), (-S / 2, S / 2)], 0.0, M["ashlar"], rng,
                  size=(0.45, 0.8), th=0.08, holes=lambda c: c.length < rd + 0.45)
    fb.blend = ASH
    ro = rd + 0.3
    prof = [(rd, -0.6), (rd, ch - 0.02), (rd + 0.03, ch + 0.02), (ro - 0.03, ch + 0.02), (ro + 0.02, ch - 0.04),
            (ro, 0.0), (ro, -0.05)]
    fb.add_bm(_revolve(prof, 20), M["ashlar"], smooth=False)
    fb.blend = (1.0, 0.0, 0.0)
    fb.add_bm(_revolve([(0.0, -0.6), (rd + 0.01, -0.6)], 20), M["ashlar"], smooth=False)
    # potence : 2 montants, traverse, poulie, corde, seau
    fb.blend = RUB
    hgt = p["frame_height_m"]
    for sx in (-1, 1):
        fb.wood((sx * (ro + 0.05), 0, hgt / 2 + 0.2), (0.14, 0.14, hgt + 0.2), M["tamanu"], bevel=0.01,
                long_u=False)
        fb.wood((sx * (ro + 0.05), 0, 0.15), (0.3, 0.3, 0.3), M["ashlar"], bevel=0.02)
    fb.wood((0, 0, hgt + 0.25), (2 * ro + 0.35, 0.16, 0.16), M["tamanu"], bevel=0.01)
    wheel = [V(0.02 * math.sin(TAU * i / 14) * 0, 0.16 * math.cos(TAU * i / 14), hgt + 0.05 + 0.16 * math.sin(TAU * i / 14))
             for i in range(14)]
    fb.tube(wheel, 0.025, M["iron"], segs=5, closed=True)
    fb.cyl(V(-0.06, 0, hgt + 0.05), V(0.06, 0, hgt + 0.05), 0.03, M["iron"], segs=8)
    fb.box((0, 0, hgt + 0.17), (0.06, 0.06, 0.2), M["iron"], bevel=0.0)
    by = p["sockets"]["Bucket"][2]
    fb.tube([V(0, 0.16, hgt + 0.05), V(0, 0.16, by + 0.25)], 0.01, M["rope"], segs=5)
    fb.tube([V(0, -0.16, hgt + 0.05), V(0, -0.2, ch + 0.3), V(0.0, -ro - 0.1, ch - 0.1)], 0.01, M["rope"], segs=5)
    bk = [(0.0, by), (0.13, by), (0.15, by + 0.24), (0.14, by + 0.24)]
    tmp = _revolve(bk, 12)
    tmp.transform(Matrix.Translation((0, 0.16, 0)))
    fb.add_bm(tmp, M["tamanu"], smooth=True)
    for z in (by + 0.05, by + 0.19):
        ring = [V(0.145 * math.cos(TAU * i / 12), 0.16 + 0.145 * math.sin(TAU * i / 12), z) for i in range(12)]
        fb.tube(ring, 0.006, M["iron"], segs=4, closed=True)
    arc = [V(0.15 * math.cos(math.pi * k / 8), 0.16, by + 0.24 + 0.12 * math.sin(math.pi * k / 8)) for k in range(9)]
    fb.tube(arc, 0.006, M["iron"], segs=4)
    ob = fb.finish(coll)
    ucx = [("margelle", None, [V(ro * math.cos(TAU * k / 12), ro * math.sin(TAU * k / 12), z) for k in range(12)
                               for z in (0, ch)]),
           ("dalle", None, _box_ucx((0, 0, -0.04), (S, S, 0.08)))]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"curb_m": ch})


# ----------------------------------------------------------------------------
# Magasin a poudre

def build_powder_magazine(coll, p=None):
    p = {**FK.mod("powder_magazine"), **(p or {})}
    M = FM.mats()
    rng = random.Random(161)
    fb = G.FB("PowderMagazine")
    W, D = p["body_m"]
    t = p["walls_m"]
    vs, vc = p["vault"]["springing_z"], p["vault"]["crown_z"]
    ztop = vc + 0.35
    ridge = 4.0
    dw, dh = p["door"]["size_m"]
    ops = [dict(face="S", at=0.0, w=dw, zb=0.0, zt=dh, kind="door", sill=0.0, leaf=False)]
    for face in ("E", "W"):
        ops.append(dict(face=face, at=0.0, w=0.1, zb=0.8, zt=1.2, kind="slit"))
    ops += [dict(face="N", at=x, w=0.1, zb=0.8, zt=1.2, kind="slit") for x in (-1.2, 1.2)]
    ucx = BL.walls(fb, M, W, D, 0.0, ztop, t, ops, rng, quoin=True)
    # voute en berceau (axe X) : intrados
    hi_y = D / 2 - t
    pts = BL.arch_points(-hi_y, hi_y, vs, vc, 12)
    poly = pts + [(hi_y, ztop), (-hi_y, ztop)]
    k = len(poly)
    verts = [V(-W / 2 + t, y, z) for y, z in poly] + [V(W / 2 - t, y, z) for y, z in poly]
    faces = [tuple(range(k)), tuple(range(k, 2 * k))[::-1]]
    for i in range(k):
        j = (i + 1) % k
        faces.append((i, j, k + j, k + i))
    fb.blend = REND
    fb.poly_bm(verts, faces, M["rubble"])
    # pignons + toit de dalles (beachrock) en batiere, faitage a 4,0
    pitch = math.degrees(math.atan2(ridge - ztop, D / 2 + 0.3))
    G.gable_fill(fb, 0, 0, W, D, ztop, pitch, 0.6, M["rubble"])
    fb.blend = BR
    zr, rp = G.hip_roof(fb, 0, 0, W + 0.3, D, ztop, pitch, 0.3, M["ashlar"], th=0.14, hip=False)
    for sy in (-1, 1):
        for k_ in range(1, 4):
            y = sy * (D / 2 + 0.3) * (1 - k_ / 4)
            z = ztop - 0.3 * math.tan(math.radians(pitch)) + (D / 2 + 0.3 - abs(y)) * math.tan(math.radians(pitch))
            fb.box((0, y, z + 0.075), (W + 0.4, 0.04, 0.03), M["ashlar"], bevel=0.0,
                   R=Matrix.Rotation(-sy * math.radians(pitch), 3, "X"))
    # contreforts (3 par long cote)
    fb.blend = REND
    for sy in (-1, 1):
        for x in (-2.0, 0.0, 2.0) if sy > 0 else (-2.0, 2.0):
            y0 = sy * D / 2
            prof = [(0.0, 0.0), (0.8, 0.0), (0.8, 0.5), (0.0, ztop - 0.2)]
            verts = [V(x - 0.3, y0 + sy * a, z) for a, z in prof] + [V(x + 0.3, y0 + sy * a, z) for a, z in prof]
            faces = [(0, 1, 2, 3), (7, 6, 5, 4), (0, 4, 5, 1), (1, 5, 6, 2), (2, 6, 7, 3), (3, 7, 4, 0)]
            fb.poly_bm(verts, faces, M["rubble"])
    # porte doublee de cuivre (clous de bronze), entrouverte
    fb.blend = RUB
    hinge = V(-dw / 2 + 0.05, -D / 2 + 0.3, 0)
    R = Matrix.Rotation(math.radians(-35), 3, "Z")
    fb.box(hinge + R @ V(dw / 2 - 0.05, 0, 0) + V(0, 0, dh / 2 - 0.01), (dw - 0.1, 0.08, dh - 0.03), M["copper"],
           R=R, bevel=0.006)
    pts_ = [hinge + R @ V(0.08 + 0.15 * i, -0.045, 0) + V(0, 0, 0.15 + 0.2 * j) for i in range(6) for j in range(8)]
    G.studs(fb, pts_, 0.012, M["copper"], R @ V(0, -1, 0))
    # interieur : chantiers de bois (2 rangs)
    for y in (-0.55, 0.55):
        for x in (-1.5, -0.5, 0.5, 1.5):
            fb.wood((x, y, 0.08), (0.12, 0.7, 0.16), M["tamanu"], bevel=0.006, long_u=False)
        for dy in (-0.2, 0.2):
            fb.wood((0, y + dy, 0.19), (3.6, 0.08, 0.08), M["tamanu"], bevel=0.006)
    fb.blend = BR
    fb.box((0, 0, 0.0), (W - 2 * t, D - 2 * t, 0.06), M["ashlar"], bevel=0.0)
    # enclos (mur d'isolement) avec porte charretiere 1,2
    ew, ed = p["enclosure"]["size_m"]
    eh = p["enclosure"]["height_m"]
    gw = p["enclosure"]["gate_m"]
    et = 0.4
    fb.blend = REND
    segs = [((-ew / 2, -ed / 2), (-gw / 2, -ed / 2)), ((gw / 2, -ed / 2), (ew / 2, -ed / 2)),
            ((ew / 2, -ed / 2), (ew / 2, ed / 2)), ((ew / 2, ed / 2), (-ew / 2, ed / 2)),
            ((-ew / 2, ed / 2), (-ew / 2, -ed / 2))]
    for a, b in segs:
        o, u, n, L = G.seg_frame(a, b)
        q = [o, o + u * L, o + u * L - n * et, o - n * et]
        G.block(fb, [(pp.x, pp.y) for pp in q], 0.0, eh, M["rubble"])
        ucx.append([V(pp.x, pp.y, z) for pp in q for z in (0, eh)])
        fb.blend = ASH
        tt = 0.0
        while tt < L - 0.05:
            ln = min(rng.uniform(0.5, 0.8), L - tt)
            c = o + u * (tt + ln / 2) - n * et / 2
            fb.box((c.x, c.y, eh + 0.04), (ln - 0.01, et + 0.06, 0.08), M["ashlar"],
                   R=Matrix.Rotation(math.atan2(u.y, u.x), 3, "Z"), bevel=0.012)
            tt += ln
        fb.blend = REND
    for sx in (-1, 1):
        fb.blend = ASH
        fb.box((sx * (gw / 2 + 0.2), -ed / 2 + et / 2, eh / 2 + 0.15), (0.4, 0.5, eh + 0.3), M["ashlar"], bevel=0.02)
        fb.sphere((sx * (gw / 2 + 0.2), -ed / 2 + et / 2, eh + 0.42), 0.14, M["ashlar"], u=10, v=6)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx] + [("toit", None, rp)]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"vault": [vs, vc], "ridge_z": ridge})


# ----------------------------------------------------------------------------
# Chapelle

def build_chapel(coll, p=None):
    p = {**FK.mod("chapel"), **(p or {})}
    M = FM.mats()
    rng = random.Random(171)
    fb = G.FB("Chapel")
    W, D = p["footprint_m"]
    fz, eave = p["floor_z"], p["eave_z"]
    t = 0.55
    dw, dh = p["door_m"]
    pitch = 40
    ops = [dict(face="S", at=0.0, w=dw, zb=0.0, zt=fz + dh, kind="door", sill=fz, open=95)]
    for face in ("E", "W"):
        for y in (-1.6, 1.6):
            ops.append(dict(face=face, at=y, w=0.6, zb=fz + 1.0, zt=fz + 2.3, kind="arch"))
    for k in range(2):
        fb.blend = ASH
        fb.box((0, -D / 2 - 0.14 - 0.28 * k, (fz - 0.165 * k) / 2), (dw + 0.8 - 0.2 * k, 0.3, fz - 0.165 * k),
               M["ashlar"], bevel=0.01)
    BL.floor_slab(fb, M, [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2)], fz)
    G.slab_paving(fb, [(-W / 2 + t, -D / 2 + t), (W / 2 - t, -D / 2 + t), (W / 2 - t, D / 2 - t),
                       (-W / 2 + t, D / 2 - t)], fz - 0.04, M["ashlar"], rng, size=(0.4, 0.6), th=0.04, blend=BR)
    ucx = BL.walls(fb, M, W, D, 0.0, eave, t, ops, rng, iron="tamanu")
    # pignons (axe Y) + toit
    G.gable_fill(fb, 0, 0, D, W, eave, pitch, t, M["rubble"], ang=math.pi / 2)
    zr, rp = BL.roof(fb, M, D + 0.4, W, eave, pitch, rng, hip=False, overhang=0.35, ang=math.pi / 2, rafters=False)
    # clocher-mur sur le pignon de facade
    bt = p["bellcote"]["top_z"]
    fb.blend = REND
    yb = -D / 2 + t / 2
    z0 = zr - 0.6
    bz = p["sockets"]["Bell"][2]
    br = p["bellcote"]["bell_d_m"] / 2
    zo0, zo1 = bz - 0.25, bz + 0.55                      # baie de la cloche (ajouree), tete en plein cintre
    ow = 0.42
    fb.box((0, yb, (z0 + zo0) / 2), (1.5, t, zo0 - z0), M["rubble"], bevel=0.0)
    for sx in (-1, 1):
        fb.box((sx * (ow + (0.75 - ow) / 2), yb, (zo0 + bt - 0.5) / 2), (0.75 - ow, t, bt - 0.5 - zo0), M["rubble"],
               bevel=0.0)
    M4 = G.frame_M4(V(0, yb - t / 2, 0), V(1, 0, 0), V(0, -1, 0))
    BL.arch_infill(fb, M, M4, 2 * ow, zo1 - ow, zo1, t, REND, "ashlar")
    fb.box((0, yb, (zo1 + bt - 0.5) / 2), (2 * ow, t, bt - 0.5 - zo1), M["rubble"], bevel=0.0)
    fb.blend = ASH
    fb.box((0, yb, zo0 - 0.04), (2 * ow + 0.1, t + 0.06, 0.08), M["ashlar"], bevel=0.01)
    tri = [(-0.85, bt - 0.5), (0.85, bt - 0.5), (0.0, bt)]
    verts = [V(x, yb - t / 2 - 0.03, z) for x, z in tri] + [V(x, yb + t / 2 + 0.03, z) for x, z in tri]
    fb.poly_bm(verts, [(0, 1, 2), (5, 4, 3), (0, 3, 4, 1), (1, 4, 5, 2), (2, 5, 3, 0)], M["ashlar"])
    from fort_rampart import _revolve
    prof = [(0.0, bz + 0.33), (br * 0.45, bz + 0.33), (br * 0.55, bz + 0.25), (br * 0.65, bz + 0.1),
            (br * 0.9, bz + 0.02), (br, bz - 0.02), (br * 0.96, bz - 0.03), (br * 0.4, bz + 0.0)]
    fb.blend = RUB
    tmp = _revolve(prof, 18)
    tmp.transform(Matrix.Translation((0, yb, 0.0)))
    fb.add_bm(tmp, M["copper"], smooth=True)
    fb.wood((0, yb, bz + 0.38), (2 * ow + 0.3, 0.1, 0.1), M["tamanu"], bevel=0.006)
    fb.tube([V(0, yb, bz + 0.33), V(0, yb, bz + 0.38)], 0.03, M["copper"], segs=8)
    # interieur : autel, croix, bancs, benitier
    fb.blend = ASH
    fb.box((0, D / 2 - t - 0.5, fz + 0.5), (1.6, 0.7, 1.0), M["ashlar"], bevel=0.02)
    fb.box((0, D / 2 - t - 0.5, fz + 1.03), (1.75, 0.82, 0.07), M["ashlar"], bevel=0.015)
    fb.blend = RUB
    fb.wood((0, D / 2 - t - 0.1, fz + 2.0), (0.08, 0.06, 1.2), M["tamanu"], bevel=0.005, long_u=False)
    fb.wood((0, D / 2 - t - 0.1, fz + 2.25), (0.6, 0.06, 0.08), M["tamanu"], bevel=0.005)
    for k in range(4):
        for sx in (-1, 1):
            BL.bench(fb, M, (sx * 0.95, -1.6 + k * 0.85, fz), 1.4, rng, h=0.36)
    shell = [(0.0, fz + 0.7), (0.18, fz + 0.72), (0.26, fz + 0.82), (0.24, fz + 0.84), (0.12, fz + 0.76),
             (0.0, fz + 0.75)]
    tmp = _revolve(shell, 16)
    tmp.transform(Matrix.Translation((0.9, -D / 2 + t + 0.4, 0)))
    fb.blend = ASH
    fb.add_bm(tmp, M["ashlar"], smooth=True)
    fb.cyl(V(0.9, -D / 2 + t + 0.4, fz), V(0.9, -D / 2 + t + 0.4, fz + 0.7), 0.08, M["ashlar"], segs=10)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx] + [("toit", None, rp),
                                            ("autel", None, _box_ucx((0, D / 2 - t - 0.5, fz + 0.5), (1.75, 0.82, 1.06)))]
    for k in range(4):
        ucx.append((f"bancs{k}", None, _box_ucx((0, -1.6 + k * 0.85, fz + 0.18), (3.4, 0.3, 0.36))))
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"ridge_z": round(zr, 3), "bellcote_top": bt})


MODULES = {
    "governor_house": ("SM_Fort_GovernorHouse", build_governor_house),
    "barracks": ("SM_Fort_Barracks", build_barracks),
    "kitchen": ("SM_Fort_Kitchen", build_kitchen),
    "cistern": ("SM_Fort_Cistern", build_cistern),
    "powder_magazine": ("SM_Fort_PowderMagazine", build_powder_magazine),
    "chapel": ("SM_Fort_Chapel", build_chapel),
}
