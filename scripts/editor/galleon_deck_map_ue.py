"""T-322 (owner 2026-10-11: "décaler la boutique vers le centre du pont du navire"): the deck of the galleon as data, to
choose the merchant's place. Lists the sockets of DA_GalleonRules' hull mesh (masts, hatches, capstan, companion,
ladders, bean_spawn...) in the hull frame and relative to DeckSpawnSocket (the frame of ShopkeeperOffset), with the
merchant's and the crate's current places. No change.
Lead session action "deckmap" (editor open, no game running). Report: monke_island/Saved/T322/deck-map.json.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T322/deck-map.json"
RULES = "/Game/MonkeIsland/Galleon/DA_GalleonRules"
NAMES = ["bean_spawn", "mast_main_cap", "mast_fore_cap", "mast_mizzen_cap", "knight_main", "companion", "hatch_main",
         "hatch_fore", "cargo_hatch", "capstan_main", "ladder_top", "boarding_ladder", "ladder_bottom_quarter_P",
         "ladder_bottom_quarter_S", "ladder_bottom_fore_P", "ladder_bottom_fore_S", "cabin_door", "fc_door", "helm",
         "lantern_gangway_P", "lantern_gangway_S"]


def cm(v):
    return [round(v.x, 1), round(v.y, 1), round(v.z, 1)]


def main():
    rules = ue.EditorAssetLibrary.load_asset(RULES)
    if not rules:
        raise RuntimeError(f"{RULES} missing")
    hull = rules.get_editor_property("hull_mesh")
    if not hull:
        raise RuntimeError("DA_GalleonRules has no HullMesh")
    spawn_name = str(rules.get_editor_property("deck_spawn_socket"))
    # the socket list is protected from Python: the deck's sockets by name (T-188 export)
    sockets = {}
    for name in NAMES:
        socket = hull.find_socket(name)
        if socket:
            sockets[name] = socket.get_editor_property("relative_location")
    base = sockets.get(spawn_name)
    report = {"hull": hull.get_path_name(), "deck_spawn_socket": spawn_name,
              "bounds": {"origin": cm(hull.get_bounds().origin), "extent": cm(hull.get_bounds().box_extent)},
              "shopkeeper_offset": cm(rules.get_editor_property("shopkeeper_offset")),
              "shopkeeper_yaw": rules.get_editor_property("shopkeeper_yaw"),
              "loot_crate_offset": cm(rules.get_editor_property("loot_crate_offset")),
              "deck_spawn_spacing": rules.get_editor_property("deck_spawn_spacing"),
              "sockets": {}}
    for name, location in sorted(sockets.items(), key=lambda kv: kv[1].y):
        row = {"hull": cm(location)}
        if base is not None:
            row["from_spawn"] = cm(location - base)
        report["sockets"][name] = row
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"DECK_MAP {len(sockets)} sockets, spawn {spawn_name}")


main()
