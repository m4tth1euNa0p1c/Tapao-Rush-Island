"""Drop key of the inventory (T-157, lot L5): IA_DropItem on G in IMC_Inventory, wired on the inventory component of
BP_IslanderBean (DropItemAction, InventoryMappingPriority 2 so G is consumed here before IA_Drop of the character).

The inventory component lets go of a lifted physics object first (what IA_Drop did), and drops the item in hand
otherwise, so nothing of the cahier's G is lost.

Editor commandlet (editor closed, editor target built with the T-157 code):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
or from the editor Python console (Output Log > Python): exec(open(r"<this file>").read())
Idempotent: the G mapping of IA_DropItem is replaced, the other mappings of IMC_Inventory (keys 1 to 4) are kept.
"""
import unreal

INPUT = "/Game/MonkeIsland/Weapons/Input"
IMC = f"{INPUT}/IMC_Inventory"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
PRIORITY = 2

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning(f"[inventory] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def key(name):
    k = unreal.Key()
    k.set_editor_property("key_name", name)
    return k


def main():
    action = load(f"{INPUT}/IA_DropItem") or tools.create_asset("IA_DropItem", INPUT, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    # G must not reach IA_Drop of the character as well
    action.set_editor_property("consume_input", True)
    eal.save_loaded_asset(action)

    imc = load(IMC)
    if not imc:
        log(f"{IMC} missing: nothing done")
        return
    imc.unmap_all_keys_from_action(action)
    imc.map_key(action, key("G"))
    eal.save_loaded_asset(imc)
    log("IMC_Inventory: G -> IA_DropItem")

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
    component.set_editor_property("drop_item_action", action)
    component.set_editor_property("inventory_mapping_priority", PRIORITY)
    if not component.get_editor_property("inventory_mapping_context"):
        component.set_editor_property("inventory_mapping_context", imc)
    unreal.BlueprintEditorLibrary.compile_blueprint(player)
    eal.save_loaded_asset(player)
    log(f"BP_IslanderBean: DropItemAction = IA_DropItem, InventoryMappingPriority = {PRIORITY}")


main()
