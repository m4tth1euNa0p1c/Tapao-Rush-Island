"""Imports the combat reaction clips (T-147) onto the existing mob skeletons of the project.

Sources (read only, delivered by the 2nd PC, see maquettes/combat-*/ue/INTEGRATION_UNREAL.txt):
  <MAQUETTES>/combat-dismemberment/exports/<Mob>/A_<Mob>_<Clip>.fbx   6 clips, armature + animation only (no mesh),
                                                                       UnitScaleFactor 1 (cm)
  <MAQUETTES>/combat-followups/exports/<Mob>/A_<Mob>_Musket_*.fbx       2 clips, armature + bind-pose mesh,
                                                                       UnitScaleFactor 100 (m)
Destination: /Game/MonkeIsland/Characters/<Mob>/Anims/Combat/A_<Mob>_<Clip>, on /Game/MonkeIsland/Characters/<Mob>/
Meshes/SK_<Mob>_Skeleton (bone names checked identical). Only the AnimSequences are imported: no SK_/SM_ parts,
no SK_Demo, no plugin. A mob without a skeleton in the project (BrambleStalker today) is skipped and reported.

Units: bConvertSceneUnit is on for every file. On a cm file (factor 1) it changes nothing; on a m file (factor 100)
it brings the translations to cm. Without it the followups come in 100x too small (measured: bounds 0.44 instead
of 43.6 on CocoWarrior).

The MCP cannot import an armature-only FBX, so this runs as an editor commandlet, EDITOR CLOSED (lock held):
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      "C:/Users/matth/monk-island/monke_island/monke_island.uproject"
      -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/import_combat_reactions.py"
      -unattended -nosplash -nullrhi -nosourcecontrol
Optional filters through the environment (pythonscript passes no argv reliably):
  MONKE_COMBAT_PACKS=dismemberment|followups|all (default dismemberment), MONKE_COMBAT_MOBS=CocoWarrior,Villager
Existing destination assets are replaced. Report: monke_island/Saved/MonkeTools/combat_reactions_report.json.

Dry run, outside Unreal (checks files, FBX unit factors and the destination plan, touches nothing):
  python scripts/character/import_combat_reactions.py --check
"""
import json
import os
import struct
import sys

MAQUETTES = r"C:\Users\matth\OneDrive\maquette-blender\maquettes"
PROJECT_CONTENT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "monke_island", "Content")
MOBS = ["CocoWarrior", "CrabGuardian", "AshWatcher", "BrambleStalker", "Villager"]
PACKS = {
	"dismemberment": ("combat-dismemberment",
		["Musket_Impact", "Musket_Burst", "Sabre_LeftToRight", "Sabre_RightToLeft", "Sabre_Down", "Sabre_Low"]),
	"followups": ("combat-followups", ["Musket_StaggerRecover", "Musket_ArticulatedFall"]),
}
FPS = 30


def selected():
	packs = os.environ.get("MONKE_COMBAT_PACKS", "dismemberment")
	packs = list(PACKS) if packs == "all" else packs.split(",")
	mobs = os.environ.get("MONKE_COMBAT_MOBS", ",".join(MOBS)).split(",")
	return packs, mobs


def plan(packs, mobs):
	for mob in mobs:
		for pack in packs:
			folder, clips = PACKS[pack]
			for clip in clips:
				yield mob, clip, os.path.join(MAQUETTES, folder, "exports", mob, f"A_{mob}_{clip}.fbx")


def skeleton_path(mob):
	return f"/Game/MonkeIsland/Characters/{mob}/Meshes/SK_{mob}_Skeleton"


def fbx_unit(path):
	"""UnitScaleFactor of a binary FBX (cm per file unit), None if not found."""
	data = open(path, "rb").read()
	i = data.find(b"UnitScaleFactor")
	if i < 0:
		return None
	j = data.find(b"D", data.find(b"Number", i))
	return struct.unpack("<d", data[j + 1:j + 9])[0]


