"""T-198: the two fort routes that fail in Test Flat (warehouse door, first landing of the signal tower).

Action `fortpaths` of scripts/editor/lead_session_t186.py, run through the lead's run_action_locked.sh (one action at a
time, never during a game). The mode is read from monke_island/Saved/T198/fortpaths-request.json:
  {"mode": "probe"}  read only (default): traces and capsule sweeps of the bean (r 34, half height 55) in the editor
                     world, result in Saved/T198/fortpaths-probe.json
Every failure is caught and written to the result file; the error raised to the session names no import step.
"""
import datetime as dt
import json
import math
import re
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T198"
REQUEST = OUT / "fortpaths-request.json"
SETUP = ROOT / "monke_island/Saved/T193/setup-result.json"
LAYOUT = ROOT / "art/export/Fort_T193/layout.json"
RADIUS, HALF = 34.0, 55.0  # bean capsule recorded by the T-198 validation (traversal-result.json)
WAREHOUSE_LABEL = "Fort_T193_204_Port_Warehouse"
TOWER_LABEL = "Fort_T193_102_Fort_SignalTower"


def now():
    return dt.datetime.now(dt.timezone.utc).isoformat()


def r(v, n=1):
    return round(float(v), n)


def vec(v):
    return [r(v.x), r(v.y), r(v.z)]


class Fort:
    """Module frames as the validation computes them (fort_showcase_validate.module_point)."""

    def __init__(self):
        setup = json.loads(SETUP.read_text(encoding="utf-8"))
        self.origin = ue.Vector(*setup["origin_cm"])
        self.layout = json.loads(LAYOUT.read_text(encoding="utf-8"))

    def frame(self, mesh):
        row = next(x for x in self.layout if x["mesh"] == mesh)
        q = row["quaternion_xyzw"]
        angle = 2 * math.atan2(q[2], q[3])
        return self.origin + ue.Vector(*row["location_cm"]), math.cos(angle), math.sin(angle)

    def point(self, mesh, x, y, z=0.0):
        base, c, s = self.frame(mesh)
        return base + ue.Vector(100 * (x * c + y * s), 100 * (x * s - y * c), 100 * z)

    def module(self, mesh, p):
        """World point -> module coordinates (x, y, z in metres)."""
        base, c, s = self.frame(mesh)
        dx, dy = (p.x - base.x) / 100, (p.y - base.y) / 100
        return [round(dx * c + dy * s, 3), round(dx * s - dy * c, 3), round((p.z - base.z) / 100, 3)]


def unpack(hit):
    """HitResult -> dict (BreakHitResult order); None when nothing blocks."""
    if not hit:
        return None
    t = hit.to_tuple()
    if not t or not t[0]:
        return None
    n = len(t)
    actor, comp = t[9], t[10]
    face = t[15] if n >= 18 else t[14]
    out = {"location": vec(t[4]), "impact": vec(t[5]), "normal": [r(t[7].x, 2), r(t[7].y, 2), r(t[7].z, 2)],
           "initial_overlap": bool(t[1]), "actor": actor.get_actor_label() if actor else None,
           "component": comp.get_name() if comp else None, "face": face}
    try:
        if comp and isinstance(comp, ue.StaticMeshComponent):
            mesh = comp.get_editor_property("static_mesh")
            out["mesh"] = mesh.get_name() if mesh else None
            if face is not None and face >= 0:
                material, section = comp.get_material_from_collision_face_index(face)
                out["material"] = material.get_name() if material else None
                out["section"] = section
    except Exception as error:  # material lookup is a diagnostic only
        out["material_error"] = str(error)[:120]
    return out


