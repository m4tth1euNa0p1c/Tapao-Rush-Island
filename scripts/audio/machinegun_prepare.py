"""T-273: designs the sounds of the crank machine gun (a heavy belt-fed gun of 1918 with a water jacket) from the CC0
recordings already fetched for T-259 (scripts/audio/weapons_v2_fetch.py, expedition_fetch.py) and a few synthetic
layers, as 48 kHz PCM16 mono Unreal sources.

  python -I scripts/audio/machinegun_prepare.py [--only NAME_PREFIX]   ->  art/audio/weapons/machinegun/*.wav + manifest.json

Aim (owner 2026-10-10: "le son ... ultra réaliste type Call of Duty"): the burst is built like an automatic weapon of a
modern game, round by round so it follows the crank whatever its cadence (450 to 600 a minute):
  - every round (FireSound, 4 variants, SoundCue Random): the report of a rifle cartridge (Mosin, SKS, CZ recordings,
    cut before their long echo), a dry supersonic crack, a sub thump, the lock slamming home 18 ms later and a short
    slap of the place; no long tail, so 10 rounds a second stay a clean rattle;
  - while the crank turns (MechanismLoopSound, seamless 2 s loop at exactly 10 rounds a second; its pitch follows the
    cadence in game, 0.75 at 450 a minute): the feed pawl at every round, the crank's ratchet (16 teeth, 2.5 turns a
    second), a recorded machine bed, the belt jingling into the feed;
  - when the burst ends (BurstTailSound, 3 variants): the echo of the place (the recordings' own tails and a synthetic
    outdoor decay with late slaps off the reef and the tree line) and the crank running down;
  - the crank engaging (BurstStartSound), the brass landing (CasingLandSound, 6 variants), the reload events (CoverOpen,
    BeltOut, BoxOut, BoxIn, BeltIn, CoverClose, Charge), the draw of a heavy gun, the empty gun thrown away, and the heat:
    the overheat (ping of the hot steel and a first hiss), the steam vent (event SteamVent) and the simmering of the
    jacket (SteamLoopSound, seamless 3 s loop).
Every source file is checked against its inventory SHA-256 (weapons_v2_prepare.Bank); the manifest records, for every
WAV, the source files and the times used, the synthetic layers and the measures. Deterministic (seeded by name).
"""
from __future__ import annotations

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path

import numpy as np

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audio_dsp as dsp  # noqa: E402
from weapons_v2_prepare import KIMP, KRPG, LFA3, RATE, Sound, cut, finish, outdoor_tail, punch, rng_for, synth  # noqa: E402

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art/audio/weapons/machinegun"

MOSIN = ("tabasco_gunshots", "mosin.wav")
SKS = ("tabasco_gunshots", "sks.wav")
CZ = ("tabasco_gunshots", "cz.wav")
UNKNOWN = ("kurt_gunshots", "Unkown.wav")
MAGNUM = ("kurt_gunshots", "22 Magnum.wav")
CANNON = ("thimras_cannon", "cannon_fire_0.ogg")
# the strong reports of the rifle recordings (onset, s; a 2 ms envelope scan) and the dry cracks; the recordings' own
# tails carry echoes and other shots within a second, so only their first 0.2 s are used, the echo is synthetic
ROUND_TAKES = ((MOSIN, 0.452), (SKS, 2.304), (MOSIN, 6.004), (CZ, 2.828))
CRACK_TAKES = ((UNKNOWN, 0.266), (MAGNUM, 0.236), (UNKNOWN, 2.310), (MAGNUM, 0.936))
ROUNDS_PER_SECOND = 10.0


# ------------------------------------------------------------------------------------------------- synthetic layers

