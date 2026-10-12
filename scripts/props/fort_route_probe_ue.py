"""Probe of the two fort routes that fail (T-198): horizontal traces at several heights where the pawn stops, and the
ground under the route (Saved/T210/routes.json)."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()


def hit(a, b):
    h = ue.SystemLibrary.line_trace_single(world, a, b, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], ue.DrawDebugTrace.NONE, True)
    t = h.to_tuple() if h else None
    if t and t[0]:
        actor = t[9] if len(t) > 9 else None
        return {"at": [round(t[4].x), round(t[4].y), round(t[4].z)], "actor": actor.get_actor_label() if actor else None,
                "normal": [round(t[6].x, 2), round(t[6].y, 2), round(t[6].z, 2)]}
    return None


out = {"warehouse": {"ground": [], "walls": []}, "tower_level_1": {"ground": [], "walls": []}}
# warehouse: from y 4000 toward y 3700 at x -67000
for y in range(4020, 3680, -10):
    out["warehouse"]["ground"].append([y, hit(ue.Vector(-67000, y, 400), ue.Vector(-67000, y, 0))])
for z in (110, 125, 140, 160, 190, 220):
    out["warehouse"]["walls"].append([z, hit(ue.Vector(-67000, 4010, z), ue.Vector(-67000, 3690, z))])
# tower level 1: from (-63320,-2090) to (-63507,-1937) at z ~676
a, b = ue.Vector(-63320.4, -2090.1, 0), ue.Vector(-63507.1, -1937.4, 0)
for k in range(0, 21):
    p = a + (b - a) * (k / 20.0)
    out["tower_level_1"]["ground"].append([k, hit(ue.Vector(p.x, p.y, 900), ue.Vector(p.x, p.y, 500))])
for z in (630, 680, 700, 730, 760, 790, 820):
    out["tower_level_1"]["walls"].append([z, hit(ue.Vector(a.x, a.y, z), ue.Vector(b.x, b.y, z))])
(ROOT / "monke_island/Saved/T210/routes.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
ue.log("[T198] route probe done")
