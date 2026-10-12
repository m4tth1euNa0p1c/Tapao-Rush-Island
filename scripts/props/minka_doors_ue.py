"""T-186: the minka of the fragment villages with working doors, in Unreal.

Imports art/export/Props/Village/MinkaDoors (scripts/props/minka_doors.py): SM_Minka_Shell and the panel meshes with
their UCX, the materials of SM_Minka slot for slot; fills DA_MinkaKit (UVillageHouseKit) from minka_doors.json, with
BP_TidalRelic in the tokonoma and the everyday objects of DA_LootRules_Houses (the small objects of DA_LootRules);
lists the kit in DA_VillageRules.HouseKits. AVillageSpawner then builds every SM_Minka of the fragment villages from it.
Runs in the editor: scripts/editor/lead_session_t186.py, action "minka".
"""
import datetime as dt
import json
import os
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "art/export/Props/Village/MinkaDoors"
OUT = ROOT / "monke_island/Saved/T186"
VILLAGE = "/Game/MonkeIsland/Environment/Village"
MESHES = VILLAGE + "/Meshes"
DATA = VILLAGE + "/Data"
SOURCE = MESHES + "/SM_Minka"
KIT = DATA + "/DA_MinkaKit"
RULES = DATA + "/DA_VillageRules"
RELIC = "/Game/MonkeIsland/Items/TidalRelic/Blueprints/BP_TidalRelic"
LOOT = "/Game/MonkeIsland/Items/Loot/DA_LootRules"
HOUSE_LOOT = "/Game/MonkeIsland/Items/Loot/DA_LootRules_Houses"
# objects lying in a house: no longer than this (cm) so they fit their slot away from walls and passages
MAX_ITEM_LENGTH = 70.0
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def check_lock():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    owner = os.environ.get("MONKE_LOCK_OWNER")
    assert owner and lock["owner"] == owner, f"editor lock held by {lock['owner']}, not {owner}"
    assert dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone(), "editor lock expired"
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def import_mesh(name):
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("import_uniform_scale", 1.0)
    task = ue.AssetImportTask()
    task.filename = str(SRC / (name + ".fbx"))
    task.destination_path = MESHES
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.options = options
    task.save = False
    TOOLS.import_asset_tasks([task])
    mesh = load(f"{MESHES}/{name}")
    assert mesh, name
    return mesh


def material_for(slot, by_slot):
    """Material of SM_Minka for the same slot; a Blender duplicate (M_Minka_wooden_bowl_01.001) takes its original's"""
    for candidate in (slot, slot.replace(".", "_"), slot.split(".")[0]):
        if candidate in by_slot:
            return by_slot[candidate]
        mi = load(f"{VILLAGE}/Materials/MI_{candidate[2:]}") if candidate.startswith("M_") else None
        if mi:
            return mi
    raise AssertionError(f"no material for slot {slot}")


def convex_count(mesh):
    body = mesh.get_editor_property("body_setup")
    return len(body.get_editor_property("agg_geom").get_editor_property("convex_elems"))


def house_loot():
    """The small objects of DA_LootRules (no rolled mat longer than a slot allows), same weights."""
    loot = load(LOOT)
    assert loot, LOOT
    table = load(HOUSE_LOOT)
    if not table:
        table = EAL.duplicate_asset(LOOT, HOUSE_LOOT)
    kept, dropped = [], []
    for entry in loot.get_editor_property("items"):
        cls = entry.get_editor_property("item_class")
        if not cls:
            continue
        cdo = ue.get_default_object(cls)
        mesh = cdo.get_editor_property("mesh").get_editor_property("static_mesh") if cdo else None
        box = mesh.get_bounding_box() if mesh else None
        length = max(box.max.x - box.min.x, box.max.y - box.min.y) if box else 0.0
        (kept if length <= MAX_ITEM_LENGTH else dropped).append((entry, cls.get_name(), round(length, 1)))
    table.set_editor_property("items", [e for e, _, _ in kept])
    # the scatter fields of the copy are never used by the houses: keep the copy from scattering anything if placed
    table.set_editor_property("count", 0)
    table.set_editor_property("spots", [])
    save(table)
    return table, [(n, l) for _, n, l in kept], [(n, l) for _, n, l in dropped]


