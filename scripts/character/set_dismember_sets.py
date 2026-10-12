"""Fills the dismemberment sets of the tiki (T-155) from the imported modules (import_dismember_parts.py).

For each mob: creates or updates /Game/MonkeIsland/Characters/<Mob>/Dismember/DA_<Mob>_DismemberSet
(UTikiDismemberSet, Creatures/TikiDismember.h) with its 7 modules (Part, Bone, Mesh), then writes it to
DismemberSet of the class defaults of BP_Tiki<Mob> (ATikiCreature) and saves both. A set is only written when the 7
modules exist (the game falls back on hidden bones with an incomplete set anyway); a missing Blueprint is reported and
skipped (BrambleStalker has none in the project).

Needs the editor built with the T-155 C++ (UTikiDismemberSet). Run with no PIE running, in the editor:
  py "C:/Users/matth/monk-island/scripts/character/set_dismember_sets.py"            (Output Log, Python)
or as a commandlet, editor closed (lock held), right after the import:
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      "C:/Users/matth/monk-island/monke_island/monke_island.uproject"
      -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/set_dismember_sets.py"
      -unattended -nosplash -nullrhi -nosourcecontrol
Add --check (sys.argv) or MONKE_DISMEMBER_CHECK=1 to only report what would be set.
"""
import os
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
# part -> first bone (combat-manifest.json characters[].parts, identical for every mob)
PARTS = {
	"Head": "head",
	"Torso": "spine_02",
	"Pelvis": "pelvis",
	"Arm_L": "upperarm_l",
	"Arm_R": "upperarm_r",
	"Leg_L": "thigh_l",
	"Leg_R": "thigh_r",
}

eal = unreal.EditorAssetLibrary


def log(msg):
	unreal.log(f"[set_dismember_sets] {msg}")


def modules_of(mob):
	modules, missing = [], []
	for part, bone in PARTS.items():
		path = f"{ROOT}/{mob}/Dismember/SK_{mob}_{part}"
		mesh = eal.load_asset(path) if eal.does_asset_exist(path) else None
		if not isinstance(mesh, unreal.SkeletalMesh):
			missing.append(path)
			continue
		module = unreal.TikiDismemberModule()
		module.set_editor_property("part", part)
		module.set_editor_property("bone", bone)
		module.set_editor_property("mesh", mesh)
		modules.append(module)
	return modules, missing


def data_asset(mob):
	folder = f"{ROOT}/{mob}/Dismember"
	name = f"DA_{mob}_DismemberSet"
	path = f"{folder}/{name}"
	if eal.does_asset_exist(path):
		return eal.load_asset(path)
	factory = unreal.DataAssetFactory()
	factory.set_editor_property("data_asset_class", unreal.TikiDismemberSet)
	return unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.TikiDismemberSet, factory)


def fill(mob, bp_path, check):
	modules, missing = modules_of(mob)
	if missing:
		log(f"{mob}: {len(missing)} module(s) missing ({', '.join(m.rsplit('/', 1)[1] for m in missing)}), not set")
		return False
	if not eal.does_asset_exist(bp_path):
		log(f"{mob}: no Blueprint {bp_path}, skipped")
		return False
	if check:
		log(f"{mob}: 7 modules found, would set DA_{mob}_DismemberSet on {bp_path}")
		return True
	asset = data_asset(mob)
	if not isinstance(asset, unreal.TikiDismemberSet):
		log(f"{mob}: could not create DA_{mob}_DismemberSet (C++ of T-155 built?)")
		return False
	asset.set_editor_property("modules", modules)
	eal.save_loaded_asset(asset, only_if_is_dirty=False)
	generated = eal.load_blueprint_class(bp_path)
	if not generated:
		log(f"{mob}: {bp_path} has no generated class, skipped")
		return False
	cdo = unreal.get_default_object(generated)
	cdo.set_editor_property("dismember_set", asset)
	eal.save_asset(bp_path, only_if_is_dirty=False)
	log(f"{mob}: {asset.get_path_name()} set on {bp_path}")
	return True


def main():
	check = "--check" in sys.argv or os.environ.get("MONKE_DISMEMBER_CHECK") == "1"
	done = sum(fill(mob, bp, check) for mob, bp in BLUEPRINTS.items())
	log(f"done: {done}/{len(BLUEPRINTS)} mobs {'ready' if check else 'set'}")


main()
