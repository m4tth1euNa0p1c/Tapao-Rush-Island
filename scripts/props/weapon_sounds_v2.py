"""Synthesises the sounds of the weapons v2 (owner request 2026-10-09: "nouvel audio" for the pistol, the double pistol,
the pump shotgun, the musket, the blunderbuss, the rifle, the grenade and the ammunition packets), on the scheme of
musket_sounds.py and shotgun_shot_sound.py: numpy only, no recorded asset, so no licence to track.

  python scripts/props/weapon_sounds_v2.py [--only "SW_Grenade_*"] [--out DIR] [--json FILE]
      ->  art/audio/weapons/SW_*.wav (48 kHz mono 16 bit) and a table of what was written

Deterministic: every sound draws from its own generator seeded by its name, so adding or changing one never changes the
others. Levels are set against SW_Musket_Shot (2.6 s, peak -1 dBFS, loudest 100 ms about -13 dB): the shots and the
blast peak at -1 dBFS, the lock work at -6, the handling (cloth, pours, rustles) at -10 to -14; the script prints the
measured peak, loudest 100 ms and spectral centroid of every file so the balance can be checked without an ear.
Kept as they are (not rewritten here): SW_Musket_Shot (the heavy, round musket shot: the reference), the musket lock set
SW_Musket_Lock / Cock / Frizzen / Prime / Powder / Ball / RamrodDraw / RamStroke (reused by the long guns) and
SW_Shotgun_Fire (T-167). Import and wiring: scripts/editor/weapons_audio_ue.py (lead session action "weaponsaudio").

Shots (FireSound, played at the muzzle at the Fire / Shot event of the clip)
  SW_Pistol_Fire            a short flintlock shot: the pan flashes ("pff"), a tight crack, a small boom, a slap echo at 80 ms
  SW_DoublePistol_Fire      the same from a heavier piece (variant: another seed, more body, a lower boom)
  SW_Rifle_Shot             dry and whip-like: the double crack of the fast ball, a snap, a small boom, a far second echo
  SW_Blunderbuss_Shot       enormous and low: a 48 -> 27 Hz boom, a wide body, a long rumble, the hail of shot whizzing out
                            and striking the sand around
Lock work of the pistols (shorter and higher than the musket's)
  SW_Pistol_CockHammer      the two detents of the tumbler (events CockHammer and HammerCocked)
  SW_Pistol_DryClick        the hammer falls, flint on the steel, no powder (DryClick)
  SW_Pistol_PaperPickup     the pouch flap, a paper cartridge taken (PaperPickup)
  SW_Pistol_PaperTransfer   the cartridge torn, powder down the barrel, the ball dropped in (PaperTransfer)
  SW_Pistol_RamrodGesture   the short rod drawn, two strokes (RamrodGesture)
  SW_Pistol_RamrodReturned  the rod back in its pipe (RamrodReturned)
Pump shotgun (a wooden fore-end sliding on a steel tube)
  SW_Shotgun_PumpBack       the fore-end slid back, the stop clack (PumpBackStart; PumpBackEnd is baked in)
  SW_Shotgun_EjectSpentShell  the spent shell flicked out, its landing 0.55 s later (EjectSpentShell)
  SW_Shotgun_PumpForward    the fore-end slid forward (PumpForwardStart)
  SW_Shotgun_ChamberSlug    the bolt home, the round seated (ChamberSlug)
  SW_Shotgun_ShellPickup    the hand on the bandolier, two shells clicking (ShellPickup)
  SW_Shotgun_InsertSlug     a shell pushed into the tube, the latch click (InsertSlug)
  SW_Shotgun_DryClick       the heavy hammer on an empty chamber (DryClick)
Long guns: the phases the musket set had no sound for
  SW_Musket_HalfCock        one soft detent (HalfCockStart)
  SW_Musket_StockLowered    the butt set on the ground (StockLowered)
  SW_Musket_RamrodTurn      the rod flipped in the hand (RamrodTurn)
  SW_Musket_RamrodReturn    the rod slid back into its pipes (RamrodReturn)
  SW_Blunderbuss_ShotPour   lead shot poured down the wide barrel (ShotPour)
  SW_Rifle_PatchBox         the brass patch box opened on the stock, a patch taken, the lid shut (PatchBox)
Grenade (a grenado: an iron ball whose fuse is lit at a slow match)
  SW_Grenade_Light          the fuse catches: a hiss, a sputter, then it crackles for the 3 s of FuseSeconds (Light)
  SW_Grenade_Fuse           the crackle alone, a 1.5 s seamless loop (for a fuse sound on the projectile, see the doc)
  SW_Grenade_Throw          the arm's whoosh and the sleeve snapping (FireSound: the Fire event is the throw)
  SW_Grenade_Bounce         cast iron on the ground (BounceSound; the game scales its volume by the impact speed)
  SW_Grenade_Explosion      the blast: sub boom, body, crack, rumble, falling clods and pebbles, a sand shower, two echoes
  SW_Grenade_BagReach       the hand in the canvas bag, grenades knocking (BagReach)
  SW_Grenade_Take           one taken out of the bag (HammerCocked of the Reload clip)
  SW_Grenade_DryClick       the empty bag patted (DryClick, and Empty of the Discard clip)
  SW_Grenade_Release        the empty bag tossed away, landing on the ground (Release of the Discard clip)
Shared by every firearm
  SW_Firearm_DrawWeapon     taken out: a cloth slide, a strap buckle, the piece settling in the hands (DrawWeapon)
  SW_Firearm_StowWeapon     put away (StowWeapon)
  SW_Firearm_Discard_Empty  the lock snaps on nothing, the empty pouch shaken (Empty of the Discard clip)
  SW_Firearm_Discard_Release  tossed away: a swish, then wood and steel clattering on the ground 0.4 s later (Release)
Ammunition
  SW_Ammo_Pickup            a packet picked up: lead balls and cartridges clinking in leather, the strap shut (no C++
                            field plays it yet: see docs/features/armes-nouvelles.md, section Audio)
"""
import argparse
import fnmatch
import json
import os
import wave
import zlib

