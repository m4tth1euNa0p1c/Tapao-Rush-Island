"""Démembrement des mobs en modules fermés (repris de la maquette mob-coco-realiste du 2e PC).

Rapatrié de OneDrive/maquette-blender/factory/kits/coco_real_gore.py (2026-10-07) : le maillage LOD0 skinné est
découpé en modules fermés (Head, Torso, Pelvis, Arm_L/R, Leg_L/R ; Torso_Upper / Torso_Lower en option). Chaque section
est scellée par un « cap » multi-couches recessé (UV dans l'atlas T_<Mob>_Gore, une tuile par couche), la coque est
creusée en plaie sous chaque membre, les poids sont rendus rigides par module. Changements : paramètre `only` (modules
à construire) ; profils de couches extensibles (PROFILES, complété par horror_mats pour le crabe et le veilleur).
Hitboxes et sockets du 2e PC non repris (le Physics Asset du jeu reste celui en place).
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector  # noqa: F401

from .base import T

TAG = "__cd_"
GRID = 4                    # atlas Gore : 4 x 4 tuiles

# couches de section : (matière, épaisseur_m ou None, fraction intérieure si None, retrait_m)
LIMB = [("skin", 0.003, 0.0, 0.0003), ("fat", 0.005, 0.0, 0.0008), ("muscle", None, 0.34, 0.003),
        ("bone", None, 0.22, -0.0012), ("marrow", None, 0.0, 0.0025)]
COCO_CENTRAL = [("husk_cut", 0.030, 0.0, 0.001), ("nut_shell", 0.008, 0.0, 0.0), ("meat", 0.018, 0.0, -0.0008),
                ("pulp", None, 0.5, 0.004), ("milk", None, 0.0, 0.009)]
COCO_SOCKET = [("husk_cut", 0.022, 0.0, 0.0), ("nut_shell", 0.006, 0.0, 0.0), ("meat", 0.010, 0.0, 0.0),
               ("muscle", None, 0.32, 0.003), ("bone", None, 0.2, -0.001), ("marrow", None, 0.0, 0.002)]
CURSED_CENTRAL = [("husk_cut", 0.032, 0.0, 0.001), ("nut_shell", 0.009, 0.0, 0.0), ("cursed_flesh", None, 0.42, 0.003),
                  ("cursed_core", None, 0.0, 0.007)]
CURSED_LIMB = [("skin", 0.003, 0.0, 0.0003), ("cursed_flesh", None, 0.32, 0.003), ("bone", None, 0.2, -0.0012),
               ("cursed_core", None, 0.0, 0.0025)]
CURSED_SOCKET = [("husk_cut", 0.024, 0.0, 0.0), ("nut_shell", 0.007, 0.0, 0.0), ("cursed_flesh", None, 0.3, 0.003),
                 ("bone", None, 0.2, -0.001), ("cursed_core", None, 0.0, 0.002)]
PROFILES = {"coco": dict(central=COCO_CENTRAL, limb=LIMB, socket=COCO_SOCKET),
            "cursed": dict(central=CURSED_CENTRAL, limb=CURSED_LIMB, socket=CURSED_SOCKET)}

MODULES = ("Head", "Torso", "Pelvis", "Arm_L", "Arm_R", "Leg_L", "Leg_R", "Torso_Upper", "Torso_Lower")
DENSITY = {"central": 520.0, "limb": 1050.0 * 0.62, "crest": 300.0}


# ------------------------------------------------------------------ outils bmesh
def loops_of(edges):
    todo = set(edges)
    out = []
    while todo:
        e = todo.pop()
        start, cur = e.verts
        chain = [start, cur]
        guard = 0
        while cur != start and guard < len(edges) + 3:
            guard += 1
            cand = [x for x in cur.link_edges if x in todo]
            if not cand:
                break
            nx = cand[0]
            todo.remove(nx)
            cur = nx.other_vert(cur)
            if cur != start:
                chain.append(cur)
        if cur == start and len(chain) >= 3:
            out.append(chain)
    return out


def tile_uv(kind, layers_order):
    i = layers_order.index(kind)
    tx, ty = i % GRID, i // GRID
    return tx / GRID, ty / GRID, 1.0 / GRID


def copy_loop_uvs(face, uvls):
    for lp in face.loops:
        v = lp.vert
        for uvl in uvls:
            for other in v.link_loops:
                if other.face is not face:
                    lp[uvl].uv = other[uvl].uv
                    break


def point_in_poly(p, poly):
    x, y = p
    inside = False
    n = len(poly)
    for i in range(n):
        x1, y1 = poly[i]
        x2, y2 = poly[(i + 1) % n]
        if (y1 > y) != (y2 > y) and x < (x2 - x1) * (y - y1) / (y2 - y1 + 1e-12) + x1:
            inside = not inside
    return inside


class CapCtx:
    """Contexte de scellement : matières des couches (index de slot), couches UV, ordre de l'atlas, H."""

    def __init__(self, ob, layer_mats, layers_order, H, uv_bake="UVBake"):
        self.ob, self.H = ob, H
        self.layer_mats = layer_mats
        self.order = layers_order
        self.uv_bake = uv_bake
        self.slots = {}

    def slot(self, kind):
        if kind in self.slots:
            return self.slots[kind]
        mat = self.layer_mats[kind]
        me = self.ob.data
        for i, m in enumerate(me.materials):
            if m == mat:
                self.slots[kind] = i
                return i
        me.materials.append(mat)
        self.slots[kind] = len(me.materials) - 1
        return self.slots[kind]


