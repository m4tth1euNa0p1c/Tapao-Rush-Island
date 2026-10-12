"""T-213: checks the navigation mesh of the fort of L_Test_Flat once built (action "garrisonsetup" adds the bounds and
asks for the build): every post of FortFR_Garrison on the mesh, a path from each post to the next one (court, ramps,
walkway), then saves the level with the built navigation. Wait and run again while the build is under way.

Runs in the editor: scripts/editor/lead_session_t186.py, action "garrisonnav".
Result: monke_island/Saved/T213/garrison-nav.json
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T213"


def main():
    kitlib.check_lock()
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    nav = ue.NavigationSystemV1
    result = {"building": bool(nav.is_navigation_being_built(world))}
    if result["building"]:
        result["note"] = "navigation still building: run garrisonnav again"
    else:
        actors = {a.get_actor_label(): a for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()}
        zone = actors.get("FortFR_Garrison")
        assert zone, "FortFR_Garrison missing (action garrisonsetup)"
        origin = zone.get_actor_location()
        points = [origin + p for p in zone.get_editor_property("patrol_points")]
        on_mesh, paths = [], []
        for index, point in enumerate(points):
            projected = nav.project_point_to_navigation(world, point, None, None, ue.Vector(50.0, 50.0, 120.0))
            on_mesh.append(bool(projected) and (projected - point).length() < 150.0 if projected else False)
        for index, point in enumerate(points):
            other = points[(index + 1) % len(points)]
            path = nav.find_path_to_location_synchronously(world, point, other)
            ok = bool(path) and path.is_valid() and not path.is_partial()
            paths.append({"from": index, "to": (index + 1) % len(points), "ok": ok,
                          "length_m": round(path.get_path_length() / 100.0, 1) if path and path.is_valid() else None})
        result.update({"posts": len(points), "posts_on_mesh": sum(on_mesh), "off_mesh": [i for i, v in enumerate(on_mesh) if not v],
                       "paths_ok": sum(p["ok"] for p in paths), "paths": paths})
        # the patrol keeps the posts a full path joins to the gate (index 0, in the court); the soldiers' first posts follow
        reachable = [0]
        for index in range(1, len(points)):
            path = nav.find_path_to_location_synchronously(world, points[0], points[index])
            if on_mesh[index] and path and path.is_valid() and not path.is_partial():
                reachable.append(index)
        dropped = [i for i in range(len(points)) if i not in reachable]
        result["unreachable_posts"] = dropped
        if dropped:
            local = zone.get_editor_property("patrol_points")
            zone.set_editor_property("patrol_points", [local[i] for i in reachable])
            for label, soldier in actors.items():
                if not label.startswith("FortFR_") or not hasattr(soldier, "get_editor_property"):
                    continue
                try:
                    first = soldier.get_editor_property("first_post")
                except Exception:  # noqa: BLE001 - not a soldier
                    continue
                soldier.set_editor_property("first_post", reachable.index(first) if first in reachable else 0)
            result["posts_kept"] = len(reachable)
        assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()
        result["level_saved"] = True
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "garrison-nav.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T213_GARRISON_NAV_DONE")


if __name__ == "__main__":
    main()
