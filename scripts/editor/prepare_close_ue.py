"""Before the lead closes the editor for a build: saves the project's unsaved maps and content, reverts any engine
content an import touched (never saved into the engine install: e.g. /Engine/EngineSounds/Master when a project sound
class was parented to it), then lists what is still unsaved. With nothing left, the editor closes without the
"Save Content" dialog. Lead session action "prepclose". Report: monke_island/Saved/T186/prepare-close.json."""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T186/prepare-close.json"
lib = ue.EditorLoadingAndSavingUtils
report = {"maps_before": [p.get_name() for p in lib.get_dirty_map_packages()],
          "content_before": [p.get_name() for p in lib.get_dirty_content_packages()]}
engine = [p for p in lib.get_dirty_content_packages() if p.get_name().startswith("/Engine/")]
if engine:
    try:
        report["engine_reverted"] = [p.get_name() for p in engine]
        report["revert_result"] = str(lib.reload_packages(engine, ue.ReloadPackagesInteractionMode.ASSUME_POSITIVE))
    except Exception as error:  # noqa: BLE001
        report["revert_error"] = repr(error)
project = [p for p in lib.get_dirty_content_packages() if p.get_name().startswith("/Game/")]
if project:
    report["content_saved"] = lib.save_packages(project, True)
if lib.get_dirty_map_packages():
    report["maps_saved"] = lib.save_dirty_packages(True, False)
report["maps_after"] = [p.get_name() for p in lib.get_dirty_map_packages()]
report["content_after"] = [p.get_name() for p in lib.get_dirty_content_packages()]
OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
