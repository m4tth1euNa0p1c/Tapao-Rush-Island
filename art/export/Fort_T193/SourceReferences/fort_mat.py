"""
Fort francais : materiaux MAT-Fort_* (scans photo CC0 2K / 1K) + usure procedurale EEVEE.

Maconneries : deux materiaux a couches (melange par l'attribut de couleur de coin `Blend`, exporte FBX / glTF COLOR_0),
pour tenir la regle « 4 materiaux max par module » sans perdre de variete :
  - MAT-Fort_CoralRubble : moellons de corail beiges (coral_fort_wall_02) ; Blend.R -> pierres de lest sombres
    (coral_fort_wall_03, soubassements, quais) ; Blend.G -> enduit de chaux blanc casse (white_plaster_02) tombe par
    plaques (laisse voir les moellons) ;
  - MAT-Fort_CoralAshlar : pierre de taille corallienne (coral_fort_wall_01, eclaircie) ; Blend.B -> dalles de
    beachrock (coral_ground_02 : chemins de ronde, marches, couronnements).
Usure (toutes les surfaces exterieures, pas de cuisson) : salissures et mousse en pied de mur (hauteur au-dessus des
niveaux de sol du fort), coulures verticales, efflorescences de sel, lichens orange sur les faces nord et les dessus,
variation macro (casse la repetition des scans). Recettes Unreal : propriete `ue_recipe` de chaque materiau.
"""
import math
import os

import bpy

import bean_kit as K

ROOT = K.ROOT
ART = os.path.join(ROOT, "art", "textures")
FAC = os.path.join(ROOT, "factory", "assets", "textures")
TEX = {
    "rubble": os.path.join(FAC, "artifacts", "coral_fort_wall_02"),
    "ashlar": os.path.join(FAC, "artifacts", "coral_fort_wall_01"),
    "ballast": os.path.join(FAC, "artifacts", "coral_fort_wall_03"),
    "beachrock": os.path.join(ART, "ground", "coral_ground_02"),
    "plaster": os.path.join(FAC, "fort", "white_plaster_02"),
    "brick": os.path.join(FAC, "fort", "red_brick_03"),
    "tiles": os.path.join(FAC, "fort", "clay_roof_tiles_02"),
    "oak": os.path.join(FAC, "ship", "dark_wooden_planks"),
    "oak2": os.path.join(ART, "characters", "weathered_planks"),
    "tamanu": os.path.join(FAC, "ship", "brown_planks_03"),
    "coconut": os.path.join(ART, "palm", "palm_tree_bark"),
    "rust": os.path.join(FAC, "ship", "rusty_metal_04"),
    "iron": os.path.join(FAC, "ship", "Metal046B"),
    "copper": os.path.join(FAC, "ship", "Metal048C"),
    "canvas": os.path.join(ART, "characters", "Fabric019"),
    "jute": os.path.join(ART, "characters", "hessian_230"),
    "rope": os.path.join(ART, "characters", "Rope001"),
    "water_n": os.path.join(ART, "water", "T_Water_N.png"),
    "gravel": os.path.join(ART, "ground", "coral_gravel"),
    "barrel": os.path.join(FAC, "ship", "wooden_barrel"),
    "shingle": os.path.join(ART, "characters", "weathered_planks"),
}

UE = {
    "MAT-Fort_CoralRubble": "Default Lit. 3 couches par VertexColor (Blend) : base T_CoralFortWall02 (moellons), R -> "
                            "T_CoralFortWall03 (lest), G -> T_WhitePlaster02 x #E6E0D2 avec masque de trous (bruit monde) "
                            "laissant voir la base. Usure : MF_FortWeathering (WorldPosition.z - niveau de sol -> salissure "
                            "brun-noir + mousse 0..0,55 m ; coulures = bruit etire en Z ; sel = Voronoi clair ; lichen = "
                            "VertexNormalWS.y > 0,4). Normales OpenGL : Flip Green.",
    "MAT-Fort_CoralAshlar": "Default Lit. Base T_CoralFortWall01 eclaircie, B (VertexColor) -> T_CoralGround02 "
                            "(beachrock). MF_FortWeathering. Flip Green.",
}


# ----------------------------------------------------------------------------
# Constructeur de noeuds

