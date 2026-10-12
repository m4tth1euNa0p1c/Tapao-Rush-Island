"""The realistic coconut mobs of the 2nd PC (maquette mob-coco-realiste, 2026-10-08) on the mobs and NPCs of the game
(owner 2026-10-08: "exporte les nouveaux skins sur les PNJ et mobs"), in Unreal.

For the Guerrier Coco (mob, BP_TikiCocoWarrior) and the Villageois (NPC, BP_TikiVillager and the villagers of the houses):
- the baked textures (art/export/Characters/CocoReal/<Mob>/Textures, copies of the maquette's exports): BaseColor sRGB,
  Normal (OpenGL: green flipped), ORM linear masks, in /Game/MonkeIsland/Characters/<Mob>/Textures;
- M_Character_Baked (/Game/MonkeIsland/Characters/_Shared/Materials, created once: BaseColor, ORM = AO / roughness /
  metallic, Normal, Emissive x EmissiveStrength) and MI_<Mob>_Body / _Weapon / _Gore;
- SK_<Mob> reimported IN PLACE from the new LOD0 (54 k tris, morph targets) on its own skeleton and physics asset: the
  blueprints, animations, dismember sets and maps keep their references; then LOD1 / LOD2 (50 % / 20 %, the engine's
  screen sizes 0.5 / 0.25: LODInfo is out of reach of Python in 5.8);
- the weapon (SK_CocoWarrior_Spear, SK_Villager_FruitBasket) and the 7 dismember modules (Head, Torso, Pelvis, Arm_L/R,
  Leg_L/R) reimported in place the same way, their slots on the same MIs (the module slots follow the body by slot name).
The skeletons and bone names of the maquette are those of the game (README_UE of the maquette), the clips stay.
The maquette's own script (ue/import_coco_real.py) is not run: it builds new skeletons, anims and blendspaces.
Runs in the editor: scripts/editor/lead_session_t186.py, action "cocoreal" (editor lock held, no game running).
Writes monke_island/Saved/T186/coco-real.json.
"""
import datetime as dt
import json
import sys
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

ART = ROOT / "art/export/Characters/CocoReal"
OUT = ROOT / "monke_island/Saved/T186/coco-real.json"
CHARS = "/Game/MonkeIsland/Characters"
MASTER = CHARS + "/_Shared/Materials/M_Character_Baked"
MOBS = {
    "CocoWarrior": {"weapon": "SK_CocoWarrior_Spear"},
    "Villager": {"weapon": "SK_Villager_FruitBasket"},
}
MODULES = ["Head", "Torso", "Pelvis", "Arm_L", "Arm_R", "Leg_L", "Leg_R"]
PARTS = ["Body", "Weapon", "Gore"]
LOD_SCREEN = {1: 0.45, 2: 0.2}
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
MEL = ue.MaterialEditingLibrary
report = {}


def import_task(filename, dest, name, options=None):
    task = ue.AssetImportTask()
    task.filename = str(filename)
    task.destination_path = dest
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    if options is not None:
        task.options = options
    TOOLS.import_asset_tasks([task])
    return EAL.load_asset(f"{dest}/{name}")