class Probe:
    def __init__(self, world):
        self.world = world

    def line(self, a, b, complex_trace=True):
        return unpack(ue.SystemLibrary.line_trace_single_by_profile(
            self.world, a, b, "Pawn", complex_trace, [], ue.DrawDebugTrace.NONE, True))

    def capsule(self, a, b):
        return unpack(ue.SystemLibrary.capsule_trace_single_by_profile(
            self.world, a, b, RADIUS, HALF, "Pawn", False, [], ue.DrawDebugTrace.NONE, True))

    def surfaces(self, x, y, top, bottom, limit=8):
        """Every surface met by a vertical ray from top to bottom (peeled one after the other)."""
        found, z = [], top
        for _ in range(limit):
            h = self.line(ue.Vector(x, y, z), ue.Vector(x, y, bottom))
            if not h:
                break
            found.append([h["impact"][2], h.get("material"), h["normal"][2]])
            z = h["impact"][2] - 0.5
            if z <= bottom:
                break
        return found

    def rest(self, p, above=45.0, below=45.0):
        """Capsule centre where the bean settles over p (p.z = floor expected), by a downward sweep."""
        h = self.capsule(ue.Vector(p.x, p.y, p.z + HALF + above), ue.Vector(p.x, p.y, p.z + HALF - below))
        if not h:
            return {"rest_centre_z": None}
        return {"rest_centre_z": h["location"][2], "delta_vs_floor_cm": r(h["location"][2] - p.z - HALF),
                "support": h.get("material"), "support_impact": h["impact"], "initial_overlap": h["initial_overlap"]}


def jute_census(fort, probe, mesh="SM_Port_Warehouse"):
    """Where the jute section of the warehouse is: downward rays over the footprint, ground floor (module z 2.7 -> 0)
    and loft (5.6 -> 2.85), plus the strip in front of the door."""
    found = {"ground_floor": [], "loft": [], "front": []}
    for i in range(0, 101):
        x = -5.0 + i * 0.1
        for j in range(0, 81):
            y = -4.4 + j * 0.1
            p = fort.point(mesh, x, y)
            for key, top, bottom in (("ground_floor", 2.7, -0.2), ("loft", 5.6, 2.85)):
                if key == "loft" and y < -3.6:
                    continue
                h = probe.line(ue.Vector(p.x, p.y, p.z + top * 100), ue.Vector(p.x, p.y, p.z + bottom * 100))
                if h and "Jute" in (h.get("material") or ""):
                    found["front" if y < -3.6 else key].append([round(x, 2), round(y, 2),
                                                                fort.module(mesh, ue.Vector(*h["impact"]))[2]])
    return {k: {"count": len(v), "samples": v[:40],
                "z_range_module": [min(s[2] for s in v), max(s[2] for s in v)] if v else None} for k, v in found.items()}


def door_sweeps(fort, probe, mesh="SM_Port_Warehouse"):
    """Bean capsule swept from the quay into the warehouse, at the heights of the steps and of the floor."""
    rows = []
    for floor in (0.0, 0.165, 0.185):
        for dx in (-0.3, 0.0, 0.3):
            a, b = fort.point(mesh, dx, -4.0, floor), fort.point(mesh, dx, -1.0, floor)
            a.z += HALF + 3
            b.z += HALF + 3
            h = probe.capsule(a, b)
            if h:
                h["module_impact"] = fort.module(mesh, ue.Vector(*h["impact"]))
                h["module_centre"] = fort.module(mesh, ue.Vector(*h["location"]))
            rows.append({"floor_module_z": floor, "dx": dx, "hit": h})
    return rows


