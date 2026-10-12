"""T-322 (owner 2026-10-11: "décaler la boutique vers le centre du pont du navire"): the merchant moves from the port
bulwark to the middle of the main deck. The deck of SM_Galleon (T-188 sockets, art/export/Galleon_T188/SM_Galleon.fbx,
read in Blender: x to port, y to the stern, metres):
- main mast at (0, -1.45) and its bitts at (0, -0.68), main hatch at (0, -3.0), companion at (-1.29, -0.93), boarding
  ladder's top at (-1.85, 2.6), quarterdeck ladders at (+-1.85, 2.12); bean_spawn at (1.1, -1.6), the crew's four places
  along the ship from it (y -2.95 to -0.25);
- the merchant stood at (1.9, 0.4), against the port bulwark; it now stands on the centre line just aft of the mast's
  bitts, at (0, 0.45): 1.1 m from the bitts, 1.1 m from the nearest crew place, 2.8 m of deck on each side;
- it faces the stern (-90): the crew coming back aboard by the ladder sees it from the top of the ladder.
In the hull frame of ShopkeeperOffset (+X to port, +Y to the bow, from bean_spawn): (-110, -205), height kept.
Every GalleonRules data asset gets it (the main one and the test map's). Lead session action "merchantdeck" (editor
open, no game running). Report: monke_island/Saved/T322/merchant-deck.json. Replays change nothing.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T322/merchant-deck.json"
OFFSET_XY = (-110.0, -205.0)
YAW = -90.0


def main():
    report = {"assets": {}}
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    found = registry.get_assets_by_class(ue.TopLevelAssetPath("/Script/monke_island", "GalleonRules"), True)
    for data in found:
        path = str(data.package_name)
        # the live rules only: the copies kept in Galleon/Backups stay as they were (2026-10-11: a first run moved
        # them too, put back by scripts/editor/merchant_deck_restore_backups_ue.py)
        if not path.startswith("/Game/MonkeIsland/") or "/Backups/" in path:
            continue
        rules = ue.EditorAssetLibrary.load_asset(path)
        before = rules.get_editor_property("shopkeeper_offset")
        row = {"before": [before.x, before.y, before.z, rules.get_editor_property("shopkeeper_yaw")],
               "class": str(rules.get_editor_property("shopkeeper_class"))}
        offset = ue.Vector(OFFSET_XY[0], OFFSET_XY[1], before.z)
        if before != offset or rules.get_editor_property("shopkeeper_yaw") != YAW:
            rules.set_editor_property("shopkeeper_offset", offset)
            rules.set_editor_property("shopkeeper_yaw", YAW)
            row["saved"] = bool(ue.EditorAssetLibrary.save_loaded_asset(rules, only_if_is_dirty=False))
        after = rules.get_editor_property("shopkeeper_offset")
        row["after"] = [after.x, after.y, after.z, rules.get_editor_property("shopkeeper_yaw")]
        report["assets"][path] = row
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"MERCHANT_DECK {len(report['assets'])} rules")


main()
