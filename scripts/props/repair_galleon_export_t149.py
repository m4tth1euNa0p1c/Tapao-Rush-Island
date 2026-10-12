"""Re-export the read-only OneDrive model with face materials preserved.

The delivered exporter's _merge_tones clears the slots after assigning indices;
Blender resets those indices to zero. Restore them after rebuilding the slots.
The source .blend and kit are never saved or edited.
"""
import hashlib
import importlib.util
import json
import re
import sys
from collections import Counter
from pathlib import Path

import bpy
import numpy as np

ROOT = Path(__file__).resolve().parents[2]
SOURCE = Path("C:/Users/matth/OneDrive/maquette-blender")
MODEL = SOURCE / "maquettes/galion-anglais"
STAGED = ROOT / "art/export/Galleon_T149/20261006_1203"
OUT = STAGED / "materialfix"
sys.dont_write_bytecode = True


def main():
    report = json.loads((STAGED / "galleon-report.json").read_text(encoding="utf-8"))
    assert (MODEL / "galleon-report.json").read_bytes() == (STAGED / "galleon-report.json").read_bytes(), (
        "Source revision changed; stage it before exporting")
    source_blend = MODEL / "galion-anglais.blend"
    source_hash = hashlib.sha256(source_blend.read_bytes()).hexdigest()
    bpy.ops.wm.open_mainfile(filepath=str(source_blend))
    sys.path.insert(0, str(SOURCE / "factory/kits"))
    import bean_kit
    from galleon import assembly, masts, materials, misc
    spec = importlib.util.spec_from_file_location("t149_source_exporter", MODEL / "scripts/galleon.py")
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    exporter.STATE.update(bk=bean_kit, kit=assembly)
    # The saved scene contains geometry, but this transient lookup is built only
    # by the generator. Recreate it for the pennants' exported wind UV channels.
    pennant_batch = materials.Batch("T149_WindLookup")
    palette = materials.mats()
    for index, mast in enumerate(("fore", "main", "mizzen")):
        masts.build_pennant(pennant_batch, palette, mast, phase=1.1 * index)
    pennant_batch.bm.free()
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
        export_name = re.sub(r"\.\d+$", "", obj.name)
        face_counts[export_name] = {names[i]: n for i, n in counts.items()}
        assert len(counts) > 1 or len(names) == 1, f"Collapsed materials: {obj.name}"
        return names

    exporter._merge_tones = merge_preserving_faces
    parts = {group: [bpy.data.objects[name] for name in names]
             for group, names in report["objects"].items()}
    # Export the rest pose, as the source exporter does before adding wind.
    for group in ("sails", "pennants", "flag"):
        for obj in parts[group]:
            for modifier in obj.modifiers:
                if modifier.type == "NODES":
                    modifier.show_viewport = False
                    modifier.show_render = False
    tmp = bpy.data.collections.new("COL-T149-MaterialFix")
    bpy.context.scene.collection.children.link(tmp)
    flag_lookup = misc.build_flag(tmp, palette)
    bpy.data.objects.remove(flag_lookup, do_unlink=True)
    OUT.mkdir(parents=True, exist_ok=True)
    export = exporter._export_group
    structure = [obj for group in ("hull", "decks", "roof", "cabin", "details", "masts") for obj in parts[group]]
    result = {"SM_Galleon": export("SM_Galleon", structure, tmp, str(OUT), assembly.ucx_specs(),
                                  assembly.SOCKETS, dirs=assembly.SOCKET_DIRS,
                                  rots=assembly.SOCKET_ROTS, merge_tones=True)}
    for group, name in (("sails", "SM_Galleon_Sails"), ("pennants", "SM_Galleon_Pennants"),
                        ("rigging", "SM_Galleon_Rigging"), ("flag", "SM_Galleon_Flag"),
                        ("wheel", "SM_Galleon_Wheel"), ("rudder", "SM_Galleon_Rudder")):
        frame = parts[group][0].matrix_world.copy() if group in ("flag", "wheel", "rudder") else None
        result[name] = export(name, parts[group], tmp, str(OUT), frame=frame, merge_tones=True,
                              wind=group if group in ("sails", "pennants", "flag") else None)
    for name, info in result.items():
        info["faces_by_material"] = face_counts[name]
        info["sha256"] = hashlib.sha256((OUT / (name + ".fbx")).read_bytes()).hexdigest()
        assert info["tris"] == report["exports"][name]["tris"], f"Geometry changed: {name}"
    cannons = {obj.name: sum(len(p.vertices)-2 for p in obj.data.polygons)
               for obj in parts["details"] if obj.name.startswith("Galleon_Cannons_")}
    assert len(cannons) == 3 and sum(cannons.values()) == 35008, cannons
    assert hashlib.sha256(source_blend.read_bytes()).hexdigest() == source_hash, "Source blend changed"
    proof = {"source_blend_sha256": source_hash, "exports": result, "embedded_cannon_triangles": cannons}
    (OUT / "repair-result.json").write_text(json.dumps(proof, indent=2), encoding="utf-8")
    print("T149_REPAIRED_EXPORT_SUCCESS " + json.dumps({"meshes": len(result), "cannons": cannons,
          "hull_used_materials": len(face_counts["SM_Galleon"])}))


if __name__ == "__main__":
    main()
