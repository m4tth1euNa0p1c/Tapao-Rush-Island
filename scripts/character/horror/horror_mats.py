"""Matériaux « Outlast » des trois Veilleurs (T-274) : réalisme sale et inquiétant, sans gore gratuit sur les vivants.

Principes (lisibles de nuit) :
  - humidité : un masque d'humidité (coulures étirées en Z, creux, bas du corps) baisse la rugosité (0,08 à 0,25) et
    assombrit la couleur : reflets mouillés qui accrochent la torche ou la lune ;
  - crasse : cavités (nœud AO, cuit dans la couleur), coulures verticales, vase sur le bas du corps ;
  - contraste de valeur : accents pâles (corail, os, chaux, sel) sur fonds sombres ; yeux laiteux ou injectés très
    lisses, avec une émission faible (carte E) ; braises du Veilleur en émission qui pulse (paramètres Unreal) ;
  - pigments rituels inventés et délavés (aucun motif réel) : traces écaillées, rongées par le bruit.
Coordonnées : Generated (mètres, kit.fixed_texspace) pour le détail ; Object (mètres, repos) pour les masques de
hauteur ; attributs de sommets posés par horror_geo ("Wound" pour les plaies du coco, "Fissure" pour les failles du
Veilleur, "Silt" pour la vase du crabe).
Les couches de coupe (démembrement) du crabe et du Veilleur complètent celles du 2e PC (real_mats.gore_layer).
"""
from mathutils import Vector

from .base import NB, srgb
from . import real_gore, real_mats

# ------------------------------------------------------------------ graphe avec quelques outils de plus


class HB(NB):
    def attr(self, name):
        a = self.n("ShaderNodeAttribute")
        a.attribute_name = name
        return a.outputs["Fac"]

    def obj(self):
        if getattr(self, "_obj", None) is None:
            tc = self.n("ShaderNodeTexCoord")
            sep = self.n("ShaderNodeSeparateXYZ")
            self.l(tc.outputs["Object"], sep.inputs[0])
            self._obj = sep
        return self._obj

    def z(self):
        return self.obj().outputs["Z"]

    def up(self):
        """Composante verticale de la normale (1 = face au ciel)."""
        if getattr(self, "_up", None) is None:
            geo = self.n("ShaderNodeNewGeometry")
            sep = self.n("ShaderNodeSeparateXYZ")
            self.l(geo.outputs["Normal"], sep.inputs[0])
            self._up = sep.outputs["Z"]
        return self._up

    def drips(self, scale=40.0, stretch=0.05, lo=0.55, hi=0.72, detail=4.0):
        """Coulures verticales (bruit très étiré en Z), 0..1."""
        return self.fac(self.noise(1.0, detail, 0.6, 0.0, vec=self.vec((scale, scale, scale * stretch))), lo, hi)

    def voro(self, scale, feature="F1", out="Distance", vec=None, rand=1.0):
        x = self.n("ShaderNodeTexVoronoi", feature=feature)
        self.l(vec if vec is not None else self.gen(), x.inputs["Vector"])
        x.inputs["Scale"].default_value = scale
        try:
            x.inputs["Randomness"].default_value = rand
        except KeyError:
            pass
        return x.outputs[out]

    def clamp01(self, v):
        return self.math("MINIMUM", self.math("MAXIMUM", v, 0.0), 1.0)

    def lerpf(self, f, a, b):
        """a + (b - a) * f (flottants ou sockets)."""
        return self.math("ADD", a, self.math("MULTIPLY", f, self.math("SUBTRACT", b, a)))

    def objv(self):
        if getattr(self, "_objv", None) is None:
            self._objv = self.n("ShaderNodeTexCoord").outputs["Object"]
        return self._objv

    def vmath(self, op, a, b=None, scale=None):
        m = self.n("ShaderNodeVectorMath", operation=op)
        self.put(m.inputs[0], tuple(a) if hasattr(a, "to_tuple") else a)
        if b is not None:
            self.put(m.inputs[1], tuple(b) if hasattr(b, "to_tuple") else b)
        if scale is not None:
            self.put(m.inputs["Scale"], scale)
        return m.outputs["Value"] if op in ("DOT_PRODUCT", "LENGTH", "DISTANCE") else m.outputs["Vector"]

    def seg_dist(self, a, b):
        """Distance (m, espace objet) du point au segment a-b (constantes)."""
        a, b = Vector(a), Vector(b)
        ab = b - a
        L2 = max(ab.length_squared, 1e-10)
        ap = self.vmath("SUBTRACT", self.objv(), a)
        t = self.math("DIVIDE", self.vmath("DOT_PRODUCT", ap, ab), L2, clamp=True)
        q = self.vmath("ADD", self.vmath("SCALE", ab, scale=t), a)
        return self.vmath("DISTANCE", self.objv(), q)

    def sap_drips(self, wounds, step=0.022, width=0.009, axes=(1.0, 1.0, 0.0), length=(0.04, 0.16)):
        """Filets de sève sous les plaies : sources régulières le long des tracés, coulure vers le bas (longueur
        variable), cassée par un bruit étiré en Z."""
        import random as _r
        rnd = _r.Random(17)
        out = None
        z = self.z()
        for w in wounds:
            P = w["pts"]
            for i in range(len(P) - 1):
                a, b = Vector(P[i]), Vector(P[i + 1])
                n = max(1, int((b - a).length / step))
                for k in range(n):
                    W = a.lerp(b, (k + rnd.random()) / n)
                    L = rnd.uniform(*length) * (1.5 if w["w"] > 0.02 else 1.0)
                    dxy = self.vmath("LENGTH", self.vmath("MULTIPLY", self.vmath("SUBTRACT", self.objv(), W), axes))
                    dz = self.math("SUBTRACT", W.z, z)
                    d = self.math("MULTIPLY", self.fac(dxy, width * rnd.uniform(0.7, 1.4), 0.0),
                                  self.math("MULTIPLY", self.fac(dz, -0.002, 0.004), self.fac(dz, L, L * 0.25)))
                    out = d if out is None else self.math("MAXIMUM", out, d)
        if out is None:
            return 0.0
        brk = self.fac(self.noise(1.0, 3.0, 0.5, 0.0, vec=self.vec((70.0, 70.0, 2.5))), 0.25, 0.55)
        return self.math("MULTIPLY", out, brk)

    def wet_finish(self, col, rough, wet, darken=0.55, wet_rough=0.12):
        """Humidité : couleur assombrie et rugosité basse là où wet = 1."""
        col = self.mix(self.math("MULTIPLY", wet, 1.0 - darken), col, self.mix(1.0, col, (darken, darken, darken, 1),
                                                                                 "MULTIPLY"))
        rough = self.lerpf(wet, rough, wet_rough)
        return col, rough


def _grime(g, col, cav_dist=0.05, amt=1.6, dirt="0C0806"):
    return g.mix(g.math("MULTIPLY", g.cavity(cav_dist), amt, clamp=True), col, srgb(dirt))


