"""T-259: designs the sounds of the weapons v2 from CC0 recordings (weapons_v2_fetch.py, plus the expedition banks of
expedition_fetch.py) and a few synthetic layers, as 48 kHz PCM16 mono Unreal sources.

  python -I scripts/audio/weapons_v2_prepare.py [--only NAME_PREFIX]   ->  art/audio/weapons/v2/*.wav + manifest.json

Aim (owner 2026-10-09: "type Call of Duty", 18th century flintlocks): every shot is layered like a modern game shot, but
on black powder recordings. The flint strikes the frizzen, the pan flashes ("pfff"), then the charge goes off: a short
crack, a heavy body, a sub punch (synthetic sweep, the recordings are previews with little sub), and the echo of the
place (the recordings' own outdoor tails and a synthetic outdoor reverb with late slap echoes). Three variants of every
shot and of the repeated mechanics (SoundCue Random in Unreal: no shot sounds twice the same in a row). The lock work
and the reload are built from real clicks (an old flintlock-style lock, bolt and stapler clicks, shotgun pump and
shells), leather, cloth, paper, wood and iron impacts; only the powder and lead-shot pours and the ramrod friction are
synthesised (no CC0 recording found). Timings follow the events of the packs (Weapons/PirateWeaponClips.inl): the long
guns' Trigger plays 125 ms before their Shot, so the flint and the pan flash fill the lock time; the pistols' shot
starts with their Fire event, so the flash leads the boom by 30 ms inside the shot itself.

Every source file is checked against its inventory SHA-256; the manifest records, for every WAV, the source files and
the times used, the processing and the measures. Deterministic (seeded by name). Levels: shots and blast peak at -1 dBFS
(peak limited), the locks around -6, the reload foley -10 to -14, the handling -14 to -18; the event volumes and the
sound class do the rest in game.
"""
from __future__ import annotations

import argparse
import json
import sys
import zlib
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_dsp as dsp  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/audio/weapons/v2"
INVENTORIES = (ROOT / "art/audio_src/weapons_v2/inventory.json", ROOT / "art/audio_src/expedition/inventory.json")
RATE = dsp.RATE


# ---------------------------------------------------------------------------------------------------------- sources

class Bank:
    """The CC0 source files, checked against their inventory hash once, cached at 48 kHz mono"""

    def __init__(self):
        self.files, self.cache, self.used = {}, {}, []
        for path in INVENTORIES:
            for key, source in json.loads(path.read_text("utf-8")).items():
                for entry in source["files"]:
                    self.files[(key, Path(entry["file"]).name)] = (entry, source)

    def load(self, key, name):
        if (key, name) not in self.cache:
            entry, source = self.files[(key, name)]
            path = ROOT / entry["file"]
            if dsp.sha256(path) != entry["sha256"]:
                raise ValueError(f"Source changed since the inventory: {path}")
            self.cache[(key, name)] = dsp.mono(dsp.load(path))
        return self.cache[(key, name)]

    def credit(self, key, name, start, length):
        entry, source = self.files[(key, name)]
        return {"source": key, "file": name, "author": source["author"], "license": source["license"], "page": source["page"],
                "start_s": round(start, 3), "seconds": round(length, 3)}


BANK = Bank()


class Sound:
    """A mono buffer being built, with the credits of its layers"""

    def __init__(self, seconds_long):
        self.data = np.zeros(dsp.samples(seconds_long))
        self.credits, self.synth = [], []

    def add(self, layer, at=0.0, gain_db=0.0):
        signal, credits = layer
        if dsp.samples(at) + len(signal) > len(self.data):
            self.data = np.concatenate((self.data, np.zeros(dsp.samples(at) + len(signal) - len(self.data))))
        dsp.place(self.data, signal, at, dsp.gain_db(gain_db))
        for credit in credits:
            (self.credits if "source" in credit else self.synth).append({**credit, "at_s": round(at, 3), "gain_db": gain_db})
        return self


def cut(key, name, start, length, pitch=1.0, hp=None, lp=None, fade_in=0.002, fade_out=0.015, snap=None):
    """A piece of a source, peak normalised to 0 dBFS. snap: move start to the first 1 ms window above snap dB (re the
    piece's peak) within the piece, minus 2 ms (no dead air before a transient)"""
    source = BANK.load(key, name)
    begin = dsp.samples(start)
    piece = source[begin:begin + dsp.samples(length)].copy()
    if snap is not None:
        env, window = dsp.envelope(piece, 0.001)
        first = int(np.argmax(dsp.db(env / max(env.max(), 1e-12)) > snap))
        offset = max(0, dsp.samples(first * window - 0.002))
        piece = source[begin + offset:begin + offset + dsp.samples(length)].copy()
        start += dsp.seconds(offset)
    if pitch != 1.0:
        piece = dsp.pitch_shift(piece, pitch)
    if hp:
        piece = dsp.butter_filter(piece, "highpass", hp, 2)
    if lp:
        piece = dsp.butter_filter(piece, "lowpass", lp, 2)
    piece = dsp.fade(piece, fade_in, fade_out)
    peak = np.max(np.abs(piece))
    if peak < 1e-7:
        raise ValueError(f"Silent piece {key}/{name} at {start}")
    return piece / peak, [BANK.credit(key, name, start, length)]


def synth(name, signal, **params):
    peak = np.max(np.abs(signal))
    return signal / max(peak, 1e-12), [{"synth": name, **params}]


def rng_for(name):
    return np.random.default_rng(zlib.crc32(name.encode("utf-8")))


# ------------------------------------------------------------------------------------------------- synthetic layers

def punch(f_start, f_end, seconds_long, decay_s, drive=2.0):
    """Sub punch: an exponential sine sweep with a 2 ms attack, slightly saturated so small speakers hear it"""
    t = np.arange(dsp.samples(seconds_long)) / RATE
    freq = f_end + (f_start - f_end) * np.exp(-t / (decay_s * 0.6))
    phase = 2 * np.pi * np.cumsum(freq) / RATE
    env = (1 - np.exp(-t / 0.002)) * np.exp(-t / decay_s)
    wave = np.tanh(drive * np.sin(phase) * env) / np.tanh(drive)
    return synth("punch", dsp.fade(wave, 0.0, 0.02), f_start=f_start, f_end=f_end, decay_s=decay_s)


