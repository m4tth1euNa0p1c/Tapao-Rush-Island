"""PIE check of the French forts built on the land of the atoll (owner 2026-10-08), in steps, during a game this session
started. Reads monke_island/Saved/T186/fortland-step.txt (steps separated by ";"), appends what it sees to
fortland-check.json:
  forts                         - the AFortSite of the world: place, yaw, level, built or not; the garrison zones, soldiers
                                  (inside or not), loot spawners, temples and marae, camps (always appended)
  tofort <x> <y> <z> <yaw> <pitch> - the player at that place of the first fort's frame (m: gate at +y, court z 1.65),
                                  looking along yaw (deg, fort frame) and pitch
  god 1|0                       - the player takes no damage
  zonehit <bone> <dmg> [class] [gun] [far] - a blow at a bone of the nearest living mob of the class, by the player's gun
                                  with "gun", from the player's eyes with "far" (1 m in front otherwise: point blank)
  cmd <console command>         - a console command in the game
  shot                          - HighResShot 1600x900 of the game view (Saved/Screenshots/WindowsEditor)
  faceactor <name> <dist m> [lift m] [side deg] - the player in front of the nearest such actor, facing it
  interact <name>               - that nearest actor's interaction (F), e.g. the merchant's shop
  burst <count> <interval s>   - game view screenshots over the next seconds (game time), at the effect's real pace
  fx <system> <ahead m> <up m> [yaw] [pitch] - a Niagara system spawned in front of the player (smoke checks, T-321)
  view fp|tp / look <pitch> [yaw] - the player's view and its pitch (weapon view audit, T-330)
  fpvis [label]                 - the meshes of the item in hand: shown, drawn lately (culled or not), bounds in view space
  orbit <yaw> <dist m> <height m> [fov] / orbitend - a camera around the player made the view (third person portraits)
  move <s> [sprint 0|1] [yaw] [tag] - walk or run ahead, every frame sampled -> Saved/T186/move-<tag>.json (jitter)
  claim                         - first step right after the lead starts a game: the steps then run in that game only
  inject <action path> [hold s] - the local player presses (or holds) an Enhanced Input action (torch raise, swing)
Runs from scripts/editor/pie_session.py, action "fortcheck".
"""
import datetime as dt
import json
import math
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186"
STEP = OUT / "fortland-step.txt"
LOG = OUT / "fortland-check.json"


def v3(v):
    return [round(v.x), round(v.y), round(v.z)]


def _local_world():
    """The game world of the player at this machine: the only one alone, the host's in a listen server game, the
    client's when the viewport plays as a client of a dedicated server in the same process (T-331)"""
    first = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
    if not first:
        return None
    for world in [first] + [w for w in ue.ObjectIterator(ue.World) if "UEDPIE_" in w.get_path_name() and w.get_name() == first.get_name()]:
        controller = ue.GameplayStatics.get_player_controller(world, 0)
        if controller and controller.is_local_controller() and controller.get_controlled_pawn():
            return world
    return first


def _in_camera(point, cam_loc, cam_rot):
    """A world point in the view's space (cm): ahead, right, up"""
    d = [point.x - cam_loc.x, point.y - cam_loc.y, point.z - cam_loc.z]
    axes = (cam_rot.get_forward_vector(), cam_rot.get_right_vector(), cam_rot.get_up_vector())
    return [round(d[0] * a.x + d[1] * a.y + d[2] * a.z, 2) for a in axes]


def _mesh_visibility(world, pawn, label):
    """The meshes of the item in hand and the player's first person meshes: shown, drawn lately, bounds (T-330)"""
    pcm = ue.GameplayStatics.get_player_camera_manager(world, 0)
    cam_loc, cam_rot = pcm.get_camera_location(), pcm.get_camera_rotation()
    now = ue.GameplayStatics.get_time_seconds(world)
    inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
    item = inventory.get_equipped_item() if inventory else None
    comps = list(item.get_components_by_class(ue.PrimitiveComponent)) if item else []
    comps += [c for c in pawn.get_components_by_class(ue.PrimitiveComponent)
              if "FirstPerson" in c.get_name() or "Arms" in c.get_class().get_name()]
    rows = []
    for comp in comps:
        origin, extent, radius = ue.SystemLibrary.get_component_bounds(comp)
        try:
            ago = comp.was_recently_rendered(0.25)
        except Exception as error:  # noqa: BLE001
            ago = str(error)[:60]
        rows.append({"comp": f"{comp.get_owner().get_name()}.{comp.get_name()}", "visible": comp.is_visible(),
                     "hidden_in_game": comp.get_editor_property("hidden_in_game"), "rendered_ago": ago,
                     "bounds_cam": _in_camera(origin, cam_loc, cam_rot), "extent": [round(extent.x), round(extent.y), round(extent.z)],
                     "radius": round(radius, 1), "origin_cam": _in_camera(comp.get_world_location(), cam_loc, cam_rot)})
    return {"label": label, "pitch": round(cam_rot.pitch, 1), "fov": round(pcm.get_fov_angle(), 1),
            "item": item.get_class().get_name() if item else None, "fp": pawn.is_first_person(), "rows": rows}


def _stats(values):
    if not values:
        return None
    mean = sum(values) / len(values)
    std = math.sqrt(sum((v - mean) ** 2 for v in values) / len(values))
    return {"mean": round(mean, 3), "std": round(std, 3), "min": round(min(values), 3), "max": round(max(values), 3)}


def _move_summary(rows):
    """Steadiness of a walk or a run (T-330 / T-331): frame times, the view's speed from frame to frame, its height,
    and how much the head of the bean and the first person meshes move in the view's space"""
    if len(rows) < 3:
        return {"frames": len(rows)}
    t0 = rows[0]["t"]
    steady = [r for r in rows if r["t"] - t0 >= 0.6] or rows
    summary = {"frames": len(rows), "steady_frames": len(steady), "dt": _stats([r["dt"] for r in steady]),
               "speed": _stats([r["speed"] for r in steady])}
    cam_speed, cap_speed, view_minus_capsule = [], [], []
    for a, b in zip(steady, steady[1:]):
        step = b["t"] - a["t"]
        if step <= 0:
            continue
        cam_speed.append(math.dist(a["cam"][:2], b["cam"][:2]) / step)
        cap_speed.append(math.dist(a["capsule"][:2], b["capsule"][:2]) / step)
        view_minus_capsule.append(math.dist([b["cam"][i] - b["capsule"][i] for i in range(3)], [a["cam"][i] - a["capsule"][i] for i in range(3)]))
    summary["view_speed"] = _stats(cam_speed)
    summary["capsule_speed"] = _stats(cap_speed)
    summary["view_vs_capsule_step_cm"] = _stats(view_minus_capsule)
    summary["view_height"] = _stats([r["cam"][2] for r in steady])
    heads = [r["head_cam"] for r in steady if "head_cam" in r]
    if heads:
        summary["head_in_view"] = {axis: _stats([h[i] for h in heads]) for i, axis in enumerate(("ahead", "right", "up"))}
    fps = {}
    for r in steady:
        for name, p in r.get("fp", {}).items():
            fps.setdefault(name, []).append(p)
    summary["fp_in_view"] = {name: {axis: _stats([p[i] for p in pts]) for i, axis in enumerate(("ahead", "right", "up"))} for name, pts in fps.items()}
    # the biggest jump of an animated bone from one frame to the next, against its usual step (pops of the clips)
    bones = {}
    for r in steady:
        for name, p in r.get("fp_bone", {}).items():
            bones.setdefault(name, []).append(p)
    summary["fp_bone_steps_cm"] = {name: _stats([math.dist(a, b) for a, b in zip(pts, pts[1:])]) for name, pts in bones.items()}
    hands = [r["tp_from_hand"] for r in steady if "tp_from_hand" in r]
    if hands:
        summary["tp_weapon_from_hand"] = {axis: _stats([h[i] for h in hands]) for i, axis in enumerate(("x", "y", "z"))}
    return summary


