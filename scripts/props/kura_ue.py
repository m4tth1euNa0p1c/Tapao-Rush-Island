"""T-190: the realistic kura in Unreal: SM_Village_Kura reimported in place from art/export/Props/Village/SM_Village_Kura.fbx
(scripts/props/kura_export.py), so the fragment villages (DA_Villages_*), the village PCG of Lvl_Raraka and the actors
placed in the levels keep their reference. Materials of the previous version slot for slot (MI_<name> otherwise),
Nanite and collision trace flag kept. A reimport keeps the old simple collision (pitfall of T-149): the open heavy
doors get their two boxes when they are missing.
Runs in the editor: scripts/editor/lead_session_t186.py, action "kura".
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

FBX = ROOT / "art/export/Props/Village/SM_Village_Kura.fbx"
OUT = ROOT / "monke_island/Saved/T190"
VILLAGE = "/Game/MonkeIsland/Environment/Village"
MESH = VILLAGE + "/Meshes/SM_Village_Kura"
# the open heavy doors, Unreal cm (centre, half size): scripts/props/village/props.py kura ucx 1 and 2
DOOR_BOXES = [((-90.0, 322.0, 160.0), (9.0, 36.0, 105.0)), ((90.0, 322.0, 160.0), (9.0, 36.0, 105.0))]


def main():
    kitlib.check_lock()
    old = kitlib.load(MESH)
    assert old, MESH
    by_slot = {str(s.get_editor_property("material_slot_name")): s.get_editor_property("material_interface")
               for s in old.get_editor_property("static_materials")}
    nanite = old.get_editor_property("nanite_settings").enabled
    ctf = old.get_editor_property("body_setup").get_editor_property("collision_trace_flag")
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
    task.filename = str(FBX)
    task.destination_path = VILLAGE + "/Meshes"
    task.destination_name = "SM_Village_Kura"
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
    slots = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        mesh.set_material(index, kitlib.material_for(name, by_slot))
        slots.append(name)
    settings = mesh.get_editor_property("nanite_settings")
    settings.enabled = nanite
    mesh.set_editor_property("nanite_settings", settings)
    body = mesh.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", ctf)
    agg = body.get_editor_property("agg_geom")
    convexes = len(agg.get_editor_property("convex_elems"))
    added = 0
    if convexes < 3:
        boxes = list(agg.get_editor_property("box_elems"))
        for centre, half in DOOR_BOXES:
            box = ue.KBoxElem()
            box.set_editor_property("center", ue.Vector(*centre))
            box.set_editor_property("x", half[0] * 2.0)
            box.set_editor_property("y", half[1] * 2.0)
            box.set_editor_property("z", half[2] * 2.0)
            boxes.append(box)
            added += 1
        agg.set_editor_property("box_elems", boxes)
        body.set_editor_property("agg_geom", agg)
    kitlib.save(mesh)
    box = mesh.get_bounding_box()
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-190", "mesh": MESH, "slots": slots, "nanite": nanite, "convexes": convexes, "door_boxes_added": added,
              "size_cm": [round(box.max.x - box.min.x, 1), round(box.max.y - box.min.y, 1), round(box.max.z - box.min.z, 1)],
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "kura-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T190_KURA_SUCCESS")


if __name__ == "__main__":
    main()
