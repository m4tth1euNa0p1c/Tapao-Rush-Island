"""Capture the tested hull and merchant in a session-owned PIE world."""
import datetime as dt
import json
from pathlib import Path
import traceback
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"monke_island/Saved/T184"
STATE={"phase":"wait","index":0,"captures":[]}
VIEWS=[("merchant",(0,-200,285),(190,-40,215)),
       ("hold",(0,-140,98),(-190,150,90)),
       ("cabin",(110,-610,242),(15,-930,245)),
       ("exterior",(1550,750,1150),(0,-150,450))]
HANDLE=None
ue.get_editor_subsystem(ue.EditorUtilitySubsystem).close_tab_by_id("MessageLog")
editor_actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
for actor in editor_actors.get_all_level_actors():
    if actor.get_actor_label()=="T184_ValidationCamera":
        editor_actors.destroy_actor(actor)
camera=editor_actors.spawn_actor_from_class(ue.CameraActor,ue.Vector(0,0,300))
camera.set_actor_label("T184_ValidationCamera")

def tick(delta):
    try:
        world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
        if not world or "UEDPIE" not in world.get_path_name():return
        now=ue.GameplayStatics.get_time_seconds(world)
        if STATE["phase"]=="wait":
            if now<2:return
            ship=next(a for a in ue.GameplayStatics.get_all_actors_of_class(world,ue.GalleonActor)
                      if a.get_actor_label()=="Beach_Galleon")
            pc=ue.GameplayStatics.get_player_controller(world,0)
            pc.set_ignore_move_input(True)
            camera=next(a for a in ue.GameplayStatics.get_all_actors_of_class(world,ue.CameraActor)
                        if a.get_actor_label()=="T184_ValidationCamera")
            pc.set_view_target_with_blend(camera,0)
            STATE.update(phase="view",hull=ship.get_editor_property("hull"),camera=camera,pc=pc,started=now)
        name,point,target=VIEWS[STATE["index"]]
        transform=STATE["hull"].get_world_transform()
        point=transform.transform_location(ue.Vector(*point))
        target=transform.transform_location(ue.Vector(*target))
        STATE["camera"].set_actor_location(point,False,True)
        STATE["camera"].set_actor_rotation(ue.MathLibrary.find_look_at_rotation(point,target),True)
        if now-STATE["started"]>3:
            filename=str(OUT/("pie-"+name+".png"))
            ue.SystemLibrary.execute_console_command(world,'HighResShot 1280x720 filename="'+filename+'"')
            STATE["captures"].append(filename)
            STATE.update(index=STATE["index"]+1,started=now)
            if STATE["index"]==len(VIEWS):
                STATE["pc"].set_ignore_move_input(False)
                (OUT/"visual-result.json").write_text(json.dumps({"captures":STATE["captures"],
                    "finished_at":dt.datetime.now(dt.timezone.utc).isoformat()},indent=2),encoding="utf-8")
                ue.unregister_slate_post_tick_callback(HANDLE)
    except Exception:
        (OUT/"visual-error.txt").write_text(traceback.format_exc(),encoding="utf-8")
        ue.unregister_slate_post_tick_callback(HANDLE)
HANDLE=ue.register_slate_post_tick_callback(tick)
