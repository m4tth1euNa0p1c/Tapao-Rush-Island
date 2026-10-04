"""Marae des Perles in every fragment (owner request 2026-10-03: more marae): DA_MaraeRules from the layout of the hand
placed marae (art/export/Props/Marae/marae_layout.json, scripts/props/marae_layout.py); ACampSpawner rebuilds it
(AMaraeSite) on a free spot of the played fragment.

Editor commandlet (editor closed, editor built with UMaraeRules / AMaraeSite):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Idempotent. The spawner of Lvl_Raraka gets the asset by MCP (CampSpawner_0.MaraeRules).
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
LAYOUT = json.load(open(os.path.join(ROOT, "art", "export", "Props", "Marae", "marae_layout.json"), encoding="utf-8"))
DEST = "/Game/MonkeIsland/Environment/Marae"
NO_COLLISION = ("SM_Grass", "SM_Fern", "SM_CoralRubble", "SM_Coconut_", "SM_CoconutFrond")

eal = unreal.EditorAssetLibrary


def transform(item):
    t = unreal.Transform()
    t.translation = unreal.Vector(*item["loc"])
    t.rotation = unreal.Rotator(0.0, 0.0, item["yaw"]).quaternion()
    t.scale3d = unreal.Vector(item["scale"], item["scale"], item["scale"])
    return t


def main():
    path = f"{DEST}/DA_MaraeRules"
    rules = unreal.load_asset(path) if eal.does_asset_exist(path) else None
    if not rules:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.MaraeRules)
        rules = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_MaraeRules", DEST, unreal.MaraeRules, factory)
    pieces, actors = [], []
    for item in LAYOUT["items"]:
        asset = unreal.load_asset(item["asset"])
        if not asset:
            unreal.log_warning(f"[marae] missing {item['asset']}")
            continue
        name = item["asset"].rsplit("/", 1)[-1]
        if item["kind"] == "mesh":
            piece = unreal.MaraePiece()
            piece.set_editor_property("mesh", asset)
            piece.set_editor_property("transform", transform(item))
            piece.set_editor_property("follow_ground", item["folder"] == "Vegetation")
            piece.set_editor_property("collision", not name.startswith(NO_COLLISION))
            pieces.append(piece)
        else:
            spot = unreal.MaraeActorSpot()
            spot.set_editor_property("actor_class", asset.generated_class())
            spot.set_editor_property("transform", transform(item))
            actors.append(spot)
    rules.set_editor_property("pieces", pieces)
    rules.set_editor_property("actors", actors)
    eal.save_loaded_asset(rules)
    unreal.log_warning(f"[marae] DA_MaraeRules: {len(pieces)} pieces, {len(actors)} actors")
    # more huts too (owner feedback 2026-10-04: not enough marae, villages and huts): as many as the fragment holds
    camp = unreal.load_asset("/Game/MonkeIsland/Environment/Camp/DA_CampRules")
    camp.set_editor_property("count_per_fragment", 8)
    eal.save_loaded_asset(camp)


main()
