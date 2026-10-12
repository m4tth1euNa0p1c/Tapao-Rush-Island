"""Bean pirate shotgun pack v2, built in this repository (T-247, owner request 2026-10-09: realistic weapons, Call of
Duty style shooting and reloading, ammunition models): 1897 style pump gun with baked PBR textures, shells, the 16
handling gestures of the second PC's bean-pirate-shotgun v0.1 (same bones, sockets, events: the C++ and the import do
not change) plus Discard, in third (TP) and first person (FP) at 30 fps, Unreal FBX and a manifest of the same format.

Writes where scripts/character/import_pirate_weapons.py expects a local pack: build/packs/bean-pirate-shotgun/
(shotgun-manifest.json, exports/{Bases,Animations,Static}, textures/, renders/, fp-framing.json, validation/grips/).

  blender -b --factory-startup --python scripts/props/shotgun_pack/build.py -- [options]
    --model-only                weapon, textures, ammunition statics and the product renders (no gesture)
    --clips FirePump,Idle       only these clips (quick look; the full pack has 17 clips per view)
    --views FP                  only these views (TP,FP)
    --no-export                 no FBX / manifest (look only)
    --no-bake                   flat procedural materials, no texture bake (quick look of the gestures)
    --render FP:FirePump:12,TP:Idle:0   stills into <out>/renders/ (view:clip:frame)
    --product                   product renders (weapon and ammunition, Cycles)
    --save                      also save <out>/bean-pirate-shotgun.blend
    --out DIR                   pack folder (default build/packs/bean-pirate-shotgun)

Kit of the musket pack reused (scripts/props/musket_pack: kit.py, and the camera / solvers of its animation.py).
"""
from __future__ import annotations

import argparse
import hashlib
import importlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
MUSKET = REPO / "scripts" / "props" / "musket_pack"
sys.path.insert(0, str(MUSKET))
sys.path.insert(0, str(HERE))
import kit as K  # noqa: E402
import shotgun_model as SM  # noqa: E402
import textures as T  # noqa: E402

importlib.reload(K)
importlib.reload(SM)
importlib.reload(T)

SOURCE = REPO / "art/blender/characters/bean_soldier.blend"
PACK_SPEC = HERE / "pack.json"
STATE = {}
K.DENSITY.update({"MAT-Shotgun-Walnut": 650, "MAT-Shotgun-WalnutPump": 650, "MAT-Shotgun-BluedSteel": 7850,
                  "MAT-Shotgun-EdgeSteel": 7850, "MAT-Shotgun-Brass": 8500, "MAT-Shotgun-Recess": 7850,
                  "MAT-Shotgun-ShellPaper": 900, "MAT-Shotgun-ShellSpent": 600, "MAT-Shotgun-ShellBrass": 8500,
                  "MAT-Shotgun-Lead": 11340, "MAT-Ammo-Kraft": 300, "MAT-Ammo-Label": 300, "MAT-Ammo-Cream": 300})


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding="utf-8")


def active(obj):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def A():
    """The animation module (loaded late: the model-only build needs no bean)."""
    import animation
    importlib.reload(animation)
    return animation


# --------------------------------------------------------------------------------------------- the bean (source)

def add_grips(arm, meshes):
    """grip_l / grip_r: non deforming bones at the centre of the body mittens (hand targets)."""
    centers = {}
    for side in ("l", "r"):
        ob = next(o for o in meshes if o.name == "Bean_Hand_" + side)
        centers[side] = sum((ob.matrix_world @ Vector(c) for c in ob.bound_box), Vector()) / 8
    active(arm)
    bpy.ops.object.mode_set(mode="EDIT")
    for side in ("l", "r"):
        b = arm.data.edit_bones.new("grip_" + side)
        b.head = centers[side] + Vector((0, 0, .004))
        b.tail = centers[side]
        b.parent = arm.data.edit_bones["hand_" + side]
        b.use_deform = False
    bpy.ops.object.mode_set(mode="OBJECT")


