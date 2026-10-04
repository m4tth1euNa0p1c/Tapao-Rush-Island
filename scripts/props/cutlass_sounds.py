"""Synthesises the cutlass sounds (no recorded asset, so no licence to track), on the scheme of musket_sounds.py.

  python scripts/props/cutlass_sounds.py  ->  art/audio/weapons/SW_Cutlass_*.wav (48 kHz mono 16 bit)
Deterministic (fixed seed).
Swing: air torn by the blade, a band of noise that rises and falls with the speed of the cut (Doppler-like sweep).
Finisher: the same, longer and lower, with a deeper body.
Hit: the blade biting a wooden tiki: sharp bite, wooden knock, short steel ring.
Kill: a deeper chop with the wood splitting (crackle) after the bite.
Parry: steel on steel, a bright clang with long inharmonic partials.
Draw / sheathe: the blade sliding along the scabbard throat, a ring when it leaves / a click of the guard when home.
"""
import os
import wave
import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons")
rng = np.random.default_rng(2070)


def lowpass(x, cutoff):
    """One pole low pass; cutoff may be a constant or an array (time-varying sweep)"""
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


def bandpass(x, low, high):
    return highpass(lowpass(x, high), low)


def env(t, attack, decay):
    return np.minimum(t / max(attack, 1e-5), 1.0) * np.exp(-t / decay)


def ring(t, freqs, decay):
    """Steel: a few inharmonic partials, the higher ones dying first"""
    return sum(np.sin(2 * np.pi * f * t) * np.exp(-t / (decay / (1 + i * 0.6))) / (1 + i) for i, f in enumerate(freqs))


def at(n, start, x):
    out = np.zeros(n)
    i = int(start * RATE)
    k = min(len(x), n - i)
    out[i:i + k] = x[:k]
    return out


def fade(x, seconds=0.02):
    m = int(seconds * RATE)
    x[-m:] *= np.linspace(1, 0, m)
    return x


def whoosh(dur, peak_at, low_hz, high_hz, body_hz):
    """Air torn by the blade: band of noise whose centre and level follow the speed of the cut"""
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    # speed of the blade: a bell peaking at peak_at
    width = dur * 0.22
    speed = np.exp(-((t - peak_at) / width) ** 2)
    noise = rng.standard_normal(n)
    hi = low_hz + (high_hz - low_hz) * speed
    swish = highpass(lowpass(noise, hi), low_hz * 0.6) * speed ** 1.5
    body = lowpass(noise, body_hz) * speed ** 2 * 0.8
    # a faint whistle of the edge at the fastest point
    whistle = np.sin(2 * np.pi * np.cumsum(900 + 700 * speed) / RATE) * speed ** 4 * 0.06
    return fade(swish + body + whistle)


def swing():
    return whoosh(0.38, 0.15, 900, 4200, 500)


def finisher_swing():
    return whoosh(0.62, 0.36, 600, 3200, 300) * 1.2


def bite(n, t, start=0.0):
    tt = np.maximum(t - start, 0) * (t >= start)
    return highpass(rng.standard_normal(n), 2500) * env(tt, 0.0003, 0.008) * (t >= start)


def hit():
    # the blade bites the wood of the tiki: bite, hollow knock, the blade rings a little
    n = int(RATE * 0.45)
    t = np.arange(n) / RATE
    knock = lowpass(rng.standard_normal(n), 900) * env(t, 0.001, 0.035) * 1.6
    hollow = (np.sin(2 * np.pi * 210 * t) + 0.5 * np.sin(2 * np.pi * 470 * t)) * env(t, 0.001, 0.06) * 0.7
    steel = ring(t, [2350, 3720, 5480], 0.12) * 0.18
    return fade(bite(n, t) * 1.2 + knock + hollow + steel)


def kill():
    # a deeper chop, then the wood splits apart (crackle) and the halves knock
    n = int(RATE * 0.9)
    t = np.arange(n) / RATE
    chop = lowpass(rng.standard_normal(n), 700) * env(t, 0.001, 0.05) * 2.0
    hollow = (np.sin(2 * np.pi * 160 * t) + 0.6 * np.sin(2 * np.pi * 380 * t)) * env(t, 0.001, 0.09) * 0.9
    split_t = np.maximum(t - 0.05, 0)
    crackle = highpass(rng.standard_normal(n) * (rng.random(n) < 0.06), 1500) * env(split_t, 0.01, 0.12) * (t > 0.05) * 1.4
    m = int(RATE * 0.15)
    tt = np.arange(m) / RATE
    thud = lowpass(rng.standard_normal(m), 350) * env(tt, 0.001, 0.04) * 1.2
    steel = ring(t, [2100, 3300, 5100], 0.18) * 0.22
    return fade(bite(n, t) * 1.4 + chop + hollow + crackle + at(n, 0.42, thud) + at(n, 0.55, thud * 0.6) + steel)


def parry():
    # steel on steel: a hard transient and a bright clang that rings out
    n = int(RATE * 1.1)
    t = np.arange(n) / RATE
    clack = highpass(rng.standard_normal(n), 1800) * env(t, 0.0002, 0.006) * 1.5
    clang = ring(t, [1180, 2710, 3960, 5830, 7420], 0.35)
    beat = 1 + 0.15 * np.sin(2 * np.pi * 7 * t)
    scrape = bandpass(rng.standard_normal(n), 2500, 7000) * env(t, 0.002, 0.05) * 0.35
    return fade(clack + clang * beat * 0.9 + scrape, 0.1)


def slide(dur, rising):
    """Blade along the scabbard throat"""
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    ramp = t / dur if rising else 1 - t / dur
    shape = np.minimum(t / 0.02, 1.0) * np.minimum((dur - t) / 0.02, 1.0)
    scrape = bandpass(rng.standard_normal(n), 2200, 8000) * (0.35 + 0.65 * ramp) * shape * 0.6
    whine = np.sin(2 * np.pi * np.cumsum(3000 + 1500 * ramp) / RATE) * 0.07 * shape
    return scrape + whine


def draw():
    # out of the scabbard: the slide rises, the blade rings free at the end
    n = int(RATE * 0.9)
    t = np.arange(n) / RATE
    x = at(n, 0.0, slide(0.32, True))
    tt = np.maximum(t - 0.32, 0)
    x += ring(tt, [2950, 4480, 6620], 0.3) * (t > 0.32) * 0.35
    return fade(x, 0.1)


def sheathe():
    # back in: the slide falls, the guard clicks against the throat
    n = int(RATE * 0.6)
    t = np.arange(n) / RATE
    x = at(n, 0.0, slide(0.38, False))
    m = int(RATE * 0.15)
    tt = np.arange(m) / RATE
    click = highpass(rng.standard_normal(m), 2000) * env(tt, 0.0002, 0.006) + ring(tt, [1650, 2900], 0.05) * 0.4 \
        + lowpass(rng.standard_normal(m), 600) * env(tt, 0.001, 0.02) * 0.8
    return fade(x + at(n, 0.38, click))


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


if __name__ == "__main__":
    write("SW_Cutlass_Swing.wav", swing(), 0.6)
    write("SW_Cutlass_FinisherSwing.wav", finisher_swing(), 0.7)
    write("SW_Cutlass_Hit.wav", hit(), 0.85)
    write("SW_Cutlass_Kill.wav", kill(), 0.89)
    write("SW_Cutlass_Parry.wav", parry(), 0.75)
    write("SW_Cutlass_Draw.wav", draw(), 0.5)
    write("SW_Cutlass_Sheathe.wav", sheathe(), 0.5)
