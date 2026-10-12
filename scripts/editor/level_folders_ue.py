"""Folders of the open level: count, bounds (m) and a few labels each. Writes monke_island/Saved/T186/level-folders.json.
Action "levelfolders"."""
import collections
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
folders = collections.defaultdict(lambda: {"n": 0, "min": [1e12] * 3, "max": [-1e12] * 3, "labels": [], "classes": collections.Counter()})
for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
    f = folders[str(actor.get_folder_path())]
    f["n"] += 1
    loc = actor.get_actor_location()
    for i, v in enumerate((loc.x, loc.y, loc.z)):
        f["min"][i] = min(f["min"][i], v)
        f["max"][i] = max(f["max"][i], v)
    if len(f["labels"]) < 6:
        f["labels"].append(actor.get_actor_label())
    f["classes"][actor.get_class().get_name()] += 1
out = {"level": world.get_path_name(), "folders": {k: {"n": v["n"], "min_m": [round(x / 100) for x in v["min"]], "max_m": [round(x / 100) for x in v["max"]],
                                                     "labels": v["labels"], "classes": dict(v["classes"].most_common(5))}
                                                 for k, v in sorted(folders.items())}}
(ROOT / "monke_island/Saved/T186/level-folders.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
