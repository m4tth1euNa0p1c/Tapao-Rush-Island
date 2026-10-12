"""Imports the machine gun pack (T-272, build/packs/bean-machinegun) into Unreal: lead session action "mgimport"
(scripts/editor/lead_session_t186.py; editor open, lock held, no game running). Run by the lead's runner:
  bash <jobs>/tmp/run_action_locked.sh mgimport 1800

Staged, stopping at the first failure (a failed FBX import leaves the importer unstable until the editor restarts):
  0. preflight, no import: the manifest, every file it lists (sha256), FBX units, no file still being written;
  1. ONE file: the gun's base skeletal mesh SK_Bean_MachineGun (its own skeleton): checked (skeleton, bones of the
     contract, size against the manifest) before anything else is imported;
  2. the generalized import, scripts/character/import_pirate_weapons.py with MONKE_PIRATE_WEAPONS=MachineGun (registry
     entry "MachineGun"): base again (replaced), FP arms on the project's bean skeleton, the 52 clips (bean / gun), the
     statics (SM_Bean_MachineGun with its UCX collision, SM_Ammo_MGBox, SM_MG_Casing, SM_MG_BeltLink), the PBR textures
     and the instances MI_MachineGun_{Body,Jacket,Belt,Box,Casing} of M_Character_Baked; its report must have no failure;
  3. best effort (logged, never raised): the barrel's glow mask T_MachineGun_Jacket_Heat as the Emissive texture of
     MI_MachineGun_Jacket with EmissiveStrength 0 (the game raises it with the heat: UMachineGunRules.HeatParameterName),
     LODs of the gun (skeletal: 3 levels) and of the pickup, box and case (static reduction), the bean's material
     instances on the FP arms (slot M_Bean_<X> -> MI_Bean_<X>, as scripts/editor/weapons_fp_arms_ue.py does for the
     other packs: a mesh imported in the same action keeps its placeholders, so a replay of the action sets them).
A replay with the same manifest (sha256 in the report) and every asset in place skips stages 1 and 2 and only does 3
(MONKE_MG_FORCE_IMPORT=1 forces the import).
Destination /Game/MonkeIsland/Weapons/MachineGun/. Report: monke_island/Saved/T272/mgimport.json (+ the generalized
report Saved/MonkeTools/pirate_weapons_report.json). The data (DA_MachineGunRules, BP, items, shop, test map) is T-273's
action "mgsetup" (scripts/editor/machinegun_setup_ue.py).

Dry run outside Unreal (stage 0 only):  python scripts/props/machinegun_pack/import_machinegun_ue.py --check
"""
import datetime as dt
import importlib
import json
import os
import sys
import time
import traceback

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", "..", ".."))
CHAR = os.path.join(ROOT, "scripts", "character")
if CHAR not in sys.path:
	sys.path.insert(0, CHAR)
PACK = os.path.join(ROOT, "build", "packs", "bean-machinegun")
DEST = "/Game/MonkeIsland/Weapons/MachineGun"
REPORT = os.path.join(ROOT, "monke_island", "Saved", "T272", "mgimport.json")
LOD_STATICS = {"SM_Bean_MachineGun": [(1.0, 1.0), (0.5, 0.35), (0.22, 0.12)], "SM_Ammo_MGBox": [(1.0, 1.0), (0.5, 0.2)],
	"SM_MG_Casing": [(1.0, 1.0), (0.45, 0.05)]}


