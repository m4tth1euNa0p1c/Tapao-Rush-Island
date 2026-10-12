"""T-333: the military bugle call of the trumpet call (key A) as a 48 kHz PCM16 mono Unreal source.

Source, public domain (licence read on its page on 2026-10-11): "Assembly", a bugle call of the U.S. Army
(http://www.music.army.mil/music/buglecalls/default.asp), on Wikimedia Commons:
https://commons.wikimedia.org/wiki/File:Assembly.mp3 -- {{PD-USGov-Military}}: a work of a U.S. military employee made
in the course of his official duties. The original stays in art/audio_src/trumpet_call/ (ignored by Git), checked here
by its SHA-256 (its SHA-1 matched the one Commons gives).

  python -I scripts/audio/trumpet_call_prepare.py [art/audio_src/trumpet_call/Assembly.mp3]
  ->  art/audio/trumpet_call/SW_Trumpet_Call.wav and manifest.json (imported by scripts/editor/trumpet_call_ue.py)

Processing: decoded, mono, 48 kHz, DC and rumble taken out (high-pass); it starts on the first note (the silence before
it is cut) and lasts the play loop of the call (10 s minus the equip and the put away of the trumpet, 0.792 s each:
8.416 s; cut at 8.4 s), with a short fade in and a fade out over the decay of the last note; its loudness is matched to
the fanfare of the ceremony (SW_Trumpet_Fanfare, the other sound of the trumpet) with the peak kept under -1 dBFS.
Deterministic.
"""
from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import audio_dsp as dsp  # noqa: E402

ROOT = HERE.parents[1]
SOURCE = ROOT / "art/audio_src/trumpet_call/Assembly.mp3"
SOURCE_SHA256 = "c19141b49d1ab4e300f9d87bb7f31f8007b699cb533c89db99ad9932c32598f6"
REFERENCE = ROOT / "art/audio/emotes/SW_Trumpet_Fanfare.wav"
OUT = ROOT / "art/audio/trumpet_call"
NAME = "SW_Trumpet_Call"

# the play loop of the call (DA_TrumpetEmoteRules: CallSeconds 10, EquipSeconds and UnequipSeconds 0.792)
LOOP_SECONDS = 10.0 - 2 * 0.792
LENGTH_SECONDS = 8.4
ONSET_DB = -35.0            # first 5 ms window this close to the peak: the first note
PRE_ROLL_SECONDS = 0.01     # kept before it
FADE_IN_SECONDS = 0.008
FADE_OUT_SECONDS = 0.45     # over the decay of the last note (it is held until ~7.95 s)
HIGH_PASS_HZ = 70.0
PEAK_CEILING_DBFS = -1.0

CREDIT = {
    "title": "Assembly (bugle call)",
    "author": "U.S. Army",
    "page": "https://commons.wikimedia.org/wiki/File:Assembly.mp3",
    "file": "https://upload.wikimedia.org/wikipedia/commons/d/d3/Assembly.mp3",
    "original_source": "http://www.music.army.mil/music/buglecalls/default.asp",
    "license": "Public domain ({{PD-USGov-Military}}: work of a U.S. military employee, official duties)",
    "license_checked": "2026-10-11",
}


def main(argv):
    source = Path(argv[1]) if len(argv) > 1 else SOURCE
    digest = dsp.sha256(source)
    if digest != SOURCE_SHA256:
        raise SystemExit(f"{source}: SHA-256 {digest} is not the one of the Commons file ({SOURCE_SHA256})")

    audio = dsp.mono(dsp.load(source))
    audio = dsp.butter_filter(audio, "highpass", HIGH_PASS_HZ, order=2, zero_phase=True)

    # from the first note
    env, window = dsp.envelope(audio, 0.005)
    loud = np.where(dsp.db(env / max(env.max(), 1e-12)) > ONSET_DB)[0]
    if not len(loud):
        raise SystemExit("no note found in the source")
    start = max(0, dsp.samples(loud[0] * window - PRE_ROLL_SECONDS))
    length = dsp.samples(LENGTH_SECONDS)
    if start + length > len(audio):
        raise SystemExit(f"the source is too short: {dsp.seconds(len(audio) - start):.2f} s after the first note")
    call = dsp.fade(audio[start:start + length], FADE_IN_SECONDS, FADE_OUT_SECONDS)

    # as loud as the fanfare of the ceremony, never above the ceiling
    reference = dsp.load(REFERENCE)
    gain = dsp.gain_db(dsp.loudness(reference) - dsp.loudness(call))
    ceiling = dsp.gain_db(PEAK_CEILING_DBFS) / max(np.max(np.abs(call)), 1e-12)
    gain = min(gain, ceiling)
    call = call * gain

    OUT.mkdir(parents=True, exist_ok=True)
    wav = OUT / f"{NAME}.wav"
    decoded = dsp.write_pcm16(wav, call)
    measures = dsp.measures(decoded)
    manifest = {
        "task": "T-333",
        "made": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "sound": f"{NAME}.wav",
        "credit": CREDIT,
        "source_sha256": digest,
        "output_sha256": dsp.sha256(wav),
        "cut": {"start_s": round(dsp.seconds(start), 4), "length_s": LENGTH_SECONDS, "loop_of_the_call_s": round(LOOP_SECONDS, 4),
                "fade_in_s": FADE_IN_SECONDS, "fade_out_s": FADE_OUT_SECONDS, "high_pass_hz": HIGH_PASS_HZ},
        "gain_db": round(float(dsp.db(gain)), 2),
        "reference": REFERENCE.name,
        "reference_loudness_lufs": round(dsp.loudness(reference), 2),
        "measures": measures,
    }
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps(manifest, indent=2, ensure_ascii=False))


if __name__ == "__main__":
    main(sys.argv)
