"""T-213: sets every material slot of the seven French bean uniforms to its MI (MI_BeanFR_<material>) and checks it held.

The import (import_bean_fr_ue.py) set the slots right after the import, while the skeletal meshes were still building:
the assignment was lost and the uniforms showed the default grid (same pitfall as the player's bean, bean_materials_ue.py).
This runs apart, once the meshes are built, and reads the slots back from the saved meshes.
Runs in the editor: scripts/editor/lead_session_t186.py, action "beanfrmats".
Result: monke_island/Saved/T213/beanfr-materials.json
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T213"
MESHES = "/Game/MonkeIsland/Characters/BeanFR/Meshes"
MATS = "/Game/MonkeIsland/Characters/BeanFR/Materials"
SKINS = ("CFM_Fusilier", "Fusilier_1767", "Fusilier_Bicorne", "Chasseur_Shako", "Grenadier", "Sergent", "Officier_Marine")


def short_name(name):
    short = name.replace("MAT-", "").replace("MAT_", "")
    return short[len("BeanFR_"):] if short.startswith("BeanFR_") else short


def main():
    kitlib.check_lock()
    report = {}
    for name in SKINS:
        path = f"{MESHES}/SK_BeanFR_{name}"
        mesh = kitlib.load(path)
        assert mesh, path
        slots = mesh.get_editor_property("materials")
        missing = []
        for slot in slots:
            slot_name = str(slot.get_editor_property("material_slot_name"))
            mi = kitlib.load(f"{MATS}/MI_BeanFR_{short_name(slot_name)}")
            if not mi:
                missing.append(slot_name)
                continue
            slot.set_editor_property("material_interface", mi)
        mesh.modify()
        mesh.set_editor_property("materials", slots)
        kitlib.save(mesh)
        held = [s.get_editor_property("material_interface") is not None for s in kitlib.load(path).get_editor_property("materials")]
        report[name] = {"slots": len(slots), "set": sum(held), "missing": missing}
        assert all(held), f"{name}: slots without material after the save: {missing}"
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "beanfr-materials.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    ue.log("T213_BEANFR_MATERIALS_DONE")


if __name__ == "__main__":
    main()
