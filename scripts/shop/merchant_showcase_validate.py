"""PIE proof: attached merchant, feet fixed through a greeting, actual shop UI."""
import datetime as dt
import json
from pathlib import Path
import traceback
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"monke_island/Saved/T181"
STATE={"phase":"wait","samples":[]}
HANDLE=None
ue.get_editor_subsystem(ue.EditorUtilitySubsystem).close_tab_by_id("MessageLog")
def xyz(v):
    return [round(v.x,3),round(v.y,3),round(v.z,3)]
def finish(passed,error=None):
    result={k:STATE[k] for k in ("samples","merchant","placement","shop_ui_requested","offer_count","cursor_visible") if k in STATE}
    result.update(passed=passed,error=error,finished_at=dt.datetime.now(dt.timezone.utc).isoformat())
    (OUT/"pie-result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    STATE["phase"]="finished"
    ue.unregister_slate_post_tick_callback(HANDLE)
def tick(delta):
    try:
        world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
        if not world or "UEDPIE" not in world.get_path_name():
            if STATE["phase"]!="wait":
                finish(False,"PIE ended early")
            return
        now=ue.GameplayStatics.get_time_seconds(world)
        if STATE["phase"]=="wait":
            if now<2:return
            ship=next(a for a in ue.GameplayStatics.get_all_actors_of_class(world,ue.GalleonActor) if a.get_actor_label()=="Beach_Galleon")
            merchant=next(a for a in ue.GameplayStatics.get_all_actors_of_class(world,ue.ShopkeeperActor)
                          if a.get_class().get_name()=="BP_BeanMerchant_C")
            hull=ship.get_editor_property("hull");body=merchant.get_editor_property("body")
            assert merchant.get_attach_parent_actor()==ship,"Merchant not attached to ship"
            catalog=merchant.get_catalog();offers=catalog.get_editor_property("offers")
            assert len(offers)==4
            pawn=ue.GameplayStatics.get_player_character(world,0)
            pawn.set_actor_location(hull.get_world_transform().transform_location(ue.Vector(20,-40,223)),False,True)
            pawn.get_component_by_class(ue.CharacterMovementComponent).set_movement_mode(ue.MovementMode.MOVE_FALLING)
            controller=ue.GameplayStatics.get_player_controller(world,0)
            controller.set_control_rotation(ue.Rotator(roll=0.0, pitch=0.0, yaw=0))
            STATE.update(phase="animation",started=now,hull=hull,body=body,shop=merchant,pawn=pawn,
                merchant=merchant.get_path_name(),offer_count=4,
                placement=json.loads((OUT/"placement-result.json").read_text(encoding="utf-8")))
        elif STATE["phase"]=="animation":
            inverse=STATE["hull"].get_world_transform()
            body=STATE["body"]
            STATE["samples"].append({"time":round(now,3),
                "body_origin":xyz(inverse.inverse_transform_location(body.get_world_location())),
                "left_foot":xyz(inverse.inverse_transform_location(body.get_socket_location("foot_l"))),
                "right_foot":xyz(inverse.inverse_transform_location(body.get_socket_location("foot_r"))),
                "hand":xyz(inverse.inverse_transform_location(body.get_socket_location("hand_r")))})
            if now-STATE["started"]<12.5:return
            samples=STATE["samples"]
            for key in ("left_foot","right_foot","body_origin"):
                for axis in range(3):
                    values=[s[key][axis] for s in samples]
                    assert max(values)-min(values)<.6,(key,axis,max(values)-min(values))
            origin=samples[-1]["body_origin"]
            assert abs(origin[2]-STATE["placement"]["foot_local_cm"][2])<.6,origin
            hand=[s["hand"][2] for s in samples]
            assert max(hand)-min(hand)>20,("Greeting not animated",min(hand),max(hand))
            assert STATE["shop"].can_interact(STATE["pawn"])
            STATE["shop"].interact(STATE["pawn"])
            STATE.update(phase="ui",started=now)
        elif STATE["phase"]=="ui" and now-STATE["started"]>.5:
            STATE["shop_ui_requested"]=True
            STATE["cursor_visible"]=ue.GameplayStatics.get_player_controller(world,0).get_editor_property("show_mouse_cursor")
            assert STATE["cursor_visible"],"Shop did not take menu input"
            finish(True)
    except Exception:
        finish(False,traceback.format_exc())
HANDLE=ue.register_slate_post_tick_callback(tick)
