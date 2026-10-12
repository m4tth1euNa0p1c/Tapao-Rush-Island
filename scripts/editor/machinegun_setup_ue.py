"""T-273: the crank machine gun in Unreal (action "mgsetup" of the lead's editor session, scripts/editor/lead_session_t186.py;
editor open, lock held, no game running). Replays are idempotent; every step skips what is already in place.

1. Sounds: art/audio/weapons/machinegun/*.wav (scripts/audio/machinegun_prepare.py, manifest.json there) as SoundWaves in
   /Game/MonkeIsland/Weapons/MachineGun/Audio (a WAV whose SHA-256 is in the asset's T273.SourceSha256 tag is kept),
   styled like the weapons of T-259 (SC_Effects; ATT_Firearm_Shot / ATT_Firearm_Handling, SCON_Firearm_Shot /
   SCON_Firearm_Handling of /Game/MonkeIsland/Weapons/Audio) plus ATT_MG_Mechanism, ATT_MG_Brass and SCON_MG_Brass;
   SoundCues with a Random node over the variants (SC_MG_Round at 0.6: ten rounds a second overlap).
2. Effects in /Game/MonkeIsland/VFX/Weapons, copies of the engine's FountainLightweight like the shotgun's
   (scripts/vfx/build_firearm_vfx_ue.py): NS_MG_MuzzleFlame and NS_MG_MuzzleSmoke (light: one per round),
   NS_MG_Steam (wisps of a hot jacket) and NS_MG_SteamVent (the overheat); the shotgun's when that fails.
3. Data (needs the T-273 C++: AMachineGunWeapon, UMachineGunRules; without it the step stops with a clear message):
   DA_MachineGunRules (the pack of T-272 when imported: weapon mesh, FP arms, the four clips of every gesture with the
   lengths and events of the pack's manifest, its FP camera and framing; the sounds, effects, tracers every 3 rounds,
   the case, link and box meshes), BP_MachineGun (rules, the musket's keys), DA_Item_MachineGun, DA_Ammo_MGBox and
   BP_Ammo_MGBox (100 rounds), the icons when their renders exist (scripts/ui/render_weapon_icons.py), the merchant's
   lines (2 500 and 250 crowns) once the gun has its model (never an invisible gun for sale).
4. Test map L_Test_Flat, once the gun has its model (only when it is the open map, or no map has unsaved changes):
   Pickup_MachineGun, Ammo_MGBox_1/2 next to the weapons of T-249 (folder WeaponsTest, tag T273_MachineGun); saved
   unless it had unsaved changes before.
Report: monke_island/Saved/T273/mgsetup.json. The editor freezes while it works: each run stops after about 25 s of
work and says so (run it again: the next run goes on from there).

Dry run outside Unreal (what the pack and the sounds give): python scripts/editor/machinegun_setup_ue.py --check
"""
import datetime as dt
import glob
import hashlib
import json
import math
import os
import re
import sys
import time
import traceback
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T273"
REPORT = OUT / "mgsetup.json"
SOUNDS = ROOT / "art/audio/weapons/machinegun"
PACKS = ROOT / "build/packs"
ICON_PNG = ROOT / "art/ui/icons/weapons"
W = "/Game/MonkeIsland/Weapons"
MG = f"{W}/MachineGun"
AUDIO = f"{MG}/Audio"
SHARED_AUDIO = f"{W}/Audio"
AMMO = f"{W}/Ammo"
MUSKET = f"{W}/Musket"
VFX = "/Game/MonkeIsland/VFX/Weapons"
TRACERS = "/Game/MonkeIsland/VFX/Tracers/M_Tracer"
ICONS = "/Game/MonkeIsland/UI/Icons/Weapons"
EFFECTS_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
SHOP = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
MAP = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
INSPECT_ACTION = f"{W}/Input/IA_FirearmInspect"
NIAGARA_TEMPLATE = "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight"
TAG = "T273.SourceSha256"
BUDGET_S = 25.0
CLIPS = ("Equip", "Holster", "Idle", "Walk", "Sprint", "Inspect", "FireStart", "FireLoop", "FireEnd", "Reload", "Overheat", "DryFire", "Discard")
LOOPS = {"Idle", "Walk", "Sprint", "FireLoop"}
STATICS = {"pickup": "SM_Bean_MachineGun", "casing": "SM_MG_Casing", "link": "SM_MG_BeltLink", "box": "SM_Ammo_MGBox"}
# sound groups -> (attenuation, concurrency, priority, volume); loops have no concurrency (never cut by the brass)
MIXING = {
    "shot": ("ATT_Firearm_Shot", "SCON_Firearm_Shot", 2.0),
    "handling": ("ATT_Firearm_Handling", "SCON_Firearm_Handling", 1.0),
    "brass": ("ATT_MG_Brass", "SCON_MG_Brass", 0.8),
    "mechanism": ("ATT_MG_Mechanism", None, 1.5),
}
GROUP_KIND = {"SW_MG_MechanismLoop": "mechanism", "SW_MG_SteamLoop": "mechanism"}
CUE_VOLUME = {"SW_MG_Round": 0.6}
# event of the clips -> (sound group, volume)
EVENT_SOUNDS = {
    "DrawWeapon": ("SW_MG_Draw", 1.0), "Ready": ("SW_Firearm_Ready", 0.8), "StowWeapon": ("SW_Firearm_Stow", 1.0),
    "CrankTick": ("SW_MG_CrankTick", 1.0), "CoverOpen": ("SW_MG_CoverOpen", 1.0), "BeltOut": ("SW_MG_BeltOut", 1.0),
    "BoxOut": ("SW_MG_BoxOut", 1.0), "BoxIn": ("SW_MG_BoxIn", 1.0), "BeltIn": ("SW_MG_BeltIn", 1.0),
    "CoverClose": ("SW_MG_CoverClose", 1.0), "Charge": ("SW_MG_Charge", 1.0), "SteamVent": ("SW_MG_SteamVent", 1.0),
    "DryClick": ("SW_MG_CrankTick", 0.8), "Empty": ("SW_Firearm_DiscardEmpty", 1.0), "Release": ("SW_MG_DiscardRelease", 1.0),
}
# rules fields -> sound group
RULE_SOUNDS = {"fire_sound": "SW_MG_Round", "mechanism_loop_sound": "SW_MG_MechanismLoop", "burst_start_sound": "SW_MG_BurstStart",
               "burst_tail_sound": "SW_MG_BurstTail", "overheat_sound": "SW_MG_Overheat", "steam_loop_sound": "SW_MG_SteamLoop",
               "casing_land_sound": "SW_MG_CasingLand"}
