"""Procedural PBR materials of the hand-cranked machine gun Mle 1918 (T-272), authored to be BAKED by
scripts/props/longarm_common/bake.py (same node helpers as scripts/props/longarm_common/materials.py: every recipe
registers its colour / roughness / metallic sockets in materials.SOCKETS so the baker can route them one channel at
a time). Wear comes from the geometry (Bevel node edge mask, pointiness, ambient occlusion in the crevices) and from
object space noise (the joined weapon mesh is in weapon space: +X muzzle, metres), so the same recipe gives every
part its own scratches. Colours are linear RGB.

Names start with MAT-MG- (bake sources, never exported); the baked materials are M_MG_<Group> (bake.py).
Extra channel "heat" (barrel group only): the glow mask of a hot barrel, baked to T_MG_Jacket_Heat.png and wired in
Unreal as the Emissive texture of MI_MachineGun_Jacket with EmissiveStrength 0 (the game raises it with the heat).
"""
from __future__ import annotations

import bpy

import materials as LA  # scripts/props/longarm_common/materials.py (Graph, SOCKETS)
from materials import Graph, SOCKETS, _finish, _new  # noqa: F401

DENSITY = {'MAT-MG-Blued': 7850, 'MAT-MG-DarkSteel': 7850, 'MAT-MG-Brass': 8500, 'MAT-MG-BrassBright': 8500,
           'MAT-MG-Copper': 8900, 'MAT-MG-Walnut': 650, 'MAT-MG-Canvas': 600, 'MAT-MG-BoxWood': 600, 'MAT-MG-Iron': 7850,
           'MAT-MG-Leather': 900, 'MAT-MG-Rubber': 1200, 'MAT-MG-Bore': 7850, 'MAT-MG-Plate': 8500, 'MAT-MG-BoxDark': 600,
           'MAT-MG-CaseBrass': 8500}
NAMES = list(DENSITY)


def edge_mask(g, radius=0.0016, strength=1.0):
    """Convex edges from the Bevel node only (Graph.edge also takes Cycles' Pointiness, which on these low-poly
    bevelled boxes is interpolated over whole faces: every face came out 'worn')."""
    bev = g.node('ShaderNodeBevel', samples=6)
    bev.inputs['Radius'].default_value = radius
    geo = g.node('ShaderNodeNewGeometry')
    dot = g.node('ShaderNodeVectorMath', operation='DOT_PRODUCT')
    g.link(bev.outputs['Normal'], dot.inputs[0])
    g.link(geo.outputs['Normal'], dot.inputs[1])
    e = g.math('SUBTRACT', 1.0, dot.outputs['Value'])
    return g.math('MULTIPLY', e, 12.0 * strength, clamp=True)


