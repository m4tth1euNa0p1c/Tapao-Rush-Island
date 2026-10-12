"""Read-only probe (T-213): the cages, the gibbet and the chapel of the French fort of L_Test_Flat, and the gold objects
already in the fort (class, value), for the prisoners and the gold of the chapel."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
out = {"cages": [], "chapel": [], "gold": []}
for actor in ue.get_editor_subsystem(ue.EditorActorSubsystem).get_all_level_actors():
    label = actor.get_actor_label()
    low = label.lower()
    origin, extent = actor.get_actor_bounds(False)
    row = {"label": label, "class": actor.get_class().get_name(), "center": [round(origin.x), round(origin.y), round(origin.z)],
           "extent": [round(extent.x), round(extent.y), round(extent.z)], "yaw": round(actor.get_actor_rotation().yaw, 1),
           "location": [round(actor.get_actor_location().x), round(actor.get_actor_location().y), round(actor.get_actor_location().z)]}
    if "cage" in low or "gibbet" in low:
        out["cages"].append(row)
    elif "chapel" in low:
        out["chapel"].append(row)
    elif any(k in low for k in ("gold", "coin", "chest", "gem", "treasure", "ingot")):
        valuable = actor.get_component_by_class(ue.ValuableComponent)
        row["value"] = valuable.get_editor_property("base_value") if valuable else None
        mesh = actor.get_component_by_class(ue.StaticMeshComponent)
        row["mesh"] = mesh.get_editor_property("static_mesh").get_name() if mesh and mesh.get_editor_property("static_mesh") else None
        out["gold"].append(row)
(ROOT / "monke_island/Saved/T213/probe-cages.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
