"""Deletes the first import of the redesigned musket (T-170) before the import of the fixed pack.

The first import read the OneDrive pack whose armature is "Armature.001": the weapon skeleton (SK_Bean_PirateMusket_Skeleton)
got an extra root bone Armature_001, the FP arms mesh and the clips carry it too. Replacing them in place would merge
into the wrong skeleton, so they are deleted, then import_pirate_weapons.py (fixed copy in build/packs) and
setup_pirate_weapons.py rebuild them and DA_PirateMusketRules. The bean skeleton and the old musket are not touched.

Editor closed (commandlet, lock held):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash -nullrhi -nosourcecontrol
"""
import unreal

FOLDER = "/Game/MonkeIsland/Weapons/Musket"
eal = unreal.EditorAssetLibrary
# the clips first: a clip whose skeleton is already gone cannot be loaded, hence not deleted
targets = []
for sub, prefix in (("Animations/Bean", "A_Bean_Musket_"), ("Animations/Weapon", "A_Musket_")):
	for path in eal.list_assets(f"{FOLDER}/{sub}", recursive=False):
		name = path.split("/")[-1].split(".")[0]
		if name.startswith(prefix + "TP_") or name.startswith(prefix + "FP_"):
			targets.append(f"{FOLDER}/{sub}/{name}")
targets += [f"{FOLDER}/SK_Bean_Musket_FP", f"{FOLDER}/SK_Bean_PirateMusket", f"{FOLDER}/SK_Bean_PirateMusket_PhysicsAsset",
	f"{FOLDER}/SK_Bean_Musket_FP_PhysicsAsset", f"{FOLDER}/SK_Bean_PirateMusket_Skeleton"]

deleted = 0
for path in targets:
	if not eal.does_asset_exist(path):
		continue
	try:
		ok = eal.delete_asset(path)
	except RuntimeError as error:
		ok = False
		unreal.log_error(f"[pirate_musket_clean] {path}: {error}")
	if ok:
		deleted += 1
	else:
		unreal.log_error(f"[pirate_musket_clean] could not delete {path} (editor closed: delete its .uasset by hand)")
unreal.log(f"[pirate_musket_clean] {deleted} assets deleted")
