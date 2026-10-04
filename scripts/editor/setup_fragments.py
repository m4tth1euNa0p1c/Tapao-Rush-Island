"""Generate the editor script that wires the Tuamotu fragments into Unreal (T-095, lot L3).

The Unreal MCP has no free Python: its ProgrammaticToolset runs a sandboxed script (json/math/re only, no file
access) that chains MCP tool calls. This generator reads the offline data and writes such a script, data inlined:

    python scripts/editor/setup_fragments.py --part assets   > out.py   # DA_Island_Raraka_NN + DA_IslandCatalog
    python scripts/editor/setup_fragments.py --part arrivals > out.py   # AExpeditionArrivalPoint in Lvl_Raraka
    python scripts/editor/setup_fragments.py --part galleon  > out.py   # Galleon anchorage in the 20 definitions

Then pass the content of out.py to ProgrammaticToolset.execute_tool_script (MCP `unreal`). Both parts are
idempotent: existing assets and arrival points (matched by FragmentId) are updated, never duplicated, and no other
asset or actor is touched. The arrivals part refuses to run when the open level is not Lvl_Raraka.

Sources:
- docs/reference/map/raraka/fragments/fragments.json (T-082): arrival (UE cm, Lvl_Raraka frame, yaw from +X to +Y),
  moorings, coves, extraction cove;
- docs/narrative/ST_Islands.csv (T-092): Island.<FRG>.Name keys, imported as /Game/MonkeIsland/UI/Text/ST_Islands.

Anchorages per fragment (cahier §6.4, the code drops a definition without 3 valid ones):
- Start     = arrival point of the boat (same transform as the arrival actor);
- Resources = first mooring of the fragment, else the first cove that is not the extraction cove;
- Fallback  = extraction cove, bAvailableDuringSubmersion;
- Galleon   = galleon_anchor of fragments.json (T-083, ADR-005): location_cm and yaw_deg, bAvailableDuringSubmersion.
  The galleon part only adds or replaces this anchorage and leaves the others untouched (T-089).

Map of each definition: Lvl_Raraka, except the fragments listed in FRAGMENT_MAPS, which have their own map (T-089).
Resources and Fallback face the centre of the fragment (provisional, until the mooring actors of T-085/T-089).
"""
import argparse
import csv
import json
import math
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FRAGMENTS_JSON = ROOT / "docs/reference/map/raraka/fragments/fragments.json"
STRING_TABLE_CSV = ROOT / "docs/narrative/ST_Islands.csv"

ISLANDS_FOLDER = "/Game/MonkeIsland/Data/Islands"
STRING_TABLE = "/Game/MonkeIsland/UI/Text/ST_Islands.ST_Islands"
MAP = "/Game/MonkeIsland/Maps/Lvl_Raraka.Lvl_Raraka"
# Fragments that have their own map (T-089); the others still point to Lvl_Raraka
FRAGMENT_MAPS = {
    "FRG_Raraka_01": "/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01.L_Frag_Raraka_01",
}
LEVEL = "/Game/MonkeIsland/Maps/Lvl_Raraka"
ARRIVAL_FOLDER = "Expedition/Arrivals"
DATA_CREDITS = (
    "Terrain d'après des données réelles de l'atoll de Raraka (Tuamotu) : Te Fenua / DAF, Polynésie française, "
    "Licence Ouverte Etalab 2.0 ; atlas des récifs coralliens, IRD. Personnages, villages et événements fictifs."
)


def yaw_towards(src, dst):
    return math.degrees(math.atan2(dst[1] - src[1], dst[0] - src[0]))


def load_names():
    names = set()
    with STRING_TABLE_CSV.open(encoding="utf-8-sig", newline="") as f:
        for row in csv.DictReader(f):
            names.add(row["Key"])
    return names


