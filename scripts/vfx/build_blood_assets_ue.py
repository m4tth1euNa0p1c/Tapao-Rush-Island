"""Builds the blood assets of the tiki creatures (T-153) in the open editor, through the Unreal MCP (HTTP JSON-RPC).

Creates in /Game/MonkeIsland/VFX/Blood/ (never touches a level):
- Textures/T_Blood_* imported from art/textures/vfx/ (scripts/vfx/gen_blood_textures.py), linear grayscale masks;
- M_Blood_Decal: deferred decal master, translucent, every parameter described (group Blood);
- MI_Blood_Splat_A/B/C and MI_Blood_Pool (pool mode: grows from GrowStartTime over GrowDuration, no tick);
- NS_Blood_MusketImpact / NS_Blood_SliceSpray / NS_Blood_Chunks duplicated from the engine Niagara templates
  (their modules are not reachable through the MCP: see docs/features/characters-tiki.md, section Sang).
Existing assets are kept (re-run safe); --rebuild-material deletes and rebuilds M_Blood_Decal.

Niagara: the three systems copy the stateless FountainLightweight template, set through its module objects
(ObjectTools): spawn, lifetime, colour, size, cone velocity along the local +X (the game turns +X toward the hit),
gravity, drag, loops. The game sets the user parameters BloodColor / Direction / Duration when present; the copies
have none (the MCP cannot add user parameters): the colour is baked from BLOOD_COLOR, the direction comes from the
component rotation.

Usage (editor open, editor lock taken, no PIE): python scripts/vfx/build_blood_assets_ue.py [--rebuild-material]
"""
import json
import sys
import urllib.request
from pathlib import Path

URL = "http://127.0.0.1:8000/mcp"
HDR = {"Content-Type": "application/json", "Accept": "application/json, text/event-stream"}
ROOT = Path(__file__).resolve().parents[2]
FOLDER = "/Game/MonkeIsland/VFX/Blood"
TEX_FOLDER = FOLDER + "/Textures"
TEXTURES = ["T_Blood_Splat_A", "T_Blood_Splat_B", "T_Blood_Splat_C", "T_Blood_Pool", "T_Blood_Droplet", "T_Blood_Noise"]
MATERIAL = "M_Blood_Decal"
BLOOD_COLOR = {"r": 0.30, "g": 0.005, "b": 0.004, "a": 1.0}
# every system is a copy of the stateless (lightweight) fountain: its modules are plain objects the MCP can set
NIAGARA_TEMPLATE = "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight"
NIAGARA = ["NS_Blood_MusketImpact", "NS_Blood_SliceSpray", "NS_Blood_Chunks"]
PARTICLE_MATERIAL = "M_Blood_Particle"
# drops: dark red, linear (the decals take DA_TikiRules.BloodColor from the game)
PARTICLE_RED = {"r": 0.35, "g": 0.0, "b": 0.0, "a": 1.0}
ASSET = "editor_toolset.toolsets.asset.AssetTools"
OBJ = "editor_toolset.toolsets.object.ObjectTools"
TEX = "editor_toolset.toolsets.texture.TextureTools"
MAT = "editor_toolset.toolsets.material.MaterialTools"
MI = "editor_toolset.toolsets.material_instance.MaterialInstanceTools"


class Mcp:
	def __init__(self):
		self.sid = None
		self.sid, _ = self._post({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {
			"protocolVersion": "2025-03-26", "capabilities": {}, "clientInfo": {"name": "blood", "version": "1"}}})
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
			"name": "call_tool", "arguments": {"toolset_name": toolset, "tool_name": tool, "arguments": args}}})
		if not txt.startswith("{"):
			txt = [l[5:].strip() for l in txt.splitlines() if l.startswith("data:")][-1]
		body = json.loads(txt)
		if "error" in body:
			raise RuntimeError(f"{tool}: {body['error']}")
		result = body["result"]
		text = "\n".join(c.get("text", "") for c in result.get("content", []))
		if result.get("isError"):
			raise RuntimeError(f"{tool}: {text}")
		try:
			out = json.loads(text, strict=False)
		except json.JSONDecodeError:
			return text
		return out.get("returnValue", out) if isinstance(out, dict) else out


