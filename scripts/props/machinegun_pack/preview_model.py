"""Quick look of the machine gun model (T-272) with its procedural materials, no bake: a few EEVEE stills into
build/packs/bean-machinegun/renders/look-*.png (or --out DIR).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/machinegun_pack/preview_model.py -- [--cycles] [--views three,left,right,top,crank,front]
"""
from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
for p in (REPO / 'scripts/props/musket_pack', REPO / 'scripts/props/longarm_common', HERE):
    if str(p) in sys.path:
        sys.path.remove(str(p))
    sys.path.insert(0, str(p))

import machinegun_model as MG  # noqa: E402

VIEWS = {
    # name: (camera location, target, lens)
    'three': ((0.95, 1.05, 0.55), (0.33, 0.0, -0.04), 45),
    'left': ((0.35, 1.45, 0.05), (0.35, 0.0, -0.05), 50),
    'right': ((0.30, -1.45, 0.08), (0.30, 0.0, -0.05), 50),
    'top': ((0.33, 0.0, 1.5), (0.33, 0.0, 0.0), 50),
    'crank': ((0.10, -0.55, 0.20), (-0.07, -0.08, -0.01), 50),
    'front': ((1.6, 0.25, 0.10), (0.6, 0.0, -0.02), 60),
    'feed': ((0.20, 0.45, 0.22), (0.05, 0.05, -0.02), 50),
    'rear': ((-0.75, -0.35, 0.30), (-0.1, 0.0, -0.03), 50),
}


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument('--cycles', action='store_true')
    ap.add_argument('--views', default='three,left,right,crank')
    ap.add_argument('--out', default=str(REPO / 'build/packs/bean-machinegun/renders'))
    a = ap.parse_args(argv)
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    scene = bpy.context.scene
    coll = bpy.data.collections.new('COL-MG')
    scene.collection.children.link(coll)
    res = MG.build_machinegun(coll)
    for c in res['cutters']:
        c.hide_render = True
    tris = 0
    dg = bpy.context.evaluated_depsgraph_get()
    for objs in res['parts'].values():
        for ob in objs:
            me = ob.evaluated_get(dg).to_mesh()
            tris += sum(len(p.vertices) - 2 for p in me.polygons)
            ob.evaluated_get(dg).to_mesh_clear()
    print('[mg look] parts', sum(len(v) for v in res['parts'].values()), 'triangles', tris, flush=True)
    if a.cycles:
        scene.render.engine = 'CYCLES'
        scene.cycles.samples = 64
        scene.cycles.use_denoising = True
    else:
        try:
            scene.render.engine = 'BLENDER_EEVEE'
        except TypeError:
            scene.render.engine = 'BLENDER_EEVEE_NEXT'
        try:
            scene.eevee.taa_render_samples = 32
        except AttributeError:
            pass
    scene.render.resolution_x, scene.render.resolution_y = 1600, 900
    try:
        scene.view_settings.view_transform = 'AgX'
        scene.view_settings.look = 'AgX - Medium High Contrast'
    except TypeError:
        pass
    world = bpy.data.worlds.new('W')
    scene.world = world
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs['Color'].default_value = (0.42, 0.45, 0.50, 1)
    bg.inputs['Strength'].default_value = 0.55
    me = bpy.data.meshes.new('REF-Floor')
    me.from_pydata([(-3, -3, -0.207), (3, -3, -0.207), (3, 3, -0.207), (-3, 3, -0.207)], [], [(0, 1, 2, 3)])
    floor = bpy.data.objects.new('REF-Floor', me)
    scene.collection.objects.link(floor)
    m = bpy.data.materials.new('REF-FloorMat')
    m.use_nodes = True
    next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED').inputs['Base Color'].default_value = (0.09, 0.075, 0.06, 1)
    floor.data.materials.append(m)
    for name, loc, power, size in (('Key', (0.9, 0.8, 1.2), 70, 1.4), ('Fill', (-0.6, -1.0, 0.7), 30, 2.0), ('Rim', (-0.8, 0.9, 0.9), 45, 0.9)):
        d = bpy.data.lights.new('LGT-' + name, 'AREA')
        d.energy = power
        d.size = size
        o = bpy.data.objects.new(d.name, d)
        scene.collection.objects.link(o)
        o.location = loc
        o.rotation_euler = (Vector((0.3, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    cd = bpy.data.cameras.new('CAM')
    cam = bpy.data.objects.new('CAM', cd)
    scene.collection.objects.link(cam)
    scene.camera = cam
    cd.clip_start = 0.01
    out = Path(a.out)
    out.mkdir(parents=True, exist_ok=True)
    for v in a.views.split(','):
        loc, tgt, lens = VIEWS[v]
        cam.location = loc
        cam.rotation_euler = (Vector(tgt) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        cd.lens = lens
        scene.render.filepath = str(out / f'look-{v}.png')
        bpy.ops.render.render(write_still=True)
        print('[mg look] rendered', v, flush=True)


main()
