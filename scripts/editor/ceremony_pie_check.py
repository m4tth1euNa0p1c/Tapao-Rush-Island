"""PIE check of the flag ceremony and of the villagers of the houses (T-194, T-195), in steps, during a game started on
L_Test_Flat. Every step reads monke_island/Saved/FlagCeremony/pie-step.txt (written by the caller) and appends what it
sees to pie-check.json:
  place  - the player beside the flag site of Uo-mura (2.5 m east of the stones), the villagers of the houses counted
  start  - AFlagCeremonySite::TryStart for the player (the server path of F), its phase and progress
  look   - phase, progress, seconds left, the villagers and their states, the relics carried
Runs in the editor: scripts/editor/lead_session_t186.py, action "ceremonycheck" (a game running, started by this session).
"""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/FlagCeremony"
STEP = OUT / "pie-step.txt"
LOG = OUT / "pie-check.json"


def game_world():
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
    assert world, "no game running"
    return world


def site_of(world):
    sites = ue.GameplayStatics.get_all_actors_of_class(world, ue.FlagCeremonySite)
    assert sites, "no flag site in the game (DA_VillageRules.FlagSiteClass, village built?)"
    return sites[0]


def villagers(world):
    out = []
    for actor in ue.GameplayStatics.get_all_actors_of_class(world, ue.TikiVillager):
        relic = actor.get_carried_relic()
        out.append({"name": actor.get_name(), "state": str(actor.get_state()).split(".")[-1],
                    "location": [round(v) for v in (actor.get_actor_location().x, actor.get_actor_location().y, actor.get_actor_location().z)],
                    "relic": relic.get_name() if relic else None})
    return out


