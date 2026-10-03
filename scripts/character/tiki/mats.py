"""Materials of the tiki creatures.

The reference sheets look like painted clay toys: every material keeps its palette colour, and the CC0 texture
sets (art/textures/characters, fetched by scripts/character/fetch_character_assets.py) bring the surface detail:
their luminance is gradient-mapped between a dark and a light palette colour, a little of their own colour is
kept (`keep`), and their height drives the bump. The images are box-projected on the Generated coordinates,
which kit.fixed_texspace() turns into plain meters: the detail sticks to the skin when the rig deforms it, and
the unwrap + bake for Unreal (export.py) reads the same shading.
"""
from pathlib import Path

import bpy

ART = Path(__file__).resolve().parents[3] / "art"
TEXDIR = ART / "textures"
REGISTRY = {}      # material name -> spec (filled by creatures.py)
_CACHE = {}


def srgb(hexa, a=1.0):
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    return [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c] + [a]


def sock(sockets, *keys):
    for key in keys:
        for s in sockets:
            if s.identifier == key:
                return s
        if key in sockets:
            return sockets[key]
    raise KeyError(keys)


def find_maps(folder):
    root = TEXDIR / folder
    files = sorted(root.glob("*")) if root.exists() else []

    def pick(*keys):
        for k in keys:
            for f in files:
                if k.lower() in f.name.lower():
                    return f
        return None

    return dict(color=pick("_color", "_diff_"), rough=pick("_roughness", "_rough_"),
                height=pick("_displacement", "_disp_"), emit=pick("_emission"), ao=pick("_ambientocclusion", "_ao_"))


def load_image(path, colorspace):
    key = (str(path), colorspace)
    if key in _CACHE:
        return _CACHE[key]
    img = bpy.data.images.load(str(path), check_existing=True)
    try:
        img.colorspace_settings.name = colorspace
    except TypeError:
        pass
    _CACHE[key] = img
    return img


class Graph:
    def __init__(self, nt):
        self.nt = nt
        self.x = -1800

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        n.location = (self.x, len(self.nt.nodes) * -60)
        self.x += 70
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.nt.links.new(a, b)

    def put(self, socket, v):
        if isinstance(v, (int, float, list, tuple)):
            socket.default_value = v
        else:
            self.link(v, socket)

    def gen(self):
        return self.node("ShaderNodeTexCoord").outputs["Generated"]

    def mapping(self, vec, scale, loc=(0.0, 0.0, 0.0)):
        n = self.node("ShaderNodeMapping")
        self.link(vec, n.inputs["Vector"])
        n.inputs["Scale"].default_value = scale
        n.inputs["Location"].default_value = loc
        return n.outputs["Vector"]

    def image(self, path, vec, colorspace="sRGB", blend=0.3):
        n = self.node("ShaderNodeTexImage")
        n.image = load_image(path, colorspace)
        n.projection = "BOX"
        n.projection_blend = blend
        n.interpolation = "Linear"
        self.link(vec, n.inputs["Vector"])
        return n.outputs["Color"]

    def bw(self, col):
        n = self.node("ShaderNodeRGBToBW")
        self.link(col, n.inputs[0])
        return n.outputs[0]

    def mix(self, fac, a, b, blend="MIX"):
        n = self.node("ShaderNodeMix", data_type="RGBA", blend_type=blend)
        self.put(sock(n.inputs, "Factor_Float"), fac)
        self.put(sock(n.inputs, "A_Color"), a)
        self.put(sock(n.inputs, "B_Color"), b)
        return sock(n.outputs, "Result_Color")

    def math(self, op, a, b=0.0, c=0.0):
        n = self.node("ShaderNodeMath", operation=op)
        for i, v in enumerate((a, b, c)):
            self.put(n.inputs[i], v)
        return n.outputs[0]

    def noise(self, vec, scale, detail=3.0, rough=0.55, distortion=0.0):
        n = self.node("ShaderNodeTexNoise")
        self.link(vec, n.inputs["Vector"])
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distortion
        return n.outputs["Fac"]

    def voronoi(self, vec, scale, feature="F1", output="Distance"):
        n = self.node("ShaderNodeTexVoronoi")
        try:
            n.feature = feature
        except TypeError:
            pass
        self.link(vec, n.inputs["Vector"])
        n.inputs["Scale"].default_value = scale
        return n.outputs[output]

    def ramp(self, fac, stops):
        n = self.node("ShaderNodeValToRGB")
        self.link(fac, n.inputs["Fac"])
        els = n.color_ramp.elements
        while len(els) < len(stops):
            els.new(0.5)
        for e, (pos, col) in zip(els, stops):
            e.position = pos
            e.color = col if len(col) == 4 else list(col) + [1.0]
        return n.outputs["Color"]