def ref(path):
	"""Object reference of an asset path"""
	if "." not in path.rsplit("/", 1)[-1]:
		path = path + "." + path.rsplit("/", 1)[-1]
	return {"refPath": path}


def save(m, paths):
	"""Saves, unless a PIE is running (the editor refuses): the assets stay dirty, saved by a later run"""
	try:
		print("saved" if m.call(ASSET, "save_assets", {"asset_paths": paths}) else "NOT saved", len(paths), "asset(s)")
	except RuntimeError as error:
		print("NOT saved (PIE running?):", str(error)[:200])


def exists(m, path):
	"""AssetTools.exists answers False for existing assets in UE 5.8: the asset registry is asked instead"""
	folder, name = path.rsplit("/", 1)
	return path in (m.call(ASSET, "find_assets", {"folder_path": folder, "name": name, "recursive": False}) or [])


def import_textures(m):
	m.call(ASSET, "create_folder", {"path": TEX_FOLDER})
	for name in TEXTURES:
		path = f"{TEX_FOLDER}/{name}"
		if not exists(m, path):
			source = str(ROOT / "art" / "textures" / "vfx" / f"{name}.png")
			m.call(TEX, "import_file", {"folder_path": TEX_FOLDER, "asset_name": name, "source_file": source})
			print("imported", path)
		# linear masks, one channel
		m.call(OBJ, "set_properties", {"instance": ref(path), "values": json.dumps({"SRGB": False, "CompressionSettings": "TC_Grayscale"})})
	save(m, [f"{TEX_FOLDER}/{n}" for n in TEXTURES])


class Graph:
	"""Adds nodes to a material and links them"""

	def __init__(self, m, material):
		self.m, self.material = m, material

	def node(self, cls, x, y, **props):
		out = self.m.call(MAT, "add_expression", {"material_or_function": ref(self.material),
			"expression_class": {"refPath": f"/Script/Engine.MaterialExpression{cls}"}, "x": x, "y": y})
		node = out["refPath"] if isinstance(out, dict) and "refPath" in out else out
		if isinstance(node, dict):
			node = json.dumps(node)
		if props:
			self.m.call(OBJ, "set_properties", {"instance": {"refPath": node}, "values": json.dumps(props)})
		return node

	def param(self, cls, name, desc, x, y, priority, **props):
		return self.node(cls, x, y, ParameterName=name, Group="Blood", Desc=desc, SortPriority=priority, **props)

	def link(self, src, dst, pin, out=""):
		self.m.call(MAT, "connect_expressions", {"from_expression": {"refPath": src}, "from_output_name": out,
			"to_expression": {"refPath": dst}, "to_input_name": pin})

	def output(self, src, prop, out=""):
		self.m.call(MAT, "connect_to_output", {"expression": {"refPath": src}, "output_name": out, "material_property": prop})


