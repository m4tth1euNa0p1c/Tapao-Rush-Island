"""Profile of the site terrain of LI_FortFrench (SM_Fort_TerrainNear) from the fort's centre outward, to set the land fort
on the atoll (owner 2026-10-08): height of the first ground hit (TerrainNear, ditch, bank) every 2 m in 8 directions.
Writes monke_island/Saved/T186/fort-terrain-profile.json. Action "fortprofile" (the editor open on LI_FortFrench)."""
import json
import math
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
keep = {"Fort_T193_TerrainNear", "Fort_T193_215_Fort_Ditch", "Fort_T210_MoatBank", "Fort_T198_SM_Fort_MoatWater", "Fort_T193_CourtPaths"}
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
ignore = [a for a in actors if a.get_actor_label() not in keep]
out = {}
for k in range(8):
    a = 2 * math.pi * k / 8
    row = []
    for d in range(0, 96, 2):
        x, y = math.cos(a) * d * 100.0, math.sin(a) * d * 100.0
        hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(x, y, 3000.0), ue.Vector(x, y, -3000.0),
                                                 ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, ue.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        if t and t[0]:
            comp = t[10]
            owner = comp.get_owner().get_actor_label() if comp and comp.get_owner() else None
            row.append([d, round(t[4].z), owner])
        else:
            row.append([d, None, None])
    out[str(round(math.degrees(a)))] = row
(ROOT / "monke_island/Saved/T186/fort-terrain-profile.json").write_text(json.dumps(out), encoding="utf-8")
