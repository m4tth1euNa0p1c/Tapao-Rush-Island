"""Synthesises the drum roll of the execution scene (owner request 2026-10-07: the crew that missed the quota is put on a
plank and shot by French soldiers; AExecutionScene, DA_ExecutionRules.DrumSound), on the scheme of boat_sounds.py: no
recorded asset, so no licence to track.

  python scripts/props/execution_sounds.py  ->  art/audio/execution/SW_Execution_DrumRoll.wav (48 kHz mono 16 bit)
Deterministic (fixed seed). Imported by scripts/editor/execution_setup_ue.py.
  SW_Execution_DrumRoll - a military snare roll swelling for 7 s (the squad comes on deck, shoulders, waits for the
                          order); the scene cuts it at the volley
"""
import os
import wave
import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "execution")
rng = np.random.default_rng(1789)


def band_noise(n, low, high):
    """White noise kept between low and high (Hz), smooth edges"""
    freqs = np.fft.rfftfreq(n, 1.0 / RATE)
    gain = 1.0 / (1.0 + (low / np.maximum(freqs, 1.0)) ** 4) / (1.0 + (freqs / high) ** 4)
    spectrum = gain * np.exp(2j * np.pi * rng.random(len(freqs)))
    x = np.fft.irfft(spectrum, n)
    return x / (np.max(np.abs(x)) + 1e-9)


def stroke(strength):
    """One stroke of the stick: the skin's thump and the rattle of the snares under it"""
    k = int(0.16 * RATE)
    t = np.arange(k) / RATE
    tone = rng.uniform(185.0, 215.0)
    skin = np.sin(2 * np.pi * np.cumsum(tone * (1.0 + 0.35 * np.exp(-t / 0.006))) / RATE) * np.exp(-t / 0.035)
    snares = band_noise(k, 1800.0, 9000.0) * np.exp(-t / 0.055)
    crack = band_noise(k, 600.0, 4000.0) * np.exp(-t / 0.008)
    return strength * (0.45 * skin + 0.65 * snares + 0.35 * crack)


def roll(seconds=7.0):
    n = int(seconds * RATE)
    x = np.zeros(n)
    t = 0.0
    hand = 0
    while t < seconds - 0.2:
        # a closed roll: two strokes a hand, the hands alternating, a little faster as it swells
        progress = t / seconds
        rate = 15.0 + 5.0 * progress
        swell = 0.32 + 0.68 * progress ** 1.6
        strength = swell * (1.0 if hand % 2 == 0 else 0.82) * rng.uniform(0.9, 1.1)
        s = stroke(strength)
        i = int(t * RATE)
        k = min(len(s), n - i)
        x[i:i + k] += s[:k]
        t += 1.0 / rate + rng.normal(0.0, 0.0025)
        hand += 1
    # a low drum under it, on the slow beat of a march
    for beat in np.arange(0.0, seconds - 0.4, 1.0):
        k = int(0.5 * RATE)
        tt = np.arange(k) / RATE
        boom = np.sin(2 * np.pi * np.cumsum(62.0 * (1.0 + 0.5 * np.exp(-tt / 0.02))) / RATE) * np.exp(-tt / 0.18)
        i = int(beat * RATE)
        k2 = min(k, n - i)
        x[i:i + k2] += 0.55 * (0.4 + 0.6 * beat / seconds) * boom[:k2]
    fade = int(0.05 * RATE)
    x[-fade:] *= np.linspace(1.0, 0.0, fade)
    return x / (np.max(np.abs(x)) + 1e-9) * 0.89


def save(name, x):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype(np.int16).tobytes())
    print(path)


if __name__ == "__main__":
    save("SW_Execution_DrumRoll", roll())