class NB:
    def __init__(self, mat):
        mat.use_nodes = True
        self.m = mat
        self.nt = mat.node_tree
        for n in list(self.nt.nodes):
            self.nt.nodes.remove(n)
        self.out = self.node("ShaderNodeOutputMaterial")
        self.bsdf = self.node("ShaderNodeBsdfPrincipled")
        self.link(self.bsdf.outputs["BSDF"], self.out.inputs["Surface"])
        self._geo = None
        self._uv = {}

    def node(self, t, **kw):
        n = self.nt.nodes.new(t)
        for k, v in kw.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.nt.links.new(a, b)

    def _in(self, sock, v):
        if isinstance(v, bpy.types.NodeSocket):
            self.link(v, sock)
        elif v is not None:
            sock.default_value = v

    def math(self, op, a, b=None, clamp=False):
        n = self.node("ShaderNodeMath", operation=op, use_clamp=clamp)
        self._in(n.inputs[0], a)
        if b is not None:
            self._in(n.inputs[1], b)
        return n.outputs[0]

    def mr(self, v, a, b, c=0.0, d=1.0, clamp=True, smooth=False):
        n = self.node("ShaderNodeMapRange", clamp=clamp)
        if smooth:
            n.interpolation_type = "SMOOTHSTEP"
        self._in(n.inputs["Value"], v)
        n.inputs["From Min"].default_value = a
        n.inputs["From Max"].default_value = b
        n.inputs["To Min"].default_value = c
        n.inputs["To Max"].default_value = d
        return n.outputs["Result"]

    def mix(self, fac, a, b, blend="MIX"):
        n = self.node("ShaderNodeMix", data_type="RGBA", blend_type=blend, clamp_factor=True)
        self._in(n.inputs[0], fac)
        self._in(n.inputs[6], a if not isinstance(a, tuple) or len(a) == 4 else (*a, 1.0))
        self._in(n.inputs[7], b if not isinstance(b, tuple) or len(b) == 4 else (*b, 1.0))
        return n.outputs[2]

    def mixf(self, fac, a, b):
        n = self.node("ShaderNodeMix", data_type="FLOAT", clamp_factor=True)
        self._in(n.inputs[0], fac)
        self._in(n.inputs[2], a)
        self._in(n.inputs[3], b)
        return n.outputs[0]

    def mixv(self, fac, a, b):
        n = self.node("ShaderNodeMix", data_type="VECTOR", clamp_factor=True)
        self._in(n.inputs[0], fac)
        self._in(n.inputs[4], a)
        self._in(n.inputs[5], b)
        return n.outputs[1]

    def geo(self):
        if self._geo is None:
            self._geo = self.node("ShaderNodeNewGeometry")
        return self._geo

    def sep(self, vec):
        n = self.node("ShaderNodeSeparateXYZ")
        self.link(vec, n.inputs[0])
        return n.outputs

    def vscale(self, vec, s):
        n = self.node("ShaderNodeVectorMath", operation="MULTIPLY")
        self.link(vec, n.inputs[0])
        n.inputs[1].default_value = s if isinstance(s, (tuple, list)) else (s, s, s)
        return n.outputs[0]

    def noise(self, vec, scale, detail=4.0, rough=0.55, dist=0.0):
        n = self.node("ShaderNodeTexNoise")
        self._in(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        n.inputs["Detail"].default_value = detail
        n.inputs["Roughness"].default_value = rough
        if dist:
            n.inputs["Distortion"].default_value = dist
        return n.outputs["Fac"]

    def voronoi(self, vec, scale, out="Distance", feature="F1"):
        n = self.node("ShaderNodeTexVoronoi", feature=feature)
        self._in(n.inputs["Vector"], vec)
        n.inputs["Scale"].default_value = scale
        return n.outputs[out]

    def attr(self, name):
        n = self.node("ShaderNodeAttribute", attribute_type="GEOMETRY", attribute_name=name)
        return n

    def uv(self, tile, rot=0.0):
        key = (round(tile, 4), round(rot, 3))
        if key not in self._uv:
            tc = self.node("ShaderNodeUVMap", uv_map="UVMap")
            mp = self.node("ShaderNodeMapping")
            mp.inputs["Scale"].default_value = (1 / tile, 1 / tile, 1)
            mp.inputs["Rotation"].default_value = (0, 0, rot)
            self.link(tc.outputs["UV"], mp.inputs["Vector"])
            self._uv[key] = mp.outputs["Vector"]
        return self._uv[key]

    def img(self, path, vec, color=True):
        n = self.node("ShaderNodeTexImage")
        n.image = bpy.data.images.load(path, check_existing=True)
        n.image.colorspace_settings.name = "sRGB" if color else "Non-Color"
        self._in(n.inputs["Vector"], vec)
        return n

    def set(self, name, v):
        if name in self.bsdf.inputs:
            self._in(self.bsdf.inputs[name], v)


def _find(folder, keys):
    if not os.path.isdir(folder):
        return None
    for f in sorted(os.listdir(folder)):
        low = f.lower()
        if any(k in low for k in keys) and low.endswith((".jpg", ".png")):
            return os.path.join(folder, f)
    return None


def texset(folder):
    return {
        "diff": _find(folder, ("_diff", "_color", "basecolor")),
        "rough": _find(folder, ("_rough", "roughness")),
        "nor": _find(folder, ("nor_gl", "normalgl")),
        "disp": _find(folder, ("_disp", "displacement")),
        "metal": _find(folder, ("_metal", "metalness")),
    }


class Layer:
    """Une couche de scan (couleur, rugosite, normale, hauteur) sur un jeu d'UV."""

    def __init__(self, nb, key, tile, tint=(1, 1, 1), sat=1.0, bright=1.0, rot=0.0, rough_add=0.0, disp=False):
        ts = texset(TEX[key])
        vec = nb.uv(tile, rot)
        d = nb.img(ts["diff"], vec, True)
        c = d.outputs["Color"]
        if sat != 1.0 or bright != 1.0:
            hs = nb.node("ShaderNodeHueSaturation")
            hs.inputs["Saturation"].default_value = sat
            hs.inputs["Value"].default_value = bright
            nb.link(c, hs.inputs["Color"])
            c = hs.outputs["Color"]
        if tuple(tint) != (1, 1, 1):
            c = nb.mix(1.0, c, (*tint, 1.0), "MULTIPLY")
        self.col = c
        self.rough = nb.math("ADD", nb.img(ts["rough"], vec, False).outputs["Color"], rough_add, True) \
            if ts["rough"] else 0.8 + rough_add
        self.nor = nb.img(ts["nor"], vec, False).outputs["Color"] if ts["nor"] else (0.5, 0.5, 1.0, 1.0)
        self.h = nb.img(ts["disp"], vec, False).outputs["Color"] if (disp and ts["disp"]) else None
        self.metal = nb.img(ts["metal"], vec, False).outputs["Color"] if ts.get("metal") else None


def _finish_normal(nb, nor_color, strength=1.0, height=None, bump=0.0):
    nm = nb.node("ShaderNodeNormalMap")
    nm.inputs["Strength"].default_value = strength
    nb._in(nm.inputs["Color"], nor_color)
    n = nm.outputs["Normal"]
    if height is not None and bump:
        b = nb.node("ShaderNodeBump")
        b.inputs["Strength"].default_value = bump
        b.inputs["Distance"].default_value = 0.02
        nb._in(b.inputs["Height"], height)
        nb.link(n, b.inputs["Normal"])
        n = b.outputs["Normal"]
    nb.link(n, nb.bsdf.inputs["Normal"])


def ground_height(nb):
    """Hauteur au-dessus du sol au pied de la piece : P.z - (Location.z + base) ; base = (Blend.A - 0,5) x 10
    (attribut de coin ecrit par fort_geo.FB ; 0 = sol local du module si absent)."""
    if getattr(nb, "_h", None) is not None:
        return nb._h
    P = nb.geo().outputs["Position"]
    _px, _py, pz = nb.sep(P)
    oi = nb.node("ShaderNodeObjectInfo")
    _ox, _oy, oz = nb.sep(oi.outputs["Location"])
    a = nb.attr("Blend").outputs["Alpha"]
    base = nb.math("MULTIPLY", nb.math("SUBTRACT", a, 0.5), 10.0)
    nb._h = nb.math("SUBTRACT", pz, nb.math("ADD", oz, base))
    return nb._h


def weathering(nb, col, rough, strength=1.0, salt=True, moss=True, lichen=True, streaks=True, foot_h=0.6):
    """Usure d'exterieur (EEVEE, coordonnees monde) ; renvoie (couleur, rugosite, masque de pied)."""
    g = nb.geo()
    P = g.outputs["Position"]
    N = g.outputs["Normal"]
    nx, ny, nz = nb.sep(N)
    vert = nb.math("SUBTRACT", 1.0, nb.math("ABSOLUTE", nz))
    vert = nb.mr(vert, 0.25, 0.8)
    top = nb.mr(nz, 0.55, 0.9)
    h = ground_height(nb)
    # variation macro (casse la repetition des scans) : taches claires / sombres a l'echelle de 2-6 m
    mac = nb.noise(nb.vscale(P, 0.22), 1.0, 4.0, 0.55)
    col = nb.mix(1.0, col, nb.mix(nb.mr(mac, 0.32, 0.68), (0.62, 0.60, 0.56), (1.08, 1.06, 1.0)), "MULTIPLY")
    # pied de mur : salissure brun-noir + humidite
    edge = nb.noise(nb.vscale(P, (1.6, 1.6, 0.9)), 2.5, 5.0, 0.6)
    foot = nb.math("MULTIPLY", nb.mr(h, 0.0, foot_h, 1.0, 0.0, smooth=True), nb.math("GREATER_THAN", h, -0.06))
    footm = nb.math("MULTIPLY", nb.math("MULTIPLY", foot, vert), nb.mr(edge, 0.3, 0.62, 0.45, 1.3))
    footm = nb.math("MULTIPLY", nb.math("MINIMUM", footm, 1.0), strength)
    col = nb.mix(nb.math("MULTIPLY", footm, 0.8), col, (0.10, 0.085, 0.06), "MULTIPLY")
    rough = nb.math("ADD", rough, nb.math("MULTIPLY", footm, 0.12), True)
    if moss:
        mn = nb.noise(nb.vscale(P, 1.0), 5.0, 6.0, 0.65)
        mm = nb.math("MULTIPLY", nb.math("MAXIMUM", nb.math("MULTIPLY", footm, 1.0), nb.math("MULTIPLY", top, 0.6)),
                     nb.mr(mn, 0.45, 0.62))
        mm = nb.math("MULTIPLY", mm, strength)
        mcol = nb.mix(nb.mr(nb.noise(P, 14.0, 3.0), 0.3, 0.7), (0.06, 0.075, 0.03), (0.17, 0.18, 0.07))
        col = nb.mix(nb.math("MULTIPLY", mm, 0.9), col, mcol)
        rough = nb.math("ADD", rough, nb.math("MULTIPLY", mm, 0.15), True)
    if streaks:
        # coulures noires (cyanobacteries) : bruit etire en Z
        sn = nb.noise(nb.vscale(P, (2.6, 2.6, 0.16)), 1.0, 4.0, 0.55)
        sn2 = nb.noise(nb.vscale(P, (9.0, 9.0, 0.5)), 1.0, 2.0, 0.5)
        sm = nb.math("MULTIPLY", nb.mr(sn, 0.48, 0.7), nb.mr(sn2, 0.35, 0.6, 0.4, 1.0))
        sm = nb.math("MULTIPLY", nb.math("MULTIPLY", sm, vert), 0.85 * strength)
        col = nb.mix(sm, col, (0.16, 0.15, 0.13), "MULTIPLY")
        rough = nb.math("ADD", rough, nb.math("MULTIPLY", sm, 0.08), True)
    if salt:
        band = nb.math("MULTIPLY", nb.mr(h, 0.08, 0.3), nb.mr(h, 0.5, 1.3, 1.0, 0.0))
        vs = nb.voronoi(nb.vscale(P, (1.0, 1.0, 1.6)), 9.0)
        sp = nb.mr(nb.noise(nb.vscale(P, 1.2), 3.0, 6.0, 0.7), 0.53, 0.68)
        sm = nb.math("MULTIPLY", nb.math("MULTIPLY", band, sp), nb.mr(vs, 0.05, 0.35, 1.0, 0.3))
        sm = nb.math("MULTIPLY", nb.math("MULTIPLY", sm, vert), 0.7 * strength)
        col = nb.mix(sm, col, (0.80, 0.78, 0.72))
        rough = nb.math("ADD", rough, nb.math("MULTIPLY", sm, 0.25), True)
    if lichen:
        north = nb.math("MAXIMUM", nb.mr(ny, 0.3, 0.8), nb.math("MULTIPLY", top, 0.7))
        lv = nb.voronoi(nb.vscale(P, 1.0), 26.0)
        ln = nb.noise(nb.vscale(P, 1.0), 2.2, 4.0, 0.6)
        lc = nb.mr(nb.noise(nb.vscale(P, 0.35), 1.0, 3.0, 0.5), 0.55, 0.68)
        lm = nb.math("MULTIPLY", nb.math("MULTIPLY", north, nb.mr(lv, 0.30, 0.10)), nb.mr(ln, 0.5, 0.66))
        lm = nb.math("MULTIPLY", nb.math("MULTIPLY", lm, lc), 0.6 * strength)
        lcol = nb.mix(nb.mr(nb.noise(P, 40.0), 0.4, 0.6), (0.60, 0.33, 0.10), (0.55, 0.55, 0.48))
        col = nb.mix(lm, col, lcol)
    # salissure des dessus (poussiere, sable, feuilles) en taches
    dn = nb.noise(nb.vscale(P, 0.8), 3.0, 5.0, 0.6)
    dm = nb.math("MULTIPLY", top, nb.mr(dn, 0.5, 0.72, 0.0, 0.45 * strength))
    col = nb.mix(dm, col, (0.42, 0.37, 0.29))
    return col, rough, footm


def _mat(name):
    m = bpy.data.materials.get(name)
    if m is not None:
        return m, None
    m = bpy.data.materials.new(name)
    return m, NB(m)


def _tag(m, recipe, color):
    m["ue_recipe"] = recipe
    m.diffuse_color = (*color, 1.0)
    return m


# ----------------------------------------------------------------------------
# Maconneries a couches

def coral_rubble():
    m, nb = _mat("MAT-Fort_CoralRubble")
    if nb is None:
        return m
    bl = nb.attr("Blend")
    sp = nb.node("ShaderNodeSeparateColor")
    nb.link(bl.outputs["Color"], sp.inputs["Color"])
    r, gch = sp.outputs["Red"], sp.outputs["Green"]
    base = Layer(nb, "rubble", 0.68, tint=(1.0, 0.97, 0.92), sat=0.85, disp=True)
    bal = Layer(nb, "ballast", 0.62, tint=(0.95, 0.95, 0.97), sat=0.7, disp=True)
    pla = Layer(nb, "plaster", 1.1, tint=(0.90, 0.86, 0.78), sat=0.6, disp=True)
    # chaux vieillie : voile ocre / gris irregulier
    pv = nb.noise(nb.vscale(nb.geo().outputs["Position"], 0.7), 2.0, 5.0, 0.6)
    pla.col = nb.mix(nb.mr(pv, 0.35, 0.7), nb.mix(1.0, pla.col, (0.80, 0.74, 0.62, 1.0), "MULTIPLY"),
                     nb.mix(1.0, pla.col, (0.86, 0.86, 0.84, 1.0), "MULTIPLY"))
    P = nb.geo().outputs["Position"]
    # enduit tombe : plaques (bruit monde) ; davantage en pied de mur et sur les aretes
    hn = nb.noise(nb.vscale(P, (1.0, 1.0, 1.3)), 0.75, 9.0, 0.68, 0.25)
    region = nb.noise(nb.vscale(P, 0.11), 1.0, 2.0, 0.5)
    _px, _py, pz = nb.sep(P)
    low = nb.mr(pz, 0.2, 1.2, 0.06, 0.0)
    hn = nb.math("SUBTRACT", hn, nb.math("MULTIPLY", nb.math("SUBTRACT", region, 0.5), 0.45))
    holes = nb.mr(nb.math("ADD", hn, low), 0.535, 0.56)              # 1 = enduit tombe (zones irregulieres)
    render = nb.math("MULTIPLY", gch, nb.math("SUBTRACT", 1.0, holes))
    col = nb.mix(r, base.col, bal.col)
    rough = nb.mixf(r, base.rough, bal.rough)
    nor = nb.mix(r, base.nor, bal.nor)
    hgt = nb.mixf(r, base.h, bal.h)
    col = nb.mix(render, col, pla.col)
    rough = nb.mixf(render, rough, nb.math("ADD", pla.rough, 0.1, True))
    nor = nb.mix(render, nor, pla.nor)
    # epaisseur de l'enduit (bord des plaques) dans la hauteur de bosse
    hgt = nb.mixf(gch, hgt, nb.math("ADD", nb.math("MULTIPLY", nb.math("SUBTRACT", 1.0, holes), 0.9),
                                      nb.math("MULTIPLY", pla.h, 0.1)))
    # bord de plaque sali (joint ombre)
    rim = nb.math("MULTIPLY", gch, nb.mr(nb.math("ADD", hn, low), 0.515, 0.535, 0.0, 1.0))
    rim = nb.math("MULTIPLY", rim, nb.math("SUBTRACT", 1.0, holes))
    col = nb.mix(nb.math("MULTIPLY", rim, 0.5), col, (0.45, 0.42, 0.36), "MULTIPLY")
    col, rough, _ = weathering(nb, col, rough)
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], rough)
    _finish_normal(nb, nor, 1.0, hgt, 0.35)
    return _tag(m, UE["MAT-Fort_CoralRubble"], (0.72, 0.66, 0.56))


