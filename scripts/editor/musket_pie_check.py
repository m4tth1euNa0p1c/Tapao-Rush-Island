"""PIE check of the rebuilt musket in first person (T-212), in steps, during a game this session started on L_Test_Flat.

Reads monke_island/Saved/T186/musket-step.txt (steps separated by ";"), appends what it sees to musket-check.json.
  equip            - selects the inventory slot holding the pirate musket
  fire / reload    - AFirearmWeapon::RequestFire / RequestReload (the player's own input path)
  aim 1|0          - SetAiming
  slomo <x>        - game speed (to catch the reload's moments on pictures), slomo 1 to restore
  look <yaw> <pitch> - view direction
  hip x y z pitch yaw roll   - HipGripLocation / HipRotation live (DA_PirateMusketRules, not saved)
  aimfp x y z pitch yaw roll - AimMuzzleLocation / AimRotation live
  view fp|tp       - first or third person (the player's own toggle)
  near <cm> <deg>  - the player at that distance and bearing from the giant crab, facing it (T-211)
  god 1|0          - the player takes no damage (the blows still push)
  crab             - state, health, target of the giant crabs and the player's health
  shot             - HighResShot 1600x900 of the game view (Saved/Screenshots/WindowsEditor)
  state            - gesture, progress, loaded, balls (always appended)
Runs from scripts/editor/pie_session.py, action "musketcheck".
"""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186"
STEP = OUT / "musket-step.txt"
LOG = OUT / "musket-check.json"
RULES = "/Game/MonkeIsland/Weapons/Musket/DA_PirateMusketRules"


def game_world():
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
    assert world, "no game running"
    return world


def musket_of(world):
    found = ue.GameplayStatics.get_all_actors_of_class(world, ue.PirateMusketWeapon)
    return found[0] if found else None


