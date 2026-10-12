"""Import and apply the T198 art revision in the already locked Test Flat editor."""
import datetime as dt
import hashlib
import importlib
import json
from pathlib import Path
import sys
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/props'))
from fort_showcase_import import DEST,load,save,check_lock,TOOLS
from fort_showcase_setup import ORIGIN,TAG,fingerprint,xyz
from fort_showcase_loot import configure,FORT_LOOT
OUT=ROOT/'monke_island/Saved/T198'
STAGE=ROOT/'art/export/Fort_T198'
REV=DEST+'/Revisions/T198/Meshes'
RULES=json.loads((ROOT/'scripts/props/fort_rework_rules.json').read_text())
REPORT={'task':'T-198','meshes':{},'updated_actors':[],'loot':[],'lights':[]}


def import_meshes(materials):
    build=json.loads((STAGE/'build-result.json').read_text())
    previous=json.loads((OUT/'import-result.json').read_text()) if (OUT/'import-result.json').exists() else {}
    meshes={}
    editor=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    for name,info in build['meshes'].items():
        path=REV+'/'+name;digest=hashlib.sha256(Path(info['file']).read_bytes()).hexdigest()
        mesh=load(path)
        if not mesh or previous.get(name,{}).get('sha256')!=digest:
            options=ue.FbxImportUI()
            for prop,value in {'import_mesh':True,'import_as_skeletal':False,'import_materials':False,'import_textures':False,
                              'import_animations':False,'mesh_type_to_import':ue.FBXImportType.FBXIT_STATIC_MESH}.items():options.set_editor_property(prop,value)
            for prop,value in {'combine_meshes':True,'auto_generate_collision':False,'one_convex_hull_per_ucx':True,
                              'import_uniform_scale':1.,'convert_scene':True,'convert_scene_unit':True,
                              'vertex_color_import_option':ue.VertexColorImportOption.REPLACE,'generate_lightmap_u_vs':False}.items():options.static_mesh_import_data.set_editor_property(prop,value)
            task=ue.AssetImportTask();task.filename=info['file'];task.destination_path=REV;task.destination_name=name
            task.automated=True;task.save=True;task.options=options;task.replace_existing=True;task.replace_existing_settings=True
            TOOLS.import_asset_tasks([task]);mesh=load(path)
        assert mesh,name
        slots=list(mesh.get_editor_property('static_materials'))
        for slot in slots:
            n=str(slot.get_editor_property('material_slot_name'))
            assert n in materials,(name,n)
            slot.set_editor_property('material_interface',materials[n])
        mesh.set_editor_property('static_materials',slots)
        flag=ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE if info['collision']=='complex' else ue.CollisionTraceFlag.CTF_USE_SIMPLE_AND_COMPLEX
        mesh.get_editor_property('body_setup').set_editor_property('collision_trace_flag',flag)
        count=editor.get_convex_collision_count(mesh)
        if info['collision']=='simple':
            # Chaos may reject the very thin, individual 2.45 cm gold hulls.
            # These piles are one pickup: a tight bounding box is a stable hull.
            if not editor.get_simple_collision_count(mesh):editor.add_simple_collisions(mesh,ue.ScriptCollisionShapeType.BOX)
            assert editor.get_simple_collision_count(mesh)>0,name
        b=mesh.get_bounding_box();dims=sorted([b.max.x-b.min.x,b.max.y-b.min.y,b.max.z-b.min.z])
        assert all(abs(a-b)<max(2,b*.03) for a,b in zip(dims,sorted(info['size_cm']))),(name,dims)
        save(mesh);meshes[name]=mesh
        REPORT['meshes'][name]={'path':path,'sha256':digest,'convex_count':count,'materials':len(slots),'size_cm':dims}
        (OUT/'import-result.json').write_text(json.dumps(REPORT['meshes'],indent=2),encoding='utf-8')
    (OUT/'import-result.json').write_text(json.dumps(REPORT['meshes'],indent=2),encoding='utf-8')
    return meshes


def price(name):
    if name=='SM_Loot_GoldBar' or name in RULES['stacks']:
        count=RULES['stacks'].get(name,1)
        return count*RULES['gold_bar']['mass_kg'],count*RULES['gold_bar']['value']
    return FORT_LOOT[name][1],RULES['items'][name]


