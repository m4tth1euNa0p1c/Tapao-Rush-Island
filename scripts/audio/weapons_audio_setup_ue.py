"""T-259: the sounds of the weapons v2 in Unreal (action "weaponsaudio" of the lead's editor session,
scripts/editor/lead_session_t186.py; editor open, lock held, no game running). Replays are idempotent.

1. Imports art/audio/weapons/v2/*.wav (scripts/audio/weapons_v2_prepare.py; manifest.json there) as SoundWaves:
     SW_<Weapon>_*            -> /Game/MonkeIsland/Weapons/<Weapon>/Audio (Musket for SW_PirateMusket_*)
     SW_Longgun_* SW_Firearm_* SW_Ammo_*  -> /Game/MonkeIsland/Weapons/Audio (shared)
   A WAV whose SHA-256 is already in the asset's T259.SourceSha256 tag is not imported again. The imports stop after
   IMPORT_BUDGET_S seconds of editor time (the editor freezes while it imports): a replay goes on from there, and the
   rules are wired only once every sound is in (a first run imports, a second one styles and wires: two short freezes
   rather than one long one).
2. Sound class SC_Effects; attenuation, concurrency and priority by kind (assets created in /Game/MonkeIsland/Weapons/Audio):
     ATT_Firearm_Shot       full to 15 m, natural fall-off to 300 m, air absorption (high cut with the distance)
     ATT_Firearm_Handling   full to 1.5 m, gone at 20 m (locks, reloads, draws)
     ATT_Grenade_Explosion  full to 20 m, natural fall-off to 400 m, air absorption
     ATT_Grenade_Bounce     full to 2 m, gone at 35 m
   all of them blend to 2D within 0.5-1.5 m (the carrier hears his own weapon centred);
     SCON_Firearm_Shot 12, SCON_Firearm_Handling 10, SCON_Grenade_Explosion 6 voices (the farthest, then the oldest, stop).
3. A SoundCue per sound with variants (SC_<Group>: Random node without repetition over SW_<Group>_01..0n).
4. The rules (UFirearmRules / UGrenadeRules, data only): FireSound, EventSounds (event of the manifest -> sound, volume),
   EventSoundAttenuation; BounceSound, ExplosionSound. The previous values are kept in the report; the old placeholder
   assets (SW_Musket_*, SW_Shotgun_Fire) stay in the project.
Report: monke_island/Saved/T259/weapons-audio-setup.json (rules read back after saving, events of the clips without sound).
"""
import datetime as dt
import hashlib
import json
import os
import time
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "art/audio/weapons/v2"
OUT = ROOT / "monke_island/Saved/T259"
EAL = ue.EditorAssetLibrary
WEAPONS = "/Game/MonkeIsland/Weapons"
SHARED = WEAPONS + "/Audio"
EFFECTS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
TAG = "T259.SourceSha256"
IMPORT_BUDGET_S = 20.0
# imports that took longer than this leave the styling and the wiring to the next run (each run stays under ~25 s)
WIRE_AFTER_IMPORT_S = 12.0
# group prefix -> weapon folder
FOLDERS = {"SW_Pistol_": "Pistol", "SW_DoublePistol_": "DoublePistol", "SW_Shotgun_": "Shotgun", "SW_PirateMusket_": "Musket",
           "SW_Rifle_": "Rifle", "SW_Blunderbuss_": "Blunderbuss", "SW_Grenade_": "Grenade"}
# weapon folder -> rules asset
RULES = {"Pistol": "DA_PistolRules", "DoublePistol": "DA_DoublePistolRules", "Shotgun": "DA_ShotgunRules",
         "Musket": "DA_PirateMusketRules", "Rifle": "DA_RifleRules", "Blunderbuss": "DA_BlunderbussRules",
         "Grenade": "DA_GrenadeRules"}

SHOT = "shot"
HANDLING = "handling"
BLAST = "blast"
BOUNCE = "bounce"
# kind -> (attenuation, concurrency, priority)
KINDS = {SHOT: ("ATT_Firearm_Shot", "SCON_Firearm_Shot", 2.0), HANDLING: ("ATT_Firearm_Handling", "SCON_Firearm_Handling", 1.0),
         BLAST: ("ATT_Grenade_Explosion", "SCON_Grenade_Explosion", 3.0), BOUNCE: ("ATT_Grenade_Bounce", "SCON_Firearm_Handling", 1.5)}
