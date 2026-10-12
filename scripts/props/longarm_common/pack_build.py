"""Generic build of a long arm pack (T-250): weapon model + PBR bake + bean clips (TP / FP, 24 fps) + FBX + manifest +
renders, driven by scripts/props/<weapon>_pack/build.py:

  blender -b --factory-startup --python scripts/props/blunderbuss_pack/build.py -- [options]
    --clips Idle,GroundReload   only these clips (quick look; the full pack has 27 clips per view)
    --views FP                  only these views (TP,FP)
    --no-export                 no FBX / manifest (look only)
    --no-bake                   keep the procedural materials (no textures; quick looks)
    --render FP:GroundReload:30,TP:Fire:8   frames to render into <out>/renders/ (view:clip:frame)
    --product                   product renders (weapon + ammunition, lock close-up)
    --save                      also save <out>/<pack>.blend
    --out DIR                   pack folder (default build/packs/<pack>)

Output: <out>/<manifest>.json, exports/{Bases,Animations,Static}, textures/, renders/, fp-framing.json,
validation/grips/*.json. Same manifest format as the musket pack (bean-musket-anim/1): scripts/character/
import_pirate_weapons.py reads it with one more entry in its WEAPONS table (see docs/features/armes-longues.md).
"""
from __future__ import annotations

import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
if str(HERE) not in sys.path:
    sys.path.insert(0, str(HERE))
from musket_ref import A, K, MB, MX, REPO  # noqa: E402
import bake as BAKE  # noqa: E402
import choreo  # noqa: E402
import export as EXPORT  # noqa: E402
import materials as MATS  # noqa: E402

STATE = {}


def assemble_weapon(profile, builder, coll, params, out, do_bake):
    res = builder(coll, params)
    res['physics'] = K.part_physics([o for objs in res['parts'].values() for o in objs])
    arm = K.make_skeleton(f'SKEL-{profile.name}-Template', res['bones'], res['sockets'], coll)
    mesh = K.join_parts(f'SK_Bean_Pirate{profile.name}', res['parts'], coll, bone_names=list(res['bones']))
    res['textures'] = {}
    if do_bake:
        res['textures'] = BAKE.bake_object(bpy.context.scene, mesh, f'Bean_Pirate{profile.name}', Path(out) / 'textures', size=2048)
    mesh.parent = arm
    mod = mesh.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    res.update(object=mesh, armature=arm)
    return res


def assemble_ammo(builder, coll, out, do_bake):
    items = builder(coll)
    for item in items:
        ob = K.join_parts(item['name'], {'root': item['parts']}, coll)
        item['object'] = ob
        item['textures'] = {}
        if do_bake:
            item['textures'] = BAKE.bake_object(bpy.context.scene, ob, item['name'][3:], Path(out) / 'textures', size=item.get('bake', 1024),
                                                ao_distance=0.02)
    return items


def hand_error_frames(scene, arm, targets, worst=3):
    """The frames where the baked mittens are farthest from their targets (third person reach limits)."""
    errors = []
    for t in targets:
        scene.frame_set(t['frame'])
        bpy.context.view_layer.update()
        for side in ('left', 'right'):
            got = arm.pose.bones['grip_' + side[0]].tail
            errors.append((round((Vector(t[side]) - got).length, 3), t['frame'], side))
    errors.sort(reverse=True)
    return [dict(error_m=e, frame=f, hand=h) for e, f, h in errors[:worst] if e > 0.005]


def clone_weapon(template, scene, view, clip, tag):
    coll = bpy.data.collections.new(f'COL-{tag}-{view}-{clip}')
    scene.collection.children.link(coll)
    arm = MB.clone_arm(template['armature'], coll, f'SKEL-{tag}-{view}-{clip}')
    meshes = MB.clone_meshes(template['meshes'], arm, coll, f'{view}-{clip}')
    return dict(armature=arm, meshes=meshes, collection=coll)


