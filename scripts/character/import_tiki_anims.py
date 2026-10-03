"""Imports the tiki animation FBX (art/export/Characters/<Name>/A_<Name>_*.fbx) onto the skeletons already in UE.

The MCP cannot import animation-only FBX, so this runs in the editor commandlet (editor closed):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os
import unreal

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "export", "Characters")
NAMES = ["Villager", "CocoWarrior", "CrabGuardian", "AshWatcher"]


def task_for(fbx, dest, skeleton):
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
    task.destination_path = dest
    task.destination_name = os.path.splitext(os.path.basename(fbx))[0]
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = opts
    return task


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    done = 0
    for name in NAMES:
        skel_path = f"/Game/MonkeIsland/Characters/{name}/Meshes/SK_{name}_Skeleton"
        skeleton = unreal.EditorAssetLibrary.load_asset(skel_path)
        if not skeleton:
            unreal.log_error(f"[tiki_anims] missing skeleton {skel_path}")
            continue
        folder = os.path.join(ROOT, name)
        tasks = [task_for(os.path.abspath(os.path.join(folder, f)), f"/Game/MonkeIsland/Characters/{name}/Anims", skeleton)
                 for f in sorted(os.listdir(folder)) if f.startswith("A_") and f.endswith(".fbx")]
        tools.import_asset_tasks(tasks)
        for t in tasks:
            paths = list(t.imported_object_paths)
            unreal.log(f"[tiki_anims] {t.destination_name}: {paths}")
            done += 1 if paths else 0
    unreal.log(f"[tiki_anims] imported {done} animations")


main()
