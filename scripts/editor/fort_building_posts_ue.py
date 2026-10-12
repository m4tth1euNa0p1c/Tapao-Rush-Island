"""T-332 (owner 2026-10-11: "fix le fort lorsqu'il apparaît sur raraka", "je veux plus de soldats dans les bâtiments"):
the land fort level LI_FortFrench_Land (streamed by AFortSite on Lvl_Raraka and on the Raraka section of L_Test_Flat).

Steps, in this order (monke_island/Saved/T332/fort-building-posts-request.json, optional:
{"steps": ["navdata", "posts"], "levels": ["land"], "dry_run": false, "restore_level": true}; without the file the
defaults written here run):
- navdata: the RecastNavMesh saved inside LI_FortFrench_Land (RecastNavMesh-Default, found in the package since the save
  of 2026-10-08 19:22; fort_land_ue.py had removed it) goes. A navmesh saved in a streamed level is discarded by the
  engine (ARecastNavMesh::CheckToDiscardSubLevelNavData): in the packaged v2.0 every fort of Lvl_Raraka logged
  "NavData RegistrationFailed_DataPendingKill (RecastNavMesh-Default)" and got no navmesh at all (its soldiers walked
  straight into the walls: 50 "is stuck and failed to move" in 2 min), where v1.4 logged "SpawnMissingNavigationData ...
  Spawning and rebuilding at runtime". The NavMeshBoundsVolume FortFR_NavBounds stays (the runtime build uses it).
- posts: the guard posts of the buildings (AFortGarrisonZone.BuildingPosts of FortFR_Garrison): for each building of the
  maquette fort-francais (guardhouse, barracks, governor's house, kitchen, chapel, prison corridor, signal tower,
  treasure vault, powder magazine enclosure), points of its interior floors are probed in the editor world: a walkable
  floor at the expected height, a roof above (rooms), room for a bean capsule (radius 45 > 30), inside the outline of
  the zone; the chosen ones (a few per building, 1.5 m apart, seeing their door when they can) face the door of their
  room (outward on a terrace or a gallery). The guards of the day stand there (DA_FortSoldierRules.BuildingGuardsByDay).
Levels: "land" (LI_FortFrench_Land, default) and "showcase" (the fort of the persistent level of L_Test_Flat, on its
island at -660 m: optional, only "posts" applies there). The level is saved once after its steps (not in a dry run).

Needs the C++ of T-332 built (FFortBuildingPost, AFortGarrisonZone::BuildingPosts). Never during a game (the editor
lock and no PIE: minka_doors_ue.check_lock). Idempotent: the posts are computed again and replace the old ones.
Result: monke_island/Saved/T332/fort-building-posts.json. Action "fortbuildingposts" of scripts/editor/lead_session_t186.py.
"""
import datetime as dt
import json
import math
import sys
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT_DIR = ROOT / "monke_island/Saved/T332"
REQUEST = OUT_DIR / "fort-building-posts-request.json"
OUT = OUT_DIR / "fort-building-posts.json"
LEVELS = {"land": "/Game/MonkeIsland/Maps/Fragments/LI_FortFrench_Land", "showcase": "/Game/MonkeIsland/Maps/Test/L_Test_Flat"}
ZONE_LABEL = "FortFR_Garrison"
DEFAULTS = {"steps": ["navdata", "posts"], "levels": ["land"], "dry_run": False, "restore_level": True}

# the bean soldier: capsule 30 x 55 (AFortSoldier); a guard needs a little more room than that to stand and turn
PROBE_RADIUS = 45.0
HALF_HEIGHT = 55.0
FLOOR_TOLERANCE = 40.0      # cm between the floor found and the floor of the maquette (slabs without collision: 16.5 cm)
MIN_WALKABLE_NZ = 0.75      # normal of a walkable floor
SPACING = 150.0             # cm between two posts of a building (same floor)
GRID = 0.4                  # m between two probed points
ROOF_REACH = 600.0          # cm above the floor a roof must be found in a room