# Niagara copies (burst along the local +X; the gun turns the steam up): duration, burst, lifetime, size, cone, speed,
# drag, gravity, bounds, material, colour (the smoke material's colour and density, the flame's colour x Glow)
SYSTEMS = {
    "NS_MG_MuzzleFlame": {"duration": 0.1, "burst": (10, 16), "lifetime": (0.03, 0.07), "size": (4.0, 13.0), "cone": 18.0,
                          "speed": (500.0, 1400.0), "drag": 8.0, "gravity": 0.0, "bounds": 250.0, "material": "M_Firearm_Flame",
                          "color": {"r": 1.0, "g": 0.5, "b": 0.14, "a": 1.0}},
    "NS_MG_MuzzleSmoke": {"duration": 1.4, "burst": (2, 4), "lifetime": (0.5, 1.2), "size": (7.0, 20.0), "cone": 25.0,
                          "speed": (80.0, 320.0), "drag": 3.0, "gravity": 30.0, "bounds": 400.0, "material": "M_Firearm_Smoke",
                          "curl": True, "color": {"r": 0.7, "g": 0.68, "b": 0.64, "a": 0.32}},
    "NS_MG_Steam": {"duration": 0.6, "burst": (3, 6), "lifetime": (0.8, 1.8), "size": (5.0, 18.0), "cone": 22.0,
                    "speed": (50.0, 160.0), "drag": 2.5, "gravity": 60.0, "bounds": 300.0, "material": "M_Firearm_Smoke",
                    "curl": True, "color": {"r": 0.93, "g": 0.95, "b": 0.96, "a": 0.42}},
    "NS_MG_SteamVent": {"duration": 3.0, "burst": (34, 48), "lifetime": (1.2, 3.0), "size": (10.0, 46.0), "cone": 35.0,
                        "speed": (150.0, 600.0), "drag": 2.5, "gravity": 70.0, "bounds": 700.0, "material": "M_Firearm_Smoke",
                        "curl": True, "color": {"r": 0.95, "g": 0.96, "b": 0.97, "a": 0.6}},
}
FALLBACK_SYSTEMS = {"NS_MG_MuzzleFlame": "NS_Shotgun_MuzzleFlame", "NS_MG_MuzzleSmoke": "NS_Shotgun_MuzzleSmoke",
                    "NS_MG_Steam": "NS_Shotgun_MuzzleSmoke", "NS_MG_SteamVent": "NS_Shotgun_MuzzleSmoke"}
# tracer look of the gun (speed cm/s, length cm, width cm, colour, brightness, every nth, round size cm); the lead's
# scripts/editor/tracer_setup_ue.py does the other firearms the same way
TRACER_LOOK = (26000, 180, 2.8, (1.0, 0.5, 0.15), 9, 3, 2.0)
TRACER_ROUND = f"{MUSKET}/Static/SM_Ammo_LeadBall"
SHOP_LINES = {"item": (2500, "Mitrailleuse à manivelle 1918 : bande de 100 coups, 450 à 600 coups/min, lourde"),
              "ammo": (250, "Caisse de bande : 100 cartouches de mitrailleuse")}


class StepError(Exception):
    """A step that cannot go on (its message is the report's)"""


# ---------------------------------------------------------------------------------------------- the pack (no Unreal)

def pack_manifest():
    """(pack folder, manifest) of the machine gun pack of T-272 (build/packs/*machinegun*, a json with scenes and a
    FireLoop), or (None, None)"""
    for folder in sorted(glob.glob(str(PACKS / "*machinegun*")) + glob.glob(str(PACKS / "*machine-gun*"))):
        for path in sorted(glob.glob(os.path.join(folder, "*.json"))):
            try:
                data = json.loads(Path(path).read_text(encoding="utf-8"))
            except (OSError, ValueError):
                continue
            if isinstance(data, dict) and any(s.get("clip") == "FireLoop" for s in data.get("scenes") or []):
                return folder, data
    return None, None


def clip_timing(manifest):
    """clip -> (seconds, [(seconds, event)], loop) from the TP scenes of the manifest"""
    out = {}
    for scene in (manifest or {}).get("scenes") or []:
        if scene.get("view") != "TP" or scene.get("clip") not in CLIPS:
            continue
        fps = float(scene.get("fps") or 30)
        start = scene.get("start", 0)
        frames = max(scene.get("end", start + 1) - start, 1)
        events = [((e["frame"] - start) / fps, e["name"]) for e in scene.get("events") or []]
        out[scene["clip"]] = (frames / fps, events, bool(scene.get("loop", scene["clip"] in LOOPS)))
    return out


def clip_assets(manifest):
    """asset name -> (clip, view, role) of the animation rows of the manifest (role: bean or weapon)"""
    out = {}
    for row in (manifest or {}).get("assets") or []:
        if row.get("type") == "animation" and row.get("clip"):
            role = "bean" if str(row.get("skeleton_group", "")).lower() == "bean" else "weapon"
            out[Path(row["path"]).stem] = (row["clip"], row.get("view"), role)
    return out


