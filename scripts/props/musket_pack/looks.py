"""Procedural PBR looks of the musket pack v2 (T-246) and their baking into textures.

A part is built with a temporary material ("TMP-<Atlas>-<look>") that carries a procedural look (walnut, browned
iron, patinated brass, leather, paper, horn, lead, flint...) and the atlas it belongs to. After the geometry is
joined, bake_all():
  1. unwraps the faces of every atlas (smart UV project + pack, across the objects sharing the atlas);
  2. bakes with Cycles: base colour (diffuse colour, with the dirt of the AO node and the edge wear of pointiness),
     tangent normal (the procedural bumps), roughness, metallic and ambient occlusion (emission bakes);
  3. writes T_<Atlas>_D.png (sRGB), T_<Atlas>_N.png (DirectX: green down, for Unreal without inversion) and
     T_<Atlas>_ORM.png (linear: R occlusion, G roughness, B metallic);
  4. replaces the temporary materials by one final material per atlas (M_Musket_Wood, M_Ammo_PowderHorn...) whose
     Image Texture nodes are labelled BaseColor / Normal / ORM: export_pack.py lists them in the manifest by label.
The Blender preview re-inverts the green channel before its Normal Map node, so the renders are right too.
"""
from __future__ import annotations

import json
import math
from pathlib import Path

import bmesh
import bpy
import numpy as np

ATLASES = {}      # atlas id -> dict(material=final name, res=int)
_GRAPHS = {}      # temp material name -> graph record (nodes used by the bake passes)
PASSES = ('BaseColor', 'Normal', 'Roughness', 'Metallic', 'AO')


def srgb(hexa, mul=1.0):
    c = [int(hexa[i:i + 2], 16) / 255 for i in (0, 2, 4)]
    lin = [x / 12.92 if x <= 0.04045 else ((x + 0.055) / 1.055) ** 2.4 for x in c]
    return tuple(min(1.0, v * mul) for v in lin)


def register_atlas(aid, material, res):
    ATLASES[aid] = dict(material=material, res=res)


# ------------------------------------------------------------------------------------------- node graph helpers

class Graph:
    def __init__(self, mat):
        mat.use_nodes = True
        self.t = mat.node_tree
        self.t.nodes.clear()
        self.out = self.t.nodes.new('ShaderNodeOutputMaterial')
        self.bsdf = self.t.nodes.new('ShaderNodeBsdfPrincipled')
        self.t.links.new(self.bsdf.outputs[0], self.out.inputs['Surface'])
        self.tc = self.t.nodes.new('ShaderNodeTexCoord')
        self.emit = self.t.nodes.new('ShaderNodeEmission')
        self.img = self.t.nodes.new('ShaderNodeTexImage')
        self.img.label = 'BAKE'

    def node(self, kind, **props):
        n = self.t.nodes.new(kind)
        for k, v in props.items():
            setattr(n, k, v)
        return n

    def put(self, socket, value):
        """Links a socket or sets a default (float, colour or vector sockets)."""
        if isinstance(value, bpy.types.NodeSocket):
            self.t.links.new(value, socket)
            return
        for link in list(socket.links):
            self.t.links.remove(link)
        dv = socket.default_value
        if hasattr(dv, '__len__'):
            n = len(dv)
            if isinstance(value, (tuple, list)):
                socket.default_value = (tuple(value) + (1.0,) * n)[:n]
            else:
                socket.default_value = ((float(value),) * 3 + (1.0,))[:n]
        else:
            socket.default_value = float(value)


def coords(g, scale=(1, 1, 1), rotation=(0, 0, 0)):
    m = g.node('ShaderNodeMapping', vector_type='POINT')
    g.t.links.new(g.tc.outputs['Object'], m.inputs['Vector'])
    m.inputs['Scale'].default_value = scale
    m.inputs['Rotation'].default_value = rotation
    return m.outputs['Vector']


def noise(g, vec, scale, detail=4.0, rough=0.5, distortion=0.0):
    n = g.node('ShaderNodeTexNoise')
    g.t.links.new(vec, n.inputs['Vector'])
    n.inputs['Scale'].default_value = scale
    n.inputs['Detail'].default_value = detail
    n.inputs['Roughness'].default_value = rough
    n.inputs['Distortion'].default_value = distortion
    return n.outputs['Fac']


