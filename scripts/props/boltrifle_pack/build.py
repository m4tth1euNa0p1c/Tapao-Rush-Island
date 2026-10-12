"""Bean bolt-action sniper rifle pack (T-334, owner 2026-10-11: "sniper verrou"): the rifle (model.py: walnut stock
with a CC0 grain recoloured, blued steel, bolt in the white, brass scope offset to the left, stripper clip of five),
PBR textures baked from procedural materials (Wood 2K, Metal 2K, Ammo 1K, casing 512, clip box 1K; lenses kept as a
glass material), the bean's 15 clips in third (TP) and first person (FP) at 30 fps (animation.py), statics (pickup,
clip of five, box of clips, spent case, empty clip), Unreal FBX and the manifest art/export/Weapons/BoltRifle/pack.json
(format bean-musket-anim/1, contract in spec.py).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/boltrifle_pack/build.py -- [options]
    --clips BoltCycle,Idle   only these clips      --views FP      only these views (TP,FP)
    --no-bake                procedural materials, no texture bake (quick look)
    --no-export              no FBX / manifest     --model-only    the rifle, textures and statics, no clip
    --render FP:BoltCycle:6  stills into <out>/renders/ (view:clip:frame)
    --product                product renders       --previews     control renders art/_previews/T334/boltrifle_*.png
    --save                   also save art/blender/weapons/bean_boltrifle.blend
    --out DIR                pack folder (default art/export/Weapons/BoltRifle)
Check: python scripts/props/period_arms_common/check_pack.py art/export/Weapons/BoltRifle
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
BLEND = REPO / 'art/blender/weapons/bean_boltrifle.blend'
BAKE_SIZES = {'Wood': 2048, 'Metal': 2048, 'Ammo': 1024}
STATE = {}


def build_template(coll, out, bake=True):
    scene = bpy.context.scene
    res = MD.build_rifle(coll)
    bone_names = list(res['bones'])
    all_parts = [o for g in res['parts'].values() for objs in g.values() for o in objs]
    physics = K.part_physics(all_parts)
    groups, textures = [], {}
    for gname, by_bone in res['parts'].items():
        if not by_bone:
            continue
        ob = K.join_parts(f'GEO-{S.TAG}-{gname}', by_bone, coll, bone_names=bone_names)
        if gname != 'Lens':
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
                    triangle_count=tris, physics=physics, params=res['params'], scope=res['scope'], iron_sights=res['iron_sights'],
                    materials=[m.name for m in mesh.data.materials], uv_layers=[u.name for u in mesh.data.uv_layers],
                    bolt={'axis_m': [0.0, 0.0, S.BORE_Z], 'lift_deg': S.BOLT_LIFT_DEG, 'travel_m': S.BOLT_TRAVEL,
                          'lift_axis': 'weapon X through the bore axis, the handle rises (rotation -lift about +X)',
                          'knob_rest_m': list(S.KNOB)},
                    magazine={'capacity': 5, 'clip': 'stripper clip of five, seated in the charger guide at x = %.4f m' % S.CLIP_X,
                              'push_depth_m': S.PUSH_DEPTH})
    print(f'[boltrifle] template: {tris} triangles, {len(res["bones"])} bones + {len(res["sockets"])} sockets, size {contract["bounds"]["size"]}, '
          f'materials {contract["materials"]}', flush=True)
    return {'armature': arm, 'meshes': [mesh], 'weapon_meshes': [mesh], 'bind_proxy': proxy, 'collection': coll, 'contract': contract,
            'bounds': contract['bounds'], 'texture_files': textures}


def lying_clip(sk, groups):
    """The clip (and its rounds) lying flat, rounds along +X, centred, bottom at z = 0 (static space)."""
    T = Matrix.Rotation(math.radians(90), 4, 'X') @ Matrix.Translation(-Vector(S.BONES['stripper_clip'][0]))
    return EX.mesh_of_groups(sk, groups, T)


def build_statics(scene, template, out, bake=True):
    coll = bpy.data.collections.new(f'COL-{S.TAG}-Statics')
    scene.collection.children.link(coll)
    sk = template['meshes'][0]
    statics = {}
    me = EX.mesh_of_groups(sk, ['stripper_clip', 'clip_rounds'], Matrix.Identity(4), drop=True)
    me.name = f'SM_Bean_{S.TAG}'
    boxes = [((-0.352, -0.024, -0.106), (-0.020, 0.032, 0.052)), ((-0.020, -0.022, -0.062), (0.245, 0.024, 0.060)),
             ((0.245, -0.018, -0.002), (0.750, 0.018, 0.058)), ((0.018, 0.010, 0.056), (0.332, 0.046, 0.100)),
             ((0.030, -0.070, 0.016), (0.062, -0.010, 0.044))]
    statics[me.name] = EX.static(coll, scene, me.name, me, hull_boxes=boxes, view='pickup', role=S.STATICS[me.name]['role'])
    me = lying_clip(sk, ['stripper_clip', 'clip_rounds'])
    me.name = 'SM_Ammo_StripperClip'
    statics[me.name] = EX.static(coll, scene, me.name, me, hull='convex', view='ammo', role=S.STATICS[me.name]['role'])
    me = lying_clip(sk, ['stripper_clip'])
    me.name = f'SM_{S.TAG}_EmptyClip'
    statics[me.name] = EX.static(coll, scene, me.name, me, hull='convex', view='fx', role=S.STATICS[me.name]['role'])
    # the spent case: its own small bake
    parts = MD.build_casing(coll)
    ob = K.join_parts('GEO-BoltRifle-Casing', {'root': parts}, coll)
    if bake:
        paths = PB.bake_group(scene, ob, f'{S.TAG}_Casing', Path(out) / 'textures', 512, color_samples=8, ao_samples=32, ao_distance=0.004)
        template['texture_files'].update({f'Casing.{k}': v for k, v in paths.items()})
    else:
        PB.flat_group(ob)
    me = ob.data.copy()
    me.transform(ob.matrix_world)
    bpy.data.objects.remove(ob, do_unlink=True)
    me.name = f'SM_{S.TAG}_Casing'
    statics[me.name] = EX.static(coll, scene, me.name, me, hull='convex', view='fx', role=S.STATICS[me.name]['role'])
    # the box of clips: cardboard tray (its own bake) and two clips on top
    parts = MD.build_clip_box(coll)
    box = K.join_parts('GEO-BoltRifle-ClipBox', {'root': parts}, coll)
    if bake:
        paths = PB.bake_group(scene, box, 'Ammo_RifleClipBox', Path(out) / 'textures', 1024, color_samples=8, ao_samples=32, ao_distance=0.01)
        template['texture_files'].update({f'ClipBox.{k}': v for k, v in paths.items()})
    else:
        PB.flat_group(box)
    me = box.data.copy()
    me.transform(box.matrix_world)
    bpy.data.objects.remove(box, do_unlink=True)
    clips = []
    for y in (-0.0305, 0.0305):
        c = lying_clip(sk, ['stripper_clip', 'clip_rounds'])
        lo, hi = EX.bounds(c)
        c.transform(Matrix.Translation((-(lo.x + hi.x) * 0.5, y - (lo.y + hi.y) * 0.5, 0.0017 - lo.z)))
        clips.append(c)
    import bmesh
    bm = bmesh.new()
    bm.from_mesh(me)
    for c in clips:
        remap = []
        for m in c.materials:
            names = [x.name for x in me.materials]
            if m.name not in names:
                me.materials.append(m)
                names.append(m.name)
            remap.append(names.index(m.name))
        tmp = bmesh.new()
        tmp.from_mesh(c)
        for f in tmp.faces:
            f.material_index = remap[f.material_index]
        tmp_me = bpy.data.meshes.new('tmp')
        tmp.to_mesh(tmp_me)
        tmp.free()
        bm.from_mesh(tmp_me)
        bpy.data.meshes.remove(tmp_me)
    bm.to_mesh(me)
    bm.free()
    me.name = 'SM_Ammo_RifleClipBox'
    statics[me.name] = EX.static(coll, scene, me.name, me, hull_boxes=[((-0.038, -0.066, 0.0), (0.038, 0.066, 0.031))], view='ammo',
                                 role=S.STATICS[me.name]['role'])
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
            print(f'[boltrifle] {view} {clip}', flush=True)
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
            print('[boltrifle] hand error %s, arm scale max %.3f' % (qa['hand_error_max_m'], qa['upperarm_scale_max']), flush=True)
    bpy.context.window.scene = root_scene
    if src:
        root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = f'SCN-{S.PACK_ID}'
    manifest.update(source=dict(blend=R.SOURCE.relative_to(REPO).as_posix(), sha256=R.sha(R.SOURCE)), views=list(views), scenes=records,
                    weapon_contract=template['contract'], fp_framing=AN.fp_framing(), delivered_clips=clips,
                    not_delivered_clips=sorted(set(S.CLIPS) - set(clips)),
                    bolt_sequence={'clip': 'BoltCycle', 'fps': S.FPS, 'frames': S.CLIPS['BoltCycle'], 'duration_s': round(S.CLIPS['BoltCycle'] / S.FPS, 3),
                                   'events': AN.events('BoltCycle'), 'lift_deg': S.BOLT_LIFT_DEG, 'travel_m': S.BOLT_TRAVEL,
                                   'note': 'Fire (or AimFire) then BoltCycle; CaseEject: spawn SM_BoltRifle_Casing at socket_eject toward socket_eject_dir'},
                    reload_sequence={'clip': 'Reload', 'fps': S.FPS, 'frames': S.CLIPS['Reload'], 'duration_s': round(S.CLIPS['Reload'] / S.FPS, 3),
                                     'events': AN.events('Reload'), 'rounds': 5,
                                     'note': 'RoundsIn adds the five rounds of the clip; ClipEject: the strip is kicked out (bone hidden 8 '
                                             'frames later; SM_BoltRifle_EmptyClip for a physical one); BoltClose chambers a round'},
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
    gun.location = Vector(pick['pivot_offset_m']) + Vector((0, 0, 0.16))      # weapon point p -> p + (0, 0, 0.16)
    for i, (name, loc, rot) in enumerate((('SM_Ammo_RifleClipBox', (0.05, -0.30, 0.0), 12), ('SM_Ammo_StripperClip', (0.20, -0.26, 0.0), 70),
                                          (f'SM_{S.TAG}_Casing', (0.30, -0.22, 0.0), 30), (f'SM_{S.TAG}_Casing', (0.33, -0.25, 0.0), 110),
                                          (f'SM_{S.TAG}_EmptyClip', (0.27, -0.31, 0.0), -20))):
        ob = bpy.data.objects.new(f'PRODUCT-{name}{i}', st[name]['object'].data)
        coll.objects.link(ob)
        ob.location = loc
        ob.rotation_euler = (0, 0, math.radians(rot))

    def g(p):
        return Vector(p) + Vector((0, 0, 0.16))

    shots = [('product-right.png', g((0.20, -1.30, 0.06)), g((0.20, 0.0, 0.0)), 40),
             ('product-left.png', g((0.20, 1.30, 0.06)), g((0.20, 0.0, 0.0)), 40),
             ('product-3-4.png', g((0.95, -0.90, 0.55)), g((0.18, -0.06, -0.04)), 38),
             ('product-action.png', g((0.12, -0.26, 0.20)), g((0.10, 0.0, 0.05)), 58),
             ('product-ammo.png', (0.18, -0.62, 0.36), (0.18, -0.27, 0.0), 55)]
    for name, loc, target, lens in shots:
        R.shoot(scene, cam, Path(out) / 'renders' / name, loc, target, lens)
        if previews:
            R.shoot(scene, cam, PREVIEWS / f'boltrifle_{name.replace("product-", "produit_")}', loc, target, lens)


def aim_camera(manifest):
    """The aim framing of the game (AimIdle frame 0): the camera at the eye, the field of view of fp_framing."""
    return MO.camera_matrix(), 18.0 / math.tan(math.radians(manifest['fp_framing']['aim_field_of_view_deg'] / 2))


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
            if clip in ('Fire', 'AimFire') and frame_ in (2, 3):
                objs += R.flash_objects(scene, prefix, 0.8)
            if clip == 'BoltCycle' and 11 <= frame_ <= 15:
                objs += R.case_object(scene, prefix, case_mesh, t=(frame_ - 10) / S.FPS)
            return objs
        return extra
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        R.render(scene_of(manifest, view, clip), out / 'renders' / ('%s-%s-%03d.png' % (view, clip, int(frame_))), int(frame_),
                 extra=fx(view, clip, int(frame_)))
    if args.previews and manifest['scenes']:
        shots = {'boltrifle_fp_repos.png': ('FP', 'Idle', 0), 'boltrifle_fp_tir.png': ('FP', 'Fire', 3),
                 'boltrifle_fp_culasse_levee.png': ('FP', 'BoltCycle', 6), 'boltrifle_fp_culasse_tiree.png': ('FP', 'BoltCycle', 12),
                 'boltrifle_fp_rechargement_lame.png': ('FP', 'Reload', 30), 'boltrifle_fp_rechargement_pouce.png': ('FP', 'Reload', 38),
                 'boltrifle_fp_rechargement_ejection_lame.png': ('FP', 'Reload', 54), 'boltrifle_fp_inspection.png': ('FP', 'Inspect', 40),
                 'boltrifle_fp_sprint.png': ('FP', 'Sprint', 20), 'boltrifle_fp_visee.png': ('FP', 'AimIdle', 0),
                 'boltrifle_tp_repos.png': ('TP', 'Idle', 0), 'boltrifle_tp_visee.png': ('TP', 'AimIdle', 0),
                 'boltrifle_tp_culasse.png': ('TP', 'BoltCycle', 11), 'boltrifle_tp_rechargement.png': ('TP', 'Reload', 38)}
        side = R.look_at((-1.80, -2.00, 0.95), (0.0, -0.30, 0.64))       # front right of the bean, square to the aimed rifle
        for name, (view, clip, frame_) in shots.items():
            if any(r['view'] == view and r['clip'] == clip for r in manifest['scenes']):
                cam = side if (view == 'TP' and clip in ('AimIdle', 'Reload')) else None
                R.render(scene_of(manifest, view, clip), PREVIEWS / name, frame_, extra=fx(view, clip, frame_), camera=cam)
        if any(r['view'] == 'FP' and r['clip'] == 'AimIdle' for r in manifest['scenes']):
            cam, lens = aim_camera(manifest)
            R.render(scene_of(manifest, 'FP', 'AimIdle'), PREVIEWS / 'boltrifle_fp_visee_zoom.png', 0, camera=cam, lens=lens)
    if args.save:
        bpy.context.window.scene = STATE['root_scene']
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
    print('[boltrifle] finished %s in %.0f s' % (out, time.time() - t0), flush=True)


if __name__ == '__main__':
    main()
