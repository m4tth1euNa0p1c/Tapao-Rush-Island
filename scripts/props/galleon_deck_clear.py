"""Galleon hull with a bare deck (T-188, owner request 2026-10-06 22:45): the T184 hull of Codex
(onedrive_showcase_galleon_export.py: hold battery, open cabin, deck crates removed) without anything else standing
on the decks but the three barrels: capstans and their bars ("le truc de l'ancre"), bell and belfry, lantern posts
and gangway lanterns, rope coils, cannonball garlands, spare anchor stock, bucket, and the binnacle box in front of
the wheel, and the companionway hood on the starboard side of the main deck (the oak box "à droite du pont"). Their
collisions go with them; the barrels keep theirs. The wheel, its pedestal, the
boarding ladder, the davits, the bow anchors and the stern lanterns stay.
No way down to the hold any more (owner request 23:20, "enlève l'accès vers la cale, enlève l'escalier et mets une
grille type pirate"): the open stairwell of T184 (onedrive_showcase_galleon_hold.add_hold: deck cut, stairs, hollowed
collision) is not built, the deck keeps its original closed hatch gratings and its solid collision; the battery of the
hold still shows at its gun ports.
The forecastle door (bulkhead at the front of the waist, its leaf closed) is solid with its wall (owner request
2026-10-07, "rends la porte qui se trouve à l'avant du galion solide et son mur"): one collision box from side to side
and from the main deck to the forecastle deck, in place of the two jambs and the lintel that left its opening
passable, as the decorative poop door of the source (hullform.POOP_DOOR, passable False).

Run (background, the OneDrive maquette is opened read only and never saved):
  blender --background --factory-startup --python scripts/props/galleon_deck_clear.py
Output: art/export/Galleon_T188/SM_Galleon.fbx (+ export-result.json)
"""
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

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import export_galleon_t149_nocannons as prior  # noqa: E402

sys.dont_write_bytecode = True
OUT = ROOT / "art/export/Galleon_T188"
REMOVED_OBJECTS = ("Galleon_Cannons_Deck", "Galleon_Cannons_Swivels", "Galleon_Fittings_Pumps",
                   "Galleon_Fittings_Capstans", "Galleon_Fittings_Bell", "Galleon_Fittings_Lanterns")
REMOVED_UCX = prior.REMOVED_UCX + ("crate_", "capstan_", "anchor_stock", "binnacle", "companion")
EXPECTED_DROPPED_UCX = 19           # 12 of T184 (guns, pumps, lantern posts, crates) + capstans 2, bars 2, stock, binnacle, companion
BARREL_MARGIN = 0.06                # m around a barrel: its hoops and staves
BINNACLE_MARGIN = 0.02              # m around the binnacle box: the pedestal of the wheel starts 4 cm further
FORECASTLE_DOOR_UCX = ("bulkhead_fc_P", "bulkhead_fc_S", "bulkhead_fc_lintel")   # jambs and lintel of its opening


