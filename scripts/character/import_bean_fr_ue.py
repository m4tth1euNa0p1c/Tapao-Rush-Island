"""T-204 / T-213: the French bean soldiers of the second PC (maquette bean-francais, owner request 2026-10-07) in Unreal.

- copies the maquette's exports (OneDrive, read only) to art/export/Characters/BeanFR/;
- SK_BeanFR_<Name> x7 (combined FBX: body, uniform and hat) on SK_Bean_Body_Skeleton, legacy FBX importer with the
  settings of the player's bean (convert scene, scale 1, no material, the bean's physics asset), LODs 50 % / 25 %;
- MI_BeanFR_<material>: MIs of M_Bean_Master with the base colour, roughness and metallic of the maquette's glTF
  (linear factors), set on the slots of the same name;
- A_BeanFR_Idle_Guard / Patrol / Bayonet_Thrust / Sabre_Combo on the same skeleton (24 fps, no root motion);
- SM_Bean_Bayonet (static, no collision, pivot at the muzzle).
Runs in the editor: scripts/editor/lead_session_t186.py, action "beanfrimport". Replays replace the assets.
Result: monke_island/Saved/T213/beanfr-import.json
"""
import datetime as dt
import json
import shutil
import struct
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

MAQUETTE = Path(r"C:\Users\matth\OneDrive\maquette-blender\maquettes\bean-francais")
STAGE = ROOT / "art/export/Characters/BeanFR"
OUT = ROOT / "monke_island/Saved/T213"
BASE = "/Game/MonkeIsland/Characters/BeanFR"
MESHES = BASE + "/Meshes"
ANIMS = BASE + "/Animations"
MATS = BASE + "/Materials"
SKELETON = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
BEAN = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body"
MASTER = "/Game/MonkeIsland/Characters/Bean/Materials/M_Bean_Master"
SKINS = ("CFM_Fusilier", "Fusilier_1767", "Fusilier_Bicorne", "Chasseur_Shako", "Grenadier", "Sergent", "Officier_Marine")
CLIPS = ("A_BeanFR_Idle_Guard", "A_BeanFR_Patrol", "A_BeanFR_Bayonet_Thrust", "A_BeanFR_Sabre_Combo")
LOD_COUNT = 3
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
EAL = ue.EditorAssetLibrary


def stage():
    """The maquette's files, complete (OneDrive synchronised), copied next to the other exports"""
    copied = []

    def copy(src, dst):
        assert src.exists() and src.stat().st_size > 0, f"missing or empty (OneDrive still syncing?): {src}"
        dst.parent.mkdir(parents=True, exist_ok=True)
        if not dst.exists() or dst.stat().st_size != src.stat().st_size or dst.stat().st_mtime < src.stat().st_mtime:
            shutil.copy2(src, dst)
            copied.append(dst.name)

    for name in SKINS:
        for suffix in (".fbx", ".glb"):
            copy(MAQUETTE / "exports" / name / f"SK_BeanFR_{name}{suffix}", STAGE / name / f"SK_BeanFR_{name}{suffix}")
    for clip in CLIPS:
        copy(MAQUETTE / "exports" / "anims" / f"{clip}.fbx", STAGE / "Anims" / f"{clip}.fbx")
    copy(MAQUETTE / "exports" / "Common" / "SM_Bean_Bayonet.fbx", STAGE / "Common" / "SM_Bean_Bayonet.fbx")
    for doc in ("BRIEF.md", "ue/BeanFR_EnemyManifest.json", "ue/BEAN_FR_ENEMY.md", "renders/preview.png", "renders/combat.png"):
        copy(MAQUETTE / doc, STAGE / "Source" / Path(doc).name)
    return copied


def glb_materials(path):
    """Materials of a glTF binary: linear base colour, metallic and roughness (glTF defaults: 1 and 1)"""
    data = path.read_bytes()
    length = struct.unpack_from("<I", data, 12)[0]
    doc = json.loads(data[20:20 + length])
    materials = {}
    for m in doc.get("materials", []):
        pbr = m.get("pbrMetallicRoughness", {})
        materials[m["name"]] = {"color": [float(c) for c in pbr.get("baseColorFactor", [0.8, 0.8, 0.8, 1.0])[:3]],
                                "metallic": float(pbr.get("metallicFactor", 1.0)),
                                "roughness": float(pbr.get("roughnessFactor", 1.0))}
    return materials


def key(name):
    return name.replace("-", "_").lower()


def short_name(name):
    short = name.replace("MAT-", "").replace("MAT_", "")
    return short[len("BeanFR_"):] if short.startswith("BeanFR_") else short


def material_instance(name, spec, master):
    asset = f"MI_BeanFR_{short_name(name)}"
    mi = kitlib.load(f"{MATS}/{asset}")
    if not mi:
        mi = TOOLS.create_asset(asset, MATS, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
    assert mi, asset
    mel = ue.MaterialEditingLibrary
    mel.set_material_instance_parent(mi, master)
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", ue.LinearColor(*spec["color"], 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", spec["roughness"])
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", spec["metallic"])
    kitlib.save(mi)
    return mi


def legacy_fbx(tasks):
    """Imports with the legacy FBX importer (the bean's settings), Interchange put back after"""
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        TOOLS.import_asset_tasks(tasks)
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))


