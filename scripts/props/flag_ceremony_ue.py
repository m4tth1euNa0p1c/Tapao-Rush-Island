"""Flag ceremony of the villages (owner requests 2026-10-07), in Unreal: the red circle of T-191 gives way to a small
pile of stones where F raises the British flag for a minute while the bean sounds the trumpet.

- imports art/export/Props/FlagCeremony (scripts/props/flag_ceremony.py): SM_FlagCeremony_Pole (UCX box) and
  SM_FlagCeremony_Flag (the Union Flag in flat cloth layers), with MIs of M_Bean_Master for their slots;
- IA_FlagCeremony (a copy of IA_TrumpetEmote: boolean, consumes its key) on F in IMC_FlagCeremony, bound by
  UTrumpetEmoteComponent (DA_TrumpetEmoteRules.CeremonyAction);
- DA_FlagCeremonyRules (UFlagCeremonyRules): SM_Temple_Debris_StonePile for the stones, the pole and the flag, the
  mapping; DA_VillageRules gets the site class (AFlagCeremonySite) and these rules: the village spawner puts one site
  on the square of every pacifiable village of the fragment played;
- L_Test_Flat: the red ring of T-191 (36 segments) and its circle actor go (the class ATrumpetCircle is removed);
  the generated village Uo-mura gets its site from the spawner at the start of the game.
Runs in the editor: scripts/editor/lead_session_t186.py, action "ceremony" (editor lock held, no game running).
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

SRC = ROOT / "art/export/Props/FlagCeremony"
OUT = ROOT / "monke_island/Saved/FlagCeremony"
BASE = "/Game/MonkeIsland/Environment/FlagCeremony"
MESHES = BASE + "/Meshes"
MATS = BASE + "/Materials"
RULES = BASE + "/DA_FlagCeremonyRules"
STONES = "/Game/MonkeIsland/Environment/TempleReliquary/Meshes/SM_Temple_Debris_StonePile"
BEAN_MATS = "/Game/MonkeIsland/Characters/Bean/Materials"
INPUT = "/Game/MonkeIsland/Emotes/Input"
TRUMPET_ACTION = INPUT + "/IA_TrumpetEmote"
CEREMONY_ACTION = INPUT + "/IA_FlagCeremony"
CEREMONY_IMC = INPUT + "/IMC_FlagCeremony"
TRUMPET_RULES = "/Game/MonkeIsland/Emotes/Trumpet/DA_TrumpetEmoteRules"
VILLAGE_RULES = "/Game/MonkeIsland/Environment/Village/Data/DA_VillageRules"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
OLD_CIRCLE = "TestFlat_TrumpetCircle"
# slot -> (MI of the bean copied, sRGB colour, roughness, metallic)
MATERIALS = {
    "M_Flag_Blue": ("MI_Bean_Navy", (0x01, 0x21, 0x69), 0.85, 0.0),
    "M_Flag_White": ("MI_Bean_Cream", (0xF4, 0xF4, 0xF0), 0.85, 0.0),
    "M_Flag_Red": ("MI_Bean_CoatRed", (0xC8, 0x10, 0x2E), 0.85, 0.0),
    "M_FlagPole_Wood": ("MI_Bean_Leather", (0x6B, 0x4A, 0x2E), 0.70, 0.0),
    "M_FlagPole_Gold": ("MI_Bean_Brass", (0xC9, 0x9A, 0x3E), 0.30, 1.0),
    "M_FlagPole_Rope": ("MI_Bean_Cream", (0xD8, 0xC9, 0xA3), 0.90, 0.0),
}
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def linear(c):
    c = c / 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def material(slot):
    source, colour, rough, metal = MATERIALS[slot]
    path = f"{MATS}/MI_{slot[2:]}"
    mi = kitlib.load(path) or EAL.duplicate_asset(f"{BEAN_MATS}/{source}", path)
    assert mi, path
    mel = ue.MaterialEditingLibrary
    mel.set_material_instance_vector_parameter_value(mi, "BaseColor", ue.LinearColor(*(linear(c) for c in colour), 1.0))
    mel.set_material_instance_scalar_parameter_value(mi, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(mi, "Metallic", metal)
    mel.set_material_instance_scalar_parameter_value(mi, "DetailStrength", 0.0)
    kitlib.save(mi)
    return mi


def import_mesh(name):
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("import_uniform_scale", 1.0)
    task = ue.AssetImportTask()
    task.filename = str(SRC / (name + ".fbx"))
    task.destination_path = MESHES
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.options = options
    task.save = False
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        TOOLS.import_asset_tasks([task])
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
    mesh = kitlib.load(f"{MESHES}/{name}")
    assert mesh, name
    # set_editor_property("static_materials") did not hold after the import (WorldGridMaterial left): one slot at a time
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        mesh.set_material(index, material(str(slot.get_editor_property("material_slot_name"))))
    kitlib.save(mesh)
    return mesh


def key(name):
    k = ue.Key()
    k.set_editor_property("key_name", name)
    return k


def ceremony_input():
    action = kitlib.load(CEREMONY_ACTION) or EAL.duplicate_asset(TRUMPET_ACTION, CEREMONY_ACTION)
    assert action, CEREMONY_ACTION
    action.set_editor_property("consume_input", True)
    kitlib.save(action)
    imc = kitlib.load(CEREMONY_IMC) or TOOLS.create_asset("IMC_FlagCeremony", INPUT, ue.InputMappingContext, ue.InputMappingContext_Factory())
    assert imc, CEREMONY_IMC
    imc.unmap_all()
    imc.map_key(action, key("F"))
    kitlib.save(imc)
    trumpet = kitlib.load(TRUMPET_RULES)
    assert trumpet, TRUMPET_RULES
    trumpet.set_editor_property("ceremony_action", action)
    kitlib.save(trumpet)
    return action, imc


def rules_asset(pole, flag, imc):
    rules = kitlib.load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.FlagCeremonyRules)
        rules = TOOLS.create_asset("DA_FlagCeremonyRules", BASE, ue.FlagCeremonyRules, factory)
    assert rules, RULES
    stones = kitlib.load(STONES)
    assert stones, STONES
    rules.set_editor_property("cairn_mesh", stones)
    rules.set_editor_property("pole_mesh", pole)
    rules.set_editor_property("flag_mesh", flag)
    rules.set_editor_property("ceremony_mapping", imc)
    kitlib.save(rules)
    village = kitlib.load(VILLAGE_RULES)
    assert village, VILLAGE_RULES
    village.set_editor_property("flag_site_class", ue.FlagCeremonySite.static_class())
    village.set_editor_property("flag_ceremony_rules", rules)
    kitlib.save(village)
    return rules


def clear_red_circle():
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world or "L_Test_Flat" not in world.get_path_name():
        assert levels.load_level(LEVEL), LEVEL
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    gone = []
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if label == OLD_CIRCLE or label.startswith(OLD_CIRCLE + "_Ring_"):
            gone.append(label)
            actors.destroy_actor(actor)
    if gone:
        assert levels.save_current_level()
    return gone


def main():
    kitlib.check_lock()
    pole = import_mesh("SM_FlagCeremony_Pole")
    flag = import_mesh("SM_FlagCeremony_Flag")
    action, imc = ceremony_input()
    rules = rules_asset(pole, flag, imc)
    gone = clear_red_circle()
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"pole": pole.get_path_name(), "pole_collision": kitlib.convex_count(pole), "flag": flag.get_path_name(),
              "action": action.get_path_name(), "mapping": imc.get_path_name(), "rules": rules.get_path_name(),
              "village_rules": VILLAGE_RULES, "red_circle_removed": gone,
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "ceremony-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("FLAG_CEREMONY_SUCCESS")


if __name__ == "__main__":
    main()
