"""T-328 (L5): no character pinned under the giant crab any more ("is stuck and failed to move", normal Z -0.98: its claws,
legs and shell at head height pressed the capsules down onto the sand, a push no move can undo).

The C++ of T-328 (Creatures/GiantCrabBody.cpp, GiantCrabPawns.cpp, GiantCrabRules.h) gives every UGiantCrabRules:
- PawnPillars: 14 upright pillars that stop the characters, on every machine, alone or in a network session (8 leg tips,
  2 x 3 under the claws), from PawnPillarBottomCm (-150) to PawnPillarTopCm (260) above the sand, instead of the fixed
  copies of the 38 bodies (T-305, PawnBlockerScale, gone);
- Pawns: the moves of the characters around the crab ignore its animated bodies (bIgnoreAnimatedBodies), the server
  frees one pinned in its shapes for PinnedSeconds (bFreePinned, PinDepthCm, NudgeMarginCm, ClearRadiusCm,
  AngleStepDegrees, GroundSearchCm, CheckSeconds, CheckRadiusCm).
Neither DA_GiantCrabRules nor DA_GiantCrabRules_Sand saved those properties (new ones; PawnBlockerScale was never saved
either): they take the class defaults. This action checks every UGiantCrabRules data asset of /Game/MonkeIsland (the
copies under /Backups/ left alone) and writes the class default back only where a value is missing or unusable: no
pillar, pillars that do not reach under the sand or above the tallest character, the two switches of T-328 off, a
freeing radius inside the footprint of the pillars. A sound asset is not saved.

Lead session action "crabunpin" (editor open after the full build, no game running). Replays change nothing.
Report: monke_island/Saved/T328/crab-unpin.json. Fails (nothing written) while the editor does not know the T-328 C++.
"""
import json
import math
import time
from pathlib import Path

import unreal as ue

OUT = Path(__file__).resolve().parents[2] / "monke_island/Saved/T328/crab-unpin.json"
RULES_CLASS = ue.TopLevelAssetPath("/Script/monke_island", "GiantCrabRules")
EAL = ue.EditorAssetLibrary

# the tallest character standing or jumping (a tiki 1.35 m), as in the spec monke_island.Creatures.GiantCrab.Pawns
TALLEST_CM = 200.0
# the characters looked at reach this far past the freeing radius (the spec's rule)
CHECK_MARGIN_CM = 300.0
# fields of FGiantCrabPawnRules, Python names (a bool loses its b)
PAWN_FIELDS = ("check_seconds", "check_radius_cm", "ignore_animated_bodies", "free_pinned", "pin_depth_cm", "pinned_seconds",
               "nudge_margin_cm", "clear_radius_cm", "angle_step_degrees", "ground_search_cm")


def has_property(obj, name):
    try:
        obj.get_editor_property(name)
        return True
    except Exception:
        return False


def pillar_rows(pillars):
    return [{"name": str(p.get_editor_property("name")),
             "center_cm": [round(p.get_editor_property("center_cm").x, 1), round(p.get_editor_property("center_cm").y, 1)],
             "radius_cm": round(p.get_editor_property("radius_cm"), 2)} for p in pillars]


def footprint(pillars):
    """Farthest reach of the pillars from the crab's centre (cm)"""
    best = 0.0
    for p in pillars:
        center = p.get_editor_property("center_cm")
        best = max(best, math.hypot(center.x, center.y) + p.get_editor_property("radius_cm"))
    return best


def pawn_row(pawns):
    return {field: pawns.get_editor_property(field) for field in PAWN_FIELDS}


