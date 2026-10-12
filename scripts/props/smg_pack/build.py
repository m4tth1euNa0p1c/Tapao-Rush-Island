"""Bean drum sub-machine gun pack "Mitraillette" (T-334, owner 2026-10-11: "mitraillette"): the gun (model.py: walnut
stock and grips with a CC0 grain recoloured, blued steel, finned barrel and compensator, brass knob, latch and drum
key, 50 round drum), PBR textures baked from procedural materials (Wood 2K, Metal 2K, Drum 1K, casing 512), the bean's
12 clips in third (TP) and first person (FP) at 30 fps (animation.py: FireLoop at 11 rounds/s, Reload / ReloadEmpty
with the drum), statics (pickup, full drum, spent case), Unreal FBX and the manifest
art/export/Weapons/Mitraillette/pack.json (format bean-musket-anim/1, contract in spec.py).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/smg_pack/build.py -- [options]
    --clips FireLoop,Idle   only these clips      --views FP      only these views (TP,FP)
    --no-bake               procedural materials, no texture bake (quick look)
    --no-export             no FBX / manifest     --model-only    the gun, textures and statics, no clip
    --render FP:FireLoop:3  stills into <out>/renders/ (view:clip:frame)
    --product               product renders       --previews     control renders art/_previews/T334/mitraillette_*.png
    --save                  also save art/blender/weapons/bean_mitraillette.blend
    --out DIR               pack folder (default art/export/Weapons/Mitraillette)
Check: python scripts/props/period_arms_common/check_pack.py art/export/Weapons/Mitraillette
"""
from __future__ import annotations

import argparse
import importlib
import json
import math
import sys
import time
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
COMMON = REPO / 'scripts/props/period_arms_common'
LONGARM = REPO / 'scripts/props/longarm_common'
MUSKET = REPO / 'scripts/props/musket_pack'
for p in (MUSKET, LONGARM, COMMON, HERE):
    if str(p) in sys.path:
        sys.path.remove(str(p))
    sys.path.insert(0, str(p))

import spec as S  # noqa: E402
import contract as C  # noqa: E402
import rig as R  # noqa: E402
import pa_export as EX  # noqa: E402
import pa_bake as PB  # noqa: E402
import pa_materials as PM  # noqa: E402
import kit as K  # noqa: E402
import model as MD  # noqa: E402
import motion as MO  # noqa: E402
import animation as AN  # noqa: E402

for _m in (S, C, R, EX, PB, PM, MD, MO, AN):
    importlib.reload(_m)

PACK_SPEC = HERE / 'pack.json'
PREVIEWS = REPO / 'art/_previews/T334'
BLEND = REPO / 'art/blender/weapons/bean_mitraillette.blend'
BAKE_SIZES = {'Wood': 2048, 'Metal': 2048, 'Drum': 1024}
STATE = {}


def build_template(coll, out, bake=True):
    scene = bpy.context.scene
    res = MD.build_gun(coll)
    bone_names = list(res['bones'])
    all_parts = [o for g in res['parts'].values() for objs in g.values() for o in objs]
    physics = K.part_physics(all_parts)
    groups, textures = [], {}
    for gname, by_bone in res['parts'].items():
        if not by_bone:
            continue
        ob = K.join_parts(f'GEO-{S.TAG}-{gname}', by_bone, coll, bone_names=bone_names)
        if bake:
            paths = PB.bake_group(scene, ob, f'{S.TAG}_{gname}', Path(out) / 'textures', BAKE_SIZES[gname])
            textures.update({f'{gname}.{k}': v for k, v in paths.items()})
        else:
            PB.flat_group(ob)
        groups.append(ob)
    bpy.context.view_layer.update()
    for o in list(bpy.context.view_layer.objects):
        if o is not None:
            o.select_set(False)
    for o in groups:
        o.select_set(True)
    bpy.context.view_layer.objects.active = groups[0]
    bpy.ops.object.join()
    mesh = bpy.context.view_layer.objects.active
    mesh.name = mesh.data.name = f'SK_Bean_{S.TAG}'
    arm = R.make_skeleton(f'SKEL-{S.TAG}-Template', res['bones'], res['sockets'], coll, markers=S.MARKERS)
    mesh.parent = arm
    mod = mesh.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    proxy = R.bind_proxy(res['bones'], coll, bone_names, f'GEO-{S.TAG}-BindProxy')
    proxy.parent = arm
    pmod = proxy.modifiers.new('Armature', 'ARMATURE')
    pmod.object = arm
    lo, hi = EX.bounds(mesh.data)
    tris = EX.tri_count(mesh.data)
    positions = {b.name: list(b.head_local) for b in arm.data.bones}
    contract = C.weapon_contract(S, positions)
    contract.update(bounds={'min': [round(v, 4) for v in lo], 'max': [round(v, 4) for v in hi], 'size': [round(hi[i] - lo[i], 4) for i in range(3)]},
                    triangle_count=tris, physics=physics, params=res['params'], sights=res['sights'],
                    materials=[m.name for m in mesh.data.materials], uv_layers=[u.name for u in mesh.data.uv_layers],
                    bolt={'type': 'open bolt, rests to the rear (cocked)', 'travel_m': S.BOLT_TRAVEL, 'knob_rest_m': list(S.KNOB)},
                    magazine={'type': 'drum', 'capacity': 50, 'centre_m': list(S.DRUM_C), 'radius_m': S.DRUM_R,
                              'insertion': 'from below in front of the trigger guard, held by the latch (bone mag_latch)'})
    print(f'[smg] template: {tris} triangles, {len(res["bones"])} bones + {len(res["sockets"])} sockets, size {contract["bounds"]["size"]}, '
          f'materials {contract["materials"]}', flush=True)
    return {'armature': arm, 'meshes': [mesh], 'weapon_meshes': [mesh], 'bind_proxy': proxy, 'collection': coll, 'contract': contract,
            'bounds': contract['bounds'], 'texture_files': textures}


