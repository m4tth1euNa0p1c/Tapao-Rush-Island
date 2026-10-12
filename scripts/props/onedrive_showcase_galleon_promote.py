"""Promote only the tested hull revision into the main game rules."""
import datetime as dt
import json
from pathlib import Path
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T184"
proof = json.loads((OUT / "access-traversal.json").read_text(encoding="utf-8"))
imported = json.loads((OUT / "import-result.json").read_text(encoding="utf-8"))
merchant = json.loads((ROOT / "monke_island/Saved/T181/pie-result.json").read_text(encoding="utf-8"))
animation = json.loads((ROOT / "monke_island/Saved/T181/animation-units.json").read_text(encoding="utf-8"))
assert merchant["passed"] and merchant["shop_ui_requested"] and merchant["cursor_visible"]
assert merchant["finished_at"] > animation["finished_at"]
assert proof["passed"] and proof["finished_at"] > imported["finished_at"]
assert len(proof["routes"]) == 10 and all(r["passed"] for r in proof["routes"])
path = "/Game/MonkeIsland/Galleon/DA_GalleonRules"
rules = ue.load_asset(path)
test = ue.load_asset("/Game/MonkeIsland/Galleon/TestFlat/DA_GalleonRules_TestFlat")
mesh = test.get_editor_property("hull_mesh")
assert mesh.get_path_name() == proof["hull_mesh"]
backup = "/Game/MonkeIsland/Galleon/Backups/DA_GalleonRules_PreT184"
if not ue.EditorAssetLibrary.does_asset_exist(backup):
    assert ue.EditorAssetLibrary.duplicate_asset(path, backup)
    assert ue.EditorAssetLibrary.save_asset(backup)
rules.set_editor_property("hull_mesh", mesh)
# The new oak interior no longer needs the strong orange fill used by the old cabin.
test.set_editor_property("cabin_light_intensity", 0.8)
test.set_editor_property("cabin_light_temperature", 3200.0)
assert ue.EditorAssetLibrary.save_loaded_asset(test, False)
for key in ("shopkeeper_class", "shopkeeper_offset", "shopkeeper_yaw", "cabin_light_intensity", "cabin_light_temperature"):
    rules.set_editor_property(key, test.get_editor_property(key))
assert ue.EditorAssetLibrary.save_loaded_asset(rules, False)
(OUT / "promotion-result.json").write_text(json.dumps({"rules": path, "mesh": mesh.get_path_name(),
    "backup": backup, "traversal_finished_at": proof["finished_at"],
    "merchant_validation_finished_at": merchant["finished_at"],
    "cabin_light_candela": 0.8, "cabin_light_kelvin": 3200,
    "saved_at": dt.datetime.now(dt.timezone.utc).isoformat()}, indent=2), encoding="utf-8")
