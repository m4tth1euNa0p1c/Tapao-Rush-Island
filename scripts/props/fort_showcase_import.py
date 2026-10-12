"""Import T193 staged modular fort, PBR layers, UCX, sockets and preview terrain."""
import datetime as dt
import hashlib
import json
from pathlib import Path
import re
import sys
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
STAGE=ROOT/'art/export/Fort_T193'
OUT=ROOT/'monke_island/Saved/T193'
DEST='/Game/MonkeIsland/Environment/FortFrench'
sys.path.insert(0,str(ROOT/'scripts/props'))
from import_galleon_t149 import Graph, linear, check_lock, load, save
EAL=ue.EditorAssetLibrary
MEL=ue.MaterialEditingLibrary
TOOLS=ue.AssetToolsHelpers.get_asset_tools()
MP=ue.MaterialProperty
F1,F2,F3=(ue.CustomMaterialOutputType.CMOT_FLOAT1,ue.CustomMaterialOutputType.CMOT_FLOAT2,ue.CustomMaterialOutputType.CMOT_FLOAT3)
SOURCES=json.loads((STAGE/'material-sources.json').read_text(encoding='utf-8'))
PREFLIGHT=json.loads((STAGE/'preflight-result.json').read_text(encoding='utf-8'))['meshes']
REPORT=json.loads((STAGE/'fort-report.json').read_text(encoding='utf-8'))
MODULES={n:i for lot in REPORT['modules'].values() for n,i in lot['modules'].items() if n.startswith('SM_')}
RESULT={'task':'T-193','materials':{},'meshes':{},'textures':[]}
TEXTURES={}
REIMPORT=OUT/'reimport-request.json'
REIMPORT_NAMES=json.loads(REIMPORT.read_text(encoding='utf-8')) if REIMPORT.exists() else []


