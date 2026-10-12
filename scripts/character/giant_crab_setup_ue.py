"""Giant crab mini-boss "Tupa Nui" (T-211): DA_GiantCrabRules from the kit's manifest, BP_GiantCrab (AGiantCrab +
SK_GiantCrab), and one crab in the red circle (AArenaZone) of L_Test_Flat.

Source of the values (read only): <MAQUETTES>/crabe-geant/ue/GiantCrabManifest.json (clips, notifies at 30 fps, gameplay
proposals: damage, reaches, multipliers, 1 500 health). Reaches of the rules are measured between the edges of the
bodies (the kit's are from the crab's centre: minus its 95 cm and the bean's 30 cm).
Editor open with L_Test_Flat, no game running: scripts/editor/lead_session_t186.py, action "crabsetup". Replays update
the assets in place and put back a single crab in the arena. Report: Saved/MonkeTools/giant_crab_setup.json.
"""
import json
import os

import unreal

MAQUETTE = r"C:\Users\matth\OneDrive\maquette-blender\maquettes\crabe-geant"
ROOT = "/Game/MonkeIsland/Characters/GiantCrab"
ANIMS = ROOT + "/Anims"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
CRAB_RADIUS, BEAN_RADIUS = 95.0, 30.0

eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
with open(os.path.join(MAQUETTE, "ue", "GiantCrabManifest.json"), encoding="utf-8") as f:
	MAN = json.load(f)


def clip(name):
	asset = eal.load_asset(f"{ANIMS}/A_GiantCrab_{name}")
	assert asset, f"A_GiantCrab_{name} missing: run the action crabimport first"
	return asset


def notify(clip_name, notify_name, default):
	for entry in MAN["clips"]:
		if entry["name"] == clip_name:
			for event in entry.get("notifies", []):
				if event["name"] == notify_name:
					return float(event["time_s"])
	return default


def frames(a, b):
	return a / MAN["fps"], b / MAN["fps"]


def blow(clip_name, damage, centre_reach_m, cone, hit, lunge_cm, lunge, knock, up, weight, recovery, shock=(0.0, 0.0)):
	rules = unreal.GiantCrabAttackRules()
	values = {"clip": clip(clip_name), "damage": damage, "reach_cm": centre_reach_m * 100.0 - CRAB_RADIUS - BEAN_RADIUS,
		"cone_degrees": cone, "hit_start": hit[0], "hit_end": hit[1], "lunge_cm": lunge_cm, "lunge_start": lunge[0],
		"lunge_end": lunge[1], "knockback_speed": knock, "knockback_up": up, "weight": weight, "recovery": recovery,
		"shockwave_radius_cm": shock[0], "shockwave_damage": shock[1]}
	for key, value in values.items():
		rules.set_editor_property(key, value)
	return rules


def weak(socket, bones, radius, scale, shot_flips):
	point = unreal.GiantCrabWeakPoint()
	point.set_editor_property("socket", socket)
	point.set_editor_property("bones", bones)
	point.set_editor_property("radius_cm", radius)
	point.set_editor_property("damage_scale", scale)
	point.set_editor_property("shot_flips", shot_flips)
	return point


def data_asset(path, cls):
	if eal.does_asset_exist(path):
		return eal.load_asset(path)
	factory = unreal.DataAssetFactory()
	factory.set_editor_property("data_asset_class", cls)
	return tools.create_asset(os.path.basename(path), os.path.dirname(path), cls, factory)


