"""T-335: the bolt-action marksman rifle, the submachine gun and the bayonet musket in Unreal (lead session action
"newweapons", scripts/editor/lead_session_t186.py; editor open, lock held, no game running). Replays are idempotent: every
step keeps what is already in place.

Packs of the art task T-334: art/export/Weapons/<Weapon>/pack.json, the manifest of the pack (format bean-musket-anim/1;
"status": "built" once the Blender build wrote its files next to it, "provisional" before), resolved by
scripts/character/import_pirate_weapons.py (registry entries BoltRifle, Mitraillette, BayonetMusket; art_pack_manifest):
  BoltRifle      BP_BoltRifle      ABoltRifleWeapon      DA_BoltRifleRules      UBoltRifleRules
  Mitraillette   BP_Mitraillette   AMachineGunWeapon     DA_MitrailletteRules   USubmachineGunRules
  BayonetMusket  BP_BayonetMusket  ABayonetMusketWeapon  DA_BayonetMusketRules  UBayonetMusketRules
MONKE_NEW_WEAPONS=BoltRifle,Mitraillette picks the weapons (default: the three); from the lead's editor session, which
cannot pass an environment, monke_island/Saved/T335/new-weapons-request.json {"weapons": ["BayonetMusket"]} does the same.
A selected weapon whose pack is missing, still provisional, incomplete or being written stops the run BEFORE anything is
touched (a clear message, the report): run it again once T-334 delivered, or pick the weapons that are ready.

0. Preflight (no change): the C++ of T-335 in this editor (classes, EFirearmGesture BoltCycle / BayonetStab /
   BayonetCharge, EAmmoKind SmgCartridge), no game running, every selected pack built: its files there (sha256 when the
   manifest gives one), FBX in metres, nothing written in the last 20 s.
1. Import (FBX: import_pirate_weapons.run_in_unreal with MONKE_PIRATE_WEAPONS = that weapon): the weapon with its own
   skeleton, the FP arms on the project's bean skeleton, the clips, the statics, the PBR materials. ONE weapon per run:
   the editor freezes for the import (about 1 to 3 min per pack, estimated); when others are left the run stops there and
   says so (run it again). A weapon whose manifest (sha256) was imported already and whose base is in place is not
   imported again (MONKE_NEW_WEAPONS_FORCE_IMPORT=1 imports it again).
2. Data, once every selected weapon is imported: DA_<Weapon>Rules (weapon mesh, FP arms, the four clips of every gesture
   of the manifest with its lengths and events, align bone, grip and muzzle sockets, FP camera, hip and aim framing of the
   pack, sounds of the existing weapons, flash and blast, tracers, the pack's cases / empty clip / drum, the event where
   the rounds count; the bayonet musket copies the tuned shot of DA_PirateMusketRules and takes the cutlass's sounds for
   its point), BP_<Weapon> (rules, the firearms' keys: IMC_Musket, IA_Musket*, IA_FirearmInspect; the bayonet is the
   right click), DA_Item_<Weapon> (name, pickup mesh, icon of the art task), the ammunition DA_Ammo_StripperClip (5 rifle
   cartridges), DA_Ammo_RifleClipBox (20), DA_Ammo_SmgDrum (50 submachine gun cartridges) and their BP_Ammo_*. Read back
   after saving.
3. The merchant (DA_ShopCatalog): the weapons and their ammunition, lines added when missing, only for a weapon that has
   its model (never an invisible gun for sale).
4. Test map L_Test_Flat (when it is the open map, or no map has unsaved changes): Pickup_BoltRifle, Pickup_Mitraillette,
   Pickup_BayonetMusket at the end of the row of the weapon pickups already there (the InventoryPickup of
   /Game/MonkeIsland/Weapons items, the row continued at its own spacing), their ammunition a row behind; folder
   WeaponsTest, tag T335_NewWeapons; saved unless the map had unsaved changes (then left for its owner to save).
Report: monke_island/Saved/T335/new-weapons.json.

Dry run outside Unreal (packs, gestures, what is missing; exit 1 while a pack is not ready):
  python scripts/editor/new_weapons_ue.py --check
"""
import datetime as dt
import glob
import hashlib
import json
import os
import re
import sys
import time
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHAR = ROOT / "scripts" / "character"
if str(CHAR) not in sys.path:
    sys.path.insert(0, str(CHAR))
import import_pirate_weapons as imp  # noqa: E402 - pure Python until run_in_unreal
import setup_pirate_weapons as sp  # noqa: E402 - pure Python until run_in_unreal

OUT = ROOT / "monke_island/Saved/T335"
REPORT = OUT / "new-weapons.json"
REQUEST = OUT / "new-weapons-request.json"
ICON_PNG = ROOT / "art/ui/icons/weapons"
W = "/Game/MonkeIsland/Weapons"
MUSKET = f"{W}/Musket"
SHARED_AUDIO = f"{W}/Audio"
AMMO_FOLDER = f"{W}/Ammo"
ICONS = "/Game/MonkeIsland/UI/Icons/Weapons"
SHOP = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
MAP = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
INSPECT_ACTION = f"{W}/Input/IA_FirearmInspect"
SPHERE = "/Engine/BasicShapes/Sphere"
FLASH = f"{MUSKET}/FX/M_MusketFlash"
FOLDER_LABEL = "WeaponsTest"
TAG = "T335_NewWeapons"
SETTLED_SECONDS = 20.0
LOOPS = {"Idle", "Walk", "Sprint", "AimIdle", "Hold", "FireLoop"}
ALL = ("BoltRifle", "Mitraillette", "BayonetMusket")

