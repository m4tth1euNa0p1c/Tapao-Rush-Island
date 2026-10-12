"""Owner requests of 2026-10-09, in the editor (action "ownerbatch1009"): "ajoute au 3e jour les forts français sur
Raraka et les temples artefacts ; mets tout devient plus dur ; ajoute tout" (T-252). Parts read from
monke_island/Saved/T186/ownerbatch1009-request.json ({"parts": [...]}, every part when the file is missing), each
reported on its own:

- runrules: DA_RunRules brought to the C++ defaults of the day (Run/RunRules.cpp, Run/RunDifficulty.h): the forts and
  every artifact temple from day 3 (Campaign.FortsFromDay, AllArtifactsFromDay), the temple and the crabs still on day 1,
  the danger and the deluge of the three stages neutral (Zones[].SpawnScale, FloodDelayScale = 1; the quotas, days and
  loot kept), and the difficulty curve of the ten days written out (DifficultyByDay, NightFallHour, ExpeditionSolarHours).
  The asset may hold values saved before the defaults changed (delta serialization keeps what was edited): everything is
  set explicitly, the values before and after are reported.
Writes monke_island/Saved/T186/ownerbatch1009.json.
Needs the editor lock (MONKE_LOCK_OWNER, scripts/editor_lock.py) and no PIE, like the batches of 10-07 and 10-08.
"""
import datetime as dt
import json
import sys
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts/props"))
import minka_doors_ue as kitlib  # noqa: E402

REQUEST = ROOT / "monke_island/Saved/T186/ownerbatch1009-request.json"
OUT = ROOT / "monke_island/Saved/T186/ownerbatch1009.json"
RUN_RULES = "/Game/MonkeIsland/Expedition/Data/DA_RunRules"

# the unlock days (docs/features/campagne-dix-jours.md, "Forts et temples à artefacts au jour 3")
CAMPAIGN = {"temples_from_day": 1, "forts_from_day": 3, "all_artifacts_from_day": 3, "giant_crabs_from_day": 1}

# the difficulty curve of the ten days, one row per day (the same table as URunRules::URunRules; the last row stands for
# the days after it): mobs, health, damage, soldiers per fort, hit chance, forts, crab health, crab minute, flood, night
CURVE = [
    # mobs  health damage soldiers hit   forts crab  crab_min flood night
    (0.80, 0.90, 0.90, 0, 1.00, 0, 0.90, 0.00, 1.00, 0.00),  # day 1: gentle
    (1.00, 1.00, 1.00, 0, 1.00, 0, 1.00, 0.00, 1.00, 0.00),  # day 2: the rules, the surplus of the campaign
    (1.10, 1.10, 1.10, 7, 1.00, 1, 1.10, 5.50, 0.95, 0.00),  # day 3: forts and every artifact temple
    (1.25, 1.20, 1.20, 8, 1.10, 1, 1.20, 5.50, 0.90, 0.00),  # day 4
    (1.25, 1.20, 1.20, 8, 1.10, 1, 1.20, 5.25, 0.90, 0.00),  # day 5
    (1.40, 1.35, 1.30, 9, 1.20, 2, 1.35, 5.00, 0.85, 0.60),  # day 6: two forts
    (1.60, 1.50, 1.40, 10, 1.25, 2, 1.50, 5.00, 0.85, 0.65),  # day 7: everything open
    (1.80, 1.70, 1.50, 11, 1.30, 2, 1.70, 4.50, 0.80, 0.70),  # day 8
    (1.80, 1.70, 1.50, 11, 1.30, 3, 1.70, 4.50, 0.80, 0.70),  # day 9: three forts
    (2.00, 2.00, 1.60, 12, 1.40, 3, 2.00, 4.00, 0.78, 0.80),  # day 10: the crab at 4:00, the deluge at 7 min
]
CURVE_FIELDS = ("mob_count_scale", "mob_health_scale", "mob_damage_scale", "soldiers_per_fort", "soldier_hit_chance_scale",
                "forts_per_expedition", "crab_health_scale", "crab_emergence_minute", "flood_delay_scale", "night_share")
NIGHT_FALL_HOUR = 18.5
EXPEDITION_SOLAR_HOURS = 8.5
report = {}


def row_dict(row):
    return {field: row.get_editor_property(field) for field in CURVE_FIELDS}


def runrules():
    rules = kitlib.load(RUN_RULES)
    assert rules, RUN_RULES
    out = {"before": {}, "after": {}}
    # the unlock days
    campaign = rules.get_editor_property("campaign")
    out["before"]["campaign"] = {field: campaign.get_editor_property(field) for field in CAMPAIGN}
    for field, value in CAMPAIGN.items():
        campaign.set_editor_property(field, value)
    rules.set_editor_property("campaign", campaign)
    # the stages: neutral danger and deluge, the rest kept
    zones = list(rules.get_editor_property("zones"))
    out["before"]["zones"] = [{"id": str(z.get_editor_property("zone_id")), "spawn": z.get_editor_property("spawn_scale"),
                               "flood": z.get_editor_property("flood_delay_scale")} for z in zones]
    for zone in zones:
        zone.set_editor_property("spawn_scale", 1.0)
        zone.set_editor_property("flood_delay_scale", 1.0)
    rules.set_editor_property("zones", zones)
    # the curve of the ten days
    out["before"]["difficulty_by_day"] = [row_dict(r) for r in rules.get_editor_property("difficulty_by_day")]
    curve = []
    for values in CURVE:
        row = ue.RunDayDifficulty()
        for field, value in zip(CURVE_FIELDS, values):
            row.set_editor_property(field, value)
        curve.append(row)
    rules.set_editor_property("difficulty_by_day", curve)
    rules.set_editor_property("night_fall_hour", NIGHT_FALL_HOUR)
    rules.set_editor_property("expedition_solar_hours", EXPEDITION_SOLAR_HOURS)
    kitlib.save(rules)
    # read back
    check = kitlib.load(RUN_RULES)
    campaign = check.get_editor_property("campaign")
    out["after"]["campaign"] = {field: campaign.get_editor_property(field) for field in CAMPAIGN}
    out["after"]["zones"] = [{"id": str(z.get_editor_property("zone_id")), "quota": z.get_editor_property("quota"),
                              "days": z.get_editor_property("days"), "spawn": z.get_editor_property("spawn_scale"),
                              "loot": z.get_editor_property("loot_value_scale"), "flood": z.get_editor_property("flood_delay_scale")}
                             for z in check.get_editor_property("zones")]
    out["after"]["difficulty_by_day"] = [row_dict(r) for r in check.get_editor_property("difficulty_by_day")]
    out["after"]["night_fall_hour"] = check.get_editor_property("night_fall_hour")
    out["after"]["expedition_solar_hours"] = check.get_editor_property("expedition_solar_hours")
    assert len(out["after"]["difficulty_by_day"]) == 10, out["after"]["difficulty_by_day"]
    assert out["after"]["campaign"] == CAMPAIGN, out["after"]["campaign"]
    return out


PARTS = {"runrules": runrules}


def main():
    kitlib.check_lock()
    parts = json.loads(REQUEST.read_text(encoding="utf-8")).get("parts", list(PARTS)) if REQUEST.exists() else list(PARTS)
    for part in parts:
        try:
            report[part] = PARTS[part]()
        except Exception:  # noqa: BLE001
            report[part] = {"error": traceback.format_exc()[-1500:]}
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")


main()
