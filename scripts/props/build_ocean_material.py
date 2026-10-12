"""Builds the wave sea: MPC_Waves, M_Water_Ocean and DA_WaveRules in /Game/MonkeIsland/Environment/Water.

Model: Gerstner waves (GPU Gems 1, ch. 1, "Effective Water Simulation from Physical Models", Finch 2004), the
classic real time ocean, with deep water dispersion (omega^2 = g k, Tessendorf 2001). The same sum runs in C++
(Water/WaveMath.h) for buoyancy: UIslandWaterSubsystem feeds MPC_Waves every frame on the server clock.
Look: Single Layer Water with the colours of M_Water_Lagoon, Gerstner normals plus two panning detail normals
(T_Water_N), foam on the highest crests.

Editor commandlet (editor closed), or the editor Python console:
  UnrealEditor-Cmd.exe monke_island.uproject -run=pythonscript -script="<this file>" -unattended -nosplash
"""
import unreal

DEST = "/Game/MonkeIsland/Environment/Water"
MPC_NAME = "MPC_Waves"
MAT_NAME = "M_Water_Ocean"
RULES_NAME = "DA_WaveRules"
NORMAL_TEX = "/Game/MonkeIsland/Environment/Water/Textures/T_Water_N"
MAX_WAVES = 8

mel = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
L = unreal.log_warning


def load_or_create(name, cls, factory):
    path = f"{DEST}/{name}"
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        return unreal.load_asset(path)
    return tools.create_asset(name, DEST, cls, factory)


# ---------------------------------------------------------------- collection

def build_mpc():
    mpc = load_or_create(MPC_NAME, unreal.MaterialParameterCollection, unreal.MaterialParameterCollectionFactoryNew())
    scalars = []
    for name, value in (("WaveTime", 0.0), ("WaveGridInner", 25.0), ("WaveGridGrowth", 0.02), ("WaveMaxHeight", 40.0)):
        p = unreal.CollectionScalarParameter()
        p.set_editor_property("parameter_name", name)
        p.set_editor_property("default_value", value)
        scalars.append(p)
    vectors = []
    follow = unreal.CollectionVectorParameter()
    follow.set_editor_property("parameter_name", "WaveFollow")
    follow.set_editor_property("default_value", unreal.LinearColor(0, 0, 0, 0))
    vectors.append(follow)
    for i in range(MAX_WAVES):
        for suffix, value in (("A", unreal.LinearColor(1, 0, 0, 0)), ("B", unreal.LinearColor(0, 0, 0, 100))):
            p = unreal.CollectionVectorParameter()
            p.set_editor_property("parameter_name", f"Wave{i}{suffix}")
            p.set_editor_property("default_value", value)
            vectors.append(p)
    mpc.set_editor_property("scalar_parameters", scalars)
    mpc.set_editor_property("vector_parameters", vectors)
    unreal.EditorAssetLibrary.save_loaded_asset(mpc)
    return mpc


# ---------------------------------------------------------------- material

WAVE_LOOP = """
float4 A[8] = { A0, A1, A2, A3, A4, A5, A6, A7 };
float4 B[8] = { B0, B1, B2, B3, B4, B5, B6, B7 };
// Pos = grid point in the world (shifted under the camera), Cam = that shift (WaveFollow)
// cells of the camera grid widen with the distance: a wave fades where they pass an eighth of its length
float Cell = Inner + Growth * length(Pos.xy - Cam.xy);
"""

WPO_CODE = WAVE_LOOP + """
float3 Offset = float3(0, 0, 0);
for (int i = 0; i < 8; i++)
{
    // A = (dir x, dir y, k, amplitude), B = (horizontal, omega, phase, wavelength): Water/WaveMath.h
    float FadeOut = saturate(2.0 - 8.0 * Cell / max(B[i].w, 1.0));
    float Angle = A[i].z * dot(A[i].xy, Pos.xy) - B[i].y * T + B[i].z;
    float S, C;
    sincos(Angle, S, C);
    Offset.xy += FadeOut * B[i].x * A[i].xy * C;
    Offset.z += FadeOut * A[i].w * S;
}
// the grid is built at the world origin: move it under the camera
Offset.xy += Cam.xy;
return Offset;
"""

NORMAL_CODE = WAVE_LOOP + """
float3 N = float3(0, 0, 1);
float H = 0;
for (int i = 0; i < 8; i++)
{
    float FadeOut = saturate(2.0 - 8.0 * Cell / max(B[i].w, 1.0));
    float Angle = A[i].z * dot(A[i].xy, Pos.xy) - B[i].y * T + B[i].z;
    float S, C;
    sincos(Angle, S, C);
    float WA = FadeOut * A[i].z * A[i].w;
    N.x -= A[i].x * WA * C;
    N.y -= A[i].y * WA * C;
    N.z -= FadeOut * B[i].x * A[i].z * S;
    H += FadeOut * A[i].w * S;
}
return float4(normalize(N), H);
"""

UV_CODE = "return Pos.xy * Scale + T * Speed;"

