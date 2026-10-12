"""T-259: prepares the owner's new sounds (decoded by convert_owner_sounds.py) as 48 kHz PCM16 Unreal sources.

  python scripts/audio/owner_sounds_prepare.py      ->  art/audio/owner/*.wav + manifest.json

What the analysis found (spectrograms and envelopes, 2026-10-09) and what is done:
  galion_song.mp3   5.0 s, dual mono: a low rumble (57 % of the energy under 120 Hz) and dull wooden knocks every
                    0.3 to 1.2 s, nothing above 2 kHz: the creaking of a ship, an AMBIENCE, not a song. A 5 s loop
                    would be heard repeating, so SW_Music_Galleon is a 64 s bed made only of this recording: pieces of
                    1.4 to 3 s cut where the knocks are silent, chained in a seeded random order, joined by equal-power
                    crossfades in the rumble, the end flowing into the start.
  barque_song.mp3   49.2 s mono: 20 oar strokes in the water, one every 2.45 s (splash 200 Hz-10 kHz, silence between):
                    a ROWING ambience. SW_Music_Boat keeps the recording from the gap before its first stroke to the
                    gap before its last, so the loop restarts in a silent gap and keeps the stroke rhythm.
  epe_slash_1_song  0.8 s whoosh peaking 0.37 s in: SW_Cutlass_Owner_Swing starts 0.20 s before its peak (the combo
                    blows cross the front of the bean 0.14-0.32 s after they start, DA_CutlassRules).
  epe_slash_2_song  0.9 s whoosh peaking 0.33 s in: SW_Cutlass_Owner_FinisherSwing is delayed so its peak falls 0.53 s
                    after the finisher starts (its blade is active 0.49-0.62 s, hit at 0.57 s).
  epe_enemie_toucher 2.2 s: a 0.12 s bright swish, then the impact (low thump from about 0.17 s) and its decay:
                    SW_Cutlass_Owner_Hit starts 25 ms before the impact (the swing sound already gives the swish).
All: mono (the sword sounds are spatialised at the blade), 48 kHz, raised-cosine fades, peaks under -1 dBFS; the loops
at about -20 LUFS. Volumes in game: DA_MusicRules (GalleonVolume, BoatVolume) and the sound classes.
The OneDrive originals are untouched; their provenance and licence are the owner's (to confirm before publishing).
"""
from __future__ import annotations

import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_dsp as dsp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "art/audio_src/owner"
OUT = ROOT / "art/audio/owner"
SEED = 259

# cutlass timing (DA_CutlassRules defaults, UCutlassRules constructor): where the whoosh peak must fall
SWING_PEAK_S = 0.20
FINISHER_PEAK_S = 0.53
HIT_LEAD_S = 0.025
GALLEON_SECONDS = 64.0
LOOP_LUFS = -20.0


def source_entry(decoded, name):
    entry = next(f for f in decoded["files"] if f.get("source") == name)
    return {"source_file": f"OneDrive/maquette-blender/audio/{name}", "source_sha256": entry["source_sha256"],
            "source_bytes": entry["source_bytes"], "decoded_wav": f"art/audio_src/owner/{entry['wav']}",
            "decoded_sha256": dsp.sha256(RAW / entry["wav"])}


def envelope_peak_time(signal):
    env, window = dsp.envelope(signal, 0.01)
    return (int(np.argmax(env)) + 0.5) * window


def tail_end(signal, threshold_db=-60.0, keep_s=0.02, hold_s=0.25):
    """End of the decay: the first time after the peak from which the level stays threshold_db under the peak for
    hold_s (later noise of the recording is dropped), plus keep_s"""
    env, window = dsp.envelope(signal, 0.005)
    level = dsp.db(env / env.max())
    hold = max(1, int(hold_s / window))
    for i in range(int(np.argmax(env)), len(level)):
        if np.all(level[i:i + hold] <= threshold_db):
            return min(len(signal), dsp.samples(i * window + keep_s))
    return len(signal)


def swing(signal, peak_at_s):
    """The whoosh with its peak peak_at_s after the start: head cut, or silence put before it"""
    peak = envelope_peak_time(signal)
    shift = peak - peak_at_s
    if shift >= 0:
        out = signal[dsp.samples(shift):]
        cut = {"head_cut_s": round(shift, 3)}
    else:
        out = np.concatenate((np.zeros(dsp.samples(-shift)), signal))
        cut = {"silence_added_s": round(-shift, 3)}
    out = out[:tail_end(out)]
    out = dsp.fade(out, 0.005, 0.03)
    return out, {**cut, "source_peak_s": round(peak, 3), "peak_at_s": peak_at_s}


