"""T-203 (lot L5): assets of the weather that turns when a temple artifact is seized.

Run as the action "stormartifact" of the lead's editor session (scripts/editor/lead_session_t186.py, through
tmp/run_action_locked.sh): editor open, editor lock held, no game running. Idempotent and short (a few seconds; the rain
material compiles its shaders in the background afterwards). Never touches a level.

Builds in /Game/MonkeIsland/Environment/Weather/ (cooked whole: DefaultGame.ini DirectoriesToAlwaysCook):
  1. MPC_Weather: scalar RainOpacity (0-1, default 1 for the previews), the fade of the rain written by
     UWeatherPresentationComponent every frame it rains.
  2. M_Rain_Streak: lit translucent sprite material (volumetric per vertex), a thin soft streak drawn from the UVs,
     opacity = streak x particle alpha x MPC_Weather.RainOpacity x Opacity; every parameter described (group Rain).
  3. NS_Rain: copy of the engine stateless template FountainLightweight (GPU, no script to compile): a box of 32 x 32 m,
     6 m high, drops falling 10 m/s along the local -Z (the game tilts the component downwind), 1.3-1.7 s, streaks of
     1.6 x 80 cm aligned to the local Z, 2600 drops a second (about 4 000 alive).
  4. DA_WeatherRules (needs UWeatherRules, i.e. the lead's editor build with T-203): artifacts of
     DA_TempleReliquaryRules, rain, parameters, the sounds prepared by T-234 (SW_Exp_Rain, SW_Exp_ArtifactCurse,
     SW_Exp_Thunder in /Game/MonkeIsland/Audio/Expedition, imported by the action "expeditionaudio"; no wind loop: the
     T-234 subsystem already plays SW_Exp_WindStrong in a strong wind), DA_WindRules for the maps without wind.
     Before the build: reported under "pending".
Report: monke_island/Saved/T203/storm-artifact-result.json (discovery of the Niagara modules, values read back).
"""
from __future__ import annotations

import datetime as dt
import json
import traceback
from pathlib import Path

import unreal as ue

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "monke_island/Saved/T203"
FOLDER = "/Game/MonkeIsland/Environment/Weather"
MPC = f"{FOLDER}/MPC_Weather"
MATERIAL = f"{FOLDER}/M_Rain_Streak"
RAIN = f"{FOLDER}/NS_Rain"
RULES = f"{FOLDER}/DA_WeatherRules"
TEMPLATE = "/Niagara/DefaultAssets/Templates/Systems/FountainLightweight"
TEMPLE_RULES = "/Game/MonkeIsland/Environment/TempleReliquary/Data/DA_TempleReliquaryRules"
WIND_RULES = "/Game/MonkeIsland/Environment/Storm/DA_WindRules"
SOUNDS = {
    "RainSound": "/Game/MonkeIsland/Audio/Expedition/SW_Exp_Rain",
    "SeizeSound": "/Game/MonkeIsland/Audio/Expedition/SW_Exp_ArtifactCurse",
    "ThunderSound": "/Game/MonkeIsland/Audio/Expedition/SW_Exp_Thunder",
}
# no wind loop of its own: UExpeditionAudioSubsystem (T-234) already plays SW_Exp_WindStrong while the wind is strong,
# which the turned weather keeps it; a second wind loop would double it
NO_WIND_LOOP = ""
# the drops: lit by the scene (day, night, storm), a cool grey
RAIN_COLOR = (0.62, 0.68, 0.74)
EAL = ue.EditorAssetLibrary
MEL = ue.MaterialEditingLibrary
TOOLS = ue.AssetToolsHelpers.get_asset_tools()


def object_path(path):
    """Object path of an asset path"""
    name = path.rsplit("/", 1)[-1]
    return path if "." in name else f"{path}.{name}"


def load(path):
    return EAL.load_asset(path) if EAL.does_asset_exist(path) else None


def save(asset):
    if not EAL.save_loaded_asset(asset, False):
        raise RuntimeError(f"could not save {asset.get_path_name()}")


