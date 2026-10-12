"""Local T184 hull: keep the hold battery, clear deck crates, preserve open cabin."""
import hashlib
import importlib.util
import json
import re
import sys
from collections import Counter
from pathlib import Path
import bmesh
import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import export_galleon_t149_nocannons as prior
import onedrive_showcase_galleon_hold as hold
sys.dont_write_bytecode = True
OUT = ROOT / "art/export/Galleon_T184"

def remove_crates(obj, crates):
    obj.data = obj.data.copy()
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    counts = []
    for x, y, z, sx, sy, sz in crates:
        faces = []
        for face in bm.faces:
            points = [obj.matrix_world @ v.co for v in face.verts]
            # Include the rope resting on the crate, with a small margin for its turn.
            if all(abs(p.x-x) < sx/2+.2 and abs(p.y-y) < sy/2+.2 and z-.01 <= p.z <= z+sz+.25 for p in points):
                faces.append(face)
        assert len(faces) > 100, (x, y, len(faces))
        counts.append({"position": [x, y, z], "removed_faces": len(faces)})
        bmesh.ops.delete(bm, geom=faces, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return counts

def main():
    source = prior.MODEL / "galion-anglais.blend"
    sha = hashlib.sha256(source.read_bytes()).hexdigest()
    report = json.loads((prior.STAGED / "galleon-report.json").read_text(encoding="utf-8"))
    assert (prior.MODEL / "galleon-report.json").read_bytes() == (prior.STAGED / "galleon-report.json").read_bytes()
    bpy.ops.wm.open_mainfile(filepath=str(source))
    sys.path.insert(0, str(prior.SOURCE / "factory/kits"))
    import bean_kit
    from galleon import assembly, fittings
    spec = importlib.util.spec_from_file_location("t184_source_exporter", prior.MODEL / "scripts/galleon.py")
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    exporter.STATE.update(bk=bean_kit, kit=assembly)
    original_merge = exporter._merge_tones
    counts = {}
    def merge(obj):
        mesh = obj.data
        old = np.empty(len(mesh.polygons), dtype=np.int32)
        mesh.polygons.foreach_get("material_index", old)
        bases = [re.sub(r"_t[0-4]$", "", m.name) for m in mesh.materials]
        names = original_merge(obj)
        final = np.array([names.index(n) for n in bases], dtype=np.int32)[old]
        mesh.polygons.foreach_set("material_index", final)
        mesh.update()
        counts[re.sub(r"\.\d+$", "", obj.name)] = {names[i]: n for i, n in Counter(int(i) for i in final).items()}
        return names
    exporter._merge_tones = merge
    doors = {name: prior.open_door(bpy.data.objects[name], limits) for name, limits in prior.DOOR_LEAVES.items()}
    crates = remove_crates(bpy.data.objects["Galleon_Fittings_Cargo"], fittings.layout()["crates"])
    removed = ("Galleon_Cannons_Deck", "Galleon_Cannons_Swivels", "Galleon_Fittings_Pumps")
    structure = [bpy.data.objects[name] for group in ("hull", "decks", "roof", "cabin", "details", "masts")
                 for name in report["objects"][group] if name not in removed]
    interior_collection = bpy.data.collections.new("COL-T184-HoldInterior")
    bpy.context.scene.collection.children.link(interior_collection)
    interior_info, extra_ucx = hold.add_hold(structure, interior_collection, assembly)
    battery = bpy.data.objects["Galleon_Cannons_Battery"]
    assert battery in structure
    battery.data.calc_loop_triangles()
    battery_tris = len(battery.data.loop_triangles)
    assert battery_tris > 1000
    ucx = [entry for entry in assembly.ucx_specs() if not entry[0].startswith(prior.REMOVED_UCX + ("crate_",))]
    dropped = [entry[0] for entry in assembly.ucx_specs() if entry not in ucx]
    assert len(ucx) == 157, (len(ucx), dropped)
    ucx = hold.hollow_collision(ucx, extra_ucx)
    tmp = bpy.data.collections.new("COL-T184-HoldBattery")
    bpy.context.scene.collection.children.link(tmp)
    OUT.mkdir(parents=True, exist_ok=True)
    hull = exporter._export_group("SM_Galleon", structure, tmp, str(OUT), ucx, assembly.SOCKETS,
                                  dirs=assembly.SOCKET_DIRS, rots=assembly.SOCKET_ROTS, merge_tones=True)
    hull.update(faces_by_material=counts["SM_Galleon"], sha256=hashlib.sha256((OUT / "SM_Galleon.fbx").read_bytes()).hexdigest())
    assert hashlib.sha256(source.read_bytes()).hexdigest() == sha
    result = {"source_sha256": sha, "removed_objects": removed, "removed_crates": crates, "doors": doors,
              "removed_ucx": dropped, "hold_battery_triangles": battery_tris,
              "hold_interior": interior_info, "exports": {"SM_Galleon": hull}}
    (OUT / "export-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("T184_EXPORT_SUCCESS", json.dumps({"ucx": len(ucx), "battery_triangles": battery_tris, "crates": crates}))

if __name__ == "__main__":
    main()