def keep_barrels(obj, barrels):
    """Deletes every face of the deck cargo but those of the barrels (x, y, z, h, r)."""
    obj.data = obj.data.copy()
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    kept = [0] * len(barrels)

    def barrel_of(face):
        points = [obj.matrix_world @ v.co for v in face.verts]
        for i, (x, y, z, h, r) in enumerate(barrels):
            if all((p.x - x) ** 2 + (p.y - y) ** 2 <= (r + BARREL_MARGIN) ** 2
                   and z - 0.02 <= p.z <= z + h + BARREL_MARGIN for p in points):
                return i
        return -1

    doomed = []
    for face in bm.faces:
        i = barrel_of(face)
        if i < 0:
            doomed.append(face)
        else:
            kept[i] += 1
    assert all(n > 50 for n in kept), f"a barrel lost its faces: {kept}"
    removed = len(doomed)
    bmesh.ops.delete(bm, geom=doomed, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return {"barrel_faces": kept, "removed_faces": removed}


def close_forecastle_door(ucx):
    """One full box for the forecastle bulkhead (door and wall) in place of its jambs and lintel (assembly_ucx)."""
    from galleon import assembly_ucx, hullform
    G = hullform.G
    y, z0, z1 = G["fc_end"], G["main_deck"], G["fc_deck"]
    xi = hullform.inner_width(hullform.station_at(y, z0), z0)
    kept = [entry for entry in ucx if entry[0] not in FORECASTLE_DOOR_UCX]
    assert len(ucx) - len(kept) == len(FORECASTLE_DOOR_UCX), "forecastle door collision not found"
    kept.append(("bulkhead_fc", None, assembly_ucx._box((0.0, y, (z0 + z1) / 2), (2 * xi, 0.12, z1 - z0))))
    return kept, {"y": y, "z": [z0, z1], "width": round(2 * xi, 3), "replaced": list(FORECASTLE_DOOR_UCX)}


def remove_box(obj, centre, size, margin):
    """Deletes the faces of obj lying entirely in a box (centre, full size, metres, world)."""
    obj.data = obj.data.copy()
    bm = bmesh.new()
    bm.from_mesh(obj.data)
    half = [s / 2 + margin for s in size]
    faces = [f for f in bm.faces if all(abs((obj.matrix_world @ v.co)[k] - centre[k]) <= half[k]
                                        for v in f.verts for k in range(3))]
    assert len(faces) > 20, (obj.name, len(faces))
    bmesh.ops.delete(bm, geom=faces, context="FACES")
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()
    return len(faces)


def main():
    source = prior.MODEL / "galion-anglais.blend"
    sha = hashlib.sha256(source.read_bytes()).hexdigest()
    report = json.loads((prior.STAGED / "galleon-report.json").read_text(encoding="utf-8"))
    assert (prior.MODEL / "galleon-report.json").read_bytes() == (prior.STAGED / "galleon-report.json").read_bytes()
    bpy.ops.wm.open_mainfile(filepath=str(source))
    sys.path.insert(0, str(prior.SOURCE / "factory/kits"))
    import bean_kit
    from galleon import assembly, fittings, hullform
    spec = importlib.util.spec_from_file_location("t188_source_exporter", prior.MODEL / "scripts/galleon.py")
    exporter = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(exporter)
    exporter.STATE.update(bk=bean_kit, kit=assembly)
    original_merge = exporter._merge_tones
    counts = {}

    def merge(obj):
        # _merge_tones resets every face to slot 0: restore the assignments (T149 / T184 fix)
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
    cargo = keep_barrels(bpy.data.objects["Galleon_Fittings_Cargo"], fittings.cargo_layout()["barrels"])
    bx, by, bz = hullform.FITTINGS["binnacle"]
    binnacle = remove_box(bpy.data.objects["Galleon_Misc_Pedestal"], (bx, by, bz + 0.4), (0.36, 0.30, 0.80),
                          BINNACLE_MARGIN)
    # companionway hood: oak box with a gabled roof standing on the main deck (its UCX box, roof included)
    cx0, cx1, cy0, cy1 = hullform.companion_footprint()
    companion = remove_box(bpy.data.objects["Galleon_Decks_Hatches"], ((cx0 + cx1) / 2, (cy0 + cy1) / 2,
                           hullform.G["main_deck"] + 0.4), (cx1 - cx0, cy1 - cy0, 0.8), 0.03)
    structure = [bpy.data.objects[name] for group in ("hull", "decks", "roof", "cabin", "details", "masts")
                 for name in report["objects"][group] if name not in REMOVED_OBJECTS]
    battery = bpy.data.objects["Galleon_Cannons_Battery"]
    assert battery in structure
    ucx = [entry for entry in assembly.ucx_specs() if not entry[0].startswith(REMOVED_UCX)]
    dropped = [entry[0] for entry in assembly.ucx_specs() if entry not in ucx]
    assert len(dropped) == EXPECTED_DROPPED_UCX, dropped
    assert sum(1 for entry in ucx if entry[0].startswith("barrel")) == 3
    assert any(entry[0].startswith("hatch_") for entry in ucx), "the hatch gratings keep their collision"
    ucx, forecastle_door = close_forecastle_door(ucx)
    tmp = bpy.data.collections.new("COL-T188-Export")
    bpy.context.scene.collection.children.link(tmp)
    OUT.mkdir(parents=True, exist_ok=True)
    hull = exporter._export_group("SM_Galleon", structure, tmp, str(OUT), ucx, assembly.SOCKETS,
                                  dirs=assembly.SOCKET_DIRS, rots=assembly.SOCKET_ROTS, merge_tones=True)
    hull.update(faces_by_material=counts["SM_Galleon"],
                sha256=hashlib.sha256((OUT / "SM_Galleon.fbx").read_bytes()).hexdigest())
    assert hashlib.sha256(source.read_bytes()).hexdigest() == sha, "the OneDrive source changed during the export"
    result = {"task": "T-188", "base": "T184 (onedrive_showcase_galleon_export.py)", "source_sha256": sha,
              "removed_objects": REMOVED_OBJECTS, "cargo": cargo, "binnacle_faces_removed": binnacle,
              "companion_faces_removed": companion,
              "doors": {k: v[0] for k, v in doors.items()}, "removed_ucx": dropped, "ucx": len(ucx),
              "forecastle_door": forecastle_door,
              "hold": "closed: original hatch gratings, no stairwell", "exports": {"SM_Galleon": hull}}
    (OUT / "export-result.json").write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    print("T188_EXPORT_SUCCESS", json.dumps({"ucx": len(ucx), "cargo": cargo, "binnacle": binnacle, "companion": companion}))
    preview(structure)


def preview(structure):
    """Top view of the decks (Workbench) for the review: art/export/Galleon_T188/deck_top.png"""
    from mathutils import Vector
    scene = bpy.context.scene
    keep = set(structure)
    for o in scene.objects:
        if o.type == "MESH":
            o.hide_render = o not in keep or o.name.startswith(("Galleon_Masts", "Galleon_Roof"))
    cam_data = bpy.data.cameras.new("T188_Top")
    cam_data.type = "ORTHO"
    cam_data.ortho_scale = 26.0
    cam = bpy.data.objects.new("T188_Top", cam_data)
    scene.collection.objects.link(cam)
    cam.location = Vector((0.0, -1.0, 40.0))
    cam.rotation_euler = (0.0, 0.0, 1.5708)
    scene.camera = cam
    try:
        scene.render.engine = "BLENDER_WORKBENCH"
    except TypeError:
        pass
    scene.display.shading.light = "STUDIO"
    scene.display.shading.color_type = "MATERIAL"
    scene.render.resolution_x, scene.render.resolution_y = 1600, 700
    scene.render.filepath = str(OUT / "deck_top.png")
    scene.render.image_settings.file_format = "PNG"
    bpy.ops.render.render(write_still=True)
    print("T188_PREVIEW", OUT / "deck_top.png")


if __name__ == "__main__":
    main()
