"""Procedural PBR materials of the T-334 packs (bolt-action rifle, sub-machine gun), authored to be BAKED by
scripts/props/longarm_common/bake.py through pa_bake.py: every recipe registers its colour / roughness / metallic
sockets in longarm_common/materials.SOCKETS (the baker routes them to an emission shader one channel at a time).
Wear comes from the geometry (Bevel node edge mask, ambient occlusion in the crevices) and from object space noise
(the joined weapon mesh is in weapon space, metres), as the machine gun pack does (recipes adapted from
scripts/props/machinegun_pack/mg_materials.py).

The walnut takes its fine grain and relief from a CC0 scan already in the repository (Poly Haven "fine_grained_wood",
art/textures/characters/fine_grained_wood, credited in art/SOURCES.md), RECOLOURED to the walnut palette of the bean's
weapons: the scan's luminance drives a dark -> light walnut ramp mixed with procedural growth rings, its normal map
gives the pores (box projection on object coordinates, grain along the weapon's X axis).
Names start with MAT-PA- (bake sources, never exported). Colours are linear RGB.
"""
from __future__ import annotations

from pathlib import Path

import bpy

import materials as LA  # scripts/props/longarm_common/materials.py (Graph, SOCKETS, _new, _finish)
from materials import SOCKETS, _finish, _new  # noqa: F401

REPO = Path(__file__).resolve().parents[3]
CC0_WOOD = REPO / 'art/textures/characters/fine_grained_wood'
DENSITY = {'MAT-PA-Walnut': 650, 'MAT-PA-WalnutDark': 650, 'MAT-PA-Blued': 7850, 'MAT-PA-BrightSteel': 7850,
           'MAT-PA-DarkSteel': 7850, 'MAT-PA-Brass': 8500, 'MAT-PA-BrassBright': 8500, 'MAT-PA-CaseBrass': 8500,
           'MAT-PA-Copper': 8900, 'MAT-PA-Leather': 900, 'MAT-PA-Cardboard': 600, 'MAT-PA-Bore': 7850, 'MAT-PA-Lens': 2500,
           'MAT-PA-CaseFired': 8500, 'MAT-PA-Lacquer': 1200}


def edge_mask(g, radius=0.0016, strength=1.0):
    """Convex edges from the Bevel node only (Cycles' Pointiness is interpolated over whole faces on low-poly parts)."""
    bev = g.node('ShaderNodeBevel', samples=6)
    bev.inputs['Radius'].default_value = radius
    geo = g.node('ShaderNodeNewGeometry')
    dot = g.node('ShaderNodeVectorMath', operation='DOT_PRODUCT')
    g.link(bev.outputs['Normal'], dot.inputs[0])
    g.link(geo.outputs['Normal'], dot.inputs[1])
    e = g.math('SUBTRACT', 1.0, dot.outputs['Value'])
    return g.math('MULTIPLY', e, 12.0 * strength, clamp=True)


def _image(path, colorspace):
    img = bpy.data.images.get(path.name)
    if img is None:
        img = bpy.data.images.load(str(path), check_existing=True)
    img.colorspace_settings.name = colorspace
    return img


