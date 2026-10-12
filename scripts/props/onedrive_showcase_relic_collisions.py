"""Repair only local FBX collision node names; retain delivered geometry/materials."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import bpy

ROOT=Path(__file__).resolve().parents[2]
STAGE=ROOT/"art/export/OneDriveShowcase/20261006"
OUT=STAGE/"repaired_collisions"
OUT.mkdir(exist_ok=True)
proof=[]
for path in sorted((STAGE/"artefacts-polynesiens/exports").glob("*_LOD0.fbx")):
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(path))
    meshes=[o for o in bpy.data.objects if o.type=="MESH" and not o.name.startswith("UCX_")]
    assert len(meshes)==1
    obj=meshes[0]
    collisions=sorted([o for o in bpy.data.objects if o.name.startswith("UCX_")], key=lambda o:o.name)
    before={"mesh":obj.name,"collision":[o.name for o in collisions]}
    obj.name=path.stem.removesuffix("_LOD0")
    for index,c in enumerate(collisions):
        c.name=f"UCX_{obj.name}_{index:02d}"
    bpy.ops.object.select_all(action="SELECT")
    dest=OUT/path.name
    bpy.ops.export_scene.fbx(filepath=str(dest),use_selection=True,object_types={"MESH","EMPTY"},
        global_scale=1,apply_unit_scale=True,apply_scale_options="FBX_SCALE_NONE",axis_forward="-Y",axis_up="Z",
        use_mesh_modifiers=True,add_leaf_bones=False,bake_anim=False,path_mode="STRIP")
    proof.append({"source":str(path),"before":before,"mesh":obj.name,"ucx_count":len(collisions),
        "repaired":str(dest),"sha256":hashlib.sha256(dest.read_bytes()).hexdigest()})
(OUT/"repair-result.json").write_text(json.dumps({"files":proof,"finished_at":dt.datetime.now(dt.timezone.utc).isoformat()},indent=2),encoding="utf-8")
print("T179_RELIC_COLLISION_NAMES_REPAIRED",[(p["before"],p["mesh"]) for p in proof])
