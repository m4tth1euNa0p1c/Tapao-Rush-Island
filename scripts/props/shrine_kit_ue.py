"""T-189: the village shrine you walk into, with a legendary artifact on its altar, in Unreal.

- BP_Artifact_<Name> (APhysicsPropActor) for the four large legendary artifacts of T-179 (artefacts-polynesiens:
  Heart of the Reef, Fish-God Fragment, Eclipse Stone, Crown of the Last Ariki), mass and value of the maquette's
  report; the two pearls (2 cm) are left for smaller places;
- imports art/export/Props/Village/ShrineDoors (scripts/props/shrine_interior.py): SM_Village_Shrine_Shell and
  SM_Village_ShrineDoor with their UCX, materials of SM_Village_Shrine / SM_Minka slot for slot;
- DA_ShrineKit (UVillageHouseKit): the two hinged leaves (OpenYaw), the doorway, the altar slot and the four artifacts
  as RelicVariants (one drawn per shrine), no everyday objects; listed in DA_VillageRules.HouseKits.
Runs in the editor: scripts/editor/lead_session_t186.py, action "shrine".
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

SRC = ROOT / "art/export/Props/Village/ShrineDoors"
OUT = ROOT / "monke_island/Saved/T186"
VILLAGE = kitlib.VILLAGE
SOURCE = VILLAGE + "/Meshes/SM_Village_Shrine"
KIT = kitlib.DATA + "/DA_ShrineKit"
ARTIFACTS = "/Game/MonkeIsland/Items/PolynesianArtifacts"
REPORT = Path("C:/Users/matth/OneDrive/maquette-blender/maquettes/artefacts-polynesiens/artifacts-report.json")
LEGENDARY = ("HeartOfTheReef", "FishGodFragment", "EclipseStone", "CrownOfTheLastAriki")
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def artifact_class(name, info):
    mesh = kitlib.load(f"{ARTIFACTS}/Meshes/SM_{name}")
    assert mesh, name
    path = f"{ARTIFACTS}/Blueprints/BP_Artifact_{name}"
    blueprint = kitlib.load(path)
    if not blueprint:
        factory = ue.BlueprintFactory()
        factory.set_editor_property("parent_class", ue.PhysicsPropActor)
        blueprint = TOOLS.create_asset(f"BP_Artifact_{name}", f"{ARTIFACTS}/Blueprints", ue.Blueprint, factory)
    assert blueprint, path
    cdo = ue.get_default_object(blueprint.generated_class())
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("static_mesh", mesh)
    body.set_linear_damping(0.35)
    body.set_angular_damping(0.6)
    cdo.set_editor_property("mass_kg", float(info["mass_kg"]))
    cdo.get_editor_property("valuable").set_editor_property("base_value", int(info["value"]))
    ue.BlueprintEditorLibrary.compile_blueprint(blueprint)
    kitlib.save(blueprint)
    convexes = kitlib.convex_count(mesh)
    return blueprint.generated_class(), {"mass_kg": info["mass_kg"], "value": info["value"], "convexes": convexes}


def main():
    kitlib.check_lock()
    doc = json.loads((SRC / "shrine_doors.json").read_text(encoding="utf-8"))
    report = json.loads(REPORT.read_text(encoding="utf-8"))["objects"]
    relics, relic_info = [], {}
    for name in LEGENDARY:
        cls, info = artifact_class(name, report[name])
        relics.append(cls)
        relic_info[name] = info

    source = kitlib.load(SOURCE)
    minka = kitlib.load(kitlib.SOURCE)
    assert source and minka
    by_slot = {}
    for mesh in (minka, source):
        for s in mesh.get_editor_property("static_materials"):
            by_slot[str(s.get_editor_property("material_slot_name"))] = s.get_editor_property("material_interface")
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    old_src = kitlib.SRC
    meshes = {}
    try:
        kitlib.SRC = SRC
        for name in [doc["shell"]["mesh"]] + list(doc["panel_meshes"]):
            meshes[name] = kitlib.import_mesh(name)
    finally:
        kitlib.SRC = old_src
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
    result_meshes = {}
    source_body = source.get_editor_property("body_setup")
    for name, mesh in meshes.items():
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            mesh.set_material(index, kitlib.material_for(str(slot.get_editor_property("material_slot_name")), by_slot))
        expected = doc["shell"]["ucx"] if name == doc["shell"]["mesh"] else 1
        count = kitlib.convex_count(mesh)
        assert count == expected, f"{name}: {count} convexes, {expected} expected"
        mesh.get_editor_property("body_setup").set_editor_property(
            "collision_trace_flag", source_body.get_editor_property("collision_trace_flag"))
        kitlib.save(mesh)
        result_meshes[name] = {"convexes": count}

    kit = kitlib.load(KIT)
    if not kit:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.VillageHouseKit)
        kit = TOOLS.create_asset("DA_ShrineKit", kitlib.DATA, ue.VillageHouseKit, factory)
    assert kit, KIT

    def vec(v):
        return ue.Vector(float(v[0]), float(v[1]), float(v[2]))

    panels = []
    for p in doc["panels"]:
        panel = ue.VillageHousePanel()
        panel.set_editor_property("mesh", meshes[p["mesh"]])
        panel.set_editor_property("closed_location", vec(p["closed"]))
        panel.set_editor_property("yaw", float(p["yaw"]))
        panel.set_editor_property("open_yaw", float(p["open_yaw"]))
        panel.set_editor_property("travel", vec(p["travel"]))
        panel.set_editor_property("open_fraction", float(p["open_fraction"]))
        panel.set_editor_property("doorway", int(p["doorway_index"]))
        panels.append(panel)
    doorways = []
    for d in doc["doorways"]:
        doorway = ue.VillageHouseDoorway()
        doorway.set_editor_property("name", d["name"])
        doorway.set_editor_property("center", vec(d["centre"]))
        doorway.set_editor_property("yaw", float(d["yaw"]))
        doorway.set_editor_property("half_width", float(d["half_width"]))
        doorway.set_editor_property("height", float(d["height"]))
        doorways.append(doorway)
    slots = []
    for e in doc["slots"]["relic"]:
        slot = ue.VillageHouseSlot()
        slot.set_editor_property("location", vec(e["location"]))
        slot.set_editor_property("yaw", float(e["yaw"]))
        slot.set_editor_property("room", e["room"])
        slots.append(slot)
    kit.set_editor_property("source_mesh", source)
    kit.set_editor_property("shell_mesh", meshes[doc["shell"]["mesh"]])
    kit.set_editor_property("panels", panels)
    kit.set_editor_property("doorways", doorways)
    kit.set_editor_property("relic_variants", relics)
    kit.set_editor_property("relic_slots", slots)
    kit.set_editor_property("item_slots", [])
    kit.set_editor_property("min_items_per_house", 0)
    kit.set_editor_property("max_items_per_house", 0)
    # hinged leaves sweep a quarter turn: a little slower than the sliding panels
    kit.set_editor_property("slide_seconds", 0.5)
    kitlib.save(kit)

    rules = kitlib.load(kitlib.RULES)
    kits = [k for k in rules.get_editor_property("house_kits") if k and k.get_path_name() != kit.get_path_name()]
    rules.set_editor_property("house_kits", kits + [kit])
    kitlib.save(rules)
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-189", "kit": KIT, "meshes": result_meshes, "artifacts": relic_info,
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "shrine-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T189_SHRINE_SUCCESS")


if __name__ == "__main__":
    main()