def grains(seconds_long, rate_hz, seed, hp=1500.0, lp=9000.0, size_s=0.0025, swell=True):
    """Poured grains (powder, lead shot): short decaying ticks at Poisson times, louder in the middle of the pour"""
    rng = np.random.default_rng(seed)
    out = np.zeros(dsp.samples(seconds_long))
    count = rng.poisson(rate_hz * seconds_long)
    tick_n = dsp.samples(size_s)
    tick_env = np.exp(-np.arange(tick_n) / (tick_n / 4))
    for _ in range(count):
        at = rng.uniform(0, seconds_long - size_s)
        amp = rng.uniform(0.3, 1.0) * (np.sin(np.pi * at / seconds_long) ** 0.7 if swell else 1.0)
        dsp.place(out, rng.standard_normal(tick_n) * tick_env * amp, at)
    out += 0.08 * rng.standard_normal(len(out)) * np.sin(np.pi * np.arange(len(out)) / len(out))
    out = dsp.butter_filter(dsp.butter_filter(out, "highpass", hp, 2), "lowpass", lp, 2)
    return synth("grains", dsp.fade(out, 0.01, 0.04), rate_hz=rate_hz, seconds=seconds_long, seed=seed)


def friction(seconds_long, seed, center_hz=2400.0, width=0.9, judder_hz=38.0):
    """Metal sliding in wooden pipes: band-passed noise with a stick-slip judder and a slight rising pitch"""
    rng = np.random.default_rng(seed)
    n = dsp.samples(seconds_long)
    t = np.arange(n) / RATE
    noise = rng.standard_normal(n)
    band = dsp.butter_filter(noise, "bandpass", (center_hz * (1 - width / 2), center_hz * (1 + width / 2)), 2)
    judder = 0.6 + 0.4 * np.abs(np.sin(2 * np.pi * judder_hz * t + rng.uniform(0, 6.28)))
    swell = np.sin(np.pi * t / seconds_long) ** 0.5
    ring = 0.15 * np.sin(2 * np.pi * (center_hz * 1.7 + 300 * t / seconds_long) * t) * swell
    return synth("friction", dsp.fade(band * judder * swell + ring * np.abs(band).mean(), 0.01, 0.03),
                 seconds=seconds_long, center_hz=center_hz, seed=seed)


def hiss(seconds_long, seed, hp=2500.0, attack_s=0.004, decay_s=0.05):
    """Short bright fizz (spark shower, sputter)"""
    rng = np.random.default_rng(seed)
    t = np.arange(dsp.samples(seconds_long)) / RATE
    env = (1 - np.exp(-t / attack_s)) * np.exp(-t / decay_s)
    return synth("hiss", dsp.butter_filter(rng.standard_normal(len(t)), "highpass", hp, 2) * env,
                 seconds=seconds_long, decay_s=decay_s, seed=seed)


def pellets(seconds_long, count, seed, start=0.12, spread=0.5):
    """The hail of a scatter shot landing: small dull thuds and leaf ticks spread after the blast"""
    rng = np.random.default_rng(seed)
    out = np.zeros(dsp.samples(seconds_long))
    for _ in range(count):
        at = start + rng.exponential(spread / 2.5)
        if at >= seconds_long - 0.03:
            continue
        n = dsp.samples(rng.uniform(0.006, 0.02))
        tick = rng.standard_normal(n) * np.exp(-np.arange(n) / (n / 5))
        dsp.place(out, tick * rng.uniform(0.2, 1.0), at)
    out = dsp.butter_filter(dsp.butter_filter(out, "highpass", 700.0, 2), "lowpass", 6000.0, 2)
    return synth("pellets", out, count=count, seed=seed)


def outdoor_tail(signal, seed, decay_s, slaps=((0.21, 0.35), (0.47, 0.22)), lowpass_hz=4500.0):
    """The place's echo: a diffuse outdoor decay with late slap echoes (cliffs of coral, the tree line)"""
    impulse = dsp.room_impulse(seed, decay_s * 1.3, decay_s, lowpass_hz, 0.012, early=slaps)
    wet = dsp.convolve(signal, impulse)
    return synth("outdoor_tail", wet, decay_s=decay_s, slaps=[list(s) for s in slaps], seed=seed)


# -------------------------------------------------------------------------------------------------------- finishing

def limiter(x, ceiling_db=-1.0, release_s=0.08):
    """Peak limiter: instant attack (2 ms look-ahead), exponential release, then a soft clip safety"""
    ceiling = dsp.gain_db(ceiling_db)
    look = dsp.samples(0.002)
    from scipy.ndimage import maximum_filter1d
    peak = maximum_filter1d(np.abs(x), size=2 * look + 1)
    target = np.minimum(1.0, ceiling / np.maximum(peak, 1e-12))
    gain = np.empty_like(target)
    coeff = np.exp(-1.0 / (release_s * RATE))
    g = 1.0
    for i, want in enumerate(target):
        g = want if want < g else want + (g - want) * coeff
        gain[i] = g
    y = x * gain
    return np.tanh(y / ceiling * 0.98) * ceiling


def tilt(x, low_cut_hz=35.0, shelf_hz=2500.0, shelf_db=0.0):
    """Mix EQ: no infrasound under low_cut_hz, a high shelf (approximated by adding a high-passed copy) for presence"""
    y = dsp.butter_filter(x, "highpass", low_cut_hz, 2)
    if shelf_db:
        y = y + (dsp.gain_db(shelf_db) - 1.0) * dsp.butter_filter(y, "highpass", shelf_hz, 2)
    return y


def finish(sound, peak_db, end_db=-62.0, fade_out=0.06, limit=False, max_seconds=None, shelf_db=0.0, loud_db=None):
    """Trims the dead tail, fades, EQ (shots: presence shelf), limits (shots) and sets the peak. loud_db: the limiter is
    driven (0 to 16 dB) until the loudest 100 ms reach it, so the weapons keep their loudness order"""
    x = tilt(sound.data, shelf_db=shelf_db) if limit or shelf_db else sound.data
    if max_seconds:
        x = x[:dsp.samples(max_seconds)]
    env, window = dsp.envelope(x, 0.01)
    level = dsp.db(env / max(env.max(), 1e-12))
    loud = np.where(level > end_db)[0]
    x = x[:min(len(x), dsp.samples((loud[-1] + 1) * window + 0.02))]
    x = dsp.fade(x, 0.0005, fade_out)
    x = dsp.normalize_peak(x, 0.0)
    if limit:
        if loud_db is None:
            x = limiter(x * dsp.gain_db(4.0), -1.0)
        else:
            low, high = 0.0, 16.0
            for _ in range(7):
                drive = (low + high) / 2
                level = dsp.loudest_window_db(dsp.normalize_peak(limiter(x * dsp.gain_db(drive), -1.0), peak_db))
                low, high = (drive, high) if level < loud_db else (low, drive)
            x = limiter(x * dsp.gain_db((low + high) / 2), -1.0)
            sound.synth.append({"synth": "limiter", "drive_db": round((low + high) / 2, 2), "loud_target_db": loud_db})
    x = dsp.normalize_peak(x, peak_db)
    sound.data = dsp.fade(x, 0.0005, min(fade_out, 0.02))
    return sound


