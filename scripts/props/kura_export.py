"""T-190: exports the new kura alone (village/props.py Props.kura) without rebuilding the whole village kit:
art/export/Props/Village/SM_Village_Kura.fbx (UCX), its entry in village_kit.json, and a render
art/_previews/village_kura.png. Same footprint as before: the layouts of the villages stay valid.

Run (background):
  blender --background --factory-startup --python scripts/props/kura_export.py
"""
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import minka_rurale  # noqa: E402
from minka import geo  # noqa: E402
from village import props as vprops  # noqa: E402

ROOT = HERE.parents[1]
EXPORT = ROOT / "art" / "export" / "Props" / "Village"
PREVIEW = ROOT / "art" / "_previews" / "village_kura.png"
ASSET = "SM_Village_Kura"


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    M = geo.materials()
    vprops.village_materials(M)
    lib = bpy.data.collections.new("VILLAGE_LIB")
    scene.collection.children.link(lib)
    props = vprops.Props(M, lib)
    coll = props.kura()
    objs = [o for o in coll.all_objects if o.type == "MESH"]
    vs = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
    bbox = [[round(min(v[i] for v in vs), 3) for i in range(3)], [round(max(v[i] for v in vs), 3) for i in range(3)]]
    # render first: the export joins and removes copies, the library objects stay
    minka_rurale.studio(scene)
    cam = minka_rurale.camera("CAM_Kura", (6.5, -8.5, 4.2), (0.0, 0.0, 2.6), 35)
    minka_rurale.render(scene, cam, PREVIEW, (1400, 1000))
    ucx = geo.ucx_boxes(ASSET, props.ucx["Kura"])
    minka_rurale.EXPORTED.clear()
    _, tris = minka_rurale.export_group(objs, ASSET, extra=ucx, out_dir=EXPORT)
    kit_path = EXPORT / "village_kit.json"
    kit = json.loads(kit_path.read_text(encoding="utf-8"))
    kit["assets"][ASSET] = {"tris": tris, "ucx": len(props.ucx["Kura"]), "bbox": bbox,
                            "ground": [[round(b[0][0], 3), round(b[0][1], 3), round(b[1][0], 3), round(b[1][1], 3)]
                                       for b in props.ucx["Kura"][:1]],
                            "materials": sorted({m.name for o in objs for m in o.data.materials if m})}
    kit_path.write_text(json.dumps(kit, indent=1), encoding="utf-8")
    print(f"[kura] {ASSET}: {tris} tris, {len(props.ucx['Kura'])} UCX, bbox {bbox} -> {EXPORT}; preview {PREVIEW}")


if __name__ == "__main__":
    main()
