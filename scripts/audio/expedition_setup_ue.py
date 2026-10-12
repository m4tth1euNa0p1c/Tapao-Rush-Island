"""T-234: imports, styles and binds the prepared expedition audio (art/audio/expedition/manifest.json) in Unreal.

Run as the action "expeditionaudio" of the lead's editor session (scripts/editor/lead_session_t186.py): editor open,
editor lock held by MONKE_LOCK_OWNER, no game running, no other session's action failed in the last 2 minutes (after a
failed FBX import, restart the editor first: on 2026-10-10 the first run crashed the editor in libfbxsdk right after
one). Every run is idempotent and short (the editor freezes while it
runs): the WAV imports stop after IMPORT_BUDGET_S seconds and the next run goes on; the bindings are written once every
sound is in. Run it again until the report says "phase_a_complete": true (and "complete": true after the build).

Phase A (no new C++ needed)
  1. SoundWaves /Game/MonkeIsland/Audio/Expedition/SW_Exp_<Id>: a WAV whose SHA-256 is already in the asset's
     T234.SourceSha256 tag is not imported again; the duration is checked against the manifest.
  2. Mixing: sound class by settings channel (cahier §18): ambience loops and thunder -> SC_Ambience, jingles and music
     -> SC_Music, coins and gems -> SC_Interface, the rest -> SC_Effects; concurrency CON_Exp_Ambience 8 /
     CON_Exp_Foley 12 / CON_Exp_Music 2 (quietest stopped); attenuation ATT_Exp_<Group> for any 3D use.
     A recording that replaces a provisional sound takes the class, attenuation, concurrency and priority of the sound
     it replaces, so the system that plays it behaves as before (only the recording changes).
  3. Bindings into existing rules, only into an empty field or a field that still holds a listed provisional sound:
       DA_FootstepRules SandDry            <- SandStep_01..04 (was SW_Step_SandDry_0N: Kenney snow steps, a substitute)
       DA_CutlassRules Draw/Sheathe/Parry  <- BladeDraw / BladeSheathe / BladeParry
       DA_FortSoldierRules DrawSound       <- BladeDraw  (were the first-pass syntheses of scripts/props/cutlass_sounds.py,
                                              whose swing and hit the owner already replaced in T-259)
     Anything else found in those fields is left alone and listed under "skipped".
  Never written, current values listed under "left_alone": the T-259 cutlass SwingSound / FinisherSwingSound /
  HitSound / KillSound and DA_MusicRules; the T-261 DA_GiantCrabRules; DA_GiantCrabRules_Sand (the same boss: the
  dedicated T-261 sounds belong there, not this wood foley); DA_BoatRules and DA_ShoreAmbienceRules (made-to-measure
  syntheses: 8 s wake following the speed, 48 s / 32 s shore loops; the prepared loops last 1.8-2.8 s and would repeat
  audibly); DA_FootstepRules SandWet (4 made-to-measure variants against 3 generic wet steps). The weapons v2 rules are
  not referenced by this script.
Phase B (needs UExpeditionAudioRules / FExpeditionAudioCue: the lead's full editor build with T-234)
  4. DA_ExpeditionAudioRules: the event -> cue table read by UExpeditionAudioSubsystem. Before that build it is reported
     under "pending" and phase A still completes.
Cooking: the subsystem loads its rules by path, so /Game/MonkeIsland/Audio/Expedition must be listed in
DirectoriesToAlwaysCook (DefaultGame.ini, lead). A PrimaryAssetLabel would not cook it: the engine scans labels as
editor-only (BaseGame.ini).
Report: monke_island/Saved/T234/import-report.json (values read back after saving). Copies of every rules asset before it
is written: monke_island/Saved/T234/backups/<UTC stamp>/.
"""
from __future__ import annotations