def get(obj, names):
    """Properties of an object as JSON (ToolsetLibrary, the MCP ObjectTools backend: works on non-Blueprint
    properties), keyed by the names asked for whatever the case of the JSON keys"""
    raw = json.loads(ue.ToolsetLibrary.get_object_properties(obj, names), strict=False)
    lowered = {key.lower(): value for key, value in raw.items()} if isinstance(raw, dict) else {}
    return {name: lowered.get(name.lower()) for name in names}


def put(obj, **values):
    """Sets properties through ToolsetLibrary; a refusal is retried key by key to name the keys refused"""
    if ue.ToolsetLibrary.set_object_properties(obj, json.dumps(values)):
        return
    refused = [key for key, value in values.items() if not ue.ToolsetLibrary.set_object_properties(obj, json.dumps({key: value}))]
    if refused:
        raise RuntimeError(f"set_object_properties refused on {obj.get_path_name()}: {refused}")


def distribution(current, low, high=None):
    """A float or vector distribution set to a constant (high None) or a uniform range (as scripts/vfx/build_blood_assets_ue.py)"""
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


def build_mpc(report):
    mpc = load(MPC)
    if not mpc:
        mpc = TOOLS.create_asset("MPC_Weather", FOLDER, ue.MaterialParameterCollection, ue.MaterialParameterCollectionFactoryNew())
        report["created"].append(MPC)
    scalars = get(mpc, ["ScalarParameters"]).get("ScalarParameters") or []
    if not any(str(p.get("parameterName", p.get("ParameterName", ""))) == "RainOpacity" for p in scalars):
        # 1 by default: the material previews show the drops; in play the game writes it every frame it rains.
        # No id: a new FCollectionScalarParameter makes its own GUID (the existing ones keep theirs)
        scalars.append({"parameterName": "RainOpacity", "defaultValue": 1.0})
        put(mpc, ScalarParameters=scalars)
        save(mpc)
    report["mpc"] = get(mpc, ["ScalarParameters"])
    return mpc


class Graph:
    """Expressions of a material, linked"""

    def __init__(self, material):
        self.material = material

    def node(self, cls, x, y, **props):
        node = MEL.create_material_expression(self.material, cls, x, y)
        for key, value in props.items():
            node.set_editor_property(key, value)
        return node

    def param(self, cls, name, desc, x, y, priority, **props):
        return self.node(cls, x, y, parameter_name=name, group="Rain", desc=desc, sort_priority=priority, **props)

    def link(self, src, dst, pin="", out=""):
        if not MEL.connect_material_expressions(src, out, dst, pin):
            raise RuntimeError(f"link {src.get_name()}.{out} -> {dst.get_name()}.{pin} failed")

    def output(self, src, prop, out=""):
        if not MEL.connect_material_property(src, out, prop):
            raise RuntimeError(f"output {src.get_name()} -> {prop} failed")


