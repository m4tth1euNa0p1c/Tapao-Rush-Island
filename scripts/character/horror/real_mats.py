"""Matériaux procéduraux « réalistes » des mobs (repris de la maquette mob-coco-realiste du 2e PC).

Rapatrié tel quel de OneDrive/maquette-blender/factory/kits/coco_real_mats.py (2026-10-07) : seuls les imports
et le chargement changent (base.T, base.ROOT). Les variantes « horreur » (T-274) sont dans horror_mats.py.

Construits avec le petit constructeur de graphes NB (base.py) : coordonnées Generated en mètres (tiki
kit.fixed_texspace), textures CC0 de art/textures projetées en boîte, cavités par le nœud Ambient Occlusion (cuit dans
la couleur au bake), relief par Bump (cuit dans la normal map). Les pièces à UV propres (feuilles, lanières de pagne,
iris) lisent la couche "UVMap" (rendu) ; le bake écrit dans la couche active "UVBake".
"""
import math  # noqa: F401
import os

import bpy  # noqa: F401

from .base import NB as _NB
from .base import ROOT, T, srgb  # noqa: F401


def NB(name):
    return _NB(name)


def tex_path(*parts):
    return os.path.join(ROOT, "art", "textures", *parts)


def mat_leather(name):
    from .base import mat_leather as _ml
    return _ml(name)


def image_node(g, path, vec, colorspace="sRGB", box=True, blend=0.3):
    img = g.n("ShaderNodeTexImage")
    if box:
        img.projection = "BOX"
        img.projection_blend = blend
    img.image = T["mats"].load_image(path, colorspace)
    if vec is not None:
        g.l(vec, img.inputs["Vector"])
    return img


def uv_vec(g, layer="UVMap", scale=(1.0, 1.0, 1.0)):
    uvn = g.n("ShaderNodeUVMap")
    uvn.uv_map = layer
    m = g.n("ShaderNodeMapping")
    g.l(uvn.outputs["UV"], m.inputs["Vector"])
    m.inputs["Scale"].default_value = scale
    return m.outputs["Vector"]


def lum(g, col):
    n = g.n("ShaderNodeRGBToBW")
    g.l(col, n.inputs[0])
    return n.outputs[0]


# ------------------------------------------------------------------ bourre de coco (husk)
def mat_husk(name, dark="1E1006", mid="4E2E14", light="8A5A30", fibre="A87648", damp=0.0, density=(2.4, 2.4, 1.6)):
    """Bourre fibreuse : CC0 Bark014 remappée + mèches longitudinales (bruit étiré en Z), éraflures, crasse AO."""
    mats = T["mats"]
    g = NB(name)
    maps = mats.find_maps("characters/Bark014")
    v = g.vec(density)
    img = image_node(g, maps["color"], v, "sRGB", blend=0.35)
    lu = lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, dark), (0.35, mid), (0.65, light)])
    fib = g.noise(3.0, 8.0, 0.7, 0.0, vec=g.vec((70.0, 70.0, 4.0)))
    fib2 = g.noise(2.0, 6.0, 0.6, 0.0, vec=g.vec((160.0, 160.0, 9.0)))
    col = g.mix(g.math("MULTIPLY", g.fac(fib, 0.45, 0.65), 0.55), col, srgb(fibre))
    col = g.mix(g.math("MULTIPLY", g.fac(fib, 0.40, 0.25), 0.6), col, srgb("140A04"))
    col = g.mix(g.math("MULTIPLY", g.fac(fib2, 0.55, 0.7), 0.35), col, srgb("C49A68"))
    scr = g.fac(g.noise(2.0, 4.0, 0.6, 0.0, vec=g.vec((4.0, 4.0, 45.0))), 0.70, 0.74)
    col = g.mix(g.math("MULTIPLY", scr, 0.45), col, srgb("B48A5E"))
    if damp:
        wet = g.fac(g.noise(1.6, 3.0, 0.5), 0.5, 0.7)
        col = g.mix(g.math("MULTIPLY", wet, damp), col, g.mix(0.6, col, srgb("120A06"), "MULTIPLY"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.05), 1.4, clamp=True), col, srgb("0C0603"))
    hi = image_node(g, maps["height"], v, "Non-Color", blend=0.35) if maps["height"] else None
    h = g.math("MULTIPLY", fib, 0.5)
    if hi is not None:
        h = g.math("ADD", g.math("MULTIPLY", hi.outputs["Color"], 0.7), h)
    h = g.math("ADD", h, g.math("MULTIPLY", fib2, 0.25))
    rough = g.math("MULTIPLY_ADD", fib, 0.2, 0.74)
    return g.finish(col, rough, g.bump(h, 0.8, 0.006), fallback=mid)


