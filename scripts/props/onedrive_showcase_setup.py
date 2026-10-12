"""Assemble the delivered temple and six grabbable relics in L_Test_Flat."""
import datetime as dt
import json
from pathlib import Path
import sys
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
from onedrive_showcase_import import STAGE, OUT, TEMPLE, RELICS, ART, TR, load, check_lock

LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
TAG = "OneDriveShowcase_T179"
FOLDER = "OneDriveShowcase/TempleReliquary"


def main():
    check_lock()
    assert (OUT / "import-result.json").exists(), "Finish the import first"
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    current = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not current or "L_Test_Flat" not in current.get_path_name():
        assert levels.load_level(LEVEL)
    before = actors.get_all_level_actors()
    existing = {a.get_actor_label(): a for a in before}
    assert "Ground_Slab" in existing, "Expected Test Flat platform"
    center, extent = existing["Ground_Slab"].get_actor_bounds(False)
    floor_z = center.z + extent.z
    east = center.x + extent.x
    # Entrance faces west, toward the existing slab. Its bridge reaches the edge.
    origin = ue.Vector(east + 1800, -1300, floor_z + 1)
    created = []
    modules = []
    relics = []

    def world(pos):
        # FBX basis (x,-y,z), followed by +90 degrees yaw for the whole scene.
        return origin + ue.Vector(pos[1]*100, pos[0]*100, pos[2]*100)

    def spawn(cls, label, location, rotation=ue.Rotator()):
        actor = existing.get(label)
        if actor:
            assert TAG in [str(t) for t in actor.tags], f"Another task owns {label}"
            actor.set_actor_location(location, False, True)
            actor.set_actor_rotation(rotation, True)
        else:
            actor = actors.spawn_actor_from_class(cls, location, rotation)
            assert actor, label
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [TAG])
            existing[label] = actor
        actor.set_folder_path(FOLDER)
        created.append(actor)
        return actor

    def mesh_actor(mesh, label, location, rotation=ue.Rotator(), scale=None):
        assert mesh, label
        actor = spawn(ue.StaticMeshActor, label, location, rotation)
        comp = actor.static_mesh_component
        comp.set_static_mesh(mesh)
        # Persist the root component transform through PIE duplication/restoration.
        # Setting the actor before assigning a mesh can leave stale editor data.
        comp.set_editor_property("relative_location", location)
        comp.set_editor_property("relative_rotation", rotation)
        comp.set_collision_profile_name("BlockAll")
        if scale:
            actor.set_actor_scale3d(ue.Vector(*scale))
        return actor

    cube = load("/Engine/BasicShapes/Cube")
    platform = mesh_actor(cube, "Showcase_Temple_Platform", ue.Vector(east+1800,-900,floor_z-15), scale=(32,32,.3))
    floor_mat = load(TEMPLE + "/Materials/M_Temple_ShowcaseGround")
    platform.static_mesh_component.set_material(0, floor_mat)
    # Cube's left edge at east+200; bridge overlaps both supports by 100 cm.
    bridge = mesh_actor(cube, "Showcase_Temple_Access", ue.Vector(east+100,-1300,floor_z-15), scale=(6,4.2,.3))
    bridge.static_mesh_component.set_material(0, floor_mat)
    layout = json.loads((STAGE / "temple-reliquaire/maquette.json").read_text(encoding="utf-8"))["params"]["scene_layout"]
    by_key = {info["key"]: name for name,info in TR["modules"].items()}

    def place(key, pos, angle=0):
        name = by_key[key]
        label = f"Showcase_Temple_{len(modules):02d}_{key}"
        actor = mesh_actor(load(TEMPLE + "/Meshes/" + name), label, world(pos), ue.Rotator(roll=0.0, pitch=0.0, yaw=90-angle))
        modules.append({"label":label,"mesh":name,"position_cm":[actor.get_actor_location().x,actor.get_actor_location().y,actor.get_actor_location().z]})
        return actor

    altar = None
    for key in ("court_paving", "relic_altar", "ahu", "gate", "fare_ia_manaha", "fata"):
        a = layout[key]
        actor = place(key,a["pos"],a["rot_z"])
        if key == "relic_altar":
            altar = actor
    for category in ("walls", "palisade"):
        for row in layout[category]:
            place(row["m"],row["pos"],row["rot_z"])
    for category, prefix in (("tiki","tiki_"),("standing_stones","standing_stone_")):
        for row in layout[category]:
            place(prefix+row["variant"],row["pos"],row["rot_z"])
    for row in layout["debris"]:
        place("debris_"+row["m"],row["pos"],row["rot_z"])
    for index,pos in enumerate(layout["torches"]):
        place("torch",pos,index*47)
        light = spawn(ue.PointLight, f"Showcase_Temple_Light_{index:02d}", world(pos)+ue.Vector(0,0,152))
        comp = light.get_component_by_class(ue.PointLightComponent)
        comp.set_editor_property("intensity_units", ue.LightUnits.CANDELAS)
        comp.set_editor_property("intensity", 3.5)
        comp.set_editor_property("attenuation_radius", 500.0)
        comp.set_editor_property("light_color", ue.Color(255,158,77,255))
        comp.set_editor_property("cast_shadows", False)
    assert len(modules) == TR["scene"]["instances_total"], len(modules)
    # Reuse the atoll vegetation already in the project around the delivered kit.
    decor = [
        ("Palms/SM_CoconutPalm_Leaning_A",(-10,-6,0),25,1.0),
        ("Palms/SM_CoconutPalm_Young_A",(-10,6,0),135,1.0),
        ("Palms/SM_CoconutPalm_Young_A",(7,8.5,0),240,.85),
        ("Shrubs/SM_Pandanus_A",(16,5,0),50,.75),
        ("Shrubs/SM_Naupaka_A",(-8.6,1,0),120,.75),
        ("Shrubs/SM_Naupaka_A",(5,8.2,0),60,.8),
        ("Shrubs/SM_Naupaka_A",(16,-5.8,0),210,.7),
        ("GroundCover/SM_Fern_A",(-6.8,6,0),90,.75),
        ("GroundCover/SM_Fern_A",(6.8,5.8,0),190,.7),
        ("GroundCover/SM_Fern_A",(-5,-7,0),10,.6),
        ("GroundCover/SM_Grass_Tuft",(-6,-9.5,0),45,1.0),
        ("GroundCover/SM_Grass_Tuft",(4,-9.5,0),150,1.0),
    ]
    for index,(relative,pos,angle,scale) in enumerate(decor):
        category,name=relative.split("/")
        asset=load(f"/Game/MonkeIsland/Environment/Vegetation/{category}/Meshes/{name}")
        if asset:
            actor=mesh_actor(asset,f"Showcase_Temple_Plant_{index:02d}",world(pos),ue.Rotator(roll=0.0, pitch=0.0, yaw=angle),(scale,scale,scale))
            actor.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    # Native physics props use the existing grab / value-loss gameplay.
    for name, info in ART.items():
        location = altar.static_mesh_component.get_socket_location("Relic_" + name)+ue.Vector(0,0,2)
        mesh = load(RELICS + "/Meshes/SM_" + name)
        actor = spawn(ue.PhysicsPropActor, "Showcase_Relic_" + name, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=90))
        comp = actor.get_editor_property("mesh")
        comp.set_simulate_physics(False)
        comp.set_static_mesh(mesh)
        comp.set_editor_property("relative_location", location)
        comp.set_collision_profile_name("PhysicsActor")
        comp.set_simulate_physics(True)
        actor.set_editor_property("mass_kg", info["mass_kg"])
        comp.set_mass_override_in_kg("None", info["mass_kg"], True)
        comp.set_linear_damping(0.35)
        comp.set_angular_damping(0.6)
        actor.get_editor_property("valuable").set_editor_property("base_value", info["value"])
        relics.append({"label":actor.get_actor_label(),"mass_kg":info["mass_kg"],"value":info["value"],
                       "position_cm":[location.x,location.y,location.z]})
    # A short label identifies the new area from the main platform.
    sign = spawn(ue.TextRenderActor,"Showcase_Temple_Sign",ue.Vector(east+280,-1300,floor_z+155),ue.Rotator(roll=0.0, pitch=0.0, yaw=180))
    text = sign.get_component_by_class(ue.TextRenderComponent)
    text.set_text(ue.Text("Temple des reliques"))
    text.set_world_size(35)
    text.set_horizontal_alignment(ue.HorizTextAligment.EHTA_CENTER)
    assert all(a in actors.get_all_level_actors() for a in before), "An unrelated actor disappeared"
    assert levels.save_current_level()
    proof = {"task":"T-179","level":LEVEL,"modules":modules,"relics":relics,"tag":TAG,
             "new_area_origin_cm":[origin.x,origin.y,origin.z],"bridge_from_cm":[east,-1300,floor_z],
             "created_or_refreshed":len(created),"before_actor_count":len(before),
             "after_actor_count":len(actors.get_all_level_actors()),
             "saved_at":dt.datetime.now(dt.timezone.utc).isoformat(),"validation":"Saved; PIE traversal and visual check pending"}
    OUT.mkdir(parents=True, exist_ok=True)
    (OUT/"setup-result.json").write_text(json.dumps(proof,indent=2),encoding="utf-8")
    ue.log("T179_SETUP_SUCCESS")


if __name__ == "__main__":
    # A bounded follow-up stage can run in the existing shared editor session.
    request = json.loads((OUT / "session-request.json").read_text(encoding="utf-8-sig"))
    stage = request.get("stage", "temple")
    if stage == "temple":
        main()
    else:
        import runpy
        stages = {
            "galleon_audit": "onedrive_showcase_galleon_audit.py",
            "galleon_access": "onedrive_showcase_galleon_access.py",
            "galleon_validate": "onedrive_showcase_galleon_validate.py",
            "galleon_import": "onedrive_showcase_galleon_import.py",
            "galleon_promote": "onedrive_showcase_galleon_promote.py",
            "merchant_validate": "../shop/merchant_showcase_validate.py",
            "merchant_animation": "../shop/merchant_showcase_animation_fix.py",
            "showcase_audit": "onedrive_showcase_audit.py",
            "galleon_visual": "onedrive_showcase_galleon_visual.py",
        }
        assert stage in stages, stage
        check_lock()
        runpy.run_path(str(ROOT / "scripts/props" / stages[stage]), run_name="__main__")