def _bsdf_inputs(bsdf, base, rough, metal=0.0):
    sock(bsdf.inputs, "Base Color").default_value = base
    sock(bsdf.inputs, "Roughness").default_value = rough if isinstance(rough, float) else 0.6
    sock(bsdf.inputs, "Metallic").default_value = metal


def make(name):
    """Create (once) the material `name` from REGISTRY."""
    mat = bpy.data.materials.get(name)
    if mat:
        return mat
    spec = REGISTRY[name]
    kind = spec["kind"]
    mat = bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    G = Graph(nt)
    out = G.node("ShaderNodeOutputMaterial")
    bsdf = G.node("ShaderNodeBsdfPrincipled")
    G.link(bsdf.outputs[0], out.inputs["Surface"])
    fallback = srgb(spec.get("light") or spec.get("base") or "808080")
    _bsdf_inputs(bsdf, fallback, spec.get("rough", 0.6) if isinstance(spec.get("rough", 0.6), float) else 0.6)
    gen = G.gen()
    height, bump_k = None, spec.get("bump", 0.3)

    if kind in ("tex", "lava"):
        maps = find_maps(spec["set"])
        if not maps["color"]:
            raise FileNotFoundError(f"texture set {spec['set']} missing: run fetch_character_assets.py")
        d = spec.get("density", 2.0)
        vec = G.mapping(gen, (d, d, d * spec.get("stretch", 1.0)), loc=spec.get("offset", (0.0, 0.0, 0.0)))
        c = G.image(maps["color"], vec, "sRGB")
        lum = G.bw(c)
        lo, hi = spec.get("range", (0.05, 0.6))
        grad = G.ramp(lum, [(lo, srgb(spec["dark"])), (hi, srgb(spec["light"]))])
        col = G.mix(spec.get("keep", 0.15), grad, c)
        if spec.get("tone"):  # large-scale tone variation (hand painted look)
            tone = G.ramp(G.noise(gen, spec.get("tone_scale", 3.0), 2.0),
                          [(0.3, [1 - spec["tone"]] * 3), (0.7, [1 + spec["tone"] * 0.5] * 3)])
            col = G.mix(1.0, col, tone, "MULTIPLY")
        if maps["rough"]:
            r = G.bw(G.image(maps["rough"], vec, "Non-Color"))
            r0, r1 = spec.get("rough", (0.55, 0.95)) if isinstance(spec.get("rough"), tuple) else (0.5, 0.9)
            rough = G.math("MULTIPLY_ADD", r, r1 - r0, r0)
        else:
            rough = spec.get("rough", 0.7) if isinstance(spec.get("rough"), float) else 0.7
        height = G.bw(G.image(maps["height"], vec, "Non-Color")) if maps["height"] else lum
        G.link(col, sock(bsdf.inputs, "Base Color"))
        G.put(sock(bsdf.inputs, "Roughness"), rough)
        if kind == "lava" and maps["emit"]:
            e = G.image(maps["emit"], vec, "sRGB")
            e = G.mix(1.0, e, srgb(spec["glow"]), "MULTIPLY")
            G.link(e, sock(bsdf.inputs, "Emission Color", "Emission"))
            sock(bsdf.inputs, "Emission Strength").default_value = spec.get("strength", 4.0)
    elif kind == "clay":
        tone = G.ramp(G.noise(gen, spec.get("tone_scale", 6.0), 3.0),
                      [(0.25, [1 - spec.get("tone", 0.1)] * 3), (0.75, [1 + spec.get("tone", 0.1) * 0.4] * 3)])
        col = G.mix(1.0, srgb(spec["base"]), tone, "MULTIPLY")
        if spec.get("spots"):  # darker blotches (crab shell, mottled skin)
            sp = G.ramp(G.noise(gen, spec.get("spot_scale", 14.0), 4.0, 0.6),
                        [(0.55, (0, 0, 0)), (0.68, (1, 1, 1))])
            col = G.mix(G.math("MULTIPLY", sp, spec.get("spot_amt", 0.6)), col, srgb(spec["spots"]))
        G.link(col, sock(bsdf.inputs, "Base Color"))
        sock(bsdf.inputs, "Roughness").default_value = spec.get("rough", 0.5)
        fine = G.noise(gen, spec.get("grain", 140.0), 2.0)
        height = fine
        if spec.get("pits"):  # pitted shell: voronoi craters
            pits = G.voronoi(gen, spec["pits"])
            height = G.math("MULTIPLY_ADD", G.math("MINIMUM", G.math("MULTIPLY", pits, 6.0), 1.0), 0.8,
                            G.math("MULTIPLY", fine, 0.2))
        if spec.get("sss"):
            try:
                sock(bsdf.inputs, "Subsurface Weight").default_value = spec["sss"]
            except KeyError:
                pass
    elif kind == "emit":
        sock(bsdf.inputs, "Base Color").default_value = srgb(spec.get("dark", "101010"))
        sock(bsdf.inputs, "Emission Strength").default_value = spec.get("strength", 5.0)
        sock(bsdf.inputs, "Roughness").default_value = 0.3
        if spec.get("vary"):  # only some cracks glow, with hot spots
            fac = G.ramp(G.noise(gen, spec["vary"], 3.0, 0.6), [(0.38, (0, 0, 0)), (0.62, (1, 1, 1))])
            G.link(G.mix(fac, srgb(spec.get("dark", "101010")), srgb(spec["base"])),
                   sock(bsdf.inputs, "Emission Color", "Emission"))
        else:
            sock(bsdf.inputs, "Emission Color", "Emission").default_value = srgb(spec["base"])
    elif kind == "leaf":
        vc = G.node("ShaderNodeVertexColor", layer_name="Col").outputs["Color"]
        vein = G.ramp(G.noise(gen, 40.0, 2.0, 0.5, 1.0), [(0.35, (0.82, 0.82, 0.82)), (0.65, (1.06, 1.06, 1.06))])
        G.link(G.mix(1.0, vc, vein, "MULTIPLY"), sock(bsdf.inputs, "Base Color"))
        sock(bsdf.inputs, "Roughness").default_value = spec.get("rough", 0.55)
        height = G.noise(gen, 90.0, 2.0)
        try:
            sock(bsdf.inputs, "Subsurface Weight").default_value = 0.08
        except KeyError:
            pass
    elif kind == "flat":
        sock(bsdf.inputs, "Base Color").default_value = srgb(spec["base"])
        sock(bsdf.inputs, "Roughness").default_value = spec.get("rough", 0.5)
        sock(bsdf.inputs, "Metallic").default_value = spec.get("metal", 0.0)
    else:
        raise ValueError(kind)

    if height is not None and bump_k:
        bump = G.node("ShaderNodeBump")
        bump.inputs["Strength"].default_value = bump_k
        bump.inputs["Distance"].default_value = spec.get("bump_dist", 0.004)
        G.link(height, bump.inputs["Height"])
        G.link(bump.outputs["Normal"], sock(bsdf.inputs, "Normal"))
    mat.diffuse_color = fallback
    mat["tiki_kind"] = kind
    return mat
