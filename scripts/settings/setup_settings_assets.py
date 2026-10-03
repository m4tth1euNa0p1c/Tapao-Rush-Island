"""Creates the audio side of the settings menu (T-073): one sound class per volume channel under the engine Master
class, the mix that carries the player's volumes, DA_SettingsRules, and puts every sound of /Game/MonkeIsland in its
channel by folder (Ambience: shore, village; Effects: steps, impacts, weapons). Re-run after adding sound folders.
Needs the editor built with USettingsRules.

Editor commandlet (editor closed), or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import unreal

DEST = "/Game/MonkeIsland/Settings"
CLASSES = DEST + "/SoundClasses"
RULES = "DA_SettingsRules"
MIX = "SM_UserVolumes"
MASTER = "/Engine/EngineSounds/Master"

# channel enum -> sound class name
CHANNELS = {
    unreal.SettingsVolume.MUSIC: "SC_Music",
    unreal.SettingsVolume.EFFECTS: "SC_Effects",
    unreal.SettingsVolume.AMBIENCE: "SC_Ambience",
    unreal.SettingsVolume.DIALOGUE: "SC_Dialogue",
    unreal.SettingsVolume.INTERFACE: "SC_Interface",
}

# folder prefix -> sound class name (first match wins)
FOLDERS = [
    ("/Game/MonkeIsland/Environment/Shore", "SC_Ambience"),
    ("/Game/MonkeIsland/Village", "SC_Ambience"),
    ("/Game/MonkeIsland/Environment/Wind", "SC_Ambience"),
    ("/Game/MonkeIsland/Music", "SC_Music"),
    ("/Game/MonkeIsland/UI", "SC_Interface"),
    ("/Game/MonkeIsland", "SC_Effects"),
]


def load_or_create(tools, name, folder, cls, factory):
    path = f"{folder}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, folder, cls, factory)


def main():
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    master = unreal.load_asset(MASTER)

    classes = {}
    for channel, name in CHANNELS.items():
        sound_class = load_or_create(tools, name, CLASSES, unreal.SoundClass, unreal.SoundClassFactory())
        sound_class.set_editor_property("parent_class", master)
        unreal.EditorAssetLibrary.save_loaded_asset(sound_class)
        classes[name] = sound_class
    # the engine Master class is never saved: the master volume goes through the audio device instead

    mix = load_or_create(tools, MIX, DEST, unreal.SoundMix, unreal.SoundMixFactory())
    unreal.EditorAssetLibrary.save_loaded_asset(mix)

    rules = load_or_create(tools, RULES, DEST, unreal.SettingsRules, unreal.DataAssetFactory())
    rules.set_editor_property("sound_classes", {channel: classes[name] for channel, name in CHANNELS.items()})
    rules.set_editor_property("volume_mix", mix)
    unreal.EditorAssetLibrary.save_loaded_asset(rules)

    # every sound of the project in its channel
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    counts = {}
    for class_path in (unreal.TopLevelAssetPath("/Script/Engine", "SoundWave"), unreal.TopLevelAssetPath("/Script/Engine", "SoundCue")):
        for data in registry.get_assets_by_class(class_path):
            package = str(data.package_name)
            if not package.startswith("/Game/MonkeIsland"):
                continue
            name = next(n for prefix, n in FOLDERS if package.startswith(prefix))
            sound = unreal.load_asset(package)
            if sound.get_editor_property("sound_class_object") != classes[name]:
                sound.set_editor_property("sound_class_object", classes[name])
                unreal.EditorAssetLibrary.save_loaded_asset(sound)
            counts[name] = counts.get(name, 0) + 1
    unreal.log(f"[settings] {DEST}/{RULES}: {len(classes)} classes, mix {MIX}; sounds per class {counts}")


main()
