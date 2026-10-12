"""The v2 weapons and their ammunition pictured in the inventory and the shop (owner 2026-10-09, T-264).

Imports the renders of scripts/ui/render_weapon_icons.py (art/ui/icons/weapons/T_Icon_*.png, the pickup models of the
packs) as UI textures in /Game/MonkeIsland/UI/Icons/Weapons, then:
  - DA_Item_<Weapon>.Icon (inventory cell, shop row); the shop's musket (DA_Item_Musket) also lies on the ground as the
    v2 model (PickupMesh SM_Bean_PirateMusket);
  - DA_Ammo_*.Icon after the model of the packet (PickupMesh SM_Ammo_X -> T_Icon_SM_Ammo_X);
  - the ammunition lines of DA_ShopCatalog described in French ("Giberne : 15 balles de mousquet").
Lead session action "weaponicons". Report: monke_island/Saved/T264/weapon-icons.json. Replays replace the textures.
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
SRC = ROOT / "art/ui/icons/weapons"
OUT = ROOT / "monke_island/Saved/T264/weapon-icons.json"
DEST = "/Game/MonkeIsland/UI/Icons/Weapons"
WEAPONS = "/Game/MonkeIsland/Weapons"
EAL = ue.EditorAssetLibrary
# DA_Item_* -> icon
ITEMS = {
    f"{WEAPONS}/Musket/DA_Item_Musket": "Musket",
    f"{WEAPONS}/Musket/DA_Item_PirateMusket": "Musket",
    f"{WEAPONS}/Pistol/DA_Item_Pistol": "Pistol",
    f"{WEAPONS}/DoublePistol/DA_Item_DoublePistol": "DoublePistol",
    f"{WEAPONS}/Shotgun/DA_Item_Shotgun": "Shotgun",
    f"{WEAPONS}/Blunderbuss/DA_Item_Blunderbuss": "Blunderbuss",
    f"{WEAPONS}/Rifle/DA_Item_Rifle": "Rifle",
    f"{WEAPONS}/Grenade/DA_Item_Grenade": "Grenade",
    # T-273: the crank machine gun (its render once the pack of T-272 has its pickup model)
    f"{WEAPONS}/MachineGun/DA_Item_MachineGun": "MachineGun",
}
V2_MUSKET_MESH = f"{WEAPONS}/Musket/Static/SM_Bean_PirateMusket"
CATALOG = "/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog"
# DA_Ammo_* -> model of the packet (several had no PickupMesh: a placeholder box lay on the ground)
AMMO_MESHES = {
    "DA_Ammo_Barrel": "Ammo/Meshes/SM_Ammo_PowderKeg",
    "DA_Ammo_Crate": "Ammo/Meshes/SM_Ammo_CrateMixed",
    "DA_Ammo_GrenadeBag": "Ammo/Meshes/SM_Ammo_GrenadeBag",
    "DA_Ammo_ShotBag": "Blunderbuss/Static/SM_Ammo_ShotBag",
    "DA_Ammo_CartridgeBox": "Musket/Static/SM_Ammo_CartridgeBox",
    "DA_Ammo_Cartridges": "Musket/Static/SM_Ammo_MusketCartridge",
    "DA_Ammo_PowderHorn": "Musket/Static/SM_Ammo_PowderHorn",
    "DA_Ammo_Pouch": "Pistol/Static/SM_Ammo_BallPouch",
    "DA_Ammo_RifleCartridges": "Rifle/Static/SM_Ammo_RifleCartridge",
    "DA_Ammo_ShellBox": "Shotgun/Static/SM_Ammo_ShellBox",
}
# ammunition kind (enum name, any spelling) -> French plural
KINDS = {"musketball": "balles de mousquet", "pistolcharge": "charges de pistolet", "shot": "charges de plombs",
         "riflecartridge": "cartouches de carabine", "shell": "cartouches de fusil à pompe", "grenade": "grenades",
         "machinegunbelt": "cartouches de mitrailleuse"}


def import_textures():
    tasks = []
    for png in sorted(SRC.glob("T_Icon_*.png")):
        task = ue.AssetImportTask()
        task.filename = str(png)
        task.destination_path = DEST
        task.destination_name = png.stem
        task.replace_existing = True
        task.automated = True
        task.save = False
        tasks.append(task)
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    textures = {}
    for task in tasks:
        name = Path(task.filename).stem
        texture = EAL.load_asset(f"{DEST}/{name}")
        if not texture:
            continue
        # a picture of the interface: no mips, never streamed, full quality
        texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_EDITOR_ICON)
        texture.set_editor_property("lod_group", ue.TextureGroup.TEXTUREGROUP_UI)
        texture.set_editor_property("mip_gen_settings", ue.TextureMipGenSettings.TMGS_NO_MIPMAPS)
        texture.set_editor_property("never_stream", True)
        texture.set_editor_property("srgb", True)
        EAL.save_loaded_asset(texture, only_if_is_dirty=False)
        textures[name] = texture
    return textures


def kind_name(kind):
    """EAmmoKind as Python prints it (<AmmoKind.MUSKET_BALL: 1>) -> French plural"""
    import re
    match = re.search(r"([A-Z][A-Z_]+)", str(kind).split(".")[-1])
    key = (match.group(1) if match else str(kind)).replace("_", "").lower()
    return KINDS.get(key, key)


def main():
    textures = import_textures()
    report = {"textures": sorted(textures), "items": {}, "ammo": {}, "catalog": []}
    for path, icon in ITEMS.items():
        item = EAL.load_asset(path)
        texture = textures.get(f"T_Icon_{icon}")
        if not item or not texture:
            report["items"][path] = "missing item or icon"
            continue
        item.set_editor_property("icon", texture)
        if path.endswith("DA_Item_Musket"):
            mesh = EAL.load_asset(V2_MUSKET_MESH)
            if mesh:
                item.set_editor_property("pickup_mesh", mesh)
        EAL.save_loaded_asset(item, only_if_is_dirty=False)
        report["items"][path.rsplit("/", 1)[1]] = texture.get_name()
    ammo_names = {}
    for path in EAL.list_assets(f"{WEAPONS}/Ammo", recursive=False, include_folder=False):
        name = path.split(".")[0].rsplit("/", 1)[1]
        if not name.startswith("DA_Ammo_"):
            continue
        ammo = EAL.load_asset(path.split(".")[0])
        if ammo and name in AMMO_MESHES:
            model = EAL.load_asset(f"{WEAPONS}/{AMMO_MESHES[name]}")
            if model:
                ammo.set_editor_property("pickup_mesh", model)
        mesh = ammo.get_editor_property("pickup_mesh") if ammo else None
        texture = textures.get(f"T_Icon_{mesh.get_name()}") if mesh else None
        if texture:
            ammo.set_editor_property("icon", texture)
            EAL.save_loaded_asset(ammo, only_if_is_dirty=False)
        lots = [(kind_name(lot.get_editor_property("kind")), lot.get_editor_property("count")) for lot in ammo.get_editor_property("lots")] if ammo else []
        ammo_names[name] = (str(ammo.get_editor_property("display_name")) if ammo else name, lots)
        report["ammo"][name] = texture.get_name() if texture else f"no icon for {mesh.get_name() if mesh else None}"
    catalog = EAL.load_asset(CATALOG)
    if catalog:
        offers = list(catalog.get_editor_property("offers"))
        for offer in offers:
            ammo = offer.get_editor_property("ammo")
            if not ammo or ammo.get_name() not in ammo_names:
                continue
            display, lots = ammo_names[ammo.get_name()]
            text = f"{display} : " + " + ".join(f"{count} {kind}" for kind, count in lots)
            offer.set_editor_property("description", text)
            report["catalog"].append(text)
        catalog.set_editor_property("offers", offers)
        EAL.save_loaded_asset(catalog, only_if_is_dirty=False)
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding="utf-8")
    ue.log(f"[weapon_icons] {len(textures)} textures, {len(report['items'])} items, {len(report['ammo'])} ammo")


main()
