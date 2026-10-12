"""Verify saved fort transforms/materials and preservation after PIE; remove test camera."""
import datetime as dt
import json
from pathlib import Path
import sys
import unreal as ue
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/'scripts/props'))
from fort_showcase_setup import OUT,check_lock,xyz,fingerprint
def legacy_audit():
    check_lock()
    levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert not levels.is_in_play_in_editor()
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    setup=json.loads((OUT/'setup-result.json').read_text(encoding='utf-8'))
    all_actors=actors.get_all_level_actors();by_label={a.get_actor_label():a for a in all_actors}
    for row in setup['actors']:
        a=by_label[row['label']]
        assert all(abs(x-y)<.1 for x,y in zip(xyz(a.get_actor_location()),row['location_cm'])),row['label']
        mesh=a.get_component_by_class(ue.StaticMeshComponent).get_editor_property('static_mesh')
        assert mesh.get_path_name().split('.')[0]==row['mesh'],row['label']
        assert all(s.get_editor_property('material_interface') for s in mesh.get_editor_property('static_materials')),row['label']
    for a in all_actors:
        if a.get_path_name() in setup['preserved_actors']:
            assert fingerprint(a)==setup['preserved_actors'][a.get_path_name()],a.get_actor_label()
        if 'FortFrench_T193_ValidationCamera' in [str(t) for t in a.tags]:actors.destroy_actor(a)
    assert levels.save_current_level()
    (OUT/'audit-result.json').write_text(json.dumps({'task':'T-193','passed':True,'actors_checked':len(setup['actors']),
         'unrelated_actors_preserved':len(setup['preserved_actors']),'finished_at':dt.datetime.now(dt.timezone.utc).isoformat()},indent=2),encoding='utf-8')
    ue.log('T193_AUDIT_SUCCESS')


if __name__=='__main__':
    request=ROOT/'monke_island/Saved/T198/request.json'
    if request.exists():
        import runpy
        action=json.loads(request.read_text(encoding='utf-8-sig'))['action']
        assert action in ('apply','validate','audit','vegetation','vegetation_audit')
        request.unlink()
        runpy.run_path(str(ROOT/('scripts/props/fort_rework_'+action+'.py')),run_name='__main__')
    else:legacy_audit()
