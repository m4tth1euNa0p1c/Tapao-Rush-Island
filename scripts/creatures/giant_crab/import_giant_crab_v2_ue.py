"""Giant crab "Tupa Nui" v2 (T-260, owner 2026-10-09: "modéliser ses pattes", "ultra terrifiant") into Unreal, IN PLACE.

Source: art/export/Characters/GiantCrab/ (built by scripts/creatures/giant_crab/build.py, manifest GiantCrabManifest_v2.json).
- Textures T_GiantCrab_{Body,Legs}_{BC,ORM,N} (2048) and T_GiantCrab_Eyes_{BC,ORM,N,E} (512) -> GiantCrab/Textures
  (BaseColor sRGB; ORM and the eye emissive mask linear masks; Normal DirectX, no green flip);
- MI_GiantCrab_Body / MI_GiantCrab_Legs from the project's baked master M_Character_Baked (usage flags skeletal mesh +
  morph targets saved on it); M_GiantCrab_Eyes (BaseColor / ORM / Normal / EmissiveMask textures, EmissiveColor vector
  and EmissiveStrength scalar: emissive = mask x colour x strength; the frenzy of T-261 sets EmissiveColor red) and
  MI_GiantCrab_Eyes; all in GiantCrab/Materials;
- SK_GiantCrab reimported over the existing asset on its own skeleton SK_GiantCrab_Skeleton and physics asset (same bone
  names, hierarchy and reference pose as v1), LOD1..3 reimported, MIs set on the slots by imported material name
  (MI_GiantCrab_Chitin -> MI_GiantCrab_Body, MI_GiantCrab_Legs -> MI_GiantCrab_Legs, MI_GiantCrab_Eye -> MI_GiantCrab_Eyes);
  sockets of the manifest checked, the missing ones added (SOCKET_LegTip_*: tips of the legs) with the scale of the
  existing ones (the bones carry a scale of 100: offsets divided by 100);
- the 26 clips A_GiantCrab_* (21 of v1 reimported in place, 5 new: BellySlam, Roar, LegStomp, Charge, Charge_R) on the
  skeleton, root motion per clip, notifies rebuilt from the manifest (track "Gameplay": cues of the actions; track
  "Steps": AN_Step_leg_<s><n>_dactyl at each foot contact), DeadEyes curves; AnimNotify Blueprints created if missing in
  /Game/MonkeIsland/Characters/_Shared/Notifies.
Editor open, no game running: scripts/editor/lead_session_t186.py, action "crabv2import". Rerunnable.
Report: monke_island/Saved/T260/giant_crab_v2_import.json (assets created / replaced, slots, sockets, LODs, timings).
"""
import datetime as dt
import json
import sys
import time
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

ART = ROOT / "art/export/Characters/GiantCrab"
OUT = ROOT / "monke_island/Saved/T260/giant_crab_v2_import.json"
CRAB = "/Game/MonkeIsland/Characters/GiantCrab"
MASTER = "/Game/MonkeIsland/Characters/_Shared/Materials/M_Character_Baked"
EYES_MASTER = CRAB + "/Materials/M_GiantCrab_Eyes"
NOTIFIES = "/Game/MonkeIsland/Characters/_Shared/Notifies"
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
MEL = ue.MaterialEditingLibrary
report = {"warnings": [], "timings_s": {}}
T0 = time.time()


def warn(text):
    report["warnings"].append(text)
    ue.log_warning("[GiantCrab v2] " + text)


def timed(name, fn, *a):
    t = time.time()
    try:
        return fn(*a)
    finally:
        report["timings_s"][name] = round(time.time() - t, 1)


def import_task(filename, dest, name, options=None):
    task = ue.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = dest
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    if options is not None:
        task.options = options
    TOOLS.import_asset_tasks([task])
    return EAL.load_asset(f"{dest}/{name}") if EAL.does_asset_exist(f"{dest}/{name}") else None


def setp(obj, prop, value):
    try:
        obj.set_editor_property(prop, value)
        return True
    except Exception as error:  # noqa: BLE001
        warn(f"{obj.get_name()}.{prop}: {error}")
        return False


