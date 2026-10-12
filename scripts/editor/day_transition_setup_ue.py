"""T-214: DA_DayTransitionRules (UDayTransitionRules, the class defaults) for the departure and arrival scenes of a day.
Runs in the editor: scripts/editor/lead_session_t186.py, action "daytransition".
"""
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

PATH = "/Game/MonkeIsland/Expedition/Data/DA_DayTransitionRules"


def main():
    kitlib.check_lock()
    rules = kitlib.load(PATH)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.DayTransitionRules)
        rules = ue.AssetToolsHelpers.get_asset_tools().create_asset("DA_DayTransitionRules", "/Game/MonkeIsland/Expedition/Data",
                                                                    ue.DayTransitionRules, factory)
    assert rules, PATH
    kitlib.save(rules)
    ue.log("T214_DAY_TRANSITION_SETUP_SUCCESS")


if __name__ == "__main__":
    main()
