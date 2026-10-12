"""Generates monke_island/Source/monke_island/Weapons/PirateWeaponClips.inl from the manifests of the weapon packs.

Packs (format bean-musket-anim/1, see scripts/props/musket_pack and the docstring of import_pirate_weapons.py), one
table per weapon of the registry WEAPONS below, read from build/packs/<pack>/<manifest> first, then from OneDrive
(<MAQUETTES>/<pack>) for the pistol and the shotgun when the rebuilt pack is not there yet:
  Pistol        bean-pirate-pistol          14 gestures, 30 fps            Musket       bean-musket-anim   24 gestures, 24 fps
  DoublePistol  bean-pirate-pistol-double   14 gestures, 30 fps            Blunderbuss  bean-blunderbuss-anim  the musket's 24, 24 fps
  Shotgun       bean-pirate-shotgun         16 gestures, 30 fps            Rifle        bean-rifle-anim        the musket's 24, 24 fps
  Grenade       bean-grenade                13 gestures, 30 fps (Equip, Idle, Walk, Sprint, AimIn, AimIdle, AimOut, Fire, AimFire,
                                            Reload, DryFire, Inspect, Holster; events Light, Fire, HammerCocked)
A pack that is not built yet gets a FALLBACK table so the C++ keeps compiling: the musket's for the blunderbuss and
the rifle, the pistol's for the double pistol, a placeholder for the grenade (marked in the .inl); rerun once the
pack lands. The v2 packs add the gesture Discard (owner 2026-10-09: the empty weapon thrown away, events Empty and
Release); a pack without it leaves the table without it and the game plays DryFire then drops the weapon.

For every gesture: its length (end - start frames / fps) and its events (frame / fps), taken from the TP scene and
checked identical to the FP scene. The .inl fills the default Clips of the rules classes, so the server logic (shot,
pump, shell inserted, reload done, throw) has the right times even before the animations are assigned;
scripts/character/setup_pirate_weapons.py then plugs the imported AnimSequences into those clips (and rewrites the
times from the manifest). Gesture names must exist in EFirearmGesture (Weapons/FirearmRules.h, read here): an unknown
clip is skipped with a warning. Shotgun gestures FirePump / AimFirePump map to Fire / AimFire; a pack with Fire and no
AimFire plays Fire for both (the musket family).

  python scripts/character/pirate_weapon_clips.py            write the .inl
  python scripts/character/pirate_weapon_clips.py --check    compare with the .inl on disk, exit 1 if stale
"""
import json
import os
import re
import sys

MAQUETTES = r"C:\Users\matth\OneDrive\maquette-blender\maquettes"
ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..")
PACKS = os.path.join(ROOT, "build", "packs")
OUT = os.path.join(ROOT, "monke_island", "Source", "monke_island", "Weapons", "PirateWeaponClips.inl")
RULES_HEADER = os.path.join(ROOT, "monke_island", "Source", "monke_island", "Weapons", "FirearmRules.h")

# weapon -> pack folder, manifest, OneDrive fallback pack (the first packs of the second PC) and the table used when
# the pack is not built yet (another weapon of this registry, or the placeholder)
WEAPONS = {
	"Pistol": {"pack": "bean-pirate-pistol", "manifest": "pistol-manifest.json", "onedrive": "bean-pirate-pistol"},
	"DoublePistol": {"pack": "bean-pirate-pistol-double", "manifest": "pistol-double-manifest.json", "fallback": "Pistol"},
	"Shotgun": {"pack": "bean-pirate-shotgun", "manifest": "shotgun-manifest.json", "onedrive": "bean-pirate-shotgun"},
	"Musket": {"pack": "bean-musket-anim", "manifest": "musket-manifest.json"},
	"Blunderbuss": {"pack": "bean-blunderbuss-anim", "manifest": "blunderbuss-manifest.json", "fallback": "Musket"},
	"Rifle": {"pack": "bean-rifle-anim", "manifest": "rifle-manifest.json", "fallback": "Musket"},
	"Grenade": {"pack": "bean-grenade", "manifest": "grenade-manifest.json", "fallback": "GrenadePlaceholder"},
}
GESTURE = {"FirePump": "Fire", "AimFirePump": "AimFire"}
LOOPS = {"Idle", "Walk", "Sprint", "AimIdle", "Hold"}
# the grenade's gestures until bean-grenade is built: (clip, frames, fps, [(frame, event)]), 30 fps
GRENADE_PLACEHOLDER = [
	("Equip", 54, 30, [(8, "DrawWeapon"), (45, "Ready")]),
	("Idle", 90, 30, []),
	("Walk", 120, 30, []),
	("Sprint", 90, 30, []),
	("AimIn", 30, 30, []),
	("AimIdle", 90, 30, []),
	("AimOut", 30, 30, []),
	("Fire", 48, 30, [(10, "Light"), (24, "Fire")]),
	("AimFire", 48, 30, [(10, "Light"), (26, "Fire")]),
	("Reload", 60, 30, [(40, "HammerCocked")]),
	("DryFire", 30, 30, [(10, "DryClick")]),
	("Inspect", 180, 30, []),
	("Holster", 54, 30, [(48, "StowWeapon")]),
]


def manifest_path(weapon):
	"""Manifest of a weapon: the pack built here, else the OneDrive pack, else None."""
	cfg = WEAPONS[weapon]
	local = os.path.join(PACKS, cfg["pack"], cfg["manifest"])
	if os.path.isfile(local):
		return local
	if cfg.get("onedrive"):
		remote = os.path.join(MAQUETTES, cfg["onedrive"], cfg["manifest"])
		if os.path.isfile(remote):
			return remote
	return None


