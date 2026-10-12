"""T-173 end of the day and contract notices, T-283 flood timer without the day (lots L4 / L5).

- DA_HudTimerRules (/Game/MonkeIsland/UI): bShowRunDayInClock set to False, written in the asset (T-283: the timer top
  right is the clock alone, the TAPAO HUD shows the day of the campaign, the quota and "Contrat : jour N" top left).
  The class default is already False: the asset only records it. Skipped with a clear message while the editor runs a
  build without the property (build first, then run the action again).
- Reports, without changing them, the values the end of a day reads (owner's tuning, never in the code):
  DA_ExpeditionRules (GameOverRestartDelay = the wait of "JOUR PERDU" before the next island, DayEndTravelDelay,
  bPermadeath), DA_RunRules (greed: time, loot, danger, default answer; failed contract: share of the treasury, zones
  back, execution rule; the stages), DA_TapaoUISkin (ribbon_blank for the CONTRAT ÉCHOUÉ ribbon, btn_wide_blank for the
  written buttons), and the [TapaoDayUISubsystem] / [TapaoUISubsystem] overrides of DefaultGame.ini.
Runs in the editor: scripts/editor/lead_session_t186.py, action "dayendux" (editor lock held, no game running).
Idempotent. Result: monke_island/Saved/T-173/day-end-ux-setup.json
"""
import datetime as dt
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T-173"
HUD_TIMER_RULES = "/Game/MonkeIsland/UI/DA_HudTimerRules"
EXPEDITION_RULES = "/Game/MonkeIsland/Expedition/Data/DA_ExpeditionRules"
RUN_RULES = "/Game/MonkeIsland/Expedition/Data/DA_RunRules"
TAPAO_SKIN = "/Game/MonkeIsland/UI/Tapao/DA_TapaoUISkin"
DEFAULT_GAME_INI = ROOT / "monke_island/Config/DefaultGame.ini"
EAL = ue.EditorAssetLibrary


def text(value):
    return str(value) if value is not None else None


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def read(asset, names):
    """The properties that exist on this build, the others reported as missing"""
    values = {}
    for name in names:
        try:
            value = asset.get_editor_property(name)
        except Exception:  # property missing in this build
            values[name] = "missing in this build"
            continue
        values[name] = value if isinstance(value, (bool, int, float, str)) else text(value)
    return values


def hud_timer_rules(result):
    rules = load(HUD_TIMER_RULES)
    if rules is None:
        result["hud_timer_rules"] = {"path": HUD_TIMER_RULES, "status": "asset missing: the class defaults apply (clock alone)"}
        return
    try:
        before = rules.get_editor_property("show_run_day_in_clock")
    except Exception:
        result["hud_timer_rules"] = {
            "path": HUD_TIMER_RULES,
            "status": "skipped: bShowRunDayInClock missing, build the editor with T-283 then run the action dayendux again",
        }
        ue.log_warning("[dayendux] DA_HudTimerRules: bShowRunDayInClock missing in this build, skipped")
        return
    changed = bool(before)
    if changed:
        rules.set_editor_property("show_run_day_in_clock", False)
        assert EAL.save_loaded_asset(rules, only_if_is_dirty=False), HUD_TIMER_RULES
    result["hud_timer_rules"] = {
        "path": HUD_TIMER_RULES,
        "status": "written" if changed else "already off",
        "show_run_day_in_clock": {"before": bool(before), "after": False},
        "clock_font_size": rules.get_editor_property("clock_font_size"),
    }


def expedition_rules(result):
    rules = load(EXPEDITION_RULES)
    if rules is None:
        result["expedition_rules"] = {"path": EXPEDITION_RULES, "status": "asset missing"}
        return
    result["expedition_rules"] = {"path": EXPEDITION_RULES, "status": "read, unchanged"}
    result["expedition_rules"].update(read(rules, ["game_over_restart_delay", "day_end_travel_delay", "permadeath", "galleon_saves_at_deluge_end"]))


def run_rules(result):
    rules = load(RUN_RULES)
    if rules is None:
        result["run_rules"] = {"path": RUN_RULES, "status": "asset missing: the class defaults apply"}
        return
    values = {"path": RUN_RULES, "status": "read, unchanged"}
    values.update(read(rules, ["greed_choice_seconds", "greed_loot_scale", "greed_danger_scale", "greed_default_stay",
                               "failed_treasury_loss", "failed_zone_drop", "execute_on_failed_contract", "excess_payout_fraction",
                               "recruit_fee", "days_per_contract"]))
    values["zones"] = [
        {
            "zone_id": text(zone.get_editor_property("zone_id")),
            "display_name": text(zone.get_editor_property("display_name")),
            "quota": zone.get_editor_property("quota"),
            "days": zone.get_editor_property("days"),
        }
        for zone in rules.get_editor_property("zones")
    ]
    result["run_rules"] = values


def tapao_skin(result):
    skin = load(TAPAO_SKIN)
    if skin is None:
        result["tapao_skin"] = {"path": TAPAO_SKIN, "status": "asset missing: the TAPAO screens are off"}
        return
    sprites = {text(key) for key in skin.get_editor_property("sprites").keys()}
    result["tapao_skin"] = {
        "path": TAPAO_SKIN,
        "ribbon_blank": "ribbon_blank" in sprites,
        "btn_wide_blank": "btn_wide_blank" in sprites,
        "ribbon_naufrage": "ribbon_naufrage" in sprites,
    }
    if "ribbon_blank" not in sprites:
        ue.log_warning("[dayendux] DA_TapaoUISkin has no ribbon_blank: the end screen of a lost campaign shows ribbon_naufrage "
                       "without its CONTRAT ÉCHOUÉ text")


def ini_overrides(result):
    sections = {"[/Script/monke_island.TapaoDayUISubsystem]": [], "[/Script/monke_island.TapaoUISubsystem]": []}
    current = None
    if DEFAULT_GAME_INI.exists():
        for line in DEFAULT_GAME_INI.read_text(encoding="utf-8", errors="replace").splitlines():
            stripped = line.strip()
            if stripped.startswith("["):
                current = stripped if stripped in sections else None
            elif current and stripped and not stripped.startswith(";"):
                sections[current].append(stripped)
    result["default_game_ini"] = {name: lines or "class defaults (SummarySeconds 9, TravelAfterChoiceSeconds 3, GameOverDelay 1.5, "
                                                  "LostCampaignScreenSeconds 8)" for name, lines in sections.items()}


def main():
    result = {"task": "T-173, T-283", "started_at": dt.datetime.now(dt.timezone.utc).isoformat()}
    try:
        hud_timer_rules(result)
        expedition_rules(result)
        run_rules(result)
        tapao_skin(result)
        ini_overrides(result)
        result["passed"] = True
    finally:
        OUT.mkdir(parents=True, exist_ok=True)
        (OUT / "day-end-ux-setup.json").write_text(json.dumps(result, indent=2, ensure_ascii=False, default=str), encoding="utf-8")
    ue.log("[dayendux] " + json.dumps(result, ensure_ascii=False, default=str))


if __name__ == "__main__":
    main()