def mat_fibre(name, dark="24140A", light="8A6038"):
    """Fibres de coir libres (mèches, touffes) : brins brun-roux, extrémités claires."""
    g = NB(name)
    n = g.noise(4.0, 6.0, 0.6, 0.0, vec=g.vec((90.0, 90.0, 9.0)))
    col = g.ramp(n, [(0.25, dark), (0.75, light)])
    col = g.mix(g.math("MULTIPLY", g.cavity(0.02), 1.2, clamp=True), col, srgb("0A0502"))
    return g.finish(col, g.math("MULTIPLY_ADD", n, 0.15, 0.72), g.bump(g.noise(300.0, 2.0), 0.4, 0.001),
                    fallback=light)


# ------------------------------------------------------------------ peau
def mat_skin(name, c0="3A160C", c1="6A2A18", c2="8C4026", vein="2A0C08", scar="B07062", sss=0.03, hair=0.0,
             ochre=None, ochre_amt=0.0):
    """Peau réaliste : marbrures, veines, pores, cicatrices pâles, plis sombres (AO), SSS léger ; option : ocre rouge
    frottée sur la peau (pigment rituel), usée aux plis et aux articulations."""
    g = NB(name)
    base = g.ramp(g.noise(5.0, 4.0, 0.55), [(0.25, c0), (0.55, c1), (0.8, c2)])
    blot = g.fac(g.noise(14.0, 5.0, 0.6), 0.55, 0.75)
    base = g.mix(g.math("MULTIPLY", blot, 0.3), base, srgb(c0))
    if ochre:
        och = g.ramp(g.noise(9.0, 5.0, 0.62), [(0.3, "5A1608"), (0.7, ochre)])
        cover = g.fac(g.noise(3.5, 6.0, 0.65, 0.6), 0.32, 0.62)
        cover = g.math("MULTIPLY", cover, g.math("SUBTRACT", 1.0, g.math("MULTIPLY", g.cavity(0.05), 2.0, clamp=True)),
                       clamp=True)
        base = g.mix(g.math("MULTIPLY", cover, ochre_amt), base, och)
    vein_f = g.noise(2.6, 6.0, 0.55, 1.8, vec=g.vec((1.0, 1.0, 2.2)))
    veins = g.fac(g.math("ABSOLUTE", g.math("SUBTRACT", vein_f, 0.5)), 0.016, 0.0)
    veins = g.math("MULTIPLY", veins, g.fac(g.noise(3.0, 2.0), 0.35, 0.6))
    col = g.mix(g.math("MULTIPLY", veins, 0.5), base, srgb(vein))
    scars = g.math("MULTIPLY", g.fac(g.edges(3.0, g.vec((1.0, 1.0, 0.35))), 0.010, 0.0),
                   g.fac(g.noise(2.0, 2.0), 0.58, 0.66))
    col = g.mix(g.math("MULTIPLY", scars, 0.8), col, srgb(scar))
    if hair:
        hairs = g.fac(g.noise(3.0, 3.0, 0.5, 0.0, vec=g.vec((600.0, 600.0, 120.0))), 0.72, 0.8)
        col = g.mix(g.math("MULTIPLY", hairs, hair), col, srgb("120806"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.06), 1.8, clamp=True), col, srgb("100402"))
    pores = g.noise(320.0, 2.0, 0.5)
    wrinkle = g.fac(g.noise(1.0, 3.0, 0.5, 0.0, vec=g.vec((60.0, 60.0, 6.0))), 0.45, 0.55)
    h = g.math("ADD", g.math("MULTIPLY", pores, 0.3), g.math("MULTIPLY", veins, 0.7))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", scars, 0.8))
    h = g.math("ADD", h, g.math("MULTIPLY", wrinkle, 0.18))
    rough = g.math("MULTIPLY_ADD", g.noise(11.0, 3.0), 0.25, 0.48)
    return g.finish(col, rough, g.bump(h, 0.6, 0.0025), sss=sss, fallback=c1)


def mat_nail(name, light="B8A68A", dark="5A4A3A"):
    """Ongles / griffes de kératine : stries longitudinales, bout plus sombre et sale."""
    g = NB(name)
    st = g.noise(3.0, 4.0, 0.6, 0.0, vec=g.vec((260.0, 260.0, 20.0)))
    col = g.ramp(st, [(0.3, dark), (0.75, light)])
    col = g.mix(g.math("MULTIPLY", g.cavity(0.01), 1.5, clamp=True), col, srgb("1A120C"))
    return g.finish(col, g.math("MULTIPLY_ADD", st, 0.2, 0.30), g.bump(st, 0.25, 0.001), fallback=light)