import numpy as np

RATE = 48000
OUT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "art", "audio", "weapons"))
BASE_SEED = 20261009


# ---------------------------------------------------------------- building blocks

def seeded(name):
    """One generator per sound, seeded by its name: adding a sound never changes the others"""
    return np.random.default_rng(BASE_SEED + zlib.crc32(name.encode("utf-8")))


def axis(seconds):
    return np.arange(int(round(seconds * RATE))) / RATE


def band_gain(n, low, high, order=4):
    """Gain of a band between low and high (Hz) over the rfft bins of n samples, smooth edges"""
    freqs = np.fft.rfftfreq(n, 1.0 / RATE)
    f = np.maximum(freqs, 1.0)
    gain = 1.0 / np.sqrt(1.0 + (low / f) ** (2 * order)) / np.sqrt(1.0 + (f / high) ** (2 * order))
    gain[0] = 0.0
    return gain


def band_noise(rng, n, low, high, order=4):
    """White noise kept between low and high (Hz), unit peak; periodic over n, so it loops"""
    gain = band_gain(n, low, high, order)
    spectrum = gain * np.exp(2j * np.pi * rng.random(len(gain)))
    x = np.fft.irfft(spectrum, n)
    return x / (np.max(np.abs(x)) + 1e-12)


def band_filter(x, low, high, order=4):
    """x kept between low and high (Hz), zero phase (circular)"""
    n = len(x)
    return np.fft.irfft(np.fft.rfft(x) * band_gain(n, low, high, order), n)


def env(t, attack, decay, start=0.0):
    """Linear attack then exponential decay from start (s); zero before it"""
    tt = t - start
    shape = np.minimum(np.maximum(tt, 0.0) / max(attack, 1e-5), 1.0) * np.exp(-np.maximum(tt, 0.0) / decay)
    return np.where(tt >= 0.0, shape, 0.0)


def bell(t, center, width):
    return np.exp(-((t - center) / width) ** 2)


def gate(t, start, end, ramp=0.01):
    """1 between start and end (s), linear ramps of ramp s on both sides"""
    return np.clip((t - start) / ramp, 0.0, 1.0) * np.clip((end - t) / ramp, 0.0, 1.0)


def glide(t, f0, f1, tau, start=0.0):
    """Sine whose frequency falls from f0 to f1 (Hz) with the time constant tau (s), from start (phase 0 there)"""
    tt = np.maximum(t - start, 0.0)
    f = (f1 + (f0 - f1) * np.exp(-tt / tau)) * (t >= start)
    return np.sin(2.0 * np.pi * np.cumsum(f) / RATE)


def sweep(t, f0, f1, start, end):
    """Sine gliding linearly from f0 to f1 (Hz) between start and end (s), gated"""
    f = (f0 + (f1 - f0) * np.clip((t - start) / max(end - start, 1e-5), 0.0, 1.0)) * (t >= start)
    return np.sin(2.0 * np.pi * np.cumsum(f) / RATE) * gate(t, start, end, 0.02)


def ring(t, freqs, decay, start=0.0, weights=None):
    """Steel or iron: a few partials, the higher ones dying first, from start (s)"""
    tt = np.maximum(t - start, 0.0)
    on = (t >= start).astype(float)
    out = np.zeros_like(t)
    for i, f in enumerate(freqs):
        w = weights[i] if weights is not None else 1.0 / (1 + i)
        out += w * np.sin(2.0 * np.pi * f * tt) * np.exp(-tt / (decay / (1 + 0.6 * i)))
    return out * on


def crackle(rng, t, low, high, density, start, end, gain=1.0, shape=None):
    """Sparse clicks between start and end (s), density per second, each a band limited impulse; unit peak * gain"""
    n = len(t)
    mask = (rng.random(n) < density / RATE) & (t >= start) & (t < end)
    amp = rng.standard_normal(n) * mask
    if shape is not None:
        amp = amp * shape
    x = band_filter(amp, low, high)
    return x / (np.max(np.abs(x)) + 1e-12) * gain


def echo(x, delay, gain_db, high=1500.0, low=40.0):
    """A darker slap back of x, delay s later"""
    out = np.zeros_like(x)
    d = int(delay * RATE)
    if d < len(x):
        out[d:] = band_filter(x, low, high)[: len(x) - d] * 10 ** (gain_db / 20.0)
    return out


def saturate(x, drive):
    """Gentle tanh saturation: a dense body without a harsh clip (unit peak out)"""
    x = x / (np.max(np.abs(x)) + 1e-12)
    return np.tanh(x * drive) / np.tanh(drive)


def fades(x, head=0.0005, tail=0.02):
    """No click at the start, a cosine fade at the end"""
    x = x.copy()
    h = min(int(head * RATE), len(x))
    k = min(int(tail * RATE), len(x))
    if h > 1:
        x[:h] *= np.linspace(0.0, 1.0, h)
    if k > 1:
        x[-k:] *= 0.5 + 0.5 * np.cos(np.linspace(0.0, np.pi, k))
    return x


