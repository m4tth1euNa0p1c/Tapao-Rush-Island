"""BS_GiantCrab_Locomotion (T-211): samples outside the axis bounds fail the cook ("LoadErrors: ... un échantillon
A_GiantCrab_Walk_Forward qui est non valide car en dehors des limites", Failure - 2 error(s)).

Reads the axes and the samples; each axis is widened to hold every sample (the samples keep their places: the speeds and
directions the crab really walks at), then the blend space is saved alone.
Writes monke_island/Saved/T186/crab-blendspace.json. Runs from scripts/editor/lead_session_t186.py, action "crabbs".
"""
import json
import math
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
PATH = "/Game/MonkeIsland/Characters/GiantCrab/Anims/BlendSpaces/BS_GiantCrab_Locomotion"

space = ue.EditorAssetLibrary.load_asset(PATH)
assert space, "no blend space"
params = list(space.get_editor_property("blend_parameters"))
samples = list(space.get_editor_property("sample_data"))
report = {"before": [], "samples": [], "after": []}
for p in params:
    report["before"].append({"name": str(p.get_editor_property("display_name")), "min": p.get_editor_property("min"),
                             "max": p.get_editor_property("max"), "grid": p.get_editor_property("grid_num")})
values = []
for s in samples:
    v = s.get_editor_property("sample_value")
    anim = s.get_editor_property("animation")
    values.append((v.x, v.y, v.z))
    report["samples"].append({"anim": anim.get_name() if anim else None, "value": [v.x, v.y, v.z]})

changed = False
for axis, p in enumerate(params[:2]):
    lo = min([p.get_editor_property("min")] + [v[axis] for v in values])
    hi = max([p.get_editor_property("max")] + [v[axis] for v in values])
    # round outwards to a tidy step (10 cm/s on a speed, 5 degrees on a direction)
    step = 10.0 if abs(hi - lo) > 100.0 else 5.0
    lo = math.floor(lo / step) * step
    hi = math.ceil(hi / step) * step
    if lo < p.get_editor_property("min") or hi > p.get_editor_property("max"):
        p.set_editor_property("min", lo)
        p.set_editor_property("max", hi)
        changed = True
if changed:
    space.set_editor_property("blend_parameters", params)
for p in list(space.get_editor_property("blend_parameters")):
    report["after"].append({"name": str(p.get_editor_property("display_name")), "min": p.get_editor_property("min"),
                            "max": p.get_editor_property("max"), "grid": p.get_editor_property("grid_num")})
report["changed"] = changed
report["saved"] = bool(ue.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=False)) if changed else False
(ROOT / "monke_island/Saved/T186/crab-blendspace.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
