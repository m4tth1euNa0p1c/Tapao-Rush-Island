"""Task-scoped editor commands, guarded by the shared editor lock and no-PIE rule."""
import datetime as dt
import json
import os
from pathlib import Path
import runpy
import time
import traceback
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'monke_island/Saved/T193'
REQUEST=OUT/'session-request.json'
LAST=time.time()
VALIDATOR=None
ACTIONS={'import':['import'],'setup':['setup'],'loot':['loot'],'apply':['import','setup','loot'],'validate':['validate'],'audit':['audit']}
ue.EditorPythonScripting.set_keep_python_script_alive(True)


def execute(action):
    global VALIDATOR
    try:
        lock=json.loads((ROOT/'.claude/factory/editor.lock.json').read_text(encoding='utf-8'))
        assert lock['owner'] in {'codex',os.environ.get('MONKE_LOCK_OWNER') or 'codex'} and dt.datetime.fromisoformat(lock['expires'])>dt.datetime.now().astimezone()
        assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(),'Current PIE must remain untouched'
        assert action in ACTIONS
        if action=='validate' and VALIDATOR:assert VALIDATOR['STATE']['phase']=='finished'
        revision=ROOT/'monke_island/Saved/T198/apply-result.json'
        if revision.exists() and action=='apply':
            namespace=runpy.run_path(str(ROOT/'scripts/props/fort_rework_apply.py'),run_name='__main__')
        else:
            assert not (revision.exists() and action in ('setup','loot')),'T198 art revision is active; use apply to preserve the new layout and prices'
            for suffix in ACTIONS[action]:namespace=runpy.run_path(str(ROOT/f'scripts/props/fort_showcase_{suffix}.py'),run_name='__main__')
        if action=='validate':VALIDATOR=namespace
        result={'action':action,'passed':True}
    except Exception:
        result={'action':action,'passed':False,'error':traceback.format_exc()};ue.log_error(result['error'])
    result['finished_at']=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/'session-result.json').write_text(json.dumps(result,indent=2),encoding='utf-8')


def poll(delta):
    global LAST
    if REQUEST.exists() and REQUEST.stat().st_mtime>LAST:
        LAST=REQUEST.stat().st_mtime;execute(json.loads(REQUEST.read_text(encoding='utf-8-sig'))['action'])


OUT.mkdir(parents=True,exist_ok=True)
HANDLE=ue.register_slate_post_tick_callback(poll)
(OUT/'session-ready.json').write_text(json.dumps({'started_at':dt.datetime.now(dt.timezone.utc).isoformat()}),encoding='utf-8')
execute('apply')
