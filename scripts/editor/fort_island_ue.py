"""T-205: the fort island of the campaign (owner 2026-10-07: "faire apparaître le fort de manière aléatoire à partir du
6e jour").

Survey (always): the Start anchorages of the 20 fragments of Raraka, their centre (the lagoon), the distance from it to
the nearest start, and what lies under it. Apply (monke_island/Saved/T186/fortisland-apply.json: {"x", "y", "yaw"}):
DA_Island_Raraka_21 (FRG_Raraka_21, "Fort Sainte-Barbe", map L_Frag_Raraka_01) made from DA_Island_Raraka_01: its
anchorages around the fort placed at (x, y) turned by yaw (fort frame of LI_FortFrench: the pier to +Y), drawn by
the daily draw from the Forts day of the campaign with DrawWeight, and added to DA_IslandCatalog.
The level instance itself is placed by MCP (SceneTools.create_level_instance) at the same transform.
Writes monke_island/Saved/T186/fort-island.json. Action "fortisland".
"""
import datetime as dt
import json
import math
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

ISLANDS = "/Game/MonkeIsland/Data/Islands"
SOURCE = ISLANDS + "/DA_Island_Raraka_01"
TARGET = ISLANDS + "/DA_Island_Raraka_21"
CATALOG = ISLANDS + "/DA_IslandCatalog"
FRAGMENT = "FRG_Raraka_21"
APPLY_FILE = ROOT / "monke_island/Saved/T186/fortisland-apply.json"
OUT = ROOT / "monke_island/Saved/T186/fort-island.json"
DRAW_WEIGHT = 10.0
# anchorages in the fort frame (cm): the pier head and the open water around the island (LI_FortFrench: pier to +Y)
ANCHORS = [
    ("ANC_Start", "Ponton du fort", "Start", False, (0.0, 8200.0), -90.0),
    ("ANC_Resources", "Anse de l'est", "Resources", False, (19500.0, 0.0), 180.0),
    ("ANC_Fallback", "Mouillage de l'ouest", "Fallback", True, (-19500.0, 0.0), 0.0),
    ("Galleon", "Rade du galion", "Galleon", True, (0.0, 15000.0), -90.0),
]
report = {}


def starts():
    out = []
    for path in ue.EditorAssetLibrary.list_assets(ISLANDS, False, False):
        if "DA_Island_Raraka_" not in path or "_21" in path:
            continue
        d = ue.EditorAssetLibrary.load_asset(path.split(".")[0])
        for anc in d.get_editor_property("anchorages"):
            if anc.get_editor_property("role") == ue.AnchorageRole.START:
                t = anc.get_editor_property("transform").translation
                out.append((str(d.get_editor_property("fragment_id")), t.x, t.y))
    return out


def survey():
    s = starts()
    cx = sum(p[1] for p in s) / len(s)
    cy = sum(p[2] for p in s) / len(s)
    nearest = min(math.hypot(p[1] - cx, p[2] - cy) for p in s)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(cx, cy, 5000.0), ue.Vector(cx, cy, -10000.0),
                                             ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], ue.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    report["starts"] = [[p[0], round(p[1]), round(p[2])] for p in s]
    report["centre"] = [round(cx), round(cy)]
    report["nearest_start_m"] = round(nearest / 100.0)
    report["under_centre"] = [round(t[4].z), t[10].get_name() if t[10] else None] if t and t[0] else None
    report["roles"] = [n for n in dir(ue.AnchorageRole) if n.isupper()]


def define(x, y, yaw):
    rad = math.radians(yaw)

    def world(px, py):
        return x + px * math.cos(rad) - py * math.sin(rad), y + px * math.sin(rad) + py * math.cos(rad)

    if not ue.EditorAssetLibrary.does_asset_exist(TARGET):
        assert ue.EditorAssetLibrary.duplicate_asset(SOURCE, TARGET), "duplicate failed"
    d = kitlib.load(TARGET)
    d.set_editor_property("fragment_id", FRAGMENT)
    d.set_editor_property("display_name", ue.Text("Fort Sainte-Barbe"))
    d.set_editor_property("zone_count", 1)
    d.set_editor_property("variant_seeds", [21])
    d.set_editor_property("indicative_difficulty", 4)
    d.set_editor_property("data_credits", ue.Text("Île et fort imaginaires, placés dans le lagon de Raraka (Tuamotu)."))
    d.set_editor_property("needs_unlock", True)
    d.set_editor_property("unlock", ue.RunUnlock.FORTS)
    d.set_editor_property("draw_weight", DRAW_WEIGHT)
    # the anchorages are set by MCP (ObjectTools.set_properties): their fields are EditDefaultsOnly, closed to Python
    anchors = []
    for anc_id, label, role, submersion, (px, py), face in ANCHORS:
        wx, wy = world(px, py)
        anchors.append({"anchorageId": anc_id, "displayName": label, "role": role, "bAvailableDuringSubmersion": submersion,
                        "transform": {"location": {"x": round(wx, 1), "y": round(wy, 1), "z": 0},
                                      "rotation": {"pitch": 0, "yaw": yaw + face, "roll": 0}, "scale": {"x": 1, "y": 1, "z": 1}}})
    report["anchorages_json"] = json.dumps({"Anchorages": anchors}, ensure_ascii=False)
    kitlib.save(d)
    catalog = kitlib.load(CATALOG)
    ids = list(catalog.get_editor_property("fragments"))
    if not any(str(i.get_editor_property("primary_asset_name")) == FRAGMENT for i in ids):
        pid = ue.PrimaryAssetId()
        pid.set_editor_property("primary_asset_type", ue.PrimaryAssetType("ExpeditionIsland"))
        pid.set_editor_property("primary_asset_name", FRAGMENT)
        ids.append(pid)
        catalog.set_editor_property("fragments", ids)
        kitlib.save(catalog)
    report["definition"] = TARGET
    report["catalog"] = len(catalog.get_editor_property("fragments"))


MAP = "/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01"

try:
    kitlib.check_lock()
    current = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world().get_path_name().split(".")[0]
    if current != MAP:
        assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(MAP), "cannot load the expedition map"
    survey()
    if APPLY_FILE.exists():
        spec = json.loads(APPLY_FILE.read_text(encoding="utf-8"))
        define(float(spec["x"]), float(spec["y"]), float(spec.get("yaw", 0.0)))
finally:
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
