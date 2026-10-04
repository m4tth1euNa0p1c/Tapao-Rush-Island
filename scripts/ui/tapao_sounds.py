"""Synthesises the button sounds of the TAPAO interface (kit ui-tapao: wooden buttons), no recorded asset.

  python scripts/ui/tapao_sounds.py  ->  art/audio/ui/SW_UI_Hover.wav, SW_UI_Click.wav (48 kHz mono 16 bit)
Hover: a soft tap on driftwood (short knock, low partials). Click: a firmer wooden knock with a rope creak tail.
Deterministic (fixed seed).
"""
import os
import wave

import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "ui")
rng = np.random.default_rng(2104)


def knock(dur, freqs, decay, noise):
    t = np.arange(int(dur * RATE)) / RATE
    x = sum(a * np.sin(2 * np.pi * f * t) * np.exp(-t / (decay * d)) for f, a, d in freqs)
    x += rng.standard_normal(len(t)) * noise * np.exp(-t / 0.004)
    return x * np.clip(t / 0.0015, 0, 1)


def creak(dur):
    t = np.arange(int(dur * RATE)) / RATE
    pulses = (np.sin(2 * np.pi * 38 * t) > 0.9).astype(float)
    tone = np.sin(2 * np.pi * (520 + 80 * t / dur) * t)
    return pulses * tone * 0.15 * np.exp(-t / 0.06)


def write(name, x, peak):
    os.makedirs(OUT, exist_ok=True)
    x = x / max(np.abs(x).max(), 1e-9) * peak
    path = os.path.abspath(os.path.join(OUT, name))
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((x * 32767).astype(np.int16).tobytes())
    print(path)


if __name__ == "__main__":
    write("SW_UI_Hover.wav", knock(0.09, [(620, 1.0, 1.0), (1180, 0.5, 0.6), (1900, 0.25, 0.4)], 0.018, 0.2), 0.35)
    click = knock(0.16, [(380, 1.0, 1.0), (760, 0.6, 0.7), (1450, 0.35, 0.5)], 0.03, 0.35)
    tail = creak(0.12)
    x = np.zeros(len(click) + len(tail) // 2)
    x[:len(click)] += click
    x[len(click) // 2:len(click) // 2 + len(tail)] += tail
    write("SW_UI_Click.wav", x, 0.6)
