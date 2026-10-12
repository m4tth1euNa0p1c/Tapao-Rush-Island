"""Sounds of the tactics of the tiki warriors (T-275, owner 2026-10-10: "rends ces mobs : crab guardian, ashwatcher,
cocowarrior [...] design comme Outlast [...] rework tous les mécanismes"): synthesised from scratch with numpy / scipy
(no recording, no external source: original work of the project, free of rights), deterministic (one seed per sound).
The signal helpers are those of scripts/audio/crab_sounds.py.

  python scripts/audio/tiki_tactics_sounds.py   -> art/audio/creatures/tiki_tactics/SW_Tiki_*.wav + manifest.json
  then the editor action "mobsmech" (scripts/editor/tiki_mechanics_setup_ue.py) imports them into
  /Game/MonkeIsland/Characters/Tiki/Audio/Tactics and binds them to DA_TikiRules (the "target" of each sound)

Families (48 kHz, 16 bit, mono, peak-normalised, 5 ms fades):
- Guerrier Coco (Stalker): Stalk (breath behind a wooden mask, clicks of fibre and shell: heard near while it circles),
  Alert (a clicking rattle), Feint (a hiss), Windup (a rising growl and creaking wood: the heavy blow comes), Flee (a
  whimpering shriek);
- Gardien Crabe (Bulwark): Alert (claw clacks and a bubbling hiss), Windup (the claws opening, faster and faster: the
  grab comes), Block (the clang of chitin parrying a blow), Break (the shell cracks), Seize (the claw closing);
- Veilleur des Cendres (Watcher): Howl (long, rising then falling, heard from far), Windup (the ash breathed in, a
  rumble and crackles: the cloud comes), Alert (a rocky growl);
- the ash itself: CloudBurst (the lump bursting), Embers (crackling, as long as a patch burns), Veil (the ringing in the
  ears of a blinded player, for that player alone).
"""
from __future__ import annotations

import hashlib
import json
import math
import sys
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import fftconvolve

sys.path.insert(0, str(Path(__file__).resolve().parent))
from crab_sounds import (  # noqa: E402  (shared signal helpers)
    RATE, attack_decay, bandpass, burst, decay, finish, glide, grains, harmonic_voice, highpass, jitter, lowpass, n_of,
    rms_norm, saturate, swell, time_axis, tone,
)

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art" / "audio" / "creatures" / "tiki_tactics"
VERSION = 1


# ---------------------------------------------------------------- shared pieces

def wood_click(rng, n, at, f_lo=520, f_hi=900, amp=1.0):
    """A hollow knock of coconut shell or wood: a few inharmonic modes and a click"""
    f1 = rng.uniform(f_lo, f_hi)
    modes = sum(a * tone(f1 * r, n, tau, delay=at) for r, a, tau in ((1.0, 1.0, 0.018), (2.3, 0.45, 0.009), (3.7, 0.2, 0.005)))
    return amp * (modes + 0.6 * burst(rng, n, 2000, 7000, 0.001, at))


def breath(rng, n, start, length, lo, hi, amount=1.0):
    """A breath through a mask: band noise under a smooth swell from start over length"""
    env = np.zeros(n)
    i, k = n_of(start), min(n - n_of(start), n_of(length))
    if k > 8:
        env[i:i + k] = swell(k, length * 0.35, length * 0.5)
    return amount * bandpass(rng.standard_normal(n), lo, hi) * env


