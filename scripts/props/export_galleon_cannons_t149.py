"""Export the cannon battery as a visible companion mesh for T-149.

Reads the Blender kit from OneDrive but writes only into the staged project
delivery. This separates the modeled artillery from the ship hull FBX so it can
be inspected and attached independently in Unreal.
"""
import hashlib
import importlib.util
import json
import random
import sys
from pathlib import Path

import bpy

ROOT = Path(__file__).resolve().parents[2]
KIT_ROOT = Path("C:/Users/matth/OneDrive/maquette-blender/factory/kits")
MODEL_ROOT = Path("C:/Users/matth/OneDrive/maquette-blender/maquettes/galion-anglais")
OUT = ROOT / "art/export/Galleon_T149/20261006_1203"


def main():
    sys.path.insert(0, str(KIT_ROOT))
    import bean_kit
    from galleon import cannons, materials

    entry = MODEL_ROOT / "scripts/galleon.py"
    spec = importlib.util.spec_from_file_location("galleon_export_t149", entry)
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)

    scene = bpy.context.scene
    collection = bpy.data.collections.new("COL-T149-Cannons")
    scene.collection.children.link(collection)
    built = cannons.build(collection, materials.mats(), random.Random(1604))
    assert len(built) == 3, f"Expected battery, deck cannons, and swivels; got {[o.name for o in built]}"
    tri_count = sum(sum(max(0, len(poly.vertices) - 2) for poly in obj.data.polygons) for obj in built)
    assert tri_count >= 5000, f"Cannon mesh unexpectedly sparse: {tri_count} triangles"

    exporter.STATE["bk"] = bean_kit
    exporter.STATE["kit"] = materials
    result = exporter._export_group("SM_Galleon_Cannons", built, collection, str(OUT), merge_tones=True)
    fbx = OUT / "SM_Galleon_Cannons.fbx"
    assert fbx.is_file() and fbx.stat().st_size > 100_000, "Cannon FBX was not exported"
    result.update(source_blend=str(MODEL_ROOT / "galion-anglais.blend"),
                  generated_triangles=tri_count,
                  fbx_sha256=hashlib.sha256(fbx.read_bytes()).hexdigest())
    (OUT / "cannon-export-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("T149_CANNONS_EXPORTED", json.dumps(result, ensure_ascii=False))


if __name__ == "__main__":
    main()
