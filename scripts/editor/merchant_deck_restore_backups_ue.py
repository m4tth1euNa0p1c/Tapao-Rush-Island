"""T-322 follow-up: the first run of merchant_deck_centre_ue.py (2026-10-11 16:42) also moved the merchant in the copies
of DA_GalleonRules kept in /Game/MonkeIsland/Galleon/Backups. Puts their ShopkeeperOffset and ShopkeeperYaw back to the
values that run recorded ("before" in monke_island/Saved/T322/merchant-deck.json). Lead session action
"merchantbackups" (editor open, no game running). Report: monke_island/Saved/T322/merchant-backups-restored.json.
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "monke_island/Saved/T322/merchant-deck.json"
OUT = ROOT / "monke_island/Saved/T322/merchant-backups-restored.json"


def main():
    recorded = json.loads(SOURCE.read_text(encoding="utf-8"))["assets"]
    report = {}
    for path, row in recorded.items():
        if "/Backups/" not in path:
            continue
        rules = ue.EditorAssetLibrary.load_asset(path)
        if not rules:
            report[path] = "missing"
            continue
        x, y, z, yaw = row["before"]
        rules.set_editor_property("shopkeeper_offset", ue.Vector(x, y, z))
        rules.set_editor_property("shopkeeper_yaw", yaw)
        saved = bool(ue.EditorAssetLibrary.save_loaded_asset(rules, only_if_is_dirty=False))
        now = rules.get_editor_property("shopkeeper_offset")
        report[path] = {"restored": [now.x, now.y, now.z, rules.get_editor_property("shopkeeper_yaw")], "saved": saved}
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"MERCHANT_BACKUPS restored {len(report)}")


main()
