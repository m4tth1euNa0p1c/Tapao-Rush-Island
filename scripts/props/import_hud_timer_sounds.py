"""Imports the flood timer HUD sounds (art/audio/ui, scripts/props/hud_timer_sounds.py) into
/Game/MonkeIsland/UI/Sounds as SoundWaves on the effects channel of the settings (SC_Effects).
DA_HudTimerRules (AlertSound, EvacuationSound, ZoneSecuredSound) is filled afterwards in the editor (MCP ObjectTools).

Editor commandlet, or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os
import unreal

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "ui")
DEST = "/Game/MonkeIsland/UI/Sounds"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
NAMES = ("S_HudTimer_Alert", "S_HudTimer_Evacuation", "S_HudTimer_ZoneSecured")


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    effects = unreal.load_asset(EFFECTS_CLASS) if unreal.EditorAssetLibrary.does_asset_exist(EFFECTS_CLASS) else None
    done = 0
    for name in NAMES:
        task = unreal.AssetImportTask()
        task.filename = os.path.abspath(os.path.join(ROOT, f"{name}.wav"))
        task.destination_path = DEST
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = True
        tools.import_asset_tasks([task])
        sound = unreal.load_asset(f"{DEST}/{name}")
        if not sound:
            unreal.log_error(f"[hud timer sounds] import failed: {name}")
            continue
        if effects:
            sound.set_editor_property("sound_class_object", effects)
        unreal.EditorAssetLibrary.save_loaded_asset(sound)
        unreal.log(f"[hud timer sounds] {DEST}/{name} {sound.get_editor_property('duration'):.2f} s")
        done += 1
    unreal.log(f"[hud timer sounds] {done}/{len(NAMES)} sounds in {DEST}, SC_Effects: {bool(effects)}")


main()
