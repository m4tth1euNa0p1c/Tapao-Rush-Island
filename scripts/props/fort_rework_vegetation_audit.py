"""T208 evidence from saved actors, rule assets and generated mesh instances."""
import datetime as dt
import json
import math
from pathlib import Path
import unreal as ue
from fort_showcase_import import check_lock

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / 'monke_island/Saved/T208'
PREFIXES = ('SM_CoconutPalm_', 'SM_Naupaka_', 'SM_Pandanus_', 'SM_MikiMiki_',
            'SM_Tahinu_', 'SM_Fern_', 'SM_Grass_', 'SM_Ipomoea_')


def inspect(actors, runtime=False):
    prep = json.loads((OUT / 'prepare-result.json').read_text())
    scene = json.loads((OUT / 'scene-result.json').read_text())
    labels = {a.get_actor_label(): a for a in actors}
    result = {'task': 'T-208', 'runtime': runtime, 'sites': [], 'pcg': {}}
    removed = [r['label'] for r in scene['removed_actors']]
    assert not set(removed).intersection(labels), 'Removed vegetation actors still present'
    result['removed_actor_count'] = len(removed)
    for previous in scene['sites']:
        a = labels[previous['label']]
        rules = a.get_editor_property('rules')
        meshes = [p.get_editor_property('mesh').get_path_name() for p in rules.get_editor_property('pieces')]
        assert all('/Environment/Vegetation/' not in p for p in meshes)
        artifacts = len(rules.get_editor_property('actors'))
        assert artifacts == previous['artifact_actor_count']
        components = list(a.get_components_by_class(ue.StaticMeshComponent))
        plants = [c.get_path_name() for c in components if c.static_mesh and '/Environment/Vegetation/' in c.static_mesh.get_path_name()]
        assert not plants, plants
        result['sites'].append({'label': previous['label'], 'pieces': len(meshes), 'artifact_spawns': artifacts,
                                'generated_mesh_components': len(components), 'plant_components': plants})
    for name, table in prep['tables'].items():
        label = ('PCG_Fort_Raraka_' + table['physics'] if table['zone'] == 'fort'
                 else 'PCG_FRG_TestFlat_' + table['physics'] + '_00')
        a = labels[label]
        count = 0; meshes = {}; intrusions = []
        for c in a.get_components_by_class(ue.InstancedStaticMeshComponent):
            mesh = c.static_mesh
            if not mesh: continue
            n = c.get_instance_count(); count += n
            meshes[mesh.get_path_name()] = meshes.get(mesh.get_path_name(), 0) + n
            if table['zone'] == 'testflat' and mesh.get_name().startswith(PREFIXES):
                for i in range(n):
                    p = c.get_instance_transform(i, world_space=True).translation
                    if any(math.hypot(p.x-x, p.y-y) < radius-.1 for x,y,radius in prep['sites']):
                        intrusions.append([p.x,p.y,p.z])
        assert count == table['rows'], (label, count, table['rows'])
        assert not intrusions, (label, intrusions[:10])
        result['pcg'][label] = {'instances': count, 'meshes': meshes, 'site_intrusions': len(intrusions)}
    result.update(passed=True, finished_at=dt.datetime.now(dt.timezone.utc).isoformat())
    path = OUT / ('pie-result.json' if runtime else 'audit-result.json')
    path.write_text(json.dumps(result, indent=2))
    return result


if __name__ == '__main__':
    check_lock()
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor()
    inspect(ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors())
    ue.log('T208_VEGETATION_AUDIT_SUCCESS')
