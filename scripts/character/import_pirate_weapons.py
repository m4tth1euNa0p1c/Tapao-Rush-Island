"""Imports the bean's weapon packs (pistol, double pistol, shotgun, musket, blunderbuss, rifle, grenade, machine gun;
T-156, T-170, T-249, T-272; bolt rifle, submachine gun, bayonet musket: T-334 / T-335) and the ammunition models (pack
ammo) into Unreal.

Sources (format bean-musket-anim/1: <manifest>.json, exports/{Bases,Animations,Static}, textures/; see
scripts/props/musket_pack and the docstring of pirate_weapon_clips.py), read from build/packs/<pack>/ first, then
from OneDrive (<MAQUETTES>/<pack>, read only) for the first pistol and shotgun when the rebuilt pack is not there:
  Pistol        build/packs/bean-pirate-pistol         pistol-manifest.json          30 fps   (OneDrive bean-pirate-pistol)
  DoublePistol  build/packs/bean-pirate-pistol-double  pistol-double-manifest.json   30 fps   sockets muzzle, muzzle2
  Shotgun       build/packs/bean-pirate-shotgun        shotgun-manifest.json         30 fps   (OneDrive bean-pirate-shotgun)
  Musket        build/packs/bean-musket-anim           musket-manifest.json          24 fps   (built here, T-212)
  Blunderbuss   build/packs/bean-blunderbuss-anim      blunderbuss-manifest.json     24 fps   the musket's 24 gestures
  Rifle         build/packs/bean-rifle-anim            rifle-manifest.json           24 fps   the musket's 24 gestures
  Grenade       build/packs/bean-grenade               grenade-manifest.json         30 fps   13 gestures
  MachineGun    build/packs/bean-machinegun            machinegun-manifest.json      30 fps   13 gestures (FireStart / FireLoop /
                                                                                              FireEnd / Overheat: T-273 adds them to
                                                                                              EFirearmGesture), statics SM_MG_*, SM_Ammo_MGBox
  BoltRifle     art/export/Weapons/BoltRifle/pack.json      (T-334: the manifest itself, or the build spec whose id names
  Mitraillette  art/export/Weapons/Mitraillette/pack.json    build/packs/<id>; see art_pack_manifest). Gestures BoltCycle,
  BayonetMusket art/export/Weapons/BayonetMusket/pack.json   BayonetStab, BayonetCharge (T-335); data: new_weapons_ue.py
  Ammo          build/packs/ammo                       ammo-manifest.json            static meshes SM_Ammo_* only
All FBX are in metres (UnitScaleFactor 100): bConvertSceneUnit brings them to cm, with an import scale of 1.
The asset names come from the manifest (assets: the weapon base = the skeletal mesh of the weapon's own skeleton
group, the FP arms = the Bean skeletal mesh of view FP, the statics = the static meshes, the pickup = view "pickup");
the registry WEAPONS below only names the packs, the classes and the asset prefixes.

Destination /Game/MonkeIsland/Weapons/<Weapon>/ (Weapon = Pistol | DoublePistol | Shotgun | Musket | Blunderbuss |
Rifle | Grenade | MachineGun | BoltRifle | Mitraillette | BayonetMusket; Ammo in /Game/MonkeIsland/Weapons/Ammo/):
  SK_Bean_Pirate<Weapon>               weapon base, with ITS OWN skeleton (SK_Bean_Pirate<Weapon>_Skeleton)
  SK_Bean_<Weapon>_FP                  first person arms of the pack, on the PROJECT's bean skeleton
                                       (/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton, reference pose kept)
  Animations/Bean/A_*, Animations/Weapon/A_*   the clips, named as the manifest names them (bean on the project's
                                       bean skeleton, weapon on the weapon skeleton)
  Static/SM_*                          pickup mesh (with collision), tokens, shells, grenade (collision on the pickup only)
  Textures/T_<Asset>_<Mat>_{BaseColor,Normal,ORM}   the PBR textures of manifest.materials[<name>].textures
  Materials/MI_<Asset>_<Mat>           instances of M_Character_Baked (import_coco_real_ue.master_material), assigned
                                       on every imported mesh by imported_material_slot_name; a material without
                                       textures keeps the material the FBX import made (M_* / MAT-*)
  Ammo/Meshes/SM_Ammo_*, Ammo/Textures, Ammo/Materials    the ammunition models the same way
The TP bean bases (SK_Bean_<Weapon>_TP) are not imported: in game the bean is the project's puppet.

After the import, the report checks (Saved/MonkeTools/pirate_weapons_report.json):
  - every AnimSequence is on the expected skeleton;
  - the root scale of the pack's TP idle clip equals the root scale of the old musket's A_Bean_Musket_Hold (the
    project's bean carries a root scale of 100);
  - the weapon mesh size against the bounds of the manifest.
Then run scripts/character/setup_pirate_weapons.py (DA, BP, DA_Item, DA_Ammo, shop, loot).

The MCP cannot import an armature-only FBX: editor commandlet, EDITOR CLOSED (lock held, scripts/editor_lock.py):
  "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
      "C:/Users/matth/monk-island/monke_island/monke_island.uproject"
      -run=pythonscript -script="C:/Users/matth/monk-island/scripts/character/import_pirate_weapons.py"
      -unattended -nosplash -nullrhi -nosourcecontrol
Optional filter (pythonscript passes no argv reliably): MONKE_PIRATE_WEAPONS=Pistol,Shotgun,Ammo (default: every pack
that is present; a missing pack is skipped with a log). Existing destination assets are replaced.

Dry run, outside Unreal (files, sha256 of the manifest, FBX units, destination plan, generated clip table; touches
nothing):
  python scripts/character/import_pirate_weapons.py --check
"""
import hashlib
import json
import os
import struct
import subprocess
import sys