# sounds of the shared events of the firearms (T-259): event -> (candidate assets, volume)
SHARED_EVENTS = {
    "DrawWeapon": (["SW_Firearm_Draw"], 0.9), "Ready": (["SW_Firearm_Ready"], 0.7), "StowWeapon": (["SW_Firearm_Stow"], 0.9),
    "Empty": (["SW_Firearm_DiscardEmpty"], 1.0), "Release": (["SC_Firearm_DiscardRelease", "SW_Firearm_DiscardRelease_01"], 1.0),
}
# the weapons: classes, item, shop line, where their sounds and effects come from (existing assets of the project)
WEAPONS = {
    "BoltRifle": {
        "rules_cls": "BoltRifleRules", "cls": "BoltRifleWeapon", "display": "Fusil à verrou", "short": "FV", "mass": 4.6,
        "price": 1200, "line": "Fusil de tireur d'élite à verrou : lunette au clic droit, 5 coups, 150 de dégâts à 300 m",
        "fire_sound": ["SC_Rifle_Fire", "SW_Rifle_Fire_01"],
        "events": {"BoltOpen": (["SC_Shotgun_PumpBack", "SW_Shotgun_PumpBack_01"], 0.7),
                   "CaseEject": (["SW_Shotgun_ShellEject"], 0.8),
                   "BoltClose": (["SC_Shotgun_PumpForward", "SW_Shotgun_PumpForward_01"], 0.8),
                   "ClipIn": (["SW_Longgun_BallPatch"], 0.9),
                   "RoundsIn": (["SC_Shotgun_ShellInsert", "SW_Shotgun_ShellInsert_01"], 1.0),
                   "ClipEject": (["SW_Firearm_HandGrab"], 0.8),
                   "DryClick": (["SW_Shotgun_DryClick"], 1.0)},
        "blast_from": f"{W}/MachineGun/DA_MachineGunRules", "tracer_from": f"{W}/Rifle/DA_RifleRules",
        "tracer": {"every_nth": 1, "speed_cmps": 32000.0, "length_cm": 260.0, "width_cm": 3.0, "max_distance_cm": 30000.0},
        "statics": {"spent_case_mesh": ["SM_BoltRifle_Casing"], "empty_clip_mesh": ["SM_BoltRifle_EmptyClip"]},
        "loaded_event": ("reload_loaded_event", "Reload", ["RoundsIn", "ClipIn", "MagIn"]),
        "sockets": {"eject_socket": ["socket_eject"], "clip_socket": ["socket_clip"]},
    },
    "Mitraillette": {
        "rules_cls": "SubmachineGunRules", "cls": "MachineGunWeapon", "display": "Mitraillette", "short": "MT", "mass": 4.9,
        "price": 1500, "line": "Mitraillette à tambour : 50 coups, 11 coups par seconde, tenue à la main",
        "fire_sound": ["SC_MG_Round", "SW_MG_Round_01"],
        "rule_sounds": {"burst_tail_sound": ["SC_MG_BurstTail", "SW_MG_BurstTail_01"],
                        "casing_land_sound": ["SC_MG_CasingLand", "SW_MG_CasingLand_01"]},
        "events": {"MagOut": (["SW_MG_BeltOut"], 0.9), "MagDrop": (["SW_MG_BoxOut"], 0.8), "MagIn": (["SW_MG_BoxIn"], 1.0),
                   "Charge": (["SW_MG_Charge"], 1.0), "DryClick": (["SW_Shotgun_DryClick"], 1.0)},
        "blast_from": f"{W}/MachineGun/DA_MachineGunRules", "tracer_from": f"{W}/MachineGun/DA_MachineGunRules",
        "tracer": {"every_nth": 4, "length_cm": 90.0, "width_cm": 1.8},
        "statics": {"casing_mesh": ["SM_Mitraillette_Casing"], "dropped_box_mesh": ["SM_Ammo_MitrailletteDrum"]},
        "loaded_event": ("reload_loaded_event", "Reload", ["MagIn"]),
        "sockets": {"eject_socket": ["socket_eject"], "eject_direction_socket": ["socket_eject_dir"], "dropped_box_socket": ["socket_drum"]},
    },
    "BayonetMusket": {
        "rules_cls": "BayonetMusketRules", "cls": "BayonetMusketWeapon", "display": "Mousquet à baïonnette", "short": "MB", "mass": 5.2,
        "price": 650, "line": "Mousquet à baïonnette : clic droit pour frapper, en courant pour charger",
        "copy_from": f"{MUSKET}/DA_PirateMusketRules",
        "events": {}, "statics": {}, "sockets": {},
    },
}
# the tuned shot of the musket the bayonet musket keeps ("le tir et le rechargement du mousquet restent inchangés")
MUSKET_SHOT = ("damage", "range_cm", "falloff_start_cm", "falloff_min_scale", "damage_falloff", "limb_damage_scale", "bone_hit_radius_cm",
               "hip_spread_degrees", "aim_spread_degrees", "steady_speed_cmps", "knockback_speed", "physics_impulse", "friendly_fire",
               "max_balls", "start_loaded", "reload_seconds", "auto_reload_on_empty_fire", "view_kick_pitch", "view_kick_yaw",
               "fire_sound", "event_sounds", "event_sound_attenuation", "flash_mesh", "flash_material", "flash_scale", "flash_seconds",
               "flash_light_intensity", "flash_light_color", "flash_light_radius", "muzzle_blast_systems", "impact_system", "tracer",
               "effects_cull_distance", "discard_when_empty")
# the ammunition of the new weapons: DA_Ammo_<key> -> name, lots (EAmmoKind, count), the pack's mesh, price, mass, the
# weapon whose pack brings it, how many lie on the test map
AMMO = {
    "StripperClip": {"display": "Lame-chargeur", "lots": [("RIFLE_CARTRIDGE", 5)], "meshes": ["SM_Ammo_StripperClip"], "price": 50, "mass": 0.2,
                     "line": "Lame-chargeur : 5 cartouches de fusil (fusil à verrou, carabine)", "weapon": "BoltRifle", "on_map": 2},
    "RifleClipBox": {"display": "Boîte de lames-chargeurs", "lots": [("RIFLE_CARTRIDGE", 20)], "meshes": ["SM_Ammo_RifleClipBox"], "price": 180,
                     "mass": 0.9, "line": "Boîte de quatre lames-chargeurs : 20 cartouches de fusil", "weapon": "BoltRifle", "on_map": 1},
    "SmgDrum": {"display": "Tambour de mitraillette", "lots": [("SMG_CARTRIDGE", 50)], "meshes": ["SM_Ammo_MitrailletteDrum"], "price": 120,
                "mass": 2.2, "line": "Tambour plein : 50 cartouches de mitraillette", "weapon": "Mitraillette", "on_map": 2},
}