# ======================================================================== GARDIEN CRABE (récif, vase, corail)
def crab_shell(name):
    """Carapace : brun-bordeaux sombre et bleui, désaturé ; tubercules lus par le relief et la crasse (pas de taches
    claires), arêtes usées plus pâles (pointiness, cuisson Cycles), cicatrices rondes de balanes arrachées, algues
    noir-vert en coulures, vase noire sur le bas, humidité presque partout sauf le sommet qui sèche en sel."""
    g = HB(name)
    base = g.ramp(g.noise(3.0, 5.0, 0.6), [(0.2, "140806"), (0.5, "2E120D"), (0.8, "4A1E14")])
    bruise = g.fac(g.noise(1.8, 4.0, 0.6, 0.4), 0.52, 0.72)
    base = g.mix(g.math("MULTIPLY", bruise, 0.6), base, srgb("1E101C"))
    mott = g.fac(g.noise(16.0, 5.0, 0.65), 0.45, 0.75)
    base = g.mix(g.math("MULTIPLY", mott, 0.35), base, srgb("0C0606"))
    pits = g.fac(g.voro(140.0), 0.10, 0.0)
    base = g.mix(g.math("MULTIPLY", pits, 0.6), base, srgb("050303"))
    geo = g.n("ShaderNodeNewGeometry")
    edge = g.fac(geo.outputs["Pointiness"], 0.52, 0.62)
    wear = g.math("MULTIPLY", edge, g.fac(g.noise(30.0, 5.0, 0.65), 0.45, 0.7))
    base = g.mix(g.math("MULTIPLY", wear, 0.55), base, srgb("7A5A4C"))
    scr = g.fac(g.noise(2.0, 4.0, 0.6, 0.0, vec=g.vec((5.0, 5.0, 42.0))), 0.72, 0.745)
    base = g.mix(g.math("MULTIPLY", scr, 0.45), base, srgb("6E5248"))
    ring = g.voro(9.0, "F1", "Distance")                         # cicatrices de balanes arrachées
    scar = g.math("MULTIPLY", g.fac(g.math("ABSOLUTE", g.math("SUBTRACT", ring, 0.13)), 0.025, 0.0),
                  g.fac(g.noise(4.0, 3.0), 0.5, 0.65))
    base = g.mix(g.math("MULTIPLY", scar, 0.7), base, srgb("8A847A"))
    algae = g.clamp01(g.math("ADD", g.math("MULTIPLY", g.cavity(0.05), 1.3),
                             g.math("MULTIPLY", g.drips(16.0, 0.06, 0.58, 0.72), 0.8)))
    algae = g.math("MULTIPLY", algae, g.fac(g.noise(5.0, 4.0), 0.4, 0.62))
    base = g.mix(g.math("MULTIPLY", algae, 0.85), base, g.ramp(g.noise(40.0), [(0.3, "0A0E06"), (0.7, "1E2A10")]))
    silt = g.clamp01(g.math("ADD", g.attr("Silt"), g.math("MULTIPLY", g.drips(24.0, 0.06, 0.55, 0.72),
                                                           g.fac(g.z(), 0.70, 0.32))))
    mud = g.ramp(g.noise(18.0, 4.0), [(0.3, "1E1A14"), (0.7, "3E3628")])
    col = g.mix(g.math("MULTIPLY", silt, 0.9), base, mud)
    col = _grime(g, col, 0.05, 1.7, "040202")
    dry_top = g.fac(g.up(), 0.45, 0.95)
    bleach = g.math("MULTIPLY", g.fac(g.noise(4.0, 5.0, 0.62), 0.5, 0.68), g.math("ADD", 0.35, dry_top))
    col = g.mix(g.math("MULTIPLY", bleach, 0.5), col, g.ramp(g.noise(22.0, 4.0), [(0.3, "4A3A34"), (0.7, "7A6A60")]))
    salt = g.math("MULTIPLY", dry_top, g.fac(g.noise(26.0, 5.0, 0.65), 0.6, 0.72))
    col = g.mix(g.math("MULTIPLY", salt, 0.55), col, srgb("A49E92"))
    streaks = g.drips(20.0, 0.05, 0.58, 0.72)
    wet = g.clamp01(g.math("ADD", g.math("MULTIPLY", streaks, 0.6),
                           g.math("ADD", g.math("MULTIPLY", g.cavity(0.03), 0.3), g.math("MULTIPLY", silt, 0.8))))
    wet = g.math("MULTIPLY", wet, g.math("SUBTRACT", 1.0, g.math("MULTIPLY", bleach, 0.8)))
    rough = g.math("MULTIPLY_ADD", g.noise(12.0, 3.0), 0.16, 0.7)
    col, rough = g.wet_finish(col, rough, wet, 0.72, 0.16)
    gran = g.fac(g.voro(48.0, "SMOOTH_F1"), 0.45, 0.0)          # granules en dôme
    gran2 = g.fac(g.voro(120.0), 0.35, 0.0)
    h = g.math("ADD", g.math("MULTIPLY", g.noise(90.0, 4.0), 0.2), g.math("MULTIPLY", mott, 0.2))
    h = g.math("ADD", h, g.math("ADD", g.math("MULTIPLY", gran, 0.55), g.math("MULTIPLY", gran2, 0.25)))
    h = g.math("SUBTRACT", h, g.math("ADD", g.math("MULTIPLY", pits, 0.7), g.math("MULTIPLY", scr, 0.4)))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", scar, 0.5))
    h = g.math("ADD", h, g.math("MULTIPLY", silt, g.math("MULTIPLY", g.noise(40.0, 3.0), 0.3)))
    return g.finish(col, rough, g.bump(h, 0.7, 0.003), fallback="2E120D")


def crab_limb(name):
    """Pattes et pinces : chitine maigre, bordeaux sombre marbré, dessous plus pâle et mou, articulations
    sombres (creux) et humides, arêtes usées, vase aux extrémités."""
    g = HB(name)
    base = g.ramp(g.noise(6.0, 5.0, 0.6), [(0.2, "1A0806"), (0.55, "36120C"), (0.85, "552016")])
    mott = g.fac(g.noise(18.0, 4.0, 0.6), 0.5, 0.72)
    base = g.mix(g.math("MULTIPLY", mott, 0.45), base, srgb("120708"))
    belly = g.fac(g.math("MULTIPLY", g.up(), -1.0), 0.1, 0.7)       # dessous pâle (face tournée vers le sol)
    base = g.mix(g.math("MULTIPLY", belly, 0.45), base, srgb("7A5A4E"))
    geo = g.n("ShaderNodeNewGeometry")
    edge = g.fac(geo.outputs["Pointiness"], 0.53, 0.64)
    base = g.mix(g.math("MULTIPLY", edge, 0.5), base, srgb("8A6A5A"))
    mud = g.fac(g.z(), 0.20, 0.04)
    col = g.mix(g.math("MULTIPLY", mud, 0.85), base, srgb("16120E"))
    col = _grime(g, col, 0.03, 2.0, "050303")
    wet = g.clamp01(g.math("ADD", g.math("MULTIPLY", g.cavity(0.02), 0.4),
                           g.math("MULTIPLY", g.drips(30.0, 0.06, 0.56, 0.72), 0.5)))
    rough = g.math("MULTIPLY_ADD", g.noise(20.0, 3.0), 0.14, 0.58)
    col, rough = g.wet_finish(col, rough, wet, 0.75, 0.2)
    spk = g.fac(g.voro(90.0), 0.25, 0.02)
    h = g.math("ADD", g.math("MULTIPLY", spk, 0.4), g.math("MULTIPLY", g.noise(140.0, 3.0), 0.2))
    return g.finish(col, rough, g.bump(h, 0.5, 0.002), sss=0.02, fallback="36120C")


def crab_claw_tip(name):
    """Bouts des pinces et des pattes : kératine noire ébréchée, brillante."""
    g = HB(name)
    col = g.ramp(g.noise(16.0, 4.0), [(0.3, "070404"), (0.75, "2A1410")])
    chip = g.fac(g.noise(40.0, 6.0, 0.7), 0.70, 0.76)
    col = g.mix(g.math("MULTIPLY", chip, 0.5), col, srgb("6A5048"))
    return g.finish(col, g.math("MULTIPLY_ADD", chip, 0.3, 0.16), g.bump(g.noise(120.0), 0.2, 0.001), fallback="1A0C0A")


def coral_crust(name):
    """Corail incrusté : calcaire blanc-gris poreux, polypes morts, verdi par les algues dans les creux, humide en
    bas."""
    g = HB(name)
    col = g.ramp(g.noise(14.0, 5.0, 0.6), [(0.25, "4E463E"), (0.6, "7E746A"), (0.9, "A49A8C")])
    por = g.voro(260.0)
    pores = g.fac(por, 0.18, 0.0)
    col = g.mix(g.math("MULTIPLY", pores, 0.75), col, srgb("2A241C"))
    pink = g.fac(g.noise(5.0, 3.0), 0.6, 0.78)
    col = g.mix(g.math("MULTIPLY", pink, 0.35), col, srgb("7A5A52"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.02), 1.8, clamp=True), col, srgb("26301A"))
    wet = g.math("MULTIPLY", g.fac(g.z(), 0.55, 0.25), 0.8)
    rough = g.math("MULTIPLY_ADD", pores, 0.1, 0.78)
    col, rough = g.wet_finish(col, rough, wet, 0.7, 0.3)
    h = g.math("SUBTRACT", g.math("MULTIPLY", g.noise(60.0, 5.0), 0.5), g.math("MULTIPLY", pores, 0.8))
    return g.finish(col, rough, g.bump(h, 0.8, 0.002), fallback="C4BCAE")


