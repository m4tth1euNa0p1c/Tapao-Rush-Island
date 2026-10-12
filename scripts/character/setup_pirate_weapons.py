"""Builds the gameplay assets of the bean's weapons (pistol, double pistol, shotgun, musket, blunderbuss, rifle,
grenade; T-156, T-170, T-249) and of the ammunition (T-249) after import_pirate_weapons.py.

Creates or updates, in /Game/MonkeIsland/Weapons/<Weapon>/ (registry import_pirate_weapons.WEAPONS):
  DA_<Asset>Rules    the rules class of the weapon (UPistolRules, UDoublePistolRules, UShotgunRules, UPirateMusketRules,
                     UBlunderbussRules, URifleRules, UGrenadeRules): weapon mesh, FP arms, the four clips of every
                     gesture of the manifest (a gesture missing from the C++ defaults is added, its length and events
                     written from the manifest; Discard included when the pack has it), the FP camera of the manifest
                     converted to the bean's mesh space, the hip framing of fp_framing, the musket's shot sound and
                     flame, placeholder event sounds, the muzzle sockets, the spent shell (shotgun), the projectile,
                     its mesh and the explosion assets (grenade: BP_GrenadeProjectile, the musket's flame, the
                     shotgun's smoke and flame, the musket's shot sound)
  BP_<Asset>         child of the weapon class: rules and the musket's keys (IMC_Musket, IA_Musket*)
  DA_Item_<Asset>    UInventoryItemDefinition (name, BP class, pickup mesh), for a slot or a pickup (T-157)
In /Game/MonkeIsland/Weapons/Ammo/ (registry AMMO_DEFS: display name, lots, keywords of the SM_Ammo_* mesh):
  DA_Ammo_<Name>     UAmmoDefinition (lots of kind and count, pickup mesh)
  BP_Ammo_<Name>     child of AAmmoPickup with that definition (placed in a level, scattered as loot, spawned)
Then the entries (owner 2026-10-09): the new weapons and the ammunition in the merchant's DA_ShopCatalog (missing
lines appended), the ammunition crates in the French fort loot (DA_FortLoot, when it exists).
BP_Islander / BP_IslanderBean and the maps are NOT touched: scripts/editor/weapons_test_map_ue.py puts pickups on the
test map, the editor integrator adds items to StartingItems.

Run after the import, editor closed (commandlet) or open (Tools > Execute Python Script, no PIE running):
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      "C:/Users/matth/monk-island/monke_island/monke_island.uproject"
      -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/setup_pirate_weapons.py"
      -unattended -nosplash -nullrhi -nosourcecontrol
Filter: MONKE_PIRATE_WEAPONS=Pistol,DoublePistol,Shotgun,Musket,Blunderbuss,Rifle,Grenade,Ammo (default: the packs
present; a weapon whose pack is missing is skipped). MONKE_WEAPONS_NO_SHOP=1 / MONKE_WEAPONS_NO_LOOT=1 skip the
entries. Report: Saved/MonkeTools/pirate_weapons_setup.json.

Dry run outside Unreal (prints the FP views computed from the manifests and the plan):
  python scripts/character/setup_pirate_weapons.py --check
"""
import json
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import import_pirate_weapons as imp  # noqa: E402

MUSKET = "/Game/MonkeIsland/Weapons/Musket"
SPHERE = "/Engine/BasicShapes/Sphere"
VFX = "/Game/MonkeIsland/VFX/Weapons"
SHOP_CATALOG = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
FORT_LOOT = "/Game/MonkeIsland/Environment/FortFrench/Data/DA_FortLoot"
# key I of the firearms (in IMC_Musket, T-160)
INSPECT_ACTION = "/Game/MonkeIsland/Weapons/Input/IA_FirearmInspect"
# placeholder sounds taken from the musket (event of the manifest -> SW_Musket_*), until the weapons get their own
MUSKET_EVENT_SOUNDS = {"HalfCockStart": "SW_Musket_Cock", "PanPrime": "SW_Musket_Prime", "FrizzenClose": "SW_Musket_Frizzen",
	"PowderCharge": "SW_Musket_Powder", "BallAndPatch": "SW_Musket_Ball", "RamrodDraw": "SW_Musket_RamrodDraw",
	"RamrodStroke": "SW_Musket_RamStroke", "RamrodReturn": "SW_Musket_RamrodDraw", "FullCockEnd": "SW_Musket_Cock",
	"Trigger": "SW_Musket_Lock", "Empty": "SW_Musket_Lock"}
