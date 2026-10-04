"""Imports the mob animation library of the second PC (maquette mob-anim-library: 95 clips, 30 fps, in place) onto the
existing tiki skeletons (T-079).

Editor commandlet (editor closed):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Source: art/export/Characters/<Name>/Anims/A_<Name>_*.fbx (copied from OneDrive). Clips land in
/Game/MonkeIsland/Characters/<Name>/Anims with replace_existing: the 6 clips the tiki already play (Idle, Walk, Run,
Attack, Hit, Death...) are reimported in place under the same names, so UTikiAnimInstance and the creature
blueprints keep their references; the 13 new clips per creature are added next to them. BrambleStalker has no
mesh in the project yet: its clips are skipped until SK_BrambleStalker is imported.
"""
import glob
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
CHARACTERS = ("CocoWarrior", "CrabGuardian", "AshWatcher", "Villager", "BrambleStalker")


def log(msg):
    unreal.log_warning(f"[mobanims] {msg}")


def task_for(fbx, skeleton, dest):
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
    task.destination_path = dest
    task.destination_name = os.path.splitext(os.path.basename(fbx))[0]
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = opts
    return task


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    for name in CHARACTERS:
        skeleton_path = f"/Game/MonkeIsland/Characters/{name}/Meshes/SK_{name}_Skeleton"
        skeleton = unreal.load_asset(skeleton_path) if unreal.EditorAssetLibrary.does_asset_exist(skeleton_path) else None
        if not skeleton:
            log(f"{name}: no skeleton in the project, skipped")
            continue
        dest = f"/Game/MonkeIsland/Characters/{name}/Anims"
        files = sorted(glob.glob(os.path.join(ROOT, "art", "export", "Characters", name, "Anims", f"A_{name}_*.fbx")))
        tools.import_asset_tasks([task_for(f, skeleton, dest) for f in files])
        ok = [os.path.splitext(os.path.basename(f))[0] for f in files
              if unreal.EditorAssetLibrary.does_asset_exist(f"{dest}/{os.path.splitext(os.path.basename(f))[0]}")]
        log(f"{name}: {len(ok)}/{len(files)} clips in {dest}")


main()