def master_material():
    if EAL.does_asset_exist(MASTER):
        return EAL.load_asset(MASTER)
    folder, name = MASTER.rsplit("/", 1)
    material = TOOLS.create_asset(name, folder, ue.Material, ue.MaterialFactoryNew())
    prop = ue.MaterialProperty

    def texture(param, x, y, sampler):
        node = MEL.create_material_expression(material, ue.MaterialExpressionTextureSampleParameter2D, x, y)
        node.set_editor_property("parameter_name", param)
        node.set_editor_property("sampler_type", sampler)
        return node

    base = texture("BaseColor", -600, -300, ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    orm = texture("ORM", -600, 0, ue.MaterialSamplerType.SAMPLERTYPE_MASKS)
    normal = texture("Normal", -600, 300, ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    emissive = texture("Emissive", -600, 600, ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    strength = MEL.create_material_expression(material, ue.MaterialExpressionScalarParameter, -600, 850)
    strength.set_editor_property("parameter_name", "EmissiveStrength")
    strength.set_editor_property("default_value", 0.0)
    times = MEL.create_material_expression(material, ue.MaterialExpressionMultiply, -250, 650)
    MEL.connect_material_property(base, "RGB", prop.MP_BASE_COLOR)
    MEL.connect_material_property(orm, "R", prop.MP_AMBIENT_OCCLUSION)
    MEL.connect_material_property(orm, "G", prop.MP_ROUGHNESS)
    MEL.connect_material_property(orm, "B", prop.MP_METALLIC)
    MEL.connect_material_property(normal, "RGB", prop.MP_NORMAL)
    MEL.connect_material_expressions(emissive, "RGB", times, "A")
    MEL.connect_material_expressions(strength, "", times, "B")
    MEL.connect_material_property(times, "", prop.MP_EMISSIVE_COLOR)
    material.set_editor_property("used_with_skeletal_mesh", True)
    MEL.recompile_material(material)
    kitlib.save(material)
    return material


def texture_asset(mob, part, kind):
    name = f"T_{mob}_{part}_{kind}"
    tex = import_task(ART / mob / "Textures" / f"{name}.png", f"{CHARS}/{mob}/Textures", name)
    assert tex, name
    if kind == "N":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", True)
    elif kind == "ORM":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    kitlib.save(tex)
    return tex


def material_instances(mob, master):
    out = {}
    folder = f"{CHARS}/{mob}/Materials"
    for part in PARTS:
        name = f"MI_{mob}_{part}"
        mi = kitlib.load(f"{folder}/{name}") or TOOLS.create_asset(name, folder, ue.MaterialInstanceConstant,
                                                                     ue.MaterialInstanceConstantFactoryNew())
        MEL.set_material_instance_parent(mi, master)
        for kind, param in (("BC", "BaseColor"), ("ORM", "ORM"), ("N", "Normal")):
            MEL.set_material_instance_texture_parameter_value(mi, param, texture_asset(mob, part, kind))
        MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.0)
        kitlib.save(mi)
        out[f"M_{mob}_{part}"] = mi
    return out


def skeletal_options(skeleton, physics):
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = True
    options.skeleton = skeleton
    if physics:
        options.physics_asset = physics
    options.create_physics_asset = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_SKELETAL_MESH
    data = options.skeletal_mesh_import_data
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("force_front_x_axis", False)
    data.set_editor_property("convert_scene_unit", False)
    data.set_editor_property("update_skeleton_reference_pose", False)
    data.set_editor_property("use_t0_as_ref_pose", False)
    data.set_editor_property("import_meshes_in_bone_hierarchy", True)
    data.set_editor_property("import_morph_targets", True)
    return options


def assign(mesh, mis):
    """MIs on the slots by the material each one imported (the reimport keeps the old slot list: the new sections take
    old slots and rename their imported material); the other slots, unused by any section, keep theirs"""
    # the array's items are copies (UE 5.8): the edited copies make the new array
    slots = list(mesh.get_editor_property("materials"))
    used = []
    for index, slot in enumerate(slots):
        imported = str(slot.get_editor_property("imported_material_slot_name"))
        mi = mis.get(imported)
        if mi:
            slot.set_editor_property("material_interface", mi)
            slots[index] = slot
            used.append([str(slot.get_editor_property("material_slot_name")), imported, mi.get_name()])
    mesh.set_editor_property("materials", slots)
    shown = {str(s.get_editor_property("imported_material_slot_name")): s.get_editor_property("material_interface")
             for s in mesh.get_editor_property("materials")}
    for _, imported, name in used:
        assert shown[imported] and shown[imported].get_name() == name, f"{mesh.get_name()}: {imported} kept {shown[imported]}"
    return used


def reimport(path, fbx, skeleton):
    """Reimports a skeletal mesh in place on its skeleton and physics asset"""
    before = kitlib.load(path)
    assert before, path
    physics = before.get_editor_property("physics_asset")
    folder, name = path.rsplit("/", 1)
    mesh = import_task(fbx, folder, name, skeletal_options(skeleton, physics))
    assert mesh and mesh.get_editor_property("skeleton") == skeleton, f"{name}: the reimport changed the skeleton"
    if physics and mesh.get_editor_property("physics_asset") != physics:
        mesh.set_editor_property("physics_asset", physics)
    return mesh


def import_lods(mesh, mob):
    sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    done = []
    for lod, screen in LOD_SCREEN.items():
        fbx = ART / mob / "LOD" / f"SK_{mob}_LOD{lod}.fbx"
        try:
            result = sub.import_lod(mesh, lod, str(fbx))
            done.append([lod, result])
        except Exception as error:  # noqa: BLE001
            done.append([lod, f"failed: {error}"])
    # the screen sizes of the LODs are not reachable from Python in 5.8 (LODInfo): the engine's own (each LOD half the
    # previous one: 1 / 0.5 / 0.25) are near those of the maquette (1 / 0.45 / 0.2)
    return done, sub.get_lod_count(mesh)


def vertices(mesh):
    """Vertices of each LOD of a skeletal mesh"""
    sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
    out = []
    for lod in range(sub.get_lod_count(mesh)):
        try:
            out.append(sub.get_num_verts(mesh, lod))
        except Exception:  # noqa: BLE001
            out.append(None)
    return out


def assign_only(mob, spec):
    """The MIs on the meshes already reimported (no import)"""
    folder = f"{CHARS}/{mob}/Materials"
    mis = {f"M_{mob}_{part}": kitlib.load(f"{folder}/MI_{mob}_{part}") for part in PARTS}
    entry = {}
    for path in [f"{CHARS}/{mob}/Meshes/SK_{mob}", f"{CHARS}/{mob}/Meshes/{spec['weapon']}"] + [
            f"{CHARS}/{mob}/Dismember/SK_{mob}_{module}" for module in MODULES]:
        mesh = kitlib.load(path)
        entry[path.rsplit("/", 1)[-1]] = assign(mesh, mis)
        kitlib.save(mesh)
    return entry


def do_mob(mob, spec, master):
    entry = {}
    skeleton = kitlib.load(f"{CHARS}/{mob}/Meshes/SK_{mob}_Skeleton")
    assert skeleton, f"SK_{mob}_Skeleton missing"
    mis = material_instances(mob, master)
    entry["mis"] = sorted(m.get_name() for m in mis.values())
    body = reimport(f"{CHARS}/{mob}/Meshes/SK_{mob}", ART / mob / "SK" / f"SK_{mob}.fbx", skeleton)
    entry["lods"], entry["lod_count"] = import_lods(body, mob)
    entry["body_slots"] = assign(body, mis)
    entry["verts_per_lod"] = vertices(body)
    kitlib.save(body)
    weapon = reimport(f"{CHARS}/{mob}/Meshes/{spec['weapon']}", ART / mob / "SK" / f"{spec['weapon']}.fbx", skeleton)
    entry["weapon_slots"] = assign(weapon, mis)
    kitlib.save(weapon)
    modules = {}
    for module in MODULES:
        try:
            mesh = reimport(f"{CHARS}/{mob}/Dismember/SK_{mob}_{module}", ART / mob / "Dismember" / f"SK_{mob}_{module}.fbx", skeleton)
            modules[module] = assign(mesh, mis)
            kitlib.save(mesh)
        except Exception:  # noqa: BLE001
            modules[module] = {"error": traceback.format_exc()[-600:]}
    entry["modules"] = modules
    return entry


def main():
    kitlib.check_lock()
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        master = master_material()
        report["master"] = master.get_path_name()
        request_file = ROOT / "monke_island/Saved/T186/coco-real-request.json"
        request = json.loads(request_file.read_text(encoding="utf-8")) if request_file.exists() else {}
        for mob in request.get("mobs", list(MOBS)):
            try:
                report[mob] = assign_only(mob, MOBS[mob]) if request.get("assign_only") else do_mob(mob, MOBS[mob], master)
            except Exception:  # noqa: BLE001
                report[mob] = {"error": traceback.format_exc()[-1500:]}
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")


if __name__ == "__main__":
    main()
