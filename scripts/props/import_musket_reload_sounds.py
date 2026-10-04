"""Imports the musket reload sounds (art/audio/weapons, scripts/props/musket_sounds.py), creates their attenuation
and fills DA_MusketRules.ReloadSoundCues, timed on the reload timeline (A_Musket_Reload, 6.583 s, the same time
base as ReloadKeys: phases of the maquette bean-musket-anim).

Editor commandlet (editor closed), or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os
import unreal

SOUND_SRC = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons")
SOUND_DEST = "/Game/MonkeIsland/Weapons/Musket/Audio"
RULES = "/Game/MonkeIsland/Weapons/Musket/DA_MusketRules"
ATTENUATION = "ATT_Musket_Reload"

# (time on the reload timeline in s, sound, volume)
CUES = [
    (0.25, "SW_Musket_Cock", 0.7),        # half cock
    (0.70, "SW_Musket_Frizzen", 0.8),     # frizzen opened
    (1.45, "SW_Musket_Prime", 0.8),       # priming the pan from the flask
    (2.10, "SW_Musket_Frizzen", 1.0),     # frizzen snapped shut
    (2.75, "SW_Musket_Paper", 0.9),       # cartridge torn
    (3.15, "SW_Musket_Powder", 1.0),      # powder down the barrel
    (4.20, "SW_Musket_Ball", 1.0),        # ball and paper rammed in
    (4.62, "SW_Musket_RamrodDraw", 1.0),  # ramrod drawn
    (5.10, "SW_Musket_RamStroke", 1.0),   # two strokes
    (5.37, "SW_Musket_RamStroke", 0.9),
    (5.70, "SW_Musket_RamrodDraw", 0.7),  # ramrod back in its pipes
    (6.15, "SW_Musket_Cock", 1.0),        # full cock
]


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


def make_attenuation(tools):
    path = f"{SOUND_DEST}/{ATTENUATION}"
    att = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(ATTENUATION, SOUND_DEST, unreal.SoundAttenuation, None)
    settings = att.get_editor_property("attenuation")
    settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
    # heard clearly within 2 m, gone at 20 m
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(200.0, 0.0, 0.0))
    settings.set_editor_property("falloff_distance", 1800.0)
    att.set_editor_property("attenuation", settings)
    unreal.EditorAssetLibrary.save_loaded_asset(att)
    return att


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    rules = unreal.load_asset(RULES)
    if not rules:
        unreal.log_error(f"[musket] {RULES} missing")
        return
    sounds = {}
    for name in sorted({c[1] for c in CUES}):
        sounds[name] = import_sound(tools, name)
        unreal.log(f"[musket] {name}: {'ok' if sounds[name] else 'FAILED'}")
    cues = []
    for time, name, volume in CUES:
        if not sounds[name]:
            continue
        cue = unreal.MusketSoundCue()
        cue.set_editor_property("time", time)
        cue.set_editor_property("sound", sounds[name])
        cue.set_editor_property("volume", volume)
        cues.append(cue)
    rules.set_editor_property("reload_sound_cues", cues)
    rules.set_editor_property("reload_sound_attenuation", make_attenuation(tools))
    # one shot kills (user request 2026-10-03); Camera: Pawn / CharacterMesh profiles ignore Visibility
    rules.set_editor_property("damage", 10000.0)
    rules.set_editor_property("trace_channel", unreal.CollisionChannel.ECC_CAMERA)
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    unreal.log(f"[musket] reload sounds done: {len(cues)} cues")


main()
