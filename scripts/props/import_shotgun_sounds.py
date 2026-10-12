"""Imports the shot of the pirate shotgun (art/audio/weapons, scripts/props/shotgun_shot_sound.py, T-167) into
/Game/MonkeIsland/Weapons/Shotgun/Audio as SoundWaves on the effects channel of the settings (SC_Effects), and sets
SW_Shotgun_Fire as the FireSound of DA_ShotgunRules (the effects script scripts/vfx/build_firearm_vfx_ue.py does it
too when the sound exists).

Editor Python console (Tools > Execute Python Script), or commandlet with the editor closed:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import os
import unreal

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons")
DEST = "/Game/MonkeIsland/Weapons/Shotgun/Audio"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
RULES = "/Game/MonkeIsland/Weapons/Shotgun/DA_ShotgunRules"
NAMES = ("SW_Shotgun_Fire", "SW_Shotgun_Fire_B")


def main():
	tools = unreal.AssetToolsHelpers.get_asset_tools()
	effects = unreal.load_asset(EFFECTS_CLASS) if unreal.EditorAssetLibrary.does_asset_exist(EFFECTS_CLASS) else None
	done = []
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
			unreal.log_error(f"[shotgun sounds] import failed: {name}")
			continue
		if effects:
			sound.set_editor_property("sound_class_object", effects)
		unreal.EditorAssetLibrary.save_loaded_asset(sound)
		done.append(sound)
		unreal.log(f"[shotgun sounds] {DEST}/{name} {sound.get_editor_property('duration'):.2f} s")
	rules = unreal.load_asset(RULES)
	if rules and done:
		rules.set_editor_property("fire_sound", done[0])
		unreal.EditorAssetLibrary.save_loaded_asset(rules)
	unreal.log(f"[shotgun sounds] {len(done)}/{len(NAMES)} sounds in {DEST}, SC_Effects: {bool(effects)}, FireSound set: {bool(rules and done)}")


main()
