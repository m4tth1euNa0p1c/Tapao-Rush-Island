"""Materials for the atoll look-dev (Blender shader nodes).

Conventions shared with the generators:
- vertex color attribute "Col": RGB = tint multiplier (1 = texture color), A = dryness 0..1
- terrain: "MaskA" (sand_dry, wrack, soil, litter), "MaskB" (rubble, feo, wetness, algae)
"""
from pathlib import Path

import bpy

ART = Path(__file__).resolve().parents[2] / "art"
TEX = ART / "textures"

_cache = {}


def img(path, non_color=False, packed=False):
    path = Path(path)
    im = bpy.data.images.load(str(path), check_existing=True)
    if non_color:
        im.colorspace_settings.name = "Non-Color"
    if packed:
        im.alpha_mode = "CHANNEL_PACKED"
    return im


def find(folder, *keys):
    folder = Path(folder)
    for k in keys:
        hits = sorted(p for p in folder.iterdir() if k.lower() in p.name.lower())
        if hits:
            return hits[0]
    raise FileNotFoundError(f"{folder} {keys}")


class NB:
    """Tiny node-graph builder. Inputs accept sockets (linked) or values (set)."""

    def __init__(self, mat):
        mat.use_nodes = True
        self.nt = mat.node_tree
        self.nt.nodes.clear()
        self.x = 0

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        n.location = (self.x, 0)
        self.x += 40
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def put(self, sock, val):
        if isinstance(val, bpy.types.NodeSocket):
            self.nt.links.new(val, sock)
        elif val is not None:
            sock.default_value = val

    @staticmethod
    def sock(sockets, ident):
        for s in sockets:
            if s.identifier == ident:
                return s
        return sockets[ident]

    def math(self, op, a, b=None, c=None, clamp=False):
        n = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
        self.put(n.inputs[0], a)
        if b is not None:
            self.put(n.inputs[1], b)
        if c is not None:
            self.put(n.inputs[2], c)
        return n.outputs[0]

    def vmath(self, op, a, b=None, scale=None):
        n = self.node("ShaderNodeVectorMath", operation=op)
        self.put(n.inputs[0], a)
        if b is not None:
            self.put(n.inputs[1], b)
        if scale is not None:
            self.put(n.inputs["Scale"], scale)
        return n.outputs["Value"] if op in ("DOT_PRODUCT", "LENGTH", "DISTANCE") else n.outputs["Vector"]

    def mix(self, fac, a, b, kind="RGBA", blend="MIX"):
        n = self.node("ShaderNodeMix", data_type=kind, blend_type=blend, clamp_factor=True)
        suffix = {"RGBA": "Color", "FLOAT": "Float", "VECTOR": "Vector"}[kind]
        self.put(self.sock(n.inputs, "Factor_Float" if kind != "VECTOR" else "Factor_Float"), fac)
        self.put(self.sock(n.inputs, f"A_{suffix}"), a)
        self.put(self.sock(n.inputs, f"B_{suffix}"), b)
        return self.sock(n.outputs, f"Result_{suffix}")

    def maprange(self, v, a, b, c=0.0, d=1.0, interp="SMOOTHSTEP"):
        n = self.node("ShaderNodeMapRange", interpolation_type=interp, clamp=True)
        self.put(n.inputs["Value"], v)
        self.put(n.inputs["From Min"], a)
        self.put(n.inputs["From Max"], b)
        self.put(n.inputs["To Min"], c)
        self.put(n.inputs["To Max"], d)
        return n.outputs["Result"]

    def tex(self, path, vector=None, non_color=False, packed=False, proj="FLAT", blend=0.0):
        n = self.node("ShaderNodeTexImage", projection=proj)
        n.image = img(path, non_color, packed)
        if proj == "BOX":
            n.projection_blend = blend
        self.put(n.inputs["Vector"], vector)
        return n

    def attr(self, name):
        return self.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name=name)

    def sep(self, v):
        n = self.node("ShaderNodeSeparateXYZ")
        self.put(n.inputs[0], v)
        return n.outputs

    def comb(self, x, y, z):
        n = self.node("ShaderNodeCombineXYZ")
        self.put(n.inputs[0], x)
        self.put(n.inputs[1], y)
        self.put(n.inputs[2], z)
        return n.outputs[0]

    def principled(self, **inputs):
        n = self.node("ShaderNodeBsdfPrincipled")
        for k, v in inputs.items():
            self.put(n.inputs[k.replace("_", " ")], v)
        return n

    def normal_map(self, color, strength=1.0):
        n = self.node("ShaderNodeNormalMap", uv_map="UVMap")
        self.put(n.inputs["Color"], color)
        self.put(n.inputs["Strength"], strength)
        return n.outputs["Normal"]

    def out(self, surface, displacement=None):
        o = self.node("ShaderNodeOutputMaterial")
        self.put(o.inputs["Surface"], surface)
        if displacement is not None:
            self.put(o.inputs["Displacement"], displacement)
        return o


