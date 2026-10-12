"""Owner requests 2026-10-07 in the French fort of L_Test_Flat: "mets des PNJ dans les cages du fort français" and
"mets de l'or dans l'église du fort français".

- Prisoners: tiki villagers (SK_Villager, no AI: skeletal mesh actors playing a loop of the villager clips: cower, shake
  no, look around) on the floor of the two ground cages and of the cage hanging from the gibbet, found by a trace from
  inside each cage; no collision (the bars hold them), facing the court.
- Gold of the chapel: copies of the treasure of the fort (APhysicsPropActor with their mesh and value: a small gold
  stack, loose bars, coin piles) on the highest surface at the back of the chapel (its altar), found by traces, else on
  its floor.
Labels FortFR_Prisoner_* and FortFR_ChapelGold_*, folder FortFrench/Garrison; a replay replaces them. The level is saved.
Runs in the editor: scripts/editor/lead_session_t186.py, action "fortcages".
Result: monke_island/Saved/T213/fort-cages-chapel.json
"""
import datetime as dt
import json
import math
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T213"
FOLDER = "FortFrench/Garrison"
VILLAGER = "/Game/MonkeIsland/Characters/Villager/Meshes/SK_Villager"
ANIMS = "/Game/MonkeIsland/Characters/Villager/Anims/"
COURT = ue.Vector(-66000.0, 0.0, 115.0)
# cage label -> villagers (offset along the cage's local x in cm, clip)
CAGES = {"Fort_T193_134_Fort_Cage_Ground": [(-45.0, "A_Villager_Cower"), (45.0, "A_Villager_Shake_No")],
         "Fort_T193_135_Fort_Cage_Ground": [(0.0, "A_Villager_Look_Around")],
         "Fort_T193_137_Fort_Cage_Hanging": [(0.0, "A_Villager_Cower")]}
CHAPEL = "Fort_T193_124_Fort_Chapel"
# treasure of the fort to copy (label of the original) and how many
GOLD = [("Fort_T193_Pickup_SM_Loot_GoldStack_S", 1), ("Fort_T193_149_Loot_GoldBar", 3), ("Fort_T193_167_Loot_CoinPile", 2)]


def trace(world, start, end, ignore=None):
    hit = ue.SystemLibrary.line_trace_single(world, start, end, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignore or [],
                                             ue.DrawDebugTrace.NONE, True)
    if not hit:
        return None
    t = hit.to_tuple()
    return t[4], t[9]


def clear_old(actors_sub, prefix):
    for actor in actors_sub.get_all_level_actors():
        if actor.get_actor_label().startswith(prefix):
            actors_sub.destroy_actor(actor)


def prisoners(world, actors_sub, by_label, mesh):
    placed = []
    for label, villagers in CAGES.items():
        cage = by_label.get(label)
        assert cage, f"no {label} in the level"
        origin, extent = cage.get_actor_bounds(False)
        yaw = cage.get_actor_rotation().yaw
        # the floor of the cage: from inside it, down
        start = ue.Vector(origin.x, origin.y, origin.z)
        floor = trace(world, start, start - ue.Vector(0.0, 0.0, extent.z + 150.0))
        floor_z = floor[0].z if floor else origin.z - extent.z
        # facing the middle of the court (the villager mesh faces its local +Y)
        facing = math.degrees(math.atan2(COURT.y - origin.y, COURT.x - origin.x))
        for index, (offset, clip_name) in enumerate(villagers):
            rad = math.radians(yaw)
            spot = ue.Vector(origin.x + math.cos(rad) * offset, origin.y + math.sin(rad) * offset, floor_z + 1.0)
            actor = actors_sub.spawn_actor_from_class(ue.SkeletalMeshActor, spot, ue.Rotator(roll=0.0, pitch=0.0, yaw=facing - 90.0))
            actor.set_actor_label(f"FortFR_Prisoner_{len(placed) + 1}")
            actor.set_folder_path(FOLDER)
            comp = actor.get_editor_property("skeletal_mesh_component")
            comp.set_skeletal_mesh_asset(mesh)
            comp.set_collision_profile_name("NoCollision")
            clip = kitlib.load(ANIMS + clip_name)
            if clip:
                comp.set_editor_property("animation_mode", ue.AnimationMode.ANIMATION_SINGLE_NODE)
                data = ue.SingleAnimationPlayData()
                data.set_editor_property("anim_to_play", clip)
                data.set_editor_property("saved_looping", True)
                data.set_editor_property("saved_playing", True)
                data.set_editor_property("saved_play_rate", 0.9 + 0.1 * index)
                comp.set_editor_property("animation_data", data)
            placed.append({"label": actor.get_actor_label(), "cage": label, "clip": clip_name,
                           "at": [round(spot.x), round(spot.y), round(spot.z)], "floor_hit": bool(floor)})
    return placed