import datetime as dt
import hashlib
import json
import os
import re
import shutil
import time
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
DEST = "/Game/MonkeIsland/Audio/Expedition"
OUT = ROOT / "monke_island/Saved/T234"
EAL = ue.EditorAssetLibrary
TAG = "T234.SourceSha256"
# editor time spent importing in one run; the next run goes on from there
IMPORT_BUDGET_S = 15.0
CLASS_ROOT = "/Game/MonkeIsland/Settings/SoundClasses"
BOAT = "/Game/MonkeIsland/Boat/Data/DA_BoatRules"
SHORE = "/Game/MonkeIsland/Environment/Shore/DA_ShoreAmbienceRules"
STEPS = "/Game/MonkeIsland/Player/Footsteps/DA_FootstepRules"
CUTLASS = "/Game/MonkeIsland/Weapons/Cutlass/DA_CutlassRules"
CRAB = "/Game/MonkeIsland/Characters/GiantCrab/DA_GiantCrabRules"
CRAB_SAND = "/Game/MonkeIsland/Characters/GiantCrab/DA_GiantCrabRules_Sand"
SOLDIER = "/Game/MonkeIsland/Characters/BeanFR/Data/DA_FortSoldierRules"
MUSIC_RULES = "/Game/MonkeIsland/Audio/Music/DA_MusicRules"
CUTLASS_AUDIO = "/Game/MonkeIsland/Weapons/Cutlass/Audio"
STEP_AUDIO = "/Game/MonkeIsland/Player/Footsteps/Audio"
RULES_PATH = DEST + "/DA_ExpeditionAudioRules"

# prepared sound -> the provisional sound it replaces (its mixing is copied)
REPLACES = {**{f"SandStep_{i:02}": f"{STEP_AUDIO}/SW_Step_SandDry_{i:02}" for i in range(1, 5)},
            "BladeDraw": f"{CUTLASS_AUDIO}/SW_Cutlass_Draw",
            "BladeSheathe": f"{CUTLASS_AUDIO}/SW_Cutlass_Sheathe",
            "BladeParry": f"{CUTLASS_AUDIO}/SW_Cutlass_Parry"}
# (rules, property, prepared sound, provisional sounds this binding may replace)
BINDINGS = [(CUTLASS, "draw_sound", "BladeDraw", {REPLACES["BladeDraw"]}),
            (CUTLASS, "sheathe_sound", "BladeSheathe", {REPLACES["BladeSheathe"]}),
            (CUTLASS, "parry_sound", "BladeParry", {REPLACES["BladeParry"]}),
            (SOLDIER, "draw_sound", "BladeDraw", {REPLACES["BladeDraw"]})]
