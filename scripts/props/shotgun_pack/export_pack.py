"""Portable FBX bases / animations, static pickup and ammunition of the shotgun pack (T-247; local copy of the musket
pack's export_pack.py, itself from the second PC's v0.2).

Every animation contains a bind mesh (animation-only armature FBXs lose their bind pose on some Blender/Unreal round
trips); Unreal imports those as animations. The armature is exported as "Armature" (Unreal drops the node). Metres,
UnitScaleFactor 100 (apply_unit_scale, FBX_SCALE_UNITS): the import converts to centimetres with a scale of 1.
Static meshes carry UCX_<Mesh>_NN convex hulls (bottom-centre pivots). Textures: path_mode COPY for the bases and
the statics (.fbm next to the FBX, as v0.1), ABSOLUTE for the 68 animation files (no material is imported from them).
"""
from __future__ import annotations
import hashlib
import json
import shutil
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def select(objs):
    """Selects exactly objs in the window's view layer (the one the export operators use); every other view layer
    is cleared too, a collection shared by several scenes keeps one selection state per scene."""
    for sc in bpy.data.scenes:
        for vl in sc.view_layers:
            for o in list(vl.objects):
                if o is not None:
                    o.select_set(False, view_layer=vl)
    vl = bpy.context.window.view_layer
    for o in objs:
        o.hide_viewport = False
        o.hide_set(False, view_layer=vl)
        o.select_set(True, view_layer=vl)
    vl.objects.active = objs[0]


def fbx(path, arm, meshes, animated, bean=False):
    path.parent.mkdir(parents=True, exist_ok=True)
    scene = bpy.context.scene
    saved = arm.animation_data.action if arm.animation_data else None
    name = arm.name
    oldpose = {p.name: p.matrix_basis.copy() for p in arm.pose.bones}
    if not animated:
        if arm.animation_data:
            arm.animation_data.action = None
        for p in arm.pose.bones:
            p.matrix_basis = Matrix.Identity(4)
    other = bpy.data.objects.get("Armature")
    if other is not None and other != arm:
        other.name = "Armature-renamed-for-export"
    arm.name = "Armature"
    assert arm.name == "Armature", arm.name
    select([arm] + meshes)
    bpy.context.view_layer.update()
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"ARMATURE", "MESH"},
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
                             use_mesh_modifiers=True, mesh_smooth_type="FACE", add_leaf_bones=False, use_armature_deform_only=bean,
                             bake_anim=animated, bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True, bake_anim_step=1., bake_anim_simplify_factor=0.,
                             path_mode="ABSOLUTE" if animated else "COPY", embed_textures=False)
    arm.name = name
    for p in arm.pose.bones:
        p.matrix_basis = oldpose[p.name]
    if arm.animation_data:
        arm.animation_data.action = saved
    scene.frame_set(scene.frame_current)
    bpy.context.view_layer.update()


def mesh_export(path, meshes, kind):
    path.parent.mkdir(parents=True, exist_ok=True)
    select(meshes)
    if kind == "fbx":
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"MESH"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_UNITS", axis_forward="-Y", axis_up="Z",
                                 use_mesh_modifiers=True, mesh_smooth_type="FACE", add_leaf_bones=False, bake_anim=False, path_mode="COPY",
                                 embed_textures=False)
    else:
        bpy.ops.export_scene.gltf(filepath=str(path), export_format="GLB", use_selection=True, use_active_scene=True,
                                  export_animations=False, export_cameras=False, export_lights=False, export_apply=True)


def weapon_rest_mesh(template):
    """The weapon at rest without its two shells (vertices of the groups shell_load / shell_eject removed), with a
    bottom-centre pivot: (mesh, pivot, bounds of the weapon in its own space)."""
    arm = template["armature"]
    for p in arm.pose.bones:
        p.matrix_basis = Matrix.Identity(4)
    src = template["weapon_meshes"][0]
    me = src.data.copy()
    shell_groups = {src.vertex_groups[n].index for n in ("shell_load", "shell_eject") if n in src.vertex_groups}
    bm = bmesh.new()
    bm.from_mesh(me)
    layer = bm.verts.layers.deform.active
    if layer is not None and shell_groups:
        doomed = [v for v in bm.verts if any(gid in shell_groups for gid in v[layer].keys())]
        bmesh.ops.delete(bm, geom=doomed, context="VERTS")
    bm.to_mesh(me)
    bm.free()
    xs = [v.co for v in me.vertices]
    lo = [min(v[i] for v in xs) for i in range(3)]
    hi = [max(v[i] for v in xs) for i in range(3)]
    pivot = Vector(((lo[0] + hi[0]) / 2, (lo[1] + hi[1]) / 2, lo[2]))
    me.transform(Matrix.Translation(-pivot) @ src.matrix_world)
    return me, pivot, {"min": lo, "max": hi, "size": [hi[i] - lo[i] for i in range(3)]}