# ------------------------------------------------------------------ bois sculpté (masque tiki)
def mat_wood_carved(name, paint="A8561E", paint_dark="5A240A", paint_amt=0.72, wood_set="fine_grained_wood"):
    """Bois sculpté : veinage CC0 contrasté (étiré verticalement), fentes de séchage, peinture ocre mince et usée
    (le bois réapparaît), coulures de crasse, crasse AO dans les creux, éclats clairs."""
    mats = T["mats"]
    g = NB(name)
    maps = mats.find_maps("characters/" + wood_set)
    v = g.vec((4.0, 4.0, 1.2))
    img = image_node(g, maps["color"], v, "sRGB", blend=0.25)
    wl = lum(g, img.outputs["Color"])
    grain = g.noise(2.0, 7.0, 0.62, 1.4, vec=g.vec((120.0, 120.0, 4.0)))
    wl2 = g.math("ADD", g.math("MULTIPLY", wl, 0.6), g.math("MULTIPLY", grain, 0.5))
    wood = g.ramp(wl2, [(0.15, "1E0E06"), (0.42, "4A2A14"), (0.7, "7E5230"), (0.9, "A87A4A")])
    chip = g.fac(g.noise(7.0, 7.0, 0.7, 0.5), 0.47, 0.55)              # 1 = peinture partie
    thin = g.fac(grain, 0.35, 0.75)                                    # peinture mince : le veinage transparaît
    pcol = g.ramp(g.noise(14.0, 4.0, 0.6), [(0.3, paint_dark), (0.75, paint)])
    pcol = g.mix(g.math("MULTIPLY", thin, 0.45), pcol, g.mix(0.6, wood, pcol, "MULTIPLY"))
    has_paint = g.math("MULTIPLY", g.math("SUBTRACT", 1.0, chip), paint_amt)
    col = g.mix(has_paint, wood, pcol)
    streak = g.fac(g.noise(2.0, 5.0, 0.6, 0.0, vec=g.vec((14.0, 14.0, 1.5))), 0.55, 0.75)
    col = g.mix(g.math("MULTIPLY", streak, 0.45), col, srgb("1A0C04"))
    cr = g.fac(g.edges(5.0, g.vec((1.0, 1.0, 0.25))), 0.014, 0.0)    # fentes de séchage verticales
    cr = g.math("MULTIPLY", cr, g.fac(g.noise(3.0, 2.0), 0.45, 0.6))
    col = g.mix(g.math("MULTIPLY", cr, 0.95), col, srgb("080402"))
    edge = g.fac(g.noise(40.0, 5.0, 0.7), 0.70, 0.76)
    col = g.mix(g.math("MULTIPLY", edge, 0.3), col, srgb("B08A5A"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.03), 2.2, clamp=True), col, srgb("0C0602"))
    rough = g.mix(has_paint, (0.82, 0.82, 0.82, 1), (0.72, 0.72, 0.72, 1))
    h = g.math("ADD", g.math("MULTIPLY", wl2, 0.5), g.math("MULTIPLY", has_paint, 0.3))
    h = g.math("SUBTRACT", h, g.math("MULTIPLY", cr, 1.2))
    return g.finish(col, rough, g.bump(h, 0.7, 0.003), fallback=paint)


def mat_lime(name, base="D2C6A8", dirt="4A3620"):
    """Chaux / kaolin blanc dans les gravures : écaillé, sale dans les creux."""
    g = NB(name)
    col = g.ramp(g.noise(30.0, 4.0, 0.6), [(0.3, "9A8E72"), (0.7, base)])
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(6.0, 4.0), 0.5, 0.7), 0.5), col, srgb("6A5434"))
    chip = g.fac(g.noise(20.0, 6.0, 0.7, 0.3), 0.55, 0.62)
    col = g.mix(chip, col, srgb("4A2E18"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.02), 1.6, clamp=True), col, srgb(dirt))
    return g.finish(col, 0.85, g.bump(g.math("ADD", chip, g.math("MULTIPLY", g.noise(200.0), 0.3)), 0.3, 0.001),
                    fallback=base)


def mat_dark(name, col="0C0705", rough=0.85):
    """Fond carbonisé (orbites, bouche) : noir-brun, légère texture."""
    g = NB(name)
    c = g.ramp(g.noise(20.0, 3.0), [(0.3, "050302"), (0.7, col)])
    return g.finish(c, rough, g.bump(g.noise(80.0), 0.2, 0.001), fallback=col)


