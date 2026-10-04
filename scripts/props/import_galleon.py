"""English galleon at the bean's scale (maquette galion-anglais of the second PC, cahier §24, T-110): the 6 static
meshes, their materials and DA_GalleonRules.

Sources copied from OneDrive (read only there) into art/export/Galleon: SM_Galleon*.fbx, galleon-report.json.
In the editor (it quits at the end), editor built with UGalleonRules / AGalleonActor:
  UnrealEditor.exe monke_island.uproject -ExecutePythonScript="<this file>"
Idempotent. AGalleonSpawner of Lvl_Raraka gets the asset by MCP (Rules).
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
SRC = os.path.join(ROOT, "art", "export", "Galleon")
REPORT = json.load(open(os.path.join(SRC, "galleon-report.json"), encoding="utf-8"))
DEST = "/Game/MonkeIsland/Galleon"
MESH = f"{DEST}/Meshes"
MAT = f"{DEST}/Materials"
SPECKLE = "/Game/MonkeIsland/Items/Loot/Materials/M_Loot_Speckle"
HULL_LENGTH_CM = 2200.0
PARTS = ("SM_Galleon", "SM_Galleon_Sails", "SM_Galleon_Rigging", "SM_Galleon_Flag", "SM_Galleon_Wheel", "SM_Galleon_Rudder")

# slot of the maquette -> existing material of the project
EXISTING = {
    "M_Galleon_Gold": "/Game/MonkeIsland/Environment/Marae/Materials/M_Marae_Gold",
    "M_Galleon_Iron": "/Game/MonkeIsland/Weapons/Torch/MI_Torch_Iron",
    "M_Galleon_Deck": "/Game/MonkeIsland/Environment/Camp/Materials/MI_Camp_Planks",
    "M_Galleon_Wood": "/Game/MonkeIsland/Environment/Camp/Materials/MI_Camp_CutWood",
    "M_Galleon_Spar": "/Game/MonkeIsland/Environment/Camp/Materials/MI_Camp_Post",
    "M_Galleon_Rope": "/Game/MonkeIsland/Environment/Camp/Materials/MI_Camp_Rope",
    "M_Galleon_Rigging": "/Game/MonkeIsland/Environment/Camp/Materials/MI_Camp_Rope",
    "M_Galleon_LampGlow": "/Game/MonkeIsland/Environment/Marae/Materials/MI_Marae_FlameCore",
}
# slot -> two colours mixed by a noise, roughness, two sided
SPECKLES = {
    "M_Galleon_Tar": ((0.020, 0.017, 0.014), (0.045, 0.038, 0.030), 0.65, False),
    "M_Galleon_HullWood": ((0.230, 0.130, 0.060), (0.330, 0.200, 0.100), 0.70, False),
    "M_Galleon_Blue": ((0.050, 0.160, 0.420), (0.080, 0.220, 0.520), 0.55, False),
    "M_Galleon_PortDark": ((0.020, 0.016, 0.012), (0.035, 0.028, 0.022), 0.90, False),
    "M_Galleon_Glass": ((0.400, 0.560, 0.680), (0.550, 0.700, 0.800), 0.08, False),
    "M_Galleon_DarkWood": ((0.090, 0.050, 0.025), (0.150, 0.085, 0.040), 0.70, False),
    "M_Galleon_Rug": ((0.080, 0.110, 0.280), (0.110, 0.150, 0.340), 0.90, False),
    "M_Galleon_Map": ((0.820, 0.730, 0.520), (0.900, 0.820, 0.620), 0.80, False),
    "M_Galleon_Blanket": ((0.130, 0.190, 0.460), (0.180, 0.240, 0.520), 0.85, False),
    "M_Galleon_Linen": ((0.860, 0.830, 0.760), (0.930, 0.910, 0.850), 0.85, False),
    "M_Galleon_Canvas": ((0.800, 0.740, 0.600), (0.900, 0.850, 0.720), 0.85, True),
    "M_Galleon_Flag": ((0.900, 0.880, 0.840), (0.960, 0.950, 0.920), 0.80, True),
}

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def log(msg):
    unreal.log_warning(f"[galleon] {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def speckle_mi(slot, a, b, rough, two_sided):
    name = "MI_" + slot[2:]
    inst = load(f"{MAT}/{name}") or tools.create_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, load(SPECKLE))
    mel.set_material_instance_scalar_parameter_value(inst, "Roughness", rough)
    mel.set_material_instance_scalar_parameter_value(inst, "NoiseScale", 0.6)
    mel.set_material_instance_vector_parameter_value(inst, "ColorA", unreal.LinearColor(*a, 1))
    mel.set_material_instance_vector_parameter_value(inst, "ColorB", unreal.LinearColor(*b, 1))
    if two_sided:
        try:
            overrides = inst.get_editor_property("base_property_overrides")
            overrides.set_editor_property("override_two_sided", True)
            overrides.set_editor_property("two_sided", True)
            inst.set_editor_property("base_property_overrides", overrides)
        except Exception as error:   # noqa: BLE001 - the sails then show from the front only
            log(f"{name}: two sided not set ({error})")
    mel.update_material_instance(inst)
    eal.save_loaded_asset(inst)
    return inst


def import_mesh(name, scale):
    opts = unreal.FbxImportUI()
    opts.import_mesh = True
    opts.import_as_skeletal = False
    opts.import_materials = False
    opts.import_textures = False
    opts.import_animations = False
    opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
    data = opts.static_mesh_import_data
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("import_uniform_scale", scale)
    task = unreal.AssetImportTask()
    task.filename = os.path.join(SRC, name + ".fbx")
    task.destination_path = MESH
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = True
    task.options = opts
    tools.import_asset_tasks([task])
    return load(f"{MESH}/{name}")


def main():
    mats = {slot: load(path) for slot, path in EXISTING.items()}
    for slot, (a, b, rough, two_sided) in SPECKLES.items():
        mats[slot] = speckle_mi(slot, a, b, rough, two_sided)

    # an FBX in metres comes in 100 times too small: the hull tells
    scale = 1.0
    hull = import_mesh("SM_Galleon", scale)
    box = hull.get_bounding_box()
    if max(box.max.x - box.min.x, box.max.y - box.min.y) < HULL_LENGTH_CM / 10.0:
        scale = 100.0
        hull = import_mesh("SM_Galleon", scale)
        box = hull.get_bounding_box()
    log(f"SM_Galleon: {box.max.x - box.min.x:.0f} x {box.max.y - box.min.y:.0f} x {box.max.z - box.min.z:.0f} cm, "
        f"x {box.min.x:.0f}..{box.max.x:.0f} y {box.min.y:.0f}..{box.max.y:.0f} z {box.min.z:.0f}..{box.max.z:.0f} (scale {scale})")

    meshes = {}
    for name in PARTS:
        sm = hull if name == "SM_Galleon" else import_mesh(name, scale)
        if not sm:
            log(f"{name} not imported")
            continue
        for index, slot in enumerate(sm.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            if mats.get(slot_name):
                sm.set_material(index, mats[slot_name])
            else:
                log(f"{name}: no material for slot {slot_name}")
        meshes[name] = sm

    # walkable decks, stairs, cabin walls and railings: the mesh itself is the collision (kinematic ship)
    body = hull.get_editor_property("body_setup")
    body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    sockets = {}
    for socket_name in REPORT["sockets"]:
        socket = hull.find_socket(socket_name)
        if socket:
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
            loc = socket.get_editor_property("relative_location")
            sockets[socket_name] = (round(loc.x), round(loc.y), round(loc.z))
    log(f"sockets: {sockets}")
    for sm in meshes.values():
        eal.save_loaded_asset(sm)

    path = f"{DEST}/DA_GalleonRules"
    rules = load(path)
    if not rules:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.GalleonRules)
        rules = tools.create_asset("DA_GalleonRules", DEST, unreal.GalleonRules, factory)
    for prop, name in (("hull_mesh", "SM_Galleon"), ("sails_mesh", "SM_Galleon_Sails"), ("rigging_mesh", "SM_Galleon_Rigging"),
                       ("flag_mesh", "SM_Galleon_Flag"), ("wheel_mesh", "SM_Galleon_Wheel"), ("rudder_mesh", "SM_Galleon_Rudder")):
        rules.set_editor_property(prop, meshes.get(name))
    # pivots of the separate parts: Blender metres (x, y, z) -> Unreal cm (x, -y, z)
    for prop, name in (("flag_pivot", "SM_Galleon_Flag"), ("wheel_pivot", "SM_Galleon_Wheel"), ("rudder_pivot", "SM_Galleon_Rudder")):
        x, y, z = REPORT["exports"][name]["pivot_in_SM_Galleon_m"]
        rules.set_editor_property(prop, unreal.Vector(x * 100.0, -y * 100.0, z * 100.0))
    eal.save_loaded_asset(rules)
    log("DA_GalleonRules saved")


main()