def texture(relative,kind):
    key=(relative,kind)
    if key in TEXTURES:return TEXTURES[key]
    name='T_Fort_'+re.sub('[^A-Za-z0-9_]','_',Path(relative).stem)+'_'+hashlib.sha1(relative.encode()).hexdigest()[:6]
    path=DEST+'/Textures/'+name
    tex=load(path)
    if not tex:
        task=ue.AssetImportTask(); task.filename=str(STAGE/relative)
        task.destination_path=DEST+'/Textures'; task.destination_name=name
        task.automated=True; task.save=True
        TOOLS.import_asset_tasks([task]); tex=load(path)
    assert tex,path
    tex.set_editor_property('srgb',kind=='diff')
    tex.set_editor_property('max_texture_size',2048)
    if kind=='normal':
        tex.set_editor_property('compression_settings',ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property('flip_green_channel',True)
    elif kind!='diff':tex.set_editor_property('compression_settings',ue.TextureCompressionSettings.TC_MASKS)
    save(tex); TEXTURES[key]=tex; RESULT['textures'].append(path)
    return tex


def classify(path):
    s=Path(path).stem.lower()
    if any(x in s for x in ('nor_gl','normalgl','_normal')) or s.endswith('_n'):return 'normal'
    if any(x in s for x in ('_diff','_color')) or s.endswith('_ch'):return 'diff'
    if '_rough' in s:return 'rough'
    if '_metal' in s:return 'metal'
    if '_ao' in s:return 'ao'
    return None


def vec(values):return 'float3('+','.join(str(v) for v in values)+')'


def sample(g,path,kind,uv):
    sampler=ue.MaterialSamplerType.SAMPLERTYPE_COLOR if kind=='diff' else ue.MaterialSamplerType.SAMPLERTYPE_NORMAL if kind=='normal' else ue.MaterialSamplerType.SAMPLERTYPE_MASKS
    node=g.node(ue.MaterialExpressionTextureSample,texture=texture(path,kind),sampler_type=sampler,
                sampler_source=ue.SamplerSourceMode.SSM_WRAP_WORLD_GROUP_SETTINGS)
    g.wire(uv,node,'UVs')
    return node


def layer(g,slot,parent,tile=1,tint=(1,1,1),sat=1,rot=False):
    uv=g.node(ue.MaterialExpressionTextureCoordinate)
    uv=g.custom(('return float2(1-UV.y,UV.x)/' if rot else 'return UV/')+str(tile)+';',F2,UV=uv)
    maps={classify(p):p for p in SOURCES[slot]['images'].values() if Path(p).parent.name==parent and classify(p)}
    assert 'diff' in maps,(slot,parent,maps)
    samples={k:sample(g,p,k,uv) for k,p in maps.items()}
    base=g.custom(f'return lerp(dot(C,float3(.2126,.7152,.0722)).xxx,C,{sat})*{vec(tint)};',F3,C=samples['diff'])
    rough=samples.get('rough') or g.scalar(.7)
    normal=samples.get('normal') or g.color((0,0,1))
    return base,rough,normal


SIMPLE={
 'Brick':('red_brick_03',.6,(.95,.85,.80),.85),
 'WoodOak':('dark_wooden_planks',1.3,(1.05,.98,.92),.8),
 'WoodWeathered':('weathered_planks',1.1,(.95,.90,.84),.55),
 'WoodTamanu':('brown_planks_03',1.6,(1.15,.8,.62),1),
 'Coconut':('palm_tree_bark',.9,(.9,.85,.8),.7),
 'Iron':('Metal046B',.6,(.55,.56,.6),.5),
 'IronRust':('rusty_metal_04',.45,(.95,.88,.8),.75),
 'Copper':('Metal048C',.35,(.585,.30,.195),1),
 'Jute':('hessian_230',.28,(1,.92,.78),.9),
 'Rope':('Rope001',.12,(.95,.85,.65),.9),
 'BarrelStaves':('wooden_barrel',1,(.95,.9,.85),.85),
 'CoralGravel':('coral_gravel',1.1,(.95,.92,.86),.5),
 'Shingles':('weathered_planks',.55,(.82,.82,.8),.25),
 'RoofTiles':('clay_roof_tiles_02',1.45,(.95,.80,.68),.78),
 'Flag':('Fabric019',.35,(.95,.92,.85),0),
}


def make_material(slot):
    name='M_'+slot.removeprefix('MAT-').removeprefix('MAT_')
    path=DEST+'/Materials/'+name
    mat=load(path) or TOOLS.create_asset(name,DEST+'/Materials',ue.Material,ue.MaterialFactoryNew())
    MEL.delete_all_material_expressions(mat)
    g=Graph(mat); v=g.node(ue.MaterialExpressionVertexColor); pos=g.node(ue.MaterialExpressionWorldPosition)
    kind=slot.removeprefix('MAT-Fort_')
    base=g.color(SOURCES[slot]['color'][:3]); rough=g.scalar(.75); normal=None; metal=g.scalar(0)
    def blend(a,b,f):return g.custom('return lerp(A,B,saturate(F));',F3,A=a,B=b,F=f)
    if kind in SIMPLE:
        base,rough,normal=layer(g,slot,*SIMPLE[kind],rot=kind=='Shingles')
    if kind in ('CoralRubble','CoralAshlar'):
        rubble=kind=='CoralRubble'
        base,rough,normal=layer(g,slot,'coral_fort_wall_02' if rubble else 'coral_fort_wall_01',.68 if rubble else .6,
                                (1,.97,.92) if rubble else (1,.95,.85),.85 if rubble else .6)
        for parent,tile,tint,sat,channel in ([('coral_fort_wall_03',.62,(.95,.95,.97),.7,'r'),('white_plaster_02',1.1,(.90,.86,.78),.6,'g')] if rubble else
                                           [('coral_ground_02',1.5,(.9975,.9765,.924),.45,'b')]):
            c,r,n=layer(g,slot,parent,tile,tint,sat)
            mask=g.custom('return V.'+channel+';',F1,V=v)
            if channel=='g':mask=g.custom('return M*smoothstep(.20,.5,.5+.28*sin(P.x*.13)*sin(P.z*.15)+.2*sin(P.y*.27));',F1,M=mask,P=pos)
            base,rough,normal=blend(base,c,mask),blend(rough,r,mask),blend(normal,n,mask)
    if kind in ('CoralRubble','CoralAshlar','Brick','RoofTiles','WoodWeathered','CoralGravel'):
        base=g.custom('float macro=.91+.07*sin(P.x*.0027)*sin(P.y*.0033)+.04*sin(P.z*.021+P.x*.006);'
                      'float foot=saturate(1-(P.z-115)/55)*.24;return lerp(C*macro,C*float3(.40,.43,.29),foot);',F3,C=base,P=pos)
    if kind=='IronRust':base=g.custom('return lerp(C,float3(.10,.085,.075),.55);',F3,C=base)
    if kind in ('Iron','IronRust','Copper'):
        metal=g.scalar({'Iron':.85,'IronRust':.35,'Copper':.82}[kind])
        rough=g.custom('return saturate(R.r+.15);',F1,R=rough)
    if kind=='Copper':
        patina=g.custom('return smoothstep(.45,.75,.5+.25*sin(P.x*.23)*sin(P.z*.27));',F1,P=pos)
        base=blend(base,g.color((.25,.42,.33)),patina)
        metal=g.custom('return .9*(1-P);',F1,P=patina)
    if kind=='Flag':
        mat.set_editor_property('two_sided',True)
        uv=g.node(ue.MaterialExpressionTextureCoordinate)
        tiled=g.custom('float2 t=UV*float2(5,4);t.x+=fmod(floor(t.y),2)*.5;return frac(t);',F2,UV=uv)
        motif=next(p for p in SOURCES[slot]['images'].values() if 'FleurDeLys' in p)
        tex=sample(g,motif,'diff',tiled)
        mask=g.node(ue.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True);g.wire(tex,mask,'','RGBA')
        base=blend(base,g.color(linear('CCAC52')),mask)
    if slot.startswith('MAT_Gold'):
        base,rough,normal=layer(g,slot,'loot_1k')
        base=g.custom('float3 c=float3(.807,.527,.195)*(.85+.3*dot(C,float3(.2126,.7152,.0722)));'
                      'c=lerp(c,float3(.043,.023,.009),V.r*.85);c=lerp(c,float3(.896,.665,.270),V.g);return lerp(c,float3(.076,.147,.091),V.b*.35);',F3,C=base,V=v)
        rough=g.custom('return saturate(.24+.3*(R.r-.3)+.35*V.r-.10*V.g);',F1,R=rough,V=v)
        metal=g.custom('return saturate(1-.55*V.r-.35*V.b);',F1,V=v)
        if slot=='MAT_Gold_CoinHeap':
            uv=g.node(ue.MaterialExpressionTextureCoordinate)
            coins=g.custom('float2 p=UV*1.86;p.x+=fmod(floor(p.y),2)*.5;float d=length(frac(p)-.5);return 1-smoothstep(.40,.49,d);',F1,UV=uv)
            base=g.custom('return C*lerp(.15,1,Coins);',F3,C=base,Coins=coins)
    if slot.startswith('MAT_Gem_'):
        base=g.color(linear({'Amethyst':'3A1450','Emerald':'08401E','Ruby':'5A0710','Sapphire':'0A1A55','Topaz':'8E4C06'}[slot.removeprefix('MAT_Gem_')]))
        f=g.node(ue.MaterialExpressionFresnel,exponent=3.0,base_reflect_fraction=.04)
        base=g.custom('return C*(.6+.4*F);',F3,C=base,F=f)
        rough=g.custom('return .04+.4*V.g;',F1,V=v);g.out(g.scalar(.9),MP.MP_SPECULAR)
    if slot.startswith('MAT_Pearl_'):
        base=g.color(linear('0E1113' if slot.endswith('Black') else 'CFC8BA'))
        f=g.node(ue.MaterialExpressionFresnel,exponent=3.0,base_reflect_fraction=.04)
        base=g.custom('return C+float3(.018,.033,.038)*F;',F3,C=base,F=f)
        rough=g.custom('return .22+.33*V.g;',F1,V=v);g.out(g.scalar(.8),MP.MP_SPECULAR)
    if kind=='LanternHorn':
        base=g.color((.85,.62,.35));g.out(g.color((6,3.3,1.32)),MP.MP_EMISSIVE_COLOR)
    if slot=='MAT-Site_Ground':
        uv=g.node(ue.MaterialExpressionTextureCoordinate)
        b1=g.node(ue.MaterialExpressionTextureCoordinate,coordinate_index=1)
        b2=g.node(ue.MaterialExpressionTextureCoordinate,coordinate_index=2)
        alpha=g.node(ue.MaterialExpressionComponentMask,r=True,g=False,b=False,a=False);g.wire(v,alpha,'','A')
        masks=[('sand_wet',3,None),('sand_dry',3.5,g.custom('return V.r;',F1,V=v)),('rubble',2.5,g.custom('return V.g;',F1,V=v)),
               ('feo',3,g.custom('return V.b;',F1,V=v)),('soil',3,alpha),('litter',2.5,g.custom('return UV.x;',F1,UV=b1)),('wrack',2,g.custom('return UV.x;',F1,UV=b2))]
        for layername,tile,mask in masks:
            mapped=g.custom(f'return UV/{tile};',F2,UV=uv)
            paths={Path(p).name:p for p in SOURCES[slot]['images'].values()}
            c=sample(g,paths[layername+'_ch.png'],'diff',mapped);n=sample(g,paths[layername+'_n.png'],'normal',mapped)
            if mask is None:base,normal=c,n
            else:
                height=g.node(ue.MaterialExpressionComponentMask,r=False,g=False,b=False,a=True);g.wire(c,height,'','RGBA')
                fac=g.custom('return saturate((M+(H-.5)*.5-.35)/.35)*saturate((M-.02)/.18);',F1,M=mask,H=height)
                base,normal=blend(base,c,fac),blend(normal,n,fac)
        wet=g.custom('return saturate(1-UV.y);',F1,UV=b1)
        base=g.custom('float depth=max(0,(-50-P.z)/100);float3 c=C*lerp(float3(1,1,1),float3(.32,.30,.26),W*.55);'
                      'c*=lerp(float3(1,1,1),lerp(float3(.55,.85,.80),float3(.10,.38,.45),saturate((depth-.5)/5.5)),saturate(depth/.5)*.85);'
                      'return c*(.78+.045*sin(P.x*.002)*sin(P.y*.002));',F3,C=base,W=wet,P=pos)
        rough=g.custom('return lerp(.9,.35,max(W,saturate((-50-P.z)/50)));',F1,W=wet,P=pos)
    if slot=='MAT-Site_Lagoon' or kind=='Puddle':
        base=g.custom('return lerp(float3(.065,.48,.43),float3(.006,.042,.09),saturate(V.r*2));',F3,V=v) if slot=='MAT-Site_Lagoon' else g.color((.035,.045,.035))
        rough=g.scalar(.08)
        uv=g.custom('return P.xy/500;',F2,P=pos)
        time=g.node(ue.MaterialExpressionTime)
        uv=g.custom('return UV+float2(T*.009,T*.006);',F2,UV=uv,T=time)
        normal=sample(g,next(iter(SOURCES[slot]['images'].values())),'normal',uv)
        if slot=='MAT-Site_Lagoon':base=g.custom('return lerp(C,float3(.8,.88,.86),V.g*.7);',F3,C=base,V=v)
        mat.set_editor_property('two_sided',True)
    g.out(base,MP.MP_BASE_COLOR);g.out(rough,MP.MP_ROUGHNESS);g.out(metal,MP.MP_METALLIC)
    if normal:g.out(normal,MP.MP_NORMAL)
    errors=MEL.recompile_material(mat)
    assert not errors,(slot,list(errors))
    save(mat)
    RESULT['materials'][slot]=path
    ue.log('T193_MATERIAL '+slot)
    return mat


def import_mesh(name,info,materials):
    path=DEST+'/Meshes/'+name
    mesh=load(path)
    editor=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    if not mesh or name in REIMPORT_NAMES:
        options=ue.FbxImportUI()
        for prop,value in {'import_mesh':True,'import_as_skeletal':False,'import_materials':False,'import_textures':False,'import_animations':False,
                            'mesh_type_to_import':ue.FBXImportType.FBXIT_STATIC_MESH}.items():options.set_editor_property(prop,value)
        data=options.static_mesh_import_data
        for prop,value in {'combine_meshes':True,'auto_generate_collision':False,'one_convex_hull_per_ucx':True,
                           'import_uniform_scale':1.,'convert_scene':True,'convert_scene_unit':True,
                           'vertex_color_import_option':ue.VertexColorImportOption.REPLACE,'generate_lightmap_u_vs':False}.items():data.set_editor_property(prop,value)
        task=ue.AssetImportTask();task.filename=str(STAGE/info['source']);task.destination_path=DEST+'/Meshes';task.destination_name=name
        task.automated=True;task.save=True;task.options=options;task.replace_existing=True;task.replace_existing_settings=True
        TOOLS.import_asset_tasks([task]);mesh=load(path)
    assert mesh,path
    slots=list(mesh.get_editor_property('static_materials'));assigned=[]
    for slot in slots:
        source_slot=str(slot.get_editor_property('material_slot_name'))
        assert source_slot in materials,(name,source_slot)
        slot.set_editor_property('material_interface',materials[source_slot]);assigned.append(source_slot)
    mesh.set_editor_property('static_materials',slots)
    assert set(info['faces_by_material']).issubset(assigned),(name,assigned,info['faces_by_material'])
    bounds=mesh.get_bounding_box();dims=sorted([bounds.max.x-bounds.min.x,bounds.max.y-bounds.min.y,bounds.max.z-bounds.min.z])
    # Imported bounds differ slightly on fine details (the gate wicket differs
    # by 1.24 cm in depth); retain a 3% scale check with 2 cm absolute slack.
    assert all(abs(a-b)<max(2.,b*.03) for a,b in zip(dims,sorted(info['size_cm']))),(name,dims,info['size_cm'])
    convex=editor.get_convex_collision_count(mesh)
    assert convex>=info['ucx_count'],(name,convex,info['ucx_count'])
    if name=='SM_Loot_Coin' and not editor.get_simple_collision_count(mesh):
        editor.add_simple_collisions(mesh,ue.ScriptCollisionShapeType.BOX)
    # These static interiors have modeled steps/floors that the delivery's UCX
    # omits (chapel floor) or replaces with a 49.5 cm vertical curb (governor).
    complex_only=(not info['ucx_count'] and name not in ('SM_Fort_Lagoon','SM_Loot_Coin')) or name in {
        'SM_Fort_GovernorHouse','SM_Fort_Chapel','SM_Fort_TreasureVault','SM_Fort_Prison',
        'SM_Fort_Barracks','SM_Fort_Kitchen','SM_Port_Warehouse','SM_Fort_SignalTower',
        'SM_Fort_GuardHouse','SM_Fort_PowderMagazine'}
    mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if complex_only else ue.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX)
    for socket_name,data in MODULES.get(name,{}).get('sockets',{}).items():
        socket=mesh.find_socket(socket_name)
        if not socket:
            socket=ue.new_object(ue.StaticMeshSocket,outer=mesh);socket.set_editor_property('socket_name',socket_name);mesh.add_socket(socket)
        p=data['pos'];r=data['rot']
        socket.set_editor_property('relative_location',ue.Vector(p[0]*100,-p[1]*100,p[2]*100))
        socket.set_editor_property('relative_rotation',ue.Rotator(roll=-r[0],pitch=r[1],yaw=-r[2]))
        socket.set_editor_property('relative_scale',ue.Vector(1,1,1))
    save(mesh)
    RESULT['meshes'][name]={'path':path,'size_cm':dims,'material_slots':assigned,'convex_count':convex,'complex_collision':complex_only,'sockets':len(MODULES.get(name,{}).get('sockets',{}))}
    ue.log('T193_MESH '+name)