def mat_tooth(name, light="EEE6D0", dark="B8A884", tip="FFFFFF"):
    """Dents de requin : émail blanc-ivoire, base plus jaune et sale, très légère translucidité."""
    g = NB(name)
    col = g.ramp(g.noise(18.0, 4.0, 0.6), [(0.3, dark), (0.75, light)])
    col = g.mix(g.math("MULTIPLY", g.cavity(0.012), 1.8, clamp=True), col, srgb("4A3A20"))
    return g.finish(col, g.math("MULTIPLY_ADD", g.noise(30.0), 0.15, 0.22), g.bump(g.noise(160.0), 0.15, 0.0008),
                    sss=0.05, fallback=light)


# ------------------------------------------------------------------ accessoires
def mat_leaf_strip(name, tint="B49A58", dark="5A5228", dry=0.75):
    """Lanières de pandanus séché (UV : u en travers, v le long) : texture CC0 pandanus désaturée vers la paille."""
    g = NB(name)
    vec = uv_vec(g)
    img = image_node(g, tex_path("plants", "pandanus_basecolor.png"), vec, "sRGB", box=False)
    col = g.mix(dry, img.outputs["Color"], g.ramp(lum(g, img.outputs["Color"]), [(0.1, dark), (0.6, tint)]))
    spots = g.fac(g.noise(18.0, 5.0, 0.6), 0.62, 0.7)
    col = g.mix(g.math("MULTIPLY", spots, 0.5), col, srgb("3A2810"))
    edge_burn = g.fac(g.noise(4.0, 3.0), 0.55, 0.75)
    col = g.mix(g.math("MULTIPLY", edge_burn, 0.3), col, srgb("6A4A22"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.02), 1.3, clamp=True), col, srgb("1A1206"))
    nrm = g.n("ShaderNodeNormalMap")
    nimg = image_node(g, tex_path("plants", "pandanus_normal_gl.png"), vec, "Non-Color", box=False)
    g.l(nimg.outputs["Color"], nrm.inputs["Color"])
    nrm.inputs["Strength"].default_value = 1.0
    return g.finish(col, 0.7, nrm.outputs["Normal"], fallback=tint)


def mat_eophyll(name):
    """Jeunes feuilles de germe de coco (texture CC0 coconut_eophyll, UV)."""
    g = NB(name)
    vec = uv_vec(g)
    img = image_node(g, tex_path("plants", "coconut_eophyll_basecolor.png"), vec, "sRGB", box=False)
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(6.0, 4.0), 0.6, 0.75), 0.6), img.outputs["Color"], srgb("7A6A2A"))
    nrm = g.n("ShaderNodeNormalMap")
    nimg = image_node(g, tex_path("plants", "coconut_eophyll_normal_gl.png"), vec, "Non-Color", box=False)
    g.l(nimg.outputs["Color"], nrm.inputs["Color"])
    rimg = image_node(g, tex_path("plants", "coconut_eophyll_roughness.png"), vec, "Non-Color", box=False)
    return g.finish(col, lum(g, rimg.outputs["Color"]), nrm.outputs["Normal"], sss=0.05, fallback="5A7A2A")


def mat_weave(name, light="C8A868", dark="6A4E24", scale=60.0):
    """Tressage (bandeaux, ceinture, sandales) : damier de brins croisés + bruit, creux sombres."""
    g = NB(name)
    w1 = g.n("ShaderNodeTexWave", wave_type="BANDS", bands_direction="X")
    g.l(g.vec((scale, scale, scale)), w1.inputs["Vector"])
    w1.inputs["Distortion"].default_value = 1.5
    w1.inputs["Detail"].default_value = 2.0
    w2 = g.n("ShaderNodeTexWave", wave_type="BANDS", bands_direction="Z")
    g.l(g.vec((scale, scale, scale)), w2.inputs["Vector"])
    w2.inputs["Distortion"].default_value = 1.5
    chk = g.n("ShaderNodeTexChecker")
    g.l(g.vec((scale * 0.5, scale * 0.5, scale * 0.5)), chk.inputs["Vector"])
    f = g.mix(chk.outputs["Fac"], w1.outputs["Fac"], w2.outputs["Fac"])
    fl = lum(g, f)
    col = g.ramp(fl, [(0.15, dark), (0.85, light)])
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(5.0, 4.0), 0.55, 0.75), 0.4), col, srgb("4A3818"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.015), 1.5, clamp=True), col, srgb("140C04"))
    return g.finish(col, 0.82, g.bump(fl, 0.5, 0.002), fallback=light)


