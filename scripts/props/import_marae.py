"""Marae des Perles, second step (after the MCP import of the textures and meshes, T-075): materials, MIs, mesh setup and
the blueprints of the torch and of the three valuables.

Editor commandlet (editor closed):
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
Reads art/export/Props/Marae/marae_materials.json (scripts/props/marae_textures.py). Idempotent: rebuilds the material
graphs, overwrites the MI parameters and the blueprint defaults.
"""
import json
import os

import unreal

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", ".."))
DEST = "/Game/MonkeIsland/Environment/Marae"
MAT = f"{DEST}/Materials"
TEX = f"{DEST}/Textures"
MESH = f"{DEST}/Meshes"
BP = f"{DEST}/Blueprints"
SPEC = json.load(open(os.path.join(ROOT, "art", "export", "Props", "Marae", "marae_materials.json"), encoding="utf-8"))

mel = unreal.MaterialEditingLibrary
eal = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
MP = unreal.MaterialProperty
LOG = "[marae]"


def log(msg):
    unreal.log_warning(f"{LOG} {msg}")


def load(path):
    return unreal.load_asset(path) if eal.does_asset_exist(path) else None


def material(name):
    m = load(f"{MAT}/{name}")
    if not m:
        m = tools.create_asset(name, MAT, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(m)
    return m


class Graph:
    def __init__(self, mat):
        self.m = mat
        self.x = -1200

    def node(self, cls, **props):
        e = mel.create_material_expression(self.m, cls, self.x, 0)
        self.x += 150
        for k, v in props.items():
            e.set_editor_property(k, v)
        return e

    def link(self, a, b, pin="", out=""):
        if not mel.connect_material_expressions(a, out, b, pin):
            raise RuntimeError(f"link {a.get_name()} -> {b.get_name()}.{pin}")

    def out(self, a, prop, out=""):
        if not mel.connect_material_property(a, out, prop):
            raise RuntimeError(f"output {a.get_name()} -> {prop}")

    def const(self, v):
        return self.node(unreal.MaterialExpressionConstant, r=v)

    def color(self, r, g, b):
        return self.node(unreal.MaterialExpressionConstant3Vector, constant=unreal.LinearColor(r, g, b, 1))

    def finish(self):
        mel.layout_material_expressions(self.m)
        mel.recompile_material(self.m)
        eal.save_loaded_asset(self.m)
        return self.m


def tex(name):
    return load(f"{TEX}/{name}")


def build_surface():
    g = Graph(material("M_Marae_Surface"))
    uv = g.node(unreal.MaterialExpressionMultiply)
    g.link(g.node(unreal.MaterialExpressionTextureCoordinate), uv, "A")
    til = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Tiling", default_value=1.0)
    g.link(til, uv, "B")
    samples = {}
    for pname, tname, sampler in (("BaseColor", "T_Marae_Basalt_D", unreal.MaterialSamplerType.SAMPLERTYPE_COLOR),
                                  ("Normal", "T_Marae_Basalt_N", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL),
                                  ("ORM", "T_Marae_Basalt_ORM", unreal.MaterialSamplerType.SAMPLERTYPE_MASKS)):
        s = g.node(unreal.MaterialExpressionTextureSampleParameter2D, parameter_name=pname, texture=tex(tname),
                   sampler_type=sampler)
        g.link(uv, s, "UVs")
        samples[pname] = s
    g.out(samples["BaseColor"], MP.MP_BASE_COLOR, "RGB")
    g.out(samples["Normal"], MP.MP_NORMAL, "RGB")
    g.out(samples["ORM"], MP.MP_AMBIENT_OCCLUSION, "R")
    g.out(samples["ORM"], MP.MP_ROUGHNESS, "G")
    g.out(samples["ORM"], MP.MP_METALLIC, "B")
    return g.finish()


def build_gold():
    g = Graph(material("M_Marae_Gold"))
    g.out(g.color(1.0, 0.71, 0.29), MP.MP_BASE_COLOR)
    g.out(g.const(1.0), MP.MP_METALLIC)
    rough = g.node(unreal.MaterialExpressionLinearInterpolate, const_a=0.12, const_b=0.28)
    g.link(g.node(unreal.MaterialExpressionNoise, scale=0.6), rough, "Alpha")
    g.out(rough, MP.MP_ROUGHNESS)
    return g.finish()


def build_pearl():
    # Tahitian black pearl: dark, glossy, peacock green facing the eye, violet toward the rim
    g = Graph(material("M_Marae_BlackPearl"))
    col = g.node(unreal.MaterialExpressionLinearInterpolate)
    g.link(g.color(0.02, 0.075, 0.06), col, "A")
    g.link(g.color(0.15, 0.05, 0.19), col, "B")
    g.link(g.node(unreal.MaterialExpressionFresnel, exponent=2.2), col, "Alpha")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.const(0.12), MP.MP_ROUGHNESS)
    g.out(g.const(0.9), MP.MP_SPECULAR)
    return g.finish()