def barnacle(name):
    """Balanes : plaques calcaires gris-blanc striées, cratère sombre, pied vaseux."""
    g = HB(name)
    st = g.noise(2.0, 4.0, 0.6, 0.0, vec=g.vec((260.0, 260.0, 30.0)))
    col = g.ramp(st, [(0.3, "322E28"), (0.7, "6E6A5E")])
    col = g.mix(g.math("MULTIPLY", g.cavity(0.012), 2.6, clamp=True), col, srgb("060504"))
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(8.0, 3.0), 0.55, 0.75), 0.4), col, srgb("3A3A26"))
    return g.finish(col, g.math("MULTIPLY_ADD", st, 0.2, 0.62), g.bump(st, 0.6, 0.0012), fallback="9A9488")


def driftwood_mask(name, pigment="2E5A56"):
    """Masque de bois flotté : gris blanchi, gorgé d'eau en bas, veinage profond, fentes, sel dans les creux,
    pigment bleu-vert délavé qui ne subsiste que dans les incisions (inventé), algues noires en coulures."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/weathered_planks")
    v = g.vec((2.6, 2.6, 2.6))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.25)
    wl = real_mats.lum(g, img.outputs["Color"])
    grain = g.noise(2.0, 6.0, 0.6, 1.2, vec=g.vec((60.0, 60.0, 9.0)))
    wl2 = g.math("ADD", g.math("MULTIPLY", wl, 0.7), g.math("MULTIPLY", grain, 0.3))
    wood = g.ramp(wl2, [(0.15, "2E2B26"), (0.45, "6A655A"), (0.75, "948D7E"), (0.95, "B2AA98")])
    stains = g.fac(g.noise(3.0, 5.0, 0.62, 0.4), 0.52, 0.72)
    wood = g.mix(g.math("MULTIPLY", stains, 0.55), wood, srgb("1E1A14"))
    soak = g.fac(g.z(), 0.62, 0.42)                                 # bas du masque gorgé d'eau
    wood = g.mix(g.math("MULTIPLY", soak, 0.55), wood, srgb("2A2620"))
    cav = g.cavity(0.025)
    pig = g.math("MULTIPLY", g.fac(cav, 0.15, 0.6), g.fac(g.noise(9.0, 5.0, 0.65), 0.35, 0.6))
    col = g.mix(g.math("MULTIPLY", pig, 0.85), wood, g.ramp(g.noise(20.0), [(0.3, "1E3A38"), (0.7, pigment)]))
    salt = g.math("MULTIPLY", g.fac(cav, 0.2, 0.7), g.fac(g.noise(30.0, 5.0, 0.7), 0.58, 0.68))
    col = g.mix(g.math("MULTIPLY", salt, 0.6), col, srgb("D2CCBE"))
    cr = g.fac(g.edges(4.0, g.vec((1.0, 1.0, 0.22))), 0.012, 0.0)        # fentes de séchage verticales
    cr = g.math("MULTIPLY", cr, g.fac(g.noise(3.0, 2.0), 0.4, 0.6))
    col = g.mix(g.math("MULTIPLY", cr, 0.95), col, srgb("060504"))
    algae = g.math("MULTIPLY", g.drips(18.0, 0.07, 0.6, 0.74), g.fac(g.z(), 0.95, 0.55))
    col = g.mix(g.math("MULTIPLY", algae, 0.7), col, srgb("10140A"))
    col = _grime(g, col, 0.03, 2.0, "080706")
    wet = g.clamp01(g.math("ADD", g.math("MULTIPLY", soak, 0.8), g.math("MULTIPLY", algae, 0.8)))
    rough = g.math("MULTIPLY_ADD", grain, 0.1, 0.8)
    col, rough = g.wet_finish(col, rough, wet, 0.65, 0.22)
    h = g.math("ADD", g.math("MULTIPLY", wl2, 0.6), g.math("MULTIPLY", grain, 0.4))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", cr, 1.4))
    return g.finish(col, rough, g.bump(h, 0.9, 0.003), fallback="7A7468")


def shell_shard(name):
    """Dents en éclats de coquillage : nacre sale, bords translucides, taches brunes, éclats vifs."""
    g = HB(name)
    col = g.ramp(g.noise(20.0, 4.0, 0.6), [(0.3, "8C8270"), (0.7, "D8CEB8")])
    nacre = g.fac(g.noise(60.0, 3.0, 0.5, 2.0), 0.5, 0.8)
    col = g.mix(g.math("MULTIPLY", nacre, 0.25), col, srgb("C8D0D4"))
    stain = g.fac(g.noise(6.0, 5.0, 0.6, 0.5), 0.6, 0.72)
    col = g.mix(g.math("MULTIPLY", stain, 0.8), col, srgb("3A2414"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.01), 2.0, clamp=True), col, srgb("140C08"))
    return g.finish(col, g.math("MULTIPLY_ADD", stain, 0.3, 0.22), g.bump(g.noise(200.0), 0.2, 0.0006), sss=0.05,
                    fallback="C8BEA8")


def milky_eye(name, globe="3E4448", iris="9AA2A6", pupil="CED6D8", vein="6A2420", vein_amt=0.5, glow="C8DCE6",
              glow_k=1.2, iris_r=(0.66, 0.84), pupil_r=(0.92, 0.975), glow_on="iris"):
    """Œil laiteux (cataracte) : globe sombre et humide, grande tache laiteuse à la place de l'iris (le regard ne
    se lit pas), veinules ; très lisse (reflets) ; émission faible sur la partie laiteuse, visible de nuit. L'iris est
    la partie de chaque globe tournée vers l'avant (-Y). glow_on="sclera" : lueur sur le blanc (œil injecté)."""
    g = HB(name)
    geo = g.n("ShaderNodeNewGeometry")
    sep = g.n("ShaderNodeSeparateXYZ")
    g.l(geo.outputs["Normal"], sep.inputs[0])
    front = g.math("MULTIPLY", sep.outputs["Y"], -1.0)
    col = g.ramp(g.noise(10.0, 4.0, 0.6), [(0.3, globe), (0.8, globe)])
    vf = g.noise(7.0, 6.0, 0.6, 2.0)
    veins = g.fac(g.math("ABSOLUTE", g.math("SUBTRACT", vf, 0.5)), 0.02 + 0.015 * vein_amt, 0.0)
    col = g.mix(g.math("MULTIPLY", veins, vein_amt), col, srgb(vein))
    irf = g.fac(front, iris_r[0], iris_r[1])
    if glow_on == "sclera":
        iris_col = g.ramp(g.noise(60.0, 3.0, 0.5, 2.0), [(0.3, "202426"), (0.7, iris)])
    else:
        iris_col = g.ramp(g.noise(30.0, 3.0, 0.5, 2.0), [(0.3, iris), (0.7, pupil)])
    col = g.mix(g.math("MULTIPLY", irf, 0.9), col, iris_col)
    puf = g.fac(front, pupil_r[0], pupil_r[1])
    col = g.mix(puf, col, srgb(pupil))
    swirl = g.fac(g.noise(4.0, 5.0, 0.6, 2.5), 0.5, 0.8)
    col = g.mix(g.math("MULTIPLY", swirl, 0.2), col, srgb("B8C0C4"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.01), 1.8, clamp=True), col, srgb("0A0606"))
    gm = g.math("SUBTRACT", 1.0, irf) if glow_on == "sclera" else g.math("MAXIMUM", irf, puf)
    emis = g.mix(gm, (0, 0, 0, 1), srgb(glow))
    return g.finish(col, 0.035, g.bump(swirl, 0.05, 0.0004), sss=0.06, emis=emis, emis_strength=glow_k,
                    fallback=globe)