SAND_DRY = [f"SandStep_{i:02}" for i in range(1, 5)]
# (rules, properties, reason): never written, their current values go in the report
LEFT_ALONE = [
    (CUTLASS, ["swing_sound", "finisher_swing_sound", "hit_sound", "kill_sound"], "T-259: the owner's cutlass sounds"),
    (MUSIC_RULES, ["galleon", "boat"], "T-259: the owner's galleon and boat tracks"),
    (CRAB, ["clack_sound", "hit_sound", "slam_sound"], "T-261: the giant crab boss's dedicated sounds"),
    (CRAB_SAND, ["clack_sound", "hit_sound", "slam_sound"],
     "same boss as DA_GiantCrabRules (empty fields): the dedicated T-261 sounds belong there, not the T-234 wood foley"),
    (BOAT, ["oar_splash_sound", "wake_sound"],
     "made-to-measure syntheses of 2026-10-07 (seamless 8 s wake following the speed, splash matched with the drip); "
     "the prepared HullWater loop lasts 2.8 s"),
    (SHORE, ["surf_sound", "sea_sound"],
     "made-to-measure loops of 48 s and 32 s; the prepared ShoreSurf / ShoreSea loops last 2.1 s and 1.8 s"),
]
SAND_WET_REASON = "4 made-to-measure variants (footstep_sounds.py) against 3 generic wet steps: kept"
# event of UExpeditionAudioSubsystem -> prepared sounds (variations)
EVENTS = {
    "HullWater": ["HullWater"], "Underwater": ["Underwater"], "WindLight": ["WindLight"],
    "WindStrong": ["WindStrong"], "WindStorm": ["WindStorm"], "Rain": ["Rain"], "Crickets": ["Crickets"],
    "Thunder": ["Thunder"], "StormStart": ["Danger"], "WoodCreak": [f"WoodCreak_{i:02}" for i in range(1, 4)],
    "SwimEnter": ["SwimEnter"], "SwimExit": ["SwimExit"], "Pickup": ["Gem_01", "Gem_02"],
    "Lift": ["WoodHit_01"], "Purchase": ["Coin_01", "Coin_02", "Coin_03"],
    "PurchaseDenied": ["QuotaFailed"], "Merchant": ["Merchant"], "DayStart": ["DayStart"],
    "HalfTime": ["Bell"], "QuotaMet": ["QuotaMet"], "Victory": ["Victory"], "Defeat": ["Defeat"],
}
# events played with a sound already in the project (provisional voices, see docs/audio/expedition-validation-matrix.md)
EXISTING = {
    "Countdown": "/Game/MonkeIsland/UI/Tapao/Audio/SW_UI_Click",
    "PlayerHurt": "/Game/MonkeIsland/Characters/Tiki/Audio/SW_Villager_Hurt_01",
    "PlayerDeath": "/Game/MonkeIsland/Characters/Tiki/Audio/SW_Villager_Death_01",
}
ATTENUATION = {"Foley": (200.0, 1800.0), "Weapon": (300.0, 2200.0), "Water": (200.0, 1800.0),
               "Distant": (1000.0, 9000.0), "Ambience": (600.0, 3400.0)}
CONCURRENCY = {"Ambience": 8, "Foley": 12, "Music": 2}
# result of the previous action of the lead's session, and how long after another session's failed action this one
# refuses to start (2026-10-10: right after a failed FBX import, still in the same engine frame, the first AssetTools
# call of this script crashed the editor in libfbxsdk)
PREVIOUS_RESULT = ROOT / "monke_island/Saved/T186/session-result.json"
SETTLE_AFTER_FAILURE_S = 120.0


def check_previous_action():
    if not PREVIOUS_RESULT.exists():
        return
    try:
        previous = json.loads(PREVIOUS_RESULT.read_text("utf-8"))
        finished = dt.datetime.fromisoformat(previous["finished_at"])
    except (ValueError, KeyError) as error:
        raise RuntimeError(f"Unreadable {PREVIOUS_RESULT.name} ({error}): nothing done") from error
    if previous.get("passed", True) or previous.get("action") == "expeditionaudio":
        return
    age = (dt.datetime.now(dt.timezone.utc) - finished).total_seconds()
    if age < SETTLE_AFTER_FAILURE_S:
        raise RuntimeError(f"The action '{previous.get('action')}' of another session failed {age:.0f} s ago; the editor "
                           f"may still be finishing its imports: restart the editor (always after a failed FBX import) "
                           f"or retry in {SETTLE_AFTER_FAILURE_S - age:.0f} s. Nothing done.")


def check_lock():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text("utf-8"))
    owner = os.environ.get("MONKE_LOCK_OWNER")
    if not owner or owner != lock["owner"]:
        raise RuntimeError(f"Editor lock mismatch: held by {lock['owner']}, requested {owner}")
    if dt.datetime.fromisoformat(lock["expires"]) <= dt.datetime.now().astimezone():
        raise RuntimeError("Editor lock expired")
    if ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("PIE running; do not stop another session's game")


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def require(path):
    asset = load(path)
    if not asset:
        raise RuntimeError(f"Required asset missing: {path}")
    return asset


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise RuntimeError(f"Save failed: {asset.get_path_name()}")


def path_of(obj):
    """Package path of an asset (no object name), None for none"""
    return obj.get_path_name().split(".")[0] if obj else None


def class_of(entry):
    if entry["group"] == "Ambience" or entry["id"] == "Thunder":
        return "SC_Ambience"
    return {"Music": "SC_Music", "UI": "SC_Interface"}.get(entry["group"], "SC_Effects")


