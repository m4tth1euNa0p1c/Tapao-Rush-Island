"""T-259: decodes the owner's new sounds of 2026-10-08 (OneDrive maquette-blender/audio, read only), as convert_music.py.

Blender (5.x, background, its aud module) decodes each MP3 with its FFmpeg, at the file's own rate and channels, into a
32-bit float WAV (no clipping of the decoder's overshoots, no resampling here): art/audio_src/owner/<stem>.wav, ignored
by Git (decoded copies of the OneDrive files). decode.json keeps the size and SHA-256 of each source and what aud read.
scripts/audio/owner_sounds_prepare.py then cuts, levels and resamples them to 48 kHz for Unreal.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/audio/convert_owner_sounds.py

The three songs already in the game (hawaiian, night, uk) stay with convert_music.py.
"""
import hashlib
import json
import sys
from pathlib import Path

import aud

SRC = Path(r"C:\Users\matth\OneDrive\maquette-blender\audio")
OUT = Path(__file__).resolve().parents[2] / "art" / "audio_src" / "owner"
FILES = ("galion_song.mp3", "barque_song.mp3", "epe_slash_1_song.mp3", "epe_slash_2_song.mp3", "epe_enemie_toucher.mp3")


def decode(source):
    """The whole file, at its own rate and channel count, as float (aud writes the WAV at once).

    galion_song.mp3 ends on a frame FFmpeg cannot fill ("filling the audio frame failed", whatever the output format):
    the decode is then retried without its last 30 ms (under two MP3 frames), and the cut is reported."""
    data = source.read_bytes()
    sound = aud.Sound(str(source))
    rate, channels = int(sound.specs[0]), int(sound.specs[1])
    target = OUT / f"{source.stem}.wav"
    layout = aud.CHANNELS_STEREO if channels == 2 else aud.CHANNELS_MONO
    seconds = sound.length / rate if sound.length > 0 else None
    dropped = 0.0
    try:
        sound.write(str(target), rate=rate, channels=layout, format=aud.FORMAT_FLOAT32, container=aud.CONTAINER_WAV,
                    codec=aud.CODEC_PCM)
    except aud.error:
        if not seconds:
            raise
        dropped = 0.03
        sound.limit(0.0, seconds - dropped).write(str(target), rate=rate, channels=layout, format=aud.FORMAT_FLOAT32,
                                                  container=aud.CONTAINER_WAV, codec=aud.CODEC_PCM)
    return {"source": source.name, "source_bytes": len(data), "source_sha256": hashlib.sha256(data).hexdigest(),
            "source_modified": source.stat().st_mtime, "wav": target.name, "rate": rate, "channels": channels,
            "seconds": round(seconds, 4) if seconds else None, "dropped_end_seconds": dropped,
            "wav_bytes": target.stat().st_size if target.exists() else 0}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = []
    for name in FILES:
        source = SRC / name
        if not source.exists():
            report.append({"source": name, "error": "missing"})
            continue
        report.append(decode(source))
        print("[owner sounds]", report[-1], flush=True)
    (OUT / "decode.json").write_text(json.dumps({"decoder": f"Blender aud {getattr(aud, 'version', '')}".strip(),
                                                 "files": report}, indent=2), encoding="utf-8")


main()
sys.exit(0)