def new_mat(name):
    if name in bpy.data.materials:
        bpy.data.materials.remove(bpy.data.materials[name])
    return bpy.data.materials.new(name)


def cached(fn):
    def wrap(*a, **k):
        key = (fn.__name__, a, tuple(sorted(k.items())))
        m = _cache.get(key)
        if m is None or m.name not in bpy.data.materials:
            m = fn(*a, **k)
            _cache[key] = m
        return m
    return wrap


# ---------------------------------------------------------------- vegetation

@cached
def leaf(species, dry=(0.55, 0.42, 0.25), translucency=0.22, rough_mul=1.0, sheen=0.0, name=None):
    """Leaf: texture * Col.rgb, mixed toward a dry color by Col.a, with back-lighting."""
    mat = new_mat(name or f"M_Leaf_{species}")
    nb = NB(mat)
    base = TEX / "plants" / species
    c = nb.tex(f"{base}_basecolor.png")
    r = nb.tex(f"{base}_roughness.png", non_color=True)
    n = nb.tex(f"{base}_normal_gl.png", non_color=True)
    a = nb.attr("Col")
    green = nb.vmath("MULTIPLY", c.outputs["Color"], a.outputs["Color"])
    lum = nb.node("ShaderNodeRGBToBW")
    nb.put(lum.inputs[0], c.outputs["Color"])
    drycol = nb.vmath("SCALE", dry, scale=nb.math("MULTIPLY", lum.outputs[0], 3.0))
    col = nb.mix(a.outputs["Alpha"], green, drycol)
    rough = nb.mix(a.outputs["Alpha"], nb.math("MULTIPLY", r.outputs["Color"], rough_mul), 0.85, "FLOAT")
    p = nb.principled(Base_Color=col, Roughness=rough, Normal=nb.normal_map(n.outputs["Color"], 0.8))
    if sheen:
        nb.put(p.inputs["Sheen Weight"], sheen)
        nb.put(p.inputs["Sheen Roughness"], 0.4)
    tr = nb.node("ShaderNodeBsdfTranslucent")
    nb.put(tr.inputs["Color"], nb.vmath("MULTIPLY", col, (0.9, 1.0, 0.6)))
    fac = nb.math("MULTIPLY", translucency, nb.math("SUBTRACT", 1.0, nb.math("MULTIPLY", a.outputs["Alpha"], 0.6)))
    mix = nb.node("ShaderNodeMixShader")
    nb.put(mix.inputs[0], fac)
    nb.put(mix.inputs[1], p.outputs[0])
    nb.put(mix.inputs[2], tr.outputs[0])
    nb.out(mix.outputs[0])
    return mat