def build_statics(scene, template, out, bake=True):
    coll = bpy.data.collections.new(f'COL-{S.TAG}-Statics')
    scene.collection.children.link(coll)
    sk = template['meshes'][0]
    statics = {}
    me = EX.mesh_of_groups(sk, [], Matrix.Identity(4), drop=True)          # the whole gun with its drum
    me.name = f'SM_Bean_{S.TAG}'
    boxes = [((-0.325, -0.021, -0.036), (-0.040, 0.021, 0.086)), ((-0.040, -0.016, 0.026), (0.205, 0.017, 0.100)),
             ((-0.030, -0.014, -0.052), (0.068, 0.014, 0.034)), ((0.060, -0.024, -0.084), (0.176, 0.030, 0.036)),
             ((0.200, -0.017, 0.044), (0.484, 0.017, 0.090)), ((0.245, -0.013, -0.048), (0.284, 0.013, 0.050))]
    statics[me.name] = EX.static(coll, scene, me.name, me, hull_boxes=boxes, view='pickup', role=S.STATICS[me.name]['role'])
    # the full drum lying on its face (axis vertical)
    T = Matrix.Rotation(math.radians(90), 4, 'X') @ Matrix.Translation(-Vector(S.DRUM_C))
    me = EX.mesh_of_groups(sk, ['drum'], T)
    me.name = 'SM_Ammo_MitrailletteDrum'
    statics[me.name] = EX.static(coll, scene, me.name, me, hull='convex', view='ammo', role=S.STATICS[me.name]['role'])
    parts = MD.build_casing(coll)
    ob = K.join_parts('GEO-Mitraillette-Casing', {'root': parts}, coll)
    if bake:
        paths = PB.bake_group(scene, ob, f'{S.TAG}_Casing', Path(out) / 'textures', 512, color_samples=8, ao_samples=32, ao_distance=0.003)
        template['texture_files'].update({f'Casing.{k}': v for k, v in paths.items()})
    else:
        PB.flat_group(ob)
    me = ob.data.copy()
    me.transform(ob.matrix_world)
    bpy.data.objects.remove(ob, do_unlink=True)
    me.name = f'SM_{S.TAG}_Casing'
    statics[me.name] = EX.static(coll, scene, me.name, me, hull='convex', view='fx', role=S.STATICS[me.name]['role'])
    STATE['statics'] = statics
    return statics


