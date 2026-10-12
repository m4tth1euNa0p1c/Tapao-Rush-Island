"""PIE check of the French garrison of the Test Flat fort (T-213), in steps, during a game this session started.

Reads monke_island/Saved/T186/garrison-step.txt (steps separated by ";"), appends what it sees to garrison-check.json.
  at <x> <y> <z> <yaw>   - the player at that place of the fort frame (m, maquette axes: gate at y -20.2, court z 0),
                           looking along the world yaw (deg)
  look <yaw> <pitch>     - view direction
  god 1|0                - the player takes no damage
  hurt <label> <amount>  - the player hurts a soldier (alarm, flinch, death)
  out <label> <x> <y>    - a soldier put outside the walls (fort frame, m): the fort must bring it back
  view fp|tp             - first or third person (the player's own toggle)
  slomo <x>              - game speed
  shot                   - HighResShot 1600x900 of the game view (Saved/Screenshots/WindowsEditor)
  soldiers               - state, target, shots, health, place (fort frame) and inside or not of every soldier
                           (always appended, with the player's health)
Runs from scripts/editor/pie_session.py, action "garrisoncheck".
"""
import datetime as dt
import json
import math
import time
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186"
STEP = OUT / "garrison-step.txt"
LOG = OUT / "garrison-check.json"
ORIGIN = ue.Vector(-66000.0, 0.0, 115.0)


def to_world(x, y, z):
    return ORIGIN + ue.Vector(x * 100.0, -y * 100.0, z * 100.0)


def to_fort(v):
    return [round((v.x - ORIGIN.x) / 100.0, 2), round(-(v.y - ORIGIN.y) / 100.0, 2), round((v.z - ORIGIN.z) / 100.0, 2)]


def soldiers_of(world):
    return sorted(ue.GameplayStatics.get_all_actors_of_class(world, ue.FortSoldier), key=lambda a: a.get_name())


