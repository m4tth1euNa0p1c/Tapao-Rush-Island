"""Inventory screen keys (T-166, lot L5): E opens the Minecraft-like inventory screen, F keeps the interaction.

What it does (idempotent, assets saved):
  1. IA_InventoryScreen (boolean, consumes its key) in /Game/MonkeIsland/Weapons/Input, mapped on E in IMC_Inventory
     (priority 2, above the character's contexts).
  2. IMC_Islander_Actions: E is taken off IA_Grab (it was the cahier's lift key); F stays on IA_Grab, mapped again
     without trigger so that Started = press and Completed = release (the grab component tells a tap from a hold,
     UIslanderGrabComponent::HoldToGrabSeconds = 0.3 s). The triggers of IA_Grab itself are cleared for the same reason.
  3. BP_IslanderBean, inventory component: InventoryScreenAction = IA_InventoryScreen (bar 4 / bag 18 / 6 columns are
     the C++ defaults, tunable there).
  4. Lists every other E mapping of the project's contexts (a conflict to look at, nothing changed there).

Run with the editor built with T-166 (game target alone is not enough: the editor DLL must have the new properties):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
or from the editor Python console (Output Log > Python): exec(open(r"<this file>").read())
Editor lock: the BP and the IMC are saved, so hold the editor lock (scripts/editor_lock.py) and no PIE running.
"""
import unreal

WEAPON_INPUT = "/Game/MonkeIsland/Weapons/Input"
PLAYER_INPUT = "/Game/MonkeIsland/Player/Input"
IMC_INVENTORY = f"{WEAPON_INPUT}/IMC_Inventory"
IMC_ACTIONS = f"{PLAYER_INPUT}/IMC_Islander_Actions"
IA_GRAB = f"{PLAYER_INPUT}/IA_Grab"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
OTHER_CONTEXTS = [
    "/Game/Input/IMC_Default",
    "/Game/MonkeIsland/Player/Input/IMC_Islander_AZERTY",
    "/Game/MonkeIsland/Emotes/Input/IMC_TrumpetEmote",
    "/Game/MonkeIsland/Weapons/Input/IMC_Musket",
    "/Game/MonkeIsland/Weapons/Input/IMC_Cutlass",
    "/Game/MonkeIsland/Weapons/Input/IMC_Torch",
]

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning(f"[inventory screen] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def key(name):
    k = unreal.Key()
    k.set_editor_property("key_name", name)
    return k


def key_name(k):
    try:
        return str(k.get_editor_property("key_name"))
    except Exception:
        return str(k)


def mappings_of(imc):
    """Key mappings of a context, whatever the engine version names them."""
    for prop in ("mappings", "default_key_mappings"):
        try:
            value = imc.get_editor_property(prop)
        except Exception:
            continue
        if prop == "default_key_mappings":
            try:
                value = value.get_editor_property("mappings")
            except Exception:
                continue
        return list(value)
    return []


def setup_screen_action():
    action = load(f"{WEAPON_INPUT}/IA_InventoryScreen") or tools.create_asset(
        "IA_InventoryScreen", WEAPON_INPUT, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    action.set_editor_property("consume_input", True)
    eal.save_loaded_asset(action)

    imc = load(IMC_INVENTORY)
    if not imc:
        log(f"{IMC_INVENTORY} missing: E not mapped")
        return action
    imc.unmap_all_keys_from_action(action)
    imc.map_key(action, key("E"))
    eal.save_loaded_asset(imc)
    log("IMC_Inventory: E -> IA_InventoryScreen")
    return action


def free_e_from_grab():
    grab = load(IA_GRAB)
    imc = load(IMC_ACTIONS)
    if not grab or not imc:
        log(f"{IA_GRAB} or {IMC_ACTIONS} missing: grab keys untouched")
        return
    # press / release must reach the code as they are (tap or hold is decided in C++)
    grab.set_editor_property("triggers", [])
    eal.save_loaded_asset(grab)
    before = [key_name(m.get_editor_property("key")) for m in mappings_of(imc) if m.get_editor_property("action") == grab]
    imc.unmap_key(grab, key("E"))
    imc.unmap_key(grab, key("F"))
    imc.map_key(grab, key("F"))
    eal.save_loaded_asset(imc)
    after = [key_name(m.get_editor_property("key")) for m in mappings_of(imc) if m.get_editor_property("action") == grab]
    log(f"IA_Grab keys: {before} -> {after} (triggers cleared)")


def wire_player(action):
    player = load(PLAYER_BP)
    if not player:
        log(f"{PLAYER_BP} missing")
        return
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    component = None
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(player):
        obj = sdl.get_object(sdl.get_data(handle))
        if obj and isinstance(obj, unreal.IslanderInventoryComponent):
            component = obj
    if not component:
        log("BP_IslanderBean has no IslanderInventoryComponent: nothing wired")
        return
    component.set_editor_property("inventory_screen_action", action)
    if not component.get_editor_property("inventory_mapping_context"):
        component.set_editor_property("inventory_mapping_context", load(IMC_INVENTORY))
    unreal.BlueprintEditorLibrary.compile_blueprint(player)
    eal.save_loaded_asset(player)
    log("BP_IslanderBean: InventoryScreenAction = IA_InventoryScreen "
        f"(bar {component.get_editor_property('slot_count')}, bag {component.get_editor_property('bag_slot_count')})")


def report_other_e():
    for path in OTHER_CONTEXTS + [IMC_ACTIONS, IMC_INVENTORY]:
        imc = load(path)
        if not imc:
            continue
        for m in mappings_of(imc):
            if key_name(m.get_editor_property("key")) == "E":
                action = m.get_editor_property("action")
                name = action.get_name() if action else "?"
                if name != "IA_InventoryScreen":
                    log(f"E still mapped in {path.rsplit('/', 1)[-1]} -> {name} (check the conflict)")


def main():
    action = setup_screen_action()
    free_e_from_grab()
    wire_player(action)
    report_other_e()


main()
