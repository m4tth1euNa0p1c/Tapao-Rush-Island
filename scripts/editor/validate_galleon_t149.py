"""PIE traversal check driven by real CharacterMovement input.

Load via Unreal -ExecutePythonScript, then arm with a fresh
Saved/T149/pie-validation-request.json while the codex lock is held and start
our own PIE via MCP. This callback never starts or stops someone else's PIE.
"""
import datetime as dt
import json
import os
import math
from pathlib import Path
import time
import traceback
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
# Lock holders allowed to run this T-149 step: codex, or the session named in MONKE_LOCK_OWNER
LOCK_OWNERS = {"codex", os.environ.get("MONKE_LOCK_OWNER") or "codex"}
OUT = ROOT / "monke_island/Saved/T149"
REQUEST = OUT / "pie-validation-request.json"
BOOT = time.time()
STATE = {"phase": "waiting", "routes": [], "samples": [], "route_index": 0}
ROUTES = [(level, side) for level in ("fore", "quarter", "poop") for side in ("P", "S")]
HANDLE = None


def xyz(v):
    return [round(v.x, 3), round(v.y, 3), round(v.z, 3)]


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2), encoding="utf-8")


def finish(success, error=None):
    if STATE.get("pawn"):
        STATE["move"].stop_movement_immediately()
    result = {"task": "T-149", "passed": success, "error": error,
              "routes": STATE["routes"], "boarding": STATE.get("boarding"),
              "sway_samples": STATE["samples"], "sails_no_collision": STATE.get("sails_no_collision"),
              "boat_count": STATE.get("boat_count"),
              "import_finished_at": STATE.get("import_finished_at"),
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat(),
              "validation": "PIE; actual CharacterMovement input on six stair ramps"}
    write("traversal-result.json", result)
    ue.log("T149_TRAVERSAL_" + ("SUCCESS" if success else "FAILED") + " " + str(error or ""))
    ue.unregister_slate_post_tick_callback(HANDLE)
    STATE["phase"] = "finished"


def start_route(now):
    index = STATE["route_index"]
    if index == len(ROUTES):
        assert STATE.get("boarding", {}).get("passed"), "Boarding did not pass"
        assert STATE["sails_no_collision"], "Sails/rigging collision enabled"
        assert STATE["boat_count"] > 0, "No boat in the test world"
        heights = [s[1] for s in STATE["samples"]]
        assert max(heights) - min(heights) > 0.5, "No hull heave observed"
        finish(True)
        return
    level, side = ROUTES[index]
    hull = STATE["hull"]
    low = hull.get_socket_location(f"ladder_bottom_{level}_{side}")
    high = hull.get_socket_location(f"ladder_top_{level}_{side}")
    dx, dy = high.x-low.x, high.y-low.y
    distance = math.hypot(dx, dy)
    direction = ue.Vector(dx/distance, dy/distance, 0)
    start = low - direction * 45 + ue.Vector(0, 0, STATE["half_height"] + 8)
    STATE["move"].stop_movement_immediately()
    STATE["pawn"].set_actor_location(start, False, True)
    STATE["move"].set_movement_mode(ue.MovementMode.MOVE_WALKING)
    STATE.update(phase="settle", phase_start=now, route_name=f"{level}_{side}",
                 top_socket=f"ladder_top_{level}_{side}", direction=direction,
                 route_start=xyz(start), route_samples=[])