def main():
	report = {}
	# the masters of the crab's instances (chitin: vertex colour; eyes, membranes, gold, pearls: the bean's) on a skeletal
	# mesh with a morph target (DeadEyes): their usage flags, saved (else the editor sets them unsaved and asks at exit)
	for path in ("/Game/MonkeIsland/Materials/M_Vegetation_VertexColor", "/Game/MonkeIsland/Characters/Bean/Materials/M_Bean_Master"):
		master = eal.load_asset(path)
		flags = [flag for flag in ("used_with_skeletal_mesh", "used_with_morph_targets") if master and not master.get_editor_property(flag)]
		for flag in flags:
			master.set_editor_property(flag, True)
		if flags:
			eal.save_loaded_asset(master)
			report.setdefault("usage_flags", {})[path] = flags

	game = MAN["gameplay"]
	damage = game["damage"]
	stats = game["stats"]
	rules = data_asset(f"{ROOT}/DA_GiantCrabRules", unreal.GiantCrabRules)
	swipe = blow("Attack_ClawSwipe", damage["Attack_ClawSwipe"]["damage"], damage["Attack_ClawSwipe"]["reach_m"], 70.0,
		(notify("Attack_ClawSwipe", "AN_HitWindow_Begin", 0.533), notify("Attack_ClawSwipe", "AN_HitWindow_End", 0.833)),
		25.0, frames(10, 24), 650.0, 250.0, 3.0, 1.0)
	slam = blow("Attack_ClawSlam", damage["Attack_ClawSlam"]["damage"], 2.8, 45.0,
		(notify("Attack_ClawSlam", "AN_HitWindow_Begin", 0.933), notify("Attack_ClawSlam", "AN_HitWindow_End", 1.067)),
		40.0, frames(22, 32), 300.0, 450.0, 2.0, 1.6,
		(damage["Attack_ClawSlam"]["shockwave_radius_m"] * 100.0, damage["Attack_ClawSlam"]["shockwave_damage"]))
	grab = blow("Attack_Grab", damage["Attack_Grab"]["damage"], 2.4, 35.0,
		(notify("Attack_Grab", "AN_Grab_Check", 0.667), notify("Attack_Grab", "AN_Grab_Attach", 0.7) + 0.05),
		60.0, frames(8, 20), 0.0, 0.0, 1.0, 1.4)
	values = {
		"max_health": float(stats.get("hp_proposal", 1500)), "incoming_damage_cap": 150.0,
		"shell_damage_scale": float(stats["multipliers"]["carapace"]), "leg_damage_scale": float(stats["multipliers"]["legs"]),
		"side_speed": float(stats["sideways_speed_cm_s"]), "forward_speed": float(stats["forward_speed_cm_s"]),
		"swipe": swipe, "slam": slam, "grab": grab,
		"throw_clip": clip("Throw"), "throw_release": notify("Throw", "AN_Throw_Release", 0.867),
		# 4.5 m on flat ground at 35 deg: v = sqrt(g d / sin 70)
		"throw_speed": (980.0 * damage["Throw"]["distance_m"] * 100.0 / 0.9397) ** 0.5, "throw_angle_degrees": 35.0,
		"throw_damage": float(damage["Throw"]["damage"]),
		"slam_stuck_start": notify("Attack_ClawSlam", "AN_ClawStuck_Begin", 1.033),
		"slam_stuck_end": notify("Attack_ClawSlam", "AN_ClawStuck_End", 1.467),
		"blocked_damage_scale": 1.0 - float(stats["block_frontal_reduction"]), "flipped_seconds": float(stats["flipped_duration_s"]),
		"idle": clip("Idle"), "walk_side_left": clip("Walk_Sideways_L"), "walk_side_right": clip("Walk_Sideways_R"),
		"walk_forward": clip("Walk_Forward"), "taunt": clip("Taunt_ClawClack"), "block_start": clip("Block_Start"),
		"block_loop": clip("Block_Loop"), "block_end": clip("Block_End"), "hit_react": clip("Hit_React"),
		"stagger_flip": clip("Stagger_Flip"), "flipped_loop": clip("Flipped_Loop"), "recover": clip("Recover"),
		"death": clip("Death"), "death_flipped": clip("Death_Flipped"),
		"weak_points": [
			weak("WeakPoint_Eye_L", ["eye_l"], 18.0, float(stats["multipliers"]["eye"]), True),
			weak("WeakPoint_Eye_R", ["eye_r"], 18.0, float(stats["multipliers"]["eye"]), True),
			weak("WeakPoint_ClawJoint_L", ["claw_l_forearm"], 30.0, float(stats["multipliers"]["claw_joint"]), False),
			weak("WeakPoint_ClawJoint_R", ["claw_r_forearm"], 30.0, float(stats["multipliers"]["claw_joint"]), False),
			weak("WeakPoint_Belly", ["abdomen"], 55.0, float(stats["multipliers"]["belly"]), True),
			weak("WeakPoint_Mouth", ["mouth"], 22.0, float(stats["multipliers"]["mouth"]), False),
		],
	}
	for key, value in values.items():
		rules.set_editor_property(key, value)
	eal.save_loaded_asset(rules)
	report["rules"] = rules.get_path_name()
	report["reach_cm"] = {k: round(v.get_editor_property("reach_cm")) for k, v in (("swipe", swipe), ("slam", slam), ("grab", grab))}

	# the Blueprint: mesh of the kit, rules
	bp_path = f"{ROOT}/BP_GiantCrab"
	if eal.does_asset_exist(bp_path):
		bp = eal.load_asset(bp_path)
	else:
		factory = unreal.BlueprintFactory()
		factory.set_editor_property("parent_class", unreal.GiantCrab)
		bp = tools.create_asset("BP_GiantCrab", ROOT, unreal.Blueprint, factory)
	unreal.BlueprintEditorLibrary.compile_blueprint(bp)
	cdo = unreal.get_default_object(bp.generated_class())
	cdo.set_editor_property("rules", rules)
	mesh = eal.load_asset(f"{ROOT}/Meshes/SK_GiantCrab")
	cdo.get_editor_property("mesh").set_skeletal_mesh_asset(mesh)
	unreal.BlueprintEditorLibrary.compile_blueprint(bp)
	eal.save_loaded_asset(bp)
	report["blueprint"] = bp.get_path_name()

	# one crab in the middle of the arena of the test map
	actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
	world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
	assert world.get_path_name().split(".")[0] == LEVEL, f"open {LEVEL} first ({world.get_path_name()})"
	zones = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.ArenaZone)]
	assert zones, "no AArenaZone in L_Test_Flat"
	zone = zones[0]
	for old in [a for a in actors.get_all_level_actors() if isinstance(a, unreal.GiantCrab)]:
		actors.destroy_actor(old)
	centre = zone.get_actor_location()
	hit = unreal.SystemLibrary.line_trace_single(world, centre + unreal.Vector(0, 0, 2000), centre - unreal.Vector(0, 0, 2000),
		unreal.TraceTypeQuery.TRACE_TYPE_QUERY1, False, [], unreal.DrawDebugTrace.NONE, True)
	ground = hit.to_tuple()[4].z if hit and hit.to_tuple()[0] else centre.z
	crab = actors.spawn_actor_from_class(bp.generated_class(), unreal.Vector(centre.x, centre.y, ground + 105.0), unreal.Rotator(0, 0, 180))
	crab.set_actor_label("GiantCrab_TupaNui")
	crab.set_folder_path("Arena")
	unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).save_current_level()
	report["crab"] = {"label": crab.get_actor_label(), "location": [round(v) for v in (crab.get_actor_location().x, crab.get_actor_location().y,
		crab.get_actor_location().z)], "arena": zone.get_actor_label(), "radius": zone.get_editor_property("radius")}
	out = os.path.join(unreal.Paths.project_saved_dir(), "MonkeTools", "giant_crab_setup.json")
	os.makedirs(os.path.dirname(out), exist_ok=True)
	with open(out, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=1, default=str)
	unreal.log(f"[giant_crab] {json.dumps(report, default=str)}")


if __name__ == "__main__":
	main()
