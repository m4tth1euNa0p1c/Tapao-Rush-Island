"""T-188: imports the bare-deck hull (art/export/Galleon_T188, scripts/props/galleon_deck_clear.py) as
/Game/MonkeIsland/Galleon/Revisions/T188/Meshes/SM_Galleon and puts it on the test galleon of L_Test_Flat
(DA_GalleonRules_TestFlat and the Beach_Galleon actor). Same import as T184: legacy FBX importer, vertex colours, UCX
kept as simple collision with complex, sockets at their delivered coordinates, Nanite off (translucent glazing).
The game galleon (DA_GalleonRules) is promoted after the test, with the access checks of T-184.
Runs in the editor: scripts/editor/lead_session_t186.py, action "galleon".
"""
import datetime as dt
import hashlib
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import import_galleon_t149 as prior  # noqa: E402

STAGE = ROOT / "art/export/Galleon_T188"
DEST = "/Game/MonkeIsland/Galleon/Revisions/T188"
OUT = ROOT / "monke_island/Saved/T188"
GLASS = "/Game/MonkeIsland/Galleon/Revisions/T184/Materials/M_Galleon_CabinGlass"
RULES = "/Game/MonkeIsland/Galleon/TestFlat/DA_GalleonRules_TestFlat"
BACKUP = "/Game/MonkeIsland/Galleon/Backups/DA_GalleonRules_TestFlat_PreT188"


def main():
    prior.check_lock()
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"
    report = json.loads((STAGE / "export-result.json").read_text(encoding="utf-8"))
    hull = report["exports"]["SM_Galleon"]
    assert hashlib.sha256((STAGE / "SM_Galleon.fbx").read_bytes()).hexdigest() == hull["sha256"], "FBX changed"
    materials = {name: prior.load(prior.DEST + "/Materials/" + name) for name in prior.RECIPES}
    assert all(materials.values()), "TestFlat galleon materials missing"
    materials["M_Galleon_Glass"] = prior.load(GLASS)
    assert materials["M_Galleon_Glass"], GLASS
    old = (prior.DEST, prior.MESH_SRC, prior.VARIANT, prior.RESULT)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        prior.DEST, prior.MESH_SRC, prior.VARIANT, prior.RESULT = DEST, STAGE, {"ucx": report["ucx"]}, {"meshes": {}}
        mesh = prior.import_mesh("SM_Galleon", materials, scale=1.0, retry=True)
        info = prior.RESULT["meshes"]["SM_Galleon"]
    finally:
        prior.DEST, prior.MESH_SRC, prior.VARIANT, prior.RESULT = old
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
    nanite = mesh.get_editor_property("nanite_settings")
    nanite.enabled = False
    mesh.set_editor_property("nanite_settings", nanite)
    prior.save(mesh)

    rules = prior.load(RULES)
    assert rules, RULES
    if not prior.load(BACKUP):
        prior.save(prior.EAL.duplicate_asset(RULES, BACKUP))
    rules.set_editor_property("hull_mesh", mesh)
    prior.save(rules)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    ship = next((a for a in actors if a.get_actor_label() == "Beach_Galleon"), None)
    if ship:
        # a reimport in place (same asset) leaves the level as it is: it is saved only when the galleon changes
        component = ship.get_editor_property("hull")
        if (ship.get_editor_property("rules") != rules or component.get_editor_property("static_mesh") != mesh
                or component.get_editor_property("override_materials")):
            ship.set_editor_property("rules", rules)
            component.set_static_mesh(mesh)
            component.set_editor_property("override_materials", [])
            assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-188", "mesh": info, "rules": RULES, "backup": BACKUP, "beach_galleon": bool(ship),
              "forecastle_door": report.get("forecastle_door"),
              "removed_objects": report["removed_objects"], "cargo": report["cargo"],
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "import-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T188_IMPORT_SUCCESS")


if __name__ == "__main__":
    main()
