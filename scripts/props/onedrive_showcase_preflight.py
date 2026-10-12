"""Inspect staged FBXs in background Blender before the T-179 Unreal import."""
from collections import Counter
import datetime as dt
import json
from pathlib import Path
import bpy
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
STAGE = ROOT / "art/export/OneDriveShowcase/20261006"


def main():
    meshes = {}
    for folder in ("artefacts-polynesiens", "temple-reliquaire"):
        for source in sorted((STAGE / folder / "exports").glob("*.fbx")):
            bpy.ops.wm.read_factory_settings(use_empty=True)
            bpy.ops.import_scene.fbx(filepath=str(source))
            solids = [o for o in bpy.data.objects if o.type == "MESH" and not o.name.startswith("UCX_")]
            assert len(solids) == 1, (source.name, [o.name for o in solids])
            obj = solids[0]
            obj.data.calc_loop_triangles()
            used = Counter(obj.data.materials[p.material_index].name for p in obj.data.polygons)
            assert len(used) == len(obj.data.materials), f"Unused/collapsed slots: {source.name} {used}"
            corners = [obj.matrix_world @ Vector(c) for c in obj.bound_box]
            size = [max(v[i] for v in corners)-min(v[i] for v in corners) for i in range(3)]
            meshes[source.stem] = {"source": str(source.relative_to(STAGE)).replace('\\', '/'),
                                   "size_cm": [round(v*100, 4) for v in size],
                                   "triangles": len(obj.data.loop_triangles), "faces_by_material": dict(used),
                                   "ucx_count": sum(o.name.startswith("UCX_") for o in bpy.data.objects),
                                   "sockets": [o.name for o in bpy.data.objects if o.name.startswith("SOCKET_")]}
            print("T179_PREFLIGHT", source.stem, len(used), size, flush=True)
    assert len(meshes) == 40, len(meshes)
    output = STAGE / "preflight-result.json"
    output.write_text(json.dumps({"task": "T-179", "meshes": meshes,
                                  "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}, indent=2), encoding="utf-8")
    print("T179_PREFLIGHT_SUCCESS", output)


if __name__ == "__main__":
    main()