def hit(signal):
    """Starts HIT_LEAD_S before the impact: the first 5 ms window where the low band (60-250 Hz) comes within 12 dB of
    its maximum"""
    low = dsp.butter_filter(signal, "bandpass", (60.0, 250.0), 4, zero_phase=True)
    env, window = dsp.envelope(low, 0.005)
    impact = int(np.argmax(dsp.db(env / env.max()) > -12.0)) * window
    start = max(0.0, impact - HIT_LEAD_S)
    out = signal[dsp.samples(start):]
    # the recording's floor (a low hum) is about 50 dB under the impact: the decay ends there
    out = out[:tail_end(out, -45.0, 0.03)]
    out = dsp.fade(out, 0.003, 0.06)
    return out, {"impact_s": round(impact, 3), "head_cut_s": round(start, 3), "tail_threshold_db": -45.0}


def quiet_points(signal, window_s=0.01):
    """Times (s) where the knocks are silent: windows under 1.25 x the median level, away from loud windows"""
    env, window = dsp.envelope(signal, window_s)
    floor = np.median(env)
    loud = env > floor * 2.5
    near_loud = np.convolve(loud.astype(float), np.ones(9), mode="same") > 0  # +-40 ms around a knock
    quiet = (env < floor * 1.25) & ~near_loud
    return np.where(quiet)[0] * window + window / 2


def galleon_bed(signal, rng):
    """Seeded shuffle of pieces cut at quiet points, joined by equal-power crossfades, looping"""
    points = quiet_points(signal)
    length = dsp.seconds(len(signal))
    points = points[(points > 0.08) & (points < length - 0.08)]
    crossfade = 0.15
    pieces, used, total, last = [], [], 0.0, None
    while total < GALLEON_SECONDS + crossfade:
        for _ in range(200):
            a = float(rng.choice(points))
            ends = points[(points >= a + 1.4) & (points <= a + 3.0)]
            if len(ends) and (last is None or abs(a - last) > 0.6):
                b = float(rng.choice(ends))
                break
        else:
            raise ValueError("No quiet cut points in the galleon recording")
        piece = signal[dsp.samples(a):dsp.samples(b)] * dsp.gain_db(float(rng.uniform(-1.0, 1.0)))
        pieces.append(piece)
        used.append([round(a, 3), round(b, 3)])
        total += b - a - crossfade
        last = a
    bed = dsp.equal_power_concat(pieces, crossfade)
    bed = dsp.seam_loop(bed, crossfade)
    return bed, {"pieces": len(pieces), "piece_ranges_s": used, "crossfade_s": crossfade, "quiet_points": int(len(points))}


def silent_gaps(signal, below_db=30.0, min_s=0.25):
    """(start, end) of the runs at least min_s long whose level (20 ms windows smoothed over 200 ms) stays below_db
    under the loudest"""
    env, window = dsp.envelope(signal, 0.02)
    level = dsp.db(np.convolve(env, np.ones(10) / 10, mode="same"))
    quiet = level < level.max() - below_db
    gaps, i = [], 0
    while i < len(quiet):
        if not quiet[i]:
            i += 1
            continue
        j = i
        while j < len(quiet) and quiet[j]:
            j += 1
        if (j - i) * window >= min_s:
            gaps.append((i * window, j * window))
        i = j
    return gaps


def boat_loop(signal):
    """From the middle of the first silent gap between two strokes to the middle of the last one: the loop restarts in
    a gap (two half gaps make one), so the stroke rhythm goes on through the seam"""
    gaps = silent_gaps(signal)
    if len(gaps) < 4:
        raise ValueError("Cannot find the gaps between the oar strokes; manual edit required")
    middles = np.array([(a + b) / 2 for a, b in gaps])
    spacing = np.diff(middles)
    crossfade = 0.2
    start, end = middles[0] - crossfade / 2, middles[-1] + crossfade / 2
    loop = dsp.seam_loop(signal[dsp.samples(start):dsp.samples(end)], crossfade)
    seam_gap = (gaps[-1][1] - gaps[-1][0]) / 2 + (gaps[0][1] - gaps[0][0]) / 2
    return loop, {"gaps": len(gaps), "stroke_period_s": round(float(np.median(spacing)), 3),
                  "gap_spacing_range_s": [round(float(spacing.min()), 3), round(float(spacing.max()), 3)],
                  "cut_s": [round(float(start), 3), round(float(end), 3)], "seam_gap_s": round(float(seam_gap), 3),
                  "crossfade_s": crossfade}


