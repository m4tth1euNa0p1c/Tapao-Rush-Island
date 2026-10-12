"""Fills the combat reaction clips (T-150) of the tiki Blueprints from the imported AnimSequences (T-147).

For each mob, reads /Game/MonkeIsland/Characters/<Mob>/Anims/Combat/A_<Mob>_<Clip> and writes it to
Anims.Combat.<Field> of the class defaults of BP_Tiki<Mob> (FTikiCombatReactions, Creatures/TikiCombat.h), then
saves the Blueprint. A missing clip leaves its field as it is (the game falls back on the next rule); a missing
Blueprint is reported and skipped (BrambleStalker has none in the project).

Run inside the editor (after the C++ build that adds the fields), with no PIE running:
  py "C:/Users/matth/monk-island/scripts/character/set_combat_reactions.py"            (Output Log, Python)
  or through the MCP: execute the file with the editor Python toolset.
Add --check (sys.argv) to only report what would be set.
"""
import sys

import unreal

ROOT = "/Game/MonkeIsland/Characters"
# mob -> Blueprint path (the villager Blueprint has no mob prefix)
BLUEPRINTS = {
	"CocoWarrior": f"{ROOT}/CocoWarrior/Blueprints/BP_TikiCocoWarrior",
	"CrabGuardian": f"{ROOT}/CrabGuardian/Blueprints/BP_TikiCrabGuardian",
	"AshWatcher": f"{ROOT}/AshWatcher/Blueprints/BP_TikiAshWatcher",
	"Villager": f"{ROOT}/Villager/Blueprints/BP_TikiVillager",
	"BrambleStalker": f"{ROOT}/BrambleStalker/Blueprints/BP_TikiBrambleStalker",
}
# clip name of the asset -> field of FTikiCombatReactions (snake_case in Python)
CLIPS = {
	"Musket_Impact": "musket_impact",
	"Musket_Burst": "musket_burst",
	"Musket_StaggerRecover": "musket_stagger_recover",
	"Musket_ArticulatedFall": "musket_articulated_fall",
	"Sabre_LeftToRight": "sabre_left_to_right",
	"Sabre_RightToLeft": "sabre_right_to_left",
	"Sabre_Down": "sabre_down",
	"Sabre_Low": "sabre_low",
}


def log(msg):
	unreal.log(f"[set_combat_reactions] {msg}")


def fill(mob, bp_path, check):
	if not unreal.EditorAssetLibrary.does_asset_exist(bp_path):
		log(f"{mob}: no Blueprint {bp_path}, skipped")
		return 0, len(CLIPS)
	generated = unreal.EditorAssetLibrary.load_blueprint_class(bp_path)
	if not generated:
		log(f"{mob}: {bp_path} has no generated class, skipped")
		return 0, len(CLIPS)
	cdo = unreal.get_default_object(generated)
	anims = cdo.get_editor_property("anims")
	combat = anims.get_editor_property("combat")
	found, missing = 0, 0
	for clip, field in CLIPS.items():
		path = f"{ROOT}/{mob}/Anims/Combat/A_{mob}_{clip}"
		asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
		if not isinstance(asset, unreal.AnimSequenceBase):
			log(f"{mob}: missing {path}")
			missing += 1
			continue
		found += 1
		if not check:
			combat.set_editor_property(field, asset)
	if not check and found:
		anims.set_editor_property("combat", combat)
		cdo.set_editor_property("anims", anims)
		unreal.EditorAssetLibrary.save_asset(bp_path, only_if_is_dirty=False)
	log(f"{mob}: {found} clips {'found' if check else 'set'}, {missing} missing")
	return found, missing


def main():
	check = "--check" in sys.argv
	total_found, total_missing = 0, 0
	for mob, bp_path in BLUEPRINTS.items():
		found, missing = fill(mob, bp_path, check)
		total_found += found
		total_missing += missing
	log(f"done: {total_found} clips {'found' if check else 'set'}, {total_missing} missing")


main()
