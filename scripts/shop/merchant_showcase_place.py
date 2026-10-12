"""Install the T-181 merchant in both ship rules, grounded on the actual hull."""
import datetime as dt
import json
from pathlib import Path
import unreal as ue

ROOT=Path(__file__).resolve().parents[2]
OUT=ROOT/"monke_island/Saved/T181"
BP="/Game/MonkeIsland/Galleon/Shop/Merchant/BP_BeanMerchant"
RULES=("/Game/MonkeIsland/Galleon/TestFlat/DA_GalleonRules_TestFlat",)


def xyz(v):
    return [v.x,v.y,v.z]


def main():
    lock=json.loads((ROOT/".claude/factory/editor.lock.json").read_text(encoding="utf-8"))
    assert lock["owner"]=="codex" and dt.datetime.fromisoformat(lock["expires"])>dt.datetime.now().astimezone()
    levels=ue.get_editor_subsystem(ue.LevelEditorSubsystem)
    assert not levels.is_in_play_in_editor()
    world=ue.get_editor_subsystem(ue.UnrealEditorSubsystem).get_editor_world()
    assert "L_Test_Flat" in world.get_path_name()
    actors=ue.get_editor_subsystem(ue.EditorActorSubsystem)
    ship=next(a for a in actors.get_all_level_actors() if a.get_actor_label()=="Beach_Galleon")
    hull=ship.get_editor_property("hull")
    transform=hull.get_world_transform()
    # Port side between the main mast and the quarterdeck stairs, facing inward.
    # This leaves the centre aisle and both pairs of stairs clear.
    local=ue.Vector(190,-40,160)
    start=ue.MathLibrary.transform_location(transform,local+ue.Vector(0,0,90))
    end=ue.MathLibrary.transform_location(transform,local-ue.Vector(0,0,40))
    trace=hull.line_trace_component(start,end,True,False,False)
    assert trace,"No deck under merchant"
    contact=trace[0]  # K2_LineTraceComponent's first out parameter is HitLocation.
    assert isinstance(contact,ue.Vector),str(trace)
    floor=ue.MathLibrary.inverse_transform_location(transform,contact)
    assert abs(floor.z-160)<10,("Unexpected obstacle at merchant feet",xyz(floor))
    cls=ue.EditorAssetLibrary.load_blueprint_class(BP)
    assert cls
    results=[]
    stamp=dt.datetime.now().strftime("%Y%m%d_%H%M%S")
    for path in RULES:
        rules=ue.load_asset(path);assert rules,path
        mesh=rules.get_editor_property("hull_mesh")
        assert mesh==hull.get_editor_property("static_mesh"),"Production ship differs: finish T-165 first"
        socket=mesh.find_socket(str(rules.get_editor_property("deck_spawn_socket")))
        assert socket
        origin=socket.get_editor_property("relative_location")
        before={"class":str(rules.get_editor_property("shopkeeper_class")),
                "offset":xyz(rules.get_editor_property("shopkeeper_offset")),"yaw":rules.get_editor_property("shopkeeper_yaw")}
        backup="/Game/MonkeIsland/Galleon/Backups/"+rules.get_name()+"_PreT181_"+stamp
        previous=ue.EditorAssetLibrary.duplicate_asset(path,backup);assert previous
        assert ue.EditorAssetLibrary.save_loaded_asset(previous,False)
        rules.set_editor_property("shopkeeper_class",cls)
        offset=ue.Vector(local.x-origin.x,local.y-origin.y,floor.z-origin.z+55)
        rules.set_editor_property("shopkeeper_offset",offset)
        rules.set_editor_property("shopkeeper_yaw",180.0)
        assert ue.EditorAssetLibrary.save_loaded_asset(rules,False)
        results.append({"rules":path,"before":before,"backup":backup,"offset_cm":xyz(offset),"yaw":180})
    OUT.mkdir(parents=True,exist_ok=True)
    (OUT/"placement-result.json").write_text(json.dumps({"task":"T-181","rules":results,"foot_local_cm":xyz(floor),
        "merchant_class":BP,"finished_at":dt.datetime.now(dt.timezone.utc).isoformat(),
        "validation":"Deck mesh traced; native server spawn attaches merchant and stall to the swaying hull. PIE pending"},indent=2),encoding="utf-8")
    ue.log("T181_PLACEMENT_SUCCESS")


if __name__=="__main__":
    main()
