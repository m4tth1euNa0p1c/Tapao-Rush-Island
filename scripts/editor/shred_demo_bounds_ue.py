"""Mobs that vanished at a close or heavy shot (owner 2026-10-10 "fix les bugs de dépop des mobs"): once the shred demo
is drawn at full size again (ATikiShredDemo plays its clip through UTikiAnimInstance, root at the reference scale), its
fragments fly up to about 4.7 m from the body, 1.8 m beyond the culling box given by ShredDemoBoundsScale 4 (PIE probe
of 2026-10-10 on SK_Demo_CocoWarrior_Musket_Burst). Every UTikiRules asset under /Game/MonkeIsland gets
ShredDemoBoundsScale = 9 when it is lower. Lead session action "shreddemobounds" (no game running).
Report: monke_island/Saved/T284/shred-demo-bounds.json. Replays change nothing.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T284/shred-demo-bounds.json"
TARGET = 9.0


def main():
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    report = {"target": TARGET, "assets": {}}
    for data in registry.get_assets_by_path("/Game/MonkeIsland", recursive=True):
        if str(data.asset_class_path.asset_name) != "TikiRules":
            continue
        path = str(data.package_name)
        rules = ue.EditorAssetLibrary.load_asset(path)
        before = rules.get_editor_property("shred_demo_bounds_scale")
        entry = {"before": round(before, 2)}
        if before < TARGET:
            rules.set_editor_property("shred_demo_bounds_scale", TARGET)
            entry["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(rules, only_if_is_dirty=False))
        entry["after"] = round(rules.get_editor_property("shred_demo_bounds_scale"), 2)
        entry["mesh_bounds_scale"] = round(rules.get_editor_property("mesh_bounds_scale"), 2)
        entry["held_item_bounds_scale"] = round(rules.get_editor_property("held_item_bounds_scale"), 2)
        report["assets"][path] = entry
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"SHRED_DEMO_BOUNDS {len(report['assets'])} rules assets")


main()