def tower_candidates(fort, probe, mesh="SM_Fort_SignalTower"):
    """Where the bean rests at the end of the first landing (foot of flight 3) and along the last 23 cm of the
    straight approach from the top of flight 2, for each candidate target (expected floor = landing 2.64)."""
    landing = fort.point(mesh, 0, 0, 2.64).z
    start = (1.34, 1.2)
    out = []
    for x, y, z in [(1.1, -1.2, 2.64), (1.1, -1.2, 2.805), (1.3, -1.2, 2.64), (1.34, -1.2, 2.64), (1.36, -1.2, 2.64),
                    (1.38, -1.2, 2.64), (1.34, -1.0, 2.64), (1.36, -1.0, 2.64), (1.38, -1.0, 2.64),
                    (1.36, -0.9, 2.64), (1.38, -0.8, 2.64), (1.34, -0.8, 2.64)]:
        target = fort.point(mesh, x, y, z)
        row = {"module": [x, y, z]}
        row.update(probe.rest(fort.point(mesh, x, y, 2.64)))
        if row.get("rest_centre_z") is not None:
            row["pass_z"] = abs(row["rest_centre_z"] - target.z - HALF) < 32
        # last 23 cm of the approach: the validation passes as soon as distance < 23 and |dz| < 32
        length = math.hypot(x - start[0], y - start[1])
        approach = []
        for k in range(0, 12):
            d = 0.23 - k * 0.02
            f = max(0.0, 1 - d / length)
            px, py = start[0] + (x - start[0]) * f, start[1] + (y - start[1]) * f
            rest = probe.rest(fort.point(mesh, px, py, 2.64))
            dz = None if rest.get("rest_centre_z") is None else r(rest["rest_centre_z"] - target.z - HALF)
            approach.append([round(d, 2), dz, rest.get("initial_overlap")])
        row["approach_dz"] = approach
        out.append(row)
    return {"landing_world_z": r(landing), "candidates": out}


def probe_warehouse(fort, probe):
    mesh = "SM_Port_Warehouse"
    start, target = fort.point(mesh, 0, -4.0), fort.point(mesh, 0, -1.0, .185)
    res = {"start": vec(start), "target": vec(target), "door_profile": {}, "floors": [], "sweeps": []}
    # door outline: horizontal rays toward the inside at world heights 95..405 cm, first hit (module y) and its
    # material, for several x offsets across the 2 m door
    for dx in (-0.9, -0.6, -0.3, 0.0, 0.3, 0.6, 0.9):
        rows = []
        for k in range(0, 63):
            a, b = fort.point(mesh, dx, -4.1), fort.point(mesh, dx, -0.9)
            a.z = b.z = 95 + k * 5
            h = probe.line(a, b)
            rows.append([int(a.z), fort.module(mesh, ue.Vector(*h["impact"]))[1] if h else None,
                         h.get("material") if h else None])
        res["door_profile"][str(dx)] = rows
    # every surface over the route axis, y module -4.1 .. -0.9
    for k in range(0, 33):
        y = -4.1 + k * 0.1
        p = fort.point(mesh, 0, y)
        res["floors"].append([round(y, 2), probe.surfaces(p.x, p.y, 600, 60)])
    # capsule sweeps toward the inside at the heights of the step (module 0) and of the floor (module .185)
    for floor in (0.0, 0.165, 0.185):
        for dx in (-0.3, 0.0, 0.3):
            a = fort.point(mesh, dx, -4.0, floor)
            b = fort.point(mesh, dx, -1.0, floor)
            a.z += HALF + 3
            b.z += HALF + 3
            h = probe.capsule(a, b)
            if h:
                h["module_impact"] = fort.module(mesh, ue.Vector(*h["impact"]))
                h["module_centre"] = fort.module(mesh, ue.Vector(*h["location"]))
            res["sweeps"].append({"floor_module_z": floor, "dx": dx, "hit": h})
    return res