def concurrency_of(entry):
    if entry["group"] == "Music":
        return "Music"
    return "Ambience" if entry["loop"] else "Foley"


def make_mixing(tools, report):
    """Concurrency and attenuation assets of the new content (no other sound class or asset is changed)"""
    mixing = {"classes": {name: require(f"{CLASS_ROOT}/{name}")
                          for name in ("SC_Ambience", "SC_Music", "SC_Interface", "SC_Effects")},
              "concurrency": {}, "attenuation": {}}
    for group, count in CONCURRENCY.items():
        name = f"CON_Exp_{group}"
        asset = load(f"{DEST}/{name}")
        created = asset is None
        asset = asset or tools.create_asset(name, DEST, ue.SoundConcurrency, None)
        if not asset:
            raise RuntimeError(f"Could not create {name}")
        settings = asset.get_editor_property("concurrency")
        wanted = ue.MaxConcurrentResolutionRule.STOP_QUIETEST
        if created or settings.get_editor_property("max_count") != count \
                or settings.get_editor_property("resolution_rule") != wanted:
            settings.set_editor_property("max_count", count)
            settings.set_editor_property("resolution_rule", wanted)
            asset.set_editor_property("concurrency", settings)
            save(asset)
        mixing["concurrency"][group] = asset
        report["mixing"][name] = {"max_count": count, "resolution_rule": "STOP_QUIETEST"}
    for group, (inner, falloff) in ATTENUATION.items():
        name = f"ATT_Exp_{group}"
        asset = load(f"{DEST}/{name}")
        created = asset is None
        asset = asset or tools.create_asset(name, DEST, ue.SoundAttenuation, None)
        if not asset:
            raise RuntimeError(f"Could not create {name}")
        settings = asset.get_editor_property("attenuation")
        extents = settings.get_editor_property("attenuation_shape_extents")
        if created or abs(extents.x - inner) > 0.5 or abs(settings.get_editor_property("falloff_distance") - falloff) > 0.5:
            settings.set_editor_property("attenuation_shape", ue.AttenuationShape.SPHERE)
            settings.set_editor_property("attenuation_shape_extents", ue.Vector(inner, 0.0, 0.0))
            settings.set_editor_property("falloff_distance", falloff)
            asset.set_editor_property("attenuation", settings)
            save(asset)
        mixing["attenuation"][group] = asset
        report["mixing"][name] = {"inner_radius_cm": inner, "falloff_cm": falloff}
    return mixing


def wanted_style(entry, mixing):
    """(class, attenuation, concurrency list, priority or None) the sound must carry"""
    template = load(REPLACES[entry["id"]]) if entry["id"] in REPLACES else None
    if template:
        return (template.get_editor_property("sound_class_object"), template.get_editor_property("attenuation_settings"),
                list(template.get_editor_property("concurrency_set")), template.get_editor_property("priority"))
    return (mixing["classes"][class_of(entry)], mixing["attenuation"].get(entry["group"]),
            [mixing["concurrency"][concurrency_of(entry)]], None)


def style(sound, entry, mixing):
    """Applies the wanted mixing; returns True when something changed"""
    sound_class, attenuation, concurrency, priority = wanted_style(entry, mixing)
    changed = False
    for prop, value in (("sound_class_object", sound_class), ("attenuation_settings", attenuation)):
        if path_of(sound.get_editor_property(prop)) != path_of(value):
            sound.set_editor_property(prop, value)
            changed = True
    if priority is not None and abs(sound.get_editor_property("priority") - priority) > 1e-4:
        sound.set_editor_property("priority", priority)
        changed = True
    if bool(sound.get_editor_property("looping")) != bool(entry["loop"]):
        sound.set_editor_property("looping", bool(entry["loop"]))
        changed = True
    current = sorted(path_of(c) for c in sound.get_editor_property("concurrency_set"))
    if current != sorted(path_of(c) for c in concurrency):
        try:
            sound.set_editor_property("concurrency_set", concurrency)
        except Exception:  # noqa: BLE001 - a TSet wants a Python set on some versions
            sound.set_editor_property("concurrency_set", set(concurrency))
        changed = True
    return changed


