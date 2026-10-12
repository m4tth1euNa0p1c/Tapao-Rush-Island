"""Place the newly delivered treasures beside Test Flat's existing loot/pickups."""
import datetime as dt
import json
from pathlib import Path
import sys
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/props'))
from fort_showcase_import import DEST,OUT,STAGE,load,check_lock
TAG='FortFrench_T193'
FORT_LOOT={
 'SM_Loot_GoldBar':('Lingot',5.5),
 'SM_Loot_GoldStack_S':('12 lingots',66.),'SM_Loot_GoldStack_M':('30 lingots',165.),'SM_Loot_GoldStack_L':('60 lingots',330.),
 'SM_Loot_Gem_Amethyst':('Améthyste',.1),'SM_Loot_Gem_Emerald':('Émeraude',.1),'SM_Loot_Gem_Ruby':('Rubis',.1),
 'SM_Loot_Gem_Sapphire':('Saphir',.1),'SM_Loot_Gem_Topaz':('Topaze',.1),'SM_Loot_GemBowl':('Coupelle de gemmes',2.),
 'SM_Loot_Coin':('Pièce d’or',.1),'SM_Loot_CoinPile':('Tas de pièces',3.),
 'SM_Loot_PearlSack_Open':('Sac de perles ouvert',6.),'SM_Loot_PearlSack_Closed':('Sac de perles fermé',6.),
 'SM_Fort_Chest_Open':('Coffre ouvert',35.),'SM_Fort_Chest_Closed':('Coffre fermé',35.),
}


def configure(a,asset,location,rotation,scale,mass,value=100):
    c=a.get_component_by_class(ue.StaticMeshComponent)
    c.set_simulate_physics(False);c.set_static_mesh(asset)
    c.set_mobility(ue.ComponentMobility.MOVABLE)
    c.set_editor_property('relative_location',location);c.set_editor_property('relative_rotation',rotation)
    c.set_editor_property('relative_scale3d',ue.Vector(*scale))
    c.set_collision_profile_name('PhysicsActor');c.set_collision_enabled(ue.CollisionEnabled.QUERY_AND_PHYSICS)
    a.set_editor_property('mass_kg',mass);c.set_mass_override_in_kg('None',mass,True)
    c.set_linear_damping(.35);c.set_angular_damping(.6);c.set_use_ccd(True)
    a.get_editor_property('valuable').set_editor_property('base_value',value)
    c.set_simulate_physics(True)
    return c


def main():
    check_lock();levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert not levels.is_in_play_in_editor()
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem);existing={a.get_actor_label():a for a in actors.get_all_level_actors()}
    assert 'Loot_SmallChest' in existing,'Expected existing Loot / Pickup area'
    cube=load('/Engine/BasicShapes/Cube');wood=load(DEST+'/Materials/M_Fort_WoodWeathered')
    entries=[{'mesh':DEST+'/Meshes/'+n,'name':n,'label':label,'mass':mass,'value':100,
              'scale':3 if n=='SM_Loot_Coin' or n.startswith('SM_Loot_Gem_') else 1} for n,(label,mass) in FORT_LOOT.items()]
    artifacts=json.loads((ROOT/'art/export/OneDriveShowcase/20261006/artefacts-polynesiens/artifacts-report.json').read_text(encoding='utf-8'))['objects']
    names={'HeartOfTheReef':'Cœur du récif','EclipseStone':'Pierre de l’éclipse','FishGodFragment':'Fragment du dieu poisson',
           'MoonPearl':'Perle de lune','RedPearlOfRaraka':'Perle rouge de Raraka','CrownOfTheLastAriki':'Couronne du dernier Ariki'}
    for name,info in artifacts.items():
        entries.append({'mesh':'/Game/MonkeIsland/Items/PolynesianArtifacts/Meshes/SM_'+name,'name':name,'label':names[name],
                        'mass':max(.1,info['mass_kg']),'value':info['value'],'scale':1})
    result=[]
    for i,row in enumerate(entries):
        x=-1850+(i%6)*250;y=3450+(i//6)*250
        asset=load(row['mesh']);assert asset,row['mesh']
        pedestal_label=f'Fort_T193_Loot_Plinth_{i:02d}'
        plinth=existing.get(pedestal_label)
        if not plinth:
            plinth=actors.spawn_actor_from_class(ue.StaticMeshActor,ue.Vector(x,y,30));plinth.set_actor_label(pedestal_label);plinth.tags=[TAG]
        else:assert TAG in [str(t) for t in plinth.tags]
        c=plinth.static_mesh_component;c.set_static_mesh(cube)
        c.set_editor_property('relative_location',ue.Vector(x,y,30));c.set_editor_property('relative_scale3d',ue.Vector(1.2,1.2,.6))
        c.set_material(0,wood);c.set_collision_profile_name('BlockAll');plinth.set_folder_path('Physics/Loot_Pickup/Nouveautes')
        label='Fort_T193_Pickup_'+row['name']
        a=existing.get(label)
        if not a:
            a=actors.spawn_actor_from_class(ue.PhysicsPropActor,ue.Vector(x,y,65));a.set_actor_label(label);a.tags=[TAG]
        else:assert TAG in [str(t) for t in a.tags]
        z=60-asset.get_bounding_box().min.z*row['scale']+1.5
        configure(a,asset,ue.Vector(x,y,z),ue.Rotator(),(row['scale'],)*3,row['mass'],row['value'])
        a.set_folder_path('Physics/Loot_Pickup/Nouveautes')
        text_label=f'Fort_T193_Loot_Label_{i:02d}'
        sign=existing.get(text_label)
        if not sign:
            sign=actors.spawn_actor_from_class(ue.TextRenderActor,ue.Vector(x,y-75,78),ue.Rotator(roll=0,pitch=0,yaw=-90));sign.set_actor_label(text_label);sign.tags=[TAG]
        text=sign.get_component_by_class(ue.TextRenderComponent);text.set_text(ue.Text(row['label']))
        text.set_world_size(12);text.set_horizontal_alignment(ue.HorizTextAligment.EHTA_CENTER);sign.set_folder_path('Physics/Loot_Pickup/Nouveautes')
        result.append({**row,'actor':label,'location_cm':[x,y,z],'display_scale':row['scale']})
    assert levels.save_current_level()
    (OUT/'loot-result.json').write_text(json.dumps({'task':'T-193','items':result,'count':len(result),
        'fort_value_note':'New fort props retain native default test value 100; no economy balancing implied',
        'finished_at':dt.datetime.now(dt.timezone.utc).isoformat()},indent=2),encoding='utf-8')
    ue.log('T193_LOOT_SUCCESS')


if __name__=='__main__':main()
