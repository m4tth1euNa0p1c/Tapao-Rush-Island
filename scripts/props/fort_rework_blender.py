"""T198 art revision: furnished interiors, cast gold, filled bastioned moat.

Run in Blender. Delivery FBX files and OneDrive helpers are read-only inputs.
All outputs, including the editable blend scene, remain in Fort_T198.
"""
import sys
sys.dont_write_bytecode=True
from pathlib import Path
import json
import math
import random
from collections import defaultdict
import bpy
import bmesh
from mathutils import Matrix,Vector

ROOT=Path(__file__).resolve().parents[2]
SRC=ROOT/'art/export/Fort_T193'
OUT=ROOT/'art/export/Fort_T198'
OUT.mkdir(exist_ok=True);(OUT/'exports').mkdir(exist_ok=True)
sys.path.insert(0,'C:/Users/matth/OneDrive/maquette-blender/factory/kits')
import bean_kit  # Initialize the kit's mutually dependent registration modules first.
import fort_geo as G
import fort_site as SITE
RULES=json.loads((ROOT/'scripts/props/fort_rework_rules.json').read_text())
RNG=random.Random(198)
REPORT={'task':'T-198','meshes':{},'rooms':{},'moat':{'surface_local_m':-.50,'depth_m':.82}}
V=Vector


def mat(name):
    return bpy.data.materials.get(name) or bpy.data.materials.new(name)


def reset():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    return bpy.context.scene.collection


