"""T-321 (owner 2026-10-11: "fix les visuels de fumée, pas assez réalistes, comme la vraie fumée"): every smoke of the
weapons becomes billowing, self-shadowed puffs of a volume-rendered flipbook.
1. imports art/textures/vfx/T_Smoke_Flipbook.png (scripts/vfx/smoke_flipbook.py: 8 x 8 frames of one puff's life, RGB the
   sunlight through the puff, A its opacity) into /Game/MonkeIsland/VFX/Weapons, effects group, BC7;
2. M_Smoke_Flipbook: translucent lit sprite (volumetric lighting): ParticleSubUV of the flipbook (frames blended), colour
   = particle colour x baked light x LightGain, opacity = flipbook x particle alpha x Opacity, softened where it meets
   the ground or a wall (DepthFade) and faded when it reaches the camera (no grey screen when the carrier walks
   through his own smoke);
3. the smoke systems (lightweight emitters set through their module objects, as scripts/vfx/build_firearm_vfx_ue.py):
   the flipbook played over each particle's life (SubUV 8 x 8, linear), puffs that swell fast then slow down (size
   curve), a slow spin, drag and buoyancy, alpha that fades in and out; longer lives and bigger clouds for black powder;
   NS_Musket_MuzzleSmoke (copy of the shotgun's) for the long guns;
4. the rules: every black powder gun gets its smoke in MuzzleBlastSystems (pistols, blunderbuss and long guns had a
   flame only); the Veilleur's ash cloud becomes NS_Tiki_AshCloud puffs (the stretched sphere goes);
   UMusketRules / UFortSoldierRules SmokeSystem (C++ of 2026-10-11, replaces their sphere puffs) when the property
   exists in this build (reported otherwise).
Lead session action "smokeflipbook" (editor open, no game running). Report: monke_island/Saved/T321/smoke.json.
Replays re-import the texture and set the same values.
"""
import json
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T321/smoke.json"
SOURCE = ROOT / "art/textures/vfx/T_Smoke_Flipbook.png"
VFX = "/Game/MonkeIsland/VFX/Weapons"
TEXTURE = f"{VFX}/T_Smoke_Flipbook"
MATERIAL = f"{VFX}/M_Smoke_Flipbook"
MUSKET_SMOKE = "NS_Musket_MuzzleSmoke"
PISTOL_SMOKE = "NS_Pistol_MuzzleSmoke"
ASH_CLOUD = "NS_Tiki_AshCloud"
TIKI_RULES = "/Game/MonkeIsland/Characters/Data/DA_TikiRules"
MEL = ue.MaterialEditingLibrary
EAL = ue.EditorAssetLibrary
FRAMES = 8
# material defaults: brightness of the baked light, distance of the soft contact, near-camera fade (cm)
LIGHT_GAIN = 1.15
SOFT_FADE_CM = 60.0
NEAR_FADE_START_CM = 25.0
NEAR_FADE_LENGTH_CM = 140.0
# nodes of the graph built below
GRAPH_NODES = 17

