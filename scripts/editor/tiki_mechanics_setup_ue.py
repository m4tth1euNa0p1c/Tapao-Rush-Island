"""T-275 (lot L5): the tactics of the Guerrier Coco, the Gardien Crabe and the Veilleur des Cendres in DA_TikiRules.

Editor open, no game running, lock held by the lead: action "mobsmech" of scripts/editor/lead_session_t186.py. Safe to
replay: an unchanged sound is not brought in again (sha256 kept in its metadata), every value is written to its target.
Needs the editor built with the T-275 C++ (UTikiRules::Pack / Stalker / Bulwark / Watcher): without it the sounds and the
material are still made, the rules are left untouched and the action fails with a clear message: run it again after
the build. Nothing is changed on the Blueprints of the mobs (BP_Tiki*: their role comes from their class name).

1. art/audio/creatures/tiki_tactics/*.wav (manifest.json, scripts/audio/tiki_tactics_sounds.py) ->
   /Game/MonkeIsland/Characters/Tiki/Audio/Tactics/SW_Tiki_*, with two attenuations (near: 4 m clear, gone at 40 m;
   far for the howl: 15 m clear, gone at 100 m), a concurrency for the stalk breaths (2), the Effects sound class; the
   veil (the ringing of a blinded player) stays 2D.
2. M_Tiki_AshCloud (/Game/MonkeIsland/Characters/Tiki/Tactics): translucent unlit two-sided ash, its edge softer
   (fresnel), for the cloud of the Veilleur. Made once.
3. DA_TikiRules: per role, the clips of the mob anim library (A_<Mob>_Strafe_L/R, _Walk_Back, _Block, the signature
   blow, the gesture) with their no-slide speeds and the rate of the signature clip that brings its hit to the end of
   the preparation of the rules (from art/export/Characters/MobAnimManifest.json); the sounds of each target; the
   attenuations; the look of the ash (engine sphere and cylinder, the flame of the musket, the smoke and the sparks of
   the shotgun); the sparks of the crab's guard.
Report: monke_island/Saved/T275/tiki-mechanics-setup.json
"""
import datetime as dt
import hashlib
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
SOUNDS = ROOT / "art/audio/creatures/tiki_tactics"
ANIM_MANIFEST = ROOT / "art/export/Characters/MobAnimManifest.json"
REPORT = ROOT / "monke_island/Saved/T275/tiki-mechanics-setup.json"
RULES = "/Game/MonkeIsland/Characters/Data/DA_TikiRules"
AUDIO_DIR = "/Game/MonkeIsland/Characters/Tiki/Audio/Tactics"
LOOK_DIR = "/Game/MonkeIsland/Characters/Tiki/Tactics"
CLOUD_MATERIAL = LOOK_DIR + "/M_Tiki_AshCloud"
SOUND_CLASS = "/Game/MonkeIsland/Settings/SoundClasses/SC_Effects"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
FLAME = "/Game/MonkeIsland/VFX/Weapons/M_Firearm_Flame"
SMOKE_FX = "/Game/MonkeIsland/VFX/Weapons/NS_Shotgun_MuzzleSmoke"
SPARKS_FX = "/Game/MonkeIsland/VFX/Weapons/NS_Shotgun_Sparks"
SHA_TAG = "monke_sha256"

# role (python name of the UTikiRules struct) -> mob of the anim library, signature clip, gesture clip, the windup of the
# rules the signature hit must land at the end of
ROLES = {
	"stalker": {"mob": "CocoWarrior", "signature": "Attack_Lunge", "gesture": "Taunt", "windup": "heavy_windup", "block": False},
	"bulwark": {"mob": "CrabGuardian", "signature": "Attack_Sweep", "gesture": "Taunt", "windup": "grab_windup", "block": True},
	"watcher": {"mob": "AshWatcher", "signature": "Attack_Overhead", "gesture": "Taunt", "windup": "cast_windup", "block": False},
}

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()


def has_property(obj, name):
	try:
		obj.get_editor_property(name)
		return True
	except Exception:
		return False


def load(path):
	return eal.load_asset(path) if eal.does_asset_exist(path) else None


