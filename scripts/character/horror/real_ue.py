"""Préparation Unreal des mobs (repris de la maquette mob-coco-realiste du 2e PC) : UV de bake, cuisson des
matériaux procéduraux en textures PNG, matériaux exportés, LOD, exports FBX.

Rapatrié de OneDrive/maquette-blender/factory/kits/coco_real_ue.py (2026-10-07) ; retirés : clips, statiques des
modules, GLB. Cuisson (Cycles, GPU si disponible) dans la couche UV « UVBake » (Smart UV Project, îlots du visage
agrandis) :
  T_<Mob>_<Set>_BC.png    couleur de base (passe Diffuse Color : crasse / cavités AO comprises), sRGB
  T_<Mob>_<Set>_N.png     normale tangente OpenGL (Unreal : Flip Green Channel, fait par le script d'import)
  T_<Mob>_<Set>_ORM.png   R = occlusion (bake AO), G = rugosité, B = métal (0), linéaire
  T_<Mob>_<Set>_E.png     émissif (braises, yeux), sRGB
Les matériaux exportés M_<Mob>_Body / _Weapon / _Gore pointent vers ces fichiers.
"""
import math
import os

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

from .base import T, set_action


def log(*a):
    print("[horror]", *a, flush=True)


# ------------------------------------------------------------------ UV
def select_only(obs, active=None):
    vl = bpy.context.view_layer
    for o in vl.objects:
        o.select_set(False)
    for o in obs:
        o.hide_set(False)
        o.select_set(True)
    vl.objects.active = active or obs[0]


def uv_islands(bm, uvl):
    """Îlots UV (faces reliées par des coins UV identiques)."""
    from collections import defaultdict
    key = lambda lp: (lp.vert.index, round(lp[uvl].uv.x, 5), round(lp[uvl].uv.y, 5))  # noqa: E731
    parent = {}

    def find(x):
        while parent[x] != x:
            parent[x] = parent[parent[x]]
            x = parent[x]
        return x

    for f in bm.faces:
        parent[f.index] = f.index
    seen = defaultdict(list)
    for f in bm.faces:
        for lp in f.loops:
            seen[key(lp)].append(f.index)
    for fl in seen.values():
        r0 = find(fl[0])
        for fi in fl[1:]:
            r = find(fi)
            if r != r0:
                parent[r] = r0
    isl = defaultdict(list)
    for f in bm.faces:
        isl[find(f.index)].append(f)
    return list(isl.values())


def bake_uv(ob, hero_mats=(), hero_scale=1.8, margin=0.004, layer="UVBake"):
    """Couche UVBake (active pour le bake ; UVMap reste la couche de rendu des feuilles / iris)."""
    me = ob.data
    render_uv = me.uv_layers.get("UVMap")
    uv = me.uv_layers.get(layer) or me.uv_layers.new(name=layer)
    me.uv_layers.active = uv
    if render_uv is not None:
        render_uv.active_render = True
    else:
        uv.active_render = True
    select_only([ob])
    with bpy.context.temp_override(active_object=ob, object=ob, selected_objects=[ob]):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        bpy.ops.uv.smart_project(angle_limit=math.radians(62), island_margin=margin, area_weight=0.0,
                                 correct_aspect=True, scale_to_bounds=False)
        bpy.ops.object.mode_set(mode="OBJECT")
    wattr = me.attributes.get("uv_weight")
    weights = None
    if wattr is not None:
        weights = [0.0] * len(me.polygons)
        wattr.data.foreach_get("value", weights)
    hero = {i for i, m in enumerate(me.materials) if m and m.name in hero_mats}
    if weights is not None or hero:
        bm = bmesh.new()
        bm.from_mesh(me)
        uvl = bm.loops.layers.uv[layer]
        bm.faces.ensure_lookup_table()
        n = 0
        for faces in uv_islands(bm, uvl):
            if weights is not None:
                sc = sum(weights[f.index] for f in faces) / len(faces)
                if sc <= 0.0:
                    sc = 0.6
            else:
                sc = hero_scale if sum(f.material_index in hero for f in faces) * 2 >= len(faces) else 1.0
            if abs(sc - 1.0) < 1e-3:
                continue
            pts = [lp[uvl].uv.copy() for f in faces for lp in f.loops]
            c = sum(pts, Vector((0, 0))) / len(pts)
            for f in faces:
                for lp in f.loops:
                    lp[uvl].uv = c + (lp[uvl].uv - c) * sc
            n += 1
        bm.to_mesh(me)
        bm.free()
        log(ob.name, ": îlots UV pondérés (", n, ")")
    with bpy.context.temp_override(active_object=ob, object=ob, selected_objects=[ob]):
        bpy.ops.object.mode_set(mode="EDIT")
        bpy.ops.mesh.select_all(action="SELECT")
        try:
            bpy.ops.uv.select_all(action="SELECT")
        except RuntimeError:
            pass
        bpy.ops.uv.pack_islands(rotate=True, margin=margin)
        bpy.ops.object.mode_set(mode="OBJECT")
    return uv