PISTOL_EVENT_SOUNDS = {"CockHammer": "SW_Musket_Cock", "HammerCocked": "SW_Musket_Cock", "DryClick": "SW_Musket_Lock",
	"PaperPickup": "SW_Musket_Paper", "RamrodGesture": "SW_Musket_RamStroke", "RamrodReturned": "SW_Musket_RamrodDraw", "Empty": "SW_Musket_Lock"}
EVENT_SOUNDS = {
	"Pistol": PISTOL_EVENT_SOUNDS,
	"DoublePistol": PISTOL_EVENT_SOUNDS,
	"Shotgun": {"PumpBackStart": "SW_Musket_Cock", "ChamberSlug": "SW_Musket_Frizzen", "InsertSlug": "SW_Musket_Ball",
		"DryClick": "SW_Musket_Lock", "Empty": "SW_Musket_Lock"},
	"Musket": MUSKET_EVENT_SOUNDS,
	"Blunderbuss": MUSKET_EVENT_SOUNDS,
	"Rifle": MUSKET_EVENT_SOUNDS,
	"Grenade": {"Light": "SW_Musket_Prime", "HammerCocked": "SW_Musket_Paper", "DryClick": "SW_Musket_Lock", "Empty": "SW_Musket_Lock"},
}
# shop lines of the weapons (price in crowns, line under the name)
SHOP_WEAPONS = {
	"DoublePistol": (400, "Pistolet double : deux coups, 16 charges"),
	"Blunderbuss": (550, "Tromblon : 10 charges de plombs, dévastateur à 4 m"),
	"Rifle": (750, "Carabine : 12 cartouches, précise à 160 m"),
	"Grenade": (350, "Sac de 3 grenades (6 au plus), mèche de 3 s"),
}
# ammunition: name -> (display name, lots [(EAmmoKind name, count)], keywords of its SM_Ammo_* mesh (longest match
# wins), shop price (None: not sold), fort loot weight (None: not scattered))
AMMO_DEFS = {
	"Cartridges": ("Cartouches de papier", [("MUSKET_BALL", 6)], ["papercartridge", "cartridges", "cartouches", "cartridge"], 40, None),
	"CartridgeBox": ("Giberne", [("MUSKET_BALL", 15)], ["cartridgebox", "cartridge_box", "giberne", "box"], 90, 0.7),
	"PowderHorn": ("Corne à poudre", [("PISTOL_CHARGE", 8)], ["powderhorn", "powder_horn", "horn", "corne"], 50, 0.5),
	"Pouch": ("Bourse de charges", [("PISTOL_CHARGE", 6)], ["pouch", "bourse", "purse", "bag_charges"], 40, None),
	"RifleCartridges": ("Cartouches de carabine", [("RIFLE_CARTRIDGE", 6)], ["riflecartridge", "rifle_cartridge", "rifle", "carabine"], 60, None),
	"ShellBox": ("Boîte de cartouches", [("SHELL", 8)], ["shellbox", "shell_box", "shells", "shell"], 70, 0.5),
	"ShotBag": ("Sachet de plombs", [("SHOT", 5)], ["shotbag", "shot_bag", "plombs", "shot"], 45, None),
	"GrenadeBag": ("Sac de grenades", [("GRENADE", 3)], ["grenadebag", "grenade_bag", "grenades", "grenade"], 150, None),
	"Crate": ("Caisse de munitions", [("MUSKET_BALL", 10), ("PISTOL_CHARGE", 10), ("SHELL", 6), ("RIFLE_CARTRIDGE", 4)],
		["crate", "caisse", "mixed", "chest"], None, 0.5),
	"Barrel": ("Baril de poudre", [("MUSKET_BALL", 15), ("PISTOL_CHARGE", 15), ("SHOT", 8), ("RIFLE_CARTRIDGE", 8)],
		["barrel", "baril", "keg"], None, 0.3),
}