def coral_ashlar():
    m, nb = _mat("MAT-Fort_CoralAshlar")
    if nb is None:
        return m
    bl = nb.attr("Blend")
    sp = nb.node("ShaderNodeSeparateColor")
    nb.link(bl.outputs["Color"], sp.inputs["Color"])
    b = sp.outputs["Blue"]
    base = Layer(nb, "ashlar", 0.6, tint=(1.0, 0.95, 0.85), sat=0.6, bright=1.0, disp=True)
    br = Layer(nb, "beachrock", 1.5, tint=(0.95, 0.93, 0.88), sat=0.45, bright=1.05, disp=True)
    col = nb.mix(b, base.col, br.col)
    rough = nb.mixf(b, base.rough, br.rough)
    nor = nb.mix(b, base.nor, br.nor)
    hgt = nb.mixf(b, base.h, br.h)
    col, rough, _ = weathering(nb, col, rough, strength=0.85)
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], rough)
    _finish_normal(nb, nor, 1.0, hgt, 0.3)
    return _tag(m, UE["MAT-Fort_CoralAshlar"], (0.80, 0.77, 0.70))


def simple(name, key, tile, tint=(1, 1, 1), sat=1.0, bright=1.0, rough_add=0.0, metal=None, weather=0.0,
           nstr=1.0, bump=0.0, rot=0.0, color=(0.5, 0.5, 0.5), recipe="", mix=None):
    """Un scan + usure optionnelle ; mix = (couleur, facteur) fondu vers une teinte unie."""
    m, nb = _mat(name)
    if nb is None:
        return m
    L = Layer(nb, key, tile, tint=tint, sat=sat, bright=bright, rot=rot, rough_add=rough_add, disp=bump > 0)
    col, rough = L.col, L.rough
    if mix:
        col = nb.mix(mix[1], col, (*mix[0], 1.0))
    if weather:
        col, rough, _ = weathering(nb, col, rough, strength=weather, salt=False)
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], rough)
    if metal is not None:
        if metal == "map" and L.metal is not None:
            nb.link(L.metal, nb.bsdf.inputs["Metallic"])
        else:
            nb.bsdf.inputs["Metallic"].default_value = float(metal) if metal != "map" else 1.0
    _finish_normal(nb, L.nor, nstr, L.h, bump)
    return _tag(m, recipe or f"Default Lit : T_{key} (tile {tile} m), teinte {tint}.", color)