def wave(g, vec, scale, distortion=0.0, detail=2.0, direction='Y', profile='SIN', kind='BANDS'):
    n = g.node('ShaderNodeTexWave', wave_type=kind, bands_direction=direction, wave_profile=profile)
    g.t.links.new(vec, n.inputs['Vector'])
    n.inputs['Scale'].default_value = scale
    n.inputs['Distortion'].default_value = distortion
    n.inputs['Detail'].default_value = detail
    return n.outputs['Fac']


def voronoi(g, vec, scale, smoothness=0.0, feature='F1'):
    n = g.node('ShaderNodeTexVoronoi', feature=feature)
    g.t.links.new(vec, n.inputs['Vector'])
    n.inputs['Scale'].default_value = scale
    if 'Smoothness' in n.inputs:
        n.inputs['Smoothness'].default_value = smoothness
    return n.outputs['Distance']


def ramp(g, fac, stops, interpolation='LINEAR'):
    n = g.node('ShaderNodeValToRGB')
    cr = n.color_ramp
    cr.interpolation = interpolation
    while len(cr.elements) > 1:
        cr.elements.remove(cr.elements[-1])
    cr.elements[0].position = stops[0][0]
    cr.elements[0].color = (*stops[0][1], 1.0)
    for pos, col in stops[1:]:
        e = cr.elements.new(pos)
        e.color = (*col, 1.0)
    g.t.links.new(fac, n.inputs[0])
    return n.outputs['Color']


def mix(g, fac, a, b):
    n = g.node('ShaderNodeMixRGB', blend_type='MIX')
    n.use_clamp = True
    g.put(n.inputs[0], fac)
    g.put(n.inputs[1], a)
    g.put(n.inputs[2], b)
    return n.outputs['Color']


def op(g, operation, a, b=0.0, clamp=True):
    n = g.node('ShaderNodeMath', operation=operation, use_clamp=clamp)
    g.put(n.inputs[0], a)
    g.put(n.inputs[1], b)
    return n.outputs[0]


def add(g, a, b):
    return op(g, 'ADD', a, b)


def sub(g, a, b):
    return op(g, 'SUBTRACT', a, b)


def mul(g, a, b):
    return op(g, 'MULTIPLY', a, b)


def maxi(g, a, b):
    return op(g, 'MAXIMUM', a, b)


def remap(g, v, lo, hi, to_lo=0.0, to_hi=1.0):
    n = g.node('ShaderNodeMapRange')
    n.clamp = True
    g.put(n.inputs[0], v)
    n.inputs[1].default_value = lo
    n.inputs[2].default_value = hi
    n.inputs[3].default_value = to_lo
    n.inputs[4].default_value = to_hi
    return n.outputs[0]


def pointiness(g, lo=0.52, hi=0.66):
    geo = g.node('ShaderNodeNewGeometry')
    return remap(g, geo.outputs['Pointiness'], lo, hi)


def occlusion(g, distance=0.03, samples=16):
    n = g.node('ShaderNodeAmbientOcclusion')
    n.samples = samples
    n.only_local = True
    n.inputs['Distance'].default_value = distance
    return n.outputs['AO']


def bump(g, height, strength=0.3, distance=0.0004):
    n = g.node('ShaderNodeBump')
    n.inputs['Strength'].default_value = strength
    n.inputs['Distance'].default_value = distance
    g.put(n.inputs['Height'], height)
    return n.outputs['Normal']


def axis_x(g):
    s = g.node('ShaderNodeSeparateXYZ')
    g.t.links.new(g.tc.outputs['Object'], s.inputs[0])
    return s.outputs['X']


# ------------------------------------------------------------------------------------------------------- looks

