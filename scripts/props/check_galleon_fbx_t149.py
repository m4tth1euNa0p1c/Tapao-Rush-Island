"""Read-only Blender inspection of the delivered hull's face/material assignments."""
import json
from collections import Counter
from pathlib import Path
import bpy

root = Path(__file__).resolve().parents[2]
source = root / "art/export/Galleon_T149/20261006_1203"
report = {}
for label, path in (("original", source / "SM_Galleon.fbx"),
                    ("repaired", source / "materialfix/SM_Galleon.fbx")):
    bpy.ops.import_scene.fbx(filepath=str(path))
    imported = list(bpy.context.selected_objects)
    for obj in imported:
        if obj.type != "MESH" or obj.name.startswith("UCX_"):
            continue
        counts = Counter(p.material_index for p in obj.data.polygons)
        report[label] = {
            "triangles": sum(len(p.vertices)-2 for p in obj.data.polygons),
            "slots": [m.name if m else "None" for m in obj.data.materials],
            "faces_by_material": {str(i): c for i, c in counts.items()},
        }
    for obj in imported:
        bpy.data.objects.remove(obj, do_unlink=True)
assert len(report["repaired"]["faces_by_material"]) == 32, "FBX collapsed material assignments again"
assert report["repaired"]["triangles"] == report["original"]["triangles"] == 329434
(source / "materialfix/roundtrip-result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
print("T149_FBX_INSPECTION " + json.dumps(report))
