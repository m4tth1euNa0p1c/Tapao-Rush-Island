"""PIE probe of the galleon for the execution scene: a grid of traces down through the hull (hull frame, cm) giving the
height of the first thing hit (deck ~162, rail ~240, masts and rigging much higher), and the places of the merchant,
the squad, the condemned and the scene's components. Writes monke_island/Saved/T186/exec-probe.json."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(r"C:/Users/matth/monk-island")
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
ship = next(iter(ue.GameplayStatics.get_all_actors_of_class(world, ue.GalleonActor)), None)
hull = ship.get_hull()
xf = hull.get_world_transform()


def local(v):
    p = xf.inverse_transform_location(v)
    return [round(p.x), round(p.y), round(p.z)]


out = {"hull_yaw": round(xf.rotation.rotator().yaw, 1)}
ignore = [a for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Character)]
grid = {}
for y in range(-700, 750, 50):
    row = []
    for x in range(-300, 325, 25):
        a = xf.transform_location(ue.Vector(x, y, 2500.0))
        b = xf.transform_location(ue.Vector(x, y, 100.0))
        hit = ue.SystemLibrary.line_trace_single(world, a, b, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore, ue.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        row.append(round(xf.inverse_transform_location(t[4]).z) if t and t[0] else None)
    grid[str(y)] = row
out["grid_x"] = list(range(-300, 325, 25))
out["grid"] = grid
rows = []
for a in ue.GameplayStatics.get_all_actors_of_class(world, ue.Actor):
    cls = a.get_class().get_name()
    if any(k in cls for k in ("Merchant", "Shopkeeper", "BeanFR", "IslanderBean", "ExecutionScene")):
        o, e = a.get_actor_bounds(True)
        rows.append({"name": a.get_name(), "at": local(a.get_actor_location()), "box_centre": local(o), "extent": [round(e.x), round(e.y), round(e.z)]})
out["actors"] = rows
(ROOT / "monke_island/Saved/T186/exec-probe.json").write_text(json.dumps(out, indent=0), encoding="utf-8")
