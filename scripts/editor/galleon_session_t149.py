"""Apply and validate T-149 in a full editor, with a fixed task action list.

All actions require the codex lock. Requests are fresh local files; no action
starts/stops PIE. This enables retries without interrupting the editor again.
"""
import datetime as dt
import json
import os
from pathlib import Path
import runpy
import time
import traceback
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
# Lock holders allowed to run this T-149 step: codex, or the session named in MONKE_LOCK_OWNER
LOCK_OWNERS = {"codex", os.environ.get("MONKE_LOCK_OWNER") or "codex"}
OUT = ROOT / "monke_island/Saved/T149"
REQUEST = OUT / "session-request.json"
BOOT = time.time()
LAST = BOOT
ACTIONS = {
    "apply": "scripts/props/apply_galleon_assets_t149.py",
    "refresh": "scripts/editor/refresh_galleon_testflat_t149.py",
    "validate": "scripts/editor/validate_galleon_t149.py",
    "promote": "scripts/props/promote_galleon_t149.py",
}
VALIDATOR = None
ue.EditorPythonScripting.set_keep_python_script_alive(True)


def write(name, value):
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / name).write_text(json.dumps(value, indent=2), encoding="utf-8")


def check_lock():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    assert lock["owner"] in LOCK_OWNERS
    assert dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone()


def execute(action):
    global VALIDATOR
    try:
        check_lock()
        assert action in ACTIONS, "Unknown T-149 action"
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
        assert not world or "UEDPIE" not in world.get_path_name(), "Leave current PIE untouched"
        if action == "validate" and VALIDATOR:
            assert VALIDATOR["STATE"]["phase"] == "finished", "Previous traversal is still active"
        namespace = runpy.run_path(str(ROOT / ACTIONS[action]), run_name="__main__")
        if action == "validate":
            VALIDATOR = namespace
            write("pie-validation-request.json", {"requested_at": dt.datetime.now(dt.timezone.utc).isoformat()})
        write("session-result.json", {"action": action, "passed": True,
                                      "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()})
    except Exception:
        error = traceback.format_exc()
        write("session-result.json", {"action": action, "passed": False, "error": error,
                                      "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()})
        ue.log_error(error)


def poll(delta):
    global LAST
    if REQUEST.exists() and REQUEST.stat().st_mtime > LAST:
        LAST = REQUEST.stat().st_mtime
        try:
            execute(json.loads(REQUEST.read_text(encoding="utf-8-sig"))["action"])
        except Exception:
            ue.log_error(traceback.format_exc())


HANDLE = ue.register_slate_post_tick_callback(poll)
write("session-ready.json", {"started_at": dt.datetime.now(dt.timezone.utc).isoformat()})
execute("apply")