def roof_tiles():
    """Tuiles canal delavees : variation par tuile (bruit en UV), lichens gris-blanc, mousse sombre en coulures."""
    m, nb = _mat("MAT-Fort_RoofTiles")
    if nb is None:
        return m
    L = Layer(nb, "tiles", 1.45, tint=(0.95, 0.80, 0.68), sat=0.78, disp=True)
    uvn = nb.uv(1.45)
    tv = nb.noise(nb.vscale(uvn, (9.0, 3.0, 1.0)), 1.0, 1.0, 0.3)
    col = nb.mix(1.0, L.col, nb.mix(nb.mr(tv, 0.3, 0.7), (0.62, 0.58, 0.55, 1.0), (1.12, 1.02, 0.92, 1.0)), "MULTIPLY")
    P = nb.geo().outputs["Position"]
    big = nb.mr(nb.noise(nb.vscale(P, 0.25), 1.0, 4.0, 0.6), 0.45, 0.62)
    col = nb.mix(nb.math("MULTIPLY", big, 0.45), col, (0.30, 0.25, 0.21, 1.0))
    li = nb.math("MULTIPLY", nb.mr(nb.voronoi(nb.vscale(P, 1.0), 14.0), 0.28, 0.08),
                 nb.mr(nb.noise(nb.vscale(P, 0.6), 2.0, 4.0, 0.6), 0.5, 0.64))
    col = nb.mix(nb.math("MULTIPLY", li, 0.35), col, (0.62, 0.60, 0.54, 1.0))
    st = nb.mr(nb.noise(nb.vscale(P, (2.5, 2.5, 0.3)), 1.0, 3.0, 0.5), 0.55, 0.72)
    col = nb.mix(nb.math("MULTIPLY", st, 0.5), col, (0.12, 0.12, 0.08, 1.0), "MULTIPLY")
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], nb.math("ADD", L.rough, nb.math("MULTIPLY", li, 0.2), True))
    _finish_normal(nb, L.nor, 1.0, L.h, 0.5)
    return _tag(m, "Default Lit : T_ClayRoofTiles02 (canal le long de V, tile 1,45 m) ; variation par tuile (bruit UV) ; "
                   "plaques sombres, lichens gris-blanc (Voronoi), coulures de mousse.", (0.5, 0.3, 0.2))


