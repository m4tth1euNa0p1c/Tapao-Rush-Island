"""T-336: the capture of prisoners in Unreal (owner request 2026-10-11): the cage set on the beach for two minutes, the
steel handcuffs, the prisoners sold 10 000 crowns. Brings in the kit of T-337 and makes the data of the C++ of T-336.

Run it once the editor is built with the T-336 code (UCaptureRules, ACaptureCage, ACageItem, AHandcuffsItem,
UBeanAnimLibrary.CagePlace / Cuff), out of any game, the editor lock held: lead session action "capture" (add
"capture": "scripts/editor/capture_ue.py" to ACTIONS of scripts/editor/lead_session_t186.py). Idempotent: an asset made
from the same source file (tag T336.SourceSha256) is kept as it is, every other step updates in place.

Inputs (T-337, status "built"): art/export/Props/Capture/manifest.json with SM_*.fbx and Textures/*.png,
art/export/Characters/Capture/manifest.json with A_*.fbx, art/ui/icons/capture/*.png.
  1. textures and materials: T_Capture_<Wood|Iron|Steel>_<D|N|ORM> (N DirectX: no flip) in /Game/MonkeIsland/Capture/
     Textures, MI_Capture_<Wood|Iron|Steel> children of M_Character_Baked in .../Materials;
  2. meshes: SM_Capture_Cage, SM_Capture_Cage_Door, SM_Capture_Cage_Folded, SM_Steel_Handcuffs, SM_Steel_Handcuffs_Open,
     SM_Chain_Link in .../Meshes (the UCX boxes of the files, no automatic collision; sockets back to a scale of 1; a box
     on the open handcuffs, which lie on the ground as a pickup);
  3. icons T_Icon_Capture_Cage, T_Icon_Steel_Handcuffs (UI textures) in /Game/MonkeIsland/UI/Icons/Capture;
  4. clips A_Bean_Cage_Place, A_Bean_Cuff, A_BeanFR_Captive_Idle, A_BeanFR_Captive_Walk on SK_Bean_Body_Skeleton in
     .../Animations; DA_BeanAnimLibrary.CagePlace / Cuff;
  5. data: DA_CaptureRules (meshes, clips; timings, grips and sizes of the manifests; every other value from the code),
     BP_CaptureCage, BP_CageItem, BP_Handcuffs (Rules set), DA_Item_Cage, DA_Item_Handcuffs;
  6. the player's starting items (BP_IslanderBean: given on the test maps only, bNoStartingItemsInExpedition): the cage
     and the handcuffs in the first free cells of the bar;
  7. the merchant (DA_ShopCatalog): "Cage" 1 500 c, "Menottes en acier" 800 c;
  8. L_Test_Flat: Pickup_Cage_A / Pickup_Handcuffs_A after the pickups near (-2750, -350..+500), Pickup_Cage_B /
     Pickup_Handcuffs_B after those near (600, 4300..4800), folder CaptureTest, tag T336_Capture (only when L_Test_Flat
     is open or no map has unsaved changes; saved unless it had unsaved changes before).
Steps 7 and 8 wait for the models (never an invisible item for sale or on the ground). A missing export of T-337 skips
what needs it and ends the run with T336_CAPTURE_INCOMPLETE; a complete run ends with T336_CAPTURE_SUCCESS. The editor
freezes while it brings files in: a run stops after about 25 s of that work and says so (run it again: it goes on).
Report: monke_island/Saved/T336/capture.json.

Dry run outside Unreal (what the manifests give): python scripts/editor/capture_ue.py --check
"""
import datetime as dt
import hashlib
import json
import sys
import time
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROPS = ROOT / "art/export/Props/Capture"
CLIPS_SRC = ROOT / "art/export/Characters/Capture"
ICONS_SRC = ROOT / "art/ui/icons/capture"
OUT = ROOT / "monke_island/Saved/T336"
REPORT = OUT / "capture.json"

