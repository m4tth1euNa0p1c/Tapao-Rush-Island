"""PIE checks for T-179: walk the bridge, stairs and hut; observe six relics."""
import datetime as dt
import json
import math
from pathlib import Path
import time
import traceback
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"monke_island/Saved/T179"
BOOT=time.time()
STATE={"phase":"waiting","routes":[],"index":0}
HANDLE=None


def xyz(v):
    return [round(v.x,3),round(v.y,3),round(v.z,3)]


def finish(passed,error=None):
    if STATE.get("move"):
        STATE["move"].stop_movement_immediately()
    if STATE.get("controller"):
        STATE["controller"].set_ignore_move_input(False)
    result={"task":"T-179","passed":passed,"error":error,"routes":STATE["routes"],
            "relic_physics":STATE.get("relic_physics"),"import_finished_at":STATE.get("import_finished_at"),
            "finished_at":dt.datetime.now(dt.timezone.utc).isoformat(),
            "validation":"PIE CharacterMovement routes and physics settling; grab UI/network not tested"}
    (OUT/"traversal-result.json").write_text(json.dumps(result,indent=2),encoding="utf-8")
    STATE["phase"]="finished"
    ue.unregister_slate_post_tick_callback(HANDLE)
    ue.log("T179_TRAVERSAL_"+("SUCCESS" if passed else "FAILED"))


def start_route(now):
    if STATE["index"]==len(STATE["plan"]):
        STATE.update(phase="physics",phase_start=now)
        return
    name,start,end=STATE["plan"][STATE["index"]]
    STATE["move"].stop_movement_immediately()
    STATE["pawn"].set_actor_location(start+ue.Vector(0,0,STATE["half_height"]+6),False,True)
    # Clear the previous floor/base after teleporting between independent routes.
    STATE["move"].set_movement_mode(ue.MovementMode.MOVE_FALLING)
    STATE.update(phase="settle",phase_start=now,name=name,start=xyz(start),target=end,samples=[])


def tick(delta):
    try:
        request=OUT/"pie-validation-request.json"
        if not request.exists() or request.stat().st_mtime<BOOT:
            return
        lock=json.loads((ROOT/".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
        if lock["owner"]!="codex" or dt.datetime.fromisoformat(lock["expires"])<=dt.datetime.now().astimezone():
            return
        world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_game_world()
        if not world or "UEDPIE" not in world.get_path_name():
            return
        now=ue.GameplayStatics.get_time_seconds(world)
        if STATE["phase"]=="waiting":
            pawn=ue.GameplayStatics.get_player_character(world,0)
            if not pawn or now<2:
                return
            setup=json.loads((OUT/"setup-result.json").read_text(encoding="utf-8"))
            o=ue.Vector(*setup["new_area_origin_cm"])
            convert=lambda x,y,z:o+ue.Vector(100*y,100*x,100*z)
            edge=ue.Vector(*setup["bridge_from_cm"])
            plan=[("bridge_and_gate",edge+ue.Vector(-150,0,0),convert(0,-5,0.03)),
                  ("ahu_stairs",convert(0,1.5,0.03),convert(0,4.9,1.485)),
                  ("hut_stairs_and_door",convert(8.6,-.5,0),convert(12.1,-.5,.99))]
            structures=json.loads((OUT/"structures-result.json").read_text(encoding="utf-8"))
            mo=ue.Vector(*structures["marae_origin_cm"])
            plan.extend([
                ("village_access",ue.Vector(*structures["village_access_start_cm"]),ue.Vector(*structures["village_access_end_cm"])),
                ("marae_access",ue.Vector(*structures["marae_access_start_cm"]),mo+ue.Vector(550,0,0)),
                ("marae_stairs",mo+ue.Vector(500,0,0),mo+ue.Vector(70,0,180)),
            ])
            move=pawn.get_component_by_class(ue.CharacterMovementComponent)
            controller=ue.GameplayStatics.get_player_controller(world,0)
            controller.set_ignore_move_input(True)
            half=pawn.get_component_by_class(ue.CapsuleComponent).get_scaled_capsule_half_height()
            relics=[a for a in ue.GameplayStatics.get_all_actors_of_class(world,ue.PhysicsPropActor)
                    if a.get_actor_label().startswith("Showcase_Relic_")]
            assert len(relics)==6, f"Expected 6 relics, found {len(relics)}"
            STATE.update(pawn=pawn,move=move,controller=controller,half_height=half,plan=plan,relics=relics,
                         relic_starts={r["label"]:r["position_cm"] for r in setup["relics"]},
                         import_finished_at=json.loads((OUT/"import-result.json").read_text())["finished_at"])
            start_route(now)
        elif STATE["phase"]=="physics":
            if now<10 or now-STATE["phase_start"]<2:
                return
            checks=[]
            for actor in STATE["relics"]:
                mesh=actor.get_editor_property("mesh")
                pos=mesh.get_world_location()
                original=STATE["relic_starts"][actor.get_actor_label()]
                velocity=mesh.get_physics_linear_velocity()
                speed=math.sqrt(velocity.x**2+velocity.y**2+velocity.z**2)
                passed=mesh.is_simulating_physics() and pos.z>original[2]-15 and math.hypot(pos.x-original[0],pos.y-original[1])<25 and speed<10
                checks.append({"name":actor.get_actor_label(),"passed":passed,"position_cm":xyz(pos),"speed_cm_s":round(speed,3)})
            STATE["relic_physics"]=checks
            assert all(x["passed"] for x in checks), "Some relics fell through the altar or did not settle"
            finish(True)
        else:
            if STATE["phase"]=="settle":
                if now-STATE["phase_start"]<.5:
                    return
                STATE.update(phase="walking",phase_start=now)
            pos=STATE["pawn"].get_actor_location()
            target=STATE["target"]
            dx,dy=target.x-pos.x,target.y-pos.y
            distance=math.hypot(dx,dy)
            STATE["samples"].append(xyz(pos))
            if distance<25 and abs(pos.z-target.z-STATE["half_height"])<30:
                STATE["routes"].append({"name":STATE["name"],"passed":True,"start":STATE["start"],"end":xyz(pos),"positions":STATE["samples"]})
                STATE["index"]+=1
                start_route(now)
            elif now-STATE["phase_start"]>18:
                STATE["routes"].append({"name":STATE["name"],"passed":False,"end":xyz(pos),"target":xyz(target),"positions":STATE["samples"]})
                raise AssertionError("Blocked route: "+STATE["name"])
            elif distance>.01:
                STATE["pawn"].add_movement_input(ue.Vector(dx/distance,dy/distance,0),1,True)
    except Exception:
        finish(False,traceback.format_exc())


HANDLE=ue.register_slate_post_tick_callback(tick)
(OUT/"validator-ready.json").write_text(json.dumps({"started_at":dt.datetime.now(dt.timezone.utc).isoformat()}),encoding="utf-8")