def camel_to_upper(name):
	return re.sub(r"(?<!^)(?=[A-Z])", "_", name).upper()


def fp_view(weapon):
	"""The FP camera of the pack's scenes in the bean's mesh space (Unreal: cm, +Y forward, -X right).

	Blender: metres, the bean faces -Y, a camera looks along its local -Z with +Y up. The FBX import maps a Blender
	point (x, y, z) to Unreal (x, -y, z) * 100 (same mapping as the project's bean, see AFirearmWeapon)."""
	scenes = [s for s in imp.manifest(weapon)["scenes"] if s["view"] == "FP"]
	if not scenes:
		return None
	cam = scenes[0].get("camera_data") or {}
	if cam:
		loc, rot, lens = cam["location_m"], cam["rotation_euler_deg"], cam["lens_mm"]
	else:
		ct = scenes[0]["camera_transform"]
		loc, rot, lens = ct["location"], ct["rotation_degrees"], ct["lens_mm"]
	rx, ry, rz = (math.radians(a) for a in rot)
	cx, sx, cy, sy, cz, sz = math.cos(rx), math.sin(rx), math.cos(ry), math.sin(ry), math.cos(rz), math.sin(rz)
	mx = [[1, 0, 0], [0, cx, -sx], [0, sx, cx]]
	my = [[cy, 0, sy], [0, 1, 0], [-sy, 0, cy]]
	mz = [[cz, -sz, 0], [sz, cz, 0], [0, 0, 1]]

	def mul(a, b):
		return [[sum(a[i][k] * b[k][j] for k in range(3)) for j in range(3)] for i in range(3)]

	m = mul(mz, mul(my, mx))  # Blender XYZ euler
	forward = (-m[0][2], m[1][2], -m[2][2])  # -Z column, Y flipped
	up = (m[0][1], -m[1][1], m[2][1])  # +Y column, Y flipped
	pitch = math.degrees(math.asin(max(-1.0, min(1.0, forward[2]))))
	yaw = math.degrees(math.atan2(forward[1], forward[0]))
	# roll from the up vector against the roll-free up of that forward
	flat_right = (-math.sin(math.radians(yaw)), math.cos(math.radians(yaw)), 0.0)
	roll = math.degrees(math.asin(max(-1.0, min(1.0, -sum(u * r for u, r in zip(up, flat_right))))))
	fov = 2.0 * math.degrees(math.atan(18.0 / lens))  # 36 mm sensor, horizontal
	return {"location_cm": [loc[0] * 100.0, -loc[1] * 100.0, loc[2] * 100.0], "rotation_pyr": [pitch, yaw, roll], "fov": fov,
		"same_for_all_fp_scenes": len({json.dumps(s.get("camera_data", s.get("camera_transform")), sort_keys=True) for s in scenes}) == 1}


def timing(weapon):
	"""clip -> (frames, fps, [(frame, event)]) of the TP scenes of the manifest."""
	out = {}
	for scene in imp.manifest(weapon)["scenes"]:
		if scene["view"] == "TP":
			out[scene["clip"]] = (scene["end"] - scene["start"], scene["fps"], [(e["frame"] - scene["start"], e["name"]) for e in scene["events"]])
	return out


def sockets(weapon):
	"""Socket names of the weapon skeleton (bones socket_*) listed by the manifest's weapon contract."""
	contract = imp.manifest(weapon).get("weapon_contract") or {}
	names = set()
	for key in ("sockets", "socket_positions"):
		for socket in (contract.get(key) or {}):
			names.add(socket if str(socket).startswith("socket_") else f"socket_{socket}")
	for bone in contract.get("bones") or []:
		if str(bone).startswith("socket_"):
			names.add(str(bone))
	return sorted(names)