def import_wave(tools, entry):
    """Imports the WAV unless the asset already holds it (same SHA-256); returns (sound, imported now)"""
    existing = load(entry["asset"])
    if existing and EAL.get_metadata_tag(existing, TAG) == entry["sha256"]:
        return existing, False
    task = ue.AssetImportTask()
    task.filename = str(ROOT / entry["file"])
    task.destination_path = DEST
    task.destination_name = entry["asset"].rsplit("/", 1)[1]
    task.replace_existing = True
    task.automated = True
    task.save = False
    tools.import_asset_tasks([task])
    sound = load(entry["asset"])
    if not sound:
        raise RuntimeError(f"{entry['id']} did not import")
    EAL.set_metadata_tag(sound, TAG, entry["sha256"])
    return sound, True


def describe(sound):
    return {"class": path_of(sound.get_editor_property("sound_class_object")),
            "attenuation": path_of(sound.get_editor_property("attenuation_settings")),
            "concurrency": sorted(path_of(c) for c in sound.get_editor_property("concurrency_set")),
            "priority": round(float(sound.get_editor_property("priority")), 3)}


def value_of(asset, prop):
    try:
        value = asset.get_editor_property(prop)
    except Exception as error:  # noqa: BLE001 - property absent from this build
        return f"unreadable: {error}"
    return path_of(value) if isinstance(value, ue.Object) or value is None else str(value)


def sounds_on_disk(rules_path):
    """Sound packages named in the saved rules asset (a soft reference may read as None while it is not loaded)"""
    disk = ROOT / "monke_island/Content" / (rules_path.removeprefix("/Game/") + ".uasset")
    if not disk.exists():
        return []
    found = re.findall(rb"/Game/[A-Za-z0-9_/]+/SW_[A-Za-z0-9_]+", disk.read_bytes())
    return sorted({f.decode("ascii") for f in found})