def generate(spec_json, out, clips=None, views=('TP', 'FP'), bake=True, model_only=False):
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    weapon_coll = bpy.data.collections.new('COL-Weapon-Template')
    root_scene.collection.children.link(weapon_coll)
    template = build_template(weapon_coll, out, bake)
    statics = build_statics(root_scene, template, out, bake)
    manifest = C.manifest_skeleton(S, spec_json['version'], 'built', S.EXPORT_DIR)
    manifest.pop('provisional_note', None)
    records = []
    src = None
    clips = [] if model_only else (list(clips) if clips else list(S.CLIPS))
    if clips:
        src = R.source_bean(root_scene)
    for view in views if clips else ():
        for clip in clips:
            print(f'[smg] {view} {clip}', flush=True)
            scene = bpy.data.scenes.new(f'{S.TAG.upper()}-{view}-{clip}')
            bpy.context.window.scene = scene
            bean = R.clone_bean(src, scene, view, clip, MO.fp_arm_transform)
            weapon = R.clone_weapon(template, scene, view, clip, S.TAG)
            cam = R.stage(scene, view, clip, S.FPS, MO.camera_matrix(), MO.FP_LENS)
            qa = MO.animate(scene, bean, weapon, clip, view, AN, S.TAG)
            targets = qa.pop('grip_targets')
            records.append(dict(scene=scene.name, view=view, clip=clip, start=0, end=S.CLIPS[clip], fps=S.FPS, loop=clip in S.LOOPS,
                                camera=cam.name, camera_data=R.camera_record(cam),
                                actors=dict(bean=dict(armature=bean['armature'].name, meshes=[o.name for o in bean['meshes']], action=qa['bean_action']),
                                            weapon=dict(armature=weapon['armature'].name, meshes=[o.name for o in weapon['proxy']],
                                                        render_meshes=[o.name for o in weapon['meshes']], action=qa['weapon_action'])),
                                events=qa['events'], preview_fx=[],
                                grip_contract=dict(bean_tail_bones={'l': 'grip_l', 'r': 'grip_r'},
                                                   weapon_head_bones={'l': 'socket_grip_l', 'r': 'socket_grip_r'},
                                                   release_intervals=AN.release_intervals(clip)), qa=qa))
            STATE.setdefault('targets', {})[f'{view}-{clip}'] = targets
            print('[smg] hand error %s, arm scale max %.3f' % (qa['hand_error_max_m'], qa['upperarm_scale_max']), flush=True)
    bpy.context.window.scene = root_scene
    if src:
        root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = f'SCN-{S.PACK_ID}'
    manifest.update(source=dict(blend=R.SOURCE.relative_to(REPO).as_posix(), sha256=R.sha(R.SOURCE)), views=list(views), scenes=records,
                    weapon_contract=template['contract'], fp_framing=AN.fp_framing(), delivered_clips=clips,
                    not_delivered_clips=sorted(set(S.CLIPS) - set(clips)),
                    fire_sequence={'clips': ['Fire', 'FireLoop'], 'fps': S.FPS, 'loop_frames': S.CLIPS['FireLoop'], 'shots_per_loop': len(S.SHOTS),
                                   'shot_frames': list(S.SHOTS), 'rounds_per_second_at_rate_1': round(len(S.SHOTS) * S.FPS / S.CLIPS['FireLoop'], 2),
                                   'note': 'FireLoop repeated while the trigger is held (seamless: frame 30 = frame 0), cut anywhere to '
                                           'Idle; Fire is one round. Eject SM_Mitraillette_Casing at every Fire from socket_eject.'},
                    reload_sequence={c: {'frames': S.CLIPS[c], 'duration_s': round(S.CLIPS[c] / S.FPS, 3), 'events': AN.events(c)}
                                     for c in ('Reload', 'ReloadEmpty')},
                    discard={'clip': 'Discard', 'frames': S.CLIPS['Discard'], 'events': AN.events('Discard'),
                             'release_frame': AN.RELEASE_FRAME, 'empty_frame': AN.EMPTY_FRAME})
    STATE.update(manifest=manifest, template=template, source=src, root_scene=root_scene, statics=statics)
    return manifest


def post_build(out):
    manifest = STATE['manifest']
    manifest['assets'] = EX.export_pack(Path(out), manifest, STATE['template'], STATE['statics'], S.TAG,
                                        bean_views=tuple(v for v in ('TP', 'FP') if any(r['view'] == v for r in manifest['scenes'])))
    manifest['statics'] = {k: {kk: vv for kk, vv in v.items() if kk not in ('object', 'hulls')} for k, v in STATE['statics'].items()}
    R.write(Path(out) / 'pack.json', manifest)
    R.write(Path(out) / 'fp-framing.json', manifest['fp_framing'])
    for name, targets in STATE.get('targets', {}).items():
        R.write(Path(out) / 'validation' / 'grips' / (name + '.json'), targets)
    bpy.context.window.scene = STATE['root_scene']


def scene_of(manifest, view, clip):
    rec = next(r for r in manifest['scenes'] if r['view'] == view and r['clip'] == clip)
    return bpy.data.scenes[rec['scene']]


