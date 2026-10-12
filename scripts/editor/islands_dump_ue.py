"""The island definitions of the daily draw (owner 2026-10-08, forts on the real atoll): fragment, map, start and galleon
anchorages, unlock and weight of every DA_Island_Raraka_*, and the order of DA_IslandCatalog.
Writes monke_island/Saved/T186/islands-dump.json. Action "islandsdump"."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
ISLANDS = "/Game/MonkeIsland/Data/Islands"
rows = []
for path in sorted(ue.EditorAssetLibrary.list_assets(ISLANDS, False, False)):
    if "DA_Island_Raraka_" not in path:
        continue
    d = ue.EditorAssetLibrary.load_asset(path.split(".")[0])
    anchors = {}
    for anc in d.get_editor_property("anchorages"):
        t = anc.get_editor_property("transform")
        anchors[str(anc.get_editor_property("anchorage_id"))] = [round(t.translation.x), round(t.translation.y),
                                                                  round(t.rotation.rotator().yaw, 1)]
    rows.append({"asset": path.split(".")[0].split("/")[-1], "fragment": str(d.get_editor_property("fragment_id")),
                 "map": str(d.get_editor_property("map").get_asset_name() if hasattr(d.get_editor_property("map"), "get_asset_name") else d.get_editor_property("map")),
                 "needs_unlock": d.get_editor_property("needs_unlock"), "unlock": str(d.get_editor_property("unlock")),
                 "weight": d.get_editor_property("draw_weight"), "anchors": anchors})
cat = ue.EditorAssetLibrary.load_asset(ISLANDS + "/DA_IslandCatalog")
order = [str(i.get_editor_property("primary_asset_name")) for i in cat.get_editor_property("fragments")]
(ROOT / "monke_island/Saved/T186/islands-dump.json").write_text(json.dumps({"catalog": order, "islands": rows}, indent=1), encoding="utf-8")
