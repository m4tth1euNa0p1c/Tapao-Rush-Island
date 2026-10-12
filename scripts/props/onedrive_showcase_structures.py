"""Place the existing complete Marae and VillageA maquettes in Test Flat.

Only adds/refreshes actors tagged for T-179. Existing meshes, Blueprints,
materials and production villages remain unchanged.
"""
import datetime as dt
import json
import math
from pathlib import Path
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T179"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
TAG = "OneDriveStructures_T179"
VILLAGE = "/Game/MonkeIsland/Environment/Village"
MARAE = "/Game/MonkeIsland/Environment/Marae"


def main():
    lock = json.loads((ROOT / ".claude/factory/editor.lock.json").read_text())
    assert lock["owner"] == "codex" and dt.datetime.fromisoformat(lock["expires"]) > dt.datetime.now().astimezone()
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert not levels.is_in_play_in_editor()
    current = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not current or "L_Test_Flat" not in current.get_path_name():
        assert levels.load_level(LEVEL)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    before = list(actors.get_all_level_actors())
    existing = {a.get_actor_label(): a for a in before}
    assert "Ground_Slab" in existing
    center, extent = existing["Ground_Slab"].get_actor_bounds(False)
    floor_z, north, west = center.z + extent.z, center.y + extent.y, center.x - extent.x
    village_origin = ue.Vector(0, north + 4000, floor_z)
    marae_origin = ue.Vector(west - 1700, 800, floor_z)
    village = json.loads((ROOT / "art/export/Props/Village/village_layout.json").read_text())
    kit = json.loads((ROOT / "art/export/Props/Village/village_kit.json").read_text())["assets"]
    marae = json.loads((ROOT / "art/export/Props/Marae/marae_layout.json").read_text())
    cache, records = {}, []

    def load(path):
        if path not in cache:
            cache[path] = ue.load_asset(path)
        assert cache[path], f"Missing existing asset: {path}"
        return cache[path]

    # Resolve the complete delivery before placing any actor.
    for row in village["items"]:
        load(VILLAGE + "/Meshes/" + row["asset"])
    for row in marae["items"]:
        load(row["asset"])

    def spawn(cls, label, loc, yaw, folder, scale=1):
        actor = existing.get(label)
        if actor:
            assert TAG in [str(t) for t in actor.tags], f"Other task owns {label}"
            actor.set_actor_location(loc, False, True)
            actor.set_actor_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=yaw), True)
        else:
            actor = actors.spawn_actor_from_class(cls, loc, ue.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
            assert actor, label
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [TAG])
            existing[label] = actor
        actor.set_actor_scale3d(ue.Vector(*scale) if isinstance(scale, tuple) else ue.Vector(scale, scale, scale))
        actor.set_folder_path("OneDriveShowcase/" + folder)
        return actor

    def mesh(path, label, loc, yaw=0, folder="Village", scale=1, material=None):
        actor = spawn(ue.StaticMeshActor, label, loc, yaw, folder, scale)
        comp = actor.static_mesh_component
        comp.set_static_mesh(load(path))
        comp.set_editor_property("relative_location", loc)
        comp.set_editor_property("relative_rotation", ue.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        comp.set_collision_profile_name("BlockAll")
        if material:
            comp.set_material(0, load(material))
        return actor

    def record(actor, asset, group):
        pos = actor.get_actor_location()
        records.append({"label": actor.get_actor_label(), "asset": asset, "group": group,
                        "position_cm": [pos.x, pos.y, pos.z], "yaw": actor.get_actor_rotation().yaw})

    def sign(label, title, loc, yaw, folder):
        actor = spawn(ue.TextRenderActor, label, loc, yaw, folder)
        comp = actor.get_component_by_class(ue.TextRenderComponent)
        comp.set_text(ue.Text(title))
        comp.set_world_size(40)
        comp.set_horizontal_alignment(ue.HorizTextAligment.EHTA_CENTER)

    # Rotate VillageA by 180 degrees after the Blender -> Unreal conversion:
    # the shrine is beyond the village square when arriving from the main slab.
    points = []
    for row in village["items"]:
        low, high = kit[row["asset"]]["bbox"]
        angle = math.radians(row["yaw"])
        c, s = math.cos(angle), math.sin(angle)
        for x in (low[0] * 100, high[0] * 100):
            for y in (low[1] * 100, high[1] * 100):
                points.append((-row["loc"][0] - x*c + y*s, row["loc"][1] + x*s + y*c))
    low = [min(p[i] for p in points) - 550 for i in range(2)]
    high = [max(p[i] for p in points) + 550 for i in range(2)]
    cube = "/Engine/BasicShapes/Cube"
    earth = VILLAGE + "/Materials/MI_Minka_Earth"
    gravel = MARAE + "/Materials/MI_Marae_CoralGravel"
    pad_pos = village_origin + ue.Vector((low[0]+high[0])/2, (low[1]+high[1])/2, -40)
    mesh(cube, "Showcase_Village_Platform", pad_pos, scale=((high[0]-low[0])/100, (high[1]-low[1])/100, .8), material=earth)
    pad_edge = village_origin.y + low[1]
    assert pad_edge > north, "Village must not overlap the existing test slab"
    mesh(cube, "Showcase_Village_Access", ue.Vector(0, (north+pad_edge)/2, floor_z-40),
         scale=(6, (pad_edge-north+240)/100, .8), material=earth)
    for row in village["items"]:
        pos = row["loc"]
        path = VILLAGE + "/Meshes/" + row["asset"]
        actor = mesh(path, "Showcase_Village_" + row["name"], village_origin + ue.Vector(-pos[0], pos[1], pos[2]), 180-row["yaw"])
        record(actor, path, "village")

    # Winding street and the shrine path follow the reference maquette.
    for i in range(28):
        x0, x1 = -7000+i*500, -6500+i*500
        y0, y1 = 400*math.sin(x0/2800), 400*math.sin(x1/2800)
        p0, p1 = (-x0,y0), (-x1,y1)
        length = math.hypot(p1[0]-p0[0], p1[1]-p0[1])
        actor = mesh(cube, f"Showcase_Village_Road_{i:02d}", village_origin+ue.Vector((p0[0]+p1[0])/2,(p0[1]+p1[1])/2,.3),
                     math.degrees(math.atan2(p1[1]-p0[1], p1[0]-p0[0])), scale=((length+10)/100,4.5,.01),material=gravel)
        actor.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    path = mesh(cube,"Showcase_Village_ShrinePath",village_origin+ue.Vector(0,2100,.4),scale=(2.4,42,.012),material=gravel)
    path.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    sign("Showcase_Village_Sign", "Village japonais - maisons et sanctuaire", ue.Vector(350,pad_edge+100,floor_z+155), -90,"Village")

    # Marae layout is already in Unreal axes; rotate -90 degrees so the stairs
    # face the platform to the east, keeping the full authored composition.
    mesh(cube, "Showcase_Marae_Platform", marae_origin+ue.Vector(0,0,-40), folder="Marae", scale=(28,28,.8), material=gravel)
    mesh(cube, "Showcase_Marae_Access", ue.Vector(west-150,800,floor_z-40), folder="Marae", scale=(6,4.5,.8), material=gravel)
    for i,row in enumerate(marae["items"]):
        x,y,z = row["loc"]
        loc = marae_origin + ue.Vector(y,-x,z)
        label = f"Showcase_Marae_{i:03d}_" + row["asset"].rsplit("/",1)[-1]
        if row["kind"] == "bp":
            cls = ue.EditorAssetLibrary.load_blueprint_class(row["asset"])
            assert cls, row["asset"]
            actor = spawn(cls,label,loc,row["yaw"]-90,"Marae/"+row["folder"],row["scale"])
        else:
            actor = mesh(row["asset"],label,loc,row["yaw"]-90,"Marae/"+row["folder"],row["scale"])
            if row["folder"] == "Vegetation":
                actor.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
        record(actor,row["asset"],"marae")
    sign("Showcase_Marae_Sign","Marae des perles",ue.Vector(west-400,1050,floor_z+155),0,"Marae")
    assert all(a in actors.get_all_level_actors() for a in before), "An unrelated actor disappeared"
    assert levels.save_current_level()
    result = {"task":"T-179","level":LEVEL,"saved_at":dt.datetime.now(dt.timezone.utc).isoformat(),
              "village_origin_cm":[village_origin.x,village_origin.y,village_origin.z],
              "village_access_start_cm":[0,north-150,floor_z],"village_access_end_cm":[0,pad_edge+250,floor_z],
              "marae_origin_cm":[marae_origin.x,marae_origin.y,marae_origin.z],
              "marae_access_start_cm":[west+150,800,floor_z],
              "counts":{"village":len(village["items"]),"marae":len(marae["items"])},
              "actors":records,"validation":"Placed and saved; visual/PIE checks pending"}
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/"structures-result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    ue.log("T179_STRUCTURES_SUCCESS")


if __name__ == "__main__":
    main()
