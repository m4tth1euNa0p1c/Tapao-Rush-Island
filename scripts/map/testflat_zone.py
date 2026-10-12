"""Extension of L_Test_Flat built by the game generators (T-183, owner request 2026-10-06).

A 400 x 320 m piece of fragment FRG_Raraka_01 around the village F01_Z1 'Uo-mura' is copied onto a flat island west
of the test platform, so that the test map shows exactly what an island of the game shows:
  - ground: M_Terrain_Atoll with the fragment masks cropped to the piece (sand, litter, reef where Raraka has them);
  - vegetation: the rows of DT_FRG_Raraka_01_{Hard,Soft}_NN inside the piece, moved, each kept at its depth under the
    real ground; same meshes and same PCG graphs as the islands (PCG_Raraka_VegHard / VegSoft);
  - the village: DA_Villages_FRG_TestFlat (F01_Z1 moved), built at runtime by a BP_VillageSpawner of the level
    (instance override, the Blueprint defaults of the 20 fragments are not touched: fragment FRG_TestFlat, arrival =
    the PlayerStart of the test map), which clears the vegetation under the houses like in the islands;
  - a marae (AMaraeSite + DA_MaraeRules) and a fishermen camp (ACampSite + DA_CampRules), built at runtime.
The reliquary temple joins once it has a generator (T-180).

    python scripts/map/testflat_zone.py prepare   # offline: tables, mask crop, village payload -> Saved/T183/zone
    python scripts/map/testflat_zone.py apply     # MCP: editor on L_Test_Flat, editor lock held (MONKE_LOCK_OWNER), no PIE
"""
import argparse
import csv
import datetime as dt
import importlib.util
import json
import os
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
REF = ROOT / "docs/reference/map/raraka"
SOURCE = "FRG_Raraka_01"
RAW = ROOT / "docs/reference/map/raw/raraka_fragments" / SOURCE
OUT = ROOT / "monke_island/Saved/T183/zone"
VILLAGE = "F01_Z1"
FRAGMENT = "FRG_TestFlat"
HALF = (20000.0, 16000.0)             # half size of the piece copied and of the island (cm)
TARGET = (-30000.0, 0.0)              # island centre in L_Test_Flat: west of the test platform, past the marae slab
TOP = 0.0                             # island top, at the height of the test platform (cm)
PLAYER_START = (-3000.0, 0.0, 100.0)  # arrival of the test fragment: the village builds once the player starts there
MARAE_OFFSET = (-13000.0, -3000.0)    # from the village centre (cm), out of its clearing
CAMP_OFFSET = (3000.0, 12500.0)
CAUSEWAY = (-7500.0, -3000.0, 5000.0, 600.0)  # centre x, y, length along x, width: island east edge -> platform west edge

LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
UE = f"/Game/MonkeIsland/Map/Fragments/{FRAGMENT}"
TERRAIN = "/Game/MonkeIsland/Materials/M_Terrain_Atoll"
GRAPHS = {"Hard": "/Game/MonkeIsland/Environment/PCG/PCG_Raraka_VegHard",
          "Soft": "/Game/MonkeIsland/Environment/PCG/PCG_Raraka_VegSoft"}
MARAE_RULES = "/Game/MonkeIsland/Environment/Marae/DA_MaraeRules"
CAMP_RULES = "/Game/MonkeIsland/Environment/Camp/DA_CampRules"
CUBE = "/Engine/BasicShapes/Cube"
FOLDER = "TestFlat/Zone_Generateurs"
TX = "editor_toolset.toolsets.texture.TextureTools"
MIT = "editor_toolset.toolsets.material_instance.MaterialInstanceTools"
DTT = "editor_toolset.toolsets.data_table.DataTableTools"
ACT = "editor_toolset.toolsets.actor.ActorTools"
PCG = "PCGToolset.PCGToolset"


def module(path, name):
    spec = importlib.util.spec_from_file_location(name, path)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


def villages_ue():
    return module(ROOT / "scripts/map/fragment_villages_ue.py", "fragment_villages_ue")


# ---------------------------------------------------------------------------------------------- prepare (offline)

