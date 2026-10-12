"""
Fort francais : tour de guet / fanal (lot 'tour').

SM_Fort_SignalTower : 5 x 5, murs 0,8 enduits, 2 niveaux praticables (planchers de chene a 2,64) + terrasse
crenelee a 5,28, escaliers de bois en L (2 volees de 8 marches de 16,5 / 25 cm + palier 1 x 1) par niveau, tremies,
trappe ferree, mat de pavillon de 6 m, rechaud de fer (fanal) sur potence.
SM_Fort_Flag : pavillon blanc seme de fleurs de lys d'or 2,4 x 1,6 (UV 0..1, UV1.x = poids de vent).
SM_Fort_Lantern : lanterne de fer et corne sur potence murale (plaque murale a l'origine, potence vers +Y).
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
REND, RUB, ASH, BR = RP.REND, RP.RUB, RP.ASH, RP.BR


def _wood_stair(fb, M, origin, ang, n, rise, tread, width, z0, rng):
    """Volee de bois : 2 limons + marches (repere : monte vers +Y local tourne de ang)."""
    R = Matrix.Rotation(ang, 3, "Z")
    o = Vector(origin)
    L = n * tread
    H = n * rise
    slope = math.atan2(H, L)
    fb.blend = RUB
    for sx in (-1, 1):
        c = o + R @ V(sx * (width / 2 - 0.03), L / 2, 0) + V(0, 0, z0 + H / 2 - 0.05)
        fb.wood(c, (0.05, math.hypot(L, H) + 0.15, 0.22), M["oak"], R=R @ Matrix.Rotation(slope, 3, "X"),
                bevel=0.006, long_u=False)
    for k in range(n):
        c = o + R @ V(0, k * tread + tread / 2, 0) + V(0, 0, z0 + (k + 1) * rise - 0.02)
        fb.wood(c, (width - 0.1, tread + 0.03, 0.04), M["oak"], R=R @ Matrix.Rotation(rng.uniform(-0.01, 0.01), 3, "Z"),
                bevel=0.006)
    return [o + R @ V(sx * width / 2, y, 0) + V(0, 0, z) for sx in (-1, 1) for y, z in
            ((0, z0), (L, z0), (L, z0 + H))]


def build_signal_tower(coll, p=None):
    p = {**FK.mod("signal_tower"), **(p or {})}
    M = FM.mats()
    rng = random.Random(101)
    fb = G.FB("SignalTower")
    W, D = p["footprint_m"]
    t = p["wall_m"]
    lv = p["levels_local_z"]
    zt = lv[2]
    dw, dh = p["door"]["size_m"]
    hi = W / 2 - t
    st = p["stairs"]
    rise, tread, sw = st["rise_m"], st["tread_m"], st["width_m"]
    ops = [dict(face="S", at=0.0, w=dw, zb=0.0, zt=dh, kind="door", sill=0.0, open=100)]
    for face in ("E", "W", "N"):
        ops.append(dict(face=face, at=0.4, w=0.15, zb=0.66, zt=1.26, kind="slit"))
    ops.append(dict(face="E", at=0.0, w=0.8, zb=lv[1] + 0.66, zt=lv[1] + 1.66, kind="window", shutters=True))
    ops.append(dict(face="N", at=-0.6, w=0.15, zb=lv[1] + 0.66, zt=lv[1] + 1.26, kind="slit"))
    ops.append(dict(face="W", at=0.0, w=0.15, zb=lv[1] + 0.66, zt=lv[1] + 1.26, kind="slit"))
    # sol du rez (dalles)
    BL.floor_slab(fb, M, [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2)], 0.02)
    ucx = BL.walls(fb, M, W, D, 0.0, zt, t, ops, rng, wood="oak")
    # cordon a la terrasse + parapet crenele
    fb.blend = ASH
    fb.sweep([V(-W / 2, -D / 2, zt), V(W / 2, -D / 2, zt), V(W / 2, D / 2, zt), V(-W / 2, D / 2, zt)],
             [(-0.1, -0.08), (0.06, -0.08), (0.1, -0.03), (0.1, 0.04), (-0.1, 0.06)], M["ashlar"], closed=True)
    Ppar = [(-W / 2, -D / 2), (W / 2, -D / 2), (W / 2, D / 2), (-W / 2, D / 2), (-W / 2, -D / 2)]
    # parapet : creneaux au pas de 2,0 (2 par face), merlons aux angles
    lay = [[("c", 1.5), ("c", 3.5)] for _ in range(4)]
    _ucxp = _tower_parapet(fb, M, Ppar, lay, zt, thick=0.6, rng=rng)
    # escaliers en L : niveau 0 (mur ouest +Y, palier NO, mur nord +X) ; niveau 1 (mur sud -X, palier SO, mur ouest +Y)
    L8 = 8 * tread
    flights = [
        (V(-hi + sw / 2, -hi + 0.4, 0), 0.0, lv[0]),
        (V(-hi + sw, hi - sw / 2, 0), -math.pi / 2, lv[0] + 8 * rise),
        (V(-hi + sw + L8, -hi + sw / 2, 0), math.pi / 2, lv[1]),
        (V(-hi + sw / 2, -hi + sw, 0), 0.0, lv[1] + 8 * rise),
    ]
    landings = [((-hi, -hi + sw, -hi + 0.4 + L8, -hi + 0.4 + L8 + sw), lv[0] + 8 * rise),
                ((-hi, -hi + sw, -hi, -hi + sw), lv[1] + 8 * rise)]
    ucx_st = []
    for o_, a_, z0 in flights:
        ucx_st.append(_wood_stair(fb, M, o_, a_, 8, rise, tread, sw, z0, rng))
    fb.blend = RUB
    for (x0, x1, y0, y1), zl in landings:
        fb.wood(((x0 + x1) / 2, (y0 + y1) / 2, zl - 0.03), (x1 - x0, y1 - y0, 0.06), M["oak"], bevel=0.006)
        fb.wood(((x0 + x1) / 2, (y0 + y1) / 2, zl - 0.12), (x1 - x0, 0.12, 0.14), M["oak"], bevel=0.006)
        for cx, cy in ((x1 - 0.05, y0 + 0.05), (x1 - 0.05, y1 - 0.05)):
            if abs(cx) < hi - 0.2 or abs(cy) < hi - 0.2:
                fb.wood((cx, cy, zl / 2 + (lv[0] if zl < lv[1] else lv[1]) / 2), (0.09, 0.09, zl - (lv[0] if zl < lv[1]
                                                                                                    else lv[1])),
                        M["oak"], bevel=0.006, long_u=False)
    # plancher du niveau 1 (chene sur solives) : tremie au-dessus de la volee nord
    hole1 = (-hi + sw, -hi + sw + L8, hi - sw, hi)
    _plank_with_hole(fb, M, -hi, hi, -hi, hi, lv[1], hole1, rng)
    fb.blend = RUB
    for x in (-1.0, 0.0, 1.0):
        for (y0, y1) in ((-hi, hi - sw),) if -hi + sw <= x <= -hi + sw + L8 else ((-hi, hi),):
            fb.wood((x, (y0 + y1) / 2, lv[1] - 0.14), (0.14, y1 - y0, 0.18), M["oak"], bevel=0.008, long_u=False)
    # garde-corps de la tremie (cote palier)
    fb.blend = RUB
    for x in (-0.65, 0.25):
        fb.wood((x, hi - sw - 0.04, lv[1] + 0.45), (0.07, 0.07, 0.9), M["oak"], bevel=0.006, long_u=False)
    fb.wood((-0.2, hi - sw - 0.04, lv[1] + 0.9), (1.0, 0.07, 0.07), M["oak"], bevel=0.006)
    # terrasse : voute plate + dalles, tremie de l'escalier ouest (trappe ferree relevee)
    hole2 = (-hi, -hi + sw, hi - 1.15, hi)
    for (x0, x1, y0, y1) in _rect_minus(-hi, hi, -hi, hi, hole2):
        fb.blend = REND
        fb.box(((x0 + x1) / 2, (y0 + y1) / 2, zt - 0.25), (x1 - x0, y1 - y0, 0.36), M["rubble"], bevel=0.0)
    for (x0, x1, y0, y1) in _rect_minus(-W / 2 + 0.6, W / 2 - 0.6, -D / 2 + 0.6, D / 2 - 0.6, hole2):
        G.slab_paving(fb, [(x0, y0), (x1, y0), (x1, y1), (x0, y1)], zt - 0.06, M["ashlar"], rng,
                      size=(0.4, 0.7), th=0.06, blend=BR)
    fb.blend = RUB
    hx0, hx1, hy0, hy1 = hole2
    trap_c = V(hx1 + 0.03, (hy0 + hy1) / 2, zt + 0.5)
    fb.wood(trap_c, (0.06, hy1 - hy0 - 0.05, 0.98), M["oak"], bevel=0.01)
    for d in (-0.3, 0.3):
        fb.box(trap_c + V(0.035, d, 0), (0.012, 0.06, 0.95), M["iron"])
    for yy in (hy0 + 0.05, hy1 - 0.05):
        fb.cyl(V(hx0 + 0.05, yy, zt), V(hx0 + 0.05, yy, zt + 0.9), 0.015, M["iron"], segs=6)
    fb.tube([V(hx0 + 0.05, hy0 + 0.05, zt + 0.9), V(hx0 + 0.05, hy1 - 0.05, zt + 0.9)], 0.015, M["iron"], segs=6)
    # mat de pavillon (6 m, haubans), fanal (rechaud sur potence) et lanterne
    fx, fy = p["sockets"]["Flag"][:2]
    mast_h = p["top"]["flagstaff_m"]
    fb.blend = RUB
    fb.tube([V(fx, fy, zt - 0.4), V(fx, fy, zt + mast_h * 0.6), V(fx, fy, zt + mast_h)], [0.075, 0.06, 0.04],
            M["oak"], segs=10)
    fb.sphere((fx, fy, zt + mast_h + 0.06), 0.07, M["oak"], u=10, v=6)
    fb.tube([V(fx, fy, zt + 0.3), V(fx, fy, zt + mast_h - 0.05)], 0.006, M["rope"], segs=4)
    fb.box((fx, fy, zt + 0.25), (0.35, 0.35, 0.5), M["oak"], bevel=0.02)
    bx, by = p["sockets"]["Fanal"][:2]
    fb.tube([V(bx, by, zt), V(bx, by, zt + 1.9)], 0.05, M["iron"], segs=8)
    fb.tube([V(bx, by, zt + 1.7), V(bx + 0.5, by + 0.0, zt + 1.85), V(bx + 0.6, by, zt + 1.85)], 0.025, M["iron"],
            segs=6)
    cage = V(bx + 0.62, by, zt + 1.55)
    for i in range(8):
        a = TAU * i / 8
        fb.tube([cage + V(0.12 * math.cos(a), 0.12 * math.sin(a), -0.12),
                 cage + V(0.24 * math.cos(a), 0.24 * math.sin(a), 0.22)], 0.012, M["iron"], segs=4)
    ring = [cage + V(0.24 * math.cos(TAU * i / 12), 0.24 * math.sin(TAU * i / 12), 0.22) for i in range(12)]
    fb.tube(ring, 0.012, M["iron"], segs=4, closed=True)
    fb.tube([cage + V(0, 0, 0.22), cage + V(0, 0, 0.3)], 0.01, M["iron"], segs=4)
    fb.blend = RUB
    for i in range(7):
        a = TAU * i / 7
        fb.wood(cage + V(0.08 * math.cos(a), 0.08 * math.sin(a), 0.02), (0.05, 0.05, 0.3), M["oak"],
                R=Matrix.Rotation(a, 3, "Z") @ Matrix.Rotation(0.5, 3, "X"), bevel=0.0, long_u=False)
    ob = fb.finish(coll)
    ucx = [("mur", None, q) for q in ucx]
    for nm, hole, z in (("plancher1", hole1, lv[1]), ("terrasse", hole2, lv[2])):
        for k, (x0, x1, y0, y1) in enumerate(_rect_minus(-hi, hi, -hi, hi, hole)):
            ucx.append((f"{nm}_{k}", None, G.rbox_pts(((x0 + x1) / 2, (y0 + y1) / 2, z - 0.1), (x1 - x0, y1 - y0, 0.2))))
    ucx += [("escalier", None, q) for q in ucx_st]
    ucx += [("parapet", None, q) for q in _ucxp]
    socks = FK.sockets_of(p["sockets"])
    socks["FlagTop"] = ("root", V(fx, fy, zt + mast_h - 0.08))
    socks["FanalFire"] = ("root", cage + V(0, 0, 0.1))
    return FK.res([ob], socks, ucx, p, info={"terrace_z_local": zt, "mast_top_z_local": zt + mast_h,
                                             "stairs": "2 x (8 + palier + 8) marches 16,5 / 25"})


def _rect_minus(x0, x1, y0, y1, hole):
    hx0, hx1, hy0, hy1 = hole
    out = []
    if hy0 > y0:
        out.append((x0, x1, y0, hy0))
    if hy1 < y1:
        out.append((x0, x1, hy1, y1))
    if hx0 > x0:
        out.append((x0, hx0, max(y0, hy0), min(y1, hy1)))
    if hx1 < x1:
        out.append((hx1, x1, max(y0, hy0), min(y1, hy1)))
    return [r for r in out if r[1] - r[0] > 0.05 and r[3] - r[2] > 0.05]


def _plank_with_hole(fb, M, x0, x1, y0, y1, z, hole, rng):
    for r in _rect_minus(x0, x1, y0, y1, hole):
        BL.plank_floor(fb, M, r[0], r[1], r[2], r[3], z, rng, mat="oak", w=0.24)


def _tower_parapet(fb, M, P, lay, zt, thick, rng):
    """Parapet de terrasse (merlons 1,32 au-dessus de la terrasse, creneaux a 0,66)."""
    ucx = []
    Q = G.offset_poly(P[:-1], -thick, closed=True)
    Q = Q + [Q[0]]
    for i in range(len(P) - 1):
        o, u, n, L = G.seg_frame(P[i], P[i + 1])
        Ai, Bi = Vector((Q[i][0], Q[i][1], 0)), Vector((Q[i + 1][0], Q[i + 1][1], 0))
        n_mark = fb.mark()

        def outer(tt):
            return o + u * tt

        def inner(tt):
            if tt <= 1e-6:
                return Ai
            if tt >= L - 1e-6:
                return Bi
            return o + u * tt - n * thick
        prev = 0.0
        for kind, c in sorted(lay[i], key=lambda q: q[1]) + [("end", L)]:
            a0, a1 = (c - 0.3, c + 0.3) if kind == "c" else (L, L)
            q = [outer(prev), outer(a0), inner(a0), inner(prev)]
            fb.blend = REND
            G.block(fb, [(pp.x, pp.y) for pp in q], zt - 0.05, zt + 1.32, M["rubble"], round_top=0.08)
            ucx.append([V(pp.x, pp.y, z) for pp in q for z in (zt, zt + 1.32)])
            if kind == "c":
                q = [outer(a0), outer(a1), inner(a1), inner(a0)]
                G.block(fb, [(pp.x, pp.y) for pp in q], zt - 0.05, zt + 0.66, M["rubble"],
                        top_z=[zt + 0.6, zt + 0.6, zt + 0.66, zt + 0.66])
                ucx.append([V(pp.x, pp.y, z) for pp in q for z in (zt, zt + 0.66)])
            prev = a1
        fb.set_base(n_mark, lambda f, n=n: -zt if f.normal.dot(n) > 0.3 else zt)
    return ucx


# ----------------------------------------------------------------------------
# Pavillon et lanterne

def build_flag(coll, p=None):
    """Pavillon (pivot = haut du guindant, s'etend vers +X, pend vers -Z) avec ondulation de vent figee."""
    p = {**FK.mod("flag"), **(p or {})}
    M = FM.mats()
    W, H = p["size_m"]
    nu, nv = 24, 16
    bm = bmesh.new()
    uv = bm.loops.layers.uv.verify()
    rows = []
    for j in range(nv + 1):
        row = []
        for i in range(nu + 1):
            u, v = i / nu, j / nv
            x = u * W
            z = -v * H
            y = 0.16 * u * math.sin(u * 7.0 + v * 1.5) + 0.04 * u * math.sin(v * 9.0 + u * 3)
            z -= 0.06 * u * u
            row.append(bm.verts.new((x, y, z)))
        rows.append(row)
    for j in range(nv):
        for i in range(nu):
            f = bm.faces.new((rows[j][i], rows[j + 1][i], rows[j + 1][i + 1], rows[j][i + 1]))
            f.smooth = True
            for lp in f.loops:
                co = lp.vert.co
                lp[uv].uv = (co.x / W, 1.0 + co.z / H)
    me = bmesh.ops.solidify(bm, geom=list(bm.faces), thickness=0.004)
    del me
    mesh = bpy_mesh("SM_Fort_Flag_tmp", bm)
    mesh.materials.append(FM.flag_mat(p.get("variant", "fleurdelise")))
    import bpy
    ob = bpy.data.objects.new("SM_Fort_Flag_tmp", mesh)
    coll.objects.link(ob)
    # UV1 = poids de vent (0 au guindant -> 1 au battant), pour un WPO Unreal
    uv1 = mesh.uv_layers.new(name="WindUV")
    for lp in mesh.loops:
        x = mesh.vertices[lp.vertex_index].co.x
        uv1.data[lp.index].uv = (x / W, 0.0)
    mesh.uv_layers[0].name = "UVMap"
    ucx = []
    return FK.res([ob], {"Hoist_Top": ("root", V(0, 0, 0)), "Hoist_Bottom": ("root", V(0, 0, -H))}, ucx, p,
                  info={"variant": p.get("variant"), "wind_uv": "UV1.x"}, no_sharp=True)


def bpy_mesh(name, bm):
    import bpy
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    return me


def build_lantern(coll, p=None):
    p = {**FK.mod("lantern"), **(p or {})}
    M = FM.mats()
    rng = random.Random(111)
    fb = G.FB("Lantern")
    w, d, h = p["size_m"]
    br = p["bracket_m"]
    c = V(0, br, -0.18)
    BL.lantern(fb, M, c, rng, h=h, bracket=(V(0, 0.02, 0.0), V(0, 1, 0)))
    ob = fb.finish(coll)
    return FK.res([ob], FK.sockets_of(p["sockets"]), [], p, info={"light": p["light"]})


def build_lantern_hanging(coll, p=None):
    p = {**FK.mod("lantern_hanging"), **(p or {})}
    M = FM.mats()
    rng = random.Random(112)
    fb = G.FB("LanternHanging")
    w, d, h = p["size_m"]
    ch = p["chain_m"]
    BL.lantern(fb, M, V(0, 0, -ch - h * 0.47), rng, h=h)
    n = max(2, int(ch / 0.07))
    for k in range(n):
        c = V(0, 0, -ch * (k + 0.5) / n)
        rot = (k % 2) * math.pi / 2
        ring = [c + Matrix.Rotation(rot, 3, "Z") @ V(0.014 * math.cos(TAU * i / 6), 0, 0.03 * math.sin(TAU * i / 6))
                for i in range(6)]
        fb.tube(ring, 0.005, M["iron"], segs=3, closed=True)
    fb.box((0, 0, 0.0), (0.08, 0.08, 0.015), M["iron"], bevel=0.003)
    ob = fb.finish(coll)
    return FK.res([ob], {"Light": ("root", V(0, 0, -ch - h * 0.5)), "Hang": ("root", V(0, 0, 0))}, [], p,
                  info={"light": p["light"]})


MODULES = {
    "lantern_hanging": ("SM_Fort_Lantern_Hanging", build_lantern_hanging),
    "signal_tower": ("SM_Fort_SignalTower", build_signal_tower),
    "flag": ("SM_Fort_Flag", build_flag),
    "lantern": ("SM_Fort_Lantern", build_lantern),
}