# per system: duration (s), burst (particles), lifetime (s), start size (cm), grow (size at the end / start), cone (deg),
# speed (cm/s), drag, gravity (cm/s2, + rises), curl, colour (rgb tint, a = density), bounds (cm), alpha keys over life
POWDER_ALPHA = [(0.0, 0.0), (0.04, 1.0), (0.45, 0.8), (1.0, 0.0)]
STEAM_ALPHA = [(0.0, 0.0), (0.05, 1.0), (0.35, 0.6), (1.0, 0.0)]
SYSTEMS = {
    # black powder of the shotgun and the pistols: a big white-grey cloud thrown forward, slowing, swelling, drifting up
    "NS_Shotgun_MuzzleSmoke": {"duration": 7.0, "burst": (10, 14), "lifetime": (2.8, 5.5), "size": (22.0, 55.0), "grow": 4.2,
                               "cone": 16.0, "speed": (120.0, 750.0), "drag": 3.2, "gravity": 22.0, "curl": True,
                               "color": (0.86, 0.85, 0.82, 0.9), "bounds": 900.0, "alpha": POWDER_ALPHA},
    # the long guns burn more powder: a longer, denser cloud along the shot
    MUSKET_SMOKE: {"duration": 8.0, "burst": (12, 16), "lifetime": (3.5, 6.5), "size": (25.0, 60.0), "grow": 4.5,
                   "cone": 12.0, "speed": (150.0, 900.0), "drag": 3.0, "gravity": 20.0, "curl": True,
                   "color": (0.88, 0.87, 0.84, 0.95), "bounds": 1100.0, "alpha": POWDER_ALPHA},
    # a pistol's smaller charge
    PISTOL_SMOKE: {"duration": 6.0, "burst": (6, 9), "lifetime": (2.2, 4.0), "size": (14.0, 34.0), "grow": 3.8,
                   "cone": 14.0, "speed": (80.0, 450.0), "drag": 3.4, "gravity": 22.0, "curl": True,
                   "color": (0.86, 0.85, 0.82, 0.85), "bounds": 700.0, "alpha": POWDER_ALPHA},
    # the machine gun: a thin puff per shot (many shots a second)
    "NS_MG_MuzzleSmoke": {"duration": 3.0, "burst": (1, 2), "lifetime": (1.2, 2.6), "size": (10.0, 22.0), "grow": 3.5,
                          "cone": 20.0, "speed": (60.0, 260.0), "drag": 3.0, "gravity": 30.0, "curl": True,
                          "color": (0.8, 0.79, 0.76, 0.32), "bounds": 500.0, "alpha": POWDER_ALPHA},
    # steam of the water jacket: white, rising fast, gone quickly
    "NS_MG_Steam": {"duration": 2.5, "burst": (3, 5), "lifetime": (0.9, 2.0), "size": (8.0, 18.0), "grow": 4.0,
                    "cone": 22.0, "speed": (40.0, 140.0), "drag": 2.5, "gravity": 70.0, "curl": True,
                    "color": (0.95, 0.96, 0.97, 0.45), "bounds": 400.0, "alpha": STEAM_ALPHA},
    "NS_MG_SteamVent": {"duration": 4.5, "burst": (30, 42), "lifetime": (1.4, 3.4), "size": (14.0, 44.0), "grow": 4.0,
                        "cone": 35.0, "speed": (150.0, 600.0), "drag": 2.5, "gravity": 80.0, "curl": True,
                        "color": (0.95, 0.96, 0.97, 0.6), "bounds": 900.0, "alpha": STEAM_ALPHA},
    # the Veilleur's cloud of ash (spawned again every ASH_EVERY s around the cloud's middle, +X turned up): dark,
    # heavy, slow puffs that churn and rise a little, in place of the stretched sphere
    ASH_CLOUD: {"duration": 6.0, "burst": (3, 4), "lifetime": (2.6, 4.2), "size": (120.0, 200.0), "grow": 2.2,
                "cone": 70.0, "speed": (20.0, 110.0), "drag": 1.6, "gravity": 18.0, "curl": True,
                "color": (0.28, 0.25, 0.23, 0.9), "bounds": 900.0, "alpha": POWDER_ALPHA},
    # coral sand where a ball strikes: falls back
    "NS_Firearm_Impact": {"duration": 2.0, "burst": (6, 10), "lifetime": (0.8, 1.7), "size": (10.0, 26.0), "grow": 3.0,
                          "cone": 35.0, "speed": (100.0, 380.0), "drag": 3.0, "gravity": -120.0, "curl": False,
                          "color": (0.8, 0.74, 0.62, 0.75), "bounds": 300.0, "alpha": STEAM_ALPHA},
}
# the black powder guns and their smoke (pistols, blunderbuss and long guns had none before: only a flame)
GUN_SMOKE = {"/Game/MonkeIsland/Weapons/Musket/DA_PirateMusketRules": MUSKET_SMOKE,
             "/Game/MonkeIsland/Weapons/Rifle/DA_RifleRules": MUSKET_SMOKE,
             "/Game/MonkeIsland/Weapons/Blunderbuss/DA_BlunderbussRules": "NS_Shotgun_MuzzleSmoke",
             "/Game/MonkeIsland/Weapons/Pistol/DA_PistolRules": PISTOL_SMOKE,
             "/Game/MonkeIsland/Weapons/DoublePistol/DA_DoublePistolRules": PISTOL_SMOKE}
