"""T-249: the new weapons and the ammunition packets lying on the test map, to try them in PIE (owner 2026-10-09).

On L_Test_Flat (loaded if another map is open), around the first PlayerStart (or the origin), in the folder
WeaponsTest with the tag T249_Weapons, idempotent by label:
  Pickup_<Weapon>   AInventoryPickup of DA_Item_<Asset> (Blunderbuss, Rifle, DoublePistol, Grenade, and the Pistol,
                    Shotgun and PirateMusket to try the ammunition and the throw of an empty weapon)
  Ammo_<Name>       BP_Ammo_<Name> (AAmmoPickup) of every DA_Ammo_* that setup_pirate_weapons.py made
Run in the editor (editor lock held, no PIE): Tools > Execute Python Script, or from the lead session:
  python scripts/editor/weapons_test_map_ue.py        (through the editor's Python, e.g. lead_session_t186-like runner)
Writes monke_island/Saved/T249/weapons-test-map.json. The level is saved. Removing the actors: delete the folder
WeaponsTest (or the actors tagged T249_Weapons) in the outliner.
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

MAP = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
WEAPONS = "/Game/MonkeIsland/Weapons"
FOLDER = "WeaponsTest"
TAG = "T249_Weapons"
OUT = ROOT / "monke_island/Saved/T249/weapons-test-map.json"
# weapon folder, item asset
ITEMS = [("Blunderbuss", "DA_Item_Blunderbuss"), ("Rifle", "DA_Item_Rifle"), ("DoublePistol", "DA_Item_DoublePistol"),
	("Grenade", "DA_Item_Grenade"), ("Pistol", "DA_Item_Pistol"), ("Shotgun", "DA_Item_Shotgun"), ("Musket", "DA_Item_PirateMusket")]
SPACING = 120.0


def main():
	kitlib.check_lock()
	levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
	world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
	if not world or "L_Test_Flat" not in world.get_path_name():
		assert levels.load_level(MAP), f"cannot load {MAP}"
		world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
	actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
	existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
	start = next((a for a in existing.values() if isinstance(a, ue.PlayerStart)), None)
	origin = start.get_actor_location() if start else ue.Vector(0.0, 0.0, 100.0)
	forward = start.get_actor_forward_vector() if start else ue.Vector(1.0, 0.0, 0.0)
	right = start.get_actor_right_vector() if start else ue.Vector(0.0, 1.0, 0.0)
	result = {"map": MAP, "origin": [origin.x, origin.y, origin.z], "spawned": [], "kept": [], "missing": []}

	def place(label, cls, index, row, configure):
		location = origin + forward * (250.0 + row * 150.0) + right * ((index - 3) * SPACING) + ue.Vector(0.0, 0.0, 40.0)
		actor = existing.get(label)
		if actor:
			result["kept"].append(label)
		else:
			actor = actors.spawn_actor_from_class(cls, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
			assert actor, label
			actor.set_actor_label(label)
			result["spawned"].append(label)
		actor.set_folder_path(FOLDER)
		actor.set_editor_property("tags", [TAG])
		configure(actor)
		return actor

	for index, (weapon, item_name) in enumerate(ITEMS):
		path = f"{WEAPONS}/{weapon}/{item_name}"
		item = kitlib.load(path)
		if not item:
			result["missing"].append(path)
			continue
		place(f"Pickup_{weapon}", ue.InventoryPickup, index, 0, lambda a, item=item: a.set_editor_property("definition", item))

	ammo_folder = f"{WEAPONS}/Ammo"
	ammo = []
	if ue.EditorAssetLibrary.does_directory_exist(ammo_folder):
		for asset_path in ue.EditorAssetLibrary.list_assets(ammo_folder, recursive=False, include_folder=False):
			name = asset_path.split("/")[-1].split(".")[0]
			if name.startswith("BP_Ammo_"):
				ammo.append((name, asset_path.split(".")[0]))
	for index, (name, bp_path) in enumerate(sorted(ammo)):
		cls = ue.EditorAssetLibrary.load_blueprint_class(bp_path)
		if not cls:
			result["missing"].append(bp_path)
			continue
		place(f"Ammo_{name[len('BP_Ammo_'):]}", cls, index, 1 + index // 7, lambda a: None)

	assert levels.save_current_level()
	result["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
	OUT.parent.mkdir(parents=True, exist_ok=True)
	OUT.write_text(json.dumps(result, indent=1, ensure_ascii=False), encoding="utf-8")
	ue.log(f"T249_WEAPONS_TEST_MAP_SUCCESS {OUT}")


if __name__ == "__main__":
	main()