def build_data():
    data = json.loads(FRAGMENTS_JSON.read_text(encoding="utf-8"))
    keys = load_names()
    out = []
    for frag in data["fragments"]:
        fid = frag["id"]
        num = fid.rsplit("_", 1)[-1]
        sites = frag["sites"]
        centre = frag["center_cm"]
        arrival = frag["arrival"]
        ax, ay, _ = arrival["location_cm"]
        extraction = sites["extraction"]["center_cm"]

        if sites["moorings"]:
            res = sites["moorings"][0]["location_cm"]
            res_name = "Mouillage des ressources"
        else:
            coves = [c["center_cm"] for c in sites["coves"] if c["center_cm"] != extraction]
            if not coves:
                raise SystemExit(f"{fid}: no mooring and no cove besides the extraction cove")
            res = coves[0]
            res_name = "Crique des ressources"

        name_key = f"Island.{fid}.Name"
        if name_key in keys:
            display = f'LOCTABLE("{STRING_TABLE}", "{name_key}")'
        else:
            display = f"Fragment {num}"  # provisional, no narrative identity yet (docs/narrative/islands.md)

        # Compact row, expanded by the editor script: id, number, display name, arrival / resources / fallback
        # (x, y, yaw), name of the resources anchorage, land area, variant seed, galleon (x, y, yaw), map
        out.append([
            fid, num, display,
            [round(ax, 1), round(ay, 1), round(arrival["yaw_deg"], 2)],
            [round(res[0], 1), round(res[1], 1), round(yaw_towards(res, centre), 2)],
            res_name,
            [round(extraction[0], 1), round(extraction[1], 1), round(yaw_towards(extraction, centre), 2)],
            frag["land_ha"], frag["seed"],
            [round(frag["galleon_anchor"]["location_cm"][0], 1), round(frag["galleon_anchor"]["location_cm"][1], 1),
             round(frag["galleon_anchor"]["yaw_deg"], 2)],
            FRAGMENT_MAPS.get(fid, MAP),
        ])
    return out


ASSETS_TEMPLATE = r'''import json
# Generated by scripts/editor/setup_fragments.py --part assets (T-095). Do not edit by hand.
A = "editor_toolset.toolsets.asset.AssetTools."
D = "editor_toolset.toolsets.data_asset.DataAssetTools."
O = "editor_toolset.toolsets.object.ObjectTools."
FOLDER = __FOLDER__
MAP = __MAP__
CREDITS = __CREDITS__
FRAGMENTS = __FRAGMENTS__

def call(tool, args):
    return execute_tool(tool, json.dumps(args))["returnValue"]

def xform(p):
    return {"location": {"x": p[0], "y": p[1], "z": 0.0}, "rotation": {"pitch": 0.0, "yaw": p[2], "roll": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0}}

def values(f):
    fid, num, display, start, res, res_name, fallback, hectares, seed, galleon, map_path = f
    anchor = lambda i, n, role, p, sub: {"AnchorageId": i, "DisplayName": n, "Role": role,
                                         "bAvailableDuringSubmersion": sub, "Transform": xform(p)}
    return {"FragmentId": fid, "DisplayName": display, "Map": {"refPath": map_path}, "ZoneCount": 3,
            "Anchorages": [anchor("ANC_Start", "Plage d'arrivée", "Start", start, False),
                           anchor("ANC_Resources", res_name, "Resources", res, False),
                           anchor("ANC_Fallback", "Crique d'extraction", "Fallback", fallback, True),
                           anchor("Galleon", "Mouillage du galion", "Galleon", galleon, True)],
            "SizeHectares": hectares, "VariantSeeds": [seed], "DataCredits": CREDITS}

def ensure(name, cls):
    path = FOLDER + "/" + name
    if call(A + "exists", {"path": path}):
        return {"refPath": path + "." + name}, False
    return call(D + "create", {"folder_path": FOLDER, "asset_name": name, "asset_type": {"refPath": cls}}), True

def run():
    report = {"created": [], "updated": [], "failed": []}
    rules, new = ensure("DA_FragmentRules", "/Script/monke_island.FragmentRules")
    (report["created"] if new else report["updated"]).append("DA_FragmentRules")
    ids = []
    names = []
    for frag in FRAGMENTS:
        name = "DA_Island_Raraka_" + frag[1]
        names.append(name)
        asset, new = ensure(name, "/Script/monke_island.ExpeditionIslandDefinition")
        # Arrays are merged element-wise by set_properties: reset them first so a re-run replaces them
        call(O + "reset_properties", {"instance": asset, "properties": ["Anchorages", "VariantSeeds"]})
        ok = call(O + "set_properties", {"instance": asset, "values": json.dumps(values(frag))})
        (report["created"] if new else report["updated"]).append(name)
        if not ok:
            report["failed"].append(name)
        ids.append("ExpeditionIsland:" + frag[0])
    catalog, new = ensure("DA_IslandCatalog", "/Script/monke_island.IslandCatalog")
    call(O + "reset_properties", {"instance": catalog, "properties": ["Fragments"]})
    ok = call(O + "set_properties", {"instance": catalog, "values": json.dumps({"Fragments": ids, "Rules": rules})})
    (report["created"] if new else report["updated"]).append("DA_IslandCatalog")
    if not ok:
        report["failed"].append("DA_IslandCatalog")
    paths = [FOLDER + "/" + n for n in ["DA_FragmentRules", "DA_IslandCatalog"] + names]
    report["saved"] = call(A + "save_assets", {"asset_paths": paths})
    return report
'''