COMBINE_CODE = """
// Gerstner normal (world) tilted by the two detail normals (tangent space of a flat grid = world)
float2 Detail = (D1.xy + D2.xy) * Strength;
return normalize(float3(N.xy + Detail, N.z));
"""

FOAM_CODE = """
// white water on the highest crests, broken by the noise
float Crest = saturate((N.w / max(MaxH, 1.0) - Start) / max(1.0 - Start, 0.01));
return saturate(Crest * Crest * (0.35 + Noise * 1.3)) * Amount;
"""


class Graph:
    def __init__(self, mat):
        self.mat = mat
        self.y = 0

    def node(self, cls, x, y=None):
        if y is None:
            y = self.y
            self.y += 140
        return mel.create_material_expression(self.mat, cls, x, y)

    def scalar(self, name, value, x):
        n = self.node(unreal.MaterialExpressionScalarParameter, x)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", value)
        return n

    def vector(self, name, value, x):
        n = self.node(unreal.MaterialExpressionVectorParameter, x)
        n.set_editor_property("parameter_name", name)
        n.set_editor_property("default_value", value)
        return n

    def collection(self, mpc, name, x):
        n = self.node(unreal.MaterialExpressionCollectionParameter, x)
        n.set_editor_property("collection", mpc)
        n.set_editor_property("parameter_name", name)
        return n

    def custom(self, name, code, inputs, output, x):
        n = self.node(unreal.MaterialExpressionCustom, x)
        n.set_editor_property("description", name)
        n.set_editor_property("code", code)
        n.set_editor_property("output_type", output)
        ins = []
        for input_name, _ in inputs:
            ci = unreal.CustomInput()
            ci.set_editor_property("input_name", input_name)
            ins.append(ci)
        n.set_editor_property("inputs", ins)
        for input_name, (src, pin) in inputs:
            if not mel.connect_material_expressions(src, pin, n, input_name):
                unreal.log_error(f"[ocean] cannot connect {input_name} of {name}")
        return n


def mask(g, src, channels, x):
    m = g.node(unreal.MaterialExpressionComponentMask, x)
    for c in "rgba":
        m.set_editor_property(c, c in channels)
    mel.connect_material_expressions(src, "", m, "")
    return m


