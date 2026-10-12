"""Procedural PBR materials of the long arm packs (T-250), authored to be BAKED (bake.py): every material registers the
sockets that feed its base colour, roughness and metallic in SOCKETS, so the baker can route them to an emission
shader one channel at a time. Wear comes from the geometry: the Bevel node edge mask (polished / scratched edges) and
the Pointiness of the mesh, plus an Ambient Occlusion node for the dirt in the crevices. Coordinates are object
space (3D noise: no seams between UV islands). Colours are linear RGB.

Names start with MAT-LA- so the musket's materials (MAT-Musket-*, M_Bean_*) are never touched; kit.py's obj_from_bm
picks an existing material by name, so these are created BEFORE the parts (materials.ensure_all()).
"""
from __future__ import annotations

import bpy

# material name -> {'color': (node, output), 'rough': (node, output), 'metal': (node, output)}
SOCKETS = {}
# density for kit.part_physics (kg/m3)
DENSITY = {'MAT-LA-Walnut': 650, 'MAT-LA-WalnutDark': 650, 'MAT-LA-BrownSteel': 7850, 'MAT-LA-GreySteel': 7850,
           'MAT-LA-Brass': 8500, 'MAT-LA-BrassBright': 8500, 'MAT-LA-Leather': 400, 'MAT-LA-Linen': 250,
           'MAT-LA-Paper': 300, 'MAT-LA-Lead': 11340, 'MAT-LA-Flint': 2600, 'MAT-LA-Powder': 900, 'MAT-LA-Bore': 7850,
           'MAT-LA-Horn': 1300}


class Graph:
    """Small helper around a node tree: every call returns the output socket of the new node."""

    def __init__(self, nt):
        self.nt = nt
        self.x = -1600

    def node(self, kind, **props):
        n = self.nt.nodes.new(kind)
        n.location = (self.x, 0)
        self.x += 180
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, src, dst):
        if src is None:
            return
        if isinstance(src, (int, float)):
            dst.default_value = src
        elif isinstance(src, tuple):
            dst.default_value = (*src, 1.0) if len(src) == 3 and len(dst.default_value) == 4 else src
        else:
            self.nt.links.new(src, dst)

    def math(self, op, a, b=None, c=None, clamp=False):
        n = self.node('ShaderNodeMath', operation=op, use_clamp=clamp)
        self.link(a, n.inputs[0])
        if b is not None:
            self.link(b, n.inputs[1])
        if c is not None:
            self.link(c, n.inputs[2])
        return n.outputs[0]

    def mix(self, fac, a, b):
        """Colour mix (a -> b by fac)."""
        n = self.node('ShaderNodeMix', data_type='RGBA', blend_type='MIX')
        self.link(fac, n.inputs['Factor'])
        self.link(a, n.inputs[6])
        self.link(b, n.inputs[7])
        return n.outputs[2]

    def mixf(self, fac, a, b):
        n = self.node('ShaderNodeMix', data_type='FLOAT')
        self.link(fac, n.inputs['Factor'])
        self.link(a, n.inputs[2])
        self.link(b, n.inputs[3])
        return n.outputs[0]

    def ramp(self, fac, stops):
        n = self.node('ShaderNodeValToRGB')
        r = n.color_ramp
        r.interpolation = 'LINEAR'
        while len(r.elements) > 1:
            r.elements.remove(r.elements[-1])
        r.elements[0].position = stops[0][0]
        r.elements[0].color = (*stops[0][1], 1)
        for pos, col in stops[1:]:
            e = r.elements.new(pos)
            e.color = (*col, 1)
        self.link(fac, n.inputs['Fac'])
        return n.outputs['Color']

    def coords(self):
        return self.node('ShaderNodeTexCoord').outputs['Object']

    def mapping(self, vec, scale=(1, 1, 1), loc=(0, 0, 0)):
        n = self.node('ShaderNodeMapping')
        self.link(vec, n.inputs['Vector'])
        n.inputs['Scale'].default_value = scale
        n.inputs['Location'].default_value = loc
        return n.outputs['Vector']

    def noise(self, vec, scale, detail=3.0, rough=0.5, dist=0.0):
        n = self.node('ShaderNodeTexNoise')
        self.link(vec, n.inputs['Vector'])
        n.inputs['Scale'].default_value = scale
        n.inputs['Detail'].default_value = detail
        n.inputs['Roughness'].default_value = rough
        n.inputs['Distortion'].default_value = dist
        return n.outputs['Fac']

    def voronoi(self, vec, scale, feature='F1'):
        n = self.node('ShaderNodeTexVoronoi', feature=feature)
        self.link(vec, n.inputs['Vector'])
        n.inputs['Scale'].default_value = scale
        return n.outputs['Distance']

    def wave(self, vec, scale, direction='X', distortion=0.0, detail=2.0, kind='BANDS'):
        n = self.node('ShaderNodeTexWave', wave_type=kind, bands_direction=direction)
        self.link(vec, n.inputs['Vector'])
        n.inputs['Scale'].default_value = scale
        n.inputs['Distortion'].default_value = distortion
        n.inputs['Detail'].default_value = detail
        return n.outputs['Fac']

    def edge(self, radius=0.0025, strength=1.0):
        """0 on flat surfaces, 1 on convex edges (Bevel node normal against the true normal) merged with pointiness."""
        bev = self.node('ShaderNodeBevel', samples=4)
        bev.inputs['Radius'].default_value = radius
        geo = self.node('ShaderNodeNewGeometry')
        dot = self.node('ShaderNodeVectorMath', operation='DOT_PRODUCT')
        self.link(bev.outputs['Normal'], dot.inputs[0])
        self.link(geo.outputs['Normal'], dot.inputs[1])
        e = self.math('SUBTRACT', 1.0, dot.outputs['Value'])
        e = self.math('MULTIPLY', e, 14.0 * strength, clamp=True)
        p = self.node('ShaderNodeMapRange')
        self.link(geo.outputs['Pointiness'], p.inputs['Value'])
        p.inputs['From Min'].default_value = 0.52
        p.inputs['From Max'].default_value = 0.62
        p.clamp = True
        return self.math('MAXIMUM', e, p.outputs['Result'], clamp=True)

    def cavity(self, distance=0.03):
        """1 in the crevices (Ambient Occlusion node), 0 on open surfaces."""
        ao = self.node('ShaderNodeAmbientOcclusion', samples=8)
        ao.inputs['Distance'].default_value = distance
        return self.math('SUBTRACT', 1.0, ao.outputs['AO'], clamp=True)

    def bump(self, height, strength, distance=0.001):
        n = self.node('ShaderNodeBump')
        self.link(height, n.inputs['Height'])
        n.inputs['Strength'].default_value = strength
        n.inputs['Distance'].default_value = distance
        return n.outputs['Normal']


