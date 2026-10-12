"""
Fort francais : habillage (lot 'habillage').

SM_Fort_Cannon (demi-couleuvrine sur affut marin : galleon/cannons.cannon, materiaux remappes sur MAT-Fort_Iron /
IronRust / WoodOak / Rope), SM_Fort_ShotPile (pyramide de 30 boulets sur cadre), SM_Fort_Crate_A / _B (caisses de
planches cerclees de corde), SM_Fort_Barrel (tonneau de douves bombees, cercles de fer), SM_Fort_PowderBarrel
(baril de poudre cercle de cuivre et de bois : pas de fer).
"""
import math
import random

import bmesh
from mathutils import Matrix, Vector

import fort_geo as G
import fort_kit as FK
import fort_mat as FM

V = G.V
TAU = 2 * math.pi
RUB = (0.0, 0.0, 0.0)


def _remap_to_fort(ob, M):
    """Materiaux du galion -> MAT-Fort_* (4 materiaux max)."""
    me = ob.data
    table = {"iron_rust": M["iron"], "iron": M["cast"], "rope": M["rope"], "pitch": M["cast"],
             "dark_recess": M["cast"]}
    new, idx = [], []
    for m in me.materials:
        n = m.name.lower() if m else ""
        if "rust" in n:
            t = table["iron_rust"]
        elif "rope" in n:
            t = table["rope"]
        elif "iron" in n or "pitch" in n or "recess" in n or "gold" in n:
            t = table["iron"]
        else:
            t = M["oak"]
        if t not in new:
            new.append(t)
        idx.append(new.index(t))
    mi = [0] * len(me.polygons)
    me.polygons.foreach_get("material_index", mi)
    me.materials.clear()
    for m in new:
        me.materials.append(m)
    me.polygons.foreach_set("material_index", [idx[i] for i in mi])
    me.update()


def build_cannon(coll, p=None):
    p = {**FK.mod("cannon"), **(p or {})}
    M = FM.mats()
    from galleon import cannons as GC
    from galleon import materials as GM
    gm = {"iron": GM.mats()["iron"] if False else M["cast"], "iron_rust": M["iron"], "pitch": M["cast"],
          "rope": M["rope"], "dark_recess": M["cast"]}
    bm, mats, info = GC.cannon("demi_culverin", M=gm, rng=random.Random(7), detail="high")
    fb = G.FB("Cannon", world_uv=False)
    fb.add_mesh_multi(bm, mats)
    ob = fb.finish(coll)
    _remap_to_fort(ob, M)
    za = info.get("za", p["trunnion_z"])
    L = GC.KINDS["demi_culverin"]["L"]
    ucx = [("affut", None, G.rbox_pts((0, -0.28, 0.2), (0.5, 1.2, 0.4))),
           ("tube", None, [Vector((0.1 * math.cos(TAU * k / 8), y, za + 0.1 * math.sin(TAU * k / 8)))
                           for k in range(8) for y in (-0.4, L * 0.58)])]
    socks = {"Muzzle": ("root", Vector(info["muzzle"])), "Breech": ("root", Vector(info["button"])),
             "Trunnion": ("root", V(0, 0, za))}
    return FK.res([ob], socks, ucx, p, info={"trunnion_z": round(za, 3), "muzzle": [round(v, 3) for v in info["muzzle"]]},
                  sharp=35)


