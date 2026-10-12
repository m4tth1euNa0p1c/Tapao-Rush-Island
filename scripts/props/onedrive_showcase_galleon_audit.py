"""Read-only Bean and galleon access audit in the full Unreal editor."""
import datetime as dt
import json
from pathlib import Path
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T184"
OUT.mkdir(parents=True, exist_ok=True)
BP = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"

def xyz(v):
    return [round(v.x, 4), round(v.y, 4), round(v.z, 4)]

def component(c):
    row = {"name": c.get_name(), "class": c.get_class().get_name(), "path": c.get_path_name()}
    if isinstance(c, ue.SceneComponent):
        row.update(location=xyz(c.get_editor_property("relative_location")),
                   scale=xyz(c.get_editor_property("relative_scale3d")),
                   parent=str(c.get_attach_parent()))
    if isinstance(c, ue.CapsuleComponent):
        row.update(half_height=c.get_unscaled_capsule_half_height(), radius=c.get_unscaled_capsule_radius())
    return row

bp = ue.load_asset(BP)
cdo = ue.get_default_object(bp.generated_class())
subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
components = []
for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
    data = ue.SubobjectDataBlueprintFunctionLibrary.get_data(handle)
    obj = ue.SubobjectDataBlueprintFunctionLibrary.get_object_for_blueprint(data, bp)
    if obj:
        components.append(component(obj))
actors = ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors()
galleon = next(a for a in actors if a.get_actor_label() == "Beach_Galleon")
hull = galleon.get_editor_property("hull")
mesh = hull.get_editor_property("static_mesh")
result = {"at": dt.datetime.now(dt.timezone.utc).isoformat(), "components": components,
          "cdo_components": [component(c) for c in cdo.get_components_by_class(ue.SceneComponent)],
          "boat_transform": str(hull.get_world_transform()), "mesh": mesh.get_path_name(),
          "glass_slots": [str(s.material_slot_name) for s in mesh.static_materials if "Glass" in str(s.material_slot_name)],
          "near_boat": [{"label": a.get_actor_label(), "location": xyz(a.get_actor_location()),
                         "rotation": str(a.get_actor_rotation())} for a in actors
                        if any(word in a.get_actor_label() for word in ("Galleon", "Gangway", "Bridge", "Rowboat", "Barque"))]}
(OUT / "access-audit.json").write_text(json.dumps(result, indent=2), encoding="utf-8")
ue.log("T184_ACCESS_AUDIT_SUCCESS")
