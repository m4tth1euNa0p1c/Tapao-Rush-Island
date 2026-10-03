"""Fills the Unreal assets of the fragment villages from their memory (scripts/map/fragment_villages.py) through the
Unreal MCP (editor open on the project, no PIE running):
  - DA_VillageRules (UVillageRules): the ambience of the villages (DA_VillageAmbience), the rest from the C++ defaults
    (clearing margins of scripts/map/village_clearing.py);
  - DA_Villages_<FRG> (UVillageSetDefinition), one per fragment: houses and props, NPC spawn points, points of interest,
    lights, doors, clearing areas;
  - BP_VillageSpawner (AVillageSpawner): the rules and the fragments (boat arrival, data asset).
--place also puts one BP_VillageSpawner in the level (the level is shared: agree with its owner first) and saves it.

Usage:
  python scripts/map/fragment_villages_ue.py raraka [--fragment FRG_Raraka_19 ...] [--place] [--dry-run]

The points never go through a tool argument of the conversation: this script posts them to the MCP itself.
"""
import argparse
import json
import sys
import urllib.request
from collections import OrderedDict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
URL = "http://127.0.0.1:8000/mcp"   # IPv4 only (CLAUDE.md)
HDR = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
UE_MESHES = "/Game/MonkeIsland/Environment/Village/Meshes"
DATA = "/Game/MonkeIsland/Environment/Village/Data"
SETS = DATA + "/Fragments"
BLUEPRINTS = "/Game/MonkeIsland/Environment/Village/Blueprints"
RULES = DATA + "/DA_VillageRules"
AMBIENCE = DATA + "/DA_VillageAmbience"
SPAWNER = BLUEPRINTS + "/BP_VillageSpawner"
LEVEL = "/Game/MonkeIsland/Maps/Lvl_Raraka"
KINDS = {"building": "Building", "small": "Small", "plaza": "Plaza", "road": "Road", "path": "Path"}

AT = "editor_toolset.toolsets.asset.AssetTools"
OT = "editor_toolset.toolsets.object.ObjectTools"
DAT = "editor_toolset.toolsets.data_asset.DataAssetTools"
BT = "editor_toolset.toolsets.blueprint.BlueprintTools"
ST = "editor_toolset.toolsets.scene.SceneTools"
APP = "EditorToolset.EditorAppToolset"


# ---------------------------------------------------------------------------------------------- MCP

class Mcp:
    def __init__(self):
        self.sid = None
        self.sid, _ = self._post({"jsonrpc": "2.0", "id": 1, "method": "initialize",
                                  "params": {"protocolVersion": "2025-03-26", "capabilities": {},
                                             "clientInfo": {"name": "fragment-villages-ue", "version": "1"}}})
        self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})

    def _post(self, body, timeout=300):
        h = dict(HDR)
        if self.sid:
            h["Mcp-Session-Id"] = self.sid
        req = urllib.request.Request(URL, json.dumps(body).encode(), h)
        with urllib.request.urlopen(req, timeout=timeout) as r:
            return r.headers.get("Mcp-Session-Id") or self.sid, r.read().decode("utf-8", "replace")

    def call(self, toolset, tool, args, timeout=300):
        _, txt = self._post({"jsonrpc": "2.0", "id": 2, "method": "tools/call",
                             "params": {"name": "call_tool", "arguments": {"tool_name": tool, "toolset_name": toolset,
                                                                           "arguments": args}}}, timeout)
        body = json.loads(txt[txt.index("{"):]) if not txt.startswith("{") else json.loads(txt)
        if "error" in body:
            raise RuntimeError(f"{tool}: {body['error']}")
        text = body["result"]["content"][0]["text"]
        try:
            out = json.loads(text)
        except json.JSONDecodeError:
            return text
        return out.get("returnValue", out) if isinstance(out, dict) else out


def ref(path):
    return {"refPath": path + "." + path.rsplit("/", 1)[-1]}


# ---------------------------------------------------------------------------------------------- payloads

def vec(p):
    return {"X": float(p[0]), "Y": float(p[1]), "Z": float(p[2]) if len(p) > 2 else 0.0}


def vec2(p):
    return {"X": float(p[0]), "Y": float(p[1])}


