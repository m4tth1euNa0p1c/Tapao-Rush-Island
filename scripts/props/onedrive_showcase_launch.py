"""Open Test Flat and apply T-179, after acquiring the shared editor lock.

No forced close, no foreign PIE stopped. Acquires at execution time so an OS
approval delay cannot expire a lock before the launch. Keeps it for validation.
"""
import datetime as dt
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"monke_island/Saved/T179"
sys.path.insert(0,str(ROOT/"scripts/props"))
from apply_galleon_t149 import lock, editor_running, Mcp, command, ENGINE, PROJECT, LEVEL
sys.path.insert(0,str(ROOT/"scripts"))
from editor_lock import read as read_editor_lock


def main():
    deadline=time.monotonic()+75*60
    last_owner=None
    while True:
        held=read_editor_lock()
        if not held or held["owner"]=="codex":
            break
        if held["owner"]!=last_owner:
            print(f"Waiting for {held['owner']} to release the editor; no forced close",flush=True)
            last_owner=held["owner"]
        if time.monotonic()>deadline:
            raise TimeoutError("Editor still owned after 75 minutes; nothing launched")
        time.sleep(10)
    lock("acquire","--reason","T-179 OneDrive temple/relics import and Test Flat checks","--minutes","45")
    launched=False
    try:
        if editor_running():
            assert Mcp().call("EditorToolset.EditorAppToolset","IsPIERunning",{}) is False, "Existing PIE; do not interrupt"
            lock("close-editor")
        assert not editor_running()
        stamp=dt.datetime.now().astimezone().isoformat(timespec="seconds")
        command(sys.executable,str(ROOT/"scripts/chantier.py"),"set","T-179","en-cours","--note",
                f"env=dev ; owner=editor codex ; owner=L_Test_Flat ; fenetre {stamp} 45min. Import 6 reliques + 22 modules PBR, assemblage temple et ouverture de Test Flat. Sources stagees 129 fichiers, preflight Blender 40 FBX OK ; controles editeur/PIE. Galion laisse a sa session T-149.")
        OUT.mkdir(parents=True,exist_ok=True)
        backup=OUT/("backup-"+dt.datetime.now().strftime("%Y%m%d-%H%M%S"))
        for rel in ("Maps/Test/L_Test_Flat.umap","Galleon/DA_GalleonRules.uasset"):
            src=ROOT/"monke_island/Content/MonkeIsland"/rel
            target=backup/rel
            target.parent.mkdir(parents=True,exist_ok=True)
            shutil.copy2(src,target)
        args=[str(ENGINE/"UnrealEditor.exe"),str(PROJECT),LEVEL,"-nosplash","-NoP4",
              "-ExecutePythonScript="+str(ROOT/"scripts/props/onedrive_showcase_session.py")]
        startup=subprocess.STARTUPINFO()
        startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW
        startup.wShowWindow=1  # User explicitly asked to open the editor.
        with (OUT/"editor-launch.log").open("a",encoding="utf-8") as log:
            process=subprocess.Popen(args,cwd=ROOT,startupinfo=startup,stdout=log,stderr=subprocess.STDOUT)
        launched=True
        print(f"T179_EDITOR_STARTED pid={process.pid}; inspect Saved/T179/session-result.json; lock retained",flush=True)
    finally:
        if not launched:
            lock("release")


if __name__=="__main__":
    main()
