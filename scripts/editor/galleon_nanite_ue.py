"""Nanite on the galleon's meshes (owner 2026-10-07: "le jeu lag lors des cinématiques"; every scene of the day frames
the galleon, whose six meshes were the only big non-Nanite ones of the expedition maps: "[VSM] Non-Nanite Marking Job
Queue overflow"). Reports the blend mode of every material first; a translucent one keeps its mesh as it is. Dry run
unless monke_island/Saved/T186/galleonnanite-apply.txt exists. Writes monke_island/Saved/T186/galleon-nanite.json.
Action "galleonnanite"."""
import json
import sys
import time
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

APPLY = (ROOT / "monke_island/Saved/T186/galleonnanite-apply.txt").exists()
out = {"apply": APPLY}
kitlib.check_lock()
rules = kitlib.load("/Game/MonkeIsland/Galleon/DA_GalleonRules")
meshes = []
for p in ("hull_mesh", "sails_mesh", "rigging_mesh", "flag_mesh", "wheel_mesh", "rudder_mesh"):
    m = rules.get_editor_property(p)
    if m and m not in meshes:
        meshes.append(m)
sms = ue.get_editor_subsystem(ue.StaticMeshEditorSubsystem)


def base_blend(mat):
    while isinstance(mat, ue.MaterialInstance):
        mat = mat.get_editor_property("parent")
    return str(mat.get_editor_property("blend_mode")).split(".")[-1] if mat else None


for mesh in meshes:
    row = {"path": mesh.get_path_name(), "slots": []}
    for slot in mesh.get_editor_property("static_materials"):
        mi = slot.get_editor_property("material_interface")
        row["slots"].append([str(slot.get_editor_property("material_slot_name")), mi.get_name() if mi else None, base_blend(mi) if mi else None])
    settings = mesh.get_editor_property("nanite_settings")
    row["nanite_before"] = bool(settings.get_editor_property("enabled"))
    translucent = any(s[2] and "TRANSLUCENT" in s[2].upper() for s in row["slots"])
    row["translucent"] = translucent
    if APPLY and not translucent and not row["nanite_before"]:
        start = time.time()
        settings.set_editor_property("enabled", True)
        sms.set_nanite_settings(mesh, settings, True)
        row["nanite_after"] = bool(mesh.get_editor_property("nanite_settings").get_editor_property("enabled"))
        row["seconds"] = round(time.time() - start, 1)
        row["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(mesh, False))
    out.setdefault("meshes", []).append(row)
(ROOT / "monke_island/Saved/T186/galleon-nanite.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
