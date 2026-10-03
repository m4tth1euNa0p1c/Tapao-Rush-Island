"""Imports the voices of the tiki creatures (art/audio/creatures, scripts/props/creature_sounds.py) into
/Game/MonkeIsland/Characters/Tiki/Audio, on the effects channel of the settings (SC_Effects), and creates their
attenuation ATT_Tiki_Voice. The voices are then given to the creature Blueprints (FTikiVoice, T-077) in the editor.

Editor commandlet (editor closed), or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import glob
import os
import unreal

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "creatures")
DEST = "/Game/MonkeIsland/Characters/Tiki/Audio"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"


def import_sound(tools, path, effects):
    name = os.path.splitext(os.path.basename(path))[0]
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(path)
    task.destination_path = DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    sound = unreal.load_asset(f"{DEST}/{name}")
    if sound and effects:
        sound.set_editor_property("sound_class_object", effects)
        unreal.EditorAssetLibrary.save_loaded_asset(sound)
    return sound


def make_attenuation(tools, name, inner, falloff):
    path = f"{DEST}/{name}"
    att = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, DEST, unreal.SoundAttenuation, None)
    settings = att.get_editor_property("attenuation")
    settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(inner, 0.0, 0.0))
    settings.set_editor_property("falloff_distance", falloff)
    att.set_editor_property("attenuation", settings)
    unreal.EditorAssetLibrary.save_loaded_asset(att)
    return att


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    effects = unreal.load_asset(EFFECTS_CLASS) if unreal.EditorAssetLibrary.does_asset_exist(EFFECTS_CLASS) else None
    files = sorted(glob.glob(os.path.join(ROOT, "SW_*.wav")))
    done = sum(1 for path in files if import_sound(tools, path, effects))
    # a voice carries further than a step: clear within 4 m, gone at 40 m
    make_attenuation(tools, "ATT_Tiki_Voice", 400.0, 3600.0)
    unreal.log(f"[creature sounds] {done}/{len(files)} sounds in {DEST}, SC_Effects: {bool(effects)}, ATT_Tiki_Voice")


main()
