"""Probe: the Python API for the LODs of a skeletal mesh (UE 5.8). Writes monke_island/Saved/T186/probe-lod.json."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
mesh = ue.EditorAssetLibrary.load_asset("/Game/MonkeIsland/Characters/CocoWarrior/Meshes/SK_CocoWarrior")
sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
out = {"mesh_lod": [n for n in dir(mesh) if "lod" in n.lower()], "sub": [n for n in dir(sub) if not n.startswith("_")],
       "lod_count": sub.get_lod_count(mesh)}
try:
    out["lod_info_array"] = [str(type(x)) for x in mesh.get_editor_property("lod_info")]
except Exception as e:  # noqa: BLE001
    out["lod_info_err"] = str(e)
try:
    infos = mesh.get_lod_info_array() if hasattr(mesh, "get_lod_info_array") else None
    out["get_lod_info_array"] = [dir(i)[:5] for i in infos] if infos else None
except Exception as e:  # noqa: BLE001
    out["get_lod_info_array_err"] = str(e)
(ROOT / "monke_island/Saved/T186/probe-lod.json").write_text(json.dumps(out, indent=1, default=str), encoding="utf-8")
