"""Profile of the galleon's deck (execution scene, owner 2026-10-07): in the editor world, line traces down through the
hull across the beam (X) at a few places along the ship (Y), in the hull's frame; the bean_spawn socket and the hull
bounds. Writes monke_island/Saved/T186/galleon-deck.json. Action "galleondeck"."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
ships = ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)
out = {"ships": len(ships)}
if ships:
    ship = ships[0]
    hull = ship.get_hull()
    xf = hull.get_world_transform()
    out["hull_world"] = [round(xf.translation.x), round(xf.translation.y), round(xf.translation.z), round(xf.rotation.rotator().yaw, 1)]
    o, e = ship.get_actor_bounds(False)
    out["bounds_world"] = [[round(o.x), round(o.y), round(o.z)], [round(e.x), round(e.y), round(e.z)]]
    if hull.does_socket_exist("bean_spawn"):
        s = xf.inverse_transform_location(hull.get_socket_location("bean_spawn"))
        out["bean_spawn_local"] = [round(s.x), round(s.y), round(s.z)]
    rows = {}
    for y in (-600, -300, 0, 300, 600):
        row = []
        for x in range(-500, 525, 25):
            a = xf.transform_location(ue.Vector(x, y, 1600.0))
            b = xf.transform_location(ue.Vector(x, y, -300.0))
            hit = ue.SystemLibrary.line_trace_single(world, a, b, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], ue.DrawDebugTrace.NONE, True)
            t = hit.to_tuple() if hit else None
            if t and t[0]:
                p = xf.inverse_transform_location(t[4])
                row.append([x, round(p.z), t[9].get_name() if t[9] else None])
            else:
                row.append([x, None, None])
        rows[str(y)] = row
    out["profile"] = rows
(ROOT / "monke_island/Saved/T186/galleon-deck.json").write_text(json.dumps(out, indent=0, default=str), encoding="utf-8")
