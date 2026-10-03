"""FBX exports of the tiki creatures for Unreal (conventions of scripts/character/bean_soldier.py).

Per creature, in art/export/Characters/<Name>/:
  SK_<Name>.fbx             body skeletal mesh + SKEL_<Name> (armature written as "Armature": Unreal drops it)
  SK_<Name>_<Weapon>.fbx    held object, skinned 100 % to weapon_r (+ prop_l for the bramble's dart), same skeleton:
                            Leader Pose on the body, or hide it to disarm
  SM_<Name>_<Weapon>.fbx    the same object as a static mesh: origin = grip, long axis +Z (pickups, displays)
  A_<Name>_<Anim>.fbx       one animation per file, skeleton only, in place, 30 fps
Materials: flat colours from the palette (box-projected CC0 detail is not baked yet), the CC0 fruit / shell / fern
models keep their image textures. Units: meters with apply_unit_scale (centimeters in Unreal), Z up, front -Y.
"""
import json
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix

ART = Path(__file__).resolve().parents[3] / "art"
EXPORT = ART / "export" / "Characters"


def fbx(objs, path, anim=False, types=("MESH", "ARMATURE")):
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.hide_set(False)
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    path.parent.mkdir(parents=True, exist_ok=True)
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types=set(types),
                             apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                             axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                             add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
                             use_armature_deform_only=True, armature_nodetype="NULL", bake_anim=anim,
                             bake_anim_use_all_bones=True, bake_anim_use_nla_strips=False,
                             bake_anim_use_all_actions=False, bake_anim_force_startend_keying=True,
                             bake_anim_step=1.0, bake_anim_simplify_factor=0.0)
    bpy.ops.object.select_all(action="DESELECT")
    return path


def static_weapon(cr, weapon):
    """Copy of the held object in grip space (origin = grip, long axis +Z), without the bramble's dart."""
    me = weapon.data.copy()
    bm = bmesh.new()
    bm.from_mesh(me)
    deform = bm.verts.layers.deform.verify()
    gi = weapon.vertex_groups.get("prop_l")
    if gi is not None:
        drop = [v for v in bm.verts if v[deform].get(gi.index, 0.0) > 0.5]
        bmesh.ops.delete(bm, geom=drop, context="VERTS")
    bm.to_mesh(me)
    bm.free()
    me.transform((Matrix.Translation(cr.grip_r) @ cr.w_rest_rot).inverted())
    ob = bpy.data.objects.new(f"SM_{cr.name}_{cr.s['weapon']}", me)
    bpy.context.scene.collection.objects.link(ob)
    return ob


def export_creature(cr, arm, meshes, actions):
    scene = bpy.context.scene
    out = EXPORT / cr.name
    files = []
    loc0 = arm.location.copy()
    arm.location = (0, 0, 0)
    ad = arm.animation_data_create()
    use_nla, action0 = ad.use_nla, ad.action
    ad.use_nla = False
    ad.action = None
    name0 = arm.name
    arm.name = "Armature"
    try:
        files.append(fbx([meshes["body"], arm], out / f"SK_{cr.name}.fbx"))
        if "weapon" in meshes:
            files.append(fbx([meshes["weapon"], arm], out / f"SK_{cr.name}_{cr.s['weapon']}.fbx"))
            sm = static_weapon(cr, meshes["weapon"])
            files.append(fbx([sm], out / f"SM_{cr.name}_{cr.s['weapon']}.fbx", types=("MESH",)))
            bpy.data.objects.remove(sm)
        f0, f1 = scene.frame_start, scene.frame_end
        for an, act in actions.items():
            ad.action = act
            if hasattr(ad, "action_slot") and act.slots:
                try:
                    ad.action_slot = act.slots[0]
                except (TypeError, RuntimeError):
                    pass
            a0, a1 = act.frame_range
            scene.frame_start, scene.frame_end = int(a0), int(a1)
            files.append(fbx([arm], out / f"A_{cr.name}_{an}.fbx", anim=True, types=("ARMATURE",)))
        scene.frame_start, scene.frame_end = f0, f1
    finally:
        arm.name = name0
        ad.action = action0
        ad.use_nla = use_nla
        arm.location = loc0
    register(cr, meshes, actions, files)
    print(f"[tiki] {cr.name}: {len(files)} FBX -> {out}")
    return files


def register(cr, meshes, actions, files):
    path = ART / "export" / "ue_manifest.json"
    manifest = json.loads(path.read_text(encoding="utf-8")) if path.exists() else {}
    base = f"/Game/MonkeIsland/Characters/{cr.name}"
    for f in files:
        key = Path(f).stem
        kind = key.split("_")[0]
        manifest[key] = {"folder": f"{base}/{'Anims' if kind == 'A' else 'Meshes'}",
                         "fbx": str(f).replace("\\", "/"), "group": "characters", "kind": kind,
                         "skeleton": f"SKEL_{cr.name}" if kind in ("SK", "A") else None,
                         "character": cr.s["cid"]}
    path.write_text(json.dumps(manifest, indent=1, ensure_ascii=False), encoding="utf-8")