# ---------------------------------------------------------------- shots

def pistol_fire(rng, heavy=False):
    t = axis(1.5 if heavy else 1.3)
    n = len(t)
    s = 0.014  # the pan flashes first, then the charge goes off
    pan = band_noise(rng, n, 2500, 9000) * env(t, 0.004, 0.012) * 0.22
    crack = band_noise(rng, n, 1500, 10000) * env(t, 0.0003, 0.006, s) * 1.5
    body = band_noise(rng, n, 300, 2600) * env(t, 0.001, 0.055 if heavy else 0.045, s) * (1.5 if heavy else 1.3)
    boom = glide(t, 115 if heavy else 130, 65 if heavy else 75, 0.03, s) * env(t, 0.002, 0.09 if heavy else 0.075, s) * (0.75 if heavy else 0.6)
    tail = band_noise(rng, n, 250, 1600) * env(t, 0.01, 0.2 if heavy else 0.16, s) * 0.45
    x = pan + crack + body + boom + tail
    x = x + echo(x, 0.08, -12.0, 1500.0)
    return fades(saturate(x, 1.4), tail=0.12)


def rifle_shot(rng):
    t = axis(2.0)
    n = len(t)
    crack = band_noise(rng, n, 2200, 11000) * env(t, 0.0003, 0.007) * 1.6
    whip = band_noise(rng, n, 3000, 12000) * env(t, 0.0002, 0.004, 0.003) * 0.9  # the fast patched ball
    snap = band_noise(rng, n, 500, 3200) * env(t, 0.001, 0.035) * 1.2
    boom = glide(t, 95, 58, 0.04) * env(t, 0.002, 0.11) * 0.7
    body = band_noise(rng, n, 120, 900) * env(t, 0.002, 0.07) * 1.0
    tail = band_noise(rng, n, 200, 1600) * env(t, 0.02, 0.22) * 0.5
    x = crack + whip + snap + boom + body + tail
    x = x + echo(x, 0.09, -10.0, 1800.0) + echo(x, 0.36, -17.0, 900.0)
    return fades(saturate(x, 1.25), tail=0.2)


def blunderbuss_shot(rng):
    t = axis(2.8)
    n = len(t)
    crack = band_noise(rng, n, 700, 6000) * env(t, 0.0005, 0.014) * 1.3
    body = band_noise(rng, n, 70, 1400) * env(t, 0.003, 0.19) * 3.0
    boom = (glide(t, 48, 27, 0.08) + 0.6 * glide(t, 80, 50, 0.06)) * env(t, 0.004, 0.38) * 1.4
    rumble = band_noise(rng, n, 50, 420) * env(t, 0.05, 0.9) * 1.3
    x = crack + body + boom + rumble
    # the hail of shot: pellets whizzing out, then striking the sand and the leaves around
    for _ in range(18):
        start = 0.02 + rng.random() * 0.16
        x += glide(t, 3400, 1300, 0.008, start) * env(t, 0.002, 0.012, start) * rng.uniform(0.05, 0.16)
    hits = band_noise(rng, n, 1000, 6500)
    for _ in range(28):
        start = 0.08 + rng.gamma(2.0, 0.08)
        x += hits * env(t, 0.0003, 0.004, start) * rng.uniform(0.08, 0.3) * np.exp(-start / 0.4)
    x = x + echo(x, 0.13, -9.0, 1100.0) + echo(x, 0.31, -16.0, 600.0)
    return fades(saturate(x, 1.8), tail=0.3)


def grenade_explosion(rng):
    t = axis(3.0)
    n = len(t)
    sub = (glide(t, 40, 24, 0.12) + 0.5 * glide(t, 64, 40, 0.1)) * env(t, 0.004, 0.5) * 1.6
    body = band_noise(rng, n, 60, 800) * env(t, 0.003, 0.25) * 3.2
    crack = band_noise(rng, n, 900, 6000) * env(t, 0.0005, 0.012) * 1.2
    rumble = band_noise(rng, n, 70, 500) * env(t, 0.08, 1.2) * 1.2
    shower = band_noise(rng, n, 2000, 8000) * bell(t, 0.45, 0.25) * 0.28  # sand thrown up, falling back
    x = sub + body + crack + rumble + shower
    pebbles = band_noise(rng, n, 500, 5000)
    for _ in range(36):  # clods and pebbles landing, fewer and fewer
        start = 0.2 + rng.gamma(2.2, 0.22)
        gain = rng.uniform(0.1, 0.4) * np.exp(-start / 0.9)
        x += pebbles * env(t, 0.0005, rng.uniform(0.006, 0.02), start) * gain
        if rng.random() < 0.35:
            x += ring(t, [rng.uniform(1500, 3000), rng.uniform(3200, 5000)], 0.03, start) * gain * 0.5
    x = x + echo(x, 0.17, -9.0, 900.0) + echo(x, 0.42, -15.0, 500.0)
    return fades(saturate(x, 2.0), tail=0.35)


# ---------------------------------------------------------------- pistol lock work

