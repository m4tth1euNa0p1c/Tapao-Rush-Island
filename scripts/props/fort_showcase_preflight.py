"""Check all staged meshes, material slots, authored UCX and expected centimeter dimensions."""
from collections import Counter
import datetime as dt
import json
from pathlib import Path
import bpy
from mathutils import Vector

ROOT=Path(__file__).resolve().parents[2]
STAGE=ROOT/'art/export/Fort_T193'
results={}
for source in sorted([*(STAGE/'exports').glob('*.fbx'),*(STAGE/'derived').glob('*.fbx')]):
    if (STAGE/'repaired'/source.name).exists():source=STAGE/'repaired'/source.name
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.ops.import_scene.fbx(filepath=str(source))
    solids=[o for o in bpy.data.objects if o.type=='MESH' and not o.name.startswith('UCX_')]
    assert solids,source
    had_children=len(solids)>1
    if had_children:
        bpy.ops.object.select_all(action='DESELECT')
        for o in solids:o.select_set(True)
        bpy.context.view_layer.objects.active=next((o for o in solids if o.name==source.stem),solids[0])
        bpy.ops.object.join()
        solids=[bpy.context.object]
    obj=solids[0]; obj.data.calc_loop_triangles()
    bounds=[obj.matrix_world@v.co for v in obj.data.vertices]
    used=Counter(obj.data.materials[p.material_index].name for p in obj.data.polygons)
    ucx=[o for o in bpy.data.objects if o.name.startswith('UCX_')]
    repaired=None
    if had_children or (ucx and obj.name!=source.stem):
        obj.name=source.stem
        repaired=STAGE/'repaired'/source.name; repaired.parent.mkdir(exist_ok=True)
        bpy.ops.object.select_all(action='SELECT')
        bpy.ops.export_scene.fbx(filepath=str(repaired),use_selection=True,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',
                                 bake_anim=False,mesh_smooth_type='FACE',colors_type='LINEAR')
    results[source.stem]={'source':(repaired or source).relative_to(STAGE).as_posix(),
                          'size_cm':[round((max(v[i] for v in bounds)-min(v[i] for v in bounds))*100,4) for i in range(3)],
                          'triangles':len(obj.data.loop_triangles),'faces_by_material':dict(used),'ucx_count':len(ucx),
                          'render_name':obj.name,'color_layers':[a.name for a in obj.data.color_attributes],
                          'sockets':[o.name for o in bpy.data.objects if o.name.startswith('SOCKET_')]}
    print('T193_PREFLIGHT',source.stem,len(used),len(ucx),flush=True)
assert len(results)==72,len(results)
(STAGE/'preflight-result.json').write_text(json.dumps({'task':'T-193','meshes':results,'finished_at':dt.datetime.now(dt.timezone.utc).isoformat()},indent=2),encoding='utf-8')
print('T193_PREFLIGHT_SUCCESS',len(results),flush=True)