def ticks(seconds_long, rate_hz, seed, band=(1800.0, 5200.0), size_s=0.002, jitter_s=0.0008, slow_to=None):
    """Regular mechanical ticks (a ratchet, a pawl): short decaying noise bursts at rate_hz, slowing down to slow_to Hz
    over the length when given (a crank running down)"""
    rng = np.random.default_rng(seed)
    out = np.zeros(dsp.samples(seconds_long))
    n = dsp.samples(size_s)
    env = np.exp(-np.arange(n) / (n / 4))
    at = 0.0
    while at < seconds_long - size_s:
        dsp.place(out, rng.standard_normal(n) * env * rng.uniform(0.6, 1.0), max(0.0, at + rng.uniform(-jitter_s, jitter_s)))
        rate = rate_hz if slow_to is None else rate_hz + (slow_to - rate_hz) * (at / seconds_long)
        at += 1.0 / max(rate, 1.0)
    out = dsp.butter_filter(out, "bandpass", band, 2)
    return synth("ticks", out, rate_hz=rate_hz, slow_to=slow_to, seconds=seconds_long, seed=seed)


def brass_ring(seconds_long, seed, tune=1.0):
    """The ring of a small brass case: a few inharmonic partials of a thin tube, fast decays"""
    rng = np.random.default_rng(seed)
    t = np.arange(dsp.samples(seconds_long)) / RATE
    partials = ((3900.0, 1.0, 0.045), (5300.0, 0.35, 0.06), (7350.0, 0.6, 0.03), (10700.0, 0.4, 0.02))
    wave = sum(a * np.sin(2 * np.pi * f * tune * t + rng.uniform(0, 6.28)) * np.exp(-t / d) for f, a, d in partials)
    return synth("brass_ring", dsp.fade(wave, 0.0005, 0.02), tune=tune, seed=seed)


def steam_hiss(seconds_long, seed, attack_s=0.03, decay_s=0.8, whistle_hz=None, sustain=0.0):
    """Steam out of a vent: bright noise, a resonant band that wavers, an optional whistle; attack, then decay to a
    sustain level"""
    rng = np.random.default_rng(seed)
    n = dsp.samples(seconds_long)
    t = np.arange(n) / RATE
    noise = rng.standard_normal(n)
    bright = dsp.butter_filter(noise, "highpass", 2200.0, 2)
    band = dsp.butter_filter(noise, "bandpass", (3800.0, 6200.0), 2)
    waver = 0.75 + 0.25 * np.sin(2 * np.pi * 5.3 * t + rng.uniform(0, 6.28)) * np.sin(2 * np.pi * 0.7 * t)
    env = (1 - np.exp(-t / attack_s)) * (sustain + (1 - sustain) * np.exp(-t / decay_s))
    wave = (0.6 * bright + band * waver) * env
    if whistle_hz:
        drift = whistle_hz * (1 + 0.02 * np.sin(2 * np.pi * 1.3 * t))
        wave += 0.08 * np.sin(2 * np.pi * np.cumsum(drift) / RATE) * env * np.abs(band).mean() * 6
    return synth("steam_hiss", dsp.fade(wave, 0.005, 0.1), seconds=seconds_long, decay_s=decay_s, whistle_hz=whistle_hz, seed=seed)


# ------------------------------------------------------------------------------------------------------------ rounds

def mg_round(name, variant):
    """One round: report cut before its echo, supersonic crack, chest thump, the lock slamming home, a short slap"""
    rng = rng_for(name)
    s = Sound(0.8)
    source, onset = ROUND_TAKES[variant]
    body = cut(*source, onset - 0.012, 0.2, snap=-24, fade_out=0.08, pitch=1.0 + 0.025 * (variant - 1.5))
    s.add(body, 0.0, 0)
    crack_source, crack_onset = CRACK_TAKES[variant]
    s.add(cut(*crack_source, crack_onset - 0.006, 0.08, snap=-24, hp=1800, fade_out=0.04), 0.0, -6)
    s.add(punch(115, 48, 0.2, 0.035, drive=2.2), 0.0, -9)
    s.add(cut(KIMP, f"impactMetal_light_00{variant}.ogg", 0.0, 0.08, pitch=0.85, snap=-30, lp=7000, fade_out=0.03), 0.018, -13)
    s.add(outdoor_tail(body[0][:dsp.samples(0.12)], int(rng.integers(1 << 30)), 0.45, ((0.09, 0.25), (0.21, 0.12))), 0.0, -17)
    return finish(s, -2.0, limit=True, max_seconds=0.6, shelf_db=3.0, loud_db=-12.0)