MAQUETTES = r"C:\Users\matth\OneDrive\maquette-blender\maquettes"
HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, "..", ".."))
PACKS = os.path.join(ROOT, "build", "packs")
PROJECT_CONTENT = os.path.join(ROOT, "monke_island", "Content")
WEAPONS_ROOT = "/Game/MonkeIsland/Weapons"
BEAN_SKELETON = "/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton"
REFERENCE_BEAN_CLIP = "/Game/MonkeIsland/Weapons/Musket/Animations/A_Bean_Musket_Hold"
AMMO_FOLDER = f"{WEAPONS_ROOT}/Ammo"

# the registry: pack and manifest (build/packs first, OneDrive as a fallback), C++ classes of the rules and the weapon,
# asset base name (DA_<asset>Rules, BP_<asset>, DA_Item_<asset>), display name, the pack's clip names that differ from
# EFirearmGesture (clip_name), gestures played with another one's clip (aliases), muzzle sockets (bones of the weapon)
WEAPONS = {
	"Pistol": {"pack": "bean-pirate-pistol", "manifest": "pistol-manifest.json", "onedrive": "bean-pirate-pistol",
		"rules_cls": "PistolRules", "cls": "PistolWeapon", "asset": "Pistol", "display": "Pistolet",
		"muzzle_sockets": ["socket_muzzle"]},
	"DoublePistol": {"pack": "bean-pirate-pistol-double", "manifest": "pistol-double-manifest.json",
		"rules_cls": "DoublePistolRules", "cls": "PistolWeapon", "asset": "DoublePistol", "display": "Pistolet double",
		"muzzle_sockets": ["socket_muzzle", "socket_muzzle2"]},
	"Shotgun": {"pack": "bean-pirate-shotgun", "manifest": "shotgun-manifest.json", "onedrive": "bean-pirate-shotgun",
		"rules_cls": "ShotgunRules", "cls": "ShotgunWeapon", "asset": "Shotgun", "display": "Fusil à pompe",
		"clip_name": {"Fire": "FirePump", "AimFire": "AimFirePump"}, "muzzle_sockets": ["socket_muzzle"]},
	"Musket": {"pack": "bean-musket-anim", "manifest": "musket-manifest.json",
		"rules_cls": "PirateMusketRules", "cls": "PirateMusketWeapon", "asset": "PirateMusket", "display": "Mousquet",
		"aliases": {"AimFire": "Fire"}, "idle": "Hold", "muzzle_sockets": ["socket_muzzle"]},
	"Blunderbuss": {"pack": "bean-blunderbuss-anim", "manifest": "blunderbuss-manifest.json",
		"rules_cls": "BlunderbussRules", "cls": "BlunderbussWeapon", "asset": "Blunderbuss", "display": "Tromblon",
		"aliases": {"AimFire": "Fire"}, "idle": "Hold", "muzzle_sockets": ["socket_muzzle"]},
	"Rifle": {"pack": "bean-rifle-anim", "manifest": "rifle-manifest.json",
		"rules_cls": "RifleRules", "cls": "PirateMusketWeapon", "asset": "Rifle", "display": "Carabine",
		"aliases": {"AimFire": "Fire"}, "idle": "Hold", "muzzle_sockets": ["socket_muzzle"]},
	"Grenade": {"pack": "bean-grenade", "manifest": "grenade-manifest.json",
		"rules_cls": "GrenadeRules", "cls": "GrenadeWeapon", "asset": "Grenade", "display": "Grenades",
		"muzzle_sockets": ["socket_fuse"], "projectile": True},
	# T-272 (pack) / T-273 (C++ AMachineGunWeapon, UMachineGunRules): hand-cranked heavy machine gun Mle 1918
	"MachineGun": {"pack": "bean-machinegun", "manifest": "machinegun-manifest.json",
		"rules_cls": "MachineGunRules", "cls": "MachineGunWeapon", "asset": "MachineGun", "display": "Mitrailleuse",
		"muzzle_sockets": ["socket_muzzle"]},
	# T-334 (packs, named by art/export/Weapons/<art_pack>/pack.json: see art_pack_manifest) / T-335 (C++): the bolt-action
	# marksman rifle with its scope, the submachine gun with its drum, the bayonet musket. Their data, shop lines and
	# test map pickups are scripts/editor/new_weapons_ue.py's ("own_setup": setup_pirate_weapons.py leaves them alone
	# unless MONKE_PIRATE_WEAPONS names them)
	"BoltRifle": {"art_pack": "BoltRifle", "rules_cls": "BoltRifleRules", "cls": "BoltRifleWeapon", "asset": "BoltRifle",
		"display": "Fusil à verrou", "muzzle_sockets": ["socket_muzzle"], "own_setup": "scripts/editor/new_weapons_ue.py"},
	"Mitraillette": {"art_pack": "Mitraillette", "rules_cls": "SubmachineGunRules", "cls": "MachineGunWeapon", "asset": "Mitraillette",
		"display": "Mitraillette", "muzzle_sockets": ["socket_muzzle"], "own_setup": "scripts/editor/new_weapons_ue.py"},
	"BayonetMusket": {"art_pack": "BayonetMusket", "rules_cls": "BayonetMusketRules", "cls": "BayonetMusketWeapon", "asset": "BayonetMusket",
		"display": "Mousquet à baïonnette", "aliases": {"AimFire": "Fire"}, "idle": "Hold", "muzzle_sockets": ["socket_muzzle"],
		"own_setup": "scripts/editor/new_weapons_ue.py"},
}
# the packs of the art task T-334: art/export/Weapons/<art_pack>/pack.json
ART_WEAPONS = os.path.join(ROOT, "art", "export", "Weapons")
AMMO = {"pack": "ammo", "manifest": "ammo-manifest.json"}
# texture labels of the manifests (image node label or name) -> parameter of M_Character_Baked
TEXTURE_LABELS = {
	"basecolor": "BaseColor", "base_color": "BaseColor", "base color": "BaseColor", "albedo": "BaseColor", "bc": "BaseColor", "diffuse": "BaseColor",
	"normal": "Normal", "normalmap": "Normal", "normal_map": "Normal", "n": "Normal", "nrm": "Normal",
	"orm": "ORM", "occlusionroughnessmetallic": "ORM", "occlusion_roughness_metallic": "ORM", "masks": "ORM",
	"emissive": "Emissive", "emission": "Emissive",
}


