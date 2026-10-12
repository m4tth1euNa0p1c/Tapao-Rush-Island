"""T-259: the owner's new sounds of 2026-10-08 in Unreal (action "owneraudio" of the lead's editor session,
scripts/editor/lead_session_t186.py; editor open, lock held, no game running).

- imports art/audio/owner/*.wav (scripts/audio/convert_owner_sounds.py then owner_sounds_prepare.py):
    SW_Cutlass_Owner_Swing / _FinisherSwing / _Hit   -> /Game/MonkeIsland/Weapons/Cutlass/Audio, sound class SC_Effects
    SW_Music_Galleon / SW_Music_Boat (looping)       -> /Game/MonkeIsland/Audio/Music, sound class SC_Ambience (the
                                                        analysis heard ambiences: a ship's creaking, oar strokes)
  a WAV whose SHA-256 matches the asset's T259.SourceSha256 tag is not imported again;
- DA_CutlassRules: SwingSound <- epe_slash_1, FinisherSwingSound <- epe_slash_2, HitSound and KillSound <- epe_enemie_toucher
  (the old SW_Cutlass_* assets stay in the project; the previous references are in the report);
- DA_MusicRules: Galleon, Boat, GalleonVolume, BoatVolume once UMusicRules has them (lead's full build of T-259); before,
  they are reported as pending and the rest is done. Replays are idempotent.
Report: monke_island/Saved/T259/owner-sounds-setup.json (properties read back after saving).
"""
import datetime as dt
import hashlib
import json
import os
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "art/audio/owner"
OUT = ROOT / "monke_island/Saved/T259"
EAL = ue.EditorAssetLibrary
CLASSES = "/Game/MonkeIsland/Settings/SoundClasses"
CUTLASS_AUDIO = "/Game/MonkeIsland/Weapons/Cutlass/Audio"
CUTLASS_RULES = "/Game/MonkeIsland/Weapons/Cutlass/DA_CutlassRules"
MUSIC = "/Game/MonkeIsland/Audio/Music"
MUSIC_RULES = MUSIC + "/DA_MusicRules"
TAG = "T259.SourceSha256"
# asset -> (folder, sound class, looping)
SOUNDS = {
    "SW_Cutlass_Owner_Swing": (CUTLASS_AUDIO, "SC_Effects", False),
    "SW_Cutlass_Owner_FinisherSwing": (CUTLASS_AUDIO, "SC_Effects", False),
    "SW_Cutlass_Owner_Hit": (CUTLASS_AUDIO, "SC_Effects", False),
    "SW_Music_Galleon": (MUSIC, "SC_Ambience", True),
    "SW_Music_Boat": (MUSIC, "SC_Ambience", True),
}
CUTLASS = {"swing_sound": "SW_Cutlass_Owner_Swing", "finisher_swing_sound": "SW_Cutlass_Owner_FinisherSwing",
           "hit_sound": "SW_Cutlass_Owner_Hit", "kill_sound": "SW_Cutlass_Owner_Hit"}
MUSIC_SOUNDS = {"galleon": "SW_Music_Galleon", "boat": "SW_Music_Boat"}
MUSIC_VOLUMES = {"galleon_volume": 0.8, "boat_volume": 0.7}


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


def import_sound(tools, name, folder):
    """Imports the WAV unless the asset already holds it (same SHA-256); returns (sound, imported)"""
    wav = SRC / f"{name}.wav"
    assert wav.exists(), f"{wav} missing: run scripts/audio/owner_sounds_prepare.py"
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


def try_set(asset, prop, value, pending):
    try:
        asset.set_editor_property(prop, value)
        return True
    except Exception as error:  # noqa: BLE001 - property missing before the lead's build
        pending.append(f"{asset.get_name()}.{prop}: {error}")
        return False


def main():
    check_editor()
    tools = ue.AssetToolsHelpers.get_asset_tools()
    report = {"task": "T-259", "sounds": {}, "cutlass": {}, "music": {}, "pending": []}

    for name, (folder, class_name, looping) in SOUNDS.items():
        sound, imported = import_sound(tools, name, folder)
        sound_class = load(f"{CLASSES}/{class_name}")
        sound.set_editor_property("looping", looping)
        if sound_class:
            sound.set_editor_property("sound_class_object", sound_class)
        save(sound)
        report["sounds"][name] = {"path": sound.get_path_name(), "imported_now": imported,
                                  "seconds": round(sound.get_editor_property("duration"), 3),
                                  "channels": sound.get_editor_property("num_channels"),
                                  "looping": sound.get_editor_property("looping"),
                                  "sound_class": path_of(sound.get_editor_property("sound_class_object"))}

    rules = load(CUTLASS_RULES)
    assert rules, f"{CUTLASS_RULES} missing"
    for prop, name in CUTLASS.items():
        before = path_of(rules.get_editor_property(prop))
        rules.set_editor_property(prop, load(f"{CUTLASS_AUDIO}/{name}"))
        report["cutlass"][prop] = {"before": before}
    save(rules)
    rules = EAL.load_asset(CUTLASS_RULES)
    for prop in CUTLASS:
        report["cutlass"][prop]["now"] = path_of(rules.get_editor_property(prop))
    report["cutlass"]["sound_attenuation"] = path_of(rules.get_editor_property("sound_attenuation"))
    report["cutlass"]["sound_pitch_jitter"] = rules.get_editor_property("sound_pitch_jitter")

    music = load(MUSIC_RULES)
    assert music, f"{MUSIC_RULES} missing"
    changed = False
    for prop, name in MUSIC_SOUNDS.items():
        changed |= try_set(music, prop, load(f"{MUSIC}/{name}"), report["pending"])
    for prop, value in MUSIC_VOLUMES.items():
        changed |= try_set(music, prop, value, report["pending"])
    if changed:
        save(music)
    music = EAL.load_asset(MUSIC_RULES)
    for prop in list(MUSIC_SOUNDS) + list(MUSIC_VOLUMES) + ["hawaiian", "night", "crown"]:
        try:
            value = music.get_editor_property(prop)
            report["music"][prop] = path_of(value) if isinstance(value, ue.Object) or value is None else value
        except Exception:  # noqa: BLE001
            report["music"][prop] = "missing (UMusicRules not rebuilt yet)"

    report["music_ready"] = not report["pending"]
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "owner-sounds-setup.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    ue.log(f"T259_OWNER_SOUNDS_SETUP_SUCCESS music_ready={report['music_ready']} pending={len(report['pending'])}")


if __name__ == "__main__":
    main()