def mechanism_loop(name, variant):
    """The crank at full speed, seamless over 2 s (20 rounds): feed pawl at every round, crank ratchet, machine bed,
    belt jingle"""
    rng = rng_for(name)
    seconds = 2.2
    s = Sound(seconds)
    s.add(cut("foley2", "sfx100v2_loop_machine_01.ogg", 1.0, seconds, hp=300, lp=6000, fade_in=0.0, fade_out=0.0), 0.0, -9)
    for index in range(int(seconds * ROUNDS_PER_SECOND)):
        click = cut(KIMP, f"impactMetal_light_00{index % 2 + 1}.ogg", 0.0, 0.05, pitch=1.6, snap=-30, hp=1500, fade_out=0.02)
        s.add(click, 0.05 + index / ROUNDS_PER_SECOND, -6 - 2 * (index % 2))
    s.add(ticks(seconds, 40.0, int(rng.integers(1 << 30))), 0.0, -13)
    for at in (0.1, 0.75, 1.4, 1.95):
        s.add(cut("rpg", "chain_03.ogg", 0.0, 0.5, pitch=0.8, snap=-30, lp=7000, fade_out=0.1), at + rng.uniform(0.0, 0.08), -17)
    # exactly 22 rounds long before the seam (a layer past the end grew the buffer): the loop is 2 s, 20 rounds
    s.data = dsp.seam_loop(dsp.normalize_peak(s.data[:dsp.samples(seconds)], -9.0), 0.2)
    return s


def burst_start(name, variant):
    """The crank engages: the handle's clunk, the gear catching"""
    rng = rng_for(name)
    s = Sound(0.7)
    s.add(cut("wood_metal", "metal_close_01.ogg", 0.0, 0.3, snap=-30, pitch=0.8, lp=5000), 0.0, -2)
    s.add(cut(KIMP, "impactMetal_heavy_002.ogg", 0.0, 0.25, pitch=0.75, snap=-30), 0.01, 0)
    s.add(ticks(0.3, 22.0, int(rng.integers(1 << 30)), slow_to=40.0), 0.02, -9)
    return finish(s, -10.0, end_db=-50)


def crank_tick(name, variant):
    """CrankTick (FireStart, FireEnd): the crank's pawl drops into its detent, a short ratchet before it"""
    rng = rng_for(name)
    s = Sound(0.4)
    s.add(ticks(0.06, 60.0, int(rng.integers(1 << 30))), 0.0, -6)
    s.add(cut(KRPG, "metalClick.ogg", 0.0, 0.2, snap=-30, pitch=0.8), 0.05, 0)
    s.add(cut(KIMP, "impactMetal_medium_002.ogg", 0.0, 0.15, snap=-30, pitch=0.9, lp=6000), 0.055, -8)
    return finish(s, -12.0, end_db=-50)


def burst_tail(name, variant):
    """The burst ends: the place answers (a synthetic outdoor decay of a dry round, late slaps off the reef and the
    tree line, the low roll of a cannon recording under it), the crank runs down"""
    rng = rng_for(name)
    s = Sound(3.4)
    source, onset = ROUND_TAKES[variant]
    dry = cut(*source, onset - 0.012, 0.2, snap=-24, fade_out=0.08)[0]
    slaps = ((0.26 + 0.03 * variant, 0.45), (0.62 + 0.05 * variant, 0.28), (1.15, 0.14))
    s.add(outdoor_tail(dry, int(rng.integers(1 << 30)), 2.4, slaps, lowpass_hz=3800.0), 0.0, 0)
    s.add(cut(*CANNON, 0.25, 2.4, lp=320, fade_in=0.08, fade_out=0.8), 0.05, -12)
    s.add(ticks(0.45, 40.0, int(rng.integers(1 << 30)), slow_to=8.0), 0.0, -16)
    return finish(s, -6.0, end_db=-58, fade_out=0.4, max_seconds=3.2)