def iron_rust():
    """Fer forge rouille : rusty_metal_04 assombri vers le gris-noir, coulures oranges en creux (1K)."""
    m, nb = _mat("MAT-Fort_IronRust")
    if nb is None:
        return m
    L = Layer(nb, "rust", 0.45, tint=(0.95, 0.88, 0.80), sat=0.75)
    P = nb.geo().outputs["Position"]
    n = nb.noise(nb.vscale(P, 1.0), 9.0, 5.0, 0.6)
    col = nb.mix(0.55, L.col, (0.10, 0.085, 0.075, 1.0))
    col = nb.mix(nb.mr(n, 0.55, 0.7, 0.0, 0.55), col, (0.36, 0.15, 0.05, 1.0))
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], nb.math("ADD", L.rough, 0.15, True))
    nb.bsdf.inputs["Metallic"].default_value = 0.35
    _finish_normal(nb, L.nor, 1.0)
    return _tag(m, "Default Lit : T_RustyMetal04 x Lerp(#1A1513, 0.55) + taches orange (bruit) ; Metallic 0.35 ; "
                   "Roughness T_Rough + 0.15.", (0.12, 0.10, 0.09))


def copper():
    m, nb = _mat("MAT-Fort_Copper")
    if nb is None:
        return m
    L = Layer(nb, "copper", 0.35, tint=(0.78, 0.40, 0.26), sat=1.0, bright=0.75)
    P = nb.geo().outputs["Position"]
    n = nb.noise(nb.vscale(P, 1.0), 18.0, 5.0, 0.6)
    vg = nb.mr(n, 0.6, 0.72)
    col = nb.mix(vg, L.col, (0.25, 0.42, 0.33, 1.0))
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], nb.mixf(vg, nb.math("ADD", L.rough, 0.1, True), 0.85))
    nb._in(nb.bsdf.inputs["Metallic"], nb.math("SUBTRACT", 1.0, vg))
    _finish_normal(nb, L.nor, 0.8)
    return _tag(m, "Default Lit : Metal048C teinte cuivre #C07850, vert-de-gris #4F7A66 (Metallic 0) en taches.",
                (0.6, 0.35, 0.22))


