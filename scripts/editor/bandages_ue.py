"""T-340: the healing bandages in Unreal (owner request 2026-10-11): a stack of linen rolls in the inventory, F or a left
click applies one (A_Bean_Bandage_Apply, about 3 s), health back to its maximum at the end. Brings in the art kit of the
bandage and makes the data of the C++ of T-340 (UBandageRules, ABandageItem, AMonkeTestGameMode.ExtraStartingItems).

Run it once the editor is built with the T-340 code, out of any game, the editor lock held: lead session action
"bandages" (add "bandages": "scripts/editor/bandages_ue.py" to ACTIONS of scripts/editor/lead_session_t186.py). Idempotent:
an asset made from the same source file (tag T340.SourceSha256) is kept as it is, every other step updates in place.

Inputs (contract bandage-kit/1, art/export/Props/Bandage/manifest.json; read, never written): SM_Bandage_Roll.fbx,
SM_Bandage_Roll_Pickup.fbx, SM_Bandage_Wrapped.fbx (and Textures/*.png when the manifest lists materials), the clip
art/export/Characters/Bandage/A_Bean_Bandage_Apply.fbx, the icon art/ui/icons/bandage/T_Icon_Bandage.png.
  1. meshes SM_Bandage_Roll (sockets grip, tail), SM_Bandage_Roll_Pickup (its UCX box; a box added when the file has
     none: a pickup needs a simple collision), SM_Bandage_Wrapped in /Game/MonkeIsland/Items/Bandage/Meshes; materials
     MI_Bandage_* children of M_Character_Baked when the manifest lists their textures, else the materials of the FBX;
  2. icon T_Icon_Bandage (UI texture) in /Game/MonkeIsland/UI/Icons/Bandage;
  3. clip A_Bean_Bandage_Apply on SK_Bean_Body_Skeleton in .../Animations;
  4. data: DA_BandageRules (meshes, clip; the clip's seconds and its events WrapStart / BandageDone, the first person
     keys (first_person.keys) and the attachments of the manifest; every other value from the code), BP_BandageItem
     (Rules set), DA_Item_Bandage (stack of DA_BandageRules.StackSize, pickup SM_Bandage_Roll_Pickup, icon);
  5. the test map's starting inventory: BP_TestGameMode.ExtraStartingItems = DA_BandageRules.TestMapStartingCount
     bandages (L_Test_Flat only; BP_IslanderBean is not changed);
  6. the merchant (DA_ShopCatalog): "Bandage" at DA_BandageRules.ShopPrice;
  7. L_Test_Flat: Pickup_Bandage_A after the pickups near (-2750, -350..+500), Pickup_Bandage_B after those near
     (600, 4300..4800), folder BandageTest, tag T340_Bandage (only when L_Test_Flat is open or no map has unsaved
     changes; saved unless it had unsaved changes before).
Steps 6 and 7 wait for the meshes (never an invisible item for sale or on the ground). A missing export of the kit skips
what needs it and ends the run with T340_BANDAGES_INCOMPLETE; a complete run ends with T340_BANDAGES_SUCCESS. The editor
freezes while it brings files in: a run stops after about 25 s of that work and says so (run it again: it goes on).
Report: monke_island/Saved/T340/bandages.json.

Dry run outside Unreal (what the manifest gives, what is missing): python scripts/editor/bandages_ue.py --check
"""
import datetime as dt
import hashlib
import json
import sys
import time
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PROPS = ROOT / "art/export/Props/Bandage"
MANIFEST = PROPS / "manifest.json"
CLIP_DEFAULT = ROOT / "art/export/Characters/Bandage/A_Bean_Bandage_Apply.fbx"
ICON_DEFAULT = ROOT / "art/ui/icons/bandage/T_Icon_Bandage.png"
OUT = ROOT / "monke_island/Saved/T340"
REPORT = OUT / "bandages.json"