def skin_task(name, skeleton, physics):
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.skeleton = skeleton
    options.physics_asset = physics
    options.create_physics_asset = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_SKELETAL_MESH
    data = options.skeletal_mesh_import_data
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    data.set_editor_property("convert_scene_unit", False)
    data.set_editor_property("update_skeleton_reference_pose", False)
    data.set_editor_property("use_t0_as_ref_pose", False)
    data.set_editor_property("import_meshes_in_bone_hierarchy", True)
    task = ue.AssetImportTask()
    task.filename = str(STAGE / name / f"SK_BeanFR_{name}.fbx")
    task.destination_path = MESHES
    task.destination_name = f"SK_BeanFR_{name}"
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = options
    return task


def clip_task(clip, skeleton):
    options = ue.FbxImportUI()
    options.import_mesh = False
    options.import_animations = True
    options.import_materials = False
    options.import_textures = False
    options.import_as_skeletal = True
    options.skeleton = skeleton
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_ANIMATION
    data = options.anim_sequence_import_data
    data.set_editor_property("import_translation", ue.Vector(0, 0, 0))
    data.set_editor_property("animation_length", ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    data.set_editor_property("remove_redundant_keys", False)
    task = ue.AssetImportTask()
    task.filename = str(STAGE / "Anims" / f"{clip}.fbx")
    task.destination_path = ANIMS
    task.destination_name = clip
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = options
    return task


def bayonet_task():
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("import_uniform_scale", 1.0)
    task = ue.AssetImportTask()
    task.filename = str(STAGE / "Common" / "SM_Bean_Bayonet.fbx")
    task.destination_path = MESHES
    task.destination_name = "SM_Bean_Bayonet"
    task.replace_existing = True
    task.automated = True
    task.save = False
    task.options = options
    return task


def main():
    kitlib.check_lock()
    copied = stage()
    skeleton = kitlib.load(SKELETON)
    bean = kitlib.load(BEAN)
    master = kitlib.load(MASTER)
    assert skeleton and bean and master, "the bean's skeleton, mesh or master material is missing"
    physics = bean.get_editor_property("physics_asset")
    legacy_fbx([skin_task(name, skeleton, physics) for name in SKINS] + [clip_task(c, skeleton) for c in CLIPS] + [bayonet_task()])

    report = {"task": "T-213", "copied": copied, "skins": {}, "clips": {}, "materials": {}}
    instances = {}
    lod_tool = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    for name in SKINS:
        mesh = kitlib.load(f"{MESHES}/SK_BeanFR_{name}")
        assert mesh, f"SK_BeanFR_{name} did not import"
        assert mesh.get_editor_property("skeleton") == skeleton, f"SK_BeanFR_{name}: not on the bean's skeleton"
        specs = {key(n): (n, s) for n, s in glb_materials(STAGE / name / f"SK_BeanFR_{name}.glb").items()}
        slots = mesh.get_editor_property("materials")
        names, missing = [], []
        for slot in slots:
            slot_name = str(slot.get_editor_property("material_slot_name"))
            found = specs.get(key(slot_name))
            if not found:
                missing.append(slot_name)
                continue
            source, spec = found
            if source not in instances:
                instances[source] = material_instance(source, spec, master)
                report["materials"][source] = {"mi": instances[source].get_name(), **{k: (round(v, 3) if isinstance(v, float) else [round(c, 3) for c in v]) for k, v in spec.items()}}
            slot.set_editor_property("material_interface", instances[source])
            names.append(slot_name)
        mesh.set_editor_property("materials", slots)
        if mesh.get_editor_property("physics_asset") != physics:
            mesh.set_editor_property("physics_asset", physics)
        lods = None
        try:
            if mesh.get_lod_num() < LOD_COUNT:
                lod_tool.regenerate_lod(mesh, LOD_COUNT, False, False)
            lods = mesh.get_lod_num()
        except Exception as error:  # noqa: BLE001 - the LODs are a nicety, the mesh is kept without them
            lods = f"not generated: {error}"
        kitlib.save(mesh)
        extent = mesh.get_bounds().box_extent
        report["skins"][name] = {"mesh": mesh.get_path_name(), "slots": names, "slots_without_material": missing, "lods": lods,
                                 "extent_cm": [round(extent.x, 1), round(extent.y, 1), round(extent.z, 1)]}
    for clip in CLIPS:
        sequence = kitlib.load(f"{ANIMS}/{clip}")
        assert sequence, f"{clip} did not import"
        kitlib.save(sequence)
        report["clips"][clip] = round(sequence.get_play_length(), 3)
    bayonet = kitlib.load(f"{MESHES}/SM_Bean_Bayonet")
    assert bayonet, "SM_Bean_Bayonet did not import"
    kitlib.save(bayonet)
    box = bayonet.get_bounding_box()
    report["bayonet"] = {"min": [round(box.min.x, 1), round(box.min.y, 1), round(box.min.z, 1)],
                         "max": [round(box.max.x, 1), round(box.max.y, 1), round(box.max.z, 1)]}
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "beanfr-import.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    ue.log("T213_BEANFR_IMPORT_SUCCESS")


if __name__ == "__main__":
    main()
