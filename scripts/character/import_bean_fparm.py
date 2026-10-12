"""Reimports the bean's first person arm SM_Bean_FPArm (T-212: cuff, buttons and mitten no longer floating) and puts the
bean's material instances on its slots (slot M_Bean_<X> -> /Game/MonkeIsland/Characters/Bean/Materials/MI_Bean_<X>).

Source: art/export/Characters/BeanSoldier/SM_Bean_FPArm.fbx, written by
  blender -b --factory-startup --python scripts/character/bean_soldier.py -- --export
Destination: /Game/MonkeIsland/Characters/Bean/Meshes/SM_Bean_FPArm (replaced, no collision: the arm is only seen).

Editor closed and lock held (scripts/editor_lock.py), as the other character imports:
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      "C:/Users/matth/monk-island/monke_island/monke_island.uproject"
      -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/import_bean_fparm.py"
      -unattended -nosplash -nullrhi -nosourcecontrol
Report: Saved/MonkeTools/bean_fparm_report.json.
"""
import json
import os

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
FBX = os.path.normpath(os.path.join(HERE, "..", "..", "art", "export", "Characters", "BeanSoldier", "SM_Bean_FPArm.fbx"))
FOLDER = "/Game/MonkeIsland/Characters/Bean/Meshes"
NAME = "SM_Bean_FPArm"
MATERIALS = "/Game/MonkeIsland/Characters/Bean/Materials"


def main():
	eal = unreal.EditorAssetLibrary
	opts = unreal.FbxImportUI()
	opts.import_mesh = True
	opts.import_animations = False
	opts.import_as_skeletal = False
	opts.import_materials = False
	opts.import_textures = False
	opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
	data = opts.static_mesh_import_data
	data.set_editor_property("convert_scene", True)
	data.set_editor_property("convert_scene_unit", True)
	data.set_editor_property("force_front_x_axis", False)
	data.set_editor_property("import_uniform_scale", 1.0)
	data.set_editor_property("auto_generate_collision", False)
	data.set_editor_property("combine_meshes", True)
	task = unreal.AssetImportTask()
	task.filename = FBX
	task.destination_path = FOLDER
	task.destination_name = NAME
	task.replace_existing = True
	task.automated = True
	task.save = True
	task.options = opts
	unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
	mesh = eal.load_asset(f"{FOLDER}/{NAME}")
	report = {"fbx": FBX, "mesh": bool(mesh), "slots": {}}
	if mesh:
		for index, slot in enumerate(mesh.get_editor_property("static_materials")):
			slot_name = str(slot.get_editor_property("material_slot_name"))
			mi_path = f"{MATERIALS}/{slot_name.replace('M_Bean_', 'MI_Bean_')}"
			mi = eal.load_asset(mi_path) if eal.does_asset_exist(mi_path) else None
			if mi:
				mesh.set_material(index, mi)
			report["slots"][slot_name] = mi_path if mi else "no MI"
		eal.save_loaded_asset(mesh)
		bounds = mesh.get_bounds().box_extent
		report["extent_cm"] = [round(bounds.x, 2), round(bounds.y, 2), round(bounds.z, 2)]
	out = os.path.join(unreal.Paths.project_saved_dir(), "MonkeTools", "bean_fparm_report.json")
	os.makedirs(os.path.dirname(out), exist_ok=True)
	with open(out, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=1)
	unreal.log(f"[bean_fparm] {json.dumps(report)}")


if __name__ == "__main__":
	main()