# ------------------------------------------------------------------------------------------------------------ brass

def casing_land(name, variant):
    """A spent case lands: a bright brass tink with its ring, a smaller bounce"""
    rng = rng_for(name)
    s = Sound(0.45)
    s.add(cut(KIMP, f"impactMetal_light_00{variant % 5}.ogg", 0.0, 0.2, pitch=1.7 + 0.07 * variant, snap=-30, hp=1200), 0.0, 0)
    s.add(brass_ring(0.25, int(rng.integers(1 << 30)), 0.92 + 0.03 * variant), 0.0, -7)
    s.add(cut(KIMP, f"impactTin_medium_00{(variant + 2) % 5}.ogg", 0.0, 0.12, pitch=1.9, snap=-30, hp=1500), 0.07 + 0.012 * variant, -10)
    return finish(s, -14.0, end_db=-50)


# ------------------------------------------------------------------------------------------------------------ reload

def cover_open(name, variant):
    s = Sound(0.8)
    s.add(cut("wood_metal", "lock_open_01.ogg", 0.0, 0.3, snap=-30, pitch=0.9), 0.0, 0)
    s.add(cut("wood_metal", "metal_open_01.ogg", 0.0, 0.6, snap=-30, pitch=0.85, lp=6000), 0.08, -4)
    return finish(s, -10.0, end_db=-50)


def belt_out(name, variant):
    s = Sound(0.9)
    s.add(cut("rpg", "chain_01.ogg", 0.0, 0.5, snap=-30, pitch=0.75, lp=8000), 0.0, 0)
    s.add(cut(KRPG, "cloth2.ogg", 0.0, 0.3, snap=-30), 0.05, -6)
    s.add(cut("rpg", "chain_02.ogg", 0.0, 0.45, snap=-30, pitch=0.7, lp=7000), 0.28, -3)
    return finish(s, -12.0, end_db=-50)


def box_out(name, variant):
    s = Sound(0.8)
    s.add(cut(KRPG, "metalLatch.ogg", 0.0, 0.25, snap=-30, pitch=0.85), 0.0, 0)
    s.add(cut(KIMP, "impactWood_medium_002.ogg", 0.0, 0.25, snap=-30, lp=3500), 0.12, -3)
    s.add(cut(KRPG, "beltHandle2.ogg", 0.0, 0.4, snap=-30), 0.2, -6)
    return finish(s, -12.0, end_db=-50)


def box_in(name, variant):
    s = Sound(0.8)
    s.add(cut(KIMP, "impactWood_heavy_001.ogg", 0.0, 0.3, snap=-30, lp=3000), 0.0, 0)
    s.add(cut(KIMP, "impactMetal_heavy_003.ogg", 0.0, 0.3, snap=-30, pitch=0.8, lp=5000), 0.005, -3)
    s.add(cut(KRPG, "metalClick.ogg", 0.0, 0.2, snap=-30, pitch=0.9), 0.3, -4)
    return finish(s, -9.0, end_db=-50)


def belt_in(name, variant):
    s = Sound(0.8)
    s.add(cut("rpg", "chain_03.ogg", 0.0, 0.6, snap=-30, pitch=0.85, lp=8000), 0.0, -2)
    s.add(cut("foley", "switch_01.ogg", 0.0, 0.2, snap=-30, pitch=0.9), 0.32, 0)
    s.add(cut(*LFA3, 2.785, 0.12, snap=-8, pitch=0.9), 0.4, -4)
    return finish(s, -11.0, end_db=-50)


def cover_close(name, variant):
    s = Sound(0.8)
    s.add(cut("wood_metal", "metal_slam_01.ogg", 0.0, 0.5, snap=-30, pitch=0.9), 0.0, 0)
    s.add(cut("wood_metal", "metal_close_01.ogg", 0.0, 0.3, snap=-30, pitch=1.05, lp=7000), 0.01, -4)
    return finish(s, -8.0, end_db=-50)


