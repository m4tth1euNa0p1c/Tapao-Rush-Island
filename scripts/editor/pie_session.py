"""Second editor Python session of the lead, for checks DURING a game this session started itself (lead_session_t186.py
refuses every action while a game runs, to never touch a player's PIE).

Started by the action "piesession" of lead_session_t186.py (no game running). Then write
monke_island/Saved/T186/pie-request.json {"action": "ceremonycheck"}: the script runs once per new request, only while
a game runs and only for the actions of PIE_ACTIONS; its result is in pie-result.json.
"""
import datetime as dt
import json
import os
import runpy
import time
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(r"C:/Users/matth/monk-island")
OUT = ROOT / "monke_island/Saved/T186"
REQUEST = OUT / "pie-request.json"
PIE_ACTIONS = {
    "ceremonycheck": "scripts/editor/ceremony_pie_check.py",
    "probe": "scripts/editor/py_probe.py",
    "musketcheck": "scripts/editor/musket_pie_check.py",
    "garrisoncheck": "scripts/editor/garrison_pie_check.py",
    "execprobe": "scripts/editor/exec_probe_pie.py",
    "fortcheck": "scripts/editor/fort_land_pie_check.py",
    "mpcheck": "scripts/editor/mp_pie_check.py",
}
LAST = time.time()


def execute(action):
    try:
        lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
        owner = os.environ.get("MONKE_LOCK_OWNER")
        assert owner and lock["owner"] == owner, f"editor lock held by {lock['owner']}"
        assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "no game running"
        assert action in PIE_ACTIONS, f"unknown action {action}"
        runpy.run_path(str(ROOT / PIE_ACTIONS[action]), run_name="__main__")
        result = {"action": action, "passed": True}
    except Exception:
        result = {"action": action, "passed": False, "error": traceback.format_exc()}
        ue.log_error(result["error"])
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT / "pie-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")


def poll(delta):
    global LAST
    if REQUEST.exists() and REQUEST.stat().st_mtime > LAST:
        LAST = REQUEST.stat().st_mtime
        try:
            execute(json.loads(REQUEST.read_text(encoding="utf-8-sig"))["action"])
        except Exception:
            ue.log_error(traceback.format_exc())


HANDLE = ue.register_slate_post_tick_callback(poll)
(OUT / "pie-session-ready.json").write_text(json.dumps({"started_at": dt.datetime.now(dt.timezone.utc).isoformat()}), encoding="utf-8")