def pistol_cock(rng):
    t = axis(0.22)
    n = len(t)
    tick = band_noise(rng, n, 2500, 11000)
    x = tick * env(t, 0.0002, 0.003) * 0.6 + ring(t, [4200, 6300, 8900], 0.014) * 0.3
    x += band_noise(rng, n, 3000, 7000) * env(t, 0.01, 0.03, 0.02) * 0.12  # the mainspring creaks between the notches
    x += tick * env(t, 0.0002, 0.004, 0.075) * 1.0 + ring(t, [3600, 5500, 8100], 0.02, 0.075) * 0.5
    return fades(x)


def pistol_dryclick(rng):
    t = axis(0.16)
    n = len(t)
    x = band_noise(rng, n, 2500, 10000) * env(t, 0.0002, 0.003) * 1.0 + ring(t, [3900, 5800, 8400], 0.02) * 0.4
    x += band_noise(rng, n, 3000, 9000) * env(t, 0.002, 0.018, 0.004) * 0.45  # the flint scrapes down the steel
    x += ring(t, [2900, 4800], 0.015, 0.012) * 0.3 + band_noise(rng, n, 1000, 4000) * env(t, 0.0003, 0.004, 0.012) * 0.4  # the frizzen kicked open
    return fades(x)


def pistol_paper_pickup(rng):
    t = axis(0.36)
    n = len(t)
    x = band_noise(rng, n, 200, 1500) * env(t, 0.01, 0.04) * 0.5 + band_noise(rng, n, 300, 2500) * bell(t, 0.05, 0.03) * 0.25  # pouch flap
    x += crackle(rng, t, 2000, 9000, 260.0, 0.08, 0.32, 0.9, shape=bell(t, 0.17, 0.09))  # the paper
    return fades(x, tail=0.04)


def pistol_paper_transfer(rng):
    t = axis(0.48)
    n = len(t)
    x = crackle(rng, t, 2000, 9000, 520.0, 0.0, 0.12, 1.0, shape=env(t, 0.01, 0.06))  # torn with the teeth
    pour = gate(t, 0.12, 0.31, 0.02) * (0.3 + 0.7 * bell(t, 0.21, 0.06))
    x += band_noise(rng, n, 1500, 7000) * pour * 0.35 + crackle(rng, t, 3000, 9000, 350.0, 0.12, 0.31, 0.5, shape=pour)
    x += ring(t, [5200, 7900], 0.004, 0.33) * 0.5 + band_noise(rng, n, 3000, 9000) * env(t, 0.0002, 0.002, 0.33) * 0.5  # the ball at the muzzle
    x += band_noise(rng, n, 150, 500) * env(t, 0.001, 0.02, 0.36) * 0.8 + glide(t, 190, 150, 0.02, 0.36) * env(t, 0.001, 0.03, 0.36) * 0.5  # down on the powder
    return fades(x, tail=0.05)


def pistol_ramrod(rng):
    t = axis(0.9)
    n = len(t)
    draw = gate(t, 0.0, 0.2, 0.02)
    x = band_noise(rng, n, 2500, 9000) * draw * (0.3 + 0.7 * np.clip(t / 0.2, 0.0, 1.0)) * 0.5 + sweep(t, 2600, 4800, 0.0, 0.2) * 0.08
    x += ring(t, [2400, 4100, 6200], 0.09, 0.19) * 0.35  # the rod rings free of its pipe
    knock = band_noise(rng, n, 150, 700)
    for start, g in ((0.45, 1.0), (0.68, 0.85)):  # two strokes on the charge
        x += knock * env(t, 0.001, 0.02, start) * 1.4 * g + ring(t, [1100, 1900, 3300], 0.05, start) * 0.4 * g
        x += glide(t, 210, 160, 0.02, start) * env(t, 0.001, 0.04, start) * 0.6 * g
    return fades(x, tail=0.06)


def pistol_ramrod_return(rng):
    t = axis(0.32)
    n = len(t)
    back = gate(t, 0.0, 0.22, 0.02)
    x = band_noise(rng, n, 2000, 8000) * back * (1.0 - 0.6 * np.clip(t / 0.22, 0.0, 1.0)) * 0.5 + sweep(t, 3400, 2200, 0.0, 0.22) * 0.06
    x += band_noise(rng, n, 1000, 4000) * env(t, 0.0002, 0.003, 0.23) * 0.8 + ring(t, [3000, 5100], 0.02, 0.23) * 0.5
    return fades(x, tail=0.04)


# ---------------------------------------------------------------- pump shotgun

def shotgun_pump_back(rng):
    t = axis(0.46)
    n = len(t)
    slide = gate(t, 0.0, 0.31, 0.03)
    x = band_noise(rng, n, 800, 4000) * slide * (0.5 + 0.5 * bell(t, 0.15, 0.1)) * 0.5 + sweep(t, 1800, 1500, 0.0, 0.31) * 0.05
    x += band_noise(rng, n, 500, 3000) * env(t, 0.0003, 0.004, 0.33) * 1.2 + ring(t, [1500, 2400, 3900], 0.05, 0.33) * 0.6  # the stop
    x += band_noise(rng, n, 120, 500) * env(t, 0.001, 0.025, 0.33) * 0.9  # the wood of the fore-end
    return fades(x, tail=0.05)


def shotgun_eject(rng):
    t = axis(0.82)
    n = len(t)
    x = ring(t, [4600, 7200, 9800], 0.015) * 0.6 + band_noise(rng, n, 3000, 11000) * env(t, 0.0002, 0.002) * 0.7  # the extractor flicks it
    for start, g in ((0.04, 0.3), (0.09, 0.2)):  # it tumbles out
        x += ring(t, [5200, 8100], 0.006, start) * g
    for start, g in ((0.55, 1.0), (0.63, 0.5)):  # brass and paper on the ground, a small hop
        x += band_noise(rng, n, 300, 1400) * env(t, 0.0003, 0.008, start) * 0.5 * g + ring(t, [2200, 3500], 0.02, start) * 0.25 * g
    return fades(x, tail=0.05)


