"""Synthesises the herald trumpet fanfare of the bean (emote V, maquette bean-trumpet-anim), no recorded asset.

  python scripts/props/trumpet_sounds.py  ->  art/audio/emotes/SW_Trumpet_Fanfare.wav (48 kHz mono 16 bit)
The rising arpeggio of the maquette (trumpet-anims.json fanfare_notes), timed on the clip A_Bean_Trumpet_Fanfare
(t = 0 at the start of the clip): G4, C5, E5 short (0.2 s), then G5 held 1.35 s, bell raised.
A brass tone: odd and even harmonics with a bright spectrum that opens with the breath, a lip attack (short
pitch scoop and noise burst), a slow vibrato on the held note, and a little room reverb.
Deterministic (fixed seed).
"""
import os
import wave

import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "emotes")
NOTES = [("G4", 0.583, 0.2), ("C5", 0.833, 0.2), ("E5", 1.083, 0.2), ("G5", 1.333, 1.35)]
FREQ = {"G4": 392.00, "C5": 523.25, "E5": 659.25, "G5": 783.99}
rng = np.random.default_rng(2099)


def note(freq, dur, held):
    n = int((dur + 0.12) * RATE)
    t = np.arange(n) / RATE
    # lip attack: the pitch scoops up a few cents, then settles; slow vibrato on the held note
    scoop = 1.0 - 0.03 * np.exp(-t / 0.025)
    vibrato = 1.0 + (0.006 * np.sin(2 * np.pi * 5.5 * t) * np.clip((t - 0.35) / 0.3, 0, 1) if held else 0.0)
    phase = 2 * np.pi * np.cumsum(freq * scoop * vibrato) / RATE
    # brightness opens with the breath (more upper harmonics after the attack, a swell on the held note)
    bright = 0.35 + 0.65 * (1 - np.exp(-t / 0.05))
    if held:
        bright *= 0.85 + 0.15 * np.clip(t / 0.8, 0, 1)
    tone = np.zeros(n)
    for k in range(1, 13):
        if k * freq > 9000:
            break
        amp = (1.0 / k ** 0.9) * bright ** (k * 0.5)
        tone += amp * np.sin(k * phase)
    # envelope: fast attack, sustain, release at the end of the note
    attack = np.clip(t / 0.018, 0, 1)
    release = np.clip((dur + 0.06 - t) / 0.06, 0, 1)
    tone *= attack * release
    # breath noise at the attack
    burst = rng.standard_normal(n) * np.exp(-t / 0.02) * 0.08
    return tone + burst


def reverb(x):
    out = x.copy()
    for delay, gain in ((0.031, 0.22), (0.047, 0.17), (0.071, 0.12), (0.113, 0.08), (0.167, 0.05)):
        d = int(delay * RATE)
        out[d:] += x[:-d] * gain
    return out


def fanfare():
    total = int(3.5 * RATE)
    x = np.zeros(total)
    for name, start, dur in NOTES:
        part = note(FREQ[name], dur, dur > 0.5)
        i = int(start * RATE)
        x[i:i + len(part)] += part[:total - i]
    return reverb(x)


def write(name, x, peak=0.85):
    path = os.path.abspath(os.path.join(OUT, name))
    os.makedirs(OUT, exist_ok=True)
    x = x / max(np.abs(x).max(), 1e-9) * peak
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((x * 32767).astype(np.int16).tobytes())
    print(path)


if __name__ == "__main__":
    write("SW_Trumpet_Fanfare.wav", fanfare())