def rotten_rope(name, dark="1A140C", light="5E4A2E", algae="1E2A10"):
    """Corde pourrie : fibres CC0 noircies, algues vert-noir, humide."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/Rope001")
    v = g.vec((14.0, 14.0, 14.0))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.3)
    lu = real_mats.lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, dark), (0.6, light)])
    alg = g.fac(g.noise(6.0, 4.0, 0.6), 0.5, 0.68)
    col = g.mix(g.math("MULTIPLY", alg, 0.75), col, srgb(algae))
    col = _grime(g, col, 0.012, 1.6, "080604")
    wet = g.math("MULTIPLY", alg, 0.9)
    rough = 0.82
    col, rough = g.wet_finish(col, rough, wet, 0.7, 0.2)
    h = lu
    if maps["height"]:
        h = real_mats.lum(g, real_mats.image_node(g, maps["height"], v, "Non-Color").outputs["Color"])
    return g.finish(col, rough, g.bump(h, 0.7, 0.002), fallback=light)


def kelp(name):
    """Lanières d'algues brunes : olive-noir, translucides, luisantes (UV : v le long)."""
    g = HB(name)
    st = g.noise(3.0, 4.0, 0.6, 0.0, vec=g.vec((90.0, 90.0, 8.0)))
    col = g.ramp(st, [(0.25, "0C0C06"), (0.6, "22200C"), (0.9, "3A3014")])
    spots = g.fac(g.noise(30.0, 4.0), 0.62, 0.72)
    col = g.mix(g.math("MULTIPLY", spots, 0.5), col, srgb("060604"))
    return g.finish(col, g.math("MULTIPLY_ADD", st, 0.15, 0.32), g.bump(st, 0.3, 0.001), sss=0.08, fallback="22200C")


def driftwood_handle(name):
    """Manche en bois flotté : gris argent délavé, fibres ouvertes, taches noires de vase, humide aux mains."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/rough_wood")
    v = g.vec((6.0, 6.0, 2.0))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.3)
    wl = real_mats.lum(g, img.outputs["Color"])
    fib = g.noise(2.0, 6.0, 0.6, 0.0, vec=g.vec((140.0, 140.0, 6.0)))
    col = g.ramp(g.math("ADD", g.math("MULTIPLY", wl, 0.6), g.math("MULTIPLY", fib, 0.4)),
                 [(0.15, "2A2824"), (0.5, "6A665C"), (0.85, "A29C8E")])
    stain = g.fac(g.noise(4.0, 5.0, 0.6), 0.58, 0.72)
    col = g.mix(g.math("MULTIPLY", stain, 0.8), col, srgb("12100C"))
    col = _grime(g, col, 0.02, 1.5, "070605")
    rough = g.math("MULTIPLY_ADD", fib, 0.15, 0.72)
    col, rough = g.wet_finish(col, rough, g.math("MULTIPLY", stain, 0.8), 0.6, 0.25)
    return g.finish(col, rough, g.bump(g.math("ADD", wl, fib), 0.8, 0.002), fallback="6A665C")


def conch(name):
    """Coquillage-marteau : ocre rosé délavé, intérieur nacré, croûte de calcaire, éclats, coulures noires."""
    g = HB(name)
    col = g.ramp(g.noise(5.0, 5.0, 0.6), [(0.2, "2A1A14"), (0.55, "5A3C2E"), (0.85, "8A6A56")])
    bands = g.n("ShaderNodeTexWave", wave_type="BANDS", bands_direction="X")
    g.l(g.vec((12.0, 12.0, 12.0)), bands.inputs["Vector"])
    bands.inputs["Distortion"].default_value = 3.0
    col = g.mix(g.math("MULTIPLY", g.fac(bands.outputs["Fac"], 0.6, 0.9), 0.35), col, srgb("2A1810"))
    crust = g.fac(g.noise(9.0, 6.0, 0.65), 0.52, 0.66)
    col = g.mix(g.math("MULTIPLY", crust, 0.7), col, srgb("B8B0A2"))
    col = g.mix(g.math("MULTIPLY", g.drips(26.0, 0.06, 0.6, 0.74), 0.6), col, srgb("14100C"))
    col = _grime(g, col, 0.02, 1.8, "0A0806")
    rough = g.lerpf(crust, 0.34, 0.82)
    return g.finish(col, rough, g.bump(g.math("ADD", crust, g.math("MULTIPLY", g.noise(90.0), 0.3)), 0.6, 0.002),
                    fallback="8A5A44")


# ======================================================================== VEILLEUR DES CENDRES (cendre, braises)
def ash_crust(name):
    """Croûte de cendre : charbon fissuré en écailles (alligator), poudre grise sur les dessus et dans les creux,
    failles (attribut Fissure + réseau fin) où couvent des braises : émission faible, plus forte au fond des failles.
    Inclusions vitreuses (obsidienne) brillantes."""
    g = HB(name)
    v = g.vec((1.0, 1.0, 1.0))
    cells = g.voro(14.0, "DISTANCE_TO_EDGE", vec=v)
    fine = g.voro(46.0, "DISTANCE_TO_EDGE")
    crack_fine = g.math("MAXIMUM", g.fac(cells, 0.035, 0.0), g.math("MULTIPLY", g.fac(fine, 0.03, 0.0), 0.55))
    fis = g.attr("Fissure")
    crack = g.clamp01(g.math("MAXIMUM", fis, crack_fine))
    char = g.ramp(g.noise(8.0, 6.0, 0.6), [(0.2, "0A0908"), (0.55, "1C1916"), (0.85, "322C27")])
    flakes = g.fac(g.noise(70.0, 6.0, 0.7), 0.62, 0.7)
    char = g.mix(g.math("MULTIPLY", flakes, 0.4), char, srgb("4A443E"))
    ash = g.ramp(g.noise(30.0, 5.0, 0.6), [(0.3, "5E5A54"), (0.75, "9C968C")])
    ashf = g.clamp01(g.math("ADD", g.math("MULTIPLY", g.fac(g.up(), 0.25, 0.85),
                                          g.fac(g.noise(6.0, 5.0, 0.6), 0.35, 0.65)),
                            g.math("MULTIPLY", g.cavity(0.04), 0.8)))
    ashf = g.math("MULTIPLY", ashf, g.math("SUBTRACT", 1.0, crack))
    col = g.mix(g.math("MULTIPLY", ashf, 0.6), char, ash)
    glass = g.math("MULTIPLY", g.fac(g.noise(11.0, 4.0, 0.6), 0.70, 0.78), g.math("SUBTRACT", 1.0, ashf))
    col = g.mix(g.math("MULTIPLY", glass, 0.8), col, srgb("050506"))
    # braises au fond des failles (bruit qui éteint une partie des fissures fines)
    live = g.fac(g.noise(3.5, 4.0, 0.6), 0.48, 0.66)
    fine_line = g.math("MAXIMUM", g.fac(cells, 0.012, 0.0), g.math("MULTIPLY", g.fac(fine, 0.01, 0.0), 0.7))
    in_fis = g.fac(fis, 0.15, 0.6)
    heat = g.math("MAXIMUM", g.math("MULTIPLY", fine_line, in_fis), g.fac(fis, 0.75, 0.98))
    heat = g.math("MAXIMUM", heat, g.math("MULTIPLY", g.math("MULTIPLY", fine_line, live), 0.25))
    ember = g.ramp(g.noise(24.0, 4.0, 0.6), [(0.25, "4A0802"), (0.6, "A02404"), (0.9, "E8601A")])
    col = g.mix(g.math("MULTIPLY", heat, 0.9), col, g.mix(0.25, srgb("0E0302"), ember))
    rough = g.lerpf(glass, 0.9, 0.12)
    rough = g.lerpf(heat, rough, 0.55)
    h = g.math("SUBTRACT", g.math("MULTIPLY", g.noise(50.0, 5.0), 0.4), g.math("MULTIPLY", crack, 1.2))
    h = g.math("ADD", h, g.math("MULTIPLY", flakes, 0.3))
    return g.finish(col, rough, g.bump(h, 0.9, 0.003), emis=g.mix(heat, (0, 0, 0, 1), ember), emis_strength=1.3,
                    fallback="1C1916")


def ash_skin(name):
    """Peau brûlée des membres : chair carbonisée, craquelée, sèche ; braises sous la peau (veines qui couvent),
    poudre de cendre, cicatrices pâles."""
    g = HB(name)
    base = g.ramp(g.noise(7.0, 5.0, 0.6), [(0.2, "0E0908"), (0.5, "241411"), (0.8, "3E2620")])
    cells = g.voro(38.0, "DISTANCE_TO_EDGE", vec=g.vec((1.0, 1.0, 0.55)))
    crack = g.fac(cells, 0.04, 0.0)
    vein_f = g.noise(3.0, 6.0, 0.55, 1.8, vec=g.vec((1.0, 1.0, 2.4)))
    veins = g.fac(g.math("ABSOLUTE", g.math("SUBTRACT", vein_f, 0.5)), 0.03, 0.0)
    live = g.fac(g.noise(4.0, 4.0, 0.6), 0.45, 0.66)
    heat = g.clamp01(g.math("MULTIPLY", g.math("MAXIMUM", g.math("MULTIPLY", crack, 0.7), veins), live))
    ember = g.ramp(g.noise(30.0, 4.0), [(0.3, "4A0A02"), (0.65, "9A2206"), (0.95, "E05A18")])
    col = g.mix(g.math("MULTIPLY", heat, 0.85), base, g.mix(0.25, srgb("0E0302"), ember))
    ash = g.math("MULTIPLY", g.fac(g.up(), 0.2, 0.9), g.fac(g.noise(9.0, 4.0), 0.4, 0.65))
    col = g.mix(g.math("MULTIPLY", ash, 0.7), col, srgb("6E6860"))
    scars = g.math("MULTIPLY", g.fac(g.edges(3.0, g.vec((1.0, 1.0, 0.35))), 0.01, 0.0), g.fac(g.noise(2.0, 2.0), 0.58, 0.66))
    col = g.mix(g.math("MULTIPLY", scars, 0.6), col, srgb("5A3A30"))
    col = _grime(g, col, 0.05, 1.8, "060403")
    rough = g.math("MULTIPLY_ADD", g.noise(14.0, 3.0), 0.12, 0.78)
    h = g.math("SUBTRACT", g.math("MULTIPLY", g.noise(160.0, 3.0), 0.3), g.math("MULTIPLY", crack, 0.9))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", scars, 0.5))
    return g.finish(col, rough, g.bump(h, 0.7, 0.0022), emis=g.mix(heat, (0, 0, 0, 1), ember), emis_strength=0.8,
                    fallback="241411")


def obsidian(name, ash_amt=0.6):
    """Obsidienne : noir vitreux, cassure conchoïdale (ondes), arêtes à peine plus claires, cendre dans les creux."""
    g = HB(name)
    fac = g.edges(18.0)
    col = g.ramp(g.noise(9.0, 4.0, 0.6), [(0.3, "030304"), (0.8, "141418")])
    col = g.mix(g.math("MULTIPLY", g.fac(fac, 0.02, 0.0), 0.5), col, srgb("3A3A40"))
    ash = g.math("MULTIPLY", g.math("ADD", g.math("MULTIPLY", g.cavity(0.02), 1.6), g.math("MULTIPLY", g.fac(g.up(), 0.6, 1.0), 0.4)),
                 g.fac(g.noise(14.0, 4.0), 0.4, 0.7))
    col = g.mix(g.math("MULTIPLY", g.clamp01(ash), ash_amt), col, srgb("6A665E"))
    ripple = g.n("ShaderNodeTexWave", wave_type="RINGS")
    g.l(g.vec((22.0, 22.0, 22.0)), ripple.inputs["Vector"])
    ripple.inputs["Distortion"].default_value = 4.0
    rough = g.lerpf(g.clamp01(ash), 0.06, 0.8)
    return g.finish(col, rough, g.bump(g.math("ADD", ripple.outputs["Fac"], g.math("MULTIPLY", fac, 2.0)), 0.25, 0.001),
                    fallback="0A0A0C")


def char_wood(name):
    """Planches du masque : bois gris-brun délavé à fil vertical, carbonisé par plaques (écailles d'alligator noires,
    surtout en bas et aux bords), cendre sur les reliefs, coulures de suie, braise au fond de rares fentes."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/weathered_planks")
    v = g.vec((2.2, 2.2, 2.2))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.25)
    wl = real_mats.lum(g, img.outputs["Color"])
    grain = g.noise(2.0, 6.0, 0.6, 1.2, vec=g.vec((80.0, 80.0, 5.0)))
    wl2 = g.math("ADD", g.math("MULTIPLY", wl, 0.55), g.math("MULTIPLY", grain, 0.45))
    wood = g.ramp(wl2, [(0.15, "1E1914"), (0.5, "463C30"), (0.85, "6E6252")])
    burn = g.fac(g.noise(2.6, 5.0, 0.62, 0.3), 0.42, 0.62)
    burn = g.clamp01(g.math("ADD", burn, g.math("ADD", g.fac(g.z(), 0.62, 0.42), g.math("MULTIPLY", g.cavity(0.04), 1.2))))
    gator = g.voro(30.0, "DISTANCE_TO_EDGE", vec=g.vec((1.0, 1.0, 0.5)))
    gcrack = g.fac(gator, 0.05, 0.0)
    char = g.ramp(g.noise(12.0, 5.0, 0.6), [(0.2, "080706"), (0.7, "1E1A16")])
    char = g.mix(g.math("MULTIPLY", gcrack, 0.95), char, srgb("020101"))
    col = g.mix(burn, wood, char)
    ash = g.math("MULTIPLY", g.fac(g.up(), 0.3, 0.9), g.fac(g.noise(16.0, 5.0, 0.6), 0.5, 0.7))
    col = g.mix(g.math("MULTIPLY", ash, 0.6), col, srgb("7A746C"))
    soot = g.drips(20.0, 0.07, 0.62, 0.76)
    col = g.mix(g.math("MULTIPLY", soot, 0.6), col, srgb("050404"))
    col = _grime(g, col, 0.03, 1.8, "040302")
    deep = g.math("MULTIPLY", g.math("MULTIPLY", gcrack, burn), g.fac(g.noise(5.0, 4.0, 0.6), 0.62, 0.75))
    ember = g.ramp(g.noise(30.0, 3.0), [(0.3, "5A0C02"), (0.8, "C83C0A")])
    col = g.mix(g.math("MULTIPLY", deep, 0.7), col, g.mix(0.3, srgb("0E0302"), ember))
    rough = g.lerpf(burn, 0.78, 0.9)
    h = g.math("ADD", g.math("MULTIPLY", wl2, 0.6), g.math("MULTIPLY", grain, 0.3))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", g.math("MULTIPLY", gcrack, burn), 1.4))
    return g.finish(col, rough, g.bump(h, 0.9, 0.003), emis=g.mix(deep, (0, 0, 0, 1), ember), emis_strength=0.6,
                    fallback="463C30")