# ------------------------------------------------------------------ cuisson
def use_gpu(scene):
    scene.render.engine = "CYCLES"
    try:
        prefs = bpy.context.preferences.addons["cycles"].preferences
        for t in ("OPTIX", "CUDA"):
            try:
                prefs.compute_device_type = t
                prefs.get_devices()
                ok = False
                for d in prefs.devices:
                    d.use = d.type == t
                    ok = ok or d.use
                if ok:
                    scene.cycles.device = "GPU"
                    log("bake : GPU", t)
                    return t
            except TypeError:
                continue
    except Exception as ex:  # noqa: BLE001
        log("GPU indisponible :", ex)
    scene.cycles.device = "CPU"
    return "CPU"


def new_image(name, res, srgb=True, color=(0, 0, 0, 1)):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, res, res, alpha=False, float_buffer=not srgb)
    img.colorspace_settings.name = "sRGB" if srgb else "Non-Color"
    img.generated_color = color
    return img


def bake_pass(scene, ob, img, kind, samples=16, margin=12, **kw):
    """Une passe Cycles : chaque matériau de ob reçoit un nœud image actif = img."""
    nodes = []
    ao_keep = []
    if kind == "DIFFUSE":
        # la cavité (nœud AO) des matières est un effet écran faible dans EEVEE : en Cycles, les fibres posées sur la
        # bourre l'assombriraient partout -> rayon réduit pendant la cuisson de la couleur
        for slot in ob.material_slots:
            m = slot.material
            if m and m.use_nodes:
                for x in m.node_tree.nodes:
                    if x.type == "AMBIENT_OCCLUSION":
                        ao_keep.append((x, x.inputs["Distance"].default_value, x.only_local))
                        x.inputs["Distance"].default_value *= 0.12
                        x.only_local = True
    for slot in ob.material_slots:
        m = slot.material
        if not m or not m.use_nodes:
            continue
        n = m.node_tree.nodes.new("ShaderNodeTexImage")
        n.image = img
        n.name = "BAKE_TARGET"
        for x in m.node_tree.nodes:
            x.select = False
        n.select = True
        m.node_tree.nodes.active = n
        nodes.append((m, n))
    scene.cycles.samples = samples
    select_only([ob])
    b = scene.render.bake
    b.margin = margin
    b.use_clear = True
    b.target = "IMAGE_TEXTURES"
    if kind == "DIFFUSE":
        b.use_pass_direct = False
        b.use_pass_indirect = False
        b.use_pass_color = True
    if kind == "NORMAL":
        b.normal_space = "TANGENT"
    if kind == "DIFFUSE":
        kw.setdefault("pass_filter", {"COLOR"})
    if ob.data.uv_layers.get("UVBake"):
        kw.setdefault("uv_layer", "UVBake")
    with bpy.context.temp_override(active_object=ob, object=ob, selected_objects=[ob],
                                   selected_editable_objects=[ob]):
        bpy.ops.object.bake(type=kind, margin=margin, use_clear=True, target="IMAGE_TEXTURES", **kw)
    for m, n in nodes:
        m.node_tree.nodes.remove(n)
    for x, d, loc in ao_keep:
        x.inputs["Distance"].default_value = d
        x.only_local = loc


def pixels(img):
    a = np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(img.size[1], img.size[0], 4)


def save_png(arr, path, srgb, name):
    h, w = arr.shape[:2]
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, w, h, alpha=False)
    img.colorspace_settings.name = "sRGB" if srgb else "Non-Color"
    img.pixels.foreach_set(np.ascontiguousarray(arr, dtype=np.float32).ravel())
    os.makedirs(os.path.dirname(path), exist_ok=True)
    img.filepath_raw = path
    img.file_format = "PNG"
    img.save()
    img.source = "FILE"
    img.filepath = path
    img.reload()
    return img


