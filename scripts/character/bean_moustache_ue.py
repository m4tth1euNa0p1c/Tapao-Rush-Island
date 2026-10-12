"""T-192: the player bean, beige with a blond English moustache and blond hair, without the white face plate (owner
requests 2026-10-06/07), in Unreal.

Reimports SK_Bean_Body in place from art/export/Characters/BeanSoldier/SK_Bean_Body.fbx (scripts/character/bean_soldier.py,
moustache rigid on the head bone) with the settings of its first import (legacy FBX importer, convert scene, scale 1,
same skeleton and physics asset, no material, no animation), then sets every slot to its MI: the new slot
M_Bean_Moustache gets MI_Bean_Moustache (dark brown 3A241D, a copy of MI_Bean_Leather). The beige of the skin is
MI_Bean_Skin (D4B48D), set apart.
Runs in the editor: scripts/editor/lead_session_t186.py, action "bean".
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

FBX = ROOT / "art/export/Characters/BeanSoldier/SK_Bean_Body.fbx"
OUT = ROOT / "monke_island/Saved/T192"
DEST = "/Game/MonkeIsland/Characters/Bean/Meshes"
MATS = "/Game/MonkeIsland/Characters/Bean/Materials"
MESH = DEST + "/SK_Bean_Body"
SKELETON = DEST + "/SK_Bean_Body_Skeleton"
# colours of the new slots (sRGB), a copy of an existing MI of the bean each (scripts/character/bean_soldier.py PALETTE)
NEW_MIS = {"MI_Bean_Moustache": ("MI_Bean_Leather", (0xE0, 0xAE, 0x45)), "MI_Bean_Hair": ("MI_Bean_Skin", (0xED, 0xBE, 0x4F))}
EAL = ue.EditorAssetLibrary


def linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def main():
    kitlib.check_lock()
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"
    skeleton = kitlib.load(SKELETON)
    before = kitlib.load(MESH)
    assert skeleton and before, "SK_Bean_Body or its skeleton missing"
    physics = before.get_editor_property("physics_asset")
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
    task.filename = str(FBX)
    task.destination_path = DEST
    task.destination_name = "SK_Bean_Body"
    task.replace_existing = True
    task.automated = True
    task.options = options
    task.save = False
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
    mesh = kitlib.load(MESH)
    assert mesh and mesh.get_editor_property("skeleton") == skeleton, "reimport changed the skeleton"

    for name, (source, colour) in NEW_MIS.items():
        mi = kitlib.load(f"{MATS}/{name}") or EAL.duplicate_asset(f"{MATS}/{source}", f"{MATS}/{name}")
        ue.MaterialEditingLibrary.set_material_instance_vector_parameter_value(
            mi, "BaseColor", ue.LinearColor(*(linear(c) for c in colour), 1.0))
        kitlib.save(mi)

    slots = mesh.get_editor_property("materials")
    names = []
    for slot in slots:
        name = str(slot.get_editor_property("material_slot_name"))
        mi = kitlib.load(f"{MATS}/MI_{name[2:]}") if name.startswith("M_") else None
        assert mi, f"no MI for {name}"
        slot.set_editor_property("material_interface", mi)
        names.append(name)
    mesh.set_editor_property("materials", slots)
    if mesh.get_editor_property("physics_asset") != physics:
        mesh.set_editor_property("physics_asset", physics)
    kitlib.save(mesh)
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-192", "mesh": MESH, "slots": names, "new_mis": sorted(NEW_MIS),
              "physics": physics.get_path_name() if physics else None,
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "bean-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T192_BEAN_SUCCESS")


if __name__ == "__main__":
    main()
