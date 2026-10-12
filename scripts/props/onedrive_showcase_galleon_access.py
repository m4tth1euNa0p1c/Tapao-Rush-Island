"""Match the playable Bean collision to its 105 cm body, preserving feet/camera."""
import datetime as dt
import json
from pathlib import Path
import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T184"
PATH = "/Game/MonkeIsland/Characters/Bean/Blueprints/BP_IslanderBean"
bp = ue.load_asset(PATH)
assert bp
backup = "/Game/MonkeIsland/Characters/Bean/Backups/BP_IslanderBean_PreT184"
if not ue.EditorAssetLibrary.does_asset_exist(backup):
    assert ue.EditorAssetLibrary.duplicate_asset(PATH, backup)
    assert ue.EditorAssetLibrary.save_asset(backup)
cdo = ue.get_default_object(bp.generated_class())
capsule = cdo.get_component_by_class(ue.CapsuleComponent)
mesh = cdo.get_editor_property("mesh")
movement = cdo.get_component_by_class(ue.CharacterMovementComponent)
before = {"capsule_half_height": capsule.get_unscaled_capsule_half_height(),
          "mesh_z": mesh.get_editor_property("relative_location").z,
          "crouched_half_height": movement.get_editor_property("crouched_half_height")}
capsule.set_editor_property("capsule_half_height", 55.0)
pos = mesh.get_editor_property("relative_location")
mesh.set_editor_property("relative_location", ue.Vector(pos.x, pos.y, -55))
movement.set_editor_property("crouched_half_height", 40.0)
ue.BlueprintEditorLibrary.compile_blueprint(bp)
cdo = ue.get_default_object(bp.generated_class())
after = {"capsule_half_height": cdo.get_component_by_class(ue.CapsuleComponent).get_unscaled_capsule_half_height(),
         "mesh_z": cdo.get_editor_property("mesh").get_editor_property("relative_location").z,
         "crouched_half_height": cdo.get_component_by_class(ue.CharacterMovementComponent).get_editor_property("crouched_half_height")}
assert after == {"capsule_half_height": 55.0, "mesh_z": -55.0, "crouched_half_height": 40.0}, after
assert ue.EditorAssetLibrary.save_loaded_asset(bp, False)
OUT.mkdir(parents=True, exist_ok=True)
(OUT / "access-settings.json").write_text(json.dumps({"asset": PATH, "backup": backup, "before": before,
    "after": after, "saved_at": dt.datetime.now(dt.timezone.utc).isoformat()}, indent=2), encoding="utf-8")
ue.log("T184_ACCESS_SETTINGS_SAVED")