class Furniture:
    def __init__(self,name):
        self.fb=G.FB(name);self.fb.blend=(0,0,0);self.counts=defaultdict(int)
    def box(self,c,size,material='MAT-Fort_WoodTamanu',bevel=.006):
        self.fb.wood(c,size,mat(material),bevel=bevel)
    def rod(self,a,b,r=.012,material='MAT-Fort_Copper'):
        self.fb.cyl(V(a),V(b),r,mat(material),segs=12)
    def placed(self,kind,pos,angle=0,**kwargs):
        start=len(self.fb.bm.verts)
        getattr(self,kind)(**kwargs)
        transform=Matrix.Translation(V(pos))@Matrix.Rotation(math.radians(angle),4,'Z')
        bmesh.ops.transform(self.fb.bm,verts=list(self.fb.bm.verts)[start:],matrix=transform)
        self.counts[kind]+=1
    def book(self):
        self.box((0,0,.013),(.14,.10,.026),'MAT-Rework_Leather',.003)
        self.box((0,-.002,.014),(.132,.091,.014),'MAT-Rework_Parchment',.001)
        for x in (-.05,.05):self.box((x,0,.028),(.002,.09,.001),'MAT-Rework_GoldStamp',.0002)
    def cushion(self,width=.64,length=1.36,cloth='MAT-Rework_Linen'):
        self.box((0,0,.06),(width,length,.12),cloth,.035)
        # Soft rolled edge and stitched seams, independent from the wooden frame.
        for side in (-1,1):
            self.fb.tube([V((side*(width/2-.018),-length/2+.04,.073)),V((side*(width/2-.018),length/2-.04,.073))],.004,mat('MAT-Rework_Linen'),segs=6)
    def bed(self,canopy=False,cloth='MAT-Rework_BlueCloth',bunk=False):
        w,L=.76,1.5
        for x in (-w/2+.035,w/2-.035):
            for y in (-L/2+.035,L/2-.035):
                h=1.62 if bunk or canopy else (.75 if y>0 else .38)
                self.box((x,y,h/2),(.065,.065,h),'MAT-Fort_WoodOak',.008)
                self.fb.sphere((x,y,h+.025),.04,mat('MAT-Fort_WoodOak'),u=12,v=8)
        for z in ((.30,1.02) if bunk else (.30,)):
            self.box((0,0,z-.045),(w,L,.075),'MAT-Fort_WoodOak',.012)
            self.placed('cushion',(0,0,z),width=w-.08,length=L-.10)
            self.box((0,-.19,z+.13),(w-.07,.97,.038),cloth,.024)
            for k in range(9):
                x=-.3+k*.075
                self.fb.tube([V((x,-.67,z+.138)),V((x,-.2,z+.154)),V((x,.24,z+.145))],.0035,mat(cloth),segs=6)
            self.box((0,.49,z+.16),(.46,.28,.105),'MAT-Rework_Linen',.045)
            self.box((0,.70,z+.23),(w-.05,.045,.42),'MAT-Fort_WoodOak',.008)
        if canopy:
            for x in (-.35,.35):self.box((x,0,1.58),(.05,1.49,.06),'MAT-Fort_WoodOak')
            self.box((0,.69,1.61),(.76,.12,.04),cloth,.01)
        if bunk:
            for x in (-.24,.24):self.rod((x,-.85,.05),(x,-.77,1.35),.023,'MAT-Fort_WoodOak')
            for z in (.22,.44,.66,.88,1.10):self.rod((-.24,-.81,z),(.24,-.81,z),.021,'MAT-Fort_WoodOak')
    def table(self,width=1.1,depth=.6,height=.61):
        self.box((0,0,height-.035),(width,depth,.07),'MAT-Fort_WoodOak',.012)
        for x in (-width/2+.08,width/2-.08):
            for y in (-depth/2+.07,depth/2-.07):self.box((x,y,(height-.07)/2),(.065,.065,height-.07))
        self.box((0,0,.16),(width-.10,.06,.045))
        for x in (-width/2+.06,width/2-.06):self.box((x,0,height-.13),(.05,depth-.07,.14))
    def chair(self):
        self.box((0,0,.30),(.39,.38,.05),'MAT-Fort_WoodOak',.012)
        for x in (-.16,.16):
            for y in (-.15,.15):
                h=.78 if y>0 else .28
                self.box((x,y,h/2),(.045,.045,h))
        for x in (-.11,0,.11):self.box((x,.15,.56),(.025,.03,.40))
        self.box((0,.15,.77),(.38,.055,.06),'MAT-Fort_WoodOak',.014)
    def chest(self,width=.64):
        self.box((0,0,.23),(width,.40,.40),'MAT-Fort_WoodOak',.018)
        self.box((0,0,.46),(width+.025,.43,.055),'MAT-Fort_WoodOak',.016)
        for x in (-width*.33,width*.33):
            self.box((x,-.209,.24),(.047,.016,.40),'MAT-Fort_IronRust',.003)
            self.box((x,0,.493),(.047,.42,.013),'MAT-Fort_IronRust',.003)
        self.box((0,-.218,.38),(.08,.025,.11),'MAT-Fort_Copper',.006)
        for x in (-width/2+.06,width/2-.06):
            for y in (-.14,.14):self.box((x,y,.045),(.06,.06,.09))
    def shelf(self,width=1.4,height=1.35,depth=.37,stock=True):
        for x in (-width/2+.03,width/2-.03):
            for y in (-depth/2+.02,depth/2-.02):self.box((x,y,height/2),(.045,.045,height))
        for z in (.15,.69,1.22):
            if z>height:continue
            self.box((0,0,z),(width,depth,.045),'MAT-Fort_WoodOak',.004)
            if stock:
                for i in range(max(1,int(width/.27))):
                    x=-width/2+.15+i*.27
                    self.placed('book',(x,-.04,z+.04),angle=RNG.uniform(-7,7))
        self.box((0,depth/2-.02,height/2),(width,.035,height),'MAT-Fort_WoodWeathered',.003)
    def desk(self):
        self.table(width=1.28,depth=.62,height=.65)
        self.box((-.25,-.02,.695),(.38,.30,.006),'MAT-Rework_Parchment',.001)
        for k in range(5):self.box((-.28,-.12+k*.045,.699),(.24,.004,.001),'MAT-Rework_Ink',.0001)
        self.placed('book',(.28,.05,.688),angle=-9)
        self.fb.sphere((.47,.12,.72),.04,mat('MAT-Rework_Ceramic'),u=16,v=10,scale=(1,1,.9))
        self.rod((.47,.12,.72),(.5,.13,.93),.005,'MAT-Rework_Parchment')
        self.box((0,-.22,.54),(.80,.12,.14),'MAT-Fort_WoodOak',.006)
        self.rod((-.08,-.29,.55),(.08,-.29,.55),.012)
    def rug(self,width=1.4,length=2.0,cloth='MAT-Rework_RedCloth'):
        self.box((0,0,.012),(width,length,.018),cloth,.004)
        for x in (-width/2+.08,width/2-.08):self.box((x,0,.023),(.035,length-.09,.003),'MAT-Rework_Linen',.001)
        for y in (-length/2+.075,length/2-.075):self.box((0,y,.023),(width-.09,.035,.003),'MAT-Rework_Linen',.001)
        for x in range(20):
            for sy in (-1,1):self.rod((-width/2+.03+x*(width-.06)/19,sy*length/2,.015),(-width/2+.03+x*(width-.06)/19,sy*(length/2+.055),.015),.003,'MAT-Rework_Linen')
    def jar(self,height=.28,radius=.13):
        self.fb.sphere((0,0,height*.46),radius,mat('MAT-Rework_Ceramic'),u=20,v=12,scale=(1,1,height/radius*.47))
        self.rod((0,0,height*.72),(0,0,height),radius*.54,'MAT-Rework_Ceramic')
        self.rod((0,0,height),(0,0,height+.015),radius*.66,'MAT-Fort_WoodOak')
    def bench(self,width=1.2,back=False):
        self.box((0,0,.33),(width,.32,.055),'MAT-Fort_WoodOak',.012)
        for x in (-width/2+.12,width/2-.12):self.box((x,0,.16),(.09,.27,.32))
        if back:
            for x in (-width/2+.06,width/2-.06):self.box((x,.12,.44),(.05,.05,.64))
            self.box((0,.13,.68),(width,.06,.22),'MAT-Fort_WoodOak',.015)