def generate(profile, builder, ammo_builder, spec, clips, views, out, do_bake=True):
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    MATS.ensure_all()
    src = MB.source_bean(root_scene)
    weapon_coll = bpy.data.collections.new('COL-Weapon-Template')
    root_scene.collection.children.link(weapon_coll)
    built = assemble_weapon(profile, builder, weapon_coll, dict(spec.get('params', {})), out, do_bake)
    ammo = assemble_ammo(ammo_builder, weapon_coll, out, do_bake)
    contract = {'bones': list(built['bones']) + ['socket_' + x for x in built['sockets']],
                'sockets': {k: list(v[1]) for k, v in built['sockets'].items()}, 'params': built['params'],
                'bounds': {'size': list(profile.bounds_size)}, 'physics': built['physics'],
                'extra_bones': list(profile.extra_bone_names), 'root_bone': 'root',
                'sight_line': {'rear': list(profile.sight_rear), 'front': list(profile.sight_front)}}
    template = {'armature': built['armature'], 'meshes': [built['object']], 'collection': weapon_coll, 'contract': contract,
                'bounds': built['bounds'], 'weapon_meshes': [built['object']], 'textures': built['textures']}
    choreo.apply(profile)
    records = []
    clips = list(clips) if clips else list(A.CLIPS)
    tag = profile.name
    for view in views:
        for clip in clips:
            print(f'[{tag}] {view} {clip}', flush=True)
            scene = bpy.data.scenes.new(f'{tag.upper()}-{view}-{clip}')
            bpy.context.window.scene = scene
            bean = MB.clone_bean(src, scene, view, clip)
            weapon = clone_weapon(template, scene, view, clip, tag)
            cam = MB.stage(scene, view, clip)
            choreo.CURRENT.update(gun=weapon['armature'], clip=clip, view=view)
            choreo.EXTRA.clear()
            qa = A.animate(scene, bean, weapon, clip, view)
            # the musket names its actions: rename, then key the extra bones and the third person head
            ba = bpy.data.actions[qa['bean_action']]
            ga = bpy.data.actions[qa['weapon_action']]
            ba.name = f'A_Bean_{tag}_{view}_{clip}'
            ga.name = f'A_{tag}_{view}_{clip}'
            qa['bean_action'], qa['weapon_action'] = ba.name, ga.name
            choreo.bake_extra(weapon['armature'], ga.name)
            choreo.head_pass(bean['armature'], ba.name, clip, view, A.CLIPS[clip])
            targets = qa.pop('grip_targets')
            qa['hand_error_frames'] = hand_error_frames(scene, bean['armature'], targets)
            qa['extra_bones_check'] = choreo.verify_extra(scene, weapon['armature'])
            scene.frame_set(0)
            qa['authored_method'] = ('T-250 long arm pack: the musket pack\'s FP arms (placed through the game camera) and TP two-segment '
                                     'arms, re-timed reload with the weapon\'s own grips, ramrod and cartridge; CoD style recoil, '
                                     'breathing, sway and settles; sights on the view axis when aiming; Discard and Inspect clips.')
            records.append(dict(scene=scene.name, view=view, clip=clip, start=0, end=A.CLIPS[clip], fps=A.FPS, camera=cam.name,
                                camera_data=MB.camera_record(cam),
                                actors=dict(bean=dict(armature=bean['armature'].name, meshes=[o.name for o in bean['meshes']],
                                                      action=qa['bean_action']),
                                            weapon=dict(armature=weapon['armature'].name, meshes=[o.name for o in weapon['meshes']],
                                                        action=qa['weapon_action'])),
                                events=qa['events'], preview_fx=[],
                                grip_contract=dict(bean_tail_bones={'l': 'grip_l', 'r': 'grip_r'},
                                                   weapon_head_bones={'l': 'socket_grip_l', 'r': 'socket_grip_r'},
                                                   release_intervals=profile.release_intervals(clip)), qa=qa))
            STATE.setdefault('targets', {})[f'{view}-{clip}'] = targets
            print(f'[{tag}] hand error {qa["hand_error_max_m"]}, arm scale max {qa["upperarm_scale_max"]:.3f}, worst '
                  f'{qa["hand_error_frames"]}, extra {qa["extra_bones_check"]}', flush=True)
    bpy.context.window.scene = root_scene
    root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = f'SCN-{profile.pack_id}'
    framing = A.fp_framing()
    reload_events = choreo.events('GroundReload')
    manifest = dict(format='bean-musket-anim/1', pack=profile.pack_id, weapon=tag, version=spec['version'], fps=A.FPS, local_build=True,
                    built_by=f'scripts/props/{tag.lower()}_pack/build.py (T-250, scripts/props/longarm_common)',
                    source=dict(blend=MB.SOURCE.relative_to(REPO).as_posix(), sha256=MB.sha(MB.SOURCE)),
                    views=list(views), scenes=records, weapon_contract=contract, assets=[],
                    requested_clips=list(A.CLIPS), delivered_clips=clips, not_delivered_clips=sorted(set(A.CLIPS) - set(clips)),
                    fp_framing=framing,
                    reload_sequence={'clip': 'GroundReload', 'view_variants': list(views), 'fps': A.FPS, 'start_frame': 0,
                                     'end_frame': profile.reload_frames, 'duration_s': profile.reload_frames / A.FPS,
                                     'windows_frames': {k: list(v) for k, v in profile.windows.items()}, 'events': reload_events},
                    discard={'clip': 'Discard', 'frames': A.CLIPS['Discard'], 'duration_s': A.CLIPS['Discard'] / A.FPS,
                             'events': choreo.events('Discard'),
                             'release_frame': A.RELEASE_FRAME, 'empty_frame': A.EMPTY_FRAME,
                             'weapon_clip': 'goes on after Release: flies right and down, spins, lies on the ground by the end (TP)',
                             'bean_clip': 'ends with both hands empty (free rest targets)'},
                    ammo=[dict(name=i['name'], size_m=i.get('size_m')) for i in ammo],
                    notes=spec.get('notes', []))
    STATE.update(manifest=manifest, template=template, source=src, root_scene=root_scene, ammo=ammo, profile=profile)
    return manifest