def source_bean(scene):
    with bpy.data.libraries.load(str(SOURCE), link=False) as (src, dst):
        dst.collections = ["BeanSoldier"]
    coll = dst.collections[0]
    scene.collection.children.link(coll)
    arm = next(o for o in coll.all_objects if o.type == "ARMATURE")
    meshes = [o for o in coll.all_objects if o.type == "MESH" and any(m.type == "ARMATURE" and m.object == arm for m in o.modifiers)]
    add_grips(arm, meshes)
    fp = [o for o in coll.all_objects if o.type == "MESH" and o.name.startswith("FPArm_")]
    if not fp:
        raise SystemExit(f"{SOURCE} has no FPArm_* parts: run scripts/character/bean_soldier.py -- --export first")
    return dict(armature=arm, meshes=meshes, fp_meshes=fp, collection=coll)


def clone_arm(source, coll, name):
    arm = source.copy()
    arm.data = source.data.copy()
    arm.name = name
    coll.objects.link(arm)
    arm.animation_data_clear()
    arm.parent = None
    arm.matrix_world = Matrix.Identity(4)
    arm.hide_viewport = False
    arm.hide_render = False
    for p in arm.pose.bones:
        for c in list(p.constraints):
            p.constraints.remove(c)
        p.rotation_mode = "QUATERNION"
        p.matrix_basis = Matrix.Identity(4)
    return arm


def clone_meshes(source, arm, coll, prefix):
    result = []
    for o in source:
        ob = o.copy()
        ob.name = "GEO-" + prefix + "-" + o.name
        ob.animation_data_clear()
        coll.objects.link(ob)
        ob.parent = arm
        ob.matrix_parent_inverse = Matrix.Identity(4)
        ob.matrix_basis = o.matrix_basis.copy()
        ob.hide_render = False
        ob.hide_viewport = False
        for m in ob.modifiers:
            if m.type == "ARMATURE":
                m.object = arm
        result.append(ob)
    return result


def ammo_pouch(bean):
    """Small leather pocket at the left hip of the TP bean: where the support hand picks the shells (v0.1)."""
    B = K.bean()
    coll = bean["collection"]
    SM.materials()
    made = []
    for name, center, size, mat in (("LeatherPocket", (.235, -.185, .347), (.080, .030, .075), "M_Bean_Leather"),
                                    ("PocketRim", (.235, -.197, .384), (.076, .019, .010), "M_Bean_Navy"),
                                    ("PocketFlap", (.235, -.205, .362), (.068, .008, .040), "M_Bean_Leather"),
                                    ("BrassStud", (.235, -.211, .356), (.009, .004, .009), "M_Bean_Brass")):
        ob = B.box("AmmoPouch_" + name, center, size, mat, coll, bevel=min(size) * 0.3)
        vg = ob.vertex_groups.new(name="pelvis")
        vg.add(list(range(len(ob.data.vertices))), 1.0, "REPLACE")
        mod = ob.modifiers.new("Armature", "ARMATURE")
        mod.object = bean["armature"]
        ob.parent = bean["armature"]
        made.append(ob)
    bean["meshes"].extend(made)


def clone_bean(source, scene, view, clip):
    coll = bpy.data.collections.new("COL-Bean-" + view + "-" + clip)
    scene.collection.children.link(coll)
    arm = clone_arm(source["armature"], coll, "SKEL-Bean-" + view + "-" + clip)
    if view == "TP":
        result = clone_meshes(source["meshes"], arm, coll, view + "-" + clip)
    else:
        # the FP arms: the FPArm parts of the bean, rigid on hand_<side>, mitten centre on grip_<side> at rest
        result = []
        for side in ("l", "r"):
            Tm = A().fp_arm_transform(arm, side)
            for src in source["fp_meshes"]:
                me = src.data.copy()
                me.transform(Tm @ src.matrix_world)
                if Tm.determinant() < 0:
                    me.flip_normals()
                ob = bpy.data.objects.new("GEO-FP-" + clip + "-" + side + "-" + src.name, me)
                coll.objects.link(ob)
                ob.vertex_groups.new(name="hand_" + side).add(list(range(len(me.vertices))), 1., "REPLACE")
                mod = ob.modifiers.new("Bean hand rig", "ARMATURE")
                mod.object = arm
                ob.parent = arm
                ob["source_mesh"] = src.name
                ob["fp_source"] = "art/blender/characters/bean_soldier.blend"
                result.append(ob)
    return dict(armature=arm, meshes=result, collection=coll)