def build_cowrie():
    g = Graph(material("M_Marae_Cowrie"))
    col = g.node(unreal.MaterialExpressionLinearInterpolate)
    g.link(g.color(0.45, 0.30, 0.18), col, "A")
    g.link(g.color(0.96, 0.92, 0.84), col, "B")
    g.link(g.node(unreal.MaterialExpressionNoise, scale=1.5), col, "Alpha")
    g.out(col, MP.MP_BASE_COLOR)
    g.out(g.const(0.18), MP.MP_ROUGHNESS)
    return g.finish()


def flicker(g, periods, depth, base):
    """base + depth * sin(t / p0) * sin(t / p1) (+ a faster ripple): an irregular flame flicker"""
    t = g.node(unreal.MaterialExpressionTime)
    s = []
    for p in periods:
        n = g.node(unreal.MaterialExpressionSine, period=p)
        g.link(t, n)
        s.append(n)
    prod = g.node(unreal.MaterialExpressionMultiply)
    g.link(s[0], prod, "A")
    g.link(s[1], prod, "B")
    amp = g.node(unreal.MaterialExpressionMultiply, const_b=depth)
    g.link(prod, amp, "A")
    total = amp
    if len(s) > 2:
        ripple = g.node(unreal.MaterialExpressionMultiply, const_b=depth * 0.3)
        g.link(s[2], ripple, "A")
        total = g.node(unreal.MaterialExpressionAdd)
        g.link(amp, total, "A")
        g.link(ripple, total, "B")
    out = g.node(unreal.MaterialExpressionAdd, const_b=base)
    g.link(total, out, "A")
    return out, t, prod


def build_flame():
    m = material("M_Marae_Flame")
    m.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    m.set_editor_property("blend_mode", unreal.BlendMode.BLEND_ADDITIVE)
    m.set_editor_property("two_sided", True)
    g = Graph(m)
    color = g.node(unreal.MaterialExpressionVectorParameter, parameter_name="Color",
                   default_value=unreal.LinearColor(1.0, 0.33, 0.04, 1))
    inten = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Intensity", default_value=8.0)
    ci = g.node(unreal.MaterialExpressionMultiply)
    g.link(color, ci, "A")
    g.link(inten, ci, "B")
    flick, t, prod = flicker(g, (0.47, 0.29, 0.11), 0.2, 0.8)
    em = g.node(unreal.MaterialExpressionMultiply)
    g.link(ci, em, "A")
    g.link(flick, em, "B")
    g.out(em, MP.MP_EMISSIVE_COLOR)
    # the flame sways a little and stretches with the flicker (cm)
    sway = g.node(unreal.MaterialExpressionScalarParameter, parameter_name="Sway", default_value=1.2)
    parts = []
    for period in (0.71, 0.53):
        s = g.node(unreal.MaterialExpressionSine, period=period)
        g.link(t, s)
        m2 = g.node(unreal.MaterialExpressionMultiply)
        g.link(s, m2, "A")
        g.link(sway, m2, "B")
        parts.append(m2)
    up = g.node(unreal.MaterialExpressionMultiply, const_b=2.0)
    g.link(prod, up, "A")
    xy = g.node(unreal.MaterialExpressionAppendVector)
    g.link(parts[0], xy, "A")
    g.link(parts[1], xy, "B")
    xyz = g.node(unreal.MaterialExpressionAppendVector)
    g.link(xy, xyz, "A")
    g.link(up, xyz, "B")
    g.out(xyz, MP.MP_WORLD_POSITION_OFFSET)
    return g.finish()


