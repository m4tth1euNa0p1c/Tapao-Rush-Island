"""Promote the fully textured T-149 galleon meshes to the production rules asset.

Run only after T-149 imports and validates in TestFlat, with the editor lock held.
A timestamped duplicate preserves the previous production rules for rollback.
"""
import datetime as dt
import json
import os
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
# Lock holders allowed to run this T-149 step: codex, or the session named in MONKE_LOCK_OWNER
LOCK_OWNERS = {"codex", os.environ.get("MONKE_LOCK_OWNER") or "codex"}
SOURCE = ROOT / "art/export/Galleon_T149/20261006_1203"
TEST_RULES = "/Game/MonkeIsland/Galleon/TestFlat/DA_GalleonRules_TestFlat"
MAIN_RULES = "/Game/MonkeIsland/Galleon/DA_GalleonRules"
DEST = "/Game/MonkeIsland/Galleon/TestFlat"
EAL = ue.EditorAssetLibrary


def main():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    assert lock["owner"] in LOCK_OWNERS, "Editor lock not owned by codex or MONKE_LOCK_OWNER"
    assert dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone()

    proof_dir = ROOT / "monke_island/Saved/T149"
    imported = json.loads((proof_dir / "import-result.json").read_text(encoding="utf-8"))
    traversal = json.loads((proof_dir / "traversal-result.json").read_text(encoding="utf-8"))
    visual = json.loads((proof_dir / "visual-result.json").read_text(encoding="utf-8"))
    assert traversal["passed"] and len(traversal["routes"]) == 6, "Six stair routes must pass in PIE"
    assert traversal["import_finished_at"] == imported["finished_at"], "Stale traversal proof"
    assert visual["passed"] and visual["import_finished_at"] == imported["finished_at"], "Fresh visual proof required"

    report = json.loads((SOURCE / "galleon-report.json").read_text(encoding="utf-8"))
    test = EAL.load_asset(TEST_RULES)
    production = EAL.load_asset(MAIN_RULES)
    assert test and production, "Both test and production rules assets are required"

    mesh_properties = {
        "hull_mesh": "SM_Galleon",
        "sails_mesh": "SM_Galleon_Sails",
        "rigging_mesh": "SM_Galleon_Rigging",
        "flag_mesh": "SM_Galleon_Flag",
        "wheel_mesh": "SM_Galleon_Wheel",
        "rudder_mesh": "SM_Galleon_Rudder",
    }
    verified = {}
    for prop, name in mesh_properties.items():
        mesh = EAL.load_asset(f"{DEST}/Meshes/{name}")
        assert mesh, f"Missing T-149 mesh: {name}"
        slots = mesh.get_editor_property("static_materials")
        expected = report["exports"][name]["materials"]
        assert len(slots) == expected, f"{name}: expected {expected} material slots, got {len(slots)}"
        for slot in slots:
            material = slot.get_editor_property("material_interface")
            assert material and material.get_path_name().startswith(f"{DEST}/Materials/"), (
                f"{name} contains an untextured or non-T-149 material slot"
            )
        verified[name] = {"path": mesh.get_path_name(), "material_slots": len(slots)}

    backup_path = ("/Game/MonkeIsland/Galleon/Backups/DA_GalleonRules_PreT149_"
                   + dt.datetime.now().strftime("%Y%m%d_%H%M%S"))
    backup = EAL.duplicate_asset(MAIN_RULES, backup_path)
    assert backup and EAL.save_loaded_asset(backup, only_if_is_dirty=False), "Production backup failed"
    for prop in mesh_properties:
        production.set_editor_property(prop, test.get_editor_property(prop))
    for prop in ("flag_pivot", "wheel_pivot", "rudder_pivot", "ladder_deck_point"):
        production.set_editor_property(prop, test.get_editor_property(prop))
    assert EAL.save_loaded_asset(production, only_if_is_dirty=False), MAIN_RULES

    result = {
        "task": "T-149",
        "production_rules": MAIN_RULES,
        "previous_rules_backup": backup_path,
        "meshes": verified,
        "finished_at": dt.datetime.now(dt.timezone.utc).isoformat(),
        "validation": "Promoted after fresh PIE traversal, visual and material-slot checks; build/network validation separate",
    }
    output = ROOT / "monke_island/Saved/T149/promotion-result.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T149_GALLEON_PROMOTION_SUCCESS " + str(output))


if __name__ == "__main__":
    main()
