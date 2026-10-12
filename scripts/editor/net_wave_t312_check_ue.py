"""T-312 / T-294 / T-295 / T-301 / T-306 (network wave of 2026-10-11): read-only check, after the build, that the editor
runs the new C++ and what the network values are. Changes nothing: safe to replay at any time, editor open, no game.

Run it from the editor (Python console or a lead session action) once the module is built with the wave's C++:
    py "C:/Users/matth/monk-island/scripts/editor/net_wave_t312_check_ue.py"
Report: monke_island/Saved/T312/net-wave-check.json ("passed": true when every new property is found).

1. DA_TikiRules: WarriorNet, VillagerNet, CorpseNetUpdateFrequency, ActionLateSoundSeconds (T-312, T-306).
2. DA_BoatRules: PhysicsReplicationMode, NetUpdateFrequency, SinkEjectUpSpeed (T-295).
3. DA_SharkRules: NetUpdateFrequency and the client glide (T-312, review R-34).
4. DA_MinkaKit, DA_ShrineKit: the lead of the doors (T-301).
5. Every Blueprint of a physics prop (native parent PhysicsPropActor: loot, artifacts, totems, relics, camp and marae
   objects): its NetRest component and its rates (T-312, T-294).
It does not prove the behaviour: that is the PIE campaign of docs/audit/revue-reseau-T268.md (trials 4, 5, 6, 15, 19).
"""
import datetime as dt
import json
from pathlib import Path

import unreal

ROOT = Path(__file__).resolve().parents[2]
REPORT = ROOT / "monke_island/Saved/T312/net-wave-check.json"

ASSETS = {
	"tiki_rules": ("/Game/MonkeIsland/Characters/Data/DA_TikiRules",
		["warrior_net", "villager_net", "corpse_net_update_frequency", "action_late_sound_seconds"]),
	"boat_rules": ("/Game/MonkeIsland/Boat/Data/DA_BoatRules",
		["physics_replication_mode", "net_update_frequency", "sink_eject_up_speed"]),
	"shark_rules": ("/Game/MonkeIsland/Characters/Shark/Data/DA_SharkRules",
		["net_update_frequency", "client_smooth_time", "client_extrapolate_time", "client_max_smooth_distance"]),
	"minka_kit": ("/Game/MonkeIsland/Environment/Village/Data/DA_MinkaKit",
		["lead_slide_share", "lead_extra_seconds", "max_lead_ping_seconds", "max_lead_distance"]),
	"shrine_kit": ("/Game/MonkeIsland/Environment/Village/Data/DA_ShrineKit",
		["lead_slide_share", "lead_extra_seconds", "max_lead_ping_seconds", "max_lead_distance"]),
}
REST_FIELDS = ["awake_net_update_frequency", "held_net_update_frequency", "dormant_at_rest", "check_interval", "glue_delay",
	"glue_still_distance", "glue_still_angle", "base_probe_depth"]
PROP_PARENT = "PhysicsPropActor"


def plain(value):
	"""A JSON-friendly copy of an editor property value"""
	if isinstance(value, (bool, int, float, str)) or value is None:
		return value
	if isinstance(value, unreal.StructBase):
		out = {}
		for name in ("net_update_frequency", "min_net_update_frequency", "net_cull_distance"):
			try:
				out[name] = value.get_editor_property(name)
			except Exception:
				pass
		return out or str(value)
	return str(value)


def read(obj, names):
	found, missing = {}, []
	for name in names:
		try:
			found[name] = plain(obj.get_editor_property(name))
		except Exception:
			missing.append(name)
	return found, missing


def check_assets(report):
	for key, (path, names) in ASSETS.items():
		asset = unreal.EditorAssetLibrary.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else None
		if not asset:
			report["assets"][key] = {"path": path, "error": "asset not found"}
			report["passed"] = False
			continue
		found, missing = read(asset, names)
		report["assets"][key] = {"path": path, "values": found, "missing": missing}
		if missing:
			report["passed"] = False


def check_props(report):
	registry = unreal.AssetRegistryHelpers.get_asset_registry()
	data = registry.get_assets_by_path("/Game/MonkeIsland", recursive=True)
	props = []
	for item in data:
		if str(item.asset_class_path.asset_name) != "Blueprint":
			continue
		parent = str(item.get_tag_value("NativeParentClass") or "")
		if parent.endswith("." + PROP_PARENT + "'") or parent.endswith("." + PROP_PARENT):
			props.append(str(item.package_name))
	report["props"] = {"count": len(props), "items": {}}
	for path in sorted(props):
		blueprint = unreal.EditorAssetLibrary.load_asset(path)
		cls = blueprint.generated_class() if blueprint else None
		cdo = unreal.get_default_object(cls) if cls else None
		rest = None
		if cdo:
			try:
				rest = cdo.get_editor_property("net_rest")
			except Exception:
				rest = None
		if not rest:
			report["props"]["items"][path] = {"error": "no NetRest component on the class default (C++ not built?)"}
			report["passed"] = False
			continue
		found, missing = read(rest, REST_FIELDS)
		report["props"]["items"][path] = {"values": found, "missing": missing}
		if missing:
			report["passed"] = False


def main():
	report = {"at": dt.datetime.now(dt.timezone.utc).isoformat(), "passed": True, "assets": {}, "props": {},
		"not_proven": ["behaviour in PIE (host + client, PktLag 100/200, PktLoss 1-3)", "stat net / Networking Insights",
			"packaged game", "two machines"]}
	check_assets(report)
	check_props(report)
	REPORT.parent.mkdir(parents=True, exist_ok=True)
	REPORT.write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
	unreal.log(f"T-312 net wave check: passed={report['passed']} -> {REPORT}")


main()