def build_material(mpc, report):
    material = load(MATERIAL)
    if material:
        report["kept"].append(MATERIAL)
        return material
    material = TOOLS.create_asset("M_Rain_Streak", FOLDER, ue.Material, ue.MaterialFactoryNew())
    material.set_editor_property("blend_mode", ue.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", ue.MaterialShadingModel.MSM_DEFAULT_LIT)
    material.set_editor_property("translucency_lighting_mode", ue.TranslucencyLightingMode.TLM_VOLUMETRIC_PER_VERTEX_NON_DIRECTIONAL)
    material.set_editor_property("used_with_niagara_sprites", True)
    g = Graph(material)
    uv = g.node(ue.MaterialExpressionTextureCoordinate, -1500, 0)
    u = g.node(ue.MaterialExpressionComponentMask, -1300, -100, r=True, g=False, b=False, a=False)
    v = g.node(ue.MaterialExpressionComponentMask, -1300, 150, r=False, g=True, b=False, a=False)
    g.link(uv, u)
    g.link(uv, v)
    # across the streak: 1 on its axis, 0 on its sides, sharpened
    centred = g.node(ue.MaterialExpressionMultiply, -1150, -100, const_b=2.0)
    g.link(u, centred, "A")
    offset = g.node(ue.MaterialExpressionSubtract, -1000, -100, const_b=1.0)
    g.link(centred, offset, "A")
    distance = g.node(ue.MaterialExpressionAbs, -850, -100)
    g.link(offset, distance)
    across = g.node(ue.MaterialExpressionOneMinus, -700, -100)
    g.link(distance, across)
    sharp = g.node(ue.MaterialExpressionPower, -550, -100, const_exponent=3.0)
    g.link(across, sharp, "Base")
    # along the streak: sin(pi v), 0 at both ends
    along = g.node(ue.MaterialExpressionSine, -850, 150, period=2.0)
    g.link(v, along)
    streak = g.node(ue.MaterialExpressionMultiply, -400, 0)
    g.link(sharp, streak, "A")
    g.link(along, streak, "B")
    particle = g.node(ue.MaterialExpressionParticleColor, -700, 400)
    fade = g.node(ue.MaterialExpressionCollectionParameter, -700, 600)
    fade.set_editor_property("collection", mpc)
    fade.set_editor_property("parameter_name", "RainOpacity")
    opacity_scale = g.param(ue.MaterialExpressionScalarParameter, "Opacity",
        "Opacity of a drop at full rain; MPC_Weather.RainOpacity (written by the game) fades the whole rain", -400, 700, 0,
        default_value=0.55)
    alpha = g.node(ue.MaterialExpressionMultiply, -250, 100)
    g.link(streak, alpha, "A")
    g.link(particle, alpha, "B", "A")
    faded = g.node(ue.MaterialExpressionMultiply, -100, 300)
    g.link(alpha, faded, "A")
    g.link(fade, faded, "B")
    final = g.node(ue.MaterialExpressionMultiply, 50, 400)
    g.link(faded, final, "A")
    g.link(opacity_scale, final, "B")
    color = g.param(ue.MaterialExpressionVectorParameter, "RainColor",
        "Colour of the drops (linear), lit by the scene: grey-blue water catching the sky", -700, -450, 1,
        default_value=ue.LinearColor(RAIN_COLOR[0], RAIN_COLOR[1], RAIN_COLOR[2], 1.0))
    base = g.node(ue.MaterialExpressionMultiply, -400, -400)
    g.link(color, base, "A")
    g.link(particle, base, "B")
    glow = g.param(ue.MaterialExpressionScalarParameter, "NightReadability",
        "Share of the colour also emitted, so the rain still reads by night and in the storm (0 = none)", -400, -200, 2,
        default_value=0.04)
    emit = g.node(ue.MaterialExpressionMultiply, -150, -300)
    g.link(base, emit, "A")
    g.link(glow, emit, "B")
    rough = g.param(ue.MaterialExpressionScalarParameter, "Roughness", "Roughness of the drops: wet and glossy", -150, -100, 3,
        default_value=0.1)
    g.output(base, ue.MaterialProperty.MP_BASE_COLOR)
    g.output(emit, ue.MaterialProperty.MP_EMISSIVE_COLOR)
    g.output(rough, ue.MaterialProperty.MP_ROUGHNESS)
    g.output(final, ue.MaterialProperty.MP_OPACITY)
    MEL.recompile_material(material)
    save(material)
    report["created"].append(MATERIAL)
    return material


STATELESS_MODULES = ["InitializeParticle", "ShapeLocation", "AddVelocity", "Drag", "GravityForce", "CurlNoiseForce",
                     "AccelerationForce", "ScaleSpriteSizeBySpeed", "RotateAroundPoint", "SpriteFacingAndAlignment",
                     "SolveVelocitiesAndForces", "ScaleColor", "ScaleSpriteSize"]


def ref_path(entry):
    if isinstance(entry, dict):
        return entry.get("refPath") or entry.get("RefPath")
    return entry if isinstance(entry, str) else None


def modules_of(emitter):
    """Module objects of a stateless emitter by short class name (InitializeParticle, ShapeLocation...): from its Modules
    list, else by the subobject names the blood systems use (<emitter>.NiagaraStatelessModule_<Name>_0)"""
    found = {}
    for entry in get(emitter, ["Modules"]).get("Modules") or []:
        path = ref_path(entry)
        module = ue.load_object(None, path) if path else None
        if module:
            found[module.get_class().get_name().replace("NiagaraStatelessModule_", "")] = module
    for name in STATELESS_MODULES:
        if name not in found:
            module = ue.find_object(None, f"{emitter.get_path_name()}.NiagaraStatelessModule_{name}_0")
            if module:
                found[name] = module
    return found


def renderers_of(emitter):
    found = []
    for entry in get(emitter, ["RendererProperties"]).get("RendererProperties") or []:
        path = ref_path(entry)
        renderer = ue.load_object(None, path) if path else None
        if renderer:
            found.append(renderer)
    if not found:
        renderer = ue.find_object(None, f"{emitter.get_path_name()}.Renderer")
        if renderer:
            found.append(renderer)
    return found


def build_rain(material, report):
    if not load(RAIN):
        if not EAL.duplicate_asset(TEMPLATE, RAIN):
            raise RuntimeError(f"could not duplicate {TEMPLATE}")
        report["created"].append(RAIN)
    system = load(RAIN)
    emitter = None
    for name in ("Fountain_1", "Fountain"):
        emitter = ue.load_object(None, f"{object_path(RAIN)}:{name}")
        if emitter:
            break
    if not emitter:
        raise RuntimeError("emitter of NS_Rain not found (expected :Fountain_1 as in the blood systems)")
    modules = modules_of(emitter)
    renderers = renderers_of(emitter)
    report["rain_discovery"] = {"emitter": emitter.get_path_name(), "modules": sorted(modules),
                                "renderers": [r.get_class().get_name() for r in renderers]}

    # endless, 2600 drops a second, bounds of the local volume and the fall below it
    state = get(emitter, ["EmitterState"])["EmitterState"]
    state["loopBehavior"] = "Infinite"
    state["loopDuration"] = distribution(state["loopDuration"], 1.0)
    state["bLoopDelayEnabled"] = False
    state["inactiveResponse"] = "Complete"
    spawn = get(emitter, ["SpawnInfos"])["SpawnInfos"][0]
    spawn["type"] = "Rate"
    spawn["rate"] = distribution(spawn["rate"], 2600.0)
    put(emitter, EmitterState=state, SpawnInfos=[spawn],
        FixedBounds={"min": {"x": -2600.0, "y": -2600.0, "z": -2600.0}, "max": {"x": 2600.0, "y": 2600.0, "z": 1000.0}, "isValid": True})

    init = modules["InitializeParticle"]
    color = get(init, ["ColorDistribution"])["ColorDistribution"]
    color["values"] = [{"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0}, {"r": 1.0, "g": 1.0, "b": 1.0, "a": 1.0}]
    color["mode"] = "NonUniformConstant"
    color["channelConstantsAndRanges"] = [1.0, 1.0, 1.0, 1.0]
    # InitializeParticle is always on (no bModuleEnabled to set, as in scripts/vfx/build_blood_assets_ue.py).
    # The setter refuses a distribution whose channel array changes length (the template's: lifetime 2 values, sprite
    # size 2, box 3, velocity 3, alignment 3): constant vectors keep them; the lifetime range gives the variety.
    put(init,
        LifetimeDistribution=distribution(get(init, ["LifetimeDistribution"])["LifetimeDistribution"], 1.3, 1.7),
        ColorDistribution=color,
        SpriteSizeDistribution=distribution(get(init, ["SpriteSizeDistribution"])["SpriteSizeDistribution"], [1.6, 80.0]))

    shape = modules["ShapeLocation"]
    put(shape, bModuleEnabled=True, ShapePrimitive="Box",
        BoxSize=distribution(get(shape, ["BoxSize"])["BoxSize"], [3200.0, 3200.0, 600.0]))

    velocity = modules["AddVelocity"]
    put(velocity, bModuleEnabled=True, VelocityType="Linear", CoordinateSpace="Local",
        LinearVelocityDistribution=distribution(get(velocity, ["LinearVelocityDistribution"])["LinearVelocityDistribution"],
                                                [0.0, 0.0, -1000.0]))
    # a raindrop falls at its terminal speed: no gravity, no drag, no curl
    for name in ("Drag", "GravityForce", "CurlNoiseForce", "AccelerationForce", "ScaleSpriteSizeBySpeed", "RotateAroundPoint"):
        if name in modules:
            put(modules[name], bModuleEnabled=False)

    # streaks along the local Z (the game tilts the component downwind), facing the camera
    aligned = "SpriteFacingAndAlignment" in modules
    if aligned:
        facing = modules["SpriteFacingAndAlignment"]
        put(facing, bModuleEnabled=True, bSpriteFacingEnabled=False, bSpriteAlignmentEnabled=True,
            SpriteAlignment=distribution(get(facing, ["SpriteAlignment"])["SpriteAlignment"], [0.0, 0.0, 1.0]))
    for renderer in renderers:
        if "Sprite" in renderer.get_class().get_name():
            put(renderer, Material={"refPath": object_path(MATERIAL)},
                Alignment="CustomAlignment" if aligned else "VelocityAligned", FacingMode="FaceCamera")
    save(system)
    report["rain"] = {
        "emitter": get(emitter, ["EmitterState", "SpawnInfos", "FixedBounds"]),
        "initialize": get(init, ["LifetimeDistribution", "SpriteSizeDistribution"]),
        "shape": get(shape, ["ShapePrimitive", "BoxSize"]),
        "velocity": get(velocity, ["VelocityType", "CoordinateSpace", "LinearVelocityDistribution"]),
        "renderers": [get(r, ["Material", "Alignment", "FacingMode"]) for r in renderers if "Sprite" in r.get_class().get_name()],
    }
    return system


def build_rules(report):
    if not hasattr(ue, "WeatherRules"):
        report["pending"].append("DA_WeatherRules: UWeatherRules is not in this editor build (lead's build with T-203), "
                                 "run the action stormartifact again after it")
        return None
    rules = load(RULES)
    if not rules:
        factory = ue.DataAssetFactory()
        factory.set_editor_property("data_asset_class", ue.WeatherRules)
        rules = TOOLS.create_asset("DA_WeatherRules", FOLDER, ue.WeatherRules, factory)
        report["created"].append(RULES)
    # the six legendary artifacts of the temple (the pools of the temples present are added at run time)
    temple = load(TEMPLE_RULES)
    pool = get(temple, ["ArtifactPool"]).get("ArtifactPool") if temple else []
    artifacts = [(entry.get("refPath") if isinstance(entry, dict) else entry) for entry in pool or []]
    missing_sounds = [path for path in SOUNDS.values() if not EAL.does_asset_exist(path)]
    put(rules,
        ArtifactClasses=[a for a in artifacts if a],
        RainSystem=object_path(RAIN),
        MaterialParameters=object_path(MPC),
        WindRulesForMapsWithoutWind=object_path(WIND_RULES),
        StormWindSound=NO_WIND_LOOP,
        **{key: object_path(path) for key, path in SOUNDS.items()})
    save(rules)
    report["rules"] = get(rules, ["ArtifactClasses", "TransitionSeconds", "WindStrength", "SkyStormFactor", "RainSystem",
                                  "MaterialParameters", "WindRulesForMapsWithoutWind", "StormWindSound", *SOUNDS])
    if missing_sounds:
        report["pending"].append("sounds not imported yet (action expeditionaudio, T-234): " + ", ".join(missing_sounds)
                                 + " - the paths are set, they play once imported")
    return rules


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    report = {"task": "T-203", "started_at": dt.datetime.now(dt.timezone.utc).isoformat(), "created": [], "kept": [],
              "pending": [], "success": False}
    try:
        EAL.make_directory(FOLDER)
        mpc = build_mpc(report)
        material = build_material(mpc, report)
        build_rain(material, report)
        build_rules(report)
        report["success"] = True
    except Exception:
        report["error"] = traceback.format_exc()
        ue.log_error(report["error"])
    report["finished_at"] = dt.datetime.now(dt.timezone.utc).isoformat()
    (OUT / "storm-artifact-result.json").write_text(json.dumps(report, indent=2, ensure_ascii=False), encoding="utf-8")
    if not report["success"]:
        raise RuntimeError("stormartifact failed, see monke_island/Saved/T203/storm-artifact-result.json")


main()