def clear_area(s):
    kind = KINDS.get(s.get("kind", "building"), "Building")
    if s["type"] == "rect":
        return {"Shape": "Rect", "Kind": kind, "Center": vec2(s["center"]), "HalfSize": vec2(s["half"]),
                "Yaw": float(s["yaw"])}
    if s["type"] == "circle":
        return {"Shape": "Circle", "Kind": kind, "Center": vec2(s["center"]), "HalfSize": {"X": float(s["radius"]), "Y": 0.0}}
    return {"Shape": "Path", "Kind": kind, "HalfSize": {"X": float(s["half_width"]), "Y": 0.0},
            "Points": [vec2(p) for p in s["points"]]}


def village_payload(v):
    groups = OrderedDict()
    for it in v["items"]:
        g = groups.setdefault(it["asset"], {"Mesh": ref(f"{UE_MESHES}/{it['asset']}"), "Locations": [], "Yaws": []})
        g["Locations"].append(vec(it["loc_cm"]))
        g["Yaws"].append(float(it["yaw"]))
    return {
        "Id": v["id"], "Name": v["name"], "Theme": v["theme"], "Site": v["site"]["id"],
        "Center": vec(v["zone"]["center_cm"]), "Radius": float(v["zone"]["radius_cm"]),
        "bZone": v.get("kind", "zone") == "zone", "VillagerCount": int(v.get("villager_count", 0)),
        "Meshes": list(groups.values()),
        "Spawns": [{"Location": vec(s["loc_cm"]), "Role": s["role"]} for s in v["spawns"]],
        "Pois": [{"Location": vec(p["loc_cm"]), "Yaw": float(p["yaw"]), "Activity": p["activity"]} for p in v["pois"]],
        "Lights": [{"Location": vec(l["loc_cm"]), "bLantern": l["kind"] == "lantern", "Candela": float(l["candela"]),
                    "Radius": float(l["radius_cm"]), "bShadows": bool(l["shadows"])} for l in v["lights"]],
        "Doors": [{"Location": vec(d["loc_cm"]), "Yaw": float(d["yaw"]), "GateDistance": float(d["gate_cm"])} for d in v["doors"]],
        "Clearing": [clear_area(s) for s in v["clear"]],
    }


def set_name(fid):
    return f"DA_Villages_{fid}"


# ---------------------------------------------------------------------------------------------- steps

