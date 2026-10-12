"""Budget de polygones et skinning des mobs (repris de la maquette mob-coco-realiste du 2e PC).

Rapatrié de OneDrive/maquette-blender/factory/kits/coco_real_rig.py (2026-10-07) : décimation par pièce au budget
LOD0 (densité de triangles égalisée, visage protégé), marqueurs de catégorie __cd_<Central|Crest|Arm_L...> pour le
démembrement, puis le skinning tiki (colonne par hauteur, membres par distance aux os, pièces rigides). Les animations
GoreBox du 2e PC ne sont pas reprises (les clips du jeu restent ceux déjà importés).
"""
import math  # noqa: F401

import bmesh  # noqa: F401
import bpy
from mathutils import Matrix, Vector  # noqa: F401

from .base import T

TAG = "__cd_"


def tris_of(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def area_of(ob):
    return sum(p.area for p in ob.data.polygons)


# ------------------------------------------------------------------ budget de triangles
PROTECT = ("Mask", "Teeth", "Eyes", "EyesCore", "Sclera", "Lids", "Nose", "Lips", "Mouth", "Jaw", "Socket", "Paint",
           "DeadEyes", "Carve", "Tongue", "Face", "Rim", "Cavity")
THIN = ("Fibre", "Coir", "Fringe", "Brows", "Topknot", "Lashing", "Rope", "Necklace", "Wrap", "Lace", "Tuft", "Cord",
        "Leather", "Strap", "Quill", "Feather", "Armband", "Wristwrap", "Stem")


def part_weight(ob):
    if "budget_weight" in ob:                      # T-274 : poids imposé par la pièce (balanes, corail)
        return float(ob["budget_weight"])
    n = ob.name.split("_", 1)[-1]
    if ob.get("mask_part") or any(k in n for k in PROTECT):
        return 6.0
    if any(k in n for k in THIN):
        return 3.0
    return 1.0


def uv_weight(ob):
    """Échelle linéaire des îlots UV de la pièce (densité de texels) : visage x2, coque x1, membres x0,85,
    fibres / cordes x0,3."""
    n = ob.name.split("_", 1)[-1]
    if ob.get("mask_part") or any(k in n for k in PROTECT):
        return 2.0
    if any(k in n for k in THIN) or n in ("HuskFibres", "LooseCoir", "SlitFibres"):
        return 0.3
    if n in ("Body",):
        return 1.0
    if ob.get("organic") or n.startswith(("Arm_", "Leg_", "Hand_", "Foot_")):
        return 0.85
    return 0.6


def tag_uv_weight(ob):
    me = ob.data
    a = me.attributes.get("uv_weight") or me.attributes.new("uv_weight", "FLOAT", "FACE")
    w = uv_weight(ob)
    a.data.foreach_set("value", [w] * len(me.polygons))


def decimate_ob(ob, ratio):
    if ratio >= 0.985:
        return
    mod = ob.modifiers.new("Budget", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = max(0.02, ratio)
    mod.use_collapse_triangulate = False
    dg = bpy.context.evaluated_depsgraph_get()
    ev = ob.evaluated_get(dg)
    me = bpy.data.meshes.new_from_object(ev, preserve_all_data_layers=True, depsgraph=dg)
    old = ob.data
    ob.modifiers.remove(mod)
    ob.data = me
    me.name = old.name
    if old.users == 0:
        bpy.data.meshes.remove(old)


def fit_budget(objs, budget, floor=80):
    """Décime les pièces pour que la somme des triangles <= budget (densité tris/m² plafonnée, pondérée)."""
    items = [(o, tris_of(o), max(area_of(o), 1e-6), part_weight(o)) for o in objs]
    total = sum(t for _, t, _, _ in items)
    if total <= budget:
        return total, total

    def kept(D):
        return sum(min(t, max(min(t, floor), D * w * a)) for _, t, a, w in items)

    lo, hi = 0.0, max(t / (a * w) for _, t, a, w in items) * 1.01
    for _ in range(60):
        mid = 0.5 * (lo + hi)
        if kept(mid) > budget:
            hi = mid
        else:
            lo = mid
    D = lo
    for o, t, a, w in items:
        target = min(t, max(min(t, floor), D * w * a))
        decimate_ob(o, target / t)
    after = sum(tris_of(o) for o in objs)
    return total, after


# ------------------------------------------------------------------ catégories de démembrement
def category(tag):
    if tag.startswith(("arm_", "hand_")):
        return "Arm_" + tag[-1].upper()
    if tag.startswith(("leg_", "foot_")):
        return "Leg_" + tag[-1].upper()
    if tag in ("crest", "mask", "jaw"):
        return "Crest"
    return "Central"


def skin_mob(cr, arm, coll, budget_body=54000, budget_weapon=6000, extra_rigid=None):
    """Skinning tiki + décimation au budget + marqueurs __cd_<catégorie>. Retourne {'body', 'weapon'}."""
    rig, K = T["rig"], T["kit"]
    H = cr.H
    for ob in cr.weapon_parts:
        ob.matrix_world = Matrix.Translation(cr.grip_r) @ cr.w_rest_rot @ ob.matrix_world
    for ob in cr.prop_parts:
        ob.matrix_world = Matrix.Translation(cr.grip_l) @ cr.w_rest_rot @ ob.matrix_world
    allp = cr.parts + cr.weapon_parts + cr.prop_parts
    tags = {ob.name: ob.get("rig", "body") for ob in allp}
    K.bake_objects(allp)
    for c in getattr(cr, "cutters", []):
        bpy.data.objects.remove(c, do_unlink=True)
    cr.cutters = []
    raw_b, after_b = fit_budget([o for o in cr.parts if o.type == "MESH"], budget_body)
    raw_w, after_w = fit_budget([o for o in cr.weapon_parts + cr.prop_parts if o.type == "MESH"], budget_weapon)
    print(f"[coco-real] {cr.name}: corps {raw_b} -> {after_b} tris, arme {raw_w} -> {after_w} tris")
    bones = {b.name: (b.head_local.copy(), b.tail_local.copy()) for b in arm.data.bones}
    zs = [z * H for z in cr.s["J"]["spine"]]
    bounds = [("pelvis", -1e9, zs[0]), ("spine_01", zs[0], zs[1]), ("spine_02", zs[1], zs[2]), ("head", zs[2], 1e9)]
    blend = 0.06 * H
    rigid = {"crest": "crest", "weapon": "weapon_r", "prop": "prop_l", "mask": "head", "jaw": "jaw"}
    rigid.update(extra_rigid or {})
    for ob in allp:
        if ob.type != "MESH":
            continue
        tag = tags[ob.name]
        me = ob.data
        tag_uv_weight(ob)
        ob.vertex_groups.new(name=TAG + category(tag)).add(list(range(len(me.vertices))), 1.0, "REPLACE")
        if tag in ("body", "head", "skirt", "belt"):
            groups = {b: ob.vertex_groups.new(name=b) for b in rig.SPINE}
            for v in me.vertices:
                for b, w in rig.spine_weights(v.co.z, bounds, blend).items():
                    groups[b].add([v.index], w, "REPLACE")
        elif tag in rigid:
            ob.vertex_groups.new(name=rigid[tag]).add([v.index for v in me.vertices], 1.0, "REPLACE")
        else:
            names = rig.limb_bones(tag)
            groups = {n: ob.vertex_groups.new(name=n) for n in names}
            for v in me.vertices:
                p = v.co
                w = sorted(((1.0 / (rig.seg_dist(p, *bones[n]) + 0.012 * H) ** 4, n) for n in names), reverse=True)[:3]
                tot = sum(x for x, _ in w)
                for x, n in w:
                    groups[n].add([v.index], x / tot, "REPLACE")
    out = {"body": K.join([o for o in cr.parts if o.type == "MESH"], f"SK_{cr.name}")}
    held = [o for o in cr.weapon_parts + cr.prop_parts if o.type == "MESH"]
    if held:
        out["weapon"] = K.join(held, f"SK_{cr.name}_{cr.s['weapon']}")
    for ob in out.values():
        K.fixed_texspace(ob)
        for c in list(ob.users_collection):
            c.objects.unlink(ob)
        coll.objects.link(ob)
        mod = ob.modifiers.new("Armature", "ARMATURE")
        mod.object = arm
        ob.parent = arm
        ob.matrix_parent_inverse = arm.matrix_world.inverted()
    out["raw_tris"] = raw_b + raw_w
    return out


def strip_tmp_groups(ob):
    for g in [g for g in ob.vertex_groups if g.name.startswith("TMP_")]:
        ob.vertex_groups.remove(g)