def build_material(m, rebuild):
	path = f"{FOLDER}/{MATERIAL}"
	if exists(m, path):
		if not rebuild:
			print("kept", path)
			return
		m.call(ASSET, "delete", {"path": path})
	m.call(MAT, "create_material", {"folder_path": FOLDER, "asset_name": MATERIAL})
	m.call(OBJ, "set_properties", {"instance": ref(path), "values": json.dumps({
		"MaterialDomain": "MD_DeferredDecal", "BlendMode": "BLEND_Translucent"})})
	g = Graph(m, path)
	tex = lambda n: f"{TEX_FOLDER}/{n}.{n}"
	mask = g.param("TextureSampleParameter2D", "Mask",
		"Shape of the blood, linear grayscale: 1 = blood. A splat (T_Blood_Splat_*) or the pool distance field (T_Blood_Pool, 1 at the centre)",
		-1400, -200, 0, Texture=tex("T_Blood_Splat_A"), SamplerType="SAMPLERTYPE_LinearGrayscale")
	noise = g.param("TextureSampleParameter2D", "Noise", "Tiling noise breaking the flat colour into darker clots (T_Blood_Noise)",
		-1400, 300, 1, Texture=tex("T_Blood_Noise"), SamplerType="SAMPLERTYPE_LinearGrayscale")
	color = g.param("VectorParameter", "BloodColor",
		"Colour of fresh blood (linear). Written by the game from DA_TikiRules.BloodColor, to tint by species", -800, 200, 2,
		DefaultValue={"R": BLOOD_COLOR["r"], "G": BLOOD_COLOR["g"], "B": BLOOD_COLOR["b"], "A": 1.0})
	clot = g.param("ScalarParameter", "ClotDarkness",
		"Brightness of the darkest clots relative to BloodColor (0 = black, 1 = flat colour)", -1000, 400, 3, DefaultValue=0.55)
	rough = g.param("ScalarParameter", "Roughness", "Roughness of the blood: low = wet and glossy, higher = drying", -400, 500, 4, DefaultValue=0.22)
	opacity = g.param("ScalarParameter", "Opacity", "Overall opacity of the decal", -400, 700, 5, DefaultValue=0.95)
	soft = g.param("ScalarParameter", "EdgeSoftness",
		"Width of the rim fade in mask units: small = sharp splat edges, larger = soft seeping rim", -1000, -500, 6, DefaultValue=0.06)
	thresh = g.param("ScalarParameter", "MaskThreshold", "Mask value under which a splat shows nothing (cuts the faint halo)", -1000, -700, 7, DefaultValue=0.08)
	pool = g.param("ScalarParameter", "PoolMode",
		"0 = splat (fixed shape), 1 = pool: the shape spreads from the centre of the mask from GrowStartTime over GrowDuration", -1000, -900, 8, DefaultValue=0.0)
	start = g.param("ScalarParameter", "GrowStartTime",
		"World time (s) the pool starts growing, written by the game per pool; the default keeps it full grown", -1600, -1100, 9, DefaultValue=-100000.0)
	dur = g.param("ScalarParameter", "GrowDuration", "Time (s) the pool takes from nothing to full size, written by the game (DA_TikiRules.PoolGrowDuration)",
		-1600, -900, 10, DefaultValue=8.0)
	time = g.node("Time", -1600, -1300)
	age = g.node("Subtract", -1300, -1200)
	g.link(time, age, "A")
	g.link(start, age, "B")
	ratio = g.node("Divide", -1150, -1100)
	g.link(age, ratio, "A")
	g.link(dur, ratio, "B")
	growth = g.node("Saturate", -1000, -1100)
	g.link(ratio, growth, "")
	# growing: the cut-off falls from 1 to 0, the pool spreads from its centre (highest mask) to its rim
	cut_pool = g.node("OneMinus", -850, -1100)
	g.link(growth, cut_pool, "")
	cutoff = g.node("LinearInterpolate", -700, -800)
	g.link(thresh, cutoff, "A")
	g.link(cut_pool, cutoff, "B")
	g.link(pool, cutoff, "Alpha")
	over = g.node("Subtract", -550, -400)
	g.link(mask, over, "A", "R")
	g.link(cutoff, over, "B")
	edge = g.node("Divide", -400, -400)
	g.link(over, edge, "A")
	g.link(soft, edge, "B")
	shape = g.node("Saturate", -250, -400)
	g.link(edge, shape, "")
	life = g.node("DecalLifetimeOpacity", -400, -200)
	fade = g.node("Multiply", -100, -300)
	g.link(shape, fade, "A")
	g.link(life, fade, "B")
	final = g.node("Multiply", 50, -200)
	g.link(fade, final, "A")
	g.link(opacity, final, "B")
	shade = g.node("LinearInterpolate", -700, 300, ConstB=1.0)
	g.link(clot, shade, "A")
	g.link(noise, shade, "Alpha", "R")
	base = g.node("Multiply", -400, 200)
	g.link(color, base, "A")
	g.link(shade, base, "B")
	g.output(base, "MP_BaseColor")
	g.output(rough, "MP_Roughness")
	g.output(final, "MP_Opacity")
	m.call(MAT, "recompile", {"material_or_function": ref(path)})
	save(m, [path])
	print("built", path)