def ember_eye(name):
    """Yeux de braise au fond des orbites : cœur jaune-orange vers l'avant, bord de croûte rouge sombre, vitreux."""
    g = HB(name)
    geo = g.n("ShaderNodeNewGeometry")
    sep = g.n("ShaderNodeSeparateXYZ")
    g.l(geo.outputs["Normal"], sep.inputs[0])
    front = g.math("MULTIPLY", sep.outputs["Y"], -1.0)
    core = g.fac(front, 0.55, 0.95)
    n1 = g.noise(30.0, 4.0, 0.6, 1.2)
    hot = g.ramp(n1, [(0.3, "A01E04"), (0.65, "F05A10"), (0.9, "FFA040")])
    crust = g.ramp(n1, [(0.3, "1A0402"), (0.8, "5A1004")])
    col = g.mix(core, crust, hot)
    emis = g.mix(core, (0.12, 0.01, 0.0, 1), hot)
    return g.finish(g.mix(1.0, col, (0.2, 0.2, 0.2, 1), "MULTIPLY"), 0.1, None, emis=emis, emis_strength=2.6,
                    fallback="FF6A1A")


def ash_smear(name):
    """Traces de cendre et de kaolin délavées (pigment rituel inventé), écaillées, sales."""
    g = HB(name)
    col = g.ramp(g.noise(30.0, 4.0, 0.6), [(0.3, "34312C"), (0.7, "6E6A62")])
    chip = g.fac(g.noise(18.0, 6.0, 0.7, 0.3), 0.42, 0.55)
    col = g.mix(chip, col, srgb("14110E"))
    col = _grime(g, col, 0.02, 1.6, "0A0806")
    return g.finish(col, 0.92, g.bump(g.math("ADD", chip, g.math("MULTIPLY", g.noise(200.0), 0.3)), 0.3, 0.001),
                    fallback="8A8478")


