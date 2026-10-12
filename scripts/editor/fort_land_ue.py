"""The French forts on the land of the Raraka atoll (owner 2026-10-08: "fais en sorte que les forts français spawn sur la
map de Raraka Tuamotu et pas sur une île personnalisée, fais apparaître le temple artefact, les crabes, les forts dès le
jour 1", "crée une map et génère procéduralement").

Steps, read from monke_island/Saved/T186/fortland-request.json ({"steps": [...]}), each reported on its own:
- clean: the editor open on LI_FortFrench_Land (a copy of LI_FortFrench): the island around the fort goes (far terrain,
  lagoon, port, pier, its vegetation and PCG volumes, the sign, its navmesh); the plateau of the fort (SM_Fort_TerrainNear:
  court, moat, buried treasure vault) stays: the fort stands on it as on a mound. A ramp of sand goes down from the gate's
  glacis, where the pier was (a 3 m quay), to the ground of the atoll. The level is saved.
- rules: DA_FortSiteRules (UFortSiteRules) from docs/reference/map/raraka/collectibles/fort_sites.json
  (scripts/map/place_forts.py): the land fort level and the sites of each fragment.
- unlocks: DA_RunRules: the reliquary temple, the giant crabs and the forts from day 1.
- catalog: the fort island of the lagoon (FRG_Raraka_21) leaves DA_IslandCatalog.
- fragone: L_Frag_Raraka_01 loses the level instance of the fort island (FRG_Raraka_01 plays that map); saved.
- templeraraka: the camp spawner of Lvl_Raraka (fragments 02 to 20) gets DA_TempleReliquaryRules (it had none: no
  temple on those fragments before); saved.
Writes monke_island/Saved/T186/fortland.json. Action "fortland" of scripts/editor/lead_session_t186.py.
"""
import datetime as dt
import json
import math
import sys
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

REQUEST = ROOT / "monke_island/Saved/T186/fortland-request.json"
OUT = ROOT / "monke_island/Saved/T186/fortland.json"
SITES = ROOT / "docs/reference/map/raraka/collectibles/fort_sites.json"
LAND = "/Game/MonkeIsland/Maps/Fragments/LI_FortFrench_Land"
SOURCE_LI = "/Game/MonkeIsland/Maps/Fragments/LI_FortFrench"
FRAG_ONE = "/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01"
RARAKA = "/Game/MonkeIsland/Maps/Lvl_Raraka"
DATA = "/Game/MonkeIsland/Environment/FortFrench/Data"
RULES = DATA + "/DA_FortSiteRules"
RUN_RULES = "/Game/MonkeIsland/Expedition/Data/DA_RunRules"
CATALOG = "/Game/MonkeIsland/Data/Islands/DA_IslandCatalog"
TEMPLE_RULES = "/Game/MonkeIsland/Environment/TempleReliquary/Data/DA_TempleReliquaryRules"
GROUND_MATERIAL = "/Game/MonkeIsland/Environment/FortFrench/Materials/M_Site_Ground"
CUBE = "/Engine/BasicShapes/Cube"

# the island of the lagoon fort, gone on the atoll
DELETE_LABELS = {"Fort_T193_TerrainFar", "Fort_T193_Lagoon", "Fort_T193_Ile_Generateurs_Extension", "Fort_T193_Light_202",
                 "RecastNavMesh-Default"}
DELETE_FOLDERS = ("/Vegetation", "/Port", "/Access", "/RarakaPlants")
PROPS_FOLDER = "/Props"
PIER_Y = 3500.0          # props beyond the gate's glacis stood on the pier
# the ramp from the glacis (court level 165, the quay at 40 m on +Y) down to below the ground of the atoll
RAMP_TOP = (3900.0, 165.0)
RAMP_BOTTOM = (5200.0, -135.0)
RAMP_WIDTH = 700.0
RAMP_THICKNESS = 60.0
RAMP_LABEL = "FortLand_GateRamp"
report = {"started_at": dt.datetime.now(dt.timezone.utc).isoformat()}


def actors():
    return ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()


def world_path():
    return ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world().get_path_name()


def load_level(path):
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    if not world_path().startswith(path):
        assert levels.load_level(path), path
    return levels