def mat_rope(name, dark="3A2410", light="9A7444"):
    mats = T["mats"]
    g = NB(name)
    maps = mats.find_maps("characters/Rope001")
    v = g.vec((14.0, 14.0, 14.0))
    img = image_node(g, maps["color"], v, "sRGB", blend=0.3)
    lu = lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, dark), (0.6, light)])
    col = g.mix(g.math("MULTIPLY", g.cavity(0.012), 1.5, clamp=True), col, srgb("100804"))
    h = lu
    if maps["height"]:
        h = lum(g, image_node(g, maps["height"], v, "Non-Color").outputs["Color"])
    return g.finish(col, 0.88, g.bump(h, 0.6, 0.002), fallback=light)


def mat_shell(name, base="EEE2C6", spot="7A5030", band="C8A070"):
    """Coquillage (cauri) : vernis brillant crème, taches brunes, bande dorée, légère SSS."""
    g = NB(name)
    col = g.ramp(g.noise(6.0, 3.0), [(0.3, band), (0.7, base)])
    sp = g.fac(g.edges(70.0), 0.03, 0.0)
    sp = g.math("MULTIPLY", sp, g.fac(g.noise(9.0, 3.0), 0.45, 0.6))
    col = g.mix(g.math("MULTIPLY", sp, 0.8), col, srgb(spot))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.01), 2.0, clamp=True), col, srgb("2A1A0A"))
    return g.finish(col, 0.18, g.bump(g.noise(120.0), 0.08, 0.0005), sss=0.06, fallback=base)


def mat_flint(name, light="5A5650", dark="1A1816"):
    """Silex / obsidienne terne : facettes conchoïdales (voronoi), arêtes plus claires, brillance."""
    g = NB(name)
    fac = g.edges(30.0)
    col = g.ramp(g.noise(12.0, 5.0, 0.6), [(0.3, dark), (0.75, light)])
    col = g.mix(g.math("MULTIPLY", g.fac(fac, 0.03, 0.0), 0.5), col, srgb("A09A90"))
    stain = g.fac(g.noise(4.5, 6.0, 0.65, 0.5), 0.62, 0.70)
    col = g.mix(g.math("MULTIPLY", stain, 0.7), col, srgb("3A0A08"))
    rough = g.mix(stain, (0.22, 0.22, 0.22, 1), (0.45, 0.45, 0.45, 1))
    return g.finish(col, rough, g.bump(fac, 0.45, 0.002), fallback=light)


def mat_wood_raw(name):
    """Hampe en bois brut : écorce CC0 bark_willow par plaques, bois écorcé clair dessous, taches de main."""
    mats = T["mats"]
    g = NB(name)
    maps = mats.find_maps("characters/bark_willow")
    v = g.vec((6.0, 6.0, 6.0))
    img = image_node(g, maps["color"], v, "sRGB", blend=0.3)
    bl = lum(g, img.outputs["Color"])
    bark = g.ramp(bl, [(0.05, "140A04"), (0.6, "54361E")])
    peeled = g.fac(g.noise(3.0, 5.0, 0.6, 0.0, vec=g.vec((6.0, 6.0, 1.5))), 0.5, 0.56)
    wood = g.ramp(g.noise(1.5, 6.0, 0.6, 0.0, vec=g.vec((80.0, 80.0, 3.0))), [(0.3, "4A3018"), (0.75, "7A5A34")])
    col = g.mix(peeled, bark, wood)
    grime = g.fac(g.noise(4.0, 4.0), 0.55, 0.7)
    col = g.mix(g.math("MULTIPLY", grime, 0.5), col, srgb("1A1008"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.02), 1.5, clamp=True), col, srgb("050302"))
    h = g.math("ADD", g.math("MULTIPLY", bl, g.math("SUBTRACT", 1.0, peeled)), g.math("MULTIPLY", peeled, -0.3))
    return g.finish(col, g.math("MULTIPLY_ADD", peeled, -0.2, 0.82), g.bump(h, 0.7, 0.003), fallback="4A3020")


def mat_feather(name):
    """Plume noire à reflets : barbes (bruit très étiré), brillance bleutée."""
    g = NB(name)
    b = g.noise(2.0, 4.0, 0.6, 0.0, vec=g.vec((400.0, 400.0, 30.0)))
    col = g.ramp(b, [(0.3, "040404"), (0.75, "1C1E26")])
    return g.finish(col, 0.42, g.bump(b, 0.3, 0.0008), fallback="121418")


