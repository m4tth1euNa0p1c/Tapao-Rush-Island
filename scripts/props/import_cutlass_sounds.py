"""Imports the cutlass sounds (art/audio/weapons, scripts/props/cutlass_sounds.py), creates their attenuation and fills
DA_CutlassRules (swing, finisher, hit, kill, parry, draw, sheathe). Also sets the third person grip on hand_r.
Needs the editor built with the sound properties of UCutlassRules (T-070).

Editor commandlet (editor closed), or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os
import unreal

SOUND_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons")
SOUND_DEST = "/Game/MonkeIsland/Weapons/Cutlass/Audio"
RULES = "/Game/MonkeIsland/Weapons/Cutlass/DA_CutlassRules"
ATTENUATION = "ATT_Cutlass"
# volume slider "Effets" of the settings menu (T-073); assigned only once it exists
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"

# property of UCutlassRules -> sound
SOUNDS = {
    "swing_sound": "SW_Cutlass_Swing",
    "finisher_swing_sound": "SW_Cutlass_FinisherSwing",
    "hit_sound": "SW_Cutlass_Hit",
    "kill_sound": "SW_Cutlass_Kill",
    "parry_sound": "SW_Cutlass_Parry",
    "draw_sound": "SW_Cutlass_Draw",
    "sheathe_sound": "SW_Cutlass_Sheathe",
}


def import_sound(tools, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(os.path.join(SOUND_SRC, name + ".wav"))
    task.destination_path = SOUND_DEST
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    sound = unreal.load_asset(f"{SOUND_DEST}/{name}")
    effects = unreal.load_asset(EFFECTS_CLASS) if unreal.EditorAssetLibrary.does_asset_exist(EFFECTS_CLASS) else None
    if sound and effects:
        sound.set_editor_property("sound_class_object", effects)
        unreal.EditorAssetLibrary.save_loaded_asset(sound)
    return sound


def make_attenuation(tools):
    path = f"{SOUND_DEST}/{ATTENUATION}"
    att = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(ATTENUATION, SOUND_DEST, unreal.SoundAttenuation, None)
    settings = att.get_editor_property("attenuation")
    settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
    # clear within 3 m, gone at 25 m
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(300.0, 0.0, 0.0))
    settings.set_editor_property("falloff_distance", 2200.0)
    att.set_editor_property("attenuation", settings)
    unreal.EditorAssetLibrary.save_loaded_asset(att)
    return att


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    rules = unreal.load_asset(RULES)
    if not rules:
        unreal.log_error(f"[cutlass] {RULES} missing")
        return
    done = 0
    for prop, name in SOUNDS.items():
        sound = import_sound(tools, name)
        unreal.log(f"[cutlass] {name}: {'ok' if sound else 'FAILED'}")
        if sound:
            rules.set_editor_property(prop, sound)
            done += 1
    rules.set_editor_property("sound_attenuation", make_attenuation(tools))
    rules.set_editor_property("grip_bone", "hand_r")
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    unreal.log(f"[cutlass] sounds done: {done}/{len(SOUNDS)}, grip on hand_r")


main()