def pickup_mesh(template, scene):
    """The weapon at rest, without its two shells, moved to a bottom-centre pivot, with three UCX boxes."""
    me, pivot, bounds = weapon_rest_mesh(template)
    lo, hi = bounds["min"], bounds["max"]
    ob = bpy.data.objects.new("SM_Bean_PirateShotgun", me)
    scene.collection.objects.link(ob)
    # three convex boxes: stock, receiver with the fore-end, barrel
    hulls = []
    for i, (x0, x1, z0, z1, hw) in enumerate(((lo[0], -0.045, lo[2], 0.085, 0.032), (-0.055, 0.300, 0.020, 0.120, 0.021),
                                              (0.300, hi[0], 0.030, 0.112, 0.018))):
        hm = bpy.data.meshes.new("UCX_SM_Bean_PirateShotgun_%02d" % i)
        bm = bmesh.new()
        bmesh.ops.create_cube(bm, size=1.0)
        for v in bm.verts:
            v.co = Vector(((x0 + x1) / 2 + v.co.x * (x1 - x0), v.co.y * 2 * hw, (z0 + z1) / 2 + v.co.z * (z1 - z0))) - pivot
        bm.to_mesh(hm)
        bm.free()
        h = bpy.data.objects.new(hm.name, hm)
        scene.collection.objects.link(h)
        h.display_type = "WIRE"
        hulls.append(h)
    return ob, hulls, pivot, bounds