# ------------------------------------------------------------------ visage du villageois
def mat_nutshell(name, dark="3A200E", light="8A5430"):
    """Coque de noix (endocarpe) découverte : brun profond poli, sillons longitudinaux, restes de fibres."""
    g = NB(name)
    gro = g.noise(2.0, 6.0, 0.6, 0.0, vec=g.vec((70.0, 70.0, 5.0)))
    col = g.ramp(g.noise(5.0, 4.0, 0.6), [(0.3, dark), (0.75, light)])
    col = g.mix(g.math("MULTIPLY", g.fac(gro, 0.5, 0.7), 0.4), col, srgb("120804"))
    fib = g.fac(g.noise(3.0, 5.0, 0.6, 0.0, vec=g.vec((120.0, 120.0, 8.0))), 0.68, 0.76)
    col = g.mix(g.math("MULTIPLY", fib, 0.5), col, srgb("8A6038"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.03), 1.6, clamp=True), col, srgb("0A0502"))
    rough = g.math("MULTIPLY_ADD", fib, 0.3, 0.5)
    return g.finish(col, rough, g.bump(g.math("ADD", gro, g.math("MULTIPLY", fib, 0.4)), 0.6, 0.002),
                    fallback=light)


def mat_sclera(name):
    """Blanc de l'œil humide : rosé vers les bords (cavité), veinules rouges."""
    g = NB(name)
    col = g.ramp(g.noise(8.0, 3.0), [(0.3, "D8CCBC"), (0.7, "F0EAE0")])
    vf = g.noise(6.0, 6.0, 0.6, 2.0)
    veins = g.fac(g.math("ABSOLUTE", g.math("SUBTRACT", vf, 0.5)), 0.02, 0.0)
    col = g.mix(g.math("MULTIPLY", veins, 0.6), col, srgb("A8302A"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.012), 1.3, clamp=True), col, srgb("7A3A30"))
    return g.finish(col, 0.08, None, sss=0.08, fallback="EEE6DA")


def mat_iris(name, outer="2A1608", inner="7A4A1C", ring="0A0604"):
    """Iris (UV polaire : u = angle, v = rayon) : stries radiales, anneau limbique sombre, pupille noire."""
    g = NB(name)
    uvn = g.n("ShaderNodeUVMap")
    uvn.uv_map = "UVMap"
    sep = g.n("ShaderNodeSeparateXYZ")
    g.l(uvn.outputs["UV"], sep.inputs[0])
    m = g.n("ShaderNodeCombineXYZ")
    g.l(g.math("MULTIPLY", sep.outputs[0], 60.0), m.inputs[0])
    g.l(g.math("MULTIPLY", sep.outputs[1], 3.0), m.inputs[1])
    streak = g.noise(1.0, 5.0, 0.6, 0.0, vec=m.outputs[0])
    r = sep.outputs[1]
    col = g.ramp(r, [(0.30, "020101"), (0.36, inner), (0.75, outer), (0.95, ring)])
    col = g.mix(g.math("MULTIPLY", g.fac(streak, 0.4, 0.7), 0.5), col, srgb("C08A40"))
    return g.finish(col, 0.06, g.bump(streak, 0.15, 0.0005), fallback=inner)


def mat_lip(name, col="5A2018"):
    g = NB(name)
    c = g.ramp(g.noise(30.0, 4.0), [(0.3, "3A120C"), (0.7, col)])
    c = g.mix(g.math("MULTIPLY", g.cavity(0.01), 1.4, clamp=True), c, srgb("120404"))
    return g.finish(c, 0.38, g.bump(g.noise(200.0, 3.0), 0.2, 0.0006), sss=0.08, fallback=col)


def mat_wet(name, col="8A3A36", rough=0.18):
    g = NB(name)
    c = g.ramp(g.noise(25.0, 3.0), [(0.3, "4A1414"), (0.7, col)])
    return g.finish(c, rough, g.bump(g.noise(90.0), 0.2, 0.0006), sss=0.1, fallback=col)


def mat_clay_paint(name, base="CFC3A8", smear="7A6A52"):
    """Peinture d'argile blanche sur le visage : traits frottés, poudreux, écaillés."""
    g = NB(name)
    col = g.ramp(g.noise(40.0, 4.0, 0.6), [(0.3, smear), (0.7, base)])
    chip = g.fac(g.noise(28.0, 6.0, 0.7, 0.3), 0.55, 0.62)
    col = g.mix(chip, col, srgb("6A4224"))
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(8.0, 4.0), 0.4, 0.7), 0.5), col, srgb("8A5A34"))
    return g.finish(col, 0.9, g.bump(g.noise(300.0), 0.2, 0.0005), fallback=base)


