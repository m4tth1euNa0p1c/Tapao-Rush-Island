"""Probe: material slots and LOD sections of the reimported coconut mobs. Writes monke_island/Saved/T186/probe-sections.json."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sub = ue.get_editor_subsystem(ue.SkeletalMeshEditorSubsystem)
out = {}
for path in ("/Game/MonkeIsland/Characters/CocoWarrior/Meshes/SK_CocoWarrior", "/Game/MonkeIsland/Characters/CocoWarrior/Meshes/SK_CocoWarrior_Spear",
             "/Game/MonkeIsland/Characters/CocoWarrior/Dismember/SK_CocoWarrior_Head", "/Game/MonkeIsland/Characters/Villager/Meshes/SK_Villager"):
    mesh = ue.EditorAssetLibrary.load_asset(path)
    slots = []
    for i, slot in enumerate(mesh.get_editor_property("materials")):
        mat = slot.get_editor_property("material_interface")
        slots.append([i, str(slot.get_editor_property("material_slot_name")), str(slot.get_editor_property("imported_material_slot_name")),
                      mat.get_name() if mat else None])
    lods = []
    for lod in range(sub.get_lod_count(mesh)):
        lods.append([sub.get_lod_material_slot(mesh, lod, s) for s in range(sub.get_num_sections(mesh, lod))])
    out[path.rsplit("/", 1)[-1]] = {"slots": slots, "lod_sections_to_slot": lods,
                                    "source": str(mesh.get_editor_property("asset_import_data").get_first_filename())}
(ROOT / "monke_island/Saved/T186/probe-sections.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