def walnut_cc0(name, dark=(0.038, 0.016, 0.0065), light=(0.150, 0.068, 0.027), worn=(0.24, 0.13, 0.058), ring_mm=3.2, oil=0.05):
    """Oiled walnut: CC0 scan (fine_grained_wood) recoloured by its luminance to the walnut ramp, procedural growth
    rings about an axis parallel to X for the figure, edges rubbed lighter, dirt in the crevices, the scan's normal map
    for the pores and the procedural rings as a bump."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    # box projection with the grain (the scan's v axis) along X on the sides AND on the top: (y + z, x, x)
    sep = g.node('ShaderNodeSeparateXYZ')
    g.link(co, sep.inputs[0])
    comb = g.node('ShaderNodeCombineXYZ')
    g.link(g.math('ADD', sep.outputs['Y'], sep.outputs['Z']), comb.inputs['X'])
    g.link(sep.outputs['X'], comb.inputs['Y'])
    g.link(sep.outputs['X'], comb.inputs['Z'])
    uvw = g.mapping(comb.outputs['Vector'], scale=(3.2, 1.6, 1.6))
    diff = g.node('ShaderNodeTexImage', projection='BOX', interpolation='Linear')
    diff.projection_blend = 0.35
    diff.image = _image(CC0_WOOD / 'fine_grained_wood_diff_1k.jpg', 'sRGB')
    g.link(uvw, diff.inputs['Vector'])
    nrm = g.node('ShaderNodeTexImage', projection='BOX', interpolation='Linear')
    nrm.projection_blend = 0.35
    nrm.image = _image(CC0_WOOD / 'fine_grained_wood_nor_gl_1k.jpg', 'Non-Color')
    g.link(uvw, nrm.inputs['Vector'])
    lum_node = g.node('ShaderNodeRGBToBW')
    g.link(diff.outputs['Color'], lum_node.inputs['Color'])
    scan = g.node('ShaderNodeMapRange')
    g.link(lum_node.outputs['Val'], scan.inputs['Value'])
    scan.inputs['From Min'].default_value = 0.025
    scan.inputs['From Max'].default_value = 0.13
    scan.clamp = True
    scan = scan.outputs['Result']
    # procedural figure: growth rings about an axis parallel to X (the stock cut along the barrel)
    off = g.mapping(co, scale=(0.0, 1.0, 1.0), loc=(0.0, -0.03, 0.05))
    length = g.node('ShaderNodeVectorMath', operation='LENGTH')
    g.link(off, length.inputs[0])
    wobble = g.noise(g.mapping(co, scale=(1.5, 6.0, 6.0)), 4.0, detail=3.0)
    r = g.math('ADD', length.outputs['Value'], g.math('MULTIPLY', wobble, 0.014))
    rings = g.math('SINE', g.math('MULTIPLY', r, 6.2832 / (ring_mm * 0.001)))
    rings = g.math('POWER', g.math('ABSOLUTE', rings), 0.55)
    grain = g.math('ADD', g.math('MULTIPLY', rings, 0.45), g.math('MULTIPLY', scan, 0.55), clamp=True)
    color = g.ramp(grain, [(0.0, dark), (0.45, ((dark[0] + light[0]) * 0.5, (dark[1] + light[1]) * 0.5, (dark[2] + light[2]) * 0.5)),
                           (1.0, light)])
    patches = g.noise(co, 2.5, detail=2.0)
    color = g.mix(g.math('MULTIPLY', patches, 0.30), color, dark)
    edge = edge_mask(g, 0.0025, 0.8)
    color = g.mix(g.math('MULTIPLY', edge, 0.55), color, worn)
    cav = g.cavity(0.02)
    color = g.mix(g.math('MULTIPLY', cav, 0.7), color, (dark[0] * 0.45, dark[1] * 0.45, dark[2] * 0.45))
    rough = g.math('ADD', 0.44 - oil, g.math('MULTIPLY', g.math('SUBTRACT', 1.0, scan), 0.18))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.12), clamp=True)
    nmap = g.node('ShaderNodeNormalMap')
    nmap.space = 'TANGENT'
    nmap.inputs['Strength'].default_value = 0.6
    g.link(nrm.outputs['Color'], nmap.inputs['Color'])
    bump = g.node('ShaderNodeBump')
    g.link(g.math('MULTIPLY', rings, 0.6), bump.inputs['Height'])
    bump.inputs['Strength'].default_value = 0.18
    bump.inputs['Distance'].default_value = 0.0010
    g.link(nmap.outputs['Normal'], bump.inputs['Normal'])
    mat['pa_cc0'] = 'Poly Haven fine_grained_wood (CC0), recoloured to the walnut ramp'
    return _finish(mat, g, bsdf, color, rough, 0.0, bump.outputs['Normal'], (0.20, 0.10, 0.04))


def blued(name, base=(0.020, 0.024, 0.032), bright=(0.46, 0.47, 0.50), rough_base=0.34, rust_amount=1.0):
    """Blued steel: mottled bluing, machining marks along X, handling wear to bare steel on the edges, scratches, oil
    sheen in patches, rust specks in the seams."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    mottle = g.noise(co, 7.0, detail=3.0, rough=0.55)
    color = g.mix(g.math('MULTIPLY', mottle, 0.8), base, (base[0] * 1.9 + 0.012, base[1] * 1.45 + 0.004, base[2] * 1.10))
    streaks = g.noise(g.mapping(co, scale=(3.0, 140.0, 140.0)), 6.0, detail=2.0, rough=0.5)
    color = g.mix(g.math('MULTIPLY', streaks, 0.30), color, (base[0] * 0.55, base[1] * 0.55, base[2] * 0.65))
    edge = edge_mask(g, 0.0014)
    scr_a = g.math('GREATER_THAN', g.noise(g.mapping(co, scale=(1.0, 38.0, 38.0)), 70.0, detail=1.0), 0.80)
    scr_b = g.math('GREATER_THAN', g.noise(g.mapping(co, scale=(36.0, 1.0, 36.0)), 70.0, detail=1.0), 0.82)
    wear = g.math('MAXIMUM', g.math('MULTIPLY', edge, 0.92), g.math('MULTIPLY', scr_a, 0.45), clamp=True)
    wear = g.math('MAXIMUM', wear, g.math('MULTIPLY', scr_b, 0.35), clamp=True)
    color = g.mix(wear, color, bright)
    cav = g.cavity(0.012)
    rust = g.math('MULTIPLY', g.math('GREATER_THAN', g.noise(co, 150.0, detail=3.0), 0.60), g.math('MULTIPLY', cav, rust_amount))
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


