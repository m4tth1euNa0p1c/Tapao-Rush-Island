"""Materials of the pistol pack (T-248): procedural Cycles shaders (object space, so the look is stable on every part),
baked into one PBR atlas per weapon, then one game material per atlas.

Baked maps (labels of the Image Texture nodes of the exported material, listed by export_pack.py):
  BaseColor  sRGB PNG         albedo with edge wear (Pointiness) and cavity dirt (AO node)
  Normal     linear PNG       tangent space, DirectX convention (green down): rounded edges (Bevel node) + grain,
                              brushing and engraving bumps
  ORM        linear PNG       R ambient occlusion, G roughness, B metallic
"""
from __future__ import annotations

import math
import time
from pathlib import Path

import bpy
import numpy as np

import meshkit as K


# ------------------------------------------------------------------------------------------- node helpers

class Nodes:
    def __init__(self, mat):
        mat.use_nodes = True
        self.tree = mat.node_tree
        self.nodes, self.links = self.tree.nodes, self.tree.links
        for n in list(self.nodes):
            self.nodes.remove(n)
        self._x = 0
        self.out = self.new('ShaderNodeOutputMaterial')
        self.bsdf = self.new('ShaderNodeBsdfPrincipled')
        self.link(self.bsdf.outputs[0], self.out.inputs['Surface'])
        self.coord = self.new('ShaderNodeTexCoord')
        self.geo = self.new('ShaderNodeNewGeometry')

    def new(self, kind, **props):
        n = self.nodes.new(kind)
        self._x += 1
        n.location = (-200 * (self._x % 9), -150 * (self._x // 9))
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def link(self, a, b):
        self.links.new(a, b)

    def value(self, v):
        n = self.new('ShaderNodeValue')
        n.outputs[0].default_value = v
        return n.outputs[0]

    def mapping(self, scale=(1, 1, 1), rot=(0, 0, 0)):
        m = self.new('ShaderNodeMapping')
        m.inputs['Scale'].default_value = scale
        m.inputs['Rotation'].default_value = rot
        self.link(self.coord.outputs['Object'], m.inputs['Vector'])
        return m.outputs['Vector']

    def noise(self, scale, detail=2.0, rough=.5, vec=None, dim='3D'):
        n = self.new('ShaderNodeTexNoise')
        n.noise_dimensions = dim
        n.inputs['Scale'].default_value = scale
        n.inputs['Detail'].default_value = detail
        n.inputs['Roughness'].default_value = rough
        self.link(vec if vec is not None else self.coord.outputs['Object'], n.inputs['Vector'])
        return n.outputs['Fac']

    def wave(self, scale, distortion=0.0, detail=1.0, vec=None, kind='BANDS', direction='X', profile='SIN'):
        w = self.new('ShaderNodeTexWave')
        w.wave_type = kind
        w.bands_direction = direction
        w.wave_profile = profile
        w.inputs['Scale'].default_value = scale
        w.inputs['Distortion'].default_value = distortion
        w.inputs['Detail'].default_value = detail
        self.link(vec if vec is not None else self.coord.outputs['Object'], w.inputs['Vector'])
        return w.outputs['Fac']

    def voronoi(self, scale, vec=None, feature='DISTANCE_TO_EDGE'):
        v = self.new('ShaderNodeTexVoronoi')
        v.feature = feature
        v.inputs['Scale'].default_value = scale
        self.link(vec if vec is not None else self.coord.outputs['Object'], v.inputs['Vector'])
        return v.outputs['Distance']

    def math(self, op, a, b=None, clamp=False):
        m = self.new('ShaderNodeMath')
        m.operation = op
        m.use_clamp = clamp
        self._in(m.inputs[0], a)
        if b is not None:
            self._in(m.inputs[1], b)
        return m.outputs[0]

    def ramp(self, fac, stops, interp='LINEAR'):
        """stops = [(pos, (r, g, b)) or (pos, value)]"""
        r = self.new('ShaderNodeValToRGB')
        r.color_ramp.interpolation = interp
        el = r.color_ramp.elements
        while len(el) > 1:
            el.remove(el[-1])
        for i, (pos, col) in enumerate(stops):
            e = el[0] if i == 0 else el.new(pos)
            e.position = pos
            if isinstance(col, (int, float)):
                col = (col, col, col)
            e.color = (*col, 1)
        self._in(r.inputs['Fac'], fac)
        return r.outputs['Color']

    def mix(self, fac, a, b):
        m = self.new('ShaderNodeMix')
        m.data_type = 'RGBA'
        self._in(m.inputs['Factor'], fac)
        self._in(m.inputs[6], a)
        self._in(m.inputs[7], b)
        return m.outputs[2]

    def mixf(self, fac, a, b):
        m = self.new('ShaderNodeMix')
        m.data_type = 'FLOAT'
        self._in(m.inputs['Factor'], fac)
        self._in(m.inputs[2], a)
        self._in(m.inputs[3], b)
        return m.outputs[0]

    def _in(self, socket, v):
        if isinstance(v, bpy.types.NodeSocket):
            self.link(v, socket)
        elif isinstance(v, (tuple, list)) and len(v) == 3 and socket.type == 'RGBA':
            socket.default_value = (*v, 1)
        elif isinstance(v, (tuple, list)):
            socket.default_value = v
        else:
            socket.default_value = v

    def edges(self, strength=1.0, width=.015, radius=.0016):
        """Edge mask (1 on sharp convex edges): 1 - dot(bevelled normal, true normal), so only real edges count, not
        the facets of the low poly cylinders (Pointiness would flag every vertex of a thin rod)."""
        bv = self.new('ShaderNodeBevel')
        bv.samples = 8
        bv.inputs['Radius'].default_value = radius
        dot = self.new('ShaderNodeVectorMath')
        dot.operation = 'DOT_PRODUCT'
        self.link(bv.outputs['Normal'], dot.inputs[0])
        self.link(self.geo.outputs['Normal'], dot.inputs[1])
        d = self.math('SUBTRACT', 1.0, dot.outputs['Value'])
        return self.ramp(d, [(.035, 0.0), (.035 + width * 8, strength)])

    def cavity(self, distance=.012, strength=1.0):
        """Cavity mask from the AO node (1 in crevices)."""
        ao = self.new('ShaderNodeAmbientOcclusion')
        ao.inputs['Distance'].default_value = distance
        ao.samples = 8
        ao.only_local = True
        return self.ramp(ao.outputs['AO'], [(.45, strength), (1.0, 0.0)])

    def surface(self, color, roughness, metallic, bump_height=None, bump_strength=.1, bevel=.0007):
        self._in(self.bsdf.inputs['Base Color'], color)
        self._in(self.bsdf.inputs['Roughness'], roughness)
        self._in(self.bsdf.inputs['Metallic'], metallic)
        normal = None
        if bevel:
            bv = self.new('ShaderNodeBevel')
            bv.samples = 6
            bv.inputs['Radius'].default_value = bevel
            normal = bv.outputs['Normal']
        if bump_height is not None:
            bump = self.new('ShaderNodeBump')
            bump.inputs['Strength'].default_value = bump_strength
            bump.inputs['Distance'].default_value = .0015
            self._in(bump.inputs['Height'], bump_height)
            if normal is not None:
                self.link(normal, bump.inputs['Normal'])
            normal = bump.outputs['Normal']
        if normal is not None:
            self.link(normal, self.bsdf.inputs['Normal'])


def _mat(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    return m


# ------------------------------------------------------------------------------------------- the materials

def create_materials(prefix='Pistol'):
    """Procedural look-dev materials (Cycles only: Pointiness, AO and Bevel nodes). Keys used by pistol_model."""
    mats = {}

    # walnut: grain along X distorted by noise, fine flecks, varnish; lighter worn edges, dirt in the cavities
    m = _mat('MAT-%s-Walnut' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.17, .08, .035, 1)
    flow = n.mapping(scale=(1.0, 3.2, 2.6), rot=(0, 0, .10))
    grain = n.wave(110.0, distortion=.55, detail=2.5, vec=flow, direction='Y')
    fleck = n.noise(900.0, detail=3.0, rough=.7)
    broad = n.noise(9.0, detail=2.0, rough=.5)
    tone = n.math('ADD', n.math('MULTIPLY', grain, .46), n.math('MULTIPLY', broad, .42))
    tone = n.math('ADD', tone, n.math('MULTIPLY', n.math('SUBTRACT', fleck, .5), .20), clamp=True)
    color = n.ramp(tone, [(0.0, (.030, .012, .006)), (.45, (.072, .031, .013)), (.75, (.125, .060, .024)), (1.0, (.185, .095, .040))])
    edge = n.edges(.40, .012)
    cav = n.cavity(.010, .65)
    color = n.mix(edge, color, (.26, .15, .065))
    color = n.mix(cav, color, (.045, .022, .010))
    rough = n.mixf(edge, n.math('ADD', .36, n.math('MULTIPLY', grain, .14)), .62)
    rough = n.mixf(cav, rough, .75)
    n.surface(color, rough, 0.0, n.math('ADD', n.math('MULTIPLY', grain, .7), n.math('MULTIPLY', fleck, .3)), .10, .0006)
    mats['wood'] = m

    # brass: warm metal, tarnish patches, polished edges, dark cavities, light engraving (bump) on the surface
    m = _mat('MAT-%s-Brass' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.78, .55, .22, 1)
    tarnish = n.ramp(n.noise(55.0, detail=3.0, rough=.6), [(.42, 0.0), (.70, 1.0)])
    spots = n.ramp(n.noise(220.0, detail=2.0), [(.55, 0.0), (.75, 1.0)])
    edge = n.edges(.9, .010)
    cav = n.cavity(.008, .8)
    color = n.mix(tarnish, (.84, .60, .26), (.46, .32, .14))
    color = n.mix(n.math('MULTIPLY', spots, .6), color, (.30, .20, .10))
    color = n.mix(edge, color, (.98, .80, .45))
    color = n.mix(cav, color, (.18, .12, .05))
    rough = n.mixf(tarnish, .28, .52)
    rough = n.mixf(edge, rough, .18)
    rough = n.mixf(cav, rough, .62)
    engrave = n.wave(260.0, distortion=4.0, detail=2.5, vec=n.mapping(scale=(1, 1, 1), rot=(.3, .7, .2)), kind='RINGS', profile='SAW')
    engrave = n.ramp(engrave, [(.80, 0.0), (.86, 1.0), (.92, 0.0)], 'LINEAR')
    scroll = n.ramp(n.voronoi(70.0, feature='DISTANCE_TO_EDGE'), [(0.0, 1.0), (.012, 1.0), (.020, 0.0)])
    height = n.math('MAXIMUM', n.math('MULTIPLY', engrave, .6), scroll)
    n.surface(color, rough, 1.0, n.math('SUBTRACT', 1.0, height), .18, .0007)
    mats['brass'] = m

    # blued steel: deep blue-black, brushed along the barrel, bare bright wear on edges, oily dark cavities
    m = _mat('MAT-%s-BluedSteel' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.06, .07, .09, 1)
    brush = n.noise(60.0, detail=4.0, rough=.8, vec=n.mapping(scale=(1.0, 60.0, 60.0)))
    mottle = n.noise(30.0, detail=2.0)
    edge = n.edges(1.0, .009)
    cav = n.cavity(.008, .7)
    color = n.mix(mottle, (.045, .055, .075), (.075, .085, .105))
    color = n.mix(n.math('MULTIPLY', n.ramp(n.noise(140.0, detail=2.0), [(.6, 0.0), (.8, 1.0)]), .5), color, (.16, .14, .11))
    color = n.mix(edge, color, (.50, .50, .50))
    color = n.mix(cav, color, (.02, .022, .026))
    rough = n.math('ADD', .26, n.math('MULTIPLY', brush, .16))
    rough = n.mixf(edge, rough, .22)
    rough = n.mixf(cav, rough, .55)
    n.surface(color, rough, 1.0, brush, .035, .0006)
    mats['steel'] = m

    # bright (polished / case-hardened) steel: screws, jaws, frizzen face, trigger
    m = _mat('MAT-%s-BrightSteel' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.55, .55, .54, 1)
    scratch = n.noise(300.0, detail=3.0, rough=.8, vec=n.mapping(scale=(1.0, 8.0, 8.0)))
    cav = n.cavity(.006, .7)
    edge = n.edges(.8, .008)
    color = n.mix(n.math('MULTIPLY', n.noise(40.0), .5), (.56, .56, .55), (.42, .40, .38))
    color = n.mix(edge, color, (.72, .72, .70))
    color = n.mix(cav, color, (.12, .12, .13))
    rough = n.math('ADD', .24, n.math('MULTIPLY', scratch, .18))
    rough = n.mixf(cav, rough, .5)
    n.surface(color, rough, 1.0, scratch, .03, .0005)
    mats['bright'] = m

    # iron (belt hook): dark, matte, rust in the pits
    m = _mat('MAT-%s-Iron' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.2, .2, .21, 1)
    rust = n.ramp(n.noise(90.0, detail=3.0), [(.5, 0.0), (.72, 1.0)])
    edge = n.edges(.8, .009)
    cav = n.cavity(.008, .7)
    color = n.mix(rust, (.20, .20, .21), (.30, .15, .07))
    color = n.mix(edge, color, (.45, .45, .45))
    color = n.mix(cav, color, (.07, .06, .05))
    rough = n.mixf(rust, .42, .72)
    rough = n.mixf(edge, rough, .3)
    n.surface(color, rough, 1.0, n.noise(400.0, detail=2.0), .05, .0006)
    mats['iron'] = m

    # flint, leather, dark recess, paper, cord, lead
    m = _mat('MAT-%s-Flint' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.2, .19, .17, 1)
    color = n.mix(n.noise(120.0, detail=3.0), (.14, .13, .12), (.30, .28, .24))
    n.surface(color, n.math('ADD', .38, n.math('MULTIPLY', n.noise(300.0), .2)), 0.0, n.noise(500.0, detail=2.0), .08, .0004)
    mats['flint'] = m

    m = _mat('MAT-%s-Leather' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.09, .05, .03, 1)
    pores = n.noise(350.0, detail=3.0, rough=.7)
    color = n.mix(n.noise(25.0), (.075, .042, .024), (.12, .07, .04))
    color = n.mix(n.edges(.6, .012), color, (.22, .14, .08))
    n.surface(color, n.math('ADD', .62, n.math('MULTIPLY', pores, .2)), 0.0, pores, .12, .0005)
    mats['leather'] = m

    m = _mat('MAT-%s-Recess' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.02, .02, .02, 1)
    n.surface((.018, .018, .02), .75, .4, None, 0, .0)
    mats['recess'] = m

    m = _mat('MAT-%s-Paper' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.8, .72, .55, 1)
    fibre = n.noise(200.0, detail=3.0, rough=.6, vec=n.mapping(scale=(6.0, 1.0, 1.0)))
    color = n.mix(n.noise(30.0), (.62, .54, .38), (.48, .40, .26))
    color = n.mix(n.cavity(.006, .6), color, (.28, .22, .12))
    n.surface(color, n.math('ADD', .78, n.math('MULTIPLY', fibre, .15)), 0.0, fibre, .10, .0004)
    mats['paper'] = m

    m = _mat('MAT-%s-Cord' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.45, .35, .2, 1)
    twist = n.wave(600.0, 0.4, 1.0, kind='BANDS', direction='X', profile='SIN')
    color = n.mix(twist, (.40, .30, .16), (.55, .44, .26))
    n.surface(color, .85, 0.0, twist, .25, .0003)
    mats['cord'] = m

    m = _mat('MAT-%s-Lead' % prefix)
    n = Nodes(m)
    m.diffuse_color = (.38, .38, .4, 1)
    color = n.mix(n.noise(200.0, detail=3.0), (.34, .34, .36), (.48, .48, .50))
    n.surface(color, n.math('ADD', .42, n.math('MULTIPLY', n.noise(80.0), .2)), 1.0, n.noise(300.0), .06, .0005)
    mats['lead'] = m
    return mats


# ------------------------------------------------------------------------------------------- baking

def cycles_gpu(scene):
    """Cycles on the GPU when one is there (RTX 5060 Ti on this PC), else the CPU."""
    scene.render.engine = 'CYCLES'
    try:
        prefs = bpy.context.preferences.addons['cycles'].preferences
        for kind in ('OPTIX', 'CUDA'):
            try:
                prefs.compute_device_type = kind
                prefs.refresh_devices() if hasattr(prefs, 'refresh_devices') else prefs.get_devices()
                gpus = [d for d in prefs.devices if d.type == kind]
                if gpus:
                    for d in prefs.devices:
                        d.use = d.type == kind
                    scene.cycles.device = 'GPU'
                    return kind
            except Exception:  # noqa: BLE001
                continue
    except Exception:  # noqa: BLE001
        pass
    scene.cycles.device = 'CPU'
    return 'CPU'


def _image(name, size, data, color=(0, 0, 0, 1)):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False, is_data=data)
    img.colorspace_settings.name = 'Non-Color' if data else 'sRGB'
    img.generated_color = color
    return img


def _pixels(img):
    size = img.size[0]
    arr = np.empty(size * size * 4, dtype=np.float32)
    img.pixels.foreach_get(arr)
    return arr.reshape(size, size, 4)


def _save(img, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.filepath_raw = str(path)
    img.file_format = 'PNG'
    img.save()
    return path


def _with_emission(mats, socket_name):
    """Temporarily output the given Principled input (Metallic...) as emission, for an EMIT bake. Returns an undo."""
    undo = []
    for mat in mats:
        tree = mat.node_tree
        bsdf = K.principled(mat)
        out = next(n for n in tree.nodes if n.type == 'OUTPUT_MATERIAL')
        emit = tree.nodes.new('ShaderNodeEmission')
        emit.inputs['Strength'].default_value = 1.0
        src = bsdf.inputs[socket_name]
        if src.links:
            tree.links.new(src.links[0].from_socket, emit.inputs['Color'])
        else:
            v = src.default_value
            if isinstance(v, (int, float)):
                emit.inputs['Color'].default_value = (v, v, v, 1)
            else:
                emit.inputs['Color'].default_value = tuple(v)
        old = out.inputs['Surface'].links[0].from_socket
        tree.links.new(emit.outputs[0], out.inputs['Surface'])
        undo.append((tree, out, old, emit))

    def restore():
        for tree, out, old, emit in undo:
            tree.links.new(old, out.inputs['Surface'])
            tree.nodes.remove(emit)
    return restore


def bake_atlas(scene, objects, size, out_dir, texture_prefix, samples_color=24, samples_ao=48, margin=12, log=print):
    """Bakes BaseColor / Normal (DirectX) / ORM of the objects (which share one UV atlas and the procedural
    materials) into <out_dir>/<texture_prefix>_<Map>.png. Returns {label: path}."""
    device = cycles_gpu(scene)
    scene.cycles.use_denoising = False
    bake = scene.render.bake
    bake.margin = margin
    bake.margin_type = 'EXTEND'
    bake.use_selected_to_active = False
    bake.use_clear = True
    bake.target = 'IMAGE_TEXTURES'
    mats = []
    for ob in objects:
        for m in ob.data.materials:
            if m and m not in mats:
                mats.append(m)
    nodes = {}
    for m in mats:
        node = m.node_tree.nodes.new('ShaderNodeTexImage')
        node.name = node.label = 'BAKE_TARGET'
        nodes[m] = node

    def target(img):
        for m, node in nodes.items():
            node.image = img
            m.node_tree.nodes.active = node

    def run(kind, img, samples, **kw):
        target(img)
        scene.cycles.samples = samples
        t0 = time.time()
        with bpy.context.temp_override(selected_objects=list(objects), active_object=objects[0], object=objects[0],
                                       selected_editable_objects=list(objects)):
            for ob in bpy.context.view_layer.objects:
                ob.select_set(ob in objects)
            bpy.context.view_layer.objects.active = objects[0]
            bpy.ops.object.bake(type=kind, **kw)
        log('[bake] %s %dpx %d spp on %s: %.1fs' % (kind, size, samples, device, time.time() - t0))

    out_dir = Path(out_dir)
    paths = {}
    # base colour: the Base Color input emitted as is (a metal has no diffuse colour pass), sRGB image
    base = _image(texture_prefix + '_BaseColor', size, False, (.5, .5, .5, 1))
    restore = _with_emission(mats, 'Base Color')
    run('EMIT', base, samples_color)
    restore()
    paths['BaseColor'] = _save(base, out_dir / (texture_prefix + '_BaseColor.png'))
    # normal: tangent space, green down (DirectX / Unreal)
    normal = _image(texture_prefix + '_Normal', size, True, (.5, .5, 1, 1))
    bake.normal_space = 'TANGENT'
    bake.normal_r, bake.normal_g, bake.normal_b = 'POS_X', 'NEG_Y', 'POS_Z'
    run('NORMAL', normal, 4)
    paths['Normal'] = _save(normal, out_dir / (texture_prefix + '_Normal.png'))
    # ORM: AO, roughness, metallic (emission trick) composed with numpy
    ao = _image(texture_prefix + '_AO_tmp', size, True, (1, 1, 1, 1))
    run('AO', ao, samples_ao)
    rough = _image(texture_prefix + '_Rough_tmp', size, True)
    run('ROUGHNESS', rough, 4)
    metal = _image(texture_prefix + '_Metal_tmp', size, True)
    restore = _with_emission(mats, 'Metallic')
    run('EMIT', metal, 4)
    restore()
    orm = _image(texture_prefix + '_ORM', size, True, (1, .5, 0, 1))
    px = np.ones((size, size, 4), dtype=np.float32)
    px[:, :, 0] = _pixels(ao)[:, :, 0]
    px[:, :, 1] = _pixels(rough)[:, :, 0]
    px[:, :, 2] = np.clip(_pixels(metal)[:, :, 0], 0, 1)
    orm.pixels.foreach_set(px.ravel())
    paths['ORM'] = _save(orm, out_dir / (texture_prefix + '_ORM.png'))
    for img in (ao, rough, metal):
        bpy.data.images.remove(img)
    for m, node in nodes.items():
        m.node_tree.nodes.remove(node)
    return paths


def baked_material(name, paths):
    """Game material of an atlas: BaseColor / Normal / ORM image nodes (labelled) on a Principled BSDF."""
    m = _mat(name)
    n = Nodes(m)
    tree = m.node_tree
    imgs = {}
    for label, path in paths.items():
        node = tree.nodes.new('ShaderNodeTexImage')
        node.label = node.name = label
        img = bpy.data.images.load(str(path), check_existing=True)
        img.colorspace_settings.name = 'sRGB' if label == 'BaseColor' else 'Non-Color'
        node.image = img
        imgs[label] = node
    n.link(imgs['BaseColor'].outputs['Color'], n.bsdf.inputs['Base Color'])
    sep = tree.nodes.new('ShaderNodeSeparateColor')
    n.link(imgs['ORM'].outputs['Color'], sep.inputs['Color'])
    n.link(sep.outputs['Green'], n.bsdf.inputs['Roughness'])
    n.link(sep.outputs['Blue'], n.bsdf.inputs['Metallic'])
    # the file is DirectX (green down): flip it back for Blender's own renders
    nsep = tree.nodes.new('ShaderNodeSeparateColor')
    n.link(imgs['Normal'].outputs['Color'], nsep.inputs['Color'])
    inv = tree.nodes.new('ShaderNodeMath')
    inv.operation = 'SUBTRACT'
    inv.inputs[0].default_value = 1.0
    n.link(nsep.outputs['Green'], inv.inputs[1])
    comb = tree.nodes.new('ShaderNodeCombineColor')
    n.link(nsep.outputs['Red'], comb.inputs['Red'])
    n.link(inv.outputs[0], comb.inputs['Green'])
    n.link(nsep.outputs['Blue'], comb.inputs['Blue'])
    nm = tree.nodes.new('ShaderNodeNormalMap')
    nm.space = 'TANGENT'
    n.link(comb.outputs['Color'], nm.inputs['Color'])
    n.link(nm.outputs['Normal'], n.bsdf.inputs['Normal'])
    m['baked_textures'] = {k: str(v) for k, v in paths.items()}
    m['normal_convention'] = 'DirectX (green down)'
    return m


def apply_baked(objects, material):
    for ob in objects:
        ob.data.materials.clear()
        ob.data.materials.append(material)
        for p in ob.data.polygons:
            p.material_index = 0
