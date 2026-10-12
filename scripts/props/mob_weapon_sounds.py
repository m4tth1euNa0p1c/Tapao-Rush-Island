"""Synthesises the sounds of the mobs' weapons picked up by the bean (T-161): the spear with stone teeth, the obsidian
club and the shell hammer, wood and stone, no steel. No recorded asset, so no licence to track; same scheme as
cutlass_sounds.py (deterministic, fixed seed).

  python scripts/props/mob_weapon_sounds.py  ->  art/audio/weapons/SW_MobWeapon_*.wav (48 kHz mono 16 bit)

Swing: a thick wooden shaft through the air, lower and rounder than the sabre, no whistle of an edge.
HeavySwing: the long overhead blow (finisher), slower and deeper.
Hit: the head of the weapon on a wooden tiki body: a deep thud, the hollow knock of the wood, a small crack.
Kill: a crushing blow: thud, the body's wood splitting (crackle), then the pieces knocking on the ground.
Parry: wood against a blade or a club: a hard "clock" with a short woody ring.
Draw / Sheathe: the hand closing on the rope grip and the shaft brought up / lowered (rustle and soft knock).
Imported and set in DA_<Weapon>Rules by scripts/editor/mob_weapons_setup_ue.py (lead session action "mobweapons").
"""
import os
import wave

import numpy as np

RATE = 48000
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons")
rng = np.random.default_rng(161)


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


def modes(t, freqs, decay):
    """Wood: a few damped modes, the higher ones dying first"""
    return sum(np.sin(2 * np.pi * f * t) * np.exp(-t / (decay / (1 + i * 0.8))) / (1 + i) for i, f in enumerate(freqs))


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
    """Air pushed by a thick shaft: a band of noise following the speed of the swing, a round body, no whistle"""
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    width = dur * 0.26
    speed = np.exp(-((t - peak_at) / width) ** 2)
    noise = rng.standard_normal(n)
    hi = low_hz + (high_hz - low_hz) * speed
    swish = highpass(lowpass(noise, hi), low_hz * 0.5) * speed ** 1.6
    body = lowpass(noise, body_hz) * speed ** 2 * 1.1
    return fade(swish + body)


def thud(n, t, start, cutoff, decay, gain):
    tt = np.maximum(t - start, 0)
    return lowpass(rng.standard_normal(n), cutoff) * env(tt, 0.0015, decay) * (t >= start) * gain


def crack(n, t, start, decay, gain):
    tt = np.maximum(t - start, 0)
    return bandpass(rng.standard_normal(n), 1200, 6000) * env(tt, 0.0004, decay) * (t >= start) * gain


def swing():
    return whoosh(0.42, 0.18, 320, 1700, 210)


def heavy_swing():
    return whoosh(0.72, 0.42, 240, 1300, 150) * 1.25


def hit():
    n = int(RATE * 0.5)
    t = np.arange(n) / RATE
    knock = modes(t, [165, 410, 780], 0.07) * env(t, 0.0012, 0.12) * 0.9
    return fade(thud(n, t, 0.0, 380, 0.05, 2.2) + knock + crack(n, t, 0.002, 0.012, 0.8))


def kill():
    n = int(RATE * 1.0)
    t = np.arange(n) / RATE
    knock = modes(t, [130, 330, 640], 0.1) * env(t, 0.0012, 0.16) * 1.1
    split = np.maximum(t - 0.04, 0)
    crackle = highpass(rng.standard_normal(n) * (rng.random(n) < 0.05), 1400) * env(split, 0.01, 0.14) * (t > 0.04) * 1.5
    m = int(RATE * 0.18)
    tt = np.arange(m) / RATE
    ground = lowpass(rng.standard_normal(m), 300) * env(tt, 0.001, 0.045) * 1.3
    return fade(thud(n, t, 0.0, 320, 0.07, 2.6) + knock + crack(n, t, 0.002, 0.02, 1.1) + crackle
                + at(n, 0.46, ground) + at(n, 0.6, ground * 0.6))


def parry():
    n = int(RATE * 0.6)
    t = np.arange(n) / RATE
    clock = highpass(rng.standard_normal(n), 1600) * env(t, 0.0002, 0.006) * 1.4
    wood = modes(t, [720, 1310, 2150], 0.09) * 0.9
    return fade(clock + wood + thud(n, t, 0.0, 600, 0.03, 0.8), 0.06)


def rustle(dur, gain):
    n = int(RATE * dur)
    t = np.arange(n) / RATE
    shape = np.sin(np.pi * np.clip(t / dur, 0, 1)) ** 1.5
    grain = bandpass(rng.standard_normal(n) * (0.4 + 0.6 * (rng.random(n) < 0.3)), 900, 5000)
    return grain * shape * gain


def draw():
    n = int(RATE * 0.5)
    t = np.arange(n) / RATE
    tap = modes(t, [240, 520], 0.04) * env(t, 0.001, 0.05) * 0.6
    return fade(at(n, 0.0, rustle(0.28, 0.5)) + at(n, 0.26, tap), 0.05)


def sheathe():
    n = int(RATE * 0.55)
    t = np.arange(n) / RATE
    m = int(RATE * 0.2)
    tt = np.arange(m) / RATE
    knock = lowpass(rng.standard_normal(m), 450) * env(tt, 0.001, 0.04) * 1.2 + modes(tt, [190, 430], 0.05) * 0.5
    return fade(at(n, 0.0, rustle(0.3, 0.45)) + at(n, 0.3, knock), 0.05)


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
    write("SW_MobWeapon_Swing.wav", swing(), 0.6)
    write("SW_MobWeapon_HeavySwing.wav", heavy_swing(), 0.72)
    write("SW_MobWeapon_Hit.wav", hit(), 0.88)
    write("SW_MobWeapon_Kill.wav", kill(), 0.9)
    write("SW_MobWeapon_Parry.wav", parry(), 0.75)
    write("SW_MobWeapon_Draw.wav", draw(), 0.45)
    write("SW_MobWeapon_Sheathe.wav", sheathe(), 0.45)