def bright_steel(name, base=(0.42, 0.43, 0.45), rough_base=0.22):
    """Steel left "in the white" (the bolt): polished, turning marks around X, a little tarnish in the corners."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    turn = g.noise(g.mapping(co, scale=(260.0, 4.0, 4.0)), 3.0, detail=2.0)
    color = g.mix(g.math('MULTIPLY', turn, 0.35), base, (base[0] * 0.75, base[1] * 0.75, base[2] * 0.78))
    cav = g.cavity(0.008)
    color = g.mix(g.math('MULTIPLY', cav, 0.7), color, (0.05, 0.045, 0.04))
    tarnish = g.math('POWER', g.noise(co, 30.0, detail=3.0), 2.5)
    color = g.mix(g.math('MULTIPLY', tarnish, 0.35), color, (0.20, 0.16, 0.11))
    rough = g.math('ADD', rough_base, g.math('MULTIPLY', turn, 0.12))
    rough = g.math('ADD', rough, g.math('MULTIPLY', cav, 0.3), clamp=True)
    normal = g.bump(turn, 0.12, 0.0003)
    return _finish(mat, g, bsdf, color, rough, 1.0, normal, (0.4, 0.4, 0.42))


def brass(name, base=(0.62, 0.40, 0.14), patina=(0.18, 0.12, 0.05), polished=(0.86, 0.62, 0.28), rough_base=0.28):
    """Brass of the scope tube and the fittings: patina in blotches and crevices, polished edges, fine scratches."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    blotch = g.noise(co, 30.0, detail=3.0, rough=0.6)
    color = g.mix(g.math('MULTIPLY', g.math('POWER', blotch, 1.6), 0.55), base, patina)
    cav = g.cavity(0.010)
    color = g.mix(g.math('MULTIPLY', cav, 0.8), color, patina)
    edge = edge_mask(g, 0.0014)
    color = g.mix(g.math('MULTIPLY', edge, 0.75), color, polished)
    scratches = g.math('GREATER_THAN', g.noise(g.mapping(co, scale=(1.0, 30.0, 30.0)), 90.0, detail=1.0), 0.82)
    color = g.mix(g.math('MULTIPLY', scratches, 0.35), color, polished)
    rough = g.math('ADD', rough_base, g.math('MULTIPLY', blotch, 0.22))
    rough = g.math('ADD', rough, g.math('MULTIPLY', cav, 0.25))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.14), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', g.noise(co, 500.0, detail=1.0), 0.5), g.math('MULTIPLY', scratches, 0.5)), 0.15, 0.0004)
    return _finish(mat, g, bsdf, color, rough, 1.0, normal, (0.7, 0.5, 0.2))


def case_brass(name, base=(0.70, 0.46, 0.17), rough_base=0.24, soot_x=None):
    """Cartridge cases: bright drawn brass with tarnish spots; soot_x: a fired case, powder soot from that X to the mouth."""
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