# ------------------------------------------------------------------------------------------------------------ shots

KURT_TAKES = (0.145, 0.840, 1.555, 2.260, 2.940, 3.505)
N3_FALLS = (8.500, 14.330, 19.645, 23.995)
N3_CLICKS = (4.955, 6.575, 17.520, 18.240, 21.795, 22.755)
KURT = ("kurt_gunshots", "Black Powder.wav")
CRAIG = ("fs_flintlock_rifle", "675618_2524442-hq.mp3")
BRUNO = ("fs_musket_bang", "538795_11519060-hq.mp3")
FENN = ("fs_musket_ignition", "347647_6207707-hq.mp3")
MLSULLI = ("fs_musket_shot", "234869_3883507-hq.mp3")
N3 = ("fs_flintlock_clicking", "656397_14213356-hq.mp3")
SPARK = ("fs_flint_lighter", "689655_3086325-hq.mp3")
CANNON = ("thimras_cannon", "cannon_fire_0.ogg")


def kurt(take, length=0.5, **kw):
    """One take of the black powder report (takes are 0.57-0.7 s apart: at most 0.5 s, faded, never the next one)"""
    return cut(*KURT, KURT_TAKES[take] - 0.01, min(length, 0.5), snap=-30, fade_out=kw.pop("fade_out", 0.12), **kw)


def pistol_shot(name, variant, heavy=False):
    rng = rng_for(name)
    pitch = (0.94 if heavy else 1.0) * (1.0 + 0.025 * (variant - 1))
    s = Sound(2.2)
    s.add(cut(*FENN, 0.015, 0.05, hp=2500, fade_out=0.01), 0.0, -14)                    # pan flash: a short "pff"
    s.add(kurt([0, 2, 4][variant] + (1 if heavy else 0), 0.7, pitch=pitch, hp=90), 0.03, 0)  # the report
    body = cut(*(MLSULLI if heavy else BRUNO), 0.712 if heavy else 0.285, 0.9, pitch=pitch * 1.12, lp=5000, snap=-25)
    s.add(body, 0.03, -5 if heavy else -8)
    s.add(punch(105 if heavy else 120, 52 if heavy else 60, 0.25, 0.05 if heavy else 0.04), 0.03, -11 if heavy else -13)
    s.add(outdoor_tail(kurt([0, 2, 4][variant], 0.4, hp=150)[0], int(rng.integers(1 << 30)), 1.0 if heavy else 0.8,
                       ((0.18, 0.3), (0.39, 0.16))), 0.03, -16 if heavy else -18)
    return finish(s, -1.0, limit=True, max_seconds=1.9 if heavy else 1.6, loud_db=-10.0 if heavy else -11.0)


def musket_shot(name, variant):
    rng = rng_for(name)
    s = Sound(3.4)
    body = [cut(*BRUNO, 0.285, 2.9, snap=-25), cut(*MLSULLI, 0.712, 1.5, snap=-25, lp=7000),
            cut(*FENN, 0.065, 1.9, snap=-20, lp=8000)][variant]
    s.add(body, 0.0, 0)
    s.add(kurt([1, 3, 5][variant], 0.5, hp=900), 0.0, [-7, -7, -4][variant])                # close crack
    s.add(cut(*BRUNO, 0.285, 0.12, hp=1500, snap=-25), 0.0, [-8, -8, -5][variant])          # the flash of the bang
    s.add(punch(90, 42, 0.4, 0.08), 0.0, -10)
    s.add(cut(*CANNON, 0.0, 0.8, lp=220, snap=-30), 0.0, -19)                                # chest
    if variant:
        s.add(cut(*BRUNO, 0.40, 2.6, hp=120), 0.08, -12)                                     # the valley answers
    s.add(outdoor_tail(body[0][:dsp.samples(0.5)], int(rng.integers(1 << 30)), 2.0, ((0.24, 0.3), (0.62, 0.2))), 0.0,
          [-12, -15, -17][variant])
    return finish(s, -1.0, limit=True, max_seconds=3.2, shelf_db=[4.0, 3.0, 7.0][variant], loud_db=-8.5)


def rifle_shot(name, variant):
    rng = rng_for(name)
    s = Sound(3.4)
    pitch = 1.0 + 0.02 * (variant - 1)
    s.add(cut(*CRAIG, 0.172, 2.55, pitch=pitch, snap=-20), 0.0, 0)                          # the flintlock rifle
    s.add(kurt([2, 4, 0][variant], 0.4, hp=600, pitch=1.06), 0.0, -3)                       # a sharper snap
    s.add(cut(*BRUNO, 0.285, 0.12, hp=1500, snap=-25), 0.0, -7)                              # bright crack
    s.add(punch(80, 40, 0.35, 0.07), 0.0, -7)
    s.add(outdoor_tail(cut(*KURT, KURT_TAKES[variant] - 0.01, 0.3, hp=300, snap=-30)[0], int(rng.integers(1 << 30)),
                       2.4, ((0.33, 0.45), (0.71, 0.3), (1.1, 0.15))), 0.0, -13)              # a long valley echo
    return finish(s, -1.0, limit=True, max_seconds=3.3, shelf_db=5.0, loud_db=-8.5)