def shotgun_pump_forward(rng):
    t = axis(0.3)
    n = len(t)
    slide = gate(t, 0.0, 0.26, 0.03)
    x = band_noise(rng, n, 800, 4000) * slide * (0.5 + 0.5 * bell(t, 0.12, 0.08)) * 0.5 + sweep(t, 1500, 1900, 0.0, 0.26) * 0.05
    return fades(x, tail=0.04)


def shotgun_chamber(rng):
    t = axis(0.3)
    n = len(t)
    x = band_noise(rng, n, 400, 2500) * env(t, 0.0003, 0.005) * 1.2 + ring(t, [1300, 2100, 3500], 0.06) * 0.7  # the bolt home
    x += band_noise(rng, n, 100, 400) * env(t, 0.001, 0.03) * 0.8
    x += ring(t, [2600, 4100], 0.03, 0.025) * 0.4 + band_noise(rng, n, 1500, 6000) * env(t, 0.0002, 0.003, 0.025) * 0.6  # the round seated
    return fades(x, tail=0.05)


def shotgun_shell_pickup(rng):
    t = axis(0.3)
    n = len(t)
    x = band_noise(rng, n, 300, 2000) * gate(t, 0.0, 0.14, 0.02) * (1.0 + 0.5 * np.sin(2.0 * np.pi * 40.0 * t)) * 0.35  # the leather
    for start, g in ((0.08, 1.0), (0.14, 0.7)):  # two shells against each other
        x += ring(t, [3900, 5600], 0.008, start) * 0.5 * g + band_noise(rng, n, 1000, 3000) * env(t, 0.0003, 0.004, start) * 0.6 * g
    return fades(x, tail=0.04)


def shotgun_insert(rng):
    t = axis(0.3)
    n = len(t)
    x = band_noise(rng, n, 1000, 3000) * gate(t, 0.0, 0.09, 0.015) * 0.3  # pushed up the tube
    x += ring(t, [2800, 4400, 6500], 0.025, 0.1) * 0.6 + band_noise(rng, n, 1500, 5000) * env(t, 0.0002, 0.003, 0.1) * 0.9  # the latch
    x += glide(t, 900, 600, 0.02, 0.11) * env(t, 0.002, 0.03, 0.11) * 0.2  # the follower spring
    return fades(x, tail=0.04)


def shotgun_dryclick(rng):
    t = axis(0.2)
    n = len(t)
    x = band_noise(rng, n, 1500, 6000) * env(t, 0.0002, 0.004) * 1.0 + ring(t, [1900, 3100, 4800], 0.04) * 0.6
    x += np.sin(2.0 * np.pi * 160.0 * t) * env(t, 0.001, 0.03) * 0.5 + band_noise(rng, n, 100, 400) * env(t, 0.001, 0.02) * 0.6  # the stock
    return fades(x, tail=0.04)


# ---------------------------------------------------------------- long guns

def musket_halfcock(rng):
    t = axis(0.15)
    n = len(t)
    x = band_noise(rng, n, 2000, 8000) * env(t, 0.0002, 0.003) * 0.9 + ring(t, [2900, 4500, 6700], 0.03) * 0.45
    x += glide(t, 2400, 2000, 0.01) * env(t, 0.001, 0.015) * 0.12  # the sear spring
    return fades(x, tail=0.03)


def musket_stock_lowered(rng):
    t = axis(0.3)
    n = len(t)
    x = band_noise(rng, n, 60, 300) * env(t, 0.002, 0.03) * 1.6 + glide(t, 90, 60, 0.03) * env(t, 0.001, 0.04) * 1.0
    x += np.sin(2.0 * np.pi * 220.0 * t) * env(t, 0.001, 0.025) * 0.4  # the stock itself
    x += crackle(rng, t, 1000, 6000, 900.0, 0.0, 0.07, 0.25, shape=env(t, 0.002, 0.03))  # the sand under the butt
    return fades(x, tail=0.05)


def musket_ramrod_turn(rng):
    t = axis(0.2)
    n = len(t)
    x = band_noise(rng, n, 600, 3000) * bell(t, 0.07, 0.035) * 0.4  # flipped in the air
    x += ring(t, [2600, 4300], 0.01, 0.13) * 0.5 + band_noise(rng, n, 2000, 7000) * env(t, 0.0002, 0.002, 0.13) * 0.5  # caught
    return fades(x, tail=0.03)


def musket_ramrod_return(rng):
    t = axis(0.36)
    n = len(t)
    back = gate(t, 0.0, 0.25, 0.02)
    x = band_noise(rng, n, 2000, 8000) * back * (1.0 - 0.5 * np.clip(t / 0.25, 0.0, 1.0)) * 0.5 + sweep(t, 3000, 2200, 0.0, 0.25) * 0.07
    x += band_noise(rng, n, 1000, 4000) * env(t, 0.0002, 0.003, 0.26) * 0.8 + ring(t, [1700, 2950, 4600], 0.04, 0.26) * 0.5  # home in its pipes
    return fades(x, tail=0.04)