def selected():
	default = list(WEAPONS) + ["Ammo"]
	names = os.environ.get("MONKE_PIRATE_WEAPONS", ",".join(default)).split(",")
	return [n.strip() for n in names if n.strip() in default]


def is_manifest(data):
	"""A pack manifest (format bean-musket-anim/1): its scenes and its assets."""
	return isinstance(data, dict) and bool(data.get("scenes")) and bool(data.get("assets"))


def art_pack_manifest(art_pack):
	"""(pack folder, manifest file) of a pack of the art task T-334 named by art/export/Weapons/<art_pack>/pack.json:
	the pack.json itself when it is the manifest (scenes and assets, the files next to it), else the manifest it points
	to (keys pack_dir / output / build_dir, then build/packs/<id>, then its own folder: the first json there with scenes
	and assets); (None, None) while the pack is not there."""
	spec_path = os.path.join(ART_WEAPONS, art_pack, "pack.json")
	if not os.path.isfile(spec_path):
		return None, None
	try:
		with open(spec_path, encoding="utf-8") as f:
			spec = json.load(f)
	except (OSError, ValueError):
		return None, None
	if is_manifest(spec):
		return os.path.dirname(spec_path), spec_path
	folders = []
	if isinstance(spec, dict):
		for key in ("pack_dir", "output", "build_dir"):
			if isinstance(spec.get(key), str) and spec[key]:
				folders.append(spec[key] if os.path.isabs(spec[key]) else os.path.join(ROOT, spec[key]))
		if isinstance(spec.get("id"), str) and spec["id"]:
			folders.append(os.path.join(PACKS, spec["id"]))
	folders.append(os.path.dirname(spec_path))
	for folder in folders:
		if not os.path.isdir(folder):
			continue
		for name in sorted(os.listdir(folder)):
			path = os.path.join(folder, name)
			if not name.lower().endswith(".json") or os.path.abspath(path) == os.path.abspath(spec_path):
				continue
			try:
				with open(path, encoding="utf-8") as f:
					data = json.load(f)
			except (OSError, ValueError):
				continue
			if is_manifest(data):
				return folder, path
	return None, None


def art_pack_built(manifest_path):
	"""An art pack whose Blender build wrote its files ("status" not provisional / planned / draft)."""
	try:
		with open(manifest_path, encoding="utf-8") as f:
			status = str(json.load(f).get("status", "")).lower()
	except (OSError, ValueError):
		return False
	return status not in ("provisional", "planned", "draft")


