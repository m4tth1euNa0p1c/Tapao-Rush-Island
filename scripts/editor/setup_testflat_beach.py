"""T-149: add an isolated beach west of L_Test_Flat, preserving existing actors.

Unreal Python commandlet; requires staged/imported T-149 galleon assets and the
codex editor lock. Geometry follows the calibrated OBJ axes in build_terrain.py.
"""
import datetime as dt
import json
import math
from pathlib import Path
import sys

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
from import_galleon_t149 import Graph, check_lock, load, save, REPORT, DEST, EAL, MEL, TOOLS

MAP = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
ASSETS = "/Game/MonkeIsland/Maps/Test/Beach"
FOLDER = "BeachSimulation"
TAG = "T149_Beach"
SEA_Z = -50.0
ORIGIN_X = -4900.0
SHIP_X = -9500.0


def height(distance, y):
    profile = [(0, 3), (300, 120), (1300, 170), (2400, 10),
               (3000, -100), (4500, -650), (9600, -750)]
    for (a, az), (b, bz) in zip(profile, profile[1:]):
        if distance <= b:
            t = max(0, min(1, (distance-a)/(b-a)))
            z = az + (bz-az)*t
            break
    else:
        z = profile[-1][1]
    edge = max(0, min(1, (abs(y)-3200)/1800))
    edge = edge*edge*(3-2*edge)*min(1, distance/1200)
    z = z*(1-edge)-750*edge
    z += math.sin(y/600)*12*min(1,distance/600)*max(0,1-distance/4500)
    return z


def terrain_source():
    path = ROOT / "art/export/Galleon_T149/SM_TestFlat_Beach.obj"
    nx, ny = 97, 101
    lines = ["# T149 beach in cm; Unreal=(OBJ.x,-OBJ.y,OBJ.z)", "o SM_TestFlat_Beach", "usemtl M_TestFlat_Sand", "s 1"]
    for j in range(ny):
        y = -5000+j*100
        for i in range(nx):
            d = i*100
            lines.append(f"v {-d:.3f} {-y:.3f} {height(d,y):.3f}")
    for j in range(ny):
        for i in range(nx):
            lines.append(f"vt {i*0.5:.4f} {1-j*0.5:.4f}")
    # X decreases along each row: inverse of the eastward terrain grid winding.
    for j in range(ny-1):
        for i in range(nx-1):
            a = j*nx+i+1
            b, c, d = a+1, a+nx, a+nx+1
            lines.extend((f"f {a}/{a} {b}/{b} {c}/{c}", f"f {b}/{b} {d}/{d} {c}/{c}"))
    path.write_text("\n".join(lines)+"\n", encoding="ascii")
    return path