def blunderbuss_shot_pour(rng):
    t = axis(0.42)
    n = len(t)
    shape = gate(t, 0.0, 0.34, 0.03) * (0.6 + 0.4 * bell(t, 0.12, 0.08))
    x = crackle(rng, t, 2500, 9000, 900.0, 0.0, 0.34, 1.0, shape=shape)  # the lead balls rattling down
    x += band_noise(rng, n, 400, 700) * shape * 0.15 * (1.0 + 0.6 * np.sin(2.0 * np.pi * 33.0 * t))  # the wide tube hums
    for start, g in ((0.36, 0.5), (0.39, 0.3)):  # the last ones settle
        x += ring(t, [5200, 7900], 0.004, start) * g
    return fades(x, tail=0.04)


def rifle_patchbox(rng):
    t = axis(0.32)
    n = len(t)
    x = ring(t, [3300, 5100, 7600], 0.025) * 0.5 + band_noise(rng, n, 1500, 5000) * env(t, 0.0002, 0.003) * 0.8  # the brass lid opened
    x += band_noise(rng, n, 300, 2500) * gate(t, 0.06, 0.19, 0.02) * 0.2 + crackle(rng, t, 2000, 8000, 200.0, 0.07, 0.19, 0.25)  # a patch taken
    x += ring(t, [3300, 5100, 7600], 0.02, 0.22) * 0.45 + band_noise(rng, n, 150, 600) * env(t, 0.0005, 0.015, 0.22) * 0.9  # shut
    return fades(x, tail=0.05)


# ---------------------------------------------------------------- grenade

def fuse_crackle(rng, t, start, end, gain=1.0, shape=None):
    """The fuse burning between start and end (s): a fizz that breathes, sparse sputters"""
    n = len(t)
    breath = band_filter(rng.standard_normal(n), 8.0, 60.0, 2)
    breath = 0.55 + 0.45 * breath / (np.max(np.abs(breath)) + 1e-12)
    x = band_noise(rng, n, 3000, 10000) * np.clip(breath, 0.0, None) * 0.35 + crackle(rng, t, 1500, 8000, 70.0, start, end, 1.0)
    x = x * gate(t, start, end, 0.01)
    if shape is not None:
        x = x * shape
    return x * gain


def grenade_light(rng):
    t = axis(3.3)
    n = len(t)
    x = band_noise(rng, n, 2000, 9000) * gate(t, 0.0, 0.09, 0.03) * np.clip(t / 0.08, 0.0, 1.0) * 0.35  # the match touches the fuse
    x += crackle(rng, t, 1000, 6000, 1500.0, 0.08, 0.2, 1.0, shape=env(t, 0.005, 0.05, 0.08))  # it sputters and catches
    x += band_noise(rng, n, 1500, 7000) * env(t, 0.005, 0.06, 0.08) * 0.5
    burn = 1.0 - 0.25 * np.clip((t - 0.3) / 2.7, 0.0, 1.0)
    x += fuse_crackle(rng, t, 0.18, 3.3, 0.9, shape=burn)
    return fades(x, tail=0.25)


def grenade_fuse_loop(rng):
    """1.5 s of fuse that loops without a seam: circular noise, whole cycles of breathing, clicks wrapped around"""
    t = axis(1.5)
    n = len(t)
    breath = 0.6 + 0.2 * np.sin(2.0 * np.pi * 4.0 * t / 1.5 + 1.0) + 0.2 * np.sin(2.0 * np.pi * 7.0 * t / 1.5)
    x = band_noise(rng, n, 3000, 10000) * breath * 0.35
    clicks = np.zeros(n)
    mask = rng.random(n) < 70.0 / RATE
    clicks[mask] = rng.standard_normal(int(mask.sum()))
    c = band_filter(clicks, 1500, 8000)  # circular: a click at the end wraps to the start
    return x + c / (np.max(np.abs(c)) + 1e-12)


def grenade_throw(rng):
    t = axis(0.45)
    n = len(t)
    speed = bell(t, 0.14, 0.07)
    x = band_noise(rng, n, 300, 2500) * speed ** 1.5 * 0.6 + band_noise(rng, n, 150, 600) * speed ** 2 * 0.4  # the arm
    x += band_noise(rng, n, 800, 4000) * env(t, 0.0005, 0.012, 0.17) * 0.5  # the sleeve snaps at the release
    x += band_noise(rng, n, 3000, 9000) * gate(t, 0.0, 0.3, 0.05) * 0.08  # the slow match in the other hand
    return fades(x, tail=0.08)


def grenade_bounce(rng):
    t = axis(0.32)
    n = len(t)
    x = band_noise(rng, n, 200, 900) * env(t, 0.0005, 0.008) * 1.2
    x += ring(t, [620, 1100, 1750, 2600], 0.08, weights=[1.0, 0.6, 0.35, 0.2]) * 0.8  # the iron, damped
    x += glide(t, 130, 95, 0.02) * env(t, 0.001, 0.03) * 0.8  # the ground
    return fades(x, tail=0.06)


def grenade_bag_reach(rng):
    t = axis(0.42)
    n = len(t)
    x = band_noise(rng, n, 200, 2000) * gate(t, 0.0, 0.3, 0.03) * (0.6 + 0.4 * np.sin(2.0 * np.pi * 17.0 * t)) * 0.4  # the canvas
    for start, g in ((0.1, 0.8), (0.17, 1.0), (0.26, 0.6)):  # iron knocking iron
        x += ring(t, [900, 1500, 2300], 0.035, start) * 0.6 * g + band_noise(rng, n, 200, 1200) * env(t, 0.0005, 0.006, start) * 0.8 * g
    return fades(x, tail=0.05)


