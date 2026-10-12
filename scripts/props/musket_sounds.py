"""Synthesises the musket sounds (no recorded asset, so no licence to track): black powder shot and flintlock.

  python scripts/props/musket_sounds.py  ->  art/audio/weapons/SW_Musket_*.wav (48 kHz mono 16 bit)
Deterministic (fixed seed). Shot: sharp crack, low boom, rolling tail with a slap back from the palms.
Lock: flint scrape on the frizzen, then the pan flash hiss.
Reload cues (played along the reload by DA_MusketRules.ReloadSoundCues): hammer cocked (two detents), frizzen
opened / snapped shut, powder poured, cartridge paper torn, ball rolling down the barrel, ramrod drawn, ram stroke.
"""
import os
import wave
import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons")
rng = np.random.default_rng(1777)


def lowpass(x, cutoff):
    a = np.exp(-2 * np.pi * cutoff / RATE)
    y = np.empty_like(x)
    acc = 0.0
    for i, v in enumerate(x):
        acc = (1 - a) * v + a * acc
        y[i] = acc
    return y


def highpass(x, cutoff):
    return x - lowpass(x, cutoff)


def env(t, attack, decay):
    return np.minimum(t / max(attack, 1e-5), 1.0) * np.exp(-t / decay)


def shot():
    n = int(RATE * 2.6)
    t = np.arange(n) / RATE
    noise = rng.standard_normal(n)
    crack = highpass(noise, 1800) * env(t, 0.0004, 0.012) * 1.4
    body = lowpass(noise, 900) * env(t, 0.002, 0.09) * 3.0
    boom = np.sin(2 * np.pi * (55 + 40 * np.exp(-t / 0.05)) * t) * env(t, 0.003, 0.22) * 0.9
    tail = lowpass(noise, 380) * env(t, 0.04, 0.55) * 1.6
    x = crack + body + boom + tail
    # slap back from the tree line ~110 ms later, darker
    d = int(0.11 * RATE)
    echo = np.zeros(n)
    echo[d:] = lowpass(x[:-d], 1200) * 0.28
    x = x + echo
    x[: int(0.0005 * RATE)] *= np.linspace(0, 1, int(0.0005 * RATE))
    x *= np.minimum(1.0, (t[-1] - t) / 0.3)  # fade out
    return x


def lock():
    n = int(RATE * 0.35)
    t = np.arange(n) / RATE
    noise = rng.standard_normal(n)
    click = highpass(noise, 3000) * env(t, 0.0002, 0.004) * 1.2
    scrape = highpass(noise, 2500) * env(np.maximum(t - 0.006, 0), 0.002, 0.02) * (t > 0.006) * 0.5
    hiss = highpass(lowpass(noise, 6000), 900) * env(np.maximum(t - 0.03, 0), 0.01, 0.07) * (t > 0.03) * 0.6
    x = click + scrape + hiss
    x *= np.minimum(1.0, (t[-1] - t) / 0.05)
    return x


def ring(t, freqs, decay):
    """Small metal part: a few inharmonic partials"""
    return sum(np.sin(2 * np.pi * f * t) * np.exp(-t / (decay / (1 + i * 0.6))) / (1 + i) for i, f in enumerate(freqs))


def at(n, start, x):
    """x placed in a buffer of n samples from start (s)"""
    out = np.zeros(n)
    i = int(start * RATE)
    k = min(len(x), n - i)
    out[i:i + k] = x[:k]
    return out


def tick(dur, hp, decay, freqs, ring_gain):
    m = int(RATE * dur)
    t = np.arange(m) / RATE
    return highpass(rng.standard_normal(m), hp) * env(t, 0.0002, decay) + ring(t, freqs, decay * 4) * ring_gain


def cock():
    # tumbler over its two notches: a soft click then the hard one, steel ringing
    n = int(RATE * 0.32)
    a = tick(0.08, 2500, 0.004, [3100, 4700, 6900], 0.25) * 0.55
    b = tick(0.15, 2000, 0.006, [2600, 4100, 6300], 0.45)
    return at(n, 0.0, a) + at(n, 0.11, b)