class Writer:
    """Copies a rules asset once before its first write in this run, refuses unsaved changes of someone else"""

    def __init__(self, report):
        self.report = report
        self.done = set()
        self.stamp = dt.datetime.now(dt.timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        self.dirty = {p.get_name() for p in ue.EditorLoadingAndSavingUtils.get_dirty_content_packages()}

    def before_write(self, rules_path):
        if rules_path in self.done:
            return
        if rules_path in self.dirty:
            raise RuntimeError(f"Unsaved changes in {rules_path} belong to another session; nothing written to it")
        disk = ROOT / "monke_island/Content" / (rules_path.removeprefix("/Game/") + ".uasset")
        dest = OUT / "backups" / self.stamp / disk.relative_to(ROOT / "monke_island/Content")
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(disk, dest)
        self.report["backups"].append(dest.relative_to(ROOT).as_posix() if dest.is_relative_to(ROOT) else str(dest))
        self.done.add(rules_path)


def bind(writer, rules_path, prop, sound, provisional, report):
    rules = require(rules_path)
    before = value_of(rules, prop)
    target = path_of(sound)
    row = {"asset": rules_path, "property": prop, "before": before, "wanted": target}
    if before == target:
        row["result"] = "already bound"
    elif before is None or before in provisional:
        writer.before_write(rules_path)
        rules.set_editor_property(prop, sound)
        save(rules)
        after = value_of(EAL.load_asset(rules_path), prop)
        if after != target:
            raise RuntimeError(f"Binding readback failed: {rules_path}.{prop} -> {after}")
        row["result"] = "bound (empty)" if before is None else "bound (replaced a provisional sound)"
        row["after"] = after
    else:
        row["result"] = "skipped: neither empty nor a listed provisional sound"
        report["skipped"].append(row)
        return
    report["bindings"].append(row)


def bind_footsteps(writer, sounds, report):
    rules = require(STEPS)
    surfaces = rules.get_editor_property("sounds")
    dry = surfaces[ue.FootstepSurface.SAND_DRY]
    before = [path_of(s) for s in dry.get_editor_property("sounds")]
    target = [path_of(sounds[i]) for i in SAND_DRY]
    row = {"asset": STEPS, "property": "sounds[SandDry].sounds", "before": before, "wanted": target}
    if before == target:
        row["result"] = "already bound"
    elif not before or set(before) <= {REPLACES[i] for i in SAND_DRY}:
        writer.before_write(STEPS)
        dry.set_editor_property("sounds", [sounds[i] for i in SAND_DRY])
        surfaces[ue.FootstepSurface.SAND_DRY] = dry
        rules.set_editor_property("sounds", surfaces)
        save(rules)
        after = [path_of(s) for s in EAL.load_asset(STEPS).get_editor_property("sounds")[ue.FootstepSurface.SAND_DRY]
                 .get_editor_property("sounds")]
        if after != target:
            raise RuntimeError(f"Footstep readback failed: {after}")
        row["result"] = "bound (replaced the snow substitute)" if before else "bound (empty)"
        row["after"] = after
    else:
        row["result"] = "skipped: neither empty nor the listed snow substitute"
        report["skipped"].append(row)
        return
    report["bindings"].append(row)
    wet = rules.get_editor_property("sounds")[ue.FootstepSurface.SAND_WET]
    report["left_alone"].append({"asset": STEPS, "property": "sounds[SandWet].sounds", "reason": SAND_WET_REASON,
                                 "current": [path_of(s) for s in wet.get_editor_property("sounds")]})


def cue_summary(cues):
    return {str(event): ([path_of(s) for s in cue.get_editor_property("sounds")],
                         round(float(cue.get_editor_property("volume")), 3),
                         round(float(cue.get_editor_property("cooldown")), 3)) for event, cue in cues.items()}


def runtime_rules(tools, sounds, by_id, report):
    """Phase B: DA_ExpeditionAudioRules, only once the T-234 classes are in the editor build"""
    if not hasattr(ue, "ExpeditionAudioRules") or not hasattr(ue, "ExpeditionAudioCue"):
        report["pending"].append("DA_ExpeditionAudioRules: UExpeditionAudioRules is not in this editor build "
                                 "(lead's full build with T-234), run the action again after it")
        return False
    existing_sounds = {event: require(path) for event, path in EXISTING.items()}
    cues = {}
    for event, ids in EVENTS.items():
        cue = ue.ExpeditionAudioCue()
        cue.set_editor_property("sounds", [sounds[key] for key in ids])
        cue.set_editor_property("volume", by_id[ids[0]]["gain"])
        cue.set_editor_property("cooldown", 0.3)
        cues[event] = cue
    for event, sound in existing_sounds.items():
        cue = ue.ExpeditionAudioCue()
        cue.set_editor_property("sounds", [sound])
        cue.set_editor_property("volume", 0.3 if event == "Countdown" else 0.45)
        cue.set_editor_property("cooldown", 0.75 if event == "Countdown" else 0.35)
        cues[event] = cue
    rules = load(RULES_PATH)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.ExpeditionAudioRules)
        rules = tools.create_asset("DA_ExpeditionAudioRules", DEST, ue.ExpeditionAudioRules, factory)
        if not rules:
            raise RuntimeError("Could not create DA_ExpeditionAudioRules")
    wanted = cue_summary(cues)
    if cue_summary(rules.get_editor_property("cues")) != wanted:
        rules.set_editor_property("cues", cues)
        save(rules)
    readback = cue_summary(EAL.load_asset(RULES_PATH).get_editor_property("cues"))
    if readback != wanted:
        raise RuntimeError("Runtime cue readback failed")
    report["runtime_rules"] = RULES_PATH
    report["events"] = {event: value[0] for event, value in readback.items()}
    return True