def grenade_take(rng):
    t = axis(0.26)
    n = len(t)
    x = ring(t, [900, 1500, 2300], 0.035) * 0.6 + band_noise(rng, n, 200, 1200) * env(t, 0.0005, 0.006) * 0.9
    x += band_noise(rng, n, 300, 2500) * gate(t, 0.03, 0.2, 0.03) * 0.3
    return fades(x, tail=0.04)


def grenade_dryclick(rng):
    t = axis(0.26)
    n = len(t)
    x = band_noise(rng, n, 100, 800) * env(t, 0.002, 0.025) * 1.0 + band_noise(rng, n, 300, 2500) * gate(t, 0.0, 0.18, 0.03) * 0.35
    return fades(x, tail=0.05)


def grenade_release(rng):
    t = axis(0.72)
    n = len(t)
    x = band_noise(rng, n, 300, 2000) * bell(t, 0.08, 0.05) ** 1.5 * 0.5  # tossed
    x += band_noise(rng, n, 80, 600) * env(t, 0.002, 0.04, 0.4) * 1.2 + band_noise(rng, n, 300, 2500) * env(t, 0.01, 0.08, 0.4) * 0.35  # lands
    return fades(x, tail=0.08)


# ---------------------------------------------------------------- shared by the firearms

def firearm_draw(rng):
    t = axis(0.36)
    n = len(t)
    x = band_noise(rng, n, 400, 3000) * bell(t, 0.1, 0.06) * 0.45 + ring(t, [4500, 7000], 0.01, 0.06) * 0.15  # cloth, a buckle
    x += band_noise(rng, n, 150, 800) * env(t, 0.0005, 0.015, 0.21) * 0.7 + ring(t, [1200, 2000], 0.03, 0.21) * 0.25  # settles in the hands
    return fades(x, tail=0.06)


def firearm_stow(rng):
    t = axis(0.36)
    n = len(t)
    x = band_noise(rng, n, 150, 800) * env(t, 0.0005, 0.015) * 0.7 + ring(t, [1200, 2000], 0.03) * 0.25
    x += band_noise(rng, n, 400, 3000) * bell(t, 0.2, 0.07) * 0.45 + ring(t, [4500, 7000], 0.01, 0.27) * 0.15
    return fades(x, tail=0.06)


def firearm_discard_empty(rng):
    t = axis(0.32)
    n = len(t)
    x = band_noise(rng, n, 2000, 8000) * env(t, 0.0002, 0.003) * 1.0 + ring(t, [2500, 4000], 0.03) * 0.5  # the lock snaps on nothing
    for start, g in ((0.12, 0.7), (0.18, 0.5)):  # the empty pouch shaken: hollow
        x += ring(t, [1800, 2600], 0.02, start) * 0.4 * g + band_noise(rng, n, 400, 2000) * env(t, 0.0005, 0.008, start) * 0.5 * g
    return fades(x, tail=0.05)


def firearm_discard_release(rng):
    t = axis(0.95)
    n = len(t)
    x = band_noise(rng, n, 300, 2500) * bell(t, 0.1, 0.06) ** 1.5 * 0.45  # the swish of the toss
    wood = band_noise(rng, n, 100, 700)
    for start, g in ((0.42, 1.0), (0.58, 0.55), (0.66, 0.3)):  # it hits the ground, bounces, settles
        x += wood * env(t, 0.0005, 0.02, start) * 1.4 * g + glide(t, 200, 150, 0.02, start) * env(t, 0.001, 0.04, start) * 0.7 * g
        x += ring(t, [1100, 1900, 3200], 0.06, start) * 0.4 * g + band_noise(rng, n, 1500, 6000) * env(t, 0.0002, 0.003, start) * 0.5 * g
    x += band_noise(rng, n, 300, 2000) * gate(t, 0.68, 0.8, 0.03) * 0.15
    return fades(x, tail=0.08)


# ---------------------------------------------------------------- ammunition

def ammo_pickup(rng):
    t = axis(0.5)
    n = len(t)
    x = band_noise(rng, n, 300, 2000) * gate(t, 0.0, 0.35, 0.03) * (0.6 + 0.4 * np.sin(2.0 * np.pi * 23.0 * t)) * 0.3  # the leather
    for k in range(7):  # lead balls and cartridges knocking inside
        start = 0.03 + k * 0.045 + rng.uniform(-0.01, 0.01)
        g = rng.uniform(0.5, 1.0)
        if k % 3 == 2:
            x += ring(t, [4200, 6400], 0.01, start) * 0.4 * g  # a brass cap
        else:
            x += ring(t, [1800, 2900, 4300], 0.025, start) * 0.5 * g + band_noise(rng, n, 500, 2500) * env(t, 0.0003, 0.005, start) * 0.6 * g
    x += band_noise(rng, n, 800, 4000) * env(t, 0.0005, 0.012, 0.4) * 0.5  # the strap snaps shut
    return fades(x, tail=0.06)


# ---------------------------------------------------------------- the set

