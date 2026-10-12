"""Builds (or refreshes) the combat reactions showcase of L_Test_Flat (T-147), through the Unreal MCP.

One row per mob (SK_<Mob>), one SkeletalMeshActor per clip in "Use Animation Asset" mode, looping, with a white
TextRender above it showing the clip name. Outliner folder AnimShowcase, south-east of the arena (x < -3000,
y > 1500): far from the arena circle (r 25 m at the origin), the pool (y < -5000) and the physics area.
Re-run it after scripts/character/import_combat_reactions.py: the actors already there are kept and receive their
AnimSequence (/Game/MonkeIsland/Characters/<Mob>/Anims/Combat/A_<Mob>_<Clip>); a clip not imported yet leaves the
mesh in its reference pose. Then save L_Test_Flat (the script does it unless --no-save).

Usage (editor open on L_Test_Flat, no PIE): python scripts/character/showcase_combat_reactions.py [--no-save]
"""
import json
import sys
import urllib.request

URL = "http://127.0.0.1:8000/mcp"
HDR = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
LEVEL = "/Game/MonkeIsland/Maps/Test/L_Test_Flat"
FOLDER = "AnimShowcase"
MOBS = ["CocoWarrior", "CrabGuardian", "AshWatcher", "Villager"]  # BrambleStalker: no skeleton in the project
CLIPS = ["Musket_Impact", "Musket_Burst", "Sabre_LeftToRight", "Sabre_RightToLeft", "Sabre_Down", "Sabre_Low",
	"Musket_StaggerRecover", "Musket_ArticulatedFall"]
ORIGIN_X, ORIGIN_Y = -3300.0, 1600.0  # first mob, first clip
ROW_STEP, COL_STEP = -500.0, 300.0  # rows go south, clips go east (3 m apart)
MESH_YAW = 90.0  # the mobs (forward = +Y in the mesh) face the viewer standing south of the showcase
TEXT_YAW = 180.0
SCENE = "editor_toolset.toolsets.scene.SceneTools"
ACTOR = "editor_toolset.toolsets.actor.ActorTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"
ASSET = "editor_toolset.toolsets.asset.AssetTools"
SKT = "editor_toolset.toolsets.skeletal_mesh.SkeletalMeshTools"


class Mcp:
	def __init__(self):
		self.sid = None
		self.sid, _ = self._post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
			"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "showcase", "version": "1"}}})
		self._post({"jsonrpc": "2.0", "method": "notifications/initialized"})

	def _post(self, body, timeout=600):
		h = dict(HDR)
		if self.sid:
			h["Mcp-Session-Id"] = self.sid
		req = urllib.request.Request(URL, json.dumps(body).encode(), h)
		with urllib.request.urlopen(req, timeout=timeout) as r:
			return r.headers.get("Mcp-Session-Id") or self.sid, r.read().decode("utf-8", "replace")

	def call(self, toolset, tool, args):
		_, txt = self._post({"jsonrpc": "2.0", "id": 2, "method": "tools/call", "params": {
			"name": "call_tool", "arguments": {"toolset_name": toolset, "tool_name": tool,
				"arguments": args}}})
		if not txt.startswith("{"):
			txt = [l[5:].strip() for l in txt.splitlines() if l.startswith("data:")][-1]
		body = json.loads(txt)
		if "error" in body:
			raise RuntimeError(f"{tool}: {body['error']}")
		text = "\n".join(c.get("text", "") for c in body["result"].get("content", []))
		try:
			out = json.loads(text)
		except json.JSONDecodeError:
			raise RuntimeError(f"{tool}: {text}")
		return out.get("returnValue", out) if isinstance(out, dict) else out


def obj_path(path):
	return path + "." + path.rsplit("/", 1)[-1]


def xform(x, y, z, yaw):
	return {"location": {"x": x, "y": y, "z": z}, "rotation": {"pitch": 0, "yaw": yaw, "roll": 0},
		"scale": {"x": 1, "y": 1, "z": 1}}


def main():
	m = Mcp()
	if m.call("EditorToolset.EditorAppToolset", "IsPIERunning", {}):
		sys.exit("a PIE is running: stop it first (never stop someone else's)")
	if m.call(SCENE, "get_current_level", {}) != LEVEL:
		sys.exit(f"open {LEVEL} first")
	existing = {}
	if FOLDER in (m.call(SCENE, "get_folders", {}) or []):
		for a in m.call(SCENE, "get_actors_in_folder", {"folder_path": FOLDER, "recursive": True}) or []:
			existing[m.call(ACTOR, "get_label", {"actor": a})] = a
	report = {"placed": 0, "animated": 0, "ref_pose": []}
	for row, mob in enumerate(MOBS):
		mesh = f"/Game/MonkeIsland/Characters/{mob}/Meshes/SK_{mob}"
		top = m.call(SKT, "get_bounds", {"mesh": {"refPath": obj_path(mesh)}})
		height = top["origin"]["z"] + top["boxExtent"]["z"]
		x = ORIGIN_X + row * ROW_STEP
		for col, clip in enumerate(CLIPS):
			y = ORIGIN_Y + col * COL_STEP
			label = f"Show_{mob}_{clip}"
			actor = existing.get(label)
			if not actor:
				actor = m.call(SCENE, "add_to_scene_from_asset", {"asset_path": mesh, "name": label,
					"xform": xform(x, y, 0.0, MESH_YAW)})
				m.call(ACTOR, "set_label", {"actor": actor, "label": label})
				m.call(SCENE, "set_actor_folder", {"actor": actor, "folder_path": FOLDER})
				report["placed"] += 1
			comp = m.call(ACTOR, "get_components", {"actor": actor})[0]
			anim = f"/Game/MonkeIsland/Characters/{mob}/Anims/Combat/A_{mob}_{clip}"
			has_anim = bool(m.call(ASSET, "exists", {"path": anim}))
			data = {"animToPlay": obj_path(anim) if has_anim else "None", "bSavedLooping": True,
				"bSavedPlaying": True, "savedPosition": 0, "savedPlayRate": 1}
			m.call(OBJ, "set_properties", {"instance": comp, "values": json.dumps(
				{"AnimationMode": "AnimationSingleNode", "AnimationData": data})})
			if has_anim:
				report["animated"] += 1
			else:
				report["ref_pose"].append(label)
			text_label = f"ShowText_{mob}_{clip}"
			if text_label not in existing:
				text = m.call(SCENE, "add_to_scene_from_class", {"actor_type": {"refPath": "/Script/Engine.TextRenderActor"},
					"name": text_label, "xform": xform(x, y, height + 40.0, TEXT_YAW)})
				m.call(ACTOR, "set_label", {"actor": text, "label": text_label})
				m.call(SCENE, "set_actor_folder", {"actor": text, "folder_path": FOLDER})
				tcomp = [c for c in m.call(ACTOR, "get_components", {"actor": text}) if "TextRender" in c["refPath"]][0]
				m.call(OBJ, "set_properties", {"instance": tcomp, "values": json.dumps({"Text": f"{mob}\n{clip}",
					"TextRenderColor": {"R": 255, "G": 255, "B": 255, "A": 255}, "HorizontalAlignment": "EHTA_Center",
					"WorldSize": 16})})
	if "--no-save" not in sys.argv:
		report["saved"] = m.call(ASSET, "save_assets", {"asset_paths": [LEVEL]})
	print(json.dumps(report, indent=1))


if __name__ == "__main__":
	main()
