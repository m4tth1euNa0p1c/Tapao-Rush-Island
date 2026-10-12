"""Open the local magazine's copper door for showroom exploration."""
import json
import math
from pathlib import Path
import bpy
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

STAGE=Path(__file__).resolve().parents[2]/'art/export/Fort_T193'
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(STAGE/'exports/SM_Fort_PowderMagazine.fbx'))
obj=bpy.data.objects['SM_Fort_PowderMagazine']
indices={v for p in obj.data.polygons if obj.data.materials[p.material_index].name=='MAT-Fort_Copper' for v in p.vertices}
assert indices,'Expected the copper door and its studs'
hinge=Vector((-.45,-1.95,0))
rotation=Matrix.Rotation(math.radians(-65),4,'Z')
transform=Matrix.Translation(hinge)@rotation@Matrix.Translation(-hinge)
local=obj.matrix_world.inverted()@transform@obj.matrix_world
for i in indices:obj.data.vertices[i].co=local@obj.data.vertices[i].co
obj.data.update()
bvh=BVHTree.FromPolygons([obj.matrix_world@v.co for v in obj.data.vertices],[list(p.vertices) for p in obj.data.polygons])
checks=[]
for x in (-.2,0,.2):
    for z in (.4,.8,1.2):
        hit,_,_,_=bvh.ray_cast(Vector((x,-2.9,z)),Vector((0,1,0)),1.7)
        assert hit is None,(x,z,list(hit))
        checks.append([x,z])
bpy.ops.object.select_all(action='SELECT')
target=STAGE/'repaired/SM_Fort_PowderMagazine.fbx'
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE',colors_type='LINEAR')
(STAGE/'access-repair-result.json').write_text(json.dumps({'task':'T-193','mesh':target.stem,'door_angle_degrees':-100,'clear_rays':checks},indent=2),encoding='utf-8')
print('T193_MAGAZINE_DOOR_OPEN',len(indices),flush=True)

bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(STAGE/'exports/SM_Fort_SignalTower.fbx'))
obj=bpy.data.objects['SM_Fort_SignalTower'];cutters=[]
# The original floor holes begin exactly at the step edge: the capsule head
# touches the ceiling before its center reaches that edge. Widen the openings.
for center,size in [((-1.16,1.2,2.53),(1.05,1.05,.44)),
                    ((-1.2,-.575,5.09),(1.03,2.23,.48)),
                    ((-.52,-1.21,5.09),(.43,1.0,.48))]:
    bpy.ops.mesh.primitive_cube_add(size=1,location=center)
    cutter=bpy.context.object;cutter.dimensions=size
    bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);cutters.append(cutter)
bpy.ops.object.select_all(action='DESELECT')
for cutter in cutters:cutter.select_set(True)
bpy.context.view_layer.objects.active=cutters[0];bpy.ops.object.join()
cutter=bpy.context.object
modifier=obj.modifiers.new('T193_StairHeadroom','BOOLEAN');modifier.operation='DIFFERENCE';modifier.solver='EXACT';modifier.use_self=True;modifier.object=cutter
bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=modifier.name)
bpy.data.objects.remove(cutter,do_unlink=True)
# Swing the upright hatch along the south edge, opening the terrace landing.
indices={v for p in obj.data.polygons
         if all(-.76<(obj.matrix_world@obj.data.vertices[v].co).x<-.55 and
                .49<(obj.matrix_world@obj.data.vertices[v].co).y<1.76 and
                5.23<(obj.matrix_world@obj.data.vertices[v].co).z<6.31 for v in p.vertices)
         for v in p.vertices}
assert len(indices)>15,len(indices)
hinge=Vector((-.67,.55,5.28))
local=obj.matrix_world.inverted()@Matrix.Translation(hinge)@Matrix.Rotation(math.radians(-90),4,'Z')@Matrix.Translation(-hinge)@obj.matrix_world
for i in indices:obj.data.vertices[i].co=local@obj.data.vertices[i].co
obj.data.update()
bpy.ops.object.select_all(action='SELECT')
target=STAGE/'repaired/SM_Fort_SignalTower.fbx'
bpy.ops.export_scene.fbx(filepath=str(target),use_selection=True,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE',colors_type='LINEAR')
print('T193_TOWER_HEADROOM_AND_HATCH',len(indices),flush=True)
