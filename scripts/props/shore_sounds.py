"""Synthesises the shore ambience loops (no recording, so no licence to track).

  python scripts/props/shore_sounds.py  ->  art/audio/ambience/SW_Shore_Surf.wav (mono, 48 s loop)
                                             art/audio/ambience/SW_Shore_Sea.wav  (stereo, 32 s loop)
Surf: waves breaking on a coral sand beach, one every 6 to 10 s: the swell rumbles in, the lip crashes, the wash runs
up the sand, then the backwash fizzes and rattles the sand back down. Placed in 3D on the nearest shore line.
Sea: the open water heard from the land or a boat: a low surge breathing slowly, distant breaks on the reef, a light
hiss. Played around the listener, louder the more water there is around.
Both loop seamlessly (the tail is cross-faded into the head). Deterministic (fixed seed). 48 kHz 16 bit.
"""
import os
import wave

import numpy as np
from scipy.signal import butter, sosfilt

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "ambience")
rng = np.random.default_rng(2026)


def band(x, low=None, high=None, order=2):
    if low and high:
        sos = butter(order, [low, high], btype="band", fs=RATE, output="sos")
    elif low:
        sos = butter(order, low, btype="high", fs=RATE, output="sos")
    else:
        sos = butter(order, high, btype="low", fs=RATE, output="sos")
    return sosfilt(sos, x)


def smooth_env(points, n):
    """Piecewise envelope through (time s, value) points, eased"""
    t = np.arange(n) / RATE
    times = np.array([p[0] for p in points])
    values = np.array([p[1] for p in points])
    out = np.interp(t, times, values)
    k = int(0.05 * RATE)
    return np.convolve(out, np.ones(k) / k, mode="same")


def wave_event(n, start, strength):
    """One wave breaking on the sand, starting at start (s), in a buffer of n samples"""
    out = np.zeros(n)
    length = 9.0
    m = min(int(length * RATE), n - int(start * RATE))
    if m <= 0:
        return out
    noise = rng.standard_normal(m)
    # swell coming in: low rumble
    rumble = band(noise, 60, 350) * smooth_env([(0, 0), (1.2, 0.5), (1.6, 0.9), (2.4, 0.3), (4.0, 0), (length, 0)], m) * 1.4
    # the lip crashes: wide, bright, short
    crash = band(rng.standard_normal(m), 150, 4500) * smooth_env([(0, 0), (1.45, 0), (1.6, 1.0), (2.1, 0.55), (3.0, 0.15), (length, 0)], m)
    # the wash runs up the sand: mid band
    wash = band(rng.standard_normal(m), 500, 3500) * smooth_env([(0, 0), (1.8, 0), (2.4, 0.55), (3.6, 0.35), (4.6, 0.08), (length, 0)], m)
    # backwash: foam fizz and sand rattle going back down
    fizz = band(rng.standard_normal(m), 3000, 11000) * smooth_env([(0, 0), (2.2, 0), (3.0, 0.45), (5.0, 0.25), (7.5, 0.0), (length, 0)], m)
    grains = band((rng.random(m) < 0.02) * rng.standard_normal(m), 2500, 9000) * smooth_env(
        [(0, 0), (3.6, 0), (4.4, 0.9), (6.5, 0.3), (8.0, 0), (length, 0)], m) * 2.2
    i = int(start * RATE)
    out[i:i + m] = (rumble + crash + wash + fizz + grains) * strength
    return out


def surf():
    seconds, fade = 48.0, 3.0
    n = int((seconds + fade) * RATE)
    x = np.zeros(n)
    t = 0.0
    while t < seconds + fade - 1.0:
        x += wave_event(n, t, rng.uniform(0.65, 1.0))
        t += rng.uniform(6.0, 10.0)
    # lapping water bed between the waves
    x += band(rng.standard_normal(n), 200, 2000) * 0.06
    return loop(x, seconds, fade)


def sea_channel():
    seconds, fade = 32.0, 3.0
    n = int((seconds + fade) * RATE)
    t = np.arange(n) / RATE
    breathe = 0.65 + 0.35 * np.sin(2 * np.pi * t / 11.0 + rng.uniform(0, 6)) * np.sin(2 * np.pi * t / 4.7 + rng.uniform(0, 6))
    surge = band(rng.standard_normal(n), 40, 420) * breathe * 1.3
    hiss = band(rng.standard_normal(n), 1500, 7000) * (0.5 + 0.5 * breathe) * 0.08
    reef = np.zeros(n)
    start = rng.uniform(0, 3)
    while start < seconds + fade - 1.0:
        reef += band(wave_event(n, start, rng.uniform(0.3, 0.6)), None, 1400)
        start += rng.uniform(5.0, 9.0)
    return loop(surge + hiss + reef * 0.5, seconds, fade)


def loop(x, seconds, fade):
    """Cross-fades the tail beyond seconds into the head: x[:seconds] loops without a click"""
    n = int(seconds * RATE)
    f = int(fade * RATE)
    out = x[:n].copy()
    w = np.linspace(0.0, 1.0, f)
    out[:f] = out[:f] * w + x[n:n + f] * (1.0 - w)
    return out


def write(name, channels, peak):
    data = np.stack(channels, axis=1) if len(channels) > 1 else channels[0][:, None]
    data = data / np.max(np.abs(data)) * peak
    path = os.path.abspath(os.path.join(OUT, name))
    os.makedirs(OUT, exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(data.shape[1])
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((data * 32767).astype("<i2").tobytes())
    print(path, f"{data.shape[0] / RATE:.1f} s, {data.shape[1]} ch")


write("SW_Shore_Surf.wav", [surf()], 0.8)
write("SW_Shore_Sea.wav", [sea_channel(), sea_channel()], 0.6)
