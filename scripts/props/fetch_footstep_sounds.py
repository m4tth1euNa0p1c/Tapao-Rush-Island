"""Fetches the free footsteps and impacts of Kenney's "Impact Sounds" (CC0, https://kenney.nl/assets/impact-sounds)
and writes them over the synthesised ones where a recorded surface exists (credited in art/SOURCES.md).

  python scripts/props/footstep_sounds.py        # synthesised set first (wet sand, water, coral have no recording)
  python scripts/props/coconut_sounds.py
  python scripts/props/fetch_footstep_sounds.py  # then this one
Recorded -> surface: wood -> Wood (boat, buildings), concrete -> Stone, grass -> Litter (fronds), snow -> SandDry
(the same soft squeaky crunch of packed grains). Coconut: impactSoft_heavy (a heavy dull thud) mixed with the
synthesised sand spray of coconut_sounds.py.
"""
import io
import os
import urllib.request
import zipfile
import numpy as np
import soundfile as sf

URL = "https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip"
ART = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio")

STEPS = {"Wood": "footstep_wood", "Stone": "footstep_concrete", "Litter": "footstep_grass", "SandDry": "footstep_snow"}


def read(archive, name):
    data, rate = sf.read(io.BytesIO(archive.read(f"Audio/{name}.ogg")), dtype="float32")
    if data.ndim > 1:
        data = data.mean(axis=1)
    return data, rate


def write(path, data, rate):
    peak = float(np.max(np.abs(data))) or 1.0
    sf.write(path, data / peak * 0.9, rate, subtype="PCM_16")
    print(path)


def resample(data, rate, target):
    if rate == target:
        return data
    n = int(round(len(data) * target / rate))
    return np.interp(np.linspace(0, len(data) - 1, n), np.arange(len(data)), data).astype(np.float32)


def main():
    with urllib.request.urlopen(URL) as response:
        archive = zipfile.ZipFile(io.BytesIO(response.read()))
    for surface, family in STEPS.items():
        for i in range(4):
            data, rate = read(archive, f"{family}_{i:03d}")
            write(os.path.join(ART, "footsteps", f"SW_Step_{surface}_{i + 1:02d}.wav"), data, rate)
    # coconut: the recorded thud, with the sand thrown up from the synthesised landing underneath
    for i in range(3):
        thud, rate = read(archive, f"impactSoft_heavy_{i:03d}")
        synth_path = os.path.join(ART, "hazards", f"SW_Coconut_Sand_{i + 1:02d}.wav")
        synth, synth_rate = sf.read(synth_path, dtype="float32")
        synth = resample(synth, synth_rate, rate)
        n = max(len(thud), len(synth))
        mix = np.zeros(n, dtype=np.float32)
        mix[:len(thud)] += thud / (np.max(np.abs(thud)) or 1.0)
        mix[:len(synth)] += 0.45 * synth / (np.max(np.abs(synth)) or 1.0)
        write(synth_path, mix, rate)


if __name__ == "__main__":
    main()
