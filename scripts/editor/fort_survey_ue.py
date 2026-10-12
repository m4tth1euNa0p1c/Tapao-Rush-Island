"""Survey of the French fort of L_Test_Flat (T-205: the fort in the campaign): its actors, by folder and class, the bounds
of the whole and of its ground meshes, the garrison zone. Read only; loads L_Test_Flat when another level is open.
Writes monke_island/Saved/T186/fort-survey.json. Runs from scripts/editor/lead_session_t186.py, action "fortsurvey".
"""
import collections
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"

world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(LEVEL):
    assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(LEVEL), "L_Test_Flat not loaded"
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()

by_folder = collections.Counter()
fort = []
for a in actors:
    folder = str(a.get_folder_path())
    by_folder[folder.split("/")[0] if folder else "(root)"] += 1
    label = a.get_actor_label()
    if "fort" in folder.lower() or label.startswith("Fort") or "FortFR" in label:
        fort.append(a)

lo = [1e12, 1e12, 1e12]
hi = [-1e12, -1e12, -1e12]
classes = collections.Counter()
folders = collections.Counter()
grounds = []
for a in fort:
    classes[a.get_class().get_name()] += 1
    folders[str(a.get_folder_path())] += 1
    o, e = a.get_actor_bounds(False)
    if e.x > 1e6:
        continue
    for i, (c, x) in enumerate(((o.x, e.x), (o.y, e.y), (o.z, e.z))):
        lo[i] = min(lo[i], c - x)
        hi[i] = max(hi[i], c + x)
    label = a.get_actor_label()
    if any(k in label for k in ("Terrain", "Ground", "Moat", "Bank", "Water", "Port", "Quay", "Pier")):
        grounds.append({"label": label, "class": a.get_class().get_name(), "origin": [round(o.x), round(o.y), round(o.z)],
                        "extent": [round(e.x), round(e.y), round(e.z)]})
zone = next((a for a in fort if a.get_class().get_name() == "FortGarrisonZone"), None)
report = {"level": world.get_path_name(), "actors_total": len(actors), "fort_actors": len(fort),
          "bounds_min": [round(v) for v in lo], "bounds_max": [round(v) for v in hi],
          "size_m": [round((hi[i] - lo[i]) / 100.0, 1) for i in range(3)],
          "classes": classes.most_common(25), "fort_folders": folders.most_common(40), "top_folders": by_folder.most_common(30),
          "grounds": grounds,
          "zone": zone.get_actor_label() if zone else None}
(ROOT / "monke_island/Saved/T186/fort-survey.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
