"""Imports the closed dismemberment modules (T-155) onto the existing mob skeletons of the project.

Source (read only, delivered by the 2nd PC, see combat-dismemberment/ue/INTEGRATION_UNREAL.txt):
  <MAQUETTES>/combat-dismemberment/exports/<Mob>/SK_<Mob>_<Part>.fbx   7 modules per mob (Head, Torso, Pelvis, Arm_L,
      Arm_R, Leg_L, Leg_R), skinned to the armature of the mob (bone names identical), cut faces closed and modelled.
      UnitScaleFactor 1 (cm): bConvertSceneUnit changes nothing on them, it is kept on as in import_combat_reactions.
  <MAQUETTES>/combat-dismemberment/combat-manifest.json                 parts (name, first bone) and materials (PBR).
  <MAQUETTES>/combat-dismemberment/textures/                             maps of the cut faces.
Destination: /Game/MonkeIsland/Characters/<Mob>/Dismember/SK_<Mob>_<Part>, on /Game/MonkeIsland/Characters/<Mob>/
Meshes/SK_<Mob>_Skeleton. Not imported: SM_ chunks, SK_Demo, the plugin (T-155 uses the SK modules only).

Materials of a module:
  MAT-CD-PBR-<Mob>_<Name>   outer surface -> the project material M_<Mob>_<Name> (Characters/<Mob>/Meshes), the slot
                            takes the slot name the body mesh gives it: at run time the module shows the material the
                            body shows (variants, overrides). Without a project material, built like a cut face.
  MAT-CD-PBR-<Mob>-<Organ>  cut face (Muscle, Fascia, Bone, Marrow) -> Dismember/Materials/M_<Mob>_Cut_<Organ>, a PBR
                            graph from the manifest (BaseColor sRGB, Roughness, Normal OpenGL flipped to DirectX).

Runs as an editor commandlet, EDITOR CLOSED (lock held), like import_combat_reactions.py:
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      "C:/Users/matth/monk-island/monke_island/monke_island.uproject"
      -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/import_dismember_parts.py"
      -unattended -nosplash -nullrhi -nosourcecontrol
Optional filter through the environment: MONKE_DISMEMBER_MOBS=CocoWarrior,Villager
Existing destination assets are replaced. Report: monke_island/Saved/MonkeTools/dismember_parts_report.json.
Then fill the DataAssets and the Blueprints: scripts/character/set_dismember_sets.py.

Dry run, outside Unreal (files synced, FBX units, materials and the destination plan; touches nothing):
  python scripts/character/import_dismember_parts.py --check
"""
import json
import os
import re
import struct
import sys
import time

MAQUETTES = r"C:\Users\matth\OneDrive\maquette-blender\maquettes"
PACK = os.path.join(MAQUETTES, "combat-dismemberment")
PROJECT_CONTENT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "monke_island", "Content")
MOBS = ["CocoWarrior", "CrabGuardian", "AshWatcher", "Villager", "BrambleStalker"]
PARTS = ["Head", "Torso", "Pelvis", "Arm_L", "Arm_R", "Leg_L", "Leg_R"]
ROOT = "/Game/MonkeIsland/Characters"
MATERIAL_RE = re.compile(r"MAT-CD-PBR-(?P<mob>[A-Za-z0-9]+)(?P<sep>[_-])(?P<name>[A-Za-z0-9_]+)")


def selected_mobs():
	return [m for m in os.environ.get("MONKE_DISMEMBER_MOBS", ",".join(MOBS)).split(",") if m]


def load_manifest():
	with open(os.path.join(PACK, "combat-manifest.json"), encoding="utf-8-sig") as f:
		return json.load(f)


def character(manifest, mob):
	return next((c for c in manifest.get("characters", []) if c.get("name") == mob), None)


def module_fbx(mob, part):
	return os.path.join(PACK, "exports", mob, f"SK_{mob}_{part}.fbx")


def skeleton_path(mob):
	return f"{ROOT}/{mob}/Meshes/SK_{mob}_Skeleton"


def has_project_asset(game_path):
	rel = game_path.replace("/Game/", "", 1).replace("/", os.sep) + ".uasset"
	return os.path.isfile(os.path.join(PROJECT_CONTENT, rel))


