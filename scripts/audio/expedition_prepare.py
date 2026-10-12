"""T-234: prepare selected CC0 recordings as 48 kHz PCM16 Unreal sources.

This checks the audio data, not its artistic quality or its playback in Unreal.
Loops are crossfaded; source files, source hashes and processing remain traceable.
"""
from __future__ import annotations

import hashlib
import json
import math
from datetime import datetime, timezone
from pathlib import Path

import numpy as np
import soundfile as sf
from scipy.signal import butter, find_peaks, resample_poly, sosfilt

ROOT = Path(__file__).resolve().parents[2]
RAW = ROOT / "art/audio_src/expedition"
OUT = ROOT / "art/audio/expedition"
RATE = 48000
DEST = "/Game/MonkeIsland/Audio/Expedition"


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def seam_loop(audio, seconds=0.12):
    """Overlap tail/head, then rotate to a continuous join inside the recording."""
    count = min(int(seconds * RATE), len(audio) // 4)
    if count < 2:
        raise ValueError("Loop too short")
    ramp = np.linspace(0, 1, count)[:, None]
    join = audio[-count:] * (1 - ramp) + audio[:count] * ramp
    return np.concatenate((audio[count:-count], join))


def load(path, stereo=False):
    audio, rate = sf.read(path, dtype="float64", always_2d=True)
    if not stereo:
        audio = np.mean(audio, axis=1, keepdims=True)
    if rate != RATE:
        div = math.gcd(rate, RATE)
        audio = resample_poly(audio, RATE // div, rate // div, axis=0)
    if not np.isfinite(audio).all() or not len(audio):
        raise ValueError(f"Invalid audio: {path}")
    return audio - np.mean(audio, axis=0)


def main():
    inventory = json.loads((RAW / "inventory.json").read_text("utf-8"))
    OUT.mkdir(parents=True, exist_ok=True)
    entries = []

    def make(name, source, filename, *, loop=False, stereo=False, lowpass=0,
             seconds=None, start=0, group="Foley", gain=0.7, comment=""):
        matches = [f for f in inventory[source]["files"] if Path(f["file"]).name == filename]
        if len(matches) != 1:
            raise ValueError(f"Ambiguous/missing source {source}:{filename}")
        original = ROOT / matches[0]["file"]
        if sha(original) != matches[0]["sha256"]:
            raise ValueError(f"Source changed: {original}")
        audio = load(original, stereo)
        begin = int(start * RATE)
        audio = audio[begin:begin + int(seconds * RATE) if seconds else None]
        if lowpass:
            audio = sosfilt(butter(4, lowpass, fs=RATE, output="sos"), audio, axis=0)
        if loop:
            audio = seam_loop(audio, 0.12 if group == "Music" else min(0.5, len(audio) / RATE / 8))
        else:
            n = min(240, len(audio) // 8)
            audio[:n] *= np.linspace(0, 1, n)[:, None]
            audio[-n:] *= np.linspace(1, 0, n)[:, None]
        peak = float(np.max(np.abs(audio)))
        if peak < 1e-6:
            raise ValueError(f"Silent source: {name}")
        # Moderate peak ceiling; actual mix levels are in the DA and sound class.
        audio *= 10 ** (-6 / 20) / peak
        path = OUT / f"SW_Exp_{name}.wav"
        sf.write(path, audio, RATE, subtype="PCM_16")
        decoded, rate = sf.read(path, always_2d=True)
        entry = {
            "id": name, "file": path.relative_to(ROOT).as_posix(), "asset": f"{DEST}/SW_Exp_{name}",
            "source_id": source, "source_file": matches[0]["file"], "source_sha256": matches[0]["sha256"],
            "author": inventory[source]["author"], "source_url": inventory[source]["page"],
            "license": inventory[source]["license"], "sha256": sha(path),
            "seconds": round(len(decoded) / rate, 4), "rate": rate, "channels": decoded.shape[1],
            "peak_dbfs": round(20 * np.log10(np.max(np.abs(decoded))), 2),
            "rms_dbfs": round(20 * np.log10(np.sqrt(np.mean(decoded ** 2))), 2),
            "loop_boundary_delta": round(float(np.max(np.abs(decoded[-1] - decoded[0]))), 6) if loop else None,
            "loop": loop, "group": group, "gain": gain,
            "processing": {"start": start, "duration_limit": seconds, "lowpass_hz": lowpass,
                           "loop_crossfade": loop, "resample_hz": RATE, "peak_ceiling_dbfs": -6},
            "note": comment, "technical_validated": True, "listened": False,
            "imported": False, "gameplay_validated": False,
        }
        assert rate == RATE and np.max(np.abs(decoded)) < 0.99 and len(decoded) > 480
        entries.append(entry)
        return entry

    make("HullWater", "waves", "wave_02_cc0-11505__transitking__wavesound.flac", loop=True, gain=.22, group="Ambience")
    make("ShoreSurf", "waves", "wave_03_cc0-11505__transitking__wavesound.flac", loop=True, group="Ambience", gain=.4)
    make("ShoreSea", "waves", "wave_04_cc0-11505__transitking__wavesound.flac", loop=True, stereo=True, group="Ambience", gain=.3)
    make("Underwater", "water", "loop_bubbles_1.ogg", loop=True, lowpass=520, group="Ambience", gain=.2)
    make("Rain", "water", "loop_rain.ogg", loop=True, group="Ambience", gain=.25)
    make("WindLight", "wind", "wind1.wav", loop=True, seconds=30, group="Ambience", gain=.12)
    make("WindStrong", "wind", "wind2.wav", loop=True, seconds=30, group="Ambience", gain=.3)
    make("WindStorm", "wind", "wind3.wav", loop=True, seconds=30, group="Ambience", gain=.45)
    make("Thunder", "foley2", "sfx100v2_thunder_01.ogg", group="Distant", gain=.5)
    make("Crickets", "crickets", "crickets_1.mp3", loop=True, group="Ambience", gain=.15,
         comment="Ambiance générique : aucune identification d'espèce de Raraka.")
    make("Campfire", "fire", "fire-1.wav", loop=True, group="Ambience", gain=.3)
    for i in range(1, 4):
        make(f"Splash_{i:02}", "water", f"splash_{i:02}.ogg", group="Water")
        make(f"WoodCreak_{i:02}", "wood_metal", f"wood_cracking_{i:02}.ogg", gain=.25)
        make(f"Chain_{i:02}", "rpg", f"chain_{i:02}.ogg", gain=.45)
        make(f"Coin_{i:02}", "rpg", f"item_coins_{i:02}.ogg", group="UI", gain=.65)
        make(f"Gem_{i:02}", "rpg", f"item_gem_{i:02}.ogg", group="UI", gain=.55)
    make("SwimEnter", "sand", "splash1_0.wav", group="Water", gain=.55)
    make("SwimExit", "sand", "splash2_0.wav", group="Water", gain=.4)
    make("WaterObject", "water", "splash_12.ogg", group="Water", gain=.7)
    make("Bell", "foley", "bell_01.ogg", group="Distant", gain=.45)
    make("OarCreak", "wood_metal", "wood_squeak_01.ogg", gain=.15,
         comment="Foley de bois, à associer au geste de rame, pas une prise nautique isolée.")
    make("ChestOpen", "foley", "wooded_box_open.ogg", gain=.7)
    make("ChestClose", "wood_metal", "wood_close_01.ogg", gain=.65)
    make("DoorOpen", "wood_metal", "door_open_01.ogg", gain=.6)
    make("DoorClose", "foley", "door_close_01.ogg", gain=.6)
    for i in range(1, 3):
        make(f"Ceramic_{i:02}", "foley", f"dishes_{i:02}.ogg", gain=.5)
        make(f"FragileBreak_{i:02}", "foley", f"glass_{i:02}.ogg", gain=.5)
        make(f"StoneScrape_{i:02}", "rpg", f"stones_{i:02}.ogg", gain=.5)
        make(f"MetalHit_{i:02}", "wood_metal", f"metal_hit_{i:02}.ogg", gain=.55)
        make(f"WoodHit_{i:02}", "wood_metal", f"wood_hit_{i:02}.ogg", gain=.6)
    for name, filename in {"CannonFire":"cannon_fire_1.ogg", "CannonWood":"cannon_hit_ship_short.ogg",
                           "CannonStone":"cannon_hit_wall_no_splash.ogg", "CannonMetal":"cannon_hit_cannon_1.ogg",
                           "CannonWater":"cannon_miss_1.ogg", "ShipCollision":"ship_ram_ship_shortened.ogg"}.items():
        make(name, "battle", filename, group="Distant", gain=.75)
    make("BladeDraw", "rpg", "blade_01.ogg", group="Weapon", gain=.7)
    make("BladeSheathe", "rpg", "blade_02.ogg", group="Weapon", gain=.6)
    make("BladeParry", "wood_metal", "metal_hit_03.ogg", group="Weapon", gain=.7)
    make("CrabClack", "wood_metal", "wood_hit_03.ogg", gain=.6, comment="Claquement stylisé de carapace (foley bois).")
    make("CrabSlam", "wood_metal", "wood_slam_02.ogg", gain=.8, comment="Impact stylisé, pas une vocalisation animale.")
    make("CrabHurt", "rpg", "creature_hurt_01.ogg", gain=.55, comment="Créature de fiction, audition artistique restante.")
    make("CreatureRare", "rpg", "creature_roar_02.ogg", group="Distant", gain=.5)
    make("ArtifactPulse", "rpg", "spell_01.ogg", gain=.35)
    make("ArtifactCurse", "foley", "weird_01.ogg", lowpass=1800, group="Distant", gain=.45)
    # Four isolated sand steps from the real recording, replacing the old snow placeholder.
    sand = load(RAW / "sand/audio/sand_footsteps_0.mp3")[:, 0]
    frame = 480
    envelope = np.sqrt(np.mean(sand[:len(sand)//frame*frame].reshape(-1, frame)**2, axis=1))
    peaks, _ = find_peaks(envelope, distance=40, prominence=max(envelope) * .15)
    if len(peaks) < 4:
        raise ValueError("Cannot isolate four sand steps; manual edit required")
    for i, peak in enumerate(peaks[:4], 1):
        make(f"SandStep_{i:02}", "sand", "sand_footsteps_0.mp3", start=max(0, peak * .01 - .1), seconds=.46, gain=.7)
    for i in range(1, 4):
        make(f"WetStep_{i:02}", "foley2", f"sfx100v2_footstep_wet_{i:02}.ogg", gain=.65)
    for name, number in {"DayStart":0, "QuotaMet":4, "QuotaFailed":11, "Victory":7, "Defeat":15,
                          "Merchant":3, "Legendary":5, "Danger":13}.items():
        make(name, "jingles", f"jingles_PIZZI{number:02}.ogg", stereo=True, group="Music", gain=.4,
             comment="Jingle candidat : vérifier la couleur émotionnelle à l'écoute en contexte.")
    make("PirateTheme", "pirate_music", "pirate_ship_in_game.ogg", loop=True, stereo=True, group="Music", gain=.45,
         comment="Alternative CC0 disponible; les trois musiques choisies par l'utilisateur restent inchangées.")
    manifest = {"schema": 1, "task": "T-234", "generated_at": datetime.now(timezone.utc).isoformat(),
                "validation": "decoded WAV data only; Unreal import and listening not yet run", "sounds": entries}
    (OUT / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{len(entries)} WAV prepared; {sum(x['seconds'] for x in entries):.1f}s; 48kHz PCM16; peak ceiling -6dBFS")


if __name__ == "__main__":
    main()