def export_pack(root, manifest, source, template, statics, model_only=False):
    root = Path(root)
    assets = []
    saved_scene = bpy.context.scene

    def asset(path, kind, group, **kw):
        row = dict(path=path.relative_to(root).as_posix(), type=kind, skeleton_group=group, sha256=sha(path), **kw)
        assets.append(row)
        return row

    if not model_only:
        # Separate FP mesh base, same original twenty-bone Bean rest hierarchy.
        for view in ("TP", "FP"):
            record = next((r for r in manifest["scenes"] if r["view"] == view), None)
            if record is None:
                continue
            scene = bpy.data.scenes[record["scene"]]
            bpy.context.window.scene = scene
            scene.frame_set(0)
            actor = record["actors"]["bean"]
            arm = bpy.data.objects[actor["armature"]]
            meshes = [bpy.data.objects[n] for n in actor["meshes"]]
            path = root / "exports" / "Bases" / ("SK_Bean_Shotgun_" + view + ".fbx")
            fbx(path, arm, meshes, False, True)
            asset(path, "skeletal_mesh", "Bean", view=view, bones=20, bind_mesh=True)
    # the weapon base from the authoritative template (bind pose, shells at their sockets as in v0.1)
    scene = bpy.data.scenes.new("SCN-Shotgun-Exports")
    bpy.context.window.scene = scene
    scene.collection.children.link(template["collection"])
    arm = template["armature"]
    path = root / "exports" / "Bases" / "SK_Bean_PirateShotgun.fbx"
    fbx(path, arm, template["meshes"], False)
    asset(path, "skeletal_mesh", "PirateShotgun", view="shared", bones=len(arm.data.bones), bind_mesh=True,
          materials=[m.name for m in template["meshes"][0].data.materials])
    if not model_only:
        for record in manifest["scenes"]:
            print("[shotgun export]", record["view"], record["clip"], flush=True)
            sc = bpy.data.scenes[record["scene"]]
            bpy.context.window.scene = sc
            sc.frame_start = record["start"]
            sc.frame_end = record["end"]
            sc.frame_set(0)
            for actor_name, group in [("bean", "Bean"), ("weapon", "PirateShotgun")]:
                actor = record["actors"][actor_name]
                a = bpy.data.objects[actor["armature"]]
                meshes = [bpy.data.objects[n] for n in actor["meshes"]]
                if actor_name == "bean" and record["view"] == "TP":
                    meshes = [next((o for o in meshes if "Bean_Body" in o.name), meshes[0])]
                p = root / "exports" / "Animations" / ("%s.fbx" % actor["action"])
                fbx(p, a, meshes, True, actor_name == "bean")
                actor["animation_fbx"] = p.relative_to(root).as_posix()
                asset(p, "animation", group, view=record["view"], clip=record["clip"], fps=record["fps"], start=record["start"], end=record["end"],
                      sample_count=record["end"] - record["start"] + 1, bind_mesh=True, events=record["events"])
        bpy.context.window.scene = scene
    # Static pickup from the template, bottom-centre pivot, three UCX boxes
    pickup, hulls, pivot, bounds = pickup_mesh(template, scene)
    for kind in ("fbx", "glb"):
        p = root / "exports" / "Static" / ("SM_Bean_PirateShotgun." + kind)
        mesh_export(p, [pickup] + (hulls if kind == "fbx" else []), kind)
        asset(p, "static_mesh", "none", view="pickup", pivot="bottom_center", source_pivot_offset_m=[*pivot], collision="UCX x3" if kind == "fbx" else "none",
              bounds=bounds)
    # Ammunition statics (built by build.build_statics in the root scene): SM_Shotgun_Full is SM_Ammo_Shell under its v0.1 name
    coll = statics[next(iter(statics))][0].users_collection[0]
    if coll.name not in scene.collection.children:
        scene.collection.children.link(coll)
    rows = [("SM_Shotgun_Spent", "SM_Shotgun_Spent", dict(ammunition_kind="spent", role="spent shell thrown at EjectSpentShell (ShotgunRules.SpentShellMesh), also the ejected shell of the clips")),
            ("SM_Ammo_Shell", "SM_Ammo_Shell", dict(ammunition_kind="full", role="loaded cartridge (pickup / inventory / loose shell)")),
            ("SM_Shotgun_Full", "SM_Ammo_Shell", dict(ammunition_kind="full", role="v0.1 name of SM_Ammo_Shell (same mesh) for the unchanged import list")),
            ("SM_Ammo_ShellBox", "SM_Ammo_ShellBox", dict(ammunition_kind="box", role="open box of 12 cartridges (ammo pickup / shop)")),
            ("SM_Shotgun_Slug", "SM_Shotgun_Slug", dict(ammunition_kind="slug", role="lead slug display token (v0.1)"))]
    for export_name, static_name, extra in rows:
        ob, ucx = statics[static_name]
        old = ob.name
        ob.name = export_name
        renamed = []
        for i, h in enumerate(ucx):
            renamed.append(h.name)
            h.name = "UCX_%s_%02d" % (export_name, i)
        for kind in ("fbx", "glb"):
            p = root / "exports" / "Static" / (export_name + "." + kind)
            mesh_export(p, [ob] + (ucx if kind == "fbx" else []), kind)
            asset(p, "static_mesh", "none", view="ammo", pivot="bottom_center", collision=("UCX x%d" % len(ucx)) if kind == "fbx" else "none",
                  material=ob.data.materials[0].name if ob.data.materials else None, **extra)
        ob.name = old
        for h, n in zip(ucx, renamed):
            h.name = n
    manifest["static_export_scene"] = scene.name
    materials = {}
    for obj in bpy.data.objects:
        if obj.type != "MESH":
            continue
        for mat in obj.data.materials:
            if not mat or mat.name in materials or mat.name.startswith("UCX"):
                continue
            p = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None) if mat.use_nodes else None
            if not p:
                continue
            textures = {}
            base_color_texture = None
            for n in mat.node_tree.nodes:
                if n.type == "TEX_IMAGE" and n.image and n.image.filepath:
                    src = Path(bpy.path.abspath(n.image.filepath)).resolve()
                    try:
                        relative = src.relative_to(root.resolve()).as_posix()
                    except ValueError:
                        target = root / "textures" / src.name
                        target.parent.mkdir(exist_ok=True, parents=True)
                        if src.exists():
                            shutil.copy2(src, target)
                        relative = target.relative_to(root).as_posix()
                        n.image.filepath = "//" + relative
                    textures[n.label or n.name] = relative
                    if any(link.from_node == n for link in p.inputs["Base Color"].links):
                        base_color_texture = relative
            row = dict(base_color=list(p.inputs["Base Color"].default_value), metallic=float(p.inputs["Metallic"].default_value),
                       roughness=float(p.inputs["Roughness"].default_value), textures=textures, base_color_texture=base_color_texture)
            if mat.get("baked"):
                row.update(baked=True, normal_convention=mat.get("normal_convention"), orm=mat.get("orm"),
                           colorspaces={"BaseColor": "sRGB", "Normal": "Non-Color (linear)", "ORM": "Non-Color (linear)"})
            if mat.get("bake_kind"):
                row.update(source_procedural=True, note="bake source, not exported on any mesh" if mat.name.startswith("MAT-") else None)
            materials[mat.name] = row
    manifest["materials"] = materials
    manifest["textures"] = sorted(p.relative_to(root).as_posix() for p in (root / "textures").glob("*.png")) if (root / "textures").is_dir() else []
    manifest["unreal_import"] = {"bean_source_skeleton_bones": 20, "bean_helper_grips_exported": False, "weapon_sockets_exported": True,
                                 "animation_only_import": True, "import_mesh": False, "fbx_frame_mapping": "Blender frames0..N reimport as1..N+1",
                                 "actor_attachment": "Both Bean and weapon actor roots use the same world transform; do not attach weapon to hand a second time.",
                                 "textures": "M_Shotgun / M_Shotgun_Spent / M_Ammo_Shell / M_Ammo_ShellBox: BaseColor (sRGB), Normal (DirectX: no green flip "
                                             "on import), ORM (linear, R occlusion G roughness B metallic: wire in the material after import)",
                                 "runtime_tested": False}
    manifest["coordinate_system"] = {"blender": "+X weapon forward, +Z up, -Y right side; bean looks along -Y", "fbx": "axis_forward -Y, axis_up Z, metres",
                                     "unreal": "convert_scene + convert_scene_unit, import scale 1"}
    bpy.context.window.scene = saved_scene
    return assets
