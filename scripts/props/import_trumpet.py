"""Herald trumpet emote of the bean, key V (maquettes bean-trumpet / bean-trumpet-anim of the second PC, owner request
2026-10-04): SM_Bean_Trumpet and its materials, the 4 clips A_Bean_Trumpet_* in DA_BeanAnimLibrary, the fanfare
(scripts/props/trumpet_sounds.py), IA_TrumpetEmote / IMC_TrumpetEmote (V), DA_TrumpetEmoteRules and the
UTrumpetEmoteComponent of BP_IslanderBean.

Copy first the exports of OneDrive (read only there) into art/export/Emotes/Trumpet (done by hand or by the session).
Editor commandlet (editor closed, editor built with UTrumpetEmoteComponent and the Trumpet* clips of UBeanAnimLibrary):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Idempotent.
"""
import glob
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "art", "export", "Emotes", "Trumpet")
SOUND = os.path.join(ROOT, "art", "audio", "emotes", "SW_Trumpet_Fanfare.wav")
DEST = "/Game/MonkeIsland/Emotes/Trumpet"
INPUT = "/Game/MonkeIsland/Emotes/Input"
ANIMS = "/Game/MonkeIsland/Characters/Bean/Animations/Library"
SKELETON = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
LIBRARY = "/Game/MonkeIsland/Characters/Bean/Animations/DA_BeanAnimLibrary"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
SPECKLE = "/Game/MonkeIsland/Items/Loot/Materials/M_Loot_Speckle"
GOLD = "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_Gold"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
CLIPS = {"trumpet_equip": "A_Bean_Trumpet_Equip", "trumpet_fanfare": "A_Bean_Trumpet_Fanfare",
         "trumpet_play_loop": "A_Bean_Trumpet_Play_Loop", "trumpet_unequip": "A_Bean_Trumpet_Unequip"}
