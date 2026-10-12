"""Owner request 2026-10-07: once the flag is up, "La Couronne britannique revendique ces terres" and +1000 crowns.

The C++ defaults of UFlagCeremonyRules carry the message, the reward line and ClaimReward 1000; this sets them on
DA_FlagCeremonyRules too, in case the asset kept an older DoneText.
Runs in the editor: scripts/editor/lead_session_t186.py, action "flagclaim".
"""
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

RULES = "/Game/MonkeIsland/Environment/FlagCeremony/DA_FlagCeremonyRules"


def main():
    kitlib.check_lock()
    rules = kitlib.load(RULES)
    assert rules, RULES
    rules.set_editor_property("done_text", "La Couronne britannique revendique ces terres")
    rules.set_editor_property("reward_text", "+{0} $")
    rules.set_editor_property("claim_reward", 1000)
    kitlib.save(rules)
    ue.log("FLAG_CLAIM_SET")


if __name__ == "__main__":
    main()