def prune_furniture(obj,name):
    """Remove old, isolated furniture primitives; retain shells, floors and stairs."""
    bm=bmesh.new();bm.from_mesh(obj.data);seen=set();remove=[];count=0
    for start in bm.verts:
        if start in seen:continue
        stack=[start];seen.add(start);verts=[]
        while stack:
            v=stack.pop();verts.append(v)
            for edge in v.link_edges:
                other=edge.other_vert(v)
                if other not in seen:seen.add(other);stack.append(other)
        lo=[min(v.co[i] for v in verts) for i in range(3)];hi=[max(v.co[i] for v in verts) for i in range(3)]
        materials={obj.data.materials[f.material_index].name for v in verts for f in v.link_faces}
        wood=any(any(s in m for s in ('Wood','Tamanu','Jute','Copper','Iron','Rope')) for m in materials)
        if not wood:continue
        inside=lambda x0,x1,y0,y1,z0,z1: lo[0]>=x0 and hi[0]<=x1 and lo[1]>=y0 and hi[1]<=y1 and lo[2]>=z0-.025 and hi[2]<=z1
        delete=False
        if name=='SM_Fort_GovernorHouse':
            delete=any(inside(a,b,-2.85,2.9,z,z+2.1) for a,b in [(-6.45,-1.65),(1.65,6.45)] for z in (.495,3.135))
        elif name=='SM_Fort_Barracks':delete=any(inside(a,b,-2.20,2.49,.165,2.35) for a,b in [(-7.45,-.30),(.30,7.45)])
        elif name=='SM_Fort_GuardHouse':delete=inside(-2.99,2.99,-1.85,2.01,.16,2.0)
        elif name=='SM_Fort_Kitchen':delete=inside(-1.15,.56,-1.19,-.4,.165,1.1)
        elif name=='SM_Fort_Prison':delete=inside(-.70,2.35,-2.82,2.82,0,1.1)
        elif name=='SM_Fort_Chapel':delete=inside(-1.92,1.92,-2.8,2.0,.33,1.25)
        elif name=='SM_Fort_TreasureVault':delete=inside(-2.51,2.51,-2.05,2.05,0,1.35)
        elif name=='SM_Port_Warehouse':delete=inside(-4.4,4.4,-2.3,2.45,.165,1.6)
        if delete:remove.extend(verts);count+=1
    if remove:bmesh.ops.delete(bm,geom=remove,context='VERTS')
    bm.to_mesh(obj.data);bm.free();obj.data.update()
    return count