def build_shot_pile(coll, p=None):
    p = {**FK.mod("shot_pile"), **(p or {})}
    M = FM.mats()
    fb = G.FB("ShotPile")
    d = p["ball_d_m"]
    r = d / 2
    n0 = 4
    fb.blend = RUB
    w = n0 * d + 0.08
    for sy in (-1, 1):
        fb.wood((0, sy * (w / 2 - 0.02), 0.025), (w, 0.04, 0.05), M["oak"], bevel=0.004)
        fb.wood((sy * (w / 2 - 0.02), 0, 0.025), (0.04, w, 0.05), M["oak"], bevel=0.004, long_u=False)
    fb.wood((0, 0, 0.01), (w - 0.06, w - 0.06, 0.02), M["oak"], bevel=0.002)
    k = 0
    for layer in range(n0):
        n = n0 - layer
        z = 0.02 + r + layer * d * 0.8165
        off = -(n - 1) * d / 2
        for i in range(n):
            for j in range(n):
                fb.sphere((off + i * d, off + j * d, z), r * 0.98, M["cast"], u=10, v=6)
                k += 1
    ob = fb.finish(coll)
    return FK.res([ob], {"Top": ("root", V(0, 0, 0.02 + r + 3 * d * 0.8165 + r))},
                  [("pile", None, G.rbox_pts((0, 0, 0.12), (w, w, 0.24)))], p, info={"balls": k})


def _crate(fb, M, size, rng, rope=True, handles=False):
    sx, sy, sz = size
    fb.blend = RUB
    n = max(2, round(sz / 0.13))
    h = sz / n
    for k in range(n):
        z = h * (k + 0.5)
        for s in (-1, 1):
            fb.wood((0, s * (sy / 2 - 0.0125), z), (sx, 0.025, h - 0.006), M["oak_light"], bevel=0.003)
            fb.wood((s * (sx / 2 - 0.0125), 0, z), (0.025, sy - 0.05, h - 0.006), M["oak_light"], bevel=0.003,
                    long_u=False)
    nt = max(2, round(sx / 0.15))
    for k in range(nt):
        x = -sx / 2 + sx * (k + 0.5) / nt
        fb.wood((x, 0, sz - 0.0125), (sx / nt - 0.006, sy, 0.025), M["oak_light"], bevel=0.003, long_u=False)
    for s in (-1, 1):
        for t in (-1, 1):
            fb.wood((s * (sx / 2 - 0.03), t * (sy / 2 + 0.008), sz / 2), (0.05, 0.02, sz), M["oak_light"], bevel=0.003,
                    long_u=False)
    if rope:
        for x in (-sx * 0.3, sx * 0.3):
            ring = [V(x, (sy / 2 + 0.012) * math.cos(TAU * i / 16), sz / 2 + (sz / 2 + 0.012) * math.sin(TAU * i / 16))
                    for i in range(16)]
            fb.tube(ring, 0.009, M["rope"], segs=5, closed=True)
    if handles:
        for s in (-1, 1):
            arc = [V(s * (sx / 2 + 0.02 + 0.03 * math.sin(math.pi * k / 6)), -0.06 + 0.12 * k / 6, sz * 0.7)
                   for k in range(7)]
            fb.tube(arc, 0.01, M["rope"], segs=5)


def build_crate_a(coll, p=None):
    p = {**FK.mod("crate_a"), **(p or {})}
    M = FM.mats()
    fb = G.FB("CrateA", world_uv=False)
    _crate(fb, M, p["size_m"], random.Random(211))
    ob = fb.finish(coll)
    sx, sy, sz = p["size_m"]
    return FK.res([ob], {"Top": ("root", V(0, 0, sz))}, [("caisse", None, G.rbox_pts((0, 0, sz / 2), p["size_m"]))], p)


def build_crate_b(coll, p=None):
    p = {**FK.mod("crate_b"), **(p or {})}
    M = FM.mats()
    fb = G.FB("CrateB", world_uv=False)
    _crate(fb, M, p["size_m"], random.Random(212), rope=False, handles=True)
    ob = fb.finish(coll)
    sx, sy, sz = p["size_m"]
    return FK.res([ob], {"Top": ("root", V(0, 0, sz))}, [("caisse", None, G.rbox_pts((0, 0, sz / 2), p["size_m"]))], p)