def gesture_names():
	"""Values of EFirearmGesture, read from FirearmRules.h."""
	text = open(RULES_HEADER, encoding="utf-8").read()
	body = re.search(r"enum class EFirearmGesture : uint8\s*\{(.*?)\};", text, re.S).group(1)
	body = re.sub(r"/\*.*?\*/", "", body, flags=re.S)
	body = re.sub(r"//[^\n]*", "", body)
	return [token.strip() for token in body.split(",") if token.strip()]


def clips(path):
	"""(clip, frames, fps, [(frame, event)]) of every TP scene of a manifest."""
	manifest = json.load(open(path, encoding="utf-8"))
	by_view = {}
	for scene in manifest["scenes"]:
		events = [(e["frame"] - scene["start"], e["name"]) for e in scene["events"]]
		by_view[(scene["view"], scene["clip"])] = (scene["end"] - scene["start"], scene["fps"], events)
	out = []
	for (view, clip), (frames, fps, events) in by_view.items():
		if view != "TP":
			continue
		fp = by_view.get(("FP", clip))
		if fp and (fp[0], sorted(fp[2])) != (frames, sorted(events)):
			print(f"warning: {os.path.basename(path)} {clip}: FP differs from TP {fp[0]}/{frames} frames, events {fp[2]} / {events}", file=sys.stderr)
		out.append((clip, frames, fps, events))
	return out


def table(weapon, seen=None):
	"""(title, clips, fallback_of) of a weapon: its manifest, or the table it falls back on."""
	seen = seen or set()
	path = manifest_path(weapon)
	if path:
		manifest = json.load(open(path, encoding="utf-8"))
		where = os.path.relpath(path, ROOT).replace(os.sep, "/")
		return f"{manifest['format']} v{manifest['version']}: {len({s['clip'] for s in manifest['scenes']})} gestures ({where})", clips(path), None
	fallback = WEAPONS[weapon].get("fallback")
	if fallback == "GrenadePlaceholder":
		return "PLACEHOLDER until build/packs/bean-grenade is built: 13 gestures at 30 fps", list(GRENADE_PLACEHOLDER), "placeholder"
	if fallback and fallback not in seen:
		title, rows, _ = table(fallback, seen | {weapon})
		return f"FALLBACK ({WEAPONS[weapon]['pack']} not built): the {fallback} table, {title}", rows, fallback
	raise SystemExit(f"{weapon}: no manifest ({WEAPONS[weapon]['pack']}) and no fallback")


def render():
	known = set(gesture_names())
	lines = [
		"// monke_island - GENERATED by scripts/character/pirate_weapon_clips.py from the manifests of the weapon packs",
		"// (build/packs/<pack>, OneDrive for the first pistol and shotgun). Do not edit by hand: rerun the script.",
		"// Included by the rules classes only (PistolRules.cpp, ShotgunRules.cpp, PirateMusketRules.cpp, BlunderbussRules.cpp,",
		"// RifleRules.cpp, GrenadeRules.cpp). A FALLBACK / PLACEHOLDER table stands for a pack not built yet.",
		"",
		"#pragma once",
		"",
		"namespace PirateWeaponClips",
		"{",
	]
	status = {}
	for weapon in WEAPONS:
		title, rows, fallback = table(weapon)
		status[weapon] = fallback or "ok"
		lines.append(f"\t/** {title} */")
		lines.append(f"\tinline void Add{weapon}Clips(TArray<FFirearmClip>& Out)")
		lines.append("\t{")
		names = {GESTURE.get(clip, clip) for clip, _, _, _ in rows}
		for clip, frames, fps, events in rows:
			gesture = GESTURE.get(clip, clip)
			if gesture not in known:
				print(f"warning: {weapon}: clip {clip} is not an EFirearmGesture, skipped", file=sys.stderr)
				continue
			loop = "true" if clip in LOOPS else "false"
			ev = ", ".join(f"{{ TEXT(\"{name}\"), {frame} }}" for frame, name in events)
			targets = [gesture]
			# a pack with one shot clip plays it aimed too (the musket family)
			if gesture == "Fire" and "AimFire" not in names:
				targets.append("AimFire")
			for target in targets:
				lines.append(f"\t\tOut.Add(FFirearmClip::Make(EFirearmGesture::{target}, {frames}, {fps}, {loop}, {{ {ev} }}));"
					if ev else f"\t\tOut.Add(FFirearmClip::Make(EFirearmGesture::{target}, {frames}, {fps}, {loop}, {{}}));")
		lines.append("\t}")
		lines.append("")
	lines[-1] = "}"
	return "\n".join(lines).replace("\n", "\r\n") + "\r\n", status


if __name__ == "__main__":
	text, status = render()
	for weapon, state in status.items():
		print(f"{weapon}: {state if state == 'ok' else 'FALLBACK -> ' + state}")
	if "--check" in sys.argv:
		current = open(OUT, encoding="utf-8", newline="").read() if os.path.isfile(OUT) else ""
		print("up to date" if current == text else "STALE: rerun without --check")
		sys.exit(0 if current == text else 1)
	with open(OUT, "w", encoding="utf-8", newline="") as f:
		f.write(text)
	print(f"wrote {os.path.normpath(OUT)}")