@cached
def leaf_card(species, dry=(0.5, 0.42, 0.28), translucency=0.15, name=None):
    """Alpha-tested leafy twig card (masked): texture alpha clipped at 0.5."""
    mat = new_mat(name or f"M_LeafCard_{species}")
    nb = NB(mat)
    base = TEX / "plants" / species
    c = nb.tex(f"{base}_basecolor.png", packed=True)
    n = nb.tex(f"{base}_normal_gl.png", non_color=True)
    a = nb.attr("Col")
    green = nb.vmath("MULTIPLY", c.outputs["Color"], a.outputs["Color"])
    lum = nb.node("ShaderNodeRGBToBW")
    nb.put(lum.inputs[0], c.outputs["Color"])
    drycol = nb.vmath("SCALE", dry, scale=nb.math("MULTIPLY", lum.outputs[0], 3.0))
    col = nb.mix(a.outputs["Alpha"], green, drycol)
    p = nb.principled(Base_Color=col, Roughness=0.62, Normal=nb.normal_map(n.outputs["Color"], 0.7))
    tr = nb.node("ShaderNodeBsdfTranslucent")
    nb.put(tr.inputs["Color"], nb.vmath("MULTIPLY", col, (0.9, 1.0, 0.6)))
    mix = nb.node("ShaderNodeMixShader")
    nb.put(mix.inputs[0], translucency)
    nb.put(mix.inputs[1], p.outputs[0])
    nb.put(mix.inputs[2], tr.outputs[0])
    cut = nb.node("ShaderNodeMixShader")
    nb.put(cut.inputs[0], nb.math("GREATER_THAN", c.outputs["Alpha"], 0.5))
    nb.put(cut.inputs[1], nb.node("ShaderNodeBsdfTransparent").outputs[0])
    nb.put(cut.inputs[2], mix.outputs[0])
    nb.out(cut.outputs[0])
    return mat


@cached
def bark(name, folder, tint=(1.0, 1.0, 1.0), rough_add=0.0):
    """Image bark (UV: u around, v along in meters) * tint * Col.rgb."""
    mat = new_mat(name)
    nb = NB(mat)
    folder = Path(folder)
    c = nb.tex(find(folder, "_diff_", "_Color"))
    r = nb.tex(find(folder, "_rough_", "_Roughness"), non_color=True)
    n = nb.tex(find(folder, "_nor_gl_", "_NormalGL"), non_color=True)
    a = nb.attr("Col")
    col = nb.vmath("MULTIPLY", nb.vmath("MULTIPLY", c.outputs["Color"], tint), a.outputs["Color"])
    p = nb.principled(Base_Color=col, Roughness=nb.math("ADD", r.outputs["Color"], rough_add, clamp=True),
                      Normal=nb.normal_map(n.outputs["Color"], 1.0))
    nb.out(p.outputs[0])
    return mat


def palm_bark():
    return bark("M_CoconutTrunk", TEX / "palm" / "palm_tree_bark")


def palm_fiber():
    return bark("M_CoconutLeafBase", TEX / "palm" / "palm_bark", (0.95, 0.72, 0.50), 0.1)


def pandanus_bark():
    return bark("M_PandanusBark", TEX / "palm" / "palm_bark", (0.92, 0.86, 0.78), 0.05)


def shrub_bark():
    return bark("M_ShrubBark", TEX / "shrub" / "bark_brown_02", (0.85, 0.82, 0.78), 0.0)


def driftwood():
    return bark("M_Driftwood", TEX / "palm" / "palm_tree_bark", (1.25, 1.22, 1.18), 0.1)


