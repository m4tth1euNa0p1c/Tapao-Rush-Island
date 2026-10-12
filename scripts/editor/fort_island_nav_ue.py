"""T-205: the fort island in L_Frag_Raraka_01: the level instance of LI_FortFrench named and filed, the navigation of
the island built (its FortFR_NavBounds, inside the instance; the garrison walks on it), the map saved.
Writes monke_island/Saved/T186/fort-island-nav.json. Action "fortnav" of scripts/editor/lead_session_t186.py.
"""
import datetime as dt
import json
import time
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
MAP = "/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01"
OUT = ROOT / "monke_island/Saved/T186/fort-island-nav.json"
report = {}
try:
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    assert world.get_path_name().startswith(MAP), f"open {MAP} first"
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    lis = [a for a in actors if a.get_class().get_name() == "LevelInstance"]
    fort = [a for a in lis if "LI_FortFrench" in str(a.get_editor_property("world_asset"))]
    assert len(fort) == 1, f"{len(fort)} fort instance(s): {[a.get_name() for a in lis]}"
    fort = fort[0]
    fort.set_actor_label("LI_FortFrench")
    fort.set_folder_path("Expedition/Sites")
    report["instance"] = [fort.get_name(), [round(v) for v in (fort.get_actor_location().x, fort.get_actor_location().y, fort.get_actor_location().z)]]
    ue.SystemLibrary.execute_console_command(world, "RebuildNavigation")
    start = time.time()
    time.sleep(0.5)
    report["nav_build_seconds"] = round(time.time() - start, 1)
    navs = [a.get_name() for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors() if "NavMesh" in a.get_class().get_name()]
    report["nav_actors"] = navs
    report["saved"] = bool(ue.EditorAssetLibrary.save_asset(MAP, only_if_is_dirty=False))
finally:
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