def furnish(name,f):
    p=f.placed
    if name=='SM_Fort_GovernorHouse':
        p('rug',(-3.9,-.05,.52),width=2.9,length=3.4)
        p('table',(-4.0,0,.52),width=1.6,depth=.78)
        for x in (-4.55,-3.95,-3.35):
            p('chair',(x,-.68,.52),180);p('chair',(x,.68,.52))
        p('shelf',(-4.1,2.60,.52),width=2.2,height=1.55)
        p('desk',(4.5,1.3,.52));p('chair',(4.5,.57,.52),180)
        p('shelf',(6.12,-.4,.52),-90,width=1.7,height=1.5)
        p('chest',(5.4,2.57,.52),width=.75)
        p('bed',(-4.5,1.0,3.135),canopy=True)
        p('bed',(4.5,1.0,3.135),cloth='MAT-Rework_Linen')
        for x in (-5.15,5.15):p('table',(x,1.5,3.135),width=.38,depth=.38,height=.47);p('book',(x,1.5,3.615))
        p('chest',(-3.1,2.25,3.135));p('shelf',(3.0,2.65,3.135),width=1.0,height=1.4)
        p('rug',(4.0,-.6,3.14),width=2.3,length=1.3,cloth='MAT-Rework_BlueCloth')
    elif name=='SM_Fort_Barracks':
        for side in (-1,1):
            for k in range(4):
                x=side*(1.12+k*1.58)
                p('bed',(x,1.50,.175),90,bunk=True)
                p('chest',(x,.55,.175),width=.50)
            p('table',(side*5.8,-1.2,.175),width=1.15,depth=.6)
            p('bench',(side*5.8,-1.87,.175),width=1.15)
            p('shelf',(side*1.3,-2.18,.175),180,width=1.2,height=1.4)
    elif name=='SM_Fort_GuardHouse':
        for x in (-2.2,-1.2):p('bed',(x,1.10,.175),cloth='MAT-Rework_BlueCloth')
        p('desk',(1.4,1.10,.175));p('chair',(1.4,.42,.175),180)
        p('shelf',(2.6,-.6,.175),-90,width=1.15)
        p('chest',(-2.45,-.85,.175))
    elif name=='SM_Fort_Kitchen':
        p('table',(1.45,-1.02,.175),width=1.15,depth=.56,height=.60)
        p('shelf',(-2.15,-.15,.175),90,width=1.4,height=1.5,depth=.40,stock=False)
        for z in (.36,.90,1.43):
            for y in (-.60,-.16,.25):p('jar',(-2.10,y,z),height=.23,radius=.10)
        p('jar',(1.73,-.96,.785),height=.20,radius=.09)
        p('book',(1.12,-1.02,.79),12)
    elif name=='SM_Fort_Prison':
        for y in (-1.9,0,1.9):
            p('bed',(1.40,y,.03),90,cloth='MAT-Rework_Linen')
            p('jar',(.80,y+.47,.025),height=.23,radius=.13)
            p('rug',(.3,y-.12,.025),width=.72,length=.86,cloth='MAT-Fort_Jute')
    elif name=='SM_Fort_Chapel':
        for y in (-1.75,-.50,.75):
            for x in (-1.35,1.35):p('bench',(x,y,.34),width=1.0,back=True)
        p('rug',(0,.7,.345),width=1.05,length=3.8)
        p('book',(-.20,2.58,1.00),12)
    elif name=='SM_Fort_TreasureVault':
        p('table',(-.85,-.05,.025),width=1.6,depth=.9,height=.65)
        p('book',(-.30,.22,.68),9);p('chair',(-.85,-.8,.025),180)
        p('shelf',(-2.13,.1,.025),90,width=2.9,height=1.30,depth=.48,stock=False)
        p('shelf',(2.15,-.75,.025),-90,width=1.7,height=1.25,depth=.43,stock=False)
        p('rug',(.65,-.35,.025),width=1.08,length=2.0,cloth='MAT-Rework_BlueCloth')
        p('table',(-.9,1.80,.025),width=1.8,depth=.40,height=.42)
    elif name=='SM_Port_Warehouse':
        for k in range(3):
            f.box((0,-3.80+k*.30,-.265+k*.165),(1.80,.34,.20),'MAT-Fort_CoralAshlar',.012)
        for x in (-3.1,2.7):p('shelf',(x,2.15,.175),width=2.35,height=1.70,depth=.58,stock=False)
        p('desk',(-3.4,-1.6,.175),180);p('chair',(-3.4,-.93,.175))
        for x,y in [(-3.6,.5),(-2.65,.5),(3.3,.7),(3.3,1.4),(2.45,1.4)]:p('chest',(x,y,.18),width=.74)
    elif name=='SM_Fort_SignalTower':
        p('table',(.35,0,2.66),width=.7,depth=.5,height=.57)
        p('book',(.35,0,3.245),15)
        p('chest',(.90,.65,.04),width=.48)
    elif name=='SM_Fort_PowderMagazine':
        p('rug',(0,-.25,.035),width=.65,length=1.4,cloth='MAT-Fort_Jute')