def check():
	plan = {"weapons": {}, "ammo": {}, "skipped": []}
	for name in imp.selected():
		if name != "Ammo" and imp.WEAPONS[name].get("own_setup") and not os.environ.get("MONKE_PIRATE_WEAPONS"):
			plan["skipped"].append(f"{name}: set up by {imp.WEAPONS[name]['own_setup']}")
			continue
		if not imp.pack_dir(name):
			plan["skipped"].append(name)
			continue
		if name == "Ammo":
			plan["ammo"] = {key: {"display": display, "lots": lots, "mesh_keywords": keywords, "price": price, "loot_weight": weight,
				"assets": [f"{imp.dest('Ammo')}/DA_Ammo_{key}", f"{imp.dest('Ammo')}/BP_Ammo_{key}"]}
				for key, (display, lots, keywords, price, weight) in AMMO_DEFS.items()}
			plan["ammo"]["meshes_in_pack"] = [s for s, _ in imp.statics("Ammo")]
			continue
		cfg = imp.WEAPONS[name]
		plan["weapons"][name] = {"fp_view": fp_view(name), "gestures": imp.gestures(name), "sockets": sockets(name),
			"fps": imp.fps_of(name), "rules_class": cfg["rules_cls"], "weapon_class": cfg["cls"],
			"assets": [f"{imp.dest(name)}/DA_{cfg['asset']}Rules", f"{imp.dest(name)}/BP_{cfg['asset']}", f"{imp.dest(name)}/DA_Item_{cfg['asset']}"]
				+ ([f"{imp.dest(name)}/BP_GrenadeProjectile"] if cfg.get("projectile") else []),
			"shop": SHOP_WEAPONS.get(name)}
	print(json.dumps(plan, indent=1, ensure_ascii=False))
	return 0


