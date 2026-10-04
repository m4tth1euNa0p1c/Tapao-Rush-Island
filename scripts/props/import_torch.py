"""Bean's hand torch (maquettes bean-torch / bean-torch-anim of the second PC, T-099): materials, input, DA_TorchRules,
BP_Torch, DA_Item_Torch in slot 3 of BP_IslanderBean, and the 7 torch clips in DA_BeanAnimLibrary.

Editor commandlet (editor closed, editor built with ATorchItem / UTorchRules and the torch clips of UBeanAnimLibrary):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Meshes imported before by MCP (/Game/MonkeIsland/Weapons/Torch/SM_Bean_Torch, SM_Bean_Torch_Flame). Idempotent.
"""
import glob
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEST = "/Game/MonkeIsland/Weapons/Torch"
INPUT = "/Game/MonkeIsland/Weapons/Input"
ANIMS = "/Game/MonkeIsland/Characters/Bean/Animations/Library"
SKELETON = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
LIBRARY = "/Game/MonkeIsland/Characters/Bean/Animations/DA_BeanAnimLibrary"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
SPECKLE = "/Game/MonkeIsland/Items/Loot/Materials/M_Loot_Speckle"
SLOTS = {
    "M_Torch_Wood": "/Game/MonkeIsland/Environment/Marae/Materials/MI_Marae_Wood",
    "M_Torch_Rope": "/Game/MonkeIsland/Environment/Marae/Materials/MI_Marae_Rope",
    "M_Torch_Leather": "/Game/MonkeIsland/Characters/Bean/Materials/MI_Bean_Leather",
    "M_Torch_Char": "/Game/MonkeIsland/Environment/Camp/Materials/MI_Camp_Charcoal",
    "M_Torch_Flame": "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_Flame",
    "M_Torch_FlameCore": "/Game/MonkeIsland/Environment/Marae/Materials/MI_Marae_FlameCore",
}
CLIPS = {"torch_equip": "A_Bean_Torch_Equip", "torch_ignite": "A_Bean_Torch_Ignite", "torch_idle": "A_Bean_Torch_Idle",
         "torch_raise": "A_Bean_Torch_Raise_Idle", "torch_light": "A_Bean_Torch_Light", "torch_swing": "A_Bean_Torch_Swing",
         "torch_unequip": "A_Bean_Torch_Unequip"}

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning(f"[torch] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def create(name, folder, cls, factory):
    return load(f"{folder}/{name}") or tools.create_asset(name, folder, cls, factory)


def speckle_mi(name, a, b, rough):
    inst = create(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, load(SPECKLE))
    mel.set_material_instance_scalar_parameter_value(inst, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(inst, "NoiseScale", 2.0)
    mel.set_material_instance_vector_parameter_value(inst, "ColorA", unreal.LinearColor(*a, 1))
    mel.set_material_instance_vector_parameter_value(inst, "ColorB", unreal.LinearColor(*b, 1))
    mel.update_material_instance(inst)
    eal.save_loaded_asset(inst)
    return inst


def set_slots(mesh, mats):
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        if mats.get(name):
            mesh.set_material(index, mats[name])
        else:
            log(f"{mesh.get_name()}: no material for {name}")
    for socket_name in ("grip", "flame", "light", "heel"):
        socket = mesh.find_socket(socket_name)
        if socket:
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
    eal.save_loaded_asset(mesh)


def input_action(name):
    action = create(name, INPUT, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    eal.save_loaded_asset(action)
    return action


def key(name):
    k = unreal.Key()
    k.set_editor_property("key_name", name)
    return k


def import_clips():
    skeleton = load(SKELETON)
    tasks = []
    for fbx in sorted(glob.glob(os.path.join(ROOT, "art", "export", "Weapons", "Torch", "Anims", "A_Bean_Torch_*.fbx"))):
        opts = unreal.FbxImportUI()
        opts.import_mesh = False
        opts.import_animations = True
        opts.import_materials = False
        opts.import_textures = False
        opts.import_as_skeletal = True
        opts.skeleton = skeleton
        opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
        data = opts.anim_sequence_import_data
        data.set_editor_property("import_translation", unreal.Vector(0, 0, 0))
        data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
        data.set_editor_property("remove_redundant_keys", False)
        task = unreal.AssetImportTask()
        task.filename = fbx
        task.destination_path = ANIMS
        task.destination_name = os.path.splitext(os.path.basename(fbx))[0]
        task.replace_existing = True
        task.automated = True
        task.save = True
        task.options = opts
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    lib = load(LIBRARY)
    found = 0
    for prop, clip in CLIPS.items():
        seq = load(f"{ANIMS}/{clip}")
        if seq:
            lib.set_editor_property(prop, seq)
            found += 1
    eal.save_loaded_asset(lib)
    log(f"{found}/{len(CLIPS)} torch clips in DA_BeanAnimLibrary")


def main():
    mats = {k: load(v) for k, v in SLOTS.items()}
    mats["M_Torch_Iron"] = speckle_mi("MI_Torch_Iron", (0.10, 0.10, 0.11), (0.20, 0.19, 0.18), 0.45)
    mats["M_Torch_Tow"] = speckle_mi("MI_Torch_Tow", (0.10, 0.07, 0.04), (0.24, 0.18, 0.10), 0.85)
    torch, flame = load(f"{DEST}/SM_Bean_Torch"), load(f"{DEST}/SM_Bean_Torch_Flame")
    set_slots(torch, mats)
    set_slots(flame, mats)

    swing, raise_ = input_action("IA_TorchSwing"), input_action("IA_TorchRaise")
    imc = create("IMC_Torch", INPUT, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    imc.unmap_all()
    imc.map_key(swing, key("LeftMouseButton"))
    imc.map_key(raise_, key("RightMouseButton"))
    eal.save_loaded_asset(imc)

    rules = load(f"{DEST}/DA_TorchRules")
    if not rules:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TorchRules)
        rules = tools.create_asset("DA_TorchRules", DEST, unreal.TorchRules, factory)
    rules.set_editor_property("torch_mesh", torch)
    rules.set_editor_property("flame_mesh", flame)
    rules.set_editor_property("flame_material", load(SLOTS["M_Torch_Flame"]))
    rules.set_editor_property("mapping_context", imc)
    rules.set_editor_property("swing_action", swing)
    rules.set_editor_property("raise_action", raise_)
    eal.save_loaded_asset(rules)

    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.TorchItem)
    bp = load(f"{DEST}/BP_Torch") or tools.create_asset("BP_Torch", DEST, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("rules", rules)
    cdo.modify()
    eal.save_loaded_asset(bp, False)

    item = load(f"{DEST}/DA_Item_Torch")
    if not item:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.InventoryItemDefinition)
        item = tools.create_asset("DA_Item_Torch", DEST, unreal.InventoryItemDefinition, factory)
    item.set_editor_property("display_name", unreal.Text("Torche"))
    item.set_editor_property("item_class", bp.generated_class())
    eal.save_loaded_asset(item)
    log("DA_TorchRules, BP_Torch, DA_Item_Torch")

    # slot 3 of the player (index 2)
    player = load(PLAYER_BP)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    for h in subsystem.k2_gather_subobject_data_for_blueprint(player):
        obj = sdl.get_object(sdl.get_data(h))
        if obj and isinstance(obj, unreal.IslanderInventoryComponent):
            items = list(obj.get_editor_property("starting_items"))
            while len(items) < 3:
                items.append(None)
            items[2] = item
            obj.set_editor_property("starting_items", items)
            log(f"BP_IslanderBean inventory: {[i.get_name() if i else None for i in items]}")
    unreal.BlueprintEditorLibrary.compile_blueprint(player)
    eal.save_loaded_asset(player)

    import_clips()


main()
