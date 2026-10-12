"""T-217: common objects scattered inside the buildings of the French fort of L_Test_Flat (owner request 2026-10-07:
"disperser des objets dans le fort français, des objets communs dans les bâtiments, de 0 à 120").

- DA_FortLoot (ULootRules): the items of the house loot (DA_LootRules_Houses: small enough for a room) and the common
  loot of the island no longer than 1.1 m, worth 0 to 120, their weights kept; GroundSearchHeight 60 cm so an item lies on the floor of a room, not on its roof;
- its spots: the floor of the fort's rooms, found by traces on a 1 m grid over the court (maquette x -20..20, y -16..16):
  a ceiling 2 to 7 m over the court level, a floor at that level, room around (no wall within 45 cm); the gate passage
  is left out;
- FortFR_Loot (ALootScatterSpawner) at the fort, folder FortFrench/Garrison. The level and the asset are saved.
Runs in the editor: scripts/editor/lead_session_t186.py, action "fortloot".
Result: monke_island/Saved/T217/fort-loot.json
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T217"
SOURCE = "/Game/MonkeIsland/Items/Loot/DA_LootRules_Houses"
COMMON = "/Game/MonkeIsland/Items/Loot/DA_LootRules"
MAX_LENGTH = 110.0
RULES = "/Game/MonkeIsland/Environment/FortFrench/Data/DA_FortLoot"
FOLDER = "FortFrench/Garrison"
ORIGIN = ue.Vector(-66000.0, 0.0, 115.0)
MAX_VALUE = 120
COUNT = 36
SPACING = 140.0


def trace(world, start, end):
    hit = ue.SystemLibrary.line_trace_single(world, start, end, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [],
                                             ue.DrawDebugTrace.NONE, True)
    return hit.to_tuple()[4] if hit else None


def value_of(cls):
    cdo = ue.get_default_object(cls)
    try:
        return int(cdo.get_editor_property("valuable").get_editor_property("base_value"))
    except Exception:  # noqa: BLE001 - an item without a value is not common loot
        return None


def item_length(cls):
    cdo = ue.get_default_object(cls)
    try:
        mesh = cdo.get_editor_property("mesh").get_editor_property("static_mesh")
        box = mesh.get_bounding_box()
        return max(box.max.x - box.min.x, box.max.y - box.min.y)
    except Exception:  # noqa: BLE001 - an item without a mesh is not placed
        return 1.0e9


def room_spots(world):
    spots = []
    for ix in range(-20, 21):
        for iy in range(-16, 17):
            x, y = float(ix), float(iy)
            if y < -15.5 and abs(x) < 3.0:
                continue  # the gate passage
            point = ORIGIN + ue.Vector(x * 100.0, -y * 100.0, 0.0)
            ceiling = trace(world, point + ue.Vector(0.0, 0.0, 60.0), point + ue.Vector(0.0, 0.0, 700.0))
            if not ceiling or ceiling.z - ORIGIN.z < 200.0:
                continue
            floor = trace(world, point + ue.Vector(0.0, 0.0, 60.0), point - ue.Vector(0.0, 0.0, 150.0))
            if not floor or abs(floor.z - ORIGIN.z) > 80.0:
                continue
            chest = ue.Vector(point.x, point.y, floor.z + 40.0)
            if any(trace(world, chest, chest + ue.Vector(dx * 45.0, dy * 45.0, 0.0)) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                continue
            spots.append(ue.Vector(point.x, point.y, floor.z))
    return spots


def main():
    kitlib.check_lock()
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    assert world and "L_Test_Flat" in world.get_path_name(), "L_Test_Flat must be open"
    source = kitlib.load(SOURCE)
    assert source, SOURCE
    kept, dropped, seen = [], [], set()
    # the house loot first (small enough for a room), then the common loot of the island up to MAX_VALUE that is not
    # longer than MAX_LENGTH (a room keeps it)
    for table, check_length in ((source, False), (kitlib.load(COMMON), True)):
        for entry in table.get_editor_property("items") if table else []:
            cls = entry.get_editor_property("item_class")
            if not cls or cls.get_name() in seen:
                continue
            seen.add(cls.get_name())
            value = value_of(cls)
            length = item_length(cls) if check_length else 0.0
            ok = value is not None and 0 <= value <= MAX_VALUE and length <= MAX_LENGTH
            (kept if ok else dropped).append((entry, cls.get_name(), value))
    assert kept, "no house loot item worth 0 to 120"
    rules = kitlib.load(RULES)
    if not rules:
        rules = ue.EditorAssetLibrary.duplicate_asset(SOURCE, RULES)
    assert rules, RULES
    spots = room_spots(world)
    assert len(spots) >= 10, f"too few room spots in the fort: {len(spots)}"
    rules.set_editor_property("items", [e for e, _, _ in kept])
    rules.set_editor_property("count", COUNT)
    rules.set_editor_property("min_spacing", SPACING)
    rules.set_editor_property("min_distance_from_start", 0.0)
    rules.set_editor_property("max_distance_from_start", 0.0)
    rules.set_editor_property("min_ground_height", -100000.0)
    rules.set_editor_property("building_mesh_keywords", [])
    rules.set_editor_property("ground_search_height", 60.0)
    # the common objects of the island are worth 5 to 40: drawn up to three times their value, never over 120
    try:
        rules.set_editor_property("value_scale_range", ue.Vector2D(1.0, 3.0))
        rules.set_editor_property("max_value", MAX_VALUE)
    except Exception:  # noqa: BLE001 - before the build that adds them
        pass
    rules.set_editor_property("spots", spots)
    kitlib.save(rules)

    actors_sub = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    spawner = next((a for a in actors_sub.get_all_level_actors() if a.get_actor_label() == "FortFR_Loot"), None)
    if not spawner:
        spawner = actors_sub.spawn_actor_from_class(ue.LootScatterSpawner, ORIGIN, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        spawner.set_actor_label("FortFR_Loot")
    spawner.set_folder_path(FOLDER)
    spawner.set_editor_property("rules", rules)
    assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()

    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-217", "rules": RULES, "items": sorted((n, v) for _, n, v in kept), "dropped": sorted((str(n), v) for _, n, v in dropped),
              "spots": len(spots), "count": COUNT, "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "fort-loot.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T217_FORT_LOOT_SUCCESS")


if __name__ == "__main__":
    main()