def check(rules, defaults, row):
    """Writes the class default back where a value of rules is missing or unusable; returns True when something changed"""
    changed = []

    pillars = list(rules.get_editor_property("pawn_pillars"))
    if not pillars:
        pillars = list(defaults.get_editor_property("pawn_pillars"))
        rules.set_editor_property("pawn_pillars", pillars)
        changed.append("pawn_pillars: none, the 14 of the class")

    widest = max(p.get_editor_property("radius_cm") for p in pillars)
    bottom = rules.get_editor_property("pawn_pillar_bottom_cm")
    top = rules.get_editor_property("pawn_pillar_top_cm")
    low, high = min(bottom, top), max(bottom, top)
    if low + widest >= 0.0 or high - widest < TALLEST_CM:
        rules.set_editor_property("pawn_pillar_bottom_cm", defaults.get_editor_property("pawn_pillar_bottom_cm"))
        rules.set_editor_property("pawn_pillar_top_cm", defaults.get_editor_property("pawn_pillar_top_cm"))
        changed.append(f"pillar height {bottom}..{top} cm: the class's")

    pawns = rules.get_editor_property("pawns")
    for switch in ("ignore_animated_bodies", "free_pinned"):
        if not pawns.get_editor_property(switch):
            pawns.set_editor_property(switch, True)
            changed.append(f"pawns.{switch}: on")
    reach = footprint(pillars)
    if pawns.get_editor_property("clear_radius_cm") < reach:
        pawns.set_editor_property("clear_radius_cm", max(defaults.get_editor_property("pawns").get_editor_property("clear_radius_cm"), reach + 25.0))
        changed.append(f"pawns.clear_radius_cm inside the footprint ({reach:.0f} cm)")
    if pawns.get_editor_property("check_radius_cm") < pawns.get_editor_property("clear_radius_cm") + CHECK_MARGIN_CM:
        pawns.set_editor_property("check_radius_cm", pawns.get_editor_property("clear_radius_cm") + CHECK_MARGIN_CM)
        changed.append("pawns.check_radius_cm short of the freeing radius")
    if any(c.startswith("pawns.") for c in changed):
        rules.set_editor_property("pawns", pawns)

    row["pillars"] = pillar_rows(rules.get_editor_property("pawn_pillars"))
    row["pillar_bottom_cm"] = rules.get_editor_property("pawn_pillar_bottom_cm")
    row["pillar_top_cm"] = rules.get_editor_property("pawn_pillar_top_cm")
    row["footprint_cm"] = round(footprint(rules.get_editor_property("pawn_pillars")), 1)
    row["pawns"] = pawn_row(rules.get_editor_property("pawns"))
    row["hit_bodies"] = len(rules.get_editor_property("hit_bodies"))
    row["hit_bodies_block_pawns"] = rules.get_editor_property("hit_bodies_block_pawns")
    if not row["hit_bodies_block_pawns"]:
        row["warning"] = "hit_bodies_block_pawns off: no pillar is built and the bodies leave the Pawn channel (shots on the limbs too)"
    row["changed"] = changed
    return bool(changed)


def write_report(report):
    OUT.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(report, indent=1, ensure_ascii=False)
    # Malwarebytes may hold a file it just saw written for a moment: try again
    for attempt in range(10):
        try:
            OUT.write_text(text, encoding="utf-8")
            return
        except PermissionError:
            time.sleep(1.5)
    raise RuntimeError(f"{OUT} could not be written")


def main():
    if ue.get_editor_subsystem(ue.LevelEditorSubsystem).is_in_play_in_editor():
        raise RuntimeError("a game is running: nothing done")
    defaults = ue.get_default_object(ue.GiantCrabRules)
    if not has_property(defaults, "pawn_pillars") or not has_property(defaults, "pawns"):
        raise RuntimeError("the editor does not know the C++ of T-328 (pawn_pillars, pawns): full build first, nothing done")
    report = {"assets": {}, "class_defaults": {"pillars": len(defaults.get_editor_property("pawn_pillars")),
                                               "pawns": pawn_row(defaults.get_editor_property("pawns"))}}
    registry = ue.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_class(RULES_CLASS, True):
        path = str(data.package_name)
        if not path.startswith("/Game/MonkeIsland/") or "/Backups/" in path:
            continue
        rules = EAL.load_asset(path)
        if not rules:
            report["assets"][path] = {"error": "could not be loaded"}
            continue
        row = {}
        if check(rules, defaults, row):
            if not EAL.save_loaded_asset(rules, only_if_is_dirty=False):
                raise RuntimeError(f"{path} not saved")
            row["saved"] = True
        else:
            row["saved"] = False
        report["assets"][path] = row
    if not report["assets"]:
        raise RuntimeError("no UGiantCrabRules data asset under /Game/MonkeIsland")
    write_report(report)
    saved = [path for path, row in report["assets"].items() if row.get("saved")]
    ue.log(f"CRAB_UNPIN {len(report['assets'])} crab rules checked, {len(saved)} saved: {saved}; pillars "
           f"{[len(row.get('pillars', [])) for row in report['assets'].values()]}")


main()
