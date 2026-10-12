"""Contents of LI_FortFrench by kind, to build the land fort of the Raraka atoll (owner 2026-10-08: "les forts français
sur la map de Raraka, générés procéduralement"): label, class, folder, place and bounds of every actor of the open level.
Writes monke_island/Saved/T186/fort-li-dump.json. Action "fortlidump" (the editor open on LI_FortFrench)."""
import collections
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
rows = []
for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
    o, e = a.get_actor_bounds(False)
    loc = a.get_actor_location()
    mesh = None
    if isinstance(a, ue.StaticMeshActor):
        m = a.static_mesh_component.get_editor_property("static_mesh")
        mesh = m.get_name() if m else None
    rows.append({"label": a.get_actor_label(), "class": a.get_class().get_name(), "folder": str(a.get_folder_path()),
                 "at": [round(loc.x), round(loc.y), round(loc.z)], "centre": [round(o.x), round(o.y), round(o.z)],
                 "extent": [round(e.x), round(e.y), round(e.z)], "mesh": mesh})
by_folder = collections.defaultdict(lambda: {"n": 0, "min": [1e9, 1e9, 1e9], "max": [-1e9, -1e9, -1e9]})
for r in rows:
    f = by_folder[r["folder"]]
    f["n"] += 1
    for i in range(3):
        f["min"][i] = min(f["min"][i], r["centre"][i] - r["extent"][i])
        f["max"][i] = max(f["max"][i], r["centre"][i] + r["extent"][i])
out = {"level": world.get_path_name(), "actors": len(rows), "by_folder": by_folder, "rows": rows}
(ROOT / "monke_island/Saved/T186/fort-li-dump.json").write_text(json.dumps(out, indent=0), encoding="utf-8")
