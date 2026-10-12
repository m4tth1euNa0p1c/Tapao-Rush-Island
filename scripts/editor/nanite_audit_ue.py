"""Nanite audit of what the cinematics see (owner 2026-10-07: "le jeu lag lors des cinématiques"; the VSM warns of
non-Nanite meshes covering much of the shadow map): the galleon's meshes, and the static meshes of the expedition map
by instance count (foliage and ISMs included), with their Nanite flag, triangles and whether they cast shadows.
Writes monke_island/Saved/T186/nanite-audit.json. Action "naniteaudit"."""
import collections
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
out = {}


def info(mesh):
    if not mesh:
        return None
    nanite = mesh.get_editor_property("nanite_settings")
    return {"nanite": bool(nanite.get_editor_property("enabled")),
            "bounds": round(mesh.get_bounds().sphere_radius)}


rules = ue.EditorAssetLibrary.load_asset("/Game/MonkeIsland/Galleon/DA_GalleonRules")
out["galleon"] = {p: dict(path=rules.get_editor_property(p).get_path_name(), **(info(rules.get_editor_property(p)) or {}))
                  for p in ("hull_mesh", "sails_mesh", "rigging_mesh", "flag_mesh", "wheel_mesh", "rudder_mesh") if rules.get_editor_property(p)}
world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
counts = collections.Counter()
shadow = {}
for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
    for comp in actor.get_components_by_class(ue.StaticMeshComponent):
        mesh = comp.get_editor_property("static_mesh")
        if not mesh:
            continue
        n = comp.get_instance_count() if isinstance(comp, ue.InstancedStaticMeshComponent) else 1
        counts[mesh.get_path_name()] += n
        shadow[mesh.get_path_name()] = bool(comp.get_editor_property("cast_shadow"))
rows = []
for path, n in counts.most_common(60):
    mesh = ue.EditorAssetLibrary.load_asset(path.split(".")[0])
    i = info(mesh) or {}
    rows.append({"mesh": path.split("/")[-1].split(".")[0], "instances": n, "shadow": shadow[path], **i})
out["map"] = world.get_path_name()
out["non_nanite_top"] = [r for r in rows if not r.get("nanite")][:30]
out["nanite_top"] = [r for r in rows if r.get("nanite")][:15]
(ROOT / "monke_island/Saved/T186/nanite-audit.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
