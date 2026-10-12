"""T-220: the music in Unreal (owner request 2026-10-07).

- imports art/audio/music/MUS_*.wav (scripts/audio/convert_music.py) as SW_Music_Hawaiian / _Night / _Crown in
  /Game/MonkeIsland/Audio/Music: the Hawaiian and the night songs loop, all three in the Music sound class of the
  settings (SC_Music: the music volume of the options);
- DA_MusicRules (UMusicRules) names them; UMusicSubsystem plays them (night, boat, flag).
Runs in the editor: scripts/editor/lead_session_t186.py, action "music". Replays replace the sounds.
Result: monke_island/Saved/T220/music-setup.json
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

SRC = ROOT / "art/audio/music"
OUT = ROOT / "monke_island/Saved/T220"
DEST = "/Game/MonkeIsland/Audio/Music"
RULES = DEST + "/DA_MusicRules"
SOUND_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Music"
# wav -> (asset, loops, rules property)
SONGS = {"MUS_Hawaiian.wav": ("SW_Music_Hawaiian", True, "hawaiian"), "MUS_Night.wav": ("SW_Music_Night", True, "night"),
         "MUS_Crown.wav": ("SW_Music_Crown", False, "crown")}


def main():
    kitlib.check_lock()
    tools = ue.AssetToolsHelpers.get_asset_tools()
    tasks = []
    for wav, (asset, _, _) in SONGS.items():
        source = SRC / wav
        assert source.exists(), f"{source} missing: run scripts/audio/convert_music.py"
        task = ue.AssetImportTask()
        task.filename = str(source)
        task.destination_path = DEST
        task.destination_name = asset
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    tools.import_asset_tasks(tasks)
    sound_class = kitlib.load(SOUND_CLASS)
    rules = kitlib.load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.MusicRules)
        rules = tools.create_asset("DA_MusicRules", DEST, ue.MusicRules, factory)
    assert rules, RULES
    report = {}
    for wav, (asset, loops, prop) in SONGS.items():
        sound = kitlib.load(f"{DEST}/{asset}")
        assert sound, f"{asset} did not import"
        sound.set_editor_property("looping", loops)
        if sound_class:
            sound.set_editor_property("sound_class_object", sound_class)
        kitlib.save(sound)
        rules.set_editor_property(prop, sound)
        report[asset] = {"seconds": round(sound.get_editor_property("duration"), 1), "loops": loops}
    kitlib.save(rules)
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "music-setup.json").write_text(json.dumps({"rules": RULES, "sounds": report, "sound_class": bool(sound_class),
        "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}, indent=2), encoding="utf-8")
    ue.log("T220_MUSIC_SETUP_SUCCESS")


if __name__ == "__main__":
    main()
