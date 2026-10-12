"""T-258 (lot L2, cahier §5-6): the wooden loot crate of the 2nd PC (maquette caisse-transport v0.4.0) in Unreal.

Source: art/export/Props/LootCrate (copies of OneDrive\\maquette-blender\\maquettes\\caisse-transport\\exports and
textures, read only there). Imports into /Game/MonkeIsland/Props/LootCrate:
  Textures/  T_LootCrate_BaseColor (sRGB), T_LootCrate_Normal (DirectX: green kept), T_LootCrate_ORM (linear masks)
  Materials/ MI_LootCrate, child of M_Character_Baked (BaseColor, ORM = AO / roughness / metallic, Normal; created by
             scripts/character/import_coco_real_ue.py when missing)
  Meshes/    SM_LootCrate (LOD0 + LOD1 / LOD2 of the maquette, its 5 UCX kept: hollow inside, sockets Grab_L/R, Carry,
             BoatSlot, Lid, LootZone, Slot_00..29) and SM_LootCrate_Lid (LOD0-2, 2 UCX, socket Grab)
Legacy FBX importer (Interchange off during the import: UCX, sockets and custom LODs), scale 1.0, no auto collision,
no material import, no Nanite (a small physics prop with its own LODs).
Runs in the editor: scripts/editor/lead_session_t186.py, action "crateimport" (editor lock held, no game running).
Idempotent: a new run reimports in place (references kept). Writes monke_island/Saved/T258/loot-crate-import.json
(sizes in cm, pivot, LODs, collision, sockets, materials).
"""
import datetime as dt
import json
import sys
import time
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
sys.path.insert(0, str(ROOT / "scripts/character"))
import minka_doors_ue as kitlib  # noqa: E402

SRC = ROOT / "art/export/Props/LootCrate"
OUT = ROOT / "monke_island/Saved/T258/loot-crate-import.json"
BASE = "/Game/MonkeIsland/Props/LootCrate"
MESHES = BASE + "/Meshes"
TEXTURES = BASE + "/Textures"
MATERIALS = BASE + "/Materials"
MI_NAME = "MI_LootCrate"
# mesh: LOD files (LOD0 is the mesh itself); screen sizes of the maquette's README (LOD1 0.5, LOD2 0.2)
MESH_LODS = {
    "SM_LootCrate": ["SM_LootCrate_LOD1", "SM_LootCrate_LOD2"],
    "SM_LootCrate_Lid": ["SM_LootCrate_Lid_LOD1", "SM_LootCrate_Lid_LOD2"],
}
SCREEN_SIZES = [1.0, 0.5, 0.2]
# expected outer size of the body (maquette 0.80 x 0.45 x 0.30 m), checked against the imported bounds (cm)
EXPECTED_BODY_CM = (80.0, 45.0, 30.0)
EAL = ue.EditorAssetLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()
MEL = ue.MaterialEditingLibrary
report = {"source": str(SRC), "steps": []}


def step(name, started):
    report["steps"].append([name, round(time.time() - started, 2)])


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
    return EAL.load_asset(f"{dest}/{name}") if EAL.does_asset_exist(f"{dest}/{name}") else None


def texture(kind):
    name = f"T_LootCrate_{kind}"
    tex = import_task(SRC / "Textures" / f"{name}.png", TEXTURES, name)
    assert tex, name
    if kind == "Normal":
        # DirectX normal of the maquette (T_LootCrate_Normal; _Normal_GL is the OpenGL copy for Blender): green kept
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
        tex.set_editor_property("srgb", False)
        tex.set_editor_property("flip_green_channel", False)
    elif kind == "ORM":
        tex.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
        tex.set_editor_property("srgb", False)
    else:
        tex.set_editor_property("srgb", True)
    kitlib.save(tex)
    return tex