def build_light_function():
    m = material("M_Marae_TorchFlicker")
    m.set_editor_property("material_domain", unreal.MaterialDomain.MD_LIGHT_FUNCTION)
    g = Graph(m)
    flick, _, _ = flicker(g, (0.43, 0.27, 0.11), 0.22, 0.8)
    g.out(flick, MP.MP_EMISSIVE_COLOR)
    return g.finish()


def mi(name, parent, scalars=None, vectors=None, textures=None):
    inst = load(f"{MAT}/{name}")
    if not inst:
        inst = tools.create_asset(name, MAT, unreal.MaterialInstanceConstant, unreal.MaterialInstanceConstantFactoryNew())
    mel.set_material_instance_parent(inst, parent)
    for k, v in (scalars or {}).items():
        mel.set_material_instance_scalar_parameter_value(inst, k, v)
    for k, v in (vectors or {}).items():
        mel.set_material_instance_vector_parameter_value(inst, k, v)
    for k, v in (textures or {}).items():
        mel.set_material_instance_texture_parameter_value(inst, k, v)
    mel.update_material_instance(inst)
    eal.save_loaded_asset(inst)
    return inst


def setup_mesh(name, slots, complex_collision=False, nanite=False):
    sm = load(f"{MESH}/{name}")
    if not sm:
        log(f"{name} missing")
        return None
    for slot, mat in slots.items():
        index = sm.get_material_index(slot)
        if index < 0 or not mat:
            log(f"{name}: slot {slot} not found or no material")
            continue
        sm.set_material(index, mat)
    # FBX sockets of Blender come in with a scale of 100 (memory: ue-mcp-import-pitfalls)
    for socket_name in SPEC["parts"].get(name, {}).get("sockets", {}):
        socket = sm.find_socket(socket_name)
        if socket:
            socket.set_editor_property("relative_scale", unreal.Vector(1, 1, 1))
    if complex_collision:
        unreal.EditorStaticMeshLibrary.remove_collisions(sm)
        body = sm.get_editor_property("body_setup")
        body.set_editor_property("collision_trace_flag", unreal.CollisionTraceFlag.CTF_USE_COMPLEX_AS_SIMPLE)
    if nanite:
        settings = sm.get_editor_property("nanite_settings")
        settings.set_editor_property("enabled", True)
        sm.set_editor_property("nanite_settings", settings)
    eal.save_loaded_asset(sm)
    return sm


# ---------------------------------------------------------------- blueprints

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
sdl = unreal.SubobjectDataBlueprintFunctionLibrary


def blueprint(name, parent):
    bp = load(f"{BP}/{name}")
    if bp:
        return bp, False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    return tools.create_asset(name, BP, unreal.Blueprint, factory), True


