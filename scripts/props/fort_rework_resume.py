"""Resume T198/T208 after a coordinated editor restart, never during another lock or PIE.

This launcher keeps the acquired lock for the parent to run imports and PIE checks.
It deliberately does not rebuild C++ or stop an existing play session.
"""
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'scripts/props'))
from apply_galleon_t149 import lock, editor_running, Mcp, command, ENGINE, PROJECT, LEVEL


def main():
    out = ROOT / 'monke_island/Saved/T198'
    lock('acquire', '--reason', 'T198/T208 rework fort, vegetation Raraka et validation', '--minutes', '45')
    launched = False
    try:
        if editor_running():
            assert Mcp().call('EditorToolset.EditorAppToolset', 'IsPIERunning', {}) is False, 'Existing PIE remains untouched'
        stamp = dt.datetime.now().astimezone().isoformat(timespec='seconds')
        for task in ('T-198', 'T-208'):
            command(sys.executable, str(ROOT / 'scripts/chantier.py'), 'set', task, 'en-cours', '--note',
                    'env=dev;owner=editor codex;owner=L_Test_Flat;fenetre45min ' + stamp +
                    ';reprise T198/T208:derniers meshes+lanternes fort,vegetation locale Raraka,PIE34routes22grabs;preserver autres modifications.')
        if editor_running(): lock('close-editor')
        assert not editor_running()
        backup = out / ('L_Test_Flat-before-resume-' + dt.datetime.now().strftime('%Y%m%d-%H%M%S') + '.umap')
        shutil.copy2(ROOT / 'monke_island/Content/MonkeIsland/Maps/Test/L_Test_Flat.umap', backup)
        args = [str(ENGINE / 'UnrealEditor.exe'), str(PROJECT), LEVEL, '-nosplash', '-NoP4',
                '-ExecutePythonScript=' + str(ROOT / 'scripts/props/fort_showcase_session.py')]
        startup = subprocess.STARTUPINFO()
        startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow = 0
        env = dict(os.environ, MONKE_LOCK_OWNER='codex')
        with (out / 'editor-resume.log').open('a', encoding='utf-8') as log:
            process = subprocess.Popen(args, cwd=ROOT, env=env, startupinfo=startup, stdout=log, stderr=subprocess.STDOUT)
        launched = True
        (out / 'resume-result.json').write_text(json.dumps({'started_at': stamp, 'pid': process.pid,
                                                         'backup': str(backup)}, indent=2))
        print('T198_EDITOR_RESUMED', process.pid, flush=True)
    finally:
        if not launched: lock('release')


if __name__ == '__main__': main()
