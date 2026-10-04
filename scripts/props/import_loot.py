"""Common loot, second step (after the MCP import of the textures and meshes, T-076): materials, MIs, mesh slots,
the 12 blueprints BP_Loot_* (APhysicsPropActor with a value) and DA_LootRules (kinds, weights, ground spots).

Editor commandlet (editor closed, editor built with ULootRules / ALootScatterSpawner):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Reads art/export/Props/Loot/loot_materials.json (scripts/props/loot_textures.py) and
docs/reference/map/raraka/collectibles/loot_spots.json (scripts/map/place_loot.py). Idempotent.
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEST = "/Game/MonkeIsland/Items/Loot"
MAT = f"{DEST}/Materials"
TEX = f"{DEST}/Textures"
MESH = f"{DEST}/Meshes"
BP = f"{DEST}/Blueprints"
SURFACE = "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_Surface"
COWRIE = "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_Cowrie"
SPEC = json.load(open(os.path.join(ROOT, "art", "export", "Props", "Loot", "loot_materials.json"), encoding="utf-8"))
SPOTS = json.load(open(os.path.join(ROOT, "docs", "reference", "map", "raraka", "collectibles", "loot_spots.json"),
                       encoding="utf-8"))["spots"]

# value of each kind (owner request 2026-10-03: 5 to 40 $), the maquette prices (1 to 6) kept in their order
VALUES = {"SM_Loot_WoodenSpoon": 5, "SM_Loot_SmoothStone": 5, "SM_Loot_WoodenComb": 8, "SM_Loot_CoconutBowl": 10,
          "SM_Loot_RopeCoil": 15, "SM_Loot_Calabash": 15, "SM_Loot_LineSpool": 15, "SM_Loot_Basket": 25,
          "SM_Loot_RolledMat": 25, "SM_Loot_SeedNecklace": 30, "SM_Loot_TiedShells": 35, "SM_Loot_SmallChest": 40}

# relative chance of each kind: everyday things often, the little chest rarely
WEIGHTS = {"SM_Loot_SmallChest": 0.35, "SM_Loot_Basket": 0.6, "SM_Loot_RolledMat": 0.6, "SM_Loot_SeedNecklace": 0.7,
           "SM_Loot_TiedShells": 0.8}

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty


def log(msg):
    unreal.log_warning(f"[loot] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def build_speckle():
    """Two colours mixed by a noise in the local space (it does not slide when the object moves), roughness"""
    m = load(f"{MAT}/M_Loot_Speckle") or tools.create_asset("M_Loot_Speckle", MAT, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)

    def node(cls, **props):
        e = mel.create_material_expression(m, cls, -600, 0)
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    pos = node(unreal.MaterialExpressionLocalPosition)
    scale = node(unreal.MaterialExpressionScalarParameter, parameter_name="NoiseScale", default_value=1.0)
    scaled = node(unreal.MaterialExpressionMultiply)
    mel.connect_material_expressions(pos, "", scaled, "A")
    mel.connect_material_expressions(scale, "", scaled, "B")
    noise = node(unreal.MaterialExpressionNoise, scale=1.0, output_min=0.0, output_max=1.0)
    mel.connect_material_expressions(scaled, "", noise, "Position")
    col = node(unreal.MaterialExpressionLinearInterpolate)
    a = node(unreal.MaterialExpressionVectorParameter, parameter_name="ColorA", default_value=unreal.LinearColor(0.3, 0.2, 0.1, 1))
    b = node(unreal.MaterialExpressionVectorParameter, parameter_name="ColorB", default_value=unreal.LinearColor(0.6, 0.4, 0.2, 1))
    mel.connect_material_expressions(a, "", col, "A")
    mel.connect_material_expressions(b, "", col, "B")
    mel.connect_material_expressions(noise, "", col, "Alpha")
    rough = node(unreal.MaterialExpressionScalarParameter, parameter_name="Roughness", default_value=0.5)
    mel.connect_material_property(col, "", MP.MP_BASE_COLOR)
    mel.connect_material_property(rough, "", MP.MP_ROUGHNESS)
    mel.layout_material_expressions(m)
    mel.recompile_material(m)
    eal.save_loaded_asset(m)
    return m


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


def blueprint(name):
    bp = load(f"{BP}/{name}")
    if bp:
        return bp
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.PhysicsPropActor)
    return tools.create_asset(name, BP, unreal.Blueprint, factory)


def main():
    surface = load(SURFACE)
    mats = {"M_Marae_Cowrie": load(COWRIE)}
    for slot, entry in SPEC["materials"].items():
        t = entry["textures"]
        mats[slot] = mi("MI_" + slot[2:], surface, scalars={"Tiling": entry["tiling"]},
                        textures={k: load(f"{TEX}/{t[s][:-4]}") for k, s in (("BaseColor", "D"), ("Normal", "N"), ("ORM", "ORM"))})
    speckle = build_speckle()
    for slot, s in SPEC["speckle"].items():
        mats[slot] = mi("MI_" + slot[2:], speckle, scalars={"Roughness": s["roughness"], "NoiseScale": s["scale"]},
                        vectors={"ColorA": unreal.LinearColor(*s["a"], 1), "ColorB": unreal.LinearColor(*s["b"], 1)})
    log(f"{len(mats)} materials")

    entries = []
    for name, item in SPEC["items"].items():
        sm = load(f"{MESH}/{name}")
        if not sm:
            log(f"{name} missing")
            continue
        for index, slot in enumerate(sm.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            if mats.get(slot_name):
                sm.set_material(index, mats[slot_name])
            else:
                log(f"{name}: no material for slot {slot_name}")
        socket = sm.find_socket("grab")
        if socket:
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
        eal.save_loaded_asset(sm)

        bp_name = name.replace("SM_", "BP_")
        bp = blueprint(bp_name)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = unreal.get_default_object(bp.generated_class())
        cdo.set_editor_property("mass_kg", max(0.1, item["mass_kg"]))
        cdo.get_editor_property("mesh").set_editor_property("static_mesh", sm)
        valuable = cdo.get_editor_property("valuable")
        valuable.set_editor_property("base_value", VALUES.get(name, int(item["value"])))
        # small everyday things: a throw does not break them, only a real fall costs a little
        valuable.set_editor_property("safe_impact_speed", 600.0)
        valuable.set_editor_property("break_at_zero", False)
        cdo.modify()
        eal.save_loaded_asset(bp, False)
        entry = unreal.LootEntry()
        entry.set_editor_property("item_class", bp.generated_class())
        entry.set_editor_property("weight", WEIGHTS.get(name, 1.0))
        entries.append(entry)
        log(f"{bp_name}: {item['label']} {VALUES.get(name, item['value'])} $ {item['mass_kg']} kg")

    path = f"{DEST}/DA_LootRules"
    rules = load(path)
    if not rules:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.LootRules)
        rules = tools.create_asset("DA_LootRules", DEST, unreal.LootRules, factory)
    rules.set_editor_property("items", entries)
    rules.set_editor_property("spots", [unreal.Vector(*s) for s in SPOTS])
    # everywhere on the island (owner request): 1200 items 20 m apart, under the vegetation too, spawned in batches
    rules.set_editor_property("count", 1200)
    rules.set_editor_property("min_spacing", 2000.0)
    rules.set_editor_property("max_distance_from_start", 0.0)
    rules.set_editor_property("spawn_batch_size", 80)
    eal.save_loaded_asset(rules)
    log(f"DA_LootRules: {len(entries)} kinds, {len(SPOTS)} spots, count {rules.get_editor_property('count')}")


main()