ATTENUATIONS = {
    "ATT_Firearm_Shot": {"inner": 1500.0, "falloff": 28500.0, "model": "NATURAL_SOUND", "lpf": (2000.0, 30000.0, 20000.0, 2500.0)},
    "ATT_Firearm_Handling": {"inner": 150.0, "falloff": 1850.0, "model": "LOGARITHMIC", "lpf": None},
    "ATT_Grenade_Explosion": {"inner": 2000.0, "falloff": 38000.0, "model": "NATURAL_SOUND", "lpf": (3000.0, 40000.0, 20000.0, 1800.0)},
    "ATT_Grenade_Bounce": {"inner": 200.0, "falloff": 3300.0, "model": "LOGARITHMIC", "lpf": None},
}
CONCURRENCIES = {"SCON_Firearm_Shot": 12, "SCON_Firearm_Handling": 10, "SCON_Grenade_Explosion": 6}


def kind_of(group):
    if group.endswith("_Fire") or group.endswith("_Fire_V2"):
        return SHOT
    if group == "SW_Grenade_Explosion":
        return BLAST
    if group == "SW_Grenade_Bounce":
        return BOUNCE
    return HANDLING


SHARED_EVENTS = {"DrawWeapon": ("SW_Firearm_Draw", 0.9), "Ready": ("SW_Firearm_Ready", 0.7),
                 "StowWeapon": ("SW_Firearm_Stow", 0.9), "Empty": ("SW_Firearm_DiscardEmpty", 1.0),
                 "Release": ("SW_Firearm_DiscardRelease", 1.0)}
PISTOL_EVENTS = {**SHARED_EVENTS, "HammerDrop": ("SW_Pistol_HammerDrop", 1.0), "AssistLeft": ("SW_Firearm_HandGrab", 0.8),
                 "PaperPickup": ("SW_Pistol_PaperPickup", 1.0), "PaperTransfer": ("SW_Pistol_PaperTransfer", 1.0),
                 "RamrodGesture": ("SW_Pistol_Ramrod", 1.0), "RamrodReturned": ("SW_Pistol_RamrodReturn", 1.0),
                 "HammerCocked": ("SW_Pistol_Cock", 1.0), "CockHammer": ("SW_Pistol_Cock", 1.0)}
LONGGUN_EVENTS = {**SHARED_EVENTS, "Trigger": ("SW_Longgun_Trigger", 1.0), "HalfCockStart": ("SW_Longgun_HalfCock", 1.0),
                  "PanPrime": ("SW_Longgun_PanPrime", 1.0), "FrizzenClose": ("SW_Longgun_FrizzenClose", 1.0),
                  "StockLowered": ("SW_Longgun_StockGround", 1.0), "PowderCharge": ("SW_Longgun_PowderCharge", 1.0),
                  "BallAndPatch": ("SW_Longgun_BallPatch", 1.0), "RamrodDraw": ("SW_Longgun_RamrodDraw", 1.0),
                  "RamrodTurn": ("SW_Longgun_RamrodTurn", 1.0), "RamrodStroke": ("SW_Longgun_RamrodStroke", 1.0),
                  "RamrodReturn": ("SW_Longgun_RamrodReturn", 1.0), "FullCockEnd": ("SW_Longgun_FullCock", 1.0)}
# weapon folder -> (fire sound group or None, events)
WIRING = {
    "Pistol": ("SW_Pistol_Fire", PISTOL_EVENTS),
    "DoublePistol": ("SW_DoublePistol_Fire", PISTOL_EVENTS),
    "Shotgun": ("SW_Shotgun_Fire_V2", {**SHARED_EVENTS, "PumpBackStart": ("SW_Shotgun_PumpSlide", 0.8),
                                       "PumpBackEnd": ("SW_Shotgun_PumpBack", 1.0),
                                       "EjectSpentShell": ("SW_Shotgun_ShellEject", 1.0),
                                       "PumpForwardStart": ("SW_Shotgun_PumpSlide", 0.6),
                                       "PumpForwardEnd": ("SW_Shotgun_PumpForward", 1.0),
                                       "LeftHandRelease": ("SW_Firearm_HandGrab", 0.5),
                                       "ShellPickup": ("SW_Shotgun_ShellPickup", 1.0),
                                       "InsertSlug": ("SW_Shotgun_ShellInsert", 1.0),
                                       "LeftHandRegrab": ("SW_Firearm_HandGrab", 0.7),
                                       "DryClick": ("SW_Shotgun_DryClick", 1.0)}),
    "Musket": ("SW_PirateMusket_Fire", LONGGUN_EVENTS),
    "Rifle": ("SW_Rifle_Fire", {**LONGGUN_EVENTS, "PatchBox": ("SW_Rifle_PatchBox", 1.0)}),
    "Blunderbuss": ("SW_Blunderbuss_Fire", {**LONGGUN_EVENTS, "ShotPour": ("SW_Blunderbuss_ShotPour", 1.0)}),
    "Grenade": (None, {"DrawWeapon": ("SW_Firearm_Draw", 0.9), "Ready": ("SW_Firearm_Ready", 0.7),
                       "StowWeapon": ("SW_Firearm_Stow", 0.9), "Light": ("SW_Grenade_Light", 1.0),
                       "Fire": ("SW_Grenade_Throw", 1.0), "BagReach": ("SW_Grenade_BagReach", 1.0),
                       "HammerCocked": ("SW_Grenade_Take", 1.0), "DryClick": ("SW_Grenade_EmptyBag", 1.0),
                       "Empty": ("SW_Grenade_EmptyBag", 0.8), "Release": ("SW_Grenade_BagToss", 1.0)}),
}
# events that need no sound of their own (the shot, its flash and recoil are in FireSound; ChamberSlug in PumpForward)
SILENT_EVENTS = {"Fire", "Shot", "PanFlash", "MuzzleFlash", "Recoil", "ChamberSlug", "DryClick"}