def preflight(imp):
	"""Stage 0: the pack is complete and settled (nothing written in the last 20 s)."""
	m = imp.manifest("MachineGun")
	missing, mismatch, units, newest = [], [], {}, 0.0
	for entry in m["assets"]:
		path = os.path.join(PACK, entry["path"])
		if not os.path.isfile(path):
			missing.append(entry["path"])
			continue
		newest = max(newest, os.path.getmtime(path))
		if entry.get("sha256") and imp.sha256(path) != entry["sha256"]:
			mismatch.append(entry["path"])
		if path.lower().endswith(".fbx"):
			u = str(imp.fbx_unit(path))
			units[u] = units.get(u, 0) + 1
	tex = imp.materials("MachineGun")
	for mat, files in tex.items():
		for param, path in files.items():
			if not os.path.isfile(path):
				missing.append(path)
	result = {"manifest": f"{m['format']} v{m['version']}", "assets": len(m["assets"]), "missing": missing, "sha256_mismatch": mismatch,
		"fbx_units": units, "textured_materials": sorted(tex), "settled_seconds": round(time.time() - newest, 1),
		"base": imp.bases("MachineGun"), "statics": imp.statics("MachineGun"), "clips": len(imp.animation_plan("MachineGun"))}
	problems = []
	if missing:
		problems.append(f"missing files: {missing[:5]}")
	if mismatch:
		problems.append(f"sha256 mismatch: {mismatch[:5]}")
	if set(units) - {"100.0"}:
		problems.append(f"FBX units {units}")
	if result["settled_seconds"] < 20:
		problems.append(f"the pack was written {result['settled_seconds']} s ago (a build still running?)")
	if not result["base"]["base"] or not result["base"]["fp"]:
		problems.append(f"bases {result['base']}")
	result["problems"] = problems
	return result


def stage_one(imp, unreal):
	"""Stage 1: only the gun's base skeletal mesh, checked."""
	eal = unreal.EditorAssetLibrary
	base = imp.bases("MachineGun")["base"]
	fbx = os.path.join(PACK, "exports", "Bases", base + ".fbx")
	opts = unreal.FbxImportUI()
	opts.import_mesh = True
	opts.import_animations = False
	opts.import_as_skeletal = True
	opts.import_materials = True
	opts.import_textures = True
	opts.create_physics_asset = False
	opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
	data = opts.skeletal_mesh_import_data
	data.set_editor_property("convert_scene", True)
	data.set_editor_property("convert_scene_unit", True)
	data.set_editor_property("force_front_x_axis", False)
	data.set_editor_property("import_uniform_scale", 1.0)
	data.set_editor_property("update_skeleton_reference_pose", False)
	data.set_editor_property("import_morph_targets", False)
	task = unreal.AssetImportTask()
	task.filename = fbx
	task.destination_path = DEST
	task.destination_name = base
	task.replace_existing = True
	task.automated = True
	task.save = True
	task.options = opts
	unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
	path = f"{DEST}/{base}"
	mesh = eal.load_asset(path) if eal.does_asset_exist(path) else None
	if not mesh or not isinstance(mesh, unreal.SkeletalMesh):
		raise RuntimeError(f"stage 1: {fbx} did not import as a skeletal mesh ({path})")
	skeleton = mesh.get_editor_property("skeleton")
	if not skeleton:
		raise RuntimeError(f"stage 1: {path} has no skeleton")
	contract = imp.manifest("MachineGun")["weapon_contract"]
	extent = mesh.get_bounds().box_extent
	size = [round(extent.x * 2, 1), round(extent.y * 2, 1), round(extent.z * 2, 1)]
	expected = [round(v * 100, 1) for v in contract["bounds"]["size"]]
	if any(abs(a - b) > 3.0 for a, b in zip(size, expected)):
		raise RuntimeError(f"stage 1: {path} is {size} cm, the pack says {expected} cm (scale problem: nothing else imported)")
	return {"asset": path, "skeleton": skeleton.get_path_name(), "size_cm": size, "manifest_size_cm": expected,
		"contract_bones": len(contract["bones"])}


def heat_mask(imp, unreal, report):
	"""Stage 3a: T_MachineGun_Jacket_Heat as the Emissive of MI_MachineGun_Jacket, EmissiveStrength 0."""
	eal = unreal.EditorAssetLibrary
	mel = unreal.MaterialEditingLibrary
	mats = imp.manifest("MachineGun").get("materials", {})
	heat = (mats.get("M_MG_Jacket", {}).get("textures") or {}).get("Heat")
	if not heat:
		report["heat"] = "no Heat texture in the manifest"
		return
	png = os.path.join(PACK, heat["path"] if isinstance(heat, dict) else heat)
	task = unreal.AssetImportTask()
	task.filename = png
	task.destination_path = f"{DEST}/Textures"
	task.destination_name = "T_MachineGun_Jacket_Heat"
	task.replace_existing = True
	task.automated = True
	task.save = True
	unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
	tex = eal.load_asset(f"{DEST}/Textures/T_MachineGun_Jacket_Heat")
	mi = eal.load_asset(f"{DEST}/Materials/MI_MachineGun_Jacket") if eal.does_asset_exist(f"{DEST}/Materials/MI_MachineGun_Jacket") else None
	if not tex or not mi:
		report["heat"] = f"texture {bool(tex)}, instance {bool(mi)}"
		return
	tex.set_editor_property("srgb", True)
	eal.save_loaded_asset(tex)
	mel.set_material_instance_texture_parameter_value(mi, "Emissive", tex)
	mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.0)
	eal.save_loaded_asset(mi)
	report["heat"] = {"texture": tex.get_path_name(), "instance": mi.get_path_name(), "EmissiveStrength": 0.0}