def build_material(mpc):
    mat = load_or_create(MAT_NAME, unreal.Material, unreal.MaterialFactoryNew())
    mel.delete_all_material_expressions(mat)
    mat.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_SINGLE_LAYER_WATER)
    mat.set_editor_property("blend_mode", unreal.BlendMode.BLEND_OPAQUE)
    mat.set_editor_property("tangent_space_normal", False)
    # seen from below too: a swimmer's camera dips under the crests
    mat.set_editor_property("two_sided", True)
    g = Graph(mat)

    # inputs: undisplaced world position (the grid point), camera, clock and the wave trains
    local = g.node(unreal.MaterialExpressionWorldPosition, -2900)
    local.set_editor_property("world_position_shader_offset", unreal.WorldPositionIncludedOffsets.WPT_EXCLUDE_ALL_SHADER_OFFSETS)
    cam = g.collection(mpc, "WaveFollow", -2900)
    pos = g.custom("Grid point", "return float3(Pos.xy + Follow.xy, Pos.z);", [("Pos", (local, "")), ("Follow", (cam, ""))],
                   unreal.CustomMaterialOutputType.CMOT_FLOAT3, -2600)
    t = g.collection(mpc, "WaveTime", -2600)
    inner = g.collection(mpc, "WaveGridInner", -2600)
    growth = g.collection(mpc, "WaveGridGrowth", -2600)
    max_h = g.collection(mpc, "WaveMaxHeight", -2600)
    waves = [("Pos", (pos, "")), ("Cam", (cam, "")), ("T", (t, "")), ("Inner", (inner, "")), ("Growth", (growth, ""))]
    for i in range(MAX_WAVES):
        waves.append((f"A{i}", (g.collection(mpc, f"Wave{i}A", -2600), "")))
    for i in range(MAX_WAVES):
        waves.append((f"B{i}", (g.collection(mpc, f"Wave{i}B", -2600), "")))

    g.y = 0
    wpo = g.custom("Gerstner offset", WPO_CODE, waves, unreal.CustomMaterialOutputType.CMOT_FLOAT3, -1800)
    gerstner = g.custom("Gerstner normal and height", NORMAL_CODE, waves, unreal.CustomMaterialOutputType.CMOT_FLOAT4, -1800)
    mel.connect_material_property(wpo, "", unreal.MaterialProperty.MP_WORLD_POSITION_OFFSET)

    # detail: two panning normal maps in world space (scales of M_Water_Lagoon)
    texture = unreal.load_asset(NORMAL_TEX)
    details = []
    for name, scale, speed in (("Small", 0.0008, unreal.LinearColor(0.021, 0.013, 0, 0)), ("Large", 0.00025, unreal.LinearColor(-0.006, 0.009, 0, 0))):
        s = g.scalar(f"WaveScale{name}", scale, -1800)
        v = g.vector(f"WaveSpeed{name}", speed, -1800)
        sp = mask(g, v, "rg", -1600)
        uv = g.custom(f"UV {name}", UV_CODE, [("Pos", (pos, "")), ("T", (t, "")), ("Scale", (s, "")), ("Speed", (sp, ""))],
                      unreal.CustomMaterialOutputType.CMOT_FLOAT2, -1400)
        tex = g.node(unreal.MaterialExpressionTextureSampleParameter2D, -1100)
        tex.set_editor_property("parameter_name", f"WaveScale{name}Normal")
        tex.set_editor_property("texture", texture)
        tex.set_editor_property("sampler_type", unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
        mel.connect_material_expressions(uv, "", tex, "UVs")
        details.append(tex)
    strength = g.scalar("WaveStrength", 0.35, -1100)
    normal = g.custom("Combine normals", COMBINE_CODE, [("N", (gerstner, "")), ("D1", (details[0], "RGB")), ("D2", (details[1], "RGB")),
                                                       ("Strength", (strength, ""))], unreal.CustomMaterialOutputType.CMOT_FLOAT3, -700)
    mel.connect_material_property(normal, "", unreal.MaterialProperty.MP_NORMAL)

    # foam on the crests
    noise = g.node(unreal.MaterialExpressionNoise, -1100)
    noise.set_editor_property("scale", 0.012)
    noise.set_editor_property("levels", 3)
    noise.set_editor_property("output_min", 0.0)
    noise.set_editor_property("output_max", 1.0)
    noise.set_editor_property("noise_function", unreal.NoiseFunction.NOISEFUNCTION_VALUE_ALU)
    mel.connect_material_expressions(pos, "", noise, "Position")
    start = g.scalar("FoamStart", 0.5, -1100)
    amount = g.scalar("FoamAmount", 0.85, -1100)
    foam = g.custom("Crest foam", FOAM_CODE, [("N", (gerstner, "")), ("MaxH", (max_h, "")), ("Start", (start, "")),
                                             ("Noise", (noise, "")), ("Amount", (amount, ""))], unreal.CustomMaterialOutputType.CMOT_FLOAT1, -700)
    foam_color = g.vector("FoamColor", unreal.LinearColor(0.85, 0.9, 0.92, 1), -700)
    mel.connect_material_property(foam_color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(foam, "", unreal.MaterialProperty.MP_OPACITY)
    rough = g.scalar("WaterRoughness", 0.04, -700)
    foam_rough = g.scalar("FoamRoughness", 0.6, -700)
    lerp_r = g.node(unreal.MaterialExpressionLinearInterpolate, -450)
    mel.connect_material_expressions(rough, "", lerp_r, "A")
    mel.connect_material_expressions(foam_rough, "", lerp_r, "B")
    mel.connect_material_expressions(foam, "", lerp_r, "Alpha")
    mel.connect_material_property(lerp_r, "", unreal.MaterialProperty.MP_ROUGHNESS)

    # water body: colours of M_Water_Lagoon (scattering = albedo x extinction, absorption = the rest)
    albedo = g.vector("WaterAlbedo", unreal.LinearColor(0.04, 0.16, 0.32, 1), -700)
    extinction = g.vector("WaterExtinction", unreal.LinearColor(0.0045, 0.001, 0.0007, 1), -700)
    phase = g.scalar("WaterPhaseG", 0.1, -700)
    scatter = g.node(unreal.MaterialExpressionMultiply, -450)
    mel.connect_material_expressions(albedo, "", scatter, "A")
    mel.connect_material_expressions(extinction, "", scatter, "B")
    absorb = g.node(unreal.MaterialExpressionSubtract, -450)
    mel.connect_material_expressions(extinction, "", absorb, "A")
    mel.connect_material_expressions(scatter, "", absorb, "B")
    out = g.node(unreal.MaterialExpressionSingleLayerWaterMaterialOutput, -200)
    for src, names in ((scatter, ("ScatteringCoefficients", "Scattering Coefficients")),
                       (absorb, ("AbsorptionCoefficients", "Absorption Coefficients")), (phase, ("PhaseG", "Phase G"))):
        if not any(mel.connect_material_expressions(src, "", out, n) for n in names):
            unreal.log_error(f"[ocean] cannot connect {names[0]} of the water output")

    mel.recompile_material(mat)
    unreal.EditorAssetLibrary.save_loaded_asset(mat)
    return mat


def build_rules(mpc, mat):
    rules = load_or_create(RULES_NAME, unreal.WaveRules, unreal.DataAssetFactory())
    rules.set_editor_property("wave_parameters", mpc)
    rules.set_editor_property("surface_material", mat)
    unreal.EditorAssetLibrary.save_loaded_asset(rules)
    return rules


mpc = build_mpc()
mat = build_material(mpc)
rules = build_rules(mpc, mat)
L(f"[ocean] done: {mpc.get_path_name()}, {mat.get_path_name()}, {rules.get_path_name()}")