def load_or_create(path, cls, factory=None):
	asset = load(path)
	if asset:
		return asset, False
	folder, name = path.rsplit("/", 1)
	asset = tools.create_asset(name, folder, cls, factory)
	if not asset:
		raise RuntimeError(f"could not create {path}")
	return asset, True


def attenuation(name, inner, falloff):
	asset, _ = load_or_create(f"{AUDIO_DIR}/{name}", unreal.SoundAttenuation)
	settings = asset.get_editor_property("attenuation")
	settings.set_editor_property("attenuation_shape", unreal.AttenuationShape.SPHERE)
	settings.set_editor_property("attenuation_shape_extents", unreal.Vector(inner, 0.0, 0.0))
	settings.set_editor_property("falloff_distance", falloff)
	asset.set_editor_property("attenuation", settings)
	eal.save_loaded_asset(asset, False)
	return asset


def concurrency(name, count):
	asset, _ = load_or_create(f"{AUDIO_DIR}/{name}", unreal.SoundConcurrency)
	settings = asset.get_editor_property("concurrency")
	settings.set_editor_property("max_count", count)
	settings.set_editor_property("resolution_rule", unreal.MaxConcurrentResolutionRule.STOP_OLDEST)
	asset.set_editor_property("concurrency", settings)
	eal.save_loaded_asset(asset, False)
	return asset


def load_sounds(report):
	"""The WAVs of the manifest as sound waves, by target path in DA_TikiRules; the attenuations"""
	manifest = json.loads((SOUNDS / "manifest.json").read_text(encoding="utf-8"))
	near = attenuation("ATT_Tiki_Tactics_Near", 400.0, 3600.0)
	far = attenuation("ATT_Tiki_Tactics_Far", 1500.0, 8500.0)
	stalk = concurrency("CON_Tiki_Stalk", 2)
	sound_class = load(SOUND_CLASS)
	far_families = set(manifest.get("far", []))
	two_d = set(manifest.get("two_d", []))
	by_target = {}
	for entry in manifest["sounds"]:
		path = ROOT / entry["file"]
		sha = hashlib.sha256(path.read_bytes()).hexdigest()
		if sha != entry["sha256"]:
			raise RuntimeError(f"{path} differs from its manifest: run scripts/audio/tiki_tactics_sounds.py again")
		target = f"{AUDIO_DIR}/{entry['id']}"
		sound = load(target)
		if sound and eal.get_metadata_tag(sound, SHA_TAG) == sha:
			report["sounds_unchanged"] += 1
		else:
			task = unreal.AssetImportTask()
			task.filename = str(path)
			task.destination_path = AUDIO_DIR
			task.destination_name = entry["id"]
			task.replace_existing = True
			task.automated = True
			task.save = False
			tools.import_asset_tasks([task])
			sound = load(target)
			if not sound:
				raise RuntimeError(f"the sound wave {entry['id']} could not be made from {path}")
			eal.set_metadata_tag(sound, SHA_TAG, sha)
			report["sounds_new"].append(entry["id"])
		family = entry["family"]
		sound.set_editor_property("attenuation_settings", None if family in two_d else (far if family in far_families else near))
		if family == "Coco_Stalk":
			sound.set_editor_property("concurrency_set", {stalk})
		if sound_class:
			sound.set_editor_property("sound_class_object", sound_class)
		eal.save_loaded_asset(sound, False)
		by_target.setdefault(entry["target"], []).append(sound)
	return by_target, near, far


