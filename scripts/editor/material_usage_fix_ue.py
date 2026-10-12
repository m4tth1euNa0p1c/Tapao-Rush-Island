"""T-270: the materials the packaged build drew with the default material ("missing usage flag Nanite / Instanced
Static Meshes! Default Material will be used in game", logs of the builds played from 2026-10-07 to 2026-10-10):
every M_Galleon_* of the galleon used in the game gets "Used with Nanite", the village butterflies and fireflies
"Used with Instanced Static Meshes"; recompiled and saved. Lead session action "matusage".
Report: monke_island/Saved/T270/material-usage.json"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T270/material-usage.json"
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary
GALLEON = "/Game/MonkeIsland/Galleon/TestFlat/Materials"
INSTANCED = ["/Game/MonkeIsland/Environment/Village/Materials/M_Village_Butterfly",
             "/Game/MonkeIsland/Environment/Village/Materials/MI_Village_Firefly"]


def base_material(asset):
    """The UMaterial under a material instance chain"""
    while isinstance(asset, ue.MaterialInstance):
        asset = asset.get_editor_property("parent")
    return asset if isinstance(asset, ue.Material) else None


def flag(material, prop, report):
    name = material.get_path_name().split(".")[0]
    before = material.get_editor_property(prop)
    if not before:
        material.set_editor_property(prop, True)
        MEL.recompile_material(material)
    # saved in every case: the editor sets these usages by itself in memory when it draws the mesh
    # (bAutomaticallySetUsageInEditor) and never saves them, so the file on disk, and the cook, lacked them
    saved = EAL.save_loaded_asset(material, only_if_is_dirty=False)
    report.setdefault(prop, {})[name] = ("in memory only, now saved" if before else "set") if saved else "SAVE FAILED"


report = {}
for path in EAL.list_assets(GALLEON, recursive=False, include_folder=False):
    asset = EAL.load_asset(path.split(".")[0])
    if isinstance(asset, ue.Material):
        flag(asset, "used_with_nanite", report)
for path in INSTANCED:
    material = base_material(EAL.load_asset(path))
    if material:
        flag(material, "used_with_instanced_static_meshes", report)
    else:
        report.setdefault("missing", []).append(path)
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
