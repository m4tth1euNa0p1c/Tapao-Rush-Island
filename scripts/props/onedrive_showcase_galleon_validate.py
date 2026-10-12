"""Verify saved Bean collision, boarding, cabin entry/exit and six stairways."""
import datetime as dt
import gc
import json
import math
from pathlib import Path
import traceback
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T184"
# Retire a prior validator if a user ended PIE before it could finish.
for callback in gc.get_objects():
    if (isinstance(callback, type(lambda: None)) and callback.__name__ == "tick"
            and Path(callback.__code__.co_filename) == Path(__file__)):
        old = callback.__globals__
        if old.get("STATE", {}).get("phase") != "finished" and old.get("HANDLE"):
            ue.unregister_slate_post_tick_callback(old["HANDLE"])
            old["STATE"]["phase"] = "finished"
STATE = {"phase": "wait", "routes": []}
HANDLE = None

def xyz(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]

def finish(passed, error=None):
    if STATE.get("controller"):
        try:
            STATE["controller"].set_ignore_move_input(False)
        except Exception:
            pass
    if STATE.get("move"):
        try:
            STATE["move"].stop_movement_immediately()
        except Exception:
            pass  # PIE may have been ended by the user.
    result = {k: v for k, v in STATE.items() if k in ("routes", "runtime_before", "boarding", "sails_no_collision", "boat_count", "hull_mesh", "sway_samples")}
    result.update(passed=passed, error=error, finished_at=dt.datetime.now(dt.timezone.utc).isoformat())
    (OUT / "access-traversal.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.unregister_slate_post_tick_callback(HANDLE)
    STATE["phase"] = "finished"

def next_route(now):
    index = len(STATE["routes"])
    if index == len(STATE["plan"]):
        heights = [p[1] for p in STATE["sway_samples"]]
        assert max(heights)-min(heights) > .5, "No hull heave observed"
        finish(True)
    else:
        begin_route(now, *STATE["plan"][index])

def begin_route(now, name, start, end):
    STATE["move"].stop_movement_immediately()
    hull = STATE["hull"]
    pos = hull.get_world_transform().transform_location(ue.Vector(*start))
    half = STATE["capsule"].get_scaled_capsule_half_height()
    STATE["pawn"].set_actor_location(pos + ue.Vector(0, 0, half+4), False, True)
    # A test teleport must reacquire its floor instead of retaining the prior stair base.
    STATE["move"].set_movement_mode(ue.MovementMode.MOVE_FALLING)
    STATE["pawn"].consume_movement_input_vector()
    STATE.update(phase="settle", started=now, name=name, target=end, samples=[])

def runtime_info(pawn):
    return [{"name": c.get_name(), "class": c.get_class().get_name(),
             "relative": xyz(c.get_editor_property("relative_location")),
             "world": xyz(c.get_world_location()),
             "parent": c.get_attach_parent().get_name() if c.get_attach_parent() else None}
            for c in pawn.get_components_by_class(ue.SceneComponent)]

def tick(delta):
    try:
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
        if not world or "UEDPIE" not in world.get_path_name():
            if STATE["phase"] != "wait":
                finish(False, "PIE ended before validation finished")
            return
        now = ue.GameplayStatics.get_time_seconds(world)
        if STATE["phase"] == "wait":
            pawn = ue.GameplayStatics.get_player_character(world, 0)
            if not pawn or now < 2:
                return
            boat = next(a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)
                        if a.get_actor_label() == "Beach_Galleon")
            move = pawn.get_component_by_class(ue.CharacterMovementComponent)
            capsule = pawn.get_component_by_class(ue.CapsuleComponent)
            controller = ue.GameplayStatics.get_player_controller(world, 0)
            controller.set_ignore_move_input(True)
            STATE.update(pawn=pawn, move=move, capsule=capsule, hull=boat.get_editor_property("hull"),
                         controller=controller,
                         runtime_before={"capsule_half_height": capsule.get_unscaled_capsule_half_height(),
                                         "movement": str(move.get_editor_property("movement_mode")),
                                         "components": runtime_info(pawn)})
            (OUT / "runtime-start.json").write_text(json.dumps(STATE["runtime_before"], indent=2), encoding="utf-8")
            assert move.get_editor_property("movement_mode") != ue.MovementMode.MOVE_NONE, "Pawn movement disabled before test (T182)"
            assert capsule.get_unscaled_capsule_half_height() == 55, "Saved Bean capsule not applied"
            assert pawn.get_editor_property("mesh").get_editor_property("relative_location").z == -55
            hull = STATE["hull"]
            # Stop in the free landing before the main capstan's projecting bars.
            plan = [("cabin_entry", (0, -305, 160), (0, -570, 160)),
                    ("cabin_exit", (0, -570, 160), (0, -305, 160))]
            for level in ("fore", "quarter", "poop"):
                for side in ("P", "S"):
                    low = hull.get_editor_property("static_mesh").find_socket(f"ladder_bottom_{level}_{side}").relative_location
                    high = hull.get_editor_property("static_mesh").find_socket(f"ladder_top_{level}_{side}").relative_location
                    d = high-low
                    length = math.hypot(d.x, d.y)
                    direction = ue.Vector(d.x/length, d.y/length, 0)
                    plan.append((level+"_"+side, xyz(low-direction*45), xyz(high+direction*60)))
            plan.extend([("hold_descent",(0,580,164),(0,225,20)),
                         ("hold_ascent",(0,225,20),(0,580,164))])
            components = {c.get_name(): c for c in boat.get_components_by_class(ue.StaticMeshComponent)}
            STATE.update(plan=plan, sway_samples=[], hull_mesh=hull.static_mesh.get_path_name(),
                         sails_no_collision=all(components[n].get_collision_enabled() == ue.CollisionEnabled.NO_COLLISION
                                                for n in ("Sails", "Flag", "Rigging")),
                         boat_count=len(ue.GameplayStatics.get_all_actors_of_class(world, ue.BoatActor)))
            assert STATE["sails_no_collision"] and STATE["boat_count"] > 0
            foot = hull.get_socket_location("boarding_ladder")
            start = foot+ue.Vector(-70, 0, 55)
            pawn.set_actor_location(start, False, True)
            move.stop_movement_immediately()
            assert boat.can_interact(pawn), "Boarding ladder not interactable"
            boat.interact(pawn)
            STATE.update(phase="boarding", started=now, boarding={"start": xyz(start)})
            return
        STATE["sway_samples"].append([round(now, 3), round(STATE["hull"].get_editor_property("relative_location").z, 3)])
        if STATE["phase"] == "boarding":
            if now-STATE["started"] < 1:
                return
            pos = STATE["pawn"].get_actor_location()
            top = STATE["hull"].get_socket_location("ladder_top")
            passed = math.hypot(pos.x-top.x, pos.y-top.y) < 110 and abs(pos.z-top.z-55) < 45
            STATE["boarding"].update(passed=passed, end=xyz(pos), deck=xyz(top))
            assert passed, "Boarding failed"
            next_route(now)
            return
        if STATE["phase"] == "settle":
            if now - STATE["started"] < .75:
                return
            STATE.update(phase="walk", started=now)
        pos = STATE["pawn"].get_actor_location()
        half = STATE["capsule"].get_scaled_capsule_half_height()
        hull_t = STATE["hull"].get_world_transform()
        target = hull_t.transform_location(ue.Vector(*STATE["target"])) + ue.Vector(0, 0, half+2)
        delta_pos = target-pos
        distance = math.hypot(delta_pos.x, delta_pos.y)
        local = hull_t.inverse_transform_location(pos)
        if len(STATE["samples"]) == 0 or now - STATE.get("last_sample", 0) > .15:
            STATE["samples"].append(xyz(local))
            STATE["last_sample"] = now
        passed = distance < 22 and abs(delta_pos.z) < 25
        if passed or now-STATE["started"] > 9:
            STATE["routes"].append({"name": STATE["name"], "passed": passed,
                                    "half_height": half, "end_local": xyz(local), "samples": STATE["samples"]})
            assert passed, "Blocked route: " + STATE["name"]
            next_route(now)
        elif distance > .01:
            STATE["pawn"].add_movement_input(ue.Vector(delta_pos.x/distance, delta_pos.y/distance, 0), 1, True)
    except Exception:
        finish(False, traceback.format_exc())

OUT.mkdir(parents=True, exist_ok=True)
HANDLE = ue.register_slate_post_tick_callback(tick)
