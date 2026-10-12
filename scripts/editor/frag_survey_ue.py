"""Survey of the expedition map for the fort island (T-205): arrival points, an island definition, the catalog, the
navigation mode, and the lagoon floor west of FRG_Raraka_01. Read only; loads L_Frag_Raraka_01 if needed.
Writes monke_island/Saved/T186/frag-survey.json. Action "fragsurvey" of scripts/editor/lead_session_t186.py.
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
LEVEL = "/Game/MonkeIsland/Maps/Fragments/L_Frag_Raraka_01"


def props(obj, names):
    out = {}
    for n in names:
        try:
            v = obj.get_editor_property(n)
            out[n] = str(v)[:300]
        except Exception as error:  # noqa: BLE001
            out[n] = f"? {error}"[:80]
    return out


world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
if not world.get_path_name().startswith(LEVEL):
    assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).load_level(LEVEL), "map not loaded"
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
report = {"arrivals": [], "nav": [], "level_instances": [], "floor": []}
for a in actors:
    cls = a.get_class().get_name()
    label = a.get_actor_label()
    if label.startswith("ARR_") or "Arrival" in cls or "Anchorage" in cls or "Mooring" in cls:
        l, r = a.get_actor_location(), a.get_actor_rotation()
        row = {"label": label, "class": cls, "at": [round(l.x), round(l.y), round(l.z)], "yaw": round(r.yaw, 1),
               "folder": str(a.get_folder_path())}
        row.update(props(a, ["fragment_id", "anchorage_id", "role", "tags"]))
        report["arrivals"].append(row)
    if cls in ("RecastNavMesh", "NavMeshBoundsVolume", "NavigationData"):
        o, e = a.get_actor_bounds(False)
        row = {"label": label, "class": cls, "origin": [round(o.x), round(o.y), round(o.z)], "extent": [round(e.x), round(e.y), round(e.z)]}
        if cls == "RecastNavMesh":
            row.update(props(a, ["runtime_generation", "cell_size", "agent_radius"]))
        report["nav"].append(row)
    if "LevelInstance" in cls:
        report["level_instances"].append({"label": label, "class": cls, "at": [round(a.get_actor_location().x), round(a.get_actor_location().y)]})
report["actors_total"] = len(actors)

d = ue.EditorAssetLibrary.load_asset("/Game/MonkeIsland/Data/Islands/DA_Island_Raraka_01")
if d:
    report["definition_01"] = props(d, ["fragment_id", "display_name", "map", "pacifiable_zone_count", "anchorages", "land_area_ha",
                                        "difficulty", "variant_seeds", "credits"])
c = ue.EditorAssetLibrary.load_asset("/Game/MonkeIsland/Data/Islands/DA_IslandCatalog")
if c:
    report["catalog"] = props(c, ["islands", "fragments", "offered", "entries"])
report["islands_dir"] = sorted(ue.EditorAssetLibrary.list_assets("/Game/MonkeIsland/Data/Islands", False, False))

# lagoon floor west of FRG_Raraka_01 (the galleon side), every 250 m
start = ue.Vector(1181314, 166316, 0)
for step in range(0, 25):
    x = start.x - step * 25000.0
    hit = ue.SystemLibrary.line_trace_single(world, ue.Vector(x, start.y, 5000.0), ue.Vector(x, start.y, -8000.0),
                                             ue.TraceTypeQuery.TRACE_TYPE_QUERY1, True, [], ue.DrawDebugTrace.NONE, True)
    t = hit.to_tuple() if hit else None
    z = round(t[4].z) if t and t[0] else None
    comp = t[10].get_name() if t and t[0] and t[10] else None
    report["floor"].append({"x": round(x), "z": z, "comp": comp})
(ROOT / "monke_island/Saved/T186/frag-survey.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
