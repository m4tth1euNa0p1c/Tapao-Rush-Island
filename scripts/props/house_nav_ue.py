"""Villagers of the village houses (owner requests 2026-10-07), in Unreal.

- DA_MinkaKit: one villager per minka (Residents 1), the waypoints of its routine (scripts/props/minka_nav.py: 17
  points in the doma and the five rooms, their links through the doorways), the node inside the door, the point
  outside the door, the node before the tokonoma where the Tidal Relic stands, and the inside of the house (theft).
- DA_ShrineKit: no villager; its doors stay shut until the flag of the village is up (bSealedUntilFlag), then open.
Runs in the editor: scripts/editor/lead_session_t186.py, action "housenav" (editor lock held, no game running).
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402
import minka_nav  # noqa: E402

OUT = ROOT / "monke_island/Saved/HouseNav"
DATA = "/Game/MonkeIsland/Environment/Village/Data"
MINKA = DATA + "/DA_MinkaKit"
SHRINE = DATA + "/DA_ShrineKit"


def vector(values):
    return ue.Vector(float(values[0]), float(values[1]), float(values[2]))


def main():
    kitlib.check_lock()
    data = minka_nav.check()
    minka = kitlib.load(MINKA)
    assert minka, MINKA
    nodes = []
    for entry in data["nodes"]:
        node = ue.VillageHouseNode()
        node.set_editor_property("location", vector(entry["location"]))
        node.set_editor_property("room", entry["room"])
        node.set_editor_property("links", entry["links"])
        node.set_editor_property("activity", entry["activity"])
        node.set_editor_property("yaw", entry["yaw"])
        nodes.append(node)
    lo, hi = data["interior"]
    interior = ue.Box()
    interior.set_editor_property("min", vector(lo))
    interior.set_editor_property("max", vector(hi))
    interior.set_editor_property("is_valid", 1)
    minka.set_editor_property("residents", 1)
    minka.set_editor_property("nodes", nodes)
    minka.set_editor_property("door_node", data["door_node"])
    minka.set_editor_property("relic_node", data["relic_node"])
    minka.set_editor_property("outside_location", vector(data["outside"]))
    minka.set_editor_property("interior", interior)
    kitlib.save(minka)

    shrine = kitlib.load(SHRINE)
    assert shrine, SHRINE
    shrine.set_editor_property("residents", 0)
    shrine.set_editor_property("sealed_until_flag", True)
    kitlib.save(shrine)

    check = kitlib.load(MINKA)
    stored = check.get_editor_property("interior")
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"minka": MINKA, "nodes": len(check.get_editor_property("nodes")), "door_node": check.get_editor_property("door_node"),
              "relic_node": check.get_editor_property("relic_node"),
              "interior": [[stored.min.x, stored.min.y, stored.min.z], [stored.max.x, stored.max.y, stored.max.z]],
              "shrine_sealed": shrine.get_editor_property("sealed_until_flag"),
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    assert result["nodes"] == len(data["nodes"]), result
    (OUT / "housenav-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("HOUSE_NAV_SUCCESS")


if __name__ == "__main__":
    main()
