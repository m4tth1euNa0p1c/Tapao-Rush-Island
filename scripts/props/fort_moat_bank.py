"""T-210: sand bank around the French fort's moat in Test Flat (owner report 2026-10-07: "les bordures de sable des douves
ne sont pas modélisées, je peux tomber dans une zone buguée").

The site terrain of the fort (SM_Fort_TerrainNear) is dug 1.6 m deep from the counterscarp line C out to C + 1.6 m,
under the covered way of the maquette's dry ditch; the T198 ditch kept its coping (C - 0.05 .. C + 0.45) but nothing
covers that trench any more: a dry gap all around the moat. This bank fills it at ground level, from under the coping
(C + 0.30) to past the trench (C + 2.10), its top just under the coping and under the sand of the terrain at its far edge,
a closed solid down to below the trench floor. Built on the maquette's own trace (fort_site, OneDrive kits, read only),
so it follows the bastions exactly.

  blender -b --factory-startup --python scripts/props/fort_moat_bank.py
  -> art/export/Fort_T198/exports/SM_Fort_MoatBank.fbx (fort-local metres, the fort's origin, as the other fort meshes)
Unreal: scripts/props/fort_moat_bank_ue.py (import, sand material of the site, complex collision, actor at the fort).
"""
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/export/Fort_T198/exports"
sys.path.insert(0, "C:/Users/matth/OneDrive/maquette-blender/factory/kits")
import bean_kit  # noqa: E402,F401 - the kit's registration modules first, as fort_rework_blender.py
import fort_site as SITE  # noqa: E402

COUNTERSCARP = 6.715              # offset of the counterscarp line from the trace (fort_site.counterscarp)
INNER, OUTER = 0.30, 2.10         # bank from C + INNER (under the coping) to C + OUTER (beyond the 1.6 m trench)
Z_INNER, Z_OUTER = -0.035, -0.012 # top: under the coping's top (0) and under the terrain's sand (0) at the far edge
BOTTOM = -1.75                    # below the trench floor (-1.6): a closed solid, nothing to see through
SEGMENTS = 480
UV_METRES = 2.0                   # sand texture tiling (m per UV unit)


def ring(offset):
    return SITE.resample_polar(SITE.offset_exact(SITE.trace(), COUNTERSCARP + offset, step=0.3), SEGMENTS)


def main():
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    inner, outer = ring(INNER), ring(OUTER)
    bm = bmesh.new()
    uv = bm.loops.layers.uv.new("UVMap")
    rows = []
    for (xi, yi), (xo, yo) in zip(inner, outer):
        rows.append([bm.verts.new((xi, yi, Z_INNER)), bm.verts.new((xo, yo, Z_OUTER)),
                     bm.verts.new((xo, yo, BOTTOM)), bm.verts.new((xi, yi, BOTTOM))])
    for i in range(SEGMENTS):
        a, b = rows[i], rows[(i + 1) % SEGMENTS]
        # top (sand), outer wall, bottom, inner wall: outward normals for a ring listed counter-clockwise
        for quad in ((a[0], a[1], b[1], b[0]), (a[1], a[2], b[2], b[1]), (a[2], a[3], b[3], b[2]), (a[3], a[0], b[0], b[3])):
            face = bm.faces.new(quad)
            for loop in face.loops:
                co = loop.vert.co
                loop[uv].uv = (co.x / UV_METRES, (co.y + co.z) / UV_METRES)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new("SM_Fort_MoatBank")
    bm.to_mesh(me)
    bm.free()
    for poly in me.polygons:
        poly.use_smooth = False
    # the layers of the site's ground material, as the terrain next to it (fort_site.Site.masks at ground level):
    # MaskA = dry sand, rubble, feo, soil (the vertex colour Unreal reads), MaskB = litter, wet, wrack
    site = SITE.Site()
    layers = {name: me.color_attributes.new(name, "FLOAT_COLOR", "POINT") for name in ("MaskA", "MaskB")}
    for v in me.vertices:
        mask_a, mask_b = site.masks(v.co.x, v.co.y, 0.0)
        layers["MaskA"].data[v.index].color = mask_a
        layers["MaskB"].data[v.index].color = mask_b
    me.color_attributes.active_color = layers["MaskA"]
    me.color_attributes.render_color_index = 0
    mat = bpy.data.materials.new("MAT-Site_Ground")
    me.materials.append(mat)
    ob = bpy.data.objects.new("SM_Fort_MoatBank", me)
    bpy.context.scene.collection.objects.link(ob)
    bpy.ops.object.select_all(action="DESELECT")
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob
    OUT.mkdir(parents=True, exist_ok=True)
    path = OUT / "SM_Fort_MoatBank.fbx"
    bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, apply_unit_scale=True, axis_forward="-Y", axis_up="Z",
                             bake_anim=False, mesh_smooth_type="FACE", colors_type="LINEAR")
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    report = {"file": str(path), "segments": SEGMENTS, "faces": len(me.polygons), "inner_offset_m": INNER, "outer_offset_m": OUTER,
              "size_m": [round(max(xs) - min(xs), 2), round(max(ys) - min(ys), 2)], "top_z_m": [Z_INNER, Z_OUTER]}
    (OUT.parent / "moat-bank-result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print("T210_MOAT_BANK", json.dumps(report), flush=True)


if __name__ == "__main__":
    main()