def chapel_gold(world, actors_sub, by_label):
    chapel = by_label.get(CHAPEL)
    assert chapel, f"no {CHAPEL}"
    origin, extent = chapel.get_actor_bounds(False)
    ground = COURT.z
    # surfaces inside the chapel under its roof: a grid of traces from 2.6 m over the ground
    surfaces = []
    for ix in range(-int(extent.x) + 60, int(extent.x) - 59, 40):
        for iy in range(-int(extent.y) + 60, int(extent.y) - 59, 40):
            start = ue.Vector(origin.x + ix, origin.y + iy, ground + 260.0)
            hit = trace(world, start, ue.Vector(start.x, start.y, ground - 60.0))
            if hit and hit[0].z - ground < 200.0:
                surfaces.append(hit[0])
    assert surfaces, "nothing found inside the chapel"
    # the altar: the highest surface between 50 cm and 1.6 m over the ground, else the floor at the back (-Y in Unreal:
    # the door of the chapel opens on the court, +Y)
    raised = [p for p in surfaces if 50.0 <= p.z - ground <= 160.0]
    if raised:
        top = max(p.z for p in raised)
        base = [p for p in raised if top - p.z < 3.0]
        where = "altar"
    else:
        back = min(p.y for p in surfaces)
        base = [p for p in surfaces if p.y - back < 120.0]
        where = "floor at the back"
    centre = ue.Vector(sum(p.x for p in base) / len(base), sum(p.y for p in base) / len(base), max(p.z for p in base))
    placed = []
    slots = [(0.0, 0.0), (-28.0, 14.0), (28.0, 14.0), (-28.0, -14.0), (28.0, -14.0), (0.0, 26.0), (0.0, -26.0)]
    for source_label, count in GOLD:
        source = by_label.get(source_label)
        if not source:
            continue
        for _ in range(count):
            dx, dy = slots[len(placed) % len(slots)]
            spot = ue.Vector(centre.x + dx, centre.y + dy, centre.z + 4.0)
            copy = actors_sub.duplicate_actor(source, world, ue.Vector(0.0, 0.0, 0.0))
            copy.set_actor_location(spot, False, True)
            copy.set_actor_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=(len(placed) * 37.0) % 360.0), True)
            copy.set_actor_label(f"FortFR_ChapelGold_{len(placed) + 1}")
            copy.set_folder_path(FOLDER)
            valuable = copy.get_component_by_class(ue.ValuableComponent)
            placed.append({"label": copy.get_actor_label(), "from": source_label,
                           "value": valuable.get_editor_property("base_value") if valuable else None,
                           "at": [round(spot.x), round(spot.y), round(spot.z)]})
    return where, [round(centre.x), round(centre.y), round(centre.z - ground)], placed


def main():
    kitlib.check_lock()
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    assert world and "L_Test_Flat" in world.get_path_name(), "L_Test_Flat must be open"
    actors_sub = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    clear_old(actors_sub, "FortFR_Prisoner_")
    clear_old(actors_sub, "FortFR_ChapelGold_")
    by_label = {a.get_actor_label(): a for a in actors_sub.get_all_level_actors()}
    mesh = kitlib.load(VILLAGER)
    assert mesh, VILLAGER
    box = mesh.get_bounds().box_extent
    caged = prisoners(world, actors_sub, by_label, mesh)
    where, altar, gold = chapel_gold(world, actors_sub, by_label)
    assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"villager_extent_cm": [round(box.x), round(box.y), round(box.z)], "prisoners": caged, "gold_on": where,
              "gold_spot": altar, "gold": gold, "gold_value": sum(g["value"] or 0 for g in gold),
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "fort-cages-chapel.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("FORT_CAGES_CHAPEL_DONE")


if __name__ == "__main__":
    main()