def main():
    check_lock()
    doc = json.loads((SRC / "minka_doors.json").read_text(encoding="utf-8"))
    source = load(SOURCE)
    assert source, SOURCE
    by_slot = {str(s.get_editor_property("material_slot_name")): s.get_editor_property("material_interface")
               for s in source.get_editor_property("static_materials")}
    source_body = source.get_editor_property("body_setup")
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    meshes, report = {}, {}
    try:
        for name in ["SM_Minka_Shell"] + sorted(doc["panel_meshes"]):
            meshes[name] = import_mesh(name)
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
    for name, mesh in meshes.items():
        slots = []
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            mesh.set_material(index, material_for(slot_name, by_slot))
            slots.append(slot_name)
        expected = doc["shell"]["ucx"] if name == "SM_Minka_Shell" else 1
        count = convex_count(mesh)
        assert count == expected, f"{name}: {count} convexes, {expected} expected"
        body = mesh.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", source_body.get_editor_property("collision_trace_flag"))
        nanite = mesh.get_editor_property("nanite_settings")
        nanite.enabled = name == "SM_Minka_Shell" and source.get_editor_property("nanite_settings").enabled
        mesh.set_editor_property("nanite_settings", nanite)
        box = mesh.get_bounding_box()
        save(mesh)
        report[name] = {"slots": slots, "convexes": count, "nanite": nanite.enabled,
                        "size_cm": [round(box.max.x - box.min.x, 1), round(box.max.y - box.min.y, 1),
                                    round(box.max.z - box.min.z, 1)]}

    table, kept, dropped = house_loot()
    relic = EAL.load_blueprint_class(RELIC)
    assert relic, RELIC
    kit = load(KIT)
    if not kit:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.VillageHouseKit)
        kit = TOOLS.create_asset("DA_MinkaKit", DATA, ue.VillageHouseKit, factory)
    assert kit, KIT

    def vec(values):
        return ue.Vector(float(values[0]), float(values[1]), float(values[2]))

    panels = []
    for p in doc["panels"]:
        panel = ue.VillageHousePanel()
        panel.set_editor_property("mesh", meshes[p["mesh"]])
        panel.set_editor_property("closed_location", vec(p["closed"]))
        panel.set_editor_property("yaw", float(p["yaw"]))
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

    def slots_of(entries):
        out = []
        for e in entries:
            slot = ue.VillageHouseSlot()
            slot.set_editor_property("location", vec(e["location"]))
            slot.set_editor_property("yaw", float(e.get("yaw", 0.0)))
            slot.set_editor_property("room", e["room"])
            out.append(slot)
        return out

    kit.set_editor_property("source_mesh", source)
    kit.set_editor_property("shell_mesh", meshes["SM_Minka_Shell"])
    kit.set_editor_property("panels", panels)
    kit.set_editor_property("doorways", doorways)
    kit.set_editor_property("relic_class", relic)
    kit.set_editor_property("relic_slots", slots_of(doc["slots"]["relic"]))
    kit.set_editor_property("item_table", table)
    kit.set_editor_property("item_slots", slots_of(doc["slots"]["items"]))
    save(kit)

    rules = load(RULES)
    assert rules, RULES
    kits = [k for k in rules.get_editor_property("house_kits") if k and k.get_path_name() != kit.get_path_name()]
    rules.set_editor_property("house_kits", kits + [kit])
    save(rules)

    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-186", "meshes": report, "kit": KIT, "panels": len(panels), "doorways": len(doorways),
              "relic": RELIC, "item_table": HOUSE_LOOT, "items_kept_cm": kept, "items_dropped_cm": dropped,
              "item_slots": len(doc["slots"]["items"]), "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "minka-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T186_MINKA_SUCCESS")


if __name__ == "__main__":
    main()
