"""Re-export the T-149 galleon hull without cannons and pumps, with the cabin door open (owner request 2026-10-06).

Same exporter and face-material fix as repair_galleon_export_t149.py; the read-only OneDrive model is never saved.
Changes to SM_Galleon only:
  - objects removed: Galleon_Cannons_Battery / _Deck / _Swivels, Galleon_Fittings_Pumps;
  - UCX removed: gun_upper_* (deck guns), pump_* (pumps), lantern_post_* (lantern posts at the top of the poop deck
    stairs: with rail_poop_1 they left a 66 cm passage, narrower than the 68 cm player capsule);
  - cabin door leaves removed: outer one in Galleon_Roof_Bulkheads, inner one and its wall panel in Galleon_Cabin_Room
    (the opening under bulkhead_qd_lintel already has no collision).
The six other parts are copied unchanged from materialfix/.

Run: blender --background --factory-startup --python scripts/props/export_galleon_t149_nocannons.py
Output: art/export/Galleon_T149/20261006_1203/nocannons/ (7 FBX + export-result.json)
"""
import hashlib
import importlib.util
import json
import re
import shutil
import sys
from collections import Counter
from pathlib import Path

import bmesh
import bpy
import numpy as np
from mathutils import Vector

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path("C:/Users/matth/OneDrive/maquette-blender")
MODEL = SOURCE / "maquettes/galion-anglais"
STAGED = ROOT / "art/export/Galleon_T149/20261006_1203"
MATERIALFIX = STAGED / "materialfix"
OUT = STAGED / "nocannons"
REMOVED_OBJECTS = ("Galleon_Cannons_Battery", "Galleon_Cannons_Deck", "Galleon_Cannons_Swivels", "Galleon_Fittings_Pumps")
REMOVED_UCX = ("gun_upper_", "pump_", "lantern_post_")
# Cabin door opening: between bulkhead_qd_P / _S (x +-0.45 m), from the deck (z 1.6 m) to bulkhead_qd_lintel (z 2.9 m).
# Two leaves close it: the outer door in the quarterdeck bulkhead and the inner door (with its wall panel) of the cabin.
DOOR_HALF_WIDTH, DOOR_SILL, DOOR_LINTEL = 0.45, 1.6, 2.9
DOOR_LEAVES = {"Galleon_Roof_Bulkheads": (3.55, 3.85), "Galleon_Cabin_Room": (3.85, 4.05)}
EPS = 0.004
sys.dont_write_bytecode = True