def run_in_unreal():
	import unreal

	eal = unreal.EditorAssetLibrary
	tools = unreal.AssetToolsHelpers.get_asset_tools()
	report = {"weapons": {}, "ammo": {}, "shop": [], "loot": [], "missing": [], "skipped": []}

	def load(path):
		asset = eal.load_asset(path) if eal.does_asset_exist(path) else None
		if not asset:
			report["missing"].append(path)
		return asset

	def data_asset(path, cls):
		if eal.does_asset_exist(path):
			return eal.load_asset(path)
		factory = unreal.DataAssetFactory()
		factory.set_editor_property("data_asset_class", cls)
		return tools.create_asset(os.path.basename(path), os.path.dirname(path), cls, factory)

	def blueprint(path, parent):
		if eal.does_asset_exist(path):
			return eal.load_asset(path)
		factory = unreal.BlueprintFactory()
		factory.set_editor_property("parent_class", parent)
		return tools.create_asset(os.path.basename(path), os.path.dirname(path), unreal.Blueprint, factory)

	def gesture_enum(gesture_name):
		return getattr(unreal.FirearmGesture, camel_to_upper(gesture_name), None)

	def pickup_mesh(name):
		for mesh_name, pickup in imp.statics(name):
			if pickup:
				return load(f"{imp.dest(name, 'Static')}/{mesh_name}")
		return None

	musket_bp = load(f"{MUSKET}/BP_Musket")
	musket_cdo = unreal.get_default_object(musket_bp.generated_class()) if musket_bp else None
	shot_sound = load(f"{MUSKET}/Audio/SW_Musket_Shot")
	attenuation = load(f"{MUSKET}/Audio/ATT_Musket_Reload")
	sphere = load(SPHERE)
	flash = load(f"{MUSKET}/FX/M_MusketFlash")
	inspect = load(INSPECT_ACTION)

	# ---------------------------------------------------------------- weapons
	for name in imp.selected():
		if name == "Ammo":
			continue
		cfg = imp.WEAPONS[name]
		if cfg.get("own_setup") and not os.environ.get("MONKE_PIRATE_WEAPONS"):
			# the weapons of T-335 have their own setup (rules, ammunition, shop, test map): a default run leaves them alone
			report["skipped"].append(f"{name}: set up by {cfg['own_setup']}")
			continue
		if not imp.pack_dir(name):
			report["skipped"].append(name)
			unreal.log_warning(f"[pirate_weapons] {name}: pack not found, setup skipped")
			continue
		folder = imp.dest(name)
		asset = cfg["asset"]
		rules_class = getattr(unreal, cfg["rules_cls"], None)
		weapon_class = getattr(unreal, cfg["cls"], None)
		if rules_class is None or weapon_class is None:
			# a pack imported before its C++ is compiled (the machine gun, T-272 / T-273): the others still get set up
			report["skipped"].append(f"{name}: class {cfg['rules_cls'] if rules_class is None else cfg['cls']} not compiled yet")
			unreal.log_warning(f"[pirate_weapons] {name}: C++ class missing ({cfg['rules_cls']} / {cfg['cls']}), setup skipped")
			continue
		rules = data_asset(f"{folder}/DA_{asset}Rules", rules_class)
		entry = {"rules": rules.get_path_name(), "clips_set": 0, "clips_added": [], "clips_missing": []}

		base = imp.bases(name)
		mesh = load(f"{folder}/{base['base']}") if base["base"] else None
		if mesh:
			rules.set_editor_property("weapon_mesh", mesh)
		arms = load(f"{folder}/{base['fp']}") if base["fp"] else None
		if arms:
			rules.set_editor_property("first_person_arms_mesh", arms)

		view = fp_view(name)
		if view:
			rules.set_editor_property("first_person_view_location", unreal.Vector(*view["location_cm"]))
			pitch, yaw, roll = view["rotation_pyr"]
			rules.set_editor_property("first_person_view_rotation", unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
			# the weapon is framed on the view (bAlignFirstPersonToView, T-160): the player's own field of view is kept
			rules.set_editor_property("first_person_field_of_view", 0.0)
		entry["fp_view"] = view

		# the four clips of every gesture of the manifest: into the clips the C++ defaults hold, added when missing
		# (Discard of the v2 packs); lengths and events written from the manifest
		clips = list(rules.get_editor_property("clips"))
		pack = dict(imp.gestures(name))
		assets = imp.clip_assets(name)
		times = timing(name)
		present = {clip.get_editor_property("gesture").name for clip in clips}
		for gesture_name in pack:
			enum_value = gesture_enum(gesture_name)
			if enum_value is None:
				entry["clips_missing"].append(f"{gesture_name} (not an EFirearmGesture)")
				continue
			if enum_value.name not in present:
				clip = unreal.FirearmClip()
				clip.set_editor_property("gesture", enum_value)
				clips.append(clip)
				present.add(enum_value.name)
				entry["clips_added"].append(gesture_name)
		for clip in clips:
			gesture = clip.get_editor_property("gesture")
			gesture_name = next((g for g in pack if camel_to_upper(g) == gesture.name), None)
			if not gesture_name:
				entry["clips_missing"].append(gesture.name)
				continue
			for prop, path in assets.get(pack[gesture_name], {}).items():
				anim = load(path)
				if anim:
					clip.set_editor_property(prop, anim)
					entry["clips_set"] += 1
			if pack[gesture_name] in times:
				frames, fps, events = times[pack[gesture_name]]
				cues = []
				for frame, event_name in events:
					event = unreal.FirearmClipEvent()
					event.set_editor_property("name", event_name)
					event.set_editor_property("time", frame / float(fps))
					cues.append(event)
				clip.set_editor_property("events", cues)
				clip.set_editor_property("duration", max(frames, 1) / float(fps))
		rules.set_editor_property("clips", clips)
		framing = imp.manifest(name).get("fp_framing")
		if not framing:
			# the v2 packs (2026-10-09) write the framing next to the manifest
			framing_file = os.path.join(imp.pack_dir(name), "fp-framing.json")
			if os.path.exists(framing_file):
				with open(framing_file, encoding="utf-8") as handle:
					framing = json.load(handle)
		if framing and "hip_grip_location_cm" in framing:
			rules.set_editor_property("hip_grip_location", unreal.Vector(*framing["hip_grip_location_cm"]))
			pitch, yaw, roll = framing["hip_rotation_pyr_deg"]
			rules.set_editor_property("hip_rotation", unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
			entry["hip_framing"] = {k: v for k, v in framing.items() if not isinstance(v, (dict, list)) or k.startswith(("hip_", "aim_"))}
		# the aim of the v2 packs: the sights on the view axis (AimMuzzleLocation / AimRotation)
		if framing and "aim_muzzle_location_cm" in framing:
			rules.set_editor_property("aim_muzzle_location", unreal.Vector(*framing["aim_muzzle_location_cm"]))
			pitch, yaw, roll = (list(framing.get("aim_rotation_pyr_deg") or [0.0, 0.0, 0.0]) + [0.0, 0.0, 0.0])[:3]
			rules.set_editor_property("aim_rotation", unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw))
			if framing.get("aim_field_of_view_deg"):
				try:
					rules.set_editor_property("aim_field_of_view", float(framing["aim_field_of_view_deg"]))
				except Exception:  # noqa: BLE001
					pass

		# shot and placeholder sounds from the musket, the musket's flame; never over the weapon's own sounds
		# (scripts/audio/weapons_audio_setup_ue.py, T-259): only the sounds still empty or still the musket's placeholders
		def placeholder(sound):
			return sound is None or sound.get_path_name().startswith(f"{MUSKET}/Audio/SW_Musket_")
		own_sounds = not placeholder(rules.get_editor_property("fire_sound")) or any(
			not placeholder(cue.get_editor_property("sound")) for cue in rules.get_editor_property("event_sounds"))
		if own_sounds:
			print(f"[setup] {name}: keeps its own sounds (T-259)")
		if shot_sound and not cfg.get("projectile") and not own_sounds:
			rules.set_editor_property("fire_sound", shot_sound)
		cues = []
		for event_name, sound_name in EVENT_SOUNDS.get(name, {}).items():
			sound = load(f"{MUSKET}/Audio/{sound_name}")
			if sound:
				cue = unreal.FirearmEventSound()
				cue.set_editor_property("event", event_name)
				cue.set_editor_property("sound", sound)
				cue.set_editor_property("volume", 0.8)
				cues.append(cue)
		if not own_sounds:
			rules.set_editor_property("event_sounds", cues)
		if attenuation and not own_sounds:
			rules.set_editor_property("event_sound_attenuation", attenuation)
		if sphere and flash and not cfg.get("projectile"):
			rules.set_editor_property("flash_mesh", sphere)
			rules.set_editor_property("flash_material", flash)

		# muzzle sockets of the pack (bones socket_*): the first for the flame, the second for a double pistol
		pack_sockets = sockets(name)
		wanted = cfg.get("muzzle_sockets", [])
		if wanted and (not pack_sockets or wanted[0] in pack_sockets):
			rules.set_editor_property("muzzle_socket", wanted[0])
		if len(wanted) > 1 and cfg["rules_cls"] in ("PistolRules", "DoublePistolRules"):
			rules.set_editor_property("second_muzzle_socket", wanted[1])
		entry["sockets_in_pack"] = pack_sockets

		if name == "Shotgun":
			spent = load(f"{folder}/Static/SM_Shotgun_Spent")
			if spent:
				rules.set_editor_property("spent_shell_mesh", spent)
		if cfg.get("projectile"):
			# the grenade in flight: BP_GrenadeProjectile on the pack's static grenade, the blast on the musket's flame and
			# the shotgun's smoke and flame, the musket's shot as the bang
			projectile_bp = blueprint(f"{folder}/BP_GrenadeProjectile", unreal.GrenadeProjectile)
			unreal.BlueprintEditorLibrary.compile_blueprint(projectile_bp)
			projectile_cdo = unreal.get_default_object(projectile_bp.generated_class())
			projectile_cdo.set_editor_property("rules", rules)
			unreal.BlueprintEditorLibrary.compile_blueprint(projectile_bp)
			eal.save_loaded_asset(projectile_bp)
			rules.set_editor_property("projectile_class", projectile_bp.generated_class())
			grenade_mesh = pickup_mesh(name)
			if grenade_mesh:
				rules.set_editor_property("projectile_mesh", grenade_mesh)
			current = rules.get_editor_property("explosion_sound")
			if shot_sound and (current is None or current.get_path_name().startswith(f"{MUSKET}/Audio/SW_Musket_")):
				rules.set_editor_property("explosion_sound", shot_sound)
			systems = [s for s in (load(f"{VFX}/NS_Shotgun_MuzzleFlame"), load(f"{VFX}/NS_Shotgun_MuzzleSmoke")) if s]
			rules.set_editor_property("explosion_systems", systems)
			if sphere and flash:
				rules.set_editor_property("explosion_flash_mesh", sphere)
				rules.set_editor_property("explosion_flash_material", flash)
			entry["projectile"] = projectile_bp.get_path_name()
		eal.save_loaded_asset(rules)

		# the BP: rules and the musket's keys
		bp = blueprint(f"{folder}/BP_{asset}", weapon_class)
		unreal.BlueprintEditorLibrary.compile_blueprint(bp)
		cdo = unreal.get_default_object(bp.generated_class())
		cdo.set_editor_property("rules", rules)
		if musket_cdo:
			cdo.set_editor_property("mapping_context", musket_cdo.get_editor_property("musket_mapping_context"))
			cdo.set_editor_property("mapping_priority", musket_cdo.get_editor_property("musket_mapping_priority"))
			for prop in ("fire_action", "aim_action", "reload_action"):
				cdo.set_editor_property(prop, musket_cdo.get_editor_property(prop))
		if inspect:
			cdo.set_editor_property("inspect_action", inspect)
		unreal.BlueprintEditorLibrary.compile_blueprint(bp)
		eal.save_loaded_asset(bp)

		# the inventory item, with the pack's pickup mesh on the ground
		item = data_asset(f"{folder}/DA_Item_{asset}", unreal.InventoryItemDefinition)
		item.set_editor_property("display_name", unreal.Text(cfg["display"]))
		item.set_editor_property("item_class", bp.generated_class())
		ground = pickup_mesh(name)
		if ground:
			item.set_editor_property("pickup_mesh", ground)
		eal.save_loaded_asset(item)
		if name == "Musket":
			# the old musket's item (StartingItems of BP_IslanderBean, pickups of the maps) gives the redesigned one
			old_item = load(f"{MUSKET}/DA_Item_Musket")
			if old_item:
				old_item.set_editor_property("item_class", bp.generated_class())
				eal.save_loaded_asset(old_item)
				entry["old_item_redirected"] = old_item.get_path_name()
		entry["blueprint"] = bp.get_path_name()
		entry["item"] = item.get_path_name()
		report["weapons"][name] = entry
		unreal.log(f"[pirate_weapons] {name}: {entry['clips_set']} clips set, added {entry['clips_added']}, missing {entry['clips_missing']}")

	# ---------------------------------------------------------------- ammunition
	ammo_folder = imp.dest("Ammo")
	ammo_items = {}
	if "Ammo" in imp.selected():
		meshes = {}
		mesh_folder = imp.dest("Ammo", "Meshes")
		if eal.does_directory_exist(mesh_folder):
			for path in eal.list_assets(mesh_folder, recursive=False, include_folder=False):
				short = os.path.basename(path).split(".")[0]
				# the folder also holds the textures and material instances of the pack: meshes only
				if not short.startswith("SM_"):
					continue
				meshes[short] = path.split(".")[0]
		# the longest keyword that names a mesh wins; a mesh serves one definition
		taken = set()
		order = sorted(AMMO_DEFS.items(), key=lambda kv: -max(len(k) for k in kv[1][2]))
		chosen = {}
		for key, (_, _, keywords, _, _) in order:
			best = None
			for keyword in sorted(keywords, key=len, reverse=True):
				for short, path in meshes.items():
					normalized = short.lower().replace("sm_ammo_", "").replace("sm_", "")
					if keyword.lower() in normalized and path not in taken:
						best = path
						break
				if best:
					break
			if best:
				taken.add(best)
			chosen[key] = best
		for key, (display, lots, keywords, price, weight) in AMMO_DEFS.items():
			definition = data_asset(f"{ammo_folder}/DA_Ammo_{key}", unreal.AmmoDefinition)
			definition.set_editor_property("display_name", unreal.Text(display))
			lot_structs = []
			for kind_name, count in lots:
				lot = unreal.AmmoLot()
				lot.set_editor_property("kind", getattr(unreal.AmmoKind, kind_name))
				lot.set_editor_property("count", int(count))
				lot_structs.append(lot)
			definition.set_editor_property("lots", lot_structs)
			mesh_path = chosen.get(key)
			mesh = load(mesh_path) if mesh_path else None
			if mesh:
				definition.set_editor_property("pickup_mesh", mesh)
			eal.save_loaded_asset(definition)
			bp = blueprint(f"{ammo_folder}/BP_Ammo_{key}", unreal.AmmoPickup)
			unreal.BlueprintEditorLibrary.compile_blueprint(bp)
			cdo = unreal.get_default_object(bp.generated_class())
			cdo.set_editor_property("definition", definition)
			unreal.BlueprintEditorLibrary.compile_blueprint(bp)
			eal.save_loaded_asset(bp)
			ammo_items[key] = (definition, bp)
			report["ammo"][key] = {"definition": definition.get_path_name(), "blueprint": bp.get_path_name(), "mesh": mesh_path,
				"lots": lots, "price": price, "loot_weight": weight}
			if not mesh_path:
				unreal.log_warning(f"[pirate_weapons] ammo {key}: no SM_Ammo_* mesh matched {keywords} (placeholder box)")

	# ---------------------------------------------------------------- the merchant's lines (missing ones appended)
	if not os.environ.get("MONKE_WEAPONS_NO_SHOP"):
		catalog = load(SHOP_CATALOG)
		if catalog:
			offers = list(catalog.get_editor_property("offers"))
			sold_items = {o.get_editor_property("item").get_name() for o in offers if o.get_editor_property("item")}
			sold_ammo = {o.get_editor_property("ammo").get_name() for o in offers if o.get_editor_property("ammo")}
			for name, (price, line) in SHOP_WEAPONS.items():
				item_path = f"{imp.dest(name)}/DA_Item_{imp.WEAPONS[name]['asset']}"
				if not eal.does_asset_exist(item_path):
					continue
				item = eal.load_asset(item_path)
				if item.get_name() in sold_items:
					continue
				offer = unreal.ShopOffer()
				offer.set_editor_property("item", item)
				offer.set_editor_property("price", price)
				offer.set_editor_property("description", unreal.Text(line))
				offers.append(offer)
				report["shop"].append([item.get_name(), price])
			for key, (display, lots, _, price, _) in AMMO_DEFS.items():
				definition = ammo_items.get(key, (None, None))[0] or (eal.load_asset(f"{ammo_folder}/DA_Ammo_{key}") if eal.does_asset_exist(f"{ammo_folder}/DA_Ammo_{key}") else None)
				if price is None or not definition or definition.get_name() in sold_ammo:
					continue
				offer = unreal.ShopOffer()
				offer.set_editor_property("ammo", definition)
				offer.set_editor_property("price", price)
				offer.set_editor_property("description", unreal.Text(f"{display} : " + ", ".join(f"{n} x {k.lower().replace('_', ' ')}" for k, n in lots)))
				offers.append(offer)
				report["shop"].append([definition.get_name(), price])
			catalog.set_editor_property("offers", offers)
			eal.save_loaded_asset(catalog)

	# ---------------------------------------------------------------- the French fort loot (ammunition crates)
	if not os.environ.get("MONKE_WEAPONS_NO_LOOT") and eal.does_asset_exist(FORT_LOOT):
		loot = eal.load_asset(FORT_LOOT)
		entries = list(loot.get_editor_property("items"))
		present = {e.get_editor_property("item_class").get_name() for e in entries if e.get_editor_property("item_class")}
		for key, (_, _, _, _, weight) in AMMO_DEFS.items():
			bp_path = f"{ammo_folder}/BP_Ammo_{key}"
			if weight is None or not eal.does_asset_exist(bp_path):
				continue
			cls = eal.load_blueprint_class(bp_path)
			if not cls or cls.get_name() in present:
				continue
			entry = unreal.LootEntry()
			entry.set_editor_property("item_class", cls)
			entry.set_editor_property("weight", float(weight))
			entries.append(entry)
			report["loot"].append([cls.get_name(), weight])
		loot.set_editor_property("items", entries)
		eal.save_loaded_asset(loot)

	out = os.path.join(unreal.Paths.project_saved_dir(), "MonkeTools", "pirate_weapons_setup.json")
	os.makedirs(os.path.dirname(out), exist_ok=True)
	with open(out, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=1, ensure_ascii=False)
	unreal.log(f"[pirate_weapons] setup done, {len(report['missing'])} missing assets, skipped {report['skipped']} -> {out}")


if __name__ == "__main__":
	if "--check" in sys.argv:
		sys.exit(check())
	try:
		import unreal  # noqa: F401
	except ImportError:
		sys.exit("run inside Unreal (commandlet -run=pythonscript) or with --check")
	run_in_unreal()
