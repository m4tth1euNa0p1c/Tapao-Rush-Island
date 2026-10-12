"""T-258 (lot L2): the wooden loot crate in the content and on the test map, after the build that brings its C++ classes.

Needs ALootCrateActor and ULootCrateRules (Source/monke_island/Props/LootCrate) compiled into the editor: run by the
lead after the full build; before it, the script stops at once with a clear message and touches nothing.
Idempotent (by asset path and by actor label):
  1. DA_LootCrateRules  /Game/MonkeIsland/Props/LootCrate/DA_LootCrateRules (ULootCrateRules): 12 objects, 60 kg, an
     8 kg crate (owner request 2026-10-09); the meshes of the rules keep their C++ defaults (SM_LootCrate, _Lid)
  2. BP_LootCrate       /Game/MonkeIsland/Props/LootCrate/BP_LootCrate (parent ALootCrateActor): Rules, CrateMesh and
     LidMesh set, the crate placed on maps; the galleon spawns the bare class at each expedition
     (UGalleonRules.LootCrateClass, C++ default /Script/monke_island.LootCrateActor), which takes DA_LootCrateRules
     and the meshes by path: reported here, not changed
  3. L_Test_Flat        a crate near the first PlayerStart (label LootCrate_Test) and four common loot objects beside it
     to try the crate at once (LootCrate_TestLoot_*), folder LootCrateTest, tag T258_LootCrate; the level is saved
Prerequisite: the meshes of scripts/props/import_loot_crate_ue.py (action "crateimport").
Runs in the editor: scripts/editor/lead_session_t186.py, action "cratemap" (editor lock held, no game running).
Writes monke_island/Saved/T258/loot-crate-map.json. Removing the test actors: delete the folder LootCrateTest.
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

MAP = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
BASE = "/Game/MonkeIsland/Props/LootCrate"
RULES = BASE + "/DA_LootCrateRules"
BLUEPRINT = BASE + "/BP_LootCrate"
MESH = BASE + "/Meshes/SM_LootCrate"
LID = BASE + "/Meshes/SM_LootCrate_Lid"
GALLEON_RULES = ["/Game/MonkeIsland/Galleon/DA_GalleonRules", "/Game/MonkeIsland/Galleon/DA_GalleonRules_TestFlat"]
LOOT = "/Game/MonkeIsland/Items/Loot/Blueprints"
TEST_LOOT = ["BP_Loot_RopeCoil", "BP_Loot_SmallChest", "BP_Loot_Calabash", "BP_Loot_CoconutBowl"]
FOLDER = "LootCrateTest"
TAG = "T258_LootCrate"
OUT = ROOT / "monke_island/Saved/T258/loot-crate-map.json"
# rules of the owner's request (2026-10-09), the other values keep the C++ defaults of ULootCrateRules
RULE_VALUES = {"max_items": 12, "max_content_kg": 60.0, "empty_mass_kg": 8.0}
# test layout from the first PlayerStart (cm): the crate ahead and to the left of the weapons row of T-249 (forward
# 250 to 550, right -360 to 360), the loot in a line beside it
CRATE_AHEAD, CRATE_RIGHT = 180.0, -520.0
LOOT_AHEAD, LOOT_RIGHT, LOOT_SPACING = 180.0, -680.0, 45.0
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
result = {"map": MAP}


def require_classes():
    """The C++ classes of the crate are in the editor only after the build that compiled them"""
    missing = [name for name in ("LootCrateActor", "LootCrateRules") if not hasattr(ue, name)]
    if missing:
        raise RuntimeError("T-258: the C++ classes " + ", ".join("A" + m if m.endswith("Actor") else "U" + m for m in missing)
                           + " are not in this editor yet: run 'cratemap' after the full build of monke_island (lead)")
    for path in (MESH, LID):
        assert kitlib.load(path), f"{path} missing: run the action 'crateimport' first"


def rules_asset():
    rules = kitlib.load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.LootCrateRules)
        folder, name = RULES.rsplit("/", 1)
        rules = TOOLS.create_asset(name, folder, ue.LootCrateRules, factory)
        result["rules"] = "created"
    else:
        result["rules"] = "kept"
    for prop, value in RULE_VALUES.items():
        rules.set_editor_property(prop, value)
    kitlib.save(rules)
    result["rules_values"] = {prop: rules.get_editor_property(prop) for prop in RULE_VALUES}
    return rules


def blueprint(rules):
    bp = kitlib.load(BLUEPRINT)
    if not bp:
        factory = ue.BlueprintFactory()
        factory.set_editor_property("parent_class", ue.LootCrateActor)
        folder, name = BLUEPRINT.rsplit("/", 1)
        bp = TOOLS.create_asset(name, folder, ue.Blueprint, factory)
        result["blueprint"] = "created"
    else:
        result["blueprint"] = "kept"
    ue.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = ue.get_default_object(bp.generated_class())
    cdo.set_editor_property("rules", rules)
    cdo.get_editor_property("crate_mesh").set_editor_property("static_mesh", kitlib.load(MESH))
    cdo.get_editor_property("lid_mesh").set_editor_property("static_mesh", kitlib.load(LID))
    cdo.modify()
    ue.BlueprintEditorLibrary.compile_blueprint(bp)
    assert ue.EditorAssetLibrary.save_loaded_asset(bp, False), BLUEPRINT
    return bp


def galleon_rules():
    """The class the galleon spawns on its deck (UGalleonRules.LootCrateClass, C++ default the bare ALootCrateActor):
    reported, not changed"""
    seen = {}
    for path in GALLEON_RULES:
        rules = kitlib.load(path)
        if rules:
            seen[path] = str(rules.get_editor_property("loot_crate_class"))
    result["galleon_loot_crate_class"] = seen


def ground_z(world, x, y, z_from, ignored):
    """Height of the ground under (x, y), the test actors themselves left out (a rerun never stacks them)"""
    start = ue.Vector(x, y, z_from + 300.0)
    end = ue.Vector(x, y, z_from - 2000.0)
    hit = ue.SystemLibrary.line_trace_single(world, start, end, ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, ignored,
                                             ue.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    if not t or not t[0]:
        return None
    return t[4].z


def place(actors, existing, world, label, cls, origin, forward, right, ahead, side, half_height):
    location = origin + forward * ahead + right * side
    ignored = [a for name, a in existing.items() if name.startswith("LootCrate_Test")]
    z = ground_z(world, location.x, location.y, origin.z, ignored)
    location = ue.Vector(location.x, location.y, (z if z is not None else origin.z - 90.0) + half_height + 2.0)
    actor = existing.get(label)
    if actor:
        actor.set_actor_location(location, False, False)
        result.setdefault("kept", []).append(label)
    else:
        actor = actors.spawn_actor_from_class(cls, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        assert actor, label
        actor.set_actor_label(label)
        result.setdefault("spawned", []).append(label)
    actor.set_folder_path(FOLDER)
    actor.set_editor_property("tags", [TAG])
    return actor


def main():
    kitlib.check_lock()
    require_classes()
    rules = rules_asset()
    bp = blueprint(rules)
    galleon_rules()

    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world or "L_Test_Flat" not in world.get_path_name():
        assert levels.load_level(MAP), f"cannot load {MAP}"
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    start = next((a for a in existing.values() if isinstance(a, ue.PlayerStart)), None)
    origin = start.get_actor_location() if start else ue.Vector(0.0, 0.0, 100.0)
    forward = start.get_actor_forward_vector() if start else ue.Vector(1.0, 0.0, 0.0)
    right = start.get_actor_right_vector() if start else ue.Vector(0.0, 1.0, 0.0)
    result["origin"] = [origin.x, origin.y, origin.z]

    half = rules.get_editor_property("body_half_size_cm").z
    crate = place(actors, existing, world, "LootCrate_Test", bp.generated_class(), origin, forward, right, CRATE_AHEAD, CRATE_RIGHT, half)
    loc = crate.get_actor_location()
    result["crate"] = [round(loc.x), round(loc.y), round(loc.z)]
    result["missing"] = []
    for index, name in enumerate(TEST_LOOT):
        cls = ue.EditorAssetLibrary.load_blueprint_class(f"{LOOT}/{name}")
        if not cls:
            result["missing"].append(name)
            continue
        place(actors, existing, world, f"LootCrate_TestLoot_{name[len('BP_Loot_'):]}", cls, origin, forward, right,
              LOOT_AHEAD + (index - 1.5) * LOOT_SPACING, LOOT_RIGHT, 15.0)

    assert levels.save_current_level()
    result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(result, indent=1, ensure_ascii=False, default=str), encoding="utf-8")
    ue.log(f"T258_LOOT_CRATE_MAP_SUCCESS {OUT}")


if __name__ == "__main__":
    main()