LENGTH_CM = 62.0

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning(f"[trumpet] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def create(name, folder, cls, factory):
    return load(f"{folder}/{name}") or tools.create_asset(name, folder, cls, factory)


def metal_material():
    """Polished metal: colour, metallic, roughness (silver plated brass of the trumpet)"""
    m = create("M_Trumpet_Metal", DEST, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    color = mel.create_material_expression(m, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(0.78, 0.78, 0.76, 1))
    metal = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 200)
    metal.set_editor_property("parameter_name", "Metallic")
    metal.set_editor_property("default_value", 1.0)
    rough = mel.create_material_expression(m, unreal.MaterialExpressionScalarParameter, -500, 300)
    rough.set_editor_property("parameter_name", "Roughness")
    rough.set_editor_property("default_value", 0.18)
    mel.connect_material_property(color, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(metal, "", MP.MP_METALLIC)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


def speckle_mi(name, a, b, rough):
    inst = create(name, DEST, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, load(SPECKLE))
    mel.set_material_instance_scalar_parameter_value(inst, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(inst, "NoiseScale", 3.0)
    mel.set_material_instance_vector_parameter_value(inst, "ColorA", unreal.LinearColor(*a, 1))
    mel.set_material_instance_vector_parameter_value(inst, "ColorB", unreal.LinearColor(*b, 1))
    mel.update_material_instance(inst)
    eal.save_loaded_asset(inst)
    return inst


def import_mesh(scale):
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = False
    opts.import_textures = False
    opts.import_animations = False
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = opts.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("import_uniform_scale", scale)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, "SM_Bean_Trumpet.fbx")
    task.destination_path = DEST
    task.destination_name = "SM_Bean_Trumpet"
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = opts
    tools.import_asset_tasks([task])
    return load(f"{DEST}/SM_Bean_Trumpet")


def mesh():
    sm = import_mesh(1.0)
    size = sm.get_bounding_box().max.x - sm.get_bounding_box().min.x if sm else 0.0
    # the length of the maquette is 0.62 m: an FBX in metres comes in 100 times too small
    if sm and size < LENGTH_CM / 10.0:
        log(f"length {size:.2f} cm, imported again x100")
        sm = import_mesh(100.0)
    box = sm.get_bounding_box()
    log(f"SM_Bean_Trumpet: {box.max.x - box.min.x:.1f} x {box.max.y - box.min.y:.1f} x {box.max.z - box.min.z:.1f} cm")
    mats = {
        "M_Trumpet_Silver": metal_material(),
        "M_Marae_Gold": load(GOLD),
        "M_Trumpet_Pearl": speckle_mi("MI_Trumpet_Pearl", (0.93, 0.9, 0.84), (0.85, 0.86, 0.9), 0.2),
        "M_Trumpet_Velvet": speckle_mi("MI_Trumpet_Velvet", (0.16, 0.018, 0.026), (0.12, 0.012, 0.02), 0.85),
    }
    for index, slot in enumerate(sm.get_editor_property("static_materials")):
        name = str(slot.get_editor_property("material_slot_name"))
        if mats.get(name):
            sm.set_material(index, mats[name])
        else:
            log(f"no material for slot {name}")
    for socket_name in ("grip_r", "grip_l", "mouth", "bell", "banner"):
        socket = sm.find_socket(socket_name)
        if socket:
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
    eal.save_loaded_asset(sm)
    return sm


def clips():
    skeleton = load(SKELETON)
    tasks = []
    for fbx in sorted(glob.glob(os.path.join(SRC, "Anims", "A_Bean_Trumpet_*.fbx"))):
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
    log(f"{found}/{len(CLIPS)} trumpet clips in DA_BeanAnimLibrary")


def sound():
    task = unreal.AssetImportTask()
    task.filename = SOUND
    task.destination_path = DEST
    task.destination_name = "SW_Trumpet_Fanfare"
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    wave = load(f"{DEST}/SW_Trumpet_Fanfare")
    effects = load(EFFECTS_CLASS)
    if wave and effects:
        wave.set_editor_property("sound_class_object", effects)
        eal.save_loaded_asset(wave)
    att = create("ATT_Trumpet", DEST, unreal.SoundAttenuation, unreal.SoundAttenuationFactory())
    settings = att.get_editor_property("attenuation")
    settings.set_editor_property("falloff_distance", 6000.0)
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(400, 0, 0))
    att.set_editor_property("attenuation", settings)
    eal.save_loaded_asset(att)
    return wave, att


def key(name):
    k = unreal.Key()
    k.set_editor_property("key_name", name)
    return k


def main():
    sm = mesh()
    wave, att = sound()

    action = create("IA_TrumpetEmote", INPUT, unreal.InputAction, unreal.InputAction_Factory())
    action.set_editor_property("value_type", unreal.InputActionValueType.BOOLEAN)
    eal.save_loaded_asset(action)
    imc = create("IMC_TrumpetEmote", INPUT, unreal.InputMappingContext, unreal.InputMappingContext_Factory())
    imc.unmap_all()
    imc.map_key(action, key("V"))
    eal.save_loaded_asset(imc)

    rules = load(f"{DEST}/DA_TrumpetEmoteRules")
    if not rules:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.TrumpetEmoteRules)
        rules = tools.create_asset("DA_TrumpetEmoteRules", DEST, unreal.TrumpetEmoteRules, factory)
    rules.set_editor_property("trumpet_mesh", sm)
    rules.set_editor_property("fanfare_sound", wave)
    rules.set_editor_property("sound_attenuation", att)
    rules.set_editor_property("emote_action", action)
    rules.set_editor_property("mapping_context", imc)
    eal.save_loaded_asset(rules)
    log("DA_TrumpetEmoteRules, IA/IMC (V)")

    # the component on the player blueprint
    player = load(PLAYER_BP)
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(player)
    component = None
    for h in handles:
        obj = sdl.get_object(sdl.get_data(h))
        if obj and isinstance(obj, unreal.TrumpetEmoteComponent):
            component = obj
    if not component:
        params = unreal.AddNewSubobjectParams()
        params.set_editor_property("parent_handle", handles[0])
        params.set_editor_property("new_class", unreal.TrumpetEmoteComponent)
        params.set_editor_property("blueprint_context", player)
        handle, reason = subsystem.add_new_subobject(params)
        if not str(reason):
            subsystem.rename_subobject(handle, unreal.Text("TrumpetEmote"))
        component = sdl.get_object(sdl.get_data(handle))
        log(f"component added ({reason})")
    if component:
        component.set_editor_property("rules", rules)
    unreal.BlueprintEditorLibrary.compile_blueprint(player)
    eal.save_loaded_asset(player)
    log(f"BP_IslanderBean: TrumpetEmote component {'ok' if component else 'MISSING'}")

    clips()


main()