BASE = "/Game/MonkeIsland/Items/Bandage"
MESHES = f"{BASE}/Meshes"
MATS = f"{BASE}/Materials"
TEXTURES = f"{BASE}/Textures"
ANIMS = f"{BASE}/Animations"
BPS = f"{BASE}/Blueprints"
DATA = f"{BASE}/Data"
ICONS = "/Game/MonkeIsland/UI/Icons/Bandage"
RULES = f"{DATA}/DA_BandageRules"
BP_ITEM = f"{BPS}/BP_BandageItem"
ITEM = f"{DATA}/DA_Item_Bandage"
MASTER = "/Game/MonkeIsland/Characters/_Shared/Materials/M_Character_Baked"
SKELETON_DEFAULT = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
TEST_GAME_MODE = "/Game/MonkeIsland/Maps/Test/BP_TestGameMode"
SHOP = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
MAP = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
TAG = "T340.SourceSha256"
MAP_TAG = "T340_Bandage"
MAP_FOLDER = "BandageTest"
BUDGET_S = 25.0

MESH_NAMES = ["SM_Bandage_Roll", "SM_Bandage_Roll_Pickup", "SM_Bandage_Wrapped"]
CLIP = "A_Bean_Bandage_Apply"
ICON = "T_Icon_Bandage"
# UBandageRules property -> mesh
RULE_MESHES = {"roll_mesh": "SM_Bandage_Roll", "wrapped_mesh": "SM_Bandage_Wrapped"}
ITEM_SPEC = {"name": "Bandage", "short": "BANDE", "mass": 0.15,
             "line": "Bandage de lin : appliqué en 3 s (F ou clic), rend toute la santé ; un coup ou une course l'interrompt"}
# pickups of L_Test_Flat: group, x of the row of pickups, its y range (the rows of scripts/editor/capture_ue.py)
PICKUP_GROUPS = [("A", -2750.0, (-350.0, 500.0)), ("B", 600.0, (4300.0, 4800.0))]
PICKUP_STEP_CM = 150.0

report = {"task": "T-340", "steps": {}, "missing": [], "issues": [], "to_check_in_editor": []}
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


# ---------------------------------------------------------------------------------------------- manifest (no Unreal)

def read_json(path):
    try:
        return json.loads(Path(path).read_text(encoding="utf-8"))
    except (OSError, ValueError):
        return {}


def manifest():
    return read_json(MANIFEST)


def clip_entry(kit):
    return ((kit.get("clip") or {}).get(CLIP)) or {}


def materials_of(kit):
    """Materials the manifest lists with their textures: name -> {"D": path, "N": path, "ORM": path}"""
    found = {}
    for name, entry in (kit.get("materials") or {}).items():
        if not isinstance(entry, dict):
            continue  # "convention" and other notes are text
        textures = entry.get("textures") or {}
        if textures:
            found[name] = textures
    return found


def sources(kit):
    """Every source file of the kit -> path (None when it is not there yet)"""
    found = {}
    for name in MESH_NAMES:
        entry = (kit.get("assets") or {}).get(name) or {}
        path = ROOT / entry["file"] if entry.get("file") else PROPS / f"{name}.fbx"
        found[name] = path if path.exists() else None
    for material, textures in materials_of(kit).items():
        for kind, rel in textures.items():
            name = f"T_{material.replace('M_', '', 1)}_{kind}"
            path = ROOT / rel
            found[name] = path if path.exists() else None
    clip = clip_entry(kit)
    path = ROOT / clip["file"] if clip.get("file") else CLIP_DEFAULT
    found[CLIP] = path if path.exists() else None
    icon = (kit.get("icons") or {}).get(ICON)
    path = ROOT / icon if icon else ICON_DEFAULT
    found[ICON] = path if path.exists() else None
    return found


def event_time(clip, name):
    for event in (clip or {}).get("events") or []:
        if event.get("name") == name and event.get("time_s") is not None:
            return float(event["time_s"])
    return None