def wood(name, dark, light, worn, ring_mm=3.0, axis=(0.0, 0.0, 0.0), oil=0.0):
    """Oiled wood (after longarm_common/materials.wood): growth rings about an axis parallel to X, pores along the
    grain, edges worn lighter (Bevel mask only), dirt in the crevices; oil lowers the roughness."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    off = g.mapping(co, scale=(0.0, 1.0, 1.0), loc=(0.0, -axis[1], -axis[2]))
    length = g.node('ShaderNodeVectorMath', operation='LENGTH')
    g.link(off, length.inputs[0])
    wobble = g.noise(g.mapping(co, scale=(1.5, 6.0, 6.0)), 4.0, detail=3.0)
    r = g.math('ADD', length.outputs['Value'], g.math('MULTIPLY', wobble, 0.012))
    rings = g.math('SINE', g.math('MULTIPLY', r, 6.2832 / (ring_mm * 0.001)))
    rings = g.math('POWER', g.math('ABSOLUTE', rings), 0.55)
    pores = g.noise(g.mapping(co, scale=(3.0, 70.0, 70.0)), 6.0, detail=4.0, rough=0.6)
    grain = g.math('ADD', g.math('MULTIPLY', rings, 0.75), g.math('MULTIPLY', pores, 0.25), clamp=True)
    patches = g.noise(co, 2.5, detail=2.0)
    color = g.mix(grain, dark, light)
    color = g.mix(g.math('MULTIPLY', patches, 0.35), color, g.mix(0.5, color, dark))
    edge = edge_mask(g, 0.0025, 0.8)
    color = g.mix(g.math('MULTIPLY', edge, 0.6), color, worn)
    cav = g.cavity(0.02)
    color = g.mix(g.math('MULTIPLY', cav, 0.7), color, (dark[0] * 0.5, dark[1] * 0.5, dark[2] * 0.5))
    rough = g.math('ADD', 0.46 - oil, g.math('MULTIPLY', grain, 0.20))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.12), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', rings, 0.6), g.math('MULTIPLY', pores, 0.4)), 0.30, 0.0010)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.25, 0.13, 0.05))


def leather(name, base=(0.10, 0.050, 0.022), worn=(0.20, 0.11, 0.05)):
    mat, g, bsdf = _new(name)
    co = g.coords()
    pores = g.voronoi(co, 420.0)
    cracks = g.noise(co, 60.0, detail=4.0, rough=0.7)
    color = g.mix(g.math('MULTIPLY', cracks, 0.35), base, (base[0] * 0.6, base[1] * 0.6, base[2] * 0.6))
    edge = edge_mask(g, 0.003, 0.8)
    color = g.mix(g.math('MULTIPLY', edge, 0.6), color, worn)
    rough = g.math('ADD', 0.58, g.math('MULTIPLY', cracks, 0.18))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.15), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', pores, 0.6), g.math('MULTIPLY', cracks, 0.4)), 0.5, 0.0006)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.2, 0.1, 0.05))


def _heat_none(name):
    """Every source material of the jacket group answers the 'heat' channel (0 when it never glows)."""
    SOCKETS[name]['heat'] = 0.0


def blued(name, base=(0.020, 0.024, 0.032), bright=(0.46, 0.47, 0.50), rough_base=0.34):
    """Blued steel of the receiver: mottled bluing (a little warmer where it is thin), machining marks along X,
    handling wear to bare steel on every edge, fine scratches, oil sheen in patches, rust specks in the seams."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    mottle = g.noise(co, 7.0, detail=3.0, rough=0.55)
    color = g.mix(g.math('MULTIPLY', mottle, 0.8), base, (base[0] * 1.9 + 0.012, base[1] * 1.45 + 0.004, base[2] * 1.10))
    streaks = g.noise(g.mapping(co, scale=(3.0, 140.0, 140.0)), 6.0, detail=2.0, rough=0.5)
    color = g.mix(g.math('MULTIPLY', streaks, 0.30), color, (base[0] * 0.55, base[1] * 0.55, base[2] * 0.65))
    edge = edge_mask(g, 0.0016)
    scr_a = g.math('GREATER_THAN', g.noise(g.mapping(co, scale=(1.0, 38.0, 38.0)), 70.0, detail=1.0), 0.80)
    scr_b = g.math('GREATER_THAN', g.noise(g.mapping(co, scale=(36.0, 1.0, 36.0)), 70.0, detail=1.0), 0.82)
    wear = g.math('MAXIMUM', g.math('MULTIPLY', edge, 0.92), g.math('MULTIPLY', scr_a, 0.45), clamp=True)
    wear = g.math('MAXIMUM', wear, g.math('MULTIPLY', scr_b, 0.35), clamp=True)
    color = g.mix(wear, color, bright)
    cav = g.cavity(0.012)
    rust = g.math('MULTIPLY', g.math('GREATER_THAN', g.noise(co, 150.0, detail=3.0), 0.60), cav)
    color = g.mix(g.math('MULTIPLY', rust, 0.85), color, (0.15, 0.055, 0.018))
    color = g.mix(g.math('MULTIPLY', cav, 0.45), color, (0.008, 0.008, 0.009))
    oil = g.math('POWER', g.noise(co, 3.5, detail=2.0), 3.0)
    rough = g.math('ADD', rough_base, g.math('MULTIPLY', streaks, 0.14))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', oil, 0.55))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', wear, 0.12))
    rough = g.math('ADD', rough, g.math('MULTIPLY', rust, 0.40), clamp=True)
    metal = g.math('SUBTRACT', 1.0, g.math('MULTIPLY', rust, 0.8), clamp=True)
    pits = g.math('GREATER_THAN', g.noise(co, 380.0, detail=2.0), 0.74)
    height = g.math('SUBTRACT', g.math('MULTIPLY', streaks, 0.35), g.math('MULTIPLY', pits, 0.5))
    normal = g.bump(height, 0.22, 0.0005)
    return _finish(mat, g, bsdf, color, rough, metal, normal, (0.05, 0.06, 0.08))