def pack_dir(name):
	"""Folder of a pack (a weapon of WEAPONS or "Ammo"): build/packs, else OneDrive, else None (the art packs of T-334:
	art_pack_manifest, None while still provisional: the import and its check skip them like a missing pack)."""
	cfg = AMMO if name == "Ammo" else WEAPONS[name]
	if cfg.get("art_pack"):
		folder, path = art_pack_manifest(cfg["art_pack"])
		return folder if path and art_pack_built(path) else None
	local = os.path.join(PACKS, cfg["pack"])
	if os.path.isfile(os.path.join(local, cfg["manifest"])):
		return local
	if cfg.get("onedrive") and os.path.isfile(os.path.join(MAQUETTES, cfg["onedrive"], cfg["manifest"])):
		return os.path.join(MAQUETTES, cfg["onedrive"])
	return None


def manifest_file(name):
	"""Manifest file of a pack, None while it is missing."""
	cfg = AMMO if name == "Ammo" else WEAPONS[name]
	if cfg.get("art_pack"):
		return art_pack_manifest(cfg["art_pack"])[1]
	root = pack_dir(name)
	return os.path.join(root, cfg["manifest"]) if root else None


def dest(weapon, sub=""):
	return (AMMO_FOLDER if weapon == "Ammo" else f"{WEAPONS_ROOT}/{weapon}") + (f"/{sub}" if sub else "")


def manifest(name):
	path = manifest_file(name)
	if not path:
		cfg = AMMO if name == "Ammo" else WEAPONS[name]
		where = f"art/export/Weapons/{cfg['art_pack']}/pack.json" if cfg.get("art_pack") else f"{cfg.get('pack')}, build/packs or OneDrive"
		raise SystemExit(f"{name}: pack missing ({where})")
	with open(path, encoding="utf-8") as f:
		return json.load(f)


def asset_file(name, stem, default_relative):
	"""Absolute path of the .fbx of an asset of a pack: where the manifest lists it, else default_relative."""
	for entry in manifest(name)["assets"]:
		if asset_name(entry) == stem and entry["path"].lower().endswith(".fbx"):
			return os.path.join(pack_dir(name), entry["path"])
	return os.path.join(pack_dir(name), default_relative)


def fps_of(weapon):
	m = manifest(weapon)
	return int(m.get("fps") or next(s["fps"] for s in m["scenes"]))


def asset_name(entry):
	return os.path.splitext(os.path.basename(entry["path"]))[0]


def bases(weapon):
	"""{"base": weapon skeletal mesh name, "fp": FP arms name} from the manifest (None when missing)."""
	out = {"base": None, "fp": None}
	for entry in manifest(weapon)["assets"]:
		if entry["type"] != "skeletal_mesh" or not entry["path"].lower().endswith(".fbx"):
			continue
		if entry["skeleton_group"].lower() == "bean":
			if entry.get("view") == "FP":
				out["fp"] = asset_name(entry)
		elif out["base"] is None:
			out["base"] = asset_name(entry)
	return out


def statics(name):
	"""[(asset name, is_pickup)] of the static meshes of a pack (the .fbx of each; the first one is the pickup when none
	is marked)."""
	rows = []
	for entry in manifest(name)["assets"]:
		if entry["type"] == "static_mesh" and entry["path"].lower().endswith(".fbx"):
			rows.append([asset_name(entry), entry.get("view") == "pickup"])
	if rows and not any(pickup for _, pickup in rows) and name != "Ammo":
		rows[0][1] = True
	if name == "Ammo":
		rows = [[n, True] for n, _ in rows]
	return [(n, p) for n, p in rows]


def animation_plan(weapon):
	"""(fbx, destination folder, asset name, role, clip) for every clip of the manifest; role = bean | weapon."""
	rows = []
	root = pack_dir(weapon)
	for entry in manifest(weapon)["assets"]:
		if entry["type"] != "animation":
			continue
		role = "bean" if entry["skeleton_group"].lower() == "bean" else "weapon"
		rows.append((os.path.join(root, entry["path"]), dest(weapon, "Animations/Bean" if role == "bean" else "Animations/Weapon"),
			asset_name(entry), role, entry.get("clip"), entry.get("view")))
	return rows


def clip_assets(weapon):
	"""clip -> {bean_third_person, weapon_third_person, bean_first_person, weapon_first_person}: asset paths of the
	four animations of every clip of the pack."""
	out = {}
	for _, folder, name, role, clip, view in animation_plan(weapon):
		if not clip or view not in ("TP", "FP"):
			continue
		prop = f"{role}_{'third' if view == 'TP' else 'first'}_person"
		out.setdefault(clip, {})[prop] = f"{folder}/{name}"
	return out


