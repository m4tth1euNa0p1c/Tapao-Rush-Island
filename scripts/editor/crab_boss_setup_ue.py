"""Giant crab boss v2 (T-261): imports the sounds of scripts/audio/crab_sounds.py and tunes DA_GiantCrabRules.

Editor open, no game running, lock held by the lead: action "crabboss" of scripts/editor/lead_session_t186.py. Safe to
replay: an unchanged sound is not imported again (sha256 kept in its metadata), every value is written to its target.
Properties the editor does not know yet (the C++ of T-261 before the lead's full build) are skipped and listed as
"pending" in the report: run the action again after the build.

1. art/audio/creatures/giant_crab/*.wav (manifest.json) -> /Game/MonkeIsland/Characters/GiantCrab/Audio/SW_GiantCrab_*,
   with two attenuations (near: steps, clacks, hits, hisses; far: roars, slams, charge, death), a concurrency for the
   steps (6, stop the oldest) and one for the voice (3), the Effects sound class when it exists.
2. DA_GiantCrabRules: the boss values (health 6000, quicker), the sounds of every cue, the older single sounds when
   empty, the sand of the impacts (NS_Firearm_Impact), the eye stalks in the eye weak points, and the clips of T-260
   (A_GiantCrab_BellySlam, _Roar, _LegStomp, _Charge, _Charge_R) once they exist anywhere under
   /Game/MonkeIsland/Characters/GiantCrab.
Report: monke_island/Saved/T261/crab_boss_setup.json.
"""
import datetime as dt
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
SOUNDS = ROOT / "art/audio/creatures/giant_crab"
CRAB = "/Game/MonkeIsland/Characters/GiantCrab"
DEST = CRAB + "/Audio"
RULES = CRAB + "/DA_GiantCrabRules"
DUST = "/Game/MonkeIsland/VFX/Weapons/NS_Firearm_Impact"
SOUND_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
REPORT = ROOT / "monke_island/Saved/T261/crab_boss_setup.json"
SHA_TAG = "monke_sha256"

# families heard from far (the voice and the impacts) and the ones heard near
FAR = {"Roar", "Charge", "Death", "Slam", "Stomp", "BellySlam", "BodyFall", "Wave"}
VOICE = {"Roar", "Charge", "Death", "Hiss", "Growl"}
STEPS = {"Step", "StepHeavy"}

# boss values of DA_GiantCrabRules (T-261): quasi unbeatable, much quicker than the T-211 crab
BOSS_VALUES = {
	"max_health": 6000.0,
	"side_speed": 200.0,
	"forward_speed": 100.0,
	"scuttle_speed": 480.0,
	"turn_rate_degrees": 140.0,
}

# clips of T-260 (any folder under the crab) -> (struct property, field)
NEW_CLIPS = {
	"A_GiantCrab_BellySlam": ("belly_slam", "clip"),
	"A_GiantCrab_Roar": ("roar", "clip"),
	"A_GiantCrab_LegStomp": ("stomp", "clip"),
	"A_GiantCrab_Charge": ("charge", "clip"),
	"A_GiantCrab_Charge_R": ("charge", "clip_right"),
}

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def has_property(obj, name):
	try:
		obj.get_editor_property(name)
		return True
	except Exception:
		return False


def asset_path(obj):
	return obj.get_path_name() if obj else None


def load_or_create(path, cls):
	if eal.does_asset_exist(path):
		return eal.load_asset(path)
	folder, name = path.rsplit("/", 1)
	asset = tools.create_asset(name, folder, cls, None)
	if not asset:
		raise RuntimeError(f"could not create {path}")
	return asset


def attenuation(name, inner, falloff):
	asset = load_or_create(f"{DEST}/{name}", unreal.SoundAttenuation)
	settings = asset.get_editor_property("attenuation")
	settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
	settings.set_editor_property("attenuation_shape_extents", unreal.Vector(inner, 0.0, 0.0))
	settings.set_editor_property("falloff_distance", falloff)
	asset.set_editor_property("attenuation", settings)
	eal.save_loaded_asset(asset, False)
	return asset


def concurrency(name, count, rule):
	asset = load_or_create(f"{DEST}/{name}", unreal.SoundConcurrency)
	settings = asset.get_editor_property("concurrency")
	settings.set_editor_property("max_count", count)
	settings.set_editor_property("resolution_rule", rule)
	asset.set_editor_property("concurrency", settings)
	eal.save_loaded_asset(asset, False)
	return asset