def clone_weapon(template, scene, view, clip):
    coll = bpy.data.collections.new("COL-Shotgun-" + view + "-" + clip)
    scene.collection.children.link(coll)
    arm = clone_arm(template["armature"], coll, "SKEL-Shotgun-" + view + "-" + clip)
    meshes = clone_meshes(template["meshes"], arm, coll, view + "-" + clip)
    return dict(armature=arm, meshes=meshes, collection=coll)


# --------------------------------------------------------------------------------------------- the weapon template

def procedural_materials():
    for mat in bpy.data.materials:
        if mat.get("bake_kind") and not mat.get("procedural"):
            T.apply_procedural(mat)


def bake_or_flat(ob, basename, matname, size, out, spec, bake):
    """Unique UVs, then the baked M_<name> (or the flat source materials with --no-bake)."""
    T.unwrap(ob, margin=0.003 if size >= 2048 else 0.006)
    if not bake:
        return None
    paths = T.bake_textures(ob, basename, size, Path(out) / "textures", ao_samples=spec.get("textures", {}).get("ao_samples", 24))
    mat = T.baked_material(matname, paths)
    T.assign(ob, mat)
    return paths


def build_template(coll, spec, out, bake=True):
    res = SM.build_shotgun(coll, dict(spec.get("shotgun_params", {})))
    parts = [o for objs in res["parts"].values() for o in objs]
    res["physics"] = K.part_physics(parts)
    procedural_materials()
    mesh = K.join_parts("SK_Bean_PirateShotgun", res["parts"], coll, bone_names=list(res["bones"]))
    for cutter in res["cutters"]:
        data = cutter.data
        bpy.data.objects.remove(cutter, do_unlink=True)
        if data.users == 0:
            bpy.data.meshes.remove(data)
    size = spec.get("textures", {}).get("weapon_size", 2048)
    res["textures"] = bake_or_flat(mesh, "T_Shotgun", "M_Shotgun", size, out, spec, bake)
    arm = K.make_skeleton("SKEL-Shotgun-Template", res["bones"], res["sockets"], coll)
    mesh.parent = arm
    mod = mesh.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    res.update(object=mesh, armature=arm)
    xs = [v.co for v in mesh.data.vertices]
    lo = [min(v[i] for v in xs) for i in range(3)]
    hi = [max(v[i] for v in xs) for i in range(3)]
    tris = sum(max(0, len(p.vertices) - 2) for p in mesh.data.polygons)
    contract = dict(res["contract"])
    contract.update(bones=list(res["bones"]) + ["socket_" + s for s in res["sockets"]],
                    socket_positions={"socket_" + s: [*v[1]] for s, v in res["sockets"].items()},
                    sockets={s: [*v[1]] for s, v in res["sockets"].items()}, params=res["params"], physics=res["physics"],
                    bounds={"min": lo, "max": hi, "size": [hi[i] - lo[i] for i in range(3)]}, triangle_count=tris,
                    weapon_triangle_count=tris, bounds_method="Rest mesh vertices (one skeletal mesh, shells at their sockets)")
    return {"armature": arm, "meshes": [mesh], "weapon_meshes": [mesh], "collection": coll, "contract": contract,
            "bounds": contract["bounds"], "physics": res["physics"], "materials": SM.materials()}


