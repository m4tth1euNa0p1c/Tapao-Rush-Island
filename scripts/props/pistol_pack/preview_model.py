"""Model look-dev of the pistol pack: builds one variant, bakes its atlas, renders product views (no animation).

  blender -b --factory-startup --python scripts/props/pistol_pack/preview_model.py -- --variant single --size 1024
    --no-bake      keep the procedural materials (Cycles renders them directly)
    --ammo         also build and render the ammunition props
    --out DIR      pack folder (default build/packs/<pack>)
"""
from __future__ import annotations

import argparse
import json
import math
import sys
import time
from pathlib import Path

import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import materials as MT  # noqa: E402
import meshkit as K  # noqa: E402
import pistol_model as PM  # noqa: E402

SPEC = json.loads((HERE / 'pack.json').read_text(encoding='utf8'))


def studio(scene, target, distance, ortho=.5):
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1
    world = bpy.data.worlds.new('WORLD-Pistol-Preview')
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs['Color'].default_value = (.16, .19, .23, 1)
    bg.inputs['Strength'].default_value = .55
    scene.world = world
    coll = bpy.data.collections.new('COL-Preview-Stage')
    scene.collection.children.link(coll)
    for label, loc, energy, size, color in [('Key', (.35, -.55, .75), 55, .6, (1, .9, .78)), ('Fill', (-.55, -.35, .35), 22, .8, (.7, .8, 1)),
                                            ('Rim', (.15, .6, .6), 55, .5, (1, .95, .85)), ('Top', (.05, 0, 1.0), 16, 1.0, (1, 1, 1))]:
        light = bpy.data.lights.new('LGT-' + label, 'AREA')
        light.energy = energy
        light.shape = 'DISK'
        light.size = size
        light.color = color
        ob = bpy.data.objects.new(light.name, light)
        coll.objects.link(ob)
        ob.location = loc
        ob.rotation_euler = (Vector(target) - ob.location).to_track_quat('-Z', 'Y').to_euler()
    floor = bpy.data.materials.new('MAT-Preview-Floor')
    floor.use_nodes = True
    p = K.principled(floor)
    p.inputs['Base Color'].default_value = (.035, .04, .05, 1)
    p.inputs['Roughness'].default_value = .7
    me = bpy.data.meshes.new('REF-PreviewFloor')
    me.from_pydata([(-3, -3, 0), (3, -3, 0), (3, 3, 0), (-3, 3, 0)], [], [(0, 1, 2, 3)])
    me.materials.append(floor)
    ob = bpy.data.objects.new('REF-PreviewFloor', me)
    coll.objects.link(ob)
    data = bpy.data.cameras.new('CAM-Preview')
    cam = bpy.data.objects.new(data.name, data)
    coll.objects.link(cam)
    data.type = 'ORTHO'
    data.ortho_scale = ortho
    scene.camera = cam
    return cam


def shot(scene, cam, target, direction, distance, path, ortho=.5, res=(1600, 1000), samples=96):
    cam.location = Vector(target) + Vector(direction).normalized() * distance
    cam.rotation_euler = (Vector(target) - cam.location).to_track_quat('-Z', 'Y').to_euler()
    cam.data.ortho_scale = ortho
    scene.render.resolution_x, scene.render.resolution_y = res
    scene.render.resolution_percentage = 100
    scene.cycles.samples = samples
    scene.cycles.use_denoising = True
    scene.render.image_settings.file_format = 'PNG'
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    scene.render.filepath = str(path)
    t0 = time.time()
    bpy.ops.render.render(write_still=True)
    print('[preview] %s (%.1fs)' % (path, time.time() - t0), flush=True)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--variant', default='single')
    p.add_argument('--size', type=int, default=1024)
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--ammo', action='store_true')
    p.add_argument('--samples', type=int, default=96)
    p.add_argument('--out', default='')
    args = p.parse_args(argv)
    variant = SPEC['variants'][args.variant]
    out = Path(args.out) if args.out else REPO / 'build' / 'packs' / variant['pack']
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    scene = bpy.context.scene
    scene.name = 'SCN-Pistol-Preview'
    coll = bpy.data.collections.new('COL-' + variant['weapon'])
    scene.collection.children.link(coll)
    mats = MT.create_materials('Pistol')
    t0 = time.time()
    weapon = PM.create_weapon(coll, mats, variant['weapon'], args.variant)
    PM.finalize(weapon, args.size)
    c = weapon['contract']
    print('[preview] built %s: %d tris (charge %d), bounds %s, texel density %s px/m, %.1fs' % (
        args.variant, c['weapon_triangle_count'], c['charge_triangle_count'], [round(v, 4) for v in c['bounds']['size']],
        c['uv_texel_density_px_per_m'], time.time() - t0), flush=True)
    bpy.context.view_layer.update()
    target = Vector((.09, 0, .02))
    cam = studio(scene, target, .8)
    MT.cycles_gpu(scene)
    if not args.no_bake:
        paths = MT.bake_atlas(scene, weapon['meshes'], args.size, out / 'textures', variant['texture'],
                              SPEC['bake']['color_samples'], SPEC['bake']['ao_samples'], SPEC['bake']['margin'])
        mat = MT.baked_material(variant['material'], paths)
        MT.apply_baked(weapon['meshes'], mat)
    # the pistol lying on the floor for the shots: lowest point on z = 0.005
    bb = K.bounds(weapon['weapon_meshes'])
    weapon['armature'].location = (0, 0, -bb['min'][2] + .005)
    bpy.context.view_layer.update()
    target = Vector((.09, 0, -bb['min'][2] + .005 + .03))
    renders = out / 'renders'
    shot(scene, cam, target, (.55, -.75, .45), .9, renders / ('product-%s-right.png' % args.variant), .46, samples=args.samples)
    shot(scene, cam, target, (-.35, .8, .4), .9, renders / ('product-%s-left.png' % args.variant), .46, samples=args.samples)
    shot(scene, cam, target, (.02, -1, .02), .9, renders / ('product-%s-side.png' % args.variant), .42, samples=args.samples)
    shot(scene, cam, Vector((.06, -.02, target.z + .04)), (.4, -.9, 1.1), .6, renders / ('product-%s-lock.png' % args.variant), .17, samples=args.samples)
    if args.ammo:
        acoll = bpy.data.collections.new('COL-Ammo')
        scene.collection.children.link(acoll)
        ammo = PM.create_ammo(acoll, mats, SPEC['bake']['ammo_size'])
        objs = [v[0] for v in ammo.values()]
        ammo['SM_Ammo_PistolCartridge'][0].location = (-.06, -.12, .0)
        ammo['SM_Ammo_BallPouch'][0].location = (.06, -.14, .0)
        for v in ammo.values():
            v[1][0].hide_render = True
        if not args.no_bake:
            paths = MT.bake_atlas(scene, objs, SPEC['bake']['ammo_size'], out / 'textures', 'T_Ammo_Pistol',
                                  SPEC['bake']['color_samples'], SPEC['bake']['ao_samples'], 8)
            MT.apply_baked(objs, MT.baked_material('M_Ammo_Pistol', paths))
        shot(scene, cam, Vector((.0, -.13, .03)), (.3, -.8, .6), .8, renders / 'product-ammo.png', .26, samples=args.samples)
    bpy.ops.wm.save_as_mainfile(filepath=str(out / 'validation' / ('preview-%s.blend' % args.variant)), compress=True)
    print('[preview] done', flush=True)


if __name__ == '__main__':
    main()