def bake_set(scene, ob, res, out_dir, prefix, emissive=False, ao_samples=48, samples=8):
    """BC / N / ORM (/ E) d'un objet -> PNG ; retourne {clé: image chargée depuis le PNG}."""
    files = {}
    bc = new_image(prefix + "_tmpBC", res, srgb=True)
    bake_pass(scene, ob, bc, "DIFFUSE", samples=samples)
    files["BC"] = save_png(pixels(bc), os.path.join(out_dir, prefix + "_BC.png"), True, prefix + "_BC")
    nrm = new_image(prefix + "_tmpN", res, srgb=False, color=(0.5, 0.5, 1.0, 1.0))
    bake_pass(scene, ob, nrm, "NORMAL", samples=samples)
    files["N"] = save_png(pixels(nrm), os.path.join(out_dir, prefix + "_N.png"), False, prefix + "_N")
    rough = new_image(prefix + "_tmpR", res, srgb=False)
    bake_pass(scene, ob, rough, "ROUGHNESS", samples=1)
    ao = new_image(prefix + "_tmpAO", res, srgb=False, color=(1, 1, 1, 1))
    bake_pass(scene, ob, ao, "AO", samples=ao_samples)
    r, a = pixels(rough), pixels(ao)
    orm = np.zeros_like(r)
    orm[..., 0] = np.clip(0.25 + 0.75 * a[..., 0], 0, 1)
    orm[..., 1] = r[..., 0]
    orm[..., 2] = 0.0
    orm[..., 3] = 1.0
    files["ORM"] = save_png(orm, os.path.join(out_dir, prefix + "_ORM.png"), False, prefix + "_ORM")
    if emissive:
        em = new_image(prefix + "_tmpE", res, srgb=True)
        bake_pass(scene, ob, em, "EMIT", samples=1)
        e = pixels(em)
        e[..., :3] = np.clip(e[..., :3] / max(1.0, float(e[..., :3].max())), 0, 1)
        files["E"] = save_png(e, os.path.join(out_dir, prefix + "_E.png"), True, prefix + "_E")
        bpy.data.images.remove(em)
    for im in (bc, nrm, rough, ao):
        bpy.data.images.remove(im)
    return files


def gore_atlas_mesh(layer_mats, order, coll, size=0.35):
    """Plan de cuisson de l'atlas Gore : une tuile (quad) par couche, UV = tuile, matériau = couche."""
    from .real_gore import GRID
    bm = bmesh.new()
    uvl = bm.loops.layers.uv.new("UVBake")
    mats = []
    for i, kind in enumerate(order):
        if kind not in layer_mats:
            continue
        tx, ty = i % GRID, i // GRID
        x0, y0 = tx * size * 1.2, ty * size * 1.2
        vs = [bm.verts.new((x0, y0, 0)), bm.verts.new((x0 + size, y0, 0)), bm.verts.new((x0 + size, y0 + size, 0)),
              bm.verts.new((x0, y0 + size, 0))]
        f = bm.faces.new(vs)
        mats.append(layer_mats[kind])
        f.material_index = len(mats) - 1
        for lp, (u, v) in zip(f.loops, ((0, 0), (1, 0), (1, 1), (0, 1))):
            lp[uvl].uv = ((tx + u) / GRID, (ty + v) / GRID)
    me = bpy.data.meshes.new("GEO-GoreAtlasBake")
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    ob = bpy.data.objects.new(me.name, me)
    coll.objects.link(ob)
    T["kit"].fixed_texspace(ob)
    return ob