def clean():
    assert world_path().startswith(LAND), f"open {LAND} first, not {world_path()}"
    deleted, kept = [], 0
    eas = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    for actor in list(actors()):
        label = actor.get_actor_label()
        folder = str(actor.get_folder_path())
        doomed = (label in DELETE_LABELS or folder.endswith(DELETE_FOLDERS) or isinstance(actor, ue.TextRenderActor)
                  or (folder.endswith(PROPS_FOLDER) and actor.get_actor_location().y > PIER_Y)
                  or label == RAMP_LABEL)
        if doomed:
            deleted.append(label)
            eas.destroy_actor(actor)
        else:
            kept += 1
    # the ramp: a box of sand, its top from the glacis down past the quay
    (y0, z0), (y1, z1) = RAMP_TOP, RAMP_BOTTOM
    angle = math.atan2(z0 - z1, y1 - y0)
    length = math.hypot(y1 - y0, z0 - z1) + 40.0
    mid_y, mid_z = (y0 + y1) / 2.0, (z0 + z1) / 2.0
    centre = ue.Vector(0.0, mid_y - math.sin(angle) * RAMP_THICKNESS / 2.0, mid_z - math.cos(angle) * RAMP_THICKNESS / 2.0)
    ramp = eas.spawn_actor_from_object(ue.EditorAssetLibrary.load_asset(CUBE), centre, ue.Rotator(roll=math.degrees(angle), pitch=0.0, yaw=0.0))
    ramp.set_actor_label(RAMP_LABEL)
    ramp.set_folder_path("FortFrench/Land")
    ramp.set_actor_scale3d(ue.Vector(RAMP_WIDTH / 100.0, length / 100.0, RAMP_THICKNESS / 100.0))
    comp = ramp.static_mesh_component
    material = ue.EditorAssetLibrary.load_asset(GROUND_MATERIAL)
    if material:
        comp.set_material(0, material)
    # where its top really is: the glacis end and the foot
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    probes = {}
    for y in (y0 + 150.0, (y0 + y1) / 2.0, y1 - 150.0):
        hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(0.0, y, 2000.0), ue.Vector(0.0, y, -2000.0),
                                                 ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], ue.DrawDebugTrace.NONE, True)
        t = hit.to_tuple() if hit else None
        owner = t[10].get_owner().get_actor_label() if t and t[0] and t[10] and t[10].get_owner() else None
        probes[str(round(y))] = [round(t[4].z) if t and t[0] else None, owner]
    expected = {str(round(y)): round(z0 - (y - y0) * (z0 - z1) / (y1 - y0)) for y in (y0 + 150.0, (y0 + y1) / 2.0, y1 - 150.0)}
    saved = ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()
    return {"deleted": len(deleted), "deleted_labels": deleted, "kept": kept, "ramp_angle_deg": round(math.degrees(angle), 2),
            "ramp_probes": probes, "ramp_expected_top": expected, "saved": saved}


def rules():
    data = json.loads(SITES.read_text(encoding="utf-8"))
    asset = kitlib.load(RULES)
    if not asset:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.FortSiteRules)
        asset = ue.AssetToolsHelpers.get_asset_tools().create_asset("DA_FortSiteRules", DATA, ue.FortSiteRules, factory)
    assert asset, RULES
    level = ue.EditorAssetLibrary.load_asset(LAND)
    assert level, LAND
    asset.set_editor_property("fort_level", level)
    fragments = []
    total = 0
    for fragment_id, sites in sorted(data["by_fragment"].items()):
        entry = ue.FortFragmentSites()
        entry.set_editor_property("fragment_id", fragment_id)
        spots = []
        for x, y, z, yaw, *_ in sites:
            spot = ue.FortSiteSpot()
            spot.set_editor_property("location", ue.Vector(float(x), float(y), float(z)))
            spot.set_editor_property("yaw", float(yaw))
            spots.append(spot)
        entry.set_editor_property("sites", spots)
        fragments.append(entry)
        total += len(spots)
    asset.set_editor_property("fragments", fragments)
    kitlib.save(asset)
    return {"asset": RULES, "fragments": len(fragments), "sites": total, "level": LAND}


def unlocks():
    asset = kitlib.load(RUN_RULES)
    assert asset, RUN_RULES
    campaign = asset.get_editor_property("campaign")
    before = {k: campaign.get_editor_property(k) for k in ("temples_from_day", "forts_from_day", "giant_crabs_from_day")}
    for key in before:
        campaign.set_editor_property(key, 1)
    asset.set_editor_property("campaign", campaign)
    kitlib.save(asset)
    after = {k: asset.get_editor_property("campaign").get_editor_property(k) for k in before}
    return {"before": before, "after": after}


def catalog():
    asset = kitlib.load(CATALOG)
    assert asset, CATALOG
    ids = list(asset.get_editor_property("fragments"))
    kept = [i for i in ids if str(i.get_editor_property("primary_asset_name")) != "FRG_Raraka_21"]
    asset.set_editor_property("fragments", kept)
    kitlib.save(asset)
    return {"before": len(ids), "after": len(kept), "names": [str(i.get_editor_property("primary_asset_name")) for i in kept]}


def fragone():
    levels = load_level(FRAG_ONE)
    eas = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    removed = []
    for actor in list(actors()):
        if actor.get_class().get_name() == "LevelInstance" and SOURCE_LI in str(actor.get_editor_property("world_asset")):
            removed.append(actor.get_actor_label())
            eas.destroy_actor(actor)
    saved = levels.save_current_level() if removed else False
    return {"removed": removed, "saved": saved}


def templeraraka():
    levels = load_level(RARAKA)
    temple = ue.EditorAssetLibrary.load_asset(TEMPLE_RULES)
    assert temple, TEMPLE_RULES
    spawners = [a for a in actors() if isinstance(a, ue.CampSpawner)]
    assert spawners, "no ACampSpawner in Lvl_Raraka"
    out = []
    for spawner in spawners:
        before = spawner.get_editor_property("temple_rules")
        spawner.set_editor_property("temple_rules", temple)
        out.append({"spawner": spawner.get_actor_label(), "before": before.get_name() if before else None,
                    "temple_class": str(spawner.get_editor_property("temple_class"))})
    return {"spawners": out, "saved": levels.save_current_level()}


STEPS = {"clean": clean, "rules": rules, "unlocks": unlocks, "catalog": catalog, "fragone": fragone, "templeraraka": templeraraka}


def main():
    kitlib.check_lock()
    steps = json.loads(REQUEST.read_text(encoding="utf-8")).get("steps", [])
    for step in steps:
        try:
            report[step] = STEPS[step]()
        except Exception:  # noqa: BLE001
            report[step] = {"error": traceback.format_exc()[-1500:]}
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")


main()