# Buildings of the maquette fort-francais (params.modules / scene_layout), module frame in metres: x right, y forward,
# the door on the -y side; Unreal's actor frame is (100 x, -100 y, 100 z). Regions: (x0, x1, y0, y1, floor_z, roofed).
# "door": door of the rooms (module metres) the guards face; None: they face out from the building's centre.
BUILDINGS = [
    {"name": "guardhouse", "mesh": "SM_Fort_GuardHouse", "max": 2, "door": [(0.0, -2.6)],
     "regions": [(-2.6, 2.6, -1.6, 1.6, 0.165, True)]},
    {"name": "barracks", "mesh": "SM_Fort_Barracks", "max": 3, "door": [(-4.0, -3.1), (4.0, -3.1)],
     "regions": [(-7.2, 7.2, -2.2, 2.2, 0.165, True)]},
    {"name": "governor", "mesh": "SM_Fort_GovernorHouse", "max": 3, "door": [(0.0, -5.6)],
     "regions": [(-6.2, 6.2, -2.9, 2.9, 0.495, True), (-6.2, 6.2, -2.9, 2.9, 3.135, True), (-6.4, 6.4, -5.3, -3.7, 3.135, False)]},
    {"name": "prison", "mesh": "SM_Fort_Prison", "max": 1, "door": [(-1.8, -3.5)],
     "regions": [(-2.3, -1.3, -2.7, 2.7, 0.0, True)]},
    {"name": "tower", "mesh": "SM_Fort_SignalTower", "max": 2, "door": [(0.0, -2.6)],
     "regions": [(-1.4, 1.4, -1.4, 1.4, 2.64, True), (-1.5, 1.5, -1.5, 1.5, 5.28, False)]},
    {"name": "chapel", "mesh": "SM_Fort_Chapel", "max": 1, "door": [(0.0, -4.1)],
     "regions": [(-1.9, 1.9, -3.3, 3.3, 0.33, True)]},
    {"name": "kitchen", "mesh": "SM_Fort_Kitchen", "max": 1, "door": [(0.0, -2.35)],
     "regions": [(-2.4, 2.4, -1.6, 1.6, 0.165, True)]},
    {"name": "magazine", "mesh": "SM_Fort_PowderMagazine", "max": 1, "door": None,
     "regions": [(-3.6, 3.6, -3.3, -2.5, 0.0, False)]},
    # the treasure vault under the south-west bastion: its corridor and chamber are not in the maquette's frame, the
    # bounds of the actor are probed at the height of its floor (66 cm in the fort's frame, the actor's pivot)
    {"name": "vault", "mesh": "SM_Fort_TreasureVault", "max": 2, "door_mesh": "SM_Fort_VaultDoor", "bounds_floor": (-30.0, 60.0)},
]
report = {"task": "T-332", "started_at": dt.datetime.now(dt.timezone.utc).isoformat()}


# ------------------------------------------------------------------------------------------------ editor helpers

def eas():
    return ue.get_editor_subsystem(ue.EditorActorSubsystem)


def editor_world():
    return ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()


def world_path():
    world = editor_world()
    return world.get_path_name() if world else ""


def load_level(path):
    if not world_path().startswith(path):
        assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(path), f"cannot load {path}"
    assert world_path().startswith(path), f"{path} not open ({world_path()})"