def blunderbuss_shot(name, variant):
    rng = rng_for(name)
    s = Sound(3.6)
    pitch = 0.84 + 0.03 * variant
    s.add(cut(*FENN, 0.065, 2.0, pitch=pitch, snap=-20), 0.0, 0)
    s.add(cut(*BRUNO, 0.285, 2.6, pitch=pitch + 0.05, snap=-25), 0.0, -4)
    s.add(cut(*CANNON, 0.0, 2.6, lp=600, snap=-30), 0.0, -6)
    s.add(cut("rubberduck_bangs", f"cannon_0{[1, 3, 5][variant]}.ogg", 0.0, 1.0, snap=-30, lp=3000), 0.0, -9)
    s.add(kurt([0, 1, 2][variant], 0.5, hp=600, pitch=0.9), 0.0, -6)
    s.add(cut(*BRUNO, 0.285, 0.12, hp=1500, pitch=0.92, snap=-25), 0.0, -8)
    s.add(punch(70, 30, 0.5, 0.12, drive=2.5), 0.0, -7)
    s.add(pellets(1.2, 26, int(rng.integers(1 << 30))), 0.0, -22)
    s.add(outdoor_tail(cut(*BRUNO, 0.285, 0.4, snap=-25)[0], int(rng.integers(1 << 30)), 2.6, ((0.26, 0.4), (0.68, 0.25))),
          0.0, -14)
    return finish(s, -1.0, limit=True, max_seconds=3.5, shelf_db=5.0, loud_db=-7.0)


def shotgun_shot(name, variant):
    rng = rng_for(name)
    s = Sound(2.4)
    s.add(kurt([5, 3, 1][variant], 0.6, pitch=0.92, hp=80), 0.0, 0)
    s.add(cut(*BRUNO, 0.285, 1.6, pitch=0.9 + 0.02 * variant, snap=-25), 0.0, -3)
    s.add(cut("rubberduck_bangs", f"shot_0{variant + 1}.ogg", 0.0, 0.45, snap=-30), 0.0, -7)
    s.add(punch(85, 38, 0.4, 0.07, drive=2.2), 0.0, -8)
    s.add(pellets(0.8, 14, int(rng.integers(1 << 30)), start=0.08, spread=0.3), 0.0, -26)
    s.add(outdoor_tail(kurt(variant, 0.4, hp=120)[0], int(rng.integers(1 << 30)), 1.5, ((0.2, 0.35), (0.5, 0.2))), 0.0, -15)
    return finish(s, -1.0, limit=True, max_seconds=2.2, shelf_db=3.0, loud_db=-9.0)


# --------------------------------------------------------------------------------------------------- locks, clicks

def n3_fall(index, pitch=1.0, length=0.2):
    return cut(*N3, N3_FALLS[index] - 0.01, length, pitch=pitch, snap=-30, fade_out=0.03)


def n3_click(index, pitch=1.0, length=0.09):
    return cut(*N3, N3_CLICKS[index] - 0.01, length, pitch=pitch, snap=-30, fade_out=0.02)


def longgun_trigger(name, variant):
    """Trigger, 125 ms before the Shot: the cock falls, the flint strikes the frizzen (sparks), the pan flashes"""
    s = Sound(0.5)
    s.add(n3_fall(variant, 0.9 - 0.02 * variant), 0.0, 0)
    s.add(cut(*SPARK, 0.13, 0.08, hp=3000, snap=-25), 0.004, -9)
    s.add(cut(*CRAIG, 0.02, 0.15, hp=400, fade_in=0.004, fade_out=0.03), 0.035 + 0.005 * variant, -3)
    s.add(cut(*FENN, 0.015, 0.06, hp=2500), 0.03, -10)
    return finish(s, -5.0, end_db=-55, fade_out=0.03, max_seconds=0.4)


def pistol_hammer(name, variant):
    """HammerDrop (shot and dry fire): a smaller lock, the flint on the steel, a few sparks"""
    s = Sound(0.3)
    s.add(n3_fall(variant + 1 if variant < 3 else 0, 1.08), 0.0, 0)
    s.add(cut(*SPARK, 0.13, 0.06, hp=3500, snap=-25), 0.003, -12)
    return finish(s, -6.0, end_db=-55, fade_out=0.03, max_seconds=0.3)


def pistol_cock(name, variant):
    s = Sound(0.35)
    a, b = ((2, 3), (4, 5))[variant]
    s.add(n3_click(a, 1.1), 0.0, -2)
    s.add(n3_click(b, 1.1), 0.11, 0)
    return finish(s, -8.0, end_db=-55, fade_out=0.02)


def single_click(index, pitch, peak, extra=None):
    def build(name, variant):
        s = Sound(0.25)
        s.add(n3_click(index, pitch), 0.0, 0)
        if extra:
            s.add(extra(), 0.035, -8)
        return finish(s, peak, end_db=-55, fade_out=0.02)
    return build


LFA3 = ("lfa_clicks3", "equipment_clicks3.wav")


def frizzen_close(name, variant):
    s = Sound(0.3)
    s.add(cut(*LFA3, 2.785, 0.12, snap=-8, pitch=1.05), 0.0, 0)                            # aligned on its snap
    s.add(n3_click(2, 0.95), 0.004, -6)
    return finish(s, -9.0, end_db=-55, fade_out=0.02)


def pan_prime(name, variant):
    s = Sound(0.5)
    s.add(cut(*LFA3, 5.085, 0.05, snap=-25, pitch=0.8, lp=6000), 0.0, -6)                  # the horn's stopper
    s.add(grains(0.3, 260, 11, hp=2500, lp=11000), 0.06, 0)                               # fine priming powder
    return finish(s, -15.0, end_db=-50, fade_out=0.04)


KIMP = "kenney_impact"
KRPG = "kenney_rpg"


def stock_ground(name, variant):
    s = Sound(0.5)
    s.add(cut(KIMP, "impactWood_heavy_001.ogg", 0.0, 0.31, lp=2500, snap=-30), 0.0, 0)
    s.add(cut(KIMP, "impactSoft_heavy_002.ogg", 0.0, 0.5, snap=-30), 0.005, -3)
    return finish(s, -10.0, end_db=-50)


def powder_charge(name, variant):
    s = Sound(0.9)
    s.add(cut("foley", "paper_02.ogg", 0.0, 0.25, snap=-30, hp=300), 0.0, -2)             # the cartridge torn
    pour = grains(0.38, 900, 21, hp=900, lp=7000)
    tube = dsp.butter_filter(pour[0], "bandpass", (700, 1300), 2)
    s.add(pour, 0.18, 0)
    s.add(synth("tube", tube), 0.18, -11)
    return finish(s, -13.0, end_db=-50, fade_out=0.05)


