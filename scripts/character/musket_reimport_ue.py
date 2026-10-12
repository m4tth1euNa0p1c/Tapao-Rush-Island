"""In-editor reimport of the musket pack built in this repository and of the bean's first person arm (T-212).

Same work as the editor-closed commandlets (import_pirate_weapons.py, setup_pirate_weapons.py, import_bean_fparm.py),
run inside an open editor with no game running: scripts/editor/lead_session_t186.py, action "musketimport".
The FP arms' slots then take the bean's MIs again (SkeletalMeshTools.set_material through the MCP: a Python
set_editor_property("materials") does not stick on a freshly imported skeletal mesh).
"""
import importlib
import json
import os
import sys

import unreal

HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
	sys.path.insert(0, HERE)


def main():
	os.environ["MONKE_PIRATE_WEAPONS"] = "Musket"
	import import_pirate_weapons
	import setup_pirate_weapons
	import import_bean_fparm
	for module in (import_pirate_weapons, setup_pirate_weapons, import_bean_fparm):
		importlib.reload(module)
	import_pirate_weapons.run_in_unreal()
	saved = unreal.Paths.project_saved_dir()
	with open(os.path.join(saved, "MonkeTools", "pirate_weapons_report.json"), encoding="utf-8") as f:
		report = json.load(f)
	assert not report["failed"], f"import failed: {report['failed'][:5]}"
	setup_pirate_weapons.run_in_unreal()
	import_bean_fparm.main()
	unreal.log(f"[musket_reimport] imported {len(report['imported'])}, checks {json.dumps(report['checks'])}")


if __name__ == "__main__":
	main()