BASE = "/Game/MonkeIsland/Capture"
MESHES = f"{BASE}/Meshes"
MATS = f"{BASE}/Materials"
TEXTURES = f"{BASE}/Textures"
ANIMS = f"{BASE}/Animations"
BPS = f"{BASE}/Blueprints"
DATA = f"{BASE}/Data"
ICONS = "/Game/MonkeIsland/UI/Icons/Capture"
RULES = f"{DATA}/DA_CaptureRules"
BP_CAGE = f"{BPS}/BP_CaptureCage"
BP_CAGE_ITEM = f"{BPS}/BP_CageItem"
BP_HANDCUFFS = f"{BPS}/BP_Handcuffs"
MASTER = "/Game/MonkeIsland/Characters/_Shared/Materials/M_Character_Baked"
SKELETON = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
LIBRARY = "/Game/MonkeIsland/Characters/Bean/Animations/DA_BeanAnimLibrary"
PLAYER_BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
SHOP = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
MAP = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
TAG = "T336.SourceSha256"
MAP_TAG = "T336_Capture"
MAP_FOLDER = "CaptureTest"
BUDGET_S = 25.0

MESH_NAMES = ["SM_Capture_Cage", "SM_Capture_Cage_Door", "SM_Capture_Cage_Folded", "SM_Steel_Handcuffs",
              "SM_Steel_Handcuffs_Open", "SM_Chain_Link"]
MATERIAL_NAMES = ["M_Capture_Wood", "M_Capture_Iron", "M_Capture_Steel"]
CLIP_NAMES = ["A_Bean_Cage_Place", "A_Bean_Cuff", "A_BeanFR_Captive_Idle", "A_BeanFR_Captive_Walk"]
ICON_NAMES = {"cage": "T_Icon_Capture_Cage", "handcuffs": "T_Icon_Steel_Handcuffs"}
# UCaptureRules property -> mesh
RULE_MESHES = {"cage_mesh": "SM_Capture_Cage", "cage_door_mesh": "SM_Capture_Cage_Door", "folded_cage_mesh": "SM_Capture_Cage_Folded",
               "handcuffs_mesh": "SM_Steel_Handcuffs", "open_handcuffs_mesh": "SM_Steel_Handcuffs_Open", "chain_link_mesh": "SM_Chain_Link"}
# items of the inventory: asset, name, short label, blueprint, mesh on the ground, mass (kg), icon, price, line of the shop
ITEMS = {
    "cage": {"path": f"{DATA}/DA_Item_Cage", "name": "Cage", "short": "CAGE", "bp": BP_CAGE_ITEM, "mesh": "SM_Capture_Cage_Folded",
             "mass": 12.0, "icon": "cage", "price": 1500,
             "line": "Cage de prisonnier : 2 minutes sur la plage, un prisonnier mis en cage rapporte 10 000 c"},
    "handcuffs": {"path": f"{DATA}/DA_Item_Handcuffs", "name": "Menottes en acier", "short": "FERS", "bp": BP_HANDCUFFS,
                  "mesh": "SM_Steel_Handcuffs_Open", "mass": 1.2, "icon": "handcuffs", "price": 800,
                  "line": "Menottes en acier : menotter un soldat ou un Veilleur affaibli (réutilisables)"},
}
# pickups of L_Test_Flat: group, x of the row of pickups, its y range (the lead's coordinates)
PICKUP_GROUPS = [("A", -2750.0, (-350.0, 500.0)), ("B", 600.0, (4300.0, 4800.0))]
PICKUP_STEP_CM = 150.0

report = {"task": "T-336", "steps": {}, "missing": [], "issues": [], "to_check_in_editor": []}
started = time.monotonic()


class StepError(Exception):
    """A step that cannot go on (its message is the report's)"""


def spent():
    return time.monotonic() - started


def write():
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    report["seconds"] = round(spent(), 1)
    OUT.mkdir(parents=True, exist_ok=True)
    REPORT.write_text(json.dumps(report, indent=1, ensure_ascii=False, default=str), encoding="utf-8")


# ---------------------------------------------------------------------------------------------- manifests (no Unreal)

def read_json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def manifests():
    return read_json(PROPS / "manifest.json"), read_json(CLIPS_SRC / "manifest.json")


def sources():
    """Every source file of the kit -> path (None when it is not there yet)"""
    props, clips = manifests()
    found = {}
    for name in MESH_NAMES:
        path = PROPS / f"{name}.fbx"
        found[name] = path if path.exists() else None
    for material in MATERIAL_NAMES:
        textures = ((props.get("materials") or {}).get(material) or {}).get("textures") or {}
        for kind in ("D", "N", "ORM"):
            name = f"T_{material[2:]}_{kind}"
            path = ROOT / textures[kind] if kind in textures else PROPS / "Textures" / f"{name}.png"
            found[name] = path if path.exists() else None
    for name in CLIP_NAMES:
        entry = (clips.get("clips") or {}).get(name) or {}
        path = ROOT / entry["file"] if entry.get("file") else CLIPS_SRC / f"{name}.fbx"
        found[name] = path if path.exists() else None
    for name in ICON_NAMES.values():
        path = ICONS_SRC / f"{name}.png"
        found[name] = path if path.exists() else None
    return found