@cached
def vcolor(name, rough=(0.45, 0.85), bump=0.15, noise_scale=40.0, translucency=0.0):
    """Procedural material from Col: rgb color, alpha = dryness -> roughness. For nuts, fruit, flowers."""
    mat = new_mat(name)
    nb = NB(mat)
    a = nb.attr("Col")
    tc = nb.node("ShaderNodeTexCoord")
    nz = nb.node("ShaderNodeTexNoise", noise_dimensions="3D")
    nb.put(nz.inputs["Vector"], tc.outputs["Object"])
    nb.put(nz.inputs["Scale"], noise_scale)
    nb.put(nz.inputs["Detail"], 6.0)
    var = nb.math("MULTIPLY_ADD", nz.outputs["Fac"], 0.35, 0.82)
    col = nb.vmath("SCALE", a.outputs["Color"], scale=var)
    rough_v = nb.mix(a.outputs["Alpha"], rough[0], rough[1], "FLOAT")
    bm = nb.node("ShaderNodeBump")
    nb.put(bm.inputs["Strength"], bump)
    nb.put(bm.inputs["Height"], nz.outputs["Fac"])
    p = nb.principled(Base_Color=col, Roughness=rough_v, Normal=bm.outputs["Normal"])
    if translucency:
        tr = nb.node("ShaderNodeBsdfTranslucent")
        nb.put(tr.inputs["Color"], col)
        mix = nb.node("ShaderNodeMixShader")
        nb.put(mix.inputs[0], translucency)
        nb.put(mix.inputs[1], p.outputs[0])
        nb.put(mix.inputs[2], tr.outputs[0])
        nb.out(mix.outputs[0])
    else:
        nb.out(p.outputs[0])
    return mat


def coconut():
    return vcolor("M_Coconut", (0.38, 0.92), 0.55, 26.0)


def fruit():
    return vcolor("M_PandanusFruit", (0.35, 0.8), 0.6, 9.0)


def flower():
    return vcolor("M_Flower", (0.45, 0.7), 0.05, 60.0, translucency=0.35)


@cached
def coral_rock(name="M_CoralBlock", layer="feo", scale=0.7):
    """Box-projected packed terrain layer on object coords (no UV needed) * Col tint."""
    mat = new_mat(name)
    nb = NB(mat)
    tc = nb.node("ShaderNodeTexCoord")
    v = nb.vmath("SCALE", tc.outputs["Object"], scale=1.0 / scale)
    L = TEX / "terrain_layers"
    ch = nb.tex(L / f"{layer}_ch.png", v, packed=True, proj="BOX", blend=0.3)
    a = nb.attr("Col")
    col = nb.vmath("MULTIPLY", ch.outputs["Color"], a.outputs["Color"])
    bm = nb.node("ShaderNodeBump")
    nb.put(bm.inputs["Strength"], 0.6)
    nb.put(bm.inputs["Distance"], 0.03)
    nb.put(bm.inputs["Height"], ch.outputs["Alpha"])
    p = nb.principled(Base_Color=col, Roughness=0.85, Normal=bm.outputs["Normal"])
    nb.out(p.outputs[0])
    return mat


def rubble():
    return vcolor("M_CoralRubble", (0.8, 0.92), 1.2, 70.0)


@cached
def frond_card():
    """Alpha-card coconut frond for far LOD palms."""
    mat = new_mat("M_CoconutFrond_Card")
    nb = NB(mat)
    base = TEX / "palm" / "coconut_frond" / "frond_green"
    c = nb.tex(f"{base}_basecolor.png")
    c.image.alpha_mode = "CHANNEL_PACKED"
    a = nb.attr("Col")
    col = nb.vmath("MULTIPLY", c.outputs["Color"], a.outputs["Color"])
    p = nb.principled(Base_Color=col, Roughness=0.55)
    tr = nb.node("ShaderNodeBsdfTransparent")
    mix = nb.node("ShaderNodeMixShader")
    nb.put(mix.inputs[0], c.outputs["Alpha"])
    nb.put(mix.inputs[1], tr.outputs[0])
    nb.put(mix.inputs[2], p.outputs[0])
    nb.out(mix.outputs[0])
    return mat