def section(bm, ctx, loop, normal, layers, planar_center=None, cap_faces=None):
    """Scelle une boucle de bord par des anneaux concentriques (couches), retrait organique, UV d'atlas Gore."""
    H = ctx.H
    n = Vector(normal).normalized()
    center = sum((v.co for v in loop), Vector()) / len(loop)
    if planar_center is not None:
        center -= n * ((center - Vector(planar_center)).dot(n))
    deform = bm.verts.layers.deform.active
    uvls = [bm.loops.layers.uv[k] for k in bm.loops.layers.uv.keys()]
    uvb = bm.loops.layers.uv.get(ctx.uv_bake)
    extent = max((v.co - center).length for v in loop)
    area = abs(sum((loop[j].co - center).cross(loop[(j + 1) % len(loop)].co - center).dot(n)
                   for j in range(len(loop)))) * 0.5
    slender = area / max(math.pi * extent * extent, 1e-12) < 0.12
    if extent < 0.016 * H or slender:
        adj = [f.material_index for v in loop for f in v.link_faces]
        try:
            f = bm.faces.new(loop)
        except ValueError:
            return None
        f.material_index = max(set(adj), key=adj.count) if adj else 0
        copy_loop_uvs(f, uvls)
        return f
    ua = n.orthogonal().normalized()
    va = n.cross(ua).normalized()

    def put_uv(face, kind):
        if uvb is None:
            return
        u0, v0, ts = tile_uv(kind, ctx.order)
        for lp in face.loops:
            d = lp.vert.co - center
            x, y = d.dot(ua) / extent, d.dot(va) / extent
            lp[uvb].uv = (u0 + ts * (0.5 + 0.46 * max(-1.0, min(1.0, x))), v0 + ts * (0.5 + 0.46 * max(-1.0, min(1.0, y))))

    for v in loop:
        for e in v.link_edges:
            if e.is_boundary:
                e.smooth = False
    rows = []
    r_out = 1.0
    for kind, th, fmin, depth in layers:
        inner = max(fmin, r_out - th / extent) if th is not None else fmin
        inner = max(0.0, min(r_out - 0.01, inner))
        rows.append(((r_out + inner) * 0.5, kind, depth))
        if inner > 0.02:
            rows.append((inner, kind, depth))
        r_out = inner
        if inner <= 0.02:
            break
    core = center.copy()
    if extent > 0.12 * H and abs(n.z) > 0.9 and layers[0][0] in ("skin",):
        core.y += extent * 0.3
    prev = loop
    made = []
    for ri, (frac, kind, depth) in enumerate(rows):
        cur = []
        for j, outer in enumerate(loop):
            ang = math.tau * j / len(loop)
            irr = 1.0 + 0.012 * math.sin(ang * 5 + 0.7) * math.cos(ang * 3 + 0.2) * (1 + 3 * (frac < 0.6))
            p = core + (outer.co - core) * (frac * irr)
            d = p - center
            x, y = d.dot(ua) / extent, d.dot(va) / extent
            lobes = 0.5 + 0.5 * math.sin(x * 7 + 1.4 * math.sin(y * 4)) * math.cos(y * 6 - x * 1.3)
            rec = depth * (0.6 + 0.4 * lobes) + 0.0004 * H
            p -= n * rec
            nv = bm.verts.new(p)
            if deform is not None:
                for g, w in outer[deform].items():
                    nv[deform][g] = w
            for sl in bm.verts.layers.shape.values():
                nv[sl] = p + (outer[sl] - outer.co)
            cur.append(nv)
        sl = ctx.slot(kind)
        for j in range(len(loop)):
            j2 = (j + 1) % len(loop)
            try:
                f = bm.faces.new((prev[j], prev[j2], cur[j2], cur[j]))
            except ValueError:
                continue
            f.material_index = sl
            f.smooth = True
            put_uv(f, kind)
            made.append(f)
        prev = cur
    kind, depth = rows[-1][1], rows[-1][2]
    cv = bm.verts.new(core - n * (depth * 1.1 + 0.0004 * H))
    for sl in bm.verts.layers.shape.values():
        cv[sl] = cv.co.copy()
    if deform is not None:
        acc = {}
        for s in loop:
            for g, w in s[deform].items():
                acc[g] = acc.get(g, 0.0) + w / len(loop)
        for g, w in acc.items():
            cv[deform][g] = w
    sl = ctx.slot(kind)
    for j in range(len(loop)):
        try:
            f = bm.faces.new((prev[j], prev[(j + 1) % len(loop)], cv))
        except ValueError:
            continue
        f.material_index = sl
        f.smooth = True
        put_uv(f, kind)
        made.append(f)
    if cap_faces is not None:
        cap_faces.extend(made)
    return made