def copper(name, base=(0.62, 0.30, 0.17)):
    """Bullet jackets (gilding metal / cupronickel)."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    tarnish = g.noise(co, 80.0, detail=3.0)
    color = g.mix(g.math('MULTIPLY', tarnish, 0.5), base, (base[0] * 0.55, base[1] * 0.5, base[2] * 0.47))
    rough = g.math('ADD', 0.26, g.math('MULTIPLY', tarnish, 0.2), clamp=True)
    return _finish(mat, g, bsdf, color, rough, 1.0, None, (0.7, 0.4, 0.25))


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


def cardboard(name, base=(0.36, 0.25, 0.13), label=(0.62, 0.55, 0.42)):
    """Kraft cardboard of the clip box: fibres, stains, a printed label band (object Z near the top of the box)."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    fibres = g.noise(g.mapping(co, scale=(1.0, 6.0, 1.0)), 500.0, detail=2.0)
    stains = g.math('POWER', g.noise(co, 20.0, detail=3.0), 2.4)
    color = g.mix(g.math('MULTIPLY', fibres, 0.25), base, (base[0] * 0.8, base[1] * 0.78, base[2] * 0.75))
    color = g.mix(g.math('MULTIPLY', stains, 0.5), color, (base[0] * 0.55, base[1] * 0.5, base[2] * 0.45))
    sep = g.node('ShaderNodeSeparateXYZ')
    g.link(co, sep.inputs[0])
    band = g.math('MULTIPLY', g.math('GREATER_THAN', sep.outputs['Z'], 0.018), g.math('LESS_THAN', sep.outputs['Z'], 0.032))
    color = g.mix(g.math('MULTIPLY', band, 0.85), color, label)
    edge = edge_mask(g, 0.0015)
    color = g.mix(g.math('MULTIPLY', edge, 0.4), color, (base[0] * 1.3, base[1] * 1.25, base[2] * 1.2))
    rough = g.math('ADD', 0.84, g.math('MULTIPLY', fibres, 0.1), clamp=True)
    normal = g.bump(fibres, 0.3, 0.0004)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.4, 0.3, 0.18))


def flat(name, base, rough=0.5, metal=0.0):
    mat, g, bsdf = _new(name)
    rgb = g.node('ShaderNodeRGB')
    rgb.outputs[0].default_value = (*base, 1)
    return _finish(mat, g, bsdf, rgb.outputs[0], rough, metal, None, base)


def lens(name):
    """Glass of the scope (NOT baked: its own material slot, kept by the Unreal import; swap it for a scope render
    material): dark green-blue coated glass, glossy."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    p = next(n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    p.inputs['Base Color'].default_value = (0.010, 0.022, 0.030, 1)
    p.inputs['Roughness'].default_value = 0.04
    p.inputs['Metallic'].default_value = 0.0
    for key in ('Specular IOR Level', 'Specular'):
        if key in p.inputs:
            p.inputs[key].default_value = 0.9
            break
    mat.diffuse_color = (0.02, 0.04, 0.05, 1)
    mat['pa_note'] = 'scope lens glass: not baked (no textures); a scope render material can replace it in Unreal'
    return mat


def ensure_all():
    """All the source materials (idempotent); densities registered for kit.part_physics."""
    from kit import DENSITY as KD
    KD.update(DENSITY)
    if 'MAT-PA-Walnut' in SOCKETS:
        return
    walnut_cc0('MAT-PA-Walnut')
    walnut_cc0('MAT-PA-WalnutDark', dark=(0.032, 0.014, 0.006), light=(0.12, 0.055, 0.022), worn=(0.20, 0.11, 0.05), ring_mm=2.6)
    blued('MAT-PA-Blued')
    blued('MAT-PA-DarkSteel', base=(0.016, 0.017, 0.019), bright=(0.32, 0.32, 0.34), rough_base=0.42, rust_amount=0.5)
    bright_steel('MAT-PA-BrightSteel')
    brass('MAT-PA-Brass')
    brass('MAT-PA-BrassBright', base=(0.72, 0.50, 0.19), patina=(0.30, 0.20, 0.08), rough_base=0.22)
    case_brass('MAT-PA-CaseBrass')
    case_brass('MAT-PA-CaseFired', base=(0.62, 0.40, 0.15), rough_base=0.30, soot_x=0.034)
    copper('MAT-PA-Copper')
    leather('MAT-PA-Leather', base=(0.070, 0.034, 0.015), worn=(0.15, 0.08, 0.035))
    cardboard('MAT-PA-Cardboard')
    flat('MAT-PA-Bore', (0.020, 0.019, 0.019), 0.55, 1.0)
    flat('MAT-PA-Lacquer', (0.10, 0.012, 0.010), 0.35, 0.0)
    lens('M_PA_Lens')