def terrain_material():
    name = "M_TestFlat_Sand"
    material = load(f"{ASSETS}/Materials/{name}")
    if material:
        return material
    material = TOOLS.create_asset(name, f"{ASSETS}/Materials", ue.Material, ue.MaterialFactoryNew())
    g = Graph(material)
    texroot = "/Game/MonkeIsland/Environment/Ground/Textures"
    dry = g.node(ue.MaterialExpressionTextureSample, texture=load(f"{texroot}/T_SandDry_CH"),
                 sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    wet = g.node(ue.MaterialExpressionTextureSample, texture=load(f"{texroot}/T_SandWet_CH"),
                 sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_COLOR)
    position = g.node(ue.MaterialExpressionWorldPosition)
    color = g.custom(f"return lerp(Wet,Dry,smoothstep({SEA_Z+20},{SEA_Z+150},Position.z));",
                     ue.CustomMaterialOutputType.CMOT_FLOAT3, Dry=dry, Wet=wet, Position=position)
    g.out(color, ue.MaterialProperty.MP_BASE_COLOR)
    normal = g.node(ue.MaterialExpressionTextureSample, texture=load(f"{texroot}/T_SandDry_N"),
                    sampler_type=ue.MaterialSamplerType.SAMPLERTYPE_NORMAL)
    g.out(normal, ue.MaterialProperty.MP_NORMAL)
    g.out(g.scalar(0.8), ue.MaterialProperty.MP_ROUGHNESS)
    MEL.recompile_material(material)
    save(material)
    return material


def terrain_mesh():
    path = f"{ASSETS}/Meshes/SM_TestFlat_Beach"
    mesh = load(path)
    if not mesh:
        source = terrain_source()
        options = ue.FbxImportUI()
        options.import_mesh = True
        options.import_as_skeletal = False
        options.import_materials = False
        options.import_textures = False
        options.mesh_type_to_import = ue.FBXImportType.FBXIT_STATIC_MESH
        options.static_mesh_import_data.set_editor_property("combine_meshes", True)
        options.static_mesh_import_data.set_editor_property("auto_generate_collision", False)
        task = ue.AssetImportTask()
        task.filename = str(source)
        task.destination_path = f"{ASSETS}/Meshes"
        task.destination_name = "SM_TestFlat_Beach"
        task.automated = True
        task.options = options
        task.save = True
        TOOLS.import_asset_tasks([task])
        mesh = load(path)
    assert mesh
    box = mesh.get_bounding_box()
    assert abs((box.max.x-box.min.x)-9600)<5 and abs((box.max.y-box.min.y)-10000)<5, box
    mesh.set_material(0, terrain_material())
    mesh.get_editor_property("body_setup").set_editor_property(
        "collision_trace_flag", ue.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    save(mesh)
    return mesh


def main():
    check_lock()
    rules = load(f"{DEST}/DA_GalleonRules_TestFlat")
    assert rules, "Import the T149 galleon first"
    subsystem = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert subsystem.load_level(MAP)
    actors = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    existing = {a.get_actor_label(): a for a in actors.get_all_level_actors()}
    assert "Ground_Slab" in existing and "Water_Surface" in existing
    assert abs(existing["Water_Surface"].get_actor_location().z-SEA_Z)<1
    ground_center, ground_extent = existing["Ground_Slab"].get_actor_bounds(False)
    assert abs(ground_center.x-ground_extent.x+5000)<1, "Map layout changed"
    before = {a.get_path_name() for a in existing.values()}
    created = []

    def spawn(cls, label, location, rotation=ue.Rotator()):
        actor = existing.get(label)
        if actor:
            tags = [str(t) for t in actor.tags]
            if TAG not in tags:
                assert label == "Beach_Galleon" and isinstance(actor, ue.GalleonActor), (
                    "Label belongs to another task")
                actor.set_editor_property("tags", [TAG])
        else:
            actor = actors.spawn_actor_from_class(cls, ue.Vector(*location), rotation)
            assert actor, label
            actor.set_actor_label(label)
            actor.set_editor_property("tags", [TAG])
            existing[label] = actor
        actor.set_folder_path(FOLDER)
        created.append(actor)
        return actor

    def mesh_actor(mesh, label, location, scale=(1,1,1), yaw=0):
        actor = spawn(ue.StaticMeshActor, label, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
        component = actor.static_mesh_component
        component.set_static_mesh(mesh)
        actor.set_actor_scale3d(ue.Vector(*scale))
        return actor

    mesh_actor(terrain_mesh(), "Beach_SandSlope", (ORIGIN_X,0,0))
    water = mesh_actor(load("/Engine/BasicShapes/Plane"), "Beach_Water", (-10200,0,SEA_Z), (125,125,1))
    water.set_editor_property("tags", [TAG,"IslandWaterSurface"])
    water.static_mesh_component.set_mobility(ue.ComponentMobility.MOVABLE)
    water.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    water.static_mesh_component.set_cast_shadow(False)
    water.static_mesh_component.set_material(0,existing["Water_Surface"].static_mesh_component.get_material(0))
    ship = spawn(ue.GalleonActor, "Beach_Galleon", (SHIP_X,0,SEA_Z), ue.Rotator(0,0,0))
    ship.set_actor_location(ue.Vector(SHIP_X,0,SEA_Z), False, False)
    ship.set_actor_rotation(ue.Rotator(0,0,0), False)
    ship.set_editor_property("rules",rules)
    # AGalleonActor applies these at BeginPlay; populate the same components for editor preview.
    components = {c.get_name():c for c in ship.get_components_by_class(ue.StaticMeshComponent)}
    for name, prop in (("Hull","hull_mesh"),("Sails","sails_mesh"),("Rigging","rigging_mesh"),
                       ("Flag","flag_mesh"),("Wheel","wheel_mesh"),("Rudder","rudder_mesh")):
        component = components[name]
        component.set_static_mesh(rules.get_editor_property(prop))
        component.set_mobility(ue.ComponentMobility.MOVABLE)
    for name, prop in (("Flag","flag_pivot"),("Wheel","wheel_pivot"),("Rudder","rudder_pivot")):
        components[name].set_relative_location(rules.get_editor_property(prop),False,False)
    # Delivery's rudder pivot is inclined; ApplyRules only replaces its translation.
    components["Rudder"].set_relative_rotation(ue.Rotator(0,0,10.49),False,False)
    pennants = mesh_actor(load(f"{DEST}/Meshes/SM_Galleon_Pennants"), "Beach_Galleon_Pennants",
                         (SHIP_X,0,SEA_Z),yaw=0)
    pennants.static_mesh_component.set_mobility(ue.ComponentMobility.MOVABLE)
    pennants.static_mesh_component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
    assert pennants.attach_to_component(components["Hull"], "",
                                       ue.AttachmentRule.SNAP_TO_TARGET,
                                       ue.AttachmentRule.SNAP_TO_TARGET,
                                       ue.AttachmentRule.KEEP_WORLD, False)
    # Use the shipped playable boat blueprint at the hull's authored dock socket.
    boat_bp = load("/Game/MonkeIsland/Boat/Blueprints/BP_Barque")
    assert boat_bp, "BP_Barque is required for the T-149 boarding test"
    boat_class = boat_bp.generated_class()
    dock = components["Hull"].get_socket_location("barque_dock")
    boat_yaw = ship.get_actor_rotation().yaw + rules.get_editor_property("dock_yaw_offset")
    boat = spawn(boat_class, "Beach_Galleon_Barque", (dock.x,dock.y,SEA_Z), ue.Rotator(roll=0.0, pitch=0.0, yaw=boat_yaw))
    boat.set_actor_scale3d(ue.Vector(1,1,1))
    palms = "/Game/MonkeIsland/Environment/Vegetation/Palms/Meshes"
    for i,(distance,y,variant,yaw) in enumerate(((650,-1900,"Straight_A",35),(1000,-2450,"SCurve_A",-20),
                                               (950,1800,"Straight_B",100),(1500,2450,"Young_A",140))):
        mesh_actor(load(f"{palms}/SM_CoconutPalm_{variant}"),f"Beach_Palm_{i+1}",
                   (ORIGIN_X-distance,y,height(distance,y)),yaw=yaw)
    start = spawn(ue.TargetPoint,"Beach_TestView",(-6200,-600,350),ue.Rotator(roll=0.0, pitch=-8.0, yaw=170.0))
    # Keep the existing PlayerStart: this marker only identifies the beach access/view.
    after = {a.get_path_name() for a in actors.get_all_level_actors()}
    assert before.issubset(after), "Existing actor removed unexpectedly"
    assert subsystem.save_current_level()
    proof = dict(task="T-149",map=MAP,sea_level=SEA_Z,
                 galleon=[SHIP_X,300,SEA_Z],existing_actor_count=len(before),
                 cannon_mesh="embedded in repaired SM_Galleon; 35008 triangles",
                 actors=[a.get_path_name() for a in created],
                 saved_at=dt.datetime.now(dt.timezone.utc).isoformat(),
                 validation="Assets and map saved; PIE verification pending")
    output = ROOT/"monke_island/Saved/T149/beach-result.json"
    output.parent.mkdir(parents=True,exist_ok=True)
    output.write_text(json.dumps(proof,indent=2),encoding="utf-8")
    ue.log("T149_BEACH_SUCCESS "+str(output))


if __name__ == "__main__":
    main()