def build_instances(m):
	parent = ref(f"{FOLDER}/{MATERIAL}")
	instances = {
		"MI_Blood_Splat_A": ("T_Blood_Splat_A", {"PoolMode": 0.0}),
		"MI_Blood_Splat_B": ("T_Blood_Splat_B", {"PoolMode": 0.0, "EdgeSoftness": 0.05}),
		"MI_Blood_Splat_C": ("T_Blood_Splat_C", {"PoolMode": 0.0, "EdgeSoftness": 0.04}),
		"MI_Blood_Pool": ("T_Blood_Pool", {"PoolMode": 1.0, "EdgeSoftness": 0.1, "Roughness": 0.12, "ClotDarkness": 0.4}),
	}
	for name, (mask, scalars) in instances.items():
		path = f"{FOLDER}/{name}"
		if not exists(m, path):
			m.call(MI, "create", {"folder_path": FOLDER, "asset_name": name, "parent": parent})
		m.call(MI, "set_texture_parameter", {"instance": ref(path), "name": "Mask", "value": ref(f"{TEX_FOLDER}/{mask}")})
		for key, value in scalars.items():
			m.call(MI, "set_scalar_parameter", {"instance": ref(path), "name": key, "value": value})
		m.call(MI, "set_vector_parameter", {"instance": ref(path), "name": "BloodColor", "value": BLOOD_COLOR})
		print("instance", path)
	save(m, [f"{FOLDER}/{n}" for n in instances])


def build_particle_material(m):
	"""Lit translucent sprite of the blood drops: the colour comes from the material (BloodColor x Particle Color as a
	brightness multiplier), so a particle colour left white by the emitter still gives red blood"""
	path = f"{FOLDER}/{PARTICLE_MATERIAL}"
	if exists(m, path):
		return
	m.call(MAT, "create_material", {"folder_path": FOLDER, "asset_name": PARTICLE_MATERIAL})
	m.call(OBJ, "set_properties", {"instance": ref(path), "values": json.dumps({
		"BlendMode": "BLEND_Translucent", "bUsedWithNiagaraSprites": True, "TranslucencyLightingMode": "TLM_VolumetricNonDirectional"})})
	g = Graph(m, path)
	drop = g.param("TextureSampleParameter2D", "Droplet", "Shape of one drop (T_Blood_Droplet), linear grayscale", -900, 0, 0,
		Texture=f"{TEX_FOLDER}/T_Blood_Droplet.T_Blood_Droplet", SamplerType="SAMPLERTYPE_LinearGrayscale")
	color = g.param("VectorParameter", "BloodColor",
		"Colour of the drops (linear, dark red). The particle colour of each system multiplies it: above 1 = fine bright drops, below 1 = dark chunks",
		-900, -400, 1, DefaultValue={"R": PARTICLE_RED["r"], "G": PARTICLE_RED["g"], "B": PARTICLE_RED["b"], "A": 1.0})
	glow = g.param("ScalarParameter", "NightReadability", "Share of the colour also emitted, so the drops still read at night (0 = none)", -500, 500, 2, DefaultValue=0.08)
	rough = g.param("ScalarParameter", "Roughness", "Roughness of the drops: wet and glossy", -500, 300, 3, DefaultValue=0.15)
	particle = g.node("ParticleColor", -900, -150)
	base = g.node("Multiply", -600, -300)
	g.link(color, base, "A", "RGB")
	g.link(particle, base, "B", "RGB")
	emit = g.node("Multiply", -300, 400)
	g.link(base, emit, "A")
	g.link(glow, emit, "B")
	alpha = g.node("Multiply", -400, 100)
	g.link(particle, alpha, "A", "A")
	g.link(drop, alpha, "B", "R")
	g.output(base, "MP_BaseColor")
	g.output(emit, "MP_EmissiveColor")
	g.output(rough, "MP_Roughness")
	g.output(alpha, "MP_Opacity")
	m.call(MAT, "recompile", {"material_or_function": ref(path)})
	save(m, [path])
	print("built", path)


