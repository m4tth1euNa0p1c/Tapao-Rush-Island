"""T-198: the warehouse route of the fort (L_Test_Flat) stopped 3.4 m before the door: the jute of SM_Port_Warehouse
(a fabric curtain hanging in the doorway down to about 2.05 m, plus awnings) collides with the bean's capsule (110 cm
standing on the 15 cm threshold: its top at about 2.26 m). The meshes use their complex collision (CTF_USE_COMPLEX_AS_SIMPLE),
so the fabric is turned into decoration: collision off on the jute sections of SM_Port_Warehouse. The other fort meshes
with a jute section are only listed (no route of theirs is blocked).
Lead session action "fortjute" (no game running). Report: monke_island/Saved/T198/fort-jute.json. Replays change nothing.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T198/fort-jute.json"
FOLDER = "/Game/MonkeIsland/Environment/FortFrench/Revisions/T198/Meshes"
TARGETS = {"SM_Port_Warehouse"}


def jute_sections(mesh):
    """(section index, slot name) of the LOD 0 sections drawn with a jute material slot"""
    sub = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    slots = mesh.get_editor_property("static_materials")
    found = []
    for section in range(mesh.get_num_sections(0)):
        slot_index = sub.get_lod_material_slot(mesh, 0, section)
        slot = slots[slot_index] if 0 <= slot_index < len(slots) else None
        name = str(slot.get_editor_property("material_slot_name")) if slot else ""
        if "jute" in name.lower():
            found.append((section, name))
    return found


def main():
    sub = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)
    report = {"changed": {}, "listed": {}}
    for path in ue.EditorAssetLibrary.list_assets(FOLDER, recursive=False):
        mesh = ue.EditorAssetLibrary.load_asset(path)
        if not isinstance(mesh, ue.StaticMesh):
            continue
        name = mesh.get_name()
        sections = jute_sections(mesh)
        if not sections:
            continue
        states = {index: sub.is_section_collision_enabled(mesh, 0, index) for index, _ in sections}
        if name not in TARGETS:
            report["listed"][name] = [{"section": i, "slot": s, "collision": states[i]} for i, s in sections]
            continue
        changed = []
        for index, slot in sections:
            if states[index]:
                sub.enable_section_collision(mesh, False, 0, index)
                changed.append({"section": index, "slot": slot})
        entry = {"turned_off": changed, "after": {i: sub.is_section_collision_enabled(mesh, 0, i) for i, _ in sections}}
        if changed:
            entry["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False))
        report["changed"][name] = entry
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"FORT_JUTE changed={list(report['changed'])} listed={list(report['listed'])}")


main()