SMOKE_SYSTEM_RULES = ["/Game/MonkeIsland/Weapons/Musket/DA_MusketRules", "/Game/MonkeIsland/Characters/BeanFR/Data/DA_FortSoldierRules"]


class StepError(RuntimeError):
    pass


def save(asset):
    if not EAL.save_loaded_asset(asset, only_if_is_dirty=False):
        raise StepError(f"{asset.get_path_name()} not saved")


# ------------------------------------------------------------------ 1. texture
def import_texture(report):
    if not SOURCE.exists():
        raise StepError(f"{SOURCE} missing: python scripts/vfx/smoke_flipbook.py")
    task = ue.AssetImportTask()
    task.filename = str(SOURCE)
    task.destination_path = VFX
    task.destination_name = "T_Smoke_Flipbook"
    task.replace_existing = True
    task.automated = True
    task.save = False
    ue.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    texture = EAL.load_asset(TEXTURE)
    if not texture:
        raise StepError(f"{TEXTURE} did not import")
    texture.set_editor_property("srgb", True)
    texture.set_editor_property("compression_settings", ue.TextureCompressionSettings.TC_BC7)
    texture.set_editor_property("lod_group", ue.TextureGroup.TEXTUREGROUP_EFFECTS)
    save(texture)
    report["texture"] = {"path": TEXTURE, "size": [texture.blueprint_get_size_x(), texture.blueprint_get_size_y()]}
    return texture


# ------------------------------------------------------------------ 2. material
def node(material, cls, x, y, **props):
    expression = MEL.create_material_expression(material, cls, x, y)
    for name, value in props.items():
        expression.set_editor_property(name, value)
    return expression


def link(source, target, source_pin="", target_pin=""):
    if not MEL.connect_material_expressions(source, source_pin, target, target_pin):
        raise StepError(f"link {source.get_name()}.{source_pin} -> {target.get_name()}.{target_pin} refused")


def scalar(material, name, value, x, y, desc):
    return node(material, ue.MaterialExpressionScalarParameter, x, y, parameter_name=name, group="Smoke", default_value=value,
                desc=desc)