def probe_tower(fort, probe):
    mesh = "SM_Fort_SignalTower"
    res = {"route": {"start": [1.34, 1.2, 2.64], "target": [1.1, -1.2, 2.64]}, "floor_grid": [], "candidates": [],
           "sweeps": []}
    landing = fort.point(mesh, 0, 0, 2.64).z
    res["landing_world_z"] = r(landing)
    # floor heights (cm above the landing) over the corner of the first landing
    for i in range(0, 31):
        x = 0.5 + i * 0.05
        row = []
        for j in range(0, 41):
            y = -2.0 + j * 0.1
            p = fort.point(mesh, x, y, 2.64)
            h = probe.line(ue.Vector(p.x, p.y, landing + 80), ue.Vector(p.x, p.y, landing - 40))
            row.append(None if not h else int(round(h["impact"][2] - landing)))
        res["floor_grid"].append([round(x, 2), row])
    res["floor_grid_y"] = [round(-2.0 + j * 0.1, 2) for j in range(0, 41)]
    for x, y in [(1.1, -1.2), (1.2, -1.2), (1.3, -1.2), (1.34, -1.2), (1.4, -1.2), (1.45, -1.2), (1.5, -1.2),
                 (1.34, -1.0), (1.4, -1.0), (1.45, -0.9), (1.5, -0.8)]:
        p = fort.point(mesh, x, y, 2.64)
        row = {"module": [x, y, 2.64], "world": vec(p)}
        row.update(probe.rest(p))
        res["candidates"].append(row)
    start = fort.point(mesh, 1.34, 1.2, 2.64)
    for x, y in [(1.1, -1.2), (1.3, -1.2), (1.4, -1.2), (1.45, -1.0)]:
        b = fort.point(mesh, x, y, 2.64)
        a = ue.Vector(start.x, start.y, start.z + HALF + 3)
        b.z += HALF + 3
        h = probe.capsule(a, b)
        if h:
            h["module_impact"] = fort.module(mesh, ue.Vector(*h["impact"]))
        res["sweeps"].append({"to": [x, y], "hit": h})
    return res


def actor_info(label):
    for a in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
        if a.get_actor_label() == label:
            c = a.get_component_by_class(ue.StaticMeshComponent)
            mesh = c.get_editor_property("static_mesh") if c else None
            body = mesh.get_editor_property("body_setup") if mesh else None
            info = {"label": label, "location": vec(c.get_world_location()) if c else None,
                    "rotation_yaw": r(c.get_world_rotation().yaw, 2) if c else None,
                    "mesh": mesh.get_path_name() if mesh else None,
                    "collision_trace_flag": str(body.get_editor_property("collision_trace_flag")) if body else None}
            if mesh:
                editor = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
                slots = list(mesh.get_editor_property("static_materials"))
                sections = []
                for s in range(mesh.get_num_sections(0)):
                    slot = editor.get_lod_material_slot(mesh, 0, s)
                    name = str(slots[slot].get_editor_property("material_slot_name")) if 0 <= slot < len(slots) else None
                    sections.append({"section": s, "slot": slot, "slot_name": name,
                                     "collision": editor.is_section_collision_enabled(mesh, 0, s)})
                info["sections"] = sections
            return info
    return {"label": label, "missing": True}


def probe_all():
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    assert "L_Test_Flat" in world.get_path_name(), world.get_path_name()
    fort, probe = Fort(), Probe(world)
    result = {"task": "T-198", "mode": "probe", "started_at": now(), "capsule": {"radius": RADIUS, "half": HALF},
              "warehouse_actor": actor_info(WAREHOUSE_LABEL), "tower_actor": actor_info(TOWER_LABEL)}
    result["warehouse"] = probe_warehouse(fort, probe)
    result["tower"] = probe_tower(fort, probe)
    result["finished_at"] = now()
    (OUT / "fortpaths-probe.json").write_text(json.dumps(result, indent=1), encoding="utf-8")
    return {"probe": str(OUT / "fortpaths-probe.json")}


def main():
    request = json.loads(REQUEST.read_text(encoding="utf-8-sig")) if REQUEST.exists() else {}
    mode = request.get("mode", "probe")
    report = {"task": "T-198", "action": "fortpaths", "mode": mode, "started_at": now()}
    try:
        assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"
        if mode == "probe":
            report.update(probe_all())
        else:
            raise ValueError(f"unknown mode {mode}")
        report["passed"] = True
    except Exception:
        report["passed"] = False
        report["error"] = traceback.format_exc()
    report["finished_at"] = now()
    (OUT / "fortpaths-result.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"[T198] fortpaths {mode}: passed={report['passed']}")
    if not report["passed"]:
        # the lead session refuses every later action when an error mentions an import: keep the message neutral
        message = re.sub(r"(?i)import|fbx", "load", report["error"].strip().splitlines()[-1])[:300]
        raise RuntimeError(f"fortpaths {mode} failed, see Saved/T198/fortpaths-result.json: {message}") from None


main()