@cached
def far_foliage():
    """Distant shrub masses: noisy green with leafy bump."""
    mat = new_mat("M_FarFoliage")
    nb = NB(mat)
    tc = nb.node("ShaderNodeTexCoord")
    vo = nb.node("ShaderNodeTexVoronoi")
    nb.put(vo.inputs["Vector"], tc.outputs["Object"])
    nb.put(vo.inputs["Scale"], 6.0)
    nz = nb.node("ShaderNodeTexNoise")
    nb.put(nz.inputs["Vector"], tc.outputs["Object"])
    nb.put(nz.inputs["Scale"], 0.4)
    col = nb.mix(nz.outputs["Fac"], (0.06, 0.13, 0.03, 1), (0.16, 0.24, 0.06, 1))
    col = nb.vmath("SCALE", col, scale=nb.math("MULTIPLY_ADD", vo.outputs["Distance"], 0.8, 0.6))
    bm = nb.node("ShaderNodeBump")
    nb.put(bm.inputs["Strength"], 0.8)
    nb.put(bm.inputs["Height"], vo.outputs["Distance"])
    p = nb.principled(Base_Color=col, Roughness=0.7, Normal=bm.outputs["Normal"])
    nb.out(p.outputs[0])
    return mat


# ---------------------------------------------------------------- terrain & water

TERRAIN_LAYERS = [  # (name, tile meters, roughness, height-blend contrast)
    ("sand_wet", 3.0, 0.55, 0.0),
    ("sand_dry", 2.4, 0.88, 0.45),
    ("wrack", 2.2, 0.85, 0.7),
    ("soil", 2.6, 0.92, 0.6),
    ("litter", 2.0, 0.85, 0.7),
    ("rubble", 2.2, 0.82, 0.8),
    ("feo", 3.5, 0.75, 0.6),
]
LAGOON_DEEP = (0.012, 0.20, 0.27)
OCEAN_DEEP = (0.004, 0.035, 0.11)
ABSORB = (0.95, 0.16, 0.11)  # per meter of depth, two-way slant path (red dies first -> turquoise)


def water_tint(nb, color, pos):
    """Seabed color seen through water: color*T + deep*(1-T), T = exp(-depth*k)."""
    s = nb.sep(pos)
    depth = nb.math("MAXIMUM", nb.math("MULTIPLY", s[2], -1.0), 0.0)
    T = [nb.math("EXPONENT", nb.math("MULTIPLY", depth, -k)) for k in ABSORB]
    Tv = nb.comb(*T)
    side = nb.maprange(s[0], -12.0, 12.0)
    deep = nb.mix(side, (*OCEAN_DEEP, 1), (*LAGOON_DEEP, 1))
    a = nb.vmath("MULTIPLY", color, Tv)
    inv = nb.vmath("SUBTRACT", (1.0, 1.0, 1.0), Tv)
    return nb.vmath("ADD", a, nb.vmath("MULTIPLY", deep, inv))


