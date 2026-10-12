"""Line traces across the fort's ditch on its east side (T-210): what is hit, at what height (Saved/T210/traces.json)."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OX, OY, OZ = -66000.0, 0.0, 115.0
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
rows = []
for side, (dx, dy) in (("east", (1, 0)), ("west", (-1, 0)), ("northeast", (0.7071, 0.7071))):
    for r in range(2400, 3500, 25) if side != "northeast" else range(3800, 5100, 25):
        x, y = OX + dx * r, OY + dy * r
        for complex_trace in (False, True):
            hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(x, y, OZ + 600), ue.Vector(x, y, OZ - 600),
                                                     ue.TraceTypeQuery.TRACE_TYPE_QUERY1, complex_trace, [], ue.DrawDebugTrace.NONE, True)
            t = hit.to_tuple() if hit else None
            if t and t[0]:
                actor = t[9] if len(t) > 9 else None
                rows.append({"side": side, "r": r, "complex": complex_trace, "z": round(t[4].z - OZ),
                             "actor": actor.get_actor_label() if actor else None})
            else:
                rows.append({"side": side, "r": r, "complex": complex_trace, "z": None, "actor": None})
(ROOT / "monke_island/Saved/T210/traces.json").write_text(json.dumps(rows, indent=1), encoding="utf-8")
ue.log("[T210] traces done")