def puddle():
    m, nb = _mat("MAT-Fort_Puddle")
    if nb is None:
        return m
    nb.bsdf.inputs["Base Color"].default_value = (0.035, 0.045, 0.035, 1)
    nb.bsdf.inputs["Roughness"].default_value = 0.04
    P = nb.geo().outputs["Position"]
    i = nb.img(TEX["water_n"], nb.vscale(P, 0.35), False)
    _finish_normal(nb, i.outputs["Color"], 0.25)
    return _tag(m, "Default Lit : #090B09, Roughness 0.04, normale T_Water_N x0.25 (panner lent).", (0.04, 0.05, 0.04))


def emissive(name, color, strength, base=(0.9, 0.7, 0.4), alpha=1.0):
    m, nb = _mat(name)
    if nb is None:
        return m
    nb.bsdf.inputs["Base Color"].default_value = (*base, 1)
    nb.bsdf.inputs["Emission Color"].default_value = (*color, 1)
    nb.bsdf.inputs["Emission Strength"].default_value = strength
    nb.bsdf.inputs["Roughness"].default_value = 0.4
    if alpha < 1:
        nb.bsdf.inputs["Alpha"].default_value = alpha
    return _tag(m, f"Unlit/Emissive : couleur {color} x {strength}.", base)


def flag_mat(variant="fleurdelise"):
    """Pavillon blanc casse seme de fleurs de lys d'or (quinconce 5 x 4) sur toile Fabric019 ; UV 0..1."""
    m, nb = _mat("MAT-Fort_Flag")
    if nb is None:
        return m
    L = Layer(nb, "canvas", 0.35, sat=0.0, bright=1.0)
    uvn = nb.node("ShaderNodeUVMap", uv_map="UVMap")
    white = nb.mix(1.0, L.col, (0.95, 0.92, 0.85, 1.0), "MULTIPLY")
    col = white
    if variant == "fleurdelise":
        # fleur de lys : image procedurale = union de formes (petale central, 2 petales recourbes, traverse)
        img = fleur_de_lys_image()
        tex = nb.node("ShaderNodeTexImage")
        tex.image = img
        tex.extension = "REPEAT"
        mp = nb.node("ShaderNodeMapping")
        mp.inputs["Scale"].default_value = (5.0, 4.0, 1.0)
        nb.link(uvn.outputs["UV"], mp.inputs["Vector"])
        # quinconce : decale une rangee sur deux d'une demi-cellule
        sx, sy, _ = nb.sep(mp.outputs["Vector"])
        row = nb.math("FLOOR", sy)
        odd = nb.math("MODULO", row, 2.0)
        sx2 = nb.math("ADD", sx, nb.math("MULTIPLY", odd, 0.5))
        cmb = nb.node("ShaderNodeCombineXYZ")
        nb.link(sx2, cmb.inputs[0])
        nb.link(sy, cmb.inputs[1])
        nb.link(cmb.outputs[0], tex.inputs["Vector"])
        a = tex.outputs["Alpha"]
        gold = nb.mix(nb.mr(nb.noise(uvn.outputs["UV"], 60.0), 0.3, 0.7), (0.50, 0.33, 0.08, 1), (0.62, 0.45, 0.14, 1))
        col = nb.mix(a, white, gold)
    # salissures : bord de guindant (drisse) et battant effiloche plus sale
    ux, uy, _ = nb.sep(uvn.outputs["UV"])
    dirt = nb.math("MAXIMUM", nb.mr(ux, 0.0, 0.08, 0.35, 0.0), nb.mr(ux, 0.85, 1.0, 0.0, 0.3))
    dirt = nb.math("MULTIPLY", dirt, nb.mr(nb.noise(uvn.outputs["UV"], 8.0), 0.3, 0.7))
    col = nb.mix(dirt, col, (0.42, 0.38, 0.30, 1.0), "MULTIPLY")
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], 0.85)
    nb.set("Sheen Weight", 0.3)
    _finish_normal(nb, L.nor, 0.6)
    return _tag(m, "Default Lit Two Sided : T_Fabric019 x #EEE9DC ; T_FleurDeLys (masque, quinconce 5 x 4) -> #C8A24A ;"
                   " WPO de vent par UV1.x.", (0.9, 0.88, 0.8))


