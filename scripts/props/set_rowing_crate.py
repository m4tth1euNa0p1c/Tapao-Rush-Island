"""T-080: plugs the rowing crate (SM_Barque_RowingCrate, from the maquette bean-anim-library) into DA_BoatRules.
Editor commandlet (editor closed). Only RowingCrateMesh is written: the other values of the DA stay as saved."""
import unreal

rules = unreal.load_asset("/Game/MonkeIsland/Boat/Data/DA_BoatRules") or unreal.load_asset("/Game/MonkeIsland/Boat/DA_BoatRules")
crate = unreal.load_asset("/Game/MonkeIsland/Boat/Meshes/SM_Barque_RowingCrate")
if rules and crate:
    rules.set_editor_property("rowing_crate_mesh", crate)
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    unreal.log_warning(f"[crate] {rules.get_path_name()} -> {crate.get_name()}, MaxRowSpeed kept {rules.get_editor_property('max_row_speed')}")
else:
    unreal.log_error(f"[crate] missing rules={rules} crate={crate}")