# --------------------------------------------------------------------------------------------- ammunition statics

def build_statics(scene, spec, out, bake=True):
    """SM_Shotgun_Spent, SM_Ammo_Shell, SM_Ammo_ShellBox (baked), SM_Shotgun_Slug, with UCX hulls; bottom-centre
    pivots. SM_Shotgun_Full is exported from SM_Ammo_Shell (same object, v0.1 name)."""
    B = K.bean()
    coll = bpy.data.collections.new("COL-Ammo-Statics")
    scene.collection.children.link(coll)
    size = spec.get("textures", {}).get("ammo_size", 1024)
    R = SM.SHELL_RADIUS
    statics = {}

    def ucx_prism(name, length, radius, z):
        ob = K.cylinder_x(name, -length * 0.5, length * 0.5, radius, (0.0, z), "M_Bean_Steel", coll, segs=10, caps=True)
        ob.data.materials.clear()
        return ob

    spent = K.join_parts("SM_Shotgun_Spent", {"root": SM.build_shell(coll, "Spent", "spent", (0, 0, R), mat_tag="spent_static")}, coll)
    procedural_materials()
    bake_or_flat(spent, "T_Shotgun_Spent", "M_Shotgun_Spent", size, out, spec, bake)
    statics["SM_Shotgun_Spent"] = (spent, [ucx_prism("UCX_SM_Shotgun_Spent_00", SM.SHELL_LENGTH, R, R)])

    shell = K.join_parts("SM_Ammo_Shell", {"root": SM.build_shell(coll, "Shell", "full", (0, 0, R), mat_tag="shell_static")}, coll)
    procedural_materials()
    bake_or_flat(shell, "T_Ammo_Shell", "M_Ammo_Shell", size, out, spec, bake)
    statics["SM_Ammo_Shell"] = (shell, [ucx_prism("UCX_SM_Ammo_Shell_00", SM.SHELL_LENGTH, R, R)])

    box = SM.build_shell_box(coll)
    boxob = K.join_parts("SM_Ammo_ShellBox", {"root": box["parts"]}, coll)
    procedural_materials()
    bake_or_flat(boxob, "T_Ammo_ShellBox", "M_Ammo_ShellBox", size, out, spec, bake)
    sx, sy, sz = box["size"]
    hull = B.box("UCX_SM_Ammo_ShellBox_00", (0, 0, sz * 0.5), (sx, sy, sz), "M_Bean_Cream", coll, bevel=0)
    hull.data.materials.clear()
    statics["SM_Ammo_ShellBox"] = (boxob, [hull])

    slug = K.join_parts("SM_Shotgun_Slug", {"root": SM.build_shell(coll, "Slug", "slug", (0, 0, 0.0097))}, coll)
    lead = bpy.data.materials.get("M_Ammo_Slug") or bpy.data.materials.new("M_Ammo_Slug")
    lead.use_nodes = True
    bsdf = next(n for n in lead.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    bsdf.inputs["Base Color"].default_value = T.srgb("7E848B")
    bsdf.inputs["Metallic"].default_value = 0.85
    bsdf.inputs["Roughness"].default_value = 0.42
    T.assign(slug, lead)
    statics["SM_Shotgun_Slug"] = (slug, [ucx_prism("UCX_SM_Shotgun_Slug_00", 0.03, 0.0097, 0.0097)])
    for ob, hulls in statics.values():
        for h in hulls:
            h.display_type = "WIRE"
            h.hide_render = True
    STATE["statics"] = statics
    STATE["statics_collection"] = coll
    return statics


# --------------------------------------------------------------------------------------------- stage and renders

def material(name, color, rough=.5, metal=0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == "BSDF_PRINCIPLED")
    p.inputs["Base Color"].default_value = (*color, 1)
    p.inputs["Roughness"].default_value = rough
    p.inputs["Metallic"].default_value = metal
    return m


def set_engine(scene, engine):
    try:
        scene.render.engine = engine
    except TypeError:
        scene.render.engine = "BLENDER_EEVEE_NEXT" if engine == "BLENDER_EEVEE" else engine


def stage(scene, view, clip):
    An = A()
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.length_unit = "METERS"
    scene.unit_settings.scale_length = 1
    set_engine(scene, "BLENDER_EEVEE")
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1280, 720, 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.fps = An.FPS
    scene.world = bpy.data.worlds.new("World-" + view + "-" + clip)
    scene.world.use_nodes = True
    w = next(n for n in scene.world.node_tree.nodes if n.type == "BACKGROUND")
    w.inputs["Color"].default_value = (.32, .42, .55, 1)
    w.inputs["Strength"].default_value = .9
    try:
        scene.view_settings.view_transform = "AgX"
    except TypeError:
        pass
    coll = bpy.data.collections.new("COL-Stage-" + view + "-" + clip)
    scene.collection.children.link(coll)
    me = bpy.data.meshes.new("REF-Floor")
    me.from_pydata([(-200, -200, 0), (200, -200, 0), (200, 200, 0), (-200, 200, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new("REF-Floor-" + view + "-" + clip, me)
    coll.objects.link(ob)
    ob.data.materials.append(material("MAT-Stage", (.62, .55, .42), .9))   # coral sand
    sun = bpy.data.lights.new("LGT-Sun-" + view + "-" + clip, "SUN")
    sun.energy = 3.2
    sun.angle = math.radians(3)
    light = bpy.data.objects.new(sun.name, sun)
    coll.objects.link(light)
    light.rotation_euler = (math.radians(50), math.radians(10), math.radians(150))
    data = bpy.data.cameras.new("CAM-" + view + "-" + clip)
    cam = bpy.data.objects.new(data.name, data)
    coll.objects.link(cam)
    if view == "FP":
        # the game's first person camera: 90 deg horizontal, looking straight ahead
        cam.matrix_world = An.camera_matrix()
        data.type = "PERSP"
        data.sensor_fit = "HORIZONTAL"
        data.sensor_width = 36.0
        data.lens = An.FP_LENS
        data.clip_start = .01
    else:
        cam.location = (1.58, -2.1, 1.17)
        target = Vector((-.03, -.10, .55))
        data.type = "ORTHO"
        data.ortho_scale = 2.4
        cam.rotation_euler = (target - cam.location).to_track_quat("-Z", "Y").to_euler()
    scene.camera = cam
    return cam


def render(scene, out, name, frame, res=1280):
    bpy.context.window.scene = scene
    scene.frame_set(frame)
    scene.render.resolution_x = res
    scene.render.resolution_y = int(res * 9 / 16)
    try:
        scene.eevee.taa_render_samples = 24
    except AttributeError:
        pass
    scene.render.filepath = str(Path(out) / "renders" / name)
    scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True, scene=scene.name)
    print("[shotgun] rendered " + name, flush=True)


def camera_record(cam):
    return dict(type=cam.data.type, matrix_world=[list(row) for row in cam.matrix_world], location_m=list(cam.location),
                rotation_euler_deg=[math.degrees(v) for v in cam.matrix_world.to_euler()], lens_mm=cam.data.lens,
                horizontal_fov_degrees=math.degrees(cam.data.angle_x), sensor_width_mm=cam.data.sensor_width,
                ortho_scale_m=cam.data.ortho_scale, clip_start_m=cam.data.clip_start, view_local_axis="-Z", up_local_axis="+Y")


def product_scene(template, statics, out):
    """Product views (Cycles): the weapon on a dark bench from its right side with the ammunition, a side view and
    an ammunition close-up."""
    scene = bpy.data.scenes.new("SCN-Product")
    bpy.context.window.scene = scene
    scene.unit_settings.system = "METRIC"
    scene.render.engine = "CYCLES"
    try:
        scene.cycles.device = "CPU"
    except AttributeError:
        pass
    scene.cycles.samples = 48
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1600, 900, 100
    scene.render.image_settings.file_format = "PNG"
    try:
        scene.view_settings.view_transform = "AgX"
        scene.view_settings.look = "AgX - Medium High Contrast"
    except TypeError:
        pass
    scene.world = bpy.data.worlds.new("World-Product")
    scene.world.use_nodes = True
    w = next(n for n in scene.world.node_tree.nodes if n.type == "BACKGROUND")
    w.inputs["Color"].default_value = (.35, .40, .47, 1)
    w.inputs["Strength"].default_value = .5
    coll = bpy.data.collections.new("COL-Product")
    scene.collection.children.link(coll)
    me = bpy.data.meshes.new("REF-Bench")
    me.from_pydata([(-3, -3, 0), (3, -3, 0), (3, 3, 0), (-3, 3, 0)], [], [(0, 1, 2, 3)])
    bench = bpy.data.objects.new("REF-Bench", me)
    coll.objects.link(bench)
    bench.data.materials.append(material("MAT-Bench", (.075, .062, .050), .75))
    for name, loc, power, size, color in (("Key", (0.8, -0.9, 1.2), 45, 1.5, (1, .93, .84)), ("Fill", (-1.0, -0.7, 0.8), 18, 2.0, (.85, .9, 1)),
                                          ("Rim", (-0.3, 1.0, 1.0), 40, 0.8, (1, .95, .9))):
        data = bpy.data.lights.new("LGT-Product-" + name, "AREA")
        data.energy = power
        data.size = size
        data.color = color
        light = bpy.data.objects.new(data.name, data)
        coll.objects.link(light)
        light.location = loc
        light.rotation_euler = (Vector((0.05, 0, 0.05)) - light.location).to_track_quat("-Z", "Y").to_euler()
    import export_pack
    me, _, _ = export_pack.weapon_rest_mesh(template)
    gun = bpy.data.objects.new("PRODUCT-Shotgun", me)
    coll.objects.link(gun)
    # the gun lies on its left side (ejection port up, top of the gun away from the camera), muzzle to the right
    R = Matrix.Rotation(math.radians(-90), 4, "X")
    lift = -min((R @ v.co).z for v in me.vertices)
    gun.matrix_world = Matrix.Translation((0.0, 0.0, lift)) @ R
    placed = []
    for i, (name, loc, rot) in enumerate((("SM_Ammo_ShellBox", (-0.20, -0.17, 0), 20), ("SM_Ammo_Shell", (-0.02, -0.14, 0), 75),
                                          ("SM_Shotgun_Spent", (0.08, -0.15, 0), 105), ("SM_Shotgun_Slug", (0.16, -0.13, 0), 60),
                                          ("SM_Ammo_Shell", (0.02, -0.19, 0), 15))):
        ob = statics[name][0]
        copy = bpy.data.objects.new("PRODUCT-" + name + str(i), ob.data)
        coll.objects.link(copy)
        copy.location = loc
        copy.rotation_euler = (0, 0, math.radians(rot))
        placed.append(copy)
    data = bpy.data.cameras.new("CAM-Product")
    cam = bpy.data.objects.new(data.name, data)
    coll.objects.link(cam)
    data.lens = 50
    scene.camera = cam

    def shoot(name, loc, target, lens=50, ortho=None):
        cam.location = loc
        cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat("-Z", "Y").to_euler()
        data.lens = lens
        if ortho:
            data.type = "ORTHO"
            data.ortho_scale = ortho
        else:
            data.type = "PERSP"
        scene.render.filepath = str(Path(out) / "renders" / name)
        bpy.ops.render.render(write_still=True, scene=scene.name)
        print("[shotgun] rendered " + name, flush=True)

    shoot("product-weapon.png", (0.30, -1.05, 0.72), (0.02, -0.07, 0.03), 40)
    shoot("product-side.png", (0.0, 0.0, 2.0), (0.0, 0.0, 0.0), 50, ortho=0.84)
    shoot("product-ammo.png", (0.0, -0.62, 0.34), (-0.02, -0.16, 0.02), 55)
    STATE["product_scene"] = scene


# --------------------------------------------------------------------------------------------- the clips

def release_intervals(clip):
    if clip == "Equip":
        return [[0, 38]]
    if clip == "Holster":
        return [[11, 54]]
    if clip == "ReloadInsert":
        return [[7, 48]]
    if clip == "ReloadEmpty":
        return [[s + 7, s + 48] for s in (32, 87, 142)]
    if clip == "Discard":
        return [[14, 40]]
    return []


def generate(spec, out, clips=None, views=("TP", "FP"), bake=True, model_only=False):
    An = None if model_only else A()
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    weapon_coll = bpy.data.collections.new("COL-Weapon-Template")
    root_scene.collection.children.link(weapon_coll)
    template = build_template(weapon_coll, spec, out, bake)
    statics = build_statics(root_scene, spec, out, bake)
    records = []
    manifest = dict(format="bean-pirate-shotgun/1", version=spec["version"], fps=30, local_build=True,
                    built_by="scripts/props/shotgun_pack/build.py (T-247)",
                    source=dict(blend=SOURCE.relative_to(REPO).as_posix(), sha256=sha(SOURCE)),
                    views=list(views), scenes=records, weapon_contract=template["contract"], assets=[])
    if model_only:
        manifest.update(requested_clips=list(spec_clips()), delivered_clips=[], not_delivered_clips=list(spec_clips()))
        STATE.update(manifest=manifest, template=template, source=None, root_scene=root_scene)
        return manifest
    src = source_bean(root_scene)
    ammo_pouch(src)
    clips = list(clips) if clips else list(An.CLIPS)
    for view in views:
        for clip in clips:
            print("[shotgun] " + view + " " + clip, flush=True)
            scene = bpy.data.scenes.new("SHOTGUN-" + view + "-" + clip)
            bpy.context.window.scene = scene
            bean = clone_bean(src, scene, view, clip)
            weapon = clone_weapon(template, scene, view, clip)
            cam = stage(scene, view, clip)
            qa = An.animate(scene, bean, weapon, clip, view)
            targets = qa.pop("grip_targets")
            records.append(dict(scene=scene.name, view=view, clip=clip, start=0, end=An.CLIPS[clip], fps=An.FPS, camera=cam.name,
                                camera_data=camera_record(cam),
                                actors=dict(bean=dict(armature=bean["armature"].name, meshes=[o.name for o in bean["meshes"]],
                                                      action=qa["bean_action"]),
                                            weapon=dict(armature=weapon["armature"].name, meshes=[o.name for o in weapon["meshes"]],
                                                        action=qa["weapon_action"])),
                                events=qa["events"], preview_fx=[],
                                grip_contract=dict(bean_tail_bones={"l": "grip_l", "r": "grip_r"},
                                                   weapon_head_bones={"l": "socket_grip_l", "r": "socket_grip_r"},
                                                   release_intervals=release_intervals(clip)), qa=qa))
            STATE.setdefault("targets", {})["%s-%s" % (view, clip)] = targets
            print("[shotgun] hand error %s, arm scale max %.3f" % (qa["hand_error_max_m"], qa["upperarm_scale_max"]), flush=True)
    bpy.context.window.scene = root_scene
    root_scene.collection.children.unlink(src["collection"])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = "SCN-bean-pirate-shotgun"
    manifest.update(requested_clips=list(An.CLIPS), delivered_clips=clips, not_delivered_clips=sorted(set(An.CLIPS) - set(clips)),
                    fp_framing=An.fp_framing(),
                    reload_sequence={"clips": ["ReloadStart", "ReloadInsert", "ReloadEnd", "ReloadEmpty"], "fps": An.FPS,
                                     "insert_event": "InsertSlug", "insert_frame": 36, "insert_clip_frames": 54,
                                     "note": "ReloadInsert keeps its 54 frames (1.8 s, C++ table unchanged); the shell moves from the "
                                             "pickup (17) to the port (36), 0.63 s. PlayRate 1.5 in DA_ShotgunRules gives 1.2 s per shell."},
                    notes=["Local rebuild of bean-pirate-shotgun v0.1 (T-247): same 16 gestures, lengths, bones, sockets and events, "
                           "plus Discard (Empty, Release) requested by the owner 2026-10-09.",
                           "First person authored through the game's camera (0, 0, 0.80 m, 90 deg): Idle frame 0 is the hip pose, "
                           "AimIdle frame 0 the aim pose with the brass bead on the view axis; fp_framing gives the DA values.",
                           "Weapon root is baked in bean coordinates: both actor roots at the same transform.",
                           "The spent shell on the bone shell_eject flies from EjectSpentShell (v0.1 behaviour); the game's own "
                           "SM_Shotgun_Spent pool doubles it unless SpentShellMesh is cleared.",
                           "Textures baked with Cycles: BaseColor sRGB, Normal DirectX (green down), ORM linear."])
    STATE.update(manifest=manifest, template=template, source=src, root_scene=root_scene)
    return manifest


def spec_clips():
    return ["Equip", "Idle", "Walk", "Sprint", "AimIn", "AimIdle", "AimOut", "FirePump", "AimFirePump", "ReloadStart", "ReloadInsert",
            "ReloadEnd", "ReloadEmpty", "DryFire", "Inspect", "Holster", "Discard"]


def post_build(out, model_only=False):
    manifest = STATE["manifest"]
    import export_pack
    importlib.reload(export_pack)
    manifest["assets"] = export_pack.export_pack(Path(out), manifest, STATE["source"], STATE["template"], STATE["statics"], model_only)
    write(Path(out) / "shotgun-manifest.json", manifest)
    if "fp_framing" in manifest:
        write(Path(out) / "fp-framing.json", manifest["fp_framing"])
    for name, targets in STATE.get("targets", {}).items():
        write(Path(out) / "validation" / "grips" / (name + ".json"), targets)
    bpy.context.window.scene = STATE["root_scene"]


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument("--clips", default="")
    p.add_argument("--views", default="TP,FP")
    p.add_argument("--no-export", action="store_true")
    p.add_argument("--no-bake", action="store_true")
    p.add_argument("--model-only", action="store_true")
    p.add_argument("--product", action="store_true")
    p.add_argument("--render", default="")
    p.add_argument("--save", action="store_true")
    p.add_argument("--out", default=str(REPO / "build" / "packs" / "bean-pirate-shotgun"))
    args = p.parse_args(argv)
    spec = json.loads(PACK_SPEC.read_text(encoding="utf8"))
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    clips = [c for c in args.clips.split(",") if c] or None
    views = tuple(v for v in args.views.split(",") if v)
    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    manifest = generate(spec, out, clips, views, bake=not args.no_bake, model_only=args.model_only)
    if not args.no_export:
        post_build(out, args.model_only)
    if args.product:
        product_scene(STATE["template"], STATE["statics"], out)
    for item in [r for r in args.render.split(",") if r]:
        view, clip, frame_ = item.split(":")
        rec = next(r for r in manifest["scenes"] if r["view"] == view and r["clip"] == clip)
        render(bpy.data.scenes[rec["scene"]], out, "%s-%s-%03d.png" % (view, clip, int(frame_)), int(frame_))
    if args.save:
        bpy.context.window.scene = STATE["root_scene"]
        bpy.ops.wm.save_as_mainfile(filepath=str(out / "bean-pirate-shotgun.blend"), compress=True)
    print("[shotgun] finished " + str(out), flush=True)


if __name__ == "__main__":
    main()