@cached
def terrain():
    mat = new_mat("M_AtollTerrain")
    nb = NB(mat)
    L = TEX / "terrain_layers"
    uvn = nb.node("ShaderNodeUVMap", uv_map="UVMap")
    # low-frequency UV warp breaks visible tiling
    wn = nb.node("ShaderNodeTexNoise", noise_dimensions="2D")
    nb.put(wn.inputs["Vector"], uvn.outputs["UV"])
    nb.put(wn.inputs["Scale"], 0.06)
    nb.put(wn.inputs["Detail"], 2.0)
    warp = nb.vmath("SCALE", nb.vmath("SUBTRACT", wn.outputs["Color"], (0.5, 0.5, 0.5)), scale=1.6)
    uv = nb.vmath("ADD", uvn.outputs["UV"], warp)
    mA = nb.attr("MaskA")
    mB = nb.attr("MaskB")
    sA = nb.node("ShaderNodeSeparateColor")
    nb.put(sA.inputs[0], mA.outputs["Color"])
    sB = nb.node("ShaderNodeSeparateColor")
    nb.put(sB.inputs[0], mB.outputs["Color"])
    masks = {"sand_dry": sA.outputs[0], "wrack": sA.outputs[1], "soil": sA.outputs[2],
             "litter": mA.outputs["Alpha"], "rubble": sB.outputs[0], "feo": sB.outputs[1]}
    col = nrm = rough = None
    for name, tile, r, hc in TERRAIN_LAYERS:
        v = nb.vmath("SCALE", uv, scale=1.0 / tile)
        ch = nb.tex(L / f"{name}_ch.png", v, packed=True)
        nm = nb.tex(L / f"{name}_n.png", v, non_color=True)
        if col is None:
            col, nrm, rough = ch.outputs["Color"], nm.outputs["Color"], r
            continue
        t = nb.math("MULTIPLY_ADD", nb.math("SUBTRACT", ch.outputs["Alpha"], 0.5), hc, masks[name])
        f = nb.maprange(t, 0.42, 0.58)
        col = nb.mix(f, col, ch.outputs["Color"])
        nrm = nb.mix(f, nrm, nm.outputs["Color"])
        rough = nb.mix(f, rough, r, "FLOAT")
    geo = nb.node("ShaderNodeNewGeometry")
    pos = geo.outputs["Position"]
    z = nb.sep(pos)[2]
    # wetness: mask + everything near/below sea level
    wet = nb.math("MAXIMUM", sB.outputs[2], nb.maprange(z, 0.5, 0.12))
    col = nb.vmath("SCALE", col, scale=nb.math("MULTIPLY_ADD", wet, -0.38, 1.0))
    rough = nb.mix(wet, rough, 0.22, "FLOAT")
    # algae film on reef flat
    col = nb.mix(nb.math("MULTIPLY", mB.outputs["Alpha"], 0.75), col,
                 nb.vmath("MULTIPLY", col, (0.55, 0.60, 0.40)))
    # macro brightness variation (tens of meters)
    mn = nb.node("ShaderNodeTexNoise", noise_dimensions="2D")
    nb.put(mn.inputs["Vector"], uvn.outputs["UV"])
    nb.put(mn.inputs["Scale"], 0.012)
    nb.put(mn.inputs["Detail"], 3.0)
    col = nb.vmath("SCALE", col, scale=nb.math("MULTIPLY_ADD", mn.outputs["Fac"], 0.28, 0.86))
    col = water_tint(nb, col, pos)
    p = nb.principled(Base_Color=col, Roughness=rough, Normal=nb.normal_map(nrm, 1.0))
    nb.put(p.inputs["Specular IOR Level"], 0.4)
    nb.out(p.outputs[0])
    return mat


@cached
def water_surface():
    """Transparent water: Fresnel reflection + see-through seabed (seabed carries the depth tint)."""
    mat = new_mat("M_WaterSurface")
    nb = NB(mat)
    geo = nb.node("ShaderNodeNewGeometry")
    pos = geo.outputs["Position"]
    x = nb.sep(pos)[0]
    n1 = nb.node("ShaderNodeTexNoise", noise_dimensions="3D")
    nb.put(n1.inputs["Vector"], nb.vmath("MULTIPLY", pos, (1.0, 0.55, 0.0)))
    nb.put(n1.inputs["Scale"], 0.9)
    nb.put(n1.inputs["Detail"], 8.0)
    n2 = nb.node("ShaderNodeTexWave", wave_type="BANDS", bands_direction="Y")
    nb.put(n2.inputs["Vector"], pos)
    nb.put(n2.inputs["Scale"], 0.09)
    nb.put(n2.inputs["Distortion"], 6.0)
    nb.put(n2.inputs["Detail"], 4.0)
    h = nb.math("MULTIPLY_ADD", n2.outputs["Fac"], 0.5, n1.outputs["Fac"])
    ocean = nb.maprange(x, -30.0, -60.0)  # rougher seaward of the reef
    bm = nb.node("ShaderNodeBump")
    nb.put(bm.inputs["Strength"], nb.math("MULTIPLY_ADD", ocean, 0.2, 0.22))
    nb.put(bm.inputs["Distance"], 0.25)
    nb.put(bm.inputs["Height"], h)
    fr = nb.node("ShaderNodeFresnel")
    nb.put(fr.inputs["IOR"], 1.333)
    nb.put(fr.inputs["Normal"], bm.outputs["Normal"])
    spec = nb.principled(Base_Color=(0, 0, 0, 1), Roughness=0.06, Normal=bm.outputs["Normal"], IOR=1.333)
    nb.put(spec.inputs["Specular IOR Level"], 0.24)
    tr = nb.node("ShaderNodeBsdfTransparent")
    tcol = nb.vmath("SCALE", (0.90, 0.99, 1.0),
                    scale=nb.math("SUBTRACT", 1.0, nb.math("MULTIPLY", fr.outputs[0], 0.64)))
    nb.put(tr.inputs["Color"], tcol)
    add = nb.node("ShaderNodeAddShader")
    nb.put(add.inputs[0], tr.outputs[0])
    nb.put(add.inputs[1], spec.outputs[0])
    nb.out(add.outputs[0])
    mat.surface_render_method = "BLENDED"
    return mat