def material_instance(master, textures):
    path = f"{MATERIALS}/{MI_NAME}"
    mi = kitlib.load(path) or TOOLS.create_asset(MI_NAME, MATERIALS, ue.MaterialInstanceConstant,
                                                 ue.MaterialInstanceConstantFactoryNew())
    MEL.set_material_instance_parent(mi, master)
    for param, tex in textures.items():
        MEL.set_material_instance_texture_parameter_value(mi, param, tex)
    MEL.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.0)
    kitlib.save(mi)
    return mi


def mesh_options():
    options = ue.FbxImportUI()
    options.import_mesh = True
    options.import_as_skeletal = False
    options.import_materials = False
    options.import_textures = False
    options.import_animations = False
    options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
    data = options.static_mesh_import_data
    # one render mesh per file: combined or not gives the same asset; the UCX_<name>_## are read as they are (hollow body)
    data.set_editor_property("combine_meshes", True)
    data.set_editor_property("auto_generate_collision", False)
    data.set_editor_property("import_uniform_scale", 1.0)
    data.set_editor_property("convert_scene", True)
    data.set_editor_property("import_mesh_lods", False)
    for prop, value in (("build_nanite", False), ("generate_lightmap_u_vs", True)):
        try:
            data.set_editor_property(prop, value)
        except Exception:  # noqa: BLE001 - absent in this engine version
            pass
    return options


def bounds_cm(mesh):
    box = mesh.get_bounding_box()
    size = [round(box.max.x - box.min.x, 2), round(box.max.y - box.min.y, 2), round(box.max.z - box.min.z, 2)]
    return {"min": [round(box.min.x, 2), round(box.min.y, 2), round(box.min.z, 2)],
            "max": [round(box.max.x, 2), round(box.max.y, 2), round(box.max.z, 2)], "size": size}


def socket_names(mesh_name):
    """Sockets of a mesh in the maquette's manifest (SOCKET_ prefix stripped, as the FBX importer names them)"""
    manifest = json.loads((SRC / "CaisseTransport_Manifest.json").read_text(encoding="utf-8"))
    names = manifest["assets"][mesh_name].get("sockets", {})
    return [n[len("SOCKET_"):] if n.startswith("SOCKET_") else n for n in names]


def sockets(mesh, names):
    """The sockets found by name (StaticMesh.Sockets is protected from Python)"""
    out = {}
    for name in names:
        socket = mesh.find_socket(name)
        if not socket:
            continue
        loc = socket.get_editor_property("relative_location")
        rot = socket.get_editor_property("relative_rotation")
        out[name] = {"loc": [round(loc.x, 2), round(loc.y, 2), round(loc.z, 2)],
                     "rot": [round(rot.pitch, 1), round(rot.yaw, 1), round(rot.roll, 1)]}
    return out


def keep_sockets(mesh, names):
    """Transforms of the sockets the LOD0 import made, read before the LOD imports"""
    kept = {}
    for name in names:
        socket = mesh.find_socket(name)
        if socket:
            kept[name] = (socket.get_editor_property("relative_location"), socket.get_editor_property("relative_rotation"),
                          socket.get_editor_property("relative_scale"))
    return kept


def restore_sockets(mesh, kept):
    """Importing LOD1 / LOD2 removes the sockets made by the LOD0 import (FbxStaticMeshImport drops the sockets
    'created at import' missing from the LOD file): they come back as plain sockets, which later imports keep"""
    restored = []
    for name, (loc, rot, scale) in kept.items():
        if mesh.find_socket(name):
            continue
        socket = ue.new_object(ue.StaticMeshSocket, outer=mesh)
        socket.set_editor_property("socket_name", name)
        socket.set_editor_property("relative_location", loc)
        socket.set_editor_property("relative_rotation", rot)
        socket.set_editor_property("relative_scale", scale)
        mesh.add_socket(socket)
        restored.append(name)
    return restored