def open_door(obj, y_range):
    """Delete the door leaf faces inside the opening; keep the jambs, the sill and the lintel (frame of the opening)."""
    obj.data = obj.data.copy()  # in memory only, never the shared or saved data
    matrix = obj.matrix_world
    bm = bmesh.new()
    bm.from_mesh(obj.data)

    def leaf(face):
        points = [matrix @ v.co for v in face.verts]
        inside = all(abs(p.x) <= DOOR_HALF_WIDTH + EPS and y_range[0] <= p.y <= y_range[1]
                     and DOOR_SILL - EPS <= p.z <= DOOR_LINTEL + EPS for p in points)
        jamb = all(abs(abs(p.x) - DOOR_HALF_WIDTH) < EPS for p in points)
        sill = all(p.z <= DOOR_SILL + 0.02 for p in points)
        lintel = all(p.z >= DOOR_LINTEL - 0.01 for p in points)
        if inside and not (jamb or sill or lintel):
            return True
        # boards wider than the opening run behind the jambs: centre in the opening, and no wider than the frame
        centre = sum(points, Vector()) / len(points)
        centred = (abs(centre.x) <= DOOR_HALF_WIDTH - 0.03 and y_range[0] <= centre.y <= y_range[1]
                   and DOOR_SILL + 0.03 <= centre.z <= DOOR_LINTEL - 0.03)
        narrow = all(abs(q.x) <= DOOR_HALF_WIDTH + 0.12 and DOOR_SILL - 0.05 <= q.z <= DOOR_LINTEL + 0.05 for q in points)
        return centred and narrow

    faces = [f for f in bm.faces if leaf(f)]
    materials = Counter(obj.data.materials[f.material_index].name for f in faces)
    bmesh.ops.delete(bm, geom=faces, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return len(faces), dict(materials)


def main():
    report = json.loads((STAGED / "galleon-report.json").read_text(encoding="utf-8"))
    assert (MODEL / "galleon-report.json").read_bytes() == (STAGED / "galleon-report.json").read_bytes(), (
        "Source revision changed; stage it before exporting")
    source_blend = MODEL / "galion-anglais.blend"
    source_hash = hashlib.sha256(source_blend.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(source_blend))
    sys.path.insert(0, str(SOURCE / "factory/kits"))
    import bean_kit
    from galleon import assembly
    spec = importlib.util.spec_from_file_location("t149_source_exporter", MODEL / "scripts/galleon.py")
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    exporter.STATE.update(bk=bean_kit, kit=assembly)

    # Same fix as repair_galleon_export_t149.py: _merge_tones resets every face to slot 0, restore the assignments
    original_merge = exporter._merge_tones
    face_counts = {}

    def merge_preserving_faces(obj):
        mesh = obj.data
        old_indices = np.empty(len(mesh.polygons), dtype=np.int32)
        mesh.polygons.foreach_get("material_index", old_indices)
        base_names = [re.sub(r"_t[0-4]$", "", material.name) for material in mesh.materials]
        names = original_merge(obj)
        remap = np.array([names.index(name) for name in base_names], dtype=np.int32)
        final_indices = remap[old_indices]
        mesh.polygons.foreach_set("material_index", final_indices)
        mesh.update()
        counts = Counter(int(i) for i in final_indices)
        face_counts[re.sub(r"\.\d+$", "", obj.name)] = {names[i]: n for i, n in counts.items()}
        assert len(counts) > 1 or len(names) == 1, f"Collapsed materials: {obj.name}"
        return names

    exporter._merge_tones = merge_preserving_faces
    parts = {group: [bpy.data.objects[name] for name in names] for group, names in report["objects"].items()}
    removed = [obj.name for obj in parts["details"] if obj.name in REMOVED_OBJECTS]
    assert sorted(removed) == sorted(REMOVED_OBJECTS), f"Objects to remove not found: {removed}"
    door = {name: open_door(bpy.data.objects[name], y_range) for name, y_range in DOOR_LEAVES.items()}
    door_faces = sum(count for count, _ in door.values())
    door_materials = {name: materials for name, (_, materials) in door.items()}
    assert all(300 < count < 1200 for count, _ in door.values()), f"Unexpected cabin door leaves: {door}"

    structure = [obj for group in ("hull", "decks", "roof", "cabin", "details", "masts") for obj in parts[group]
                 if obj.name not in REMOVED_OBJECTS]
    ucx = [entry for entry in assembly.ucx_specs() if not entry[0].startswith(REMOVED_UCX)]
    dropped_ucx = [entry[0] for entry in assembly.ucx_specs() if entry[0].startswith(REMOVED_UCX)]
    assert len(dropped_ucx) == 10, dropped_ucx

    tmp = bpy.data.collections.new("COL-T149-NoCannons")
    bpy.context.scene.collection.children.link(tmp)
    OUT.mkdir(parents=True, exist_ok=True)
    hull = exporter._export_group("SM_Galleon", structure, tmp, str(OUT), ucx, assembly.SOCKETS,
                                  dirs=assembly.SOCKET_DIRS, rots=assembly.SOCKET_ROTS, merge_tones=True)
    hull["faces_by_material"] = face_counts["SM_Galleon"]
    hull["sha256"] = hashlib.sha256((OUT / "SM_Galleon.fbx").read_bytes()).hexdigest()
    hull["material_names"] = sorted(face_counts["SM_Galleon"])

    previous = json.loads((MATERIALFIX / "repair-result.json").read_text(encoding="utf-8"))
    exports = {"SM_Galleon": hull}
    for name, info in previous["exports"].items():
        if name != "SM_Galleon":
            shutil.copy2(MATERIALFIX / f"{name}.fbx", OUT / f"{name}.fbx")
            exports[name] = info
    assert hashlib.sha256(source_blend.read_bytes()).hexdigest() == source_hash, "Source blend changed"
    result = {
        "source_blend_sha256": source_hash,
        "request": "owner 2026-10-06: no cannons, no pumps, open interior (cabin door)",
        "removed_objects": removed,
        "removed_ucx": dropped_ucx,
        "cabin_door_faces_removed": door_faces,
        "cabin_door_materials": door_materials,
        "hull_tris_before": report["exports"]["SM_Galleon"]["tris"],
        "exports": exports,
    }
    (OUT / "export-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    print("T149_NOCANNONS_EXPORT_SUCCESS " + json.dumps({"tris": hull["tris"], "ucx": hull["ucx"],
          "materials": len(hull["material_names"]), "door_faces": door_faces}))


if __name__ == "__main__":
    main()