def ball_patch(name, variant):
    s = Sound(0.5)
    s.add(cut(KRPG, "cloth4.ogg", 0.0, 0.3, snap=-30), 0.0, -4)
    s.add(cut(KIMP, "impactTin_medium_002.ogg", 0.0, 0.13, pitch=1.45, snap=-30), 0.12, 0)  # the ball in the muzzle
    return finish(s, -13.0, end_db=-50)


def ramrod_draw(name, variant):
    s = Sound(0.6)
    s.add(cut(*LFA3, 5.085, 0.05, snap=-25, pitch=1.2), 0.0, -8)
    s.add(friction(0.42, 31, 2600, 0.9, 41), 0.02, 0)
    s.add(cut(KIMP, "impactMetal_light_002.ogg", 0.0, 0.2, snap=-30, pitch=1.3), 0.43, -6)  # free of the pipes
    return finish(s, -13.0, end_db=-50, fade_out=0.04)


def ramrod_turn(name, variant):
    s = Sound(0.35)
    s.add(cut("swishes", "swish-12.wav", 0.0, 0.08, snap=-30), 0.0, 0)
    s.add(cut(KIMP, "impactMetal_light_000.ogg", 0.0, 0.2, snap=-30, pitch=1.5), 0.07, -9)
    return finish(s, -15.0, end_db=-50)


def ramrod_stroke(name, variant):
    """The rod rams the ball home: a dull iron thud in a long tube"""
    s = Sound(0.4)
    metal = ["impactMetal_medium_001.ogg", "impactMetal_medium_003.ogg", "impactMetal_medium_004.ogg"][variant]
    thud = cut(KIMP, metal, 0.0, 0.25, pitch=0.7 + 0.04 * variant, lp=3500, snap=-30)
    s.add(thud, 0.0, 0)
    s.add(synth("tube", dsp.butter_filter(thud[0], "bandpass", (500, 800), 2)), 0.0, -10)
    s.add(cut(KIMP, f"impactWood_medium_00{variant}.ogg", 0.0, 0.2, lp=1800, snap=-30), 0.0, -6)
    return finish(s, -10.0, end_db=-50)


def ramrod_return(name, variant):
    s = Sound(0.6)
    s.add(friction(0.3, 37, 2300, 0.8, 47), 0.0, 0)
    s.add(n3_click(4, 1.3), 0.3, -4)
    return finish(s, -14.0, end_db=-50, fade_out=0.04)


def shot_pour(name, variant):
    s = Sound(0.7)
    s.add(cut(KRPG, "handleCoins.ogg", 0.0, 0.6, pitch=1.35, hp=1200, snap=-30), 0.0, -6)
    s.add(grains(0.45, 120, 41, hp=1500, lp=9000, size_s=0.006), 0.05, 0)
    return finish(s, -12.0, end_db=-50, fade_out=0.05)


def patch_box(name, variant):
    s = Sound(0.8)
    s.add(cut(KRPG, "metalLatch.ogg", 0.0, 0.26, snap=-30, pitch=1.15), 0.0, 0)
    s.add(cut(KRPG, "cloth4.ogg", 0.0, 0.25, snap=-30), 0.18, -10)
    s.add(cut(KRPG, "metalClick.ogg", 0.0, 0.2, snap=-30, pitch=1.1), 0.5, -2)
    return finish(s, -12.0, end_db=-50)


# ------------------------------------------------------------------------------------------------- pistol reload

def paper_pickup(name, variant):
    s = Sound(0.6)
    s.add(cut(KRPG, "beltHandle2.ogg", 0.0, 0.45, snap=-30), 0.0, 0)
    s.add(cut("foley", "paper_01.ogg", 0.0, 0.2, snap=-30, hp=400), 0.18, -6)
    return finish(s, -14.0, end_db=-50)


def paper_transfer(name, variant):
    s = Sound(1.0)
    s.add(cut("foley", "paper_03.ogg", 0.0, 0.22, snap=-30, hp=300), 0.0, 0)
    s.add(grains(0.28, 700, 51, hp=900, lp=7000), 0.2, -2)
    s.add(cut(KIMP, "impactTin_medium_004.ogg", 0.0, 0.12, pitch=1.6, snap=-30), 0.55, -3)
    return finish(s, -13.0, end_db=-50)


def pistol_ramrod(name, variant):
    s = Sound(1.2)
    s.add(friction(0.24, 61, 2900, 0.9, 44), 0.0, -2)
    for i, at in enumerate((0.45, 0.76)):
        thud = cut(KIMP, f"impactMetal_medium_00{i + 1}.ogg", 0.0, 0.2, pitch=0.85, lp=4000, snap=-30)
        s.add(thud, at, -1)
        s.add(synth("tube", dsp.butter_filter(thud[0], "bandpass", (700, 1100), 2)), at, -6)
    return finish(s, -12.0, end_db=-50)


def pistol_ramrod_return(name, variant):
    s = Sound(0.5)
    s.add(friction(0.2, 67, 2700, 0.8, 50), 0.0, 0)
    s.add(n3_click(5, 1.4), 0.2, -5)
    return finish(s, -15.0, end_db=-50)


# ----------------------------------------------------------------------------------------------------- pump shotgun

PUMP = ("fs_pump", "769904_16350347-hq.mp3")
PUMP_PAIRS = ((3.965, 4.150), (6.180, 6.385), (8.535, 8.755))
ZER0 = "zer0_shotgun"


def pump_back(name, variant):
    s = Sound(0.3)
    s.add(cut(*PUMP, PUMP_PAIRS[variant][0] - 0.01, 0.15, snap=-30, fade_out=0.03), 0.0, 0)
    return finish(s, -7.0, end_db=-55)


def pump_forward(name, variant):
    s = Sound(0.3)
    s.add(cut(*PUMP, PUMP_PAIRS[variant][1] - 0.01, 0.16, snap=-30, fade_out=0.03), 0.0, 0)
    return finish(s, -6.0, end_db=-55)


def pump_slide(name, variant):
    s = Sound(0.3)
    s.add(friction(0.18, 71, 1600, 1.0, 30), 0.0, 0)
    s.add(cut(KRPG, "handleSmallLeather2.ogg", 0.0, 0.2, snap=-30), 0.0, -8)
    return finish(s, -18.0, end_db=-50, fade_out=0.03)


