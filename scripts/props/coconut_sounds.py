"""Synthesises the sound of a coconut landing on sand (no recorded asset, so no licence to track), on the scheme of
cutlass_sounds.py.

  python scripts/props/coconut_sounds.py  ->  art/audio/hazards/SW_Coconut_Sand_0N.wav (48 kHz mono 16 bit)
Deterministic (fixed seed). Imported by scripts/props/import_coconut_sounds.py.
A 1.8 kg nut falling from the crown (6 to 19 m): a dull thump of the ground taking the weight (the sand absorbs the
attack, no ring), a short hollow knock of the husk, then the sand thrown up: a spray of grains falling back.
Three variants so repeated falls do not sound alike; the game scales the volume with the impact speed.
"""
import os
import wave
import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "hazards")
rng = np.random.default_rng(1811)


def lowpass(x, cutoff):
    """One pole low pass; cutoff may be a constant or an array"""
    cut = np.broadcast_to(np.asarray(cutoff, dtype=float), x.shape)
    a = np.exp(-2 * np.pi * cut / RATE)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a[i]) * v + a[i] * acc
        y[i] = acc
    return y


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def env(t, attack, decay):
    return np.minimum(t / max(attack, 1e-5), 1.0) * np.exp(-t / decay)


def fade(x, seconds=0.03):
    m = int(seconds * RATE)
    x[-m:] *= np.linspace(1, 0, m)
    return x


def landing(body_hz, husk_hz, spray, dur=0.75):
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    # the ground taking the weight: a low thump whose pitch drops as the sand gives way
    pitch = body_hz * (1.0 + 0.6 * np.exp(-t / 0.012))
    thump = np.sin(2 * np.pi * np.cumsum(pitch) / RATE) * env(t, 0.002, 0.045)
    thump += lowpass(rng.standard_normal(n), 220) * env(t, 0.001, 0.03) * 2.5
    # the husk: a short hollow knock, damped by the fibre
    husk = sum(np.sin(2 * np.pi * f * t) * np.exp(-t / d) / (1 + i)
               for i, (f, d) in enumerate([(husk_hz, 0.022), (husk_hz * 2.3, 0.012), (husk_hz * 3.7, 0.007)]))
    husk *= np.minimum(t / 0.0015, 1.0) * 0.35
    # the sand thrown up: a hiss that swells a little then falls, plus the grains raining back
    hiss = highpass(lowpass(rng.standard_normal(n), 5200), 900) * env(t, 0.008, 0.11) * 0.55 * spray
    grains = np.zeros(n)
    count = int(140 * spray)
    for _ in range(count):
        at = 0.02 + rng.exponential(0.12)
        i = int(at * RATE)
        if i >= n - 40:
            continue
        k = np.arange(40) / RATE
        grains[i:i + 40] += rng.choice([-1, 1]) * np.exp(-k / 0.0012) * rng.uniform(0.05, 0.25) * np.exp(-at / 0.2)
    grains = highpass(grains, 1500)
    x = thump + husk + hiss + grains
    x /= np.max(np.abs(x)) + 1e-9
    return fade(x * 0.9)


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
    save("SW_Coconut_Sand_01", landing(62, 310, 1.0))
    save("SW_Coconut_Sand_02", landing(70, 360, 0.8))
    save("SW_Coconut_Sand_03", landing(56, 280, 1.2))
