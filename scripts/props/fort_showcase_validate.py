"""PIE capsule traversal through T193 port/gate/interiors/ramparts with screenshots."""
import datetime as dt
import json
import math
import os
from pathlib import Path
import time
import traceback
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
TASK=globals().get('VALIDATION_TASK','T-193')
OUT=ROOT/'monke_island/Saved'/TASK.replace('-','')
SETUP=json.loads((ROOT/'monke_island/Saved/T193/setup-result.json').read_text(encoding='utf-8'))
ORIGIN=ue.Vector(*SETUP['origin_cm'])
LAYOUT=json.loads((ROOT/'art/export/Fort_T193/layout.json').read_text(encoding='utf-8'))
STATE={'phase':'waiting','routes':[],'index':0,'screenshots':[],'loot':[]}
HANDLE=None
CAMERA_TAG='FortFrench_T193_ValidationCamera'
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
camera=next((a for a in actors.get_all_level_actors() if CAMERA_TAG in [str(t) for t in a.tags]),None)
if camera is None:
    camera=actors.spawn_actor_from_class(ue.CameraActor,ORIGIN)
    camera.set_actor_label('Fort_T193_ValidationCamera');camera.set_editor_property('tags',[CAMERA_TAG])
camera.get_component_by_class(ue.CameraComponent).set_field_of_view(75)


def xyz(v):return [round(v.x,3),round(v.y,3),round(v.z,3)]


def progress():
    (OUT/'traversal-progress.json').write_text(json.dumps({'phase':STATE['phase'],'name':STATE.get('name'),
        'completed':len(STATE['routes']),'total':len(STATE.get('plan',[])),
        'routes':[{'name':r['name'],'passed':r['passed'],'end':r['end']} for r in STATE['routes']],
        'loot':STATE['loot']},indent=2),encoding='utf-8')


def point(x,y,z=0):return ORIGIN+ue.Vector(x*100,-y*100,z*100)


def module_point(name,x,y,z=0):
    row=next(r for r in LAYOUT if r['mesh']==name)
    q=row['quaternion_xyzw'];angle=2*math.atan2(q[2],q[3]);c,s=math.cos(angle),math.sin(angle)
    return ORIGIN+ue.Vector(*row['location_cm'])+ue.Vector(100*(x*c+y*s),100*(x*s-y*c),100*z)