def fp_view(manifest):
    """The FP camera of the pack's scenes in the bean's mesh space (same conversion as setup_pirate_weapons.fp_view)"""
    scenes = [s for s in (manifest or {}).get("scenes") or [] if s.get("view") == "FP"]
    if not scenes:
        return None
    cam = scenes[0].get("camera_data") or {}
    if cam:
        loc, rot, lens = cam["location_m"], cam["rotation_euler_deg"], cam["lens_mm"]
    elif scenes[0].get("camera_transform"):
        ct = scenes[0]["camera_transform"]
        loc, rot, lens = ct["location"], ct["rotation_degrees"], ct["lens_mm"]
    else:
        return None
    rx, ry, rz = (math.radians(a) for a in rot)
    cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
    mx = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]
    my = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
    mz = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]

    def mul(a, b):
        return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]

    m = mul(mz, mul(my, mx))
    forward = (-m[0][2], m[1][2], -m[2][2])
    up = (m[0][1], -m[1][1], m[2][1])
    pitch = math.degrees(math.asin(max(-1.0, min(1.0, forward[2]))))
    yaw = math.degrees(math.atan2(forward[1], forward[0]))
    flat_right = (-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0.0)
    roll = math.degrees(math.asin(max(-1.0, min(1.0, -sum(u * r for u, r in zip(up, flat_right))))))
    return {"location_cm": [loc[0] * 100.0, -loc[1] * 100.0, loc[2] * 100.0], "rotation_pyr": [pitch, yaw, roll],
            "fov": 2.0 * math.degrees(math.atan(18.0 / lens))}


def fp_framing(folder, manifest):
    framing = (manifest or {}).get("fp_framing")
    if not framing and folder and os.path.exists(os.path.join(folder, "fp-framing.json")):
        framing = json.loads(Path(folder, "fp-framing.json").read_text(encoding="utf-8"))
    return framing if isinstance(framing, dict) else None


def sound_manifest():
    path = SOUNDS / "manifest.json"
    return json.loads(path.read_text(encoding="utf-8"))["sounds"] if path.exists() else []


def check():
    folder, manifest = pack_manifest()
    plan = {"pack": folder, "clips": {k: [round(v[0], 3), v[1], v[2]] for k, v in clip_timing(manifest).items()},
            "animations": clip_assets(manifest), "fp_view": fp_view(manifest), "fp_framing": fp_framing(folder, manifest),
            "sounds": sorted({e["group"] for e in sound_manifest()})}
    print(json.dumps(plan, indent=1, ensure_ascii=False))
    return 0


# ---------------------------------------------------------------------------------------------------------- Unreal

