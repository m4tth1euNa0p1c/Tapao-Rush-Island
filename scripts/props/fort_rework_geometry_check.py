"""Offline headroom check on the actual revised Blender exports; not a PIE substitute."""
import json
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree
ROOT=Path(__file__).resolve().parents[2]
STAGE=ROOT/'art/export/Fort_T198'
report={'task':'T-198','scope':'Mesh vertical headroom rays, separate from CharacterMovement PIE','checks':[]}


def load(name):
    bpy.ops.wm.open_mainfile(filepath=str(STAGE/'blend'/(name+'.blend')),load_ui=False)
    o=bpy.data.objects[name]
    return BVHTree.FromPolygons([o.matrix_world@v.co for v in o.data.vertices],[list(p.vertices) for p in o.data.polygons])


def check(tree,name,x,y,floor,sideways_y=False):
    for offset in (-.25,0,.25):
        px=x if sideways_y else x+offset;py=y+offset if sideways_y else y
        hit,normal,index,distance=tree.ray_cast(Vector((px,py,floor+.035)),Vector((0,0,1)),1.09)
        report['checks'].append({'name':name,'x':px,'y':py,'floor':floor,'clear':hit is None,
                                 'hit':list(hit) if hit is not None else None})


tree=load('SM_Fort_SignalTower')
for k in range(8):
    check(tree,'tower_flight3_'+str(k),1.3-(k+.5)*.25,-1.2,2.64+(k+1)*.165,True)
    check(tree,'tower_flight4_'+str(k),-1.2,-.7+(k+.5)*.25,3.96+(k+1)*.165)
tree=load('SM_Port_Warehouse')
for k in range(3):check(tree,'warehouse_step_'+str(k),0,-3.8+k*.3,-.165+k*.165)
tree=load('SM_Fort_GovernorHouse')
for k in range(5):check(tree,'office_rear_access_'+str(k),.7+k*.45,2.3,.515)
report['passed']=all(c['clear'] for c in report['checks'])
(STAGE/'headroom-result.json').write_text(json.dumps(report,indent=2),encoding='utf-8')
print('T198_HEADROOM',report['passed'],len(report['checks']))
print('BLOCKED',[c for c in report['checks'] if not c['clear']])
