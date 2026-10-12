"""Fixes of 2026-10-06 evening (owner: "de travers", "je vois à travers les murs"), idempotent, saved explicitly.

- L_Test_Flat: actors placed by Codex's showcase scripts with unreal.Rotator(0, yaw, 0), whose arguments are
  (roll, pitch, yaw): their yaw went into the pitch (pyramid -90, houses upside down). The yaw is read back from the
  forward vector of the wrong rotation and the pitch cleared (T-179 / T-183).
- BP_BeanMerchant: its stand had Pitch -90 for the same reason (T-181).
- Galleon materials of TestFlat: two-sided, the hull and decks being single shells seen from the hold (T-184 / T-188).
Runs in the editor: scripts/editor/lead_session_t186.py, action "fixes".
"""
import datetime as dt
import json
import math
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
TAGS = ("OneDriveStructures_T179", "OneDriveShowcase_T179")
MERCHANT = "/Game/MonkeIsland/Galleon/Shop/Merchant/BP_BeanMerchant"
MATERIALS = "/Game/MonkeIsland/Galleon/TestFlat/Materials"
EAL = ue.EditorAssetLibrary


def straighten(actor):
    r = actor.get_actor_rotation()
    if abs(r.pitch) < 0.5 and abs(r.roll) < 0.5:
        return False
    pitch, yaw = math.radians(r.pitch), math.radians(r.yaw)
    # the wrong rotation was a pure pitch by the wanted yaw: its forward vector is (cos a, 0, sin a)
    wanted = math.degrees(math.atan2(math.sin(pitch), math.cos(pitch) * math.cos(yaw)))
    actor.set_actor_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=wanted), False)
    return True


def sync_session_actions():
    """The session started before an action was added keeps its old table: update it from its file (gc finds the
    namespace of the running session, the one with ACTIONS, REQUEST and poll)"""
    import ast
    import gc
    source = (ROOT / "scripts/editor/lead_session_t186.py").read_text(encoding="utf-8")
    table = {}
    for node in ast.parse(source).body:
        if isinstance(node, ast.Assign) and any(getattr(t, "id", "") == "ACTIONS" for t in node.targets):
            table = ast.literal_eval(node.value)
    for obj in gc.get_objects():
        if isinstance(obj, dict) and "ACTIONS" in obj and "REQUEST" in obj and "poll" in obj:
            obj["ACTIONS"].update(table)
    return sorted(table)


def main():
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"
    synced = sync_session_actions()
    result = {"actors": 0, "merchant": False, "materials": [], "session_actions": synced}
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world or "L_Test_Flat" not in world.get_path_name():
        assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(LEVEL)
    for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
        if any(str(t) in TAGS for t in actor.tags) and straighten(actor):
            result["actors"] += 1
    if result["actors"]:
        assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()

    stand = ue.find_object(None, MERCHANT + ".BP_BeanMerchant_C:MerchantStand_GEN_VARIABLE")
    if stand:
        r = stand.get_editor_property("relative_rotation")
        if abs(r.pitch) > 0.5 or abs(r.yaw + 90.0) > 0.5:
            stand.set_editor_property("relative_rotation", ue.Rotator(roll=0.0, pitch=0.0, yaw=-90.0))
            result["merchant"] = True
        assert EAL.save_asset(MERCHANT, only_if_is_dirty=False)

    for path in EAL.list_assets(MATERIALS, recursive=False):
        material = EAL.load_asset(path)
        if not isinstance(material, ue.Material) or "Glass" in material.get_name():
            continue
        if not material.get_editor_property("two_sided"):
            material.set_editor_property("two_sided", True)
            ue.MaterialEditingLibrary.recompile_material(material)
            result["materials"].append(material.get_name())
        assert EAL.save_loaded_asset(material, only_if_is_dirty=False), path
    OUT.mkdir(parents=True, exist_ok=True)
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT / "fixes-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T186_FIXES_SUCCESS " + json.dumps({k: (len(v) if isinstance(v, list) else v) for k, v in result.items()}))


if __name__ == "__main__":
    main()