def mat_ochre(name, base="7A2A12"):
    g = NB(name)
    col = g.ramp(g.noise(22.0, 4.0, 0.6), [(0.3, "4A1A0C"), (0.7, base)])
    rub = g.fac(g.noise(9.0, 5.0, 0.65), 0.45, 0.65)
    col = g.mix(g.math("MULTIPLY", rub, 0.7), col, srgb("7A4A2A"))
    return g.finish(col, 0.85, g.bump(g.noise(260.0), 0.15, 0.0005), fallback=base)


def mat_stem(name):
    g = NB(name)
    st = g.noise(3.0, 4.0, 0.6, 0.0, vec=g.vec((90.0, 90.0, 8.0)))
    col = g.ramp(st, [(0.3, "3A3214"), (0.75, "7A7030")])
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(6.0, 3.0), 0.55, 0.7), 0.6), col, srgb("4A2A12"))
    return g.finish(col, 0.6, g.bump(st, 0.3, 0.001), sss=0.04, fallback="5A5020")


def mat_hessian(name, dark="5A4A24", light="B09A60"):
    """Natte / toile tressée (hessian_230 CC0) teintée paille."""
    mats = T["mats"]
    g = NB(name)
    maps = mats.find_maps("characters/hessian_230")
    v = g.vec((7.0, 7.0, 7.0))
    img = image_node(g, maps["color"], v, "sRGB")
    lu = lum(g, img.outputs["Color"])
    col = g.ramp(lu, [(0.05, dark), (0.6, light)])
    col = g.mix(g.math("MULTIPLY", g.fac(g.noise(3.0, 4.0), 0.55, 0.72), 0.5), col, srgb("3A2A10"))
    col = g.mix(g.math("MULTIPLY", g.cavity(0.02), 1.4, clamp=True), col, srgb("140C04"))
    return g.finish(col, 0.85, g.bump(lu, 0.6, 0.002), fallback=light)


