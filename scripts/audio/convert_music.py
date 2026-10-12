"""T-220: the music of the owner (OneDrive maquette-blender/audio, read only) in WAV for Unreal, which does not import MP3.

Blender (5.x, background, its aud module) decodes the MP3 with its FFmpeg and writes a 16-bit PCM WAV of the first SECONDS of each song,
with a short fade in and a fade out at the end so that the loop does not click. Unreal keeps the imported PCM inside
the asset: the long songs are cut (Hawaiian 74 min -> 8 min, night 130 min -> 4 min); the crown song (1 min 18) whole.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/audio/convert_music.py

Output: art/audio/music/MUS_*.wav (+ music.json: source, duration, cut).
"""
import json
import sys
from pathlib import Path

import aud

SRC = Path(r"C:\Users\matth\OneDrive\maquette-blender\audio")
OUT = Path(__file__).resolve().parents[2] / "art" / "audio" / "music"
FADE_IN = 1.0
FADE_OUT = 4.0
# source file -> (output name, seconds kept; 0 = whole)
SONGS = {"hawaiian_song.mp3": ("MUS_Hawaiian", 480.0), "night_song.mp3": ("MUS_Night", 240.0), "uk_song.mp3": ("MUS_Crown", 0.0)}


def convert(source, name, keep):
    """aud (Audaspace, in Blender) decodes the MP3 with FFmpeg and writes the WAV at once (the VSE mixdown is a job)"""
    sound = aud.Sound(str(source))
    length = sound.length / sound.specs[0] if sound.length > 0 else 0.0
    seconds = min(length, keep) if keep > 0 and length > 0 else (length if length > 0 else keep)
    cut = sound.limit(0.0, seconds).fadein(0.0, FADE_IN).fadeout(max(0.0, seconds - FADE_OUT), FADE_OUT)
    target = OUT / f"{name}.wav"
    cut.write(str(target), rate=44100, channels=aud.CHANNELS_STEREO, format=aud.FORMAT_S16, container=aud.CONTAINER_WAV,
              codec=aud.CODEC_PCM)
    return {"source": source.name, "wav": target.name, "source_minutes": round(length / 60.0, 1), "kept_seconds": round(seconds, 1),
            "bytes": target.stat().st_size if target.exists() else 0}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = []
    for file, (name, keep) in SONGS.items():
        source = SRC / file
        if not source.exists():
            report.append({"source": file, "error": "missing"})
            continue
        report.append(convert(source, name, keep))
        print("[music]", report[-1], flush=True)
    (OUT / "music.json").write_text(json.dumps(report, indent=2), encoding="utf-8")


main()
sys.exit(0)