def seal_plane(bm, ctx, center, normal, layers):
    """Ferme toutes les boucles de bord situées dans le plan : la plus grande reçoit les couches, les boucles
    contenues dans une plus grande sont repoussées sous le cap puis fermées à plat, les petites sont bouchées."""
    c, n = Vector(center), Vector(normal).normalized()
    edges = [e for e in bm.edges if e.is_boundary and all(abs((v.co - c).dot(n)) < 2e-5 for v in e.verts)]
    loops = loops_of(edges)
    ua = n.orthogonal().normalized()
    va = n.cross(ua).normalized()
    info = []
    for lp in loops:
        ctr = sum((v.co for v in lp), Vector()) / len(lp)
        ext = max((v.co - ctr).length for v in lp)
        poly = [((v.co - c).dot(ua), (v.co - c).dot(va)) for v in lp]
        info.append((ext, lp, poly, ((ctr - c).dot(ua), (ctr - c).dot(va))))
    info.sort(key=lambda x: -x[0])
    big = []
    for ext, lp, poly, c2 in info:
        inside = any(point_in_poly(c2, bp) and ext < be for be, bp in big)
        if inside:
            for v in lp:                       # vers l'intérieur de la pièce gardée (côté de la normale)
                v.co += n * (0.02 * ctx.H)
                for sl in bm.verts.layers.shape.values():
                    v[sl] = v[sl] + n * (0.02 * ctx.H)
            try:
                f = bm.faces.new(lp)
                copy_loop_uvs(f, [bm.loops.layers.uv[k] for k in bm.loops.layers.uv.keys()])
            except ValueError:
                pass
            continue
        res = section(bm, ctx, lp, -n, layers, center)
        if isinstance(res, list) and ext > 0.05 * ctx.H:
            big.append((ext, poly))
    return len(loops)


def socket_wound(bm, ctx, site, layers, shell_group):
    """Ouvre la coque sous un membre (cylindre site) et la referme par une plaie recessée (couches socket)."""
    c, n = Vector(site["center"]), Vector(site["normal"]).normalized()
    radius = site["radius"] * 0.91
    deform = bm.verts.layers.deform.active
    gi = ctx.ob.vertex_groups.get(shell_group)
    if gi is None or deform is None:
        return 0
    gi = gi.index
    old = {e for e in bm.edges if e.is_boundary}
    rm = []
    for f in bm.faces:
        if not all(v[deform].get(gi, 0.0) > 0.5 for v in f.verts):
            continue
        p = f.calc_center_median()
        d = p - c
        ax = d.dot(n)
        rad = (d - n * ax).length
        if ax > -0.002 * ctx.H and rad < radius:
            rm.append(f)
    if not rm:
        return 0
    bmesh.ops.delete(bm, geom=rm, context="FACES")
    rings = loops_of([e for e in bm.edges if e.is_boundary and e not in old])
    uvls = [bm.loops.layers.uv[k] for k in bm.loops.layers.uv.keys()]
    for ring in rings:
        inner = []
        for v in ring:
            d = v.co - c
            nv = bm.verts.new(v.co - n * (d.dot(n) + 0.0003 * ctx.H))
            for g, w in v[deform].items():
                nv[deform][g] = w
            for sl in bm.verts.layers.shape.values():
                nv[sl] = nv.co + (v[sl] - v.co)
            inner.append(nv)
        sl = ctx.slot(layers[0][0])
        for j in range(len(ring)):
            j2 = (j + 1) % len(ring)
            try:
                f = bm.faces.new((ring[j], ring[j2], inner[j2], inner[j]))
            except ValueError:
                continue
            f.material_index = sl
            u0, v0, ts = tile_uv(layers[0][0], ctx.order)
            uvb = bm.loops.layers.uv.get(ctx.uv_bake)
            if uvb is not None:
                for k, lp in enumerate(f.loops):
                    lp[uvb].uv = (u0 + ts * (0.1 + 0.8 * ((j + (k in (1, 2))) / len(ring))), v0 + ts * (0.3 + 0.4 * (k >= 2)))
            for uvl in uvls:
                if uvl.name != ctx.uv_bake:
                    for lp in f.loops:
                        lp[uvl].uv = (0.0, 0.0)
        section(bm, ctx, inner, n, layers, c)
    return len(rings)