def rasp_voice(rng, n, f0, harmonics=40, rolloff=1.0, drive=2.5):
    """A buzzing throat on a pitch contour f0 (array), saturated"""
    source = harmonic_voice(rng, f0, n, harmonics, rolloff=rolloff)
    source += 0.35 * harmonic_voice(rng, f0 * 0.5, n, harmonics // 2, rolloff=1.2)
    return saturate(source / (np.max(np.abs(source)) or 1.0), drive)


def reverb_tail(rng, x, seconds, damping_hz, wet_level=0.35):
    """A cheap diffuse tail: the sound convolved with a decaying noise, darker as it fades"""
    k = n_of(seconds)
    ir = rng.standard_normal(k) * np.exp(-time_axis(k) / (seconds * 0.28))
    ir = lowpass(ir, damping_hz)
    ir /= np.sqrt(np.sum(ir ** 2)) or 1.0
    wet = fftconvolve(x, ir)
    dry = np.concatenate([x, np.zeros(len(wet) - len(x))])
    return dry + wet_level * wet / (np.max(np.abs(wet)) or 1.0) * np.max(np.abs(x))


# ---------------------------------------------------------------- Guerrier Coco (Stalker)

def coco_stalk(rng):
    """Breath behind a wooden mask, clicks of fibre and shell: what the player hears just before he sees it"""
    seconds = rng.uniform(1.8, 2.5)
    n = n_of(seconds)
    inhale = breath(rng, n, 0.0, seconds * 0.45, 500, 2600, 0.8)
    exhale = breath(rng, n, seconds * 0.5, seconds * 0.45, 250, 1800, 1.0)
    clicks = sum(wood_click(rng, n, rng.uniform(0.1, seconds - 0.2), amp=rng.uniform(0.25, 0.6)) for _ in range(rng.integers(2, 5)))
    rustle = grains(rng, n, 0.0, seconds, 50, 2500, 9000, tau=0.0012, fade=False) * 0.35
    rattle = tone(rng.uniform(75, 95), n) * 0.06 * swell(n, 0.5, 0.6)
    x = 1.0 * rms_norm(inhale + exhale) + 0.5 * rms_norm(clicks) + 0.25 * rms_norm(rustle) + rattle
    return finish(x, -9.0)


def coco_alert(rng):
    """A clicking rattle, faster and higher: it has seen you"""
    n = n_of(0.9)
    x = np.zeros(n)
    at = 0.0
    gap = rng.uniform(0.07, 0.09)
    while at < 0.7:
        x += wood_click(rng, n, at, 700, 1200, amp=rng.uniform(0.5, 1.0))
        at += gap
        gap = max(0.025, gap * 0.86)
    hiss = bandpass(rng.standard_normal(n), 2500, 8000) * attack_decay(n, 0.25, 0.25)
    return finish(rms_norm(x) + 0.5 * rms_norm(hiss), -3.0)


def coco_feint(rng):
    """A sharp hiss and a snarl"""
    n = n_of(0.55)
    hiss = bandpass(rng.standard_normal(n), 2000, 7500) * attack_decay(n, 0.01, 0.16)
    f0 = glide(rng.uniform(150, 170), rng.uniform(110, 125), n, 0.3)
    snarl = rasp_voice(rng, n, f0 * jitter(rng, n, 0.05, 12.0), 30, 1.1, 3.0) * attack_decay(n, 0.02, 0.18)
    return finish(0.9 * rms_norm(hiss) + 0.8 * rms_norm(bandpass(snarl, 200, 3500)), -2.0)


def coco_windup(rng):
    """A rising growl and the creak of fibre bending: the heavy blow comes (about 0.9 s)"""
    n = n_of(1.0)
    t = time_axis(n)
    f0 = glide(rng.uniform(70, 80), rng.uniform(135, 150), n, 0.85) * jitter(rng, n, 0.03, 8.0)
    growl = bandpass(rasp_voice(rng, n, f0, 40, 1.0, 3.2), 120, 3200) * swell(n, 0.5, 0.15)
    # stick-slip: the knocks of the wood come faster as it bends
    creak = np.zeros(n)
    at, gap = 0.05, 0.11
    while at < 0.85:
        creak += wood_click(rng, n, at, 300, 520, amp=0.3 + 0.7 * at)
        at += gap
        gap = max(0.018, gap * 0.8)
    x = 1.0 * rms_norm(growl) + 0.45 * rms_norm(creak)
    return finish(x * (0.7 + 0.3 * t / t[-1]), -1.5)


def coco_flee(rng):
    """A whimpering shriek, the voice breaking down"""
    n = n_of(0.95)
    t = time_axis(n)
    f0 = glide(rng.uniform(420, 470), rng.uniform(230, 260), n, 0.8) * (1.0 + 0.05 * np.sin(2 * np.pi * 9.0 * t))
    voice = bandpass(rasp_voice(rng, n, f0, 25, 1.2, 1.8), 300, 4500) * swell(n, 0.05, 0.4)
    sob = bandpass(rng.standard_normal(n), 800, 3000) * (np.abs(np.sin(2 * np.pi * 5.5 * t)) ** 3) * swell(n, 0.05, 0.4)
    return finish(rms_norm(voice) + 0.35 * rms_norm(sob), -2.0)


# ---------------------------------------------------------------- Gardien Crabe (Bulwark)

def chitin(rng, n, at, f_lo, f_hi, amp=1.0, ring=1.0):
    """A hard shell struck: inharmonic modes, a bright click, a body thump"""
    f1 = rng.uniform(f_lo, f_hi)
    modes = sum(a * tone(f1 * r, n, tau * ring, delay=at) for r, a, tau in
                ((1.0, 1.0, 0.03), (1.62, 0.7, 0.022), (2.41, 0.5, 0.015), (3.53, 0.3, 0.009), (5.1, 0.15, 0.005)))
    return amp * (modes + 0.8 * burst(rng, n, 2500, 12000, 0.0008, at) + 0.4 * tone(rng.uniform(150, 210), n, 0.03, delay=at))


def crab_alert(rng):
    """Two or three quick clacks and a bubbling hiss"""
    n = n_of(0.95)
    x = sum(chitin(rng, n, at, 1100, 1500, amp=a) for at, a in ((0.0, 1.0), (rng.uniform(0.08, 0.11), 0.8), (rng.uniform(0.2, 0.26), 0.6)))
    pops = (rng.random(n) < 24.0 / RATE).astype(float)
    bubbles = np.clip(lowpass(pops, 30.0) * 200.0, 0.0, 1.0)
    hiss = bandpass(rng.standard_normal(n), 2800, 9000) * (0.4 + 0.6 * bubbles) * swell(n, 0.25, 0.4)
    return finish(rms_norm(x) + 0.45 * rms_norm(hiss), -2.0)


def crab_windup(rng):
    """The claws opening: clacks faster and faster, a creak of joints (about 0.8 s)"""
    n = n_of(0.9)
    x = np.zeros(n)
    at, gap = 0.0, 0.16
    while at < 0.78:
        x += chitin(rng, n, at, 1200, 1700, amp=0.5 + 0.5 * at / 0.78, ring=0.7)
        at += gap
        gap = max(0.035, gap * 0.78)
    joint = bandpass(rng.standard_normal(n), 400, 1400) * swell(n, 0.4, 0.2) * (1.0 + np.sin(2 * np.pi * 37.0 * time_axis(n)))
    return finish(rms_norm(x) + 0.3 * rms_norm(joint), -2.0)


def crab_block(rng):
    """The clang of a claw parrying a blade or a ball: short, hard, ringing a little"""
    n = n_of(0.45)
    x = chitin(rng, n, 0.0, 1000, 1500, amp=1.0, ring=1.6)
    body = tone(glide(240, 180, n, 0.08), n, 0.06)
    scrape = bandpass(rng.standard_normal(n), 2000, 6000) * attack_decay(n, 0.001, 0.03)
    return finish(saturate(rms_norm(lowpass(x, 7000)) + 0.5 * rms_norm(body) + 0.2 * rms_norm(scrape), 1.2), -1.0)


def crab_break(rng):
    """The shell cracks: dry cracks, a heavy thump, a grunt"""
    n = n_of(0.9)
    cracks = sum(burst(rng, n, 1500, 7000, 0.003, rng.uniform(0.0, 0.07)) for _ in range(5))
    thump = tone(glide(130, 80, n, 0.1), n, 0.09)
    f0 = glide(rng.uniform(95, 110), rng.uniform(70, 80), n, 0.4)
    grunt = bandpass(rasp_voice(rng, n, f0, 30, 1.1, 2.6), 150, 2500) * attack_decay(n, 0.05, 0.25, delay=0.08)
    x = 0.6 * rms_norm(cracks) + 1.0 * rms_norm(thump) + 1.0 * rms_norm(grunt)
    return finish(saturate(x / (np.max(np.abs(x)) or 1.0), 2.0), -1.0)


def crab_seize(rng):
    """The claw closing on a body: a clack, a crunch, a wet squeeze"""
    n = n_of(0.55)
    clack = chitin(rng, n, 0.0, 1000, 1300, amp=1.0, ring=0.8)
    crunch = grains(rng, n, 0.01, 0.15, 30, 800, 5000, tau=0.0018)
    squelch = burst(rng, n, 500, 1600, 0.09, 0.03)
    return finish(rms_norm(clack) + 0.7 * rms_norm(crunch) + 0.5 * rms_norm(squelch), -1.5)


# ---------------------------------------------------------------- Veilleur des Cendres (Watcher)

def ash_howl(rng):
    """A long howl, rising then falling, breathy and rough, its tail in the distance (heard from far)"""
    seconds = rng.uniform(2.6, 3.0)
    n = n_of(seconds)
    t = time_axis(n)
    k = rng.uniform(0.94, 1.06)
    up = seconds * 0.35
    contour = np.where(t < up, 170 * k + (330 - 170) * k * (t / up) ** 0.7,
                       330 * k - (330 - 150) * k * np.clip((t - up) / (seconds - up), 0.0, 1.0) ** 1.3)
    f0 = contour * jitter(rng, n, 0.015, 4.0) * (1.0 + 0.02 * np.sin(2 * np.pi * 5.0 * t))
    source = harmonic_voice(rng, f0, n, 30, rolloff=1.1)
    # vowels: "oo" to "aa" to "oo"
    oo = bandpass(source, 280, 900) + 0.5 * bandpass(source, 700, 1200)
    aa = bandpass(source, 650, 1300) + 0.6 * bandpass(source, 1100, 1900)
    open_amount = np.sin(np.pi * np.clip(t / seconds, 0.0, 1.0)) ** 2
    voiced = (1.0 - open_amount) * oo + open_amount * aa
    rasp = bandpass(rng.standard_normal(n), 1500, 6000) * (0.5 + 0.5 * np.abs(np.sin(np.pi * np.cumsum(f0 * 0.25) / RATE)))
    rumble = tone(f0 * 0.5, n) * 0.25
    x = (1.0 * rms_norm(lowpass(voiced, 3000, 3)) + 0.12 * rms_norm(rasp) + rumble) * swell(n, 0.35, 0.9)
    x = reverb_tail(rng, lowpass(x, 5000), 1.4, 1800)
    return finish(x, -0.5)


def ash_windup(rng):
    """The ash breathed in: a deep rising rumble, a hiss drawn in, embers crackling (about 0.9 s)"""
    n = n_of(1.0)
    t = time_axis(n)
    inhale = lowpass(rng.standard_normal(n), 800) * (t / t[-1]) ** 1.5
    draw = bandpass(rng.standard_normal(n), 1500, 6000) * swell(n, 0.6, 0.1)
    rumble = tone(glide(45, 70, n, 0.9), n) * swell(n, 0.3, 0.1)
    crackle = grains(rng, n, 0.05, 0.9, 40, 1500, 7000, tau=0.0009, fade=False)
    return finish(0.9 * rms_norm(inhale) + 0.5 * rms_norm(draw) + 0.8 * rms_norm(rumble) + 0.5 * rms_norm(crackle), -1.5)


def ash_alert(rng):
    """A rocky growl, grinding"""
    n = n_of(0.85)
    t = time_axis(n)
    f0 = rng.uniform(55, 65) * (1.0 + 0.05 * np.sin(2 * np.pi * 3.0 * t)) * jitter(rng, n, 0.03, 6.0)
    growl = lowpass(rasp_voice(rng, n, f0, 30, 1.0, 3.5), 1500) * swell(n, 0.1, 0.3)
    grind = grains(rng, n, 0.0, 0.8, 60, 300, 2500, tau=0.003, fade=False)
    return finish(rms_norm(growl) + 0.4 * rms_norm(grind), -2.0)


# ---------------------------------------------------------------- the ash

def cloud_burst(rng):
    """The lump bursting into a cloud: a soft thump, a whoosh of ash, sizzling grains"""
    n = n_of(1.3)
    thump = tone(glide(90, 55, n, 0.15), n, 0.12)
    whoosh = lowpass(rng.standard_normal(n), 1400) * attack_decay(n, 0.03, 0.35)
    puff = bandpass(rng.standard_normal(n), 400, 3000) * attack_decay(n, 0.01, 0.15)
    sizzle = grains(rng, n, 0.05, 1.1, 70, 2500, 9000, tau=0.0008)
    return finish(0.8 * rms_norm(thump) + 1.0 * rms_norm(whoosh) + 0.5 * rms_norm(puff) + 0.35 * rms_norm(sizzle), -2.0)


def embers(rng):
    """Embers crackling on the sand, as long as a patch burns (about 5 s)"""
    seconds = 5.0
    n = n_of(seconds)
    crackle = grains(rng, n, 0.0, seconds, 260, 1200, 8000, tau=0.0011, fade=False)
    pops = grains(rng, n, 0.0, seconds, 30, 300, 1800, tau=0.004, fade=False)
    bed = bandpass(rng.standard_normal(n), 2500, 7000) * 0.4
    hum = lowpass(rng.standard_normal(n), 250) * 0.5
    x = (rms_norm(crackle) + 0.6 * rms_norm(pops) + 0.25 * rms_norm(bed) + 0.3 * rms_norm(hum)) * swell(n, 0.08, 1.2)
    return finish(x, -5.0)


def veil(rng):
    """The ringing in the ears of a blinded player, the world muffled under it (2D, for that player alone)"""
    n = n_of(2.6)
    t = time_axis(n)
    ring = tone(rng.uniform(3000, 3400), n) * np.exp(-t / 1.4) * (1.0 + 0.04 * np.sin(2 * np.pi * 6.0 * t))
    muffle = lowpass(rng.standard_normal(n), 300) * swell(n, 0.05, 1.5)
    heart = sum(tone(55, n, 0.08, delay=at) for at in (0.15, 0.45, 1.05, 1.35, 1.95, 2.25))
    return finish(0.7 * rms_norm(ring) + 0.6 * rms_norm(muffle) + 0.5 * rms_norm(heart), -8.0)


# family, role, count, target in DA_TikiRules (python property path), builder
FAMILIES = [
    ("Coco_Stalk", 4, "stalker.sounds.stalk", coco_stalk),
    ("Coco_Alert", 2, "stalker.sounds.alert", coco_alert),
    ("Coco_Feint", 2, "stalker.sounds.feint", coco_feint),
    ("Coco_Windup", 2, "stalker.sounds.windup", coco_windup),
    ("Coco_Flee", 2, "stalker.sounds.flee", coco_flee),
    ("Crab_Alert", 2, "bulwark.sounds.alert", crab_alert),
    ("Crab_Windup", 2, "bulwark.sounds.windup", crab_windup),
    ("Crab_Block", 4, "bulwark.sounds.block", crab_block),
    ("Crab_Break", 2, "bulwark.sounds.guard_break", crab_break),
    ("Crab_Seize", 2, "bulwark.sounds.seize", crab_seize),
    ("Ash_Howl", 2, "watcher.sounds.howl", ash_howl),
    ("Ash_Windup", 2, "watcher.sounds.windup", ash_windup),
    ("Ash_Alert", 1, "watcher.sounds.alert", ash_alert),
    ("Ash_CloudBurst", 2, "watcher.ash.burst_sound", cloud_burst),
    ("Ash_Embers", 2, "watcher.ash.ember_sound", embers),
    ("Ash_Veil", 1, "watcher.ash.veil_sound", veil),
]

# families heard from far (the howl) and the one played in 2D for the blinded player only
FAR = {"Ash_Howl"}
TWO_D = {"Ash_Veil"}


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    manifest = {"format": "tiki-tactics-sounds/1", "version": VERSION, "rate": RATE, "generator": "scripts/audio/tiki_tactics_sounds.py",
                "license": "original work of the project (synthesised, no external source)", "far": sorted(FAR), "two_d": sorted(TWO_D),
                "sounds": []}
    for family_index, (family, count, target, build) in enumerate(FAMILIES):
        for index in range(count):
            seed = 2750000 + family_index * 100 + index
            rng = np.random.default_rng(seed)
            data = build(rng).astype(np.float32)
            if not np.isfinite(data).all():
                raise ValueError(f"{family} {index}: not a number in the signal")
            name = f"SW_Tiki_{family}_{index + 1:02d}"
            path = OUT / f"{name}.wav"
            sf.write(str(path), data, RATE, subtype="PCM_16")
            manifest["sounds"].append({
                "id": name, "family": family, "target": target, "file": str(path.relative_to(ROOT)).replace("\\", "/"),
                "seconds": round(len(data) / RATE, 4), "peak_dbfs": round(20 * math.log10(max(float(np.max(np.abs(data))), 1e-9)), 2),
                "seed": seed, "sha256": hashlib.sha256(path.read_bytes()).hexdigest()})
            print(f"{name}: {len(data) / RATE:.2f} s")
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    print(f"{len(manifest['sounds'])} sounds in {OUT}")


if __name__ == "__main__":
    main()
