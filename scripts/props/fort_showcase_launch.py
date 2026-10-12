"""Acquire an available editor, back up Test Flat, launch the prepared T193 import."""
import datetime as dt
from pathlib import Path
import shutil
import subprocess
import sys

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/'monke_island/Saved/T193'
sys.path.insert(0,str(ROOT/'scripts/props'))
from apply_galleon_t149 import lock,editor_running,Mcp,command,ENGINE,PROJECT,LEVEL


def main():
    lock('acquire','--reason','T-193 fort francais: 68 modules PBR, ile/port Test Flat, controles PIE','--minutes','45')
    launched=False
    try:
        if editor_running():assert Mcp().call('EditorToolset.EditorAppToolset','IsPIERunning',{}) is False,'Existing PIE remains untouched'
        stamp=dt.datetime.now().astimezone().isoformat(timespec='seconds')
        command(sys.executable,str(ROOT/'scripts/chantier.py'),'set','T-193','en-cours','--note',
                f'env=dev ; owner=editor codex ; owner=L_Test_Flat ; owner=Content/MonkeIsland/Environment/FortFrench ; fenetre editeur {stamp} 45min. '
                '68 modules + 4 terrains locaux, 30 materiaux PBR, 450 UCX, 216 instances + 391 vegetaux ; preflight Blender OK. '
                'Extension ouest Ile_Generateurs 90m et fort (-66000,0,115), ponton22m,22nouveaux butins pres Loot/Pickup. Tests acces,prise/relache et captures. Conserver T185 T187 galionT188.')
        if editor_running():lock('close-editor')
        assert not editor_running()
        OUT.mkdir(parents=True,exist_ok=True)
        backup=OUT/('L_Test_Flat-before-'+dt.datetime.now().strftime('%Y%m%d-%H%M%S')+'.umap')
        shutil.copy2(ROOT/'monke_island/Content/MonkeIsland/Maps/Test/L_Test_Flat.umap',backup)
        args=[str(ENGINE/'UnrealEditor.exe'),str(PROJECT),LEVEL,'-nosplash','-NoP4','-ExecutePythonScript='+str(ROOT/'scripts/props/fort_showcase_session.py')]
        startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=1
        with (OUT/'editor-launch.log').open('a',encoding='utf-8') as log:
            process=subprocess.Popen(args,cwd=ROOT,startupinfo=startup,stdout=log,stderr=subprocess.STDOUT)
        launched=True
        print(f'T193_EDITOR_STARTED pid={process.pid}; lock retained for validation',flush=True)
    finally:
        if not launched:lock('release')


if __name__=='__main__':main()