# ---------------------------------------------------------------- textures
def textures(man):
    out, done = {}, []
    for t in man["textures"]:
        path = ROOT / t["file"]
        existed = EAL.does_asset_exist(f"{CRAB}/Textures/{t['asset']}")
        tex = import_task(path, CRAB + "/Textures", t["asset"])
        assert tex, f"texture {t['asset']} not imported"
        kind = t["kind"]
        if kind == "color":
            setp(tex, "srgb", True)
            setp(tex, "compression_settings", ue.TextureCompressionSettings.TC_DEFAULT)
            group = "TEXTUREGROUP_CHARACTER"
        elif kind == "normal_directx":
            setp(tex, "compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
            setp(tex, "srgb", False)
            setp(tex, "flip_green_channel", False)
            group = "TEXTUREGROUP_CHARACTER_NORMAL_MAP"
        else:
            setp(tex, "compression_settings", ue.TextureCompressionSettings.TC_MASKS)
            setp(tex, "srgb", False)
            group = "TEXTUREGROUP_CHARACTER_SPECULAR"
        if hasattr(ue.TextureGroup, group):
            setp(tex, "lod_group", getattr(ue.TextureGroup, group))
        kitlib.save(tex)
        out[t["asset"]] = tex
        done.append([t["asset"], "replaced" if existed else "created", kind])
    report["textures"] = done
    return out


# ---------------------------------------------------------------- materials
def master_baked():
    if EAL.does_asset_exist(MASTER):
        master = EAL.load_asset(MASTER)
    else:
        sys.path.insert(0, str(ROOT / "scripts/character"))
        import import_coco_real_ue as coco  # noqa: E402
        master = coco.master_material()
        report["master_created"] = True
    flags = [f for f in ("used_with_skeletal_mesh", "used_with_morph_targets") if not master.get_editor_property(f)]
    for f in flags:
        master.set_editor_property(f, True)
    if flags:
        MEL.recompile_material(master)
        kitlib.save(master)
    report["master"] = {"path": master.get_path_name(), "usage_flags_set": flags}
    return master


EYES_VERSION = "2"


def eyes_master(tex):
    """M_GiantCrab_Eyes : BaseColor, ORM (AO / rugosité / métal), Normal, EmissiveMask x EmissiveColor x EmissiveStrength ;
    DeadEyes (0..1) : œil laiteux et éteint (le morph DeadEyes n'arrive pas par l'import FBX hérité)."""
    if EAL.does_asset_exist(EYES_MASTER):
        mat = EAL.load_asset(EYES_MASTER)
        if EAL.get_metadata_tag(mat, "giant_crab_eyes_version") == EYES_VERSION:
            report["eyes_master"] = "kept"
            return mat
        MEL.delete_all_material_expressions(mat)
        report["eyes_master"] = "rebuilt"
    else:
        folder, name = EYES_MASTER.rsplit("/", 1)
        mat = TOOLS.create_asset(name, folder, ue.Material, ue.MaterialFactoryNew())
        report["eyes_master"] = "created"
    prop = ue.MaterialProperty

    def sampler(param, x, y, kind, default):
        node = MEL.create_material_expression(mat, ue.MaterialExpressionTextureSampleParameter2D, x, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", kind)
        node.set_editor_property("texture", default)
        return node

    base = sampler("BaseColor", -700, -300, ue.MaterialSamplerType.SAMPLERTYPE_COLOR, tex["T_GiantCrab_Eyes_BC"])
    orm = sampler("ORM", -700, 0, ue.MaterialSamplerType.SAMPLERTYPE_MASKS, tex["T_GiantCrab_Eyes_ORM"])
    nrm = sampler("Normal", -700, 300, ue.MaterialSamplerType.SAMPLERTYPE_NORMAL, tex["T_GiantCrab_Eyes_N"])
    emask = sampler("EmissiveMask", -700, 600, ue.MaterialSamplerType.SAMPLERTYPE_MASKS, tex["T_GiantCrab_Eyes_E"])
    color = MEL.create_material_expression(mat, ue.MaterialExpressionVectorParameter, -700, 900)
    color.set_editor_property("parameter_name", "EmissiveColor")
    color.set_editor_property("default_value", ue.LinearColor(1.0, 0.45, 0.06, 1.0))
    strength = MEL.create_material_expression(mat, ue.MaterialExpressionScalarParameter, -700, 1100)
    strength.set_editor_property("parameter_name", "EmissiveStrength")
    strength.set_editor_property("default_value", 2.0)
    spec = MEL.create_material_expression(mat, ue.MaterialExpressionConstant, -300, 150)
    spec.set_editor_property("r", 0.8)
    dead = MEL.create_material_expression(mat, ue.MaterialExpressionScalarParameter, -700, 1300)
    dead.set_editor_property("parameter_name", "DeadEyes")
    dead.set_editor_property("default_value", 0.0)
    veil = MEL.create_material_expression(mat, ue.MaterialExpressionConstant3Vector, -700, 1450)
    veil.set_editor_property("constant", ue.LinearColor(0.50, 0.48, 0.45, 1.0))
    alive = MEL.create_material_expression(mat, ue.MaterialExpressionOneMinus, -500, 1300)
    m1 = MEL.create_material_expression(mat, ue.MaterialExpressionMultiply, -350, 700)
    m2 = MEL.create_material_expression(mat, ue.MaterialExpressionMultiply, -200, 800)
    m3 = MEL.create_material_expression(mat, ue.MaterialExpressionMultiply, -80, 900)
    lerp_col = MEL.create_material_expression(mat, ue.MaterialExpressionLinearInterpolate, -250, -300)
    lerp_rough = MEL.create_material_expression(mat, ue.MaterialExpressionLinearInterpolate, -250, 0)
    dull = MEL.create_material_expression(mat, ue.MaterialExpressionConstant, -450, 80)
    dull.set_editor_property("r", 0.55)
    MEL.connect_material_expressions(base, "RGB", lerp_col, "A")
    MEL.connect_material_expressions(veil, "", lerp_col, "B")
    MEL.connect_material_expressions(dead, "", lerp_col, "Alpha")
    MEL.connect_material_property(lerp_col, "", prop.MP_BASE_COLOR)
    MEL.connect_material_property(orm, "R", prop.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_expressions(orm, "G", lerp_rough, "A")
    MEL.connect_material_expressions(dull, "", lerp_rough, "B")
    MEL.connect_material_expressions(dead, "", lerp_rough, "Alpha")
    MEL.connect_material_property(lerp_rough, "", prop.MP_ROUGHNESS)
    MEL.connect_material_property(orm, "B", prop.MP_METALLIC)
    MEL.connect_material_property(nrm, "RGB", prop.MP_NORMAL)
    MEL.connect_material_property(spec, "", prop.MP_SPECULAR)
    MEL.connect_material_expressions(emask, "R", m1, "A")
    MEL.connect_material_expressions(color, "", m1, "B")
    MEL.connect_material_expressions(m1, "", m2, "A")
    MEL.connect_material_expressions(strength, "", m2, "B")
    MEL.connect_material_expressions(dead, "", alive, "")
    MEL.connect_material_expressions(m2, "", m3, "A")
    MEL.connect_material_expressions(alive, "", m3, "B")
    MEL.connect_material_property(m3, "", prop.MP_EMISSIVE_COLOR)
    mat.set_editor_property("used_with_skeletal_mesh", True)
    mat.set_editor_property("used_with_morph_targets", True)
    MEL.recompile_material(mat)
    EAL.set_metadata_tag(mat, "giant_crab_eyes_version", EYES_VERSION)
    kitlib.save(mat)
    return mat


def instance(name, parent, textures_, scalars=None, vectors=None):
    path = f"{CRAB}/Materials/{name}"
    existed = EAL.does_asset_exist(path)
    mi = EAL.load_asset(path) if existed else TOOLS.create_asset(name, CRAB + "/Materials", ue.MaterialInstanceConstant,
                                                                  ue.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, parent)
    for param, tex in textures_.items():
        MEL.set_material_instance_texture_parameter_value(mi, param, tex)
    for param, value in (scalars or {}).items():
        MEL.set_material_instance_scalar_parameter_value(mi, param, float(value))
    for param, value in (vectors or {}).items():
        MEL.set_material_instance_vector_parameter_value(mi, param, ue.LinearColor(*value))
    kitlib.save(mi)
    report.setdefault("material_instances", []).append([name, "replaced" if existed else "created", parent.get_name()])
    return mi


def materials(man, tex):
    master = master_baked()
    eyes = eyes_master(tex)
    mis = {}
    for mi_name, spec in man["materials_ue"].items():
        parent = eyes if mi_name == "MI_GiantCrab_Eyes" else master
        mis[spec["slot"]] = instance(mi_name, parent, {p: tex[a] for p, a in spec["textures"].items()},
                                     spec.get("scalars"), spec.get("vectors"))
    return mis


# ---------------------------------------------------------------- skeletal mesh
def skeletal_options(skeleton, physics):
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.skeleton = skeleton
    if physics:
        options.physics_asset = physics
    options.create_physics_asset = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_SKELETAL_MESH
    data = options.skeletal_mesh_import_data
    for prop, value in (("import_uniform_scale", 1.0), ("convert_scene", True), ("force_front_x_axis", False),
                        ("convert_scene_unit", False), ("update_skeleton_reference_pose", False),
                        ("use_t0_as_ref_pose", False), ("import_meshes_in_bone_hierarchy", True),
                        ("import_morph_targets", True)):
        setp(data, prop, value)
    if hasattr(ue, "FBXNormalImportMethod"):
        setp(data, "normal_import_method", ue.FBXNormalImportMethod.FBXNIM_IMPORT_NORMALS_AND_TANGENTS)
    return options


def assign(mesh, mis):
    slots = list(mesh.get_editor_property("materials"))
    used = []
    for index, slot in enumerate(slots):
        imported = str(slot.get_editor_property("imported_material_slot_name"))
        mi = mis.get(imported)
        if mi:
            slot.set_editor_property("material_interface", mi)
            slots[index] = slot
            used.append(imported)
    for index, slot in enumerate(slots):
        imported = str(slot.get_editor_property("imported_material_slot_name"))
        if imported in mis and str(slot.get_editor_property("material_slot_name")) != imported:
            slot.set_editor_property("material_slot_name", imported)
            slots[index] = slot
            report.setdefault("slots_renamed", []).append([index, imported])
    mesh.set_editor_property("materials", slots)
    shown = []
    for index, slot in enumerate(mesh.get_editor_property("materials")):
        mat = slot.get_editor_property("material_interface")
        shown.append({"index": index, "slot": str(slot.get_editor_property("material_slot_name")),
                      "imported": str(slot.get_editor_property("imported_material_slot_name")),
                      "material": mat.get_name() if mat else None})
    missing = [k for k in mis if k not in used]
    if missing:
        warn(f"slots without the new material: {missing}")
    return shown


def sockets(mesh, man):
    """Sockets of the manifest: the existing ones are kept; the missing ones are added with the scale of the existing
    ones (offset in the bone's space: the bones carry the 100 scale of the armature)."""
    ratio = None
    for s in man["sockets"]:
        sock = mesh.find_socket(s["name"])
        loc = s["unreal_relative"]["location_cm"]
        if sock and sum(abs(v) for v in loc) > 1.0:
            rel = sock.get_editor_property("relative_location")
            got = (rel.x ** 2 + rel.y ** 2 + rel.z ** 2) ** 0.5
            want = sum(v * v for v in loc) ** 0.5
            ratio = got / want
            report["socket_scale_from"] = [s["name"], round(ratio, 4)]
            break
    if ratio is None:
        ratio = 0.01
        warn("no existing socket to read the scale: 1/100 assumed (bones scaled by 100)")
    added, kept, todo = [], [], []
    for s in man["sockets"]:
        if mesh.find_socket(s["name"]):
            kept.append(s["name"])
            continue
        loc = [round(v * ratio, 5) for v in s["unreal_relative"]["location_cm"]]
        rot = s["unreal_relative"]["rotation_deg_rpy"]
        try:
            sock = ue.SkeletalMeshSocket()
            sock.set_editor_property("socket_name", s["name"])
            sock.set_editor_property("bone_name", s["bone"])
            sock.set_editor_property("relative_location", ue.Vector(*loc))
            sock.set_editor_property("relative_rotation", ue.Rotator(roll=rot[0], pitch=rot[1], yaw=rot[2]))
            sock.set_editor_property("relative_scale", ue.Vector(1.0, 1.0, 1.0))
            mesh.add_socket(sock, False)
            added.append(s["name"])
        except Exception as error:  # noqa: BLE001  (UE 5.8 : SocketName en lecture seule depuis Python)
            todo.append({"name": s["name"], "bone": s["bone"], "relative_location": loc, "rotation_rpy": rot,
                         "why": str(error)[:120]})
    if todo:
        warn(f"{len(todo)} sockets not creatable from Python (to add by hand or with the MCP ObjectTools): "
             f"{[t['name'] for t in todo]}")
    found = [s["name"] for s in man["sockets"] if mesh.find_socket(s["name"])]
    return {"kept": len(kept), "added": added, "to_add_by_hand": todo, "found": len(found),
            "expected": len(man["sockets"])}


def skeletal(man, mis):
    path = f"{CRAB}/Meshes/SK_GiantCrab"
    before = kitlib.load(path)
    assert before, f"{path} missing: the v2 reimports over the v1 asset"
    skeleton = before.get_editor_property("skeleton")
    physics = before.get_editor_property("physics_asset")
    sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    entry = {"skeleton": skeleton.get_path_name(), "physics_asset": physics.get_path_name() if physics else None,
             "lods_before": sub.get_lod_count(before)}
    t = time.time()
    mesh = import_task(ROOT / man["meshes"]["lod0"], CRAB + "/Meshes", "SK_GiantCrab", skeletal_options(skeleton, physics))
    entry["lod0_import_s"] = round(time.time() - t, 1)
    assert mesh and mesh.get_editor_property("skeleton") == skeleton, "the reimport changed the skeleton"
    if physics and mesh.get_editor_property("physics_asset") != physics:
        mesh.set_editor_property("physics_asset", physics)
        warn("physics asset set back after the reimport")
    t = time.time()
    lods = []
    for lod in man["meshes"]["lods"]:
        try:
            lods.append([lod["lod"], sub.import_lod(mesh, lod["lod"], str(ROOT / lod["file"]))])
        except Exception as error:  # noqa: BLE001
            lods.append([lod["lod"], f"failed: {error}"])
            warn(f"LOD{lod['lod']}: {error}")
    entry["lods"] = lods
    entry["lods_import_s"] = round(time.time() - t, 1)
    entry["lod_count"] = sub.get_lod_count(mesh)
    entry["verts_per_lod"] = []
    for lod in range(entry["lod_count"]):
        try:
            entry["verts_per_lod"].append(sub.get_num_verts(mesh, lod))
        except Exception:  # noqa: BLE001
            entry["verts_per_lod"].append(None)
    entry["slots"] = assign(mesh, mis)
    entry["sockets"] = sockets(mesh, man)
    try:
        box = mesh.get_bounds()
        entry["bounds_extent_cm"] = [round(box.box_extent.x), round(box.box_extent.y), round(box.box_extent.z)]
    except Exception:  # noqa: BLE001
        pass
    try:
        entry["morph_targets"] = [str(n) for n in mesh.get_all_morph_target_names()]
        if "DeadEyes" not in entry["morph_targets"]:
            warn("morph target DeadEyes not imported by the legacy FBX importer (as for the other meshes of the "
                 "project): use the DeadEyes scalar of M_GiantCrab_Eyes (0 alive, 1 milky)")
    except Exception:  # noqa: BLE001
        pass
    kitlib.save(mesh)
    report["mesh"] = entry
    # the blueprint keeps the mesh; overrides of its materials would hide the new ones: read only
    bp = kitlib.load(CRAB + "/BP_GiantCrab")
    if bp:
        try:
            comp = ue.get_default_object(bp.generated_class()).get_editor_property("mesh")
            overrides = [m.get_name() if m else None for m in comp.get_editor_property("override_materials")]
            asset = comp.get_skeletal_mesh_asset() if hasattr(comp, "get_skeletal_mesh_asset") else None
            report["bp_mesh"] = {"mesh": asset.get_path_name() if asset else None, "override_materials": overrides}
            if any(overrides):
                warn(f"BP_GiantCrab overrides materials {overrides}: the new MIs are hidden there")
        except Exception as error:  # noqa: BLE001
            report["bp_mesh"] = f"not read: {error}"
    return mesh, skeleton


# ---------------------------------------------------------------- animations
_notify = {}


def notify_class(name):
    if name in _notify:
        return _notify[name]
    path = f"{NOTIFIES}/{name}"
    if not EAL.does_asset_exist(path):
        factory = ue.BlueprintFactory()
        factory.set_editor_property("parent_class", ue.AnimNotify)
        TOOLS.create_asset(name, NOTIFIES, ue.Blueprint, factory)
        kitlib.save(EAL.load_asset(path))
        report.setdefault("notify_classes_created", []).append(name)
    _notify[name] = EAL.load_blueprint_class(path)
    return _notify[name]


def clip_options(skeleton):
    options = ue.FbxImportUI()
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = True
    options.skeleton = skeleton
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_ANIMATION
    data = options.anim_sequence_import_data
    data.set_editor_property("animation_length", ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property("remove_redundant_keys", False)
    setp(data, "import_bone_tracks", True)
    return options


def animations(man, skeleton):
    lib = ue.AnimationLibrary
    done, failed = [], []
    for clip in man["clips"]:
        name = clip["asset"]
        path = f"{CRAB}/Anims/{name}"
        existed = EAL.does_asset_exist(path)
        try:
            seq = import_task(ROOT / clip["file"], CRAB + "/Anims", name, clip_options(skeleton))
            assert isinstance(seq, ue.AnimSequence), "not an AnimSequence"
            assert seq.get_editor_property("skeleton") == skeleton, "other skeleton"
            setp(seq, "enable_root_motion", bool(clip["enable_root_motion"]))
            setp(seq, "root_motion_root_lock", ue.RootMotionRootLock.REF_POSE)
            tracks = [str(t) for t in lib.get_animation_notify_track_names(seq)]
            for track in ("Gameplay", "Steps"):
                if track in tracks:
                    lib.remove_animation_notify_track(seq, track)
            gameplay = [n for n in clip["notifies"] if not n["name"].startswith("AN_Step_")]
            steps = [n for n in clip["notifies"] if n["name"].startswith("AN_Step_")]
            for track, events in (("Gameplay", gameplay), ("Steps", steps)):
                if not events:
                    continue
                lib.add_animation_notify_track(seq, track)
                for n in events:
                    lib.add_animation_notify_event(seq, track, float(n["time_s"]), notify_class(n["name"]))
            for cname, keys in (clip.get("curves") or {}).items():
                if lib.does_curve_exist(seq, cname, ue.RawCurveTrackTypes.RCT_FLOAT):
                    lib.remove_curve(seq, cname)
                lib.add_curve(seq, cname, ue.RawCurveTrackTypes.RCT_FLOAT, False)
                lib.add_float_curve_keys(seq, cname, [float(t) for t, _ in keys], [float(v) for _, v in keys])
            kitlib.save(seq)
            done.append([name, "replaced" if existed else "created", round(seq.get_editor_property("sequence_length"), 3),
                         len(gameplay), len(steps)])
        except Exception:  # noqa: BLE001
            failed.append([name, traceback.format_exc()[-600:]])
    report["animations"] = {"done": done, "failed": failed,
                            "columns": ["asset", "status", "seconds", "gameplay notifies", "step notifies"]}


def main():
    kitlib.check_lock()
    partial = [str(p) for p in ART.rglob("*") if p.name.endswith((".partial", ".partial.fbx", ".tmp"))]
    assert not partial, f"export not complete: {partial}"
    man = json.loads((ART / "GiantCrabManifest_v2.json").read_text(encoding="utf-8"))
    report["manifest"] = {"version": man["version"], "lod0_tris": man["meshes"]["lod0_tris"], "clips": len(man["clips"])}
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        tex = timed("textures", textures, man)
        mis = timed("materials", materials, man, tex)
        mesh, skeleton = timed("mesh", skeletal, man, mis)
        timed("animations", animations, man, skeleton)
        report["passed"] = not report["animations"]["failed"]
    except Exception:  # noqa: BLE001
        report["error"] = traceback.format_exc()[-3000:]
        report["passed"] = False
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
        report["total_s"] = round(time.time() - T0, 1)
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(report, indent=1, default=str, ensure_ascii=False), encoding="utf-8")
        ue.log(f"[GiantCrab v2] import {'ok' if report.get('passed') else 'FAILED'} in {report['total_s']} s -> {OUT}")
    if not report.get("passed"):
        raise RuntimeError(report.get("error") or f"animations failed: {report['animations']['failed']}")


if __name__ == "__main__":
    main()