def fleur_de_lys_image(res=256):
    """Masque alpha d'une fleur de lys heraldique (petale central en fer de lance, deux petales lateraux recourbes vers
    l'exterieur puis vers le bas, traverse, pied trifolie), dessinee par distance a des polylignes (numpy)."""
    img = bpy.data.images.get("T_FleurDeLys")
    if img is not None:
        return img
    import numpy as np
    yy, xx = np.mgrid[0:res, 0:res] / (res - 1)
    x = (xx - 0.5) * 2.0
    y = (yy - 0.5) * 2.0

    def stroke(pts, w0, w1):
        """Union d'un trait epais (largeur w0 -> w1 le long de la polyligne)."""
        pts = np.asarray(pts, float)
        seg_len = np.linalg.norm(np.diff(pts, axis=0), axis=1)
        acc = np.concatenate([[0], np.cumsum(seg_len)])
        tot = acc[-1]
        best = np.full(x.shape, 9.0)
        for i in range(len(pts) - 1):
            a_, b_ = pts[i], pts[i + 1]
            d = b_ - a_
            L2 = max(float(d @ d), 1e-9)
            t = np.clip(((x - a_[0]) * d[0] + (y - a_[1]) * d[1]) / L2, 0, 1)
            px, py = a_[0] + t * d[0], a_[1] + t * d[1]
            dist = np.hypot(x - px, y - py)
            s_ = (acc[i] + t * np.sqrt(L2)) / tot
            w = w0 + (w1 - w0) * s_
            best = np.minimum(best, dist - w)
        return best < 0
    ax = np.abs(x)
    # petale central : fer de lance
    yc = np.clip((y + 0.05) / 0.95, 0, 1)
    c = (ax < 0.2 * np.sin(np.pi * np.clip(yc * 0.92 + 0.08, 0, 1)) ** 0.8) & (y > -0.05) & (y < 0.9)
    # petales lateraux : montent, s'evasent, retombent en crosse
    for sgn in (-1, 1):
        pl = [(0.12, 0.0), (0.26, 0.2), (0.42, 0.42), (0.6, 0.5), (0.74, 0.42), (0.78, 0.28), (0.7, 0.18),
              (0.62, 0.22)]
        c |= stroke([(sgn * px, py) for px, py in pl], 0.12, 0.035)
    # traverse (lien)
    c |= (ax < 0.36) & (y > -0.2) & (y < -0.04)
    # pied trifolie
    c |= (ax < 0.07) & (y > -0.55) & (y <= -0.2)
    for sgn in (-1, 1):
        pl = [(0.06, -0.22), (0.2, -0.32), (0.32, -0.46), (0.3, -0.58), (0.22, -0.6)]
        c |= stroke([(sgn * px, py) for px, py in pl], 0.07, 0.03)
    c |= np.hypot(x, y + 0.6) < 0.075
    a = c.astype(np.float32) * 0.95
    pix = np.zeros((res, res, 4), np.float32)
    pix[..., :3] = 1.0
    pix[..., 3] = a
    img = bpy.data.images.new("T_FleurDeLys", res, res, alpha=True)
    img.pixels.foreach_set(pix.ravel())
    img.pack()
    return img


# ----------------------------------------------------------------------------
# Catalogue

_CACHE = [None]


def mats():
    """Dictionnaire cle -> materiau (crees a la demande, une fois par session)."""
    c = _CACHE[0]
    if c is not None:
        try:
            for m in c.values():
                m.name
        except ReferenceError:
            c = None
    if c is None:
        c = _CACHE[0] = _Lazy()
    return c