def fbx_unit(path):
	"""UnitScaleFactor of a binary FBX (cm per file unit), None if not found."""
	data = open(path, "rb").read()
	i = data.find(b"UnitScaleFactor")
	if i < 0:
		return None
	j = data.find(b"D", data.find(b"Number", i))
	return struct.unpack("<d", data[j + 1:j + 9])[0]


def fbx_materials(path):
	data = open(path, "rb").read()
	return sorted(set(m.decode() for m in re.findall(rb"MAT-CD-PBR-[A-Za-z0-9_\-]+", data)))


def is_synced(path):
	"""OneDrive: the file is complete (size stable, no temporary file next to it)."""
	if not os.path.isfile(path) or os.path.getsize(path) == 0:
		return False
	size = os.path.getsize(path)
	time.sleep(0.05)
	folder = os.path.dirname(path)
	temporary = [f for f in os.listdir(folder) if f.startswith("~") or f.endswith((".tmp", ".partial"))]
	return os.path.getsize(path) == size and not temporary


def material_plan(mob, slot):
	"""(kind, destination) of an FBX material: ('outer', M_<Mob>_<Name>) or ('cut', M_<Mob>_Cut_<Organ>)."""
	match = MATERIAL_RE.fullmatch(slot)
	if not match:
		return "unknown", None
	name = match.group("name")
	if match.group("sep") == "_":
		project = f"{ROOT}/{mob}/Meshes/M_{mob}_{name}"
		if has_project_asset(project):
			return "outer", project
		return "built", f"{ROOT}/{mob}/Dismember/Materials/M_{mob}_{name}"
	return "cut", f"{ROOT}/{mob}/Dismember/Materials/M_{mob}_Cut_{name}"


def check():
	manifest = load_manifest()
	rows, problems = [], []
	for mob in selected_mobs():
		info = character(manifest, mob)
		has_skel = has_project_asset(skeleton_path(mob))
		if not info:
			problems.append(f"{mob}: not in the manifest")
			continue
		bones = {p["name"]: p["bone"] for p in info.get("parts", [])}
		for part in PARTS:
			fbx = module_fbx(mob, part)
			if not is_synced(fbx):
				problems.append(f"missing or not synced: {fbx}")
				continue
			unit = fbx_unit(fbx)
			plan = {slot: material_plan(mob, slot) for slot in fbx_materials(fbx)}
			unknown = [s for s, (kind, _) in plan.items() if kind == "unknown"]
			if unit not in (1.0, 100.0):
				problems.append(f"{fbx}: unexpected UnitScaleFactor {unit}")
			if unknown:
				problems.append(f"{fbx}: unknown materials {unknown}")
			for slot, (kind, _) in plan.items():
				settings = manifest.get("materials", {}).get(slot)
				if kind in ("cut", "built") and not settings:
					problems.append(f"{fbx}: no manifest material for {slot}")
				for field in ("base_color_texture", "roughness_texture", "normal_texture"):
					if kind in ("cut", "built") and settings and settings.get(field) \
						and not os.path.isfile(os.path.join(PACK, settings[field])):
						problems.append(f"{slot}: missing texture {settings[field]}")
			rows.append({"mob": mob, "part": part, "bone": bones.get(part), "unit_cm": unit,
				"outer": sum(k == "outer" for k, _ in plan.values()),
				"cut": sorted(d.rsplit("/", 1)[1] for k, d in plan.values() if k == "cut"),
				"built": sorted(d.rsplit("/", 1)[1] for k, d in plan.values() if k == "built"),
				"dest": f"{ROOT}/{mob}/Dismember/SK_{mob}_{part}" if has_skel else "SKIPPED (no skeleton)"})
	print(json.dumps({"pack": PACK, "to_import": sum(not r["dest"].startswith("SKIPPED") for r in rows),
		"skipped": sorted({r["mob"] for r in rows if r["dest"].startswith("SKIPPED")}),
		"problems": problems, "rows": rows}, indent=1))
	return 0 if not problems else 1