@cached
def deep_water():
    mat = new_mat("M_DeepWater")
    nb = NB(mat)
    geo = nb.node("ShaderNodeNewGeometry")
    x = nb.sep(geo.outputs["Position"])[0]
    side = nb.maprange(x, -12.0, 12.0)
    col = nb.mix(side, (*OCEAN_DEEP, 1), (*LAGOON_DEEP, 1))
    p = nb.principled(Base_Color=col, Roughness=1.0)
    nb.put(p.inputs["Specular IOR Level"], 0.0)
    nb.out(p.outputs[0])
    return mat


@cached
def foam(name="M_Foam", density=0.55, scale=1.6):
    """White water ribbon: UV u across (fade at both edges), noise alpha."""
    mat = new_mat(name)
    nb = NB(mat)
    uvn = nb.node("ShaderNodeUVMap", uv_map="UVMap")
    s = nb.sep(uvn.outputs["UV"])
    edge = nb.math("SINE", nb.math("MULTIPLY", s[0], 3.14159))
    geo = nb.node("ShaderNodeNewGeometry")
    nz = nb.node("ShaderNodeTexNoise", noise_dimensions="3D")
    nb.put(nz.inputs["Vector"], geo.outputs["Position"])
    nb.put(nz.inputs["Scale"], scale)
    nb.put(nz.inputs["Detail"], 10.0)
    nb.put(nz.inputs["Roughness"], 0.65)
    m = nb.maprange(nb.math("MULTIPLY", nz.outputs["Fac"], edge), 1.0 - density - 0.05, 1.0 - density + 0.25)
    p = nb.principled(Base_Color=(0.92, 0.95, 0.95, 1), Roughness=0.45)
    nb.put(p.inputs["Subsurface Weight"], 0.2)
    tr = nb.node("ShaderNodeBsdfTransparent")
    mix = nb.node("ShaderNodeMixShader")
    nb.put(mix.inputs[0], m)
    nb.put(mix.inputs[1], tr.outputs[0])
    nb.put(mix.inputs[2], p.outputs[0])
    nb.out(mix.outputs[0])
    mat.surface_render_method = "BLENDED"
    return mat


@cached
def lineup_ground():
    mat = new_mat("M_LineupGround")
    nb = NB(mat)
    uvn = nb.node("ShaderNodeUVMap", uv_map="UVMap")
    v = nb.vmath("SCALE", uvn.outputs["UV"], scale=1.0 / 2.4)
    L = TEX / "terrain_layers"
    ch = nb.tex(L / "sand_dry_ch.png", v, packed=True)
    nm = nb.tex(L / "sand_dry_n.png", v, non_color=True)
    p = nb.principled(Base_Color=ch.outputs["Color"], Roughness=0.9, Normal=nb.normal_map(nm.outputs["Color"]))
    nb.out(p.outputs[0])
    return mat


@cached
def label():
    mat = new_mat("M_Label")
    nb = NB(mat)
    p = nb.principled(Base_Color=(0.05, 0.05, 0.05, 1), Roughness=0.6)
    nb.out(p.outputs[0])
    return mat
