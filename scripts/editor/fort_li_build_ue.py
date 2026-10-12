"""T-205: the French fort of L_Test_Flat as a level instance, LI_FortFrench, for the campaign's fort island.

1. In L_Test_Flat: DA_FortLoot.Spots go from world positions to the frame of the FortFR_Loot spawner (its
   bSpotsInActorFrame on): the loot follows the fort wherever it stands; L_Test_Flat keeps working the same.
2. L_Test_Flat is duplicated as /Game/MonkeIsland/Maps/Fragments/LI_FortFrench; in the copy only the fort is kept:
   the actors on the fort island (the bounds of Fort_T193_TerrainFar, 10 m more) that are not the test map's own
   (its light, sky, ground, sea, player start, arena, showcases); then everything is moved by SHIFT so the fort stands
   at the origin of the level, its sea level (-50 cm in L_Test_Flat) at 0 like the expedition maps.

Dry run unless monke_island/Saved/T186/fortli-apply.txt exists: only the report of what would be kept.
Writes monke_island/Saved/T186/fort-li.json. Action "fortli" of scripts/editor/lead_session_t186.py.
"""
import collections
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

TEST = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
LI = "/Game/MonkeIsland/Maps/Fragments/LI_FortFrench"
LOOT_DA = "/Game/MonkeIsland/Environment/FortFrench/Data/DA_FortLoot"
SHIFT = ue.Vector(66000.0, 0.0, 50.0)
APPLY = (ROOT / "monke_island/Saved/T186/fortli-apply.txt").exists()
OUT = ROOT / "monke_island/Saved/T186/fort-li.json"
DROP_FOLDERS = ("Lighting", "Ground", "Water", "Player", "BeachSimulation", "Arena", "AnimShowcase", "OneDriveShowcase", "Mobs")
DROP_CLASSES = ("WorldSettings", "DirectionalLight", "SkyLight", "SkyAtmosphere", "ExponentialHeightFog", "VolumetricCloud",
                "PostProcessVolume", "PlayerStart", "Brush", "WaveSurfaceActor", "ShoreAmbienceActor", "WindLeavesActor",
                "DayNightCycleActor", "GameplayDebuggerPlayerManager", "AbstractNavData", "RecastNavMesh")
report = {"apply": APPLY}


def actors():
    return ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()


def load_level(path):
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world.get_path_name().startswith(path):
        assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(path), f"cannot load {path}"


def by_label(label):
    return next((a for a in actors() if a.get_actor_label() == label), None)


def site_box():
    ground = by_label("Fort_T193_TerrainFar")
    assert ground, "Fort_T193_TerrainFar not found"
    o, e = ground.get_actor_bounds(False)
    return (o.x - e.x - 1000, o.y - e.y - 1000, o.x + e.x + 1000, o.y + e.y + 1000)


def keep_set(box):
    keep, drop = [], collections.Counter()
    for a in actors():
        cls = a.get_class().get_name()
        folder = str(a.get_folder_path())
        top = folder.split("/")[0] if folder and folder != "None" else ""
        if cls in DROP_CLASSES or top in DROP_FOLDERS:
            drop[f"{top}:{cls}"] += 1
            continue
        if a.get_attach_parent_actor():
            continue  # moves and goes with its parent
        loc = a.get_actor_location()
        if not (box[0] <= loc.x <= box[2] and box[1] <= loc.y <= box[3]):
            drop[f"{top}:out of the site"] += 1
            continue
        keep.append(a)
    return keep, drop


def convert_loot():
    loot = by_label("FortFR_Loot")
    assert loot, "FortFR_Loot not found in L_Test_Flat"
    rules = kitlib.load(LOOT_DA)
    assert rules, LOOT_DA
    if loot.get_editor_property("spots_in_actor_frame"):
        return {"already": True, "spots": len(rules.get_editor_property("spots"))}
    xform = loot.get_actor_transform()
    spots = list(rules.get_editor_property("spots"))
    rel = [xform.inverse_transform_location(s) for s in spots]
    if APPLY:
        rules.set_editor_property("spots", rel)
        kitlib.save(rules)
        loot.set_editor_property("spots_in_actor_frame", True)
        ue.EditorAssetLibrary.save_asset(TEST, only_if_is_dirty=False)
    return {"spots": len(spots), "first_world": str(spots[0]) if spots else None, "first_rel": str(rel[0]) if rel else None,
            "max_from_start": rules.get_editor_property("max_distance_from_start")}


def current_level():
    return ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world().get_path_name().split(".")[0]


def main():
    kitlib.check_lock()
    # in two editor sessions: the duplicate of L_Test_Flat must not be loaded as the editor world in the session that
    # made it (the old world check of the map load is fatal), so this session duplicates and saves, and the editor is
    # started again on LI_FortFrench for the second step
    if current_level() != LI:
        load_level(TEST)
        report["loot"] = convert_loot()
        box = site_box()
        report["site_box"] = [round(v) for v in box]
        keep, drop = keep_set(box)
        report["keep_count"] = len(keep)
        report["keep_by_folder"] = collections.Counter(str(a.get_folder_path()) for a in keep).most_common(60)
        report["keep_by_class"] = collections.Counter(a.get_class().get_name() for a in keep).most_common(40)
        report["dropped"] = drop.most_common(40)
        if APPLY and not ue.EditorAssetLibrary.does_asset_exist(LI):
            assert ue.EditorAssetLibrary.duplicate_asset(TEST, LI), "duplicate failed"
            report["duplicated"] = bool(ue.EditorAssetLibrary.save_asset(LI, only_if_is_dirty=False))
            report["next"] = "start the editor on " + LI + " and run fortli again"
        return
    # second step, LI_FortFrench open: only the fort, at the origin
    ground = by_label("Fort_T193_TerrainFar")
    assert ground, "Fort_T193_TerrainFar not found in LI_FortFrench"
    if abs(ground.get_actor_location().x) < 5000.0:
        report["already_built"] = True
        return
    box = site_box()
    keep, _drop = keep_set(box)
    keep_labels = {a.get_actor_label() for a in keep}
    gone = []
    for a in actors():
        if a.get_actor_label() in keep_labels or a.get_attach_parent_actor():
            continue
        if a.get_class().get_name() in ("WorldSettings", "Brush"):
            continue
        gone.append(a)
    ue.get_editor_subsystem(ue.EditorActorSubsystem).destroy_actors(gone)
    moved = 0
    for a in actors():
        if a.get_attach_parent_actor() or a.get_class().get_name() in ("WorldSettings", "Brush"):
            continue
        a.set_actor_location(a.get_actor_location() + SHIFT, False, False)
        moved += 1
    report["li_destroyed"] = len(gone)
    report["li_moved"] = moved
    report["li_actors"] = len(actors())
    report["li_saved"] = bool(ue.EditorAssetLibrary.save_asset(LI, only_if_is_dirty=False))


try:
    main()
finally:
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