def look_wood(g, p):
    dark, base = srgb(p.get('dark', '24130A')), srgb(p.get('base', '5A3418'))
    light, polish = srgb(p.get('light', '8E5F33')), srgb(p.get('polish', '9C7040'))
    fine = noise(g, coords(g, (0.10, 1, 1)), p.get('grain_scale', 240), 5, 0.65)
    figure = wave(g, coords(g, (0.22, 1, 1)), p.get('figure_scale', 34), 3.2, 2.5, direction=p.get('figure_dir', 'Y'))
    grain = mix(g, 0.45, figure, fine)
    col = ramp(g, grain, [(0.0, dark), (0.40, base), (0.72, light), (1.0, tuple(min(1, v * 1.2) for v in light))])
    mottle = remap(g, noise(g, coords(g), 3.0, 2, 0.5), 0.35, 0.65, 0, 0.35)
    col = mix(g, mottle, col, dark)
    dings = remap(g, noise(g, coords(g), 9.0, 3, 0.6), 0.66, 0.74)
    col = mix(g, mul(g, dings, 0.6), col, dark)
    wear = pointiness(g, 0.52, 0.66)
    handling = mul(g, remap(g, noise(g, coords(g), 2.2, 2, 0.5), 0.45, 0.72), p.get('handling', 0.3))
    polished = maxi(g, wear, handling)
    col = mix(g, mul(g, polished, 0.6), col, polish)
    occ = occlusion(g, 0.02, 16)
    col = mix(g, mul(g, sub(g, 1.0, occ), 0.65), col, dark)
    rough = add(g, add(g, p.get('rough', 0.46), mul(g, fine, 0.18)), sub(g, mul(g, dings, 0.3), mul(g, polished, 0.22)))
    height = sub(g, add(g, mul(g, fine, 0.6), mul(g, figure, 0.4)), mul(g, dings, 2.0))
    return dict(color=col, rough=rough, metal=0.0, normal=bump(g, height, 0.8, 0.004), ao=occlusion(g, 0.06, 16))


def look_iron(g, p):
    base, bare, rust = srgb(p.get('base', '2B3137')), srgb(p.get('bare', '8E969B')), srgb(p.get('rust', '5C341B'))
    soot = srgb('0B0C0E')
    brushed = noise(g, coords(g, (0.02, 1, 1)), 420, 3, 0.6)
    mottle = noise(g, coords(g), 18, 4, 0.55)
    col = ramp(g, mottle, [(0.0, tuple(v * 0.75 for v in base)), (0.5, base), (1.0, tuple(min(1, v * 1.3) for v in base))])
    col = mix(g, mul(g, brushed, 0.22), col, tuple(v * 0.6 for v in bare))
    wear = maxi(g, pointiness(g, 0.53, 0.68), mul(g, remap(g, noise(g, coords(g), p.get('wear_scale', 14.0), 3, 0.5), 0.60, 0.76), p.get('wear', 0.7)))
    col = mix(g, wear, col, bare)
    occ = occlusion(g, 0.02, 16)
    rustm = mul(g, remap(g, noise(g, coords(g), 16, 5, 0.6), 0.72, 0.84), sub(g, 1.0, wear))
    rustm = maxi(g, rustm, mul(g, mul(g, sub(g, 1.0, occ), remap(g, noise(g, coords(g), 30, 3, 0.5), 0.5, 0.8)), p.get('rust_amount', 0.6)))
    col = mix(g, rustm, col, rust)
    col = mix(g, mul(g, sub(g, 1.0, occ), 0.55), col, soot)
    rough = add(g, add(g, p.get('rough', 0.36), mul(g, brushed, 0.12)), sub(g, mul(g, rustm, 0.45), mul(g, wear, 0.16)))
    pits = sub(g, 1.0, voronoi(g, coords(g), 260, 0.2))
    height = sub(g, add(g, mul(g, brushed, 0.6), mul(g, pits, 0.12)), mul(g, rustm, 0.6))
    metal = sub(g, 1.0, mul(g, rustm, 0.5))
    return dict(color=col, rough=rough, metal=metal, normal=bump(g, height, 0.5, 0.0012), ao=occlusion(g, 0.06, 16))


def look_darkiron(g, p):
    q = dict(base='15171A', bare='3A3F44', wear=0.3, rust_amount=0.3, rough=0.6)
    q.update(p)
    return look_iron(g, q)