def main():
    world = _local_world()
    assert world, "no game running"
    controller = ue.GameplayStatics.get_player_controller(world, 0)
    pawn = controller.get_controlled_pawn() if controller else None
    assert pawn and controller, "no player"
    steps = [s.strip() for s in (STEP.read_text(encoding="utf-8") if STEP.exists() else "forts").split(";") if s.strip()]
    # only in a game the lead started itself: "claim" marks it right after the start; any other game (a player's
    # own PIE, started after the lead's one ended) is refused (2026-10-11: steps ran in the owner's game)
    game_state = ue.GameplayStatics.get_game_state(world)
    if steps and steps[0] == "claim":
        game_state.tags = list(game_state.tags) + ["LeadPIE"]
        steps = steps[1:]
    assert game_state and game_state.actor_has_tag("LeadPIE"), "this game was not started by the lead (no claim): refused"
    entry = {"steps": steps, "at": dt.datetime.now(dt.timezone.utc).isoformat(), "game_time": round(ue.GameplayStatics.get_time_seconds(world), 2)}
    forts = ue.GameplayStatics.get_all_actors_of_class(world, ue.FortSite)
    for step in steps:
        parts = step.split()
        if parts[0] == "tofort":
            assert forts, "no fort in the world"
            fort = forts[0]
            frame = fort.get_editor_property("level_transform")
            x, y, z, yaw, pitch = (float(v) for v in parts[1:6])
            target = frame.transform_location(ue.Vector(x * 100.0, y * 100.0, z * 100.0))
            pawn.set_actor_location(target + ue.Vector(0.0, 0.0, 90.0), False, True)
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=pitch, yaw=frame.rotation.rotator().yaw + yaw))
            entry["tofort"] = [x, y, z, yaw, pitch, v3(target)]
        elif parts[0] == "goto":
            # the player at a place of the world (m), looking along yaw and pitch
            x, y, z, yaw, pitch = (float(v) for v in parts[1:6])
            pawn.set_actor_location(ue.Vector(x * 100.0, y * 100.0, z * 100.0), False, True)
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
        elif parts[0] == "lookat":
            # the player DIST m in front of the nearest actor whose class has NAME, facing it
            name, dist = parts[1], float(parts[2])
            lift = float(parts[3]) * 100.0 if len(parts) > 3 else 20.0
            here = pawn.get_actor_location()
            found = [a for k in (ue.TikiCreature, ue.FortSoldier, ue.GiantCrab) for a in ue.GameplayStatics.get_all_actors_of_class(world, k)
                     if name in a.get_class().get_name()]
            if found:
                target = min(found, key=lambda a: a.get_actor_location().distance(here))
                loc = target.get_actor_location()
                fwd = target.get_actor_forward_vector()
                base = math.atan2(fwd.y, fwd.x)
                spot = None
                # the first clear view around it, from its front: nothing between its head and the camera
                for k in (0, 1, -1, 2, -2, 3, -3, 4):
                    a = base + k * math.pi / 4.0
                    candidate = loc + ue.Vector(math.cos(a), math.sin(a), 0.0) * (dist * 100.0)
                    seen = ue.SystemLibrary.line_trace_single(world, loc + ue.Vector(0, 0, 40), candidate + ue.Vector(0, 0, lift + 40),
                                                              ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [target, pawn],
                                                              ue.DrawDebugTrace.NONE, True)
                    if not (seen and seen.to_tuple()[0]):
                        spot = candidate
                        break
                spot = spot or (loc + ue.Vector(fwd.x, fwd.y, 0.0).normal() * (dist * 100.0))
                pawn.set_actor_location(ue.Vector(spot.x, spot.y, loc.z + lift), False, True)
                yaw = math.degrees(math.atan2(loc.y - spot.y, loc.x - spot.x))
                pitch = -math.degrees(math.atan2(lift + 40.0, dist * 100.0))
                controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=pitch, yaw=yaw))
                entry["lookat"] = [target.get_name(), [round(loc.x / 100, 1), round(loc.y / 100, 1)]]
        elif parts[0] == "bounds":
            # size of the body of the nearest actor whose class has NAME (cm)
            name = parts[1]
            here = pawn.get_actor_location()
            found = [a for k in (ue.TikiCreature, ue.FortSoldier, ue.GiantCrab) for a in ue.GameplayStatics.get_all_actors_of_class(world, k)
                     if name in a.get_class().get_name()]
            if found:
                target = min(found, key=lambda a: a.get_actor_location().distance(here))
                mesh = target.get_editor_property("mesh")
                asset = mesh.get_editor_property("skeletal_mesh_asset")
                box = asset.get_bounds() if asset else None
                world_box = mesh.bounds if hasattr(mesh, "bounds") else None
                entry["visibility"] = {"actor_hidden": target.is_hidden_ed() if hasattr(target, "is_hidden_ed") else None,
                                       "hidden_in_game": target.get_editor_property("hidden"),
                                       "mesh_visible": mesh.get_editor_property("visible"),
                                       "mesh_hidden_in_game": mesh.get_editor_property("hidden_in_game"),
                                       "owner_no_see": mesh.get_editor_property("owner_no_see"),
                                       "materials": [m.get_name() if m else None for m in mesh.get_materials()][:4],
                                       "lod": mesh.get_editor_property("forced_lod_model") if hasattr(mesh, "get_editor_property") else None}
                entry["bounds"] = [target.get_name(), asset.get_name() if asset else None,
                                   [round(v) for v in (box.box_extent.x, box.box_extent.y, box.box_extent.z)] if box else None,
                                   str(mesh.get_editor_property("relative_scale3d")),
                                   [round(v) for v in (target.get_actor_location().x / 100, target.get_actor_location().y / 100)]]
        elif parts[0] == "animof":
            # path of the anim instance of the nearest actor whose class has NAME (read its clip through the MCP)
            here = pawn.get_actor_location()
            found = [a for k in (ue.TikiCreature, ue.FortSoldier, ue.GiantCrab) for a in ue.GameplayStatics.get_all_actors_of_class(world, k)
                     if parts[1] in a.get_class().get_name()]
            if found:
                target = min(found, key=lambda a: a.get_actor_location().distance(here))
                inst = target.get_editor_property("mesh").get_anim_instance()
                entry["animof"] = [target.get_name(), inst.get_path_name() if inst else None]
        elif parts[0] == "near":
            # the mobs nearest to the player, by class
            here = pawn.get_actor_location()
            mobs = [a for k in (ue.TikiCreature, ue.FortSoldier, ue.GiantCrab) for a in ue.GameplayStatics.get_all_actors_of_class(world, k)
                    if len(parts) < 2 or parts[1] in a.get_class().get_name()]
            mobs.sort(key=lambda a: a.get_actor_location().distance(here))
            entry["near"] = [[a.get_class().get_name(), [round(v / 100.0, 1) for v in (a.get_actor_location().x, a.get_actor_location().y, a.get_actor_location().z)]]
                             for a in mobs[:8]]
        elif parts[0] == "god":
            ue.SystemLibrary.execute_console_command(world, "god", controller) if parts[1] == "1" else None
        elif parts[0] == "cmd":
            ue.SystemLibrary.execute_console_command(world, " ".join(parts[1:]), controller)
        elif parts[0] == "shot":
            ue.SystemLibrary.execute_console_command(world, "HighResShot 1600x900", controller)
        elif parts[0] == "navbuild":
            # navbuild <seconds>: over the next seconds of game time, how often the navigation mesh is being rebuilt
            # (sampled every editor tick; T-319: L_Test_Flat's navmesh made dynamic for the fort streamed at runtime)
            seconds = float(parts[1]) if len(parts) > 1 else 10.0
            state = {"samples": 0, "building": 0, "end": ue.GameplayStatics.get_time_seconds(world) + seconds}
            OUT_BUILD = OUT / "navbuild.json"

            def tick(_delta, state=state):
                game = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
                if not game or ue.GameplayStatics.get_time_seconds(game) >= state["end"]:
                    ue.unregister_slate_post_tick_callback(state["handle"])
                    OUT_BUILD.write_text(json.dumps({k: v for k, v in state.items() if k != "handle"}), encoding="utf-8")
                    return
                state["samples"] += 1
                if ue.NavigationSystemV1.is_navigation_being_built(game):
                    state["building"] += 1

            state["handle"] = ue.register_slate_post_tick_callback(tick)
            entry["navbuild"] = seconds
        elif parts[0] == "navbounds":
            # the navigation volumes of the world (m) and the navigation meshes' runtime generation (T-319)
            rows = []
            for volume in ue.GameplayStatics.get_all_actors_of_class(world, ue.NavMeshBoundsVolume):
                origin, extent = volume.get_actor_bounds(False)
                rows.append([volume.get_name(), [round((origin.x - extent.x) / 100), round((origin.y - extent.y) / 100), round((origin.z - extent.z) / 100)],
                             [round((origin.x + extent.x) / 100), round((origin.y + extent.y) / 100), round((origin.z + extent.z) / 100)]])
            meshes = []
            for navmesh in ue.GameplayStatics.get_all_actors_of_class(world, ue.RecastNavMesh):
                info = {"name": navmesh.get_name()}
                for prop in ("runtime_generation", "agent_radius", "tile_size_uu", "cell_size"):
                    try:
                        info[prop] = str(navmesh.get_editor_property(prop))
                    except Exception as error:  # noqa: BLE001
                        info[prop] = f"? {error}"[:60]
                meshes.append(info)
            entry["navbounds"] = {"volumes": rows, "navmeshes": meshes}
        elif parts[0] == "navcheck":
            # navcheck <soldier name>: that soldier's place, its AI's move status, and for every patrol post of its fort
            # whether the navigation mesh has a full path there (T-319: an officer ground against a wall for minutes)
            found = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.FortSoldier) if parts[1] in a.get_name()]
            if len(parts) > 3:
                # navcheck <name> <x m> <y m>: the one of that name nearest that place (several forts in the map)
                near = ue.Vector(float(parts[2]) * 100.0, float(parts[3]) * 100.0, 0.0)
                found.sort(key=lambda a: math.hypot(a.get_actor_location().x - near.x, a.get_actor_location().y - near.y))
            row = {"soldier": None}
            if found:
                soldier = found[0]
                here = soldier.get_actor_location()
                ai = soldier.get_controller()
                row = {"soldier": soldier.get_name(), "at": v3(here), "velocity": v3(soldier.get_velocity()),
                       "move_status": str(ai.get_move_status()) if ai and hasattr(ai, "get_move_status") else None}
                nav = ue.NavigationSystemV1
                projector = next((getattr(nav, n) for n in ("project_point_to_navigation", "k2_project_point_to_navigation")
                                  if hasattr(nav, n)), None)
                try:
                    on_nav = projector(world, here, None, None, ue.Vector(50.0, 50.0, 250.0)) if projector else None
                    row["on_navmesh"] = [bool(on_nav[1]), v3(on_nav[0])] if isinstance(on_nav, tuple) else str(on_nav)[:80]
                except Exception as error:  # noqa: BLE001
                    row["on_navmesh"] = f"? {error}"[:120]
                posts = []
                for zone in ue.GameplayStatics.get_all_actors_of_class(world, ue.FortGarrisonZone):
                    if not zone.is_inside(here) and math.hypot(zone.get_actor_location().x - here.x, zone.get_actor_location().y - here.y) > 6000.0:
                        continue
                    transform = zone.get_actor_transform()
                    for index, local in enumerate(zone.get_editor_property("patrol_points")):
                        point = transform.transform_location(local)
                        path = nav.find_path_to_location_synchronously(world, here, point)
                        points = path.get_editor_property("path_points") if path else []
                        posts.append({"zone": zone.get_name(), "post": index, "at": v3(point),
                                      "path": None if not path else {"partial": bool(path.is_partial()), "points": len(points),
                                                                      "length_m": round(path.get_path_length() / 100.0, 1),
                                                                      "end": v3(points[-1]) if points else None}})
                row["posts"] = posts
            entry.setdefault("navcheck", []).append(row)
        elif parts[0] in ("faceactor", "interact"):
            # faceactor <name> <dist m> [lift m] [side deg]: the player DIST m in front of the nearest actor whose class
            # or name has NAME (turned SIDE degrees around it), facing it; interact <name>: that actor's F (T-322)
            here = pawn.get_actor_location()
            found = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor)
                     if parts[1] in a.get_class().get_name() or parts[1] in a.get_name()]
            target = min(found, key=lambda a: a.get_actor_location().distance(here)) if found else None
            row = [parts[0], target.get_name() if target else None]
            if target and parts[0] == "faceactor":
                dist, lift = float(parts[2]) * 100.0, float(parts[3]) * 100.0 if len(parts) > 3 else 40.0
                side = math.radians(float(parts[4])) if len(parts) > 4 else 0.0
                loc, fwd = target.get_actor_location(), target.get_actor_forward_vector()
                base = math.atan2(fwd.y, fwd.x) + side
                spot = loc + ue.Vector(math.cos(base), math.sin(base), 0.0) * dist + ue.Vector(0.0, 0.0, lift)
                pawn.set_actor_location(spot, False, True)
                look = math.degrees(math.atan2(loc.y - spot.y, loc.x - spot.x))
                controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=-8.0, yaw=look))
                row += [v3(loc), round(target.get_actor_rotation().yaw, 1), v3(spot)]
            elif target:
                try:
                    target.interact(pawn)
                    row.append("interacted")
                except Exception as error:  # noqa: BLE001
                    row.append(str(error)[:160])
            entry.setdefault("actors", []).append(row)
        elif parts[0] == "burst":
            # burst <count> <interval s>: game view screenshots (HighResShot) over the next seconds, from a tick of the
            # editor, so an effect is seen at its real pace (a step takes about 3 s of game time; T-321)
            count, interval = int(parts[1]), float(parts[2])
            state = {"n": 0, "next": 0.0}

            def tick(_delta, state=state):
                game = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
                if not game or state["n"] >= count:
                    ue.unregister_slate_post_tick_callback(state["handle"])
                    return
                now = ue.GameplayStatics.get_time_seconds(game)
                if now >= state["next"]:
                    ue.SystemLibrary.execute_console_command(game, "HighResShot 1280x720")
                    state.setdefault("at", []).append(round(now, 2))
                    state["n"] += 1
                    state["next"] = now + interval

            state["handle"] = ue.register_slate_post_tick_callback(tick)
            entry["burst"] = [count, interval]
        elif parts[0] == "fx":
            # fx <system path> <ahead m> <up m> [yaw offset deg]: a Niagara system in front of the player, its +X along
            # the view turned by the offset (90: a jet across the view, as a shot of someone beside the player)
            # (EditorAssetLibrary refuses to load during a game: the plain loader)
            system = ue.load_asset(parts[1])
            ahead, up = float(parts[2]) * 100.0, float(parts[3]) * 100.0
            offset = float(parts[4]) if len(parts) > 4 else 0.0
            view = controller.get_control_rotation()
            forward = ue.Vector(math.cos(math.radians(view.yaw)), math.sin(math.radians(view.yaw)), 0.0)
            spot = pawn.get_actor_location() + forward * ahead + ue.Vector(0.0, 0.0, up)
            pitch = float(parts[5]) if len(parts) > 5 else 0.0
            component, error = None, None
            try:
                component = ue.NiagaraFunctionLibrary.spawn_system_at_location(
                    pawn, system, spot, ue.Rotator(roll=0.0, pitch=pitch, yaw=view.yaw + offset))
            except Exception as failure:  # noqa: BLE001
                error = str(failure)[:200]
            entry.setdefault("fx", []).append([parts[1].rsplit("/", 1)[-1], system.get_class().get_name() if system else None,
                                               v3(spot), component.get_name() if component else None, error])
        elif parts[0] == "zonehit":
            # a blow on the nearest living tiki or soldier, struck at a bone (zone reaction check)
            bone, damage = parts[1], float(parts[2])
            here = pawn.get_actor_location()
            kinds = [ue.TikiCreature, ue.FortSoldier]
            living = [a for k in kinds for a in ue.GameplayStatics.get_all_actors_of_class(world, k)
                      if not (a.is_dead() if hasattr(a, "is_dead") else False) and (len(parts) < 4 or parts[3] in ("gun", "far") or parts[3] in a.get_class().get_name())]
            # the blow of the gun the player holds (its musket reactions), of the player otherwise
            causer = pawn
            if "gun" in parts[3:]:
                guns = [g for k in (ue.PirateMusketWeapon, ue.MusketWeapon) for g in ue.GameplayStatics.get_all_actors_of_class(world, k)
                        if g.get_owner() == pawn or g.get_attach_parent_actor() == pawn]
                causer = guns[0] if guns else pawn
                entry["causer"] = causer.get_name()
            target = min(living, key=lambda a: a.get_actor_location().distance(here)) if living else None
            if target:
                mesh = target.get_editor_property("mesh")
                loc = mesh.get_socket_location(bone)
                # a real trace at the bone from 1 m in front (point blank: a shot this close bursts a soldier), or from the
                # player's eyes with "far" (the distance of the shot is the trace's)
                start = loc + target.get_actor_forward_vector() * 100.0
                if "far" in parts[3:]:
                    start = pawn.get_actor_location() + ue.Vector(0.0, 0.0, 60.0)
                # a shot at the bone: the capsule takes it, the game finds the bone the line passes nearest to
                # the musket's channel (Camera): the first thing struck along the line, the target's capsule here
                others = [a for k in kinds for a in ue.GameplayStatics.get_all_actors_of_class(world, k) if a != target]
                result = ue.SystemLibrary.line_trace_single(world, start, loc - target.get_actor_forward_vector() * 60.0,
                                                            ue.TraceTypeQuery.TRACE_TYPE_QUERY2, False, [pawn] + others,
                                                            ue.DrawDebugTrace.NONE, True)
                t = result.to_tuple() if result else None
                hit = result if t and t[0] and t[9] == target else None
                entry["trace"] = [t[0], t[9].get_name() if t and t[9] else None] if t else None
                if hit is not None:
                    ue.GameplayStatics.apply_point_damage(target, damage, (loc - start).normal(), hit, controller, causer, ue.DamageType)
                entry["zonehit"] = [target.get_name(), bone, damage, "struck its capsule" if hit else "missed"]
        elif parts[0] == "clips":
            out = []
            for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.TikiCreature):
                try:
                    clip = a.get_editor_property("current_clip")
                except Exception as error:  # noqa: BLE001
                    clip = f"? {error}"
                out.append([a.get_name(), clip.get_name() if hasattr(clip, "get_name") else str(clip)])
            entry["clips"] = out[:12]
        elif parts[0] == "mortal":
            # the test map spawns the player invulnerable (MonkeTestGameMode): mortal again for a death check
            pawn.set_editor_property("can_be_damaged", True)
            entry["mortal"] = pawn.get_editor_property("can_be_damaged")
        elif parts[0] in ("lodcheck", "forcelod", "freeze"):
            # lodcheck NAME: LOD, visibility and bounds of the nearest actor whose class has NAME;
            # forcelod NAME n: that mesh drawn at LOD n-1 (0: automatic); freeze NAME 1|0: its tick stopped (it holds still)
            here = pawn.get_actor_location()
            found = [a for k in (ue.TikiCreature, ue.FortSoldier, ue.GiantCrab) for a in ue.GameplayStatics.get_all_actors_of_class(world, k)
                     if parts[1] in a.get_class().get_name()]
            if found:
                target = min(found, key=lambda a: a.get_actor_location().distance(here))
                mesh = target.get_editor_property("mesh")
                if parts[0] == "forcelod":
                    mesh.set_forced_lod(int(parts[2]))
                elif parts[0] == "freeze":
                    target.set_actor_tick_enabled(parts[2] != "1")
                    mesh.set_component_tick_enabled(parts[2] != "1")
                info = {"actor": target.get_name(), "dist_m": round(target.get_actor_location().distance(here) / 100.0, 2),
                        "lods": mesh.get_num_lods() if hasattr(mesh, "get_num_lods") else None,
                        "forced": mesh.get_forced_lod() if hasattr(mesh, "get_forced_lod") else None,
                        "predicted": mesh.get_predicted_lod_level() if hasattr(mesh, "get_predicted_lod_level") else None,
                        "visible": mesh.is_visible(), "rendered": mesh.was_recently_rendered(0.2) if hasattr(mesh, "was_recently_rendered") else None,
                        "hidden_actor": target.is_hidden() if hasattr(target, "is_hidden") else None}
                origin, extent, radius = ue.SystemLibrary.get_component_bounds(mesh)
                info["bounds_origin_rel_cm"] = [round(v) for v in (origin.x - target.get_actor_location().x, origin.y - target.get_actor_location().y,
                                                                  origin.z - target.get_actor_location().z)]
                info["bounds_extent_cm"] = [round(extent.x), round(extent.y), round(extent.z)]
                info["mesh_rel"] = str(mesh.get_editor_property("relative_location"))
                for bone in ("pelvis", "head", "hand_r", "foot_l"):
                    p = mesh.get_socket_location(bone)
                    info.setdefault("bones_rel_cm", {})[bone] = [round(p.x - target.get_actor_location().x), round(p.y - target.get_actor_location().y),
                                                                round(p.z - target.get_actor_location().z)]
                info["asset"] = mesh.get_editor_property("skeletal_mesh_asset").get_name() if mesh.get_editor_property("skeletal_mesh_asset") else None
                for bone in ("root", "pelvis", "head"):
                    try:
                        t = mesh.get_socket_transform(bone, ue.RelativeTransformSpace.RTS_PARENT_BONE_SPACE)
                        c = mesh.get_socket_transform(bone, ue.RelativeTransformSpace.RTS_COMPONENT)
                        idx = mesh.get_bone_index(bone)
                        ref = mesh.get_ref_pose_position(idx) if hasattr(mesh, "get_ref_pose_position") else None
                        info.setdefault("bone_local", {})[bone] = {"t": [round(v, 3) for v in (t.translation.x, t.translation.y, t.translation.z)],
                                                                  "s": [round(v, 3) for v in (t.scale3d.x, t.scale3d.y, t.scale3d.z)],
                                                                  "comp_t": [round(v, 2) for v in (c.translation.x, c.translation.y, c.translation.z)],
                                                                  "comp_s": [round(v, 3) for v in (c.scale3d.x, c.scale3d.y, c.scale3d.z)],
                                                                  "ref": [round(v, 3) for v in (ref.x, ref.y, ref.z)] if ref else None}
                    except Exception as error:  # noqa: BLE001
                        info.setdefault("bone_local", {})[bone] = str(error)
                inst = mesh.get_anim_instance()
                info["anim_instance"] = inst.get_class().get_name() if inst else None
                entry.setdefault(parts[0], []).append(info)
        elif parts[0] == "bonesdump":
            # bonesdump NAME: every bone of the nearest actor whose class has NAME (or the player: "bonesdump player"):
            # component-space scale and place, and the reference pose place of the mesh asset
            here = pawn.get_actor_location()
            if parts[1] == "player":
                target, mesh = pawn, pawn.get_editor_property("mesh")
            else:
                found = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Character) if parts[1] in a.get_class().get_name()]
                target = min(found, key=lambda a: a.get_actor_location().distance(here)) if found else None
                mesh = target.get_editor_property("mesh") if target else None
            if mesh:
                rows = []
                limit = int(parts[2]) if len(parts) > 2 else 8
                for idx in range(min(mesh.get_num_bones(), limit)):
                    bone = mesh.get_bone_name(idx)
                    c = mesh.get_socket_transform(bone, ue.RelativeTransformSpace.RTS_COMPONENT)
                    ref = mesh.get_ref_pose_position(idx)
                    try:
                        d = mesh.get_delta_transform_from_ref_pose(bone)
                        delta = [[round(v, 2) for v in (d.translation.x, d.translation.y, d.translation.z)], [round(v, 3) for v in (d.scale3d.x, d.scale3d.y, d.scale3d.z)]]
                    except Exception as error:  # noqa: BLE001
                        delta = str(error)
                    rows.append([str(bone), [round(v, 2) for v in (c.translation.x, c.translation.y, c.translation.z)],
                                 [round(v, 3) for v in (c.scale3d.x, c.scale3d.y, c.scale3d.z)],
                                 [round(v, 3) for v in (ref.x, ref.y, ref.z)], delta])
                world_hand = mesh.get_socket_transform("hand_r", ue.RelativeTransformSpace.RTS_WORLD)
                inst = mesh.get_anim_instance()
                entry.setdefault("bonesdump", {})[target.get_name()] = {"asset": mesh.get_editor_property("skeletal_mesh_asset").get_name(),
                                                                       "anim": inst.get_class().get_name() if inst else None,
                                                                       "hand_r_world_scale": round(world_hand.scale3d.x, 3),
                                                                       "lod": mesh.get_predicted_lod_level() if hasattr(mesh, "get_predicted_lod_level") else None,
                                                                       "dist_m": round(target.get_actor_location().distance(here) / 100.0, 1), "bones": rows}
        elif parts[0] == "slot":
            # slot N: the player takes the item of slot N of its bar in hand (0 = the first)
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            if inventory:
                inventory.select_slot(int(parts[1]))
                item = inventory.get_equipped_item()
                entry["slot"] = [int(parts[1]), item.get_class().get_name() if item else None,
                                 [(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(inventory.get_bar_num())]]
        elif parts[0] == "fpstate":
            # the item in hand and what its first person look shows
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            item = inventory.get_equipped_item() if inventory else None
            info = {"item": item.get_class().get_name() if item else None, "first_person": pawn.is_first_person() if hasattr(pawn, "is_first_person") else None,
                    "active_slot": inventory.get_active_slot() if inventory else None,
                    "bar": [(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(inventory.get_slot_num())] if inventory else None}
            if item:
                for prop in ("first_person_mesh", "mesh", "static_mesh", "body_mesh"):
                    try:
                        comp = item.get_editor_property(prop)
                        info[prop] = {"visible": comp.is_visible(), "hidden_in_game": comp.get_editor_property("hidden_in_game"), "owner_no_see": comp.get_editor_property("owner_no_see"),
                                      "only_owner_see": comp.get_editor_property("only_owner_see"), "world": str(comp.get_component_location())[:60]} if comp else None
                    except Exception as error:  # noqa: BLE001
                        info[prop] = str(error)[:80]
            for comp in pawn.get_components_by_class(ue.SkeletalMeshComponent):
                info.setdefault("pawn_skel", []).append([comp.get_name(), comp.is_visible(), comp.get_editor_property("owner_no_see"), comp.get_editor_property("only_owner_see")])
            entry["fpstate"] = info
        elif parts[0] == "find":
            # find <ClassOrLabelSubstring>: actors whose class or label has it, with their place (m)
            rows = []
            for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor):
                label = a.get_actor_label() if hasattr(a, "get_actor_label") else a.get_name()
                if parts[1] in a.get_class().get_name() or parts[1] in label or parts[1] in a.get_name():
                    l = a.get_actor_location()
                    rows.append([a.get_name(), a.get_class().get_name(), [round(l.x / 100, 1), round(l.y / 100, 1), round(l.z / 100, 2)]])
            entry.setdefault("find", {})[parts[1]] = rows[:20]
        elif parts[0] == "pickup":
            # pickup <ClassOrNameSubstring>: the player takes the nearest such pickup (server interaction)
            here = pawn.get_actor_location()
            found = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor)
                     if parts[1] in a.get_class().get_name() or parts[1] in a.get_name()]
            target = min(found, key=lambda a: a.get_actor_location().distance(here)) if found else None
            result = [target.get_name() if target else None, len(found)]
            if target:
                pawn.set_actor_location(target.get_actor_location() + ue.Vector(-120.0, 0.0, 90.0), False, True)
                try:
                    result.append(target.can_interact(pawn))
                    target.interact(pawn)
                    result.append("interacted")
                except Exception as error:  # noqa: BLE001
                    result.append(str(error)[:160])
                inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
                result.append([(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(inventory.get_slot_num())] if inventory else None)
            entry.setdefault("pickup", []).append(result)
        elif parts[0] in ("fire", "reload", "inspect", "aim"):
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            item = inventory.get_equipped_item() if inventory else None
            if item and isinstance(item, ue.FirearmWeapon):
                if parts[0] == "fire":
                    item.request_fire()
                elif parts[0] == "reload":
                    item.request_reload()
                elif parts[0] == "inspect":
                    item.request_inspect()
                else:
                    # aim 1|0: the rules may refuse aiming (bAllowAiming, T-254): record what the weapon answers
                    item.set_aiming(parts[1] == "1")
                    camera = pawn.get_component_by_class(ue.CameraComponent)
                    entry.setdefault("weapon", []).append(["aiming", item.is_aiming(), "fov", camera.get_editor_property("field_of_view") if camera else None])
                entry.setdefault("weapon", []).append([parts[0], item.get_class().get_name()])
            else:
                entry.setdefault("weapon", []).append([parts[0], "no firearm in hand", item.get_class().get_name() if item else None])
        elif parts[0] in ("drop", "quickmove"):
            # drop <slot> [count] / quickmove <slot>: the player's inventory (server requests)
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            if inventory:
                if parts[0] == "drop":
                    inventory.request_drop(int(parts[1]), int(parts[2]) if len(parts) > 2 else 1)
                else:
                    inventory.request_quick_move(int(parts[1]))
                entry.setdefault("inventory", []).append([parts[0], parts[1], [(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(inventory.get_slot_num())]])
        elif parts[0] == "tracers":
            # tracers: every tracer round in flight, its streak and round (place, scale, mesh, material, visible)
            rows = []
            for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor):
                if "FirearmTracer" in a.get_class().get_name():
                    for c in a.get_components_by_class(ue.StaticMeshComponent):
                        mesh = c.get_editor_property("static_mesh")
                        mat = c.get_material(0)
                        rows.append([a.get_name(), c.get_name(), v3(c.get_world_location()), str(c.get_world_scale())[:60],
                                     mesh.get_name() if mesh else None, mat.get_name() if mat else None, c.is_visible()])
            entry["tracers"] = rows
        elif parts[0] == "itemcall":
            # itemcall <method> [args...]: a callable function of the item in hand (request_cease_fire, get_heat...)
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            item = inventory.get_equipped_item() if inventory else None
            result = None
            if item and hasattr(item, parts[1]):
                result = getattr(item, parts[1])(*[float(a) if a.replace(".", "", 1).isdigit() else a for a in parts[2:]])
            entry.setdefault("itemcall", []).append([parts[1], item.get_class().get_name() if item else None, str(result)[:80]])
        elif parts[0] == "tracerside":
            # tracerside <cm>: the player beside the first tracer in flight, looking at it side on, then a screenshot
            side_cm = float(parts[1]) if len(parts) > 1 else 350.0
            found = None
            for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor):
                if "FirearmTracer" in a.get_class().get_name():
                    parts_by_name = {c.get_name(): c for c in a.get_components_by_class(ue.StaticMeshComponent)}
                    if "Round" in parts_by_name and "Streak" in parts_by_name:
                        found = (parts_by_name["Round"].get_world_location(), parts_by_name["Streak"].get_world_location())
                        break
            if found:
                head, tail = found
                along = ue.Vector(head.x - tail.x, head.y - tail.y, 0.0)
                length = max(math.hypot(along.x, along.y), 1.0)
                ax, ay = along.x / length, along.y / length
                spot = ue.Vector(tail.x - ay * side_cm, tail.y + ax * side_cm, head.z - 40.0)
                pawn.set_actor_location(spot, False, True)
                yaw = math.degrees(math.atan2(tail.y - spot.y, tail.x - spot.x))
                controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
                ue.SystemLibrary.execute_console_command(world, "HighResShot 1600x900", controller)
            entry["tracerside"] = [v3(found[0]), v3(found[1])] if found else None
        elif parts[0] == "movestate":
            # movestate <ClassOrNameSubstring>: movement mode, velocity, gravity, ticking, base of every such character
            rows = []
            for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Character):
                if parts[1] in a.get_name():
                    m = a.get_editor_property("character_movement")
                    rows.append([a.get_name(), str(m.get_editor_property("movement_mode")), str(m.get_editor_property("velocity"))[:60],
                                 m.get_editor_property("gravity_scale"), m.is_component_tick_enabled(), m.is_active(),
                                 v3(a.get_actor_location()),
                                 a.get_attach_parent_actor().get_name() if a.get_attach_parent_actor() else None])
            entry.setdefault("movestate", []).extend(rows)
        elif parts[0] in ("launch", "kill"):
            # launch <ClassOrNameSubstring> <up cm/s> / kill <ClassOrNameSubstring>: the first living such character
            # thrown up, or killed (the case of a body killed in the air, owner 2026-10-10)
            found = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Character)
                     if parts[1] in a.get_name() and not (hasattr(a, "is_dead") and a.is_dead())]
            target = found[0] if found else None
            if target and parts[0] == "launch":
                target.launch_character(ue.Vector(0.0, 0.0, float(parts[2])), True, True)
            elif target:
                ue.GameplayStatics.apply_damage(target, 5000.0, controller, pawn, ue.DamageType)
            entry.setdefault(parts[0], []).append([target.get_name() if target else None, v3(target.get_actor_location()) if target else None])
        elif parts[0] == "crabhp":
            # crabhp <amount>: health taken straight from the nearest giant crab (its phases follow its health)
            crabs = ue.GameplayStatics.get_all_actors_of_class(world, ue.GiantCrab)
            if crabs:
                here = pawn.get_actor_location()
                crab = min(crabs, key=lambda a: a.get_actor_location().distance(here))
                crab.get_health().apply_health_damage(float(parts[1]), pawn)
                entry.setdefault("crabhp", []).append([crab.get_name(), parts[1], round(crab.get_health().get_health()), crab.get_phase()])
        elif parts[0] in ("crab", "crabhurt"):
            # crab: the giant crabs (phase, health, state, target); crabhurt <amount> <times>: blows on the nearest one
            crabs = ue.GameplayStatics.get_all_actors_of_class(world, ue.GiantCrab)
            if parts[0] == "crabhurt" and crabs:
                here = pawn.get_actor_location()
                crab = min(crabs, key=lambda a: a.get_actor_location().distance(here))
                for _ in range(int(parts[2]) if len(parts) > 2 else 1):
                    ue.GameplayStatics.apply_damage(crab, float(parts[1]), controller, pawn, ue.DamageType)
                entry.setdefault("crabhurt", []).append([crab.get_name(), parts[1:]])
            entry["crabs"] = [[c.get_name(), c.get_phase(), round(c.get_health().get_health()) if c.get_health() else None, str(c.get_state()).split(".")[-1][:20],
                               c.is_engaged(), c.get_target().get_name() if c.get_target() else None, v3(c.get_actor_location())] for c in crabs]
        elif parts[0] == "renderprobe":
            # renderprobe <ClassSubstring>: the nearest mob of the class; for each mesh component, shown or not, drawn in
            # the last 0.2 s or not, its LOD and its culling box, against the box of its bones (mobs that vanish,
            # owner 2026-10-10 "fix les bugs de dépop des mobs")
            name = parts[1]
            here = pawn.get_actor_location()
            camera = ue.GameplayStatics.get_player_camera_manager(world, 0)
            eye = camera.get_camera_location() if camera else here
            kinds = [ue.Actor] if "Shred" in name else [ue.TikiCreature, ue.FortSoldier, ue.GiantCrab]
            found = [a for k in kinds for a in ue.GameplayStatics.get_all_actors_of_class(world, k) if name in a.get_class().get_name()]
            probe = None
            if found:
                target = min(found, key=lambda a: a.get_actor_location().distance(here))
                comps = []
                for c in target.get_components_by_class(ue.PrimitiveComponent):
                    if not isinstance(c, (ue.SkeletalMeshComponent, ue.StaticMeshComponent)):
                        continue
                    origin, extent, radius = ue.SystemLibrary.get_component_bounds(c)
                    info = {"name": c.get_name(), "visible": c.is_visible(), "hidden_in_game": c.get_editor_property("hidden_in_game"),
                            "rendered": c.was_recently_rendered(0.2), "bounds_origin": v3(origin), "bounds_extent": v3(extent)}
                    if isinstance(c, ue.SkeletalMeshComponent):
                        asset = c.get_skeletal_mesh_asset()
                        info["asset"] = asset.get_name() if asset else None
                        for getter in ("get_predicted_lod_level", "get_forced_lod"):
                            if hasattr(c, getter):
                                try:
                                    info[getter] = getattr(c, getter)()
                                except Exception:  # noqa: BLE001
                                    pass
                        bones = [c.get_socket_location(b) for b in c.get_all_socket_names()]
                        if bones:
                            lo = [min(b.x for b in bones), min(b.y for b in bones), min(b.z for b in bones)]
                            hi = [max(b.x for b in bones), max(b.y for b in bones), max(b.z for b in bones)]
                            info["bones_box"] = [[round(v) for v in lo], [round(v) for v in hi]]
                            box_lo = [origin.x - extent.x, origin.y - extent.y, origin.z - extent.z]
                            box_hi = [origin.x + extent.x, origin.y + extent.y, origin.z + extent.z]
                            info["bones_outside_cm"] = round(max([0.0] + [box_lo[i] - lo[i] for i in range(3)] + [hi[i] - box_hi[i] for i in range(3)]))
                        try:
                            root = c.get_delta_transform_from_ref_pose("root")
                            info["root_scale_vs_ref"] = round(root.scale3d.x, 3)
                        except Exception:  # noqa: BLE001
                            pass
                        for key, prop in (("leader", "leader_pose_component"), ("fixed_bounds", "component_use_fixed_skel_bounds"),
                                          ("bounds_scale", "bounds_scale")):
                            try:
                                value = c.get_editor_property(prop)
                                info[key] = value.get_name() if hasattr(value, "get_name") else (round(value, 2) if isinstance(value, float) else value)
                            except Exception as error:  # noqa: BLE001
                                info[key] = f"? {error}"[:80]
                    else:
                        mesh = c.get_editor_property("static_mesh")
                        info["asset"] = mesh.get_name() if mesh else None
                    comps.append(info)
                scale = target.get_actor_scale3d()
                probe = {"actor": target.get_name(), "at": v3(target.get_actor_location()), "eye_dist_m": round(target.get_actor_location().distance(eye) / 100.0, 1),
                         "actor_scale": [round(scale.x, 3), round(scale.y, 3), round(scale.z, 3)],
                         "actor_hidden": target.get_editor_property("hidden"), "components": comps}
            entry.setdefault("renderprobe", []).append(probe)
        elif parts[0] == "health":
            health = pawn.get_component_by_class(ue.IslanderHealthComponent)
            entry["health"] = [health.get_health(), health.is_dead()] if health else None
        elif parts[0] in ("pickups", "take"):
            # pickups: every item pickup of the world with its item; take <item substring>: the player takes the nearest
            # pickup whose item has that name (its interaction, as the key F does)
            here = pawn.get_actor_location()
            rows = []
            for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.InventoryPickup):
                definition = a.get_editor_property("definition")
                rows.append([a, definition.get_name() if definition else None])
            if parts[0] == "pickups":
                entry["pickups"] = [[a.get_name(), d, v3(a.get_actor_location())] for a, d in rows]
            else:
                found = [(a, d) for a, d in rows if d and parts[1].lower() in d.lower()]
                target = min(found, key=lambda r: r[0].get_actor_location().distance(here))[0] if found else None
                result = [parts[1], target.get_name() if target else None]
                if target:
                    pawn.set_actor_location(target.get_actor_location() + ue.Vector(-120.0, 0.0, 90.0), False, True)
                    try:
                        target.interact(pawn)
                        result.append("taken")
                    except Exception as error:  # noqa: BLE001
                        result.append(str(error)[:160])
                inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
                result.append([(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(inventory.get_slot_num())] if inventory else None)
                entry.setdefault("take", []).append(result)
        elif parts[0] == "equip":
            # equip <item class substring>: the player takes in hand the first carried item whose class has it
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            found = None
            for i in range(inventory.get_slot_num() if inventory else 0):
                item = inventory.get_item(i)
                if item and parts[1].lower() in item.get_class().get_name().lower():
                    found = i
                    break
            if found is not None and inventory.get_active_slot() != found:
                inventory.select_slot(found)
            held = inventory.get_equipped_item() if inventory else None
            entry.setdefault("equip", []).append([parts[1], found, held.get_class().get_name() if held else None,
                                                  [(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(inventory.get_slot_num())] if inventory else None])
        elif parts[0] == "inject":
            # inject <action asset path> [hold s]: the player at this machine presses an Enhanced Input action (handled at
            # its next input tick), or holds it for that many seconds of game time (the torch raise, the sprint)
            action = ue.load_asset(parts[1])
            hold = float(parts[2]) if len(parts) > 2 else 0.0
            subs = sorted((c for c in ue.ObjectIterator(ue.EnhancedInputLocalPlayerSubsystem)
                           if c.get_outer() and c.get_outer().get_class().get_name() == "LocalPlayer"),
                          key=lambda c: int(c.get_outer().get_name().rsplit("_", 1)[-1]) if c.get_outer().get_name().rsplit("_", 1)[-1].isdigit() else 0)
            sub = subs[0] if subs else None
            if sub and action:
                sub.inject_input_vector_for_action(action, ue.Vector(1.0, 0.0, 0.0), [], [])
                if hold > 0.0:
                    state = {"end": ue.GameplayStatics.get_time_seconds(world) + hold}

                    def tick(_delta, state=state, sub=sub, action=action):
                        game = _local_world()
                        if not game or ue.GameplayStatics.get_time_seconds(game) >= state["end"]:
                            ue.unregister_slate_post_tick_callback(state["handle"])
                            return
                        sub.inject_input_vector_for_action(action, ue.Vector(1.0, 0.0, 0.0), [], [])

                    state["handle"] = ue.register_slate_post_tick_callback(tick)
            entry.setdefault("inject", []).append([parts[1].rsplit("/", 1)[-1], hold, bool(sub), bool(action)])
        elif parts[0] == "armsstate":
            # the procedural first person arms of the player: each arm mesh shown or not, drawn lately, place in view (T-330)
            pcm = ue.GameplayStatics.get_player_camera_manager(world, 0)
            cam_loc, cam_rot = pcm.get_camera_location(), pcm.get_camera_rotation()
            rows = []
            for comp in pawn.get_components_by_class(ue.SceneComponent):
                cls = comp.get_class().get_name()
                if "Arm" in comp.get_name() or "Arms" in cls:
                    row = [comp.get_name(), cls, comp.is_visible(), comp.get_editor_property("hidden_in_game") if hasattr(comp, "get_editor_property") else None,
                           _in_camera(comp.get_world_location(), cam_loc, cam_rot)]
                    try:
                        row.append(comp.was_recently_rendered(0.25))
                    except Exception:  # noqa: BLE001
                        pass
                    try:
                        parent = comp.get_attach_parent()
                        row.append(parent.get_name() if parent else None)
                    except Exception:  # noqa: BLE001
                        pass
                    rows.append(row)
            entry["armsstate"] = rows
        elif parts[0] == "itemrule":
            # itemrule <property> <values...>: a value of the rules of the item in hand, live (6 numbers: a transform x y z
            # pitch yaw roll; 3: a vector; 1: a number). The data asset is shared with the editor: put the values found
            # back in the C++ defaults or the asset afterwards (T-330 tuning of the first person poses)
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            item = inventory.get_equipped_item() if inventory else None
            rules = item.get_editor_property("rules") if item else None
            values = [float(v) for v in parts[2:]]
            if rules and len(values) == 6:
                rules.set_editor_property(parts[1], ue.Transform(ue.Vector(*values[:3]), ue.Rotator(roll=values[5], pitch=values[3], yaw=values[4]), ue.Vector(1, 1, 1)))
            elif rules and len(values) == 3:
                rules.set_editor_property(parts[1], ue.Vector(*values))
            elif rules and len(values) == 1:
                rules.set_editor_property(parts[1], values[0])
            entry.setdefault("itemrule", []).append([parts[1], values, str(rules.get_editor_property(parts[1]))[:120] if rules else None])
        elif parts[0] == "stamina":
            # stamina of the player (and exhausted or not), the trumpet call's phase (T-333)
            move = pawn.get_editor_property("character_movement")
            trumpet = pawn.get_component_by_class(ue.TrumpetEmoteComponent) if hasattr(ue, "TrumpetEmoteComponent") else None
            row = [round(ue.GameplayStatics.get_time_seconds(world), 2), round(move.get_stamina(), 1), move.is_exhausted(), pawn.is_first_person()]
            if trumpet:
                row.append(trumpet.is_playing())
            entry.setdefault("stamina", []).append(row)
        elif parts[0] == "lookfree":
            # the mouse turns the view again (after "look")
            controller.reset_ignore_look_input()
            entry["lookfree"] = controller.is_look_input_ignored()
        elif parts[0] == "view":
            # view fp|tp: the player's view (T-330 weapon view audit)
            want = parts[1] == "fp"
            if pawn.is_first_person() != want:
                pawn.toggle_view()
            entry.setdefault("view", []).append([parts[1], pawn.is_first_person()])
        elif parts[0] == "look":
            # look <pitch> [yaw]: the view pitch (deg, + up), the yaw kept unless given
            # the mouse of whoever sits at the machine no longer turns the view of this check
            controller.set_ignore_look_input(True)
            rot = controller.get_control_rotation()
            yaw = float(parts[2]) if len(parts) > 2 else rot.yaw
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=float(parts[1]), yaw=yaw))
        elif parts[0] == "fpvis":
            # fpvis [label]: every mesh of the item in hand and the first person meshes of the player: shown or not,
            # drawn in the last 0.2 s or not (culled), bounds in the camera's space (cm: ahead, right, up) (T-330)
            entry.setdefault("fpvis", []).append(_mesh_visibility(world, pawn, parts[1] if len(parts) > 1 else ""))
        elif parts[0] == "orbit":
            # orbit <yaw deg> <dist m> <height m> [fov]: a camera around the player (yaw 0 = in front of it, 90 = its
            # right side), looking at its chest, made the view (the next "shot" photographs the third person hold)
            yaw_off, dist, height = float(parts[1]), float(parts[2]) * 100.0, float(parts[3]) * 100.0
            fov = float(parts[4]) if len(parts) > 4 else 50.0
            base = pawn.get_actor_location()
            facing = math.radians(pawn.get_actor_rotation().yaw + yaw_off)
            spot = base + ue.Vector(math.cos(facing) * dist, math.sin(facing) * dist, height)
            aim = base + ue.Vector(0.0, 0.0, 5.0)
            look_rot = ue.MathLibrary.find_look_at_rotation(spot, aim)
            # (actors cannot be spawned from Python in a game world: the level's own camera actor is borrowed)
            cams = ue.GameplayStatics.get_all_actors_of_class(world, ue.CameraActor)
            assert cams, "no camera actor in the level to borrow"
            cam = cams[0]
            cam.set_actor_location_and_rotation(spot, look_rot, False, True)
            cam.get_editor_property("camera_component").set_field_of_view(fov)
            controller.set_view_target_with_blend(cam, 0.0)
            entry.setdefault("orbit", []).append([yaw_off, v3(spot)])
        elif parts[0] == "orbitend":
            controller.set_view_target_with_blend(pawn, 0.0)
        elif parts[0] == "move":
            # move <seconds> [sprint 0|1] [yaw offset deg] [tag]: the player walks or runs straight ahead for that many
            # seconds of game time; every frame records the view (camera manager), the capsule, the bean body and the
            # item's first person mesh in the camera's space -> Saved/T186/move-<tag>.json (T-330 / T-331 jitter)
            seconds = float(parts[1])
            sprint = len(parts) > 2 and parts[2] == "1"
            yaw_off = float(parts[3]) if len(parts) > 3 else 0.0
            tag = parts[4] if len(parts) > 4 else "run"
            subs = sorted((c for c in ue.ObjectIterator(ue.EnhancedInputLocalPlayerSubsystem)
                           if c.get_outer() and c.get_outer().get_class().get_name() == "LocalPlayer"),
                          key=lambda c: int(c.get_outer().get_name().rsplit("_", 1)[-1]) if c.get_outer().get_name().rsplit("_", 1)[-1].isdigit() else 0)
            state = {"end": ue.GameplayStatics.get_time_seconds(world) + seconds, "rows": [], "sprint": sprint,
                     "sub": subs[0] if subs else None, "action": pawn.get_editor_property("sprint_action"),
                     "dir": ue.Rotator(roll=0.0, pitch=0.0, yaw=controller.get_control_rotation().yaw + yaw_off).get_forward_vector()}

            def tick(delta, state=state, tag=tag):
                game = _local_world()
                if not game or ue.GameplayStatics.get_time_seconds(game) >= state["end"]:
                    ue.unregister_slate_post_tick_callback(state["handle"])
                    rows = state["rows"]
                    (OUT / f"move-{tag}.json").write_text(json.dumps({"sprint": state["sprint"], "summary": _move_summary(rows), "rows": rows}), encoding="utf-8")
                    return
                me = ue.GameplayStatics.get_player_pawn(game, 0)
                pcm = ue.GameplayStatics.get_player_camera_manager(game, 0)
                if not me or not pcm:
                    return
                me.add_movement_input(state["dir"], 1.0, False)
                if state["sprint"] and state["sub"] and state["action"]:
                    state["sub"].inject_input_vector_for_action(state["action"], ue.Vector(1.0, 0.0, 0.0), [], [])
                cam_loc, cam_rot = pcm.get_camera_location(), pcm.get_camera_rotation()
                row = {"t": round(ue.GameplayStatics.get_time_seconds(game), 4), "dt": round(delta, 5),
                       "cam": [round(cam_loc.x, 2), round(cam_loc.y, 2), round(cam_loc.z, 2)],
                       "rot": [round(cam_rot.pitch, 3), round(cam_rot.yaw, 3)],
                       "capsule": [round(c, 2) for c in (me.get_actor_location().x, me.get_actor_location().y, me.get_actor_location().z)],
                       "speed": round(me.get_velocity().length(), 1)}
                puppet = next((c for c in me.get_components_by_class(ue.PoseableMeshComponent) if "Puppet" in c.get_class().get_name()), None)
                if puppet:
                    head = puppet.get_socket_location("head") if puppet.does_socket_exist("head") else puppet.get_world_location()
                    row["head_cam"] = _in_camera(head, cam_loc, cam_rot)
                inventory = me.get_component_by_class(ue.IslanderInventoryComponent)
                item = inventory.get_equipped_item() if inventory else None
                if item:
                    for comp in item.get_components_by_class(ue.MeshComponent):
                        if "FirstPerson" in comp.get_name() and comp.is_visible() and not comp.get_editor_property("hidden_in_game"):
                            row.setdefault("fp", {})[comp.get_name()] = _in_camera(comp.get_world_location(), cam_loc, cam_rot)
                            # a bone at the end of the skeleton (the muzzle, a finger): the clip's pops show there (T-330)
                            if isinstance(comp, ue.SkeletalMeshComponent) and comp.get_num_bones() > 1:
                                bone = comp.get_bone_name(comp.get_num_bones() - 1)
                                row.setdefault("fp_bone", {})[comp.get_name()] = _in_camera(comp.get_socket_location(bone), cam_loc, cam_rot)
                        elif "ThirdPerson" in comp.get_name() and comp.is_visible() and not comp.get_editor_property("hidden_in_game") and puppet:
                            hand = puppet.get_socket_location("hand_r")
                            w = comp.get_world_location()
                            row["tp_from_hand"] = [round(w.x - hand.x, 2), round(w.y - hand.y, 2), round(w.z - hand.z, 2)]
                state["rows"].append(row)

            state["handle"] = ue.register_slate_post_tick_callback(tick)
            entry["move"] = [seconds, sprint, yaw_off, tag, bool(state["sub"]), bool(state["action"])]
    # what the world holds
    report = []
    for fort in forts:
        frame = fort.get_editor_property("level_transform")
        report.append({"fort": fort.get_name(), "at": v3(fort.get_actor_location()), "level_origin": v3(frame.translation),
                       "yaw": round(frame.rotation.rotator().yaw, 1), "level": fort.get_editor_property("level_name"),
                       "built": fort.is_built()})
    entry["forts"] = report
    zones = ue.GameplayStatics.get_all_actors_of_class(world, ue.FortGarrisonZone)
    entry["zones"] = [{"zone": z.get_name(), "at": v3(z.get_actor_location())} for z in zones]
    soldiers = ue.GameplayStatics.get_all_actors_of_class(world, ue.FortSoldier)
    entry["soldiers"] = [{"name": s.get_name(), "at": v3(s.get_actor_location()), "dead": s.is_dead(),
                          "inside": any(z.is_inside(s.get_actor_location()) for z in zones)} for s in soldiers]
    entry["loot_spawners"] = len(ue.GameplayStatics.get_all_actors_of_class(world, ue.LootScatterSpawner))
    marae = ue.GameplayStatics.get_all_actors_of_class(world, ue.MaraeSite)
    rules = [str(m.get_editor_property("rules").get_name()) if m.get_editor_property("rules") else None for m in marae]
    entry["marae"] = len([r for r in rules if r and "Temple" not in r])
    entry["temples"] = [v3(m.get_actor_location()) for m, r in zip(marae, rules) if r and "Temple" in r]
    entry["camps"] = len(ue.GameplayStatics.get_all_actors_of_class(world, ue.CampSite))
    entry["player"] = v3(pawn.get_actor_location())
    if forts:
        entry["player_to_fort_m"] = round(math.dist([pawn.get_actor_location().x, pawn.get_actor_location().y],
                                                   [forts[0].get_actor_location().x, forts[0].get_actor_location().y]) / 100.0)
    log = json.loads(LOG.read_text(encoding="utf-8")) if LOG.exists() else []
    log.append(entry)
    LOG.write_text(json.dumps(log, indent=1, default=str), encoding="utf-8")


main()
