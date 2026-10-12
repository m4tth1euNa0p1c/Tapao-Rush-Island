"""Cycles bake of the T-334 weapon groups (after the machine gun pack's bake_group): the procedural materials of a
group (pa_materials.py) baked to T_<Name>_D.png (sRGB), _N.png (tangent, DirectX: green down) and _ORM.png (linear:
R occlusion, G roughness, B metallic) with scripts/props/longarm_common/bake.py, then the group gets ONE material
M_<Name> whose Image Texture nodes are labelled BaseColor / Normal / ORM (listed by label in manifest.materials).
GPU (OptiX / CUDA) when Blender sees one.
"""
from __future__ import annotations

from pathlib import Path

import bpy

import bake as LB  # scripts/props/longarm_common/bake.py

gpu = LB.gpu


def _select_only(ob):
    """bake.select_only, tolerant of the stale (None) entries the view layer keeps right after kit.join_parts deleted
    the parts."""
    vl = bpy.context.view_layer
    vl.update()
    for o in list(vl.objects):
        if o is not None:
            o.select_set(False)
    ob.hide_set(False)
    ob.hide_viewport = False
    ob.select_set(True)
    vl.objects.active = ob


LB.select_only = _select_only


def bake_group(scene, ob, name, folder, size, color_samples=8, ao_samples=48, ao_distance=0.03, margin=None):
    folder = Path(folder)
    LB.unwrap(ob, margin=margin if margin is not None else (0.003 if size >= 2048 else 0.005))
    device = LB.gpu()
    scene.cycles.device = 'GPU' if device else 'CPU'
    print(f'[bake] {name}: {size} px, {len(ob.data.polygons)} faces, {device or "CPU"}', flush=True)
    color = LB.new_image(f'T_{name}_D', size, True)
    LB.bake_channel(scene, ob, color, 'color', color_samples)
    normal = LB.new_image(f'T_{name}_N', size, False)
    LB.bake_channel(scene, ob, normal, 'NORMAL', color_samples)
    rough = LB.new_image(f'T_{name}_R', size, False)
    LB.bake_channel(scene, ob, rough, 'ROUGHNESS', color_samples)
    metal = LB.new_image(f'T_{name}_M', size, False)
    LB.bake_channel(scene, ob, metal, 'metal', color_samples)
    ao = LB.new_image(f'T_{name}_AO', size, False)
    LB.bake_channel(scene, ob, ao, 'AO', ao_samples, ao_distance)
    orm = LB.new_image(f'T_{name}_ORM', size, False)
    LB.pack_orm(ao, rough, metal, orm)
    paths = {'BaseColor': LB.save(color, folder / f'T_{name}_D.png'), 'Normal': LB.save(normal, folder / f'T_{name}_N.png'),
             'ORM': LB.save(orm, folder / f'T_{name}_ORM.png')}
    for img in (rough, metal, ao):
        bpy.data.images.remove(img)
    for slot in ob.material_slots:
        node = slot.material.node_tree.nodes.get('_bake')
        if node:
            slot.material.node_tree.nodes.remove(node)
    mat = LB.textured_material(f'M_{name}', {'BaseColor': color, 'Normal': normal, 'ORM': orm})
    _preview_normal_fix(mat)
    me = ob.data
    me.materials.clear()
    me.materials.append(mat)
    for poly in me.polygons:
        poly.material_index = 0
    return paths


def _preview_normal_fix(mat):
    """The file is DirectX (green down) for Unreal; the Blender preview wants OpenGL: invert the green channel before
    the Normal Map node (the musket pack's looks.final_material does the same)."""
    nt = mat.node_tree
    nrm = nt.nodes.get('Normal')
    nmap = next((n for n in nt.nodes if n.type == 'NORMAL_MAP'), None)
    if not nrm or not nmap:
        return
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    nt.links.new(nrm.outputs['Color'], sep.inputs['Color'])
    inv = nt.nodes.new('ShaderNodeMath')
    inv.operation = 'SUBTRACT'
    inv.inputs[0].default_value = 1.0
    nt.links.new(sep.outputs['Green'], inv.inputs[1])
    comb = nt.nodes.new('ShaderNodeCombineColor')
    nt.links.new(sep.outputs['Red'], comb.inputs['Red'])
    nt.links.new(inv.outputs[0], comb.inputs['Green'])
    nt.links.new(sep.outputs['Blue'], comb.inputs['Blue'])
    nt.links.new(comb.outputs['Color'], nmap.inputs['Color'])


def flat_group(ob):
    """--no-bake: keep the procedural materials, only give the group a unique UV layout."""
    LB.unwrap(ob, margin=0.004)
    return {}