def _new(name):
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    out.location = (400, 0)
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.location = (100, 0)
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    return mat, Graph(nt), bsdf


def _finish(mat, g, bsdf, color, rough, metal, normal=None, diffuse=(0.5, 0.5, 0.5)):
    g.link(color, bsdf.inputs['Base Color'])
    g.link(rough, bsdf.inputs['Roughness'])
    g.link(metal, bsdf.inputs['Metallic'])
    if normal is not None:
        g.link(normal, bsdf.inputs['Normal'])
    mat.diffuse_color = (*diffuse, 1)
    SOCKETS[mat.name] = {'color': color, 'rough': rough, 'metal': metal}
    return mat


# ------------------------------------------------------------------------------------------------- recipes

def wood(name, dark=(0.075, 0.032, 0.012), light=(0.21, 0.100, 0.038), worn=(0.33, 0.20, 0.09), ring_mm=3.2, axis=(0.0, 0.09, -0.06)):
    """Oiled walnut: growth rings around an axis parallel to X (the stock is cut from a plank along the barrel), fine
    pores along the grain, edges worn lighter, crevices darker."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    # distance from the ring axis (y0, z0): rings = sin(distance / ring spacing) distorted by noise
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
    edge = g.edge(0.002)
    color = g.mix(g.math('MULTIPLY', edge, 0.55), color, worn)
    cav = g.cavity(0.025)
    color = g.mix(g.math('MULTIPLY', cav, 0.6), color, (0.05, 0.025, 0.012))
    rough = g.math('ADD', 0.40, g.math('MULTIPLY', grain, 0.22))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.12), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', rings, 0.6), g.math('MULTIPLY', pores, 0.4)), 0.35, 0.0010)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.25, 0.13, 0.05))


def steel(name, base=(0.085, 0.072, 0.062), bright=(0.56, 0.56, 0.58), rough_base=0.38, pits=True, mottle=None):
    """Browned (rust brown finish) barrel and lock steel: brushed streaks along X, pits, edges worn to bare metal."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    streaks = g.noise(g.mapping(co, scale=(2.0, 90.0, 90.0)), 8.0, detail=3.0, rough=0.55)
    color = g.mix(g.math('MULTIPLY', streaks, 0.5), g.mix(0.5, base, (base[0] * 0.7, base[1] * 0.7, base[2] * 0.7)), base)
    if mottle:
        m = g.noise(co, 18.0, detail=2.0)
        color = g.mix(g.math('MULTIPLY', m, 0.6), color, mottle)
    scratches = g.noise(g.mapping(co, scale=(1.0, 40.0, 40.0)), 60.0, detail=1.0)
    scratches = g.math('GREATER_THAN', scratches, 0.80)
    edge = g.edge(0.0018)
    wear = g.math('MAXIMUM', g.math('MULTIPLY', edge, 0.85), g.math('MULTIPLY', scratches, 0.45), clamp=True)
    color = g.mix(wear, color, bright)
    height = g.math('MULTIPLY', streaks, 0.4)
    if pits:
        pit = g.math('GREATER_THAN', g.noise(co, 320.0, detail=2.0), 0.73)
        color = g.mix(g.math('MULTIPLY', pit, 0.7), color, (0.03, 0.025, 0.02))
        height = g.math('SUBTRACT', height, g.math('MULTIPLY', pit, 0.8))
    cav = g.cavity(0.02)
    color = g.mix(g.math('MULTIPLY', cav, 0.5), color, (0.02, 0.018, 0.016))
    rough = g.math('ADD', rough_base, g.math('MULTIPLY', streaks, 0.20))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', wear, 0.18), clamp=True)
    normal = g.bump(height, 0.25, 0.0005)
    return _finish(mat, g, bsdf, color, rough, 1.0, normal, (0.25, 0.22, 0.2))


