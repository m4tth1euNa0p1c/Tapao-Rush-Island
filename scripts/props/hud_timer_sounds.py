"""Generate the three short sounds of the flood timer HUD (DA_HudTimerRules, T-073 / cahier §19).

Made by script for the project (no external source, no licence to credit): 44.1 kHz, mono, 16 bit PCM WAV,
peak normalised to about -6 dBFS, soft attack / release envelopes (no click).

  S_HudTimer_Alert        double clear beep around 880 Hz, ~0.35 s   (60 / 30 / 10 s alerts)
  S_HudTimer_Evacuation   low rising horn 180 -> 260 Hz with harmonics, ~1.2 s   (start of the submersion)
  S_HudTimer_ZoneSecured  small chime of 3 rising notes, ~0.6 s   (zone bonus +2 min)

Usage: python scripts/props/hud_timer_sounds.py [output dir]   (default: art/audio/ui)
Then import the WAVs into /Game/MonkeIsland/UI/Sounds/ (SoundWave).
"""

import sys
import wave
from pathlib import Path

import numpy as np

RATE = 44100
PEAK_DBFS = -6.0


def envelope(n, attack_s, release_s):
    """Raised-cosine attack and release over n samples."""
    env = np.ones(n)
    a = max(1, int(attack_s * RATE))
    r = max(1, int(release_s * RATE))
    env[:a] = 0.5 - 0.5 * np.cos(np.linspace(0, np.pi, a))
    env[-r:] = 0.5 + 0.5 * np.cos(np.linspace(0, np.pi, r))
    return env


def tone(freq, dur, harmonics=((1, 1.0),), attack=0.005, release=0.03, decay=0.0):
    """Sum of harmonics of a fixed frequency, optional exponential decay (per second)."""
    t = np.arange(int(dur * RATE)) / RATE
    sig = sum(amp * np.sin(2 * np.pi * freq * k * t) for k, amp in harmonics)
    if decay:
        sig = sig * np.exp(-decay * t)
    return sig * envelope(len(t), attack, release)


def alert():
    """Two 880 Hz beeps (a touch of the octave for clarity), 0.12 s each, 0.07 s apart."""
    beep = tone(880.0, 0.12, ((1, 1.0), (2, 0.18), (3, 0.06)), attack=0.006, release=0.04)
    gap = np.zeros(int(0.07 * RATE))
    tail = np.zeros(int(0.04 * RATE))
    return np.concatenate([beep, gap, beep, tail])


def evacuation():
    """Low horn sweeping 180 -> 260 Hz over 1.2 s, rich in harmonics, slight vibrato."""
    dur = 1.2
    t = np.arange(int(dur * RATE)) / RATE
    # smooth rise (ease in-out) plus a light 5 Hz vibrato
    rise = 0.5 - 0.5 * np.cos(np.pi * np.clip(t / (dur * 0.85), 0, 1))
    freq = 180.0 + 80.0 * rise + 2.0 * np.sin(2 * np.pi * 5.0 * t)
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    harmonics = ((1, 1.0), (2, 0.55), (3, 0.38), (4, 0.22), (5, 0.12), (6, 0.07))
    sig = sum(amp * np.sin(k * phase) for k, amp in harmonics)
    return sig * envelope(len(t), 0.08, 0.18)


def zone_secured():
    """Three rising bell notes (C6, E6, G6), struck every 0.13 s, ringing out to ~0.6 s."""
    notes = (1046.5, 1318.5, 1568.0)
    total = int(0.6 * RATE)
    out = np.zeros(total)
    for i, f in enumerate(notes):
        start = int(i * 0.13 * RATE)
        dur = (total - start) / RATE
        bell = tone(f, dur, ((1, 1.0), (2.01, 0.3), (3.02, 0.1)), attack=0.003, release=0.05, decay=7.0)
        out[start:start + len(bell)] += bell
    return out


def normalise(sig):
    peak = np.max(np.abs(sig))
    return sig * (10 ** (PEAK_DBFS / 20) / peak) if peak > 0 else sig


def write_wav(path, sig):
    data = np.round(np.clip(normalise(sig), -1, 1) * 32767).astype("<i2")
    with wave.open(str(path), "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(data.tobytes())
    return len(data) / RATE


def main():
    root = Path(__file__).resolve().parents[2]
    out_dir = Path(sys.argv[1]) if len(sys.argv) > 1 else root / "art" / "audio" / "ui"
    out_dir.mkdir(parents=True, exist_ok=True)
    for name, make in (("S_HudTimer_Alert", alert), ("S_HudTimer_Evacuation", evacuation),
                       ("S_HudTimer_ZoneSecured", zone_secured)):
        path = out_dir / f"{name}.wav"
        dur = write_wav(path, make())
        print(f"{path}  {dur:.2f} s")


if __name__ == "__main__":
    main()