def level_loop(signal):
    """To LOOP_LUFS, never above -1 dBFS"""
    gain = LOOP_LUFS - dsp.loudness(signal)
    peak_room = -1.0 - float(dsp.db(np.max(np.abs(signal))))
    applied = min(gain, peak_room)
    return signal * dsp.gain_db(applied), round(applied, 2)


def main():
    decoded = json.loads((RAW / "decode.json").read_text("utf-8"))
    OUT.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(SEED)
    entries = []

    def write(asset, signal, source, role, unreal_path, loop=False, processing=None, note=""):
        path = OUT / f"{asset}.wav"
        check = dsp.write_pcm16(path, signal)
        measured = dsp.measures(check, loop)
        entry = {"asset": asset, "file": path.relative_to(ROOT).as_posix(), "unreal": f"{unreal_path}/{asset}", "role": role,
                 **source_entry(decoded, source), "sha256": dsp.sha256(path), **measured, "loop": loop,
                 "processing": processing or {}, "note": note}
        assert measured["peak_dbfs"] < -0.9, asset
        assert loop or measured["ends_silent"], asset
        assert not loop or measured["loop_seam_ok"], asset
        entries.append(entry)
        print(f"{asset}: {measured['seconds']} s, peak {measured['peak_dbfs']} dBFS, {measured['loudness_lufs']} LUFS"
              f"{', seam ok' if loop else ''}  {processing}")

    cutlass = "/Game/MonkeIsland/Weapons/Cutlass/Audio"
    music = "/Game/MonkeIsland/Audio/Music"

    slash1 = dsp.mono(dsp.load(RAW / "epe_slash_1_song.wav"))
    out, info = swing(slash1, SWING_PEAK_S)
    write("SW_Cutlass_Owner_Swing", dsp.normalize_peak(out, -3.0), "epe_slash_1_song.mp3", "DA_CutlassRules.SwingSound",
          cutlass, processing={**info, "peak_dbfs": -3.0})

    slash2 = dsp.mono(dsp.load(RAW / "epe_slash_2_song.wav"))
    out, info = swing(slash2, FINISHER_PEAK_S)
    write("SW_Cutlass_Owner_FinisherSwing", dsp.normalize_peak(out, -2.5), "epe_slash_2_song.mp3",
          "DA_CutlassRules.FinisherSwingSound", cutlass, processing={**info, "peak_dbfs": -2.5})

    toucher = dsp.mono(dsp.load(RAW / "epe_enemie_toucher.wav"))
    out, info = hit(toucher)
    write("SW_Cutlass_Owner_Hit", dsp.normalize_peak(out, -1.5), "epe_enemie_toucher.mp3",
          "DA_CutlassRules.HitSound and KillSound", cutlass, processing={**info, "peak_dbfs": -1.5},
          note="KillSound reprend ce son (aucun son de mise à mort fourni) ; l'ancien SW_Cutlass_Kill reste dans le projet.")

    galleon = dsp.mono(dsp.load(RAW / "galion_song.wav"))
    bed, info = galleon_bed(galleon, rng)
    bed, applied = level_loop(bed)
    write("SW_Music_Galleon", bed, "galion_song.mp3", "DA_MusicRules.Galleon (à bord du galion)", music, loop=True,
          processing={**info, "gain_db": applied, "target_lufs": LOOP_LUFS, "seed": SEED},
          note="Ambiance de navire (craquements, coups sourds, grondement) ; classe SC_Ambience.")

    boat = dsp.mono(dsp.load(RAW / "barque_song.wav"))
    loop, info = boat_loop(boat)
    loop, applied = level_loop(loop)
    write("SW_Music_Boat", loop, "barque_song.mp3", "DA_MusicRules.Boat (assis dans la barque)", music, loop=True,
          processing={**info, "gain_db": applied, "target_lufs": LOOP_LUFS},
          note="Coups de rame dans l'eau, un toutes les 2,45 s ; classe SC_Ambience.")

    manifest = {"schema": 1, "task": "T-259", "generated_at": datetime.now(timezone.utc).isoformat(),
                "decoder": decoded.get("decoder"), "licence": "fichiers du propriétaire (OneDrive), provenance et licence "
                "à confirmer par le propriétaire avant toute publication", "validation": "decoded WAV data only; "
                "Unreal import by scripts/audio/owner_sounds_setup_ue.py, listening in PIE by the owner", "sounds": entries}
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{len(entries)} WAV in {OUT.relative_to(ROOT).as_posix()} (48 kHz PCM16 mono)")


if __name__ == "__main__":
    main()