def brass(name, base=(0.78, 0.55, 0.22), patina=(0.30, 0.22, 0.09), polished=(0.96, 0.78, 0.42), rough_base=0.30):
    """Cast brass furniture: patina in the crevices and in blotches, polished on the edges, fine scratches."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    blotch = g.noise(co, 35.0, detail=3.0, rough=0.6)
    color = g.mix(g.math('MULTIPLY', g.math('POWER', blotch, 1.6), 0.5), base, patina)
    cav = g.cavity(0.012)
    color = g.mix(g.math('MULTIPLY', cav, 0.75), color, patina)
    edge = g.edge(0.0015)
    color = g.mix(g.math('MULTIPLY', edge, 0.8), color, polished)
    scratches = g.math('GREATER_THAN', g.noise(g.mapping(co, scale=(1.0, 30.0, 30.0)), 90.0, detail=1.0), 0.82)
    color = g.mix(g.math('MULTIPLY', scratches, 0.35), color, polished)
    rough = g.math('ADD', rough_base, g.math('MULTIPLY', blotch, 0.22))
    rough = g.math('ADD', rough, g.math('MULTIPLY', cav, 0.25))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.14), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', g.noise(co, 500.0, detail=1.0), 0.5), g.math('MULTIPLY', scratches, 0.5)), 0.15, 0.0004)
    return _finish(mat, g, bsdf, color, rough, 1.0, normal, (0.8, 0.6, 0.25))


def leather(name, base=(0.11, 0.058, 0.030), worn=(0.26, 0.15, 0.08)):
    mat, g, bsdf = _new(name)
    co = g.coords()
    pores = g.voronoi(co, 420.0)
    cracks = g.noise(co, 60.0, detail=4.0, rough=0.7)
    color = g.mix(g.math('MULTIPLY', cracks, 0.35), base, (base[0] * 0.6, base[1] * 0.6, base[2] * 0.6))
    edge = g.edge(0.003)
    color = g.mix(g.math('MULTIPLY', edge, 0.6), color, worn)
    rough = g.math('ADD', 0.58, g.math('MULTIPLY', cracks, 0.18))
    rough = g.math('SUBTRACT', rough, g.math('MULTIPLY', edge, 0.15), clamp=True)
    normal = g.bump(g.math('ADD', g.math('MULTIPLY', pores, 0.6), g.math('MULTIPLY', cracks, 0.4)), 0.5, 0.0006)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.2, 0.1, 0.05))


def linen(name, base=(0.52, 0.45, 0.33), dirt=(0.26, 0.21, 0.14)):
    """Coarse linen: two crossed thread waves, dirt in the folds."""
    mat, g, bsdf = _new(name)
    co = g.coords()
    warp = g.wave(co, 900.0, 'X', distortion=0.6, detail=1.0)
    weft = g.wave(co, 900.0, 'Z', distortion=0.6, detail=1.0)
    weave = g.math('MULTIPLY', g.math('ADD', warp, weft), 0.5)
    stains = g.noise(co, 25.0, detail=3.0)
    color = g.mix(g.math('MULTIPLY', weave, 0.25), base, (base[0] * 0.8, base[1] * 0.8, base[2] * 0.8))
    color = g.mix(g.math('MULTIPLY', g.math('POWER', stains, 2.0), 0.6), color, dirt)
    cav = g.cavity(0.01)
    color = g.mix(g.math('MULTIPLY', cav, 0.5), color, dirt)
    rough = g.math('ADD', 0.82, g.math('MULTIPLY', weave, 0.15), clamp=True)
    normal = g.bump(weave, 0.6, 0.0004)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.7, 0.63, 0.48))


def paper(name, base=(0.78, 0.71, 0.56)):
    mat, g, bsdf = _new(name)
    co = g.coords()
    crinkle = g.noise(co, 120.0, detail=4.0, rough=0.65)
    fibres = g.noise(g.mapping(co, scale=(1.0, 8.0, 8.0)), 400.0, detail=1.0)
    stains = g.noise(co, 30.0, detail=2.0)
    color = g.mix(g.math('MULTIPLY', g.math('POWER', stains, 2.2), 0.5), base, (0.45, 0.38, 0.26))
    color = g.mix(g.math('MULTIPLY', fibres, 0.15), color, (0.9, 0.85, 0.72))
    rough = g.math('ADD', 0.80, g.math('MULTIPLY', crinkle, 0.15), clamp=True)
    normal = g.bump(crinkle, 0.4, 0.0006)
    return _finish(mat, g, bsdf, color, rough, 0.0, normal, (0.8, 0.72, 0.55))


def lead(name, base=(0.34, 0.35, 0.38)):
    mat, g, bsdf = _new(name)
    co = g.coords()
    oxide = g.noise(co, 200.0, detail=3.0)
    color = g.mix(g.math('MULTIPLY', oxide, 0.5), base, (0.20, 0.21, 0.24))
    rough = g.math('ADD', 0.45, g.math('MULTIPLY', oxide, 0.3), clamp=True)
    normal = g.bump(oxide, 0.2, 0.0004)
    return _finish(mat, g, bsdf, color, rough, 0.7, normal, (0.35, 0.36, 0.4))


def flat(name, base, rough=0.5, metal=0.0, diffuse=None):
    mat, g, bsdf = _new(name)
    rgb = g.node('ShaderNodeRGB')
    rgb.outputs[0].default_value = (*base, 1)
    return _finish(mat, g, bsdf, rgb.outputs[0], rough, metal, None, diffuse or base)


def ensure_all():
    """All the materials of the long arm packs (idempotent); registers their densities for kit.part_physics."""
    from musket_ref import K
    K.DENSITY.update(DENSITY)
    if 'MAT-LA-Walnut' in SOCKETS:
        return
    wood('MAT-LA-Walnut')
    wood('MAT-LA-WalnutDark', dark=(0.045, 0.019, 0.008), light=(0.13, 0.060, 0.022), worn=(0.22, 0.13, 0.06), ring_mm=2.6)
    steel('MAT-LA-BrownSteel')
    steel('MAT-LA-GreySteel', base=(0.16, 0.165, 0.18), bright=(0.62, 0.62, 0.64), rough_base=0.32, pits=False,
          mottle=(0.12, 0.13, 0.19))
    brass('MAT-LA-Brass')
    brass('MAT-LA-BrassBright', base=(0.86, 0.64, 0.28), patina=(0.42, 0.31, 0.13), rough_base=0.24)
    leather('MAT-LA-Leather')
    linen('MAT-LA-Linen')
    paper('MAT-LA-Paper')
    lead('MAT-LA-Lead')
    flat('MAT-LA-Flint', (0.09, 0.09, 0.11), 0.22, 0.0)
    flat('MAT-LA-Powder', (0.012, 0.012, 0.013), 0.92, 0.0)
    flat('MAT-LA-Bore', (0.025, 0.024, 0.024), 0.55, 1.0)
    flat('MAT-LA-Horn', (0.42, 0.33, 0.18), 0.35, 0.0)
