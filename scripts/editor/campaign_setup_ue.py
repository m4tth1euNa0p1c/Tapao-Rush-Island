"""T-200 / T-202: the data of the ten-day campaign (owner requests 2026-10-07, docs/features/campagne-dix-jours.md).

- DA_RunRules: the three stages (5 500 by day 3, 10 500 by day 6, 20 500 by day 10, the last stage 4 days with longer
  days), and the days the content opens (Campaign): first temple on day 3, forts on day 6, every building and the other
  artifact temples on day 7, giant crabs on day 4 (owner, 2026-10-07: "les crabes apparaissent au 4e jour");
- BP_Artifact_MoonPearl and BP_Artifact_RedPearlOfRaraka (APhysicsPropActor like the four others of T-189, mass and
  value of the maquette's report); the pearls of 2.3 cm are shown three times bigger (PEARL_SCALE) so a player sees and
  grabs them on their pedestal;
- DA_TempleReliquaryRules: the two pedestals of the pearls (positions of the maquette temple-reliquaire) added to the
  four of T-187, and ArtifactPool = the six legendary artifacts: the campaign deals them to the temples (one each).
Runs in the editor: scripts/editor/lead_session_t186.py, action "campaignsetup". Replays set the same values.
Result: monke_island/Saved/T200/campaign-setup.json
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T200"
RUN_RULES = "/Game/MonkeIsland/Expedition/Data/DA_RunRules"
TEMPLE_RULES = "/Game/MonkeIsland/Environment/TempleReliquary/Data/DA_TempleReliquaryRules"
ARTIFACTS = "/Game/MonkeIsland/Items/PolynesianArtifacts"
REPORT = Path("C:/Users/matth/OneDrive/maquette-blender/maquettes/artefacts-polynesiens/artifacts-report.json")
TEMPLE_REPORT = ROOT / "art/export/OneDriveShowcase/20261006/temple-reliquaire/temple-report.json"
SIX = ("HeartOfTheReef", "FishGodFragment", "EclipseStone", "CrownOfTheLastAriki", "MoonPearl", "RedPearlOfRaraka")
PEARLS = ("MoonPearl", "RedPearlOfRaraka")
PEARL_SCALE = 3.0
# id, name, quota, days, spawn, loot, flood (the C++ defaults of URunRules, set on the asset in case it kept older ones)
STAGES = [("Villages", "Villages et marae", 5500, 3, 1.0, 1.0, 1.0), ("Temples", "Temples et forts", 10500, 3, 1.3, 1.5, 1.0),
          ("GreatAtoll", "Grand atoll", 20500, 4, 1.7, 2.2, 1.5)]
CAMPAIGN = {"temples_from_day": 3, "forts_from_day": 6, "all_buildings_from_day": 7, "all_artifacts_from_day": 7,
            "giant_crabs_from_day": 4, "temples_per_fragment_first": 1, "temples_per_fragment_all": 3, "artifacts_per_temple": 1}
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def run_rules():
    rules = kitlib.load(RUN_RULES)
    assert rules, RUN_RULES
    zones = []
    for zone_id, name, quota, days, spawn, loot, flood in STAGES:
        zone = ue.RunZone()
        zone.set_editor_property("zone_id", zone_id)
        zone.set_editor_property("display_name", name)
        zone.set_editor_property("quota", quota)
        zone.set_editor_property("days", days)
        zone.set_editor_property("spawn_scale", spawn)
        zone.set_editor_property("loot_value_scale", loot)
        zone.set_editor_property("flood_delay_scale", flood)
        zones.append(zone)
    rules.set_editor_property("zones", zones)
    campaign = rules.get_editor_property("campaign")
    for name, value in CAMPAIGN.items():
        campaign.set_editor_property(name, value)
    rules.set_editor_property("campaign", campaign)
    kitlib.save(rules)
    return {"zones": [s[:4] for s in STAGES], "campaign": CAMPAIGN}


def pearl_class(name, info):
    mesh = kitlib.load(f"{ARTIFACTS}/Meshes/SM_{name}")
    assert mesh, name
    path = f"{ARTIFACTS}/Blueprints/BP_Artifact_{name}"
    blueprint = kitlib.load(path)
    if not blueprint:
        factory = ue.BlueprintFactory()
        factory.set_editor_property("parent_class", ue.PhysicsPropActor)
        blueprint = TOOLS.create_asset(f"BP_Artifact_{name}", f"{ARTIFACTS}/Blueprints", ue.Blueprint, factory)
    assert blueprint, path
    cdo = ue.get_default_object(blueprint.generated_class())
    body = cdo.get_editor_property("mesh")
    body.set_editor_property("static_mesh", mesh)
    body.set_editor_property("relative_scale3d", ue.Vector(PEARL_SCALE, PEARL_SCALE, PEARL_SCALE))
    body.set_linear_damping(0.35)
    body.set_angular_damping(0.6)
    cdo.set_editor_property("mass_kg", max(0.1, float(info["mass_kg"])))
    cdo.get_editor_property("valuable").set_editor_property("base_value", int(info["value"]))
    ue.BlueprintEditorLibrary.compile_blueprint(blueprint)
    kitlib.save(blueprint)
    return blueprint.generated_class()


def spot_transform(pos):
    return ue.Transform(location=ue.Vector(pos[0] * 100.0, -pos[1] * 100.0, pos[2] * 100.0),
                        rotation=ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0), scale=ue.Vector(1.0, 1.0, 1.0))


def temple_rules(classes):
    rules = kitlib.load(TEMPLE_RULES)
    assert rules, TEMPLE_RULES
    relics = json.loads(TEMPLE_REPORT.read_text(encoding="utf-8"))["scene"]["relics"]
    spots = list(rules.get_editor_property("actors"))
    present = {s.get_editor_property("actor_class") for s in spots}
    added = []
    for name in PEARLS:
        cls = classes[name]
        if cls in present:
            continue
        box = relics[name]["world_bbox"]
        bottom = [(box["min"][0] + box["max"][0]) / 2, (box["min"][1] + box["max"][1]) / 2, box["min"][2] + 0.01]
        spot = ue.MaraeActorSpot()
        spot.set_editor_property("actor_class", cls)
        spot.set_editor_property("transform", spot_transform(bottom))
        spots.append(spot)
        added.append(name)
    rules.set_editor_property("actors", spots)
    rules.set_editor_property("artifact_pool", [classes[name] for name in SIX])
    kitlib.save(rules)
    return {"pedestals": len([s for s in spots if s.get_editor_property("actor_class") in classes.values()]), "added": added,
            "pool": list(SIX)}


def main():
    kitlib.check_lock()
    report = json.loads(REPORT.read_text(encoding="utf-8"))["objects"]
    classes = {}
    for name in SIX:
        cls = ue.EditorAssetLibrary.load_blueprint_class(f"{ARTIFACTS}/Blueprints/BP_Artifact_{name}") if name not in PEARLS else None
        classes[name] = cls or pearl_class(name, report[name])
        assert classes[name], name
    result = {"task": "T-200", "run_rules": run_rules(), "temple": temple_rules(classes),
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT / "campaign-setup.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T200_CAMPAIGN_SETUP_SUCCESS")


if __name__ == "__main__":
    main()