class NotReady(Exception):
    """A pack or the C++ is not there yet: nothing was touched"""


class IngestFailed(Exception):
    """The FBX import of a pack failed (the lead's session then refuses further imports until a restart)"""


# ---------------------------------------------------------------------------------------------- the packs (no Unreal)

def selected():
    """The weapons to do: MONKE_NEW_WEAPONS, else monke_island/Saved/T335/new-weapons-request.json {"weapons": [...]} (the
    lead's editor session cannot pass an environment to an action), else the three"""
    names = None
    if os.environ.get("MONKE_NEW_WEAPONS"):
        names = [n.strip() for n in os.environ["MONKE_NEW_WEAPONS"].split(",") if n.strip()]
    elif REQUEST.exists():
        try:
            names = [str(n).strip() for n in json.loads(REQUEST.read_text(encoding="utf-8-sig")).get("weapons") or [] if str(n).strip()]
        except (OSError, ValueError, AttributeError) as error:
            raise NotReady(f"{REQUEST.name} unreadable ({error}): fix or delete it") from None
    names = names or list(ALL)
    unknown = [n for n in names if n not in WEAPONS]
    if unknown:
        raise NotReady(f"unknown weapon(s) {unknown} in MONKE_NEW_WEAPONS (known: {', '.join(ALL)})")
    return names


def pack_of(name):
    """(folder, manifest file, manifest) of a weapon's pack, (None, None, None) when missing"""
    folder, path = imp.art_pack_manifest(imp.WEAPONS[name]["art_pack"])
    if not path:
        return None, None, None
    with open(path, encoding="utf-8") as handle:
        return folder, path, json.load(handle)


def sha256(path):
    digest = hashlib.sha256()
    with open(path, "rb") as handle:
        for chunk in iter(lambda: handle.read(1 << 20), b""):
            digest.update(chunk)
    return digest.hexdigest()


def pack_problems(name):
    """Why a weapon's pack cannot be taken now (empty list: ready)"""
    folder, path, manifest = pack_of(name)
    if not path:
        return [f"no pack: art/export/Weapons/{imp.WEAPONS[name]['art_pack']}/pack.json (T-334)"]
    problems = []
    status = str(manifest.get("status", "")).lower()
    if status in ("provisional", "planned", "draft"):
        problems.append(f"pack still {status} (T-334 has not built it yet)")
    missing, mismatch, units, newest = [], [], set(), os.path.getmtime(path)
    for entry in manifest.get("assets") or []:
        file = os.path.join(folder, entry["path"])
        if not os.path.isfile(file):
            missing.append(entry["path"])
            continue
        newest = max(newest, os.path.getmtime(file))
        if entry.get("sha256") and sha256(file) != entry["sha256"]:
            mismatch.append(entry["path"])
        if file.lower().endswith(".fbx"):
            units.add(str(imp.fbx_unit(file)))
    if missing:
        problems.append(f"{len(missing)} file(s) of the manifest missing, e.g. {missing[:3]}")
    if mismatch:
        problems.append(f"{len(mismatch)} file(s) differ from their sha256, e.g. {mismatch[:3]}")
    if units - {"100.0"}:
        problems.append(f"FBX units {sorted(units)} (metres expected: 100.0)")
    settled = time.time() - newest
    if settled < SETTLED_SECONDS:
        problems.append(f"the pack was written {settled:.0f} s ago (a build still running?)")
    base = imp.bases(name)
    if not base["base"] or not base["fp"]:
        problems.append(f"no weapon or FP arms skeletal mesh in the manifest ({base})")
    return problems


def clip_timing(manifest):
    """clip -> (seconds, [(seconds, event)], loop) from the TP scenes of the manifest"""
    out = {}
    for scene in (manifest or {}).get("scenes") or []:
        if scene.get("view") != "TP":
            continue
        fps = float(scene.get("fps") or 30)
        start = scene.get("start", 0)
        frames = max(scene.get("end", start + 1) - start, 1)
        events = [((event["frame"] - start) / fps, event["name"]) for event in scene.get("events") or []]
        out[scene["clip"]] = (frames / fps, events, bool(scene.get("loop", scene["clip"] in LOOPS)))
    return out


def fp_framing(folder, manifest):
    framing = (manifest or {}).get("fp_framing")
    path = os.path.join(folder, "fp-framing.json") if folder else None
    if not framing and path and os.path.exists(path):
        with open(path, encoding="utf-8") as handle:
            framing = json.load(handle)
    return framing if isinstance(framing, dict) else None


def contract_bones(manifest):
    contract = (manifest or {}).get("weapon_contract") or {}
    return set(contract.get("bones") or []) | {f"socket_{key}" for key in (contract.get("sockets") or {})}


def first_bone(bones, candidates):
    """The first candidate the weapon has (any when the manifest lists no bones)"""
    for candidate in candidates:
        if not bones or candidate in bones:
            return candidate
    return None


def icon_png(name, folder):
    """The picture of an inventory cell: T_Icon_<name>.png of the art task (art/ui/icons/weapons, else in its pack)"""
    candidates = [ICON_PNG / f"T_Icon_{name}.png"]
    if folder:
        candidates += [Path(p) for p in glob.glob(os.path.join(folder, "**", f"T_Icon_{name}.png"), recursive=True)]
    return next((path for path in candidates if path.exists()), None)


def check():
    plan = {"weapons": {}, "ready": True}
    try:
        names = selected()
    except NotReady as error:
        print(error)
        return 1
    for name in names:
        folder, path, manifest = pack_of(name)
        problems = pack_problems(name)
        entry = {"pack": path, "status": (manifest or {}).get("status"), "problems": problems}
        if manifest:
            entry["gestures"] = {clip: [round(seconds, 3), [event for _, event in events], loop] for clip, (seconds, events, loop)
                                 in clip_timing(manifest).items()}
            entry["statics"] = [s for s, _ in imp.statics(name)]
            entry["fp_framing"] = sorted((fp_framing(folder, manifest) or {}).keys())
            entry["icon"] = str(icon_png(name, folder) or "none yet")
        plan["weapons"][name] = entry
        plan["ready"] &= not problems
    print(json.dumps(plan, indent=1, ensure_ascii=False))
    print("READY" if plan["ready"] else "NOT READY: the run would stop before touching anything")
    return 0 if plan["ready"] else 1