def ensure(mcp, toolset, folder, name, cls):
    path = f"{folder}/{name}"
    if mcp.call(AT, "exists", {"path": path}) is True:
        return path, False
    mcp.call(toolset, "create", {"folder_path": folder, "asset_name": name, "asset_type": {"refPath": cls}})
    if mcp.call(AT, "exists", {"path": path}) is not True:
        raise RuntimeError(f"{path} was not created")
    return path, True


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("island")
    ap.add_argument("--fragment", action="append", default=[], help="only this fragment (repeatable)")
    ap.add_argument("--place", action="store_true", help="put BP_VillageSpawner in the level and save it")
    ap.add_argument("--dry-run", action="store_true", help="write the payloads next to the memory, touch nothing")
    args = ap.parse_args()

    mem_dir = ROOT / "docs" / "reference" / "map" / args.island / "villages" / "fragments"
    files = sorted(p for p in mem_dir.glob("FRG_*.json") if not args.fragment or p.stem in args.fragment)
    if not files:
        sys.exit(f"no fragment memory in {mem_dir}")
    payloads = {}
    entries = []
    for p in files:
        mem = json.loads(p.read_text(encoding="utf-8"))
        payloads[mem["fragment"]] = {"FragmentId": mem["fragment"], "Villages": [village_payload(v) for v in mem["villages"]]}
        entries.append({"FragmentId": mem["fragment"], "Arrival": vec(mem["arrival_cm"]),
                        "Set": ref(f"{SETS}/{set_name(mem['fragment'])}")})
    size = sum(len(json.dumps(x)) for x in payloads.values())
    print(f"[villages-ue] {len(payloads)} fragments, {sum(len(x['Villages']) for x in payloads.values())} villages, {size / 1024:.0f} KB of data")
    if args.dry_run:
        out = ROOT / "monke_island" / "Saved" / "VillageImport"
        out.mkdir(parents=True, exist_ok=True)
        for fid, data in payloads.items():
            (out / f"{set_name(fid)}.json").write_text(json.dumps(data), encoding="utf-8")
        (out / "BP_VillageSpawner.json").write_text(json.dumps({"Fragments": entries}, indent=1), encoding="utf-8")
        print(f"[villages-ue] dry run: payloads in {out}")
        return

    mcp = Mcp()
    if mcp.call(APP, "IsPIERunning", {}) is True:
        sys.exit("a PIE is running: the assets cannot be saved now, try again when it stops")

    rules, created = ensure(mcp, DAT, DATA, "DA_VillageRules", "/Script/monke_island.VillageRules")
    if created or mcp.call(AT, "exists", {"path": AMBIENCE}) is True:
        print("[villages-ue] DA_VillageRules ambience:",
              mcp.call(OT, "set_properties", {"instance": ref(rules), "values": json.dumps({"AmbienceRules": ref(AMBIENCE)})}))
    saved = [rules]
    for fid, data in payloads.items():
        path, _ = ensure(mcp, DAT, SETS, set_name(fid), "/Script/monke_island.VillageSetDefinition")
        ok = mcp.call(OT, "set_properties", {"instance": ref(path), "values": json.dumps(data)}, timeout=600)
        back = mcp.call(OT, "get_properties", {"instance": ref(path), "properties": ["FragmentId", "Villages"]}, timeout=600)
        back = json.loads(back) if isinstance(back, str) else back
        villages = back.get("Villages") or back.get("villages") or []
        got = [(v.get("Id") or v.get("id"), len(v.get("Meshes") or v.get("meshes") or []),
                len(v.get("Spawns") or v.get("spawns") or []), len(v.get("Clearing") or v.get("clearing") or [])) for v in villages]
        print(f"[villages-ue] {set_name(fid)}: set {ok}, read back {got}")
        saved.append(path)

    bp, _ = ensure(mcp, BT, BLUEPRINTS, "BP_VillageSpawner", "/Script/monke_island.VillageSpawner")
    cdo = {"refPath": f"{bp}.Default__BP_VillageSpawner_C"}
    print("[villages-ue] BP_VillageSpawner:",
          mcp.call(OT, "set_properties", {"instance": cdo, "values": json.dumps({"Rules": ref(rules), "Fragments": entries})}))
    mcp.call(BT, "compile_blueprint", {"blueprint": ref(bp), "warnings_as_errors": False})
    back = mcp.call(OT, "get_properties", {"instance": cdo, "properties": ["Rules", "Fragments"]})
    back = json.loads(back) if isinstance(back, str) else back
    print(f"[villages-ue] BP_VillageSpawner: rules {back.get('Rules') or back.get('rules')}, "
          f"{len(back.get('Fragments') or back.get('fragments') or [])} fragments")
    saved.append(bp)
    print("[villages-ue] saved:", mcp.call(AT, "save_assets", {"asset_paths": saved}, timeout=600))

    if args.place:
        if mcp.call(ST, "get_current_level", {}) != LEVEL:
            sys.exit(f"the editor is not on {LEVEL}: not placed")
        if mcp.call(ST, "find_actors", {"name": "VillageSpawner", "tag": "", "collision_channels": []}):
            print("[villages-ue] a VillageSpawner is already in the level")
        else:
            actor = mcp.call(ST, "add_to_scene_from_class", {
                "actor_type": {"refPath": f"{bp}.BP_VillageSpawner_C"}, "name": "VillageSpawner",
                "xform": {"location": {"x": -653334.7, "y": -1013400.0, "z": 600.0},
                          "rotation": {"pitch": 0, "yaw": 0, "roll": 0}, "scale": {"x": 1, "y": 1, "z": 1}}})
            mcp.call("editor_toolset.toolsets.actor.ActorTools", "set_label", {"actor": actor, "label": "VillageSpawner"})
            mcp.call(ST, "set_actor_folder", {"actor": actor, "folder_path": "Raraka/Villages"})
            print("[villages-ue] placed:", actor)
        print("[villages-ue] level saved:", mcp.call(AT, "save_assets", {"asset_paths": [LEVEL]}, timeout=600))


if __name__ == "__main__":
    main()
