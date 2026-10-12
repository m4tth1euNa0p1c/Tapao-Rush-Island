"""Places of the execution scene tuned in PIE (DA_ExecutionRules): reads monke_island/Saved/T186/exectune.json
{"plank_start": [x, y, z], "squad_centre": [...], "camera_start": [...], ...} (vectors in the hull frame, numbers as
they are) and writes them into the data asset. Action "exectune"."""
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

kitlib.check_lock()
rules = kitlib.load("/Game/MonkeIsland/Galleon/Execution/DA_ExecutionRules")
values = json.loads((ROOT / "monke_island/Saved/T186/exectune.json").read_text(encoding="utf-8"))
done = {}
for key, value in values.items():
    if isinstance(value, list):
        rules.set_editor_property(key, ue.Vector(*value))
    else:
        rules.set_editor_property(key, value)
    done[key] = str(rules.get_editor_property(key))
kitlib.save(rules)
(ROOT / "monke_island/Saved/T186/exectune-done.json").write_text(json.dumps(done, indent=1), encoding="utf-8")
