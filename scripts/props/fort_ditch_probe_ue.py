"""Probe of the fort's dry ditch collision in L_Test_Flat (T-210): simple collision of SM_Fort_Ditch (T198 revision)
and of the site terrain, and the collision settings of their actors. Writes Saved/T210/probe.json."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T210"
OUT.mkdir(parents=True, exist_ok=True)
report = {}
for path in ("/Game/MonkeIsland/Environment/FortFrench/Revisions/T198/Meshes/SM_Fort_Ditch",
             "/Game/MonkeIsland/Environment/FortFrench/Meshes/SM_Fort_Ditch",
             "/Game/MonkeIsland/Environment/FortFrench/Meshes/SM_Fort_TerrainNear"):
    mesh = ue.EditorAssetLibrary.load_asset(path) if ue.EditorAssetLibrary.does_asset_exist(path) else None
    if not mesh:
        report[path] = "missing"
        continue
    body = mesh.get_editor_property("body_setup")
    geom = body.get_editor_property("agg_geom") if body else None
    report[path] = {"convex": len(geom.get_editor_property("convex_elems")) if geom else None,
                    "boxes": len(geom.get_editor_property("box_elems")) if geom else None,
                    "flag": str(body.get_editor_property("collision_trace_flag")) if body else None,
                    "bounds": [round(v) for v in (mesh.get_bounds().box_extent.x, mesh.get_bounds().box_extent.y, mesh.get_bounds().box_extent.z)]}
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    label = actor.get_actor_label()
    if "Ditch" in label or "Terrain" in label or "Moat" in label:
        comp = actor.get_component_by_class(ue.StaticMeshComponent)
        mesh = comp.get_editor_property("static_mesh") if comp else None
        report[label] = {"mesh": mesh.get_path_name() if mesh else None,
                         "profile": str(comp.get_collision_profile_name()) if comp else None,
                         "enabled": str(comp.get_collision_enabled()) if comp else None,
                         "location": [round(v) for v in (actor.get_actor_location().x, actor.get_actor_location().y, actor.get_actor_location().z)]}
(OUT / "probe.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
ue.log(f"[T210] {json.dumps(report)}")
