"""Synthesises the sounds of the barque on the water (owner request 2026-10-07: "ajoute son de barque quand elle navigue
sur l'eau"), on the scheme of footstep_sounds.py: no recorded asset, so no licence to track.

  python scripts/props/boat_sounds.py  ->  art/audio/boat/SW_Boat_*.wav (48 kHz mono 16 bit)
Deterministic (fixed seed). Imported by scripts/props/import_boat_sounds_ue.py (DA_BoatRules).
  SW_Boat_Wake       - the water along the hull, an 8 s loop with no seam (shaped in the frequency domain, every
                       modulation a whole number of periods over the loop): its volume follows the speed in game
  SW_Boat_OarSplash_0N - a blade biting the water: a slap, a short spray and a few droplets
  SW_Boat_OarDrip_0N   - a blade leaving the water: a soft swish and droplets falling back
"""
import os
import wave
import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "boat")
rng = np.random.default_rng(2718)


def shaped_noise(n, low, high, tilt=0.0):
    """Noise of n samples whose spectrum is a band between low and high (Hz, smooth edges); circular, so it loops"""
    freqs = np.fft.rfftfreq(n, 1.0 / RATE)
    gain = 1.0 / (1.0 + (low / np.maximum(freqs, 1.0)) ** 4) / (1.0 + (freqs / high) ** 4)
    if tilt:
        gain *= (np.maximum(freqs, 20.0) / 1000.0) ** tilt
    spectrum = gain * np.exp(2j * np.pi * rng.random(len(freqs)))
    x = np.fft.irfft(spectrum, n)
    return x / (np.max(np.abs(x)) + 1e-9)


def env(t, attack, decay):
    return np.minimum(t / max(attack, 1e-5), 1.0) * np.exp(-t / decay)


def blip(dur, f0, f1, decay):
    """A droplet or a bubble: a short sine gliding from f0 to f1, dying away"""
    k = int(dur * RATE)
    t = np.arange(k) / RATE
    f = f0 + (f1 - f0) * np.minimum(t / dur, 1.0)
    return np.sin(2 * np.pi * np.cumsum(f) / RATE) * env(t, 0.001, decay)


def add_at(out, start, x, wrap=False):
    i = int(start * RATE)
    if wrap:
        idx = (np.arange(len(x)) + i) % len(out)
        np.add.at(out, idx, x)
        return
    k = min(len(x), len(out) - i)
    if k > 0:
        out[i:i + k] += x[:k]


def fade(x, seconds=0.03):
    m = int(seconds * RATE)
    x[-m:] *= np.linspace(1, 0, m)
    return x


def normalise(x, peak=0.89):
    return x / (np.max(np.abs(x)) + 1e-9) * peak


def wake(seconds=8.0):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    # the rush of water along the planks, darker below, a little hiss above
    body = shaped_noise(n, 140, 1800, tilt=-0.3)
    hiss = shaped_noise(n, 1500, 6000)
    # swells and lapping: whole periods over the loop, so the end meets the start
    swell = 0.68 + 0.18 * np.sin(2 * np.pi * 2 * t / seconds) + 0.09 * np.sin(2 * np.pi * 5 * t / seconds + 1.3)
    lap = 0.75 + 0.25 * np.clip(np.sin(2 * np.pi * 11 * t / seconds + 0.4), 0, None) ** 3
    x = body * swell * lap + 0.22 * hiss * swell
    # gurgles: bubbles rising in the wash, placed round the loop
    bubbles = np.zeros(n)
    for _ in range(46):
        f0 = rng.uniform(260, 900)
        add_at(bubbles, rng.uniform(0, seconds), blip(rng.uniform(0.02, 0.06), f0, f0 * rng.uniform(1.3, 1.9), rng.uniform(0.01, 0.025))
               * rng.uniform(0.15, 0.4), wrap=True)
    x += bubbles
    return normalise(x, 0.8)


def splash(seconds=0.75):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    x = np.zeros(n)
    # the blade slapping in: a dull plunge and a burst of water
    plunge = rng.uniform(105, 150)
    x += np.sin(2 * np.pi * np.cumsum(plunge * (1 + 0.5 * np.exp(-t / 0.01))) / RATE) * env(t, 0.002, 0.045) * 0.55
    burst = shaped_noise(n, 260, 3600)
    x += burst * env(t, 0.003, 0.075) * 0.9
    # the spray thrown up, then falling back
    spray = shaped_noise(n, 2000, 7000)
    x += spray * env(t, 0.012, 0.16) * 0.28
    for _ in range(int(rng.integers(14, 22))):
        f0 = rng.uniform(1200, 3800)
        add_at(x, 0.05 + rng.exponential(0.12), blip(0.03, f0, f0 * rng.uniform(0.7, 0.95), rng.uniform(0.006, 0.015)) * rng.uniform(0.08, 0.25))
    return fade(normalise(x))


def drip(seconds=0.9):
    n = int(seconds * RATE)
    t = np.arange(n) / RATE
    x = np.zeros(n)
    # the blade sliding out: a soft swish
    swish = shaped_noise(n, 400, 2600)
    x += swish * env(t, 0.02, 0.09) * 0.45
    # the water running off the blade and falling back in drops, thinning out
    count = int(rng.integers(7, 12))
    for i in range(count):
        at = 0.08 + rng.uniform(0, 0.7) * (i + 1) / count
        f0 = rng.uniform(1100, 3300)
        add_at(x, at, blip(0.04, f0, f0 * rng.uniform(0.75, 0.95), rng.uniform(0.008, 0.02)) * rng.uniform(0.2, 0.5) * (1 - 0.6 * at / seconds))
    return fade(normalise(x, 0.7))


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
    save("SW_Boat_Wake", wake())
    for i in range(3):
        save(f"SW_Boat_OarSplash_0{i + 1}", splash())
    for i in range(2):
        save(f"SW_Boat_OarDrip_0{i + 1}", drip())