def shell_eject(name, variant):
    s = Sound(0.9)
    s.add(cut(*LFA3, 4.34, 0.03, snap=-25, pitch=1.3), 0.0, -2)
    s.add(cut(KIMP, "impactTin_medium_000.ogg", 0.0, 0.16, pitch=1.25, snap=-30), 0.48, -4)
    s.add(cut(KIMP, "impactTin_medium_003.ogg", 0.0, 0.2, pitch=1.35, snap=-30), 0.6, -10)
    return finish(s, -15.0, end_db=-50)


def shell_pickup(name, variant):
    s = Sound(0.5)
    s.add(cut(KRPG, "handleSmallLeather.ogg", 0.0, 0.34, snap=-30), 0.0, 0)
    s.add(cut(KRPG, "handleCoins2.ogg", 0.0, 0.3, snap=-30, pitch=0.85, lp=6000), 0.08, -6)
    return finish(s, -16.0, end_db=-50)


SHELL_INSERTS = (("Subsequent Shells.mp3", 0.735), ("Subsequent Shells.mp3", 0.865), ("First Shell.mp3", 0.495))


def shell_insert(name, variant):
    file, at = SHELL_INSERTS[variant]
    s = Sound(0.3)
    s.add(cut(ZER0, file, at - 0.01, 0.16, snap=-25, fade_out=0.03), 0.0, 0)
    return finish(s, -9.0, end_db=-55)


def shotgun_dry(name, variant):
    s = Sound(0.3)
    s.add(n3_fall(3, 0.8), 0.0, 0)
    s.add(cut(*LFA3, 1.465, 0.13, snap=-25, pitch=0.85, lp=7000), 0.0, -6)
    return finish(s, -8.0, end_db=-55)


# ---------------------------------------------------------------------------------------------------------- grenade

GRENADE = ("fs_grenade", "609587_1038806-hq.mp3")
KINOTON = ("fs_explosion_medium", "516914_2247456-hq.mp3")
FUSE = ("fs_fuse", "483320_2524442-hq.mp3")
MATCH = ("fs_match", "237402_1502374-hq.mp3")


def explosion(name, variant):
    rng = rng_for(name)
    s = Sound(5.5)
    main = [cut(*GRENADE, 0.0, 5.2, snap=-25), cut(*KINOTON, 0.0, 4.9, snap=-25),
            cut(*GRENADE, 0.0, 5.2, snap=-25, pitch=0.9)][variant]
    s.add(main, 0.0, 0)
    if variant != 1:
        s.add(cut(*KINOTON, 0.0, 1.5, snap=-25, hp=1500), 0.0, -10)                        # the crack of the casing
    s.add(cut("qubodup_explosion", "synthetic_explosion_1.flac", 0.0, 1.15, lp=1200, snap=-30), 0.0, -9)
    s.add(punch(65, 26, 0.8, 0.18, drive=2.6), 0.0, -6)
    s.add(cut(*CANNON, 0.0, 3.2, lp=400, snap=-30), 0.02, -9)
    for at, stones in ((0.35, "stones_02.ogg"), (0.7, "stones_04.ogg")):                    # coral rubble falling back
        s.add(cut("rpg", stones, 0.0, 0.8, snap=-30, lp=6000), at + 0.05 * variant, -16)
    s.add(cut("foley2", "sfx100v2_stones_0" + str(variant + 1) + ".ogg", 0.0, 1.0, snap=-30, lp=7000), 0.5, -17)
    s.add(grains(1.4, 70, int(rng.integers(1 << 30)), hp=800, lp=5000, size_s=0.008), 0.3, -20)  # sand shower
    s.add(outdoor_tail(main[0][:dsp.samples(0.6)], int(rng.integers(1 << 30)), 3.0, ((0.3, 0.4), (0.8, 0.25), (1.3, 0.12))),
          0.0, -12)
    return finish(s, -1.0, limit=True, max_seconds=5.4, shelf_db=3.0, loud_db=-6.0)


def bounce(name, variant):
    s = Sound(0.5)
    metal = ["impactMetal_heavy_000.ogg", "impactMetal_heavy_003.ogg", "impactMetal_heavy_001.ogg"][variant]
    s.add(cut(KIMP, metal, 0.0, 0.36, pitch=0.78, lp=5000, snap=-30), 0.0, 0)
    s.add(cut(KIMP, f"impactSoft_heavy_00{variant + 1}.ogg", 0.0, 0.5, snap=-30, hp=90), 0.0, -4)
    if variant == 2:
        s.add(cut(KIMP, "impactMining_002.ogg", 0.0, 0.4, snap=-30, lp=4000), 0.0, -8)
    return finish(s, -3.0, end_db=-50)


def fuse_light(name, variant):
    """Light: the slow match touches the fuse, it catches and sputters until the throw (0.5-0.7 s later)"""
    s = Sound(1.0)
    s.add(cut(*MATCH, 0.0, 0.5, snap=-30, hp=500), 0.0, -4)
    sizzle = cut(*FUSE, 6.0, 0.95, hp=700, fade_in=0.04, fade_out=0.25)
    s.add(sizzle, 0.06, 0)
    s.add(cut("fuse_short", "fuse_0.ogg", 0.0, 0.5, snap=-30, hp=600), 0.08, -6)
    return finish(s, -9.0, end_db=-50, fade_out=0.2, max_seconds=0.95)


def fuse_loop(name, variant):
    """A seamless 3 s loop of the burning fuse, for a fuse sound on the projectile (no field yet)"""
    s = Sound(3.3)
    s.add(cut(*FUSE, 9.0, 3.3, hp=600, fade_in=0.0, fade_out=0.0), 0.0, 0)
    s.data = dsp.seam_loop(dsp.normalize_peak(s.data, -10.0), 0.3)
    return s


def grenade_throw(name, variant):
    s = Sound(0.5)
    s.add(cut("swishes", "swish-9.wav", 0.0, 0.2, pitch=0.78, snap=-30), 0.0, 0)
    s.add(cut(KRPG, "cloth1.ogg", 0.0, 0.3, snap=-30), 0.0, -9)
    return finish(s, -8.0, end_db=-50)