def vec(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def mesh_name(actor):
    comp = actor.get_component_by_class(ue.StaticMeshComponent)
    mesh = comp.get_editor_property("static_mesh") if comp else None
    return mesh.get_name() if mesh else None


def unpack(hit):
    """HitResult -> (impact point, impact normal, initial overlap, actor label); None when nothing blocks
    (BreakHitResult order, as scripts/editor/fort_paths_fix_ue.py reads it)"""
    if not hit:
        return None
    t = hit.to_tuple()
    if not t or not t[0]:
        return None
    actor = t[9]
    return t[5], t[7], bool(t[1]), actor.get_actor_label() if actor else None


class Probe:
    def __init__(self, world):
        self.world = world

    def line(self, a, b):
        # simple collision, as the characters stand on and bump into it
        return unpack(ue.SystemLibrary.line_trace_single_by_profile(self.world, a, b, "Pawn", False, [], ue.DrawDebugTrace.NONE, True))

    def capsule_blocked(self, centre):
        hit = unpack(ue.SystemLibrary.capsule_trace_single_by_profile(self.world, centre, centre + ue.Vector(0.0, 0.0, 1.0), PROBE_RADIUS,
                                                                       HALF_HEIGHT, "Pawn", False, [], ue.DrawDebugTrace.NONE, True))
        return hit is not None

    def floor(self, point, expected_z):
        """The walkable floor under point near expected_z: its height, or the reason it is refused"""
        hit = self.line(ue.Vector(point.x, point.y, expected_z + 150.0), ue.Vector(point.x, point.y, expected_z - 80.0))
        if not hit:
            return None, "no floor"
        impact, normal, _, _ = hit
        if abs(impact.z - expected_z) > FLOOR_TOLERANCE:
            return None, "floor height"
        if normal.z < MIN_WALKABLE_NZ:
            return None, "not walkable"
        return impact.z, None

    def roofed(self, point, floor_z):
        return self.line(ue.Vector(point.x, point.y, floor_z + 80.0), ue.Vector(point.x, point.y, floor_z + ROOF_REACH)) is not None

    def sees(self, a, b):
        return self.line(a, b) is None


# ------------------------------------------------------------------------------------------------ geometry

def inside_polygon(polygon, x, y):
    """Even-odd test, as FortGarrisonMath::IsInside"""
    inside = False
    j = len(polygon) - 1
    for i in range(len(polygon)):
        ax, ay = polygon[i]
        bx, by = polygon[j]
        if (ay > y) != (by > y):
            cross = ax + (y - ay) * (bx - ax) / (by - ay)
            if x < cross:
                inside = not inside
        j = i
    return inside


def module_point(transform, x, y, z):
    """A point of the maquette's module frame (m) in the world"""
    return ue.MathLibrary.transform_location(transform, ue.Vector(100.0 * x, -100.0 * y, 100.0 * z))


def yaw_toward(a, b):
    return math.degrees(math.atan2(b.y - a.y, b.x - a.x))


def frange(a, b, step):
    n = max(0, int(math.floor((b - a) / step + 1e-6)))
    return [a + i * step for i in range(n + 1)]


class Zone:
    def __init__(self, actor):
        self.actor = actor
        self.transform = actor.get_actor_transform()
        self.yaw = actor.get_actor_rotation().yaw
        self.outline = [(p.x, p.y) for p in actor.get_editor_property("outline")]
        self.min_z = actor.get_editor_property("min_z")
        self.max_z = actor.get_editor_property("max_z")

    def local(self, world):
        return ue.MathLibrary.inverse_transform_location(self.transform, world)

    def contains(self, world):
        p = self.local(world)
        return self.min_z <= p.z <= self.max_z and inside_polygon(self.outline, p.x, p.y)


# ------------------------------------------------------------------------------------------------ posts of a building

def candidates_of(spec, actor, probe, zone, refused):
    """Valid standing points of a building: (world point on the floor, roofed, sees a door, distance to the door, facing yaw)"""
    transform = actor.get_actor_transform()
    centre = actor.get_actor_location()
    found = []
    doors = [module_point(transform, dx, dy, 0.0) for dx, dy in (spec.get("door") or [])]
    if spec.get("door_mesh"):
        doors += [a.get_actor_location() for a in eas().get_all_level_actors() if mesh_name(a) == spec["door_mesh"]]
    if "regions" in spec:
        grid = []
        for x0, x1, y0, y1, floor_z, roofed in spec["regions"]:
            for x in frange(x0, x1, GRID):
                for y in frange(y0, y1, GRID):
                    p = module_point(transform, x, y, floor_z)
                    grid.append((p, p.z, roofed))
    else:
        origin, extent = actor.get_actor_bounds(False)
        low, high = spec["bounds_floor"]
        base_z = actor.get_actor_location().z
        grid = []
        for x in frange(origin.x - extent.x, origin.x + extent.x, GRID * 100.0):
            for y in frange(origin.y - extent.y, origin.y + extent.y, GRID * 100.0):
                grid.append((ue.Vector(x, y, base_z + (low + high) / 2.0), base_z + (low + high) / 2.0, True))
        tolerance = (high - low) / 2.0
    for point, expected, roofed in grid:
        if "regions" in spec:
            floor_z, why = probe.floor(point, expected)
        else:
            floor_z, why = probe.floor(point, expected)
            if why == "floor height" or (floor_z is not None and abs(floor_z - expected) > tolerance):
                floor_z, why = None, "floor height"
        if floor_z is None:
            refused[why] = refused.get(why, 0) + 1
            continue
        stand = ue.Vector(point.x, point.y, floor_z)
        if roofed and not probe.roofed(stand, floor_z):
            refused["no roof"] = refused.get("no roof", 0) + 1
            continue
        if probe.capsule_blocked(ue.Vector(point.x, point.y, floor_z + HALF_HEIGHT + 5.0)):
            refused["no room"] = refused.get("no room", 0) + 1
            continue
        if not zone.contains(stand):
            refused["out of the zone"] = refused.get("out of the zone", 0) + 1
            continue
        chest = stand + ue.Vector(0.0, 0.0, 80.0)
        best_door, best_dist, seen = None, 1e9, False
        for door in doors:
            target = ue.Vector(door.x, door.y, chest.z)
            dist = math.hypot(door.x - stand.x, door.y - stand.y)
            visible = probe.sees(chest, target)
            if (visible and not seen) or (visible == seen and dist < best_dist):
                best_door, best_dist, seen = door, dist, visible
        if best_door is not None and best_dist < 90.0:
            refused["in the doorway"] = refused.get("in the doorway", 0) + 1
            continue
        if best_door is not None and roofed:
            yaw = yaw_toward(stand, best_door)
        else:
            yaw = yaw_toward(centre, stand)
        found.append({"point": stand, "roofed": roofed, "sees_door": seen, "door_dist": best_dist if best_door is not None else None, "yaw": yaw})
    return found


def choose(found, count):
    """Up to count posts, SPACING apart on a floor: those that see their door first, about 3 m from it"""
    def score(c):
        return (0 if c["sees_door"] else 1, abs((c["door_dist"] or 300.0) - 300.0))
    chosen = []
    for c in sorted(found, key=score):
        p = c["point"]
        if all(math.hypot(p.x - o["point"].x, p.y - o["point"].y) >= SPACING or abs(p.z - o["point"].z) > 150.0 for o in chosen):
            chosen.append(c)
        if len(chosen) >= count:
            break
    return chosen


# ------------------------------------------------------------------------------------------------ steps

def step_navdata(level_key, dry_run):
    """The navigation data saved in the fort level goes (the volume stays)"""
    assert level_key == "land", "navdata only applies to LI_FortFrench_Land"
    found = [a for a in eas().get_all_level_actors() if a.get_class().get_name() in ("RecastNavMesh", "AbstractNavData")]
    out = {"found": [f"{a.get_actor_label()} ({a.get_class().get_name()})" for a in found], "deleted": []}
    if not dry_run:
        for actor in found:
            label = actor.get_actor_label()
            if eas().destroy_actor(actor):
                out["deleted"].append(label)
    out["volumes_kept"] = [a.get_actor_label() for a in eas().get_all_level_actors() if a.get_class().get_name() == "NavMeshBoundsVolume"]
    return out


def step_posts(level_key, dry_run):
    actors = eas().get_all_level_actors()
    zones = [a for a in actors if a.get_actor_label() == ZONE_LABEL]
    assert len(zones) == 1, f"{len(zones)} actor(s) labelled {ZONE_LABEL} in {world_path()}"
    zone = Zone(zones[0])
    probe = Probe(editor_world())
    by_mesh = {}
    for actor in actors:
        name = mesh_name(actor) if actor.get_class().get_name() == "StaticMeshActor" else None
        if name:
            by_mesh.setdefault(name, []).append(actor)
    posts, out = [], {"zone": {"label": ZONE_LABEL, "location": vec(zone.actor.get_actor_location()), "yaw": round(zone.yaw, 1)}, "buildings": {}}
    for spec in BUILDINGS:
        buildings = [a for a in by_mesh.get(spec["mesh"], []) if zone.contains(a.get_actor_location()) or spec["name"] in ("vault", "tower")]
        entry = {"actors": [a.get_actor_label() for a in buildings], "refused": {}, "candidates": 0, "posts": []}
        out["buildings"][spec["name"]] = entry
        if not buildings:
            entry["error"] = f"no actor with {spec['mesh']} in the fort"
            continue
        found = []
        for actor in buildings:
            found += candidates_of(spec, actor, probe, zone, entry["refused"])
        entry["candidates"] = len(found)
        for c in choose(found, spec["max"]):
            local = zone.local(c["point"])
            post = ue.FortBuildingPost()
            post.set_editor_property("location", local)
            post.set_editor_property("yaw", float(((c["yaw"] - zone.yaw + 180.0) % 360.0) - 180.0))
            post.set_editor_property("building", spec["name"])
            posts.append(post)
            entry["posts"].append({"world": vec(c["point"]), "zone_local": vec(local), "yaw_world": round(c["yaw"], 1),
                                   "sees_door": c["sees_door"], "door_m": None if c["door_dist"] is None else round(c["door_dist"] / 100.0, 2),
                                   "roofed": c["roofed"]})
    out["count"] = len(posts)
    out["by_building"] = {k: len(v["posts"]) for k, v in out["buildings"].items()}
    assert posts, "no building post found: nothing written (see the refusals of each building)"
    if not dry_run:
        zone.actor.modify()
        zone.actor.set_editor_property("building_posts", posts)
        out["written"] = len(zone.actor.get_editor_property("building_posts"))
    return out


STEPS = {"navdata": step_navdata, "posts": step_posts}


def main():
    kitlib.check_lock()
    request = dict(DEFAULTS)
    if REQUEST.exists():
        request.update(json.loads(REQUEST.read_text(encoding="utf-8-sig")))
    report["request"] = request
    start_level = world_path().split(".")[0]
    failed = []
    for level_key in request["levels"]:
        level = LEVELS[level_key]
        entry = report.setdefault(level_key, {"level": level})
        try:
            load_level(level)
        except Exception:  # noqa: BLE001
            entry["error"] = traceback.format_exc()[-1500:]
            failed.append(level_key)
            continue
        changed = False
        for step in request["steps"]:
            if step == "navdata" and level_key != "land":
                continue
            try:
                entry[step] = STEPS[step](level_key, request["dry_run"])
                changed = True
            except Exception:  # noqa: BLE001
                entry[step] = {"error": traceback.format_exc()[-1500:]}
                failed.append(f"{level_key}:{step}")
        if changed and not request["dry_run"]:
            entry["saved"] = bool(ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level())
            # what stays after the save: no navigation data in the fort level, the posts in the zone
            left = [a.get_actor_label() for a in eas().get_all_level_actors() if a.get_class().get_name() in ("RecastNavMesh", "AbstractNavData")]
            entry["nav_data_after_save"] = left
            if level_key == "land" and "navdata" in request["steps"] and left:
                failed.append("land:navdata (navigation data still in the level after the save)")
    if request.get("restore_level", True) and start_level and not world_path().startswith(start_level):
        try:
            load_level(start_level)
            report["restored_level"] = start_level
        except Exception:  # noqa: BLE001
            report["restore_error"] = traceback.format_exc()[-800:]
    report["failed"] = failed
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
    ue.log(f"T332_FORT_BUILDING_POSTS {'FAILED ' + ', '.join(failed) if failed else 'DONE'}")
    if failed:
        raise RuntimeError(f"fort_building_posts_ue: {', '.join(failed)} (see {OUT})")


if __name__ == "__main__":
    main()
