"""Repair the local governor export's overlapping door/window wall volumes.

The delivered wall_openings routine fills the lower opening again when an upper
opening shares its X interval. Cut only the modeled doorway; OneDrive is unchanged.
"""
import json
from pathlib import Path
import bpy
from mathutils import Vector
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
STAGE=ROOT/'art/export/Fort_T193'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(STAGE/'exports/SM_Fort_GovernorHouse.fbx'))
obj=bpy.data.objects['SM_Fort_GovernorHouse']
original_count=len(obj.data.polygons)
print('T193_REPAIR_MATRIX',list(obj.matrix_world),flush=True)
cutters=[]
for x,z,width,height in [(0,1.49,1.20,1.98),(-3,4.03,.88,1.80),(0,4.03,.88,1.80),(3,4.03,.88,1.80)]:
    bpy.ops.mesh.primitive_cube_add(size=1,location=(x,-3.28,z))
    box=bpy.context.object;box.dimensions=(width,.90,height)
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);cutters.append(box)
# Give the walking capsule headroom before it reaches the upstairs opening.
# Shorten the lower handrail so the entrance can reach the first steps.
for center,size in [((.85,-.94,3.1),(1.12,.92,.30)),((.38,-2.43,1.35),(.13,1.10,1.50))]:
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    box=bpy.context.object;box.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);cutters.append(box)
bpy.ops.object.select_all(action='DESELECT')
for box in cutters:box.select_set(True)
bpy.context.view_layer.objects.active=cutters[0];bpy.ops.object.join()
cutter=bpy.context.object;cutter.name='T193_DoorwayCutter'
modifier=obj.modifiers.new('T193_OpenMainDoor','BOOLEAN');modifier.operation='DIFFERENCE';modifier.solver='EXACT';modifier.object=cutter
modifier.use_self=True
bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=modifier.name)
bpy.data.objects.remove(cutter,do_unlink=True)
assert len(obj.data.polygons)>original_count*.90,'Boolean removed unrelated structure'
bvh=BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],[list(p.vertices) for p in obj.data.polygons])
checks=[]
for x0,heights in [(0,(.9,1.3,1.6)),(-3,(3.5,3.9,4.3)),(0,(3.5,3.9,4.3)),(3,(3.5,3.9,4.3))]:
  for x in (x0-.2,x0,x0+.2):
    for z in heights:
        hit,normal,index,distance=bvh.ray_cast(Vector((x,-3.8,z)),Vector((0,1,0)),1.3)
        assert hit is None,(x,z,list(hit))
        checks.append([x,z])
bpy.ops.object.select_all(action='SELECT')
target=STAGE/'repaired/SM_Fort_GovernorHouse.fbx';target.parent.mkdir(exist_ok=True)
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE',colors_type='LINEAR')
(STAGE/'repair-result.json').write_text(json.dumps({'task':'T-193','source':'exports/SM_Fort_GovernorHouse.fbx','repaired':target.relative_to(STAGE).as_posix(),
   'reason':'Stacked openings refill lower doorway in delivered mesh','before_faces':original_count,'after_faces':len(obj.data.polygons),'clear_rays':checks},indent=2),encoding='utf-8')
print('T193_REPAIR_SUCCESS',original_count,len(obj.data.polygons),flush=True)
