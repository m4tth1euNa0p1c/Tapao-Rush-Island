"""T-259: checks the prepared WAVs against their manifests, without Unreal (proof of the offline part).

  python -I scripts/audio/t259_verify.py   ->  monke_island/Saved/T259/audio-verify.json

art/audio/weapons/v2 (manifest.json of weapons_v2_prepare.py) and art/audio/owner (owner_sounds_prepare.py): every WAV
listed is there, unchanged (SHA-256), WAV PCM16 48 kHz mono, finite, not silent, peak under -0.9 dBFS, one-shots start
and end on silence, loops join without a step; weapons sources all CC0. It does not prove the import, the playback in
Unreal or how the sounds feel: that is the PIE listening.
"""
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import soundfile as sf

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_dsp as dsp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
SETS = {"weapons_v2": ROOT / "art/audio/weapons/v2/manifest.json", "owner": ROOT / "art/audio/owner/manifest.json"}


def check(entry):
    path = ROOT / entry["file"]
    problems = []
    if not path.exists():
        return ["missing"]
    if dsp.sha256(path) != entry["sha256"]:
        problems.append("changed since the manifest")
    info = sf.info(str(path))
    audio, rate = sf.read(str(path), always_2d=True)
    if info.format != "WAV" or info.subtype != "PCM_16" or rate != 48000 or audio.shape[1] != 1:
        problems.append(f"format {info.format}/{info.subtype}/{rate}/{audio.shape[1]}ch")
    if not np.isfinite(audio).all() or np.sqrt(np.mean(audio ** 2)) < 1e-5:
        problems.append("silent or invalid")
    if np.max(np.abs(audio)) >= 10 ** (-0.9 / 20):
        problems.append("peak over -0.9 dBFS")
    if entry.get("loop"):
        seam = np.max(np.abs(audio[-1] - audio[0]))
        if seam > np.percentile(np.abs(np.diff(audio[:, 0])), 99.9) + 2 / 32768:
            problems.append("loop seam step")
    elif np.max(np.abs(audio[[0, -1]])) >= 4 / 32768:
        problems.append("one-shot does not start and end on silence")
    if "licenses" in entry and entry["licenses"] and entry["licenses"] != ["CC0-1.0"]:
        problems.append(f"licences {entry['licenses']}")
    return problems


def main():
    report = {"at": datetime.now(timezone.utc).isoformat(), "sets": {}, "passed": True,
              "not_proven": ["Unreal import (see weapons-audio-setup.json, owner-sounds-setup.json)", "listening in PIE",
                             "packaged game", "multiplayer"]}
    for name, manifest_path in SETS.items():
        sounds = json.loads(manifest_path.read_text("utf-8"))["sounds"]
        failures = {e["asset"]: check(e) for e in sounds}
        failures = {k: v for k, v in failures.items() if v}
        seconds = sum(e["seconds"] for e in sounds)
        report["sets"][name] = {"sounds": len(sounds), "seconds": round(seconds, 2), "failures": failures}
        report["passed"] &= not failures
        print(f"{name}: {len(sounds)} WAV, {seconds:.1f} s, {'PASS' if not failures else 'FAIL ' + json.dumps(failures)}")
    out = ROOT / "monke_island/Saved/T259"
    out.mkdir(parents=True, exist_ok=True)
    (out / "audio-verify.json").write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    sys.exit(0 if report["passed"] else 1)


if __name__ == "__main__":
    main()