def import_sounds(report):
	manifest = json.loads((SOUNDS / "manifest.json").read_text(encoding="utf-8"))
	near = attenuation("ATT_GiantCrab_Near", 300.0, 3500.0)
	far = attenuation("ATT_GiantCrab_Far", 800.0, 9000.0)
	steps = concurrency("CON_GiantCrab_Steps", 6, unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
	voice = concurrency("CON_GiantCrab_Voice", 3, unreal.MaxConcurrentResolutionRule.STOP_QUIETEST)
	sound_class = eal.load_asset(SOUND_CLASS) if eal.does_asset_exist(SOUND_CLASS) else None
	by_cue = {}
	for entry in manifest["sounds"]:
		path = ROOT / entry["file"]
		sha = hashlib.sha256(path.read_bytes()).hexdigest()
		if sha != entry["sha256"]:
			raise RuntimeError(f"{path} differs from its manifest: run scripts/audio/crab_sounds.py again")
		target = f"{DEST}/{entry['id']}"
		sound = eal.load_asset(target) if eal.does_asset_exist(target) else None
		if sound and eal.get_metadata_tag(sound, SHA_TAG) == sha:
			report["sounds_unchanged"] += 1
		else:
			task = unreal.AssetImportTask()
			task.filename = str(path)
			task.destination_path = DEST
			task.destination_name = entry["id"]
			task.replace_existing = True
			task.automated = True
			task.save = False
			tools.import_asset_tasks([task])
			sound = eal.load_asset(target)
			if not sound:
				raise RuntimeError(f"import of {path} failed")
			eal.set_metadata_tag(sound, SHA_TAG, sha)
			report["sounds_imported"].append(entry["id"])
		family = entry["family"]
		sound.set_editor_property("attenuation_settings", far if family in FAR else near)
		if family in STEPS:
			sound.set_editor_property("concurrency_set", {steps})
		elif family in VOICE:
			sound.set_editor_property("concurrency_set", {voice})
		if sound_class:
			sound.set_editor_property("sound_class_object", sound_class)
		eal.save_loaded_asset(sound, False)
		by_cue.setdefault(entry["cue"], []).append(sound)
	return by_cue


def find_new_clips():
	found = {}
	for path in eal.list_assets(CRAB, recursive=True, include_folder=False):
		name = path.rsplit("/", 1)[-1].split(".")[0]
		if name in NEW_CLIPS and name not in found:
			found[name] = eal.load_asset(path)
	return found


def tune_rules(by_cue, report):
	rules = eal.load_asset(RULES)
	if not rules:
		raise RuntimeError(f"{RULES} missing")

	def put(name, value):
		if not has_property(rules, name):
			report["pending"].append(name)
			return False
		before = rules.get_editor_property(name)
		rules.set_editor_property(name, value)
		report["set"][name] = {"before": asset_path(before) if hasattr(before, "get_path_name") else str(before),
			"after": asset_path(value) if hasattr(value, "get_path_name") else str(value)}
		return True

	for name, value in BOSS_VALUES.items():
		put(name, value)

	# the sounds of every cue (properties of the T-261 C++)
	for cue, sounds in sorted(by_cue.items()):
		if not has_property(rules, cue):
			report["pending"].append(cue)
			continue
		rules_cue = rules.get_editor_property(cue)
		rules_cue.set_editor_property("sounds", sounds)
		rules.set_editor_property(cue, rules_cue)
		report["set"][cue] = [s.get_name() for s in sounds]

	# the older single sounds, left alone when another binding (expedition audio) set them first
	for prop, cue in (("hit_sound", "hit_cue"), ("clack_sound", "clack_cue"), ("slam_sound", "slam_cue")):
		if has_property(rules, prop) and rules.get_editor_property(prop) is None and by_cue.get(cue):
			put(prop, by_cue[cue][0])

	# the sand thrown up by the impacts
	if eal.does_asset_exist(DUST):
		put("dust_system", eal.load_asset(DUST))

	# the eye stalks count as the eyes
	if has_property(rules, "weak_points"):
		points = list(rules.get_editor_property("weak_points"))
		changed = False
		for point in points:
			socket = str(point.get_editor_property("socket"))
			if socket in ("WeakPoint_Eye_L", "WeakPoint_Eye_R"):
				stalk = "eye_stalk_l" if socket.endswith("_L") else "eye_stalk_r"
				bones = [str(b) for b in point.get_editor_property("bones")]
				if stalk not in bones:
					point.set_editor_property("bones", bones + [stalk])
					changed = True
		if changed:
			rules.set_editor_property("weak_points", points)
			report["set"]["weak_points"] = "eye stalks added to the eyes"

	# the clips of T-260, once imported
	clips = find_new_clips()
	for name, (prop, field) in NEW_CLIPS.items():
		clip = clips.get(name)
		if not clip:
			report["clips_missing"].append(name)
			continue
		if not has_property(rules, prop):
			report["pending"].append(prop)
			continue
		block = rules.get_editor_property(prop)
		block.set_editor_property(field, clip)
		rules.set_editor_property(prop, block)
		report["set"][f"{prop}.{field}"] = clip.get_path_name()

	eal.save_loaded_asset(rules, False)


def main():
	if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
		raise RuntimeError("a game is running: nothing done")
	report = {"started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "sounds_imported": [], "sounds_unchanged": 0,
		"set": {}, "pending": [], "clips_missing": []}
	try:
		by_cue = import_sounds(report)
		tune_rules(by_cue, report)
		report["passed"] = True
	except Exception as error:
		report["passed"] = False
		report["error"] = repr(error)
		raise
	finally:
		report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
		REPORT.parent.mkdir(parents=True, exist_ok=True)
		REPORT.write_text(json.dumps(report, indent=2), encoding="utf-8")
		unreal.log(f"crab_boss_setup: {len(report['sounds_imported'])} sounds imported, {report['sounds_unchanged']} unchanged, "
			f"{len(report['set'])} values set, pending (C++ build): {sorted(set(report['pending']))}, clips missing: {report['clips_missing']}")


if __name__ == "__main__":
	main()
