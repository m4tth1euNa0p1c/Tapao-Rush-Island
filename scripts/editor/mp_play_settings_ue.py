"""Editor play settings for the two-player checks of the network wave (2026-10-11): PIE as a listen server with one client
in the same process, or back to a single player. The mode is read from monke_island/Saved/T186/mp-settings.json
({"players": 2} or {"players": 1}; {"players": 1, "mode": "client"}: the viewport is a client of a dedicated server in the
same process, to see and measure what a joining player gets, T-330 / T-331); the previous values are kept in the report.
Lead session action "mpsettings" (no game running). Report: monke_island/Saved/T186/mp-settings-result.json.
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
REQUEST = ROOT / "monke_island/Saved/T186/mp-settings.json"
OUT = ROOT / "monke_island/Saved/T186/mp-settings-result.json"


SETTINGS = "/Script/UnrealEd.Default__LevelEditorPlaySettings"
NAMES = ["PlayNumberOfClients", "PlayNetMode", "RunUnderOneProcess", "bLaunchSeparateServer", "NewWindowWidth", "NewWindowHeight"]


def read(settings):
    # ULevelEditorPlaySettings is not exposed to Python: its default object through the toolset (2026-10-11)
    out = ue.ToolsetLibrary.get_object_properties(settings, NAMES)
    return json.loads(out) if isinstance(out, str) else out


def main():
    request = json.loads(REQUEST.read_text(encoding="utf-8")) if REQUEST.exists() else {}
    players = request.get("players", 1)
    as_client = request.get("mode") == "client"
    settings = ue.find_object(None, SETTINGS)
    if settings is None:
        raise RuntimeError(f"{SETTINGS} not found")
    report = {"players": players, "before": read(settings)}
    values = {"PlayNumberOfClients": players, "RunUnderOneProcess": True, "bLaunchSeparateServer": as_client,
              "PlayNetMode": "PIE_Client" if as_client else ("PIE_ListenServer" if players > 1 else "PIE_Standalone")}
    if players > 1:
        values.update({"NewWindowWidth": 960, "NewWindowHeight": 540})
    if not ue.ToolsetLibrary.set_object_properties(settings, json.dumps(values)):
        raise RuntimeError("play settings refused")
    report["after"] = read(settings)
    OUT.write_text(json.dumps(report, indent=1), encoding="utf-8")
    ue.log(f"MP_SETTINGS players={players} client={as_client}")


main()