def distribution(current, low, high=None):
	"""A float or vector distribution set to a constant (high None) or a uniform range"""
	value = dict(current)
	if isinstance(low, (list, tuple)):
		keys = "xyz"[:len(low)]
		value["min"] = dict(zip(keys, low))
		value["max"] = dict(zip(keys, high if high is not None else low))
		value["mode"] = "NonUniformConstant" if high is None else "NonUniformRange"
		value["channelConstantsAndRanges"] = list(low) + ([] if high is None else list(high))
	else:
		value["min"], value["max"] = low, low if high is None else high
		value["mode"] = "UniformConstant" if high is None else "UniformRange"
		value["channelConstantsAndRanges"] = [low] if high is None else [low, high]
	return value


def get(m, path, prop):
	out = m.call(OBJ, "get_properties", {"instance": {"refPath": path}, "properties": [prop]})
	out = json.loads(out, strict=False) if isinstance(out, str) else out
	return out[prop]


def put(m, path, **values):
	m.call(OBJ, "set_properties", {"instance": {"refPath": path}, "values": json.dumps(values)})


def configure_system(m, name, spec):
	emitter = f"{FOLDER}/{name}.{name}:Fountain_1"
	module = lambda n: f"{emitter}.NiagaraStatelessModule_{n}_0"
	# loops: a single burst, or a pulsed jet (the game stops it after DA_TikiRules.SliceSprayDuration)
	state = get(m, emitter, "emitterState")
	state["loopBehavior"] = spec["loop"]
	state["loopDuration"] = distribution(state["loopDuration"], spec["loop_duration"])
	state["bLoopDelayEnabled"] = spec.get("loop_delay", 0.0) > 0.0
	state["loopDelay"] = distribution(state["loopDelay"], spec.get("loop_delay", 0.0))
	state["inactiveResponse"] = "Complete"
	spawn = get(m, emitter, "spawnInfos")[0]
	if spec.get("burst"):
		spawn["type"] = "Burst"
		spawn["spawnTime"] = 0.0
		spawn["amount"].update({"mode": "UniformRange", "min": spec["burst"][0], "max": spec["burst"][1]})
	else:
		spawn["type"] = "Rate"
		spawn["rate"] = distribution(spawn["rate"], spec["rate"])
	bounds = spec["bounds"]
	put(m, emitter, emitterState=state, spawnInfos=[spawn],
		fixedBounds={"min": {"x": -bounds, "y": -bounds, "z": -bounds}, "max": {"x": bounds, "y": bounds, "z": bounds}, "isValid": True})

	init = module("InitializeParticle")
	color = get(m, init, "colorDistribution")
	color["values"] = [spec["color"], spec["color"]]
	color["mode"] = "NonUniformConstant"
	color["channelConstantsAndRanges"] = [spec["color"][c] for c in "rgba"]
	sizes = get(m, init, "spriteSizeDistribution")
	sizes["min"], sizes["max"] = {"x": spec["size"][0], "y": spec["size"][0]}, {"x": spec["size"][1], "y": spec["size"][1]}
	sizes["mode"], sizes["channelConstantsAndRanges"] = "UniformRange", [spec["size"][0], spec["size"][1]]
	put(m, init, lifetimeDistribution=distribution(get(m, init, "lifetimeDistribution"), *spec["lifetime"]),
		colorDistribution=color, spriteSizeDistribution=sizes)

	# from the point of the wound, in a cone around the local +X (the game turns +X toward the hit: UTikiBloodSubsystem)
	put(m, module("ShapeLocation"), bModuleEnabled=False)
	velocity = module("AddVelocity")
	# the module reads ConeDirection only with ConeRotationType Direction (else the cone points up, 2026-10-11)
	put(m, velocity, velocityType="InCone", coneRotationType="Direction", coneAngle=spec["cone"],
		coneDirection=distribution(get(m, velocity, "coneDirection"), [1.0, 0.0, 0.0]),
		coneVelocityDistribution=distribution(get(m, velocity, "coneVelocityDistribution"), *spec["speed"]))
	put(m, module("Drag"), dragDistribution=distribution(get(m, module("Drag"), "dragDistribution"), spec["drag"]))
	put(m, module("GravityForce"), bModuleEnabled=True,
		gravityDistribution=distribution(get(m, module("GravityForce"), "gravityDistribution"), [0.0, 0.0, -980.0]))
	put(m, module("CurlNoiseForce"), bModuleEnabled=False)
	put(m, f"{emitter}.Renderer", material={"refPath": f"{FOLDER}/{PARTICLE_MATERIAL}.{PARTICLE_MATERIAL}"})
	print("configured", name)