class _Lazy(dict):
    """Cree les materiaux a la premiere demande (economise images et compilation EEVEE)."""
    MAKERS = {
        "rubble": coral_rubble,
        "ashlar": coral_ashlar,
        "brick": lambda: simple("MAT-Fort_Brick", "brick", 0.6, tint=(0.95, 0.85, 0.80), sat=0.85, weather=0.9,
                                bump=0.4, color=(0.45, 0.2, 0.15),
                                recipe="Default Lit : T_RedBrick03 (tile 0,6 m) x #F2D9CC + MF_FortWeathering."),
        "tiles": roof_tiles,
        "oak": lambda: simple("MAT-Fort_WoodOak", "oak", 1.3, tint=(1.05, 0.98, 0.92), sat=0.8, weather=0.5,
                              color=(0.22, 0.17, 0.13),
                              recipe="Default Lit : T_DarkWoodenPlanks (fil le long de U) chene goudronne."),
        "oak_light": lambda: simple("MAT-Fort_WoodWeathered", "oak2", 1.1, tint=(0.95, 0.90, 0.84), sat=0.55,
                                    weather=0.5, color=(0.4, 0.35, 0.3),
                                    recipe="Default Lit : T_WeatheredPlanks gris argente (planches usees au sel)."),
        "tamanu": lambda: simple("MAT-Fort_WoodTamanu", "tamanu", 1.6, tint=(1.15, 0.80, 0.62), sat=1.0, weather=0.4,
                                 color=(0.35, 0.18, 0.1),
                                 recipe="Default Lit : T_BrownPlanks03 teinte rouge-brun (tamanu)."),
        "coconut": lambda: simple("MAT-Fort_Coconut", "coconut", 0.9, tint=(0.9, 0.85, 0.8), sat=0.7, weather=0.5,
                                  color=(0.35, 0.3, 0.25), recipe="Default Lit : T_PalmTreeBark (pieux de cocotier)."),
        "iron": iron_rust,
        "cast": lambda: simple("MAT-Fort_Iron", "iron", 0.6, tint=(0.55, 0.56, 0.6), sat=0.5, metal=0.85,
                               rough_add=0.15, color=(0.12, 0.12, 0.13),
                               recipe="Default Lit : Metal046B fonte noire (Metallic 0.85, Roughness +0.15)."),
        "copper": copper,
        "canvas": lambda: simple("MAT-Fort_Canvas", "canvas", 0.5, tint=(1.0, 0.95, 0.85), sat=0.6,
                                 color=(0.8, 0.75, 0.65), recipe="Default Lit Two Sided : T_Fabric019."),
        "jute": lambda: simple("MAT-Fort_Jute", "jute", 0.28, tint=(1.0, 0.92, 0.78), sat=0.9, color=(0.55, 0.45, 0.3),
                               recipe="Default Lit : T_Hessian230 (sacs)."),
        "rope": lambda: simple("MAT-Fort_Rope", "rope", 0.12, tint=(0.95, 0.85, 0.65), sat=0.9,
                               color=(0.55, 0.45, 0.3), recipe="Default Lit : T_Rope001 (fil le long de V)."),
        "puddle": puddle,
        "barrel": lambda: simple("MAT-Fort_BarrelStaves", "barrel", 1.0, tint=(0.95, 0.9, 0.85), sat=0.85,
                                 weather=0.3, color=(0.4, 0.28, 0.18),
                                 recipe="Default Lit : T_WoodenBarrel (douves, u = tour, v = hauteur)."),
        "gravel": lambda: simple("MAT-Fort_CoralGravel", "gravel", 1.1, tint=(0.95, 0.92, 0.86), sat=0.5,
                                 weather=0.6, bump=0.4, color=(0.7, 0.66, 0.58),
                                 recipe="Default Lit : T_CoralGravel (sable corallien + gravats)."),
        "shingles": lambda: simple("MAT-Fort_Shingles", "shingle", 0.55, tint=(0.82, 0.82, 0.80), sat=0.25,
                                   weather=0.7, bump=0.3, rot=1.5708, color=(0.45, 0.44, 0.42),
                                   recipe="Default Lit : T_WeatheredPlanks a petite echelle (essentes gris argent)."),
        "flag": flag_mat,
        "lamp": lambda: emissive("MAT-Fort_LanternHorn", (1.0, 0.55, 0.22), 6.0, base=(0.85, 0.62, 0.35)),
        "fire": lambda: emissive("MAT-Fort_Embers", (1.0, 0.35, 0.06), 12.0, base=(0.2, 0.06, 0.02)),
        "dark": lambda: simple("MAT-Fort_Soot", "iron", 1.0, tint=(0.08, 0.075, 0.07), sat=0.0, rough_add=0.3,
                               color=(0.03, 0.03, 0.03), recipe="Default Lit : suie / fond noir #0E0D0C."),
    }

    def __missing__(self, key):
        mk = self.MAKERS[key]
        m = mk()
        self[key] = m
        return m


def blend_col(ballast=0.0, render=0.0, beachrock=0.0):
    """Couleur de coin Blend : R lest, G enduit, B beachrock."""
    return (ballast, render, beachrock, 1.0)