def look_brass(g, p):
    base, polished = srgb(p.get('base', 'B58B42')), srgb(p.get('polished', 'E6C470'))
    patina, green = srgb('5E4B26'), srgb('4C5A3B')
    mottle = noise(g, coords(g), 12, 4, 0.55)
    col = ramp(g, mottle, [(0.0, tuple(v * 0.82 for v in base)), (0.5, base), (1.0, tuple(min(1, v * 1.15) for v in base))])
    wear = maxi(g, pointiness(g, 0.53, 0.66), mul(g, remap(g, noise(g, coords(g), 3.5, 3, 0.5), 0.55, 0.72), 0.9))
    col = mix(g, mul(g, wear, 0.85), col, polished)
    occ = occlusion(g, 0.02, 16)
    pat = maxi(g, mul(g, sub(g, 1.0, occ), 0.9), mul(g, remap(g, noise(g, coords(g), 22, 5, 0.6), 0.64, 0.80), 0.7))
    pat = mul(g, pat, sub(g, 1.0, wear))
    col = mix(g, pat, col, mix(g, 0.45, patina, green))
    scratch = noise(g, coords(g, (0.03, 1, 1)), 320, 3, 0.6)
    rough = add(g, add(g, 0.30, mul(g, scratch, 0.08)), sub(g, mul(g, pat, 0.38), mul(g, wear, 0.14)))
    dents = sub(g, 1.0, voronoi(g, coords(g), 140, 0.4))
    height = add(g, mul(g, scratch, 0.5), mul(g, dents, 0.3))
    return dict(color=col, rough=rough, metal=1.0, normal=bump(g, height, 0.5, 0.0015), ao=occlusion(g, 0.06, 16))


def look_leather(g, p):
    base, worn, dark = srgb(p.get('base', '4A2E1C')), srgb(p.get('worn', '7E5B3B')), srgb('25150D')
    grain = sub(g, 1.0, voronoi(g, coords(g), 420, 0.4))
    mottle = noise(g, coords(g), 9, 3, 0.55)
    col = ramp(g, mottle, [(0.0, dark), (0.5, base), (1.0, tuple(min(1, v * 1.25) for v in base))])
    wear = maxi(g, pointiness(g, 0.52, 0.65), mul(g, remap(g, noise(g, coords(g), 5, 3, 0.5), 0.6, 0.76), 0.5))
    col = mix(g, mul(g, wear, 0.7), col, worn)
    occ = occlusion(g, 0.015, 16)
    col = mix(g, mul(g, sub(g, 1.0, occ), 0.5), col, dark)
    rough = add(g, add(g, 0.60, mul(g, grain, 0.12)), mul(g, wear, -0.2))
    height = add(g, mul(g, grain, 0.7), mul(g, noise(g, coords(g), 60, 2, 0.5), 0.3))
    return dict(color=col, rough=rough, metal=0.0, normal=bump(g, height, 0.7, 0.002), ao=occlusion(g, 0.05, 16))


def look_paper(g, p):
    base, stain, dark = srgb(p.get('base', 'E6D8BC')), srgb(p.get('stain', 'B89A6B')), srgb(p.get('dark', 'C9B994'))
    fibers = noise(g, coords(g, (0.15, 1, 1)), 600, 2, 0.6)
    stains = remap(g, noise(g, coords(g), 7, 3, 0.5), 0.58, 0.82)
    col = mix(g, mul(g, fibers, 0.3), base, dark)
    col = mix(g, mul(g, stains, 0.7), col, stain)
    occ = occlusion(g, 0.01, 16)
    col = mix(g, mul(g, sub(g, 1.0, occ), 0.35), col, stain)
    rough = add(g, 0.82, mul(g, fibers, 0.12))
    return dict(color=col, rough=rough, metal=0.0, normal=bump(g, fibers, 0.5, 0.001), ao=occlusion(g, 0.03, 16))


def look_linen(g, p):
    q = dict(base='D8CCAA', stain='A8946C', dark='B9AD8C')
    q.update(p)
    return look_paper(g, q)