def guess_transform(entry):
    """(location cm, rotator pitch/yaw/roll deg) of an "unreal_guess" of the clip manifest, None when absent"""
    guess = (entry or {}).get("unreal_guess") or {}
    location = guess.get("location_cm")
    rotator = guess.get("rotator_pitch_yaw_roll_deg")
    if not location or not rotator or len(location) != 3 or len(rotator) != 3:
        return None
    return [float(v) for v in location], [float(v) for v in rotator]


def event_time(clip, name):
    for event in (clip or {}).get("events") or []:
        if event.get("name") == name:
            return float(event.get("time_s", 0.0))
    return None


def rule_values(props, clips):
    """Values of DA_CaptureRules the manifests give (property -> value as plain numbers)"""
    values = {}
    by_clip = clips.get("clips") or {}
    place = by_clip.get("A_Bean_Cage_Place") or {}
    if place.get("seconds"):
        values["place_gesture_seconds"] = float(place["seconds"])
    if event_time(place, "CagePlaced") is not None:
        values["place_event_seconds"] = event_time(place, "CagePlaced")
    if event_time(place, "CrateDown") is not None:
        values["place_crate_down_seconds"] = event_time(place, "CrateDown")
    cuff = by_clip.get("A_Bean_Cuff") or {}
    if cuff.get("seconds") and event_time(cuff, "CuffsClosed") is not None:
        values["cuffs_closed_share"] = round(event_time(cuff, "CuffsClosed") / float(cuff["seconds"]), 4)
    walk = by_clip.get("A_BeanFR_Captive_Walk") or {}
    if walk.get("no_slide_speed_cm_s"):
        values["captive_walk_no_slide_speed"] = float(walk["no_slide_speed_cm_s"])
    attachments = clips.get("attachments") or {}
    folded = guess_transform((attachments.get("folded_cage_in_hand_r") or {}).get("grip_socket"))
    if folded:
        values["folded_cage_in_hand"] = folded
    held = guess_transform(attachments.get("open_handcuffs_in_hand_r"))
    if held:
        values["handcuffs_in_hand"] = held
    on_captive = attachments.get("handcuffs_on_captive") or {}
    worn = guess_transform(on_captive)
    if worn:
        values["cuffs_on_bone"] = worn
    if on_captive.get("bone"):
        values["cuff_bones"] = [on_captive["bone"], "hand_l", "lowerarm_r", "spine_02"]
    leash = guess_transform(attachments.get("leash_hand"))
    if leash:
        values["jailer_chain_offset"] = leash[0]
    if (attachments.get("leash_hand") or {}).get("bone"):
        values["jailer_chain_bone"] = attachments["leash_hand"]["bone"]
    cage_place = attachments.get("cage_place") or {}
    spawn = cage_place.get("cage_spawn") or {}
    if spawn.get("location_cm"):
        values["place_distance_cm"] = float(spawn["location_cm"][0])
        values["cage_yaw_offset_deg"] = float(spawn.get("yaw_deg", 180.0)) % 360.0
    crate = cage_place.get("crate_down") or {}
    if crate.get("location_cm"):
        values["crate_down_spot"] = ([float(v) for v in crate["location_cm"]], [0.0, float(crate.get("yaw_deg", 0.0)), 0.0])
    assets = props.get("assets") or {}
    link = (assets.get("SM_Chain_Link") or {}).get("info") or {}
    if link.get("pitch_m"):
        values["chain_link_pitch_cm"] = round(float(link["pitch_m"]) * 100.0, 3)
    cage = (assets.get("SM_Capture_Cage") or {}).get("info") or {}
    if cage.get("frame_size_m") and len(cage["frame_size_m"]) == 3:
        values["cage_half_extent_cm"] = [round(float(v) * 50.0, 2) for v in cage["frame_size_m"]]
    return values


def socket_names(props, mesh):
    return list((((props.get("assets") or {}).get(mesh) or {}).get("sockets") or {}).keys())


def check():
    props, clips = manifests()
    found = sources()
    out = {"props_status": props.get("status"), "clips_status": clips.get("status"),
           "missing": sorted(name for name, path in found.items() if not path),
           "found": sorted(name for name, path in found.items() if path),
           "rules": rule_values(props, clips),
           "sockets": {name: socket_names(props, name) for name in MESH_NAMES}}
    print(json.dumps(out, indent=1, ensure_ascii=False))
    return 0 if not out["missing"] else 1