def import_mesh(name, lods, mi):
    sub = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    started = time.time()
    names = socket_names(name)
    mesh = import_task(SRC / f"{name}.fbx", MESHES, name, mesh_options())
    assert mesh, f"{name}: import failed"
    entry = {"path": mesh.get_path_name()}
    kept = keep_sockets(mesh, names)
    entry["sockets_after_lod0"] = len(kept)
    lod_results = []
    for index, lod_name in enumerate(lods, start=1):
        try:
            lod_results.append([index, lod_name, sub.import_lod(mesh, index, str(SRC / f"{lod_name}.fbx"))])
        except Exception as error:  # noqa: BLE001
            lod_results.append([index, lod_name, f"failed: {error}"])
    entry["lod_import"] = lod_results
    entry["sockets_restored"] = len(restore_sockets(mesh, kept))
    count = sub.get_lod_count(mesh)
    entry["lod_count"] = count
    if count == len(SCREEN_SIZES):
        entry["screen_sizes_set"] = sub.set_lod_screen_sizes(mesh, SCREEN_SIZES)
        entry["screen_sizes"] = SCREEN_SIZES
    entry["verts_per_lod"] = [sub.get_number_verts(mesh, lod) for lod in range(count)]
    # the one atlas material on every slot of every LOD
    slots = []
    for index, slot in enumerate(mesh.get_editor_property("static_materials")):
        mesh.set_material(index, mi)
        slots.append([index, str(slot.get_editor_property("material_slot_name")), mi.get_name()])
    entry["material_slots"] = slots
    entry["convex_collision"] = sub.get_convex_collision_count(mesh)
    entry["simple_collision"] = sub.get_simple_collision_count(mesh)
    entry["bounds_cm"] = bounds_cm(mesh)
    entry["pivot"] = "middle of the bottom" if abs(entry["bounds_cm"]["min"][2]) < 1.0 else "check: bottom at z %.1f cm" % entry["bounds_cm"]["min"][2]
    entry["sockets"] = sockets(mesh, names)
    entry["sockets_expected"] = len(names)
    assert len(entry["sockets"]) == len(names), f"{name}: {len(entry['sockets'])} sockets of {len(names)}"
    kitlib.save(mesh)
    step(name, started)
    return entry


def main():
    kitlib.check_lock()
    import import_coco_real_ue as coco  # noqa: E402 - M_Character_Baked, created there when missing
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    flag = "Interchange.FeatureFlags.Import.FBX"
    previous = ue.SystemLibrary.get_console_variable_int_value(flag)
    ue.SystemLibrary.execute_console_command(world, flag + " 0")
    try:
        started = time.time()
        master = coco.master_material()
        report["master"] = master.get_path_name()
        textures = {"BaseColor": texture("BaseColor"), "Normal": texture("Normal"), "ORM": texture("ORM")}
        report["textures"] = {k: v.get_path_name() for k, v in textures.items()}
        step("textures", started)
        mi = material_instance(master, textures)
        report["material_instance"] = mi.get_path_name()
        report["meshes"] = {}
        for name, lods in MESH_LODS.items():
            try:
                report["meshes"][name] = import_mesh(name, lods, mi)
            except Exception:  # noqa: BLE001
                report["meshes"][name] = {"error": traceback.format_exc()[-1500:]}
        body = report["meshes"].get("SM_LootCrate", {})
        size = body.get("bounds_cm", {}).get("size")
        if size:
            # the ropes and handles stand a little out of the planks: the body box is checked within 15 %
            report["size_check"] = {"expected_cm": EXPECTED_BODY_CM, "imported_cm": size,
                                    "ok": all(abs(s - e) <= e * 0.15 for s, e in zip(size, EXPECTED_BODY_CM))}
        report["passed"] = all("error" not in m for m in report["meshes"].values())
    finally:
        ue.SystemLibrary.execute_console_command(world, flag + " " + str(previous))
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(report, indent=1, default=str, ensure_ascii=False), encoding="utf-8")
    assert report.get("passed"), f"loot crate import failed, see {OUT}"
    ue.log(f"T258_LOOT_CRATE_IMPORT_SUCCESS {OUT}")


if __name__ == "__main__":
    main()
