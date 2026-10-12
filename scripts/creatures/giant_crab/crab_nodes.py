"""
Petit constructeur de nœuds de shader (Blender) pour les matériaux de cuisson du crabe géant v2.

Repris de factory/kits/artifacts/materials.py (2e PC, NodeBuilder et lin), sans la bibliothèque de scans photo :
la v2 du crabe est entièrement procédurale (bruits, Voronoi, attributs de sommet) puis cuite en textures.
Les nœuds sont créés par type (jamais cherchés par nom : les noms sont localisés).
"""
import bpy


def lin(h):
    """'#RRGGBB' (sRGB) -> RGBA linéaire."""
    h = h.lstrip("#")
    c = [int(h[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    c = [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]
    return (*c, 1.0)


class NodeBuilder:
    def __init__(self, nt):
        self.nt = nt

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    @staticmethod
    def _set(sock, v):
        if isinstance(v, bpy.types.NodeSocket):
            sock.node.id_data.links.new(v, sock)
        elif v is not None:
            sock.default_value = v

    def link(self, a, b):
        self.nt.links.new(a, b)

    def noise(self, vec, scale, detail=4.0, rough=0.55, distortion=0.0, lac=2.0):
        n = self.node("ShaderNodeTexNoise", noise_dimensions="3D")
        self._set(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        n.inputs["Distortion"].default_value = distortion
        n.inputs["Lacunarity"].default_value = lac
        return n.outputs["Factor"]

    def voronoi(self, vec, scale, rand=1.0, feature="F1", out="Distance"):
        n = self.node("ShaderNodeTexVoronoi", voronoi_dimensions="3D", feature=feature)
        self._set(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Randomness"].default_value = rand
        return n.outputs[out]

    def wave(self, vec, scale, distortion=0.0, detail=2.0, kind="BANDS", direction="DIAGONAL"):
        n = self.node("ShaderNodeTexWave", wave_type=kind)
        if kind == "BANDS":
            n.bands_direction = direction
        self._set(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Distortion"].default_value = distortion
        n.inputs["Detail"].default_value = detail
        return n.outputs["Factor"]

    def math(self, op, a, b=None, clamp=False):
        n = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
        self._set(n.inputs[0], a)
        if b is not None:
            self._set(n.inputs[1], b)
        return n.outputs[0]

    def add(self, *xs):
        out = xs[0]
        for x in xs[1:]:
            out = self.math("ADD", out, x)
        return out

    def mul(self, a, b):
        return self.math("MULTIPLY", a, b)

    def maprange(self, v, a0, a1, b0=0.0, b1=1.0, clamp=True):
        n = self.node("ShaderNodeMapRange", clamp=clamp)
        self._set(n.inputs["Value"], v)
        n.inputs["From Min"].default_value = a0
        n.inputs["From Max"].default_value = a1
        n.inputs["To Min"].default_value = b0
        n.inputs["To Max"].default_value = b1
        return n.outputs["Result"]

    def mix(self, fac, a, b):
        n = self.node("ShaderNodeMix", data_type="RGBA")
        self._set(n.inputs[0], fac)
        self._set(n.inputs[6], lin(a) if isinstance(a, str) else a)
        self._set(n.inputs[7], lin(b) if isinstance(b, str) else b)
        return n.outputs[2]

    def mul_col(self, a, b):
        n = self.node("ShaderNodeMix", data_type="RGBA", blend_type="MULTIPLY")
        n.inputs[0].default_value = 1.0
        self._set(n.inputs[6], a)
        self._set(n.inputs[7], lin(b) if isinstance(b, str) else b)
        return n.outputs[2]

    def mixf(self, fac, a, b):
        n = self.node("ShaderNodeMix", data_type="FLOAT")
        self._set(n.inputs[0], fac)
        self._set(n.inputs[2], a)
        self._set(n.inputs[3], b)
        return n.outputs[0]

    def attr_color(self, name):
        n = self.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name=name)
        return n.outputs["Color"], n.outputs["Alpha"]

    def attr_fac(self, name):
        n = self.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name=name)
        return n.outputs["Fac"]

    def sep(self, col):
        s = self.node("ShaderNodeSeparateColor")
        self._set(s.inputs["Color"], col)
        return s.outputs[0], s.outputs[1], s.outputs[2]

    def combine(self, r, g, b):
        c = self.node("ShaderNodeCombineColor")
        for i, v in enumerate((r, g, b)):
            self._set(c.inputs[i], v)
        return c.outputs[0]

    def gray(self, v):
        return self.combine(v, v, v)

    def texcoord(self, which="Object"):
        return self.node("ShaderNodeTexCoord").outputs[which]

    def vscale(self, vec, s):
        n = self.node("ShaderNodeVectorMath", operation="SCALE")
        self._set(n.inputs[0], vec)
        n.inputs["Scale"].default_value = s
        return n.outputs[0]

    def hsv(self, color, sat=1.0, val=1.0, hue=0.5):
        n = self.node("ShaderNodeHueSaturation")
        self._set(n.inputs["Color"], color)
        n.inputs["Hue"].default_value = hue
        n.inputs["Saturation"].default_value = sat
        n.inputs["Value"].default_value = val
        return n.outputs["Color"]

    def ao(self, distance, samples=16, only_local=True):
        n = self.node("ShaderNodeAmbientOcclusion", samples=samples, only_local=only_local)
        n.inputs["Distance"].default_value = distance
        return n.outputs["AO"]

    def bump(self, height, strength, distance, normal=None):
        n = self.node("ShaderNodeBump")
        n.inputs["Strength"].default_value = strength
        n.inputs["Distance"].default_value = distance
        self._set(n.inputs["Height"], height)
        if normal is not None:
            self._set(n.inputs["Normal"], normal)
        return n.outputs["Normal"]