def lods(unreal, report):
	"""Stage 3b: LODs of the gun (skeletal reduction) and of the statics (static reduction)."""
	eal = unreal.EditorAssetLibrary
	out = {}
	try:
		sk = eal.load_asset(f"{DEST}/SK_Bean_MachineGun")
		sub = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
		ok = sub.regenerate_lod(sk, 3, False, False)
		eal.save_loaded_asset(sk)
		out["SK_Bean_MachineGun"] = {"regenerated": bool(ok), "lods": sk.get_lod_num() if hasattr(sk, "get_lod_num") else None}
	except Exception as error:  # noqa: BLE001 - optional
		out["SK_Bean_MachineGun"] = f"skipped: {error!r}"
	sub = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
	for name, levels in LOD_STATICS.items():
		path = f"{DEST}/Static/{name}"
		try:
			mesh = eal.load_asset(path)
			if hasattr(unreal, "StaticMeshReductionOptions"):
				opts = unreal.StaticMeshReductionOptions()
				settings = [unreal.StaticMeshReductionSettings(percent_triangles=p, screen_size=s) for p, s in levels]
			else:
				opts = unreal.EditorScriptingMeshReductionOptions()
				settings = [unreal.EditorScriptingMeshReductionSettings(percent_triangles=p, screen_size=s) for p, s in levels]
			opts.set_editor_property("auto_compute_lod_screen_size", False)
			opts.set_editor_property("reduction_settings", settings)
			n = sub.set_lods_with_notification(mesh, opts, True)
			eal.save_loaded_asset(mesh)
			out[name] = {"lods": n, "levels": levels}
		except Exception as error:  # noqa: BLE001 - optional
			out[name] = f"skipped: {error!r}"
	report["lods"] = out


def fp_arm_materials(unreal, report):
	"""Stage 3c: the bean's instances on the FP arms' slots (imported names M_Bean_<Part>, maybe suffixed _001)."""
	import re
	eal = unreal.EditorAssetLibrary
	path = f"{DEST}/SK_Bean_MachineGun_FP"
	mesh = eal.load_asset(path) if eal.does_asset_exist(path) else None
	if not mesh:
		report["fp_arms"] = "missing"
		return
	slots = list(mesh.get_editor_property("materials"))
	rows = []
	for slot in slots:
		imported = str(slot.get_editor_property("imported_material_slot_name"))
		base = re.sub(r"_\d{3}$", "", imported)
		if base.startswith("M_Bean_"):
			candidate = f"/Game/MonkeIsland/Characters/Bean/Materials/MI_Bean_{base[len('M_Bean_'):]}"
			mi = eal.load_asset(candidate) if eal.does_asset_exist(candidate) else None
			if mi:
				slot.set_editor_property("material_interface", mi)
			rows.append([imported, mi.get_name() if mi else "no MI"])
	mesh.set_editor_property("materials", slots)
	saved = eal.save_asset(path, only_if_is_dirty=False)
	check = [[str(s.get_editor_property("imported_material_slot_name")),
		s.get_editor_property("material_interface").get_name() if s.get_editor_property("material_interface") else None]
		for s in mesh.get_editor_property("materials")]
	report["fp_arms"] = {"set": rows, "saved": saved, "check": check}