def make_cloud_material(report):
	"""Translucent unlit two-sided ash, the edge of the sphere softer than its middle"""
	existing = load(CLOUD_MATERIAL)
	if existing:
		report["cloud_material"] = "kept"
		return existing
	try:
		material, _ = load_or_create(CLOUD_MATERIAL, unreal.Material, unreal.MaterialFactoryNew())
		material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
		material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
		material.set_editor_property("two_sided", True)
		lib = unreal.MaterialEditingLibrary
		colour = lib.create_material_expression(material, unreal.MaterialExpressionConstant3Vector, -500, -100)
		colour.set_editor_property("constant", unreal.LinearColor(0.045, 0.04, 0.036, 1.0))
		lib.connect_material_property(colour, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
		fresnel = lib.create_material_expression(material, unreal.MaterialExpressionFresnel, -700, 150)
		fresnel.set_editor_property("exponent", 1.6)
		edge = lib.create_material_expression(material, unreal.MaterialExpressionOneMinus, -500, 150)
		lib.connect_material_expressions(fresnel, "", edge, "")
		opacity = lib.create_material_expression(material, unreal.MaterialExpressionMultiply, -300, 150)
		opacity.set_editor_property("const_b", 0.82)
		lib.connect_material_expressions(edge, "", opacity, "A")
		lib.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
		lib.recompile_material(material)
		eal.save_loaded_asset(material, False)
		report["cloud_material"] = "made"
		return material
	except Exception as error:
		# the cloud then shows its smoke only
		report["cloud_material"] = f"not made: {error!r}"
		unreal.log_warning(f"tiki_mechanics_setup: {report['cloud_material']}")
		return None


def anim_info():
	"""Per mob: the no-slide speeds of the side steps and the walk backward, the first hit of each clip (s)"""
	manifest = json.loads(ANIM_MANIFEST.read_text(encoding="utf-8"))
	info = {}
	for character in manifest["characters"]:
		mob = character["name"]
		speeds, hits = {}, {}
		for anim in character.get("anims", []):
			asset = anim["asset"]
			if anim.get("no_slide_speed_cm_s"):
				speeds[asset] = float(anim["no_slide_speed_cm_s"])
			for notify in anim.get("notifies", []):
				if notify["name"] == "AN_HitWindow_Begin":
					hits.setdefault(asset, float(notify["time_s"]))
		info[mob] = {"speeds": speeds, "hits": hits}
	return info


def clip(mob, name, report):
	path = f"/Game/MonkeIsland/Characters/{mob}/Anims/A_{mob}_{name}"
	asset = load(path)
	if not asset:
		report["clips_missing"].append(path)
	return asset


def set_in(obj, name, value, report, label):
	"""Writes a property of a struct copy; a property the build does not know yet is listed as pending"""
	if not has_property(obj, name):
		report["pending"].append(label)
		return False
	try:
		obj.set_editor_property(name, value)
	except Exception as error:
		report["failed"].append(f"{label}: {error!r}")
		return False
	report["set"][label] = value.get_path_name() if hasattr(value, "get_path_name") else (
		[v.get_name() for v in value] if isinstance(value, list) else value)
	return True


def tune_role(rules, role, spec, by_target, near, far, info, report):
	block = rules.get_editor_property(role)
	mob = spec["mob"]
	mob_info = info.get(mob, {"speeds": {}, "hits": {}})

	# the clips of the mob anim library, never a clip of another skeleton (the C++ checks it again)
	clips = block.get_editor_property("clips")
	for prop, name in (("strafe_left", "Strafe_L"), ("strafe_right", "Strafe_R"), ("walk_back", "Walk_Back"),
					   ("signature", spec["signature"]), ("gesture", spec["gesture"])):
		asset = clip(mob, name, report)
		if asset:
			set_in(clips, prop, asset, report, f"{role}.clips.{prop}")
	if spec["block"]:
		asset = clip(mob, "Block", report)
		if asset:
			set_in(clips, "block", asset, report, f"{role}.clips.block")
	strafe = mob_info["speeds"].get(f"A_{mob}_Strafe_L")
	back = mob_info["speeds"].get(f"A_{mob}_Walk_Back")
	if strafe:
		set_in(clips, "strafe_clip_speed", strafe, report, f"{role}.clips.strafe_clip_speed")
	if back:
		set_in(clips, "walk_back_clip_speed", back, report, f"{role}.clips.walk_back_clip_speed")
	# the signature clip slowed so that its hit lands at the end of the preparation of the rules
	hit = mob_info["hits"].get(f"A_{mob}_{spec['signature']}")
	windup = block.get_editor_property(spec["windup"]) if has_property(block, spec["windup"]) else None
	if hit and windup:
		rate = max(0.3, min(1.5, hit / float(windup)))
		set_in(clips, "signature_rate", round(rate, 3), report, f"{role}.clips.signature_rate")
	block.set_editor_property("clips", clips)

	# the sounds of the role, and how far they carry
	sounds = block.get_editor_property("sounds")
	prefix = f"{role}.sounds."
	for target, waves in by_target.items():
		if target.startswith(prefix):
			set_in(sounds, target[len(prefix):], waves, report, target)
	set_in(sounds, "attenuation", near, report, f"{role}.sounds.attenuation")
	set_in(sounds, "far_attenuation", far, report, f"{role}.sounds.far_attenuation")
	block.set_editor_property("sounds", sounds)
	return block


def tune_ash(watcher, rules, by_target, near, cloud_material, report):
	ash = watcher.get_editor_property("ash")
	sphere, cylinder = load(SPHERE), load(CYLINDER)
	# the flame of the musket (it draws on a sphere already), else the flame of the firearms
	musket = rules.get_editor_property("musket")
	flame = musket.get_editor_property("flash_material") or load(FLAME)
	smoke, sparks = load(SMOKE_FX), load(SPARKS_FX)
	for prop, value in (("lump_mesh", sphere), ("lump_material", flame), ("cloud_mesh", sphere if cloud_material else None),
						("cloud_material", cloud_material), ("cloud_effect", smoke), ("ember_mesh", cylinder),
						("ember_material", flame), ("ember_effect", sparks), ("attenuation", near)):
		if value is not None:
			set_in(ash, prop, value, report, f"watcher.ash.{prop}")
		else:
			report["look_missing"].append(f"watcher.ash.{prop}")
	prefix = "watcher.ash."
	for target, waves in by_target.items():
		if target.startswith(prefix):
			set_in(ash, target[len(prefix):], waves, report, target)
	watcher.set_editor_property("ash", ash)
	return watcher


def tune_rules(by_target, near, far, cloud_material, report):
	rules = load(RULES)
	if not rules:
		raise RuntimeError(f"{RULES} missing")
	if not has_property(rules, "pack"):
		return False
	info = anim_info()
	for role, spec in ROLES.items():
		block = tune_role(rules, role, spec, by_target, near, far, info, report)
		if role == "watcher":
			block = tune_ash(block, rules, by_target, near, cloud_material, report)
		if role == "bulwark":
			sparks = load(SPARKS_FX)
			if sparks:
				set_in(block, "block_system", sparks, report, "bulwark.block_system")
		rules.set_editor_property(role, block)
	eal.save_loaded_asset(rules, False)
	return True


def main():
	if unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).is_in_play_in_editor():
		raise RuntimeError("a game is running: nothing done")
	report = {"started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "sounds_new": [], "sounds_unchanged": 0, "set": {},
			  "pending": [], "failed": [], "clips_missing": [], "look_missing": []}
	tuned = False
	try:
		by_target, near, far = load_sounds(report)
		cloud_material = make_cloud_material(report)
		tuned = tune_rules(by_target, near, far, cloud_material, report)
		report["rules"] = "tuned" if tuned else "waiting for the build (UTikiRules has no Pack yet)"
		report["passed"] = tuned and not report["failed"]
	except Exception as error:
		report["passed"] = False
		report["error"] = repr(error)
		raise
	finally:
		report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
		REPORT.parent.mkdir(parents=True, exist_ok=True)
		REPORT.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
		unreal.log(f"tiki_mechanics_setup: {len(report['sounds_new'])} new sounds, {report['sounds_unchanged']} unchanged, "
				   f"{len(report['set'])} values set, pending: {sorted(set(report['pending']))}, failed: {report['failed']}, "
				   f"clips missing: {report['clips_missing']}")
	if not tuned:
		raise RuntimeError("DA_TikiRules is not tuned yet: build the editor with the T-275 code, then run mobsmech again "
						   "(sounds and cloud material are ready)")
	if report["failed"]:
		raise RuntimeError(f"some values could not be written: {report['failed']}")


if __name__ == "__main__":
	main()
