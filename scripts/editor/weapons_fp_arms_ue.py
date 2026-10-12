"""The first person arms of the weapon packs wear the bean's materials (owner 2026-10-09, weapons v2).

The packs export the FP arms (SK_Bean_<Weapon>_FP) with the bean's material NAMES (M_Bean_Skin, M_Bean_Navy...) but no
textures: the import gives them flat placeholder materials (light blue hands, grid cream). This puts the bean's own
instances (/Game/MonkeIsland/Characters/Bean/Materials/MI_Bean_<Part>) on every slot whose imported name is M_Bean_<Part>,
like SK_Bean_Musket_FP already has, and saves the meshes. Also saves the rules assets tuned in memory during the PIE
checks (DA_PirateMusketRules.AimRotation, DA_GrenadeRules.FirstPersonAlignBone).
Lead session action "fparms". Report: monke_island/Saved/T186/fp-arms.json.
"""
import json
import re
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T186/fp-arms.json"
EAL = ue.EditorAssetLibrary
WEAPONS = "/Game/MonkeIsland/Weapons"
BEAN_MATERIALS = "/Game/MonkeIsland/Characters/Bean/Materials"
ARMS = {"Pistol": "SK_Bean_Pistol_FP", "DoublePistol": "SK_Bean_DoublePistol_FP", "Shotgun": "SK_Bean_Shotgun_FP", "Musket": "SK_Bean_Musket_FP",
        "Blunderbuss": "SK_Bean_Blunderbuss_FP", "Rifle": "SK_Bean_Rifle_FP", "Grenade": "SK_Bean_Grenade_FP",
        "MachineGun": "SK_Bean_MachineGun_FP"}
RULES = [f"{WEAPONS}/Musket/DA_PirateMusketRules", f"{WEAPONS}/Grenade/DA_GrenadeRules"]


def main():
    report = {}
    for weapon, asset in ARMS.items():
        path = f"{WEAPONS}/{weapon}/{asset}"
        mesh = EAL.load_asset(path)
        if not mesh:
            report[weapon] = "missing"
            continue
        # the array's items are copies (UE 5.8): edit the copies, then set the whole array back
        slots = list(mesh.get_editor_property("materials"))
        rows = []
        for slot in slots:
            imported = str(slot.get_editor_property("imported_material_slot_name"))
            # a pack may suffix a duplicated material (M_Bean_Brass_001)
            base = re.sub(r"_\d{3}$", "", imported)
            if base.startswith("M_Bean_"):
                candidate = f"{BEAN_MATERIALS}/MI_Bean_{base[len('M_Bean_'):]}"
                mi = EAL.load_asset(candidate) if EAL.does_asset_exist(candidate) else None
                if mi:
                    slot.set_editor_property("material_interface", mi)
                rows.append([imported, mi.get_name() if mi else "no MI"])
        mesh.set_editor_property("materials", slots)
        saved = EAL.save_asset(path, only_if_is_dirty=False)
        check = [[str(s.get_editor_property("imported_material_slot_name")), s.get_editor_property("material_interface").get_name() if s.get_editor_property("material_interface") else None]
                 for s in mesh.get_editor_property("materials")]
        report[weapon] = {"set": rows, "saved": saved, "check": check}
    for path in RULES:
        report[path.rsplit("/", 1)[1]] = EAL.save_asset(path, only_if_is_dirty=False)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")


main()