def check():
	packs, mobs = selected()
	rows, missing = [], []
	for mob, clip, fbx in plan(packs, mobs):
		has_skel = os.path.isfile(os.path.join(PROJECT_CONTENT, "MonkeIsland", "Characters", mob, "Meshes",
			f"SK_{mob}_Skeleton.uasset"))
		if not os.path.isfile(fbx):
			missing.append(fbx)
			continue
		rows.append({"mob": mob, "clip": clip, "unit_cm": fbx_unit(fbx), "skeleton": has_skel,
			"dest": f"/Game/MonkeIsland/Characters/{mob}/Anims/Combat/A_{mob}_{clip}" if has_skel else "SKIPPED"})
	print(json.dumps({"packs": packs, "missing": missing, "to_import": sum(r["skeleton"] for r in rows),
		"skipped": sorted({r["mob"] for r in rows if not r["skeleton"]}), "rows": rows}, indent=1))
	return 0 if not missing else 1


def run_in_unreal():
	import unreal

	def task_for(fbx, dest, name, skeleton):
		opts = unreal.FbxImportUI()
		opts.import_mesh = False
		opts.import_animations = True
		opts.import_materials = False
		opts.import_textures = False
		opts.import_as_skeletal = True
		opts.skeleton = skeleton
		opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
		for data in (opts.skeletal_mesh_import_data, opts.anim_sequence_import_data):
			data.set_editor_property("convert_scene", True)
			data.set_editor_property("convert_scene_unit", True)
			data.set_editor_property("force_front_x_axis", False)
			data.set_editor_property("import_uniform_scale", 1.0)
		anim = opts.anim_sequence_import_data
		anim.set_editor_property("import_translation", unreal.Vector(0, 0, 0))
		anim.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
		anim.set_editor_property("import_bone_tracks", True)
		anim.set_editor_property("remove_redundant_keys", False)
		anim.set_editor_property("use_default_sample_rate", False)
		anim.set_editor_property("custom_sample_rate", FPS)
		task = unreal.AssetImportTask()
		task.filename = fbx
		task.destination_path = dest
		task.destination_name = name
		task.replace_existing = True
		task.automated = True
		task.save = True
		task.options = opts
		return task

	packs, mobs = selected()
	tools = unreal.AssetToolsHelpers.get_asset_tools()
	report = {"packs": packs, "imported": [], "failed": [], "skipped": []}
	for mob in mobs:
		skeleton = unreal.EditorAssetLibrary.load_asset(skeleton_path(mob)) \
			if unreal.EditorAssetLibrary.does_asset_exist(skeleton_path(mob)) else None
		if not skeleton:
			report["skipped"].append(f"{mob}: no skeleton {skeleton_path(mob)}")
			unreal.log_warning(f"[combat_reactions] {mob}: no skeleton in the project, skipped")
			continue
		dest = f"/Game/MonkeIsland/Characters/{mob}/Anims/Combat"
		tasks = [task_for(fbx, dest, f"A_{m}_{clip}", skeleton) for m, clip, fbx in plan(packs, [mob])]
		tools.import_asset_tasks(tasks)
		for t in tasks:
			path = f"{dest}/{t.destination_name}"
			seq = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
			if isinstance(seq, unreal.AnimSequence):
				entry = {"asset": path, "skeleton": seq.get_editor_property("skeleton").get_path_name(),
					"length_s": round(seq.get_play_length(), 3)}
				report["imported"].append(entry)
				unreal.log(f"[combat_reactions] {path}: {entry['length_s']} s on {entry['skeleton']}")
			else:
				report["failed"].append(f"{t.filename} -> {path}")
				unreal.log_error(f"[combat_reactions] FAILED {t.filename}")
	out = os.path.join(unreal.Paths.project_saved_dir(), "MonkeTools", "combat_reactions_report.json")
	os.makedirs(os.path.dirname(out), exist_ok=True)
	with open(out, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=1)
	unreal.log(f"[combat_reactions] imported {len(report['imported'])}, failed {len(report['failed'])}, "
		f"skipped {len(report['skipped'])} -> {out}")


if __name__ == "__main__":
	if "--check" in sys.argv:
		sys.exit(check())
	try:
		import unreal  # noqa: F401
	except ImportError:
		sys.exit("run inside Unreal (commandlet -run=pythonscript) or with --check")
	run_in_unreal()