def dark_steel(name, heat=False):
    """Barrel, gland nut and conical flash hider: matt black steel, temper colours near the breech end of the
    exposed barrel, soot growing toward the muzzle (object X > 0.80 m), powder streaks inside the cone."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    base = (0.040, 0.040, 0.044)
    grain = g.noise(g.mapping(co, scale=(2.0, 90.0, 90.0)), 8.0, detail=3.0, rough=0.6)
    color = g.mix(g.math('MULTIPLY', grain, 0.4), base, (0.026, 0.026, 0.029))
    sep = g.node('ShaderNodeSeparateXYZ')
    g.link(co, sep.inputs[0])
    x = sep.outputs['X']
    # temper colours (straw, purple, blue) on the exposed barrel between the gland nut and the cone
    band = g.math('MULTIPLY', g.math('GREATER_THAN', x, 0.735), g.math('LESS_THAN', x, 0.790))
    temper = g.ramp(g.math('MULTIPLY', g.math('SUBTRACT', x, 0.735), 18.0),
                    [(0.0, (0.20, 0.13, 0.05)), (0.35, (0.16, 0.06, 0.12)), (0.7, (0.05, 0.07, 0.17)), (1.0, (0.06, 0.06, 0.07))])
    color = g.mix(g.math('MULTIPLY', band, 0.55), color, temper)
    soot = g.math('MULTIPLY', g.math('SUBTRACT', x, 0.79), 9.0, clamp=True)
    soot = g.math('MULTIPLY', soot, g.math('ADD', 0.55, g.math('MULTIPLY', g.noise(co, 60.0, detail=3.0), 0.45)), clamp=True)
    color = g.mix(soot, color, (0.010, 0.009, 0.008))
    edge = edge_mask(g, 0.0014)
    color = g.mix(g.math('MULTIPLY', edge, 0.5), color, (0.30, 0.30, 0.31))
    rough = g.math('ADD', 0.48, g.math('MULTIPLY', grain, 0.12))
    rough = g.math('ADD', rough, g.math('MULTIPLY', soot, 0.32), clamp=True)
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.15), clamp=True)
    metal = g.math('SUBTRACT', 1.0, g.math('MULTIPLY', soot, 0.7), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', grain, 0.3), g.math('MULTIPLY', g.noise(co, 400.0, detail=1.0), 0.2)), 0.18, 0.0005)
    _finish(mat, g, bsdf, color, rough, metal, normal, (0.04, 0.04, 0.045))
    if heat:
        # the glow mask: hottest at the barrel and the root of the cone, fading toward the cone's mouth
        hot = g.ramp(g.math('MULTIPLY', g.math('SUBTRACT', x, 0.70), 5.0),
                     [(0.0, (0.0, 0.0, 0.0)), (0.12, (0.55, 0.12, 0.02)), (0.40, (1.0, 0.32, 0.06)), (0.62, (1.0, 0.45, 0.10)),
                      (0.85, (0.55, 0.13, 0.02)), (1.0, (0.12, 0.02, 0.0))])
        hot = g.mix(g.math('MULTIPLY', g.noise(co, 25.0, detail=2.0), 0.35), hot, (0.35, 0.06, 0.01))
        SOCKETS[name]['heat'] = hot
    return mat


def browned_brass(name):
    """The water jacket: browned brass ("laiton bruni"), dark patina deep in the flutes, the crests rubbed back to
    warm brass, water stains below the caps, oily fingerprints, a few dents."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    base = (0.088, 0.052, 0.020)
    blotch = g.noise(co, 14.0, detail=3.0, rough=0.6)
    color = g.mix(g.math('MULTIPLY', g.math('POWER', blotch, 1.4), 0.75), base, (0.048, 0.030, 0.013))
    cav = g.cavity(0.010)
    color = g.mix(g.math('MULTIPLY', cav, 0.9), color, (0.016, 0.012, 0.007))
    edge = edge_mask(g, 0.0018, 1.0)
    color = g.mix(g.math('MULTIPLY', edge, 0.65), color, (0.30, 0.20, 0.09))
    scratches = g.math('GREATER_THAN', g.noise(g.mapping(co, scale=(1.0, 28.0, 28.0)), 85.0, detail=1.0), 0.81)
    color = g.mix(g.math('MULTIPLY', scratches, 0.45), color, (0.26, 0.18, 0.08))
    stains = g.math('POWER', g.noise(g.mapping(co, scale=(4.0, 4.0, 1.2)), 9.0, detail=2.0), 2.5)
    color = g.mix(g.math('MULTIPLY', stains, 0.55), color, (0.055, 0.062, 0.040))
    rough = g.math('ADD', 0.42, g.math('MULTIPLY', blotch, 0.20))
    rough = g.math('ADD', rough, g.math('MULTIPLY', cav, 0.25))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.18), clamp=True)
    rough = g.math('ADD', rough, g.math('MULTIPLY', stains, 0.18), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', g.noise(co, 420.0, detail=1.0), 0.5), g.math('MULTIPLY', scratches, 0.5)), 0.16, 0.0004)
    mat_ = _finish(mat, g, bsdf, color, rough, 1.0, normal, (0.45, 0.30, 0.12))
    return mat_


def bright_brass(name, base=(0.70, 0.46, 0.17), rough_base=0.24, soot_x=None):
    """Cartridge cases and the brass spacers of the belt: bright drawn brass with tarnish spots and handling marks.
    soot_x: a fired case, powder soot from that object X to the mouth (+X)."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    tarnish = g.math('POWER', g.noise(co, 55.0, detail=3.0, rough=0.6), 2.0)
    color = g.mix(g.math('MULTIPLY', tarnish, 0.7), base, (0.38, 0.24, 0.08))
    cav = g.cavity(0.006)
    color = g.mix(g.math('MULTIPLY', cav, 0.6), color, (0.16, 0.10, 0.035))
    rough = g.math('ADD', rough_base, g.math('MULTIPLY', tarnish, 0.25), clamp=True)
    metal = 1.0
    if soot_x is not None:
        sep = g.node('ShaderNodeSeparateXYZ')
        g.link(co, sep.inputs[0])
        soot = g.math('MULTIPLY', g.math('SUBTRACT', sep.outputs['X'], soot_x), 110.0, clamp=True)
        soot = g.math('MULTIPLY', soot, g.math('ADD', 0.55, g.math('MULTIPLY', g.noise(co, 300.0, detail=3.0), 0.45)), clamp=True)
        color = g.mix(soot, color, (0.030, 0.024, 0.018))
        rough = g.math('ADD', rough, g.math('MULTIPLY', soot, 0.45), clamp=True)
        metal = g.math('SUBTRACT', 1.0, g.math('MULTIPLY', soot, 0.6), clamp=True)
    normal = g.bump(g.noise(co, 600.0, detail=1.0), 0.08, 0.0003)
    return _finish(mat, g, bsdf, color, rough, metal, normal, (0.8, 0.6, 0.25))


def copper(name):
    """Gilding metal bullet jackets."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    tarnish = g.noise(co, 80.0, detail=3.0)
    color = g.mix(g.math('MULTIPLY', tarnish, 0.5), (0.62, 0.30, 0.17), (0.34, 0.15, 0.08))
    rough = g.math('ADD', 0.26, g.math('MULTIPLY', tarnish, 0.2), clamp=True)
    return _finish(mat, g, bsdf, color, rough, 1.0, None, (0.7, 0.4, 0.25))


def canvas(name, base=(0.098, 0.082, 0.046), dirt=(0.034, 0.028, 0.017)):
    """Webbing of the cartridge belt: tight twill weave, frayed fibres, gun oil and powder stains."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    warp = g.wave(g.mapping(co, scale=(1.0, 1.0, 1.0)), 1400.0, 'X', distortion=0.8, detail=1.0)
    weft = g.wave(g.mapping(co, scale=(1.0, 1.0, 1.0)), 1400.0, 'Z', distortion=0.8, detail=1.0)
    weave = g.math('MULTIPLY', g.math('ADD', warp, weft), 0.5)
    fibres = g.noise(co, 300.0, detail=4.0, rough=0.7)
    color = g.mix(g.math('MULTIPLY', weave, 0.30), base, (base[0] * 0.78, base[1] * 0.78, base[2] * 0.78))
    color = g.mix(g.math('MULTIPLY', fibres, 0.18), color, (base[0] * 1.25, base[1] * 1.2, base[2] * 1.1))
    stains = g.math('POWER', g.noise(co, 30.0, detail=3.0), 2.2)
    color = g.mix(g.math('MULTIPLY', stains, 0.75), color, dirt)
    cav = g.cavity(0.006)
    color = g.mix(g.math('MULTIPLY', cav, 0.6), color, dirt)
    rough = g.math('ADD', 0.80, g.math('MULTIPLY', weave, 0.12))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', stains, 0.25), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', weave, 0.7), g.math('MULTIPLY', fibres, 0.3)), 0.55, 0.0003)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.45, 0.38, 0.25))


def box_wood(name, base=(0.235, 0.150, 0.075), grey=(0.22, 0.20, 0.17)):
    """Ammunition box planks: oak boards with the grain along the box, weathered grey on the edges and the corners,
    dark in the seams, a few dents."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    grain = g.wave(g.mapping(co, scale=(0.3, 9.0, 9.0)), 6.0, 'Z', distortion=6.0, detail=3.0)
    pores = g.noise(g.mapping(co, scale=(1.0, 30.0, 30.0)), 30.0, detail=3.0)
    color = g.mix(g.math('MULTIPLY', grain, 0.45), base, (base[0] * 0.62, base[1] * 0.58, base[2] * 0.52))
    color = g.mix(g.math('MULTIPLY', pores, 0.2), color, (base[0] * 1.2, base[1] * 1.15, base[2] * 1.05))
    edge = edge_mask(g, 0.003)
    weather = g.math('MAXIMUM', g.math('MULTIPLY', edge, 0.7), g.math('MULTIPLY', g.math('POWER', g.noise(co, 6.0, detail=2.0), 3.0), 0.8), clamp=True)
    color = g.mix(weather, color, grey)
    cav = g.cavity(0.012)
    color = g.mix(g.math('MULTIPLY', cav, 0.85), color, (0.035, 0.025, 0.015))
    rough = g.math('ADD', 0.70, g.math('MULTIPLY', grain, 0.12), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', grain, 0.6), g.math('MULTIPLY', pores, 0.4)), 0.45, 0.0012)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.35, 0.25, 0.15))


def iron(name, base=(0.040, 0.038, 0.036), rust=(0.20, 0.075, 0.025)):
    """Iron hoops, corners, handle and nails of the box: black iron, rust bloom in blotches and in the seams."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    blotch = g.math('POWER', g.noise(co, 20.0, detail=4.0, rough=0.65), 1.8)
    cav = g.cavity(0.008)
    rusty = g.math('ADD', g.math('MULTIPLY', blotch, 0.55), g.math('MULTIPLY', cav, 0.45), clamp=True)
    color = g.mix(rusty, base, rust)
    edge = edge_mask(g, 0.0012)
    color = g.mix(g.math('MULTIPLY', edge, 0.4), color, (0.25, 0.25, 0.26))
    rough = g.math('ADD', 0.55, g.math('MULTIPLY', rusty, 0.35), clamp=True)
    metal = g.math('SUBTRACT', 0.9, g.math('MULTIPLY', rusty, 0.85), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', blotch, 0.6), g.math('MULTIPLY', g.noise(co, 300.0, detail=2.0), 0.4)), 0.35, 0.0006)
    return _finish(mat, g, bsdf, color, rough, metal, normal, (0.08, 0.06, 0.05))


def rubber(name):
    """Condensing hose: black rubber under a spiral canvas wrap, dusty."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    wrap = g.wave(g.mapping(co, scale=(1.0, 1.0, 1.0)), 120.0, 'X', distortion=1.5, detail=1.0)
    dust = g.noise(co, 40.0, detail=3.0)
    color = g.mix(g.math('MULTIPLY', wrap, 0.25), (0.022, 0.021, 0.020), (0.045, 0.040, 0.034))
    color = g.mix(g.math('MULTIPLY', dust, 0.3), color, (0.10, 0.09, 0.075))
    rough = g.math('ADD', 0.62, g.math('MULTIPLY', dust, 0.2), clamp=True)
    normal = g.bump(wrap, 0.5, 0.0008)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.03, 0.03, 0.03))


