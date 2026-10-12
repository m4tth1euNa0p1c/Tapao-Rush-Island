"""Pickups that fall through the ground (v1.8 check of 2026-10-10: the machine gun's ammo boxes, BP_Ammo_MGBox, ended
26 m under L_Test_Flat). An AAmmoPickup or AInventoryPickup simulates its pickup mesh, and a simulated body needs simple
collision: a mesh with none (or set to "complex as simple") has no physics shape and drops through everything.

Every PickupMesh of the ammo and item definitions under /Game/MonkeIsland is checked. One without simple collision gets
a box fitted to its bounds; "complex as simple" goes back to the project default. The meshes changed are saved.
Lead session action "pickupcollision" (editor open, no game running). Report: monke_island/Saved/T273/pickup-collision.json.
Replays change nothing once every mesh has its collision.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T273/pickup-collision.json"
ROOT = "/Game/MonkeIsland"
CLASSES = {"AmmoDefinition", "InventoryItemDefinition"}
EAL = ue.EditorAssetLibrary


def definitions():
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    found = []
    for data in registry.get_assets_by_path(ROOT, recursive=True):
        if str(data.asset_class_path.asset_name) in CLASSES:
            found.append(str(data.package_name))
    return sorted(found)


def main():
    meshes = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    report = {"definitions": 0, "meshes": {}, "fixed": [], "issues": []}
    seen = {}
    for path in definitions():
        asset = EAL.load_asset(path)
        if not asset:
            report["issues"].append(f"{path}: not loaded")
            continue
        report["definitions"] += 1
        mesh = asset.get_editor_property("pickup_mesh")
        if mesh:
            seen.setdefault(mesh.get_path_name().split(".")[0], (mesh, []))[1].append(path.rsplit("/", 1)[-1])
    for mesh_path, (mesh, users) in sorted(seen.items()):
        entry = {"used_by": users}
        body = mesh.get_editor_property("body_setup")
        simple = meshes.get_simple_collision_count(mesh)
        convex = meshes.get_convex_collision_count(mesh)
        flag = body.get_editor_property("collision_trace_flag") if body else None
        entry["before"] = {"simple": simple, "convex": convex, "trace_flag": str(flag)}
        changed = False
        if body and flag == ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE:
            body.set_editor_property("collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_DEFAULT)
            changed = True
        if simple + convex == 0:
            index = meshes.add_simple_collisions(mesh, ue.ScriptCollisionShapeType.BOX)
            entry["box_added"] = index
            changed = changed or index >= 0
            if index < 0:
                report["issues"].append(f"{mesh_path}: the box collision could not be added")
        if changed:
            entry["saved"] = bool(EAL.save_loaded_asset(mesh, only_if_is_dirty=False))
            entry["after"] = {"simple": meshes.get_simple_collision_count(mesh), "convex": meshes.get_convex_collision_count(mesh),
                              "trace_flag": str(body.get_editor_property("collision_trace_flag")) if body else None}
            report["fixed"].append(mesh_path)
        report["meshes"][mesh_path] = entry
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"PICKUP_COLLISION fixed={len(report['fixed'])} meshes={len(report['meshes'])} issues={len(report['issues'])}")


main()
