"""Imports the Bean animation library of the second PC (maquette bean-anim-library, 30 clips on SK_Bean_Body, 24 fps,
no root motion) into /Game/MonkeIsland/Characters/Bean/Animations/Library (T-078).

The MCP cannot import animation-only FBX, so this runs in the editor commandlet (editor closed):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Source: art/export/Characters/Bean/Anims/A_Bean_*.fbx (copied from OneDrive). Same import settings as the musket
clips of import_musket_anims.py (they play on this skeleton already). No montage, notify or blendspace: the
library is played by UBeanAnimDriverComponent from DA_BeanAnimLibrary (C++), which this script fills.
"""
import glob
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "art", "export", "Characters", "Bean", "Anims")
DEST = "/Game/MonkeIsland/Characters/Bean/Animations/Library"
SKELETON = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
LIBRARY = "/Game/MonkeIsland/Characters/Bean/Animations/DA_BeanAnimLibrary"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
DRIVER_NAME = "BeanAnimDriver"

# UBeanAnimLibrary property -> clip
SLOTS = {
    "idle": "A_Bean_Idle", "walk": "A_Bean_Walk", "run": "A_Bean_Run",
    "jump_start": "A_Bean_Jump_Start", "jump_loop": "A_Bean_Jump_Loop", "jump_land": "A_Bean_Jump_Land",
    "carry_idle": "A_Bean_Carry_Idle", "carry_walk": "A_Bean_Carry_Walk",
    "boat_sit": "A_Bean_Boat_Sit_Idle", "boat_row": "A_Bean_Boat_Row",
    "death": "A_Bean_Death", "hit_react": "A_Bean_Hit_React", "pickup": "A_Bean_Pickup",
    "interact": "A_Bean_Interact", "eat_coconut": "A_Bean_Eat_Coconut", "wave": "A_Bean_Wave",
    "celebrate": "A_Bean_Celebrate",
    "sabre_idle": "A_Bean_Sabre_Idle", "sabre_draw": "A_Bean_Sabre_Draw", "sabre_slash_right": "A_Bean_Sabre_Slash_R",
    "sabre_slash_left": "A_Bean_Sabre_Slash_L", "sabre_thrust": "A_Bean_Sabre_Thrust",
    "sabre_finisher": "A_Bean_Sabre_Finisher", "sabre_block": "A_Bean_Sabre_Block", "sabre_sheathe": "A_Bean_Sabre_Sheathe",
    "musket_hold": "A_Bean_Musket_Hold", "musket_aim": "A_Bean_Musket_Aim", "musket_reload": "A_Bean_Musket_Reload",
    "musket_fire": "A_Bean_Musket_Fire", "musket_lower": "A_Bean_Musket_Lower",
}


def log(msg):
    unreal.log_warning(f"[beananims] {msg}")


def task_for(fbx, skeleton):
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
    task.destination_path = DEST
    task.destination_name = os.path.splitext(os.path.basename(fbx))[0]
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = opts
    return task


def main():
    skeleton = unreal.load_asset(SKELETON)
    if not skeleton:
        log(f"{SKELETON} missing")
        return
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    files = sorted(glob.glob(os.path.join(SRC, "A_Bean_*.fbx")))
    tools.import_asset_tasks([task_for(f, skeleton) for f in files])
    clips = {}
    for f in files:
        name = os.path.splitext(os.path.basename(f))[0]
        seq = unreal.load_asset(f"{DEST}/{name}")
        if seq:
            clips[name] = seq
            log(f"{name}: {seq.get_play_length():.3f} s")
        else:
            log(f"{name}: IMPORT FAILED")

    lib = unreal.load_asset(LIBRARY) if unreal.EditorAssetLibrary.does_asset_exist(LIBRARY) else None
    if not lib:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.BeanAnimLibrary)
        lib = tools.create_asset("DA_BeanAnimLibrary", os.path.dirname(LIBRARY), unreal.BeanAnimLibrary, factory)
    missing = []
    for prop, clip in SLOTS.items():
        if clip in clips:
            lib.set_editor_property(prop, clips[clip])
        else:
            missing.append(clip)
    unreal.EditorAssetLibrary.save_loaded_asset(lib)
    log(f"{len(clips)}/{len(files)} clips, DA_BeanAnimLibrary filled, missing: {missing}")
    add_driver(lib)


def add_driver(lib):
    """UBeanAnimDriverComponent on the player blueprint, Library = DA_BeanAnimLibrary"""
    bp = unreal.load_asset(PLAYER_BP)
    if not bp:
        log(f"{PLAYER_BP} missing")
        return
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    sdl = unreal.SubobjectDataBlueprintFunctionLibrary
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    template = None
    for h in handles:
        obj = sdl.get_object(sdl.get_data(h))
        if obj and isinstance(obj, unreal.BeanAnimDriverComponent):
            template = obj
    if not template:
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.BeanAnimDriverComponent,
                                              blueprint_context=bp)
        handle, fail = subsystem.add_new_subobject(params)
        subsystem.rename_subobject(handle, unreal.Text(DRIVER_NAME))
        template = sdl.get_object(sdl.get_data(handle))
        log(f"driver added to BP_IslanderBean ({fail})")
    template.set_editor_property("library", lib)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    unreal.EditorAssetLibrary.save_loaded_asset(bp)
    log("BP_IslanderBean: BeanAnimDriver -> DA_BeanAnimLibrary")


main()
