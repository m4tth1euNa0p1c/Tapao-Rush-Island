"""T-262 stealth (lot L5): DA_StealthRules and the crouch held on left Ctrl (owner request 2026-10-09).

- DA_StealthRules (UStealthRules, the class defaults) in /Game/MonkeIsland/Stealth/Data, loaded by path by
  UStealthSubsystem. Needs the editor built with the T-262 code (class StealthRules): without it this step is skipped
  with a clear message and the rest still runs; run the action again after the build.
- IA_Crouch (/Game/MonkeIsland/Player/Input) mapped on LeftControl in IMC_Islander_Actions (added when missing, its
  other keys kept). The crouch is held (Started: crouch, Completed: stand up; Triggered while held dives in the water),
  so no trigger that ends the action at once may sit on it (Pressed, Released, Tap, Pulse): such triggers are taken off
  the action and its mappings, and reported.
- Lists every other action on LeftControl in the input contexts of the project (conflicts to look at, nothing changed)
  and the crouch action of BP_IslanderBean.
Runs in the editor: scripts/editor/lead_session_t186.py, action "stealthsetup" (editor lock held, no game running).
Idempotent. Result: monke_island/Saved/T262/stealth-setup.json
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T262"
RULES_DIR = "/Game/MonkeIsland/Stealth/Data"
RULES = RULES_DIR + "/DA_StealthRules"
INPUT = "/Game/MonkeIsland/Player/Input"
CROUCH = INPUT + "/IA_Crouch"
IMC = INPUT + "/IMC_Islander_Actions"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
CTRL = "LeftControl"
# triggers that end the action on the frame it starts, or fire it only on the release: a held crouch cannot have them
INSTANT_TRIGGERS = ("InputTriggerPressed", "InputTriggerReleased", "InputTriggerTap", "InputTriggerPulse")


def key(name):
    k = ue.Key()
    k.set_editor_property("key_name", name)
    return k


def key_name(mapping):
    return str(mapping.get_editor_property("key").get_editor_property("key_name"))


def action_name(mapping):
    action = mapping.get_editor_property("action")
    return action.get_name() if action else None


def instant(triggers):
    return [t for t in triggers if t and t.get_class().get_name() in INSTANT_TRIGGERS]


def setup_rules(result):
    cls = getattr(ue, "StealthRules", None)
    if cls is None:
        result["rules"] = "class StealthRules missing: build the editor with T-262, then run the action again"
        ue.log_warning("[stealth] " + result["rules"])
        return
    rules = kitlib.load(RULES)
    created = rules is None
    if created:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        rules = ue.AssetToolsHelpers.get_asset_tools().create_asset("DA_StealthRules", RULES_DIR, cls, factory)
    assert rules, RULES
    kitlib.save(rules)
    sight = rules.get_editor_property("sight")
    hearing = rules.get_editor_property("hearing")
    result["rules"] = {
        "path": RULES, "created": created,
        "contact_gap_cm": sight.get_editor_property("contact_gap_cm"),
        "bush_contact_gap_cm": sight.get_editor_property("bush_contact_gap_cm"),
        "run_step_radius_cm": hearing.get_editor_property("run_step_radius_cm"),
        "voice_radius_cm": hearing.get_editor_property("voice_radius_cm"),
        "gunshot_radius_cm": hearing.get_editor_property("gunshot_radius_cm"),
        "impact_radius_cm": hearing.get_editor_property("impact_radius_cm"),
        "noise_memory_seconds": rules.get_editor_property("noise_memory_seconds"),
    }


def setup_crouch_key(result):
    crouch = kitlib.load(CROUCH)
    imc = kitlib.load(IMC)
    assert crouch, CROUCH
    assert imc, IMC
    report = {"action": CROUCH, "context": IMC}

    # the action: digital, without an instant trigger
    report["value_type"] = str(crouch.get_editor_property("value_type"))
    triggers = list(crouch.get_editor_property("triggers"))
    removed = [t.get_class().get_name() for t in instant(triggers)]
    if removed:
        crouch.set_editor_property("triggers", [t for t in triggers if t not in instant(triggers)])
        kitlib.save(crouch)
    report["action_triggers"] = [t.get_class().get_name() for t in triggers if t]
    report["action_triggers_removed"] = removed

    # its mappings in the context: LeftControl added when missing, instant triggers taken off
    data = imc.get_editor_property("default_key_mappings")
    mappings = list(data.get_editor_property("mappings"))
    keys = [key_name(m) for m in mappings if m.get_editor_property("action") == crouch]
    report["keys_before"] = keys
    changed = False
    mapping_removed = []
    for mapping in mappings:
        if mapping.get_editor_property("action") != crouch:
            continue
        mapped = list(mapping.get_editor_property("triggers"))
        if instant(mapped):
            mapping_removed += [f"{key_name(mapping)}:{t.get_class().get_name()}" for t in instant(mapped)]
            mapping.set_editor_property("triggers", [t for t in mapped if t not in instant(mapped)])
            changed = True
    if changed:
        data.set_editor_property("mappings", mappings)
        imc.set_editor_property("default_key_mappings", data)
    if CTRL not in keys:
        imc.map_key(crouch, key(CTRL))
        changed = True
    if changed:
        kitlib.save(imc)
    after = imc.get_editor_property("default_key_mappings").get_editor_property("mappings")
    report["keys_after"] = [key_name(m) for m in after if m.get_editor_property("action") == crouch]
    report["mapping_triggers_removed"] = mapping_removed
    report["context_saved"] = changed
    result["crouch"] = report


def list_ctrl_conflicts(result):
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    contexts = registry.get_assets_by_class(ue.TopLevelAssetPath("/Script/EnhancedInput", "InputMappingContext"))
    conflicts = []
    for data in contexts:
        path = str(data.package_name)
        if not path.startswith("/Game/"):
            continue
        imc = kitlib.load(path)
        if not imc:
            continue
        for mapping in imc.get_editor_property("default_key_mappings").get_editor_property("mappings"):
            if key_name(mapping) == CTRL and action_name(mapping) != "IA_Crouch":
                conflicts.append({"context": path, "action": action_name(mapping)})
    result["left_control_other_actions"] = conflicts


def check_player(result):
    player = kitlib.load(PLAYER_BP)
    if not player:
        result["player"] = f"{PLAYER_BP} missing"
        return
    try:
        cdo = ue.get_default_object(player.generated_class())
        action = cdo.get_editor_property("crouch_action")
        contexts = cdo.get_editor_property("character_mapping_contexts")
        result["player"] = {"crouch_action": action.get_path_name() if action else None,
                            "contexts": [c.get_path_name() for c in contexts if c]}
    except Exception as error:  # protected properties may not be readable from Python: reported, not fatal
        result["player"] = f"not readable from Python: {error}"


def main():
    kitlib.check_lock()
    result = {"task": "T-262"}
    setup_rules(result)
    setup_crouch_key(result)
    list_ctrl_conflicts(result)
    check_player(result)
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "stealth-setup.json").write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    ue.log("T262_STEALTH_SETUP_SUCCESS")


if __name__ == "__main__":
    main()