def run():
    import unreal as ue

    eal = ue.EditorAssetLibrary
    tools = ue.AssetToolsHelpers.get_asset_tools()
    started = time.monotonic()
    report = {"task": "T-273", "started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "steps": {}, "issues": [],
              "missing": [], "complete": False}

    def spent():
        return time.monotonic() - started

    def write():
        report["editor_seconds"] = round(spent(), 1)
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.mkdir(parents=True, exist_ok=True)
        REPORT.write_text(json.dumps(report, indent=1, ensure_ascii=False, default=str), encoding="utf-8")

    def load(path, note=True):
        asset = eal.load_asset(path) if eal.does_asset_exist(path) else None
        if asset is None and note:
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
        factory = ue.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        return tools.create_asset(path.rsplit("/", 1)[1], path.rsplit("/", 1)[0], ue.Blueprint, factory)

    def names_under(folder):
        """short name -> object path of the assets under a folder (recursive)"""
        out = {}
        if eal.does_directory_exist(folder):
            for path in eal.list_assets(folder, recursive=True, include_folder=False):
                package = path.split(".")[0]
                out.setdefault(package.rsplit("/", 1)[1], package)
        return out

    # ------------------------------------------------------------------ 1. sounds
    def ingest_wave(entry):
        name = entry["asset"]
        wav = SOUNDS / f"{name}.wav"
        digest = hashlib.sha256(wav.read_bytes()).hexdigest()
        existing = load(f"{AUDIO}/{name}", note=False)
        if existing and eal.get_metadata_tag(existing, TAG) == digest:
            return existing, False
        task = ue.AssetImportTask()
        task.filename = str(wav)
        task.destination_path = AUDIO
        task.destination_name = name
        task.replace_existing = True
        task.automated = True
        task.save = False
        tools.import_asset_tasks([task])
        sound = load(f"{AUDIO}/{name}", note=False)
        if not sound:
            raise StepError(f"{name}: the WAV did not come in")
        eal.set_metadata_tag(sound, TAG, digest)
        return sound, True

    def mixing_asset(name, cls, values, settings_prop):
        folder = SHARED_AUDIO if eal.does_asset_exist(f"{SHARED_AUDIO}/{name}") else AUDIO
        asset = load(f"{folder}/{name}", note=False)
        if asset and folder == SHARED_AUDIO:
            return asset
        asset = asset or tools.create_asset(name, AUDIO, cls, None)
        settings = asset.get_editor_property(settings_prop)
        set_props(settings, values, name)
        asset.set_editor_property(settings_prop, settings)
        save(asset)
        return asset

    def style(sound, kind, mixing, loop):
        attenuation, concurrency, priority = mixing[kind]
        set_props(sound, {"sound_class_object": mixing["class"], "attenuation_settings": attenuation, "priority": priority}, sound.get_name())
        try:
            sound.set_editor_property("concurrency_set", [concurrency] if concurrency else [])
        except Exception:  # noqa: BLE001 - a TSet on some versions
            set_props(sound, {"concurrency_set": {concurrency} if concurrency else set()}, sound.get_name())
        if isinstance(sound, ue.SoundWave):
            set_props(sound, {"looping": bool(loop)}, sound.get_name())

    def sounds_step():
        entries = sound_manifest()
        step = {"waves_in": [], "kept": 0, "remaining": [], "cues": {}, "sounds": {}}
        report["steps"]["sounds"] = step
        if not entries:
            report["issues"].append("no art/audio/weapons/machinegun/manifest.json: run scripts/audio/machinegun_prepare.py")
            return {}
        ensure_folder(AUDIO)
        waves = {}
        for entry in entries:
            if spent() > BUDGET_S * 0.6:
                step["remaining"].append(entry["asset"])
                continue
            sound, fresh = ingest_wave(entry)
            waves[entry["asset"]] = (sound, entry)
            if fresh:
                step["waves_in"].append(entry["asset"])
                save(sound)
            else:
                step["kept"] += 1
        if step["remaining"]:
            return None
        mixing = {"class": load(EFFECTS_CLASS)}
        mixing_assets = {
            "ATT_MG_Brass": (ue.SoundAttenuation, {"attenuate": True, "spatialize": True, "attenuation_shape_extents": ue.Vector(100.0, 0.0, 0.0),
                                                    "falloff_distance": 1100.0, "distance_algorithm": ue.AttenuationDistanceModel.LOGARITHMIC,
                                                    "non_spatialized_radius_start": 150.0, "non_spatialized_radius_end": 50.0}, "attenuation"),
            "ATT_MG_Mechanism": (ue.SoundAttenuation, {"attenuate": True, "spatialize": True, "attenuation_shape_extents": ue.Vector(300.0, 0.0, 0.0),
                                                        "falloff_distance": 5700.0, "distance_algorithm": ue.AttenuationDistanceModel.LOGARITHMIC,
                                                        "non_spatialized_radius_start": 150.0, "non_spatialized_radius_end": 50.0}, "attenuation"),
            "SCON_MG_Brass": (ue.SoundConcurrency, {"max_count": 6, "limit_to_owner": False,
                                                     "resolution_rule": ue.MaxConcurrentResolutionRule.STOP_FARTHEST_THEN_OLDEST}, "concurrency"),
        }
        for kind, (attenuation, concurrency, priority) in MIXING.items():
            att = load(f"{SHARED_AUDIO}/{attenuation}", note=False) or (mixing_asset(attenuation, *mixing_assets[attenuation]) if attenuation in mixing_assets else None)
            con = None
            if concurrency:
                con = load(f"{SHARED_AUDIO}/{concurrency}", note=False) or (mixing_asset(concurrency, *mixing_assets[concurrency]) if concurrency in mixing_assets else None)
            if not att:
                report["missing"].append(f"{SHARED_AUDIO}/{attenuation}")
            mixing[kind] = (att, con, priority)
        restyle = bool(os.environ.get("MONKE_MG_RESTYLE"))
        groups = {}
        for name, (sound, entry) in waves.items():
            kind = GROUP_KIND.get(entry["group"], entry.get("kind", "handling"))
            # styled when it comes in (MONKE_MG_RESTYLE=1 styles them all again): a replay saves nothing it need not
            if name in step["waves_in"] or restyle:
                style(sound, kind, mixing, entry.get("loop"))
                save(sound)
            groups.setdefault(entry["group"], []).append((entry["variant"], sound, kind))
        sounds = {}
        for group, members in groups.items():
            members.sort(key=lambda m: m[0])
            kind = members[0][2]
            if len(members) == 1 and group not in CUE_VOLUME:
                sounds[group] = members[0][1]
                continue
            cue_name = "SC_" + group[len("SW_"):]
            cue = load(f"{AUDIO}/{cue_name}", note=False)
            created = cue is None
            if created:
                factory = ue.SoundCueFactoryNew()
                try:
                    factory.set_editor_property("initial_sound_waves", [m[1] for m in members])
                except Exception as error:  # noqa: BLE001
                    report["issues"].append(f"{cue_name}: one wave only ({error})")
                    factory.set_editor_property("initial_sound_wave", members[0][1])
                cue = tools.create_asset(cue_name, AUDIO, ue.SoundCue, factory)
            if created or restyle:
                style(cue, kind, mixing, False)
                set_props(cue, {"volume_multiplier": CUE_VOLUME.get(group, 1.0)}, cue_name)
                save(cue)
            sounds[group] = cue
            step["cues"][cue_name] = {"variants": len(members), "created_now": created}
        # the shared sounds of the firearms (draw, ready, stow, empty: T-259)
        for group in ("SW_Firearm_Ready", "SW_Firearm_Stow", "SW_Firearm_DiscardEmpty"):
            shared = load(f"{SHARED_AUDIO}/{group}", note=False)
            if shared:
                sounds.setdefault(group, shared)
        step["sounds"] = {g: path_of(s) for g, s in sounds.items()}
        return sounds

    # ------------------------------------------------------------------ 2. effects
    def distribution(current, low, high=None):
        value = dict(current)
        if isinstance(low, (list, tuple)):
            keys = "xyz"[:len(low)]
            value["min"] = dict(zip(keys, low))
            value["max"] = dict(zip(keys, high if high is not None else low))
            value["mode"] = "NonUniformConstant" if high is None else "NonUniformRange"
            value["channelConstantsAndRanges"] = list(low) + ([] if high is None else list(high))
        else:
            value["min"], value["max"] = low, low if high is None else high
            value["mode"] = "UniformConstant" if high is None else "UniformRange"
            value["channelConstantsAndRanges"] = [low] if high is None else [low, high]
        return value

    def tl_get(path, prop):
        obj = ue.load_object(None, path)
        if obj is None:
            raise StepError(f"no object {path}")
        out = ue.ToolsetLibrary.get_object_properties(obj, [prop])
        return (json.loads(out, strict=False) if isinstance(out, str) else out)[prop]

    def tl_put(path, **values):
        obj = ue.load_object(None, path)
        if obj is None or not ue.ToolsetLibrary.set_object_properties(obj, json.dumps(values)):
            raise StepError(f"{path}: properties refused ({', '.join(values)})")

    def configure(name, spec):
        """One burst along the local +X (scripts/vfx/build_firearm_vfx_ue.py configure, in the editor's own Python)"""
        emitter = f"{VFX}/{name}.{name}:Fountain_1"

        def module(n):
            return f"{emitter}.NiagaraStatelessModule_{n}_0"

        state = tl_get(emitter, "emitterState")
        state["loopBehavior"] = "Once"
        state["loopDuration"] = distribution(state["loopDuration"], spec["duration"])
        state["bLoopDelayEnabled"] = False
        state["inactiveResponse"] = "Complete"
        spawn = tl_get(emitter, "spawnInfos")[0]
        spawn["type"] = "Burst"
        spawn["spawnTime"] = 0.0
        spawn["amount"].update({"mode": "UniformRange", "min": spec["burst"][0], "max": spec["burst"][1]})
        bounds = spec["bounds"]
        tl_put(emitter, emitterState=state, spawnInfos=[spawn],
               fixedBounds={"min": {"x": -bounds, "y": -bounds, "z": -bounds}, "max": {"x": bounds, "y": bounds, "z": bounds}, "isValid": True})
        init = module("InitializeParticle")
        color = tl_get(init, "colorDistribution")
        color["values"] = [spec["color"], spec["color"]]
        color["mode"] = "NonUniformConstant"
        color["channelConstantsAndRanges"] = [spec["color"][c] for c in "rgba"]
        sizes = tl_get(init, "spriteSizeDistribution")
        sizes["min"], sizes["max"] = {"x": spec["size"][0], "y": spec["size"][0]}, {"x": spec["size"][1], "y": spec["size"][1]}
        sizes["mode"], sizes["channelConstantsAndRanges"] = "UniformRange", [spec["size"][0], spec["size"][1]]
        tl_put(init, lifetimeDistribution=distribution(tl_get(init, "lifetimeDistribution"), *spec["lifetime"]),
               colorDistribution=color, spriteSizeDistribution=sizes)
        tl_put(module("ShapeLocation"), bModuleEnabled=False)
        velocity = module("AddVelocity")
        tl_put(velocity, velocityType="InCone", coneRotationType="Direction", coneAngle=spec["cone"],
               coneDirection=distribution(tl_get(velocity, "coneDirection"), [1.0, 0.0, 0.0]),
               coneVelocityDistribution=distribution(tl_get(velocity, "coneVelocityDistribution"), *spec["speed"]))
        tl_put(module("Drag"), dragDistribution=distribution(tl_get(module("Drag"), "dragDistribution"), spec["drag"]))
        tl_put(module("GravityForce"), bModuleEnabled=True,
               gravityDistribution=distribution(tl_get(module("GravityForce"), "gravityDistribution"), [0.0, 0.0, spec["gravity"]]))
        tl_put(module("CurlNoiseForce"), bModuleEnabled=spec.get("curl", False))
        tl_put(f"{emitter}.Renderer", material={"refPath": f"{VFX}/{spec['material']}.{spec['material']}"})

    def effects_step():
        step = {"built": [], "kept": [], "fallback": {}}
        report["steps"]["effects"] = step
        systems = {}
        for name, spec in SYSTEMS.items():
            path = f"{VFX}/{name}"
            try:
                if eal.does_asset_exist(path):
                    step["kept"].append(name)
                else:
                    if not hasattr(ue, "ToolsetLibrary"):
                        raise StepError("no ToolsetLibrary in this editor")
                    if not eal.duplicate_asset(NIAGARA_TEMPLATE, path):
                        raise StepError(f"cannot copy {NIAGARA_TEMPLATE}")
                    configure(name, spec)
                    save(eal.load_asset(path))
                    step["built"].append(name)
                systems[name] = eal.load_asset(path)
            except Exception as error:  # noqa: BLE001
                fallback = load(f"{VFX}/{FALLBACK_SYSTEMS[name]}", note=False)
                systems[name] = fallback
                step["fallback"][name] = f"{FALLBACK_SYSTEMS[name]} ({str(error)[:200]})"
                if eal.does_asset_exist(path) and name not in step["kept"]:
                    # a half-made copy would be kept by the next run: removed
                    eal.delete_asset(path)
        return systems

    # ------------------------------------------------------------------ 3. data
    def missing_code():
        missing = [n for n in ("MachineGunWeapon", "MachineGunRules", "MachineGunFireCameraShake") if not hasattr(ue, n)]
        if not hasattr(ue.AmmoKind, "MACHINE_GUN_BELT"):
            missing.append("EAmmoKind::MachineGunBelt")
        if not hasattr(ue.FirearmGesture, "FIRE_LOOP"):
            missing.append("EFirearmGesture::FireLoop")
        return missing

    def camel_upper(name):
        return re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()

    def icon(name):
        """T_Icon_<name> of the interface: the asset, or the render of scripts/ui/render_weapon_icons.py brought in"""
        existing = load(f"{ICONS}/T_Icon_{name}", note=False)
        png = ICON_PNG / f"T_Icon_{name}.png"
        if existing or not png.exists():
            return existing
        task = ue.AssetImportTask()
        task.filename = str(png)
        task.destination_path = ICONS
        task.destination_name = f"T_Icon_{name}"
        task.replace_existing = True
        task.automated = True
        task.save = False
        tools.import_asset_tasks([task])
        texture = load(f"{ICONS}/T_Icon_{name}", note=False)
        if texture:
            set_props(texture, {"compression_settings": ue.TextureCompressionSettings.TC_EDITOR_ICON, "lod_group": ue.TextureGroup.TEXTUREGROUP_UI,
                                "mip_gen_settings": ue.TextureMipGenSettings.TMGS_NO_MIPMAPS, "never_stream": True, "srgb": True}, texture.get_name())
            save(texture)
        return texture

    def rules_step(sounds, systems):
        step = {"clips_set": 0, "clips": {}, "pack": None}
        report["steps"]["rules"] = step
        rules = data_asset(f"{MG}/DA_MachineGunRules", ue.MachineGunRules)
        folder, manifest = pack_manifest()
        step["pack"] = folder
        assets = names_under(MG)
        weapon_assets = names_under(W)

        # the pack of T-272: the gun (SK_Bean_MachineGun, the gun skeleton), the FP arms (SK_Bean_MachineGun_FP)
        mesh = load(assets["SK_Bean_MachineGun"], note=False) if "SK_Bean_MachineGun" in assets else None
        arms = load(assets["SK_Bean_MachineGun_FP"], note=False) if "SK_Bean_MachineGun_FP" in assets else None
        if mesh:
            rules.set_editor_property("weapon_mesh", mesh)
        else:
            report["missing"].append(f"{MG}/.../SK_Bean_MachineGun (the pack of T-272 is not in the project yet)")
        if arms:
            rules.set_editor_property("first_person_arms_mesh", arms)
        gun_skeleton = mesh.get_editor_property("skeleton") if mesh else None

        # the four animations of every clip: the manifest's rows when the pack is there, else the names
        rows = clip_assets(manifest)
        found = {}
        for short, package in assets.items():
            data = eal.find_asset_data(package)
            if not str(data.asset_class_path.asset_name).startswith("AnimSequence"):
                continue
            if short in rows:
                clip, view, role = rows[short]
            else:
                clip = next((c for c in sorted(CLIPS, key=len, reverse=True) if short.lower().endswith(c.lower())), None)
                view = "FP" if re.search(r"(^|_)FP(_|$)", short) else ("TP" if re.search(r"(^|_)TP(_|$)", short) else None)
                anim = load(package, note=False)
                role = "weapon" if anim and gun_skeleton and anim.get_editor_property("skeleton") == gun_skeleton else "bean"
            if clip in CLIPS and view in ("TP", "FP"):
                found.setdefault(clip, {})[f"{role}_{'third' if view == 'TP' else 'first'}_person"] = package
        timing = clip_timing(manifest)
        clips = list(rules.get_editor_property("clips"))
        present = {c.get_editor_property("gesture").name: c for c in clips}
        defaults = {c.get_editor_property("gesture").name: c for c in ue.get_default_object(ue.MachineGunRules.static_class()).get_editor_property("clips")}
        for clip_name in CLIPS:
            enum_value = getattr(ue.FirearmGesture, camel_upper(clip_name), None)
            if enum_value is None:
                continue
            clip = present.get(enum_value.name)
            if clip is None and (clip_name in found or clip_name in timing or enum_value.name in defaults):
                clip = ue.FirearmClip()
                clip.set_editor_property("gesture", enum_value)
                clips.append(clip)
            if clip is None:
                continue
            if clip_name not in timing and enum_value.name in defaults:
                # no manifest: the lengths and events of the C++ defaults (a DA saved by an older build keeps up)
                for prop in ("events", "duration", "loop"):
                    clip.set_editor_property(prop, defaults[enum_value.name].get_editor_property(prop))
            entry = {}
            for prop, package in found.get(clip_name, {}).items():
                anim = load(package, note=False)
                if anim:
                    clip.set_editor_property(prop, anim)
                    step["clips_set"] += 1
                    entry[prop] = package.rsplit("/", 1)[1]
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
                entry["seconds"] = round(seconds, 3)
                entry["events"] = [e for _, e in events]
            step["clips"][clip_name] = entry
        rules.set_editor_property("clips", clips)

        view = fp_view(manifest)
        if view:
            rules.set_editor_property("first_person_view_location", ue.Vector(*view["location_cm"]))
            pitch, yaw, roll = view["rotation_pyr"]
            rules.set_editor_property("first_person_view_rotation", ue.Rotator(roll=roll, pitch=pitch, yaw=yaw))
            rules.set_editor_property("first_person_field_of_view", 0.0)
            step["fp_view"] = view
        framing = fp_framing(folder, manifest)
        if framing and "hip_grip_location_cm" in framing:
            rules.set_editor_property("hip_grip_location", ue.Vector(*framing["hip_grip_location_cm"]))
            pitch, yaw, roll = (list(framing.get("hip_rotation_pyr_deg") or [0.0, 0.0, 0.0]) + [0.0, 0.0, 0.0])[:3]
            rules.set_editor_property("hip_rotation", ue.Rotator(roll=roll, pitch=pitch, yaw=yaw))
            step["hip_framing"] = {k: v for k, v in framing.items() if k.startswith("hip_")}

        # sounds
        for prop, group in RULE_SOUNDS.items():
            if group in sounds:
                set_props(rules, {prop: sounds[group]}, "DA_MachineGunRules")
        cues = []
        for event_name, (group, volume) in EVENT_SOUNDS.items():
            if group in sounds:
                cue = ue.FirearmEventSound()
                cue.set_editor_property("event", event_name)
                cue.set_editor_property("sound", sounds[group])
                cue.set_editor_property("volume", volume)
                cues.append(cue)
        if cues:
            rules.set_editor_property("event_sounds", cues)
        handling = load(f"{SHARED_AUDIO}/ATT_Firearm_Handling", note=False)
        if handling:
            rules.set_editor_property("event_sound_attenuation", handling)

        # effects: one light blast per round, the steam, the impact puffs, the flame of the musket
        blast = [systems.get("NS_MG_MuzzleFlame"), systems.get("NS_MG_MuzzleSmoke")]
        rules.set_editor_property("muzzle_blast_systems", [s for s in blast if s])
        set_props(rules, {"steam_system": systems.get("NS_MG_Steam"), "steam_vent_system": systems.get("NS_MG_SteamVent"),
                          "impact_system": load(f"{VFX}/NS_Firearm_Impact", note=False)}, "DA_MachineGunRules")
        sphere = load("/Engine/BasicShapes/Sphere", note=False)
        flash = load(f"{MUSKET}/FX/M_MusketFlash", note=False)
        if sphere and flash:
            rules.set_editor_property("flash_mesh", sphere)
            rules.set_editor_property("flash_material", flash)

        # tracers every 3 rounds, drawn like the other firearms' (T-276)
        try:
            look = rules.get_editor_property("tracer")
            speed, length, width, colour, power, nth, size = TRACER_LOOK
            values = {"enabled": True, "every_nth": nth, "speed_cmps": float(speed), "length_cm": float(length), "width_cm": float(width),
                      "color": ue.LinearColor(colour[0], colour[1], colour[2], 1.0), "intensity": float(power),
                      "streak_mesh": load("/Engine/BasicShapes/Cylinder", note=False), "streak_material": load(TRACERS, note=False),
                      "round_mesh": load(TRACER_ROUND, note=False)}
            set_props(look, values, "Tracer")
            try:
                look.set_editor_property("round_size_cm", float(size))
            except Exception:  # noqa: BLE001 - an older look scales the round instead
                pass
            rules.set_editor_property("tracer", look)
        except Exception as error:  # noqa: BLE001
            report["issues"].append(f"tracer look: {error}")

        # cases, links, the box let go during the reload (statics of the pack)
        for prop, key in (("casing_mesh", "casing"), ("link_mesh", "link"), ("dropped_box_mesh", "box")):
            short = STATICS[key]
            mesh_path = assets.get(short) or weapon_assets.get(short)
            static = load(mesh_path, note=False) if mesh_path else None
            if static:
                set_props(rules, {prop: static}, "DA_MachineGunRules")
            else:
                report["missing"].append(f"{short} (statics of T-272)")
        save(rules)

        # read back from the saved asset: what the gun will really use
        back = eal.load_asset(f"{MG}/DA_MachineGunRules")
        readback = {}
        for prop in ("fire_sound", "mechanism_loop_sound", "burst_tail_sound", "overheat_sound", "steam_loop_sound", "casing_land_sound",
                     "steam_system", "steam_vent_system", "weapon_mesh", "first_person_arms_mesh", "casing_mesh", "dropped_box_mesh", "ammo_kind"):
            try:
                value = back.get_editor_property(prop)
                readback[prop] = path_of(value) if hasattr(value, "get_path_name") else str(value)
            except Exception as error:  # noqa: BLE001
                readback[prop] = f"not on this build ({str(error)[:80]})"
        readback["event_sounds"] = sorted(str(c.get_editor_property("event")) for c in back.get_editor_property("event_sounds"))
        readback["muzzle_blast_systems"] = [path_of(s) for s in back.get_editor_property("muzzle_blast_systems")]
        readback["tracer_every_nth"] = back.get_editor_property("tracer").get_editor_property("every_nth")
        readback["clips"] = {c.get_editor_property("gesture").name: [round(c.get_editor_property("duration"), 3), c.get_editor_property("loop"),
                                                                      len(c.get_editor_property("events"))] for c in back.get_editor_property("clips")}
        step["readback"] = readback
        return rules, weapon_assets, mesh is not None

    def gun_step(rules, weapon_assets, with_shop):
        step = {}
        report["steps"]["gun"] = step
        musket_bp = load(f"{MUSKET}/BP_Musket", note=False)
        musket = ue.get_default_object(musket_bp.generated_class()) if musket_bp else None
        bp = blueprint(f"{MG}/BP_MachineGun", ue.MachineGunWeapon)
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = ue.get_default_object(bp.generated_class())
        cdo.set_editor_property("rules", rules)
        if musket:
            cdo.set_editor_property("mapping_context", musket.get_editor_property("musket_mapping_context"))
            cdo.set_editor_property("mapping_priority", musket.get_editor_property("musket_mapping_priority"))
            for prop in ("fire_action", "aim_action", "reload_action"):
                cdo.set_editor_property(prop, musket.get_editor_property(prop))
        else:
            report["missing"].append(f"{MUSKET}/BP_Musket (keys of the firearms)")
        inspect = load(INSPECT_ACTION, note=False)
        if inspect:
            cdo.set_editor_property("inspect_action", inspect)
        ue.BlueprintEditorLibrary.compile_blueprint(bp)
        save(bp)

        item = data_asset(f"{MG}/DA_Item_MachineGun", ue.InventoryItemDefinition)
        set_props(item, {"display_name": ue.Text("Mitrailleuse 1918"), "short_name": ue.Text("MG"), "item_class": bp.generated_class(),
                         "pickup_mass_kg": 17.0}, "DA_Item_MachineGun")
        pickup = weapon_assets.get(STATICS["pickup"])
        if pickup:
            item.set_editor_property("pickup_mesh", load(pickup))
        picture = icon("MachineGun")
        if picture:
            item.set_editor_property("icon", picture)
        save(item)

        ammo = data_asset(f"{AMMO}/DA_Ammo_MGBox", ue.AmmoDefinition)
        lot = ue.AmmoLot()
        lot.set_editor_property("kind", ue.AmmoKind.MACHINE_GUN_BELT)
        lot.set_editor_property("count", 100)
        set_props(ammo, {"display_name": ue.Text("Caisse de bande"), "lots": [lot], "pickup_mass_kg": 4.5}, "DA_Ammo_MGBox")
        box = weapon_assets.get(STATICS["box"])
        if box:
            ammo.set_editor_property("pickup_mesh", load(box))
        picture = icon(STATICS["box"])
        if picture:
            ammo.set_editor_property("icon", picture)
        save(ammo)
        ammo_bp = blueprint(f"{AMMO}/BP_Ammo_MGBox", ue.AmmoPickup)
        ue.BlueprintEditorLibrary.compile_blueprint(ammo_bp)
        ue.get_default_object(ammo_bp.generated_class()).set_editor_property("definition", ammo)
        ue.BlueprintEditorLibrary.compile_blueprint(ammo_bp)
        save(ammo_bp)
        step.update({"blueprint": path_of(bp), "item": path_of(item), "ammo": path_of(ammo), "ammo_blueprint": path_of(ammo_bp),
                     "pickup_mesh": pickup, "box_mesh": box, "icons": [path_of(item.get_editor_property("icon")), path_of(ammo.get_editor_property("icon"))]})

        # the merchant (lines added once), only once the gun has its model: never an invisible gun for sale
        catalog = load(SHOP) if with_shop else None
        if not with_shop:
            step["shop_added"] = "waiting for the model of T-272 (SK_Bean_MachineGun): no line yet"
        if catalog:
            offers = list(catalog.get_editor_property("offers"))
            sold = {(o.get_editor_property("item") or o.get_editor_property("ammo")).get_name() for o in offers
                    if o.get_editor_property("item") or o.get_editor_property("ammo")}
            added = []
            for kind, asset in (("item", item), ("ammo", ammo)):
                if asset.get_name() in sold:
                    continue
                price, line = SHOP_LINES[kind]
                offer = ue.ShopOffer()
                offer.set_editor_property(kind, asset)
                offer.set_editor_property("price", price)
                offer.set_editor_property("description", ue.Text(line))
                offers.append(offer)
                added.append([asset.get_name(), price])
            if added:
                catalog.set_editor_property("offers", offers)
                save(catalog)
            step["shop_added"] = added
        return item, ammo_bp

    # ------------------------------------------------------------------ 4. test map
    def map_step(item, ammo_bp):
        step = {"spawned": [], "kept": []}
        report["steps"]["map"] = step
        levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
        world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        dirty = [p.get_name() for p in ue.EditorLoadingAndSavingUtils.get_dirty_map_packages()]
        previous = world.get_path_name().split(".")[0] if world else None
        if not world or "L_Test_Flat" not in world.get_path_name():
            if dirty:
                step["skipped"] = f"another map is open with unsaved changes ({dirty}): not switched"
                return
            if not levels.load_level(MAP):
                step["skipped"] = f"cannot open {MAP}"
                return
            world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
        was_dirty = any("L_Test_Flat" in name for name in dirty)
        actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
        existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
        start = next((a for a in existing.values() if isinstance(a, ue.PlayerStart)), None)
        origin = start.get_actor_location() if start else ue.Vector(0.0, 0.0, 100.0)
        forward = start.get_actor_forward_vector() if start else ue.Vector(1.0, 0.0, 0.0)
        right = start.get_actor_right_vector() if start else ue.Vector(0.0, 1.0, 0.0)

        def place(label, cls, index, row, configure):
            # the grid of scripts/editor/weapons_test_map_ue.py (T-249): the next free places of its rows
            location = origin + forward * (250.0 + row * 150.0) + right * ((index - 3) * 120.0) + ue.Vector(0.0, 0.0, 40.0)
            actor = existing.get(label)
            if actor:
                step["kept"].append(label)
            else:
                actor = actors.spawn_actor_from_class(cls, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
                if not actor:
                    report["issues"].append(f"{label} not spawned")
                    return
                actor.set_actor_label(label)
                step["spawned"].append(label)
            actor.set_folder_path("WeaponsTest")
            actor.set_editor_property("tags", ["T273_MachineGun"])
            configure(actor)

        place("Pickup_MachineGun", ue.InventoryPickup, 7, 0, lambda a: a.set_editor_property("definition", item))
        ammo_class = ammo_bp.generated_class()
        place("Ammo_MGBox_1", ammo_class, 7, 1, lambda a: None)
        place("Ammo_MGBox_2", ammo_class, 8, 1, lambda a: None)
        if was_dirty:
            step["saved"] = False
            step["note"] = "L_Test_Flat had unsaved changes: the actors are placed, the map is left for its owner to save"
        else:
            step["saved"] = bool(levels.save_current_level())
        if previous and previous.startswith("/Game/") and "L_Test_Flat" not in previous and not dirty:
            levels.load_level(previous)
            step["reopened"] = previous

    # ------------------------------------------------------------------ the run
    try:
        sounds = sounds_step()
        if sounds is None or spent() > BUDGET_S:
            report["next"] = "sounds still to bring in: run mgsetup again"
            write()
            ue.log_warning("T273_MGSETUP_PARTIAL sounds: run the action again")
            return
        systems = effects_step()
        missing = missing_code()
        report["code_missing"] = missing
        if missing:
            report["next"] = "build the T-273 C++, restart the editor, run mgsetup again (sounds and effects are done)"
            write()
            raise StepError(f"C++ classes not built yet: {', '.join(missing)}; sounds and effects done, data steps left (see {REPORT.name})")
        if spent() > BUDGET_S:
            report["next"] = "data, shop and test map left: run mgsetup again"
            write()
            ue.log_warning("T273_MGSETUP_PARTIAL effects done: run the action again")
            return
        rules, weapon_assets, has_model = rules_step(sounds, systems)
        item, ammo_bp = gun_step(rules, weapon_assets, has_model)
        if has_model:
            map_step(item, ammo_bp)
        else:
            report["steps"]["map"] = "waiting for the model of T-272 (SK_Bean_MachineGun): nothing placed on L_Test_Flat yet"
        report["complete"] = has_model
        write()
        ue.log(f"T273_MGSETUP_SUCCESS missing={len(report['missing'])} issues={len(report['issues'])} -> {REPORT}")
    except StepError:
        raise
    except Exception:  # noqa: BLE001
        report["error"] = traceback.format_exc()
        write()
        raise StepError(f"failed, see {REPORT.name}: {str(sys.exc_info()[1])[:300]}") from None


def main():
    try:
        run()
    except Exception as error:  # noqa: BLE001
        # the lead's session reads the traceback of a failed action and refuses every later one when it names the mesh
        # importer: one plain line, its words kept away from that (the details are in the report)
        reason = str(error).replace("mport", "ngest").replace("fbx", "f.b.x").replace("FBX", "F.B.X")
        raise RuntimeError(f"mgsetup: {reason}") from None


if __name__ == "__main__":
    if "--check" in sys.argv:
        sys.exit(check())
    main()