def gestures(weapon):
	"""(EFirearmGesture name, clip name of the pack) of every gesture of the manifest, aliases included."""
	cfg = WEAPONS[weapon]
	clip_name = cfg.get("clip_name", {})
	out = []
	for scene in manifest(weapon)["scenes"]:
		if scene["view"] != "TP":
			continue
		clip = scene["clip"]
		gesture = next((g for g, c in clip_name.items() if c == clip), clip)
		out.append((gesture, clip))
	clips = {c for _, c in out}
	out += [(g, c) for g, c in cfg.get("aliases", {}).items() if c in clips and g not in {x for x, _ in out}]
	return out


def materials(name):
	"""material name -> {parameter: texture path (absolute)} for the materials of a pack that carry textures."""
	root = pack_dir(name)
	out = {}
	for mat, data in (manifest(name).get("materials") or {}).items():
		textures = data.get("textures") or {}
		if isinstance(textures, list):
			textures = {t.get("label") or t.get("name"): t.get("path") for t in textures if t.get("path")}
		mapped = {}
		for label, relative in textures.items():
			# a pack may describe a texture as a record (path, size, colour space...) rather than a path
			if isinstance(relative, dict):
				relative = relative.get("path") or relative.get("file") or relative.get("relative") or relative.get("filepath")
			param = TEXTURE_LABELS.get(str(label).strip().lower())
			if param and isinstance(relative, str) and relative:
				mapped[param] = os.path.join(root, relative)
		if mapped:
			out[mat] = mapped
	return out


def material_short_name(mat):
	"""MAT-Musket-Brass / M_Bean_Wood / MAT-Ammo-Leather -> Brass / Wood / Leather (asset friendly)."""
	short = mat
	for prefix in ("MAT-", "M_", "MI_"):
		if short.startswith(prefix):
			short = short[len(prefix):]
	parts = [p for p in short.replace("-", "_").split("_") if p]
	if len(parts) > 1 and parts[0].lower() in ("bean", "musket", "pistol", "shotgun", "blunderbuss", "rifle", "grenade", "ammo", "pistoldouble", "doublepistol",
			"mg", "machinegun", "boltrifle", "sniper", "mitraillette", "smg", "bayonetmusket", "bayonet"):
		parts = parts[1:]
	return "".join(p[:1].upper() + p[1:] for p in parts) or "Mat"


def fbx_unit(path):
	"""UnitScaleFactor of a binary FBX (cm per file unit), None if not found."""
	data = open(path, "rb").read()
	i = data.find(b"UnitScaleFactor")
	if i < 0:
		return None
	j = data.find(b"D", data.find(b"Number", i))
	return struct.unpack("<d", data[j + 1:j + 9])[0]


def sha256(path):
	h = hashlib.sha256()
	with open(path, "rb") as f:
		for chunk in iter(lambda: f.read(1 << 20), b""):
			h.update(chunk)
	return h.hexdigest()


def check():
	report = {"packs": {}, "missing_packs": [], "missing": [], "hash_mismatch": [], "units": {}, "plan": {}}
	for name in selected():
		root = pack_dir(name)
		if not root:
			report["missing_packs"].append(name)
			continue
		report["packs"][name] = root.replace(os.sep, "/")
		m = manifest(name)
		for entry in m["assets"]:
			path = os.path.join(root, entry["path"])
			if not os.path.isfile(path):
				report["missing"].append(path)
				continue
			if entry.get("sha256") and sha256(path) != entry["sha256"]:
				report["hash_mismatch"].append(path)
			if path.lower().endswith(".fbx"):
				unit = fbx_unit(path)
				report["units"][str(unit)] = report["units"].get(str(unit), 0) + 1
		for mat, textures in materials(name).items():
			for param, path in textures.items():
				if not os.path.isfile(path):
					report["missing"].append(path)
		if name == "Ammo":
			report["plan"]["Ammo"] = {"meshes": [f"{dest('Ammo', 'Meshes')}/{s}" for s, _ in statics("Ammo")],
				"materials": {mat: list(t) for mat, t in materials("Ammo").items()}, "format": f"{m.get('format')} v{m.get('version')}"}
			continue
		anims = animation_plan(name)
		base = bases(name)
		report["plan"][name] = {
			"base": f"{dest(name)}/{base['base']} (own skeleton)",
			"fp_arms": f"{dest(name)}/{base['fp']} on {BEAN_SKELETON}",
			"fps": fps_of(name),
			"bean_clips": sum(1 for a in anims if a[3] == "bean"),
			"weapon_clips": sum(1 for a in anims if a[3] == "weapon"),
			"gestures": gestures(name),
			"static": [f"{dest(name, 'Static')}/{s}{' (pickup)' if p else ''}" for s, p in statics(name)],
			"materials": {mat: list(t) for mat, t in materials(name).items()},
			"format": f"{m.get('format')} v{m.get('version')}",
			"runtime_tested_by_pack": m.get("unreal_import", {}).get("runtime_tested"),
		}
	report["bean_skeleton_in_project"] = os.path.isfile(os.path.join(PROJECT_CONTENT, "MonkeIsland", "Characters", "Bean", "Meshes",
		"SK_Bean_Body_Skeleton.uasset"))
	clips = subprocess.run([sys.executable, os.path.join(HERE, "pirate_weapon_clips.py"), "--check"], capture_output=True, text=True)
	report["clip_table"] = clips.stdout.strip()
	print(json.dumps(report, indent=1, ensure_ascii=False))
	ok = not report["missing"] and not report["hash_mismatch"] and report["bean_skeleton_in_project"] and clips.returncode == 0
	print("CHECK OK" if ok else "CHECK FAILED", f"(packs missing, skipped: {report['missing_packs']})" if report["missing_packs"] else "")
	return 0 if ok else 1