# ------------------------------------------------------------------ matériaux exportés
def baked_material(name, tex, emissive_strength=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    nt.nodes.clear()
    out = nt.nodes.new("ShaderNodeOutputMaterial")
    bs = nt.nodes.new("ShaderNodeBsdfPrincipled")
    nt.links.new(bs.outputs[0], out.inputs["Surface"])
    bc = nt.nodes.new("ShaderNodeTexImage")
    bc.image = tex["BC"]
    nt.links.new(bc.outputs["Color"], bs.inputs["Base Color"])
    orm = nt.nodes.new("ShaderNodeTexImage")
    orm.image = tex["ORM"]
    orm.image.colorspace_settings.name = "Non-Color"
    sep = nt.nodes.new("ShaderNodeSeparateColor")
    nt.links.new(orm.outputs["Color"], sep.inputs[0])
    nt.links.new(sep.outputs[1], bs.inputs["Roughness"])
    nt.links.new(sep.outputs[2], bs.inputs["Metallic"])
    nimg = nt.nodes.new("ShaderNodeTexImage")
    nimg.image = tex["N"]
    nimg.image.colorspace_settings.name = "Non-Color"
    nm = nt.nodes.new("ShaderNodeNormalMap")
    nt.links.new(nimg.outputs["Color"], nm.inputs["Color"])
    nt.links.new(nm.outputs["Normal"], bs.inputs["Normal"])
    if "E" in tex:
        e = nt.nodes.new("ShaderNodeTexImage")
        e.image = tex["E"]
        nt.links.new(e.outputs["Color"], bs.inputs["Emission Color"])
        bs.inputs["Emission Strength"].default_value = emissive_strength
    m["baked"] = True
    return m


def export_copy(ob, name, slot_map, keep_shape_keys=True):
    """Copie d'export : un matériau par set (slot_map : index de slot source -> matériau cuit), couche UV unique
    « UVMap » (ex-UVBake), groupes techniques retirés, même parent / armature."""
    new = ob.copy()
    new.data = ob.data.copy()
    new.name = new.data.name = name
    for c in ob.users_collection:
        c.objects.link(new)
    me = new.data
    if not keep_shape_keys and me.shape_keys:
        new.shape_key_clear()
    if me.shape_keys:
        for kb in me.shape_keys.key_blocks:
            kb.value = 0.0
        me.shape_keys.animation_data_clear()
    uniq = []
    for m in slot_map.values():
        if m not in uniq:
            uniq.append(m)
    remap = {i: uniq.index(m) for i, m in slot_map.items()}
    idx = np.zeros(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get("material_index", idx)
    idx = np.array([remap.get(int(i), 0) for i in idx], dtype=np.int32)
    me.materials.clear()
    for m in uniq:
        me.materials.append(m)
    me.polygons.foreach_set("material_index", idx)
    for uv in [u for u in me.uv_layers if u.name != "UVBake"]:
        me.uv_layers.remove(uv)
    if me.uv_layers.get("UVBake"):
        me.uv_layers["UVBake"].name = "UVMap"
        me.uv_layers.active = me.uv_layers["UVMap"]
        me.uv_layers["UVMap"].active_render = True
    for g in [g for g in new.vertex_groups if g.name.startswith(("__cd_", "TMP_"))]:
        new.vertex_groups.remove(g)
    for a in [a for a in me.attributes if a.name in ("uv_weight",) or a.name.startswith("__cd")]:
        me.attributes.remove(a)
    me.validate(clean_customdata=False)
    return new


def make_lod(src, name, ratio):
    ob = src.copy()
    ob.data = src.data.copy()
    ob.name = ob.data.name = name
    for c in src.users_collection:
        c.objects.link(ob)
    if ob.data.shape_keys:
        ob.shape_key_clear()
    arm_mod = [m for m in ob.modifiers if m.type == "ARMATURE"]
    for m in arm_mod:
        m.show_viewport = False
    mod = ob.modifiers.new("LOD", "DECIMATE")
    mod.decimate_type = "COLLAPSE"
    mod.ratio = ratio
    mod.use_collapse_triangulate = True
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    me = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), preserve_all_data_layers=True, depsgraph=dg)
    old = ob.data
    ob.modifiers.remove(mod)
    ob.data = me
    me.name = name
    for m in arm_mod:
        m.show_viewport = True
    if old.users == 0:
        bpy.data.meshes.remove(old)
    return ob


def tris(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


# ------------------------------------------------------------------ exports
def fbx(objs, path, anim=False, types=("MESH", "ARMATURE"), textures=True):
    select_only(objs)
    os.makedirs(os.path.dirname(path), exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=path, use_selection=True, object_types=set(types),
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             use_mesh_modifiers_render=True, add_leaf_bones=False, primary_bone_axis="Y",
                             secondary_bone_axis="X", use_armature_deform_only=True, armature_nodetype="NULL",
                             bake_anim=anim, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                             bake_anim_step=1.0, bake_anim_simplify_factor=0.0, use_tspace=True,
                             path_mode="RELATIVE" if textures else "STRIP", embed_textures=False)
    for o in objs:
        o.select_set(False)
    return path


class AsArmature:
    """Renomme temporairement l'armature « Armature » (Unreal ignore ce nœud racine), sans action, à l'origine."""

    def __init__(self, arm):
        self.arm = arm

    def __enter__(self):
        a = self.arm
        self.name0, self.loc0 = a.name, a.location.copy()
        clash = bpy.data.objects.get("Armature")
        if clash is not None and clash is not a:
            clash.name = "Armature_tmp"
        a.name = "Armature"
        a.location = (0, 0, 0)
        ad = a.animation_data_create()
        self.nla = ad.use_nla
        ad.use_nla = False
        set_action(a, None)
        bpy.context.view_layer.update()
        return a

    def __exit__(self, *exc):
        a = self.arm
        a.name = self.name0
        a.location = self.loc0
        a.animation_data.use_nla = self.nla
        set_action(a, None)
        return False
