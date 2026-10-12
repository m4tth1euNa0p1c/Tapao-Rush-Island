"""Task-scoped full editor session for the T-179 temple/relic import."""
import datetime as dt
import json
from pathlib import Path
import runpy
import time
import traceback
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT/"monke_island/Saved/T179"
REQUEST = OUT/"session-request.json"
LAST = time.time()
ACTIONS = {"import":["scripts/props/onedrive_showcase_import.py"],
           "setup":["scripts/props/onedrive_showcase_setup.py"],
           "structures":["scripts/props/onedrive_showcase_structures.py"],
           "merchant_import":["scripts/shop/merchant_showcase_import.py"],
           "merchant_place":["scripts/shop/merchant_showcase_place.py"],
           "apply":["scripts/props/onedrive_showcase_structures.py","scripts/props/onedrive_showcase_import.py","scripts/props/onedrive_showcase_setup.py"],
           "validate":["scripts/props/onedrive_showcase_validate.py"]}
ue.EditorPythonScripting.set_keep_python_script_alive(True)
VALIDATOR = None


def execute(action):
    global VALIDATOR
    try:
        lock=json.loads((ROOT/".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
        assert lock["owner"]=="codex" and dt.datetime.fromisoformat(lock["expires"])>dt.datetime.now().astimezone()
        assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "Current PIE must remain untouched"
        assert action in ACTIONS
        if action=="validate" and VALIDATOR:
            assert VALIDATOR["STATE"]["phase"]=="finished", "Previous validator still active"
        for script in ACTIONS[action]:
            namespace=runpy.run_path(str(ROOT/script),run_name="__main__")
        if action=="validate":
            VALIDATOR=namespace
            (OUT/"pie-validation-request.json").write_text("{}",encoding="utf-8")
        result={"action":action,"passed":True}
    except Exception:
        result={"action":action,"passed":False,"error":traceback.format_exc()}
        ue.log_error(result["error"])
    result["finished_at"]=dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT/"session-result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")


def poll(delta):
    global LAST
    if REQUEST.exists() and REQUEST.stat().st_mtime>LAST:
        LAST=REQUEST.stat().st_mtime
        execute(json.loads(REQUEST.read_text(encoding="utf-8-sig"))["action"])


OUT.mkdir(parents=True,exist_ok=True)
HANDLE=ue.register_slate_post_tick_callback(poll)
(OUT/"session-ready.json").write_text(json.dumps({"started_at":dt.datetime.now(dt.timezone.utc).isoformat()}),encoding="utf-8")
execute("apply")