# name -> (generator, peak dBFS, loops)
SOUNDS = [
    ("SW_Pistol_Fire", pistol_fire, -1.0, False),
    ("SW_DoublePistol_Fire", lambda rng: pistol_fire(rng, heavy=True), -1.0, False),
    ("SW_Rifle_Shot", rifle_shot, -1.0, False),
    ("SW_Blunderbuss_Shot", blunderbuss_shot, -1.0, False),
    ("SW_Pistol_CockHammer", pistol_cock, -6.0, False),
    ("SW_Pistol_DryClick", pistol_dryclick, -6.0, False),
    ("SW_Pistol_PaperPickup", pistol_paper_pickup, -12.0, False),
    ("SW_Pistol_PaperTransfer", pistol_paper_transfer, -9.0, False),
    ("SW_Pistol_RamrodGesture", pistol_ramrod, -6.0, False),
    ("SW_Pistol_RamrodReturned", pistol_ramrod_return, -9.0, False),
    ("SW_Shotgun_PumpBack", shotgun_pump_back, -5.0, False),
    ("SW_Shotgun_EjectSpentShell", shotgun_eject, -9.0, False),
    ("SW_Shotgun_PumpForward", shotgun_pump_forward, -10.0, False),
    ("SW_Shotgun_ChamberSlug", shotgun_chamber, -5.0, False),
    ("SW_Shotgun_ShellPickup", shotgun_shell_pickup, -10.0, False),
    ("SW_Shotgun_InsertSlug", shotgun_insert, -7.0, False),
    ("SW_Shotgun_DryClick", shotgun_dryclick, -6.0, False),
    ("SW_Musket_HalfCock", musket_halfcock, -8.0, False),
    ("SW_Musket_StockLowered", musket_stock_lowered, -6.0, False),
    ("SW_Musket_RamrodTurn", musket_ramrod_turn, -12.0, False),
    ("SW_Musket_RamrodReturn", musket_ramrod_return, -8.0, False),
    ("SW_Blunderbuss_ShotPour", blunderbuss_shot_pour, -9.0, False),
    ("SW_Rifle_PatchBox", rifle_patchbox, -9.0, False),
    ("SW_Grenade_Light", grenade_light, -8.0, False),
    ("SW_Grenade_Fuse", grenade_fuse_loop, -10.0, True),
    ("SW_Grenade_Throw", grenade_throw, -8.0, False),
    ("SW_Grenade_Bounce", grenade_bounce, -6.0, False),
    ("SW_Grenade_Explosion", grenade_explosion, -1.0, False),
    ("SW_Grenade_BagReach", grenade_bag_reach, -10.0, False),
    ("SW_Grenade_Take", grenade_take, -9.0, False),
    ("SW_Grenade_DryClick", grenade_dryclick, -10.0, False),
    ("SW_Grenade_Release", grenade_release, -8.0, False),
    ("SW_Firearm_DrawWeapon", firearm_draw, -12.0, False),
    ("SW_Firearm_StowWeapon", firearm_stow, -12.0, False),
    ("SW_Firearm_Discard_Empty", firearm_discard_empty, -8.0, False),
    ("SW_Firearm_Discard_Release", firearm_discard_release, -4.0, False),
    ("SW_Ammo_Pickup", ammo_pickup, -8.0, False),
]


def measure(x):
    """Peak (dBFS), loudest 100 ms (dB RMS) and spectral centroid (Hz)"""
    peak = np.max(np.abs(x)) + 1e-12
    win = max(min(int(0.1 * RATE), len(x) - 1), 1)
    c = np.concatenate([[0.0], np.cumsum(x ** 2)])
    loud = np.sqrt(np.max(c[win:] - c[:-win]) / win) + 1e-12
    spec = np.abs(np.fft.rfft(x))
    f = np.fft.rfftfreq(len(x), 1.0 / RATE)
    return 20.0 * np.log10(peak), 20.0 * np.log10(loud), float((spec * f).sum() / (spec.sum() + 1e-12))


def write(folder, name, x):
    os.makedirs(folder, exist_ok=True)
    path = os.path.join(folder, name + ".wav")
    pcm = np.clip(x * 32767.0, -32768, 32767).astype("<i2")
    with wave.open(path, "wb") as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes(pcm.tobytes())
    return path


def main(argv=None):
    parser = argparse.ArgumentParser(description="Synthesises the sounds of the weapons v2 (see the module docstring)")
    parser.add_argument("--only", help="fnmatch pattern of the sounds to write, e.g. SW_Grenade_*")
    parser.add_argument("--out", default=OUT, help="output folder (default art/audio/weapons)")
    parser.add_argument("--json", help="also write the table of the files (name, seconds, peak, loudest 100 ms, centroid)")
    args = parser.parse_args(argv)
    rows = []
    for name, make, peak_db, loops in SOUNDS:
        if args.only and not fnmatch.fnmatch(name, args.only):
            continue
        x = make(seeded(name))
        x = x / (np.max(np.abs(x)) + 1e-12) * 10 ** (peak_db / 20.0)
        try:
            path = write(args.out, name, x)
        except PermissionError:
            print(f"{name}: cannot write (file read only?), skipped")
            continue
        peak, loud, centroid = measure(x)
        rows.append({"name": name, "seconds": round(len(x) / RATE, 3), "peak_dbfs": round(peak, 1), "loudest_100ms_db": round(loud, 1),
                     "centroid_hz": round(centroid), "loop": loops, "path": path})
        print(f"{name:28s} {len(x) / RATE:5.2f} s  peak {peak:6.1f} dBFS  loudest 100 ms {loud:6.1f} dB  centroid {centroid:6.0f} Hz{'  loop' if loops else ''}")
    print(f"{len(rows)} sounds in {os.path.normpath(args.out)}")
    if args.json:
        with open(args.json, "w", encoding="utf-8") as handle:
            json.dump(rows, handle, indent=1)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