def charge(name, variant):
    """The cocking handle pulled back twice, heavy: the first round fed"""
    s = Sound(1.2)
    rack = cut("zer0_shotgun", "Rack.mp3", 0.6, 0.4, snap=-20, pitch=0.78, lp=7000)
    s.add(rack, 0.0, 0)
    s.add(cut("springy_reloads", "shotguncock_0.wav", 0.05, 0.35, snap=-20, pitch=0.8), 0.42, -2)
    s.add(cut(KIMP, "impactMetal_heavy_001.ogg", 0.0, 0.2, snap=-30, pitch=0.7, lp=4000), 0.5, -8)
    return finish(s, -7.0, end_db=-50)


def draw_heavy(name, variant):
    """A heavy gun lifted off the shoulder: strap, leather, the iron settling in the hands"""
    s = Sound(1.0)
    s.add(cut(KRPG, "clothBelt.ogg", 0.0, 0.7, snap=-30), 0.0, 0)
    s.add(cut(KIMP, "impactMetal_heavy_004.ogg", 0.0, 0.25, snap=-30, pitch=0.65, lp=3000), 0.45, -4)
    s.add(cut(KRPG, "metalLatch.ogg", 0.0, 0.2, snap=-30, pitch=0.8, lp=6000), 0.6, -9)
    return finish(s, -12.0, end_db=-50)


def discard_release(name, variant):
    """The empty gun thrown down: a short swing, iron and wood landing heavily on the sand"""
    s = Sound(1.3)
    s.add(cut("swishes", "swish-7.wav", 0.0, 0.2, snap=-30, pitch=0.7), 0.0, -4)
    s.add(cut(KIMP, "impactSoft_heavy_004.ogg", 0.0, 0.5, snap=-30, pitch=0.8), 0.3, 0)
    s.add(cut(KIMP, "impactMetal_heavy_000.ogg", 0.0, 0.3, snap=-30, pitch=0.7, lp=4500), 0.3, -3)
    s.add(cut(KIMP, "impactWood_heavy_003.ogg", 0.0, 0.3, snap=-30, lp=3500), 0.32, -6)
    s.add(cut(KIMP, "impactMetal_light_004.ogg", 0.0, 0.25, snap=-30, pitch=0.85), 0.5, -12)
    return finish(s, -7.0, end_db=-50)


# ------------------------------------------------------------------------------------------------------------- heat

def overheat(name, variant):
    """The barrel overheats: the steel pings, the jacket hisses at once"""
    rng = rng_for(name)
    s = Sound(1.8)
    s.add(cut(KIMP, "impactBell_heavy_002.ogg", 0.0, 0.6, snap=-30, pitch=1.6, hp=900), 0.0, -4)
    s.add(cut("foley2", "sfx100v2_air_02.ogg", 0.0, 1.1, snap=-30, fade_out=0.3), 0.04, 0)
    s.add(steam_hiss(1.6, int(rng.integers(1 << 30)), 0.02, 0.5, whistle_hz=3150.0), 0.05, -3)
    return finish(s, -8.0, end_db=-55, fade_out=0.25)


def steam_vent(name, variant):
    """SteamVent: the steam bursts out of the cap of the jacket, a long jet that dies down"""
    rng = rng_for(name)
    s = Sound(3.2)
    s.add(cut("foley2", "sfx100v2_air_01.ogg", 0.0, 2.6, snap=-30, fade_out=0.6), 0.0, 0)
    s.add(steam_hiss(3.0, int(rng.integers(1 << 30)), 0.05, 1.1, whistle_hz=2650.0), 0.0, -2)
    s.add(cut("water", "bubble_01.ogg", 0.0, 0.6, snap=-30, lp=3000), 0.4, -14)
    return finish(s, -7.0, end_db=-55, fade_out=0.5)