# ---------------------------------------------------------------------------------------------------------- Unreal

def run():
    import unreal as ue

    eal = ue.EditorAssetLibrary
    tools = ue.AssetToolsHelpers.get_asset_tools()
    started = time.monotonic()
    previous = {}
    if REPORT.exists():
        try:
            previous = json.loads(REPORT.read_text(encoding="utf-8"))
        except ValueError:
            previous = {}
    report = {"task": "T-335", "started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "weapons": {}, "issues": [], "missing": [],
              "imports": dict(previous.get("imports") or {}), "complete": False}

    def write():
        report["editor_seconds"] = round(time.monotonic() - started, 1)
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(report, indent=1, ensure_ascii=False, default=str), encoding="utf-8")

    def load(path, note=True):
        asset = eal.load_asset(path) if path and eal.does_asset_exist(path) else None
        if asset is None and note and path:
            report["missing"].append(path)
        return asset

    def save(asset):
        if not eal.save_loaded_asset(asset, only_if_is_dirty=False):
            report["issues"].append(f"not saved: {asset.get_path_name()}")

    def path_of(obj):
        return obj.get_path_name() if obj else None

    def set_props(obj, values, where):
        """Sets what this engine version knows, reports the rest"""
        for prop, value in values.items():
            try:
                obj.set_editor_property(prop, value)
            except Exception as error:  # noqa: BLE001
                report["issues"].append(f"{where}.{prop}: {error}")

    def ensure_folder(folder):
        if not eal.does_directory_exist(folder):
            eal.make_directory(folder)

    def data_asset(path, cls):
        if eal.does_asset_exist(path):
            return eal.load_asset(path)
        ensure_folder(path.rsplit("/", 1)[0])
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        return tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], cls, factory)

    def blueprint(path, parent):
        if eal.does_asset_exist(path):
            return eal.load_asset(path)
        ensure_folder(path.rsplit("/", 1)[0])
        factory = ue.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        return tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], ue.Blueprint, factory)

    names_cache = {}

    def names_under(folder):
        """short name -> package of the assets under a folder (recursive)"""
        if folder not in names_cache:
            out = {}
            if eal.does_directory_exist(folder):
                for path in eal.list_assets(folder, recursive=True, include_folder=False):
                    package = path.split(".")[0]
                    out.setdefault(package.rsplit("/", 1)[1], package)
            names_cache[folder] = out
        return names_cache[folder]

    def find(candidates, folder=W):
        """The first of the candidate short names that exists under folder (loaded), None otherwise"""
        known = names_under(folder)
        for short in candidates:
            if short in known:
                return load(known[short], note=False)
        return None

    def camel_upper(name):
        return re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()

    # ------------------------------------------------------------------ 0. preflight
    if ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor():
        raise NotReady("a game is running in the editor: stop it first")
    names = selected()
    report["selected"] = names
    missing_code = [n for n in ("BoltRifleWeapon", "BoltRifleRules", "SubmachineGunRules", "BayonetMusketWeapon", "BayonetMusketRules",
                                "MachineGunWeapon") if not hasattr(ue, n)]
    for enum, value in (("FirearmGesture", "BOLT_CYCLE"), ("FirearmGesture", "BAYONET_STAB"), ("FirearmGesture", "BAYONET_CHARGE"),
                        ("AmmoKind", "SMG_CARTRIDGE")):
        if not hasattr(getattr(ue, enum), value):
            missing_code.append(f"{enum}.{value}")
    if missing_code:
        report["code_missing"] = missing_code
        write()
        raise NotReady(f"the C++ of T-335 is not in this editor ({', '.join(missing_code)}): build it (editor closed), then run again")
    problems = {name: pack_problems(name) for name in names}
    problems = {name: found for name, found in problems.items() if found}
    if problems:
        report["packs_not_ready"] = problems
        write()
        raise NotReady("pack(s) not ready, nothing touched: " + "; ".join(f"{name}: {found[0]}" for name, found in problems.items())
                       + " (MONKE_NEW_WEAPONS picks the weapons that are ready)")

    # ------------------------------------------------------------------ 1. import, one pack per run
    force = bool(os.environ.get("MONKE_NEW_WEAPONS_FORCE_IMPORT"))
    pending = []
    for name in names:
        digest = sha256(imp.manifest_file(name))
        in_place = eal.does_asset_exist(f"{imp.dest(name)}/{imp.bases(name)['base']}")
        if force or (report["imports"].get(name) or {}).get("sha256") != digest or not in_place:
            pending.append((name, digest))
    if pending:
        name, digest = pending[0]
        ue.log_warning(f"[newweapons] importing the pack of {name}: the editor freezes for a minute or more")
        imported = Path(ue.Paths.project_saved_dir()) / "MonkeTools" / "pirate_weapons_report.json"
        if imported.exists():
            # the report of this import only, never a former one
            imported.unlink()
        before = os.environ.get("MONKE_PIRATE_WEAPONS")
        os.environ["MONKE_PIRATE_WEAPONS"] = name
        try:
            imp.run_in_unreal()
        finally:
            if before is None:
                os.environ.pop("MONKE_PIRATE_WEAPONS", None)
            else:
                os.environ["MONKE_PIRATE_WEAPONS"] = before
        result = json.loads(imported.read_text(encoding="utf-8")) if imported.exists() else {}
        failed = result.get("failed") or []
        report["imports"][name] = {"sha256": digest, "at": dt.datetime.now(dt.timezone.utc).isoformat(), "imported": len(result.get("imported") or []),
                                   "failed": failed, "checks": (result.get("checks") or {}).get(name)}
        if failed or not eal.does_asset_exist(f"{imp.dest(name)}/{imp.bases(name)['base']}"):
            report["imports"][name]["sha256"] = None
            write()
            raise IngestFailed(f"{name}: {len(failed)} file(s) failed, e.g. {failed[:2]} (pirate_weapons_report.json)")
        if len(pending) > 1:
            report["next"] = f"run again: {', '.join(n for n, _ in pending[1:])} still to bring in, then the data"
            write()
            ue.log_warning(f"T335_NEWWEAPONS_PARTIAL {name} imported; run the action again for {[n for n, _ in pending[1:]]}")
            return
    names_cache.clear()

    # ------------------------------------------------------------------ 2. data
    musket_bp = load(f"{MUSKET}/BP_Musket", note=False)
    musket_cdo = ue.get_default_object(musket_bp.generated_class()) if musket_bp else None
    if not musket_cdo:
        report["missing"].append(f"{MUSKET}/BP_Musket (the keys of the firearms)")
    inspect = load(INSPECT_ACTION, note=False)
    handling = load(f"{SHARED_AUDIO}/ATT_Firearm_Handling", note=False)
    cutlass = find(["DA_CutlassRules"])
    sphere, flash = load(SPHERE, note=False), load(FLASH, note=False)

    def icon(short, folder):
        """T_Icon_<short> of the interface: the asset, or the picture of the art task brought in"""
        existing = load(f"{ICONS}/T_Icon_{short}", note=False)
        png = icon_png(short, folder)
        if existing or not png:
            return existing
        task = ue.AssetImportTask()
        task.filename = str(png)
        task.destination_path = ICONS
        task.destination_name = f"T_Icon_{short}"
        task.replace_existing = True
        task.automated = True
        task.save = False
        tools.import_asset_tasks([task])
        texture = load(f"{ICONS}/T_Icon_{short}", note=False)
        if texture:
            set_props(texture, {"compression_settings": ue.TextureCompressionSettings.TC_EDITOR_ICON, "lod_group": ue.TextureGroup.TEXTUREGROUP_UI,
                                "mip_gen_settings": ue.TextureMipGenSettings.TMGS_NO_MIPMAPS, "never_stream": True, "srgb": True}, texture.get_name())
            save(texture)
        return texture

    def sound_cues(events):
        cues = []
        for event_name, (candidates, volume) in events.items():
            sound = find(candidates)
            if not sound:
                report["issues"].append(f"no sound among {candidates} for {event_name}")
                continue
            cue = ue.FirearmEventSound()
            cue.set_editor_property("event", event_name)
            cue.set_editor_property("sound", sound)
            cue.set_editor_property("volume", float(volume))
            cues.append(cue)
        return cues

    def rules_step(name, cfg):
        folder = imp.dest(name)
        pack_folder, _, manifest = pack_of(name)
        rules = data_asset(f"{folder}/DA_{name}Rules", getattr(ue, cfg["rules_cls"]))
        entry = {"rules": path_of(rules), "clips_set": 0, "clips": {}, "clips_missing": []}
        bones = contract_bones(manifest)
        base = imp.bases(name)
        mesh = load(f"{folder}/{base['base']}")
        arms = load(f"{folder}/{base['fp']}") if base["fp"] else None
        if mesh:
            rules.set_editor_property("weapon_mesh", mesh)
        if arms:
            rules.set_editor_property("first_person_arms_mesh", arms)

        # the bayonet musket: the tuned shot and reload of the pirate musket first (its own clips come after)
        if cfg.get("copy_from"):
            source = load(cfg["copy_from"])
            if source:
                for prop in MUSKET_SHOT:
                    try:
                        rules.set_editor_property(prop, source.get_editor_property(prop))
                    except Exception as error:  # noqa: BLE001
                        report["issues"].append(f"{name}.{prop} from the musket: {error}")

        # the four clips of every gesture of the manifest, its lengths, events and loops
        clips = list(rules.get_editor_property("clips"))
        present = {clip.get_editor_property("gesture").name: clip for clip in clips}
        assets = imp.clip_assets(name)
        timing = clip_timing(manifest)
        for gesture_name, clip_name in imp.gestures(name):
            value = getattr(ue.FirearmGesture, camel_upper(gesture_name), None)
            if value is None:
                entry["clips_missing"].append(f"{gesture_name} (not an EFirearmGesture)")
                continue
            clip = present.get(value.name)
            if clip is None:
                clip = ue.FirearmClip()
                clip.set_editor_property("gesture", value)
                clips.append(clip)
                present[value.name] = clip
            row = {}
            for prop, path in assets.get(clip_name, {}).items():
                anim = load(path)
                if anim:
                    clip.set_editor_property(prop, anim)
                    entry["clips_set"] += 1
                    row[prop] = path.rsplit("/", 1)[1]
            if clip_name in timing:
                seconds, events, loop = timing[clip_name]
                cues = []
                for at, event_name in events:
                    event = ue.FirearmClipEvent()
                    event.set_editor_property("name", event_name)
                    event.set_editor_property("time", float(at))
                    cues.append(event)
                clip.set_editor_property("events", cues)
                clip.set_editor_property("duration", float(seconds))
                clip.set_editor_property("loop", bool(loop))
                row["seconds"] = round(seconds, 3)
                row["events"] = [event_name for _, event_name in events]
            entry["clips"][gesture_name] = row
        rules.set_editor_property("clips", clips)

        # bones and sockets of the pack, the FP camera and framing
        framing = fp_framing(pack_folder, manifest) or {}
        contract = manifest.get("weapon_contract") or {}
        align = framing.get("align_bone") or contract.get("root_bone")
        if align and (not bones or align in bones):
            rules.set_editor_property("first_person_align_bone", align)
        grip = framing.get("grip_socket") or first_bone(bones, ["socket_grip_r"])
        if grip:
            rules.set_editor_property("grip_socket", grip)
        muzzle = contract.get("muzzle_socket") or first_bone(bones, ["socket_muzzle"])
        if muzzle:
            rules.set_editor_property("muzzle_socket", muzzle)
        for prop, candidates in cfg.get("sockets", {}).items():
            bone = first_bone(bones, candidates)
            set_props(rules, {prop: bone or "None"}, f"DA_{name}Rules")
        view = sp.fp_view(name)
        if view:
            rules.set_editor_property("first_person_view_location", ue.Vector(*view["location_cm"]))
            pitch, yaw, roll = view["rotation_pyr"]
            rules.set_editor_property("first_person_view_rotation", ue.Rotator(roll=roll, pitch=pitch, yaw=yaw))
            rules.set_editor_property("first_person_field_of_view", 0.0)
            entry["fp_view"] = view
        if "hip_grip_location_cm" in framing:
            rules.set_editor_property("hip_grip_location", ue.Vector(*framing["hip_grip_location_cm"]))
            pitch, yaw, roll = (list(framing.get("hip_rotation_pyr_deg") or [0.0, 0.0, 0.0]) + [0.0, 0.0, 0.0])[:3]
            rules.set_editor_property("hip_rotation", ue.Rotator(roll=roll, pitch=pitch, yaw=yaw))
        if "aim_muzzle_location_cm" in framing:
            # the pack's aim camera reproduced exactly (the scope's eye on the view axis for the rifle): the muzzle placed
            rules.set_editor_property("aim_muzzle_location", ue.Vector(*framing["aim_muzzle_location_cm"]))
            pitch, yaw, roll = (list(framing.get("aim_rotation_pyr_deg") or [0.0, 0.0, 0.0]) + [0.0, 0.0, 0.0])[:3]
            rules.set_editor_property("aim_rotation", ue.Rotator(roll=roll, pitch=pitch, yaw=yaw))
            set_props(rules, {"aim_sight_socket": "None"}, f"DA_{name}Rules")
            if framing.get("aim_field_of_view_deg") and name != "BayonetMusket":
                set_props(rules, {"aim_field_of_view": float(framing["aim_field_of_view_deg"])}, f"DA_{name}Rules")
        elif name == "BoltRifle":
            # no aim framing in the pack: the eyepiece (socket_scope_eye) on the view axis at the C++ eye relief
            set_props(rules, {"aim_sight_socket": first_bone(bones, ["socket_scope_eye"]) or "None"}, f"DA_{name}Rules")
        entry["framing"] = {k: v for k, v in framing.items() if k.startswith(("hip_", "aim_", "align", "grip"))}

        # the event where the rounds count (the pack names it)
        if cfg.get("loaded_event"):
            prop, gesture_name, candidates = cfg["loaded_event"]
            names_in_clip = [event for _, event in (timing.get(gesture_name) or (0, [], False))[1]]
            chosen = next((candidate for candidate in candidates if candidate in names_in_clip), None)
            if chosen:
                rules.set_editor_property(prop, chosen)
            else:
                report["issues"].append(f"{name}: none of {candidates} in its {gesture_name} clip: the rounds count at its end")
        if name == "Mitraillette" and "FireLoop" in timing:
            rounds = sum(1 for _, event in timing["FireLoop"][1] if event == "Fire")
            if rounds > 0:
                rules.set_editor_property("rounds_per_crank_turn", int(rounds))
                entry["rounds_per_fire_loop"] = rounds

        # the pack's pieces (cases, empty clip, drum)
        statics = {short: f"{folder}/Static/{short}" for short, _ in imp.statics(name)}
        for prop, candidates in cfg.get("statics", {}).items():
            piece = next((load(statics[short], note=False) for short in candidates if short in statics), None)
            if piece:
                rules.set_editor_property(prop, piece)
            else:
                report["missing"].append(f"{name}: {candidates} (statics of T-334) for {prop}")

        # sounds, flash, blast, tracers (the assets of the existing weapons)
        if not cfg.get("copy_from"):
            fire = find(cfg.get("fire_sound", []))
            if fire:
                rules.set_editor_property("fire_sound", fire)
            else:
                report["issues"].append(f"{name}: no shot sound among {cfg.get('fire_sound')}")
            rules.set_editor_property("event_sounds", sound_cues({**SHARED_EVENTS, **cfg.get("events", {})}))
            if handling:
                rules.set_editor_property("event_sound_attenuation", handling)
            for prop, candidates in cfg.get("rule_sounds", {}).items():
                sound = find(candidates)
                if sound:
                    set_props(rules, {prop: sound}, f"DA_{name}Rules")
            if sphere and flash:
                rules.set_editor_property("flash_mesh", sphere)
                rules.set_editor_property("flash_material", flash)
            blast = load(cfg.get("blast_from"), note=False)
            if blast:
                rules.set_editor_property("muzzle_blast_systems", blast.get_editor_property("muzzle_blast_systems"))
                rules.set_editor_property("impact_system", blast.get_editor_property("impact_system"))
            reference = load(cfg.get("tracer_from"), note=False)
            if reference:
                look = reference.get_editor_property("tracer")
                set_props(look, cfg.get("tracer", {}), f"DA_{name}Rules.tracer")
                rules.set_editor_property("tracer", look)
        else:
            # the musket's sounds (copied above) and the point: the cutlass's swing, hit and kill
            cues = list(rules.get_editor_property("event_sounds"))
            known = {str(cue.get_editor_property("event")) for cue in cues}
            if cutlass:
                for event_name, prop, volume in (("StabStart", "swing_sound", 0.9), ("ChargeStart", "finisher_swing_sound", 1.0)):
                    try:
                        sound = cutlass.get_editor_property(prop)
                    except Exception:  # noqa: BLE001
                        sound = None
                    if sound and event_name not in known:
                        cue = ue.FirearmEventSound()
                        cue.set_editor_property("event", event_name)
                        cue.set_editor_property("sound", sound)
                        cue.set_editor_property("volume", volume)
                        cues.append(cue)
                for prop, source in (("thrust_hit_sound", "hit_sound"), ("thrust_kill_sound", "kill_sound")):
                    try:
                        set_props(rules, {prop: cutlass.get_editor_property(source)}, f"DA_{name}Rules")
                    except Exception as error:  # noqa: BLE001
                        report["issues"].append(f"{name}.{prop}: {error}")
            else:
                report["missing"].append("DA_CutlassRules (the sounds of the bayonet's point)")
            rules.set_editor_property("event_sounds", cues)
        save(rules)

        # read back from the saved asset: what the weapon will really use
        back = eal.load_asset(f"{folder}/DA_{name}Rules")
        readback = {}
        for prop in ("weapon_mesh", "first_person_arms_mesh", "first_person_align_bone", "muzzle_socket", "fire_sound", "ammo_kind", "allow_aiming",
                     "aim_sight_socket", "aim_field_of_view"):
            try:
                value = back.get_editor_property(prop)
                readback[prop] = path_of(value) if hasattr(value, "get_path_name") else str(value)
            except Exception as error:  # noqa: BLE001
                readback[prop] = f"not on this build ({str(error)[:80]})"
        readback["clips"] = {clip.get_editor_property("gesture").name: [round(clip.get_editor_property("duration"), 3), clip.get_editor_property("loop"),
                                                                         len(clip.get_editor_property("events")),
                                                                         bool(clip.get_editor_property("weapon_first_person"))]
                             for clip in back.get_editor_property("clips")}
        entry["readback"] = readback
        report.setdefault("details", {})[name] = entry
        return rules, mesh is not None

    def weapon_step(name, cfg, rules):
        folder = imp.dest(name)
        pack_folder, _, _ = pack_of(name)
        bp = blueprint(f"{folder}/BP_{name}", getattr(ue, cfg["cls"]))
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = ue.get_default_object(bp.generated_class())
        cdo.set_editor_property("rules", rules)
        if musket_cdo:
            cdo.set_editor_property("mapping_context", musket_cdo.get_editor_property("musket_mapping_context"))
            cdo.set_editor_property("mapping_priority", musket_cdo.get_editor_property("musket_mapping_priority"))
            for prop in ("fire_action", "aim_action", "reload_action"):
                cdo.set_editor_property(prop, musket_cdo.get_editor_property(prop))
        if inspect:
            cdo.set_editor_property("inspect_action", inspect)
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
        item = data_asset(f"{folder}/DA_Item_{name}", ue.InventoryItemDefinition)
        set_props(item, {"display_name": ue.Text(cfg["display"]), "short_name": ue.Text(cfg["short"]), "item_class": bp.generated_class(),
                         "pickup_mass_kg": float(cfg["mass"])}, f"DA_Item_{name}")
        pickup = next((short for short, is_pickup in imp.statics(name) if is_pickup), None)
        ground = load(f"{folder}/Static/{pickup}") if pickup else None
        if ground:
            item.set_editor_property("pickup_mesh", ground)
        picture = icon(name, pack_folder)
        if picture:
            item.set_editor_property("icon", picture)
        else:
            report["missing"].append(f"T_Icon_{name}.png (the art task's picture; the cell shows {cfg['short']})")
        save(item)
        return bp, item

    def ammo_step(key, spec):
        folder = imp.dest(spec["weapon"])
        pack_folder, _, _ = pack_of(spec["weapon"])
        definition = data_asset(f"{AMMO_FOLDER}/DA_Ammo_{key}", ue.AmmoDefinition)
        lots = []
        for kind, count in spec["lots"]:
            lot = ue.AmmoLot()
            lot.set_editor_property("kind", getattr(ue.AmmoKind, kind))
            lot.set_editor_property("count", int(count))
            lots.append(lot)
        set_props(definition, {"display_name": ue.Text(spec["display"]), "lots": lots, "pickup_mass_kg": float(spec["mass"])}, f"DA_Ammo_{key}")
        mesh_name = next((short for short in spec["meshes"] if eal.does_asset_exist(f"{folder}/Static/{short}")), None)
        if mesh_name:
            definition.set_editor_property("pickup_mesh", load(f"{folder}/Static/{mesh_name}"))
            picture = icon(mesh_name, pack_folder)
            if picture:
                definition.set_editor_property("icon", picture)
        else:
            report["missing"].append(f"DA_Ammo_{key}: {spec['meshes']} (a placeholder box lies on the ground)")
        save(definition)
        bp = blueprint(f"{AMMO_FOLDER}/BP_Ammo_{key}", ue.AmmoPickup)
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        ue.get_default_object(bp.generated_class()).set_editor_property("definition", definition)
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)
        return definition, bp

    # ------------------------------------------------------------------ 3. the merchant
    def shop_step(items, ammo):
        catalog = load(SHOP)
        if not catalog:
            return
        offers = list(catalog.get_editor_property("offers"))
        sold = {(offer.get_editor_property("item") or offer.get_editor_property("ammo")).get_name() for offer in offers
                if offer.get_editor_property("item") or offer.get_editor_property("ammo")}
        added = []
        for kind, asset, price, line in [("item", *row) for row in items] + [("ammo", *row) for row in ammo]:
            if asset.get_name() in sold:
                continue
            offer = ue.ShopOffer()
            offer.set_editor_property(kind, asset)
            offer.set_editor_property("price", int(price))
            offer.set_editor_property("description", ue.Text(line))
            offers.append(offer)
            added.append([asset.get_name(), price])
        if added:
            catalog.set_editor_property("offers", offers)
            save(catalog)
        report["shop_added"] = added

    # ------------------------------------------------------------------ 4. the test map
    def map_step(pickups, boxes):
        step = {"spawned": [], "kept": []}
        report["map"] = step
        levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        dirty = [package.get_name() for package in ue.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
        previous_map = world.get_path_name().split(".")[0] if world else None
        if not world or "L_Test_Flat" not in world.get_path_name():
            if dirty:
                step["skipped"] = f"another map is open with unsaved changes ({dirty}): not switched"
                return
            if not levels.load_level(MAP):
                step["skipped"] = f"cannot open {MAP}"
                return
        was_dirty = any("L_Test_Flat" in package for package in dirty)
        actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
        everything = actors.get_all_level_actors()
        existing = {actor.get_actor_label(): actor for actor in everything}
        mine = set(f"Pickup_{name}" for name, _ in pickups) | {label for label, _ in boxes}

        # the row of the weapon pickups already there: the most crowded spot of them, its axis and its spacing
        row = []
        for actor in everything:
            if not isinstance(actor, ue.InventoryPickup) or actor.get_actor_label() in mine:
                continue
            try:
                definition = actor.get_editor_property("definition")
            except Exception:  # noqa: BLE001
                definition = None
            if definition and definition.get_path_name().startswith(W + "/"):
                row.append(actor.get_actor_location())
        if row:
            def near(a, b):
                return (a.x - b.x) ** 2 + (a.y - b.y) ** 2 < 1500.0 ** 2
            centre = max(row, key=lambda p: sum(1 for q in row if near(p, q)))
            row = [p for p in row if near(centre, p)]
            along_y = (max(p.y for p in row) - min(p.y for p in row)) >= (max(p.x for p in row) - min(p.x for p in row))
            coords = sorted(p.y if along_y else p.x for p in row)
            gaps = sorted(b - a for a, b in zip(coords, coords[1:]) if 40.0 <= b - a <= 400.0)
            spacing = gaps[len(gaps) // 2] if gaps else 120.0
            other = sorted(p.x if along_y else p.y for p in row)[len(row) // 2]
            z = max(p.z for p in row) + 20.0

            def spot(index, line):
                along = coords[-1] + spacing * (index + 1)
                across = other - 150.0 * line
                return ue.Vector(across, along, z) if along_y else ue.Vector(along, across, z)
            step["row"] = {"pickups": len(row), "axis": "y" if along_y else "x", "spacing": spacing, "end": coords[-1], "other": other, "z": z}
        else:
            # no weapon there yet: the grid of scripts/editor/weapons_test_map_ue.py (T-249) in front of the PlayerStart
            start = next((actor for actor in everything if isinstance(actor, ue.PlayerStart)), None)
            origin = start.get_actor_location() if start else ue.Vector(0.0, 0.0, 100.0)
            forward = start.get_actor_forward_vector() if start else ue.Vector(1.0, 0.0, 0.0)
            right = start.get_actor_right_vector() if start else ue.Vector(0.0, 1.0, 0.0)

            def spot(index, line):
                return origin + forward * (250.0 + line * 150.0) + right * ((index + 6) * 120.0) + ue.Vector(0.0, 0.0, 40.0)
            step["row"] = "no weapon pickup found: the T-249 grid in front of the PlayerStart"

        def place(label, cls, location, configure):
            actor = existing.get(label)
            if actor:
                step["kept"].append(label)
            else:
                actor = actors.spawn_actor_from_class(cls, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
                if not actor:
                    report["issues"].append(f"{label} not spawned")
                    return
                actor.set_actor_label(label)
                step["spawned"].append([label, [round(location.x), round(location.y), round(location.z)]])
            actor.set_folder_path(FOLDER_LABEL)
            actor.set_editor_property("tags", [TAG])
            configure(actor)

        for index, (name, item) in enumerate(pickups):
            place(f"Pickup_{name}", ue.InventoryPickup, spot(index, 0), lambda actor, item=item: actor.set_editor_property("definition", item))
        for index, (label, cls) in enumerate(boxes):
            place(label, cls, spot(index, 1), lambda actor: None)
        if was_dirty:
            step["saved"] = False
            step["note"] = "L_Test_Flat had unsaved changes: the actors are placed, the map is left for its owner to save"
        else:
            step["saved"] = bool(levels.save_current_level())
        if previous_map and previous_map.startswith("/Game/") and "L_Test_Flat" not in previous_map and not dirty:
            levels.load_level(previous_map)
            step["reopened"] = previous_map

    # ------------------------------------------------------------------ the run
    shop_items, pickups = [], []
    for name in names:
        cfg = WEAPONS[name]
        rules, has_model = rules_step(name, cfg)
        bp, item = weapon_step(name, cfg, rules)
        report["weapons"][name] = {"rules": path_of(rules), "blueprint": path_of(bp), "item": path_of(item), "has_model": has_model}
        if has_model:
            shop_items.append((item, cfg["price"], cfg["line"]))
            pickups.append((name, item))
    shop_ammo, boxes = [], []
    for key, spec in AMMO.items():
        if spec["weapon"] not in names:
            continue
        definition, bp = ammo_step(key, spec)
        report.setdefault("ammo", {})[key] = {"definition": path_of(definition), "blueprint": path_of(bp),
                                              "mesh": path_of(definition.get_editor_property("pickup_mesh"))}
        if report["weapons"].get(spec["weapon"], {}).get("has_model"):
            shop_ammo.append((definition, spec["price"], spec["line"]))
            boxes += [(f"Ammo_{key}_{count + 1}", bp.generated_class()) for count in range(spec["on_map"])]
    if os.environ.get("MONKE_WEAPONS_NO_SHOP"):
        report["shop_added"] = "skipped (MONKE_WEAPONS_NO_SHOP)"
    else:
        shop_step(shop_items, shop_ammo)
    if pickups:
        map_step(pickups, boxes)
    else:
        report["map"] = "no weapon with its model: nothing placed on L_Test_Flat"
    report["complete"] = len(pickups) == len(names)
    write()
    ue.log(f"T335_NEWWEAPONS_SUCCESS weapons={names} missing={len(report['missing'])} issues={len(report['issues'])} -> {REPORT}")


def main():
    try:
        run()
    except NotReady as error:
        # nothing was touched: a plain line (no words of the mesh importer, the lead's session keeps taking actions)
        raise RuntimeError(f"newweapons not ready: {error}") from None
    except IngestFailed as error:
        # a failed FBX import: the lead's session refuses further imports until the editor is restarted, on purpose
        raise RuntimeError(f"newweapons: FBX import failed: {error}") from None
    except Exception:  # noqa: BLE001
        detail = traceback.format_exc()
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "new-weapons-error.txt").write_text(detail, encoding="utf-8")
        # the details in new-weapons-error.txt; the line kept away from the words of the mesh importer (see mgsetup)
        reason = str(sys.exc_info()[1])[:300].replace("mport", "ngest").replace("fbx", "f.b.x").replace("FBX", "F.B.X")
        raise RuntimeError(f"newweapons failed (see Saved/T335/new-weapons-error.txt): {reason}") from None


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(check())
    main()