def main():
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
    assert world, "no game running"
    pawn = ue.GameplayStatics.get_player_pawn(world, 0)
    controller = ue.GameplayStatics.get_player_controller(world, 0)
    assert pawn and controller, "no player"
    steps = [s.strip() for s in (STEP.read_text(encoding="utf-8") if STEP.exists() else "soldiers").split(";") if s.strip()]
    entry = {"steps": steps, "at": dt.datetime.now(dt.timezone.utc).isoformat(), "game_time": round(ue.GameplayStatics.get_time_seconds(world), 2)}
    by_label = {s.get_actor_label() if hasattr(s, "get_actor_label") else s.get_name(): s for s in soldiers_of(world)}
    zone = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.FortGarrisonZone)), None)
    for step in steps:
        parts = step.split()
        if parts[0] == "at":
            x, y, z, yaw = (float(v) for v in parts[1:5])
            pawn.set_actor_location(to_world(x, y, z) + ue.Vector(0.0, 0.0, 60.0), False, True)
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=-5.0, yaw=yaw))
            entry["at"] = [x, y, z, yaw]
        elif parts[0] == "look":
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=float(parts[2]), yaw=float(parts[1])))
        elif parts[0] == "god":
            pawn.set_editor_property("can_be_damaged", parts[1] != "1")
            entry["god"] = parts[1]
        elif parts[0] == "hurt":
            soldier = next((s for label, s in by_label.items() if parts[1] in label), None)
            assert soldier, f"no soldier {parts[1]}: {sorted(by_label)}"
            ue.GameplayStatics.apply_damage(soldier, float(parts[2]), controller, pawn, None)
            entry["hurt"] = [soldier.get_name(), float(parts[2])]
        elif parts[0] == "out":
            soldier = next((s for label, s in by_label.items() if parts[1] in label), None)
            assert soldier, f"no soldier {parts[1]}"
            soldier.set_actor_location(to_world(float(parts[2]), float(parts[3]), 0.0) + ue.Vector(0.0, 0.0, 80.0), False, True)
            entry["out"] = [soldier.get_name(), float(parts[2]), float(parts[3])]
        elif parts[0] == "view":
            # view fp|tp: the player's own view toggle when needed
            want_fp = parts[1] == "fp"
            if pawn.is_first_person() != want_fp:
                pawn.toggle_view()
            entry["view"] = parts[1]
        elif parts[0] == "galleon":
            # the player on the deck of the galleon (aboard at the end of the deluge: the day is survived)
            # (the bean_spawn socket of the hull, where the galleon puts the crew at the start; never the top of the
            # bounds: that is the masthead, a deadly fall)
            ship = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)), None)
            assert ship, "no galleon in this map"
            hull = next((c for c in ship.get_components_by_class(ue.StaticMeshComponent) if c.does_socket_exist("bean_spawn")), None)
            assert hull, "no hull with a bean_spawn socket"
            spot = hull.get_socket_location("bean_spawn") + ue.Vector(0.0, 0.0, 110.0)
            pawn.set_actor_location(spot, False, True)
            entry["galleon"] = [round(spot.x), round(spot.y), round(spot.z)]
        elif parts[0] == "dayinfo":
            state = ue.GameplayStatics.get_game_state(world)
            info = {}
            for name in ("is_day_survived", "is_team_wiped", "get_next_travel_time", "get_time_before_submersion",
                         "get_flood_start_time", "get_expedition_start_time"):
                try:
                    value = getattr(state, name)()
                    info[name] = str(value) if not isinstance(value, (int, float, bool)) else value
                except Exception as error:  # noqa: BLE001
                    info[name] = f"? {error}"
            info["server_time"] = state.get_server_world_time_seconds()
            ships = ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)
            info["aboard"] = bool(ships) and ships[0].is_actor_aboard(pawn)
            entry["dayinfo"] = info
        elif parts[0] == "probe":
            # probe <count> <interval s>: what the player's view shows over the next seconds (view target, camera, the
            # first thing 3 m ahead of the camera, ragdoll / first person), a screenshot with each sample; appended to
            # garrison-probe.json
            count, interval = int(parts[1]), float(parts[2])
            state = {"n": 0, "next": time.time(), "rows": []}
            probe_log = OUT / "garrison-probe.json"

            def sample(_delta, state=state):
                game = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
                if not game or state["n"] >= count:
                    ue.unregister_slate_post_tick_callback(state["handle"])
                    probe_log.write_text(json.dumps(state["rows"], indent=1), encoding="utf-8")
                    return
                if time.time() < state["next"]:
                    return
                state["n"] += 1
                state["next"] = time.time() + interval
                pc = ue.GameplayStatics.get_player_controller(game, 0)
                me = ue.GameplayStatics.get_player_pawn(game, 0)
                cam = pc.player_camera_manager if pc else None
                row = {"t": round(time.time(), 2), "game_time": round(ue.GameplayStatics.get_time_seconds(game), 2)}
                if cam:
                    loc, rot = cam.get_camera_location(), cam.get_camera_rotation()
                    row["view_target"] = pc.get_view_target().get_name() if pc.get_view_target() else None
                    row["cam"] = [round(loc.x), round(loc.y), round(loc.z), round(rot.pitch, 1), round(rot.yaw, 1)]
                    end = loc + rot.get_forward_vector() * 300.0
                    hit = ue.SystemLibrary.line_trace_single(game, loc, end, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], ue.DrawDebugTrace.NONE, True)
                    if hit:
                        h = hit.to_tuple()
                        row["hit"] = [h[9].get_name() if h[9] else None, h[10].get_name() if h[10] else None, round(h[3], 1)] if len(h) > 10 else str(h)[:200]
                if me:
                    for name in ("is_ragdoll", "is_first_person"):
                        try:
                            row[name] = bool(getattr(me, name)())
                        except Exception as error:  # noqa: BLE001
                            row[name] = f"? {error}"[:60]
                    pl = me.get_actor_location()
                    row["pawn"] = [round(pl.x), round(pl.y), round(pl.z)]
                ue.SystemLibrary.execute_console_command(game, "shot showui")
                state["rows"].append(row)

            state["handle"] = ue.register_slate_post_tick_callback(sample)
            entry["probe"] = [count, interval]
        elif parts[0] == "parts":
            # the player's primitives (and those of the actors attached to it): visible, owner-no-see, owner, attach parent
            rows = []
            attached = [pawn] + list(pawn.get_attached_actors(True))
            for actor in attached:
                for comp in actor.get_components_by_class(ue.PrimitiveComponent):
                    parent = comp.get_attach_parent()
                    rows.append({"actor": actor.get_name(), "owner": actor.get_owner().get_name() if actor.get_owner() else None,
                                 "comp": comp.get_name(), "class": comp.get_class().get_name(), "visible": comp.is_visible(),
                                 "owner_no_see": bool(comp.get_editor_property("owner_no_see")),
                                 "only_owner_see": bool(comp.get_editor_property("only_owner_see")),
                                 "parent": (parent.get_name() + "@" + str(comp.get_attach_socket_name())) if parent else None})
            entry["parts"] = rows
        elif parts[0] == "hide":
            # hide <name part>: hides the player's components whose name holds it (which one covers the view?)
            hidden = []
            for actor in [pawn] + list(pawn.get_attached_actors(True)):
                for comp in actor.get_components_by_class(ue.PrimitiveComponent):
                    if parts[1].lower() in comp.get_name().lower():
                        comp.set_visibility(False, True)
                        hidden.append(actor.get_name() + "." + comp.get_name())
            entry["hidden"] = hidden
        elif parts[0] == "near":
            # near <cm>: the primitives whose bounds come within that distance of the camera (what can cover the view?)
            reach = float(parts[1]) if len(parts) > 1 else 100.0
            cam = controller.player_camera_manager.get_camera_location()
            rows = []
            for actor in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor):
                if actor == pawn:
                    continue
                for comp in actor.get_components_by_class(ue.PrimitiveComponent):
                    if not comp.is_visible():
                        continue
                    o, e, _r = ue.SystemLibrary.get_component_bounds(comp)
                    dx = max(abs(cam.x - o.x) - e.x, 0.0)
                    dy = max(abs(cam.y - o.y) - e.y, 0.0)
                    dz = max(abs(cam.z - o.z) - e.z, 0.0)
                    gap = math.sqrt(dx * dx + dy * dy + dz * dz)
                    if gap <= reach:
                        mesh = None
                        if hasattr(comp, "static_mesh"):
                            sm = comp.get_editor_property("static_mesh")
                            mesh = sm.get_name() if sm else None
                        rows.append({"actor": actor.get_name(), "comp": comp.get_name(), "class": comp.get_class().get_name(),
                                     "mesh": mesh, "gap": round(gap, 1), "mobility": str(comp.get_editor_property("mobility")).split(".")[-1],
                                     "extent": [round(e.x), round(e.y), round(e.z)]})
            entry["near"] = sorted(rows, key=lambda r: r["gap"])[:40]
            entry["cam_at"] = [round(cam.x), round(cam.y), round(cam.z)]
        elif parts[0] == "merchant":
            # the merchant of the galleon, its stand, what it is attached to; the deck spawn socket; the player's place
            ship = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)), None)
            hull = next((c for c in ship.get_components_by_class(ue.StaticMeshComponent) if c.does_socket_exist("bean_spawn")), None)
            info = {"ship": [round(v) for v in (ship.get_actor_location().x, ship.get_actor_location().y, ship.get_actor_location().z)],
                    "ship_yaw": round(ship.get_actor_rotation().yaw, 1),
                    "bean_spawn": [round(v) for v in (lambda q: (q.x, q.y, q.z))(hull.get_socket_location("bean_spawn"))]}
            for actor in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor):
                if "Merchant" in actor.get_name():
                    parent = actor.get_attach_parent_actor()
                    stand = next((c for c in actor.get_components_by_class(ue.StaticMeshComponent) if c.get_name() == "MerchantStand"), None)
                    o, e, _r = ue.SystemLibrary.get_component_bounds(stand) if stand else (ue.Vector(), ue.Vector(), 0)
                    info["merchant"] = {"name": actor.get_name(), "at": [round(actor.get_actor_location().x), round(actor.get_actor_location().y), round(actor.get_actor_location().z)],
                                        "yaw": round(actor.get_actor_rotation().yaw, 1), "attached_to": parent.get_name() if parent else None,
                                        "stand_origin": [round(o.x), round(o.y), round(o.z)], "stand_extent": [round(e.x), round(e.y), round(e.z)],
                                        "stand_rel": [round(v) for v in (lambda t: (t.x, t.y, t.z))(stand.get_editor_property("relative_location"))] if stand else None}
            cam = controller.player_camera_manager.get_camera_location()
            info["camera"] = [round(cam.x), round(cam.y), round(cam.z)]
            base = pawn.get_editor_property("character_movement").get_movement_base()
            info["pawn_base"] = base.get_name() if base else None
            entry["merchant"] = info
        elif parts[0] == "show":
            # show <actor part>.<component> ...: makes those components of the player (or its gear) visible again, one by one
            shown = []
            for item in parts[1:]:
                actor_part, comp_name = item.split(".")
                for actor in [pawn] + list(pawn.get_attached_actors(True)):
                    if actor_part.lower() not in actor.get_name().lower():
                        continue
                    for comp in actor.get_components_by_class(ue.PrimitiveComponent):
                        if comp.get_name() == comp_name:
                            comp.set_visibility(True, False)
                            shown.append(actor.get_name() + "." + comp.get_name())
            entry["shown"] = shown
        elif parts[0] == "tpw":
            # tpw <x> <y> <z>: the player at a world place (cm)
            pawn.set_actor_location(ue.Vector(float(parts[1]), float(parts[2]), float(parts[3])), False, True)
            entry["tpw"] = parts[1:4]
        elif parts[0] == "facearmed":
            # facearmed <cm>: the player that far in front of the nearest armed tiki, looking at it (its musket in hand)
            far = float(parts[1]) if len(parts) > 1 else 400.0
            armed = [t for t in ue.GameplayStatics.get_all_actors_of_class(world, ue.TikiWarrior)
                     if any("TikiMusket" in c.get_name() for c in t.get_components_by_class(ue.StaticMeshComponent))]
            assert armed, "no armed tiki"
            here = pawn.get_actor_location()
            t = min(armed, key=lambda a: (a.get_actor_location() - here).length())
            at = t.get_actor_location()
            fwd = t.get_actor_forward_vector()
            spot = at + ue.Vector(fwd.x, fwd.y, 0.0) * far + ue.Vector(0.0, 0.0, 60.0)
            pawn.set_actor_location(spot, False, True)
            look = (at - spot)
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=-8.0, yaw=math.degrees(math.atan2(look.y, look.x))))
            entry["facearmed"] = [t.get_name(), [round(at.x), round(at.y), round(at.z)], len(armed)]
        elif parts[0] == "musketinfo":
            # the musket of the nearest armed tiki: where it is against the hand and the actor, visible or not
            armed = [t for t in ue.GameplayStatics.get_all_actors_of_class(world, ue.TikiWarrior)
                     if any("TikiMusket" in c.get_name() for c in t.get_components_by_class(ue.StaticMeshComponent))]
            here = pawn.get_actor_location()
            t = min(armed, key=lambda a: (a.get_actor_location() - here).length())
            gun = next(c for c in t.get_components_by_class(ue.StaticMeshComponent) if "TikiMusket" in c.get_name())
            mesh = t.get_component_by_class(ue.SkeletalMeshComponent)
            hand = mesh.get_socket_transform("hand_r", ue.RelativeTransformSpace.RTS_WORLD)
            weapon = mesh.get_socket_transform("weapon_r", ue.RelativeTransformSpace.RTS_WORLD)
            o, e, _r = ue.SystemLibrary.get_component_bounds(gun)
            gt = gun.get_world_transform()
            loc = t.get_actor_location()
            entry["musket"] = {"tiki": t.get_name(), "actor": [round(loc.x), round(loc.y), round(loc.z)],
                               "hand": [round(hand.translation.x), round(hand.translation.y), round(hand.translation.z)],
                               "hand_rot": [round(v, 1) for v in (hand.rotation.rotator().roll, hand.rotation.rotator().pitch, hand.rotation.rotator().yaw)],
                               "weapon_bone": [round(weapon.translation.x), round(weapon.translation.y), round(weapon.translation.z)],
                               "weapon_rot": [round(v, 1) for v in (weapon.rotation.rotator().roll, weapon.rotation.rotator().pitch, weapon.rotation.rotator().yaw)],
                               "gun": [round(gt.translation.x), round(gt.translation.y), round(gt.translation.z)],
                               "gun_scale": [round(v, 3) for v in (gt.scale3d.x, gt.scale3d.y, gt.scale3d.z)],
                               "gun_bounds_extent": [round(e.x), round(e.y), round(e.z)], "visible": gun.is_visible(),
                               "attached_to": str(gun.get_attach_socket_name()), "mesh_scale": [round(v, 3) for v in (mesh.get_world_transform().scale3d.x,)]}
        elif parts[0] == "facesoldier":
            # facesoldier <cm> [side deg]: the player that far from the nearest French soldier, in front of it (or turned
            # by side degrees around it), looking at it (to watch its aim and its shot)
            far = float(parts[1]) if len(parts) > 1 else 1000.0
            side = math.radians(float(parts[2])) if len(parts) > 2 else 0.0
            here = pawn.get_actor_location()
            soldiers = list(ue.GameplayStatics.get_all_actors_of_class(world, ue.FortSoldier))
            assert soldiers, "no French soldier"
            t = min(soldiers, key=lambda a: (a.get_actor_location() - here).length())
            at = t.get_actor_location()
            fwd = t.get_actor_forward_vector()
            ang = math.atan2(fwd.y, fwd.x) + side
            spot = at + ue.Vector(math.cos(ang), math.sin(ang), 0.0) * far + ue.Vector(0.0, 0.0, 60.0)
            pawn.set_actor_location(spot, False, True)
            look = at - spot
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=-4.0, yaw=math.degrees(math.atan2(look.y, look.x))))
            entry["facesoldier"] = [t.get_name(), [round(at.x), round(at.y), round(at.z)]]
        elif parts[0] == "sidecam":
            # sidecam <cm> <deg> [height cm]: the view of the player taken by a camera that far from the nearest French
            # soldier, turned by deg from its front (90: its left side), looking at it; "sidecam off" gives it back
            if parts[1] == "off":
                controller.set_view_target_with_blend(pawn, 0.0)
            else:
                far, ang = float(parts[1]), math.radians(float(parts[2]))
                rise = float(parts[3]) if len(parts) > 3 else 60.0
                here = pawn.get_actor_location()
                t = min(ue.GameplayStatics.get_all_actors_of_class(world, ue.FortSoldier), key=lambda a: (a.get_actor_location() - here).length())
                at = t.get_actor_location()
                fwd = t.get_actor_forward_vector()
                yaw = math.atan2(fwd.y, fwd.x) + ang
                spot = at + ue.Vector(math.cos(yaw), math.sin(yaw), 0.0) * far + ue.Vector(0.0, 0.0, rise)
                look = at + ue.Vector(0.0, 0.0, 20.0) - spot
                rot = ue.Rotator(roll=0.0, pitch=math.degrees(math.atan2(look.z, math.hypot(look.x, look.y))), yaw=math.degrees(math.atan2(look.y, look.x)))
                xform = ue.Transform(spot, rot, ue.Vector(1.0, 1.0, 1.0))
                cam = ue.GameplayStatics.finish_spawning_actor(ue.GameplayStatics.begin_deferred_actor_spawn_from_class(world, ue.CameraActor, xform), xform)
                controller.set_view_target_with_blend(cam, 0.0)
                entry["sidecam"] = [t.get_name(), [round(spot.x), round(spot.y), round(spot.z)]]
        elif parts[0] == "flagsite":
            # flagsite <hamlet|zone> [n]: the player 4 m from the n-th flag site of hamlets (zone id "_H") or zones, facing it
            want_hamlet = parts[1] == "hamlet"
            n = int(parts[2]) if len(parts) > 2 else 0
            sites = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.FlagCeremonySite)
                     if ("_H" in str(a.get_editor_property("zone_id"))) == want_hamlet]
            entry["flagsites"] = [[str(a.get_editor_property("zone_id")), [round(v) for v in (a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z)]] for a in sites]
            if sites:
                site = sites[min(n, len(sites) - 1)]
                at = site.get_actor_location()
                spot = at + ue.Vector(-400.0, -150.0, 120.0)
                pawn.set_actor_location(spot, False, True)
                look = at - spot
                controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=math.degrees(math.atan2(look.z, math.hypot(look.x, look.y))) - 5.0,
                                                           yaw=math.degrees(math.atan2(look.y, look.x))))
                entry["flagsite"] = str(site.get_editor_property("zone_id"))
                if len(parts) > 3 and parts[3] == "start":
                    # the player at the stones: the server path of F (TryStart)
                    pawn.set_actor_location(at + ue.Vector(-90.0, 0.0, 120.0), False, True)
                    entry["flagsite_started"] = bool(site.try_start(pawn))
        elif parts[0] == "shoredist":
            # distance from the galleon and the barque to the nearest land (traces from high above, as the spawner)
            sea = 0.0
            rows = {}
            floating = list(ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)) + list(ue.GameplayStatics.get_all_actors_of_class(world, ue.BoatActor))
            for cls in (ue.GalleonActor, ue.BoatActor):
                actor = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, cls)), None)
                if not actor:
                    continue
                here = actor.get_actor_location()
                best = None
                for k in range(72):
                    ang = 2.0 * math.pi * k / 72
                    for step in range(1, 241):
                        d = step * 500.0
                        x, y = here.x + math.cos(ang) * d, here.y + math.sin(ang) * d
                        hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(x, y, 20000.0), ue.Vector(x, y, -600.0),
                                                                 ue.TraceTypeQuery.TRACE_TYPE_QUERY1, False, floating, ue.DrawDebugTrace.NONE, True)
                        t = hit.to_tuple() if hit else None
                        if t and t[0] and t[4].z > sea + 1.0:
                            if best is None or d < best[0]:
                                best = (d, round(math.degrees(ang)), t[10].get_name() if t[10] else None)
                            break
                rows[cls.__name__] = {"at": [round(here.x), round(here.y)], "land_m": round(best[0] / 100.0) if best else None,
                                      "dir": best[1] if best else None, "what": best[2] if best else None}
            entry["shoredist"] = rows
        elif parts[0] == "depthline":
            # depthline <deg> <step m> <count>: water depth from the galleon toward a direction (traces from high above)
            ang, step_m, count = math.radians(float(parts[1])), float(parts[2]), int(parts[3])
            ship = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)), None)
            floating = list(ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)) + list(ue.GameplayStatics.get_all_actors_of_class(world, ue.BoatActor))
            here = ship.get_actor_location()
            line = []
            for k in range(count + 1):
                d = k * step_m * 100.0
                x, y = here.x + math.cos(ang) * d, here.y + math.sin(ang) * d
                hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(x, y, 20000.0), ue.Vector(x, y, -5000.0),
                                                         ue.TraceTypeQuery.TRACE_TYPE_QUERY1, False, floating, ue.DrawDebugTrace.NONE, True)
                t = hit.to_tuple() if hit else None
                line.append([round(d / 100.0), round(-t[4].z) if t and t[0] else None])
            entry["depthline"] = line
        elif parts[0] == "helm":
            # helm [use]: the player at the galleon's wheel; the prompt it sees there; "use" interacts (the server path)
            ship = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)), None)
            hull = ship.get_hull()
            spot = hull.get_socket_location("helm") if hull.does_socket_exist("helm") else ship.get_actor_location()
            pawn.set_actor_location(spot + ue.Vector(0.0, 0.0, 110.0), False, True)
            info = {"socket": hull.does_socket_exist("helm")}
            for name in ("get_interaction_prompt", "can_interact"):
                try:
                    info[name] = str(getattr(ship, name)(pawn))
                except Exception as error:  # noqa: BLE001
                    info[name] = f"? {error}"[:120]
            if len(parts) > 1 and parts[1] == "use":
                try:
                    ship.interact(pawn)
                    info["used"] = True
                except Exception as error:  # noqa: BLE001
                    info["used"] = f"? {error}"[:120]
            entry["helm"] = info
        elif parts[0] == "execprobe":
            # places of the execution scene in the hull frame (exec-probe.json)
            import runpy
            runpy.run_path(str(ROOT / "scripts/editor/exec_probe_pie.py"), run_name="__main__")
            entry["execprobe"] = True
        elif parts[0] == "cmd":
            # cmd <console command...>: any console command in the game (monke.SetHour 22, monke.CrabEmerge...)
            ue.SystemLibrary.execute_console_command(world, " ".join(parts[1:]))
            entry.setdefault("cmds", []).append(" ".join(parts[1:]))
        elif parts[0] == "slomo":
            ue.SystemLibrary.execute_console_command(world, f"slomo {parts[1]}")
        elif parts[0] == "bounds":
            # mesh and gear bounds of every soldier, relative to it (culling when the player comes close?)
            entry["bounds"] = {}
            for label, soldier in by_label.items():
                here = soldier.get_actor_location()
                rows = {}
                for comp in soldier.get_components_by_class(ue.PrimitiveComponent):
                    o, e, r = ue.SystemLibrary.get_component_bounds(comp)
                    rows[comp.get_name()] = {"offset": [round(o.x - here.x), round(o.y - here.y), round(o.z - here.z)],
                                             "extent": [round(e.x), round(e.y), round(e.z)], "visible": comp.is_visible()}
                entry["bounds"][label] = rows
        elif parts[0] == "shot":
            ue.SystemLibrary.execute_console_command(world, "HighResShot 1600x900")
        elif parts[0] == "burst":
            # burst <count> <interval s>: screenshots with the UI ("shot showui") over the next seconds, from a tick of
            # the editor (the day transition scenes go on while this step returns)
            count, interval = int(parts[1]), float(parts[2])
            state = {"n": 0, "next": time.time()}

            def tick(_delta, state=state):
                game = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
                if not game or state["n"] >= count:
                    ue.unregister_slate_post_tick_callback(state["handle"])
                    return
                if time.time() >= state["next"]:
                    ue.SystemLibrary.execute_console_command(game, "shot showui")
                    state["n"] += 1
                    state["next"] = time.time() + interval

            state["handle"] = ue.register_slate_post_tick_callback(tick)
            entry["burst"] = [count, interval]
    rows = []
    for label, soldier in by_label.items():
        target = soldier.get_target()
        health = soldier.get_health()
        loc = soldier.get_actor_location()
        rows.append({"soldier": label, "state": str(soldier.get_state()).split(".")[-1], "target": target.get_name() if target else None,
                     "shots": soldier.get_shots_left(), "health": round(health.get_health(), 1) if health else None,
                     "fort_m": to_fort(loc), "inside": bool(zone.is_inside(loc)) if zone else None,
                     "dist_to_player_m": round(math.dist((loc.x, loc.y), (pawn.get_actor_location().x, pawn.get_actor_location().y)) / 100.0, 1)})
    entry["soldiers"] = rows
    player_health = pawn.get_component_by_class(ue.IslanderHealthComponent)
    entry["player"] = {"fort_m": to_fort(pawn.get_actor_location()),
                       "health": round(player_health.get_health(), 1) if player_health else None}
    log = json.loads(LOG.read_text(encoding="utf-8")) if LOG.exists() else []
    log.append(entry)
    LOG.write_text(json.dumps(log, indent=1), encoding="utf-8")


if __name__ == "__main__":
    main()