def component(bp, cls, name):
    """Component template of a blueprint, added under the root if missing"""
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    for h in handles:
        obj = sdl.get_object(sdl.get_data(h))
        if obj and obj.get_name().startswith(name):
            return obj
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=cls, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not str(fail) in ("", "None"):
        log(f"{bp.get_name()}: {name}: {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return sdl.get_object(sdl.get_data(handle))


def build_torch(torch_mesh, light_function):
    bp, _ = blueprint("BP_Marae_Torch", unreal.Actor)
    mesh = component(bp, unreal.StaticMeshComponent, "Torch")
    mesh.set_editor_property("static_mesh", torch_mesh)
    light = component(bp, unreal.PointLightComponent, "FlameLight")
    light.set_editor_property("relative_location", unreal.Vector(0, 0, 152))
    light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
    light.set_editor_property("intensity", 1.5)
    light.set_editor_property("light_color", unreal.Color(r=255, g=158, b=77, a=255))
    light.set_editor_property("attenuation_radius", 550.0)
    light.set_editor_property("source_radius", 4.0)
    light.set_editor_property("cast_shadows", False)
    light.set_editor_property("light_function_material", light_function)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    eal.save_loaded_asset(bp)
    return bp


def build_valuable(name, mesh, mass, value, glow=None):
    bp, _ = blueprint(name, unreal.PhysicsPropActor)
    if glow:
        light = component(bp, unreal.PointLightComponent, "Glow")
        light.set_editor_property("relative_location", unreal.Vector(0, 0, 30))
        light.set_editor_property("intensity_units", unreal.LightUnits.CANDELAS)
        light.set_editor_property("intensity", glow[0])
        light.set_editor_property("light_color", glow[1])
        light.set_editor_property("attenuation_radius", 120.0)
        light.set_editor_property("cast_shadows", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # class defaults after the compile (a compile rebuilds the default object): inherited native components
    cdo = unreal.get_default_object(bp.generated_class())
    cdo.set_editor_property("mass_kg", mass)
    cdo.get_editor_property("mesh").set_editor_property("static_mesh", mesh)
    cdo.get_editor_property("valuable").set_editor_property("base_value", value)
    cdo.modify()
    eal.save_loaded_asset(bp, False)
    log(f"{name}: mesh {cdo.get_editor_property('mesh').get_editor_property('static_mesh')}, "
        f"{cdo.get_editor_property('mass_kg')} kg, {cdo.get_editor_property('valuable').get_editor_property('base_value')} $")
    return bp


def main():
    surface = build_surface()
    gold, pearl, cowrie = build_gold(), build_pearl(), build_cowrie()
    flame = build_flame()
    flicker_lf = build_light_function()
    log("materials rebuilt")

    mis = {}
    for slot, entry in SPEC["materials"].items():
        name = "MI_" + slot[2:]
        t = entry["textures"]
        mis[slot] = mi(name, surface, scalars={"Tiling": entry["tiling"]},
                       textures={"BaseColor": tex(t["D"][:-4]), "Normal": tex(t["N"][:-4]), "ORM": tex(t["ORM"][:-4])})
    core = mi("MI_Marae_FlameCore", flame, scalars={"Intensity": 14.0},
              vectors={"Color": unreal.LinearColor(1.0, 0.75, 0.35, 1)})
    log(f"{len(mis) + 1} material instances")

    stone = {k: mis[k] for k in ("M_Marae_BasaltMoss", "M_Marae_Basalt", "M_Marae_BasaltLight", "M_Marae_CoralBlock")}
    setup_mesh("SM_Marae_Pyramid", {**stone, "M_Marae_CoralGravel": mis["M_Marae_CoralGravel"]},
               complex_collision=True, nanite=True)
    setup_mesh("SM_Marae_Altar", stone)
    setup_mesh("SM_Marae_StoneWall", stone)
    torch = setup_mesh("SM_Marae_Torch", {"M_Marae_Wood": mis["M_Marae_Wood"], "M_Marae_Sticks": mis["M_Marae_Sticks"],
                                          "M_Marae_Rope": mis["M_Marae_Rope"], "M_Marae_Flame": flame,
                                          "M_Marae_FlameCore": core})
    offerings = setup_mesh("SM_Marae_Offerings", {
        "wooden_bowl_01": load("/Game/MonkeIsland/Environment/Village/Materials/MI_Minka_wooden_bowl_01"),
        "M_Marae_Cowrie": cowrie})
    conch = setup_mesh("SM_Marae_Conch", {"lambis_shell": load("/Game/MonkeIsland/Environment/Props/Beach/Materials/MI_LambisShell")})
    artifact = setup_mesh("SM_Marae_PearlArtifact", {"M_Marae_Gold": gold, "M_Marae_BlackPearl": pearl})
    log("meshes set up")

    build_torch(torch, flicker_lf)
    # the rare one: the black pearl of the marae, worth 1 500 $, with a faint violet glint so it is spotted at night
    build_valuable("BP_Marae_PearlArtifact", artifact, round(SPEC["parts"]["SM_Marae_PearlArtifact"]["physics"]["mass_kg"], 2),
                   1500, glow=(0.25, unreal.Color(r=200, g=150, b=255, a=255)))
    build_valuable("BP_Marae_Offerings", offerings, round(SPEC["parts"]["SM_Marae_Offerings"]["physics"]["mass_kg"], 2), 120)
    build_valuable("BP_Marae_Conch", conch, max(0.25, SPEC["parts"]["SM_Marae_Conch"]["physics"]["mass_kg"]), 80)
    log("done")


main()