def check_editor():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    owner = os.environ.get("MONKE_LOCK_OWNER")
    assert owner and lock["owner"] == owner, f"editor lock held by {lock['owner']}, not {owner}"
    assert dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone(), "editor lock expired"
    assert not ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor(), "a game is running"


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    assert EAL.save_loaded_asset(asset, only_if_is_dirty=False), asset.get_path_name()


def path_of(obj):
    return obj.get_path_name() if obj else None


def folder_of(name):
    for prefix, weapon in FOLDERS.items():
        if name.startswith(prefix):
            return f"{WEAPONS}/{weapon}/Audio"
    return SHARED


def set_props(obj, values, report):
    """Sets what exists on this engine version, reports the rest"""
    for prop, value in values.items():
        try:
            obj.set_editor_property(prop, value)
        except Exception as error:  # noqa: BLE001
            report.append(f"{obj.get_name() if hasattr(obj, 'get_name') else type(obj).__name__}.{prop}: {error}")


def make_attenuation(tools, name, spec, issues):
    path = f"{SHARED}/{name}"
    asset = load(path) or tools.create_asset(name, SHARED, ue.SoundAttenuation, None)
    settings = asset.get_editor_property("attenuation")
    values = {"attenuate": True, "spatialize": True, "attenuation_shape": ue.AttenuationShape.SPHERE,
              "attenuation_shape_extents": ue.Vector(spec["inner"], 0.0, 0.0), "falloff_distance": spec["falloff"],
              "distance_algorithm": getattr(ue.AttenuationDistanceModel, spec["model"]),
              "non_spatialized_radius_start": 150.0, "non_spatialized_radius_end": 50.0}
    if spec["lpf"]:
        near, far, freq_near, freq_far = spec["lpf"]
        values.update({"attenuate_with_lpf": True, "lpf_radius_min": near, "lpf_radius_max": far,
                       "lpf_frequency_at_min": freq_near, "lpf_frequency_at_max": freq_far})
    set_props(settings, values, issues)
    asset.set_editor_property("attenuation", settings)
    save(asset)
    return asset


def make_concurrency(tools, name, count, issues):
    path = f"{SHARED}/{name}"
    asset = load(path) or tools.create_asset(name, SHARED, ue.SoundConcurrency, None)
    settings = asset.get_editor_property("concurrency")
    set_props(settings, {"max_count": count, "limit_to_owner": False,
                         "resolution_rule": ue.MaxConcurrentResolutionRule.STOP_FARTHEST_THEN_OLDEST}, issues)
    asset.set_editor_property("concurrency", settings)
    save(asset)
    return asset


def style(sound, kind, mixing, issues):
    """Class, attenuation, concurrency, priority of a SoundWave or SoundCue"""
    attenuation, concurrency, priority = mixing[kind]
    set_props(sound, {"sound_class_object": mixing["class"], "attenuation_settings": attenuation, "priority": priority},
              issues)
    try:
        sound.set_editor_property("concurrency_set", [concurrency])
    except Exception:  # noqa: BLE001 - a TSet on some versions
        set_props(sound, {"concurrency_set": {concurrency}}, issues)


def import_wave(tools, entry):
    name = entry["asset"]
    folder = folder_of(name)
    wav = SRC / f"{name}.wav"
    digest = hashlib.sha256(wav.read_bytes()).hexdigest()
    existing = load(f"{folder}/{name}")
    if existing and EAL.get_metadata_tag(existing, TAG) == digest:
        return existing, False
    task = ue.AssetImportTask()
    task.filename = str(wav)
    task.destination_path = folder
    task.destination_name = name
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    sound = load(f"{folder}/{name}")
    assert sound, f"{name} did not import"
    EAL.set_metadata_tag(sound, TAG, digest)
    return sound, True