def bag_reach(name, variant):
    s = Sound(0.7)
    s.add(cut(KRPG, "clothBelt.ogg", 0.0, 0.6, snap=-30), 0.0, 0)
    for at, metal in ((0.16, "impactMetal_heavy_002.ogg"), (0.31, "impactMetal_heavy_004.ogg")):
        s.add(cut(KIMP, metal, 0.0, 0.15, pitch=0.7, lp=2500, snap=-30), at, -7)
    return finish(s, -14.0, end_db=-50)


def grenade_take(name, variant):
    s = Sound(0.5)
    s.add(cut(KRPG, "handleSmallLeather.ogg", 0.0, 0.34, snap=-30), 0.0, 0)
    s.add(cut(KIMP, "impactMetal_medium_000.ogg", 0.0, 0.2, pitch=0.7, lp=3000, snap=-30), 0.05, -4)
    return finish(s, -14.0, end_db=-50)


def empty_bag(name, variant):
    s = Sound(0.5)
    s.add(cut(KRPG, "cloth3.ogg", 0.0, 0.3, snap=-30), 0.0, 0)
    s.add(cut(KRPG, "cloth2.ogg", 0.0, 0.3, snap=-30), 0.17, -3)
    return finish(s, -15.0, end_db=-50)


def bag_toss(name, variant):
    s = Sound(0.8)
    s.add(cut("swishes", "swish-3.wav", 0.0, 0.15, snap=-30), 0.0, -4)
    s.add(cut(KRPG, "dropLeather.ogg", 0.0, 0.42, snap=-30), 0.3, 0)
    return finish(s, -12.0, end_db=-50)


# ------------------------------------------------------------------------------------------------- shared handling

def draw(name, variant):
    s = Sound(0.8)
    s.add(cut(KRPG, "clothBelt.ogg", 0.0, 0.7, snap=-30), 0.0, 0)
    s.add(cut(KRPG, "metalLatch.ogg", 0.0, 0.2, snap=-30, pitch=0.9, lp=7000), 0.28, -10)
    return finish(s, -14.0, end_db=-50)


def stow(name, variant):
    s = Sound(0.8)
    s.add(cut(KRPG, "clothBelt2.ogg", 0.0, 0.7, snap=-30), 0.0, 0)
    s.add(cut(KRPG, "beltHandle1.ogg", 0.0, 0.28, snap=-30), 0.36, -6)
    return finish(s, -15.0, end_db=-50)


def ready(name, variant):
    s = Sound(0.4)
    s.add(cut(KRPG, "handleSmallLeather2.ogg", 0.0, 0.31, snap=-30), 0.0, 0)
    return finish(s, -18.0, end_db=-50)


def hand_grab(name, variant):
    s = Sound(0.4)
    s.add(cut(KRPG, "cloth4.ogg", 0.0, 0.3, snap=-30), 0.0, 0)
    s.add(cut(KRPG, "handleSmallLeather.ogg", 0.0, 0.25, snap=-30), 0.02, -6)
    return finish(s, -18.0, end_db=-50)


def discard_empty(name, variant):
    """Empty: the lock snaps on nothing, the empty pouch is patted"""
    s = Sound(0.6)
    s.add(n3_fall(2, 1.0), 0.0, 0)
    s.add(cut(KRPG, "handleSmallLeather2.ogg", 0.0, 0.3, snap=-30), 0.22, -6)
    return finish(s, -9.0, end_db=-50)


def discard_release(name, variant):
    """Release: tossed away, wood and steel landing on the ground 0.3 s later (the toss of the code takes about
    0.26 s from the hand to the sand)"""
    s = Sound(1.2)
    s.add(cut("swishes", ["swish-7.wav", "swish-8.wav"][variant], 0.0, 0.2, snap=-30, pitch=0.85), 0.0, -3)
    s.add(cut(KIMP, f"impactSoft_heavy_00{3 + variant}.ogg", 0.0, 0.5, snap=-30), 0.3, 0)
    s.add(cut(KIMP, f"impactWood_heavy_00{2 + variant}.ogg", 0.0, 0.31, snap=-30, lp=4000), 0.3, -4)
    s.add(cut(KIMP, f"impactMetal_light_00{3 + variant}.ogg", 0.0, 0.3, snap=-30, pitch=0.9), 0.31, -9)
    s.add(cut(KIMP, f"impactWood_light_00{variant}.ogg", 0.0, 0.27, snap=-30, lp=4000), 0.47, -12)
    return finish(s, -8.0, end_db=-50)


def ammo_pickup(name, variant):
    s = Sound(0.8)
    s.add(cut(KRPG, "handleSmallLeather.ogg", 0.0, 0.34, snap=-30), 0.0, -2)
    s.add(cut(KRPG, "handleCoins2.ogg", 0.0, 0.34, snap=-30, pitch=0.75, lp=5000), 0.06, 0)
    s.add(cut(KRPG, "beltHandle1.ogg", 0.0, 0.28, snap=-30), 0.36, -5)
    return finish(s, -12.0, end_db=-50)


# ----------------------------------------------------------------------------------------------------- the catalogue

