"""Imports the footsteps (art/audio/footsteps, scripts/props/footstep_sounds.py) and the coconut thuds
(art/audio/hazards, scripts/props/coconut_sounds.py), creates their attenuations, creates DA_FootstepRules and fills
DA_CoconutFallRules. Needs the editor built with UFootstepRules and the sound properties of UCoconutFallRules (T-071).

Editor commandlet (editor closed), or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os
import unreal

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio")
STEP_DEST = "/Game/MonkeIsland/Player/Footsteps"
STEP_RULES = "DA_FootstepRules"
COCONUT_DEST = "/Game/MonkeIsland/Environment/Storm/Audio"
COCONUT_RULES = "/Game/MonkeIsland/Environment/Storm/DA_CoconutFallRules"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"

# surface -> (enum, relative volume)
SURFACES = {
    "SandDry": (unreal.FootstepSurface.SAND_DRY, 0.8),
    "SandWet": (unreal.FootstepSurface.SAND_WET, 0.85),
    "Water": (unreal.FootstepSurface.WATER, 1.0),
    "Litter": (unreal.FootstepSurface.LITTER, 0.8),
    "Coral": (unreal.FootstepSurface.CORAL, 0.9),
    "Stone": (unreal.FootstepSurface.STONE, 0.9),
    "Wood": (unreal.FootstepSurface.WOOD, 1.0),
}


def import_sound(tools, folder, dest, name):
    task = unreal.AssetImportTask()
    task.filename = os.path.abspath(os.path.join(ROOT, folder, name + ".wav"))
    task.destination_path = dest
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    tools.import_asset_tasks([task])
    sound = unreal.load_asset(f"{dest}/{name}")
    # volume channel of the settings menu (T-073), kept through reimports
    effects = unreal.load_asset(EFFECTS_CLASS) if unreal.EditorAssetLibrary.does_asset_exist(EFFECTS_CLASS) else None
    if sound and effects:
        sound.set_editor_property("sound_class_object", effects)
        unreal.EditorAssetLibrary.save_loaded_asset(sound)
    return sound


def make_attenuation(tools, dest, name, inner, falloff):
    path = f"{dest}/{name}"
    att = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(name, dest, unreal.SoundAttenuation, None)
    settings = att.get_editor_property("attenuation")
    settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
    settings.set_editor_property("attenuation_shape_extents", unreal.Vector(inner, 0.0, 0.0))
    settings.set_editor_property("falloff_distance", falloff)
    att.set_editor_property("attenuation", settings)
    unreal.EditorAssetLibrary.save_loaded_asset(att)
    return att


def footsteps(tools):
    path = f"{STEP_DEST}/{STEP_RULES}"
    rules = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else \
        tools.create_asset(STEP_RULES, STEP_DEST, unreal.FootstepRules, unreal.DataAssetFactory())
    sounds = {}
    total = 0
    for surface, (enum, volume) in SURFACES.items():
        variants = []
        for i in range(1, 5):
            sound = import_sound(tools, "footsteps", STEP_DEST + "/Audio", f"SW_Step_{surface}_{i:02d}")
            if sound:
                variants.append(sound)
        entry = unreal.FootstepSurfaceSounds()
        entry.set_editor_property("sounds", variants)
        entry.set_editor_property("volume", volume)
        sounds[enum] = entry
        total += len(variants)
        unreal.log(f"[footsteps] {surface}: {len(variants)}/4")
    rules.set_editor_property("sounds", sounds)
    # steps are quiet: clear within 1.5 m, gone at 15 m
    rules.set_editor_property("attenuation", make_attenuation(tools, STEP_DEST + "/Audio", "ATT_Footstep", 150.0, 1350.0))
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    unreal.log(f"[footsteps] {path}: {total}/28 sounds")


def coconut(tools):
    rules = unreal.load_asset(COCONUT_RULES)
    if not rules:
        unreal.log_error(f"[coconut] {COCONUT_RULES} missing")
        return
    thuds = [s for s in (import_sound(tools, "hazards", COCONUT_DEST, f"SW_Coconut_Sand_{i:02d}") for i in range(1, 4)) if s]
    rules.set_editor_property("land_sounds", thuds)
    # a heavy nut on the sand: clear within 4 m, gone at 35 m
    rules.set_editor_property("land_attenuation", make_attenuation(tools, COCONUT_DEST, "ATT_CoconutLand", 400.0, 3100.0))
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    unreal.log(f"[coconut] {COCONUT_RULES}: {len(thuds)}/3 thuds")


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    footsteps(tools)
    coconut(tools)


main()