ARRIVALS_TEMPLATE = r'''import json
# Generated by scripts/editor/setup_fragments.py --part arrivals (T-095). Do not edit by hand.
S = "editor_toolset.toolsets.scene.SceneTools."
T = "editor_toolset.toolsets.actor.ActorTools."
O = "editor_toolset.toolsets.object.ObjectTools."
A = "editor_toolset.toolsets.asset.AssetTools."
LEVEL = __LEVEL__
FOLDER = __ARRIVAL_FOLDER__
CLS = {"refPath": "/Script/monke_island.ExpeditionArrivalPoint"}
FRAGMENTS = __FRAGMENTS__

def call(tool, args):
    return execute_tool(tool, json.dumps(args))["returnValue"]

def xform(p):
    return {"location": {"x": p[0], "y": p[1], "z": 0.0}, "rotation": {"pitch": 0.0, "yaw": p[2], "roll": 0.0},
            "scale": {"x": 1.0, "y": 1.0, "z": 1.0}}

def run():
    level = call(S + "get_current_level", {})
    if level != LEVEL:
        return {"error": "open level is " + level + ", expected " + LEVEL}
    existing = {}
    for actor in call(S + "find_actors", {"name": "", "tag": "", "actor_type": CLS, "collision_channels": []}):
        fid = json.loads(call(O + "get_properties", {"instance": actor, "properties": ["FragmentId"]}))["FragmentId"]
        existing.setdefault(fid, actor)
    report = {"created": [], "updated": []}
    for fid, start in FRAGMENTS:
        label = "ARR_" + fid
        actor = existing.get(fid)
        if actor is None:
            actor = call(S + "add_to_scene_from_class", {"actor_type": CLS, "name": label, "xform": xform(start)})
            report["created"].append(label)
        else:
            call(T + "set_actor_transform", {"actor": actor, "xform": xform(start), "worldspace": True})
            report["updated"].append(label)
        call(O + "set_properties", {"instance": actor, "values": json.dumps({"FragmentId": fid})})
        call(T + "set_label", {"actor": actor, "label": label})
        call(S + "set_actor_folder", {"actor": actor, "folder_path": FOLDER})
    report["in_folder"] = len(call(S + "get_actors_in_folder", {"folder_path": FOLDER}))
    report["saved"] = call(A + "save_assets", {"asset_paths": [LEVEL]})
    return report
'''