def scorched_rope(name):
    """Corde roussie : fibres CC0 noircies, cendre grise, fibres cramées plus claires."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/Rope001")
    v = g.vec((14.0, 14.0, 14.0))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.3)
    lu = real_mats.lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, "0A0807"), (0.65, "3A2C20")])
    ash = g.fac(g.noise(9.0, 4.0, 0.6), 0.55, 0.7)
    col = g.mix(g.math("MULTIPLY", ash, 0.7), col, srgb("6A6458"))
    col = _grime(g, col, 0.012, 1.6, "050403")
    h = lu
    if maps["height"]:
        h = real_mats.lum(g, real_mats.image_node(g, maps["height"], v, "Non-Color").outputs["Color"])
    return g.finish(col, 0.9, g.bump(h, 0.7, 0.002), fallback="2A2018")


def char_cloth(name):
    """Bandes de tissu brûlées : lin roussi, bords noirs, taches de suie."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/hessian_230")
    v = g.vec((9.0, 9.0, 9.0))
    img = real_mats.image_node(g, maps["color"], v, "sRGB")
    lu = real_mats.lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, "1E1810"), (0.6, "5A4C38")])
    burn = g.fac(g.noise(5.0, 5.0, 0.65), 0.5, 0.66)
    col = g.mix(burn, col, srgb("0A0806"))
    col = _grime(g, col, 0.015, 1.4, "080604")
    return g.finish(col, 0.9, g.bump(lu, 0.6, 0.0015), fallback="4A3E2E")


def lava_rock(name):
    """Tête de massue : basalte bulleux, failles de braise, croûte de cendre."""
    g = HB(name)
    ves = g.fac(g.voro(70.0), 0.2, 0.0)
    col = g.ramp(g.noise(10.0, 5.0, 0.6), [(0.2, "0C0A0A"), (0.7, "2A2420")])
    col = g.mix(g.math("MULTIPLY", ves, 0.8), col, srgb("030202"))
    cr = g.fac(g.voro(9.0, "DISTANCE_TO_EDGE"), 0.03, 0.0)
    live = g.fac(g.noise(4.0, 3.0), 0.4, 0.65)
    heat = g.math("MULTIPLY", cr, live)
    ember = g.ramp(g.noise(26.0, 3.0), [(0.3, "8A1604"), (0.8, "FF6A1A")])
    col = g.mix(g.math("MULTIPLY", heat, 0.9), col, g.mix(0.6, srgb("200604"), ember, "MULTIPLY"))
    ash = g.math("MULTIPLY", g.fac(g.up(), 0.4, 0.95), g.fac(g.noise(12.0, 4.0), 0.45, 0.7))
    col = g.mix(g.math("MULTIPLY", ash, 0.6), col, srgb("6E6860"))
    h = g.math("SUBTRACT", g.math("MULTIPLY", g.noise(40.0, 5.0), 0.5), g.math("ADD", g.math("MULTIPLY", ves, 0.8),
                                                                          g.math("MULTIPLY", cr, 1.0)))
    return g.finish(col, 0.86, g.bump(h, 0.9, 0.0025), emis=g.mix(heat, (0, 0, 0, 1), ember), emis_strength=1.2,
                    fallback="1C1816")


def char_handle(name):
    """Manche calciné : bois noir craquelé, cendre, prise lustrée par les mains."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/rough_wood")
    v = g.vec((6.0, 6.0, 2.0))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.3)
    wl = real_mats.lum(g, img.outputs["Color"])
    gator = g.fac(g.voro(40.0, "DISTANCE_TO_EDGE", vec=g.vec((1.0, 1.0, 0.4))), 0.05, 0.0)
    col = g.ramp(wl, [(0.1, "080605"), (0.6, "2A1E16")])
    col = g.mix(g.math("MULTIPLY", gator, 0.9), col, srgb("020101"))
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(12.0, 4.0), 0.6, 0.72), 0.6), col, srgb("5E5850"))
    return g.finish(col, 0.7, g.bump(g.math("SUBTRACT", wl, gator), 0.8, 0.002), fallback="1A140F")


# ======================================================================== GUERRIER COCO (bourre lacérée, sève)
def husk_rot(name, wounds=None):
    """Bourre de coco pourrissante : Bark014 remappée plus sombre, mèches arrachées, moisissure vert-noir ; plaies
    (tracés de horror_geo, distance calculée dans le shader en espace objet) : chair de coco fibreuse et lacérée,
    crème rosé, bordée de sève ambrée séchée qui coule en filets ; humidité dans les creux et les coulures."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/Bark014")
    v = g.vec((2.4, 2.4, 1.6))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.35)
    lu = real_mats.lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, "100804"), (0.35, "2E1A0C"), (0.65, "5A3A1E")])
    fib = g.noise(3.0, 8.0, 0.7, 0.0, vec=g.vec((70.0, 70.0, 4.0)))
    col = g.mix(g.math("MULTIPLY", g.fac(fib, 0.45, 0.65), 0.45), col, srgb("7A5432"))
    col = g.mix(g.math("MULTIPLY", g.fac(fib, 0.40, 0.25), 0.6), col, srgb("0A0502"))
    mold = g.math("MULTIPLY", g.fac(g.noise(2.6, 5.0, 0.62, 0.5), 0.56, 0.7), g.fac(g.noise(18.0, 4.0), 0.4, 0.7))
    col = g.mix(g.math("MULTIPLY", mold, 0.8), col, g.ramp(g.noise(40.0), [(0.3, "141A0E"), (0.7, "34402A")]))
    inner, rim, drips = 0.0, 0.0, 0.0
    if wounds:
        ratio = None
        for w in wounds:
            P = w["pts"]
            for i in range(len(P) - 1):
                r = g.math("DIVIDE", g.seg_dist(P[i], P[i + 1]), w["w"])
                ratio = r if ratio is None else g.math("MINIMUM", ratio, r)
        inner = g.fac(ratio, 0.9, 0.35)
        rim = g.math("MULTIPLY", g.fac(ratio, 1.9, 1.0), g.math("SUBTRACT", 1.0, inner))
        drips = g.sap_drips(wounds)
    flesh_fib = g.noise(2.0, 6.0, 0.6, 0.0, vec=g.vec((220.0, 220.0, 14.0)))
    flesh = g.ramp(flesh_fib, [(0.25, "6A5236"), (0.55, "B49E7A"), (0.85, "DCCDAE")])
    flesh = g.mix(g.math("MULTIPLY", g.fac(g.noise(9.0, 4.0), 0.5, 0.7), 0.5), flesh, srgb("A86A4A"))
    sap = g.ramp(g.noise(30.0, 4.0), [(0.3, "3A1A02"), (0.7, "7A3A06")])
    sapf = g.clamp01(g.math("ADD", rim, drips))
    col = g.mix(g.math("MULTIPLY", sapf, 0.92), col, sap)
    col = g.mix(inner, col, flesh)
    col = _grime(g, col, 0.05, 1.5, "070402")
    wet = g.clamp01(g.math("ADD", g.math("MULTIPLY", g.drips(18.0, 0.06, 0.55, 0.72), 0.6),
                           g.math("ADD", g.math("MULTIPLY", inner, 0.9), sapf)))
    rough = g.math("MULTIPLY_ADD", fib, 0.18, 0.76)
    col, rough = g.wet_finish(col, rough, wet, 0.72, 0.14)
    hi = real_mats.image_node(g, maps["height"], v, "Non-Color", blend=0.35) if maps["height"] else None
    h = g.math("MULTIPLY", fib, 0.5)
    if hi is not None:
        h = g.math("ADD", g.math("MULTIPLY", hi.outputs["Color"], 0.7), h)
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", inner, 0.9))
    h = g.math("ADD", h, g.math("MULTIPLY", inner, g.math("MULTIPLY", flesh_fib, 0.8)))
    h = g.math("ADD", h, g.math("MULTIPLY", rim, 0.35))
    return g.finish(col, rough, g.bump(h, 0.8, 0.006), fallback="2E1A0C")