# ------------------------------------------------------------------ modules
def remap_weights(ob, mapping, keep_only=None):
    """Fusionne des groupes d'os dans un os cible (poids rigides près des coupes)."""
    me = ob.data
    vg = ob.vertex_groups
    idx = {g.name: g.index for g in vg}
    for tgt in set(mapping.values()):
        if tgt not in idx:
            idx[tgt] = vg.new(name=tgt).index
    name_of = {g.index: g.name for g in vg}
    adds = {}
    for v in me.vertices:
        acc = {}
        for e in v.groups:
            nm = name_of[e.group]
            if nm.startswith((TAG, "TMP_")):
                continue
            acc[mapping.get(nm, nm)] = acc.get(mapping.get(nm, nm), 0.0) + e.weight
        tot = sum(acc.values()) or 1.0
        adds[v.index] = {k: w / tot for k, w in acc.items()}
    for g in list(vg):
        if not g.name.startswith(TAG) and g.name in mapping:
            vg.remove(g)
    for g in list(vg):
        if not g.name.startswith(TAG):
            g.remove(list(range(len(me.vertices))))
    idx = {g.name: g for g in vg}
    for vi, ws in adds.items():
        for k, w in ws.items():
            if k not in idx:
                idx[k] = vg.new(name=k)
            idx[k].add([vi], w, "REPLACE")


def split_module(src, name, coll, keep_cats, cuts, ctx_factory, sockets=()):
    """Copie src, garde les catégories keep_cats, coupe par les plans cuts [(centre, normale, couches)] (la partie
    gardée est du côté de la normale), scelle, ouvre les plaies sockets [(site, couches)]."""
    out = src.copy()
    out.data = src.data.copy()
    out.name = out.data.name = name
    coll.objects.link(out)
    ctx = ctx_factory(out)
    bm = bmesh.new()
    bm.from_mesh(out.data)
    deform = bm.verts.layers.deform.active
    keep_idx = [out.vertex_groups[TAG + c].index for c in keep_cats if out.vertex_groups.get(TAG + c)]
    drop = [v for v in bm.verts if not any(v[deform].get(i, 0.0) > 0.5 for i in keep_idx)]
    bmesh.ops.delete(bm, geom=drop, context="VERTS")
    for site, layers in sockets:
        socket_wound(bm, ctx, site, layers, TAG + "Shell")
    for center, normal, layers in cuts:
        bmesh.ops.bisect_plane(bm, geom=list(bm.verts) + list(bm.edges) + list(bm.faces), dist=1e-7,
                               plane_co=Vector(center), plane_no=Vector(normal), clear_inner=True, clear_outer=False)
        seal_plane(bm, ctx, center, normal, layers)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    bm.to_mesh(out.data)
    bm.free()
    return out


def hull_volume(ob):
    bm = bmesh.new()
    bm.from_mesh(ob.data)
    if len(bm.verts) < 4:
        bm.free()
        return 0.0
    bmesh.ops.convex_hull(bm, input=list(bm.verts))
    for v in [v for v in bm.verts if not v.link_faces]:
        bm.verts.remove(v)
    vol = abs(bm.calc_volume())
    bm.free()
    return vol