def make_cue(tools, group, waves, issues):
    """SC_<Group without SW_>: a Random node over the variants (SoundCueFactoryNew with several initial waves)"""
    name = "SC_" + group[len("SW_"):]
    folder = folder_of(group)
    cue = load(f"{folder}/{name}")
    if cue:
        return cue, False
    factory = ue.SoundCueFactoryNew()
    try:
        factory.set_editor_property("initial_sound_waves", waves)
    except Exception as error:  # noqa: BLE001
        issues.append(f"{name}: initial_sound_waves refused ({error}), single wave cue")
        factory.set_editor_property("initial_sound_wave", waves[0])
    cue = tools.create_asset(name, folder, ue.SoundCue, factory)
    assert cue, f"{name} not created"
    return cue, True


def package_file(asset):
    """The saved .uasset of an asset of /Game"""
    package = asset.get_path_name().split(".")[0]
    return ROOT / "monke_island/Content" / (package[len("/Game/"):] + ".uasset")


def saved_references(asset, others):
    """The assets among `others` whose package path is written in the saved package of `asset` (its import table)"""
    data = package_file(asset).read_bytes()
    return [o for o in others if o.get_path_name().split(".")[0].encode("utf-8") in data]


def wire(weapon, sounds, mixing, report):
    rules_path = f"{WEAPONS}/{weapon}/{RULES[weapon]}"
    rules = load(rules_path)
    if not rules:
        report["missing_rules"].append(rules_path)
        return
    fire_group, events = WIRING[weapon]
    before = {"fire_sound": path_of(rules.get_editor_property("fire_sound")),
              "event_sounds": [[str(c.get_editor_property("event")), path_of(c.get_editor_property("sound"))]
                               for c in rules.get_editor_property("event_sounds")],
              "event_sound_attenuation": path_of(rules.get_editor_property("event_sound_attenuation"))}
    if fire_group:
        rules.set_editor_property("fire_sound", sounds[fire_group])
    cues = []
    for event, (group, volume) in events.items():
        cue = ue.FirearmEventSound()
        cue.set_editor_property("event", event)
        cue.set_editor_property("sound", sounds[group])
        cue.set_editor_property("volume", volume)
        cues.append(cue)
    rules.set_editor_property("event_sounds", cues)
    rules.set_editor_property("event_sound_attenuation", mixing[HANDLING][0])
    if weapon == "Grenade":
        before["bounce_sound"] = path_of(rules.get_editor_property("bounce_sound"))
        before["explosion_sound"] = path_of(rules.get_editor_property("explosion_sound"))
        rules.set_editor_property("bounce_sound", sounds["SW_Grenade_Bounce"])
        rules.set_editor_property("explosion_sound", sounds["SW_Grenade_Explosion"])
    save(rules)

    rules = EAL.load_asset(rules_path)
    after = {"fire_sound": path_of(rules.get_editor_property("fire_sound")),
             "event_sounds": [[str(c.get_editor_property("event")), path_of(c.get_editor_property("sound")),
                               round(c.get_editor_property("volume"), 2)] for c in rules.get_editor_property("event_sounds")],
             "event_sound_attenuation": path_of(rules.get_editor_property("event_sound_attenuation"))}
    if weapon == "Grenade":
        after["bounce_sound"] = path_of(rules.get_editor_property("bounce_sound"))
        after["explosion_sound"] = path_of(rules.get_editor_property("explosion_sound"))
    clip_events = sorted({str(e.get_editor_property("name")) for clip in rules.get_editor_property("clips")
                          for e in clip.get_editor_property("events")})
    sounded = {str(c[0]) for c in after["event_sounds"]}
    report["rules"][weapon] = {"path": rules_path, "before": before, "after": after, "clip_events": clip_events,
                               "events_without_sound": [e for e in clip_events if e not in sounded and e not in SILENT_EVENTS],
                               "sounds_for_absent_events": sorted(e for e in sounded if e not in clip_events)}