def frizzen():
    # the steel springs open / shut: a snap with a low body
    n = int(RATE * 0.25)
    t = np.arange(n) / RATE
    x = tick(0.25, 1500, 0.007, [1900, 3300, 5200], 0.4)
    return x + lowpass(rng.standard_normal(n), 500) * env(t, 0.001, 0.02) * 0.8


def pour(dur):
    # black powder grains running: a granular hiss with random grain clicks
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    shape = np.minimum(t / 0.06, 1.0) * np.minimum((dur - t) / 0.12, 1.0)
    hiss = highpass(lowpass(rng.standard_normal(n), 7000), 1500) * 0.35
    grains = (rng.random(n) < 0.004) * rng.standard_normal(n) * 3.0
    grains = highpass(grains, 3000)
    return (hiss + grains) * shape


def paper():
    # cartridge torn with the teeth: crackling bursts
    n = int(RATE * 0.32)
    t = np.arange(n) / RATE
    crackle = highpass(rng.standard_normal(n) * (rng.random(n) < 0.08), 2000)
    shape = env(t, 0.01, 0.08) + env(np.maximum(t - 0.12, 0), 0.005, 0.05) * (t > 0.12) * 0.7
    return crackle * shape


def ball():
    # the ball rattles down the barrel and lands on the powder
    n = int(RATE * 0.6)
    x = np.zeros(n)
    times = np.cumsum(np.linspace(0.045, 0.02, 9))
    for i, start in enumerate(times):
        x += at(n, start, tick(0.05, 3000, 0.003, [5200 - i * 250, 7900 - i * 300], 0.3) * (0.4 + i * 0.04))
    m = int(RATE * 0.2)
    tt = np.arange(m) / RATE
    thud = lowpass(rng.standard_normal(m), 400) * env(tt, 0.001, 0.03) * 1.6 + np.sin(2 * np.pi * 180 * tt) * env(tt, 0.001, 0.04)
    return x + at(n, 0.4, thud)


def ramrod_draw(dur):
    # steel rod sliding out of its pipes: a scrape that rises, a ring when it leaves
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    scrape = highpass(rng.standard_normal(n), 2500) * (0.3 + 0.7 * t / dur) * np.minimum((dur - t) / 0.03, 1.0)
    whine = np.sin(2 * np.pi * (2400 + 1800 * t / dur) * t) * 0.15 * np.minimum(t / 0.05, 1.0)
    out = ring(np.maximum(t - dur * 0.8, 0), [1700, 2950, 4600], 0.18) * (t > dur * 0.8) * 0.5
    return (scrape + whine) * 0.6 + out


def ram_stroke():
    # the rod packs the ball down: a dull knock with the rod ringing
    n = int(RATE * 0.3)
    t = np.arange(n) / RATE
    knock = lowpass(rng.standard_normal(n), 600) * env(t, 0.001, 0.025) * 1.8
    return knock + ring(t, [820, 1460, 2700], 0.09) * 0.45 + np.sin(2 * np.pi * 140 * t) * env(t, 0.001, 0.05) * 0.8


def write(name, x, peak=0.89):
    x = x / np.max(np.abs(x)) * peak
    path = os.path.abspath(os.path.join(OUT, name))
    os.makedirs(OUT, exist_ok=True)
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((x * 32767).astype("<i2").tobytes())
    print(path, f"{len(x) / RATE:.2f} s")


write("SW_Musket_Shot.wav", shot())
write("SW_Musket_Lock.wav", lock(), 0.5)
write("SW_Musket_Cock.wav", cock(), 0.55)
write("SW_Musket_Frizzen.wav", frizzen(), 0.5)
write("SW_Musket_Prime.wav", pour(0.4), 0.3)
write("SW_Musket_Powder.wav", pour(0.75), 0.35)
write("SW_Musket_Paper.wav", paper(), 0.35)
write("SW_Musket_Ball.wav", ball(), 0.5)
write("SW_Musket_RamrodDraw.wav", ramrod_draw(0.45), 0.45)
write("SW_Musket_RamStroke.wav", ram_stroke(), 0.6)