def export(name,obj,extras=(),collision='complex'):
    obj.name=name;obj.data.name=name
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
    for item in extras:item.select_set(True)
    bpy.context.view_layer.objects.active=obj
    path=OUT/'exports'/(name+'.fbx')
    bpy.ops.export_scene.fbx(filepath=str(path),use_selection=True,apply_unit_scale=True,axis_forward='-Y',axis_up='Z',bake_anim=False,mesh_smooth_type='FACE',colors_type='LINEAR')
    points=[obj.matrix_world@v.co for v in obj.data.vertices]
    used=sorted({obj.data.materials[p.material_index].name for p in obj.data.polygons})
    obj.data.calc_loop_triangles()
    REPORT['meshes'][name]={'file':str(path),'materials':used,'triangles':len(obj.data.loop_triangles),
        'size_cm':[(max(v[i] for v in points)-min(v[i] for v in points))*100 for i in range(3)],'collision':collision,
        'ucx_count':sum(o.name.startswith('UCX_') for o in extras)}
    print('T198_EXPORT',name,len(obj.data.loop_triangles),used,flush=True)
    # One editable source per delivered module, without touching OneDrive.
    blend_dir=OUT/'blend';blend_dir.mkdir(exist_ok=True)
    bpy.ops.wm.save_as_mainfile(filepath=str(blend_dir/(name+'.blend')),check_existing=False)


