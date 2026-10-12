"""Builds the shot effects of the pirate shotgun and pistol (T-167) in the open editor, through the Unreal MCP (HTTP).

Creates in /Game/MonkeIsland/VFX/Weapons/ (never touches a level):
- M_Firearm_Flame: translucent sprite, emissive (ParticleColor x Glow, soft round mask): flame and sparks. Not the
  default sprite material (it renders everything white);
- M_Firearm_Smoke: translucent lit sprite (ParticleColor, soft round mask x Opacity): smoke and dust;
- NS_Shotgun_MuzzleFlame / NS_Shotgun_MuzzleSmoke / NS_Shotgun_Sparks: the blast of the muzzle (flame, a big slow
  cloud, a fast tight spray of sparks that draws the cone of the pellets), copies of the engine FountainLightweight
  template set through its module objects, all along the local +X (the game turns +X along the barrel);
- NS_Firearm_Impact: a puff of sand where a pellet or a ball strikes the ground or a wall (+X = hit normal).
Then fills the DAs: DA_ShotgunRules MuzzleBlastSystems (the three) and ImpactSystem, FireSound = SW_Shotgun_Fire
when it was imported (scripts/props/import_shotgun_sounds.py); DA_PistolRules ImpactSystem.
Existing assets are kept and set again (re-run safe).

Usage (editor open, editor lock taken, no PIE): python scripts/vfx/build_firearm_vfx_ue.py
"""
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from build_blood_assets_ue import (ASSET, MAT, NIAGARA_TEMPLATE, OBJ, Graph, Mcp, distribution, exists, get, put, ref,  # noqa: E402
	save)

FOLDER = "/Game/MonkeIsland/VFX/Weapons"
FLAME_MATERIAL = "M_Firearm_Flame"
SMOKE_MATERIAL = "M_Firearm_Smoke"
SHOTGUN_RULES = "/Game/MonkeIsland/Weapons/Shotgun/DA_ShotgunRules"
PISTOL_RULES = "/Game/MonkeIsland/Weapons/Pistol/DA_PistolRules"
SHOT_SOUND = "/Game/MonkeIsland/Weapons/Shotgun/Audio/SW_Shotgun_Fire"


def round_mask(g, x, y):
	"""Soft disc over the sprite: SphereMask of the texture coordinates around (0.5, 0.5)"""
	uv = g.node("TextureCoordinate", x - 300, y)
	centre = g.node("Constant2Vector", x - 300, y + 120, R=0.5, G=0.5)
	mask = g.node("SphereMask", x, y, AttenuationRadius=0.5, HardnessPercent=0.0)
	g.link(uv, mask, "A")
	g.link(centre, mask, "B")
	return mask


def build_flame_material(m):
	path = f"{FOLDER}/{FLAME_MATERIAL}"
	if exists(m, path):
		print("kept", path)
		return
	m.call(MAT, "create_material", {"folder_path": FOLDER, "asset_name": FLAME_MATERIAL})
	m.call(OBJ, "set_properties", {"instance": ref(path), "values": json.dumps({
		"BlendMode": "BLEND_Additive", "ShadingModel": "MSM_Unlit", "bUsedWithNiagaraSprites": True})})
	g = Graph(m, path)
	glow = g.node("ScalarParameter", -600, 300, ParameterName="Glow", Group="Firearm", SortPriority=0,
		Desc="Brightness of the flame and the sparks: the particle colour of each system times this", DefaultValue=6.0)
	particle = g.node("ParticleColor", -900, -100)
	mask = round_mask(g, -600, 0)
	colour = g.node("Multiply", -300, -100)
	g.link(particle, colour, "A", "RGB")
	g.link(glow, colour, "B")
	alpha = g.node("Multiply", -300, 100)
	g.link(particle, alpha, "A", "A")
	g.link(mask, alpha, "B")
	emissive = g.node("Multiply", -100, 0)
	g.link(colour, emissive, "A")
	g.link(alpha, emissive, "B")
	g.output(emissive, "MP_EmissiveColor")
	m.call(MAT, "recompile", {"material_or_function": ref(path)})
	save(m, [path])
	print("built", path)


def build_smoke_material(m):
	path = f"{FOLDER}/{SMOKE_MATERIAL}"
	if exists(m, path):
		print("kept", path)
		return
	m.call(MAT, "create_material", {"folder_path": FOLDER, "asset_name": SMOKE_MATERIAL})
	m.call(OBJ, "set_properties", {"instance": ref(path), "values": json.dumps({
		"BlendMode": "BLEND_Translucent", "bUsedWithNiagaraSprites": True, "TranslucencyLightingMode": "TLM_VolumetricNonDirectional"})})
	g = Graph(m, path)
	opacity = g.node("ScalarParameter", -600, 300, ParameterName="Opacity", Group="Firearm", SortPriority=0,
		Desc="Density of the smoke or the dust (the particle alpha multiplies it)", DefaultValue=0.6)
	particle = g.node("ParticleColor", -900, -100)
	mask = round_mask(g, -600, 0)
	alpha = g.node("Multiply", -300, 100)
	g.link(particle, alpha, "A", "A")
	g.link(mask, alpha, "B")
	final = g.node("Multiply", -100, 150)
	g.link(alpha, final, "A")
	g.link(opacity, final, "B")
	g.output(particle, "MP_BaseColor", "RGB")
	g.output(final, "MP_Opacity")
	m.call(MAT, "recompile", {"material_or_function": ref(path)})
	save(m, [path])
	print("built", path)


