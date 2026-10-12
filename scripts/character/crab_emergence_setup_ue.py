"""T-214: the giant crabs that come out of the sand during the campaign (owner requests 2026-10-07: from day 4, at 5:30 of
the day, 50 m from the player, on the sand).

- DA_GiantCrabRules_Sand: a copy of DA_GiantCrabRules whose leash reaches the player it rose near (90 m);
- DA_GiantCrabEmergenceRules (UGiantCrabEmergenceRules): BP_GiantCrab with those rules, one crab at 330 s.
Runs in the editor: scripts/editor/lead_session_t186.py, action "crabsand".
"""
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

BASE = "/Game/MonkeIsland/Characters/GiantCrab"
SOURCE = BASE + "/DA_GiantCrabRules"
SAND = BASE + "/DA_GiantCrabRules_Sand"
RULES = BASE + "/DA_GiantCrabEmergenceRules"
LEASH_CM = 9000.0


def main():
    kitlib.check_lock()
    sand = kitlib.load(SAND) or ue.EditorAssetLibrary.duplicate_asset(SOURCE, SAND)
    assert sand, SAND
    sand.set_editor_property("leash_radius_cm", LEASH_CM)
    kitlib.save(sand)
    rules = kitlib.load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.GiantCrabEmergenceRules)
        rules = ue.AssetToolsHelpers.get_asset_tools().create_asset("DA_GiantCrabEmergenceRules", BASE, ue.GiantCrabEmergenceRules, factory)
    assert rules, RULES
    crab = ue.EditorAssetLibrary.load_blueprint_class(BASE + "/BP_GiantCrab")
    assert crab, "BP_GiantCrab missing"
    rules.set_editor_property("crab_class", crab)
    rules.set_editor_property("crab_rules", sand)
    rules.set_editor_property("emerge_seconds", [330.0])
    kitlib.save(rules)
    ue.log("T214_CRAB_SAND_SETUP_SUCCESS")


if __name__ == "__main__":
    main()