def main():
    world = game_world()
    pawn = ue.GameplayStatics.get_player_pawn(world, 0)
    assert pawn, "no player pawn"
    controller = ue.GameplayStatics.get_player_controller(world, 0)
    steps = [s.strip() for s in (STEP.read_text(encoding="utf-8") if STEP.exists() else "state").split(";") if s.strip()]
    entry = {"steps": steps, "at": dt.datetime.now(dt.timezone.utc).isoformat()}
    for step in steps:
        parts = step.split()
        if parts[0] == "equip":
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            items = []
            for i in range(inventory.get_editor_property("slot_count")):
                item = inventory.get_item(i)
                items.append(item.get_class().get_name() if item else None)
            slot = next((i for i, name in enumerate(items) if name and "PirateMusket" in name), None)
            assert slot is not None, f"no pirate musket in the bar: {items}"
            # SelectSlot puts the active item away: only when it is not in hand already
            if inventory.get_active_slot() != slot:
                inventory.select_slot(slot)
            entry["equip"] = {"slot": slot, "items": items, "active": inventory.get_active_slot()}
        elif parts[0] in ("fire", "reload"):
            musket = musket_of(world)
            assert musket, "no musket in the game"
            (musket.request_fire if parts[0] == "fire" else musket.request_reload)()
            entry[parts[0]] = True
        elif parts[0] == "aim":
            musket_of(world).set_aiming(parts[1] == "1")
        elif parts[0] == "slomo":
            ue.SystemLibrary.execute_console_command(world, f"slomo {parts[1]}")
        elif parts[0] == "look":
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=float(parts[2]), yaw=float(parts[1])))
        elif parts[0] in ("hip", "aimfp"):
            v = [float(x) for x in parts[1:]]
            rules = ue.load_asset(RULES)
            loc, rot = ("hip_grip_location", "hip_rotation") if parts[0] == "hip" else ("aim_muzzle_location", "aim_rotation")
            rules.set_editor_property(loc, ue.Vector(v[0], v[1], v[2]))
            rules.set_editor_property(rot, ue.Rotator(roll=v[5], pitch=v[3], yaw=v[4]))
            entry[parts[0]] = v
        elif parts[0] == "view":
            # view fp|tp: the player's own view toggle (AIslanderCharacter::ToggleView) when needed
            want_fp = parts[1] == "fp"
            if pawn.is_first_person() != want_fp:
                pawn.toggle_view()
            entry["view"] = parts[1]
        elif parts[0] == "trumpet":
            # the flag ceremony hold (T-194 follow-up): third person and no movement while the ritual plays
            trumpet = pawn.get_component_by_class(ue.TrumpetEmoteComponent)
            entry["trumpet"] = {"ritual_playing": trumpet.is_ritual_playing(), "ritual_locked": trumpet.is_ritual_locked(),
                                "first_person": pawn.is_first_person(),
                                "move_ignored": controller.is_move_input_ignored()}
        elif parts[0] == "near":
            # near <distance cm> <bearing deg>: the player on the sand at that distance from the giant crab, facing it
            crab = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.GiantCrab)), None)
            assert crab, "no giant crab in the game"
            import math
            dist, bearing = float(parts[1]), math.radians(float(parts[2]))
            centre = crab.get_actor_location()
            spot = ue.Vector(centre.x + dist * math.cos(bearing), centre.y + dist * math.sin(bearing), centre.z + 20.0)
            pawn.set_actor_location(spot, False, True)
            yaw = math.degrees(math.atan2(centre.y - spot.y, centre.x - spot.x))
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=-8.0, yaw=yaw))
            entry["near"] = [round(spot.x), round(spot.y), round(spot.z)]
        elif parts[0] == "god":
            # god 1|0: the player takes no damage (the blows still push)
            pawn.set_editor_property("can_be_damaged", parts[1] != "1")
            entry["god"] = parts[1]
        elif parts[0] == "crab":
            entry["crabs"] = []
            for crab in ue.GameplayStatics.get_all_actors_of_class(world, ue.GiantCrab):
                target = crab.get_target()
                loc = crab.get_actor_location()
                extra = {}
                if target:
                    import math
                    t = target.get_actor_location()
                    d = math.hypot(t.x - loc.x, t.y - loc.y)
                    fwd = crab.get_actor_forward_vector()
                    ang = math.degrees(math.atan2(fwd.x * (t.y - loc.y) - fwd.y * (t.x - loc.x), fwd.x * (t.x - loc.x) + fwd.y * (t.y - loc.y)))
                    extra = {"target_at": [round(t.x), round(t.y), round(t.z)], "centre_dist": round(d), "angle": round(ang)}
                entry["crabs"].append({**extra, "name": crab.get_name(), "state": str(crab.get_state()).split(".")[-1],
                                       "health": round(crab.get_health().get_health()), "target": target.get_name() if target else None,
                                       "location": [round(loc.x), round(loc.y), round(loc.z)],
                                       "yaw": round(crab.get_actor_rotation().yaw)})
            here = pawn.get_actor_location()
            entry["player"] = {"location": [round(here.x), round(here.y), round(here.z)],
                               "health": round(pawn.get_component_by_class(ue.IslanderHealthComponent).get_health())}
        elif parts[0] == "sockets":
            # where the crab's sockets are against their bones (the kit's bones carry a scale)
            crab = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.GiantCrab)), None)
            mesh = crab.get_editor_property("mesh")
            out = {}
            for socket, bone in (("SOCKET_ClawGrab_R", "claw_r_hand"), ("WeakPoint_Eye_L", "eye_l"), ("WeakPoint_Belly", "abdomen")):
                sl, bl = mesh.get_socket_location(socket), mesh.get_socket_location(bone)
                bt = mesh.get_socket_transform(bone, ue.RelativeTransformSpace.RTS_WORLD)
                out[socket] = {"socket": [round(sl.x), round(sl.y), round(sl.z)], "bone": [round(bl.x), round(bl.y), round(bl.z)],
                               "bone_scale": round(bt.scale3d.x, 3)}
            c = crab.get_actor_location()
            out["actor"] = [round(c.x), round(c.y), round(c.z)]
            entry["sockets"] = out
        elif parts[0] == "shot":
            ue.SystemLibrary.execute_console_command(world, "HighResShot 1600x900")
            entry.setdefault("shots", 0)
            entry["shots"] += 1
    musket = musket_of(world)
    if musket:
        entry["state"] = {"gesture": str(musket.get_server_gesture()).split(".")[-1],
                          "progress": round(musket.get_server_gesture_progress(), 3), "loaded": musket.is_loaded(),
                          "pouch": musket.get_pouch_balls(), "reloading": musket.is_reloading(), "aiming": musket.is_aiming()}
    log = json.loads(LOG.read_text(encoding="utf-8")) if LOG.exists() else []
    log.append(entry)
    LOG.write_text(json.dumps(log, indent=1), encoding="utf-8")
    ue.log(f"MUSKET_CHECK {steps} {entry.get('state')}")


if __name__ == "__main__":
    main()