def run_in_unreal():
	import unreal

	sys.path.insert(0, HERE)
	import import_coco_real_ue  # noqa: E402 - master material M_Character_Baked (needs unreal)

	eal = unreal.EditorAssetLibrary
	tools = unreal.AssetToolsHelpers.get_asset_tools()
	mel = unreal.MaterialEditingLibrary

	def load(path):
		return eal.load_asset(path) if eal.does_asset_exist(path) else None

	def common(data):
		data.set_editor_property("convert_scene", True)
		data.set_editor_property("convert_scene_unit", True)
		data.set_editor_property("force_front_x_axis", False)
		data.set_editor_property("import_uniform_scale", 1.0)

	def task(filename, folder, name, opts):
		t = unreal.AssetImportTask()
		t.filename = filename
		t.destination_path = folder
		t.destination_name = name
		t.replace_existing = True
		t.automated = True
		t.save = True
		if opts is not None:
			t.options = opts
		return t

	def skeletal_task(fbx, folder, name, skeleton):
		opts = unreal.FbxImportUI()
		opts.import_mesh = True
		opts.import_animations = False
		opts.import_as_skeletal = True
		opts.import_materials = True
		opts.import_textures = True
		opts.create_physics_asset = False
		opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_SKELETAL_MESH
		if skeleton:
			opts.skeleton = skeleton
		mesh = opts.skeletal_mesh_import_data
		common(mesh)
		mesh.set_editor_property("update_skeleton_reference_pose", False)
		mesh.set_editor_property("import_morph_targets", False)
		return task(fbx, folder, name, opts)

	def animation_task(fbx, folder, name, skeleton, fps):
		opts = unreal.FbxImportUI()
		opts.import_mesh = False
		opts.import_animations = True
		opts.import_materials = False
		opts.import_textures = False
		opts.import_as_skeletal = True
		opts.skeleton = skeleton
		opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_ANIMATION
		common(opts.skeletal_mesh_import_data)
		anim = opts.anim_sequence_import_data
		common(anim)
		anim.set_editor_property("import_translation", unreal.Vector(0, 0, 0))
		anim.set_editor_property("animation_length", unreal.FBXAnimationLengthImportType.FBXALIT_EXPORTED_TIME)
		anim.set_editor_property("import_bone_tracks", True)
		anim.set_editor_property("remove_redundant_keys", False)
		anim.set_editor_property("use_default_sample_rate", False)
		anim.set_editor_property("custom_sample_rate", fps)
		return task(fbx, folder, name, opts)

	def static_task(fbx, folder, name, collision):
		opts = unreal.FbxImportUI()
		opts.import_mesh = True
		opts.import_animations = False
		opts.import_as_skeletal = False
		opts.import_materials = True
		opts.import_textures = True
		opts.mesh_type_to_import = unreal.FBXImportType.FBXIT_STATIC_MESH
		data = opts.static_mesh_import_data
		common(data)
		data.set_editor_property("auto_generate_collision", collision)
		data.set_editor_property("combine_meshes", True)
		return task(fbx, folder, name, opts)

	def import_texture(png, folder, name, param):
		"""A PBR texture of the pack: sRGB colour, DirectX normal (no green flip), linear masks."""
		tools.import_asset_tasks([task(png, folder, name, None)])
		tex = load(f"{folder}/{name}")
		if not tex:
			return None
		if param == "Normal":
			tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_NORMALMAP)
			tex.set_editor_property("srgb", False)
			tex.set_editor_property("flip_green_channel", False)
		elif param == "ORM":
			tex.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_MASKS)
			tex.set_editor_property("srgb", False)
		else:
			tex.set_editor_property("srgb", True)
		eal.save_loaded_asset(tex)
		return tex

	def material_instances(name, asset):
		"""MI_<asset>_<Mat> of every textured material of a pack, parented to M_Character_Baked: {material name: MI}."""
		out = {}
		textured = materials(name)
		if not textured:
			return out
		master = import_coco_real_ue.master_material()
		for mat, textures in textured.items():
			short = material_short_name(mat)
			mi_path = f"{dest(name, 'Materials')}/MI_{asset}_{short}"
			mi = load(mi_path)
			if not mi:
				factory = unreal.MaterialInstanceConstantFactoryNew()
				mi = tools.create_asset(f"MI_{asset}_{short}", dest(name, "Materials"), unreal.MaterialInstanceConstant, factory)
			mel.set_material_instance_parent(mi, master)
			for param, png in textures.items():
				tex = import_texture(png, dest(name, "Textures"), f"T_{asset}_{short}_{param}", param)
				if tex:
					mel.set_material_instance_texture_parameter_value(mi, param, tex)
			mel.set_material_instance_scalar_parameter_value(mi, "EmissiveStrength", 1.0 if "Emissive" in textures else 0.0)
			eal.save_loaded_asset(mi)
			out[mat] = mi
		return out

	def assign(mesh, mis):
		"""The MIs on the slots whose imported material has one (copies of the slot structs, UE 5.8); returns [slot, mi]."""
		if not mesh or not mis:
			return []
		prop = "materials" if isinstance(mesh, unreal.SkeletalMesh) else "static_materials"
		slots = list(mesh.get_editor_property(prop))
		used = []
		for index, slot in enumerate(slots):
			imported = str(slot.get_editor_property("imported_material_slot_name"))
			mi = mis.get(imported) or mis.get(imported.split(".")[0])
			if mi:
				slot.set_editor_property("material_interface", mi)
				slots[index] = slot
				used.append([imported, mi.get_name()])
		if used:
			mesh.set_editor_property(prop, slots)
			eal.save_loaded_asset(mesh)
		return used

	def root_scale(sequence):
		try:
			pose = unreal.AnimationLibrary.get_bone_pose_for_frame(sequence, "root", 0, False)
			s = pose.scale3d
			return [round(s.x, 3), round(s.y, 3), round(s.z, 3)]
		except Exception as error:  # noqa: BLE001 - reported, not fatal
			return f"unreadable: {error}"

	report = {"selected": selected(), "skipped": [], "imported": [], "failed": [], "checks": {}, "materials": {}}
	bean_skeleton = load(BEAN_SKELETON)
	if not bean_skeleton:
		unreal.log_error(f"[pirate_weapons] {BEAN_SKELETON} missing: nothing imported")
		return
	reference = load(REFERENCE_BEAN_CLIP)
	reference_scale = root_scale(reference) if reference else None

	for name in selected():
		root = pack_dir(name)
		if not root:
			report["skipped"].append(name)
			unreal.log_warning(f"[pirate_weapons] {name}: pack not found (build/packs or OneDrive), skipped")
			continue
		if name == "Ammo":
			# the ammunition models: static meshes with collision, their PBR materials
			static_tasks = [static_task(os.path.join(root, next(e["path"] for e in manifest("Ammo")["assets"] if asset_name(e) == s and e["path"].lower().endswith(".fbx"))),
				dest("Ammo", "Meshes"), s, True) for s, _ in statics("Ammo")]
			tools.import_asset_tasks(static_tasks)
			mis = material_instances("Ammo", "Ammo")
			report["materials"]["Ammo"] = sorted(mi.get_name() for mi in mis.values())
			for t in static_tasks:
				path = f"{t.destination_path}/{t.destination_name}"
				mesh = load(path)
				(report["imported"] if mesh else report["failed"]).append(path)
				assign(mesh, mis)
			continue

		cfg = WEAPONS[name]
		base = bases(name)
		if not base["base"]:
			report["failed"].append(f"{name}: no weapon skeletal mesh in the manifest")
			unreal.log_error(f"[pirate_weapons] {name}: no weapon base in the manifest, skipped")
			continue
		# 1. bases: the weapon with its own skeleton, the FP arms on the project's bean skeleton (where the manifest lists
		# them; exports/Bases for the packs of the weapon pack format)
		base_fbx = asset_file(name, base["base"], os.path.join("exports", "Bases", base["base"] + ".fbx"))
		tools.import_asset_tasks([skeletal_task(base_fbx, dest(name), base["base"], None)])
		weapon_mesh = load(f"{dest(name)}/{base['base']}")
		weapon_skeleton = weapon_mesh.get_editor_property("skeleton") if weapon_mesh else None
		if not weapon_skeleton:
			report["failed"].append(f"{base_fbx}: no skeletal mesh / skeleton")
			unreal.log_error(f"[pirate_weapons] {name}: base import failed, its clips are skipped")
			continue
		report["imported"].append(f"{dest(name)}/{base['base']}")
		fp_mesh = None
		if base["fp"]:
			fp_fbx = asset_file(name, base["fp"], os.path.join("exports", "Bases", base["fp"] + ".fbx"))
			tools.import_asset_tasks([skeletal_task(fp_fbx, dest(name), base["fp"], bean_skeleton)])
			fp_mesh = load(f"{dest(name)}/{base['fp']}")
			if fp_mesh and fp_mesh.get_editor_property("skeleton").get_path_name() == bean_skeleton.get_path_name():
				report["imported"].append(f"{dest(name)}/{base['fp']}")
			else:
				report["failed"].append(f"{fp_fbx}: not on {BEAN_SKELETON}")

		# 2. clips: bean on the project's skeleton, weapon on its own
		fps = fps_of(name)
		tasks = []
		for fbx, folder, clip_asset, role, _, _ in animation_plan(name):
			tasks.append((role, animation_task(fbx, folder, clip_asset, bean_skeleton if role == "bean" else weapon_skeleton, fps)))
		tools.import_asset_tasks([t for _, t in tasks])
		for role, t in tasks:
			path = f"{t.destination_path}/{t.destination_name}"
			seq = load(path)
			expected = bean_skeleton if role == "bean" else weapon_skeleton
			if isinstance(seq, unreal.AnimSequence) and seq.get_editor_property("skeleton").get_path_name() == expected.get_path_name():
				report["imported"].append(path)
			else:
				report["failed"].append(f"{t.filename} -> {path}")
				unreal.log_error(f"[pirate_weapons] FAILED {t.filename}")

		# 3. statics: the pickup mesh keeps a collision (pickups, T-157), not the shells and the tokens
		# the ammunition models of the T-334 packs lie on the ground too (DA_Ammo_* pickups): a collision, or they fall
		# through the ground (the "pickupcollision" fix of 2026-10-10)
		views = {asset_name(e): e.get("view") for e in manifest(name)["assets"] if e["type"] == "static_mesh"}
		static_tasks = [static_task(asset_file(name, s, os.path.join("exports", "Static", s + ".fbx")), dest(name, "Static"), s,
			pickup or bool(cfg.get("art_pack") and views.get(s) == "ammo")) for s, pickup in statics(name)]
		tools.import_asset_tasks(static_tasks)
		static_meshes = []
		for t in static_tasks:
			path = f"{t.destination_path}/{t.destination_name}"
			mesh = load(path)
			(report["imported"] if mesh else report["failed"]).append(path)
			static_meshes.append(mesh)

		# 4. PBR materials of the pack on every imported mesh (slot by imported material name)
		mis = material_instances(name, cfg["asset"])
		report["materials"][name] = {"instances": sorted(mi.get_name() for mi in mis.values()), "slots": {}}
		for mesh in [weapon_mesh, fp_mesh] + static_meshes:
			if mesh:
				report["materials"][name]["slots"][mesh.get_name()] = assign(mesh, mis)

		# 5. checks: bean root scale against the project's clips, weapon size against the manifest
		idle_clip = cfg.get("idle", "Idle")
		idle_path = clip_assets(name).get(idle_clip, {}).get("bean_third_person")
		idle = load(idle_path) if idle_path else None
		bounds = manifest(name).get("weapon_contract", {}).get("bounds", {}).get("size")
		mesh_size = None
		if weapon_mesh:
			extent = weapon_mesh.get_bounds().box_extent
			mesh_size = [round(extent.x * 2, 1), round(extent.y * 2, 1), round(extent.z * 2, 1)]
		report["checks"][name] = {
			"bean_root_scale": root_scale(idle) if idle else f"{idle_path or idle_clip} missing",
			"bean_root_scale_clip": idle_path,
			"project_bean_root_scale": reference_scale,
			"weapon_mesh_size_cm": mesh_size,
			"manifest_weapon_size_cm": [round(v * 100, 1) for v in bounds] if bounds else None,
			# bones of the weapon skeleton the effects attach to (the game falls back to the mesh origin when missing)
			"muzzle_sockets_expected": cfg.get("muzzle_sockets", []),
		}
		check = report["checks"][name]
		if isinstance(check["bean_root_scale"], list) and isinstance(reference_scale, list) \
				and any(abs(a - b) > 0.01 * max(abs(b), 1.0) for a, b in zip(check["bean_root_scale"], reference_scale)):
			unreal.log_error(f"[pirate_weapons] {name}: bean root scale {check['bean_root_scale']} != project {reference_scale}: "
				"the clips would resize the bean, see the docstring")

	out = os.path.join(unreal.Paths.project_saved_dir(), "MonkeTools", "pirate_weapons_report.json")
	os.makedirs(os.path.dirname(out), exist_ok=True)
	with open(out, "w", encoding="utf-8") as f:
		json.dump(report, f, indent=1, ensure_ascii=False)
	unreal.log(f"[pirate_weapons] imported {len(report['imported'])}, failed {len(report['failed'])}, skipped packs {report['skipped']} -> {out}")


if __name__ == "__main__":
	if "--check" in sys.argv:
		sys.exit(check())
	try:
		import unreal  # noqa: F401
	except ImportError:
		sys.exit("run inside Unreal (commandlet -run=pythonscript) or with --check")
	run_in_unreal()
