"""Probe: setting a material slot of a skeletal mesh from Python (UE 5.8). Writes monke_island/Saved/T186/probe-setmat.json."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
EAL = ue.EditorAssetLibrary
mesh = EAL.load_asset("/Game/MonkeIsland/Characters/CocoWarrior/Meshes/SK_CocoWarrior")
mi = EAL.load_asset("/Game/MonkeIsland/Characters/CocoWarrior/Materials/MI_CocoWarrior_Body")
out = {"mi": mi.get_path_name() if mi else None}
slots = mesh.get_editor_property("materials")
out["before"] = slots[0].get_editor_property("material_interface").get_name()
slots[0].set_editor_property("material_interface", mi)
out["copy_after_set"] = slots[0].get_editor_property("material_interface").get_name()
mesh.set_editor_property("materials", slots)
out["mesh_after_set_editor_property"] = mesh.get_editor_property("materials")[0].get_editor_property("material_interface").get_name()
out["methods"] = [n for n in dir(mesh) if "material" in n.lower()]
(ROOT / "monke_island/Saved/T186/probe-setmat.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
