"""Probe for the French garrison of the Test Flat fort (owner request 2026-10-07): navigation volumes, the fort's
footprint and ground, the weapon grips the player's musket and cutlass already use, the sockets of the musket.

Runs in the editor: scripts/editor/lead_session_t186.py, action "garrisonprobe". Read only (no save).
Result: monke_island/Saved/T213/garrison-probe.json
"""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T213"
ORIGIN = ue.Vector(-66000.0, 0.0, 115.0)          # fort origin in L_Test_Flat (fort_showcase_setup.py)


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def xf(t):
    r = t.rotation.rotator()
    return {"location": vec(t.translation), "rotation_pyr": [round(r.pitch, 2), round(r.yaw, 2), round(r.roll, 2)],
            "scale": vec(t.scale3d)}


def fort_point(x, y, z=0.0):
    return ORIGIN + ue.Vector(x * 100.0, -y * 100.0, z * 100.0)


def ground(world, point):
    start = point + ue.Vector(0.0, 0.0, 1500.0)
    end = point - ue.Vector(0.0, 0.0, 1500.0)
    hit = ue.SystemLibrary.line_trace_single(world, start, end, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [],
                                             ue.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    actor = t[9] if len(t) > 9 else None
    return {"z": round(t[4].z, 1), "actor": actor.get_actor_label() if actor else None}


def main():
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
    result = {"level": world.get_path_name(), "nav": [], "fort": {}, "ground": {}, "grips": {}, "sockets": {}}
    for a in actors:
        cls = a.get_class().get_name()
        if cls in ("NavMeshBoundsVolume", "RecastNavMesh", "NavModifierVolume", "NavigationData"):
            origin, extent = a.get_actor_bounds(False)
            result["nav"].append({"label": a.get_actor_label(), "class": cls, "center": vec(origin), "extent": vec(extent)})
    fort = [a for a in actors if a.get_actor_label().startswith(("Fort_", "Showcase_Fort", "FortFrench"))
            or "FortFrench" in str(a.get_folder_path())]
    lo, hi = None, None
    for a in fort:
        origin, extent = a.get_actor_bounds(False)
        if extent.x > 20000 or extent.y > 20000:
            continue
        a0, a1 = origin - extent, origin + extent
        lo = a0 if lo is None else ue.Vector(min(lo.x, a0.x), min(lo.y, a0.y), min(lo.z, a0.z))
        hi = a1 if hi is None else ue.Vector(max(hi.x, a1.x), max(hi.y, a1.y), max(hi.z, a1.z))
    result["fort"] = {"actors": len(fort), "min": vec(lo) if lo else None, "max": vec(hi) if hi else None,
                      "labels": sorted({a.get_actor_label().split("_")[0] + "_" + a.get_actor_label().split("_")[1]
                                        for a in fort if "_" in a.get_actor_label()})[:40]}
    # the court (x -20..20, y -16..16 m), the gate, the ramps and the walkway (2.97 m) of the maquette
    probes = {
        "court_center": (0, 0), "cistern_south": (0, -5), "gate_inside": (0, -17.5), "gate_outside": (0, -23.4),
        "court_sw": (-17, -13), "court_se": (17, -13), "court_nw": (-17, 13), "court_ne": (17, 13),
        "barracks_front": (-10.5, 0), "governor_front": (0, 7.5), "chapel_front": (14, 7.5), "prison_door": (11, -6.7),
        "kitchen_front": (-6, -7.5), "ramp_w_foot": (-19, -3.5), "ramp_w_top": (-19, 12), "ramp_e_foot": (19, 3.5),
        "ramp_e_top": (19, -12), "walk_n": (0, 22), "walk_s": (-10, -17.6), "walk_w": (-22, 0), "walk_e": (22, 0),
        "bastion_ne": (26, 22), "bastion_sw": (-26, -22), "ditch_s": (10, -30), "glacis_s": (0, -38),
    }
    for name, (x, y) in probes.items():
        result["ground"][name] = {"fort_m": [x, y], "ue": vec(fort_point(x, y)), "hit": ground(world, fort_point(x, y))}
    musket_rules = ue.load_asset("/Game/MonkeIsland/Weapons/Musket/DA_MusketRules")
    cutlass_rules = ue.load_asset("/Game/MonkeIsland/Weapons/Cutlass/DA_CutlassRules")
    if musket_rules:
        result["grips"]["musket_hand_offset"] = xf(musket_rules.get_editor_property("hand_offset"))
        result["grips"]["musket_hand_bone"] = str(musket_rules.get_editor_property("hand_bone"))
        mesh = musket_rules.get_editor_property("musket_mesh")
        result["grips"]["musket_mesh"] = mesh.get_path_name() if mesh else None
        for name in ("flash_mesh", "flash_material", "smoke_mesh", "smoke_material", "shot_sound"):
            try:
                v = musket_rules.get_editor_property(name)
                result["grips"]["musket_" + name] = v.get_path_name() if v else None
            except Exception as e:  # noqa: BLE001
                result["grips"]["musket_" + name] = f"? {e}"
    if cutlass_rules:
        for name in ("library_grip_in_hand", "scabbard_rest", "scabbard_drawn"):
            result["grips"]["cutlass_" + name] = xf(cutlass_rules.get_editor_property(name))
        for name in ("cutlass_mesh", "scabbard_mesh"):
            v = cutlass_rules.get_editor_property(name)
            result["grips"]["cutlass_" + name] = v.get_path_name() if v else None
    for path in ("/Game/MonkeIsland/Weapons/Musket/SK_Bean_Musket", "/Game/MonkeIsland/Weapons/Musket/Static/SM_Bean_Musket",
                 "/Game/MonkeIsland/Weapons/Cutlass/SM_Bean_Cutlass", "/Game/MonkeIsland/Weapons/Cutlass/SM_Bean_Scabbard"):
        asset = ue.load_asset(path)
        if not asset:
            result["sockets"][path] = None
            continue
        names = []
        if isinstance(asset, ue.SkeletalMesh):
            names = [str(asset.get_socket_by_index(i).get_editor_property("socket_name")) for i in range(asset.num_sockets())]
            box = asset.get_bounds()
            result["sockets"][path] = {"sockets": sorted(set(names)), "bounds_extent": vec(box.box_extent), "origin": vec(box.origin)}
        else:
            names = []
            for candidate in ("muzzle", "socket_muzzle", "Muzzle", "SOCKET_Muzzle", "grip", "socket_grip_r"):
                if asset.find_socket(candidate):
                    names.append(candidate)
            box = asset.get_bounding_box()
            result["sockets"][path] = {"sockets": names, "min": vec(box.min), "max": vec(box.max)}
    skel = ue.load_asset("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton")
    body = ue.load_asset("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body")
    if body:
        result["bean_sockets"] = [str(body.get_socket_by_index(i).get_editor_property("socket_name")) for i in range(body.num_sockets())]
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "garrison-probe.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T213_GARRISON_PROBE_DONE")


if __name__ == "__main__":
    main()
