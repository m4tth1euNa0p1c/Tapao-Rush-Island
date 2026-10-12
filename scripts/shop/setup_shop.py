"""Merchant of the galleon (owner request 2026-10-06, ADR-007 §8): DA_ShopCatalog and 15 rounds per firearm.

Creates or updates /Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog (musket, pistol, shotgun, torch with their prices) and
sets MaxBalls / MaxCharges / MaxShells to 15 in the firearm DataAssets (empty, the weapon stays and reloads: T-329).
Editor closed (commandlet, lock held):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash -nullrhi -nosourcecontrol
Or in the editor through the Python console. Prices are starting values to test.
"""
import unreal

FOLDER = "/Game/MonkeIsland/Galleon/Shop"
NAME = "DA_ShopCatalog"
ROUNDS = 15

OFFERS = [
	("/Game/MonkeIsland/Weapons/Musket/DA_Item_Musket", 400, "Mousquet : 15 balles, se recharge avec ses munitions"),
	("/Game/MonkeIsland/Weapons/Pistol/DA_Item_Pistol", 250, "Pistolet : 15 charges"),
	("/Game/MonkeIsland/Weapons/Shotgun/DA_Item_Shotgun", 600, "Fusil à pompe : 15 cartouches"),
	("/Game/MonkeIsland/Weapons/Torch/DA_Item_Torch", 120, "Torche : éclaire la nuit"),
]

AMMO = [
	("/Game/MonkeIsland/Weapons/Musket/DA_PirateMusketRules", "max_balls"),
	("/Game/MonkeIsland/Weapons/Pistol/DA_PistolRules", "max_charges"),
	("/Game/MonkeIsland/Weapons/Shotgun/DA_ShotgunRules", "max_shells"),
]

eal = unreal.EditorAssetLibrary
path = f"{FOLDER}/{NAME}"
if eal.does_asset_exist(path):
	catalog = eal.load_asset(path)
else:
	eal.make_directory(FOLDER)
	catalog = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.ShopCatalog, unreal.DataAssetFactory())

offers = []
for item_path, price, line in OFFERS:
	item = eal.load_asset(item_path)
	if not item:
		unreal.log_error(f"[shop] missing {item_path}")
		continue
	offer = unreal.ShopOffer()
	offer.set_editor_property("item", item)
	offer.set_editor_property("price", price)
	offer.set_editor_property("description", line)
	offers.append(offer)
catalog.set_editor_property("offers", offers)
eal.save_loaded_asset(catalog)
unreal.log(f"[shop] {path}: {len(offers)} offers")

for rules_path, prop in AMMO:
	rules = eal.load_asset(rules_path)
	if not rules:
		unreal.log_error(f"[shop] missing {rules_path}")
		continue
	rules.set_editor_property(prop, ROUNDS)
	# owner 2026-10-11 (T-329): the normal reload, no weapon thrown away once empty
	rules.set_editor_property("discard_when_empty", False)
	eal.save_loaded_asset(rules)
	unreal.log(f"[shop] {rules_path}: {prop} = {ROUNDS}")