# name -> (builder, variants, role) ; a name with variants gives NAME_01..NAME_0n (one SoundCue Random in Unreal)
CATALOGUE = {
    "SW_Pistol_Fire": (lambda n, v: pistol_shot(n, v), 3, "FireSound of DA_PistolRules (shot, pan flash 30 ms ahead)"),
    "SW_DoublePistol_Fire": (lambda n, v: pistol_shot(n, v, heavy=True), 3, "FireSound of DA_DoublePistolRules"),
    "SW_PirateMusket_Fire": (musket_shot, 3, "FireSound of DA_PirateMusketRules (event Shot)"),
    "SW_Rifle_Fire": (rifle_shot, 3, "FireSound of DA_RifleRules (event Shot)"),
    "SW_Blunderbuss_Fire": (blunderbuss_shot, 3, "FireSound of DA_BlunderbussRules (event Shot)"),
    "SW_Shotgun_Fire_V2": (shotgun_shot, 3, "FireSound of DA_ShotgunRules"),
    "SW_Grenade_Explosion": (explosion, 3, "ExplosionSound of DA_GrenadeRules"),
    "SW_Grenade_Bounce": (bounce, 3, "BounceSound of DA_GrenadeRules"),
    "SW_Longgun_Trigger": (longgun_trigger, 3, "event Trigger (musket, blunderbuss, rifle)"),
    "SW_Pistol_HammerDrop": (pistol_hammer, 3, "event HammerDrop (pistols: shot and dry fire)"),
    "SW_Pistol_Cock": (pistol_cock, 2, "events CockHammer, HammerCocked (pistols)"),
    "SW_Longgun_HalfCock": (single_click(0, 0.92, -10.0), 1, "event HalfCockStart"),
    "SW_Longgun_FullCock": (single_click(1, 0.9, -8.0, lambda: n3_click(3, 1.05)), 1, "event FullCockEnd"),
    "SW_Longgun_FrizzenClose": (frizzen_close, 1, "event FrizzenClose"),
    "SW_Longgun_PanPrime": (pan_prime, 1, "event PanPrime"),
    "SW_Longgun_StockGround": (stock_ground, 1, "event StockLowered"),
    "SW_Longgun_PowderCharge": (powder_charge, 1, "event PowderCharge"),
    "SW_Longgun_BallPatch": (ball_patch, 1, "event BallAndPatch"),
    "SW_Longgun_RamrodDraw": (ramrod_draw, 1, "event RamrodDraw"),
    "SW_Longgun_RamrodTurn": (ramrod_turn, 1, "event RamrodTurn"),
    "SW_Longgun_RamrodStroke": (ramrod_stroke, 3, "event RamrodStroke (twice per reload)"),
    "SW_Longgun_RamrodReturn": (ramrod_return, 1, "event RamrodReturn"),
    "SW_Blunderbuss_ShotPour": (shot_pour, 1, "event ShotPour (blunderbuss)"),
    "SW_Rifle_PatchBox": (patch_box, 1, "event PatchBox (rifle)"),
    "SW_Pistol_PaperPickup": (paper_pickup, 1, "event PaperPickup"),
    "SW_Pistol_PaperTransfer": (paper_transfer, 1, "event PaperTransfer"),
    "SW_Pistol_Ramrod": (pistol_ramrod, 1, "event RamrodGesture"),
    "SW_Pistol_RamrodReturn": (pistol_ramrod_return, 1, "event RamrodReturned"),
    "SW_Shotgun_PumpSlide": (pump_slide, 1, "events PumpBackStart, PumpForwardStart"),
    "SW_Shotgun_PumpBack": (pump_back, 3, "event PumpBackEnd"),
    "SW_Shotgun_PumpForward": (pump_forward, 3, "event PumpForwardEnd"),
    "SW_Shotgun_ShellEject": (shell_eject, 1, "event EjectSpentShell"),
    "SW_Shotgun_ShellPickup": (shell_pickup, 1, "event ShellPickup"),
    "SW_Shotgun_ShellInsert": (shell_insert, 3, "event InsertSlug (up to 8 per reload)"),
    "SW_Shotgun_DryClick": (shotgun_dry, 1, "event DryClick (shotgun)"),
    "SW_Grenade_Light": (fuse_light, 1, "event Light (grenade)"),
    "SW_Grenade_FuseLoop": (fuse_loop, 1, "fuse on the projectile: no field yet (see the doc)"),
    "SW_Grenade_Throw": (grenade_throw, 1, "event Fire (grenade throw)"),
    "SW_Grenade_BagReach": (bag_reach, 1, "event BagReach"),
    "SW_Grenade_Take": (grenade_take, 1, "event HammerCocked (grenade)"),
    "SW_Grenade_EmptyBag": (empty_bag, 1, "events DryClick and Empty (grenade)"),
    "SW_Grenade_BagToss": (bag_toss, 1, "event Release (grenade)"),
    "SW_Firearm_Draw": (draw, 1, "event DrawWeapon"),
    "SW_Firearm_Stow": (stow, 1, "event StowWeapon"),
    "SW_Firearm_Ready": (ready, 1, "event Ready"),
    "SW_Firearm_HandGrab": (hand_grab, 1, "events AssistLeft, LeftHandRegrab"),
    "SW_Firearm_DiscardEmpty": (discard_empty, 1, "event Empty (Discard clip)"),
    "SW_Firearm_DiscardRelease": (discard_release, 2, "event Release (Discard clip)"),
    "SW_Ammo_Pickup": (ammo_pickup, 1, "ammunition picked up: no field yet (see the doc)"),
}
LOOPS = {"SW_Grenade_FuseLoop"}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--only", default="")
    args = parser.parse_args()
    OUT.mkdir(parents=True, exist_ok=True)
    manifest_path = OUT / "manifest.json"
    previous = json.loads(manifest_path.read_text("utf-8"))["sounds"] if manifest_path.exists() else []
    entries = [e for e in previous if args.only and not e["group"].startswith(args.only)]
    for group, (builder, variants, role) in CATALOGUE.items():
        if args.only and not group.startswith(args.only):
            continue
        for variant in range(variants):
            name = f"{group}_{variant + 1:02d}" if variants > 1 else group
            sound = builder(name, variant)
            path = OUT / f"{name}.wav"
            decoded = dsp.write_pcm16(path, sound.data)
            loop = group in LOOPS
            measured = dsp.measures(decoded, loop)
            assert measured["peak_dbfs"] < -0.9, name
            assert loop or measured["ends_silent"], name
            assert not loop or measured["loop_seam_ok"], name
            entries.append({"asset": name, "group": group, "variant": variant + 1, "variants": variants, "role": role,
                            "file": path.relative_to(ROOT).as_posix(), "sha256": dsp.sha256(path), **measured, "loop": loop,
                            "sources": sound.credits, "synthetic_layers": sound.synth,
                            "licenses": sorted({c["license"] for c in sound.credits})})
            print(f"{name:34s} {measured['seconds']:5.2f} s  peak {measured['peak_dbfs']:6.1f}  loud100 "
                  f"{measured['loudest_100ms_db']:6.1f}  {len(sound.credits)} rec + {len(sound.synth)} synth", flush=True)
    entries.sort(key=lambda e: e["asset"])
    manifest = {"schema": 1, "task": "T-259", "generated_at": datetime.now(timezone.utc).isoformat(),
                "format": "48 kHz PCM16 mono", "validation": "decoded WAV data only; Unreal import by "
                "scripts/audio/weapons_audio_setup_ue.py, listening in PIE to do", "sounds": entries}
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    licences = sorted({lic for e in entries for lic in e["licenses"]})
    print(f"{len(entries)} WAV in {OUT.relative_to(ROOT).as_posix()}; source licences: {licences}")


if __name__ == "__main__":
    main()
