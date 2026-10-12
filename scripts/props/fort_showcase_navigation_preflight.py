"""Read-only source UCX/terrain sampling to catch obvious blocked showroom routes."""
import json
from pathlib import Path
import bpy
from mathutils import Vector,Matrix
from mathutils.bvhtree import BVHTree

ROOT=Path(__file__).resolve().parents[2]
STAGE=ROOT/'art/export/Fort_T193'
bpy.ops.wm.open_mainfile(filepath='C:/Users/matth/OneDrive/maquette-blender/maquettes/fort-francais/fort-francais.blend',load_ui=False)
before=set(bpy.data.objects)
bpy.ops.import_scene.fbx(filepath=str(STAGE/'repaired/SM_Fort_GovernorHouse.fbx'))
governor=next(o for o in bpy.data.objects if o not in before and o.type=='MESH' and not o.name.startswith('UCX_'))
vertices=[];faces=[];owners=[]
for inst in bpy.context.scene.objects:
    if inst.type!='EMPTY' or not inst.instance_collection:continue
    coll=inst.instance_collection
    if not coll.name.startswith(('SM_Fort_','SM_Port_','REF-site')):continue
    complex_names={'SM_Fort_GovernorHouse','SM_Fort_Chapel','SM_Fort_TreasureVault','SM_Fort_Prison','SM_Fort_Barracks','SM_Fort_Kitchen','SM_Port_Warehouse'}
    inst_matrix=inst.matrix_world.copy()
    if coll.name=='SM_Fort_BridgeFixed':inst_matrix=inst_matrix@Matrix.Diagonal((1,1.65,1,1))
    if coll.name=='SM_Port_Jetty_6m' and abs(inst.location.y+45)<.01:inst_matrix=inst_matrix@Matrix.Diagonal((1,1.4,1,1))
    if coll.name=='SM_Fort_StuddedDoor' and inst.name.endswith('_00'):
        inst_matrix=Matrix.Translation(inst.matrix_world.translation)
    for obj in coll.all_objects:
        if obj.type!='MESH':continue
        is_ucx=obj.name.startswith('UCX_')
        terrain=obj.name.startswith(('REF-Site_Terrain','REF-Court_Paths'))
        if coll.name in complex_names:
            if is_ucx:continue
        elif not (is_ucx or terrain):continue
        if coll.name=='SM_Fort_GovernorHouse':obj=governor
        matrix=inst_matrix@obj.matrix_world
        if coll.name=='SM_Fort_VaultGrille':matrix.translation.z+=2.5
        offset=len(vertices);vertices.extend(matrix@v.co for v in obj.data.vertices)
        faces.extend(tuple(offset+i for i in poly.vertices) for poly in obj.data.polygons)
        owners.extend([inst.name+' / '+obj.name]*len(obj.data.polygons))
bvh=BVHTree.FromPolygons(vertices,faces)
routes=[('gate',(0,-30,0),(0,-14,0)),('ramp',(-19,-4.5,0),(-19,12,2.97)),
        ('stairs',(-8.7,-15.4,0),(-15.5,-15.4,2.97)),('governor',(0,4.5,0),(0,8.2,.495)),
        ('governor_upper',(.85,8.1,.495),(.85,12.8,3.135)),('governor_balcony',(0,8.1,3.135),(0,6.5,3.135)),
        ('chapel',(14,5.7,0),(14,8.2,.33)),('prison',(9.5,-6.7,0),(12.2,-6.7,0)),
        ('vault',(-19,-14.8,0),(-19,-17.2,0)),('vault_descent',(-19,-17.2,0),(-23,-17.2,-.99)),
        ('vault_inner',(-23,-17.2,-.99),(-23,-19.1,-.99)),('vault_room',(-23.3,-19.1,-.99),(-23.3,-20.5,-.99)),('jetty',(0,-70,-.33),(0,-39,-.08))]
results=[]
for name,a,b in routes:
    a,b=Vector(a),Vector(b);count=max(2,int((b-a).length/.1));floor=a.z;issues=[];samples=[]
    direction=b-a;direction.z=0;direction.normalize()
    for i in range(count+1):
        pos=a.lerp(b,i/count)
        hit,normal,index,distance=bvh.ray_cast(Vector((pos.x,pos.y,floor+.42)),Vector((0,0,-1)),3)
        if hit is None:
            issues.append({'step':i,'kind':'no_floor','position':list(pos)});continue
        floor=hit.z;samples.append([pos.x,pos.y,floor])
        for z in (.40,.72,1.0):
            p=Vector((pos.x,pos.y,floor+z))
            for side in (direction,-direction,Vector((-direction.y,direction.x,0)),Vector((direction.y,-direction.x,0))):
                hit,normal,index,distance=bvh.ray_cast(p,side,.30)
                if hit is not None and abs(normal.z)<.65:
                    issues.append({'step':i,'kind':'clearance_under_30cm','position':list(p),'hit':list(hit),'normal':list(normal),'owner':owners[index]});break
    result={'name':name,'issues':issues,'floor_samples':samples};results.append(result)
    print('T193_NAV_PREFLIGHT',name,'issues',len(issues),'first',issues[:2],flush=True)
(STAGE/'navigation-preflight.json').write_text(json.dumps({'task':'T-193','scope':'Approximate Blender UCX ray sampling; UE capsule traversal still required','routes':results},indent=2),encoding='utf-8')
