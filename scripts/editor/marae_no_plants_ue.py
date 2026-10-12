"""T-144: palms and bushes out of the marae (owner's request). DA_MaraeRules still lays 115 pieces per marae in every
build, 41 of them vegetation meshes (/Game/MonkeIsland/Environment/Vegetation/...): T-208 had removed them only from the
Test Flat copy (DA_TestFlat_Marae_NoPlants, 115 -> 74 pieces).

The pieces of DA_MaraeRules whose mesh lives under /Game/MonkeIsland/Environment/Vegetation/ are removed and the asset
saved; every removed piece (mesh and transform) is written to the report, enough to put it back. The other UMaraeRules
assets are only listed with their vegetation count (a temple keeps its own look).
Lead session action "maraeplants" (no game running). Report: monke_island/Saved/T144/marae-no-plants.json.
Replays change nothing once the vegetation is out. Proof in game: the log line "marae of 74 pieces".
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T144/marae-no-plants.json"
TARGET = "/Game/MonkeIsland/Environment/Marae/DA_MaraeRules"
VEGETATION = "/Game/MonkeIsland/Environment/Vegetation/"


def describe(piece):
    out = {}
    for name in ("mesh", "transform", "location", "rotation", "scale"):
        try:
            value = piece.get_editor_property(name)
        except Exception:  # noqa: BLE001
            continue
        out[name] = value.get_path_name() if hasattr(value, "get_path_name") else str(value)
    return out


def vegetation(piece):
    mesh = piece.get_editor_property("mesh")
    return bool(mesh) and mesh.get_path_name().startswith(VEGETATION)


def main():
    report = {"target": TARGET, "others": {}}
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path("/Game/MonkeIsland", recursive=True):
        if str(data.asset_class_path.asset_name) != "MaraeRules":
            continue
        path = str(data.package_name)
        rules = ue.EditorAssetLibrary.load_asset(path)
        pieces = list(rules.get_editor_property("pieces"))
        plants = [p for p in pieces if vegetation(p)]
        if path != TARGET:
            report["others"][path] = {"pieces": len(pieces), "vegetation": len(plants)}
            continue
        report["before"] = len(pieces)
        report["removed"] = [describe(p) for p in plants]
        if plants:
            kept = [p for p in pieces if not vegetation(p)]
            rules.set_editor_property("pieces", kept)
            report["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(rules, only_if_is_dirty=False))
        report["after"] = len(rules.get_editor_property("pieces"))
    if "before" not in report:
        report["error"] = f"{TARGET} not found"
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"MARAE_NO_PLANTS before={report.get('before')} after={report.get('after')} removed={len(report.get('removed', []))}")


main()
