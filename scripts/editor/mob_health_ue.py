"""T-338 (owner 2026-10-11: « mets plus de point de vie au mob »): the Veilleurs (tiki warriors of every role) get
half again their health: DA_TikiRules.WarriorHealth 80 -> 120 (the roles keep their HealthScale, the day curve its
multiplier). The villagers are not mobs and keep theirs. Lead session action "mobhealth" (editor open, no game
running). Idempotent: sets the target value, never multiplies twice. Report: monke_island/Saved/T338/mob-health.json.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T338/mob-health.json"
RULES = "/Game/MonkeIsland/Characters/Data/DA_TikiRules"
TARGET = {"warrior_health": 120.0}


def main():
    rules = ue.EditorAssetLibrary.load_asset(RULES)
    if rules is None:
        raise RuntimeError(f"{RULES} not found")
    report = {"rules": RULES, "before": {}, "after": {}}
    changed = False
    for prop, value in TARGET.items():
        before = float(rules.get_editor_property(prop))
        report["before"][prop] = before
        if abs(before - value) > 0.01:
            rules.set_editor_property(prop, value)
            changed = True
        report["after"][prop] = float(rules.get_editor_property(prop))
    if changed and not ue.EditorAssetLibrary.save_loaded_asset(rules, only_if_is_dirty=False):
        raise RuntimeError(f"{RULES} not saved")
    report["saved"] = changed
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"MOB_HEALTH {report['before']} -> {report['after']}")


main()