def advance(now):
    pawn, hull = STATE["pawn"], STATE["hull"]
    if STATE["phase"] == "boarding":
        if now - STATE["phase_start"] < 1.0:
            return
        top = hull.get_socket_location("ladder_top")
        position = pawn.get_actor_location()
        distance = math.hypot(position.x-top.x, position.y-top.y)
        passed = distance < 110 and abs(position.z-top.z-STATE["half_height"]) < 45
        STATE["boarding"].update(passed=passed, final=xyz(position), deck=xyz(top))
        assert passed, "Boarding interaction did not place the character on deck"
        start_route(now)
        return
    if STATE["phase"] == "settle":
        if now - STATE["phase_start"] < 0.6:
            return
        STATE.update(phase="walking", phase_start=now)
    if STATE["phase"] != "walking":
        return
    position = pawn.get_actor_location()
    top = hull.get_socket_location(STATE["top_socket"])
    target = top + STATE["direction"] * 60
    dx, dy = target.x-position.x, target.y-position.y
    horizontal = math.hypot(dx, dy)
    STATE["route_samples"].append(xyz(position))
    if horizontal < 30 and abs(position.z-top.z-STATE["half_height"]) < 45:
        STATE["move"].stop_movement_immediately()
        STATE["routes"].append({"name": STATE["route_name"], "passed": True,
                                "start": STATE["route_start"], "end": xyz(position),
                                "duration": round(now-STATE["phase_start"], 3),
                                "positions": STATE["route_samples"]})
        STATE["route_index"] += 1
        start_route(now)
    elif now - STATE["phase_start"] > 8.0:
        STATE["routes"].append({"name": STATE["route_name"], "passed": False,
                                "start": STATE["route_start"], "end": xyz(position),
                                "target": xyz(target), "positions": STATE["route_samples"]})
        raise AssertionError("Stair blocked: " + STATE["route_name"])
    elif horizontal > 0.01:
        pawn.add_movement_input(ue.Vector(dx/horizontal, dy/horizontal, 0), 1.0, True)


def tick(delta):
    try:
        if not REQUEST.exists() or REQUEST.stat().st_mtime < BOOT:
            return
        lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
        if lock["owner"] not in LOCK_OWNERS or dt.datetime.fromisoformat(lock["expires"]) <= dt.datetime.now().astimezone():
            return
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
        if not world or "UEDPIE" not in world.get_path_name():
            return
        now = ue.GameplayStatics.get_time_seconds(world)
        if STATE["phase"] == "waiting":
            pawn = ue.GameplayStatics.get_player_character(world, 0)
            if not pawn or now < 2:
                return
            ships = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)
                     if a.get_actor_label() == "Beach_Galleon"]
            assert len(ships) == 1, "Expected one Beach_Galleon"
            ship = ships[0]
            components = {c.get_name(): c for c in ship.get_components_by_class(ue.StaticMeshComponent)}
            hull = components["Hull"]
            capsule = pawn.get_component_by_class(ue.CapsuleComponent)
            STATE.update(pawn=pawn, ship=ship, hull=hull, move=pawn.get_component_by_class(ue.CharacterMovementComponent),
                         half_height=capsule.get_scaled_capsule_half_height(),
                         sails_no_collision=all(components[n].get_collision_enabled() == ue.CollisionEnabled.NO_COLLISION
                                                for n in ("Sails", "Rigging", "Flag")),
                         boat_count=len(ue.GameplayStatics.get_all_actors_of_class(world, ue.BoatActor)),
                         import_finished_at=json.loads((OUT / "import-result.json").read_text())["finished_at"])
            foot = hull.get_socket_location("boarding_ladder")
            # Start outside the aboard volume, on the ladder's water side.
            start = foot + ue.Vector(-70, 0, STATE["half_height"])
            pawn.set_actor_location(start, False, True)
            STATE["move"].stop_movement_immediately()
            assert ship.can_interact(pawn), "Boarding ladder not interactable from water"
            ship.interact(pawn)
            STATE.update(phase="boarding", phase_start=now, boarding={"start": xyz(start), "passed": False})
        else:
            pose = STATE["hull"].get_editor_property("relative_location")
            STATE["samples"].append([round(now, 3), round(pose.z, 3)])
            advance(now)
    except Exception:
        finish(False, traceback.format_exc())


HANDLE = ue.register_slate_post_tick_callback(tick)
write("validator-ready.json", {"started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "requires_fresh_request": True})
ue.log("T149_VALIDATOR_READY")
