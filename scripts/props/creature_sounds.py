"""Voices of the tiki creatures: picks free creature sounds (rubberduck, "80 CC0 creature SFX" and "#2" on
OpenGameArt, CC0) and writes them as mono WAV to art/audio/creatures (credited in art/SOURCES.md).

  python scripts/props/creature_sounds.py
  then, editor closed: UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="scripts/props/import_creature_sounds.py"

Picks (by name, length, loudness and brightness, the low ones for the warriors):
- warriors (SW_Tiki_*): Emerge = roars out of the bush, Aggro = growls when they spot a player, Attack = efforts of
  the blow, Hurt, Death, Idle = grunts now and then near a player;
- villagers (SW_Villager_*): Hurt, Flee = screams when the village runs away, Death.
Each sound is trimmed of its silences, peak-normalised to -1 dB and given 10 ms fades.
"""
import io
import os
import urllib.request
import zipfile

import numpy as np
import soundfile as sf

PACKS = {
    "creature1": "https://opengameart.org/sites/default/files/80-CC0-creature-SFX_0.zip",
    "creature2": "https://opengameart.org/sites/default/files/80-CC0-creature-sfx-2.zip",
}
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
CACHE = os.path.join(ROOT, "art", "audio_src", "creatures")
OUT = os.path.join(ROOT, "art", "audio", "creatures")

PICKS = {
    "Tiki_Emerge": ["roar_02", "roar_04", "roar_05", "roar_06", "monster_04", "monster_07"],
    "Tiki_Aggro": ["monster_03", "monster_06", "monster_09", "monster_12", "monster_20", "troll_01"],
    "Tiki_Attack": ["grunt_02", "grunt_06", "grunt_07", "grunt_08", "grunt_09", "weird_05"],
    "Tiki_Hurt": ["hurt_03", "hurt_04", "hurt_05", "troll_02", "troll_03", "monster_15"],
    "Tiki_Death": ["die_01", "die_02", "die_03", "howl"],
    "Tiki_Idle": ["grunt_01", "grunt_03", "weird_07", "weird_08", "weird_09", "breath_02"],
    "Villager_Hurt": ["human_01", "human_04", "human_08", "hurt_01", "hurt_02"],
    "Villager_Flee": ["scream_01", "scream_02", "human_02", "human_03", "human_05"],
    "Villager_Death": ["human_06", "human_07", "die_04"],
}


def archives():
    os.makedirs(CACHE, exist_ok=True)
    found = {}
    for key, url in PACKS.items():
        path = os.path.join(CACHE, key + ".zip")
        if not os.path.exists(path):
            with urllib.request.urlopen(url) as response, open(path, "wb") as f:
                f.write(response.read())
        archive = zipfile.ZipFile(path)
        for name in archive.namelist():
            found[os.path.splitext(os.path.basename(name))[0]] = (archive, name)
    return found


def clean(data, rate):
    if data.ndim > 1:
        data = data.mean(axis=1)
    data = data.astype(np.float32)
    level = np.abs(data)
    loud = np.nonzero(level > max(level.max(), 1e-6) * 0.01)[0]  # -40 dB
    if len(loud):
        data = data[max(loud[0] - int(0.005 * rate), 0): loud[-1] + int(0.02 * rate)]
    fade = min(int(0.01 * rate), len(data) // 4)
    if fade > 1:
        ramp = np.linspace(0.0, 1.0, fade, dtype=np.float32)
        data[:fade] *= ramp
        data[-fade:] *= ramp[::-1]
    peak = float(np.max(np.abs(data))) or 1.0
    return data / peak * 0.89


def main():
    found = archives()
    os.makedirs(OUT, exist_ok=True)
    total = 0
    for event, names in PICKS.items():
        for i, name in enumerate(names):
            archive, member = found[name]
            data, rate = sf.read(io.BytesIO(archive.read(member)), dtype="float32")
            path = os.path.join(OUT, f"SW_{event}_{i + 1:02d}.wav")
            sf.write(path, clean(data, rate), rate, subtype="PCM_16")
            total += 1
    print(f"{total} sounds in {os.path.normpath(OUT)}")


if __name__ == "__main__":
    main()
