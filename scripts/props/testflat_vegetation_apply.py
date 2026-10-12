"""Apply T208 through discovered MCP tools and the guarded Python bridge of the editor session.

Safe to retry: the operation stops before any new mutation if PIE starts.
Handed over to Claude (docs/production/handoffs/T198-T208-claude.txt): the lock may be held by codex or by the session
named in MONKE_LOCK_OWNER; the bridge is scripts/editor/lead_session_t186.py (actions fortveg / fortvegaudit, after the
action "paths") instead of Codex's T193 session.
"""
import csv
import argparse
import datetime as dt
import json
import os
import sys
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/map'))
from fragment_villages_ue import Mcp, ref

OUT = ROOT / 'monke_island/Saved/T208'
APP = 'EditorToolset.EditorAppToolset'
PCG = 'PCGToolset.PCGToolset'
ACT = 'editor_toolset.toolsets.actor.ActorTools'
ASSET = 'editor_toolset.toolsets.asset.AssetTools'
TABLE = 'editor_toolset.toolsets.data_table.DataTableTools'


OWNERS = {'codex', os.environ.get('MONKE_LOCK_OWNER') or 'codex'}
# T193 bridge action -> action of the lead session
BRIDGE_ACTIONS = {'vegetation': 'fortveg', 'vegetation_audit': 'fortvegaudit'}


def guard(mcp):
    lock = json.loads((ROOT / '.claude/factory/editor.lock.json').read_text())
    assert lock['owner'] in OWNERS, lock['owner']
    assert dt.datetime.fromisoformat(lock['expires']) > dt.datetime.now().astimezone()
    assert mcp.call(APP, 'IsPIERunning', {}) is False, 'User PIE is running; no mutation'
    level = mcp.call('editor_toolset.toolsets.scene.SceneTools', 'get_current_level', {})
    assert level.split('.')[0] == '/Game/MonkeIsland/Maps/Test/L_Test_Flat', level


def bridge(mcp, action, timeout=300):
    guard(mcp)
    output = ROOT / 'monke_island/Saved/T186/session-result.json'
    old = output.stat().st_mtime_ns if output.exists() else 0
    time.sleep(1.1)
    (ROOT / 'monke_island/Saved/T186/session-request.json').write_text(json.dumps({'action': BRIDGE_ACTIONS[action]}))
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if output.exists() and output.stat().st_mtime_ns > old:
            result = json.loads(output.read_text())
            assert result['passed'], result
            return result
        time.sleep(1)
    raise TimeoutError(action)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--wait-seconds', type=int, default=0)
    args = parser.parse_args()
    mcp = Mcp()
    deadline = time.monotonic() + args.wait_seconds
    while mcp.call(APP, 'IsPIERunning', {}) is True and time.monotonic() < deadline:
        lock = json.loads((ROOT / '.claude/factory/editor.lock.json').read_text())
        assert lock['owner'] in OWNERS and dt.datetime.fromisoformat(lock['expires']) > dt.datetime.now().astimezone()
        time.sleep(5)
    guard(mcp)
    prep = json.loads((OUT / 'prepare-result.json').read_text())
    result = {'task': 'T-208', 'tables': {}, 'graphs': {}}
    for name, info in prep['tables'].items():
        guard(mcp)
        if mcp.call(ASSET, 'exists', {'path': info['asset']}) is not True:
            created = mcp.call(TABLE, 'import_file', {
                'folder_path': info['asset'].rsplit('/', 1)[0], 'asset_name': name,
                'source_file': info['file'],
                'schema': {'refPath': '/Script/GameplayTags.GameplayTagTableRow'}})
            assert created, name
        rows = mcp.call(TABLE, 'list_rows', {'data_table': ref(info['asset'])})
        assert len(rows) == info['rows'], (name, len(rows), info['rows'])
        result['tables'][name] = len(rows)
        print('table', name, len(rows), flush=True)
    volumes = {}
    for info in mcp.call(PCG, 'ListGraphInstances', {}):
        label = mcp.call(ACT, 'get_label', {'actor': info['actor']})
        if label.startswith(('PCG_FRG_TestFlat_', 'PCG_Fort_Raraka_')):
            volumes[label] = info['actor']
    for physics in ('Hard', 'Soft'):
        label = 'PCG_Fort_Raraka_' + physics
        table = prep['tables']['DT_Fort_Raraka_' + physics]
        if label not in volumes:
            guard(mcp)
            with open(table['file'], newline='') as f:
                positions = [list(map(float, r['DevComment'].split('|')[0].split(','))) for r in csv.DictReader(f)]
            low = [min(p[i] for p in positions) - 1000 for i in range(3)]
            high = [max(p[i] for p in positions) + 2000 for i in range(3)]
            volumes[label] = mcp.call(PCG, 'SpawnGraphInstance', {
                'graph': ref('/Game/MonkeIsland/Environment/PCG/PCG_Raraka_Veg' + physics),
                'name': label,
                'transform': {'location': dict(zip('xyz', [(a+b)/2 for a,b in zip(low,high)])),
                              'rotation': {'pitch': 0, 'yaw': 0, 'roll': 0},
                              'scale': dict(zip('xyz', [(b-a)/200 for a,b in zip(low,high)]))},
                'jsonParams': json.dumps({'DataTable': ref(table['asset'])['refPath']})})['actor']
            assert mcp.call(ACT, 'set_label', {'actor': volumes[label], 'label': label})
    bridge(mcp, 'vegetation')
    for name, info in prep['tables'].items():
        guard(mcp)
        label = ('PCG_Fort_Raraka_' + info['physics'] if info['zone'] == 'fort'
                 else 'PCG_FRG_TestFlat_' + info['physics'] + '_00')
        volume = volumes[label]
        assert mcp.call(PCG, 'SetGraphInstanceParams', {
            'pCGVolume': volume, 'jsonParams': json.dumps({'DataTable': ref(info['asset'])['refPath']})})
        messages = mcp.call(PCG, 'ExecuteGraphInstance', {'pCGVolume': volume}, timeout=900)
        assert not any('error' in r['severity'].lower() for r in messages), messages
        params = mcp.call(PCG, 'GetGraphInstanceParams', {'pCGVolume': volume})
        result['graphs'][label] = {'actor': volume, 'rows': info['rows'], 'messages': messages, 'params': params}
        print('graph', label, info['rows'], messages, flush=True)
    guard(mcp)
    assert mcp.call(ASSET, 'save_assets', {'asset_paths': [r['asset'] for r in prep['tables'].values()] + ['/Game/MonkeIsland/Maps/Test/L_Test_Flat']})
    result['finished_at'] = dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT / 'apply-result.json').write_text(json.dumps(result, indent=2))
    bridge(mcp, 'vegetation_audit')
    print('T208_APPLY_SUCCESS', flush=True)


if __name__ == '__main__':
    main()