def configure(m, name, spec):
	"""One burst along the local +X, as scripts/vfx/build_blood_assets_ue.py does, with its own gravity and material"""
	emitter = f"{FOLDER}/{name}.{name}:Fountain_1"
	module = lambda n: f"{emitter}.NiagaraStatelessModule_{n}_0"
	state = get(m, emitter, "emitterState")
	state["loopBehavior"] = "Once"
	state["loopDuration"] = distribution(state["loopDuration"], spec["duration"])
	state["bLoopDelayEnabled"] = False
	state["inactiveResponse"] = "Complete"
	spawn = get(m, emitter, "spawnInfos")[0]
	spawn["type"] = "Burst"
	spawn["spawnTime"] = 0.0
	spawn["amount"].update({"mode": "UniformRange", "min": spec["burst"][0], "max": spec["burst"][1]})
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
	put(m, module("ShapeLocation"), bModuleEnabled=False)
	velocity = module("AddVelocity")
	put(m, velocity, velocityType="InCone", coneRotationType="Direction", coneAngle=spec["cone"],
		coneDirection=distribution(get(m, velocity, "coneDirection"), [1.0, 0.0, 0.0]),
		coneVelocityDistribution=distribution(get(m, velocity, "coneVelocityDistribution"), *spec["speed"]))
	put(m, module("Drag"), dragDistribution=distribution(get(m, module("Drag"), "dragDistribution"), spec["drag"]))
	put(m, module("GravityForce"), bModuleEnabled=True,
		gravityDistribution=distribution(get(m, module("GravityForce"), "gravityDistribution"), [0.0, 0.0, spec["gravity"]]))
	put(m, module("CurlNoiseForce"), bModuleEnabled=spec.get("curl", False))
	material = spec["material"]
	put(m, f"{emitter}.Renderer", material={"refPath": f"{FOLDER}/{material}.{material}"})
	print("configured", name)


# particle colour: brightness of the flame material (x Glow) or colour and density of the smoke material
SYSTEMS = {
	# a short tongue of fire out of the barrel, gone in a tenth of a second
	"NS_Shotgun_MuzzleFlame": {"duration": 0.15, "burst": (26, 36), "lifetime": (0.04, 0.11), "size": (6.0, 20.0), "cone": 14.0,
		"speed": (700.0, 2000.0), "drag": 8.0, "gravity": 0.0, "bounds": 300.0, "material": FLAME_MATERIAL,
		"color": {"r": 1.0, "g": 0.42, "b": 0.1, "a": 1.0}},
	# black powder: a big grey cloud thrown forward, slowing and drifting up
	"NS_Shotgun_MuzzleSmoke": {"duration": 2.5, "burst": (18, 26), "lifetime": (1.2, 2.4), "size": (18.0, 48.0), "cone": 20.0,
		"speed": (150.0, 650.0), "drag": 3.0, "gravity": 25.0, "bounds": 600.0, "material": SMOKE_MATERIAL, "curl": True,
		"color": {"r": 0.62, "g": 0.6, "b": 0.56, "a": 0.7}},
	# burning grains and the pellets' streaks: a fast, tight spray that draws the cone of the shot
	"NS_Shotgun_Sparks": {"duration": 0.4, "burst": (24, 34), "lifetime": (0.1, 0.28), "size": (0.6, 1.6), "cone": 3.0,
		"speed": (4000.0, 7500.0), "drag": 0.4, "gravity": -980.0, "bounds": 2500.0, "material": FLAME_MATERIAL,
		"color": {"r": 1.0, "g": 0.55, "b": 0.2, "a": 1.0}},
	# a puff of coral sand where a pellet strikes
	"NS_Firearm_Impact": {"duration": 1.0, "burst": (8, 14), "lifetime": (0.4, 0.9), "size": (5.0, 16.0), "cone": 35.0,
		"speed": (120.0, 420.0), "drag": 2.5, "gravity": -300.0, "bounds": 200.0, "material": SMOKE_MATERIAL,
		"color": {"r": 0.78, "g": 0.72, "b": 0.6, "a": 0.8}},
}


def build_systems(m):
	for name, spec in SYSTEMS.items():
		path = f"{FOLDER}/{name}"
		if not exists(m, path):
			m.call(ASSET, "duplicate", {"path": NIAGARA_TEMPLATE, "new_path": path})
			print("duplicated", NIAGARA_TEMPLATE, "->", path)
		configure(m, name, spec)
	save(m, [f"{FOLDER}/{n}" for n in SYSTEMS])


def fill_rules(m):
	system = lambda n: ref(f"{FOLDER}/{n}")
	blast = [system("NS_Shotgun_MuzzleFlame"), system("NS_Shotgun_MuzzleSmoke"), system("NS_Shotgun_Sparks")]
	values = {"MuzzleBlastSystems": blast, "ImpactSystem": system("NS_Firearm_Impact")}
	if exists(m, SHOT_SOUND):
		values["FireSound"] = ref(SHOT_SOUND)
	else:
		print("no", SHOT_SOUND, "yet: run scripts/props/import_shotgun_sounds.py, then this script again")
	put(m, ref(SHOTGUN_RULES)["refPath"], **values)
	put(m, ref(PISTOL_RULES)["refPath"], ImpactSystem=system("NS_Firearm_Impact"))
	save(m, [SHOTGUN_RULES, PISTOL_RULES])
	print("rules filled:", ", ".join(values))


def main():
	m = Mcp()
	m.call(ASSET, "create_folder", {"path": FOLDER})
	build_flame_material(m)
	build_smoke_material(m)
	build_systems(m)
	fill_rules(m)


if __name__ == "__main__":
	main()
