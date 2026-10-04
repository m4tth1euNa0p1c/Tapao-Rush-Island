"""Camps in the played fragment (owner request 2026-10-03: villages and camps in the play zones): fills
DA_CampRules.FragmentSpots from docs/reference/map/raraka/collectibles/camp_spots.json "by_fragment"
(scripts/map/place_camps.py) and refreshes the island wide Spots.

Editor commandlet (editor closed, editor built with FCampFragmentSpots):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DATA = json.load(open(os.path.join(ROOT, "docs", "reference", "map", "raraka", "collectibles", "camp_spots.json"),
                      encoding="utf-8"))
RULES = "/Game/MonkeIsland/Environment/Camp/DA_CampRules"

rules = unreal.load_asset(RULES)
spots = DATA["spots"]
rules.set_editor_property("spots", [unreal.Vector(*s) for s in spots])
entries = []
for fragment_id, indices in sorted(DATA.get("by_fragment", {}).items()):
    entry = unreal.CampFragmentSpots()
    entry.set_editor_property("fragment_id", fragment_id)
    entry.set_editor_property("spots", [unreal.Vector(*spots[i]) for i in indices])
    entries.append(entry)
rules.set_editor_property("fragment_spots", entries)
unreal.EditorAssetLibrary.save_loaded_asset(rules)
unreal.log_warning(f"[camp] DA_CampRules: {len(entries)} fragments, {sum(len(e.get_editor_property('spots')) for e in entries)} spots, {len(spots)} island wide")