def ensure_all():
    """All the source materials of the pack (idempotent), densities registered for kit.part_physics. (Never import
    longarm_common/musket_ref.py here: it imports the musket pack's build.py and animation.py by their bare names,
    which in this pack are ours.)"""
    import kit  # scripts/props/musket_pack/kit.py
    kit.DENSITY.update(DENSITY)
    if 'MAT-MG-Blued' in SOCKETS:
        return
    blued('MAT-MG-Blued')
    dark_steel('MAT-MG-DarkSteel', heat=True)
    browned_brass('MAT-MG-Brass')
    bright_brass('MAT-MG-BrassBright')
    bright_brass('MAT-MG-Plate', base=(0.55, 0.40, 0.17), rough_base=0.30)
    bright_brass('MAT-MG-CaseBrass', base=(0.66, 0.43, 0.16), rough_base=0.28, soot_x=0.017)
    copper('MAT-MG-Copper')
    wood('MAT-MG-Walnut', dark=(0.040, 0.018, 0.007), light=(0.13, 0.062, 0.024), worn=(0.22, 0.13, 0.060), ring_mm=2.4, oil=0.10)
    canvas('MAT-MG-Canvas')
    wood('MAT-MG-BoxWood', dark=(0.040, 0.027, 0.015), light=(0.105, 0.070, 0.040), worn=(0.15, 0.13, 0.10), ring_mm=3.4,
         axis=(0.0, 0.059, -0.141))
    LA.flat('MAT-MG-BoxDark', (0.030, 0.021, 0.013), 0.85, 0.0)
    iron('MAT-MG-Iron')
    leather('MAT-MG-Leather')
    rubber('MAT-MG-Rubber')
    LA.flat('MAT-MG-Bore', (0.018, 0.018, 0.019), 0.55, 1.0)
    # the jacket group bakes a heat channel: every material that may land on it answers it
    for name in ('MAT-MG-Brass', 'MAT-MG-Blued', 'MAT-MG-Plate', 'MAT-MG-Rubber', 'MAT-MG-Bore', 'MAT-MG-Walnut',
                 'MAT-MG-Leather', 'MAT-MG-BrassBright', 'MAT-MG-Iron'):
        _heat_none(name)
    for name in NAMES:
        bpy.data.materials[name]['mg_source'] = True
