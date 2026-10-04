"""Imports the musket animations of the second PC (art/export/Weapons/Musket/A_*.fbx), the synthesised sounds
(art/audio/weapons, scripts/props/musket_sounds.py) and builds the shot materials, then plugs them into DA_MusketRules.

The MCP cannot import animation-only FBX, so this runs in the editor commandlet (editor closed):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Bean body clips go on the bean skeleton, lock / ramrod clips on the musket skeleton.
"""
import os
import unreal

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "export", "Weapons", "Musket")
DEST = "/Game/MonkeIsland/Weapons/Musket/Animations"
BEAN_SKELETON = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
MUSKET_SKELETON = "/Game/MonkeIsland/Weapons/Musket/SK_Bean_Musket_Skeleton"
RULES = "/Game/MonkeIsland/Weapons/Musket/DA_MusketRules"

CLIPS = {
    "A_Bean_Musket_Reload": (BEAN_SKELETON, "body_reload_animation"),
    "A_Bean_Musket_Fire": (BEAN_SKELETON, "body_fire_animation"),
    "A_Bean_Musket_Hold": (BEAN_SKELETON, "body_hold_animation"),
    "A_Musket_Reload": (MUSKET_SKELETON, "musket_reload_animation"),
    "A_Musket_Fire": (MUSKET_SKELETON, "musket_fire_animation"),
}


def task_for(fbx, skeleton):
    opts = unreal.FbxImportUI()
    opts.import_mesh = False
    opts.import_animations = True
    opts.import_materials = False
    opts.import_textures = False
    opts.import_as_skeletal = True
    opts.skeleton = skeleton
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
    opts.anim_sequence_import_data.set_editor_property("import_translation", unreal.Vector(0, 0, 0))
    opts.anim_sequence_import_data.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
    opts.anim_sequence_import_data.set_editor_property("remove_redundant_keys", False)
    task = unreal.AssetImportTask()
    task.filename = fbx
    task.destination_path = DEST
    task.destination_name = os.path.splitext(os.path.basename(fbx))[0]
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = opts
    return task


SOUND_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons")
SOUND_DEST = "/Game/MonkeIsland/Weapons/Musket/Audio"
SOUNDS = {"SW_Musket_Shot": "fire_sound", "SW_Musket_Lock": "lock_sound"}
FX_DEST = "/Game/MonkeIsland/Weapons/Musket/FX"
SPHERE = "/Engine/BasicShapes/Sphere"


def import_sound(tools, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(os.path.join(SOUND_SRC, name + ".wav"))
    task.destination_path = SOUND_DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    return unreal.load_asset(f"{SOUND_DEST}/{name}")


def soft_edge(mel, mat, x, y):
    """1 - fresnel, squared: opaque in the middle of the sphere, fading to nothing on its outline."""
    fres = mel.create_material_expression(mat, unreal.MaterialExpressionFresnel, x, y)
    fres.set_editor_property("exponent", 1.5)
    inv = mel.create_material_expression(mat, unreal.MaterialExpressionOneMinus, x + 200, y)
    mel.connect_material_expressions(fres, "", inv, "")
    sq = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, x + 400, y)
    mel.connect_material_expressions(inv, "", sq, "A")
    mel.connect_material_expressions(inv, "", sq, "B")
    return sq


def make_material(tools, name, build):
    path = f"{FX_DEST}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.EditorAssetLibrary.delete_asset(path)
    mat = tools.create_asset(name, FX_DEST, unreal.Material, unreal.MaterialFactoryNew())
    build(unreal.MaterialEditingLibrary, mat)
    unreal.MaterialEditingLibrary.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def build_flash(mel, mat):
    """Muzzle flame: unlit additive, white-hot core to orange edge."""
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    edge = soft_edge(mel, mat, -900, 0)
    color = mel.create_material_expression(mat, unreal.MaterialExpressionVectorParameter, -700, -250)
    color.set_editor_property("parameter_name", "FlashColor")
    color.set_editor_property("default_value", unreal.LinearColor(18.0, 7.0, 1.6, 1.0))
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 0)
    mel.connect_material_expressions(color, "", mul, "A")
    mel.connect_material_expressions(edge, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)


def build_smoke(mel, mat):
    """Powder smoke: lit translucent grey, opacity from the Opacity parameter, soft outline."""
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    edge = soft_edge(mel, mat, -900, 200)
    opacity = mel.create_material_expression(mat, unreal.MaterialExpressionScalarParameter, -700, 0)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.5)
    mul = mel.create_material_expression(mat, unreal.MaterialExpressionMultiply, -200, 100)
    mel.connect_material_expressions(opacity, "", mul, "A")
    mel.connect_material_expressions(edge, "", mul, "B")
    mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_OPACITY)
    grey = mel.create_material_expression(mat, unreal.MaterialExpressionConstant3Vector, -400, -200)
    grey.set_editor_property("constant", unreal.LinearColor(0.72, 0.71, 0.68, 1.0))
    mel.connect_material_property(grey, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = mel.create_material_expression(mat, unreal.MaterialExpressionConstant, -400, -50)
    rough.set_editor_property("r", 1.0)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    rules = unreal.load_asset(RULES)
    if not rules:
        unreal.log_error(f"[musket] {RULES} missing")
        return
    for name, (skel_path, prop) in CLIPS.items():
        skeleton = unreal.load_asset(skel_path)
        fbx = os.path.abspath(os.path.join(SRC, name + ".fbx"))
        if not skeleton or not os.path.exists(fbx):
            unreal.log_error(f"[musket] skip {name}: skeleton {skel_path} or {fbx} missing")
            continue
        tools.import_asset_tasks([task_for(fbx, skeleton)])
        anim = unreal.load_asset(f"{DEST}/{name}")
        unreal.log(f"[musket] {name}: {'ok' if anim else 'FAILED'}")
        if anim and prop:
            rules.set_editor_property(prop, anim)
    for name, prop in SOUNDS.items():
        sound = import_sound(tools, name)
        unreal.log(f"[musket] {name}: {'ok' if sound else 'FAILED'}")
        if sound:
            rules.set_editor_property(prop, sound)
    sphere = unreal.load_asset(SPHERE)
    rules.set_editor_property("flash_mesh", sphere)
    rules.set_editor_property("smoke_mesh", sphere)
    rules.set_editor_property("flash_material", make_material(tools, "M_MusketFlash", build_flash))
    rules.set_editor_property("smoke_material", make_material(tools, "M_MusketSmoke", build_smoke))
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    unreal.log("[musket] done")


main()
