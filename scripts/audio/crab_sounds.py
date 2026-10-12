"""Sounds of the giant crab boss "Tupa Nui" (T-261, owner 2026-10-09: "ajoute aussi les bruits de pas des pattes du crabe
géant", "rends le crabe ultra terrifiant"): synthesised from scratch with numpy / scipy (no recording, no external
source: original work of the project, free of rights), deterministic (one seed per variation).

  python scripts/audio/crab_sounds.py            -> art/audio/creatures/giant_crab/SW_GiantCrab_*.wav + manifest.json
  then the editor action "crabboss" (scripts/editor/crab_boss_setup_ue.py) imports them and binds them to DA_GiantCrabRules

Families (48 kHz, 16 bit, mono, peak-normalised, 5 ms fades):
- Step (6): a chitin leg tip on the sand: a dry click, the hollow "tok" of the shell, a little sand crunch, a soft thud;
- StepHeavy (4): a hind leg: deeper thud, more sand, the leg settling a moment after;
- Clack (3): the two halves of a pincer slammed together (resonant modes of a hard shell);
- Hit (3): a blow of the crab landing on a body;
- Slam (2), Stomp (3), BellySlam (2), BodyFall (2), Wave (1): impacts on the sand, booms with a falling pitch, sand
  bursts and debris;
- Roar (2), Charge (2), Death (1): the voice: a harmonic growl with a falling pitch, sub-harmonic and rattle (vocal fry),
  vowel formants that close, a breath and a bubbling hiss, mandibles clattering, saturated;
- Hiss (3): a bubbling crustacean hiss; Growl (3): a low rumble while it waits.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, sosfilt

RATE = 48000
ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art" / "audio" / "creatures" / "giant_crab"
VERSION = 1


# ---------------------------------------------------------------- helpers

def n_of(seconds):
    return int(round(seconds * RATE))


def time_axis(n):
    return np.arange(n) / RATE


def sos_band(lo, hi, order=2):
    nyq = RATE / 2
    lo = max(lo, 10.0)
    hi = min(hi, nyq * 0.98)
    return butter(order, [lo / nyq, hi / nyq], btype="band", output="sos")


def lowpass(x, freq, order=2):
    return sosfilt(butter(order, min(freq, RATE * 0.49) / (RATE / 2), btype="low", output="sos"), x)


def highpass(x, freq, order=2):
    return sosfilt(butter(order, freq / (RATE / 2), btype="high", output="sos"), x)


def bandpass(x, lo, hi, order=2):
    return sosfilt(sos_band(lo, hi, order), x)


def decay(n, tau, delay=0.0):
    t = time_axis(n) - delay
    env = np.exp(-np.maximum(t, 0.0) / max(tau, 1e-4))
    env[t < 0] = 0.0
    return env


def attack_decay(n, attack, tau, delay=0.0):
    t = time_axis(n) - delay
    env = np.where(t < attack, np.clip(t / max(attack, 1e-4), 0.0, 1.0), np.exp(-(t - attack) / max(tau, 1e-4)))
    env[t < 0] = 0.0
    return env


def swell(n, attack, release):
    """Smooth rise over attack seconds, smooth fall over the last release seconds"""
    t = time_axis(n)
    length = n / RATE
    rise = np.clip(t / max(attack, 1e-4), 0.0, 1.0)
    fall = np.clip((length - t) / max(release, 1e-4), 0.0, 1.0)
    return (0.5 - 0.5 * np.cos(np.pi * rise)) * (0.5 - 0.5 * np.cos(np.pi * fall))


def tone(freq, n, tau=None, phase=0.0, delay=0.0):
    """Sine of a constant or varying frequency (array), damped when tau is given, starting after delay"""
    f = np.broadcast_to(np.asarray(freq, dtype=np.float64), (n,))
    ph = phase + 2.0 * np.pi * np.cumsum(f) / RATE
    x = np.sin(ph)
    if tau is not None:
        x = x * decay(n, tau, delay)
    elif delay > 0.0:
        x[: n_of(delay)] = 0.0
    return x


def glide(start, end, n, seconds):
    """Frequency gliding exponentially from start to end over seconds, then held"""
    t = np.clip(time_axis(n) / max(seconds, 1e-4), 0.0, 1.0)
    return start * (end / start) ** t


def burst(rng, n, lo, hi, tau, delay=0.0, order=2):
    """Band-limited noise burst"""
    return bandpass(rng.standard_normal(n), lo, hi, order) * decay(n, tau, delay)


def grains(rng, n, start, length, count, lo, hi, tau=0.0015, fade=True):
    """Sparse little impacts (sand crunch, debris) over [start, start + length], thinning out when fade"""
    out = np.zeros(n)
    for _ in range(count):
        u = rng.random()
        at = start + length * (u ** 1.8 if fade else u)
        amp = rng.uniform(0.2, 1.0) * ((1.0 - (at - start) / max(length, 1e-4)) if fade else 1.0)
        i = n_of(at)
        k = min(n - i, n_of(0.012))
        if k <= 8:
            continue
        g = rng.standard_normal(k) * np.exp(-np.arange(k) / RATE / tau)
        out[i:i + k] += amp * g
    return bandpass(out, lo, hi)


def saturate(x, drive):
    return np.tanh(drive * x) / np.tanh(drive)


def rms_norm(x):
    """The layer brought to an RMS of 1, so that the mix weights are loudness ratios"""
    rms = float(np.sqrt(np.mean(x ** 2)))
    return x / rms if rms > 1e-9 else x


def harmonic_voice(rng, f0, n, harmonics, rolloff=0.9, detune=0.004):
    """Band-limited buzzy source on a pitch contour f0 (array): harmonics with 1/k^rolloff amplitudes, slightly detuned"""
    phase = 2.0 * np.pi * np.cumsum(f0) / RATE
    out = np.zeros(n)
    nyq = RATE / 2
    for k in range(1, harmonics + 1):
        if k * float(np.max(f0)) > nyq * 0.9:
            break
        d = 1.0 + rng.uniform(-detune, detune)
        out += np.sin(k * d * phase + rng.uniform(0, 2 * np.pi)) / (k ** rolloff)
    return out


def jitter(rng, n, depth, rate):
    """Slow random wobble around 1 (depth: relative), smoothed at about rate Hz"""
    steps = max(2, int(n / RATE * rate) + 2)
    points = 1.0 + rng.uniform(-depth, depth, steps)
    return np.interp(np.linspace(0, steps - 1, n), np.arange(steps), points)


def formants(x, bands, gains):
    out = np.zeros_like(x)
    for (lo, hi), g in zip(bands, gains):
        out += g * bandpass(x, lo, hi, 2)
    return out


def finish(x, peak_db, fade_ms=5.0):
    x = x - np.mean(x)
    k = max(1, int(RATE * fade_ms / 1000.0))
    x[:k] *= np.linspace(0.0, 1.0, k)
    x[-k:] *= np.linspace(1.0, 0.0, k)
    peak = float(np.max(np.abs(x))) or 1.0
    return x / peak * (10.0 ** (peak_db / 20.0))


# ---------------------------------------------------------------- the sounds

def step(rng, heavy):
    n = n_of(0.42 if heavy else 0.24)
    f_tok = rng.uniform(450, 680) if heavy else rng.uniform(700, 1250)
    click = burst(rng, n, 1500 if heavy else 2500, 5500 if heavy else 7500, 0.0016)
    tok = tone(f_tok, n, 0.02 if heavy else 0.012) + 0.45 * tone(f_tok * 2.31, n, 0.007)
    sand = lowpass(highpass(rng.standard_normal(n), 250), 2000 if heavy else 3200) * attack_decay(n, 0.002, 0.08 if heavy else 0.035)
    crunch = grains(rng, n, 0.003, 0.09 if heavy else 0.04, 18 if heavy else 9, 1200, 5000)
    f_thump = rng.uniform(55, 72) if heavy else rng.uniform(90, 130)
    thump = tone(glide(f_thump * 1.25, f_thump, n, 0.03), n, 0.075 if heavy else 0.03)
    x = 0.5 * click + 0.55 * tok + 0.55 * sand + 0.45 * crunch + (1.1 if heavy else 0.6) * thump
    if heavy:
        # the leg settles a moment later
        settle = rng.uniform(0.035, 0.06)
        x += 0.35 * (tone(f_tok * 1.1, n, 0.01, delay=settle) + 0.6 * burst(rng, n, 1500, 5000, 0.002, settle))
    return finish(x, -4.0 if heavy else -8.0)


def clack(rng):
    n = n_of(0.36)
    hits = [0.0] if rng.random() < 0.5 else [0.0, rng.uniform(0.045, 0.075)]
    x = np.zeros(n)
    for j, at in enumerate(hits):
        f1 = rng.uniform(1050, 1500)
        amp = 1.0 if j == 0 else 0.6
        modes = sum(a * tone(f1 * r, n, tau, delay=at) for r, a, tau in
                    ((1.0, 1.0, 0.026), (1.72, 0.6, 0.018), (2.63, 0.4, 0.012), (3.91, 0.25, 0.007)))
        x += amp * (modes + 0.9 * burst(rng, n, 1500, 12000, 0.0008, at) + 0.5 * tone(rng.uniform(160, 220), n, 0.025, delay=at))
    return finish(x, -3.0)


def hit(rng):
    n = n_of(0.45)
    thud = tone(glide(rng.uniform(140, 170), rng.uniform(95, 120), n, 0.05), n, 0.055)
    body = lowpass(rng.standard_normal(n), 1800) * attack_decay(n, 0.002, 0.035)
    squelch = burst(rng, n, 600, 1800, 0.06)
    click = burst(rng, n, 2500, 8000, 0.0015)
    return finish(1.0 * thud + 0.6 * body + 0.3 * squelch + 0.4 * click, -2.0)


def impact(rng, seconds, boom_from, boom_to, boom_tau, sand_cut, sand_tau, debris, crack, peak_db, second_boom=0.0):
    n = n_of(seconds)
    boom = tone(glide(boom_from, boom_to, n, 0.25), n, boom_tau)
    if second_boom > 0.0:
        boom += 0.5 * tone(glide(boom_to * 1.5, boom_to, n, 0.15), n, boom_tau * 0.45, delay=second_boom)
    thud = tone(rng.uniform(90, 120), n, 0.06)
    sand = lowpass(rng.standard_normal(n), sand_cut) * attack_decay(n, 0.005, sand_tau)
    bright = highpass(lowpass(rng.standard_normal(n), 6000), 1500) * attack_decay(n, 0.002, 0.06)
    rubble = grains(rng, n, 0.01, seconds * 0.6, debris, 400, 4000, tau=0.002)
    cracks = sum(burst(rng, n, 1200, 6000, 0.002, rng.uniform(0.0, 0.08)) for _ in range(crack))
    x = 1.0 * boom + 0.6 * thud + 0.7 * sand + 0.3 * bright + 0.35 * rubble + 0.35 * cracks
    return finish(saturate(x / (np.max(np.abs(x)) or 1.0), 1.6), peak_db)


def wave(rng):
    n = n_of(1.3)
    whoomp = lowpass(rng.standard_normal(n), 900) * attack_decay(n, 0.04, 0.35)
    sweep = sosfilt(butter(2, 400 / (RATE / 2), btype="low", output="sos"), rng.standard_normal(n)) * attack_decay(n, 0.02, 0.5)
    sub = tone(glide(60, 32, n, 0.6), n, 0.5)
    sand = lowpass(rng.standard_normal(n), 2500) * attack_decay(n, 0.08, 0.45)
    return finish(0.8 * whoomp + 0.6 * sweep + 1.0 * sub + 0.5 * sand, -1.0)


def voice(rng, seconds, f_start, f_peak, f_end, fry_hz, vowels, hiss_amount, clatter_rate, drive, peak_db, attack=0.12, release=0.7,
          sub_boom=0.0):
    """The crab's voice: a buzzing harmonic source on a pitch contour with a sub-harmonic and a rattle, through vowel
    formants that move from vowels[0] to vowels[1], a breath, a bubbling hiss and the clatter of the mandibles"""
    n = n_of(seconds)
    t = time_axis(n)
    peak_at = min(0.35, seconds * 0.2)
    contour = np.where(t < peak_at, f_start + (f_peak - f_start) * t / peak_at,
                       f_peak + (f_end - f_peak) * np.clip((t - peak_at) / max(seconds - peak_at, 1e-3), 0.0, 1.0))
    f0 = contour * jitter(rng, n, 0.03, 9.0) * (1.0 + 0.012 * np.sin(2 * np.pi * 4.5 * t))
    source = harmonic_voice(rng, f0, n, 70)
    # period doubling and vocal fry: the rattle of a throat that is not a throat
    source += 0.45 * harmonic_voice(rng, f0 * 0.5, n, 40, rolloff=1.1)
    fry = 0.55 + 0.45 * np.abs(np.sin(np.pi * np.cumsum(fry_hz * jitter(rng, n, 0.15, 6.0)) / RATE))
    source *= fry
    # vowels: open to closed (or the other way), a crossfade of two formant sets
    mix = np.clip(t / seconds, 0.0, 1.0) ** 0.8
    a = formants(source, vowels[0], (1.0, 0.7, 0.45))
    b = formants(source, vowels[1], (1.0, 0.7, 0.45))
    voiced = (1.0 - mix) * a + mix * b
    # the chest: the fundamental and its first harmonics, under the formants
    chest = lowpass(source, 380.0, 3)
    breath = bandpass(rng.standard_normal(n), 300, 3500) * fry
    bubbles = (rng.random(n) < 22.0 / RATE).astype(float)
    bubble_env = lowpass(bubbles, 40.0) * 180.0
    hiss = lowpass(highpass(rng.standard_normal(n), 3000), 9000) * (0.3 + np.clip(bubble_env, 0.0, 1.0))
    clatter_hits = (rng.random(n) < clatter_rate / RATE).astype(float)
    clatter = bandpass(np.convolve(clatter_hits, np.exp(-np.arange(n_of(0.004)) / RATE / 0.0012), mode="same"), 1500, 4500)
    # loudness ratios: the voice and its chest first, the breath, the hiss and the clatter under them
    x = (1.0 * rms_norm(voiced) + 0.8 * rms_norm(chest) + 0.22 * rms_norm(breath) + 0.35 * hiss_amount * rms_norm(hiss)
         + 0.14 * rms_norm(clatter))
    x = saturate(x / (np.max(np.abs(x)) or 1.0), drive) * swell(n, attack, release) * (1.0 + 0.1 * np.sin(2 * np.pi * 4.0 * t))
    if sub_boom > 0.0:
        x += sub_boom * tone(glide(55, 34, n, 0.4), n, 0.4)
    return finish(x, peak_db)


OPEN = ((550, 900), (1000, 1500), (2300, 2900))
CLOSED = ((300, 520), (700, 1000), (2100, 2700))
SHRIEK = ((500, 800), (1500, 2100), (2700, 3400))


def roar(rng):
    k = rng.uniform(0.92, 1.08)
    return voice(rng, rng.uniform(2.5, 3.0), 62 * k, 84 * k, 47 * k, rng.uniform(26, 34), (OPEN, CLOSED), 0.3, 16.0, 2.2, -1.0,
                 sub_boom=0.8)


def charge(rng):
    k = rng.uniform(0.95, 1.06)
    return voice(rng, rng.uniform(1.3, 1.6), 140 * k, 215 * k, 170 * k, rng.uniform(32, 40), (SHRIEK, OPEN), 0.45, 24.0, 3.0, -1.0,
                 attack=0.06, release=0.45)


def death(rng):
    n_extra = 0.6
    body = voice(rng, 3.2, 72, 80, 28, 22.0, (OPEN, CLOSED), 0.2, 9.0, 1.8, -1.0, attack=0.1, release=1.4)
    n = len(body) + n_of(n_extra)
    exhale = np.zeros(n)
    tail = bandpass(rng.standard_normal(n_of(1.6)), 900, 4200) * swell(n_of(1.6), 0.3, 1.2) * 0.35
    exhale[n - len(tail):] = tail
    x = np.concatenate([body, np.zeros(n_of(n_extra))]) + exhale
    return finish(x, -1.0)


def hiss(rng):
    n = n_of(rng.uniform(1.2, 1.6))
    t = time_axis(n)
    air = bandpass(rng.standard_normal(n), rng.uniform(2500, 3200), rng.uniform(7500, 9500))
    pops = (rng.random(n) < rng.uniform(16, 26) / RATE).astype(float)
    bubbles = np.clip(lowpass(pops, 30.0) * 200.0, 0.0, 1.0)
    wet = bandpass(np.convolve((rng.random(n) < 30.0 / RATE).astype(float), np.exp(-np.arange(n_of(0.003)) / RATE / 0.0008), mode="same"),
                   3000, 8000) * 3.0
    rumble = tone(rng.uniform(55, 70), n) * 0.12
    x = air * (0.45 + 0.55 * bubbles) + 0.4 * wet + rumble
    return finish(x * swell(n, 0.15, 0.45) * (1.0 + 0.15 * np.sin(2 * np.pi * 3.0 * t)), -4.0)


def growl(rng):
    n = n_of(rng.uniform(2.0, 2.4))
    t = time_axis(n)
    f0 = rng.uniform(38, 45) * (1.0 + 0.04 * np.sin(2 * np.pi * 0.7 * t)) * jitter(rng, n, 0.02, 5.0)
    source = harmonic_voice(rng, f0, n, 30, rolloff=1.0)
    am = (0.65 + 0.35 * np.sin(2 * np.pi * rng.uniform(6, 8) * t)) * (0.7 + 0.3 * np.abs(np.sin(np.pi * 22.0 * t)))
    x = lowpass(saturate(source * am / (np.max(np.abs(source)) or 1.0), 3.0), 1300)
    x += 0.15 * bandpass(rng.standard_normal(n), 200, 900) * am
    return finish(x * swell(n, 0.4, 0.6), -4.0)


FAMILIES = [
    # (family, count, cue of DA_GiantCrabRules, builder)
    ("Step", 6, "step_cue", lambda rng: step(rng, False)),
    ("StepHeavy", 4, "heavy_step_cue", lambda rng: step(rng, True)),
    ("Clack", 3, "clack_cue", clack),
    ("Hit", 3, "hit_cue", hit),
    ("Slam", 2, "slam_cue", lambda rng: impact(rng, 1.3, 72, 38, 0.35, 2500, 0.25, 40, 2, -1.0)),
    ("Stomp", 3, "stomp_cue", lambda rng: impact(rng, 0.8, 64, 45, 0.18, 2200, 0.12, 18, 1, -2.0)),
    ("BellySlam", 2, "belly_slam_cue", lambda rng: impact(rng, 1.9, 55, 28, 0.7, 1800, 0.7, 70, 4, -0.5, second_boom=0.12)),
    ("BodyFall", 2, "body_fall_cue", lambda rng: impact(rng, 1.0, 70, 40, 0.25, 2000, 0.35, 45, 3, -2.0)),
    ("Wave", 1, "roar_wave_cue", wave),
    ("Roar", 2, "roar_cue", roar),
    ("Charge", 2, "charge_cue", charge),
    ("Death", 1, "death_cue", death),
    ("Hiss", 3, "hiss_cue", hiss),
    ("Growl", 3, "growl_cue", growl),
]


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = {"format": "giant-crab-sounds/1", "version": VERSION, "rate": RATE, "generator": "scripts/audio/crab_sounds.py",
                "license": "original work of the project (synthesised, no external source)", "sounds": []}
    for family_index, (family, count, cue, build) in enumerate(FAMILIES):
        for index in range(count):
            seed = 2610000 + family_index * 100 + index
            rng = np.random.default_rng(seed)
            data = build(rng).astype(np.float32)
            name = f"SW_GiantCrab_{family}_{index + 1:02d}"
            path = OUT / f"{name}.wav"
            sf.write(str(path), data, RATE, subtype="PCM_16")
            manifest["sounds"].append({
                "id": name, "family": family, "cue": cue, "file": str(path.relative_to(ROOT)).replace("\\", "/"),
                "seconds": round(len(data) / RATE, 4), "peak_dbfs": round(20 * math.log10(max(float(np.max(np.abs(data))), 1e-9)), 2),
                "seed": seed, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
            print(f"{name}: {len(data) / RATE:.2f} s")
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"{len(manifest['sounds'])} sounds in {OUT}")


if __name__ == "__main__":
    main()
