"""Bean bayonet musket pack (T-334, owner 2026-10-11: "fusil mousquet avec baïonnette et utilisation de baïonnette"):
the pirate musket of scripts/props/musket_pack (its model, looks and animation module, imported unchanged) with a
socket bayonet (bayonet_model.py, bone "bayonet", socket_bayonet_tip), every musket clip rebuilt by the musket's own
choreography (GroundReload included) plus BayonetStab and BayonetCharge (animation.py), TP and FP, 24 fps; PBR textures
baked by the musket's looks (atlases BayonetMusket_*), Unreal FBX and the manifest art/export/Weapons/BayonetMusket/
pack.json (format bean-musket-anim/1, contract in spec.py).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/bayonet_musket_pack/build.py -- [options]
    --clips BayonetStab,Idle   only these clips (quick look; the full pack has 28 clips per view)
    --views FP                 only these views (TP,FP)
    --no-bake                  procedural materials, no texture bake (quick look)
    --no-export                no FBX / manifest
    --render FP:BayonetStab:7  stills into <out>/renders/ (view:clip:frame)
    --product                  product renders
    --previews                 the control renders of the report (art/_previews/T334/bayonet_musket_*.png)
    --save                     also save art/blender/weapons/bean_bayonet_musket.blend
    --out DIR                  pack folder (default art/export/Weapons/BayonetMusket)
Check: python scripts/props/period_arms_common/check_pack.py art/export/Weapons/BayonetMusket
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
MUSKET = REPO / 'scripts/props/musket_pack'
for p in (MUSKET, COMMON, HERE):          # this pack first (spec, animation), then the common code, then the musket kit
    if str(p) in sys.path:
        sys.path.remove(str(p))
    sys.path.insert(0, str(p))

import spec as S  # noqa: E402
import contract as C  # noqa: E402
import rig as R  # noqa: E402
import pa_export as EX  # noqa: E402
import kit as K  # noqa: E402
import looks as L  # noqa: E402
import musket_model as MM  # noqa: E402
import bayonet_model as BM  # noqa: E402
import animation as AN  # noqa: E402

for _m in (S, C, R, EX, BM, AN):
    importlib.reload(_m)

def _select(objs):
    """looks._select, tolerant of the stale (None) entries the view layer keeps right after kit.join_parts deleted the
    parts (the machine gun pack patches bake.select_only the same way)."""
    vl = bpy.context.view_layer
    vl.update()
    for o in list(vl.objects):
        if o is not None:
            o.select_set(False)
    for o in objs:
        o.hide_set(False)
        o.hide_viewport = False
        o.select_set(True)
    vl.objects.active = objs[0]


L._select = _select
PACK_SPEC = HERE / 'pack.json'
PREVIEWS = REPO / 'art/_previews/T334'
BLEND = REPO / 'art/blender/weapons/bean_bayonet_musket.blend'
STATE = {}
ATLAS = {'BayonetMusket_Wood': ('M_BayonetMusket_Wood', 2048), 'BayonetMusket_Iron': ('M_BayonetMusket_Iron', 2048),
         'BayonetMusket_Brass': ('M_BayonetMusket_Brass', 2048), 'BayonetMusket_Leather': ('M_BayonetMusket_Leather', 1024),
         'BayonetMusket_Paper': ('M_BayonetMusket_Paper', 512)}


def materials():
    """The musket's looks on this pack's atlases (T_BayonetMusket_<Atlas>_*), plus the blade."""
    for aid, (final, res) in ATLAS.items():
        L.register_atlas(aid, final, res)
    L.temp_material(MM.WOOD, 'wood', 'BayonetMusket_Wood', handling=0.3)
    L.temp_material(MM.IRON, 'iron', 'BayonetMusket_Iron')
    L.temp_material(MM.DARK, 'darkiron', 'BayonetMusket_Iron')
    L.temp_material(MM.FLINT, 'flint', 'BayonetMusket_Iron')
    L.temp_material(MM.BRASS, 'brass', 'BayonetMusket_Brass')
    L.temp_material(MM.LEATHER, 'leather', 'BayonetMusket_Leather')
    L.temp_material(MM.PAPER, 'paper', 'BayonetMusket_Paper')
    L.temp_material(MM.LINEN, 'linen', 'BayonetMusket_Paper')
    BM.materials('BayonetMusket_Iron')


def build_template(coll, out, bake=True):
    MM.materials = materials          # build_musket registers the looks through this hook: ours, on our atlases
    res = MM.build_musket(coll, dict(json.loads(PACK_SPEC.read_text(encoding='utf-8')).get('musket_params', {})))
    res['parts']['bayonet'] = BM.build_bayonet(coll, MM.IRON)
    bones = dict(res['bones'])
    for name, (head, tail, parent) in S.BONES.items():
        bones[name] = (Vector(head), Vector(tail), parent)
    sockets = {k: (v[0], Vector(v[1])) for k, v in res['sockets'].items()}
    for name, (parent, pos) in S.SOCKETS.items():
        sockets[name] = (parent, Vector(pos))
    all_parts = [o for objs in res['parts'].values() for o in objs]
    physics = K.part_physics(all_parts)
    bone_names = list(bones)
    arm = R.make_skeleton('SKEL-BayonetMusket-Template', bones, sockets, coll, markers=S.MARKERS)
    mesh = K.join_parts('SK_Bean_BayonetMusket', res['parts'], coll, bone_names=bone_names)
    textures = {}
    if bake:
        t0 = time.time()
        baked = L.bake_all([mesh], Path(out) / 'textures')
        print('[bayonet] baked %d atlases in %.0f s' % (len(baked), time.time() - t0), flush=True)
        for aid, d in baked.items():
            for label, p in d.items():
                textures[f'{aid}.{label}'] = Path(p)
    mesh.parent = arm
    mod = mesh.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    proxy = R.bind_proxy(bones, coll, bone_names, 'GEO-BayonetMusket-BindProxy')
    proxy.parent = arm
    pmod = proxy.modifiers.new('Armature', 'ARMATURE')
    pmod.object = arm
    xs = [v.co for v in mesh.data.vertices]
    lo = [min(v[i] for v in xs) for i in range(3)]
    hi = [max(v[i] for v in xs) for i in range(3)]
    tris = EX.tri_count(mesh.data)
    positions = {}
    for b in arm.data.bones:
        positions[b.name] = list(b.head_local)
    contract = C.weapon_contract(S, positions)
    contract.update(bounds={'min': [round(v, 4) for v in lo], 'max': [round(v, 4) for v in hi], 'size': [round(hi[i] - lo[i], 4) for i in range(3)]},
                    triangle_count=tris, physics=physics, params=res['params'], sights=res['sights'],
                    materials=[m.name for m in mesh.data.materials], uv_layers=[u.name for u in mesh.data.uv_layers],
                    bayonet=dict(socket_x=list(S.SOCKET_X), socket_radius=list(S.SOCKET_R), blade_root=list(S.BLADE_ROOT),
                                 blade_tip=list(S.BLADE_TIP), blade_length_m=S.BLADE_LEN,
                                 note='blade under the muzzle, 2 cm to the left of the bore (clear of the ramrod channel)'))
    print(f'[bayonet] template: {tris} triangles, {len(bones)} bones + {len(sockets)} sockets, size {contract["bounds"]["size"]}, '
          f'materials {contract["materials"]}', flush=True)
    return {'armature': arm, 'meshes': [mesh], 'weapon_meshes': [mesh], 'bind_proxy': proxy, 'collection': coll,
            'contract': contract, 'bounds': contract['bounds'], 'texture_files': textures, 'bones': bones}


def build_statics(scene, template):
    coll = bpy.data.collections.new('COL-BayonetMusket-Statics')
    scene.collection.children.link(coll)
    sk = template['meshes'][0]
    statics = {}
    me = EX.mesh_of_groups(sk, ['cartridge'], Matrix.Identity(4), drop=True)
    me.name = 'SM_Bean_BayonetMusket'
    lo, hi = EX.bounds(me)
    boxes = [((-0.312, -0.030, -0.092), (0.10, 0.030, 0.090)),        # butt, wrist and lock
             ((0.10, -0.024, -0.062), (0.626, 0.024, 0.068)),         # fore-end, barrel, sling
             ((0.585, -0.022, 0.006), (S.BLADE_TIP[0] + 0.005, S.BLADE_ROOT[1] + 0.012, 0.066))]   # socket and blade
    statics['SM_Bean_BayonetMusket'] = EX.static(coll, scene, 'SM_Bean_BayonetMusket', me, hull_boxes=boxes, view='pickup',
                                                 role=S.STATICS['SM_Bean_BayonetMusket']['role'])
    STATE['statics'] = statics
    return statics


def generate(spec_json, out, clips=None, views=('TP', 'FP'), bake=True):
    MA = AN.install()
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    weapon_coll = bpy.data.collections.new('COL-Weapon-Template')
    root_scene.collection.children.link(weapon_coll)
    template = build_template(weapon_coll, out, bake)
    statics = build_statics(root_scene, template)
    src = R.source_bean(root_scene)
    records = []
    clips = list(clips) if clips else list(S.CLIPS)
    for view in views:
        for clip in clips:
            print(f'[bayonet] {view} {clip}', flush=True)
            scene = bpy.data.scenes.new(f'{S.TAG.upper()}-{view}-{clip}')
            bpy.context.window.scene = scene
            bean = R.clone_bean(src, scene, view, clip, MA.fp_arm_transform)
            weapon = R.clone_weapon(template, scene, view, clip, S.TAG)
            cam = R.stage(scene, view, clip, S.FPS, MA.camera_matrix(), MA.FP_LENS)
            AN.CURRENT['arm'] = bean['armature'] if view == 'TP' else None
            qa = MA.animate(scene, bean, weapon, clip, view)
            AN.CURRENT['arm'] = None
            ba = bpy.data.actions[qa['bean_action']]
            ga = bpy.data.actions[qa['weapon_action']]
            ba.name = C.bean_action(S, view, clip)
            ga.name = C.weapon_action(S, view, clip)
            qa['bean_action'], qa['weapon_action'] = ba.name, ga.name
            targets = qa.pop('grip_targets')
            if clip in S.NEW_CLIPS:
                qa['authored_method'] = ('T-334: bayonet gesture added to the musket animation module (same FP camera, FP arms placed '
                                         'directly, TP two-segment arms from leaned shoulders); CoD easing with an impact jolt.')
                if view == 'FP':
                    qa['bayonet_tip_in_view_cm'] = {str(fr): AN.tip_in_view(clip, fr) for fr in (0, S.NEW_EVENTS[clip][-1][0], S.NEW_CLIPS[clip])}
            records.append(dict(scene=scene.name, view=view, clip=clip, start=0, end=S.CLIPS[clip], fps=S.FPS, loop=clip in S.LOOPS,
                                camera=cam.name, camera_data=R.camera_record(cam),
                                actors=dict(bean=dict(armature=bean['armature'].name, meshes=[o.name for o in bean['meshes']], action=ba.name),
                                            weapon=dict(armature=weapon['armature'].name, meshes=[o.name for o in weapon['proxy']],
                                                        render_meshes=[o.name for o in weapon['meshes']], action=ga.name)),
                                events=qa['events'], preview_fx=[],
                                grip_contract=dict(bean_tail_bones={'l': 'grip_l', 'r': 'grip_r'},
                                                   weapon_head_bones={'l': 'socket_grip_l', 'r': 'socket_grip_r'},
                                                   release_intervals=MA.release_intervals(clip)), qa=qa))
            STATE.setdefault('targets', {})[f'{view}-{clip}'] = targets
            print('[bayonet] hand error %s, arm scale max %.3f' % (qa['hand_error_max_m'], qa['upperarm_scale_max']), flush=True)
    bpy.context.window.scene = root_scene
    root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = 'SCN-bean-bayonet-musket'
    manifest = C.manifest_skeleton(S, spec_json['version'], 'built', S.EXPORT_DIR)
    framing = MA.fp_framing()
    framing['align_bone'] = 'root'
    framing['grip_socket'] = 'socket_grip_r'
    framing['bayonet'] = {clip: AN.tip_in_view(clip, S.NEW_EVENTS[clip][-1][0]) for clip in S.NEW_CLIPS}
    framing['bayonet_note'] = 'bayonet point in the view (cm) at StabHit / ChargeHit with the pack camera (hip framing of the game = this camera)'
    manifest.update(source=dict(blend=R.SOURCE.relative_to(REPO).as_posix(), sha256=R.sha(R.SOURCE)),
                    views=list(views), scenes=records, weapon_contract=template['contract'], fp_framing=framing,
                    delivered_clips=clips, not_delivered_clips=sorted(set(S.CLIPS) - set(clips)),
                    reload_sequence={'clip': 'GroundReload', 'view_variants': list(views), 'fps': S.FPS, 'start_frame': 0,
                                     'end_frame': S.CLIPS['GroundReload'], 'duration_s': round(S.CLIPS['GroundReload'] / S.FPS, 3),
                                     'windows_frames': {k: list(v) for k, v in MA.RELOAD_WINDOWS.items()}, 'events': MA.events('GroundReload')},
                    bayonet_sequence={clip: {'frames': S.NEW_CLIPS[clip], 'duration_s': round(S.NEW_CLIPS[clip] / S.FPS, 3),
                                             'events': MA.events(clip)} for clip in S.NEW_CLIPS},
                    discard={'clip': 'Discard', 'frames': S.CLIPS['Discard'], 'events': MA.events('Discard'),
                             'release_frame': MA.RELEASE_FRAME, 'empty_frame': MA.EMPTY_FRAME})
    manifest.pop('provisional_note', None)
    STATE.update(manifest=manifest, template=template, source=src, root_scene=root_scene, statics=statics)
    return manifest


def post_build(out):
    manifest = STATE['manifest']
    manifest['assets'] = EX.export_pack(Path(out), manifest, STATE['template'], STATE['statics'], S.TAG)
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
    """The musket with its bayonet on a dark bench (Cycles): side, three quarter, the socket and the blade close."""
    scene, cam, coll = R.studio('SCN-Product-BayonetMusket')
    info = STATE['statics']['SM_Bean_BayonetMusket']
    gun = bpy.data.objects.new('PRODUCT-BayonetMusket', info['object'].data)
    coll.objects.link(gun)
    lift = max(v.co.y for v in gun.data.vertices)
    gun.matrix_world = Matrix.Translation((0, 0, lift)) @ Matrix.Rotation(math.radians(-90), 4, 'X')   # on its left side, lock up
    off = Vector(info['pivot_offset_m'])

    def g(p):
        """weapon point -> world (the static is the weapon at its pivot, turned lock up)"""
        return gun.matrix_world @ (Vector(p) - off)

    shots = [('product-weapon.png', g((0.30, -1.9, 0.0)), g((0.30, 0.0, 0.0)), 35),
             ('product-bayonet.png', g((0.80, -0.42, 0.10)), g((0.76, 0.0, 0.03)), 50),
             ('product-3-4.png', g((1.15, -0.95, 0.55)), g((0.30, 0.0, 0.02)), 40)]
    for name, loc, target, lens in shots:
        R.shoot(scene, cam, Path(out) / 'renders' / name, loc, target, lens)
        if previews:
            R.shoot(scene, cam, PREVIEWS / f'bayonet_musket_{name.replace("product-", "produit_")}', loc, target, lens)


def side_camera():
    """TP previews of the bayonet gestures: from the bean's front right, square to the barrel, so the thrust is seen side on."""
    loc = Vector((-1.75, -2.05, 0.85))          # front right of the bean, square to the thrust (barrel toward +X -Y)
    target = Vector((0.0, -0.30, 0.52))
    m = (target - loc).to_track_quat('-Z', 'Y').to_matrix().to_4x4()
    m.translation = loc
    return m


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--clips', default='')
    p.add_argument('--views', default='TP,FP')
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--no-export', action='store_true')
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
    manifest = generate(spec_json, out, clips, views, bake=not args.no_bake)
    if not args.no_export:
        post_build(out)
    if args.product or args.previews:
        product(out, args.previews)
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        R.render(scene_of(manifest, view, clip), out / 'renders' / ('%s-%s-%03d.png' % (view, clip, int(frame_))), int(frame_))
    if args.previews:
        shots = {'bayonet_musket_fp_repos.png': ('FP', 'Idle', 0), 'bayonet_musket_fp_estoc_armer.png': ('FP', 'BayonetStab', 3),
                 'bayonet_musket_fp_estoc_impact.png': ('FP', 'BayonetStab', 7), 'bayonet_musket_fp_charge.png': ('FP', 'BayonetCharge', 9),
                 'bayonet_musket_fp_charge_impact.png': ('FP', 'BayonetCharge', 16), 'bayonet_musket_fp_rechargement_poudre.png': ('FP', 'GroundReload', 58),
                 'bayonet_musket_fp_rechargement_baguette.png': ('FP', 'GroundReload', 100), 'bayonet_musket_fp_tir.png': ('FP', 'Fire', 10),
                 'bayonet_musket_tp_repos.png': ('TP', 'Hold', 0), 'bayonet_musket_tp_estoc.png': ('TP', 'BayonetStab', 7),
                 'bayonet_musket_tp_charge.png': ('TP', 'BayonetCharge', 16), 'bayonet_musket_tp_rechargement.png': ('TP', 'GroundReload', 99)}
        side = side_camera()
        for name, (view, clip, frame_) in shots.items():
            if any(r['view'] == view and r['clip'] == clip for r in manifest['scenes']):
                cam = side if (view == 'TP' and clip in S.NEW_CLIPS) else None
                R.render(scene_of(manifest, view, clip), PREVIEWS / name, frame_, camera=cam)
    if args.save:
        bpy.context.window.scene = STATE['root_scene']
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
    print('[bayonet] finished %s in %.0f s' % (out, time.time() - t0), flush=True)


if __name__ == '__main__':
    main()