def main():
    check_lock();levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert not levels.is_in_play_in_editor()
    world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    assert 'L_Test_Flat' in world.get_path_name(),world.get_path_name()
    from fort_showcase_callback_cleanup import cleanup
    cleanup()
    import fort_rework_materials as material_builder
    importlib.reload(material_builder)
    ready=OUT/'materials-result.json'
    if ready.exists():
        mats={s:load(p) for s,p in json.loads(ready.read_text()).items()}
        assert all(mats.values())
    else:mats=material_builder.main()
    meshes=import_meshes(mats)
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    before=actors.get_all_level_actors();existing={a.get_actor_label():a for a in before}
    preserved={a.get_path_name():fingerprint(a) for a in before if not any(str(t) in (TAG,'FortFrench_T198','FortFrench_T193_ValidationCamera') for t in a.tags)}
    counters={}
    def location(x,y,z):return ORIGIN+ue.Vector(x*100,-y*100,z*100)
    def vault(x,y,z):return location(-25+x,-21+y,-.99+z)
    # Gold on shelf tiers; gems at the counting desk; pearls on the north table.
    places={
      'SM_Loot_GoldStack_L':[(-2.12,.92,.21)],
      'SM_Loot_GoldStack_M':[(-2.12,0,.75),(-2.12,-.96,.21)],
      'SM_Loot_GoldBar':[(-2.12,.50,.75),(-2.12,.80,.75),(-2.12,1.1,.75)],
      'SM_Fort_Chest_Open':[(-1.77,-1.72,.02),(.0,-1.72,.02)],
      'SM_Fort_Chest_Closed':[(-.88,-1.72,.02),(.90,-1.72,.02),(1.80,-1.72,.02)],
      'SM_Loot_GemBowl':[(-1.32,-.15,.70),(-.86,-.12,.70)],
      'SM_Loot_PearlSack_Open':[(-1.50,1.8,.455),(-.36,1.8,.455)],
      'SM_Loot_PearlSack_Closed':[(-.94,1.8,.455)],
      'SM_Loot_CoinPile':[(2.14,-.15,.75),(2.14,-1.25,.21)]}
    gem_index=0;pallet_index=0
    for a in before:
        if TAG not in [str(t) for t in a.tags]:continue
        label=a.get_actor_label();c=a.get_component_by_class(ue.StaticMeshComponent)
        if c and c.get_editor_property('static_mesh'):
            asset=c.get_editor_property('static_mesh');name=asset.get_name()
            if name in meshes:asset=meshes[name];c.set_static_mesh(asset);REPORT['updated_actors'].append(label)
            if name=='SM_Fort_Pallet':
                # Old vault pallets now belong to the warehouse loading yard.
                pos=location(-15.8,-33.8-pallet_index*1.25,.02);pallet_index+=1
                c.set_editor_property('relative_location',pos)
                a.set_actor_location(pos,False,True)
                assert abs(c.get_world_location().x-pos.x)<.1
            if isinstance(a,ue.PhysicsPropActor) and name in FORT_LOOT:
                mass,value=price(name);pos=a.get_actor_location();rotation=a.get_actor_rotation();scale=a.get_actor_scale3d()
                if label.startswith('Fort_T193_Pickup_'):
                    pos.z=60-asset.get_bounding_box().min.z*scale.z+1.5
                elif name in places:
                    index=counters.get(name,0);counters[name]=index+1
                    assert index<len(places[name]),(name,index)
                    pos=vault(*places[name][index]);pos.z-=asset.get_bounding_box().min.z*scale.z
                    rotation=ue.Rotator()
                elif name.startswith('SM_Loot_Gem_'):
                    pos=vault(-.51+gem_index*.045,-.30,.692);gem_index+=1
                configure(a,asset,pos,rotation,(scale.x,scale.y,scale.z),mass,value)
                REPORT['loot'].append({'actor':label,'name':name,'mass_kg':mass,'value':value,'location_cm':xyz(pos)})
        if isinstance(a,ue.PointLight):
            c=a.get_component_by_class(ue.PointLightComponent)
            index=label.rsplit('_',1)[-1]
            lamp=next((o for key,o in existing.items() if key.startswith('Fort_T193_'+index+'_Fort_Lantern')),None)
            if lamp:a.set_actor_location(lamp.get_component_by_class(ue.StaticMeshComponent).get_socket_location('Light'),False,True)
            c.set_editor_property('light_color',ue.Color(r=255,g=192,b=121,a=255))
            c.set_editor_property('intensity',5.);c.set_editor_property('attenuation_radius',380.)
            REPORT['lights'].append(label)
    def spawn(cls,label,pos):
        a=existing.get(label)
        if not a:
            a=actors.spawn_actor_from_class(cls,pos);a.set_actor_label(label);a.tags=['FortFrench_T198'];existing[label]=a
        else:assert 'FortFrench_T198' in [str(t) for t in a.tags],label
        a.set_folder_path('TestFlat/Zone_Generateurs/FortFrench/Rework');return a
    for name in ('SM_Fort_MoatWater','SM_Fort_MoatSteps','SM_Fort_CourtyardDetails'):
        a=spawn(ue.StaticMeshActor,'Fort_T198_'+name,ORIGIN);c=a.static_mesh_component;c.set_static_mesh(meshes[name])
        c.set_editor_property('relative_location',ORIGIN)
        c.set_collision_profile_name('BlockAll')
        if name.endswith('Water'):c.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION);c.set_cast_shadow(False)
    # Wall-mounted lanterns give coherent warm illumination in occupied rooms.
    layout=json.loads((ROOT/'art/export/Fort_T193/layout.json').read_text())
    room_lights=[('GovernorHouse',(-4,2.96,1.9)),('GovernorHouse',(4,2.96,1.9)),
       ('GovernorHouse',(-4,2.96,4.45)),('GovernorHouse',(4,2.96,4.45)),
       ('Barracks',(-4,2.46,1.75)),('Barracks',(4,2.46,1.75)),('GuardHouse',(1.4,1.96,1.6)),
       ('Kitchen',(-1,1.71,1.65)),('Chapel',(0,3.46,1.9)),('Prison',(-2.2,2.86,1.8)),
       ('TreasureVault',(.30,2.06,1.6)),('TreasureVault',(1.75,-2.06,1.6)),
       ('Port_Warehouse',(-2,2.46,1.9)),('Port_Warehouse',(2,2.46,1.9))]
    lantern=load(DEST+'/Meshes/SM_Fort_Lantern')
    for i,(room,p) in enumerate(room_lights):
        name='SM_'+room if room.startswith('Port_') else 'SM_Fort_'+room
        row=next(r for r in layout if r['mesh']==name);rot=ue.Quat(*row['quaternion_xyzw']).rotator()
        pos=ORIGIN+ue.Vector(*row['location_cm'])+ue.MathLibrary.quat_rotate_vector(ue.Quat(*row['quaternion_xyzw']),ue.Vector(p[0]*100,-p[1]*100,p[2]*100))
        if p[1]>0:rot.yaw+=180
        a=spawn(ue.StaticMeshActor,f'Fort_T198_Lantern_{i:02d}',pos);c=a.static_mesh_component;c.set_static_mesh(lantern)
        c.set_editor_property('relative_location',pos);c.set_editor_property('relative_rotation',rot);c.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        a.set_actor_location(pos,False,True);a.set_actor_rotation(rot,True)
        light_pos=c.get_socket_location('Light')
        l=spawn(ue.PointLight,f'Fort_T198_RoomLight_{i:02d}',light_pos)
        l.set_actor_location(light_pos,False,True);lc=l.get_component_by_class(ue.PointLightComponent)
        lc.set_editor_property('light_color',ue.Color(r=255,g=218,b=175,a=255));lc.set_editor_property('intensity_units',ue.LightUnits.CANDELAS)
        lc.set_editor_property('intensity',6. if room=='TreasureVault' else 8.)
        lc.set_editor_property('attenuation_radius',420.);lc.set_editor_property('cast_shadows',True)
        REPORT['lights'].append(l.get_actor_label())
    entries=json.loads((ROOT/'monke_island/Saved/T193/loot-result.json').read_text())['items']
    for i,row in enumerate(entries):
        if row['name'] not in FORT_LOOT:continue
        name=row['name'];mass,value=price(name);label=row['label']
        if name in RULES['stacks']:label=str(RULES['stacks'][name])+' lingots'
        if name=='SM_Loot_GoldBar':label='Lingot d’or · 5,5 kg'
        text=existing[f'Fort_T193_Loot_Label_{i:02d}'].get_component_by_class(ue.TextRenderComponent)
        text.set_text(ue.Text(label+'\n'+f'{value:,}'.replace(',',' ')+' couronnes'))
        text.set_world_size(11)
        row.update(mass=mass,value=value,label=label)
        if name in meshes:row['mesh']=REV+'/'+name
        actor=existing[row['actor']];row['location_cm']=xyz(actor.get_actor_location())
    (OUT/'loot-result.json').write_text(json.dumps({'items':entries,'count':len(entries),'pricing':RULES},indent=2),encoding='utf-8')
    changed=[a.get_actor_label() for a in actors.get_all_level_actors() if a.get_path_name() in preserved and fingerprint(a)!=preserved[a.get_path_name()]]
    assert not changed,changed
    assert sum(a.get_path_name() in preserved for a in actors.get_all_level_actors())==len(preserved)
    REPORT['preserved_actors']=preserved;REPORT['unrelated_actors_preserved']=len(preserved)
    assert levels.save_current_level()
    REPORT['saved_at']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'apply-result.json').write_text(json.dumps(REPORT,indent=2),encoding='utf-8')
    ue.log('T198_APPLY_SUCCESS')


if __name__=='__main__':main()
