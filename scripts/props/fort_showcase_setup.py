"""Place the authored fort in its own Test Flat area; preserve all other tasks."""
import datetime as dt
import json
import math
from pathlib import Path
import sys
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/props'))
from fort_showcase_import import STAGE,OUT,DEST,load,check_lock
from fort_showcase_loot import FORT_LOOT,configure
LEVEL='/Game/MonkeIsland/Maps/Test/L_Test_Flat'
TAG='FortFrench_T193'
ORIGIN=ue.Vector(-66000,0,115)


def xyz(v):return [round(v.x,4),round(v.y,4),round(v.z,4)]


def angles(r):return [round(r.pitch,4),round(r.yaw,4),round(r.roll,4)]


def fingerprint(a):
    meshes=[c.get_editor_property('static_mesh').get_path_name() if c.get_editor_property('static_mesh') else None for c in a.get_components_by_class(ue.StaticMeshComponent)]
    return {'path':a.get_path_name(),'location':xyz(a.get_actor_location()),'rotation':angles(a.get_actor_rotation()),'scale':xyz(a.get_actor_scale3d()),'meshes':meshes}


def main():
    check_lock()
    levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert not levels.is_in_play_in_editor()
    from fort_showcase_callback_cleanup import cleanup
    cleanup()
    world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if 'L_Test_Flat' not in world.get_path_name():assert levels.load_level(LEVEL)
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    before=actors.get_all_level_actors();existing={a.get_actor_label():a for a in before}
    preserved={a.get_path_name():fingerprint(a) for a in before if TAG not in [str(t) for t in a.tags]}
    (OUT/'preserved-before.json').write_text(json.dumps(preserved,indent=2),encoding='utf-8')
    assert 'Ground_Slab' in existing
    rows=[]
    # Authored UCX omits the guard floor and tower landings. Keep the modeled
    # surfaces for these static interiors, including the magazine threshold.
    for name in ('SM_Fort_SignalTower','SM_Fort_GuardHouse','SM_Fort_PowderMagazine'):
        asset=load(DEST+'/Meshes/'+name);assert asset,name
        asset.get_editor_property('body_setup').set_editor_property('collision_trace_flag',ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
        assert ue.EditorAssetLibrary.save_loaded_asset(asset)

    def spawn(cls,label,location,rotation=ue.Rotator(),folder='FortFrench'):
        a=existing.get(label)
        if a:assert TAG in [str(t) for t in a.tags],label
        else:
            a=actors.spawn_actor_from_class(cls,location,rotation);assert a,label
            a.set_actor_label(label);a.set_editor_property('tags',[TAG]);existing[label]=a
        a.set_folder_path('TestFlat/Zone_Generateurs/'+folder)
        return a

    def mesh(meshpath,label,location,rotation=ue.Rotator(),scale=(1,1,1),folder='FortFrench',collision=True):
        asset=load(meshpath);assert asset,meshpath
        loot_name=meshpath.rsplit('/',1)[-1]
        is_loot=loot_name in FORT_LOOT and folder!='FortFrench/Access'
        a=spawn(ue.PhysicsPropActor if is_loot else ue.StaticMeshActor,label,location,rotation,folder)
        c=a.get_component_by_class(ue.StaticMeshComponent);c.set_static_mesh(asset)
        c.set_editor_property('relative_location',location);c.set_editor_property('relative_rotation',rotation)
        c.set_editor_property('relative_scale3d',ue.Vector(*scale))
        c.set_collision_profile_name('BlockAll')
        if not collision:c.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        if is_loot:configure(a,asset,location+ue.Vector(0,0,1),rotation,scale,FORT_LOOT[loot_name][1])
        rows.append({'label':label,'mesh':meshpath,'location_cm':xyz(location+ue.Vector(0,0,1) if is_loot else location),'rotation':angles(rotation),'scale':scale,'physics_loot':is_loot})
        return a

    layout=json.loads((STAGE/'layout.json').read_text(encoding='utf-8'))
    content=ROOT/'monke_island/Content'
    known={p.stem:'/Game/'+p.relative_to(content).with_suffix('').as_posix() for p in (content/'MonkeIsland').rglob('SM_*.uasset') if '/Revisions/' not in p.as_posix()}
    for i,row in enumerate(layout):
        module=row['kind']=='module';name=row['mesh']
        path=DEST+'/Meshes/'+name if module else known[name]
        pos=ORIGIN+ue.Vector(*row['location_cm'])
        rotation=ue.Quat(*row['quaternion_xyzw']).rotator()
        scale=list(row['scale'])
        # The raised gate and lowered drawbridge are authored. Open the vault grille
        # in the showroom so its model can be inspected from the court.
        if name=='SM_Fort_VaultGrille':pos.z+=250
        if name=='SM_Fort_StuddedDoor' and row['name'].endswith('_00'):rotation=ue.Rotator()
        # Authored terrain has two narrow gaps at the dry-ditch bridge and the
        # central quay connection; extend the existing modules to overlap land.
        if name=='SM_Fort_BridgeFixed':scale[1]*=1.65
        if name=='SM_Port_Jetty_6m' and abs(row['location_cm'][1]-4500)<1:scale[1]*=1.4
        decorative=not module or name in ('SM_Fort_Flag','SM_Fort_Lantern','SM_Fort_Lantern_Hanging','SM_Fort_WallShackles','SM_Loot_Coin')
        a=mesh(path,f'Fort_T193_{i:03d}_{name.removeprefix("SM_")}',pos,rotation,scale,
               'FortFrench/'+row['folder'].removeprefix('COL-'),not decorative)
        if name in ('SM_Fort_Lantern','SM_Fort_Lantern_Hanging'):
            light=spawn(ue.PointLight,f'Fort_T193_Light_{i:03d}',pos+ue.Vector(0,0,12),folder='FortFrench/Lights')
            light.set_actor_location(pos+ue.Vector(0,0,12),False,True)
            c=light.get_component_by_class(ue.PointLightComponent)
            c.set_editor_property('intensity_units',ue.LightUnits.CANDELAS);c.set_editor_property('intensity',4.)
            c.set_editor_property('attenuation_radius',330.);c.set_editor_property('light_color',ue.Color(255,174,88,255));c.set_editor_property('cast_shadows',False)
    for name in ('TerrainNear','TerrainFar','CourtPaths','Lagoon'):
        mesh(DEST+'/Meshes/SM_Fort_'+name,'Fort_T193_'+name,ORIGIN,folder='FortFrench/Site',collision=name!='Lagoon')

    # User requested an enlargement of Ile_Generateurs. Extend its west bank by
    # 90 m, preserving the existing village/PCG transforms and original mask crop.
    island=existing['TestFlat_Ile_Generateurs']
    center,extent=island.get_actor_bounds(False)
    top=center.z+extent.z;west=center.x-extent.x
    extension=mesh('/Engine/BasicShapes/Cube','Fort_T193_Ile_Generateurs_Extension',
                   ue.Vector(west-4500,center.y,top-100),scale=(90,extent.y/50,2),folder='FortFrench/Site')
    ground_mat=island.static_mesh_component.get_material(0)
    assert ground_mat
    extension.static_mesh_component.set_material(0,ground_mat)
    # Short timber connection to the fort's dry eastern approach. The port and
    # lagoon remain beyond the new land instead of being buried under a slab.
    start_x=west-8800;end_x=ORIGIN.x+5000;y=3200;target_z=ORIGIN.z+20
    length=end_x-start_x;count=math.ceil(abs(length)/600);segment=abs(length)/count
    pitch=math.degrees(math.atan((target_z-top)/length))
    for i in range(count):
        fraction=(i+.5)/count
        mesh(DEST+'/Meshes/SM_Port_Pier_6m',f'Fort_T193_Access_{i:03d}',ue.Vector(start_x+length*fraction,y,top+(target_z-top)*fraction),
             ue.Rotator(roll=pitch,pitch=0,yaw=90),(1,segment/600+0.003,1),'FortFrench/Access')
    # Clear, player-facing directional sign at the platform edge.
    sign=spawn(ue.TextRenderActor,'Fort_T193_Sign',ue.Vector(start_x+200,y-190,top+170),ue.Rotator(roll=0,pitch=0,yaw=0),'FortFrench/Access')
    c=sign.get_component_by_class(ue.TextRenderComponent);c.set_text(ue.Text('FORT FRANÇAIS  →'));c.set_world_size(35)
    c.set_horizontal_alignment(ue.HorizTextAligment.EHTA_CENTER)
    changed=[{'label':a.get_actor_label(),'before':preserved[a.get_path_name()],'after':fingerprint(a)}
             for a in actors.get_all_level_actors() if a.get_path_name() in preserved and fingerprint(a)!=preserved[a.get_path_name()]]
    (OUT/'preservation-differences.json').write_text(json.dumps(changed,indent=2),encoding='utf-8')
    assert not changed,[(r['label'],[k for k in r['before'] if r['before'][k]!=r['after'][k]]) for r in changed]
    assert len(preserved)==sum(a.get_path_name() in preserved for a in actors.get_all_level_actors()),'Unrelated actors removed'
    assert levels.save_current_level()
    result={'task':'T-193','level':LEVEL,'tag':TAG,'origin_cm':xyz(ORIGIN),'module_count':sum(x['kind']=='module' for x in layout),
            'vegetation_count':sum(x['kind']=='vegetation' for x in layout),'actors':rows,'preserved_actors':preserved,
            'access_start_cm':[start_x,y,top],'access_end_cm':[end_x,y,target_z],
            'island_extension':{'west_before_cm':west,'west_after_cm':west-9000,'original_actor_preserved':True},
            'saved_at':dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT/'setup-result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    location=ORIGIN+ue.Vector(9500,10000,7500)
    rotation=ue.MathLibrary.find_look_at_rotation(location,ORIGIN+ue.Vector(0,1300,200))
    ue.EditorLevelLibrary.set_level_viewport_camera_info(location,rotation)
    ue.get_editor_subsystem(ue.EditorUtilitySubsystem).close_tab_by_id('MessageLog')
    ue.log('T193_SETUP_SUCCESS')


if __name__=='__main__':main()