def buildings():
    for name in ['SM_Fort_GovernorHouse','SM_Fort_Barracks','SM_Fort_GuardHouse','SM_Fort_Kitchen',
                 'SM_Fort_Prison','SM_Fort_Chapel','SM_Fort_TreasureVault','SM_Port_Warehouse',
                 'SM_Fort_SignalTower','SM_Fort_PowderMagazine']:
        coll=reset();source=SRC/'repaired'/(name+'.fbx')
        if not source.exists() or name in ('SM_Fort_GovernorHouse','SM_Fort_SignalTower'):source=SRC/'exports'/(name+'.fbx')
        bpy.ops.import_scene.fbx(filepath=str(source));obj=bpy.data.objects[name]
        removed=prune_furniture(obj,name)
        if name=='SM_Fort_SignalTower':
            bpy.ops.mesh.primitive_cube_add(size=1,location=(-1.18,1.2,2.53));cutter=bpy.context.object;cutter.dimensions=(1.06,1.1,.52)
            bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
            mod=obj.modifiers.new('FirstLandingHeadroom','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter;mod.use_self=True
            bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cutter,do_unlink=True)
            # Move complete hatch islands only. Vertex-window selection also
            # catches one end face of the top step and distorts that tread.
            bm=bmesh.new();bm.from_mesh(obj.data);seen=set();hatch=[]
            for start in bm.verts:
                if start in seen:continue
                todo=[start];seen.add(start);component=[]
                while todo:
                    v=todo.pop();component.append(v)
                    for edge in v.link_edges:
                        other=edge.other_vert(v)
                        if other not in seen:seen.add(other);todo.append(other)
                if all(-.76<v.co.x<-.55 and .49<v.co.y<1.76 and 5.23<v.co.z<6.31 for v in component):hatch.extend(component)
            assert len(hatch)>15,len(hatch)
            hinge=V((-.67,.55,5.28));transform=Matrix.Translation(hinge)@Matrix.Rotation(-math.pi/2,4,'Z')@Matrix.Translation(-hinge)
            bmesh.ops.transform(bm,verts=hatch,matrix=transform);bm.to_mesh(obj.data);bm.free()
            # Cut only masonry, preserving the wooden steps below the opening.
            # The old cut touched the slab's underside and left collision faces.
            stone_slots={i for i,m in enumerate(obj.data.materials) if m and 'Coral' in m.name}
            bm=bmesh.new();bm.from_mesh(obj.data)
            bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.material_index not in stone_slots],context='FACES')
            data=bpy.data.meshes.new('TowerMasonry');bm.to_mesh(data);bm.free()
            for m in obj.data.materials:data.materials.append(m or mat('MAT-Fort_CoralRubble'))
            stone=bpy.data.objects.new('TowerMasonry',data);coll.objects.link(stone)
            bm=bmesh.new();bm.from_mesh(obj.data)
            bmesh.ops.delete(bm,geom=[f for f in bm.faces if f.material_index in stone_slots],context='FACES');bm.to_mesh(obj.data);bm.free()
            for center,size in [((-.1,-1.2,5.1),(2.72,1.04,1.0)),((-1.2,-.55,5.1),(1.04,2.30,1.0))]:
                bpy.ops.mesh.primitive_cube_add(size=1,location=center);cutter=bpy.context.object;cutter.dimensions=size
                bpy.ops.object.transform_apply(location=False,rotation=False,scale=True)
                mod=stone.modifiers.new('ClearRoofUnderside','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter;mod.use_self=True
                bpy.context.view_layer.objects.active=stone;bpy.ops.object.modifier_apply(modifier=mod.name)
                bpy.data.objects.remove(cutter,do_unlink=True)
            bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);stone.select_set(True)
            bpy.context.view_layer.objects.active=obj;bpy.ops.object.join()
        if name=='SM_Fort_GovernorHouse':
            cutters=[]
            for x,z,width,height in [(0,1.49,1.20,1.98),(-3,4.03,.88,1.80),(0,4.03,.88,1.80),(3,4.03,.88,1.80)]:
                bpy.ops.mesh.primitive_cube_add(size=1,location=(x,-3.28,z));box=bpy.context.object;box.dimensions=(width,.90,height)
                bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);cutters.append(box)
            for center,size in [((.85,-.94,3.1),(1.12,.92,.30)),((.38,-2.43,1.35),(.13,1.10,1.50)),
                                ((1.5,2.25,1.44),(.50,1.20,1.90))]:
                bpy.ops.mesh.primitive_cube_add(size=1,location=center);box=bpy.context.object;box.dimensions=size
                bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);cutters.append(box)
            bpy.ops.object.select_all(action='DESELECT')
            for c in cutters:c.select_set(True)
            bpy.context.view_layer.objects.active=cutters[0];bpy.ops.object.join();cutter=bpy.context.object
            mod=obj.modifiers.new('PreserveOpenAccess','BOOLEAN');mod.operation='DIFFERENCE';mod.solver='EXACT';mod.object=cutter;mod.use_self=True
            bpy.context.view_layer.objects.active=obj;bpy.ops.object.modifier_apply(modifier=mod.name);bpy.data.objects.remove(cutter,do_unlink=True)
        f=Furniture(name+'_Furnishing');furnish(name,f)
        furniture=f.fb.finish(coll)
        bpy.ops.object.select_all(action='DESELECT');obj.select_set(True);furniture.select_set(True)
        bpy.context.view_layer.objects.active=obj;bpy.ops.object.join()
        extras=[o for o in bpy.data.objects if o.name.startswith(('UCX_','SOCKET_'))]
        export(name,obj,extras)
        REPORT['rooms'][name]={'old_primitives_removed':removed,'new_furniture':dict(f.counts)}


