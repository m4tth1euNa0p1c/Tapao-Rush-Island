"""Inspect the imported showcase transforms and stair collision heights."""
import json
from pathlib import Path
import unreal as ue
ROOT=Path(__file__).resolve().parents[2]
setup=json.loads((ROOT/"monke_island/Saved/T179/setup-result.json").read_text(encoding="utf-8"))
expected={r["label"]:r["position_cm"] for r in setup["modules"]}
structures=json.loads((ROOT/"monke_island/Saved/T179/structures-result.json").read_text(encoding="utf-8"))
expected.update({r["label"]:r["position_cm"] for r in structures["actors"]})
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
for actor in actors:
    if actor.get_actor_label()=="T184_ValidationCamera":
        ue.get_editor_subsystem(ue.EditorActorSubsystem).destroy_actor(actor)
actors=ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
rows=[]
for a in actors:
    label=a.get_actor_label()
    if label in expected:
        assert (a.get_actor_location()-ue.Vector(*expected[label])).length()<0.1, (label,str(a.get_actor_location()))
    if label.startswith("Showcase_Temple_") and isinstance(a,ue.StaticMeshActor):
        comp=a.static_mesh_component
        row={"label":label,"path":a.get_path_name(),"transform":str(a.get_actor_transform()),
             "bounds":str(a.get_actor_bounds(False)),"hidden":a.is_hidden_ed(),
             "mesh":str(comp.static_mesh),"visible":comp.is_visible()}
        if label in expected:
            assert (a.get_actor_location()-ue.Vector(*expected[label])).length()<0.1, (label,str(a.get_actor_location()))
        if label.endswith("ahu"):
            row["rays"]=[]
            for x in range(6940,7361,20):
                for complex_trace in (False,True):
                    hit=comp.line_trace_component(ue.Vector(x,-1300,600),ue.Vector(x,-1300,-100),complex_trace,False,False)
                    row["rays"].append({"x":x,"complex":complex_trace,"hit":str(hit)})
        rows.append(row)
(ROOT/"monke_island/Saved/T179/placed-audit.json").write_text(json.dumps(rows,indent=2),encoding="utf-8")
assert set(expected).issubset({a.get_actor_label() for a in actors}), "Missing showcase actor"
assert ue.get_editor_subsystem(ue.LevelEditorSubsystem).save_current_level()