def skin_grim(name, c0="1E0E08", c1="3A1C10", c2="54301C", ochre="6A2A14", ochre_amt=0.45):
    """Peau des membres : maigre, sale, marbrée, cicatrices, veines, ocre rituelle délavée et écaillée, coulures de
    sève séchée, boue sur les mains et les pieds, sueur luisante dans les plis."""
    g = HB(name)
    base = g.ramp(g.noise(5.0, 4.0, 0.55), [(0.25, c0), (0.55, c1), (0.8, c2)])
    blot = g.fac(g.noise(14.0, 5.0, 0.6), 0.55, 0.75)
    base = g.mix(g.math("MULTIPLY", blot, 0.35), base, srgb("1A0E14"))
    och = g.ramp(g.noise(9.0, 5.0, 0.62), [(0.3, "3A1208"), (0.7, ochre)])
    cover = g.fac(g.noise(3.5, 6.0, 0.65, 0.6), 0.42, 0.62)
    chip = g.fac(g.noise(26.0, 6.0, 0.7), 0.45, 0.6)
    cover = g.math("MULTIPLY", g.math("MULTIPLY", cover, chip),
                   g.math("SUBTRACT", 1.0, g.math("MULTIPLY", g.cavity(0.05), 2.0, clamp=True)), clamp=True)
    base = g.mix(g.math("MULTIPLY", cover, ochre_amt), base, och)
    vein_f = g.noise(2.6, 6.0, 0.55, 1.8, vec=g.vec((1.0, 1.0, 2.2)))
    veins = g.fac(g.math("ABSOLUTE", g.math("SUBTRACT", vein_f, 0.5)), 0.018, 0.0)
    col = g.mix(g.math("MULTIPLY", veins, 0.55), base, srgb("140808"))
    scars = g.math("MULTIPLY", g.fac(g.edges(3.0, g.vec((1.0, 1.0, 0.35))), 0.012, 0.0),
                   g.fac(g.noise(2.0, 2.0), 0.55, 0.66))
    col = g.mix(g.math("MULTIPLY", scars, 0.85), col, srgb("8A6052"))
    sap = g.math("MULTIPLY", g.drips(26.0, 0.05, 0.64, 0.74), g.fac(g.noise(3.0, 3.0), 0.5, 0.7))
    col = g.mix(g.math("MULTIPLY", sap, 0.85), col, srgb("5A2A04"))
    mud = g.fac(g.z(), 0.16, 0.04)
    col = g.mix(g.math("MULTIPLY", mud, 0.8), col, srgb("1E160E"))
    col = _grime(g, col, 0.06, 1.9, "080402")
    pores = g.noise(320.0, 2.0, 0.5)
    h = g.math("ADD", g.math("MULTIPLY", pores, 0.3), g.math("MULTIPLY", veins, 0.7))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", scars, 0.9))
    wet = g.clamp01(g.math("ADD", g.math("MULTIPLY", g.cavity(0.03), 0.35), g.math("MULTIPLY", sap, 0.9)))
    rough = g.math("MULTIPLY_ADD", g.noise(11.0, 3.0), 0.2, 0.58)
    col, rough = g.wet_finish(col, rough, wet, 0.8, 0.28)
    return g.finish(col, rough, g.bump(h, 0.65, 0.0025), sss=0.02, fallback=c1)


def bandage(name):
    """Bandes de lin souillées : trame CC0 (hessian), crasse, auréoles de sève ambrée et de boue, bords effilochés
    plus sombres."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/hessian_230")
    v = g.vec((11.0, 11.0, 11.0))
    img = real_mats.image_node(g, maps["color"], v, "sRGB")
    lu = real_mats.lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, "5A5040"), (0.6, "A69A80")])
    st = g.fac(g.noise(4.0, 5.0, 0.62, 0.4), 0.5, 0.66)
    col = g.mix(g.math("MULTIPLY", st, 0.75), col, srgb("5A3008"))
    dirt = g.fac(g.noise(7.0, 5.0, 0.6), 0.55, 0.75)
    col = g.mix(g.math("MULTIPLY", dirt, 0.7), col, srgb("2A2216"))
    col = _grime(g, col, 0.015, 1.8, "0C0804")
    rough = g.lerpf(st, 0.9, 0.5)
    return g.finish(col, rough, g.bump(lu, 0.7, 0.0015), fallback="8A7E64")


def wood_mask_faded(name, eyes=None):
    """Masque du Guerrier : bois sculpté gris-brun gorgé d'eau (veinage CC0), ocre presque partie (pigments
    délavés dans les creux), moisissure noire dans les fentes, coulures sombres de sève qui partent des orbites
    (eyes : centres des yeux en espace objet), crasse dans les creux."""
    mats = real_mats.T["mats"]
    g = HB(name)
    maps = mats.find_maps("characters/fine_grained_wood")
    v = g.vec((4.0, 4.0, 1.4))
    img = real_mats.image_node(g, maps["color"], v, "sRGB", blend=0.25)
    wl = real_mats.lum(g, img.outputs["Color"])
    grain = g.noise(2.0, 7.0, 0.62, 1.4, vec=g.vec((120.0, 120.0, 4.0)))
    wl2 = g.math("ADD", g.math("MULTIPLY", wl, 0.6), g.math("MULTIPLY", grain, 0.45))
    wood = g.ramp(wl2, [(0.15, "16100C"), (0.42, "3A2E24"), (0.7, "5E5040"), (0.9, "7A6C58")])
    cav = g.cavity(0.03)
    ochre = g.math("MULTIPLY", g.fac(cav, 0.1, 0.5), g.fac(g.noise(7.0, 5.0, 0.65), 0.4, 0.62))
    col = g.mix(g.math("MULTIPLY", ochre, 0.75), wood, g.ramp(g.noise(14.0, 4.0), [(0.3, "3A1408"), (0.7, "6A2A10")]))
    cr = g.fac(g.edges(5.0, g.vec((1.0, 1.0, 0.25))), 0.016, 0.0)
    cr = g.math("MULTIPLY", cr, g.fac(g.noise(3.0, 2.0), 0.4, 0.6))
    col = g.mix(g.math("MULTIPLY", cr, 0.95), col, srgb("050302"))
    mold = g.math("MULTIPLY", g.fac(g.noise(4.0, 5.0, 0.62), 0.55, 0.7), g.math("ADD", 0.3, cav))
    col = g.mix(g.math("MULTIPLY", g.clamp01(mold), 0.7), col, g.ramp(g.noise(50.0), [(0.3, "0A0C06"), (0.7, "1E2414")]))
    tears = 0.0
    if eyes:
        tears = g.sap_drips([dict(pts=[Vector(e) + Vector((-0.02, 0, 0.0)), Vector(e) + Vector((0.02, 0, -0.004))],
                                  w=0.01) for e in eyes], step=0.009, width=0.008, axes=(1.0, 0.0, 0.0),
                            length=(0.06, 0.2))
        col = g.mix(g.math("MULTIPLY", tears, 0.9), col, g.ramp(g.noise(30.0), [(0.3, "3A1804"), (0.7, "6A3208")]))
    col = _grime(g, col, 0.03, 2.2, "060402")
    rough = g.lerpf(g.clamp01(g.math("ADD", tears, g.math("MULTIPLY", mold, 0.3))), 0.82, 0.25)
    h = g.math("ADD", g.math("MULTIPLY", wl2, 0.5), g.math("MULTIPLY", grain, 0.2))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", cr, 1.2))
    return g.finish(col, rough, g.bump(h, 0.7, 0.003), fallback="3A2E24")


def eye_bloodshot(name):
    """Œil injecté : sclère jaunie, veinules rouges épaisses, iris brun voilé, pupille noire, très lisse ;
    émission faible (jaune malade) pour la nuit."""
    return milky_eye(name, globe="8E7A6C", iris="2A1A0E", pupil="050303", vein="6A0602", vein_amt=1.0, glow="A89070",
                     glow_k=0.35, iris_r=(0.86, 0.95), pupil_r=(0.97, 0.992), glow_on="sclera")


def tooth_yellow(name):
    return real_mats.mat_tooth(name, light="C8B890", dark="7A6A44")


def lime_faded(name):
    """Chaux des gravures, sale et écaillée : on la devine plus qu'on ne la voit."""
    g = HB(name)
    col = g.ramp(g.noise(30.0, 4.0, 0.6), [(0.3, "3E382C"), (0.7, "6E6656")])
    chip = g.fac(g.noise(20.0, 6.0, 0.7, 0.3), 0.48, 0.6)
    col = g.mix(chip, col, srgb("1E140C"))
    col = _grime(g, col, 0.02, 1.8, "0C0806")
    return g.finish(col, 0.88, g.bump(g.math("ADD", chip, g.math("MULTIPLY", g.noise(200.0), 0.3)), 0.3, 0.001),
                    fallback="5A5444")