def main():
    check_lock()
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor()
    manifest=json.loads((STAGE/'source-manifest.json').read_text(encoding='utf-8'))
    for relative,info in manifest['files'].items():
        assert hashlib.sha256((STAGE/relative).read_bytes()).hexdigest()==info['sha256'],relative
        source=Path(info['source']);stat=source.stat()
        assert (stat.st_size,stat.st_mtime_ns)==(info['bytes'],info['mtime_ns']),f'OneDrive revision changed: {source}'
    ready=OUT/'materials-ready.json'
    if ready.exists():
        paths=json.loads(ready.read_text(encoding='utf-8'))
        mats={slot:load(paths[slot]) for slot in SOURCES}
        for slot,mat in mats.items():
            assert mat,slot
            errors=MEL.recompile_material(mat);assert not errors,(slot,list(errors))
        RESULT['materials']=paths
        RESULT['textures']=[p for p in EAL.list_assets(DEST+'/Textures') if not EAL.does_directory_exist(p)]
    else:
        mats={slot:make_material(slot) for slot in SOURCES}
        ready.write_text(json.dumps(RESULT['materials'],indent=2),encoding='utf-8')
    for name,info in PREFLIGHT.items():import_mesh(name,info,mats)
    RESULT['finished_at']=dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/'import-result.json').write_text(json.dumps(RESULT,indent=2),encoding='utf-8')
    REIMPORT.unlink(missing_ok=True)
    ue.log('T193_IMPORT_SUCCESS')


if __name__=='__main__':
    world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag='Interchange.FeatureFlags.Import.FBX';previous=ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world,flag+' 0')
    try:main()
    finally:ue.SystemLibrary.execute_console_command(world,flag+' '+str(previous))