def main():
    check_lock()
    check_previous_action()
    started = time.monotonic()
    manifest = json.loads((ROOT / "art/audio/expedition/manifest.json").read_text("utf-8"))
    for entry in manifest["sounds"]:
        if hashlib.sha256((ROOT / entry["file"]).read_bytes()).hexdigest() != entry["sha256"]:
            raise RuntimeError(f"Prepared WAV changed since the manifest: {entry['file']}")
    by_id = {entry["id"]: entry for entry in manifest["sounds"]}
    report = {"task": "T-234", "started_at": dt.datetime.now(dt.timezone.utc).isoformat(),
              "owner": os.environ["MONKE_LOCK_OWNER"], "imported": [], "imported_now": [], "restyled": [],
              "remaining": [], "mixing": {}, "bindings": [], "skipped": [], "left_alone": [], "backups": [],
              "pending": [], "runtime_rules": False, "phase_a_complete": False, "complete": False, "success": False,
              "pie_listened": False, "packaged_tested": False,
              "cook": "add +DirectoriesToAlwaysCook=(Path=\"/Game/MonkeIsland/Audio/Expedition\") to DefaultGame.ini"}
    writer = Writer(report)
    conflicts = sorted(p for p in writer.dirty if p.startswith(DEST))
    if conflicts:
        raise RuntimeError(f"Unsaved packages under {DEST} belong to another session: {conflicts}")
    tools = ue.AssetToolsHelpers.get_asset_tools()
    try:
        mixing = make_mixing(tools, report)
        sounds = {}
        for entry in manifest["sounds"]:
            if load(entry["asset"]) is None or EAL.get_metadata_tag(load(entry["asset"]), TAG) != entry["sha256"]:
                if time.monotonic() - started > IMPORT_BUDGET_S:
                    report["remaining"].append(entry["id"])
                    continue
                check_lock()
            sound, imported = import_wave(tools, entry)
            changed = style(sound, entry, mixing)
            duration = float(sound.get_editor_property("duration"))
            if abs(duration - entry["seconds"]) > 0.08:
                raise RuntimeError(f"Duration mismatch: {entry['id']} {duration} s, manifest {entry['seconds']} s")
            if imported or changed:
                save(sound)
            if imported:
                report["imported_now"].append(entry["id"])
            elif changed:
                report["restyled"].append(entry["id"])
            sounds[entry["id"]] = sound
            report["imported"].append({"id": entry["id"], "asset": entry["asset"], "wav_sha256": entry["sha256"],
                                       "seconds": round(duration, 4), "loop": bool(sound.get_editor_property("looping")),
                                       **describe(sound)})
        if report["remaining"]:
            ue.log_warning(f"T234_AUDIO_PARTIAL imported={len(report['imported'])} "
                           f"remaining={len(report['remaining'])}: run the action again")
            return

        for rules_path, prop, sound_id, provisional in BINDINGS:
            bind(writer, rules_path, prop, sounds[sound_id], provisional, report)
        bind_footsteps(writer, sounds, report)
        for rules_path, props, reason in LEFT_ALONE:
            rules = load(rules_path)
            report["left_alone"].append({"asset": rules_path, "reason": reason,
                                         "current": {p: value_of(rules, p) for p in props} if rules else "missing",
                                         "sounds_on_disk": sounds_on_disk(rules_path)})
        report["phase_a_complete"] = True
        report["success"] = True
        report["complete"] = runtime_rules(tools, sounds, by_id, report)
    except Exception as exc:
        report["success"] = False
        report["error"] = str(exc)
        raise
    finally:
        report["editor_seconds"] = round(time.monotonic() - started, 1)
        report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "import-report.json").write_text(json.dumps(report, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    ue.log(f"T234_AUDIO_PHASE_A_COMPLETE sounds={len(report['imported'])} bindings={len(report['bindings'])} "
           f"skipped={len(report['skipped'])} runtime_rules={bool(report['runtime_rules'])}; PIE listening still required")


if __name__ == "__main__":
    main()
