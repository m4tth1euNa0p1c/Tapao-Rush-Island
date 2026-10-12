"""SUPERSEDED (2026-10-07, T-194): ATrumpetCircle was removed; the flag site of scripts/props/flag_ceremony_ue.py
replaces the red circle, which that script takes out of L_Test_Flat. Kept for the history of T-191.

T-191: red circle in the middle of the generated village of L_Test_Flat (Uo-mura, FRG_TestFlat): while a player
stands in it, F sounds the trumpet (ATrumpetCircle, IMC_TrumpetCircle: F -> IA_TrumpetEmote above the interaction).
The ring is drawn like the arena ring of the level: 36 flat red segments (SM_Cube, M_Test_ArenaRing) around the
centre, without collision, attached to the circle actor.
Runs in the editor: scripts/editor/lead_session_t186.py, action "circle".
"""
import datetime as dt
import json
import sys
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

OUT = ROOT / "monke_island/Saved/T186"
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
VILLAGES = "/Game/MonkeIsland/Environment/Village/Data/Fragments/DA_Villages_FRG_TestFlat"
INPUT = "/Game/MonkeIsland/Emotes/Input"
ACTION = INPUT + "/IA_TrumpetEmote"
IMC = INPUT + "/IMC_TrumpetCircle"
LABEL = "TestFlat_TrumpetCircle"
FOLDER = "TestFlat/Zone_Generateurs"
RADIUS = 600.0
SEGMENTS = 36
SEGMENT_WIDTH = 0.3             # m
SEGMENT_HEIGHT = 0.05           # m


def ring_source(actors):
    """Mesh, material and radius (cm, at scale 1) of the arena ring of the level"""
    for actor in actors:
        if not isinstance(actor, ue.StaticMeshActor):
            continue
        component = actor.static_mesh_component
        mesh = component.static_mesh
        material = component.get_material(0)
        if mesh and material and "ArenaRing" in material.get_name():
            extent = mesh.get_bounding_box().max
            return mesh, material, max(extent.x, extent.y), actor.get_actor_label()
    return None, None, 0.0, None


def main():
    kitlib.check_lock()
    levels = ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    world = ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    if not world or "L_Test_Flat" not in world.get_path_name():
        assert levels.load_level(LEVEL)
    action = kitlib.load(ACTION)
    assert action, ACTION
    imc = kitlib.load(IMC)
    if not imc:
        imc = ue.AssetToolsHelpers.get_asset_tools().create_asset("IMC_TrumpetCircle", INPUT, ue.InputMappingContext,
                                                                  ue.InputMappingContext_Factory())
    assert imc, IMC
    imc.unmap_all()
    key = ue.Key()
    key.set_editor_property("key_name", "F")
    imc.map_key(action, key)
    kitlib.save(imc)

    village = kitlib.load(VILLAGES).get_editor_property("villages")[0]
    centre = village.get_editor_property("center")
    actors_sub = ue.get_editor_subsystem(ue.EditorActorSubsystem)
    actors = actors_sub.get_all_level_actors()
    mesh, material, ring_radius, ring_label = ring_source(actors)
    circle = next((a for a in actors if a.get_actor_label() == LABEL), None)
    location = ue.Vector(centre.x, centre.y, centre.z + 2.0)
    if not circle:
        circle = actors_sub.spawn_actor_from_class(ue.TrumpetCircle, location, ue.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
        circle.set_actor_label(LABEL)
    circle.set_actor_location(location, False, False)
    circle.set_folder_path(FOLDER)
    circle.set_editor_property("radius", RADIUS)
    circle.set_editor_property("circle_mapping", imc)
    circle.get_editor_property("ring").set_static_mesh(None)
    for actor in actors:
        if actor.get_actor_label().startswith(LABEL + "_Ring_"):
            actors_sub.destroy_actor(actor)
    segments = 0
    if mesh:
        import math
        length = 2.0 * math.pi * RADIUS / SEGMENTS * 1.04 / 100.0
        for index in range(SEGMENTS):
            angle = 2.0 * math.pi * index / SEGMENTS
            spot = ue.Vector(location.x + RADIUS * math.cos(angle), location.y + RADIUS * math.sin(angle), location.z - 1.5)
            segment = actors_sub.spawn_actor_from_class(ue.StaticMeshActor, spot,
                                                        ue.Rotator(roll=0.0, pitch=0.0, yaw=math.degrees(angle) + 90.0))
            segment.set_actor_label(f"{LABEL}_Ring_{index:02d}")
            component = segment.static_mesh_component
            component.set_static_mesh(mesh)
            component.set_material(0, material)
            component.set_collision_enabled(ue.CollisionEnabled.NO_COLLISION)
            component.set_editor_property("cast_shadow", False)
            segment.set_actor_scale3d(ue.Vector(length, SEGMENT_WIDTH, SEGMENT_HEIGHT))
            segment.attach_to_actor(circle, "", ue.AttachmentRule.KEEP_WORLD, ue.AttachmentRule.KEEP_WORLD,
                                    ue.AttachmentRule.KEEP_WORLD, False)
            segment.set_folder_path(FOLDER)
            segments += 1
    assert levels.save_current_level()
    OUT.mkdir(parents=True, exist_ok=True)
    result = {"task": "T-191", "circle": LABEL, "centre": [centre.x, centre.y, centre.z], "radius_cm": RADIUS,
              "village": str(village.get_editor_property("name")), "ring_from": ring_label,
              "ring_mesh": mesh.get_path_name() if mesh else None, "segments": segments, "imc": IMC,
              "finished_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    (OUT / "circle-result.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
    ue.log("T191_CIRCLE_SUCCESS")


if __name__ == "__main__":
    main()
