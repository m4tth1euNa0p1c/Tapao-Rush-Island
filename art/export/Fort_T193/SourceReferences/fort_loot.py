"""
Fort francais : salle des coffres et butin (lot 'tresor').

SM_Fort_TreasureVault : casemate voutee sous le bastion SO (plancher a -0,99 monde), couloir coude depuis la cour
(porte cloutee, sas a grille, 6 marches de 16,5, porte blindee), berceau surbaisse en pierre de taille a doubleaux de
briques, niche aux perles, etagere, table de compte, caniveau et puisard ; SM_Fort_VaultDoor, SM_Fort_VaultGrille,
SM_Fort_Chest_Closed / _Open, SM_Fort_Pallet, SM_Loot_GoldBar, SM_Loot_GoldStack_S/M/L (croisillons), SM_Loot_Gem_*,
SM_Loot_GemBowl, SM_Loot_PearlSack_Open / _Closed, SM_Loot_Coin, SM_Loot_CoinPile.

Or, gemmes, perles : materiaux partages de factory/kits/artifacts/treasure.py (MAT_Gold_Aged, MAT_Gem_*, MAT_Pearl_*),
avec l'attribut OrnMask (cavites, usure, occlusion courte) calcule par treasure.to_object ; textures du butin
reduites a 1K (maquettes/fort-francais/textures/loot_1k).
"""
import math
import os
import random

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

import fort_bldg as BL
import fort_geo as G
import fort_kit as FK
import fort_mat as FM
import fort_rampart as RP

V = G.V
TAU = 2 * math.pi
REND, RUB, ASH, BR = RP.REND, RP.RUB, RP.ASH, RP.BR
MDIR = os.path.dirname(FK.SPEC)
_TR = {}


def TR():
    """Module artifacts.treasure, textures partagees dans la maquette, reduites a 1K."""
    if "m" not in _TR:
        from artifacts import treasure
        treasure.TEX_DIR = os.path.join(MDIR, "textures", "loot_shared")
        _TR["m"] = treasure
    return _TR["m"]


def shrink_loot_images(max_px=1024):
    out = os.path.join(MDIR, "textures", "loot_1k")
    os.makedirs(out, exist_ok=True)
    for img in bpy.data.images:
        fp = bpy.path.abspath(img.filepath) if img.filepath else ""
        if "loot_shared" not in fp.replace("\\", "/") or img.size[0] <= max_px:
            continue
        w, h = img.size
        f = max_px / max(w, h)
        img.scale(max(1, int(w * f)), max(1, int(h * f)))
        dst = os.path.join(out, os.path.splitext(os.path.basename(fp))[0] + ".jpg")
        img.filepath_raw = dst
        img.file_format = "JPEG"
        img.save(filepath=dst, quality=90)
        img.filepath = dst


def bm_to_omesh(bm, mat, seed=0, kind="gold", smooth=True, tile=None):
    """bmesh -> treasure.OMesh (UV cubiques en metres / TILE)."""
    T = TR()
    tile = tile or T.TILE
    big = [f for f in bm.faces if len(f.verts) > 4]
    if big:
        bmesh.ops.triangulate(bm, faces=big)
    bm.verts.index_update()
    Vv = np.array([v.co[:] for v in bm.verts])
    Q, Tt, QUV, TUV = [], [], [], []
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda k: abs(n[k]))
        o = [k for k in range(3) if k != ax]
        idx = [v.index for v in f.verts]
        uv = [(v.co[o[0]] / tile, v.co[o[1]] / tile) for v in f.verts]
        if len(idx) == 4:
            Q.append(idx)
            QUV.append(uv)
        else:
            Tt.append(idx)
            TUV.append(uv)
    bm.free()
    return T.OMesh(Vv, np.array(Q, dtype=np.int64).reshape(-1, 4) if Q else None,
                   np.array(Tt, dtype=np.int64).reshape(-1, 3) if Tt else None,
                   np.array(QUV).reshape(-1, 4, 2) if QUV else None, np.array(TUV).reshape(-1, 3, 2) if TUV else None,
                   mat, smooth, None, seed, kind)


def _om_copy(om, M4):
    T = TR()
    c = T.OMesh(om.V.copy(), om.Q.copy(), om.T.copy(), om.QUV.copy(), om.TUV.copy(), om.mat, om.smooth, None, om.seed,
                om.kind)
    return c.transform(np.array(M4))


def _seeded(om, seed):
    om.seed = seed
    return om


# ----------------------------------------------------------------------------
# Lingots, piles

GOLD = "MAT_Gold_Aged"