def post_build(out, profile):
    manifest = STATE['manifest']
    manifest['assets'] = EXPORT.export_pack(Path(out), manifest, STATE['source'], STATE['template'], profile, STATE['ammo'])
    MB.write(Path(out) / profile.manifest_file, manifest)
    MB.write(Path(out) / 'fp-framing.json', manifest['fp_framing'])
    for name, targets in STATE.get('targets', {}).items():
        MB.write(Path(out) / 'validation' / 'grips' / (name + '.json'), targets)
    bpy.context.window.scene = STATE['root_scene']


def product_renders(out, profile):
    """Weapon and ammunition on a dark table, three quarter view, plus a close-up of the lock."""
    template, ammo = STATE['template'], STATE['ammo']
    scene = bpy.data.scenes.new(f'PRODUCT-{profile.name}')
    bpy.context.window.scene = scene
    scene.collection.children.link(template['collection'])
    scene.unit_settings.system = 'METRIC'
    try:
        scene.render.engine = 'BLENDER_EEVEE'
    except TypeError:
        scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1600, 900, 100
    scene.render.image_settings.file_format = 'PNG'
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    scene.world = bpy.data.worlds.new('World-product')
    scene.world.use_nodes = True
    bg = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs['Color'].default_value = (.18, .19, .21, 1)
    bg.inputs['Strength'].default_value = .35
    arm = template['armature']
    for p in arm.pose.bones:
        p.matrix_basis = Matrix.Identity(4)
    arm.location = (0, 0, 0.0)
    # table
    me = bpy.data.meshes.new('REF-Table')
    me.from_pydata([(-3, -3, -0.09), (3, -3, -0.09), (3, 3, -0.09), (-3, 3, -0.09)], [], [(0, 1, 2, 3)])
    table = bpy.data.objects.new('REF-Table', me)
    scene.collection.objects.link(table)
    table.data.materials.append(MB.material('MAT-Table', (.09, .07, .06), .7))
    for item in ammo:
        ob = item['object']
        ob.hide_render = False
        ob.hide_viewport = False
        ob.location = (profile.muzzle.x * 0.25, -0.16, -0.09)
    for i, (loc, energy, size) in enumerate((((0.4, -0.8, 1.0), 60, 1.2), ((-0.6, 0.6, 0.8), 28, 1.5), ((1.0, 0.4, 0.5), 22, 0.8))):
        data = bpy.data.lights.new(f'LGT-Product-{i}', 'AREA')
        data.energy = energy
        data.size = size
        light = bpy.data.objects.new(data.name, data)
        scene.collection.objects.link(light)
        light.location = loc
        light.rotation_euler = (Vector((0, 0, 0)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    data = bpy.data.cameras.new(f'CAM-Product-{profile.name}')
    cam = bpy.data.objects.new(data.name, data)
    scene.collection.objects.link(cam)
    scene.camera = cam
    centre = Vector(((template['bounds']['min'][0] + template['bounds']['max'][0]) / 2, -0.04, -0.02))
    length = template['bounds']['max'][0] - template['bounds']['min'][0]
    views = {'product': (centre + Vector((0.25 * length, -1.15 * length, 0.75 * length)), 42.0),
             'product-lock': (Vector((0.10, -0.42, 0.22)), 60.0)}
    targets = {'product': centre, 'product-lock': Vector((0.05, -0.02, 0.03))}
    for name, (loc, lens) in views.items():
        cam.location = loc
        cam.rotation_euler = (targets[name] - loc).to_track_quat('-Z', 'Y').to_euler()
        data.lens = lens
        data.clip_start = 0.01
        scene.render.filepath = str(Path(out) / 'renders' / f'{name}.png')
        try:
            scene.eevee.taa_render_samples = 32
        except AttributeError:
            pass
        bpy.ops.render.render(write_still=True, scene=scene.name)
    for item in ammo:
        item['object'].location = (0, 0, 0)
        item['object'].hide_render = True
    arm.location = (0, 0, 0)
    scene.collection.children.unlink(template['collection'])


def main(profile, builder, ammo_builder, spec_path, default_out):
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--clips', default='')
    p.add_argument('--views', default='TP,FP')
    p.add_argument('--no-export', action='store_true')
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--render', default='')
    p.add_argument('--product', action='store_true')
    p.add_argument('--save', action='store_true')
    p.add_argument('--out', default=str(default_out))
    args = p.parse_args(argv)
    spec = json.loads(Path(spec_path).read_text(encoding='utf8'))
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    clips = [c for c in args.clips.split(',') if c] or None
    views = tuple(v for v in args.views.split(',') if v)
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    manifest = generate(profile, builder, ammo_builder, spec, clips, views, out, do_bake=not args.no_bake)
    if not args.no_export:
        post_build(out, profile)
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        rec = next(r for r in manifest['scenes'] if r['view'] == view and r['clip'] == clip)
        MB.render(bpy.data.scenes[rec['scene']], out, '%s-%s-%03d.png' % (view, clip, int(frame_)), int(frame_))
    if args.product:
        product_renders(out, profile)
    if args.save:
        bpy.ops.wm.save_as_mainfile(filepath=str(out / f'{profile.pack_id}.blend'), compress=True)
    print(f'[{profile.name}] finished {out}', flush=True)
