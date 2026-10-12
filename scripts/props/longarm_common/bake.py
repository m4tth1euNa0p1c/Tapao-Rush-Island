"""UV unwrap and Cycles bake of the long arm meshes (T-250): BaseColor (sRGB), Normal (tangent space, DirectX: green
down) and ORM (AO / roughness / metallic, linear) at 2048 for the weapons, 1024 for the ammunition, written to
<pack>/textures/T_<Name>_{D,N,ORM}.png. After the bake the mesh keeps ONE material M_<Name> whose Image Texture nodes
are labelled BaseColor / Normal / ORM (export.py lists them in manifest.materials[].textures by label).

The procedural materials (materials.py) are baked one channel at a time: NORMAL and ROUGHNESS are Cycles bake passes,
AO is the AO pass (world distance set to the size of the crevices), colour and metallic go through a temporary emission
shader (Cycles has no albedo pass for metals). GPU (OptiX / CUDA) when Blender sees one, CPU otherwise.
"""
from __future__ import annotations

import math
from pathlib import Path

import bpy
import numpy as np

import materials as MATS


def gpu():
    try:
        prefs = bpy.context.preferences.addons['cycles'].preferences
        for kind in ('OPTIX', 'CUDA', 'HIP', 'ONEAPI'):
            try:
                prefs.compute_device_type = kind
            except TypeError:
                continue
            prefs.get_devices()
            found = False
            for d in prefs.devices:
                d.use = d.type != 'CPU' or kind == 'CPU'
                found |= d.type != 'CPU'
            if found:
                return kind
    except Exception as error:  # noqa: BLE001
        print('[bake] no GPU:', error, flush=True)
    return None


def select_only(ob):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    ob.hide_set(False)
    ob.hide_viewport = False
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


def unwrap(ob, margin=0.004):
    """Smart UV project + pack of the whole mesh (the box UVs of kit.join_parts overlap everywhere)."""
    select_only(ob)
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.smart_project(angle_limit=math.radians(66), island_margin=margin, area_weight=0.0, correct_aspect=True,
                             scale_to_bounds=False)
    try:
        bpy.ops.uv.pack_islands(rotate=True, margin_method='FRACTION', margin=margin)
    except TypeError:
        bpy.ops.uv.pack_islands(margin=margin)
    bpy.ops.object.mode_set(mode='OBJECT')


def new_image(name, size, srgb):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, size, size, alpha=False, float_buffer=False)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    return img


def _bake_nodes(ob, image):
    """One active Image Texture node per material slot pointing at the target image."""
    for slot in ob.material_slots:
        mat = slot.material
        nt = mat.node_tree
        node = nt.nodes.get('_bake') or nt.nodes.new('ShaderNodeTexImage')
        node.name = '_bake'
        node.label = '_bake'
        node.image = image
        node.location = (-300, 600)
        node.select = True
        nt.nodes.active = node


