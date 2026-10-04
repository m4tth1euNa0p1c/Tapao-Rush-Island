"""Fishermen camp, second step (after the MCP import of the textures and meshes, T-082): materials, mesh slots, the
items (tools worth stealing, fish to eat) and DA_CampRules.

Editor commandlet (editor closed, editor built with UCampRules / ACampSite / ACampSpawner):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Reads art/export/Props/Camp/camp_materials.json (scripts/props/camp_textures.py) and
docs/reference/map/raraka/collectibles/camp_spots.json (scripts/map/place_camps.py). Idempotent.
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEST = "/Game/MonkeIsland/Environment/Camp"
MAT, TEX, MESH, BP = f"{DEST}/Materials", f"{DEST}/Textures", f"{DEST}/Meshes", f"{DEST}/Blueprints"
SURFACE = "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_Surface"
SPECKLE = "/Game/MonkeIsland/Items/Loot/Materials/M_Loot_Speckle"
FLAME = "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_Flame"
FLAME_CORE = "/Game/MonkeIsland/Environment/Marae/Materials/MI_Marae_FlameCore"
FLICKER = "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_TorchFlicker"
LOOT_RULES = "/Game/MonkeIsland/Items/Loot/DA_LootRules"
SPEC = json.load(open(os.path.join(ROOT, "art", "export", "Props", "Camp", "camp_materials.json"), encoding="utf-8"))
SPOTS = json.load(open(os.path.join(ROOT, "docs", "reference", "map", "raraka", "collectibles", "camp_spots.json"),
                       encoding="utf-8"))["spots"]
# tools in the order of UCampRules::Tools: rod, trap, net, basket
TOOLS = ("SM_Camp_FishingRod", "SM_Camp_FishTrap", "SM_Camp_Net", "SM_Camp_FishBasket")

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning(f"[camp] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def mi(name, parent, scalars=None, vectors=None, textures=None):
    inst = load(f"{MAT}/{name}") or tools.create_asset(name, MAT, unreal.MaterialInstanceConstant,
                                                       unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(inst, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(inst, k, v)
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(inst, k, v)
    mel.update_material_instance(inst)
    eal.save_loaded_asset(inst)
    return inst


def blueprint(name, parent):
    bp = load(f"{BP}/{name}")
    if bp:
        return bp
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    return tools.create_asset(name, BP, unreal.Blueprint, factory)


def data_asset(path, cls):
    asset = load(path)
    if asset:
        return asset
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(os.path.basename(path), os.path.dirname(path), cls, factory)


def main():
    surface, speckle = load(SURFACE), load(SPECKLE)
    mats = {}
    for slot, entry in SPEC["materials"].items():
        t = entry["textures"]
        mats[slot] = mi("MI_" + slot[2:], surface, scalars={"Tiling": entry["tiling"]},
                        textures={k: load(f"{TEX}/{t[s][:-4]}") for k, s in (("BaseColor", "D"), ("Normal", "N"), ("ORM", "ORM"))})
    for slot, s in SPEC["speckle"].items():
        mats[slot] = mi("MI_" + slot[2:], speckle, scalars={"Roughness": s["roughness"], "NoiseScale": s["scale"]},
                        vectors={"ColorA": unreal.LinearColor(*s["a"], 1), "ColorB": unreal.LinearColor(*s["b"], 1)})
    mats["M_Camp_Flame"] = load(FLAME)
    mats["M_Camp_FlameCore"] = load(FLAME_CORE)
    mats["M_Camp_Embers"] = mi("MI_Camp_Embers", load(FLAME), scalars={"Intensity": 3.0, "Sway": 0.0},
                               vectors={"Color": unreal.LinearColor(1.0, 0.22, 0.03, 1)})
    log(f"{len(mats)} materials")

    meshes = {}
    for fbx in sorted(os.listdir(os.path.join(ROOT, "art", "export", "Props", "Camp"))):
        if not fbx.endswith(".fbx"):
            continue
        name = fbx[:-4]
        sm = load(f"{MESH}/{name}")
        if not sm:
            log(f"{name} missing")
            continue
        for index, slot in enumerate(sm.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            if mats.get(slot_name):
                sm.set_material(index, mats[slot_name])
            else:
                log(f"{name}: no material for {slot_name}")
        for socket_name in SPEC["parts"].get(name, {}).get("sockets", []):
            socket = sm.find_socket(socket_name.replace("SOCKET_", ""))
            if socket:
                socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
        eal.save_loaded_asset(sm)
        meshes[name] = sm

    # tools worth stealing: physics props with their price as value
    tool_classes = []
    for name in TOOLS:
        part = SPEC["parts"][name]
        bp = blueprint(name.replace("SM_", "BP_"), unreal.PhysicsPropActor)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property("mass_kg", max(0.1, part["mass_kg"]))
        cdo.get_editor_property("mesh").set_editor_property("static_mesh", meshes[name])
        valuable = cdo.get_editor_property("valuable")
        valuable.set_editor_property("base_value", int(part["price"]))
        valuable.set_editor_property("safe_impact_speed", 500.0)
        valuable.set_editor_property("break_at_zero", False)
        cdo.modify()
        eal.save_loaded_asset(bp, False)
        tool_classes.append(bp.generated_class())
        log(f"{bp.get_name()}: {part['label']} {part['price']} $ {part['mass_kg']} kg")

    # fish: eaten with the interaction key, gives health back
    fish_def = data_asset(f"{DEST}/DA_Consumable_Fish", unreal.ConsumableDefinition)
    fish_def.set_editor_property("action_text", unreal.Text("Manger le poisson"))
    fish_def.set_editor_property("heal_amount", 15.0)
    fish_def.set_editor_property("mesh", meshes.get("SM_Camp_Fish"))
    eal.save_loaded_asset(fish_def)
    fish_bp = blueprint("BP_Camp_Fish", unreal.ConsumablePickupActor)
    unreal.BlueprintEditorLibrary.compile_blueprint(fish_bp)
    fish_cdo = unreal.get_default_object(fish_bp.generated_class())
    fish_cdo.set_editor_property("definition", fish_def)
    fish_cdo.get_editor_property("mesh").set_editor_property("static_mesh", meshes.get("SM_Camp_Fish"))
    fish_cdo.modify()
    eal.save_loaded_asset(fish_bp, False)
    log("BP_Camp_Fish: +15 PV")

    rules = data_asset(f"{DEST}/DA_CampRules", unreal.CampRules)
    rules.set_editor_property("hut_mesh", meshes.get("SM_Camp_Hut"))
    rules.set_editor_property("shelter_mesh", meshes.get("SM_Camp_WorkShelter"))
    rules.set_editor_property("campfire_mesh", meshes.get("SM_Camp_Campfire"))
    rules.set_editor_property("log_seat_mesh", meshes.get("SM_Camp_LogSeat"))
    rules.set_editor_property("drying_rack_mesh", meshes.get("SM_Camp_DryingRack"))
    rules.set_editor_property("fire_light_function", load(FLICKER))
    # new structs: the ones read back from the asset are copies, setting their class would be lost
    items = []
    for old, cls in zip(rules.get_editor_property("tools"), tool_classes):
        item = unreal.CampItem()
        item.set_editor_property("item_class", cls)
        item.set_editor_property("transform", old.get_editor_property("transform"))
        items.append(item)
    rules.set_editor_property("tools", items)
    rules.set_editor_property("fish_class", fish_bp.generated_class())
    loot = load(LOOT_RULES)
    if loot:
        rules.set_editor_property("hut_loot_classes", [e.get_editor_property("item_class") for e in loot.get_editor_property("items")])
    rules.set_editor_property("spots", [unreal.Vector(*s) for s in SPOTS])
    eal.save_loaded_asset(rules)
    log(f"DA_CampRules: {len(items)} tools, {len(SPOTS)} spots, count {rules.get_editor_property('count')}")


main()