def look_horn(g, p):
    pale, mid, tip = srgb(p.get('pale', 'EADBB4')), srgb(p.get('mid', 'B89A68')), srgb(p.get('tip', '2E2217'))
    grad = remap(g, axis_x(g), p.get('x0', 0.0), p.get('x1', 0.15))
    streaks = noise(g, coords(g, (0.08, 1, 1)), 90, 3, 0.55)
    t = add(g, grad, mul(g, sub(g, streaks, 0.5), 0.5))
    col = ramp(g, t, [(0.0, pale), (0.45, mid), (0.78, tip), (1.0, tip)])
    occ = occlusion(g, 0.02, 16)
    col = mix(g, mul(g, sub(g, 1.0, occ), 0.3), col, tip)
    rough = add(g, 0.26, mul(g, streaks, 0.12))
    return dict(color=col, rough=rough, metal=0.0, normal=bump(g, streaks, 0.5, 0.0015), ao=occlusion(g, 0.05, 16))


def look_lead(g, p):
    base, dark = srgb(p.get('base', '70747A')), srgb(p.get('dark', '45484C'))
    mottle = noise(g, coords(g), 90, 4, 0.6)
    col = ramp(g, mottle, [(0.0, dark), (1.0, base)])
    occ = occlusion(g, 0.01, 16)
    col = mix(g, mul(g, sub(g, 1.0, occ), 0.4), col, dark)
    rough = add(g, 0.48, mul(g, mottle, 0.18))
    return dict(color=col, rough=rough, metal=0.92, normal=bump(g, noise(g, coords(g), 300, 3, 0.6), 0.35, 0.0005),
                ao=occlusion(g, 0.03, 16))


def look_flint(g, p):
    base, vein = srgb('3A343E'), srgb('8C878F')
    col = mix(g, remap(g, noise(g, coords(g), 70, 2, 0.5), 0.62, 0.76), base, vein)
    return dict(color=col, rough=0.3, metal=0.0, normal=bump(g, noise(g, coords(g), 200, 3, 0.6), 0.5, 0.001),
                ao=occlusion(g, 0.03, 16))


LOOKS = dict(wood=look_wood, iron=look_iron, darkiron=look_darkiron, brass=look_brass, leather=look_leather,
             paper=look_paper, linen=look_linen, horn=look_horn, lead=look_lead, flint=look_flint)