def build_modules(src, cr, coll, layer_mats, layers_order, profile, cuts_z, sites, mats_body_count=None,
                  only=None):
    """Retourne {module: objet}, rapport. cuts_z : neck / waist / bisect (m) ; sites : Arm_L... (centre, normale,
    rayon, os)."""
    H = cr.H
    prof = PROFILES[profile]
    up, dn = (0.0, 0.0, 1.0), (0.0, 0.0, -1.0)
    neck = (0.0, 0.0, cuts_z["neck"])
    waist = (0.0, 0.0, cuts_z["waist"])
    bis = (0.0, 0.0, cuts_z["bisect"])

    def ctxf(ob):
        return CapCtx(ob, layer_mats, layers_order, H)

    C = prof["central"]
    socks_torso = [(sites["Arm_L"], prof["socket"]), (sites["Arm_R"], prof["socket"])]
    socks_pelvis = [(sites["Leg_L"], prof["socket"]), (sites["Leg_R"], prof["socket"])]
    spec = {
        "Head": (["Central"], [(neck, up, C)], []),
        "Torso": (["Central"], [(neck, dn, C), (waist, up, C)], socks_torso),
        "Torso_Upper": (["Central"], [(neck, dn, C), (bis, up, C)], socks_torso),
        "Torso_Lower": (["Central"], [(bis, dn, C), (waist, up, C)], []),
        "Pelvis": (["Central"], [(waist, dn, C)], socks_pelvis),
    }
    for side in ("L", "R"):
        a, lg = sites["Arm_" + side], sites["Leg_" + side]
        spec["Arm_" + side] = (["Arm_" + side], [(a["center"], a["normal"], prof["limb"])], [])
        spec["Leg_" + side] = (["Leg_" + side], [(lg["center"], lg["normal"], prof["limb"])], [])
    if only:
        spec = {k: v for k, v in spec.items() if k in only}
    out, rep = {}, {}
    for name, (cats, cuts, socks) in spec.items():
        ob = split_module(src, f"SK_{cr.name}_{name}", coll, cats, cuts, ctxf, socks)
        if name == "Head":
            # la crête (crest, masque / mâchoire du Coco Maudit) part entière avec la tête, sans être coupée
            crest = split_module(src, f"SK_{cr.name}_Crest", coll, ["Crest"], [], ctxf, [])
            if len(crest.data.vertices):
                ob = T["kit"].join([ob, crest], ob.name)
            else:
                bpy.data.objects.remove(crest, do_unlink=True)
        out[name] = ob
    # poids rigides par module (la coupe ne s'étire jamais)
    rig = {"Head": {"pelvis": "head", "spine_01": "head", "spine_02": "head"},
           "Torso": {"pelvis": "spine_01", "head": "spine_02", "crest": "spine_02", "jaw": "spine_02"},
           "Torso_Upper": {"pelvis": "spine_02", "spine_01": "spine_02", "head": "spine_02", "crest": "spine_02",
                           "jaw": "spine_02"},
           "Torso_Lower": {"pelvis": "spine_01", "spine_02": "spine_01", "head": "spine_01"},
           "Pelvis": {"spine_01": "pelvis", "spine_02": "pelvis", "head": "pelvis"}}
    for side in ("l", "r"):
        rig["Arm_" + side.upper()] = {"spine_02": f"upperarm_{side}", "spine_01": f"upperarm_{side}"}
        rig["Leg_" + side.upper()] = {"pelvis": f"thigh_{side}"}
    for name, ob in out.items():
        remap_weights(ob, rig[name])
        for g in [g for g in ob.vertex_groups if g.name.startswith((TAG, "TMP_"))]:
            ob.vertex_groups.remove(g)
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        kind = "limb" if name.startswith(("Arm", "Leg")) else "central"
        vol = hull_volume(ob)
        mass = vol * DENSITY[kind]
        rep[name] = dict(triangles=sum(len(f.verts) - 2 for f in bm.faces),
                         boundary_edges=sum(e.is_boundary for e in bm.edges),
                         unweighted_vertices=sum(not v.groups for v in ob.data.vertices),
                         hull_volume_m3=round(vol, 5), mass_kg=round(mass, 2))
        bm.free()
        ob["dismemberment_piece"] = name
        ob["mass_kg"] = round(mass, 2)
    return out, rep


# ------------------------------------------------------------------ sites de coupe et sockets
def cut_sites(cr, neck_frac=None):
    H = cr.H
    J = cr.s["J"]["spine"]
    cuts = {"neck": (neck_frac or J[2]) * H, "waist": J[0] * H, "bisect": J[1] * H}
    sites = {}
    for s, side in ((1, "L"), (-1, "R")):
        sites["Arm_" + side] = dict(center=list(cr.J("shoulder", s) - Vector((s * 0.05 * H, 0, 0))),
                                    normal=[float(s), 0.0, 0.0], radius=cr.s["arm_r"] * H * 1.18,
                                    bone="upperarm_" + side.lower())
        sites["Leg_" + side] = dict(center=list(cr.J("hip", s)), normal=[0.0, 0.0, -1.0],
                                    radius=cr.s["leg_r"] * H * 1.08, bone="thigh_" + side.lower())
    return cuts, sites
