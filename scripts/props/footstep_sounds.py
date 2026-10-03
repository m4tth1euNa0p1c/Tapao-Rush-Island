"""Synthesises the footsteps of the bean and the creatures on each ground of the atoll (no recorded asset, so no
licence to track), on the scheme of cutlass_sounds.py.

  python scripts/props/footstep_sounds.py  ->  art/audio/footsteps/SW_Step_<Surface>_0N.wav (48 kHz mono 16 bit)
Deterministic (fixed seed). Imported by scripts/props/import_footstep_sounds.py (DA_FootstepRules).
A light bare-footed / booted step: heel then toe, about 30 ms apart.
  SandDry  - soft compression and a dry squeak-crunch of the grains, no thump
  SandWet  - a damp pat, darker, with a short suck when the foot lifts
  Water    - ankle-deep splash: a slap and a spray of droplets
  Litter   - palm fronds and dry leaves: crackle and rustle
  Coral    - coral rubble: hard clicky fragments shifting under the foot
  Stone    - a hard tap with grit
  Wood     - a hollow knock of the boat's planks or an engawa
Four variants per surface.
"""
import os
import wave
import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "footsteps")
rng = np.random.default_rng(3141)


def lowpass(x, cutoff):
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


def band(x, low, high):
    return highpass(lowpass(x, high), low)


def env(t, attack, decay):
    return np.minimum(t / max(attack, 1e-5), 1.0) * np.exp(-t / decay)


def place(n, start, x):
    out = np.zeros(n)
    i = int(start * RATE)
    k = min(len(x), n - i)
    if k > 0:
        out[i:i + k] = x[:k]
    return out


def fade(x, seconds=0.02):
    m = int(seconds * RATE)
    x[-m:] *= np.linspace(1, 0, m)
    return x


def clicks(n, count, start, spread, low, decay, amp=(0.1, 0.4)):
    """Sparse small impacts (grains, fragments, droplets) after start, thinning out"""
    out = np.zeros(n)
    for _ in range(count):
        at = start + rng.exponential(spread)
        i = int(at * RATE)
        if i >= n - 64:
            continue
        k = np.arange(64) / RATE
        out[i:i + 64] += rng.choice([-1, 1]) * np.exp(-k / decay) * rng.uniform(*amp)
    return highpass(out, low)


def thud(n, t, hz, decay, amp):
    pitch = hz * (1 + 0.4 * np.exp(-t / 0.008))
    return np.sin(2 * np.pi * np.cumsum(pitch) / RATE) * env(t, 0.002, decay) * amp


def step(surface, dur=0.38):
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    noise = rng.standard_normal(n)
    toe = 0.025 + rng.uniform(0, 0.015)
    x = np.zeros(n)
    for start, weight in ((0.0, 1.0), (toe, 0.6)):
        tt = np.maximum(t - start, 0) * (t >= start)
        e = lambda a, d: env(tt, a, d) * (t >= start)
        if surface == "SandDry":
            x += band(noise, 700, 6000) * e(0.01, 0.05) * 0.5 * weight
            x += lowpass(noise, 300) * e(0.004, 0.03) * 0.8 * weight
        elif surface == "SandWet":
            x += lowpass(noise, 900) * e(0.003, 0.035) * 1.0 * weight
            x += thud(n, tt, 90, 0.03, 0.4) * (t >= start) * weight
        elif surface == "Water":
            x += band(noise, 300, 3500) * e(0.004, 0.06) * 0.9 * weight
            x += thud(n, tt, 140, 0.02, 0.3) * (t >= start) * weight
        elif surface == "Litter":
            x += band(noise, 1500, 8000) * e(0.006, 0.04) * 0.35 * weight
            x += lowpass(noise, 350) * e(0.003, 0.025) * 0.5 * weight
        elif surface == "Coral":
            x += lowpass(noise, 600) * e(0.002, 0.02) * 0.5 * weight
        elif surface == "Stone":
            x += band(noise, 200, 2500) * e(0.001, 0.015) * 0.8 * weight
            x += thud(n, tt, 120, 0.018, 0.4) * (t >= start) * weight
        elif surface == "Wood":
            x += thud(n, tt, 160 + rng.uniform(-15, 15), 0.06, 0.9) * (t >= start) * weight
            x += thud(n, tt, 410, 0.03, 0.3) * (t >= start) * weight
            x += band(noise, 300, 3000) * e(0.001, 0.01) * 0.4 * weight
    # texture after the impact
    if surface == "SandDry":
        x += clicks(n, 90, 0.005, 0.05, 2500, 0.0006, (0.05, 0.2))
    elif surface == "SandWet":
        # the suck of the foot lifting off the wet sand
        x += place(n, 0.17, band(rng.standard_normal(int(0.07 * RATE)), 200, 1200) * env(np.arange(int(0.07 * RATE)) / RATE, 0.02, 0.02) * 0.4)
    elif surface == "Water":
        x += clicks(n, 70, 0.03, 0.08, 1800, 0.002, (0.05, 0.3))
        x += band(noise, 600, 4000) * env(t, 0.03, 0.12) * 0.25
    elif surface == "Litter":
        x += clicks(n, 60, 0.0, 0.07, 1200, 0.0015, (0.1, 0.5))
    elif surface == "Coral":
        x += clicks(n, 45, 0.0, 0.05, 1500, 0.003, (0.2, 0.7))
    elif surface == "Stone":
        x += clicks(n, 12, 0.0, 0.03, 3000, 0.0008, (0.05, 0.2))
    x /= np.max(np.abs(x)) + 1e-9
    return fade(x * 0.85)


def save(name, x):
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((np.clip(x, -1, 1) * 32767).astype(np.int16).tobytes())
    print(path)


SURFACES = ["SandDry", "SandWet", "Water", "Litter", "Coral", "Stone", "Wood"]

if __name__ == "__main__":
    for surface in SURFACES:
        for i in range(1, 5):
            save(f"SW_Step_{surface}_{i:02d}", step(surface))