def temp_material(name, look, atlas, **params):
    """A temporary material carrying a procedural look, baked later into its atlas."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat['look'] = look
    mat['atlas'] = atlas
    mat['params'] = json.dumps(params)
    g = Graph(mat)
    out = LOOKS[look](g, params)
    g.put(g.bsdf.inputs['Base Color'], out['color'])
    g.put(g.bsdf.inputs['Roughness'], out['rough'])
    g.put(g.bsdf.inputs['Metallic'], out['metal'])
    g.put(g.bsdf.inputs['Normal'], out['normal'])
    _GRAPHS[mat.name] = dict(g=g, metal=out['metal'], ao=out['ao'], color=out['color'])
    return mat


# ------------------------------------------------------------------------------------------------------ baking

def _engine(scene, name):
    try:
        scene.render.engine = name
    except TypeError as e:
        raise SystemExit('render engine %s unavailable: %s' % (name, e))


def _select(objs):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in objs:
        o.hide_set(False)
        o.hide_viewport = False
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def unwrap(groups, margin):
    """groups: {object: set(material indices)} of one atlas -> smart projected and packed together."""
    objs = list(groups)
    _select(objs)
    ts = bpy.context.scene.tool_settings
    ts.use_uv_select_sync = True
    ts.mesh_select_mode = (False, False, True)
    bpy.ops.object.mode_set(mode='EDIT')
    for ob, indices in groups.items():
        bm = bmesh.from_edit_mesh(ob.data)
        bm.select_mode = {'FACE'}
        for v in bm.verts:
            v.select = False
        for e in bm.edges:
            e.select = False
        for f in bm.faces:
            f.select = False
        for f in bm.faces:
            if f.material_index in indices:
                f.select = True
        bm.select_flush_mode()
        bmesh.update_edit_mesh(ob.data)
    bpy.ops.uv.smart_project(angle_limit=math.radians(66.0), island_margin=0.001, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    try:
        bpy.ops.uv.pack_islands(udim_source='CLOSEST_UDIM', rotate=True, scale=True, merge_overlap=False,
                                margin_method='FRACTION', margin=margin, shape_method='CONCAVE')
    except TypeError:
        bpy.ops.uv.pack_islands(rotate=True, margin=margin)
    bpy.ops.object.mode_set(mode='OBJECT')


def new_image(name, res, color):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, res, res, alpha=False, float_buffer=False)
    img.colorspace_settings.name = 'sRGB' if color else 'Non-Color'
    return img


def _pixels(img):
    a = np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(a)
    return a.reshape(-1, 4)


def _save(img, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.filepath_raw = str(path)
    img.file_format = 'PNG'
    img.save()
    img.source = 'FILE'
    img.filepath = str(path)
    img.reload()


def _bake(objs, kind, samples):
    scene = bpy.context.scene
    _engine(scene, 'CYCLES')
    scene.cycles.device = 'CPU'
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    scene.cycles.use_adaptive_sampling = False
    bake = scene.render.bake
    bake.target = 'IMAGE_TEXTURES'
    bake.use_selected_to_active = False
    bake.margin = 16
    bake.margin_type = 'EXTEND'
    bake.use_clear = True
    bake.normal_space = 'TANGENT'
    bake.use_pass_direct = False
    bake.use_pass_indirect = False
    bake.use_pass_color = True
    _select(objs)
    bpy.ops.object.bake(type=kind)


def _prepare(mat, image, emit_key):
    rec = _GRAPHS[mat.name]
    g = rec['g']
    g.img.image = image
    g.t.nodes.active = g.img
    for link in list(g.out.inputs['Surface'].links):
        g.t.links.remove(link)
    if emit_key:
        g.put(g.emit.inputs['Color'], rec[emit_key])
        g.emit.inputs['Strength'].default_value = 1.0
        g.t.links.new(g.emit.outputs[0], g.out.inputs['Surface'])
    else:
        g.t.links.new(g.bsdf.outputs[0], g.out.inputs['Surface'])


def final_material(aid, paths):
    """The baked material of an atlas: BaseColor / Normal (DirectX) / ORM image nodes, labelled for the manifest."""
    name = ATLASES[aid]['material']
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    mat['atlas'] = aid
    mat['baked'] = True
    t = mat.node_tree
    t.nodes.clear()
    out = t.nodes.new('ShaderNodeOutputMaterial')
    bsdf = t.nodes.new('ShaderNodeBsdfPrincipled')
    t.links.new(bsdf.outputs[0], out.inputs['Surface'])
    imgs = {}
    for label in ('BaseColor', 'Normal', 'ORM'):
        n = t.nodes.new('ShaderNodeTexImage')
        n.label = label
        n.name = label
        n.image = paths[label]
        n.image.colorspace_settings.name = 'sRGB' if label == 'BaseColor' else 'Non-Color'
        imgs[label] = n
    t.links.new(imgs['BaseColor'].outputs['Color'], bsdf.inputs['Base Color'])
    sep = t.nodes.new('ShaderNodeSeparateColor')
    t.links.new(imgs['ORM'].outputs['Color'], sep.inputs['Color'])
    t.links.new(sep.outputs['Green'], bsdf.inputs['Roughness'])
    t.links.new(sep.outputs['Blue'], bsdf.inputs['Metallic'])
    # the file is DirectX (green down): Blender wants OpenGL for its preview
    sn = t.nodes.new('ShaderNodeSeparateColor')
    t.links.new(imgs['Normal'].outputs['Color'], sn.inputs['Color'])
    inv = t.nodes.new('ShaderNodeMath')
    inv.operation = 'SUBTRACT'
    inv.inputs[0].default_value = 1.0
    t.links.new(sn.outputs['Green'], inv.inputs[1])
    comb = t.nodes.new('ShaderNodeCombineColor')
    t.links.new(sn.outputs['Red'], comb.inputs['Red'])
    t.links.new(inv.outputs[0], comb.inputs['Green'])
    t.links.new(sn.outputs['Blue'], comb.inputs['Blue'])
    nm = t.nodes.new('ShaderNodeNormalMap')
    nm.space = 'TANGENT'
    t.links.new(comb.outputs['Color'], nm.inputs['Color'])
    t.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    # viewport colour: the average of the base colour
    return mat


def _remap_materials(ob, finals):
    """Replaces the temporary materials of ob by the final material of their atlas (slots merged)."""
    me = ob.data
    old = list(me.materials)
    new = []
    index = {}
    for i, m in enumerate(old):
        target = finals.get(m.get('atlas')) if m else None
        target = target or m
        if target not in new:
            new.append(target)
        index[i] = new.index(target)
    # clear() resets every polygon's material_index: keep them first
    polys = np.empty(len(me.polygons), dtype=np.int32)
    me.polygons.foreach_get('material_index', polys)
    me.materials.clear()
    for m in new:
        me.materials.append(m)
    remapped = np.array([index.get(int(i), 0) for i in polys], dtype=np.int32)
    me.polygons.foreach_set('material_index', remapped)
    me.update()


def bake_all(objects, out_dir, margin=0.006, log=print):
    """Unwraps, bakes and finalises every atlas used by objects; returns {atlas: {label: path}}."""
    out_dir = Path(out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    groups = {}
    mats = {}
    for ob in objects:
        for i, slot in enumerate(ob.material_slots):
            m = slot.material
            if m and m.get('atlas') and not m.get('baked'):
                groups.setdefault(m['atlas'], {}).setdefault(ob, set()).add(i)
                mats.setdefault(m['atlas'], [])
                if m not in mats[m['atlas']]:
                    mats[m['atlas']].append(m)
    saved_engine = bpy.context.scene.render.engine
    for aid, g in groups.items():
        log('[looks] unwrap %s: %d objects' % (aid, len(g)))
        unwrap(g, margin)
    images = {aid: {p: new_image('BAKE_%s_%s' % (aid, p), ATLASES[aid]['res'], p == 'BaseColor') for p in PASSES} for aid in groups}
    all_mats = [m for ms in mats.values() for m in ms]
    passes = [('BaseColor', 'EMIT', 1, 'color'), ('Normal', 'NORMAL', 1, None), ('Roughness', 'ROUGHNESS', 1, None),
              ('Metallic', 'EMIT', 1, 'metal'), ('AO', 'EMIT', 4, 'ao')]
    for key, kind, samples, emit in passes:
        for aid, ms in mats.items():
            for m in ms:
                _prepare(m, images[aid][key], emit)
        log('[looks] bake %s' % key)
        _bake(list(objects), kind, samples)
    for m in all_mats:
        _prepare(m, None, None)
    result = {}
    finals = {}
    for aid in groups:
        im = images[aid]
        n = _pixels(im['Normal'])
        n[:, 1] = 1.0 - n[:, 1]
        im['Normal'].pixels.foreach_set(n.ravel())
        orm = np.ones_like(n)
        orm[:, 0] = _pixels(im['AO'])[:, 0]
        orm[:, 1] = _pixels(im['Roughness'])[:, 0]
        orm[:, 2] = _pixels(im['Metallic'])[:, 0]
        im['ORM'] = new_image('BAKE_%s_ORM' % aid, ATLASES[aid]['res'], False)
        im['ORM'].pixels.foreach_set(orm.ravel())
        paths = {}
        for label, suffix in (('BaseColor', 'D'), ('Normal', 'N'), ('ORM', 'ORM')):
            path = out_dir / ('T_%s_%s.png' % (aid, suffix))
            _save(im[label], path)
            im[label].name = 'T_%s_%s' % (aid, suffix)
            paths[label] = im[label]
        for key in ('Roughness', 'Metallic', 'AO'):
            bpy.data.images.remove(im[key])
        finals[aid] = final_material(aid, paths)
        result[aid] = {k: str(out_dir / ('T_%s_%s.png' % (aid, s))) for k, s in (('BaseColor', 'D'), ('Normal', 'N'), ('ORM', 'ORM'))}
    for ob in objects:
        _remap_materials(ob, finals)
    for m in all_mats:
        if m.users == 0:
            bpy.data.materials.remove(m)
    try:
        bpy.context.scene.render.engine = saved_engine
    except TypeError:
        pass
    return result