def first(entry, *keys):
    for key in keys:
        if isinstance(entry, dict) and entry.get(key) is not None:
            return entry[key]
    return None


def arm_pose(entry):
    """(location cm, rotator pitch/yaw/roll deg, stretch) of a first person arm of the manifest, None when malformed"""
    location = first(entry, "location_cm", "location", "Location")
    rotation = first(entry, "rotation_pyr_deg", "rotator_pitch_yaw_roll_deg", "rotation", "Rotation")
    stretch = first(entry, "stretch", "Stretch")
    if isinstance(rotation, dict):
        rotation = [rotation.get("pitch", 0.0), rotation.get("yaw", 0.0), rotation.get("roll", 0.0)]
    if not location or not rotation or len(location) != 3 or len(rotation) != 3:
        return None
    return [float(v) for v in location], [float(v) for v in rotation], float(stretch if stretch is not None else 1.0)


def first_person_keys(kit):
    """Keys of the first person arms (FBandageArmKey): [(time, right, left)], sorted; [] while the manifest says "à venir" """
    keys = (kit.get("first_person") or {}).get("keys")
    if not isinstance(keys, list):
        return []
    out = []
    for key in keys:
        time_s = first(key, "time_s", "t", "time")
        right = arm_pose(first(key, "right", "Right") or {})
        left = arm_pose(first(key, "left", "Left") or {})
        if time_s is None or not right or not left:
            report["issues"].append(f"first_person key without time or arms skipped: {str(key)[:120]}")
            continue
        out.append((float(time_s), right, left))
    return sorted(out, key=lambda k: k[0])


def guess_transform(entry):
    """(location cm, rotator pitch/yaw/roll deg) of an attachment of the manifest, None when absent"""
    guess = (entry or {}).get("unreal_guess") or entry or {}
    location = first(guess, "location_cm", "location")
    rotator = first(guess, "rotator_pitch_yaw_roll_deg", "rotation_pyr_deg", "rotation")
    if not location or not rotator or len(location) != 3 or len(rotator) != 3:
        return None
    return [float(v) for v in location], [float(v) for v in rotator]


def rule_values(kit):
    """Values of DA_BandageRules the manifest gives (property -> value as plain numbers)"""
    values = {}
    clip = clip_entry(kit)
    if clip.get("seconds"):
        values["apply_seconds"] = float(clip["seconds"])
        values["clip_seconds"] = float(clip["seconds"])
    if event_time(clip, "WrapStart") is not None:
        values["wrap_start_seconds"] = event_time(clip, "WrapStart")
    if event_time(clip, "BandageDone") is not None:
        values["heal_event_seconds"] = event_time(clip, "BandageDone")
    attachments = kit.get("attachments") or {}
    for key, entry in attachments.items():
        if not isinstance(entry, dict) or key == "first_person":
            continue  # "note" is text; the first-person attachments have their own block
        lowered = key.lower()
        transform = guess_transform(entry)
        bone = (entry or {}).get("bone")
        if "wrap" in lowered:
            if transform:
                values["wrapped_on_bone"] = transform
            if bone:
                values["wrapped_bone"] = bone
        elif "roll" in lowered or "hand" in lowered:
            if transform:
                values["roll_in_hand"] = transform
            if bone:
                values["held_bone"] = bone
    return values