def steam_loop(name, variant):
    """The water of the jacket simmering: bubbles and a soft hiss, seamless over 3 s"""
    rng = rng_for(name)
    seconds = 3.3
    s = Sound(seconds)
    s.add(cut("water", "loop_bubbles_02.ogg", 0.5, seconds, lp=4000, fade_in=0.0, fade_out=0.0), 0.0, 0)
    s.add(steam_hiss(seconds, int(rng.integers(1 << 30)), 0.001, 100.0, sustain=1.0), 0.0, -12)
    s.data = dsp.seam_loop(dsp.normalize_peak(s.data, -14.0), 0.3)
    return s


# ----------------------------------------------------------------------------------------------------- the catalogue

# name -> (builder, variants, role); a name with variants gives NAME_01..NAME_0n (one SoundCue Random in Unreal)
CATALOGUE = {
    "SW_MG_Round": (mg_round, 4, "FireSound of DA_MachineGunRules: one round"),
    "SW_MG_MechanismLoop": (mechanism_loop, 1, "MechanismLoopSound: crank and feed while the crank turns (loop, pitch with the cadence)"),
    "SW_MG_BurstStart": (burst_start, 1, "BurstStartSound: the crank engages"),
    "SW_MG_CrankTick": (crank_tick, 1, "event CrankTick (FireStart, FireEnd): the pawl in its detent"),
    "SW_MG_BurstTail": (burst_tail, 3, "BurstTailSound: echo of the place and the crank running down"),
    "SW_MG_CasingLand": (casing_land, 6, "CasingLandSound: a spent case lands"),
    "SW_MG_CoverOpen": (cover_open, 1, "event CoverOpen (Reload)"),
    "SW_MG_BeltOut": (belt_out, 1, "event BeltOut (Reload)"),
    "SW_MG_BoxOut": (box_out, 1, "event BoxOut (Reload)"),
    "SW_MG_BoxIn": (box_in, 1, "event BoxIn (Reload)"),
    "SW_MG_BeltIn": (belt_in, 1, "event BeltIn (Reload)"),
    "SW_MG_CoverClose": (cover_close, 1, "event CoverClose (Reload)"),
    "SW_MG_Charge": (charge, 1, "event Charge (Reload)"),
    "SW_MG_Draw": (draw_heavy, 1, "event DrawWeapon (Equip)"),
    "SW_MG_DiscardRelease": (discard_release, 1, "event Release (Discard)"),
    "SW_MG_Overheat": (overheat, 1, "OverheatSound: at the start of the overheat"),
    "SW_MG_SteamVent": (steam_vent, 1, "event SteamVent (Overheat)"),
    "SW_MG_SteamLoop": (steam_loop, 1, "SteamLoopSound: the hot jacket simmering (loop)"),
}
LOOPS = {"SW_MG_MechanismLoop", "SW_MG_SteamLoop"}
# the kind of each sound in Unreal (attenuation and concurrency of scripts/editor/machinegun_setup_ue.py)
KINDS = {"SW_MG_Round": "shot", "SW_MG_BurstTail": "shot", "SW_MG_CasingLand": "brass"}


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
                            "kind": KINDS.get(group, "handling"), "file": path.relative_to(ROOT).as_posix(),
                            "sha256": dsp.sha256(path), **measured, "loop": loop, "sources": sound.credits,
                            "synthetic_layers": sound.synth, "licenses": sorted({c["license"] for c in sound.credits})})
            print(f"{name:26s} {measured['seconds']:5.2f} s  peak {measured['peak_dbfs']:6.1f}  loud100 "
                  f"{measured['loudest_100ms_db']:6.1f}  {len(sound.credits)} rec + {len(sound.synth)} synth", flush=True)
    entries.sort(key=lambda e: e["asset"])
    manifest = {"schema": 1, "task": "T-273", "generated_at": datetime.now(timezone.utc).isoformat(),
                "format": "48 kHz PCM16 mono", "validation": "decoded WAV data only; Unreal import by "
                "scripts/editor/machinegun_setup_ue.py, listening in PIE to do", "sounds": entries}
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    licences = sorted({lic for e in entries for lic in e["licenses"]})
    print(f"{len(entries)} WAV in {OUT.relative_to(ROOT).as_posix()}; source licences: {licences}")


if __name__ == "__main__":
    main()
