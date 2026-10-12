"""Two-player checks of the network wave of 2026-10-11 (host + one client in the same editor process, PIE as a listen
server: action "mpsettings" first). Runs from scripts/editor/pie_session.py (action "mpcheck") during a game this session
started. Reads monke_island/Saved/T186/mp-step.txt (steps separated by ";"), appends what it sees to mp-check.json.

Worlds: "host" is the listen server's world (UEDPIE_0), "client" the client's own world (UEDPIE_1). Moves are made on the
server (the authority), commands and injected keys in the world of the player who types them.
  worlds                        - the PIE worlds, their players and pawns (always appended)
  cmd <host|client> <command>   - a console command typed by that player (its own controller, so RPCs go the real way)
  move <host|client> x y z yaw  - the server moves that player's pawn (cm, world)
  tohelm <host|client>          - the server puts that player at the galleon's wheel
  ashore <host|client>          - the server puts that player on the island (near the village spawner, falls to the ground)
  inject <host|client> <action asset path> - that player presses an Enhanced Input action (one press, next input tick)
  give <host|client> <name-in-pickup-or-definition> - the server makes that player pick up the nearest such pickup
  slot <host|client> <n>        - the server takes slot n of that player's bar in hand
  drop <host|client>            - the server drops the item in that player's hand
  items                         - every firearm in the server world: carrier, loaded rounds when readable
  pawns                         - both players: location in the server world and in the client world, ragdoll / KO
  launch <host|client> vx vy vz - the server launches that player (cm/s), knocked down
"""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186"
STEP = OUT / "mp-step.txt"
LOG = OUT / "mp-check.json"


def v3(v):
    return [round(v.x), round(v.y), round(v.z)]


def pie_worlds():
    """The host's world (the editor's game world: the first PIE instance) and the client's world of the same map (the
    level instances streamed in, e.g. the fort, are worlds too: UEDPIE_0_LI_FortFrench_..., never taken)"""
    worlds = {}
    host = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
    if host:
        worlds["host"] = host
        for world in ue.ObjectIterator(ue.World):
            if "UEDPIE_1_" in world.get_path_name() and world.get_name() == host.get_name():
                worlds["client"] = world
                break
    return worlds


def server_pawn(worlds, who):
    """The pawn of a player in the server world: index 0 is the host, the other player is the client"""
    server = worlds["host"]
    pawns = [p for p in ue.GameplayStatics.get_all_actors_of_class(server, ue.Character) if p.get_controller()
             and isinstance(p.get_controller(), ue.PlayerController)]
    for pawn in pawns:
        local = pawn.is_locally_controlled()
        if (who == "host") == local:
            return pawn
    return None


def player_state_of(pawn):
    """The pawn's player state, or None (APawn::GetPlayerState is not a Python method in 5.8)"""
    for read in (lambda: pawn.get_editor_property("player_state"), lambda: pawn.player_state):
        try:
            return read()
        except Exception:  # noqa: BLE001
            continue
    return None


def local_controller(worlds, who):
    return ue.GameplayStatics.get_player_controller(worlds[who], 0)