def main():
    check_editor()
    started = time.monotonic()
    tools = ue.AssetToolsHelpers.get_asset_tools()
    manifest = json.loads((SRC / "manifest.json").read_text(encoding="utf-8"))
    report = {"task": "T-259", "imported_now": [], "kept": 0, "remaining": [], "cues": {}, "issues": [], "rules": {},
              "missing_rules": [], "assets": {}, "styles": {}, "attenuations": {}, "concurrencies": {}}

    # 1. waves (time-boxed)
    waves = {}
    for entry in manifest["sounds"]:
        if time.monotonic() - started > IMPORT_BUDGET_S:
            report["remaining"].append(entry["asset"])
            continue
        sound, imported = import_wave(tools, entry)
        waves[entry["asset"]] = (sound, entry)
        if imported:
            report["imported_now"].append(entry["asset"])
        else:
            report["kept"] += 1
    if report["remaining"] or (report["imported_now"] and time.monotonic() - started > WIRE_AFTER_IMPORT_S):
        # the editor has been frozen long enough: the imports are saved, the next run styles and wires
        for sound, _ in waves.values():
            if sound.get_name() in report["imported_now"]:
                save(sound)
        report["complete"] = False
        write(report, started)
        ue.log_warning(f"T259_WEAPONS_AUDIO_PARTIAL remaining={len(report['remaining'])}: run the action again")
        return

    # 2. mixing assets, then every wave styled
    mixing = {"class": load(EFFECTS)}
    for name, spec in ATTENUATIONS.items():
        make_attenuation(tools, name, spec, report["issues"])
    for name, count in CONCURRENCIES.items():
        make_concurrency(tools, name, count, report["issues"])
    for kind, (attenuation, concurrency, priority) in KINDS.items():
        mixing[kind] = (load(f"{SHARED}/{attenuation}"), load(f"{SHARED}/{concurrency}"), priority)
    for name, (sound, entry) in waves.items():
        style(sound, kind_of(entry["group"]), mixing, report["issues"])
        sound.set_editor_property("looping", bool(entry.get("loop")))
        save(sound)
        report["assets"][name] = {"path": sound.get_path_name(), "seconds": round(sound.get_editor_property("duration"), 3)}

    # 3. cues over the variants
    groups = {}
    for name, (sound, entry) in waves.items():
        groups.setdefault(entry["group"], []).append((entry["variant"], sound))
    sounds = {}
    for group, members in groups.items():
        members = [s for _, s in sorted(members, key=lambda m: m[0])]
        if len(members) == 1:
            sounds[group] = members[0]
            continue
        cue, created = make_cue(tools, group, members, report["issues"])
        style(cue, kind_of(group), mixing, report["issues"])
        save(cue)
        sounds[group] = cue
        data = package_file(cue).read_bytes()
        report["cues"][cue.get_name()] = {"path": cue.get_path_name(), "created_now": created, "variants": len(members),
                                          "variants_referenced": len(saved_references(cue, members)),
                                          "random_node": b"SoundNodeRandom" in data,
                                          "seconds": round(cue.get_editor_property("duration"), 3)}

    # what the sounds carry, read back from one of each kind
    for kind, group in ((SHOT, "SW_Rifle_Fire_01"), (HANDLING, "SW_Longgun_RamrodDraw"), (BLAST, "SW_Grenade_Explosion_01"),
                        (BOUNCE, "SW_Grenade_Bounce_01")):
        sound = waves[group][0]
        report["styles"][kind] = {"sample": group, "class": path_of(sound.get_editor_property("sound_class_object")),
                                  "attenuation": path_of(sound.get_editor_property("attenuation_settings")),
                                  "concurrency": [path_of(c) for c in sound.get_editor_property("concurrency_set")],
                                  "priority": sound.get_editor_property("priority")}
    for name in ATTENUATIONS:
        settings = load(f"{SHARED}/{name}").get_editor_property("attenuation")
        report["attenuations"][name] = {prop: str(settings.get_editor_property(prop)) for prop in (
            "attenuation_shape_extents", "falloff_distance", "distance_algorithm", "attenuate_with_lpf", "lpf_radius_max",
            "lpf_frequency_at_max", "non_spatialized_radius_start", "non_spatialized_radius_end")}
    for name in CONCURRENCIES:
        settings = load(f"{SHARED}/{name}").get_editor_property("concurrency")
        report["concurrencies"][name] = {prop: str(settings.get_editor_property(prop)) for prop in ("max_count", "resolution_rule")}

    # 4. the rules
    for weapon in WIRING:
        wire(weapon, sounds, mixing, report)
    report["complete"] = not report["missing_rules"]
    write(report, started)
    ue.log(f"T259_WEAPONS_AUDIO_SETUP_SUCCESS sounds={len(waves)} cues={len(report['cues'])} rules={len(report['rules'])}")


def write(report, started):
    report["editor_seconds"] = round(time.monotonic() - started, 1)
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "weapons-audio-setup.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


if __name__ == "__main__":
    main()
