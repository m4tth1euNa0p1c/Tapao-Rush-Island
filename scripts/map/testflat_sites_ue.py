"""T-187: the pearl marae and the reliquary temple, each in a distant area of the Tuamotu zone of L_Test_Flat (owner
request 2026-10-06: "ajoute dans la zone tuamotu le marae perle et le temple artefact dans des zones éloignées").

- DA_TempleReliquaryRules (UMaraeRules, built at runtime by AMaraeSite like the marae: vegetation cleared around it,
  quiet zone, pieces with their UCX): the 62 placements of the maquette temple-reliquaire (scene_layout, modules of
  T-179 by key), in the frame of the module FBX (x, -y, z; yaw = -rot_z); on the plinths of the altar, the four
  large legendary artifacts (BP_Artifact_*, T-189). Reusable by T-180 (a temple in every played island).
- The marae (TestFlat_Marae) moves to the far south-west of the island, the temple (TestFlat_Temple) stands in the far
  north-east, its gate turned towards the village: both ~175 m from Uo-mura, ~370 m from each other.
Runs in the editor: scripts/editor/lead_session_t186.py, action "sites".
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

OUT = ROOT / "monke_island/Saved/T187"
STAGE = ROOT / "art/export/OneDriveShowcase/20261006/temple-reliquaire"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
TEMPLE = "/Game/MonkeIsland/Environment/TempleReliquary"
RULES = TEMPLE + "/Data/DA_TempleReliquaryRules"
ARTIFACTS = "/Game/MonkeIsland/Items/PolynesianArtifacts/Blueprints/BP_Artifact_"
LEGENDARY = ("HeartOfTheReef", "FishGodFragment", "EclipseStone", "CrownOfTheLastAriki")
FOLDER = "TestFlat/Zone_Generateurs"
VILLAGE = (-30000.0, 0.0)
MARAE = ("TestFlat_Marae", (-44000.0, -10500.0))
TEMPLE_SITE = ("TestFlat_Temple", (-16000.0, 10500.0))
GATE_DIRECTION = 90.0            # the gate of the maquette (y -8 m in Blender) looks along +Y in the module frame
CLEAR_RADIUS = 1900.0            # enclosure 15 x 15 m plus the fare at x 12.5 m
EAL = ue.EditorAssetLibrary


def transform(pos, yaw):
    return ue.Transform(location=ue.Vector(pos[0] * 100.0, -pos[1] * 100.0, pos[2] * 100.0),
                        rotation=ue.Rotator(roll=0.0, pitch=0.0, yaw=-yaw), scale=ue.Vector(1.0, 1.0, 1.0))


def temple_rules():
    layout = json.loads((STAGE / "maquette.json").read_text(encoding="utf-8"))["params"]["scene_layout"]
    report = json.loads((STAGE / "temple-report.json").read_text(encoding="utf-8"))
    by_key = {info["key"]: name for name, info in report["modules"].items()}
    pieces = []

    def piece(key, pos, yaw):
        mesh = kitlib.load(f"{TEMPLE}/Meshes/{by_key[key]}")
        assert mesh, key
        p = ue.MaraePiece()
        p.set_editor_property("mesh", mesh)
        p.set_editor_property("transform", transform(pos, yaw))
        p.set_editor_property("follow_ground", False)
        p.set_editor_property("collision", True)
        pieces.append(p)

    for key in ("court_paving", "relic_altar", "ahu", "gate", "fare_ia_manaha", "fata"):
        piece(key, layout[key]["pos"], layout[key]["rot_z"])
    for category in ("walls", "palisade"):
        for row in layout[category]:
            piece(row["m"], row["pos"], row["rot_z"])
    for category, prefix in (("tiki", "tiki_"), ("standing_stones", "standing_stone_")):
        for row in layout[category]:
            piece(prefix + row["variant"], row["pos"], row["rot_z"])
    for row in layout["debris"]:
        piece("debris_" + row["m"], row["pos"], row["rot_z"])
    for index, pos in enumerate(layout["torches"]):
        piece("torch", pos, index * 47)
    assert len(pieces) == report["scene"]["instances_total"], len(pieces)

    spots = []
    for name in LEGENDARY:
        cls = EAL.load_blueprint_class(ARTIFACTS + name)
        assert cls, name
        box = report["scene"]["relics"][name]["world_bbox"]
        bottom = [(box["min"][0] + box["max"][0]) / 2, (box["min"][1] + box["max"][1]) / 2, box["min"][2] + 0.01]
        spot = ue.MaraeActorSpot()
        spot.set_editor_property("actor_class", cls)
        spot.set_editor_property("transform", transform(bottom, 0.0))
        spots.append(spot)

    rules = kitlib.load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.MaraeRules)
        rules = ue.AssetToolsHelpers.get_asset_tools().create_asset("DA_TempleReliquaryRules", TEMPLE + "/Data",
                                                                    ue.MaraeRules, factory)
    assert rules, RULES
    rules.set_editor_property("pieces", pieces)
    rules.set_editor_property("actors", spots)
    rules.set_editor_property("structure_offset_z", 0.0)
    rules.set_editor_property("clear_radius", CLEAR_RADIUS)
    rules.set_editor_property("quiet_radius", CLEAR_RADIUS)
    kitlib.save(rules)
    return rules, len(pieces), len(spots)


def main():
    kitlib.check_lock()
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world or "L_Test_Flat" not in world.get_path_name():
        assert levels.load_level(LEVEL)
    rules, pieces, spots = temple_rules()
    actors_sub = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    actors = {a.get_actor_label(): a for a in actors_sub.get_all_level_actors()}

    marae = actors.get(MARAE[0])
    assert marae, "TestFlat_Marae is missing (scripts/map/testflat_zone.py apply)"
    marae.set_actor_location(ue.Vector(MARAE[1][0], MARAE[1][1], 0.0), False, False)

    label, (x, y) = TEMPLE_SITE
    facing = math.degrees(math.atan2(VILLAGE[1] - y, VILLAGE[0] - x))
    rotation = ue.Rotator(roll=0.0, pitch=0.0, yaw=facing - GATE_DIRECTION)
    temple = actors.get(label)
    if not temple:
        temple = actors_sub.spawn_actor_from_class(ue.MaraeSite, ue.Vector(x, y, 0.0), rotation)
        temple.set_actor_label(label)
    temple.set_actor_location(ue.Vector(x, y, 0.0), False, False)
    temple.set_actor_rotation(rotation, False)
    temple.set_editor_property("rules", rules)
    temple.set_folder_path(FOLDER)
    assert levels.save_current_level()

    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-187", "temple_rules": RULES, "pieces": pieces, "relics": spots,
              "marae": [MARAE[1][0], MARAE[1][1]], "temple": [x, y], "temple_yaw": round(facing - GATE_DIRECTION, 1),
              "distance_m": round(math.dist(MARAE[1], (x, y)) / 100.0, 1),
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "sites-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T187_SITES_SUCCESS")


if __name__ == "__main__":
    main()