def main():
    worlds = pie_worlds()
    assert "host" in worlds and "client" in worlds, f"two PIE worlds expected, found {list(worlds)}"
    steps = [s.strip() for s in (STEP.read_text(encoding="utf-8") if STEP.exists() else "worlds").split(";") if s.strip()]
    entry = {"steps": steps, "at": dt.datetime.now(dt.timezone.utc).isoformat()}
    server = worlds["host"]
    for step in steps:
        parts = step.split()
        verb = parts[0]
        if verb == "cmd":
            who = parts[1]
            ue.SystemLibrary.execute_console_command(worlds[who], " ".join(parts[2:]), local_controller(worlds, who))
            entry.setdefault("cmd", []).append([who, " ".join(parts[2:])])
        elif verb == "move":
            pawn = server_pawn(worlds, parts[1])
            x, y, z, yaw = (float(v) for v in parts[2:6])
            pawn.set_actor_location(ue.Vector(x, y, z), False, True)
            pawn.set_actor_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=yaw), True)
            entry.setdefault("move", []).append([parts[1], v3(pawn.get_actor_location())])
        elif verb in ("tohelm", "ashore"):
            pawn = server_pawn(worlds, parts[1])
            if verb == "tohelm":
                galleon = next(iter(ue.GameplayStatics.get_all_actors_of_class(server, ue.GalleonActor)), None)
                assert galleon, "no galleon"
                hull = next((c for c in galleon.get_components_by_class(ue.StaticMeshComponent) if c.does_socket_exist("helm")), None)
                assert hull, "no helm socket"
                spot = hull.get_socket_location("helm") + ue.Vector(0.0, 0.0, 90.0)
            else:
                anchor = next(iter(ue.GameplayStatics.get_all_actors_of_class(server, ue.VillageSpawner)), None) if hasattr(ue, "VillageSpawner") else None
                spot = (anchor.get_actor_location() if anchor else ue.Vector(0.0, 0.0, 0.0)) + ue.Vector(0.0, 0.0, 800.0)
            pawn.set_actor_location(spot, False, True)
            entry.setdefault(verb, []).append([parts[1], v3(spot)])
        elif verb == "inject":
            # inject <host|client> <action asset path>: that player's Enhanced Input subsystem gets the action, handled at
            # its next input tick inside its own world (the way a key press goes; a call from here would not: a server
            # RPC called outside the client world's tick ran on the client's copy, 2026-10-11)
            who, path = parts[1], parts[2]
            action = ue.load_asset(path)
            controller = local_controller(worlds, who)
            # the local player's controller and game instance are out of reach from Python (local players belong to the
            # engine): the PIE instances create theirs in order, the host's first (LocalPlayer_1), the client's next
            subs = sorted((c for c in ue.ObjectIterator(ue.EnhancedInputLocalPlayerSubsystem)
                           if c.get_outer() and c.get_outer().get_class().get_name() == "LocalPlayer"),
                          key=lambda c: int(c.get_outer().get_name().rsplit("_", 1)[-1]) if c.get_outer().get_name().rsplit("_", 1)[-1].isdigit() else 0)
            sub = (subs[0] if who == "host" else subs[-1]) if len(subs) >= 2 else None
            result = None
            if sub and action:
                sub.inject_input_vector_for_action(action, ue.Vector(1.0, 0.0, 0.0), [], [])
                result = "injected"
            entry.setdefault("inject", []).append([who, path.rsplit("/", 1)[-1], result or ("no subsystem" if not sub else "no action")])
        elif verb == "use":
            # use <host|client> <method>: that player's own copy of the item in its hand calls METHOD (request_fire,
            # request_reload...): on the client it goes to the server the way its input does (RPC through its pawn)
            who, method = parts[1], parts[2]
            world_of = worlds[who]
            mine = [p for p in ue.GameplayStatics.get_all_actors_of_class(world_of, ue.Character)
                    if p.is_locally_controlled() and p.get_component_by_class(ue.IslanderInventoryComponent)]
            item = mine[0].get_component_by_class(ue.IslanderInventoryComponent).get_equipped_item() if mine else None
            result = None
            if item and hasattr(item, method):
                result = str(getattr(item, method)())
            entry.setdefault("use", []).append([who, item.get_name() if item else None, method, result])
        elif verb == "give":
            who, name = parts[1], parts[2]
            pawn = server_pawn(worlds, who)
            here = pawn.get_actor_location()
            found = []
            for pickup in ue.GameplayStatics.get_all_actors_of_class(server, ue.InventoryPickup):
                definition = pickup.get_editor_property("definition")
                label = (definition.get_name() if definition else "") + " " + pickup.get_name()
                if name.lower() in label.lower() and not pickup.is_claimed():
                    found.append(pickup)
            target = min(found, key=lambda a: a.get_actor_location().distance(here)) if found else None
            result = None
            if target:
                pawn.set_actor_location(target.get_actor_location() + ue.Vector(-80.0, 0.0, 60.0), False, True)
                result = target.interact(pawn) if hasattr(target, "interact") else "no interact"
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            bar = [(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(inventory.get_bar_num())]
            entry.setdefault("give", []).append([who, target.get_name() if target else None, str(result), bar])
        elif verb == "slot":
            pawn = server_pawn(worlds, parts[1])
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            inventory.select_slot(int(parts[2]))
            item = inventory.get_equipped_item()
            entry.setdefault("slot", []).append([parts[1], int(parts[2]), item.get_name() if item else None])
        elif verb == "drop":
            pawn = server_pawn(worlds, parts[1])
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            result = None
            for name in ("drop_equipped_item", "drop_active", "request_drop", "drop_active_item"):
                if hasattr(inventory, name):
                    result = [name, str(getattr(inventory, name)())]
                    break
            entry.setdefault("drop", []).append([parts[1], result])
        elif verb == "launch":
            pawn = server_pawn(worlds, parts[1])
            vx, vy, vz = (float(v) for v in parts[2:5])
            pawn.launch_character(ue.Vector(vx, vy, vz), True, True)
            if hasattr(pawn, "start_ragdoll"):
                pawn.start_ragdoll(ue.Vector(vx, vy, vz))
            entry.setdefault("launch", []).append([parts[1], [vx, vy, vz]])
        elif verb == "inputsubs":
            # inputsubs: the Enhanced Input subsystems of the local players and how to reach their controllers (debug)
            rows = []
            for candidate in ue.ObjectIterator(ue.EnhancedInputLocalPlayerSubsystem):
                outer = candidate.get_outer()
                row = [candidate.get_name(), outer.get_name() if outer else None, outer.get_class().get_name() if outer else None]
                for getter in ("player_controller", "PlayerController"):
                    try:
                        pc = outer.get_editor_property(getter)
                        row.append([getter, pc.get_name() if pc else None, pc.get_world().get_path_name() if pc else None])
                        break
                    except Exception as error:  # noqa: BLE001
                        row.append([getter, str(error)[:60]])
                row.append([n for n in dir(outer) if "controller" in n.lower()][:6] if outer else None)
                rows.append(row)
            entry["inputsubs"] = rows
            entry["controllers"] = {w: local_controller(worlds, w).get_name() for w in worlds}
        elif verb == "bar":
            # bar <host|client>: that player's bar on the server and the item in its hand
            pawn = server_pawn(worlds, parts[1])
            inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
            bar = [(inventory.get_item(i).get_name() if inventory.get_item(i) else None) for i in range(inventory.get_bar_num())]
            item = inventory.get_equipped_item()
            entry.setdefault("bar", []).append([parts[1], bar, item.get_name() if item else None])
        elif verb == "dormancy":
            # dormancy <name>: in the server world, the network dormancy of the actors whose name or class has NAME and
            # their place (T-316: objects at rest stop replicating)
            rows = []
            for actor in ue.GameplayStatics.get_all_actors_of_class(server, ue.Actor):
                if parts[1] in actor.get_name() or parts[1] in actor.get_class().get_name():
                    try:
                        dormancy = str(actor.get_editor_property("net_dormancy")).split(".")[-1]
                    except Exception as error:  # noqa: BLE001
                        dormancy = f"? {error}"[:40]
                    rows.append([actor.get_name(), dormancy, v3(actor.get_actor_location())])
            entry.setdefault("dormancy", []).append([parts[1], rows[:40]])
        elif verb == "items":
            rows = []
            for weapon in ue.GameplayStatics.get_all_actors_of_class(server, ue.FirearmWeapon):
                owner = weapon.get_owner()
                row = {"item": weapon.get_name(), "carrier": owner.get_name() if owner else None}
                for getter in ("get_loaded_rounds", "get_rounds_loaded", "get_ammo_loaded", "is_loaded", "get_loaded"):
                    if hasattr(weapon, getter):
                        try:
                            row[getter] = getattr(weapon, getter)()
                        except Exception:  # noqa: BLE001
                            pass
                rows.append(row)
            entry["items"] = rows
        elif verb == "pawns":
            rows = {}
            for who in ("host", "client"):
                pawn = server_pawn(worlds, who)
                row = {"server": v3(pawn.get_actor_location()) if pawn else None}
                if pawn and hasattr(pawn, "is_ragdoll"):
                    row["server_ragdoll"] = pawn.is_ragdoll()
                rows[who] = row
            # the client's own view of both pawns
            client = worlds["client"]
            for pawn in ue.GameplayStatics.get_all_actors_of_class(client, ue.Character):
                if not player_state_of(pawn):
                    continue
                key = "client" if pawn.is_locally_controlled() else "host"
                rows.setdefault(key, {})["in_client_world"] = v3(pawn.get_actor_location())
                if hasattr(pawn, "is_ragdoll"):
                    rows[key]["client_ragdoll"] = pawn.is_ragdoll()
            entry["pawns"] = rows
    entry["worlds"] = {k: w.get_path_name() for k, w in worlds.items()}
    log = json.loads(LOG.read_text(encoding="utf-8")) if LOG.exists() else []
    log.append(entry)
    LOG.write_text(json.dumps(log, indent=1, default=str), encoding="utf-8")


main()
