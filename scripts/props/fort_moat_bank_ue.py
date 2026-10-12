"""T-210, Unreal side: imports SM_Fort_MoatBank (scripts/props/fort_moat_bank.py), gives it the site's ground material and
a complex-as-simple collision, and puts one actor at the fort's origin in L_Test_Flat (folder FortFrench/Site).
Editor open on L_Test_Flat, no game running: scripts/editor/lead_session_t186.py, action "moatbank". Replays replace the
mesh and keep a single actor. Then the action "ditchtraces" shows the ground across the moat (Saved/T210/traces.json).
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
FBX = ROOT / "art/export/Fort_T198/exports/SM_Fort_MoatBank.fbx"
FOLDER = "/Game/MonkeIsland/Environment/FortFrench/Revisions/T210/Meshes"
NAME = "SM_Fort_MoatBank"
GROUND = "/Game/MonkeIsland/Environment/FortFrench/Materials/M_Site_Ground"
ORIGIN = ue.Vector(-66000.0, 0.0, 115.0)
LABEL = "Fort_T210_MoatBank"
EAL = ue.EditorAssetLibrary


def main():
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"
    options = ue.FbxImportUI()
    for prop, value in {"import_mesh": True, "import_as_skeletal": False, "import_materials": False, "import_textures": False,
                        "import_animations": False, "mesh_type_to_import": ue.FBXImportType.FBXIT_STATIC_MESH}.items():
        options.set_editor_property(prop, value)
    for prop, value in {"combine_meshes": True, "auto_generate_collision": False, "import_uniform_scale": 1.0,
                        "convert_scene": True, "convert_scene_unit": True, "generate_lightmap_u_vs": False}.items():
        options.static_mesh_import_data.set_editor_property(prop, value)
    task = ue.AssetImportTask()
    task.filename = str(FBX)
    task.destination_path = FOLDER
    task.destination_name = NAME
    task.automated = True
    task.save = True
    task.replace_existing = True
    task.options = options
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    mesh = EAL.load_asset(f"{FOLDER}/{NAME}")
    assert mesh, "import failed"
    mesh.set_material(0, EAL.load_asset(GROUND))
    # the walking surface is the mesh itself (a closed ring, 1 920 faces)
    mesh.get_editor_property("body_setup").set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    EAL.save_loaded_asset(mesh)

    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for old in [a for a in actors.get_all_level_actors() if a.get_actor_label() == LABEL]:
        actors.destroy_actor(old)
    actor = actors.spawn_actor_from_object(mesh, ORIGIN, ue.Rotator(0.0, 0.0, 0.0))
    actor.set_actor_label(LABEL)
    actor.set_folder_path("FortFrench/Site")
    comp = actor.get_component_by_class(ue.StaticMeshComponent)
    comp.set_collision_profile_name("BlockAll")
    assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()
    extent = mesh.get_bounds().box_extent
    report = {"mesh": mesh.get_path_name(), "actor": LABEL, "extent_cm": [round(extent.x), round(extent.y), round(extent.z)],
              "material": GROUND, "collision": "complex as simple"}
    out = ROOT / "monke_island/Saved/T210"
    out.mkdir(parents=True, exist_ok=True)
    (out / "moat-bank.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"[T210] {json.dumps(report)}")


if __name__ == "__main__":
    main()