def _barrel(fb, M, d, h, hoop_mat, staves_mat, n=24, bulge=0.12, hoops=(0.08, 0.25, 0.75, 0.92), rng=None):
    """Tonneau : douves bombees (UV u = tour, v = hauteur), rainures entre douves, fonds, cercles."""
    rng = rng or random.Random(1)
    R = d / 2
    rows = 9
    tmp = bmesh.new()
    uv = tmp.loops.layers.uv.verify()
    grid = []
    for j in range(rows + 1):
        z = h * j / rows
        k = 1.0 + bulge * math.sin(math.pi * j / rows) - bulge * 0.0
        row = []
        for i in range(n * 2):
            a = TAU * i / (n * 2)
            rr = R * (1 - bulge * 0.45) * k * (0.985 if i % 2 else 1.0)
            row.append(tmp.verts.new((rr * math.cos(a), rr * math.sin(a), z)))
        grid.append(row)
    m = n * 2
    for j in range(rows):
        for i in range(m):
            i2 = (i + 1) % m
            f = tmp.faces.new((grid[j][i], grid[j][i2], grid[j + 1][i2], grid[j + 1][i]))
            f.smooth = True
            for lp, (uu, vv) in zip(f.loops, ((i, j), (i + 1, j), (i + 1, j + 1), (i, j + 1))):
                lp[uv].uv = (uu / m, vv / rows)
    for j, z in ((0, 0.03), (rows, h - 0.03)):
        ring = grid[j]
        c = tmp.verts.new((0, 0, z))
        for i in range(m):
            i2 = (i + 1) % m
            f = tmp.faces.new((ring[i], ring[i2], c) if j == 0 else (ring[i2], ring[i], c))
            for lp in f.loops:
                lp[uv].uv = (lp.vert.co.x / d + 0.5, lp.vert.co.y / d + 0.5)
    bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
    fb.local(fb._merge, tmp, staves_mat, True)
    for t in hoops:
        z = h * t
        k = 1.0 + bulge * math.sin(math.pi * t)
        rr = R * (1 - bulge * 0.45) * k + 0.004
        fb.tube([V(0, 0, z - 0.012), V(0, 0, z + 0.012)], [rr, rr], hoop_mat, segs=n, caps=False)


def build_barrel(coll, p=None):
    p = {**FK.mod("barrel"), **(p or {})}
    M = FM.mats()
    fb = G.FB("Barrel", world_uv=False)
    d, _d2, h = p["size_m"]
    _barrel(fb, M, d, h, M["iron"], M["barrel"])
    ob = fb.finish(coll)
    return FK.res([ob], {"Top": ("root", V(0, 0, h))},
                  [("tonneau", None, [V(d / 2 * math.cos(TAU * k / 8), d / 2 * math.sin(TAU * k / 8), z)
                                      for k in range(8) for z in (0, h)])], p)


def build_powder_barrel(coll, p=None):
    p = {**FK.mod("powder_barrel"), **(p or {})}
    M = FM.mats()
    fb = G.FB("PowderBarrel", world_uv=False)
    d, _d2, h = p["size_m"]
    _barrel(fb, M, d, h, M["copper"], M["barrel"], n=20, hoops=(0.1, 0.9))
    for t in (0.22, 0.32, 0.68, 0.78):
        z = h * t
        rr = d / 2 * (1 + 0.12 * math.sin(math.pi * t)) * (1 - 0.054) + 0.006
        fb.tube([V(0, 0, z - 0.018), V(0, 0, z + 0.018)], [rr, rr], M["oak"], segs=20, caps=False)
    ob = fb.finish(coll)
    return FK.res([ob], {"Top": ("root", V(0, 0, h))},
                  [("baril", None, [V(d / 2 * math.cos(TAU * k / 8), d / 2 * math.sin(TAU * k / 8), z)
                                    for k in range(8) for z in (0, h)])], p)


MODULES = {
    "cannon": ("SM_Fort_Cannon", build_cannon),
    "shot_pile": ("SM_Fort_ShotPile", build_shot_pile),
    "crate_a": ("SM_Fort_Crate_A", build_crate_a),
    "crate_b": ("SM_Fort_Crate_B", build_crate_b),
    "barrel": ("SM_Fort_Barrel", build_barrel),
    "powder_barrel": ("SM_Fort_PowderBarrel", build_powder_barrel),
}