# ------------------------------------------------------------------ couches de coupe (démembrement GoreBox)
def gore_layer(name, kind):
    """Matériaux des sections : un par couche ; mêmes graphes pour le rendu et le bake de l'atlas Gore."""
    g = NB(name)
    if kind == "skin":                 # derme : brun-rosé, bord net
        col = g.ramp(g.noise(30.0, 4.0), [(0.3, "6A2A20"), (0.7, "9A5040")])
        return g.finish(col, 0.45, g.bump(g.noise(200.0), 0.2, 0.0005), sss=0.1, fallback="8A4434")
    if kind == "fat":                  # graisse sous-cutanée : jaune crème, lobulée
        lob = g.edges(160.0)
        col = g.ramp(g.noise(40.0, 4.0), [(0.3, "C89A50"), (0.7, "E8C880")])
        col = g.mix(g.math("MULTIPLY", g.fac(lob, 0.06, 0.0), 0.5), col, srgb("A04A30"))
        return g.finish(col, 0.3, g.bump(g.fac(lob, 0.0, 0.08), 0.35, 0.0008), sss=0.15, fallback="D8B470")
    if kind == "muscle":               # muscle : rouge profond fibreux, humide, sang plus sombre dans les creux
        fib = g.noise(2.0, 6.0, 0.6, 0.0, vec=g.vec((260.0, 18.0, 260.0)))
        col = g.ramp(fib, [(0.25, "3A0408"), (0.55, "7A0E16"), (0.85, "B02A2A")])
        fas = g.fac(g.edges(70.0), 0.02, 0.0)
        col = g.mix(g.math("MULTIPLY", fas, 0.22), col, srgb("C88A84"))
        col = g.mix(g.math("MULTIPLY", g.cavity(0.02), 1.8, clamp=True), col, srgb("1A0002"))
        return g.finish(col, g.math("MULTIPLY_ADD", fib, 0.18, 0.16), g.bump(fib, 0.5, 0.0012), sss=0.12,
                        fallback="7A0E16")
    if kind == "bone":                 # os cortical : ivoire, bord poreux
        col = g.ramp(g.noise(40.0, 4.0), [(0.3, "C8B490"), (0.7, "EEE2C6")])
        por = g.fac(g.edges(220.0), 0.05, 0.0)
        col = g.mix(g.math("MULTIPLY", por, 0.4), col, srgb("8A6A4A"))
        return g.finish(col, 0.42, g.bump(por, 0.3, 0.0005), fallback="E0D0B0")
    if kind == "marrow":               # moelle : rouge sombre spongieux
        sp = g.edges(300.0)
        col = g.ramp(g.noise(50.0, 4.0), [(0.3, "2A0206"), (0.7, "6A1418")])
        col = g.mix(g.math("MULTIPLY", g.fac(sp, 0.08, 0.0), 0.5), col, srgb("C88A50"))
        return g.finish(col, 0.25, g.bump(g.fac(sp, 0.0, 0.1), 0.4, 0.0006), sss=0.1, fallback="4A0A0E")
    if kind == "husk_cut":             # bourre tranchée : fibres serrées brun-roux
        fib = g.noise(2.0, 6.0, 0.6, 0.0, vec=g.vec((300.0, 300.0, 300.0)))
        col = g.ramp(fib, [(0.25, "3A2410"), (0.6, "8A5A30"), (0.9, "C49A68")])
        col = g.mix(g.math("MULTIPLY", g.cavity(0.01), 1.4, clamp=True), col, srgb("140A04"))
        return g.finish(col, 0.9, g.bump(fib, 0.8, 0.0015), fallback="7A5030")
    if kind == "nut_shell":            # coque dure (endocarpe) : brun très sombre, dense
        col = g.ramp(g.noise(60.0, 4.0), [(0.3, "140A04"), (0.7, "3A2210")])
        return g.finish(col, 0.45, g.bump(g.noise(300.0), 0.2, 0.0004), fallback="24140A")
    if kind == "meat":                 # chair de coco (albumen) : blanc crème humide
        col = g.ramp(g.noise(20.0, 4.0), [(0.3, "DCD2BA"), (0.7, "F4EEE0")])
        col = g.mix(g.math("MULTIPLY", g.cavity(0.01), 1.2, clamp=True), col, srgb("A89A80"))
        return g.finish(col, 0.28, g.bump(g.noise(150.0, 3.0), 0.25, 0.0006), sss=0.2, fallback="F2ECDE")
    if kind == "pulp":                 # pulpe gélatineuse translucide
        col = g.ramp(g.noise(12.0, 3.0), [(0.3, "C8C4B0"), (0.7, "E8E6D8")])
        return g.finish(col, 0.12, g.bump(g.noise(40.0, 3.0), 0.3, 0.001), sss=0.3, fallback="DCD8C8")
    if kind == "milk":                 # lait de coco : blanc laiteux, surface liquide brillante
        col = g.ramp(g.noise(6.0, 2.0), [(0.3, "C8C6BC"), (0.7, "E2E0D6")])
        return g.finish(col, 0.04, g.bump(g.noise(10.0, 2.0), 0.1, 0.002), sss=0.25, fallback="F2F0E8")
    if kind == "cursed_flesh":         # chair maudite : rouge-noir veiné de violet
        vf = g.noise(8.0, 6.0, 0.6, 1.5)
        col = g.ramp(vf, [(0.3, "14020A"), (0.6, "4A0614"), (0.85, "6A1030")])
        veins = g.fac(g.math("ABSOLUTE", g.math("SUBTRACT", vf, 0.5)), 0.03, 0.0)
        col = g.mix(g.math("MULTIPLY", veins, 0.8), col, srgb("5A1A8A"))
        return g.finish(col, 0.22, g.bump(vf, 0.5, 0.001), sss=0.1, emis=g.mix(veins, (0, 0, 0, 1), srgb("6A20C0")),
                        emis_strength=1.5, fallback="3A0610")
    if kind == "cursed_core":          # coeur corrompu : lueur violet-vert
        n1 = g.noise(16.0, 6.0, 0.62, 1.6)
        col = g.ramp(n1, [(0.45, "120020"), (0.58, "3A0A70"), (0.68, "106A2A"), (0.82, "7AF070")])
        return g.finish(g.mix(1.0, col, (0.1, 0.1, 0.1, 1), "MULTIPLY"), 0.3, None, emis=col, emis_strength=4.0,
                        fallback="5A14A0")
    if kind == "wood_cut":             # bois du masque tranché (bout de fil)
        rings = g.n("ShaderNodeTexWave", wave_type="RINGS")
        g.l(g.vec((40.0, 40.0, 40.0)), rings.inputs["Vector"])
        rings.inputs["Distortion"].default_value = 3.0
        col = g.ramp(rings.outputs["Fac"], [(0.2, "6A4424"), (0.8, "B48A58")])
        return g.finish(col, 0.75, g.bump(rings.outputs["Fac"], 0.2, 0.001), fallback="9A7040")
    raise ValueError(kind)


GORE_LAYERS = ("skin", "fat", "muscle", "bone", "marrow", "husk_cut", "nut_shell", "meat", "pulp", "milk",
               "cursed_flesh", "cursed_core", "wood_cut")
