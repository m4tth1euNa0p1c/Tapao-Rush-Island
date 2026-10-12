"""Audit persisted art, real texture references, collisions and loot economy."""
import datetime as dt
import json
from pathlib import Path
import unreal as ue
from fort_showcase_import import check_lock,load,MEL
from fort_showcase_setup import fingerprint
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'monke_island/Saved/T198'
check_lock();levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
assert not levels.is_in_play_in_editor()
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
all_actors=actors.get_all_level_actors();by_label={a.get_actor_label():a for a in all_actors}
applied=json.loads((OUT/'apply-result.json').read_text())
materials=json.loads((OUT/'materials-result.json').read_text())
result={'task':'T-198','materials':{},'meshes':{},'loot':[]}
for slot,path in materials.items():
    mat=load(path);assert mat,path
    errors=MEL.recompile_material(mat);assert not errors,(path,list(errors))
    textures=[t.get_path_name() for t in MEL.get_used_textures(mat)]
    if any(s in slot for s in ('Wood','Coral','Brick','Roof','Cloth','Linen','Gold','Jute','Leather','Parchment')):assert textures,slot
    result['materials'][slot]={'path':path,'textures':textures,'compile_errors':[]}
editor=ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
for name,info in applied['meshes'].items():
    mesh=load(info['path']);assert mesh,name
    slots=list(mesh.get_editor_property('static_materials'))
    assert all(s.get_editor_property('material_interface') for s in slots),name
    collisions=editor.get_simple_collision_count(mesh)
    if name.startswith('SM_Loot_Gold'):assert collisions>0,name
    result['meshes'][name]={'slots':len(slots),'simple_collision_count':collisions,
                          'collision_mode':str(mesh.get_editor_property('body_setup').get_editor_property('collision_trace_flag'))}
for row in applied['loot']:
    a=by_label[row['actor']];value=a.get_editor_property('valuable').get_editor_property('base_value')
    assert value==row['value'],row['actor']
    assert abs(a.get_editor_property('mass_kg')-row['mass_kg'])<.01,row['actor']
    result['loot'].append({'actor':row['actor'],'value':value,'mass_kg':row['mass_kg']})
for a in all_actors:
    if a.get_path_name() in applied['preserved_actors']:
        assert fingerprint(a)==applied['preserved_actors'][a.get_path_name()],a.get_actor_label()
    if 'FortFrench_T193_ValidationCamera' in [str(t) for t in a.tags]:actors.destroy_actor(a)
water=by_label['Fort_T198_SM_Fort_MoatWater'].static_mesh_component
assert water.get_collision_enabled()==ue.CollisionEnabled.NO_COLLISION
result['unrelated_actors_preserved']=len(applied['preserved_actors'])
assert levels.save_current_level()
result.update(passed=True,finished_at=dt.datetime.now(dt.timezone.utc).isoformat())
(OUT/'audit-result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
ue.log('T198_AUDIT_SUCCESS')