GALLEON_TEMPLATE = r'''import json
# Generated by scripts/editor/setup_fragments.py --part galleon (T-089). Do not edit by hand.
A = "editor_toolset.toolsets.asset.AssetTools."
O = "editor_toolset.toolsets.object.ObjectTools."
FOLDER = __FOLDER__
GALLEONS = __GALLEONS__

def call(tool, args):
    return execute_tool(tool, json.dumps(args))["returnValue"]

def run():
    report = {"updated": [], "failed": [], "missing": []}
    paths = []
    for num, p in GALLEONS:
        name = "DA_Island_Raraka_" + num
        path = FOLDER + "/" + name
        if not call(A + "exists", {"path": path}):
            report["missing"].append(name)
            continue
        asset = {"refPath": path + "." + name}
        current = json.loads(call(O + "get_properties", {"instance": asset, "properties": ["Anchorages"]}))["Anchorages"]
        keep = [a for a in current if a.get("anchorageId", a.get("AnchorageId")) != "Galleon"]
        # Same transform format as the assets part (location + rotator), which set_properties accepts
        galleon = {"AnchorageId": "Galleon", "DisplayName": "Mouillage du galion", "Role": "Galleon",
                   "bAvailableDuringSubmersion": True,
                   "Transform": {"location": {"x": p[0], "y": p[1], "z": 0.0},
                                 "rotation": {"pitch": 0.0, "yaw": p[2], "roll": 0.0},
                                 "scale": {"x": 1.0, "y": 1.0, "z": 1.0}}}
        # A struct array cannot change size and elements in one set_properties: reset, then set the whole list
        call(O + "reset_properties", {"instance": asset, "properties": ["Anchorages"]})
        ok = call(O + "set_properties", {"instance": asset, "values": json.dumps({"Anchorages": keep + [galleon]})})
        after = json.loads(call(O + "get_properties", {"instance": asset, "properties": ["Anchorages"]}))["Anchorages"]
        # The other anchorages must come back unchanged (ids, roles, positions to the cm: the json round trip
        # changes float digits, so an exact comparison fails)
        def sig(a):
            t = a["transform"]
            return (a["anchorageId"], a["role"], round(t["location"]["x"]), round(t["location"]["y"]),
                    round(t["rotation"]["yaw"], 1), a["bAvailableDuringSubmersion"])
        if ok and len(after) == len(keep) + 1 and [sig(a) for a in after[:len(keep)]] == [sig(a) for a in keep]:
            report["updated"].append([name, len(after)])
        else:
            report["failed"].append([name, ok, len(after)])
        paths.append(path)
    report["saved"] = call(A + "save_assets", {"asset_paths": paths})
    report["sample"] = after if paths else None
    return report
'''


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--part", choices=["assets", "arrivals", "galleon"], required=True)
    parser.add_argument("--out", type=Path, help="write the script here instead of stdout")
    args = parser.parse_args()

    frags = build_data()
    if args.part == "assets":
        script = (ASSETS_TEMPLATE.replace("__FOLDER__", repr(ISLANDS_FOLDER)).replace("__MAP__", repr(MAP))
                  .replace("__CREDITS__", repr(DATA_CREDITS)).replace("__FRAGMENTS__", repr(frags)))
    elif args.part == "galleon":
        script = (GALLEON_TEMPLATE.replace("__FOLDER__", repr(ISLANDS_FOLDER))
                  .replace("__GALLEONS__", repr([[f[1], f[9]] for f in frags])))
        script = script.replace("__SINHALF__(p[2])", "math.sin(math.radians(p[2]) / 2.0)")
        script = script.replace("__COSHALF__(p[2])", "math.cos(math.radians(p[2]) / 2.0)")
        script = script.replace("import json\n", "import json\nimport math\n", 1)
    else:
        script = (ARRIVALS_TEMPLATE.replace("__LEVEL__", json.dumps(LEVEL))
                  .replace("__ARRIVAL_FOLDER__", json.dumps(ARRIVAL_FOLDER))
                  .replace("__FRAGMENTS__", repr([[f[0], f[3]] for f in frags])))
    if args.out:
        args.out.write_text(script, encoding="utf-8")
    else:
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stdout.write(script)


if __name__ == "__main__":
    main()