def build_material(texture, report):
    material = EAL.load_asset(MATERIAL)
    if material and MEL.get_num_material_expressions(material) == GRAPH_NODES:
        # built already: delete_all_material_expressions leaves nodes behind in this build (2026-10-11), so the graph
        # is only built once (delete M_Smoke_Flipbook's nodes by hand, or the asset, to build it again)
        report["material"] = {"path": MATERIAL, "kept": True}
        return material
    if not material:
        material = ue.AssetToolsHelpers.get_asset_tools().create_asset("M_Smoke_Flipbook", VFX, ue.Material, ue.MaterialFactoryNew())
    if not material:
        raise StepError(f"{MATERIAL} not created")
    MEL.delete_all_material_expressions(material)
    material.set_editor_property("blend_mode", ue.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("translucency_lighting_mode", ue.TranslucencyLightingMode.TLM_VOLUMETRIC_NON_DIRECTIONAL)
    material.set_editor_property("used_with_niagara_sprites", True)

    flipbook = node(material, ue.MaterialExpressionParticleSubUV, -1200, 0, texture=texture, blend=True)
    particle = node(material, ue.MaterialExpressionParticleColor, -1200, -300)
    gain = scalar(material, "LightGain", LIGHT_GAIN, -1200, -450, "Brightness of the sunlight baked in the flipbook")
    opacity = scalar(material, "Opacity", 1.0, -1200, 300, "Density of the smoke (times the particle alpha and the flipbook)")
    soft = scalar(material, "SoftFadeDistance", SOFT_FADE_CM, -500, 520, "Distance over which the smoke fades into the ground or a wall (cm)")
    near_start = scalar(material, "NearFadeStart", NEAR_FADE_START_CM, -1200, 600, "Below this distance from the camera the smoke is invisible (cm)")
    near_length = scalar(material, "NearFadeLength", NEAR_FADE_LENGTH_CM, -1200, 700, "Distance over which it comes back to full density (cm)")

    # colour: tint x baked light x gain
    lit = node(material, ue.MaterialExpressionMultiply, -800, -250)
    link(particle, lit, "RGB", "A")
    link(flipbook, lit, "RGB", "B")
    colour = node(material, ue.MaterialExpressionMultiply, -600, -300)
    link(lit, colour, "", "A")
    link(gain, colour, "", "B")

    # opacity: flipbook x particle alpha x Opacity x near-camera fade, then soft contact
    density = node(material, ue.MaterialExpressionMultiply, -800, 100)
    link(flipbook, density, "A", "A")
    link(particle, density, "A", "B")
    dense = node(material, ue.MaterialExpressionMultiply, -650, 160)
    link(density, dense, "", "A")
    link(opacity, dense, "", "B")
    depth = node(material, ue.MaterialExpressionPixelDepth, -1000, 600)
    near = node(material, ue.MaterialExpressionSubtract, -850, 620)
    link(depth, near, "", "A")
    link(near_start, near, "", "B")
    ratio = node(material, ue.MaterialExpressionDivide, -700, 640)
    link(near, ratio, "", "A")
    link(near_length, ratio, "", "B")
    near_fade = node(material, ue.MaterialExpressionSaturate, -560, 640)
    link(ratio, near_fade)
    faded = node(material, ue.MaterialExpressionMultiply, -450, 250)
    link(dense, faded, "", "A")
    link(near_fade, faded, "", "B")
    contact = node(material, ue.MaterialExpressionDepthFade, -250, 300)
    link(faded, contact, "", "Opacity")
    link(soft, contact, "", "FadeDistance")

    if not MEL.connect_material_property(colour, "", ue.MaterialProperty.MP_BASE_COLOR):
        raise StepError("base colour not connected")
    if not MEL.connect_material_property(contact, "", ue.MaterialProperty.MP_OPACITY):
        raise StepError("opacity not connected")
    MEL.recompile_material(material)
    save(material)
    report["material"] = {"path": MATERIAL, "expressions": MEL.get_num_material_expressions(material)}
    return material


# ------------------------------------------------------------------ 3. systems
def tl_get(path, prop):
    obj = ue.load_object(None, path)
    if obj is None:
        raise StepError(f"no object {path}")
    out = ue.ToolsetLibrary.get_object_properties(obj, [prop])
    return (json.loads(out, strict=False) if isinstance(out, str) else out)[prop]


def tl_put(path, **values):
    """Sets the properties; when refused (an array whose size and elements change together), arrays are resized first
    with their elements kept (resize_like), then set again"""
    obj = ue.load_object(None, path)
    if obj is None:
        raise StepError(f"no object {path}")
    for _ in range(4):
        if ue.ToolsetLibrary.set_object_properties(obj, json.dumps(values)):
            return
        for prop, target in values.items():
            if isinstance(target, (dict, list)):
                ue.ToolsetLibrary.set_object_properties(obj, json.dumps({prop: resize_like(tl_get(path, prop), target)}))
    raise StepError(f"{path}: properties refused ({', '.join(values)})")


def resize_like(current, target):
    """The current value with every array at the target's length: existing elements kept as they are, new ones taken
    from the target (the toolset refuses an array whose size and elements change in the same set)"""
    if isinstance(target, list):
        current = current if isinstance(current, list) else []
        return [resize_like(current[i], t) if i < len(current) else t for i, t in enumerate(target)]
    if isinstance(target, dict):
        current = current if isinstance(current, dict) else {}
        return {k: resize_like(current[k], v) if k in current else v for k, v in target.items()}
    return target if current is None else current


def tl_put_resized(path, enabled, prop, target):
    """Sets prop to target in passes: arrays resized first (elements kept), then the elements"""
    obj = ue.load_object(None, path)
    if obj is None:
        raise StepError(f"no object {path}")
    for _ in range(4):
        if ue.ToolsetLibrary.set_object_properties(obj, json.dumps({"bModuleEnabled": enabled, prop: target})):
            return
        ue.ToolsetLibrary.set_object_properties(obj, json.dumps({prop: resize_like(tl_get(path, prop), target)}))
    raise StepError(f"{path}: {prop} refused")


def distribution(current, low, high=None):
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


def sample(keys, t):
    for (t0, v0), (t1, v1) in zip(keys, keys[1:]):
        if t <= t1:
            return v0 + (v1 - v0) * (0.0 if t1 == t0 else (t - t0) / (t1 - t0))
    return keys[-1][1]


def rich_curve(keys):
    return {"keys": [{"interpMode": "RCIM_Linear", "tangentMode": "RCTM_Auto", "tangentWeightMode": "RCTWM_WeightedNone",
                      "time": t, "value": v, "arriveTangent": 0.0, "arriveTangentWeight": 0.0, "leaveTangent": 0.0,
                      "leaveTangentWeight": 0.0} for t, v in keys],
            "defaultValue": 3.4028234663852886e+38, "preInfinityExtrap": "RCCE_Constant", "postInfinityExtrap": "RCCE_Constant"}


def curve_distribution(current, channels, samples=16):
    """Curve over the particle's life, one key list per channel (name, keys): the curves and their sampled table"""
    value = dict(current)
    value["mode"] = "NonUniformCurve"
    value["valuesTimeRange"] = {"x": 0.0, "y": 1.0}
    value["channelCurves"] = [rich_curve(keys) for _, keys in channels]
    value["channelConstantsAndRanges"] = []
    value["values"] = [{name: sample(keys, i / (samples - 1)) for name, keys in channels} for i in range(samples)]
    return value


def configure(name, spec, report):
    emitter = f"{VFX}/{name}.{name}:Fountain_1"

    def module(n):
        return f"{emitter}.NiagaraStatelessModule_{n}_0"

    state = tl_get(emitter, "emitterState")
    state["loopBehavior"] = "Once"
    state["loopDuration"] = distribution(state["loopDuration"], spec["duration"])
    state["bLoopDelayEnabled"] = False
    state["inactiveResponse"] = "Complete"
    spawn = tl_get(emitter, "spawnInfos")[0]
    spawn["type"] = "Burst"
    spawn["spawnTime"] = 0.0
    spawn["amount"].update({"mode": "UniformRange", "min": spec["burst"][0], "max": spec["burst"][1]})
    bounds = spec["bounds"]
    tl_put(emitter, emitterState=state, spawnInfos=[spawn],
           fixedBounds={"min": {"x": -bounds * 0.4, "y": -bounds, "z": -bounds}, "max": {"x": bounds * 1.6, "y": bounds, "z": bounds * 1.5},
                        "isValid": True})
    init = module("InitializeParticle")
    r, g, b, a = spec["color"]
    color = tl_get(init, "colorDistribution")
    tint = {"r": r, "g": g, "b": b, "a": a}
    color["values"] = [tint, tint]
    color["mode"] = "NonUniformConstant"
    color["channelConstantsAndRanges"] = [r, g, b, a]
    sizes = tl_get(init, "spriteSizeDistribution")
    sizes["min"], sizes["max"] = {"x": spec["size"][0], "y": spec["size"][0]}, {"x": spec["size"][1], "y": spec["size"][1]}
    sizes["mode"], sizes["channelConstantsAndRanges"] = "UniformRange", [spec["size"][0], spec["size"][1]]
    # the flipbook is lit from above: only a slight tilt, so the light stays on top
    tl_put(init, lifetimeDistribution=distribution(tl_get(init, "lifetimeDistribution"), *spec["lifetime"]),
           colorDistribution=color, spriteSizeDistribution=sizes,
           spriteRotationDistribution=distribution(tl_get(init, "spriteRotationDistribution"), -25.0, 25.0))
    tl_put(module("ShapeLocation"), bModuleEnabled=False)
    velocity = module("AddVelocity")
    tl_put(velocity, velocityType="InCone", coneRotationType="Direction", coneAngle=spec["cone"],
           coneDirection=distribution(tl_get(velocity, "coneDirection"), [1.0, 0.0, 0.0]),
           coneVelocityDistribution=distribution(tl_get(velocity, "coneVelocityDistribution"), *spec["speed"]))
    tl_put(module("Drag"), bModuleEnabled=True, dragDistribution=distribution(tl_get(module("Drag"), "dragDistribution"), spec["drag"]))
    tl_put(module("GravityForce"), bModuleEnabled=True,
           gravityDistribution=distribution(tl_get(module("GravityForce"), "gravityDistribution"), [0.0, 0.0, spec["gravity"]]))
    tl_put(module("CurlNoiseForce"), bModuleEnabled=spec["curl"])
    # swells fast just out of the muzzle, then slower and slower
    grow = spec["grow"]
    size_keys = [(0.0, 1.0), (0.12, 1.0 + (grow - 1.0) * 0.4), (0.4, 1.0 + (grow - 1.0) * 0.75), (1.0, grow)]
    tl_put_resized(module("ScaleSpriteSize"), True, "scaleDistribution",
                   curve_distribution(tl_get(module("ScaleSpriteSize"), "scaleDistribution"), [("x", size_keys), ("y", size_keys)]))
    alpha = spec["alpha"]
    ones = [(0.0, 1.0)]
    tl_put_resized(module("ScaleColor"), True, "scaleDistribution",
                   curve_distribution(tl_get(module("ScaleColor"), "scaleDistribution"),
                                      [("r", ones), ("g", ones), ("b", ones), ("a", alpha)]))
    rate = module("SpriteRotationRate")
    tl_put(rate, bModuleEnabled=True, rotationRateDistribution=distribution(tl_get(rate, "rotationRateDistribution"), -14.0, 14.0))
    tl_put(module("SubUVAnimation"), bModuleEnabled=True, numFrames=FRAMES * FRAMES, animationMode="Linear")
    tl_put(f"{emitter}.Renderer", material={"refPath": f"{MATERIAL}.M_Smoke_Flipbook"}, subImageSize={"x": FRAMES, "y": FRAMES},
           bSubImageBlend=True, sortMode="ViewDepth")
    report.setdefault("systems", {})[name] = {"lifetime": spec["lifetime"], "size": spec["size"], "grow": grow}


def build_systems(report):
    for copy in (MUSKET_SMOKE, PISTOL_SMOKE, ASH_CLOUD):
        path = f"{VFX}/{copy}"
        if not EAL.does_asset_exist(path):
            if not EAL.duplicate_asset(f"{VFX}/NS_Shotgun_MuzzleSmoke", path):
                raise StepError(f"{path} not duplicated")
            report.setdefault("created", []).append(path)
    for name, spec in SYSTEMS.items():
        path = f"{VFX}/{name}"
        if not EAL.does_asset_exist(path):
            report.setdefault("missing", []).append(name)
            continue
        configure(name, spec, report)
        system = EAL.load_asset(path)
        save(system)


# ------------------------------------------------------------------ 4. rules
def set_rules(report):
    musket_smoke = EAL.load_asset(f"{VFX}/{MUSKET_SMOKE}")
    rules = report.setdefault("rules", {})
    for path, smoke_name in GUN_SMOKE.items():
        asset = EAL.load_asset(path)
        smoke = EAL.load_asset(f"{VFX}/{smoke_name}")
        if not asset or not smoke:
            rules[path] = "missing"
            continue
        # one smoke per gun: any other muzzle smoke in the list is replaced, the flame and sparks are kept
        systems = [s for s in asset.get_editor_property("muzzle_blast_systems") if s and not s.get_name().endswith("MuzzleSmoke")]
        systems.append(smoke)
        asset.set_editor_property("muzzle_blast_systems", systems)
        save(asset)
        rules[path] = [s.get_name() for s in systems]
    for path in SMOKE_SYSTEM_RULES:
        asset = EAL.load_asset(path)
        if not asset:
            rules[path] = "missing"
            continue
        try:
            asset.set_editor_property("smoke_system", musket_smoke)
        except Exception as error:  # noqa: BLE001 - the property comes with the C++ of 2026-10-11 (next build)
            rules[path] = f"SmokeSystem not in this build yet ({str(error)[:80]})"
            continue
        save(asset)
        rules[path] = "SmokeSystem = " + MUSKET_SMOKE


ASH_EVERY = 0.6


def set_ash_cloud(report):
    """The Veilleur's cloud: the ash puffs only (no stretched sphere), at their own size, a little more often"""
    rules = EAL.load_asset(TIKI_RULES)
    cloud = EAL.load_asset(f"{VFX}/{ASH_CLOUD}")
    if not rules or not cloud:
        report["ash"] = "missing"
        return
    watcher = rules.get_editor_property("watcher")
    ash = watcher.get_editor_property("ash")
    before = {"cloud_mesh": str(ash.get_editor_property("cloud_mesh")), "cloud_effect": str(ash.get_editor_property("cloud_effect")),
              "scale": ash.get_editor_property("cloud_effect_scale"), "every": ash.get_editor_property("cloud_effect_seconds")}
    ash.set_editor_property("cloud_mesh", None)
    ash.set_editor_property("cloud_effect", cloud)
    ash.set_editor_property("cloud_effect_scale", 1.0)
    ash.set_editor_property("cloud_effect_seconds", ASH_EVERY)
    watcher.set_editor_property("ash", ash)
    rules.set_editor_property("watcher", watcher)
    save(rules)
    report["ash"] = {"before": before, "after": {"cloud_mesh": None, "cloud_effect": ASH_CLOUD, "scale": 1.0, "every": ASH_EVERY}}


def reload_systems(report):
    """The lightweight emitters keep the data they were built with until they are loaded again (2026-10-11: the new
    sub-image size was ignored in a game, the whole flipbook drawn on every sprite): the saved systems are reloaded"""
    packages = []
    for name in SYSTEMS:
        system = EAL.load_asset(f"{VFX}/{name}")
        if system:
            packages.append(system.get_outermost())
    result = ue.EditorLoadingAndSavingUtils.reload_packages(packages, ue.ReloadPackagesInteractionMode.ASSUME_POSITIVE)
    report["reloaded"] = [len(packages), str(result)[:200]]


def main():
    report = {"issues": []}
    try:
        texture = import_texture(report)
        build_material(texture, report)
        build_systems(report)
        set_rules(report)
        set_ash_cloud(report)
        reload_systems(report)
    finally:
        OUT.parent.mkdir(parents=True, exist_ok=True)
        OUT.write_text(json.dumps(report, indent=1, default=str), encoding="utf-8")
    ue.log(f"SMOKE_FLIPBOOK systems={len(report.get('systems', {}))} rules={report.get('rules')}")


main()
