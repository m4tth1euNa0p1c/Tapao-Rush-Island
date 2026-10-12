"""Imports the shore ambience loops (art/audio/ambience, scripts/props/shore_sounds.py) and creates
DA_ShoreAmbienceRules, read by UShoreAmbienceSubsystem.

Editor commandlet (editor closed), or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os
import unreal

SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "ambience")
DEST = "/Game/MonkeIsland/Environment/Shore"
AUDIO = f"{DEST}/Audio"
RULES = "DA_ShoreAmbienceRules"
SOUNDS = {"SW_Shore_Surf": "surf_sound", "SW_Shore_Sea": "sea_sound"}


def import_loop(tools, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(os.path.join(SRC, name + ".wav"))
    task.destination_path = AUDIO
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    sound = unreal.load_asset(f"{AUDIO}/{name}")
    if sound:
        sound.set_editor_property("looping", True)
        unreal.EditorAssetLibrary.save_loaded_asset(sound)
    return sound


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    path = f"{DEST}/{RULES}"
    rules = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(RULES, DEST, unreal.ShoreAmbienceRules, unreal.DataAssetFactory())
    if not rules:
        unreal.log_error(f"[shore] cannot create {path}")
        return
    for name, prop in SOUNDS.items():
        sound = import_loop(tools, name)
        unreal.log_warning(f"[shore] {name}: {'ok' if sound else 'FAILED'}")
        if sound:
            rules.set_editor_property(prop, sound)
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    unreal.log_warning("[shore] done")


main()