# particle colour = brightness multiplier of M_Blood_Particle's BloodColor (fine drops lighter, chunks darker)
FINE_DROPS = {"r": 1.3, "g": 1.3, "b": 1.3, "a": 1.0}
JET = {"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0}
CHUNKS = {"r": 0.55, "g": 0.55, "b": 0.55, "a": 1.0}
SYSTEMS = {
	# exit spray of a musket ball and its drops falling, under a second (the mist of the brief is not a separate
	# emitter: the small, short-lived fast drops play it)
	"NS_Blood_MusketImpact": {"loop": "Once", "loop_duration": 0.9, "burst": (35, 50), "lifetime": (0.25, 0.8),
		"size": (0.8, 3.5), "cone": 22.0, "speed": (350.0, 900.0), "drag": 1.2, "bounds": 400.0, "color": FINE_DROPS},
	# pulsed jet out of a cut: 0.22 s on, 0.12 s off, until the game deactivates it
	"NS_Blood_SliceSpray": {"loop": "Infinite", "loop_duration": 0.22, "loop_delay": 0.12, "rate": 160.0, "lifetime": (0.4, 0.9),
		"size": (0.8, 2.5), "cone": 10.0, "speed": (180.0, 380.0), "drag": 0.6, "bounds": 300.0, "color": JET},
	# a few bigger, darker chunks and splashes
	"NS_Blood_Chunks": {"loop": "Once", "loop_duration": 1.2, "burst": (10, 16), "lifetime": (0.7, 1.2),
		"size": (2.5, 6.0), "cone": 55.0, "speed": (220.0, 520.0), "drag": 0.3, "bounds": 400.0,
		"color": CHUNKS},
}


def build_niagara(m):
	build_particle_material(m)
	for name in NIAGARA:
		path = f"{FOLDER}/{name}"
		if not exists(m, path):
			m.call(ASSET, "duplicate", {"path": NIAGARA_TEMPLATE, "new_path": path})
			print("duplicated", NIAGARA_TEMPLATE, "->", path)
		configure_system(m, name, SYSTEMS[name])
	save(m, [f"{FOLDER}/{n}" for n in NIAGARA])


def main():
	m = Mcp()
	m.call(ASSET, "create_folder", {"path": FOLDER})
	import_textures(m)
	build_material(m, "--rebuild-material" in sys.argv)
	build_instances(m)
	build_niagara(m)


if __name__ == "__main__":
	main()