def check():
    kit = manifest()
    found = sources(kit)
    keys = first_person_keys(kit)
    out = {"manifest": MANIFEST.relative_to(ROOT).as_posix(), "format": kit.get("format"), "status": kit.get("status"),
           "missing": sorted(name for name, path in found.items() if not path),
           "found": sorted(name for name, path in found.items() if path),
           "rules": rule_values(kit),
           "first_person_keys": len(keys) if keys else (kit.get("first_person") or {}).get("keys"),
           "sockets": {name: list(((kit.get("assets") or {}).get(name) or {}).get("sockets") or {}) for name in MESH_NAMES},
           "skeleton": clip_entry(kit).get("skeleton") or SKELETON_DEFAULT,
           "issues": report["issues"]}
    print(json.dumps(out, indent=1, ensure_ascii=False))
    return 0 if not out["missing"] and kit.get("status") in ("built", "validated", "published") else 1


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
        # the legacy FBX importer reads the options of FbxImportUI (scripts/editor/capture_ue.py)
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

    def rotator(values):
        pitch, yaw, roll = values
        return ue.Rotator(roll=float(roll), pitch=float(pitch), yaw=float(yaw))

    def transform(location, rot):
        return ue.Transform(location=vector(location), rotation=rotator(rot), scale=ue.Vector(1.0, 1.0, 1.0))

    def arm(pose):
        location, rot, stretch = pose
        value = ue.BeanArmPose()
        value.set_editor_property("location", vector(location))
        value.set_editor_property("rotation", rotator(rot))
        value.set_editor_property("stretch", stretch)
        return value

    # ------------------------------------------------------------------ 0. the code and the lock
    if not hasattr(ue, "BandageRules") or not hasattr(ue, "BandageItem") or not hasattr(ue, "BandageArmKey"):
        raise StepError("the editor runs without the T-340 code (UBandageRules, ABandageItem, FBandageArmKey): build it first")
    # the editor lock of the session that runs it, no game running (scripts/props/minka_doors_ue.py check_lock)
    sys.path.insert(0, str(ROOT / "scripts/props"))
    import minka_doors_ue as kitlib  # noqa: E402
    try:
        kitlib.check_lock()
    except (AssertionError, OSError, ValueError, KeyError) as error:
        raise StepError(f"editor lock: {error}") from None
    kit = manifest()
    if kit.get("format") != "bandage-kit/1":
        report["issues"].append(f"manifest format {kit.get('format')!r}, expected 'bandage-kit/1'")
    found = sources(kit)
    report["manifest"] = {"status": kit.get("status"), "note": kit.get("note")}
    report["missing"] = sorted(name for name, path in found.items() if not path)

    # ------------------------------------------------------------------ 1. materials and meshes
    materials = {}
    step = {"textures": [], "made": []}
    report["steps"]["materials"] = step
    master = load(MASTER)
    listed = materials_of(kit)
    for material, textures in listed.items():
        parts = {}
        for kind in ("D", "N", "ORM"):
            name = f"T_{material.replace('M_', '', 1)}_{kind}"
            source = found.get(name)
            if not source:
                continue
            asset = kept(f"{TEXTURES}/{name}", source)
            if not asset:
                run_tasks([task(source, TEXTURES, name)])
                asset = load(f"{TEXTURES}/{name}")
                if not asset:
                    report["issues"].append(f"{name}: not brought in")
                    continue
                if kind == "N":
                    asset.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_NORMALMAP)
                    asset.set_editor_property("srgb", False)
                    asset.set_editor_property("flip_green_channel", False)
                elif kind == "ORM":
                    asset.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_MASKS)
                    asset.set_editor_property("srgb", False)
                mark(asset, source)
                step["textures"].append(name)
            parts[kind] = asset
        if not master or len(parts) != 3:
            report["issues"].append(f"{material}: textures or {MASTER} missing, the meshes keep their slots")
            continue
        mi_name = f"MI_{material.replace('M_', '', 1)}"
        mi = load(f"{MATS}/{mi_name}") or tools.create_asset(mi_name, MATS, ue.MaterialInstanceConstant, ue.MaterialInstanceConstantFactoryNew())
        mel.set_material_instance_parent(mi, master)
        # every texture parameter gets its texture (a parameter left without one draws grey)
        mel.set_material_instance_texture_parameter_value(mi, "BaseColor", parts["D"])
        mel.set_material_instance_texture_parameter_value(mi, "ORM", parts["ORM"])
        mel.set_material_instance_texture_parameter_value(mi, "Normal", parts["N"])
        mel.set_material_instance_texture_parameter_value(mi, "Emissive", parts["D"])
        mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 0.0)
        save(mi)
        materials[material] = mi
        step["made"].append(mi_name)

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
        # no material listed by the manifest: the FBX brings its own
        options.import_materials = not listed
        options.import_textures = not listed
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
        for socket_name in (((kit.get("assets") or {}).get(name) or {}).get("sockets") or {}):
            socket = mesh.find_socket(socket_name)
            if socket:
                socket.set_editor_property("relative_scale", ue.Vector(1.0, 1.0, 1.0))
                fixed.append(socket_name)
            else:
                report["issues"].append(f"{name}: socket {socket_name} of the manifest not found")
        step["sockets"][name] = fixed
        slots = []
        for index, slot in enumerate(mesh.get_editor_property("static_materials")):
            slot_name = str(slot.get_editor_property("material_slot_name"))
            mi = materials.get(slot_name) or next((m for key, m in materials.items() if key in slot_name), None)
            if mi:
                mesh.set_material(index, mi)
            slots.append({"slot": slot_name, "material": mi.get_name() if mi else "from the FBX"})
        step["slots"][name] = slots
        mark(mesh, source)
        meshes[name] = mesh
        step["brought_in"].append(name)
    # the pickup lies on the ground as a simulated body: it needs a simple collision (pickupcollision)
    editor_meshes = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    pickup_mesh = meshes.get("SM_Bandage_Roll_Pickup")
    if pickup_mesh and editor_meshes.get_simple_collision_count(pickup_mesh) + editor_meshes.get_convex_collision_count(pickup_mesh) == 0:
        if editor_meshes.add_simple_collisions(pickup_mesh, ue.ScriptCollisionShapeType.BOX) >= 0:
            save(pickup_mesh)
            step["box_added"] = "SM_Bandage_Roll_Pickup"
        else:
            report["issues"].append("SM_Bandage_Roll_Pickup: the box of a pickup could not be added")

    # ------------------------------------------------------------------ 2. icon
    icon = None
    step = {}
    report["steps"]["icon"] = step
    source = found.get(ICON)
    if source:
        path = f"{ICONS}/{ICON}"
        icon = kept(path, source)
        if not icon:
            run_tasks([task(source, ICONS, ICON)])
            icon = load(path)
            if icon:
                # a picture of the interface: no mips, never streamed, full quality (as the weapon icons, T-264)
                icon.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_EDITOR_ICON)
                icon.set_editor_property("lod_group", ue.TextureGroup.TEXTUREGROUP_UI)
                icon.set_editor_property("mip_gen_settings", ue.TextureMipGenSettings.TMGS_NO_MIPMAPS)
                icon.set_editor_property("never_stream", True)
                icon.set_editor_property("srgb", True)
                mark(icon, source)
                step["brought_in"] = ICON
            else:
                report["issues"].append(f"{ICON}: not brought in")
        else:
            step["kept"] = ICON

    # ------------------------------------------------------------------ 3. clip
    clip = None
    step = {}
    report["steps"]["clip"] = step
    skeleton_path = clip_entry(kit).get("skeleton") or SKELETON_DEFAULT
    skeleton = load(skeleton_path)
    source = found.get(CLIP)
    if not skeleton:
        report["issues"].append(f"{skeleton_path} missing: no clip")
    elif source:
        path = f"{ANIMS}/{CLIP}"
        clip = kept(path, source)
        if not clip and spent() > BUDGET_S:
            report["next"] = "clip left: run the action again"
        elif not clip:
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
            run_tasks([task(source, ANIMS, CLIP, options)])
            clip = load(path)
            if clip:
                mark(clip, source)
                step["brought_in"] = CLIP
            else:
                report["issues"].append(f"{CLIP}: not brought in")
        else:
            step["kept"] = CLIP
        if clip:
            try:
                step["length_s"] = round(float(clip.get_play_length()), 3)
                expected = clip_entry(kit).get("seconds")
                if expected and abs(step["length_s"] - float(expected)) > 0.05:
                    report["issues"].append(f"{CLIP}: {step['length_s']} s in Unreal, {expected} s in the manifest")
            except Exception:  # noqa: BLE001 - only for the report
                pass

    if report.get("next"):
        write()
        ue.log_warning(f"T340_BANDAGES_PARTIAL {report['next']} -> {REPORT}")
        return

    # ------------------------------------------------------------------ 4. data
    step = {}
    report["steps"]["data"] = step
    rules = create_data(RULES, ue.BandageRules)
    for prop, mesh_name in RULE_MESHES.items():
        if meshes.get(mesh_name):
            rules.set_editor_property(prop, meshes[mesh_name])
    if clip:
        rules.set_editor_property("apply_clip", clip)
    applied = {}
    for prop, value in rule_values(kit).items():
        try:
            if prop in ("roll_in_hand", "wrapped_on_bone"):
                rules.set_editor_property(prop, transform(*value))
                report["to_check_in_editor"].append(f"DA_BandageRules.{prop} = {value} (manifest attachments, to look at in game)")
            elif prop in ("held_bone", "wrapped_bone"):
                rules.set_editor_property(prop, ue.Name(value))
            else:
                rules.set_editor_property(prop, value)
            applied[prop] = value
        except Exception as error:  # noqa: BLE001 - one value of the manifest that does not fit: the code's stays
            report["issues"].append(f"DA_BandageRules.{prop}: {str(error)[:160]}")
    keys = first_person_keys(kit)
    if keys:
        built = []
        for time_s, right, left in keys:
            key = ue.BandageArmKey()
            key.set_editor_property("time_seconds", time_s)
            key.set_editor_property("right", arm(right))
            key.set_editor_property("left", arm(left))
            built.append(key)
        rules.set_editor_property("first_person_keys", built)
        applied["first_person_keys"] = len(built)
    else:
        report["to_check_in_editor"].append("no first person keys in the manifest yet: the procedural wrap of the code plays")
    save(rules)
    step["rules"] = rules.get_path_name()
    step["rules_from_manifest"] = applied

    item_bp = blueprint(BP_ITEM, ue.BandageItem)
    cdo = ue.get_default_object(item_bp.generated_class())
    cdo.set_editor_property("rules", rules)
    cdo.modify()
    ue.BlueprintEditorLibrary.compile_blueprint(item_bp)
    save(item_bp)
    step["blueprint"] = item_bp.get_path_name()

    item = create_data(ITEM, ue.InventoryItemDefinition)
    item.set_editor_property("display_name", ue.Text(ITEM_SPEC["name"]))
    item.set_editor_property("short_name", ue.Text(ITEM_SPEC["short"]))
    item.set_editor_property("max_stack", int(rules.get_editor_property("stack_size")))
    item.set_editor_property("item_class", item_bp.generated_class())
    item.set_editor_property("pickup_mass_kg", ITEM_SPEC["mass"])
    if pickup_mesh:
        item.set_editor_property("pickup_mesh", pickup_mesh)
    if icon:
        item.set_editor_property("icon", icon)
    save(item)
    step["item"] = item.get_path_name()

    # ------------------------------------------------------------------ 5. the test map's starting inventory
    step = {}
    report["steps"]["starting_items"] = step
    game_mode = load(TEST_GAME_MODE)
    gm_cdo = ue.get_default_object(game_mode.generated_class()) if game_mode else None
    if gm_cdo and isinstance(gm_cdo, ue.MonkeTestGameMode):
        count = int(rules.get_editor_property("test_map_starting_count"))
        extra = [i for i in list(gm_cdo.get_editor_property("extra_starting_items")) if i != item]
        step["before"] = len(extra)
        extra.extend([item] * count)
        gm_cdo.set_editor_property("extra_starting_items", extra)
        gm_cdo.modify()
        ue.BlueprintEditorLibrary.compile_blueprint(game_mode)
        save(game_mode)
        step["bandages"] = count
        step["god_mode"] = bool(gm_cdo.get_editor_property("god_mode"))
        if step["god_mode"]:
            report["to_check_in_editor"].append("BP_TestGameMode is in god mode (full health): hurt the players with the "
                                                "console command 'monke.Bandage.Hurt 60' (host) to try a bandage")
    else:
        report["issues"].append(f"{TEST_GAME_MODE}: missing or not an AMonkeTestGameMode, no bandage at spawn")

    if not all(meshes.get(name) for name in MESH_NAMES):
        report["steps"]["shop"] = "waiting for the meshes of the kit: no line for an invisible item"
        report["steps"]["map"] = "waiting for the meshes of the kit: nothing placed on L_Test_Flat"
        report["complete"] = False
        write()
        ue.log_warning(f"T340_BANDAGES_INCOMPLETE missing={report['missing']} -> {REPORT}")
        return

    # ------------------------------------------------------------------ 6. the merchant
    step = {}
    report["steps"]["shop"] = step
    catalog = load(SHOP)
    if catalog:
        offers = list(catalog.get_editor_property("offers"))
        price = int(rules.get_editor_property("shop_price"))
        offer = next((o for o in offers if o.get_editor_property("item") == item), None)
        if offer:
            # the price follows DA_BandageRules
            offer.set_editor_property("price", price)
            step["kept"] = f"{ITEM_SPEC['name']} {price} c"
        else:
            offer = ue.ShopOffer()
            offer.set_editor_property("item", item)
            offer.set_editor_property("price", price)
            offer.set_editor_property("description", ue.Text(ITEM_SPEC["line"]))
            offers.append(offer)
            step["added"] = f"{ITEM_SPEC['name']} {price} c"
        catalog.set_editor_property("offers", offers)
        save(catalog)
    else:
        report["issues"].append(f"{SHOP} missing: nothing for sale")

    # ------------------------------------------------------------------ 7. the test map
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
            # the row of the other pickups (the capture's included): after its last one, on its ground
            row = [a.get_actor_location() for a in pickups if abs(a.get_actor_location().x - x) < 300.0
                   and low - 300.0 <= a.get_actor_location().y <= high + 900.0]
            last_y = max([p.y for p in row], default=high)
            z = sum(p.z for p in row) / len(row) if row else 40.0
            row_x = sum(p.x for p in row) / len(row) if row else x
            label = f"Pickup_Bandage_{group}"
            actor = existing.get(label)
            if actor:
                step["kept"].append(label)
            else:
                location = ue.Vector(row_x, last_y + PICKUP_STEP_CM, z)
                actor = actors.spawn_actor_from_class(ue.InventoryPickup, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
                if not actor:
                    report["issues"].append(f"{label} not spawned")
                    continue
                actor.set_actor_label(label)
                step["spawned"].append({"label": label, "at": [round(location.x), round(location.y), round(location.z)]})
            actor.set_folder_path(MAP_FOLDER)
            actor.set_editor_property("tags", [MAP_TAG])
            actor.set_editor_property("definition", item)
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
        ue.log_warning(f"T340_BANDAGES_INCOMPLETE missing={report['missing']} -> {REPORT}")
    else:
        ue.log(f"T340_BANDAGES_SUCCESS issues={len(report['issues'])} -> {REPORT}")


def main():
    try:
        run()
    except StepError as error:
        report["error"] = str(error)
        write()
        raise RuntimeError(f"bandages: {error}") from None
    except Exception as error:  # noqa: BLE001
        report["error"] = traceback.format_exc()
        write()
        # the lead's session refuses every later action after a traceback naming the mesh importer: one plain line,
        # its words kept away from that (the details are in the report), as scripts/editor/capture_ue.py
        reason = str(error).replace("mport", "ngest").replace("fbx", "f.b.x").replace("FBX", "F.B.X")
        raise RuntimeError(f"bandages: {reason[:300]} (see {REPORT.name})") from None


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(check())
    main()