def gold():
    coll=reset();f=Furniture('GoldBlank');fb=f.fb
    bottom=[V((x,y,0)) for x,y in [(-.09,-.042),(.09,-.042),(.09,.042),(-.09,.042)]]
    top=[V((x,y,.0245)) for x,y in [(-.082,-.034),(.082,-.034),(.082,.034),(-.082,.034)]]
    fb.loft(bottom,top,mat('MAT_Gold_Aged'),bevel=.0015)
    obj=fb.finish(coll);text_objects=[]
    for label,size,y in [('OR FIN',.012,.008),('5.5 KG  999.9',.0068,-.009),('CF 0178',.0055,-.023)]:
        curve=bpy.data.curves.new('GoldStamp','FONT');curve.body=label;curve.size=size;curve.align_x='CENTER';curve.extrude=.00012;curve.bevel_depth=0;curve.resolution_u=2
        text=bpy.data.objects.new('Stamp',curve);coll.objects.link(text);text.location=(0,y,.0245);curve.materials.append(mat('MAT-Rework_GoldStamp'))
        bpy.ops.object.select_all(action='DESELECT');text.select_set(True);bpy.context.view_layer.objects.active=text;bpy.ops.object.convert(target='MESH');text_objects.append(bpy.context.object)
    bpy.ops.object.select_all(action='DESELECT');obj.select_set(True)
    for t in text_objects:t.select_set(True)
    bpy.context.view_layer.objects.active=obj;bpy.ops.object.join()
    template=obj.data.copy();bpy.data.objects.remove(obj,do_unlink=True)
    for name,count in [('SM_Loot_GoldBar',1),*RULES['stacks'].items()]:
        parts=[];hulls=[]
        per_layer=1 if count==1 else (2 if count==6 else 3 if count==12 else 4)
        for i in range(count):
            layer=i//per_layer;slot=i%per_layer
            cols=2 if per_layer==4 else 1;rows=per_layer//cols
            x=(slot%cols-(cols-1)/2)*.186;y=(slot//cols-(rows-1)/2)*.088
            x+=(layer%2)*.002
            mesh=bpy.data.objects.new('CastBar',template.copy());coll.objects.link(mesh);mesh.location=(x,y,layer*.027);mesh.rotation_euler.z=RNG.uniform(-.013,.013);parts.append(mesh)
            bpy.ops.mesh.primitive_cube_add(size=1,location=(x,y,layer*.027+.0123))
            hull=bpy.context.object;hull.name=f'UCX_{name}_{i:02d}';hull.dimensions=(.178,.082,.0245);bpy.ops.object.transform_apply(location=False,rotation=False,scale=True);hulls.append(hull)
        bpy.ops.object.select_all(action='DESELECT')
        for o in parts:o.select_set(True)
        bpy.context.view_layer.objects.active=parts[0];bpy.ops.object.join();main=bpy.context.object
        bpy.ops.object.transform_apply(location=True,rotation=True,scale=True)
        export(name,main,hulls,'simple')
        REPORT['meshes'][name].update(bar_count=count,mass_kg=count*RULES['gold_bar']['mass_kg'],value=count*RULES['gold_bar']['value'])
        for o in [main,*hulls]:bpy.data.objects.remove(o,do_unlink=True)


def moat():
    coll=reset();N=384
    inner=SITE.resample_polar(SITE.offset_exact(SITE.trace(),.62,step=.3),N)
    outer=SITE.resample_polar(SITE.offset_exact(SITE.trace(),6.65,step=.3),N)
    fb=G.FB('MoatWater');fb.blend=(.42,0,0)
    for i in range(N):
        j=(i+1)%N
        fb.poly_bm([V((inner[i][0],inner[i][1],-.5)),V((outer[i][0],outer[i][1],-.5)),
                    V((outer[j][0],outer[j][1],-.5)),V((inner[j][0],inner[j][1],-.5))],[(0,1,2,3)],mat('MAT-Rework_MoatWater'))
    export('SM_Fort_MoatWater',fb.finish(coll),collision='none')
    f=Furniture('MoatSteps')
    for k in range(8):
        top=-k*.165
        f.box((7,-27.05+(k+.5)*.26,top-.0825),(1.25,.29,.165),'MAT-Fort_CoralAshlar',.018)
    for x in (6.32,7.68):
        f.rod((x,-27.2,.72),(x,-25.18,-.58),.033,'MAT-Fort_WoodWeathered')
    export('SM_Fort_MoatSteps',f.fb.finish(coll))


def courtyard():
    coll=reset();f=Furniture('CourtyardDetails')
    # Seats outside the governor and chapel, planted stone tubs and a covered
    # notice board along the kitchen wall. Main axes and doorways stay clear.
    for x,y,a in [(-5,6.55,0),(5,6.55,0),(11,5.2,90),(-10,-7,90)]:
        f.placed('bench',(x,y,.08),a,width=1.8,back=True)
    for x,y in [(-6.3,6.4),(6.3,6.4),(11,3.7),(-10,-8.5)]:
        n=16;f.fb.blend=(0,0,0)
        for k in range(n):
            t=k*math.tau/n;u=(k+1)*math.tau/n
            bottom=[V((x+r*math.cos(a),y+r*math.sin(a),.02)) for r,a in [(.3,t),(.43,t),(.43,u),(.3,u)]]
            top=[V((v.x,v.y,.46)) for v in bottom];f.fb.loft(bottom,top,mat('MAT-Fort_CoralAshlar'),bevel=.005)
        f.fb.cyl(V((x,y,.30)),V((x,y,.34)),.31,mat('MAT-Rework_Leather'),segs=20)
        # T208 plants are native Raraka naupaka instances, placed separately.
    for x in (-7.7,-6.3):f.box((x,-7.9,.72),(.075,.075,1.44),'MAT-Fort_WoodWeathered')
    f.box((-7,-7.9,1.15),(1.6,.075,.7),'MAT-Fort_WoodOak')
    for x in (-7.45,-7,-6.55):f.box((x,-7.95,1.16),(.34,.008,.45),'MAT-Rework_Parchment',.001)
    export('SM_Fort_CourtyardDetails',f.fb.finish(coll))


def ditch():
    reset();name='SM_Fort_Ditch'
    bpy.ops.import_scene.fbx(filepath=str(SRC/'exports'/(name+'.fbx')))
    obj=bpy.data.objects[name];bm=bmesh.new();bm.from_mesh(obj.data)
    faces=[f for f in bm.faces if obj.data.materials[f.material_index].name=='MAT-Fort_Puddle']
    bmesh.ops.delete(bm,geom=faces,context='FACES');bm.to_mesh(obj.data);bm.free()
    export(name,obj,[o for o in bpy.data.objects if o.name.startswith(('UCX_','SOCKET_'))])


if __name__=='__main__':
    if '--courtyard-only' in sys.argv:
        REPORT=json.loads((OUT/'build-result.json').read_text())
        courtyard()
    elif '--buildings-only' in sys.argv:
        REPORT=json.loads((OUT/'build-result.json').read_text())
        buildings()
    else:buildings();gold();moat();courtyard()
    ditch()
    (OUT/'build-result.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    print('T198_BUILD_COMPLETE',len(REPORT['meshes']),flush=True)
