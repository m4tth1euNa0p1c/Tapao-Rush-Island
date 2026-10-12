"""T208 editor half: remove only requested plants; preserve architecture/artifacts."""
import datetime as dt
import json
from pathlib import Path
import unreal as ue
from fort_showcase_import import check_lock,load,save,EAL
from fort_showcase_setup import fingerprint
ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'monke_island/Saved/T208'
PREP=json.loads((OUT/'prepare-result.json').read_text())
DEST='/Game/MonkeIsland/Environment/FortFrench/Revisions/T208/Data'


def main():
    check_lock();levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem);assert not levels.is_in_play_in_editor()
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem);before=actors.get_all_level_actors()
    by_label={a.get_actor_label():a for a in before};targets=[]
    for a in before:
        label=a.get_actor_label();c=a.get_component_by_class(ue.StaticMeshComponent)
        mesh=c.get_editor_property('static_mesh') if c else None
        if label in PREP['replace_fort_labels'] or (label.startswith(('Showcase_Temple_','Showcase_Marae_')) and mesh and '/Environment/Vegetation/' in mesh.get_path_name()):targets.append(a)
    previous=json.loads((OUT/'scene-result.json').read_text()) if (OUT/'scene-result.json').exists() else {}
    removed={r['label']:r for r in previous.get('removed_actors',[])}
    removed.update({a.get_actor_label():{'label':a.get_actor_label(),**fingerprint(a)} for a in targets})
    result={'task':'T-208','removed_actors':list(removed.values()),'sites':[]}
    preserved={a.get_path_name():fingerprint(a) for a in before if a not in targets}
    for label in ('TestFlat_Marae','TestFlat_Temple'):
        a=by_label[label];source=a.get_editor_property('rules');assert source,label
        path=DEST+'/DA_'+label+'_NoPlants'
        rules=load(path) or EAL.duplicate_asset(source.get_path_name(),path);assert rules,path
        pieces=list(source.get_editor_property('pieces'))
        kept=[p for p in pieces if '/Environment/Vegetation/' not in p.get_editor_property('mesh').get_path_name()]
        rules.set_editor_property('pieces',kept);rules.set_editor_property('clear_radius',2600.)
        save(rules);a.set_editor_property('rules',rules)
        original=next((r['pieces_before'] for r in previous.get('sites',[]) if r['label']==label),len(pieces))
        result['sites'].append({'label':label,'rules':path,'pieces_before':original,'pieces_after':len(kept),
                                'artifact_actor_count':len(rules.get_editor_property('actors'))})
    for a in targets:assert actors.destroy_actor(a)
    result['pcg_volumes']={}
    for a in actors.get_all_level_actors():
        label=a.get_actor_label()
        if label.startswith(('PCG_FRG_TestFlat_','PCG_Fort_Raraka_')):
            result['pcg_volumes'][label]={'refPath':a.get_path_name()}
            if label.startswith('PCG_Fort_Raraka_'):a.set_folder_path('TestFlat/Zone_Generateurs/FortFrench/RarakaPlants');a.tags=['FortFrench_T208']
    assert all(fingerprint(a)==preserved[a.get_path_name()] for a in actors.get_all_level_actors() if a.get_path_name() in preserved)
    assert levels.save_current_level()
    result['saved_at']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'scene-result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')
    ue.log('T208_VEGETATION_SCENE_SUCCESS')


if __name__=='__main__':main()
