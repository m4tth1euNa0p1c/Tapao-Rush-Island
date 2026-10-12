"""Refresh only the existing beach galleon after the corrected T-149 import.

Run with Unreal Python, outside PIE and with the codex editor lock. Preserve the
current ship position, water, boat, platform, and all other sessions' actors.
"""
import datetime as dt
import json
from pathlib import Path
import sys
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
from import_galleon_t149 import check_lock, load, DEST


def main():
    check_lock()
    level = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    # Reloading the open (possibly dirty) map could raise a modal save prompt: load only when another map is open
    current = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not current or "L_Test_Flat" not in current.get_path_name():
        assert level.load_level("/Game/MonkeIsland/Maps/Test/L_Test_Flat")
    all_actors = actors.get_all_level_actors()
    ships = [a for a in all_actors if a.get_actor_label() == "Beach_Galleon"]
    assert len(ships) == 1 and isinstance(ships[0], ue.GalleonActor), "Existing beach galleon not found"
    ship = ships[0]
    location = ship.get_actor_location()
    rules = load(f"{DEST}/DA_GalleonRules_TestFlat")
    assert rules
    ship.set_editor_property("rules", rules)
    components = {c.get_name(): c for c in ship.get_components_by_class(ue.StaticMeshComponent)}
    for name, prop in (("Hull", "hull_mesh"), ("Sails", "sails_mesh"), ("Rigging", "rigging_mesh"),
                       ("Flag", "flag_mesh"), ("Wheel", "wheel_mesh"), ("Rudder", "rudder_mesh")):
        mesh = rules.get_editor_property(prop)
        assert mesh
        component = components[name]
        component.set_static_mesh(mesh)
        component.set_editor_property("override_materials", [])
        component.set_mobility(ue.ComponentMobility.MOVABLE)
        if name == "Hull":
            component.set_collision_profile_name("BlockAll")
            component.set_collision_response_to_channel(ue.CollisionChannel.ECC_CAMERA, ue.CollisionResponseType.ECR_IGNORE)
        else:
            component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    for name, prop in (("Flag", "flag_pivot"), ("Wheel", "wheel_pivot"), ("Rudder", "rudder_pivot")):
        components[name].set_relative_location(rules.get_editor_property(prop), False, False)
    components["Rudder"].set_relative_rotation(ue.Rotator(0, 0, 10.49), False, False)
    assert len(actors.get_all_level_actors()) == len(all_actors), "Actor count changed"
    assert level.save_current_level()
    proof = {"task": "T-149", "ship": ship.get_path_name(), "actor_count": len(all_actors),
             "ship_location_cm": [location.x, location.y, location.z],
             "saved_at": dt.datetime.now(dt.timezone.utc).isoformat(),
             "validation": "Existing actor refreshed and map saved; PIE validation still required"}
    output = ROOT / "monke_island/Saved/T149/refresh-result.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(proof, indent=2), encoding="utf-8")
    ue.log("T149_REFRESH_SUCCESS " + str(output))


if __name__ == "__main__":
    main()