def run_in_unreal():
	import unreal

	assets = unreal.AssetToolsHelpers.get_asset_tools()
	eal = unreal.EditorAssetLibrary
	mel = unreal.MaterialEditingLibrary
	manifest = load_manifest()
	report = {"imported": [], "failed": [], "skipped": [], "materials": [], "errors": []}
	textures = {}

	def texture(path, dest, name, suffix, flip_green):
		key = (path, suffix)
		if key in textures:
			return textures[key]
		task = unreal.AssetImportTask()
		task.filename = os.path.join(PACK, path)
		task.destination_path = dest
		task.destination_name = name
		task.automated = True
		task.replace_existing = True
		task.save = True
		assets.import_asset_tasks([task])
		image = next((o for o in (eal.load_asset(p) for p in task.imported_object_paths) if isinstance(o, unreal.Texture2D)), None)
		if image is None:
			raise RuntimeError(f"texture import failed: {path}")
		image.set_editor_property("srgb", suffix == "BaseColor")
		if suffix == "Normal":
			image.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
			image.set_editor_property("flip_green_channel", flip_green)
		elif suffix == "Roughness":
			image.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
		eal.save_loaded_asset(image)
		textures[key] = image
		return image

	def build_material(slot, game_path):
		"""PBR graph of a cut face (or of an outer surface the project lacks), from the manifest."""
		if game_path in built:
			return eal.load_asset(game_path)
		settings = manifest.get("materials", {}).get(slot, {})
		folder, name = game_path.rsplit("/", 1)
		material = eal.load_asset(game_path) if eal.does_asset_exist(game_path) else \
			assets.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
		if not isinstance(material, unreal.Material):
			raise RuntimeError(f"{game_path} is not a material")
		mel.delete_all_material_expressions(material)
		material.set_editor_property("used_with_skeletal_mesh", True)
		flip = bool(settings.get("normal_flip_green", True))
		tex_folder = folder + "/Textures"
		maps = [("base_color_texture", "BaseColor", unreal.MaterialProperty.MP_BASE_COLOR, unreal.MaterialSamplerType.SAMPLERTYPE_COLOR, "RGB", 0),
			("roughness_texture", "Roughness", unreal.MaterialProperty.MP_ROUGHNESS, unreal.MaterialSamplerType.SAMPLERTYPE_MASKS, "R", 300),
			("normal_texture", "Normal", unreal.MaterialProperty.MP_NORMAL, unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL, "RGB", 600)]
		for field, suffix, prop, sampler, output, y in maps:
			if settings.get(field):
				image = texture(settings[field], tex_folder, "T_" + name[2:] + "_" + suffix, suffix, flip)
				node = mel.create_material_expression(material, unreal.MaterialExpressionTextureSample, -640, y)
				node.set_editor_property("texture", image)
				node.set_editor_property("sampler_type", sampler)
				mel.connect_material_property(node, output, prop)
			elif suffix == "BaseColor":
				color = (list(settings.get("base_color", [0.3, 0.02, 0.02, 1.0])) + [1.0])[:4]
				node = mel.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -320, y)
				node.set_editor_property("constant", unreal.LinearColor(*color))
				mel.connect_material_property(node, "", prop)
			elif suffix == "Roughness":
				node = mel.create_material_expression(material, unreal.MaterialExpressionConstant, -320, y)
				node.set_editor_property("r", float(settings.get("roughness", 0.4)))
				mel.connect_material_property(node, "", prop)
		mel.recompile_material(material)
		eal.save_loaded_asset(material)
		built.add(game_path)
		report["materials"].append(game_path)
		return material

	built = set()
	for mob in selected_mobs():
		info = character(manifest, mob)
		if not info or not eal.does_asset_exist(skeleton_path(mob)):
			report["skipped"].append(f"{mob}: no skeleton {skeleton_path(mob)}" if info else f"{mob}: not in the manifest")
			unreal.log_warning(f"[dismember_parts] {mob}: skipped")
			continue
		skeleton = eal.load_asset(skeleton_path(mob))
		body = eal.load_asset(f"{ROOT}/{mob}/Meshes/SK_{mob}")
		# slot name the body gives each of its materials
		body_slots = {}
		if body:
			for slot in body.get_editor_property("materials"):
				mat = slot.get_editor_property("material_interface")
				if mat:
					body_slots[mat.get_path_name().split(".")[0]] = slot.get_editor_property("material_slot_name")
		dest = f"{ROOT}/{mob}/Dismember"
		for part in PARTS:
			fbx = module_fbx(mob, part)
			name = f"SK_{mob}_{part}"
			opts = unreal.FbxImportUI()
			opts.import_mesh = True
			opts.import_as_skeletal = True
			opts.import_animations = False
			opts.import_materials = False
			opts.import_textures = False
			opts.create_physics_asset = False
			opts.skeleton = skeleton
			opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
			data = opts.skeletal_mesh_import_data
			data.set_editor_property("convert_scene", True)
			data.set_editor_property("convert_scene_unit", True)
			data.set_editor_property("force_front_x_axis", False)
			data.set_editor_property("import_uniform_scale", 1.0)
			data.set_editor_property("import_morph_targets", False)
			data.set_editor_property("update_skeleton_reference_pose", False)
			task = unreal.AssetImportTask()
			task.filename = fbx
			task.destination_path = dest
			task.destination_name = name
			task.replace_existing = True
			task.automated = True
			task.save = True
			task.options = opts
			assets.import_asset_tasks([task])
			path = f"{dest}/{name}"
			mesh = eal.load_asset(path) if eal.does_asset_exist(path) else None
			if not isinstance(mesh, unreal.SkeletalMesh) or mesh.get_editor_property("skeleton") != skeleton:
				report["failed"].append(f"{fbx} -> {path}")
				unreal.log_error(f"[dismember_parts] FAILED {fbx} (no mesh or not on {skeleton_path(mob)})")
				continue
			slots = list(mesh.get_editor_property("materials"))
			kinds = {"outer": 0, "cut": 0, "built": 0, "unknown": 0}
			for slot in slots:
				imported = str(slot.get_editor_property("imported_material_slot_name")) or str(slot.get_editor_property("material_slot_name"))
				kind, target = material_plan(mob, imported)
				kinds[kind] += 1
				try:
					if kind == "outer":
						material = eal.load_asset(target)
						slot.set_editor_property("material_interface", material)
						if target in body_slots:
							slot.set_editor_property("material_slot_name", body_slots[target])
					elif kind in ("cut", "built"):
						slot.set_editor_property("material_interface", build_material(imported, target))
				except Exception as exc:  # noqa: BLE001 - reported, the module keeps its slot
					report["errors"].append({"mesh": path, "slot": imported, "error": str(exc)})
					unreal.log_error(f"[dismember_parts] {path} {imported}: {exc}")
			mesh.set_editor_property("materials", slots)
			eal.save_loaded_asset(mesh)
			bounds = mesh.get_bounds()
			entry = {"asset": path, "bone": next((p["bone"] for p in info["parts"] if p["name"] == part), None),
				"slots": kinds, "extent_cm": [round(v, 1) for v in (bounds.box_extent.x, bounds.box_extent.y, bounds.box_extent.z)],
				"body_extent_cm": [round(v, 1) for v in (body.get_bounds().box_extent.x, body.get_bounds().box_extent.y,
					body.get_bounds().box_extent.z)] if body else None}
			report["imported"].append(entry)
			unreal.log(f"[dismember_parts] {path}: {kinds}, extent {entry['extent_cm']} (body {entry['body_extent_cm']})")
	out = os.path.join(unreal.Paths.project_saved_dir(), "MonkeTools", "dismember_parts_report.json")
	os.makedirs(os.path.dirname(out), exist_ok=True)
	with open(out, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=1)
	unreal.log(f"[dismember_parts] imported {len(report['imported'])}, failed {len(report['failed'])}, "
		f"skipped {len(report['skipped'])}, errors {len(report['errors'])} -> {out}")


if __name__ == "__main__":
	if "--check" in sys.argv:
		sys.exit(check())
	try:
		import unreal  # noqa: F401
	except ImportError:
		sys.exit("run inside Unreal (commandlet -run=pythonscript) or with --check")
	run_in_unreal()