def already_imported(imp, unreal):
	"""True when the previous run passed on this very manifest and its assets are all in the project."""
	if os.environ.get("MONKE_MG_FORCE_IMPORT") or not os.path.isfile(REPORT):
		return False
	try:
		with open(REPORT, encoding="utf-8") as f:
			previous = json.load(f)
	except (OSError, ValueError):
		return False
	if not previous.get("passed") or previous.get("manifest_sha256") != imp.sha256(os.path.join(PACK, "machinegun-manifest.json")):
		return False
	eal = unreal.EditorAssetLibrary
	base = imp.bases("MachineGun")
	needed = [f"{DEST}/{base['base']}", f"{DEST}/{base['fp']}"] + [f"{DEST}/Static/{n}" for n, _ in imp.statics("MachineGun")]
	needed += [f"{folder}/{name}" for _, folder, name, _, _, _ in imp.animation_plan("MachineGun")]
	return all(eal.does_asset_exist(p) for p in needed)


def write(report):
	os.makedirs(os.path.dirname(REPORT), exist_ok=True)
	report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
	with open(REPORT, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=1, ensure_ascii=False)


def main():
	import unreal

	previous = os.environ.get("MONKE_PIRATE_WEAPONS")
	os.environ["MONKE_PIRATE_WEAPONS"] = "MachineGun"
	report = {"task": "T-272", "pack": PACK.replace(os.sep, "/"), "stages": {}}
	try:
		import import_pirate_weapons as imp
		importlib.reload(imp)
		pre = preflight(imp)
		report["stages"]["0_preflight"] = pre
		report["manifest_sha256"] = imp.sha256(os.path.join(PACK, "machinegun-manifest.json"))
		if pre["problems"]:
			write(report)
			raise RuntimeError(f"preflight refused (no import done): {pre['problems']}")
		if already_imported(imp, unreal):
			report["stages"]["1_2_skipped"] = "same manifest already imported (every asset in place): extras only"
		else:
			report["stages"]["1_base_only"] = stage_one(imp, unreal)
			write(report)
			imp.run_in_unreal()
			with open(os.path.join(unreal.Paths.project_saved_dir(), "MonkeTools", "pirate_weapons_report.json"), encoding="utf-8") as f:
				general = json.load(f)
			report["stages"]["2_generalized"] = {"imported": len(general["imported"]), "failed": general["failed"], "checks": general["checks"],
				"materials": general["materials"]}
			write(report)
			if general["failed"]:
				raise RuntimeError(f"generalized import: {len(general['failed'])} failed, first {general['failed'][:3]}")
		extras = {}
		try:
			heat_mask(imp, unreal, extras)
		except Exception as error:  # noqa: BLE001 - optional, logged
			extras["heat"] = f"failed: {error!r}"
		try:
			lods(unreal, extras)
		except Exception as error:  # noqa: BLE001
			extras["lods"] = f"failed: {error!r}"
		try:
			fp_arm_materials(unreal, extras)
		except Exception as error:  # noqa: BLE001
			extras["fp_arms"] = f"failed: {error!r}"
		report["stages"]["3_extras"] = extras
		report["passed"] = True
		write(report)
		# never raise past this point: the lead session takes any failed action whose error mentions "import" for a
		# broken FBX importer and refuses every action until the editor restarts (2026-10-10: a KeyError here did that)
		try:
			done = report["stages"].get("2_generalized", {}).get("imported", "none (already imported)")
			unreal.log(f"[mgimport] done: {done} assets imported, extras {json.dumps(extras, default=str)[:400]}")
		except Exception:  # noqa: BLE001
			pass
	except Exception:
		report["passed"] = False
		report["error"] = traceback.format_exc()
		write(report)
		raise
	finally:
		if previous is None:
			os.environ.pop("MONKE_PIRATE_WEAPONS", None)
		else:
			os.environ["MONKE_PIRATE_WEAPONS"] = previous


if __name__ == "__main__":
	if "--check" in sys.argv:
		sys.path.insert(0, CHAR)
		os.environ["MONKE_PIRATE_WEAPONS"] = "MachineGun"
		import import_pirate_weapons as imp_check
		result = preflight(imp_check)
		print(json.dumps(result, indent=1, ensure_ascii=False))
		sys.exit(1 if result["problems"] else 0)
	main()