def product(out, previews):
    scene, cam, coll = R.studio(f'SCN-Product-{S.TAG}')
    st = STATE['statics']
    pick = st[f'SM_Bean_{S.TAG}']
    gun = bpy.data.objects.new(f'PRODUCT-{S.TAG}', pick['object'].data)
    coll.objects.link(gun)
    gun.location = Vector(pick['pivot_offset_m']) + Vector((0, 0, 0.12))
    for i, (name, loc, rot) in enumerate((('SM_Ammo_MitrailletteDrum', (0.05, -0.30, 0.0), 20), (f'SM_{S.TAG}_Casing', (0.22, -0.22, 0.0), 30),
                                          (f'SM_{S.TAG}_Casing', (0.25, -0.26, 0.0), 110), (f'SM_{S.TAG}_Casing', (0.20, -0.28, 0.0), 75))):
        ob = bpy.data.objects.new(f'PRODUCT-{name}{i}', st[name]['object'].data)
        coll.objects.link(ob)
        ob.location = loc
        ob.rotation_euler = (0, 0, math.radians(rot))

    def g(p):
        return Vector(p) + Vector((0, 0, 0.12))

    shots = [('product-right.png', g((0.08, -1.05, 0.04)), g((0.08, 0.0, 0.0)), 40),
             ('product-left.png', g((0.08, 1.05, 0.04)), g((0.08, 0.0, 0.0)), 40),
             ('product-3-4.png', g((0.75, -0.70, 0.50)), g((0.06, -0.06, -0.03)), 38),
             ('product-drum.png', g((0.12, 0.36, -0.02)), g((0.12, 0.0, -0.03)), 55)]
    for name, loc, target, lens in shots:
        R.shoot(scene, cam, Path(out) / 'renders' / name, loc, target, lens)
        if previews:
            R.shoot(scene, cam, PREVIEWS / f'mitraillette_{name.replace("product-", "produit_")}', loc, target, lens)


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--clips', default='')
    p.add_argument('--views', default='TP,FP')
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--no-export', action='store_true')
    p.add_argument('--model-only', action='store_true')
    p.add_argument('--render', default='')
    p.add_argument('--product', action='store_true')
    p.add_argument('--previews', action='store_true')
    p.add_argument('--save', action='store_true')
    p.add_argument('--out', default=str(REPO / S.EXPORT_DIR))
    args = p.parse_args(argv)
    spec_json = json.loads(PACK_SPEC.read_text(encoding='utf-8'))
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    clips = [c for c in args.clips.split(',') if c] or None
    views = tuple(v for v in args.views.split(',') if v)
    out = Path(args.out).resolve()
    out.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    manifest = generate(spec_json, out, clips, views, bake=not args.no_bake, model_only=args.model_only)
    if not args.no_export and not args.model_only:
        post_build(out)
    if args.product or args.previews:
        product(out, args.previews)
    case_mesh = STATE['statics'][f'SM_{S.TAG}_Casing']['object'].data
    prefix = f'SKEL-{S.TAG}'

    def fx(view, clip, frame_):
        def extra(scene):
            objs = []
            if (clip == 'FireLoop' and frame_ in S.SHOTS) or (clip == 'Fire' and frame_ == 1):
                objs += R.flash_objects(scene, prefix, 0.55)
            if clip == 'FireLoop':
                for k, s in enumerate(S.SHOTS):
                    d = frame_ - s
                    if 1 <= d <= 4:
                        objs += R.case_object(scene, prefix, case_mesh, t=d / S.FPS)
            return objs
        return extra
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        R.render(scene_of(manifest, view, clip), out / 'renders' / ('%s-%s-%03d.png' % (view, clip, int(frame_))), int(frame_),
                 extra=fx(view, clip, int(frame_)))
    if args.previews and manifest['scenes']:
        shots = {'mitraillette_fp_repos.png': ('FP', 'Idle', 0), 'mitraillette_fp_rafale.png': ('FP', 'FireLoop', 3),
                 'mitraillette_fp_rafale_2.png': ('FP', 'FireLoop', 9), 'mitraillette_fp_verrou.png': ('FP', 'Reload', 13),
                 'mitraillette_fp_tambour_retire.png': ('FP', 'Reload', 22), 'mitraillette_fp_tambour_nouveau.png': ('FP', 'Reload', 46),
                 'mitraillette_fp_armement.png': ('FP', 'Reload', 67), 'mitraillette_fp_tambour_lache.png': ('FP', 'ReloadEmpty', 25),
                 'mitraillette_fp_inspection.png': ('FP', 'Inspect', 40), 'mitraillette_fp_sprint.png': ('FP', 'Sprint', 20),
                 'mitraillette_tp_repos.png': ('TP', 'Idle', 0), 'mitraillette_tp_rafale.png': ('TP', 'FireLoop', 3),
                 'mitraillette_tp_rechargement.png': ('TP', 'Reload', 46)}
        for name, (view, clip, frame_) in shots.items():
            if any(r['view'] == view and r['clip'] == clip for r in manifest['scenes']):
                R.render(scene_of(manifest, view, clip), PREVIEWS / name, frame_, extra=fx(view, clip, frame_))
    if args.save:
        bpy.context.window.scene = STATE['root_scene']
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
    print('[smg] finished %s in %.0f s' % (out, time.time() - t0), flush=True)


if __name__ == '__main__':
    main()