def finish(passed,error=None):
    result={'task':TASK,'passed':passed,'error':error,'routes':STATE['routes'],'screenshots':STATE['screenshots'],
            'loot':STATE['loot'],'loot_physics':STATE.get('loot_physics',[]),
            'fort_loot_physics':STATE.get('fort_loot_physics',[]),
            'capsule_half_height_cm':STATE.get('half'),'capsule_radius_cm':STATE.get('radius'),
            'finished_at':dt.datetime.now(dt.timezone.utc).isoformat(),'scope':'Single-player PIE CharacterMovement traversal; multiplayer and fort gameplay not implemented by this import'}
    (OUT/'traversal-result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    STATE['phase']='finished';ue.unregister_slate_post_tick_callback(HANDLE)
    for key,method,args in [('grab','request_release',()),('move','stop_movement_immediately',()),
                            ('controller','set_ignore_move_input',(False,)),('controller','set_view_target_with_blend',(STATE.get('pawn'),0))]:
        try:
            if STATE.get(key):getattr(STATE[key],method)(*args)
        except Exception:pass  # A user may have ended PIE or destroyed the pawn.
    ue.log('T193_TRAVERSAL_'+('SUCCESS' if passed else 'FAILED'))


def start_route(now):
    if STATE['index']==len(STATE['plan']):
        STATE.update(phase='loot_start',loot_index=0,phase_start=now)
        return
    name,start,target,timeout=STATE['plan'][STATE['index']]
    STATE['move'].stop_movement_immediately()
    STATE['pawn'].set_actor_location(start+ue.Vector(0,0,STATE['half']+6),False,True)
    STATE['move'].set_movement_mode(ue.MovementMode.MOVE_FALLING)
    STATE.update(phase='settle',phase_start=now,name=name,start=xyz(start),target=target,timeout=timeout,samples=[])
    progress()


def tick(delta):
    try:
        lock=json.loads((ROOT/'.claude/factory/editor.lock.json').read_text(encoding='utf-8'))
        # handed over to Claude (T198-T208-claude.txt): codex or the session named in MONKE_LOCK_OWNER
        if lock['owner'] not in {'codex',os.environ.get('MONKE_LOCK_OWNER') or 'codex'} or dt.datetime.fromisoformat(lock['expires'])<=dt.datetime.now().astimezone():return
        world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
        if not world or 'UEDPIE' not in world.get_path_name():
            if STATE['phase']!='waiting':finish(False,'PIE ended before validation completed')
            return
        now=ue.GameplayStatics.get_time_seconds(world)
        if STATE['phase']=='waiting':
            pawn=ue.GameplayStatics.get_player_character(world,0)
            if not pawn or now<2:return
            if TASK=='T-198' and (ROOT/'monke_island/Saved/T208/apply-result.json').exists():
                from fort_rework_vegetation_audit import inspect
                inspect(ue.GameplayStatics.get_all_actors_of_class(world,ue.Actor),runtime=True)
            capsule=pawn.get_component_by_class(ue.CapsuleComponent)
            controller=ue.GameplayStatics.get_player_controller(world,0);controller.set_ignore_move_input(True)
            plan=[('island_extension_to_fort',ue.Vector(*SETUP['access_start_cm'])+ue.Vector(50,0,0),ue.Vector(*SETUP['access_end_cm']),30),
                  ('jetty_to_shore',point(0,-70,-.33),point(0,-39,-.08),35),
                  ('drawbridge_and_gate',point(0,-30),point(0,-14,.02),25),
                  ('west_ramp',point(-19,-4.5),point(-19,12,2.97),30),
                  ('south_stairs',point(-8.7,-15.4),point(-15.5,-15.4,2.97),25),
                  ('governor_entrance',point(0,4.5),point(0,8.2,.495),20),
                  ('governor_stairs_approach',point(0,8.6,.495),point(.85,8.7,.825),20),
                  ('governor_upper_stairs',point(.85,8.7,.825),point(.85,12.8,3.135),20),
                  ('governor_balcony',point(0,8.1,3.135),point(0,6.5,3.135),20),
                  ('chapel_entrance',point(14,5.7),point(14,8.2,.33),20),
                  ('prison_entrance',point(9.5,-6.7),point(12.2,-6.7,.02),20),
                  ('vault_court_entrance',point(-19,-14.8),point(-19,-17.2),20),
                  ('vault_descent',point(-19,-17.2),point(-23,-17.2,-.99),20),
                  ('vault_inner_door',point(-23.3,-17.2,-.99),point(-23.3,-19.1,-.99),20),
                  ('vault_room',point(-23.3,-19.1,-.99),point(-23.3,-20.5,-.99),20)]
            for name,mesh,start,target in [
                ('guardhouse','SM_Fort_GuardHouse',(0,-3.5,0),(0,-1.3,.165)),
                ('barracks_west','SM_Fort_Barracks',(-4,-3.8,0),(-4,-1.5,.165)),
                ('barracks_east','SM_Fort_Barracks',(4,-3.8,0),(4,-1.5,.165)),
                ('kitchen','SM_Fort_Kitchen',(0,-3.1,0),(0,-1.55,.165)),
                ('kitchen_aisle','SM_Fort_Kitchen',(0,-1.55,.165),(1.05,-1.4,.165)),
                ('powder_magazine','SM_Fort_PowderMagazine',(0,-4.2,0),(0,-1,.03)),
                ('warehouse','SM_Port_Warehouse',(0,-4.0,0),(0,-1,.185)),
                ('tower_entrance','SM_Fort_SignalTower',(0,-3.3,0),(0,-1,.02)),
                ('tower_stair_1','SM_Fort_SignalTower',(-1.2,-1.3,0),(-1.2,1.2,1.32)),
                ('tower_stair_2','SM_Fort_SignalTower',(-1.2,1.2,1.32),(1.34,1.2,2.64)),
                ('tower_level_1','SM_Fort_SignalTower',(1.34,1.2,2.64),(1.2,-.5,2.64)),  # T-198 2026-10-11: target on the landing floor (it was on the 3rd step of the next stair)
                ('tower_stair_3','SM_Fort_SignalTower',(1.3,-1.2,2.64),(-1.2,-1.2,3.96)),
                ('tower_stair_4','SM_Fort_SignalTower',(-1.2,-1.2,3.96),(-1.2,1.34,5.28)),
                ('tower_terrace','SM_Fort_SignalTower',(-1.2,1.34,5.28),(0,1.34,5.28)),
            ]:plan.append((name,module_point(mesh,*start),module_point(mesh,*target),20))
            if TASK=='T-198':
                plan=[('vault_inner_door',point(-23,-17.2,-.99),point(-23,-19.55,-.99),20) if n=='vault_inner_door' else
                      ('vault_room',point(-23,-19.55,-.99),point(-23.7,-20.5,-.99),20) if n=='vault_room' else
                      (n,s,t,seconds) for n,s,t,seconds in plan]
                plan=[(n,s,module_point('SM_Fort_Kitchen',0,-.35,.165) if n=='kitchen_aisle' else
                       point(-23.7,-20.5,-.99) if n=='vault_room' else t,seconds) for n,s,t,seconds in plan]
                for name,mesh,start,target in [
                    ('governor_dining','SM_Fort_GovernorHouse',(0,0,.495),(-2.7,0,.495)),
                    ('governor_office','SM_Fort_GovernorHouse',(0,2.30,.495),(3.0,2.30,.495)),
                    ('governor_bedroom','SM_Fort_GovernorHouse',(-.35,0,3.135),(-3,0,3.135)),
                    ('chapel_aisle','SM_Fort_Chapel',(0,-2.8,.33),(0,1.4,.33)),
                    ('barracks_aisle','SM_Fort_Barracks',(-4,-1.4,.165),(-4,-.1,.165)),
                ]:plan.append((name,module_point(mesh,*start),module_point(mesh,*target),15))
                requested=globals().get('ROUTE_NAMES')
                if requested:plan=[row for row in plan if row[0] in requested]
            cameras=[('overview',point(-82,-110,82),point(0,-12,1)),
                     ('gate',point(-7,-32,3),point(0,-19,1.8)),
                     ('courtyard',point(-9,-12,6),point(1,8,2)),
                     ('port',point(15,-80,8),point(0,-38,1)),
                     ('prison',point(11.8,-6.7,1.1),point(14,-7.7,.8)),
                     ('loot',ue.Vector(-1200,2900,650),ue.Vector(-1200,3850,70))]
            if TASK=='T-198':
                cameras=[('overview',point(-75,-88,57),point(0,-5,1)),
                         ('moat',point(-8,-31,2.5),point(6,-23,-.4)),
                         ('courtyard',point(-8,-8,2),point(0,10,2)),
                         ('bedroom',module_point('SM_Fort_GovernorHouse',-2.1,-1.8,4.3),module_point('SM_Fort_GovernorHouse',-4.6,1.15,3.8)),
                         ('barracks',module_point('SM_Fort_Barracks',-2.6,-1.7,1.35),module_point('SM_Fort_Barracks',-5,1.5,1.15)),
                         ('vault',point(-23.8,-19.9,.20),point(-26.4,-21.7,-.25)),
                         ('loot',ue.Vector(-1500,3250,155),ue.Vector(-1500,3500,75))]
            cam=next(a for a in ue.GameplayStatics.get_all_actors_of_class(world,ue.CameraActor) if CAMERA_TAG in [str(t) for t in a.tags])
            STATE.update(pawn=pawn,controller=controller,move=pawn.get_component_by_class(ue.CharacterMovementComponent),
                         half=capsule.get_scaled_capsule_half_height(),radius=capsule.get_scaled_capsule_radius(),plan=plan,cameras=cameras,camera=cam,
                         loot_plan=[] if globals().get('SKIP_LOOT') else json.loads((OUT/'loot-result.json').read_text(encoding='utf-8'))['items'],
                         loot_actors={a.get_actor_label():a for a in ue.GameplayStatics.get_all_actors_of_class(world,ue.PhysicsPropActor)},
                         grab=pawn.get_grab_component())
            start_route(now)
        elif STATE['phase'].startswith('loot_'):
            if STATE['phase']=='loot_start':
                if STATE['loot_index']==len(STATE['loot_plan']):
                    STATE.update(phase='photos',photo_index=0,phase_start=now,shot_pending=False);return
                row=STATE['loot_plan'][STATE['loot_index']];actor=STATE['loot_actors'][row['actor']]
                comp=actor.get_mesh();position=comp.get_world_location()
                if not STATE.get('loot_physics'):
                    checks=[]
                    for item in STATE['loot_plan']:
                        a=STATE['loot_actors'][item['actor']];c=a.get_mesh();p=c.get_world_location();v=c.get_physics_linear_velocity()
                        checks.append({'name':item['name'],'simulating':c.is_simulating_physics(),'mass_kg':c.get_mass(),
                                       'expected_mass_kg':item['mass'],'value':a.get_editor_property('valuable').get_value(),'expected_value':item['value'],
                                       'position_cm':xyz(p),'speed_cm_s':math.sqrt(v.x*v.x+v.y*v.y+v.z*v.z),
                                       'on_display':abs(p.x-item['location_cm'][0])<55 and abs(p.y-item['location_cm'][1])<55 and p.z>50})
                    STATE['loot_physics']=checks
                    if TASK=='T-198':
                        fort_checks=[]
                        for item in json.loads((OUT/'apply-result.json').read_text())['loot']:
                            if item['actor'].startswith('Fort_T193_Pickup_'):continue
                            a=STATE['loot_actors'][item['actor']];p=a.get_mesh().get_world_location();expected=item['location_cm']
                            fort_checks.append({'actor':item['actor'],'position_cm':xyz(p),'expected_cm':expected,
                                                'on_support':abs(p.x-expected[0])<25 and abs(p.y-expected[1])<25 and abs(p.z-expected[2])<18})
                        STATE['fort_loot_physics']=fort_checks
                STATE['pawn'].set_actor_location(ue.Vector(position.x,position.y-145,STATE['half']+5),False,True)
                STATE['move'].stop_movement_immediately();STATE['move'].set_movement_mode(ue.MovementMode.MOVE_FALLING)
                STATE.update(phase='loot_aim',phase_start=now,loot_row=row,loot_comp=comp,loot_attempt=0)
            elif STATE['phase']=='loot_aim' and now-STATE['phase_start']>.6:
                comp=STATE['loot_comp'];eyes=STATE['pawn'].get_first_person_camera().get_world_location()
                target,extent=comp.get_owner().get_actor_bounds(True)
                if STATE['loot_row']['name']=='CrownOfTheLastAriki' or STATE['loot_attempt']:
                    target.z-=extent.z*.65  # Aim at the crown's solid band, not its open center.
                STATE['controller'].set_control_rotation(ue.MathLibrary.find_look_at_rotation(eyes,target))
                STATE['grab'].request_grab();STATE.update(phase='loot_check',phase_start=now)
            elif STATE['phase']=='loot_check' and now-STATE['phase_start']>.4:
                grabbed=STATE['grab'].get_held_component()==STATE['loot_comp']
                if not grabbed and STATE['loot_attempt']==0:
                    STATE['grab'].request_release();STATE['loot_attempt']=1
                    p=STATE['loot_comp'].get_world_location()
                    STATE['pawn'].set_actor_location(ue.Vector(p.x+65,p.y-130,STATE['half']+5),False,True)
                    STATE['move'].stop_movement_immediately();STATE.update(phase='loot_aim',phase_start=now);return
                STATE['loot'].append({'name':STATE['loot_row']['name'],'grabbed':grabbed,'aim_attempts':STATE['loot_attempt']+1})
                STATE['grab'].request_release();STATE['loot_index']+=1;STATE['phase']='loot_start'
                progress()
        elif STATE['phase']=='photos':
            if STATE['photo_index']>=len(STATE['cameras']):
                if now-STATE['phase_start']<4:return
                finish(all(x['passed'] for x in STATE['routes']) and all(x['grabbed'] for x in STATE['loot'])
                       and all(x['simulating'] and x['on_display'] and abs(x['mass_kg']-x['expected_mass_kg'])<.05 and x['value']==x['expected_value'] for x in STATE.get('loot_physics',[]))
                       and all(x['on_support'] for x in STATE.get('fort_loot_physics',[])));return
            name,location,target=STATE['cameras'][STATE['photo_index']]
            if not STATE['shot_pending']:
                STATE['pawn'].set_actor_location(point(0,-35)+ue.Vector(0,0,STATE['half']+5),False,True)
                STATE['camera'].set_actor_location(location,False,True)
                STATE['camera'].set_actor_rotation(ue.MathLibrary.find_look_at_rotation(location,target),True)
                STATE['controller'].set_view_target_with_blend(STATE['camera'],0)
                STATE.update(shot_pending=True,phase_start=now)
            elif now-STATE['phase_start']>3:
                path=OUT/(name+'.png')
                ue.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+str(path)+'"')
                STATE['screenshots'].append(str(path));STATE['photo_index']+=1;STATE['shot_pending']=False
        else:
            if STATE['phase']=='settle':
                if now-STATE['phase_start']<.6:return
                STATE.update(phase='walking',phase_start=now)
            pos=STATE['pawn'].get_actor_location();target=STATE['target']
            dx,dy=target.x-pos.x,target.y-pos.y;distance=math.hypot(dx,dy)
            STATE['samples'].append(xyz(pos))
            passed=distance<23 and abs(pos.z-target.z-STATE['half'])<32
            failed=now-STATE['phase_start']>STATE['timeout'] or pos.z<ORIGIN.z-1000
            if passed or failed:
                STATE['move'].stop_movement_immediately()
                STATE['routes'].append({'name':STATE['name'],'passed':passed,'start':STATE['start'],'end':xyz(pos),'target':xyz(target),'positions':STATE['samples']})
                STATE['index']+=1;start_route(now)
            elif distance>.01:
                # Slow the actual movement near the destination; full input can
                # overshoot repeatedly when the shared editor renders slowly.
                STATE['pawn'].add_movement_input(ue.Vector(dx/distance,dy/distance,0),min(1.,max(.08,distance/200)),True)
    except Exception:finish(False,traceback.format_exc())


HANDLE=ue.register_slate_post_tick_callback(tick)
(OUT/'validator-ready.json').write_text(json.dumps({'started_at':dt.datetime.now(dt.timezone.utc).isoformat()}),encoding='utf-8')