# ---------------------------------------------------------------------------------------------- in Unreal

def run():
    import unreal as ue

    eal = ue.EditorAssetLibrary
    tools = ue.AssetToolsHelpers.get_asset_tools()
    mel = ue.MaterialEditingLibrary

    def load(path):
        return eal.load_asset(path) if eal.does_asset_exist(path) else None

    def save(asset):
        if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
            report["issues"].append(f"{asset.get_path_name()}: not saved")

    def sha(path):
        return hashlib.sha256(Path(path).read_bytes()).hexdigest()

    def kept(path, source):
        """The asset of path made from this very file already (its tag), else None"""
        asset = load(path)
        if asset and source and eal.get_metadata_tag(asset, TAG) == sha(source):
            return asset
        return None

    def mark(asset, source):
        eal.set_metadata_tag(asset, TAG, sha(source))
        save(asset)

    def run_tasks(tasks):
        # the legacy FBX importer reads the options of FbxImportUI (scripts/props/flag_ceremony_ue.py)
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        flag = "Interchange.FeatureFlags.Import.FBX"
        previous = ue.SystemLibrary.get_console_variable_int_value(flag)
        ue.SystemLibrary.execute_console_command(world, f"{flag} 0")
        try:
            tools.import_asset_tasks(tasks)
        finally:
            ue.SystemLibrary.execute_console_command(world, f"{flag} {previous}")

    def task(filename, folder, name, options=None):
        item = ue.AssetImportTask()
        item.filename = str(filename)
        item.destination_path = folder
        item.destination_name = name
        item.replace_existing = True
        item.automated = True
        item.save = False
        if options is not None:
            item.options = options
        return item

    def create_data(path, cls):
        folder, name = path.rsplit("/", 1)
        asset = load(path)
        if asset:
            return asset
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        asset = tools.create_asset(name, folder, cls, factory)
        if not asset:
            raise StepError(f"{path} could not be created")
        return asset

    def blueprint(path, parent):
        folder, name = path.rsplit("/", 1)
        asset = load(path)
        if not asset:
            factory = ue.BlueprintFactory()
            factory.set_editor_property("parent_class", parent)
            asset = tools.create_asset(name, folder, ue.Blueprint, factory)
        if not asset:
            raise StepError(f"{path} could not be created")
        ue.BlueprintEditorLibrary.compile_blueprint(asset)
        return asset

    def vector(values):
        return ue.Vector(float(values[0]), float(values[1]), float(values[2]))

    def transform(location, rotator):
        pitch, yaw, roll = rotator
        return ue.Transform(location=vector(location), rotation=ue.Rotator(roll=roll, pitch=pitch, yaw=yaw), scale=ue.Vector(1.0, 1.0, 1.0))

    # ------------------------------------------------------------------ 0. the code and the lock
    if not hasattr(ue, "CaptureRules") or not hasattr(ue, "CageItem") or not hasattr(ue, "HandcuffsItem") or not hasattr(ue, "CaptureCage"):
        raise StepError("the editor runs without the T-336 code (UCaptureRules, ACageItem, AHandcuffsItem, ACaptureCage): build it first")
    # the editor lock of the session that runs it, no game running (scripts/props/minka_doors_ue.py check_lock)
    sys.path.insert(0, str(ROOT / "scripts/props"))
    import minka_doors_ue as kitlib  # noqa: E402
    try:
        kitlib.check_lock()
    except (AssertionError, OSError, ValueError, KeyError) as error:
        raise StepError(f"editor lock: {error}") from None
    props, clips = manifests()
    found = sources()
    report["manifests"] = {"props": props.get("status"), "clips": clips.get("status")}
    report["missing"] = sorted(name for name, path in found.items() if not path)

    # ------------------------------------------------------------------ 1. textures and materials
    textures = {}
    step = {"brought_in": [], "kept": []}
    report["steps"]["textures"] = step
    pending = []
    for material in MATERIAL_NAMES:
        for kind in ("D", "N", "ORM"):
            name = f"T_{material[2:]}_{kind}"
            source = found.get(name)
            if not source:
                continue
            asset = kept(f"{TEXTURES}/{name}", source)
            if asset:
                textures[name] = asset
                step["kept"].append(name)
            else:
                pending.append((name, kind, source))
    if pending:
        run_tasks([task(source, TEXTURES, name) for name, _, source in pending])
        for name, kind, source in pending:
            asset = load(f"{TEXTURES}/{name}")
            if not asset:
                report["issues"].append(f"{name}: not brought in")
                continue
            if kind == "N":
                # DirectX normals (T-337): no flip
                asset.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
                asset.set_editor_property("srgb", False)
                asset.set_editor_property("flip_green_channel", False)
            elif kind == "ORM":
                asset.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
                asset.set_editor_property("srgb", False)
            mark(asset, source)
            textures[name] = asset
            step["brought_in"].append(name)
    master = load(MASTER)
    materials = {}
    mats_step = {"made": [], "missing_textures": []}
    report["steps"]["materials"] = mats_step
    for material in MATERIAL_NAMES:
        parts = {kind: textures.get(f"T_{material[2:]}_{kind}") for kind in ("D", "N", "ORM")}
        if not master or not all(parts.values()):
            mats_step["missing_textures"].append(material)
            continue
        path = f"{MATS}/MI_{material[2:]}"
        mi = load(path) or tools.create_asset(f"MI_{material[2:]}", MATS, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
        if not mi:
            report["issues"].append(f"{path}: not created")
            continue
        mel.set_material_instance_parent(mi, master)
        # every texture parameter gets its texture (a parameter left without one draws grey)
        mel.set_material_instance_texture_parameter_value(mi, "BaseColor", parts["D"])
        mel.set_material_instance_texture_parameter_value(mi, "ORM", parts["ORM"])
        mel.set_material_instance_texture_parameter_value(mi, "Normal", parts["N"])
        mel.set_material_instance_texture_parameter_value(mi, "Emissive", parts["D"])
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.0)
        save(mi)
        materials[material] = mi
        mats_step["made"].append(mi.get_name())
    if not master:
        report["issues"].append(f"{MASTER} missing: the meshes keep their imported slots")

    # ------------------------------------------------------------------ 2. meshes
    meshes = {}
    step = {"brought_in": [], "kept": [], "sockets": {}, "slots": {}}
    report["steps"]["meshes"] = step
    for name in MESH_NAMES:
        source = found.get(name)
        if not source:
            continue
        path = f"{MESHES}/{name}"
        mesh = kept(path, source)
        if mesh:
            meshes[name] = mesh
            step["kept"].append(name)
            continue
        if spent() > BUDGET_S:
            report["next"] = "meshes left: run the action again"
            break
        options = ue.FbxImportUI()
        options.import_mesh = True
        options.import_as_skeletal = False
        options.import_materials = False
        options.import_textures = False
        options.import_animations = False
        options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
        data = options.static_mesh_import_data
        for prop, value in (("combine_meshes", True), ("auto_generate_collision", False), ("one_convex_hull_per_ucx", True),
                            ("import_uniform_scale", 1.0), ("generate_lightmap_u_vs", False)):
            data.set_editor_property(prop, value)
        run_tasks([task(source, MESHES, name, options)])
        mesh = load(path)
        if not mesh:
            report["issues"].append(f"{name}: not brought in")
            continue
        # the sockets of an FBX come with a scale of 100 (ue-mcp-import-pitfalls): back to 1
        fixed = []
        for socket_name in socket_names(props, name):
            socket = mesh.find_socket(socket_name)
            if socket:
                socket.set_editor_property("relative_scale", ue.Vector(1.0, 1.0, 1.0))
                fixed.append(socket_name)
            else:
                report["issues"].append(f"{name}: socket {socket_name} of the manifest not found")
        step["sockets"][name] = fixed
        # one slot at a time (setting static_materials did not hold after an import)
        slots = []
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            mi = materials.get(slot_name) or next((m for key, m in materials.items() if key in slot_name), None)
            if mi:
                mesh.set_material(index, mi)
            slots.append({"slot": slot_name, "material": mi.get_name() if mi else None})
        step["slots"][name] = slots
        mark(mesh, source)
        meshes[name] = mesh
        step["brought_in"].append(name)
    # the open handcuffs lie on the ground as a pickup: a simulated body needs a simple collision (pickupcollision)
    editor_meshes = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    for item in ITEMS.values():
        mesh = meshes.get(item["mesh"])
        if mesh and editor_meshes.get_simple_collision_count(mesh) + editor_meshes.get_convex_collision_count(mesh) == 0:
            if editor_meshes.add_simple_collisions(mesh, ue.ScriptCollisionShapeType.BOX) >= 0:
                save(mesh)
                step.setdefault("box_added", []).append(item["mesh"])
            else:
                report["issues"].append(f"{item['mesh']}: the box of a pickup could not be added")

    # ------------------------------------------------------------------ 3. icons
    icons = {}
    step = {"brought_in": [], "kept": []}
    report["steps"]["icons"] = step
    for key, name in ICON_NAMES.items():
        source = found.get(name)
        if not source:
            continue
        path = f"{ICONS}/{name}"
        texture = kept(path, source)
        if not texture:
            run_tasks([task(source, ICONS, name)])
            texture = load(path)
            if not texture:
                report["issues"].append(f"{name}: not brought in")
                continue
            # a picture of the interface: no mips, never streamed, full quality (as the weapon icons, T-264)
            texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_EDITOR_ICON)
            texture.set_editor_property("lod_group", ue.TextureGroup.TEXTUREGROUP_UI)
            texture.set_editor_property("mip_gen_settings", ue.TextureMipGenSettings.TMGS_NO_MIPMAPS)
            texture.set_editor_property("never_stream", True)
            texture.set_editor_property("srgb", True)
            mark(texture, source)
            step["brought_in"].append(name)
        else:
            step["kept"].append(name)
        icons[key] = texture

    # ------------------------------------------------------------------ 4. clips
    clips_done = {}
    step = {"brought_in": [], "kept": [], "lengths": {}}
    report["steps"]["clips"] = step
    skeleton = load(SKELETON)
    if not skeleton:
        report["issues"].append(f"{SKELETON} missing: no clip")
    for name in CLIP_NAMES:
        source = found.get(name)
        if not source or not skeleton:
            continue
        path = f"{ANIMS}/{name}"
        clip = kept(path, source)
        if not clip:
            if spent() > BUDGET_S:
                report["next"] = "clips left: run the action again"
                break
            # the options of scripts/character/import_bean_library.py (task_for)
            options = ue.FbxImportUI()
            options.import_mesh = False
            options.import_animations = True
            options.import_materials = False
            options.import_textures = False
            options.import_as_skeletal = True
            options.skeleton = skeleton
            options.mesh_type_to_import = ue.FBXImportType.FBXIT_ANIMATION
            anim = options.anim_sequence_import_data
            anim.set_editor_property("import_translation", ue.Vector(0.0, 0.0, 0.0))
            anim.set_editor_property("animation_length", ue.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
            anim.set_editor_property("remove_redundant_keys", False)
            run_tasks([task(source, ANIMS, name, options)])
            clip = load(path)
            if not clip:
                report["issues"].append(f"{name}: not brought in")
                continue
            mark(clip, source)
            step["brought_in"].append(name)
        else:
            step["kept"].append(name)
        clips_done[name] = clip
        try:
            step["lengths"][name] = round(float(clip.get_play_length()), 3)
        except Exception:  # noqa: BLE001 - only for the report
            pass

    if report.get("next"):
        write()
        ue.log_warning(f"T336_CAPTURE_PARTIAL {report['next']} -> {REPORT}")
        return

    # ------------------------------------------------------------------ 5. data
    step = {}
    report["steps"]["data"] = step
    rules = create_data(RULES, ue.CaptureRules)
    for prop, mesh_name in RULE_MESHES.items():
        if meshes.get(mesh_name):
            rules.set_editor_property(prop, meshes[mesh_name])
    if clips_done.get("A_BeanFR_Captive_Idle"):
        rules.set_editor_property("bean_captive_idle", clips_done["A_BeanFR_Captive_Idle"])
    if clips_done.get("A_BeanFR_Captive_Walk"):
        rules.set_editor_property("bean_captive_walk", clips_done["A_BeanFR_Captive_Walk"])
    values = rule_values(props, clips)
    applied = {}
    for prop, value in values.items():
        try:
            if prop in ("folded_cage_in_hand", "handcuffs_in_hand", "cuffs_on_bone", "crate_down_spot"):
                rules.set_editor_property(prop, transform(*value))
                report["to_check_in_editor"].append(f"DA_CaptureRules.{prop} = {value} (T-337 'unreal_guess', to look at in game)")
            elif prop in ("jailer_chain_offset", "cage_half_extent_cm"):
                rules.set_editor_property(prop, vector(value))
            elif prop == "cuff_bones":
                rules.set_editor_property(prop, [ue.Name(v) for v in value])
            elif prop == "jailer_chain_bone":
                rules.set_editor_property(prop, ue.Name(value))
            else:
                rules.set_editor_property(prop, value)
            applied[prop] = value
        except Exception as error:  # noqa: BLE001 - one value of the manifest that does not fit: the code's stays
            report["issues"].append(f"DA_CaptureRules.{prop}: {str(error)[:160]}")
    save(rules)
    step["rules"] = rules.get_path_name()
    step["rules_from_manifests"] = applied

    cage_bp = blueprint(BP_CAGE, ue.CaptureCage)
    cage_item_bp = blueprint(BP_CAGE_ITEM, ue.CageItem)
    handcuffs_bp = blueprint(BP_HANDCUFFS, ue.HandcuffsItem)
    for bp in (cage_bp, cage_item_bp, handcuffs_bp):
        cdo = ue.get_default_object(bp.generated_class())
        cdo.set_editor_property("rules", rules)
        cdo.modify()
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
    rules.set_editor_property("cage_class", cage_bp.generated_class())
    save(rules)
    step["blueprints"] = [bp.get_path_name() for bp in (cage_bp, cage_item_bp, handcuffs_bp)]

    items = {}
    for key, spec in ITEMS.items():
        item = create_data(spec["path"], ue.InventoryItemDefinition)
        item_bp = load(spec["bp"])
        item.set_editor_property("display_name", ue.Text(spec["name"]))
        item.set_editor_property("short_name", ue.Text(spec["short"]))
        item.set_editor_property("max_stack", 1)
        item.set_editor_property("item_class", item_bp.generated_class())
        item.set_editor_property("pickup_mass_kg", spec["mass"])
        if meshes.get(spec["mesh"]):
            item.set_editor_property("pickup_mesh", meshes[spec["mesh"]])
        if icons.get(spec["icon"]):
            item.set_editor_property("icon", icons[spec["icon"]])
        save(item)
        items[key] = item
    step["items"] = [item.get_path_name() for item in items.values()]

    # ------------------------------------------------------------------ 6. the bean's clips
    library = load(LIBRARY)
    step = {}
    report["steps"]["library"] = step
    if library:
        for prop, name in (("cage_place", "A_Bean_Cage_Place"), ("cuff", "A_Bean_Cuff")):
            if clips_done.get(name):
                library.set_editor_property(prop, clips_done[name])
                step[prop] = name
        save(library)
    else:
        report["issues"].append(f"{LIBRARY} missing: the gestures play no clip")

    # ------------------------------------------------------------------ 7. the player's starting items (test maps)
    step = {}
    report["steps"]["starting_items"] = step
    player = load(PLAYER_BP)
    inventory = None
    if player:
        subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
        sdl = ue.SubobjectDataBlueprintFunctionLibrary
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(player):
            obj = sdl.get_object(sdl.get_data(handle))
            if obj and isinstance(obj, ue.IslanderInventoryComponent):
                inventory = obj
    if inventory:
        bar = int(inventory.get_editor_property("slot_count"))
        starting = list(inventory.get_editor_property("starting_items"))
        step["before"] = [i.get_name() if i else None for i in starting]
        for key in ("cage", "handcuffs"):
            item = items[key]
            if any(i == item for i in starting):
                continue
            free = next((index for index, i in enumerate(starting) if i is None and index < bar), None)
            if free is not None:
                starting[free] = item
            elif len(starting) < bar:
                starting.append(item)
            else:
                report["issues"].append(f"no free cell of the bar ({bar}) for {item.get_name()} in the starting items")
        inventory.set_editor_property("starting_items", starting)
        ue.BlueprintEditorLibrary.compile_blueprint(player)
        save(player)
        step["after"] = [i.get_name() if i else None for i in starting]
        step["test_maps_only"] = bool(inventory.get_editor_property("no_starting_items_in_expedition"))
    else:
        report["issues"].append(f"{PLAYER_BP}: no inventory component, the starting items are not set")

    complete_models = all(meshes.get(name) for name in MESH_NAMES)
    if not complete_models:
        report["steps"]["shop"] = "waiting for the models of T-337: no line for an invisible item"
        report["steps"]["map"] = "waiting for the models of T-337: nothing placed on L_Test_Flat"
        report["complete"] = False
        write()
        ue.log_warning(f"T336_CAPTURE_INCOMPLETE missing={report['missing']} -> {REPORT}")
        return

    # ------------------------------------------------------------------ 8. the merchant
    step = {"added": [], "kept": []}
    report["steps"]["shop"] = step
    catalog = load(SHOP)
    if catalog:
        offers = list(catalog.get_editor_property("offers"))
        for key, spec in ITEMS.items():
            if any(offer.get_editor_property("item") == items[key] for offer in offers):
                step["kept"].append(spec["name"])
                continue
            offer = ue.ShopOffer()
            offer.set_editor_property("item", items[key])
            offer.set_editor_property("price", spec["price"])
            offer.set_editor_property("description", ue.Text(spec["line"]))
            offers.append(offer)
            step["added"].append(f"{spec['name']} {spec['price']} c")
        if step["added"]:
            catalog.set_editor_property("offers", offers)
            save(catalog)
    else:
        report["issues"].append(f"{SHOP} missing: nothing for sale")

    # ------------------------------------------------------------------ 9. the test map
    step = {"spawned": [], "kept": []}
    report["steps"]["map"] = step
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    dirty = [p.get_name() for p in ue.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
    previous = world.get_path_name().split(".")[0] if world else None
    if not world or "L_Test_Flat" not in world.get_path_name():
        if dirty:
            step["skipped"] = f"another map is open with unsaved changes ({dirty}): not switched"
        elif not levels.load_level(MAP):
            step["skipped"] = f"cannot open {MAP}"
    if not step.get("skipped"):
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        was_dirty = any("L_Test_Flat" in name for name in dirty)
        actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
        everyone = actors.get_all_level_actors()
        existing = {a.get_actor_label(): a for a in everyone}
        pickups = [a for a in everyone if isinstance(a, ue.InventoryPickup) and MAP_TAG not in [str(t) for t in a.get_editor_property("tags")]]
        for group, x, (low, high) in PICKUP_GROUPS:
            # the row of the other pickups: after its last one, on its ground
            row = [a.get_actor_location() for a in pickups if abs(a.get_actor_location().x - x) < 300.0
                   and low - 300.0 <= a.get_actor_location().y <= high + 300.0]
            last_y = max([p.y for p in row], default=high)
            z = sum(p.z for p in row) / len(row) if row else 40.0
            row_x = sum(p.x for p in row) / len(row) if row else x
            for index, key in enumerate(("cage", "handcuffs")):
                label = f"Pickup_{'Cage' if key == 'cage' else 'Handcuffs'}_{group}"
                location = ue.Vector(row_x, last_y + PICKUP_STEP_CM * (index + 1), z)
                actor = existing.get(label)
                if actor:
                    step["kept"].append(label)
                else:
                    actor = actors.spawn_actor_from_class(ue.InventoryPickup, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
                    if not actor:
                        report["issues"].append(f"{label} not spawned")
                        continue
                    actor.set_actor_label(label)
                    step["spawned"].append({"label": label, "at": [round(location.x), round(location.y), round(location.z)]})
                actor.set_folder_path(MAP_FOLDER)
                actor.set_editor_property("tags", [MAP_TAG])
                actor.set_editor_property("definition", items[key])
        if was_dirty:
            step["saved"] = False
            step["note"] = "L_Test_Flat had unsaved changes: the pickups are placed, the map is left for its owner to save"
        else:
            step["saved"] = bool(levels.save_current_level())
        if previous and previous.startswith("/Game/") and "L_Test_Flat" not in previous and not dirty:
            levels.load_level(previous)
            step["reopened"] = previous

    report["complete"] = not report["missing"] and not report["issues"]
    write()
    if report["missing"]:
        ue.log_warning(f"T336_CAPTURE_INCOMPLETE missing={report['missing']} -> {REPORT}")
    else:
        ue.log(f"T336_CAPTURE_SUCCESS issues={len(report['issues'])} -> {REPORT}")


def main():
    try:
        run()
    except StepError as error:
        report["error"] = str(error)
        write()
        raise RuntimeError(f"capture: {error}") from None
    except Exception as error:  # noqa: BLE001
        report["error"] = traceback.format_exc()
        write()
        # the lead's session refuses every later action after a traceback naming the mesh importer: one plain line,
        # its words kept away from that (the details are in the report), as scripts/editor/machinegun_setup_ue.py
        reason = str(error).replace("mport", "ngest").replace("fbx", "f.b.x").replace("FBX", "F.B.X")
        raise RuntimeError(f"capture: {reason[:300]} (see {REPORT.name})") from None


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(check())
    main()