def _emission(ob, channel):
    """Route the registered channel socket of every material to a temporary emission shader; returns the undo list."""
    undo = []
    for slot in ob.material_slots:
        mat = slot.material
        nt = mat.node_tree
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL')
        previous = out.inputs['Surface'].links[0].from_socket if out.inputs['Surface'].links else None
        em = nt.nodes.new('ShaderNodeEmission')
        em.name = '_bake_emission'
        src = MATS.SOCKETS.get(mat.name, {}).get(channel)
        if src is None:
            bsdf = next((n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED'), None)
            key = {'color': 'Base Color', 'rough': 'Roughness', 'metal': 'Metallic'}[channel]
            src = bsdf.inputs[key].default_value if bsdf else 0.0
        if isinstance(src, (int, float)):
            em.inputs['Color'].default_value = (src, src, src, 1)
        elif hasattr(src, '__len__') and not hasattr(src, 'node'):
            v = list(src)
            em.inputs['Color'].default_value = (v[0], v[1], v[2], 1)
        else:
            nt.links.new(src, em.inputs['Color'])
        em.inputs['Strength'].default_value = 1.0
        nt.links.new(em.outputs['Emission'], out.inputs['Surface'])
        undo.append((nt, out, previous, em))
    return undo


def _restore(undo):
    for nt, out, previous, em in undo:
        nt.nodes.remove(em)
        if previous is not None:
            nt.links.new(previous, out.inputs['Surface'])


def bake_channel(scene, ob, image, kind, samples, ao_distance=0.04):
    scene.render.engine = 'CYCLES'
    scene.cycles.samples = samples
    scene.cycles.use_denoising = False
    scene.cycles.use_adaptive_sampling = False
    bk = scene.render.bake
    bk.margin = 10
    bk.use_clear = True
    bk.use_selected_to_active = False
    bk.use_pass_direct = False
    bk.use_pass_indirect = False
    bk.use_pass_color = True
    bk.target = 'IMAGE_TEXTURES'
    select_only(ob)
    _bake_nodes(ob, image)
    undo = []
    if kind == 'NORMAL':
        bk.normal_space = 'TANGENT'
        bk.normal_r, bk.normal_g, bk.normal_b = 'POS_X', 'NEG_Y', 'POS_Z'
        bpy.ops.object.bake(type='NORMAL')
    elif kind == 'AO':
        if scene.world is None:
            scene.world = bpy.data.worlds.new('World-bake')
        scene.world.light_settings.distance = ao_distance
        bpy.ops.object.bake(type='AO')
    elif kind == 'ROUGHNESS':
        bpy.ops.object.bake(type='ROUGHNESS')
    else:
        undo = _emission(ob, kind)
        try:
            bpy.ops.object.bake(type='EMIT')
        finally:
            _restore(undo)


def pack_orm(ao, rough, metal, out):
    n = out.size[0] * out.size[1] * 4
    buf = {}
    for key, img in (('ao', ao), ('r', rough), ('m', metal)):
        a = np.empty(n, dtype=np.float32)
        img.pixels.foreach_get(a)
        buf[key] = a
    orm = np.ones(n, dtype=np.float32)
    orm[0::4] = buf['ao'][0::4]
    orm[1::4] = buf['r'][0::4]
    orm[2::4] = buf['m'][0::4]
    out.pixels.foreach_set(orm)


def save(image, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    image.filepath_raw = str(path)
    image.file_format = 'PNG'
    image.save()
    image.filepath = str(path)
    image.source = 'FILE'
    image.reload()
    return path


def textured_material(name, textures):
    """The export material: Image Texture nodes labelled BaseColor / Normal / ORM wired to a Principled BSDF."""
    mat = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    out.location = (600, 0)
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    bsdf.location = (300, 0)
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    base = nt.nodes.new('ShaderNodeTexImage')
    base.label = base.name = 'BaseColor'
    base.image = textures['BaseColor']
    base.location = (-300, 300)
    nt.links.new(base.outputs['Color'], bsdf.inputs['Base Color'])
    orm = nt.nodes.new('ShaderNodeTexImage')
    orm.label = orm.name = 'ORM'
    orm.image = textures['ORM']
    orm.location = (-300, 0)
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    sep.location = (0, 0)
    nt.links.new(orm.outputs['Color'], sep.inputs['Color'])
    nt.links.new(sep.outputs['Green'], bsdf.inputs['Roughness'])
    nt.links.new(sep.outputs['Blue'], bsdf.inputs['Metallic'])
    nrm = nt.nodes.new('ShaderNodeTexImage')
    nrm.label = nrm.name = 'Normal'
    nrm.image = textures['Normal']
    nrm.location = (-300, -300)
    nmap = nt.nodes.new('ShaderNodeNormalMap')
    nmap.location = (0, -300)
    nmap.space = 'TANGENT'
    nt.links.new(nrm.outputs['Color'], nmap.inputs['Color'])
    nt.links.new(nmap.outputs['Normal'], bsdf.inputs['Normal'])
    mat['textures_note'] = 'Normal is DirectX (green down); ORM = AO / roughness / metallic, linear'
    mat.diffuse_color = (0.3, 0.2, 0.1, 1)
    return mat


def bake_object(scene, ob, name, folder, size=2048, color_samples=8, ao_samples=48, ao_distance=0.04):
    """Unwraps ob, bakes its procedural materials to T_<name>_{D,N,ORM}.png in folder, replaces every material slot
    by the single textured material M_<name>. Returns {'label': Path}."""
    folder = Path(folder)
    unwrap(ob)
    device = gpu()
    scene.cycles.device = 'GPU' if device else 'CPU'
    print(f'[bake] {name}: {size} px, {len(ob.data.polygons)} faces, {device or "CPU"}', flush=True)
    color = new_image(f'T_{name}_D', size, True)
    bake_channel(scene, ob, color, 'color', color_samples)
    normal = new_image(f'T_{name}_N', size, False)
    bake_channel(scene, ob, normal, 'NORMAL', color_samples)
    rough = new_image(f'T_{name}_R', size, False)
    bake_channel(scene, ob, rough, 'ROUGHNESS', color_samples)
    metal = new_image(f'T_{name}_M', size, False)
    bake_channel(scene, ob, metal, 'metal', color_samples)
    ao = new_image(f'T_{name}_AO', size, False)
    bake_channel(scene, ob, ao, 'AO', ao_samples, ao_distance)
    orm = new_image(f'T_{name}_ORM', size, False)
    pack_orm(ao, rough, metal, orm)
    paths = {'BaseColor': save(color, folder / f'T_{name}_D.png'), 'Normal': save(normal, folder / f'T_{name}_N.png'),
             'ORM': save(orm, folder / f'T_{name}_ORM.png')}
    for img in (rough, metal, ao):
        bpy.data.images.remove(img)
    for slot in ob.material_slots:
        node = slot.material.node_tree.nodes.get('_bake')
        if node:
            slot.material.node_tree.nodes.remove(node)
    mat = textured_material(f'M_{name}', {'BaseColor': color, 'Normal': normal, 'ORM': orm})
    me = ob.data
    me.materials.clear()
    me.materials.append(mat)
    for poly in me.polygons:
        poly.material_index = 0
    return paths