def salt_groove(name):
    """Incision : creux sombre et humide, croûte de sel par plaques (le motif se lit en creux, pas en relief)."""
    g = HB(name)
    col = g.ramp(g.noise(30.0, 4.0), [(0.3, "0C0806"), (0.7, "241A12")])
    salt = g.fac(g.noise(40.0, 5.0, 0.7), 0.62, 0.7)
    col = g.mix(g.math("MULTIPLY", salt, 0.5), col, srgb("8A8478"))
    return g.finish(col, g.lerpf(salt, 0.62, 0.9), g.bump(salt, 0.3, 0.0008), fallback="2A2016")


def pandanus_rot(name):
    return real_mats.mat_leaf_strip(name, tint="7A6A3A", dark="2A2412", dry=0.96)


def cowrie_dirty(name):
    return real_mats.mat_shell(name, base="B8AC90", spot="3A2414", band="7A6A4A")


def flint_sap(name):
    """Lame de silex tachée de sève ambrée séchée (le « sang » des cocos, T-162)."""
    g = HB(name)
    fac = g.edges(30.0)
    col = g.ramp(g.noise(12.0, 5.0, 0.6), [(0.3, "1A1816"), (0.75, "4A4640")])
    col = g.mix(g.math("MULTIPLY", g.fac(fac, 0.03, 0.0), 0.5), col, srgb("8A847A"))
    stain = g.fac(g.noise(4.5, 6.0, 0.65, 0.5), 0.55, 0.68)
    col = g.mix(g.math("MULTIPLY", stain, 0.9), col, g.ramp(g.noise(30.0), [(0.3, "3A1A02"), (0.7, "6A3204")]))
    col = _grime(g, col, 0.02, 1.4, "0C0A06")
    rough = g.lerpf(stain, 0.28, 0.14)
    return g.finish(col, rough, g.bump(fac, 0.45, 0.002), fallback="4A4640")


# ======================================================================== couches de coupe (démembrement)
def gore_layer(name, kind):
    """Couches de coupe : celles du 2e PC (real_mats.gore_layer) + crabe (chitine, chair, hémolymphe bleue, T-162)
    et Veilleur (croûte, cendre rouge, cœur de braise, sang de lave, T-162)."""
    g = HB(name)
    if kind == "chitin_cut":           # tranche de carapace : brun-rouge dense, liseré clair interne
        st = g.noise(2.0, 5.0, 0.6, 0.0, vec=g.vec((40.0, 40.0, 400.0)))
        col = g.ramp(st, [(0.2, "1E0806"), (0.6, "4A1A10"), (0.95, "C8B49A")])
        return g.finish(col, 0.5, g.bump(st, 0.4, 0.0008), fallback="4A1A10")
    if kind == "crab_meat":            # chair de crabe : crème rosé fibreuse, veinée de rouge-brun, humide
        fib = g.noise(2.0, 6.0, 0.6, 0.0, vec=g.vec((300.0, 20.0, 300.0)))
        col = g.ramp(fib, [(0.25, "6A4A42"), (0.6, "A8867A"), (0.9, "C8AEA2")])
        col = g.mix(g.math("MULTIPLY", g.fac(g.noise(9.0, 4.0), 0.5, 0.72), 0.6), col, srgb("8A3A2A"))
        streak = g.fac(g.noise(2.0, 5.0, 0.6, 0.0, vec=g.vec((40.0, 400.0, 40.0))), 0.6, 0.72)
        col = g.mix(g.math("MULTIPLY", streak, 0.5), col, srgb("3A1A20"))
        col = g.mix(g.math("MULTIPLY", g.cavity(0.015), 1.6, clamp=True), col, srgb("2A1418"))
        return g.finish(col, g.math("MULTIPLY_ADD", fib, 0.15, 0.18), g.bump(fib, 0.5, 0.001), sss=0.2,
                        fallback="DCCBC2")
    if kind == "hemolymph":            # hémolymphe : gel bleu-gris laiteux (hémocyanine), très brillant
        col = g.ramp(g.noise(8.0, 3.0), [(0.3, "06101E"), (0.7, "1E3248")])
        col = g.mix(g.math("MULTIPLY", g.fac(g.noise(30.0, 3.0), 0.5, 0.8), 0.35), col, srgb("5A7890"))
        return g.finish(col, 0.05, g.bump(g.noise(12.0, 2.0), 0.15, 0.002), sss=0.3, fallback="2A4A6A")
    if kind == "crust_cut":            # tranche de croûte : charbon feuilleté
        st = g.noise(2.0, 5.0, 0.6, 0.0, vec=g.vec((60.0, 60.0, 500.0)))
        col = g.ramp(st, [(0.2, "050404"), (0.7, "2A2420")])
        return g.finish(col, 0.9, g.bump(st, 0.6, 0.001), fallback="1A1614")
    if kind == "cinder":               # cendre rouge poreuse, encore chaude par endroits
        ves = g.fac(g.voro(160.0), 0.2, 0.0)
        col = g.ramp(g.noise(20.0, 4.0), [(0.3, "2A0A06"), (0.7, "5A1A0C")])
        col = g.mix(g.math("MULTIPLY", ves, 0.8), col, srgb("080202"))
        hot = g.fac(g.noise(12.0, 4.0, 0.6), 0.55, 0.75)
        ember = g.ramp(g.noise(40.0, 3.0), [(0.3, "8A1604"), (0.8, "FF5A10")])
        return g.finish(g.mix(hot, col, srgb("3A0A04")), 0.75, g.bump(ves, 0.6, 0.001),
                        emis=g.mix(hot, (0, 0, 0, 1), ember), emis_strength=1.5, fallback="4A140A")
    if kind == "ember_core":           # cœur de braise / sang de lave : orange vif, lumineux
        n1 = g.noise(14.0, 5.0, 0.6, 1.2)
        col = g.ramp(n1, [(0.3, "B01C04"), (0.6, "FF5A10"), (0.85, "FFB04A")])
        return g.finish(g.mix(1.0, col, (0.2, 0.2, 0.2, 1), "MULTIPLY"), 0.3, None, emis=col, emis_strength=6.0,
                        fallback="FF5A10")
    if kind == "char_skin":            # peau carbonisée tranchée
        col = g.ramp(g.noise(40.0, 4.0), [(0.3, "0A0605"), (0.7, "2A1610")])
        return g.finish(col, 0.85, g.bump(g.noise(200.0), 0.3, 0.0005), fallback="1A0E0A")
    return real_mats.gore_layer(name, kind)


# profils de couches (épaisseur m ou None, fraction intérieure si None, retrait m) — même forme que real_gore
CRAB_CENTRAL = [("chitin_cut", 0.03, 0.0, 0.001), ("crab_meat", None, 0.30, 0.006), ("hemolymph", None, 0.0, 0.012)]
CRAB_LIMB = [("chitin_cut", 0.006, 0.0, 0.0004), ("crab_meat", None, 0.25, 0.003), ("hemolymph", None, 0.0, 0.004)]
CRAB_SOCKET = [("chitin_cut", 0.018, 0.0, 0.0), ("crab_meat", None, 0.3, 0.003), ("hemolymph", None, 0.0, 0.004)]
ASH_CENTRAL = [("crust_cut", 0.028, 0.0, 0.001), ("cinder", None, 0.40, 0.004), ("ember_core", None, 0.0, 0.009)]
ASH_LIMB = [("char_skin", 0.004, 0.0, 0.0004), ("cinder", None, 0.32, 0.003), ("ember_core", None, 0.0, 0.004)]
ASH_SOCKET = [("crust_cut", 0.022, 0.0, 0.0), ("cinder", None, 0.32, 0.003), ("ember_core", None, 0.0, 0.004)]
real_gore.PROFILES["crab"] = dict(central=CRAB_CENTRAL, limb=CRAB_LIMB, socket=CRAB_SOCKET)
real_gore.PROFILES["ash"] = dict(central=ASH_CENTRAL, limb=ASH_LIMB, socket=ASH_SOCKET)

# ordre des tuiles de l'atlas Gore (4 x 4) par profil
LAYER_ORDER = {
    "coco": ("skin", "fat", "muscle", "bone", "marrow", "husk_cut", "nut_shell", "meat", "pulp", "milk"),
    "crab": ("chitin_cut", "crab_meat", "hemolymph"),
    "ash": ("crust_cut", "cinder", "ember_core", "char_skin"),
}
