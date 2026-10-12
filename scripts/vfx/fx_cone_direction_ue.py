"""T-321 follow-up (2026-10-11): the lightweight emitters of the project threw their particles UP instead of along their +X.
The builders (build_firearm_vfx_ue.py, machinegun_setup_ue.py, build_blood_assets_ue.py) set the AddVelocity module's
ConeDirection to +X, but the module only reads it when ConeRotationType is Direction; left on its default (Rotation, a
zero rotator) the cone points along +Z. Seen in PIE: the musket smoke rose in a column instead of leaving the barrel;
the same held for the muzzle flames and sparks, the impact dust and the blood sprays (+X = the hit normal).
Every Niagara system under /Game/MonkeIsland whose stateless emitter Fountain_1 throws InCone with a ConeDirection other
than +Z gets ConeRotationType = Direction, is saved, then reloaded (the emitters keep their built data until loaded
again). Lead session action "fxcone" (editor open, no game running). Report: monke_island/Saved/T321/fx-cone.json.
Replays change nothing.
"""
import json
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T321/fx-cone.json"
EAL = ue.EditorAssetLibrary


def tl_get(path, prop):
    obj = ue.load_object(None, path)
    if obj is None:
        return None
    out = ue.ToolsetLibrary.get_object_properties(obj, [prop])
    return (json.loads(out, strict=False) if isinstance(out, str) else out).get(prop)


def main():
    report = {"fixed": [], "kept": [], "skipped": []}
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    systems = registry.get_assets_by_class(ue.TopLevelAssetPath("/Script/Niagara", "NiagaraSystem"), True)
    packages = []
    for data in systems:
        path = str(data.package_name)
        if not path.startswith("/Game/MonkeIsland/"):
            continue
        name = path.rsplit("/", 1)[1]
        module = f"{path}.{name}:Fountain_1.NiagaraStatelessModule_AddVelocity_0"
        if ue.load_object(None, module) is None:
            report["skipped"].append(name)
            continue
        kind = tl_get(module, "velocityType")
        rotation_type = tl_get(module, "coneRotationType")
        direction = (tl_get(module, "coneDirection") or {}).get("min", {})
        along_z = abs(direction.get("x", 0.0)) < 1e-4 and abs(direction.get("y", 0.0)) < 1e-4
        if kind != "InCone" or along_z or rotation_type == "Direction":
            report["kept"].append([name, kind, rotation_type, direction])
            continue
        if not ue.ToolsetLibrary.set_object_properties(ue.load_object(None, module), json.dumps({"coneRotationType": "Direction"})):
            raise RuntimeError(f"{module}: coneRotationType refused")
        system = EAL.load_asset(path)
        if not EAL.save_loaded_asset(system, only_if_is_dirty=False):
            raise RuntimeError(f"{path} not saved")
        packages.append(system.get_outermost())
        report["fixed"].append([name, direction])
    if packages:
        report["reloaded"] = str(ue.EditorLoadingAndSavingUtils.reload_packages(packages, ue.ReloadPackagesInteractionMode.ASSUME_POSITIVE))[:200]
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"FX_CONE fixed {len(report['fixed'])}, kept {len(report['kept'])}")


main()
