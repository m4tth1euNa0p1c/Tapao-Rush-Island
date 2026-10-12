"""T-135 death and spectator (lots L3 / L5): DA_DeathRules and a check of the player controller.

- DA_DeathRules (UDeathRules, the class defaults: death screen 4 s, follow camera, short causes of death) in
  /Game/MonkeIsland/UI/Tapao (cooked with the TAPAO screens), read by path by UDeathSpectatorComponent
  (RulesAsset). Without the asset the game uses the same class defaults: the asset only makes them tunable.
- Reports the component DeathSpectator of BP_FirstPersonPlayerController (the controller of the island game modes, a
  child of Amonke_islandPlayerController) and the rules path of the component: nothing is changed there.
Needs the editor built with the T-135 code (classes DeathRules, DeathSpectatorComponent): without it the action fails
with a clear message; build, then run it again.
Runs in the editor: scripts/editor/lead_session_t186.py, action "deathux" (editor lock held, no game running).
Idempotent. Result: monke_island/Saved/T135/death-ux-setup.json
"""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T135"
RULES_DIR = "/Game/MonkeIsland/UI/Tapao"
RULES = RULES_DIR + "/DA_DeathRules"
CONTROLLER_BP = "/Game/FirstPerson/Blueprints/BP_FirstPersonPlayerController.BP_FirstPersonPlayerController_C"
EAL = ue.EditorAssetLibrary


def text(value):
    return str(value) if value is not None else None


def setup_rules(result):
    cls = getattr(ue, "DeathRules", None)
    if cls is None:
        raise RuntimeError("class DeathRules missing: build the editor with T-135, then run the action deathux again")
    rules = EAL.load_asset(RULES) if EAL.does_asset_exist(RULES) else None
    created = rules is None
    if created:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        rules = ue.AssetToolsHelpers.get_asset_tools().create_asset("DA_DeathRules", RULES_DIR, cls, factory)
    assert rules, RULES
    assert EAL.save_loaded_asset(rules, only_if_is_dirty=False), RULES
    sources = rules.get_editor_property("source_texts")
    result["rules"] = {
        "path": RULES,
        "created": created,
        "death_screen_seconds": rules.get_editor_property("death_screen_seconds"),
        "skip_death_screen_after": rules.get_editor_property("skip_death_screen_after"),
        "followed_death_seconds": rules.get_editor_property("followed_death_seconds"),
        "min_cycle_interval": rules.get_editor_property("min_cycle_interval"),
        "follow_distance": rules.get_editor_property("follow_distance"),
        "follow_pitch": rules.get_editor_property("follow_pitch"),
        "cause_texts": len(rules.get_editor_property("cause_texts")),
        "source_texts": [
            {"source": text(s.get_editor_property("source")), "text": text(s.get_editor_property("text"))} for s in sources
        ],
    }


def check_controller(result):
    component_cls = getattr(ue, "DeathSpectatorComponent", None)
    if component_cls is None:
        raise RuntimeError("class DeathSpectatorComponent missing: build the editor with T-135, then run the action deathux again")
    component_cdo = ue.get_default_object(component_cls)
    check = {"rules_asset": text(component_cdo.get_editor_property("rules_asset"))}
    controller_cls = ue.load_class(None, CONTROLLER_BP)
    if controller_cls is None:
        check["controller"] = "BP_FirstPersonPlayerController not found"
    else:
        controller = ue.get_default_object(controller_cls)
        spectator = controller.get_editor_property("death_spectator")
        check["controller"] = CONTROLLER_BP
        check["death_spectator"] = spectator.get_name() if spectator else None
    result["controller"] = check
    if not check.get("death_spectator"):
        ue.log_warning("[deathux] BP_FirstPersonPlayerController has no DeathSpectator component: is it a child of "
                       "Amonke_islandPlayerController in this build?")


def main():
    result = {"task": "T-135", "started_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    try:
        setup_rules(result)
        check_controller(result)
    finally:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "death-ux-setup.json").write_text(json.dumps(result, indent=2, ensure_ascii=False), encoding="utf-8")
    ue.log("[deathux] " + json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