def main():
    world = game_world()
    step = STEP.read_text(encoding="utf-8").strip() if STEP.exists() else "look"
    site = site_of(world)
    pawn = ue.GameplayStatics.get_player_pawn(world, 0)
    assert pawn, "no player pawn"
    entry = {"step": step, "at": dt.datetime.now(dt.timezone.utc).isoformat(), "site": site.get_name(),
             "site_location": [round(site.get_actor_location().x), round(site.get_actor_location().y), round(site.get_actor_location().z)]}
    if step == "director":
        # L_Test_Flat has no tiki director (only LI_ExpeditionCore does): one for this game only, as setup_fragment_map.py
        G = "/Game/MonkeIsland/"
        here = ue.Transform(location=ue.Vector(-30000.0, 0.0, 300.0))
        director = ue.GameplayStatics.begin_deferred_actor_spawn_from_class(world, ue.TikiDirector, here)
        director.set_editor_property("rules", ue.load_asset(G + "Characters/Data/DA_TikiRules"))
        director.set_editor_property("villager_class", ue.load_class(None, G + "Characters/Villager/Blueprints/BP_TikiVillager.BP_TikiVillager_C"))
        director.set_editor_property("warrior_classes", [ue.load_class(None, G + "Characters/CocoWarrior/Blueprints/BP_TikiCocoWarrior.BP_TikiCocoWarrior_C")])
        director.set_editor_property("coconut_class", ue.load_class(None, G + "Items/Blueprints/BP_CoconutPickup.BP_CoconutPickup_C"))
        director.set_editor_property("village_radius", 5000.0)
        ue.GameplayStatics.finish_spawning_actor(director, here)
        entry["director"] = director.get_name()
    if step == "place":
        # 4 m east of the stones (in the area), looking at them
        spot = site.get_actor_location() + ue.Vector(400.0, 0.0, 120.0)
        pawn.set_actor_location(spot, False, True)
        pawn.set_actor_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=180.0), True)
        controller = ue.GameplayStatics.get_player_controller(world, 0)
        if controller:
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=-6.0, yaw=180.0))
        entry["pawn"] = [round(spot.x), round(spot.y), round(spot.z)]
    elif step.startswith("move"):
        # move <dx> <dy> from the stones (cm), on foot
        parts = step.split()
        spot = site.get_actor_location() + ue.Vector(float(parts[1]), float(parts[2]), 120.0)
        pawn.set_actor_location(spot, False, True)
        entry["pawn"] = [round(spot.x), round(spot.y), round(spot.z)]
    elif step.startswith("aim"):
        # aim <yaw> <pitch>: turn the view without moving
        parts = step.split()
        controller = ue.GameplayStatics.get_player_controller(world, 0)
        controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=float(parts[2]), yaw=float(parts[1])))
        entry["aim"] = parts[1:]
    elif step == "openinv":
        # the inventory key of the local player, as if pressed (IA_InventoryScreen)
        controller = ue.GameplayStatics.get_player_controller(world, 0)
        local = controller.get_local_player() if hasattr(controller, "get_local_player") else None
        subsystem = ue.SubsystemBlueprintLibrary.get_local_player_subsystem(controller, ue.EnhancedInputLocalPlayerSubsystem)
        action = ue.load_asset("/Game/MonkeIsland/Weapons/Input/IA_InventoryScreen")
        value = ue.InputActionValue()
        subsystem.inject_input_for_action(action, value, [], [])
        entry["openinv"] = True
    elif step.startswith("fp "):
        # fp x y z pitch yaw roll fov: first person framing of the musket, live (DA_PirateMusketRules, not saved)
        v = [float(x) for x in step.split()[1:]]
        rules = ue.load_asset("/Game/MonkeIsland/Weapons/Musket/DA_PirateMusketRules")
        rules.set_editor_property("hip_grip_location", ue.Vector(v[0], v[1], v[2]))
        rules.set_editor_property("hip_rotation", ue.Rotator(roll=v[5], pitch=v[3], yaw=v[4]))
        if len(v) > 6:
            rules.set_editor_property("first_person_field_of_view", v[6])
        entry["fp"] = v
    elif step == "fpread":
        rules = ue.load_asset("/Game/MonkeIsland/Weapons/Musket/DA_PirateMusketRules")
        entry["fpread"] = {k: str(rules.get_editor_property(k)) for k in ("hip_grip_location", "hip_rotation", "aim_muzzle_location",
            "aim_rotation", "first_person_field_of_view", "aim_field_of_view", "align_first_person_to_view", "first_person_view_location",
            "first_person_view_rotation")}
    elif step.startswith("slot "):
        inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
        inventory.select_slot(int(step.split()[1]))
        entry["slot"] = int(step.split()[1])
    elif step == "inv":
        inventory = pawn.get_component_by_class(ue.IslanderInventoryComponent)
        entry["inventory"] = {"slot_count": inventory.get_editor_property("slot_count"),
                              "bag": inventory.get_editor_property("bag_slot_count"),
                              "actions": [a.get_name() if a else None for a in inventory.get_editor_property("slot_actions")],
                              "context": str(inventory.get_editor_property("inventory_mapping_context").get_name()),
                              "starting": [d.get_name() if d else None for d in inventory.get_editor_property("starting_items")],
                              "items": [(inventory.get_item(i).get_class().get_name() if inventory.get_item(i) else None) for i in range(6)]}
    elif step == "shot":
        # a picture of the game view (Saved/Screenshots), not of the editor window
        ue.SystemLibrary.execute_console_command(world, "HighResShot 1600x900")
        entry["shot"] = "requested"
    elif step == "start":
        entry["started"] = bool(site.try_start(pawn))
    entry["phase"] = str(site.get_phase()).split(".")[-1]
    entry["progress"] = round(site.get_progress(), 3)
    entry["seconds_left"] = round(site.get_seconds_left(), 1)
    entry["villagers"] = villagers(world)
    here = pawn.get_actor_location()
    relics = []
    for actor in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor):
        if actor.get_class().get_name().startswith("BP_TidalRelic"):
            loc = actor.get_actor_location()
            relics.append({"name": actor.get_name(), "to_player_cm": round(((loc.x - here.x) ** 2 + (loc.y - here.y) ** 2) ** 0.5),
                           "z": round(loc.z), "attached": bool(actor.get_attach_parent_actor())})
    entry["relics"] = sorted(relics, key=lambda r: r["to_player_cm"])
    entry["pawn_now"] = [round(here.x), round(here.y), round(here.z)]
    log = json.loads(LOG.read_text(encoding="utf-8")) if LOG.exists() else []
    log.append(entry)
    OUT.mkdir(parents=True, exist_ok=True)
    LOG.write_text(json.dumps(log, indent=1), encoding="utf-8")
    ue.log(f"CEREMONY_CHECK {step} {entry['phase']} {entry['progress']}")


if __name__ == "__main__":
    main()
