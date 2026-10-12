"""Probe: override materials and meshes of the mesh components of the mob blueprints. Writes Saved/T186/probe-bp-mats.json."""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
EAL = ue.EditorAssetLibrary
paths = ["/Game/MonkeIsland/Characters/CocoWarrior/Blueprints/BP_TikiCocoWarrior", "/Game/MonkeIsland/Characters/Villager/Blueprints/BP_TikiVillager"]
paths += [p.split(".")[0] for p in EAL.list_assets("/Game/MonkeIsland/Characters/BeanFR/Blueprints", False, False)]
out = {}
for path in paths:
    bp = EAL.load_asset(path)
    cls = bp.generated_class()
    cdo = ue.get_default_object(cls)
    comps = []
    for comp in cdo.get_components_by_class(ue.SkinnedMeshComponent):
        mesh = comp.get_editor_property("skinned_asset") if hasattr(comp, "skinned_asset") else None
        try:
            mesh = comp.get_editor_property("skeletal_mesh_asset")
        except Exception:  # noqa: BLE001
            pass
        overrides = [m.get_name() if m else None for m in comp.get_editor_property("override_materials")]
        comps.append([comp.get_name(), mesh.get_name() if mesh else None, overrides])
    # the blueprint's own components (SCS)
    scs = []
    try:
        subsystem = ue.get_engine_subsystem(ue.SubobjectDataSubsystem)
        handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        for handle in handles:
            data = subsystem.k2_find_subobject_data_from_handle(handle)
            obj = ue.SubobjectDataBlueprintFunctionLibrary.get_object(data)
            if isinstance(obj, ue.SkinnedMeshComponent):
                scs.append([obj.get_name(), [m.get_name() if m else None for m in obj.get_editor_property("override_materials")]])
    except Exception as e:  # noqa: BLE001
        scs.append(str(e))
    out[path.rsplit("/", 1)[-1]] = {"cdo": comps, "scs": scs}
(ROOT / "monke_island/Saved/T186/probe-bp-mats.json").write_text(json.dumps(out, indent=1), encoding="utf-8")