def prepare():
    OUT.mkdir(parents=True, exist_ok=True)
    frame = module(ROOT / "scripts/map/place_palms.py", "place_palms").Frame(REF)

    def ground(xs, ys):
        """Height of the Raraka terrain under these points (cm): the fragments keep the world frame of Lvl_Raraka."""
        return np.asarray(frame.mesh_z(np.asarray(xs) / 100.0, np.asarray(ys) / 100.0), dtype=np.float64) * 100.0

    memory = json.loads((REF / "villages/fragments" / f"{SOURCE}.json").read_text(encoding="utf-8"))
    village = next(v for v in memory["villages"] if v["id"] == VILLAGE)
    sx, sy = village["center_cm"][:2]
    dx, dy = TARGET[0] - sx, TARGET[1] - sy

    def inside(x, y):
        return abs(x - sx) <= HALF[0] and abs(y - sy) <= HALF[1]

    # Vegetation of the piece: same rows, moved, each at its depth under the real ground
    tables = {}
    for physics in ("Hard", "Soft"):
        rows = []
        for path in sorted((RAW / "vegetation").glob(f"DT_{SOURCE}_{physics}_*.csv")):
            with path.open(encoding="utf-8", newline="") as f:
                for row in csv.DictReader(f):
                    location, rotation, scale = row["DevComment"].split("|")
                    x, y, z = (float(v) for v in location.split(","))
                    if inside(x, y):
                        rows.append((row["Tag"], x, y, z, rotation, scale))
        depth = np.array([r[3] for r in rows]) - ground([r[1] for r in rows], [r[2] for r in rows])
        name = f"DT_{FRAGMENT}_{physics}_00"
        with (OUT / f"{name}.csv").open("w", encoding="utf-8", newline="") as f:
            writer = csv.writer(f)
            writer.writerow(["---", "Tag", "DevComment"])
            for i, (tag, x, y, _, rotation, scale) in enumerate(rows):
                writer.writerow([f"{physics[0]}{i:06d}", tag, f"{x + dx:.1f},{y + dy:.1f},{TOP + depth[i]:.1f}|{rotation}|{scale}"])
        tables[name] = {"physics": physics, "rows": len(rows), "file": str(OUT / f"{name}.csv")}

    # The village, moved: every point keeps its height above the real ground
    def moved(point):
        x, y = point[0] + dx, point[1] + dy
        if len(point) < 3:
            return [x, y]
        return [x, y, TOP + point[2] - float(ground([point[0]], [point[1]])[0])]

    v = json.loads(json.dumps(village))
    v["center_cm"] = moved(v["center_cm"])
    v["zone"]["center_cm"] = moved(v["zone"]["center_cm"])
    v["zone"]["flag_cm"] = moved(v["zone"]["flag_cm"])
    for group in ("items", "spawns", "pois", "lights", "doors"):
        for entry in v[group]:
            entry["loc_cm"] = moved(entry["loc_cm"])
    v["road_cm"] = [moved(p) for p in v["road_cm"]]
    v["site"]["center_cm"] = moved(v["site"]["center_cm"])
    for shape in v["clear"]:
        if "center" in shape:
            shape["center"] = moved(shape["center"])
        if "points" in shape:
            shape["points"] = [moved(p) for p in shape["points"]]
    v["fragment"] = v["chunk"] = FRAGMENT
    payload = {"FragmentId": FRAGMENT, "Villages": [villages_ue().village_payload(v)]}
    (OUT / f"DA_Villages_{FRAGMENT}.json").write_text(json.dumps(payload), encoding="utf-8")

    # Ground: the fragment masks over the piece (UV0 of M_Terrain_Atoll spans the island face)
    geo = json.loads((REF / "fragments" / SOURCE / "georef.json").read_text(encoding="utf-8"))
    ext = geo["terrain_extent_cm"]
    masks = Image.open(ROOT / geo["masks_texture"]["file"])
    w, h = masks.size
    box = ((sx - HALF[0] - ext[0]) / (ext[2] - ext[0]) * w, (sy - HALF[1] - ext[1]) / (ext[3] - ext[1]) * h,
           (sx + HALF[0] - ext[0]) / (ext[2] - ext[0]) * w, (sy + HALF[1] - ext[1]) / (ext[3] - ext[1]) * h)
    # channel by channel: Pillow resizes RGBA premultiplied, and A (wetness) is 0 on land, which would zero the colours
    region = masks.crop(tuple(round(b) for b in box)).convert("RGBA")
    crop = np.stack([np.asarray(band.resize((256, 256), Image.BICUBIC)) for band in region.split()], axis=-1).copy()
    wet = float((crop[..., 3] > 127).mean())
    crop[..., 3] = 0  # the island is dry: no water tint where Raraka has lagoon
    mask_file = OUT / f"T_{FRAGMENT}_Masks.png"
    Image.fromarray(crop, "RGBA").save(mask_file)

    centre = (TARGET[0], TARGET[1])
    marae = (centre[0] + MARAE_OFFSET[0], centre[1] + MARAE_OFFSET[1], TOP)
    camp = (centre[0] + CAMP_OFFSET[0], centre[1] + CAMP_OFFSET[1], TOP)
    for label, p in (("marae", marae), ("camp", camp)):
        assert abs(p[0] - TARGET[0]) <= HALF[0] - 1500 and abs(p[1] - TARGET[1]) <= HALF[1] - 1500, f"{label} off the island"
        assert np.hypot(p[0] - v["center_cm"][0], p[1] - v["center_cm"][1]) >= v["zone"]["radius_cm"] + 2000, f"{label} in the village"
    zone = {"fragment": FRAGMENT, "source": {"fragment": SOURCE, "village": VILLAGE, "centre_cm": [sx, sy]},
            "island": {"centre_cm": list(TARGET), "half_cm": list(HALF), "top_cm": TOP}, "offset_cm": [dx, dy],
            "tables": tables, "village_payload": str(OUT / f"DA_Villages_{FRAGMENT}.json"),
            "village_centre_cm": v["center_cm"], "mask": str(mask_file), "mask_wet_fraction": round(wet, 3),
            "marae_cm": list(marae), "camp_cm": list(camp), "player_start_cm": list(PLAYER_START),
            "prepared_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "zone.json").write_text(json.dumps(zone, indent=2), encoding="utf-8")
    print(f"[testflat] {SOURCE}/{VILLAGE} -> {FRAGMENT}: village at {[round(c) for c in v['center_cm']]}, "
          f"{', '.join(f'{k} {t['rows']} rows' for k, t in tables.items())}, masks wet {wet:.0%} (made dry), "
          f"marae {[round(c) for c in marae]}, camp {[round(c) for c in camp]}")


# ---------------------------------------------------------------------------------------------- apply (MCP)

def apply():
    zone = json.loads((OUT / "zone.json").read_text(encoding="utf-8"))
    owner = os.environ.get("MONKE_LOCK_OWNER", "")
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    if not owner or lock.get("owner") != owner or dt.datetime.fromisoformat(lock["expires"]) <= dt.datetime.now().astimezone():
        sys.exit(f"the editor lock is not held by MONKE_LOCK_OWNER={owner!r}: nothing done")
    fv = villages_ue()
    mcp = fv.Mcp()
    if mcp.call(fv.APP, "IsPIERunning", {}) is True:
        sys.exit("a PIE is running: nothing done")
    if mcp.call(fv.ST, "get_current_level", {}) != LEVEL:
        sys.exit(f"the editor is not on {LEVEL}: nothing done")

    def exists(path):
        return mcp.call(fv.AT, "exists", {"path": path}) is True

    def props(instance, values):
        return mcp.call(fv.OT, "set_properties", {"instance": instance, "values": json.dumps(values)})

    def actor(label, create):
        for found in mcp.call(fv.ST, "find_actors", {"name": label, "tag": "", "collision_channels": []}) or []:
            if mcp.call(ACT, "get_label", {"actor": found}) == label:
                return found
        made = create()
        mcp.call(ACT, "set_label", {"actor": made, "label": label})
        mcp.call(fv.ST, "set_actor_folder", {"actor": made, "folder_path": FOLDER})
        return made

    def xform(x, y, z, sx=1.0, sy=1.0, sz=1.0, yaw=0.0):
        return {"location": {"x": x, "y": y, "z": z}, "rotation": {"pitch": 0.0, "yaw": yaw, "roll": 0.0},
                "scale": {"x": sx, "y": sy, "z": sz}}

    saved, report = [], {}

    # Ground: fragment masks on M_Terrain_Atoll
    tex, mi = f"{UE}/T_{FRAGMENT}_Masks", f"{UE}/MI_{FRAGMENT}_Terrain"
    if not exists(tex):
        mcp.call(TX, "import_file", {"folder_path": UE, "asset_name": f"T_{FRAGMENT}_Masks", "source_file": zone["mask"]})
    props(fv.ref(tex), {"SRGB": False, "CompressionSettings": "TC_Masks", "LODGroup": "TEXTUREGROUP_World"})
    if not exists(mi):
        mcp.call(MIT, "create", {"folder_path": UE, "asset_name": f"MI_{FRAGMENT}_Terrain", "parent": fv.ref(TERRAIN)})
    mcp.call(MIT, "set_texture_parameter", {"instance": fv.ref(mi), "name": "Masks", "value": fv.ref(tex)})
    saved += [tex, mi]

    # Island and causeway: cubes of 1 m centred on their pivot, top at TOP
    (cx, cy), (hx, hy) = zone["island"]["centre_cm"], zone["island"]["half_cm"]
    slabs = {"TestFlat_Ile_Generateurs": xform(cx, cy, TOP - 100.0, hx / 50.0, hy / 50.0, 2.0),
             "TestFlat_Chaussee": xform(CAUSEWAY[0], CAUSEWAY[1], TOP - 50.0, CAUSEWAY[2] / 100.0, CAUSEWAY[3] / 100.0, 1.0)}
    for label, x in slabs.items():
        a = actor(label, lambda x=x: mcp.call(fv.ST, "add_to_scene_from_asset", {"asset_path": CUBE, "name": label, "xform": x}))
        mcp.call(ACT, "set_actor_transform", {"actor": a, "xform": x, "worldspace": True})
        props({"refPath": a["refPath"] + ".StaticMeshComponent0"}, {"OverrideMaterials": [fv.ref(mi)]})
    report["island"] = list(slabs)

    # Vegetation: DataTables + the PCG graphs of the islands, one volume covering the island
    report["pcg"] = []
    for name, table in zone["tables"].items():
        asset = f"{UE}/Vegetation/{name}"
        if not exists(asset):
            mcp.call(DTT, "import_file", {"folder_path": f"{UE}/Vegetation", "asset_name": name, "source_file": table["file"],
                                          "schema": {"refPath": "/Script/GameplayTags.GameplayTagTableRow"}})
        saved.append(asset)
        params = json.dumps({"DataTable": f"{asset}.{name}"})
        label = "PCG_" + name[3:]
        volume = actor(label, lambda label=label, physics=table["physics"], params=params: mcp.call(
            PCG, "SpawnGraphInstance", {"graph": fv.ref(GRAPHS[physics]), "name": label, "jsonParams": params,
                                        "transform": xform(cx, cy, 0.0, hx / 100.0 + 2.0, hy / 100.0 + 2.0, 20.0)})["actor"])
        mcp.call(PCG, "SetGraphInstanceParams", {"pCGVolume": volume, "jsonParams": params})
        messages = mcp.call(PCG, "ExecuteGraphInstance", {"pCGVolume": volume}, timeout=900)
        report["pcg"].append([label, table["rows"], [m.get("message", "")[:100] for m in (messages or [])][:2]])

    # The village: its data asset, and a spawner of the level that plays only the test fragment
    payload = json.loads(Path(zone["village_payload"]).read_text(encoding="utf-8"))
    sets = f"{fv.DATA}/Fragments"
    data, _ = fv.ensure(mcp, fv.DAT, sets, f"DA_Villages_{FRAGMENT}", "/Script/monke_island.VillageSetDefinition")
    report["village_set"] = props(fv.ref(data), payload)
    saved.append(data)
    vc = zone["village_centre_cm"]
    spawner = actor("TestFlat_VillageSpawner", lambda: mcp.call(fv.ST, "add_to_scene_from_asset", {
        "asset_path": fv.SPAWNER, "name": "TestFlat_VillageSpawner", "xform": xform(vc[0], vc[1], TOP + 600.0)}))
    # emptied first: the instance inherits the 20 fragments of the Blueprint, and a size change with new elements is refused
    props(spawner, {"Fragments": []})
    report["village_spawner"] = props(spawner, {"Fragments": [{"FragmentId": FRAGMENT, "Arrival": fv.vec(zone["player_start_cm"]),
                                                               "Set": fv.ref(data)}]})

    # Marae and fishermen camp of the islands, built at BeginPlay
    m = zone["marae_cm"]
    marae = actor("TestFlat_Marae", lambda: mcp.call(fv.ST, "add_to_scene_from_class", {
        "actor_type": {"refPath": "/Script/monke_island.MaraeSite"}, "name": "TestFlat_Marae", "xform": xform(*m)}))
    report["marae"] = props(marae, {"Rules": fv.ref(MARAE_RULES)})
    c = zone["camp_cm"]
    camp = actor("TestFlat_Camp", lambda: mcp.call(fv.ST, "add_to_scene_from_class", {
        "actor_type": {"refPath": "/Script/monke_island.CampSite"}, "name": "TestFlat_Camp", "xform": xform(*c)}))
    report["camp"] = props(camp, {"Rules": fv.ref(CAMP_RULES)})  # CampSeed is not editable: the camp keeps its default seed

    report["saved_assets"] = mcp.call(fv.AT, "save_assets", {"asset_paths": saved}, timeout=600)
    report["saved_level"] = mcp.call(fv.AT, "save_assets", {"asset_paths": [LEVEL]}, timeout=600)
    report["applied_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT / "apply-result.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    print(json.dumps(report, indent=1)[:3000])


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("step", choices=["prepare", "apply"])
    {"prepare": prepare, "apply": apply}[ap.parse_args().step]()


if __name__ == "__main__":
    main()
