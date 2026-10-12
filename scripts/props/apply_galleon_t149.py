"""Apply T-149 after acquiring a fresh editor window at execution time.

Requires OS permission to close/relaunch Unreal. Never force-closes an editor,
never stops an existing PIE, and preserves backups of the affected packages.
Keeps the codex lock after success for the parent's visual/PIE checks.
"""
import datetime as dt
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T149"
ENGINE = Path(r"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64")
PROJECT = ROOT / "monke_island/monke_island.uproject"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
sys.path.insert(0, str(ROOT / "scripts/character"))
from showcase_combat_reactions import Mcp


def command(*args):
    return subprocess.run(args, cwd=ROOT, check=True)


def lock(action, *args):
    return command(sys.executable, str(ROOT / "scripts/editor_lock.py"), action, "--owner", "codex", *args)


def editor_running():
    probe = subprocess.run(["powershell", "-NoProfile", "-Command",
                            "if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { exit 0 } else { exit 1 }"],
                           capture_output=True)
    return probe.returncode == 0


def launch_editor(validate):
    args = [str(ENGINE / "UnrealEditor.exe"), str(PROJECT), LEVEL, "-nosplash", "-NoP4"]
    if validate:
        args.append("-ExecutePythonScript=" + str(ROOT / "scripts/editor/galleon_session_t149.py"))
    startup = subprocess.STARTUPINFO()
    startup.dwFlags |= subprocess.STARTF_USESHOWWINDOW
    startup.wShowWindow = 0
    with (OUT / "editor-launch.log").open("a", encoding="utf-8") as log:
        process = subprocess.Popen(args, cwd=ROOT, startupinfo=startup, stdout=log, stderr=subprocess.STDOUT)
    print(f"Editor launched: pid={process.pid}; validation={validate}", flush=True)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    lock("acquire", "--reason", "T-149 corrected galleon import and PIE validation", "--minutes", "45")
    closed = False
    launched = False
    success = False
    try:
        if editor_running():
            pie = Mcp().call("EditorToolset.EditorAppToolset", "IsPIERunning", {})
            assert pie is False, f"Existing PIE or unknown state: {pie!r}; leave editor untouched"
        stamp = dt.datetime.now().astimezone().isoformat(timespec="seconds")
        command(sys.executable, str(ROOT / "scripts/chantier.py"), "set", "T-149", "en-cours", "--note",
                f"owner=editor codex ; fenetre editeur {stamp} pour 45min. PIE absent, import 7 FBX corriges + PBR puis refresh cible L_Test_Flat et validation PIE ; T-165 apres controles.")
        lock("close-editor")
        assert not editor_running(), "Editor still open; abort import"
        closed = True
        backup = OUT / ("backup-" + dt.datetime.now().strftime("%Y%m%d-%H%M%S"))
        for relative in ("Galleon/TestFlat", "Maps/Test/L_Test_Flat.umap", "Galleon/DA_GalleonRules.uasset"):
            source = ROOT / "monke_island/Content/MonkeIsland" / relative
            target = backup / relative
            if source.exists():
                target.parent.mkdir(parents=True, exist_ok=True)
                if source.is_dir():
                    shutil.copytree(source, target)
                else:
                    shutil.copy2(source, target)
        print(f"Backups: {backup}", flush=True)
        # StaticMeshEditorSubsystem / LevelEditorSubsystem are unavailable in
        # commandlet mode. Apply from the actual editor once its registry is ready.
        launch_editor(validate=True)
        launched = True
        success = True
        print("T149_APPLICATION_STARTED; inspect session-result.json; lock retained for checks", flush=True)
    finally:
        if not success:
            if closed and not launched and not editor_running():
                launch_editor(validate=False)
            lock("release")


if __name__ == "__main__":
    main()