def gold_bar_om(p=None, seed=0):
    p = p or FK.mod("gold_bar")
    (bx, by), (tx, ty), h = p["size_m"]["base"], p["size_m"]["top"], p["size_m"]["height"]
    bm = bmesh.new()
    bot = [bm.verts.new((sx * bx / 2, sy * by / 2, 0.0)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    top = [bm.verts.new((sx * tx / 2, sy * ty / 2, h)) for sx, sy in ((-1, -1), (1, -1), (1, 1), (-1, 1))]
    bm.faces.new(bot[::-1])
    ft = bm.faces.new(top)
    for i in range(4):
        j = (i + 1) % 4
        bm.faces.new((bot[i], bot[j], top[j], top[i]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    bmesh.ops.bevel(bm, geom=list(bm.edges), offset=0.0035, segments=2, affect="EDGES", profile=0.6)
    # poincon en creux (cartouche generique, sans armoiries) sur le dessus
    topf = max(bm.faces, key=lambda f: f.calc_center_median().z * (1 if f.normal.z > 0.9 else -1))
    bmesh.ops.inset_region(bm, faces=[topf], thickness=0.011, depth=0.0)
    bmesh.ops.inset_region(bm, faces=[topf], thickness=0.0015, depth=-0.0012)
    # retassure : affaissement leger du centre du dessus
    rng = random.Random(seed)
    for v in bm.verts:
        if v.co.z > h - 0.002:
            v.co.z -= 0.0008 * rng.random()
    return bm_to_omesh(bm, GOLD, seed=seed)


def stack_layout(kind):
    """Croisillons : (nombre de rangs par lit, nombre de lingots par rang) ; lits alternes X / Y."""
    return {"S": [(3, 2), (2, 2), (1, 2)], "M": [(5, 2), (4, 2), (3, 2), (2, 2), (1, 2)],
            "L": [(5, 2)] * 6}[kind]


def gold_stack_oms(kind, seed=1):
    p = FK.mod("gold_bar")
    h = p["size_m"]["height"]
    bl = p["size_m"]["base"][0]
    pitch_r = p["size_m"]["base"][1] + 0.006
    rng = random.Random(seed)
    base = gold_bar_om(p, seed)
    oms = []
    z = 0.0
    lay = stack_layout(kind)
    for k, (rows, per) in enumerate(lay):
        along_x = k % 2 == 0
        for r in range(rows):
            for c in range(per):
                if k == len(lay) - 1 and kind != "S" and rng.random() < 0.25:
                    continue
                u = (c - (per - 1) / 2) * (bl + 0.004)
                v = (r - (rows - 1) / 2) * pitch_r
                x, y = (u, v) if along_x else (v, u)
                ang = (0.0 if along_x else math.pi / 2) + rng.uniform(-0.03, 0.03)
                M4 = Matrix.Translation((x + rng.uniform(-0.003, 0.003), y + rng.uniform(-0.003, 0.003), z)) @ \
                    Matrix.Rotation(ang, 4, "Z")
                oms.append(_seeded(_om_copy(base, M4), seed * 100 + len(oms)))
        z += h
    # lingots de travers sur le dessus
    for i in range(2 if kind != "S" else 1):
        a = rng.uniform(0.3, 1.2)
        tilt = Matrix.Rotation(rng.uniform(-0.08, 0.08), 4, "X")
        M4 = Matrix.Translation((rng.uniform(-0.05, 0.05), rng.uniform(-0.05, 0.05), z + i * h)) @ \
            Matrix.Rotation(a, 4, "Z") @ tilt
        oms.append(_seeded(_om_copy(base, M4), seed * 100 + 90 + i))
    return oms, z + h


def build_gold_bar(coll, p=None):
    p = {**FK.mod("gold_bar"), **(p or {})}
    T = TR()
    ob = T.to_object([gold_bar_om(p, 3)], "GoldBar", coll)
    shrink_loot_images()
    h = p["size_m"]["height"]
    bx, by = p["size_m"]["base"]
    return FK.res([ob], FK.sockets_of(p["sockets"]), [("lingot", None, G.rbox_pts((0, 0, h / 2), (bx, by, h)))], p,
                  info={"mass_kg_game": p["mass_kg_game"]}, sharp=50)


def _stack_builder(kind):
    key = {"S": "gold_stack_s", "M": "gold_stack_m", "L": "gold_stack_l"}[kind]

    def build(coll, p=None):
        p = {**FK.mod(key), **(p or {})}
        T = TR()
        oms, top = gold_stack_oms(kind, seed={"S": 11, "M": 12, "L": 13}[kind])
        ob = T.to_object(oms, "GoldStack" + kind, coll)
        shrink_loot_images()
        ob.data.update()
        xs = [v.co.x for v in ob.data.vertices]
        ys = [v.co.y for v in ob.data.vertices]
        ucx = [("pile", None, G.rbox_pts(((max(xs) + min(xs)) / 2, (max(ys) + min(ys)) / 2, top / 2),
                                         (max(xs) - min(xs), max(ys) - min(ys), top)))]
        socks = FK.sockets_of(p["sockets"])
        socks["Grab"] = ("root", V(0, 0, top))
        return FK.res([ob], socks, ucx, p, info={"bars": len(oms), "height_m": round(top, 3)}, sharp=50)
    return build


# ----------------------------------------------------------------------------
# Gemmes, coupelles

GEM_KINDS = {
    "Ruby": ("MAT_Gem_Ruby", "cushion", (0.016, 0.014, 0.008)),
    "Sapphire": ("MAT_Gem_Sapphire", "table", (0.018, 0.014, 0.007)),
    "Emerald": ("MAT_Gem_Emerald", "emerald", (0.02, 0.014, 0.007)),
    "Amethyst": ("MAT_Gem_Amethyst", "rose", (0.017, 0.017, 0.008)),
    "Topaz": ("MAT_Gem_Topaz", "cabochon", (0.015, 0.012, 0.007)),
}


def gem_om(cut, size, mat, seed, q=1.0):
    T = TR()
    w, l, h = size
    if cut == "cushion":
        g = T.gem_cushion(w, l, h, q=q, mat=mat, seed=seed)
    elif cut in ("table", "emerald"):
        g = T.gem_table(w, l, h, q=q, mat=mat, seed=seed, pav_ratio=1.0 if cut == "table" else 0.8)
    elif cut == "rose":
        g = T.gem_rose(w, h, q=q, mat=mat, seed=seed)
    else:
        g = T.gem_cabochon(w, l, h, q=q, mat=mat, seed=seed)
    return g.mesh, g


def build_gems(coll, p=None):
    p = {**FK.mod("gems"), **(p or {})}
    T = TR()
    out = []
    for i, (name, (mat, cut, size)) in enumerate(GEM_KINDS.items()):
        om, g = gem_om(cut, size, mat, 300 + i)
        om.transform(np.array(Matrix.Translation((0, 0, g.pav))))
        ob = T.to_object([om], "Gem_" + name, coll)
        h = g.pav + size[2]
        out.append(FK.res([ob], {"Grab": ("root", V(0, 0, h / 2))},
                          [("gemme", None, G.rbox_pts((0, 0, h / 2), (size[0], size[1], h)))], p,
                          export_name="SM_Loot_Gem_" + name, info={"cut": cut, "size_m": list(size)}, no_sharp=True))
    shrink_loot_images()
    return out


def _bowl_fb(fb, M, d, h, rng, mat="copper"):
    from fort_rampart import _revolve
    r = d / 2
    prof = [(0.0, 0.004), (r * 0.45, 0.004), (r * 0.75, h * 0.35), (r * 0.95, h * 0.85), (r, h), (r * 0.97, h + 0.003),
            (r * 0.9, h * 0.85), (r * 0.7, h * 0.4), (r * 0.4, 0.008), (0.0, 0.008)]
    tmp = _revolve(prof, 28)
    for v in tmp.verts:                                    # martele : petites facettes
        v.co += v.co.normalized() * rng.uniform(-0.0008, 0.0008) if v.co.length > 1e-4 else Vector()
    fb.blend = RUB
    fb.add_bm(tmp, M[mat], smooth=True, uv_size=0.2)
    return h


def heap_points(n, r, h0, h1, rng, min_d):
    """Points sur une calotte (rayon r, base h0, sommet h1) espaces de min_d."""
    pts = []
    tries = 0
    while len(pts) < n and tries < n * 400:
        tries += 1
        a = rng.uniform(0, TAU)
        rr = r * math.sqrt(rng.random())
        z = h0 + (h1 - h0) * max(0.0, 1 - (rr / r) ** 2) ** 0.8
        p = Vector((rr * math.cos(a), rr * math.sin(a), z))
        if all((p - q).length > min_d for q in pts):
            pts.append(p)
    return pts


def build_gem_bowl(coll, p=None):
    p = {**FK.mod("gem_bowl"), **(p or {})}
    M = FM.mats()
    T = TR()
    rng = random.Random(311)
    fb = G.FB("GemBowl", world_uv=False)
    d, h = p["bowl_d_m"], p["bowl_h_m"]
    _bowl_fb(fb, M, d, h, rng)
    bowl = fb.finish(coll)
    mix = p["gems"]["mix"]
    names = list(mix)
    pts = heap_points(p["gems"]["count"], d / 2 * 0.86, h * 0.45, h + 0.022, rng, 0.0135)
    oms = []
    for i, c in enumerate(pts):
        x = rng.random()
        acc = 0.0
        mat = names[-1]
        for nm in names:
            acc += mix[nm]
            if x < acc:
                mat = nm
                break
        cut = {"MAT_Gem_Ruby": "cushion", "MAT_Gem_Sapphire": "table", "MAT_Gem_Emerald": "emerald"}[mat]
        s = rng.uniform(0.010, 0.016)
        om, g = gem_om(cut, (s * rng.uniform(1.0, 1.25), s, s * 0.55), mat, 400 + i, q=0.5)
        R = Matrix.Rotation(rng.uniform(0, TAU), 4, "Z") @ Matrix.Rotation(rng.uniform(-0.9, 0.9), 4, "X") @ \
            Matrix.Rotation(rng.uniform(-0.9, 0.9), 4, "Y")
        om.transform(np.array(Matrix.Translation(c) @ R))
        oms.append(om)
    gems = T.to_object(oms, "GemBowl_Gems", coll)
    shrink_loot_images()
    ucx = [("coupelle", None, [V(d / 2 * math.cos(TAU * k / 10), d / 2 * math.sin(TAU * k / 10), z)
                               for k in range(10) for z in (0, h + 0.02)])]
    return FK.res([bowl, gems], FK.sockets_of(p["sockets"]), ucx, p, info={"gems": len(pts)}, no_sharp=True)


# ----------------------------------------------------------------------------
# Perles, sacs

def _sack_fb(fb, M, rng, d, H, open_=True):
    """Sac de jute : revolution bossuee (plis), col roule (ouvert) ou noue (ferme)."""
    from fort_rampart import _revolve
    r = d / 2
    if open_:
        prof = [(0.0, 0.0), (r * 0.8, 0.0), (r * 1.0, 0.04), (r * 1.08, H * 0.35), (r * 1.0, H * 0.7), (r * 0.86, H * 0.86),
                (r * 0.92, H * 0.93), (r * 1.0, H * 0.96), (r * 0.95, H), (r * 0.82, H * 0.95)]
    else:
        prof = [(0.0, 0.0), (r * 0.8, 0.0), (r * 1.0, 0.04), (r * 1.06, H * 0.35), (r * 0.95, H * 0.62),
                (r * 0.45, H * 0.8), (r * 0.18, H * 0.84), (r * 0.2, H * 0.88), (r * 0.35, H * 0.95), (r * 0.25, H),
                (0.0, H * 0.97)]
    tmp = _revolve(prof, 36)
    off = Vector((rng.uniform(0, 50), rng.uniform(0, 50), rng.uniform(0, 50)))
    from mathutils import noise as mnoise
    ph = rng.uniform(0, TAU)
    for v in tmp.verts:
        co = v.co.copy()
        if co.length < 1e-5:
            continue
        rad = Vector((co.x, co.y, 0))
        t = max(0.0, min(1.0, co.z / H))
        a = math.atan2(co.y, co.x)
        # plis verticaux (plus marques vers le col), affaissement du fond, bosses du contenu
        fold = 0.07 * math.sin(6 * a + ph + 2.5 * mnoise.noise(co * 6 + off)) * max(0.0, t - 0.25) / 0.75
        sag = 0.06 * (1 - t) ** 2
        k = fold + sag + mnoise.noise(co * 14 + off) * 0.05 + mnoise.noise(co * 40 + off) * 0.012
        if rad.length > 1e-4:
            v.co += rad.normalized() * rad.length * k * (1.0 if co.z > 0.02 else 0.4)
        v.co.z += mnoise.noise(co * 20 + off) * 0.01 * t
        v.co.x *= 1.07
        v.co.y *= 0.94
        v.co.x += 0.03 * t * t
    bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
    uv = tmp.loops.layers.uv.verify()
    for f in tmp.faces:
        for lp in f.loops:
            co = lp.vert.co
            lp[uv].uv = (math.atan2(co.y, co.x) * r, co.z)
    fb.blend = RUB
    fb.local(fb._merge, tmp, M["jute"], True)


def _pearl_oms(n, pts_fn, rng, mix_black=0.4, d=(0.008, 0.012), nu=6, nv=4):
    T = TR()
    oms = []
    for i in range(n):
        c, r = pts_fn(i)
        mat = "MAT_Pearl_Black" if rng.random() < mix_black else "MAT_Pearl_White"
        oms.append(T.sphere(tuple(c), r if r else rng.uniform(*d) / 2, nu=nu, nv=nv, mat=mat, seed=500 + i,
                            baroque=0.06))
    return oms


def build_pearl_sack(coll, p=None, open_=True):
    key = "pearl_sack_open" if open_ else "pearl_sack_closed"
    p = {**FK.mod(key), **(p or {})}
    M = FM.mats()
    T = TR()
    rng = random.Random(321 if open_ else 322)
    fb = G.FB("PearlSack", world_uv=False)
    d, _d2, H = p["size_m"]
    _sack_fb(fb, M, rng, d, H, open_)
    parts = []
    if open_:
        # cordelette denouee
        pts = [V(d / 2 * 0.95 * math.cos(a), d / 2 * 0.95 * math.sin(a), H * 0.88) for a in
               [TAU * k / 14 for k in range(11)]]
        pts += [V(d * 0.62, -d * 0.08, H * 0.82), V(d * 0.66, -d * 0.18, H * 0.5), V(d * 0.64, -d * 0.26, 0.12),
                V(d * 0.74, -d * 0.4, 0.006), V(d * 0.95, -d * 0.62, 0.005), V(d * 1.15, -d * 0.55, 0.005)]
        fb.tube(pts, 0.004, M["rope"], segs=5)
        sack = fb.finish(coll)
        parts.append(sack)
        pr = p["pearls"]
        top = H * 0.93
        n_spill = int(str(pr.get("spill", 20)).split()[0])
        mound = heap_points(pr["visible"] - n_spill, d / 2 * 0.8, top - 0.01, top + 0.035, rng, 0.0095)
        spill = []
        for i in range(n_spill):
            a = rng.uniform(-1.4, 0.4)
            rr = rng.uniform(d * 0.55, d * 1.4)
            spill.append(Vector((rr * math.cos(a), rr * math.sin(a), 0.005)))
        allp = mound + spill
        oms = _pearl_oms(len(allp), lambda i: (allp[i], None), rng, mix_black=pr["mix"]["MAT_Pearl_Black"])
        parts.append(T.to_object(oms, "PearlSack_Pearls", coll))
        info = {"pearls": len(allp)}
    else:
        # cordon noue + plomb (sceau generique)
        ring = [V(d * 0.1 * math.cos(TAU * k / 12), d * 0.1 * math.sin(TAU * k / 12), H * 0.85) for k in range(12)]
        fb.tube(ring, 0.006, M["rope"], segs=5, closed=True)
        fb.tube([V(d * 0.1, 0, H * 0.85), V(d * 0.2, -0.02, H * 0.7), V(d * 0.25, -0.03, H * 0.55)], 0.004, M["rope"],
                segs=5)
        fb.sphere((d * 0.25, -0.03, H * 0.53), 0.012, M["iron"], u=8, v=5, scale=(1, 0.4, 1))
        sack = fb.finish(coll)
        parts.append(sack)
        info = {}
    shrink_loot_images()
    ucx = [("sac", None, [V(d / 2 * math.cos(TAU * k / 8), d / 2 * math.sin(TAU * k / 8), z) for k in range(8)
                          for z in (0, H)])]
    return FK.res(parts, FK.sockets_of(p["sockets"]), ucx, p, info=info, no_sharp=True)


def build_pearl_sack_open(coll, p=None):
    return build_pearl_sack(coll, p, True)


def build_pearl_sack_closed(coll, p=None):
    return build_pearl_sack(coll, p, False)


# ----------------------------------------------------------------------------
# Pieces

def coin_om(seed=0, segs=12, d=0.02, t=0.002):
    """Louis d'or generique : flan a listel, champ en creux, effigie fictive (bosse) ; sans legende."""
    r = d / 2
    from fort_rampart import _revolve
    prof = [(0.0, t * 0.62), (r * 0.25, t * 0.68), (r * 0.5, t * 0.55), (r * 0.82, t * 0.5), (r * 0.9, t),
            (r, t * 0.9), (r, t * 0.1), (r * 0.9, 0.0), (r * 0.82, t * 0.5 - t * 0.5), (0.0, t * 0.0)]
    prof = [(0.0, t), (r * 0.3, t * 0.95), (r * 0.82, t * 0.82), (r * 0.9, t), (r, t * 0.92), (r, t * 0.08),
            (r * 0.9, 0.0), (r * 0.82, t * 0.18), (r * 0.3, t * 0.05), (0.0, 0.0)]
    tmp = _revolve(prof, segs)
    return bm_to_omesh(tmp, GOLD, seed=seed, tile=0.03)


def build_coin(coll, p=None):
    p = {**FK.mod("coin"), **(p or {})}
    T = TR()
    ob = T.to_object([coin_om(7, 12)], "Coin", coll)
    shrink_loot_images()
    d, _, t = p["size_m"]
    return FK.res([ob], {"Grab": ("root", V(0, 0, t / 2))}, [], p, no_sharp=True)


def coin_heap_material():
    """Or des tas de pieces : MAT_Gold_Aged + relief de disques (Voronoi en coordonnees objet) et joints sombres."""
    name = "MAT_Gold_CoinHeap"
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    m = TR().material(GOLD).copy()
    m.name = name
    nt = m.node_tree
    b = next(n for n in nt.nodes if n.type == "BSDF_PRINCIPLED")
    tc = nt.nodes.new("ShaderNodeTexCoord")
    vor = nt.nodes.new("ShaderNodeTexVoronoi")
    vor.inputs["Scale"].default_value = 62.0
    vor.inputs["Randomness"].default_value = 0.85
    nt.links.new(tc.outputs["Object"], vor.inputs["Vector"])
    disc = nt.nodes.new("ShaderNodeMapRange")
    disc.inputs["From Min"].default_value = 0.42
    disc.inputs["From Max"].default_value = 0.30
    disc.interpolation_type = "SMOOTHSTEP"
    nt.links.new(vor.outputs["Distance"], disc.inputs["Value"])
    rim = nt.nodes.new("ShaderNodeMapRange")
    rim.inputs["From Min"].default_value = 0.36
    rim.inputs["From Max"].default_value = 0.32
    rim.inputs["To Min"].default_value = 0.0
    rim.inputs["To Max"].default_value = 0.35
    nt.links.new(vor.outputs["Distance"], rim.inputs["Value"])
    h = nt.nodes.new("ShaderNodeMath")
    h.operation = "ADD"
    nt.links.new(disc.outputs["Result"], h.inputs[0])
    nt.links.new(rim.outputs["Result"], h.inputs[1])
    bump = nt.nodes.new("ShaderNodeBump")
    bump.inputs["Strength"].default_value = 1.0
    bump.inputs["Distance"].default_value = 0.004
    nt.links.new(h.outputs[0], bump.inputs["Height"])
    old = b.inputs["Normal"].links[0].from_socket if b.inputs["Normal"].links else None
    if old is not None:
        nt.links.new(old, bump.inputs["Normal"])
    nt.links.new(bump.outputs["Normal"], b.inputs["Normal"])
    src = b.inputs["Base Color"].links[0].from_socket
    mix = nt.nodes.new("ShaderNodeMix")
    mix.data_type = "RGBA"
    mix.blend_type = "MULTIPLY"
    inv = nt.nodes.new("ShaderNodeMath")
    inv.operation = "SUBTRACT"
    inv.inputs[0].default_value = 1.0
    nt.links.new(disc.outputs["Result"], inv.inputs[1])
    nt.links.new(inv.outputs[0], mix.inputs[0])
    nt.links.new(src, mix.inputs[6])
    mix.inputs[7].default_value = (0.12, 0.08, 0.04, 1.0)
    nt.links.new(mix.outputs[2], b.inputs["Base Color"])
    rsrc = b.inputs["Roughness"].links[0].from_socket if b.inputs["Roughness"].links else None
    if rsrc is not None:
        add = nt.nodes.new("ShaderNodeMath")
        add.operation = "ADD"
        add.use_clamp = True
        nt.links.new(rsrc, add.inputs[0])
        add.inputs[1].default_value = 0.14
        nt.links.new(add.outputs[0], b.inputs["Roughness"])
    m["ue_recipe"] = ("MAT_Gold_Aged + relief de pieces : Voronoi (62/m, coordonnees objet) -> disques (Distance "
                      "0,42..0,30) + listel ; joints assombris #1F140A ; a reproduire par une normal map de pieces "
                      "tuilees dans Unreal.")
    return m


def coin_heap_oms(n, r, h, rng, base_z=0.0, segs=8, seed=0, mound=True, aspect=(1.0, 1.0)):
    """Tas de pieces : calotte d'or bossuee (remplissage) + n pieces posees a plat / inclinees a sa surface."""
    T = TR()
    oms = []
    if mound:
        bm = bmesh.new()
        bmesh.ops.create_uvsphere(bm, u_segments=20, v_segments=10, radius=1.0)
        from mathutils import noise as mnoise
        for v in bm.verts:
            co = v.co.copy()
            z = max(co.z, 0.0)
            v.co = Vector((co.x * r * aspect[0], co.y * r * aspect[1], base_z + z * h * 0.92))
            v.co.z += mnoise.noise(co * 6 + Vector((seed, 0, 0))) * 0.006 * z
        bmesh.ops.delete(bm, geom=[v for v in bm.verts if v.co.z < base_z - 1e-4], context="VERTS")
        coin_heap_material()
        oms.append(bm_to_omesh(bm, "MAT_Gold_CoinHeap", seed=seed))
    base = coin_om(seed, segs)
    pts = heap_points(n, r * 0.95, base_z + 0.004, base_z + h, rng, 0.012)
    for i, c in enumerate(pts):
        c = Vector((c.x * aspect[0], c.y * aspect[1], c.z))
        R = Matrix.Rotation(rng.uniform(0, TAU), 4, "Z") @ Matrix.Rotation(rng.uniform(-0.5, 0.5), 4, "X") @ \
            Matrix.Rotation(rng.uniform(-0.5, 0.5), 4, "Y")
        oms.append(_seeded(_om_copy(base, Matrix.Translation(c) @ R), seed * 1000 + i))
    return oms


def build_coin_pile(coll, p=None):
    p = {**FK.mod("coin_pile"), **(p or {})}
    T = TR()
    rng = random.Random(331)
    sx, sy, sz = p["size_m"]
    oms = coin_heap_oms(78, sx / 2 * 0.8, sz, rng, segs=8, seed=5, aspect=(1.0, sy / sx))
    base = coin_om(9, 8)
    for k, (x, y) in enumerate(((sx * 0.42, sy * 0.25), (-sx * 0.45, -sy * 0.3))):
        for j in range(9 - 3 * k):
            M4 = Matrix.Translation((x + rng.uniform(-0.001, 0.001), y + rng.uniform(-0.001, 0.001), j * 0.0021)) @ \
                Matrix.Rotation(rng.uniform(0, TAU), 4, "Z")
            oms.append(_seeded(_om_copy(base, M4), 900 + 20 * k + j))
    ob = T.to_object(oms, "CoinPile", coll)
    shrink_loot_images()
    ucx = [("tas", None, [V(sx / 2 * math.cos(TAU * k / 8), sy / 2 * math.sin(TAU * k / 8), z) for k in range(8)
                          for z in (0, 0.01)] + [V(0, 0, sz)])]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"coins": len(oms) - 1}, no_sharp=True)


# ----------------------------------------------------------------------------
# Coffres, chantier

def chest_fb(fb, M, size, rng, lid_deg=0.0):
    """Coffre de chene cercle de fer (feuillards rivetes, coins ferres), couvercle bombe, moraillon + cadenas,
    poignees. Retourne (z du bord, z haut du couvercle)."""
    w, d, h = size
    hb = h * 0.62
    fb.blend = RUB
    t = 0.022
    for s in (-1, 1):
        fb.wood((0, s * (d / 2 - t / 2), hb / 2), (w, t, hb), M["oak"], bevel=0.004)
        fb.wood((s * (w / 2 - t / 2), 0, hb / 2), (t, d - 2 * t, hb), M["oak"], bevel=0.004, long_u=False)
    fb.wood((0, 0, 0.015), (w - 2 * t, d - 2 * t, 0.03), M["oak"], bevel=0.003)
    # couvercle bombe (demi-berceau) pivotant a l'arriere
    rl = d / 2
    hl = h - hb
    pivot = V(0, d / 2, hb)
    R = Matrix.Rotation(-math.radians(lid_deg), 3, "X")
    n = 8
    for k in range(n):
        a0 = math.pi * k / n
        a1 = math.pi * (k + 1) / n
        p0 = V(0, -rl * math.cos(a0), hl * math.sin(a0))
        p1 = V(0, -rl * math.cos(a1), hl * math.sin(a1))
        c = (p0 + p1) / 2 + V(0, 0, 0)
        L = (p1 - p0).length
        ang = math.atan2(p1.z - p0.z, p1.y - p0.y)
        cc = pivot + R @ (c - V(0, d / 2, 0))
        fb.wood(cc, (w + 0.01, L + 0.004, 0.02), M["oak"], R=R @ Matrix.Rotation(ang, 3, "X"), bevel=0.003)
    for sx in (-1, 1):
        verts = [pivot + R @ (V(sx * (w / 2 + 0.006), -rl * math.cos(math.pi * k / n), hl * math.sin(math.pi * k / n))
                             - V(0, d / 2, 0)) for k in range(n + 1)]
        fb.poly_bm(verts + [pivot + R @ (V(sx * (w / 2 + 0.006), 0, 0) - V(0, d / 2, 0))],
                   [tuple(range(n + 2))], M["oak"], world=False)
    # feuillards de fer (corps + couvercle), coins, poignees, moraillon
    for x in (-w * 0.32, w * 0.32):
        for s in (-1, 1):
            fb.box((x, s * (d / 2 + 0.003), hb / 2), (0.04, 0.006, hb + 0.004), M["iron"], bevel=0.0)
        fb.box((x, 0, 0.003), (0.04, d + 0.006, 0.006), M["iron"], bevel=0.0)
        pts = [pivot + R @ (V(x, -(rl + 0.004) * math.cos(math.pi * k / n), (hl + 0.004) * math.sin(math.pi * k / n))
                            - V(0, d / 2, 0)) for k in range(n + 1)]
        fb.tube(pts, 0.006, M["iron"], segs=4)
    for sx in (-1, 1):
        for sy in (-1, 1):
            fb.box((sx * (w / 2 - 0.02), sy * (d / 2 - 0.02), hb - 0.03), (0.05, 0.05, 0.06), M["iron"], bevel=0.003)
        arc = [V(sx * (w / 2 + 0.02 + 0.025 * math.sin(math.pi * k / 6)), -0.05 + 0.1 * k / 6, hb * 0.7)
               for k in range(7)]
        fb.tube(arc, 0.007, M["iron"], segs=5)
    lockc = pivot + R @ (V(0, -rl - 0.006, 0.02) - V(0, d / 2, 0))
    fb.box(lockc + V(0, 0, -0.03), (0.05, 0.01, 0.1), M["iron"], bevel=0.003)
    if lid_deg < 5:
        fb.box((0, -d / 2 - 0.02, hb - 0.03), (0.06, 0.025, 0.07), M["iron"], bevel=0.006)
        arc = [V(0.025 * math.cos(math.pi * k / 6), -d / 2 - 0.02, hb + 0.005 + 0.03 * math.sin(math.pi * k / 6))
               for k in range(7)]
        fb.tube(arc, 0.005, M["iron"], segs=4)
    return hb, h


def build_chest(coll, p=None, open_=False):
    key = "chest_open" if open_ else "chest_closed"
    p = {**FK.mod(key), **(p or {})}
    M = FM.mats()
    T = TR()
    rng = random.Random(341 if open_ else 342)
    fb = G.FB("Chest", world_uv=False)
    w, d, h = p["size_m"]
    hb, _ = chest_fb(fb, M, (w, d, h), rng, lid_deg=p.get("lid_angle_deg", 105) if open_ else 0.0)
    parts = [fb.finish(coll)]
    info = {}
    if open_:
        oms = coin_heap_oms(40, (w - 0.06) / 2, 0.07, rng, base_z=hb - 0.04, segs=8, seed=21,
                            aspect=(1.0, (d - 0.06) / (w - 0.06)))
        for om in oms:                     # 5 materiaux : pieces et remplissage partagent l'or des tas de pieces
            om.mat = "MAT_Gold_CoinHeap"
        for i, (mat, cut) in enumerate((("MAT_Gem_Ruby", "cushion"), ("MAT_Gem_Ruby", "cushion"),
                                        ("MAT_Gem_Ruby", "cushion"))):
            om, g = gem_om(cut, (0.018, 0.015, 0.009), mat, 600 + i, q=0.6)
            c = Vector((rng.uniform(-0.2, 0.2), rng.uniform(-0.1, 0.1), hb + 0.03))
            om.transform(np.array(Matrix.Translation(c) @ Matrix.Rotation(rng.uniform(-0.5, 0.5), 4, "X")))
            oms.append(om)
        # collier de perles debordant sur le bord avant
        path = [Vector((-0.1 + 0.2 * t, -d / 2 + 0.06 - 0.03 * math.sin(math.pi * t), hb + 0.05 - 0.02 * t))
                for t in [k / 12 for k in range(13)]]
        path += [Vector((0.1 + 0.01 * k, -d / 2 - 0.005, hb - 0.03 * k)) for k in range(1, 7)]
        pearls = _pearl_oms(len(path), lambda i: (path[i], 0.0055), rng, mix_black=0.0)
        oms += pearls
        parts.append(T.to_object(oms, "Chest_Fill", coll))
        info = {"coins": 40, "gems": 3, "pearls": len(path)}
    shrink_loot_images()
    ucx = [("corps", None, G.rbox_pts((0, 0, hb / 2), (w, d, hb)))]
    if open_:
        ucx.append(("couvercle", None, G.rbox_pts((0, d / 2 + 0.1, hb + 0.2), (w, 0.15, 0.42))))
    else:
        ucx[0] = ("coffre", None, G.rbox_pts((0, 0, h / 2), (w, d, h)))
    return FK.res(parts, FK.sockets_of(p["sockets"]), ucx, p, info=info, sharp=45)


def build_chest_closed(coll, p=None):
    return build_chest(coll, p, False)


def build_chest_open(coll, p=None):
    return build_chest(coll, p, True)


def build_pallet(coll, p=None):
    p = {**FK.mod("pallet"), **(p or {})}
    M = FM.mats()
    rng = random.Random(351)
    fb = G.FB("Pallet", world_uv=False)
    L, W, H = p["size_m"]
    fb.blend = RUB
    for x in (-L / 2 + 0.06, 0.0, L / 2 - 0.06):
        fb.wood((x, 0, 0.04), (0.09, W, 0.08), M["oak"], bevel=0.006, long_u=False)
    for k in range(5):
        y = -W / 2 + W * (k + 0.5) / 5
        fb.wood((rng.uniform(-0.01, 0.01), y, H - 0.02), (L, W / 5 - 0.012, 0.04), M["oak"], bevel=0.005)
    ob = fb.finish(coll)
    return FK.res([ob], {"Top": ("root", V(0, 0, H))}, [("chantier", None, G.rbox_pts((0, 0, H / 2), (L, W, H)))], p)


# ----------------------------------------------------------------------------
# Portes

def build_vault_door(coll, p=None):
    p = {**FK.mod("vault_door"), **(p or {})}
    M = FM.mats()
    fb = G.FB("VaultDoor", world_uv=False)
    w, th, h = p["size_m"]
    fb.blend = RUB
    for k in range(5):
        fb.wood((w * (k + 0.5) / 5, th * 0.25, h / 2), (w / 5 - 0.005, th / 2, h), M["oak"], bevel=0.004, long_u=False)
    # plaques de fer rivetees (face exterieure -Y)
    rows, cols = 4, 2
    for i in range(cols):
        for j in range(rows):
            cx = w * (i + 0.5) / cols
            cz = h * (j + 0.5) / rows
            fb.box((cx, -th * 0.25, cz), (w / cols - 0.012, th / 2, h / rows - 0.012), M["iron"], bevel=0.004)
            pts = [V(cx + sx * (w / cols / 2 - 0.035), -th / 2 - 0.002, cz + sz * (h / rows / 2 - 0.035))
                   for sx in (-1, 0, 1) for sz in (-1, 1)]
            G.studs(fb, pts, 0.011, M["iron"], (0, -1, 0))
    for z in (0.25, 0.9, h - 0.25):
        fb.box((w * 0.45, -th / 2 - 0.008, z), (w * 0.9, 0.012, 0.08), M["iron"], bevel=0.0)
        fb.cyl(V(0, 0, z - 0.08), V(0, 0, z + 0.08), 0.03, M["iron"], segs=8)
    lx, ly, lz = p["sockets"]["Lock"]
    fb.box((lx - 0.06, -th / 2 - 0.03, lz), (0.16, 0.05, 0.3), M["iron"], bevel=0.008)
    fb.box((lx - 0.06, -th / 2 - 0.056, lz + 0.05), (0.02, 0.004, 0.05), M["cast"], bevel=0.0)
    fb.box((w / 2, -th / 2 - 0.02, 1.5), (0.22, 0.03, 0.14), M["iron"], bevel=0.006)
    ring = [V(lx - 0.12, -th / 2 - 0.06 + 0.0, lz - 0.2 + 0.06 * math.sin(TAU * i / 12)) +
            V(0.06 * math.cos(TAU * i / 12), 0, 0) for i in range(12)]
    fb.tube(ring, 0.01, M["iron"], segs=5, closed=True)
    ob = fb.finish(coll)
    return FK.res([ob], FK.sockets_of(p["sockets"]), [("porte", None, G.rbox_pts((w / 2, 0, h / 2), (w, th, h)))], p,
                  info={"anim": p.get("anim")})


def build_vault_grille(coll, p=None):
    p = {**FK.mod("vault_grille"), **(p or {})}
    M = FM.mats()
    fb = G.FB("VaultGrille", world_uv=False)
    W, th, H = p["size_m"]
    dw, dh = p["door_m"]
    fb.blend = RUB
    for x in (-W / 2 + 0.025, W / 2 - 0.025):
        fb.box((x, 0, H / 2), (0.05, 0.05, H), M["iron"], bevel=0.004)
    fb.box((0, 0, H - 0.025), (W, 0.05, 0.05), M["iron"], bevel=0.004)
    fb.box((0, 0, dh + 0.02), (W, 0.04, 0.04), M["iron"], bevel=0.003)
    n = int(W / 0.1)
    for i in range(1, n):
        x = -W / 2 + W * i / n
        fb.cyl(V(x, 0, dh + 0.02), V(x, 0, H - 0.03), 0.015, M["iron"], segs=6)
    # porte grille (1,0 x 1,8) centree
    x0 = -dw / 2
    for z in (0.05, 0.9, dh - 0.03):
        fb.box((0, -0.01, z), (dw, 0.012, 0.05), M["iron"], bevel=0.0)
    for i in range(int(dw / 0.1) + 1):
        x = x0 + dw * i / int(dw / 0.1)
        fb.cyl(V(x, -0.01, 0.02), V(x, -0.01, dh - 0.01), 0.015, M["iron"], segs=6)
    fb.box((dw / 2 + 0.02, -0.05, 1.0), (0.07, 0.03, 0.08), M["iron"], bevel=0.008)
    ob = fb.finish(coll)
    ucx = [("dormant", None, G.rbox_pts((0, 0, (dh + H) / 2), (W, th, H - dh))),
           ("porte", None, G.rbox_pts((0, 0, dh / 2), (W, th, dh)))]
    return FK.res([ob], {"DoorHinge": ("root", V(-dw / 2, 0, 0)), "Lock": ("root", V(dw / 2, -0.05, 1.0))}, ucx, p)


# ----------------------------------------------------------------------------
# Salle des coffres

def build_treasure_vault(coll, p=None):
    p = {**FK.mod("treasure_vault"), **(p or {})}
    M = FM.mats()
    rng = random.Random(361)
    fb = G.FB("TreasureVault")
    ch = p["chamber"]
    iw, idp = ch["inner_m"]
    t = ch["walls_m"]
    vs, vc = ch["springing_z"], ch["crown_z"]
    hx, hy = iw / 2, idp / 2
    cor = p["corridor"]
    cw = cor["width_m"]
    cl = cor["clear_height_m"]
    zc = 0.99                                            # niveau de la cour en repere local (plancher a -0,99)
    L = p["layout_local"]
    d_x = L["court_door_x"]                              # axe du couloir 1 (depuis la cour)
    y_court = L["court_face_y"]
    y2 = L["leg2_y"]                                     # axe du couloir 2 (vers l'ouest)
    x_steps0 = L["steps_x0"]
    x_door = L["vault_door_x"]
    zt = zc + cl                                         # plafond du couloir (plat)
    # --- chambre : murs (ouverture de porte dans le mur nord, niche aux perles), sol, voute
    fb.base = 0.0
    nich = (L["niche_x"][0], L["niche_x"][1])
    ops = [dict(face="N", at=x_door, w=1.0, zb=0.0, zt=min(1.85, vs - 0.05), kind="door", leaf=False),
           dict(face="N", at=(nich[0] + nich[1]) / 2, w=nich[1] - nich[0], zb=0.33, zt=1.2, kind="arch")]
    Wd, Dd = iw + 2 * t, idp + 2 * t
    ucx = BL.walls(fb, M, Wd, Dd, 0.0, vs, t, ops, rng, quoin=False, frames=False)
    # fond de niche + banquette
    fb.blend = REND
    fb.box(((nich[0] + nich[1]) / 2, hy + 0.65, 0.9), (nich[1] - nich[0] + 0.05, 0.1, 1.9), M["rubble"], bevel=0.0)
    fb.blend = BR
    fb.box(((nich[0] + nich[1]) / 2, hy + 0.05, 0.165), (nich[1] - nich[0] + 0.3, 1.1, 0.33), M["ashlar"], bevel=0.015)
    # sol de dalles (sur un lit de mortier : pas de jour vers le lagon) + caniveau + puisard
    fb.blend = BR
    fb.box((0, 0, -0.25), (iw + 0.1, idp + 0.1, 0.4), M["ashlar"], bevel=0.0)
    G.slab_paving(fb, [(-hx, -hy), (hx, -hy), (hx, hy), (-hx, hy)], -0.06, M["ashlar"], rng, size=(0.35, 0.65),
                  th=0.06, blend=BR, holes=lambda c: abs(c.y - L["gutter_y"]) < 0.1)
    fb.blend = BR
    fb.box((0, L["gutter_y"], -0.05), (iw, 0.18, 0.04), M["ashlar"], bevel=0.004)
    fb.blend = RUB
    gx = hx - 0.35
    for k in range(5):
        fb.box((gx - 0.12 + 0.06 * k, L["gutter_y"], 0.004), (0.02, 0.26, 0.012), M["iron"], bevel=0.0)
    # voute en berceau surbaisse (axe X) : intrados en pierre de taille, tympans enduits, doubleaux de briques
    pts = BL.arch_points(-hy, hy, vs, vc, 14)
    poly = pts + [(hy + t, vs), (hy + t, vc + 0.35), (-hy - t, vc + 0.35), (-hy - t, vs)]
    k_ = len(poly)
    verts = [V(-hx, y, z) for y, z in poly] + [V(hx, y, z) for y, z in poly]
    faces = [tuple(range(k_)), tuple(range(k_, 2 * k_))[::-1]]
    for i in range(k_):
        j = (i + 1) % k_
        faces.append((i, j, k_ + j, k_ + i))
    fb.blend = ASH
    n0 = fb.mark()
    fb.poly_bm(verts, faces, M["ashlar"])
    fb.blend = REND
    for sx in (-1, 1):
        x0 = sx * hx
        tymp = list(pts)                                  # segment sous l'intrados (lunette du mur de bout)
        kk = len(tymp)
        vv = [V(x0, y, z) for y, z in tymp] + [V(x0 + sx * t, y, z) for y, z in tymp]
        ff = [tuple(range(kk)), tuple(range(kk, 2 * kk))[::-1]]
        for i in range(kk):
            j = (i + 1) % kk
            ff.append((i, j, kk + j, kk + i))
        fb.poly_bm(vv, ff, M["rubble"])
    fb.blend = RUB
    Rv = (hy * hy + (vc - vs) ** 2) / (2 * (vc - vs))
    zcv = vc - Rv
    a0 = math.atan2(vs - zcv, -hy)
    a1 = math.atan2(vs - zcv, hy)
    for xd in (-1.25, 0.0, 1.25):
        nv = 15
        for k in range(nv):
            aa = a0 + (a1 - a0) * (k + 0.04) / nv
            ab = a0 + (a1 - a0) * (k + 0.96) / nv
            q = [(Rv * math.cos(aa), zcv + Rv * math.sin(aa)), (Rv * math.cos(ab), zcv + Rv * math.sin(ab)),
                 ((Rv - 0.12) * math.cos(ab), zcv + (Rv - 0.12) * math.sin(ab)),
                 ((Rv - 0.12) * math.cos(aa), zcv + (Rv - 0.12) * math.sin(aa))]
            vv = [V(xd - 0.15, y, z) for y, z in q] + [V(xd + 0.15, y, z) for y, z in q]
            fb.poly_bm(vv, [(3, 2, 1, 0), (4, 5, 6, 7), (0, 1, 5, 4), (1, 2, 6, 5), (2, 3, 7, 6), (3, 0, 4, 7)],
                       M["brick"])
        for s_ in (-1, 1):
            fb.box((xd, s_ * (hy - 0.06), vs / 2), (0.3, 0.12, vs), M["brick"], bevel=0.008)
    # soupirail (lumiere froide) dans le mur nord, haut
    fb.blend = REND
    fb.box((0.0, hy + 0.3, 2.35), (0.42, 0.6, 0.06), M["rubble"], bevel=0.0)
    # --- couloir : branche 1 (depuis la cour, vers le sud), branche 2 (vers l'ouest, 6 marches), porte blindee
    ct = 0.5

    y1a = y_court
    xw = x_door - cw / 2                                  # extremite ouest du couloir 2
    yS, yN = y2 - cw / 2, y2 + cw / 2
    walls_c = [((d_x - cw / 2 - ct, d_x - cw / 2), (yN, y_court), zc - 0.2),        # couloir 1, mur ouest
               ((d_x + cw / 2, d_x + cw / 2 + ct), (yS - ct, y_court), -0.2),       # couloir 1, mur est (+ retour)
               ((xw - ct, d_x - cw / 2), (yN, yN + ct), -0.2),                     # couloir 2, mur nord
               ((x_door + 0.5, d_x + cw / 2), (yS - ct, yS), -0.2),                # couloir 2, mur sud
               ((xw - ct, xw), (yS - ct, yN + ct), -0.2)]                          # fermeture ouest
    for (xa, xb), (ya, yb), z0 in walls_c:
        fb.blend = REND
        q = [(xa, ya), (xb, ya), (xb, yb), (xa, yb)]
        G.block(fb, q, z0, zt, M["rubble"])
        ucx.append([V(x, y, z) for x, y in q for z in (z0, zt)])
    # plafonds (dalle enduite + poutres de pierre)
    fb.blend = REND
    fb.box((d_x, (y1a + y2 - cw / 2) / 2, zt + 0.15), (cw + 2 * ct, y1a - y2 + cw / 2, 0.3), M["rubble"], bevel=0.0)
    fb.box(((xw - ct + d_x + cw / 2) / 2, y2, zt + 0.15), (d_x + cw / 2 - xw + ct, cw + 2 * ct, 0.3),
           M["rubble"], bevel=0.0)
    fb.blend = ASH
    xx = x_door
    while xx < d_x:
        fb.box((xx, y2, zt - 0.06), (0.2, cw + 0.05, 0.12), M["ashlar"], bevel=0.01)
        xx += 1.1
    # sols : branche haute (Z cour), marches, branche basse
    G.slab_paving(fb, [(d_x - cw / 2, y2 - cw / 2), (d_x + cw / 2, y2 - cw / 2), (d_x + cw / 2, y1a),
                       (d_x - cw / 2, y1a)], zc - 0.06, M["ashlar"], rng, size=(0.3, 0.55), th=0.06, blend=BR)
    G.slab_paving(fb, [(x_steps0, y2 - cw / 2), (d_x - cw / 2, y2 - cw / 2), (d_x - cw / 2, y2 + cw / 2),
                       (x_steps0, y2 + cw / 2)], zc - 0.06, M["ashlar"], rng, size=(0.3, 0.55), th=0.06, blend=BR)
    fb.blend = REND
    fb.box(((x_steps0 + d_x + cw / 2) / 2, y2, (zc - 0.06 - 0.2) / 2 + 0.0), (d_x + cw / 2 - x_steps0, cw, zc - 0.06),
           M["rubble"], bevel=0.0)
    nst = 6
    rise = zc / nst
    tread = 0.28
    for k in range(nst):
        x1 = x_steps0 - k * tread
        ztop = zc - (k + 1) * rise
        fb.blend = REND
        fb.box((x1 - tread / 2, y2, ztop / 2 - 0.0), (tread, cw, max(ztop, 0.02)), M["rubble"], bevel=0.0)
        fb.blend = BR
        fb.box((x1 - tread / 2 + 0.01, y2, ztop + 0.0), (tread + 0.03, cw - 0.02, 0.05), M["ashlar"], bevel=0.01)
    xb = x_steps0 - nst * tread
    fb.blend = BR
    fb.box(((xw + xb) / 2, y2, -0.25), (xb - xw + 0.1, cw + 0.1, 0.4), M["ashlar"], bevel=0.0)
    fb.box((x_door, (hy + y2 - cw / 2) / 2, -0.25), (1.1, y2 - cw / 2 - hy + 0.1, 0.4), M["ashlar"], bevel=0.0)
    fb.box((d_x, (y1a + y2 - cw / 2) / 2, zc - 0.25), (cw + 0.1, y1a - y2 + cw / 2, 0.4), M["ashlar"], bevel=0.0)
    G.slab_paving(fb, [(xw, y2 - cw / 2), (xb, y2 - cw / 2), (xb, y2 + cw / 2),
                       (xw, y2 + cw / 2)], -0.06, M["ashlar"], rng, size=(0.3, 0.55), th=0.06, blend=BR)
    G.slab_paving(fb, [(x_door - 0.5, hy - 0.05), (x_door + 0.5, hy - 0.05), (x_door + 0.5, y2 - cw / 2),
                       (x_door - 0.5, y2 - cw / 2)], -0.06, M["ashlar"], rng, size=(0.3, 0.55), th=0.06, blend=BR)
    # portail cote cour (encadrement de pierre de taille sur la face du bastion)
    M4 = G.frame_M4(V(d_x, y_court, 0), V(1, 0, 0), V(0, 1, 0))
    BL.frame(fb, M, M4 @ Matrix.Translation((0, 0, zc)), dict(w=1.0, zb=zc, zt=zc + 1.8, kind="door", sill=0.0), 0.3,
             rng)
    fb.blend = ASH
    fb.box((d_x, y_court - 0.05, zc + 1.8 + 0.12), (1.4, 0.35, 0.24), M["ashlar"], bevel=0.01)
    # --- mobilier : table de compte, balance, registre, etagere, chaine de la lanterne centrale
    tx, ty = L["table_xy"]
    BL.table(fb, M, (tx, ty, 0.0), (1.4, 0.7, 0.66), rng)
    sx_, sy_, sz_ = p["sockets"]["Loot_Scale"]
    fb.blend = RUB
    fb.cyl(V(sx_, sy_, sz_), V(sx_, sy_, sz_ + 0.32), 0.008, M["iron"], segs=6)
    fb.box((sx_, sy_, sz_ + 0.005), (0.12, 0.08, 0.01), M["iron"], bevel=0.002)
    fb.box((sx_, sy_, sz_ + 0.32), (0.34, 0.012, 0.012), M["iron"], bevel=0.0)
    from fort_rampart import _revolve
    for s in (-1, 1):
        fb.cyl(V(sx_ + s * 0.16, sy_, sz_ + 0.32), V(sx_ + s * 0.16, sy_, sz_ + 0.12), 0.002, M["iron"], segs=4)
        tmp = _revolve([(0.0, 0.1), (0.05, 0.11), (0.06, 0.125)], 12)
        tmp.transform(Matrix.Translation((sx_ + s * 0.16, sy_, sz_)))
        fb.add_bm(tmp, M["iron"], smooth=True)
    fb.wood((tx - 0.45, ty + 0.12, 0.66 + 0.025), (0.32, 0.24, 0.05), M["tamanu"], bevel=0.01)
    fb.wood((tx - 0.45, ty + 0.12, 0.66 + 0.052), (0.3, 0.22, 0.006), M["tamanu"], bevel=0.0)
    # etagere de chene (2 niveaux) contre le mur est
    shy0, shy1 = L["shelf_y"]
    for z in (0.33, 0.82):
        fb.wood((hx - 0.2, (shy0 + shy1) / 2, z), (0.4, shy1 - shy0, 0.04), M["tamanu"], bevel=0.005, long_u=False)
    for y in (shy0 + 0.03, shy1 - 0.03):
        for x in (hx - 0.03, hx - 0.37):
            fb.wood((x, y, 0.45), (0.05, 0.05, 0.9), M["tamanu"], bevel=0.004, long_u=False)
    for k, y in enumerate((shy0 + 0.35, shy0 + 0.95)):
        BL.chest(fb, M, (hx - 0.2, y, 0.35), (0.3, 0.22, 0.2), rng, wood="tamanu", iron="iron")
        BL.chest(fb, M, (hx - 0.2, y + 0.3, 0.84), (0.26, 0.2, 0.18), rng, wood="tamanu", iron="iron")
    lx, ly, lz = p["sockets"]["Lantern_1"]
    fb.blend = RUB
    top_c = V(lx, ly, vc)
    n = 6
    for k in range(n):
        c = top_c.lerp(V(lx, ly, lz + 0.26), (k + 0.5) / n)
        rot = (k % 2) * math.pi / 2
        ring = [c + Matrix.Rotation(rot, 3, "Z") @ V(0.015 * math.cos(TAU * i / 6), 0, 0.035 * math.sin(TAU * i / 6))
                for i in range(6)]
        fb.tube(ring, 0.006, M["iron"], segs=3, closed=True)
    ob = fb.finish(coll)
    # UCX
    ucx = [("mur", None, q) for q in ucx]
    ucx += [("sol", None, G.rbox_pts((0, 0, -0.1), (iw, idp, 0.2))),
            ("voute_n", None, [V(x, y, z) for x in (-hx, hx) for y, z in ((hy * 0.33, vc), (hy, vs), (hy, vc + 0.3),
                                                                         (hy * 0.33, vc + 0.3))]),
            ("voute_s", None, [V(x, y, z) for x in (-hx, hx) for y, z in ((-hy * 0.33, vc), (-hy, vs), (-hy, vc + 0.3),
                                                                         (-hy * 0.33, vc + 0.3))]),
            ("voute_c", None, G.rbox_pts((0, 0, vc + 0.15), (iw, idp * 0.66, 0.3))),
            ("table", None, G.rbox_pts((tx, ty, 0.33), (1.4, 0.7, 0.66))),
            ("etagere", None, G.rbox_pts((hx - 0.2, (shy0 + shy1) / 2, 0.45), (0.4, shy1 - shy0, 0.9))),
            ("banquette", None, G.rbox_pts(((nich[0] + nich[1]) / 2, hy + 0.05, 0.165), (nich[1] - nich[0] + 0.3, 1.1,
                                                                                          0.33))),
            ("marches", None, [V(x, y, z) for y in (y2 - cw / 2, y2 + cw / 2) for x, z in
                               ((x_steps0, zc), (x_steps0 - nst * tread, 0.0), (x_steps0, 0.0))]),
            ("couloir_haut", None, G.rbox_pts(((x_steps0 + d_x + cw / 2) / 2, y2, (zc - 0.06) / 2),
                                              (d_x + cw / 2 - x_steps0, cw, zc - 0.06))),
            ("plafond_1", None, G.rbox_pts((d_x, (y1a + y2 - cw / 2) / 2, zt + 0.15), (cw + 2 * ct, y1a - y2 + cw / 2,
                                                                                        0.3))),
            ("plafond_2", None, G.rbox_pts(((x_door - cw / 2 + d_x + cw / 2) / 2, y2, zt + 0.15),
                                           (d_x - x_door + cw + 2 * ct, cw + 2 * ct, 0.3)))]
    socks = FK.sockets_of(p["sockets"])
    rot = {}
    for nm, (pos, rz) in L["door_sockets"].items():
        socks[nm] = ("root", Vector(pos))
        rot[nm] = (0, 0, rz)
    for k, (x, y) in enumerate(L["pallets"], 1):
        socks[f"Pallet_{k}"] = ("root", V(x, y, 0))
        rot[f"Pallet_{k}"] = (0, 0, 90)
    rot.update({"Loot_Chest_5": (0, 0, 90), "Lantern_2": (0, 0, 0), "Lantern_3": (0, 0, 90)})
    return FK.res([ob], socks, ucx, p, socket_rot=rot,
                  info={"chamber_m": [iw, idp], "vault_z": [vs, vc], "corridor_clear_m": cl, "steps": nst})


MODULES = {
    "treasure_vault": ("SM_Fort_TreasureVault", build_treasure_vault),
    "vault_door": ("SM_Fort_VaultDoor", build_vault_door),
    "vault_grille": ("SM_Fort_VaultGrille", build_vault_grille),
    "chest_closed": ("SM_Fort_Chest_Closed", build_chest_closed),
    "chest_open": ("SM_Fort_Chest_Open", build_chest_open),
    "pallet": ("SM_Fort_Pallet", build_pallet),
    "gold_bar": ("SM_Loot_GoldBar", build_gold_bar),
    "gold_stack_s": ("SM_Loot_GoldStack_S", _stack_builder("S")),
    "gold_stack_m": ("SM_Loot_GoldStack_M", _stack_builder("M")),
    "gold_stack_l": ("SM_Loot_GoldStack_L", _stack_builder("L")),
    "gems": (None, build_gems),
    "gem_bowl": ("SM_Loot_GemBowl", build_gem_bowl),
    "pearl_sack_open": ("SM_Loot_PearlSack_Open", build_pearl_sack_open),
    "pearl_sack_closed": ("SM_Loot_PearlSack_Closed", build_pearl_sack_closed),
    "coin": ("SM_Loot_Coin", build_coin),
    "coin_pile": ("SM_Loot_CoinPile", build_coin_pile),
}
