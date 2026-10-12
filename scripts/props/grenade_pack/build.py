"""Bean grenade pack, built in this repository (T-253, owner request 2026-10-09 "plus d'armes, munitions, modèles
réalistes, animations type Call of Duty"): grenado model with baked PBR textures + 14 TP/FP handling clips at 30 fps +
Unreal FBX, in the format of the second PC's pistol pack (bean-pirate-pistol/1), where
scripts/character/import_pirate_weapons.py reads the packs: build/packs/bean-grenade/
(grenade-manifest.json, exports/{Bases,Animations,Static}, textures/, renders/, fp-framing.json).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/grenade_pack/build.py -- [options]
    --clips Idle,Fire           only these clips (quick look; the full pack has 14 clips per view)
    --views FP                  only these views (TP,FP)
    --no-export                 no FBX / manifest (look only)
    --no-bake                   keep the procedural materials (no textures; quick look)
    --bake-size 2048            atlas size (default 2048)
    --render FP:Fire:14,TP:Fire:34   frames to render into <out>/renders/ (view:clip:frame)
    --product                   render the product view of the grenade (renders/product-grenade.png)
    --save                      also save <out>/bean-grenade.blend
    --out DIR                   pack folder (default build/packs/bean-grenade)

Then (editor closed, lock held): the importer entry for the grenade, see docs/features/grenade.md; check with
python scripts/props/grenade_pack/check_pack.py.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import common as C  # noqa: E402
import grenade_model as G  # noqa: E402
import animation as A  # noqa: E402

SOURCE = REPO / 'art/blender/characters/bean_soldier.blend'
PACK_SPEC = HERE / 'pack.json'
STATE = {}


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding='utf-8')


def active(obj):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def add_grips(arm, meshes):
    """grip_l / grip_r: non deforming bones at the centre of the body mittens (hand targets)."""
    centers = {}
    for side in ('l', 'r'):
        ob = next(o for o in meshes if o.name == 'Bean_Hand_' + side)
        centers[side] = sum((ob.matrix_world @ Vector(c) for c in ob.bound_box), Vector()) / 8
    active(arm)
    bpy.ops.object.mode_set(mode='EDIT')
    for side in ('l', 'r'):
        b = arm.data.edit_bones.new('grip_' + side)
        b.head = centers[side] + Vector((0, 0, .004))
        b.tail = centers[side]
        b.parent = arm.data.edit_bones['hand_' + side]
        b.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')


def source_bean(scene):
    with bpy.data.libraries.load(str(SOURCE), link=False) as (src, dst):
        dst.collections = ['BeanSoldier']
    coll = dst.collections[0]
    scene.collection.children.link(coll)
    arm = next(o for o in coll.all_objects if o.type == 'ARMATURE')
    meshes = [o for o in coll.all_objects if o.type == 'MESH' and any(m.type == 'ARMATURE' and m.object == arm for m in o.modifiers)]
    add_grips(arm, meshes)
    fp = [o for o in coll.all_objects if o.type == 'MESH' and o.name.startswith('FPArm_')]
    if not fp:
        raise SystemExit(f'{SOURCE} has no FPArm_* parts: run scripts/character/bean_soldier.py -- --export first')
    return dict(armature=arm, meshes=meshes, fp_meshes=fp, collection=coll)


def clone_arm(source, coll, name):
    arm = source.copy()
    arm.data = source.data.copy()
    arm.name = name
    coll.objects.link(arm)
    arm.animation_data_clear()
    arm.parent = None
    arm.matrix_world = Matrix.Identity(4)
    arm.hide_viewport = False
    arm.hide_render = False
    for p in arm.pose.bones:
        for c in list(p.constraints):
            p.constraints.remove(c)
        p.rotation_mode = 'QUATERNION'
        p.matrix_basis = Matrix.Identity(4)
    return arm


def clone_meshes(source, arm, coll, prefix):
    result = []
    for o in source:
        ob = o.copy()
        ob.name = 'GEO-' + prefix + '-' + o.name
        ob.animation_data_clear()
        coll.objects.link(ob)
        ob.parent = arm
        ob.matrix_parent_inverse = Matrix.Identity(4)
        ob.matrix_basis = o.matrix_basis.copy()
        ob.hide_render = False
        ob.hide_viewport = False
        for m in ob.modifiers:
            if m.type == 'ARMATURE':
                m.object = arm
        result.append(ob)
    return result


def clone_bean(source, scene, view, clip):
    coll = bpy.data.collections.new('COL-Bean-' + view + '-' + clip)
    scene.collection.children.link(coll)
    arm = clone_arm(source['armature'], coll, 'SKEL-Bean-' + view + '-' + clip)
    if view == 'TP':
        result = clone_meshes(source['meshes'], arm, coll, view + '-' + clip)
    else:
        # the FP arms: the FPArm parts of the bean, rigid on hand_<side>, placed in the rest pose along the forearm
        # with the mitten centre on grip_<side> (animation.fp_arm_transform)
        result = []
        for side in ('l', 'r'):
            T = A.fp_arm_transform(arm, side)
            for src in source['fp_meshes']:
                me = src.data.copy()
                me.transform(T @ src.matrix_world)
                if T.determinant() < 0:
                    me.flip_normals()
                ob = bpy.data.objects.new('GEO-FP-' + clip + '-' + side + '-' + src.name, me)
                coll.objects.link(ob)
                ob.vertex_groups.new(name='hand_' + side).add(list(range(len(me.vertices))), 1., 'REPLACE')
                mod = ob.modifiers.new('Bean hand rig', 'ARMATURE')
                mod.object = arm
                ob.parent = arm
                ob['source_mesh'] = src.name
                ob['fp_source'] = 'art/blender/characters/bean_soldier.blend'
                result.append(ob)
    return dict(armature=arm, meshes=result, collection=coll)


def clone_weapon(source, scene, view, clip):
    coll = bpy.data.collections.new('COL-Grenade-' + view + '-' + clip)
    scene.collection.children.link(coll)
    arm = clone_arm(source['armature'], coll, 'SKEL-Grenade-' + view + '-' + clip)
    meshes = clone_meshes(source['meshes'], arm, coll, view + '-' + clip)
    return dict(armature=arm, meshes=meshes, collection=coll)


def material(name, color, rough=.5, metal=0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    return m


def stage(scene, view, clip):
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.length_unit = 'METERS'
    scene.unit_settings.scale_length = 1
    try:
        scene.render.engine = 'BLENDER_EEVEE'
    except TypeError:
        scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1280, 720, 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.fps = A.FPS
    scene.world = bpy.data.worlds.new('World-' + view + '-' + clip)
    scene.world.use_nodes = True
    w = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    w.inputs['Color'].default_value = (.32, .42, .55, 1)
    w.inputs['Strength'].default_value = .9
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    coll = bpy.data.collections.new('COL-Stage-' + view + '-' + clip)
    scene.collection.children.link(coll)
    me = bpy.data.meshes.new('REF-Floor')
    me.from_pydata([(-200, -200, 0), (200, -200, 0), (200, 200, 0), (-200, 200, 0)], [], [(0, 1, 2, 3)])
    ob = bpy.data.objects.new('REF-Floor-' + view + '-' + clip, me)
    coll.objects.link(ob)
    ob.data.materials.append(material('MAT-Stage', (.62, .55, .42), .9))   # coral sand
    sun = bpy.data.lights.new('LGT-Sun-' + view + '-' + clip, 'SUN')
    sun.energy = 3.2
    sun.angle = math.radians(3)
    light = bpy.data.objects.new(sun.name, sun)
    coll.objects.link(light)
    light.rotation_euler = (math.radians(50), math.radians(10), math.radians(150))
    data = bpy.data.cameras.new('CAM-' + view + '-' + clip)
    cam = bpy.data.objects.new(data.name, data)
    coll.objects.link(cam)
    if view == 'FP':
        cam.matrix_world = A.camera_matrix()
        data.type = 'PERSP'
        data.sensor_fit = 'HORIZONTAL'
        data.sensor_width = 36.0
        data.lens = A.FP_LENS
        data.clip_start = .01
    else:
        cam.location = (1.58, -2.1, 1.17)
        target = Vector((-.03, -.10, .55))
        data.type = 'ORTHO'
        data.ortho_scale = 2.4
        cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    return cam


def render(scene, out, name, frame, res=960):
    bpy.context.window.scene = scene
    scene.frame_set(frame)
    scene.render.resolution_x = res
    scene.render.resolution_y = int(res * 9 / 16)
    try:
        scene.eevee.taa_render_samples = 16
    except AttributeError:
        pass
    scene.render.filepath = str(Path(out) / 'renders' / name)
    scene.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True, scene=scene.name)


def camera_record(cam):
    return dict(type=cam.data.type, matrix_world=[list(row) for row in cam.matrix_world], location_m=list(cam.location),
                rotation_euler_deg=[math.degrees(v) for v in cam.matrix_world.to_euler()], lens_mm=cam.data.lens,
                horizontal_fov_degrees=math.degrees(cam.data.angle_x), sensor_width_mm=cam.data.sensor_width,
                ortho_scale_m=cam.data.ortho_scale, clip_start_m=cam.data.clip_start, view_local_axis='-Z', up_local_axis='+Y')


def bake(root_scene, built, out, size, ao_samples):
    """Bakes the grenade atlas in its own scene (self occlusion only) and leaves the baked material on the mesh."""
    scene = bpy.data.scenes.new('SCN-Bake')
    scene.collection.children.link(built['collection'])
    scene.unit_settings.system = 'METRIC'
    paths = C.bake_atlas(scene, built['object'], 'PirateGrenade', size, Path(out) / 'textures', ao_samples=ao_samples)
    scene.collection.children.unlink(built['collection'])
    bpy.context.window.scene = root_scene
    return {k: str(Path(v).relative_to(Path(out)).as_posix()) for k, v in paths.items()}


def generate(spec, out, clips=None, views=('TP', 'FP'), do_bake=True, bake_size=2048, ao_samples=48):
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    src = source_bean(root_scene)
    weapon_coll = bpy.data.collections.new('COL-Weapon-Template')
    root_scene.collection.children.link(weapon_coll)
    built = G.build_skeletal_grenade(weapon_coll)
    built['collection'] = weapon_coll
    textures = bake(root_scene, built, out, bake_size, ao_samples) if do_bake else {}
    bounds = C.world_bounds([built['object']])
    template = {'armature': built['armature'], 'meshes': [built['object']], 'object': built['object'], 'collection': weapon_coll,
                'contract': {'forward_axis': '+X', 'up_axis': '+Z', 'primary_hand': 'right',
                             'bones': list(built['bones']) + ['socket_' + x for x in built['sockets']],
                             'socket_positions': {'socket_' + k: [round(x, 4) for x in v[1]] for k, v in built['sockets'].items()},
                             'socket_parents': {'socket_' + k: v[0] for k, v in built['sockets'].items()},
                             'bounds': bounds, 'ball_radius_m': G.R_BALL, 'ball_centre_m': [*G.BALL], 'fuse_tip_m': [*G.FUSE_TIP],
                             'physics': built['physics'], 'triangle_count': C.triangle_count([built['object']]),
                             'animation_only_bones': ['match', 'bag'], 'hidden_when_unloaded': ['body', 'fuse'],
                             'textures': textures, 'material': built['object'].data.materials[0].name},
                'bounds': bounds, 'weapon_meshes': [built['object']]}
    records = []
    clips = list(clips) if clips else list(A.CLIPS)
    for view in views:
        for clip in clips:
            print('[grenade] ' + view + ' ' + clip, flush=True)
            scene = bpy.data.scenes.new('GRENADE-' + view + '-' + clip)
            bpy.context.window.scene = scene
            bean = clone_bean(src, scene, view, clip)
            weapon = clone_weapon(template, scene, view, clip)
            cam = stage(scene, view, clip)
            qa = A.animate(scene, bean, weapon, clip, view)
            targets = qa.pop('grip_targets')
            records.append(dict(scene=scene.name, view=view, clip=clip, start=0, end=A.CLIPS[clip], fps=A.FPS, camera=cam.name,
                                camera_data=camera_record(cam),
                                actors=dict(bean=dict(armature=bean['armature'].name, meshes=[o.name for o in bean['meshes']],
                                                      action=qa['bean_action']),
                                            weapon=dict(armature=weapon['armature'].name, meshes=[o.name for o in weapon['meshes']],
                                                        action=qa['weapon_action'])),
                                events=qa['events'], preview_fx=[],
                                grip_contract=dict(bean_tail_bones={'l': 'grip_l', 'r': 'grip_r'}, weapon_head_bones={'r': 'socket_grip_r'},
                                                   right_grip_always=True, left_hand='carries the slow match (weapon bone "match")',
                                                   release_intervals=[]), qa=qa))
            STATE.setdefault('targets', {})['%s-%s' % (view, clip)] = targets
            print('[grenade] hand error %s, arm scale max %.3f' % (qa['hand_error_max_m'], qa['upperarm_scale_max']), flush=True)
    bpy.context.window.scene = root_scene
    root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = 'SCN-bean-grenade'
    manifest = dict(format='bean-grenade/1', version=spec['version'], fps=A.FPS, local_build=True,
                    built_by='scripts/props/grenade_pack/build.py (T-253)',
                    source=dict(blend=SOURCE.relative_to(REPO).as_posix(), sha256=sha(SOURCE)),
                    views=list(views), scenes=records, weapon_contract=template['contract'], assets=[],
                    requested_clips=list(A.CLIPS), delivered_clips=clips, not_delivered_clips=sorted(set(A.CLIPS) - set(clips)),
                    loops=sorted(A.LOOPS), fp_framing=A.fp_framing(),
                    gesture_notes={
                        'Fire': 'short throw from the hip: the ball is brought to the slow match (Light, frame %d), wound back, '
                                'thrown (Fire, frame %d: body and fuse collapse, spawn the projectile at socket_body)' % (A.FIRE_LIGHT, A.FIRE_RELEASE),
                        'AimFire': 'long overhand throw from the cocked pose: Light frame %d, Fire frame %d' % (A.AIMFIRE_LIGHT, A.AIMFIRE_RELEASE),
                        'Reload': 'the next grenade out of the bag: HammerCocked (frame %d) = the grenade is in the hand (body shown from there)' % A.RELOAD_LOADED,
                        'DryFire': 'taps the empty bag (DryClick)', 'Discard': 'no grenades left: the empty bag is turned over, shaken, '
                        'tossed aside (Release frame %d: hide bone bag, drop the pickup at socket_bag); the clip ends hands empty' % A.DISCARD_RELEASE},
                    notes=['The grenade is held and thrown by the right hand; the left hand carries the slow match (weapon bone "match").',
                           'Weapon gun_root is baked in bean coordinates: place both actor roots at the same transform.',
                           'Body and fuse bones are scaled to 0.0001 whenever no grenade is in the hand (after Fire, before HammerCocked, '
                           'in DryFire and Discard); the bag bone is scaled to 0.0001 outside Discard frames %d-%d.' % (A.DISCARD_SHOW, A.DISCARD_RELEASE - 1),
                           'The Fire / AimFire clips must play from frame 0 (AFirearmWeapon::FireClipStart starts them at the Fire event: '
                           'AGrenadeWeapon overrides it to 0 so the lighting is seen).',
                           'First person: the base pose is framed by HipGripLocation / HipRotation of fp_framing (or the pack camera with '
                           'bAlignFirstPersonToView off).',
                           'No Unreal runtime validation has been performed.'])
    STATE.update(manifest=manifest, template=template, source=src, root_scene=root_scene)
    return manifest


def post_build(out):
    manifest = STATE['manifest']
    from export_pack import export_pack
    manifest['assets'] = export_pack(Path(out), manifest, STATE['source'], STATE['template'])
    write(Path(out) / 'grenade-manifest.json', manifest)
    write(Path(out) / 'fp-framing.json', manifest['fp_framing'])
    for name, targets in STATE.get('targets', {}).items():
        write(Path(out) / 'validation' / 'grips' / (name + '.json'), targets)
    bpy.context.window.scene = STATE['root_scene']


def product_render(out):
    """Product view of the grenade (rest pose, no match, no bag) in a studio."""
    template = STATE['template']
    scene = C.studio_scene('SCN-Product-Grenade', Vector((0, 0, 0.075)), 0.12)
    bpy.context.window.scene = scene
    from export_pack import pickup_objects
    ob, ucx, pivot = pickup_objects(template, scene)
    bpy.data.objects.remove(ucx, do_unlink=True)
    ob.rotation_euler = (0, 0, math.radians(-30))
    C.render_scene(scene, Path(out) / 'renders' / 'product-grenade.png')
    bpy.context.window.scene = STATE['root_scene']


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--clips', default='')
    p.add_argument('--views', default='TP,FP')
    p.add_argument('--no-export', action='store_true')
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--bake-size', type=int, default=2048)
    p.add_argument('--ao-samples', type=int, default=48)
    p.add_argument('--render', default='')
    p.add_argument('--product', action='store_true')
    p.add_argument('--save', action='store_true')
    p.add_argument('--out', default=str(REPO / 'build' / 'packs' / 'bean-grenade'))
    args = p.parse_args(argv)
    spec = json.loads(PACK_SPEC.read_text(encoding='utf8'))
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    clips = [c for c in args.clips.split(',') if c] or None
    views = tuple(v for v in args.views.split(',') if v)
    out = Path(args.out)
    if not out.is_absolute():
        out = REPO / out          # Blender's working directory is not the repository
    out.mkdir(parents=True, exist_ok=True)
    manifest = generate(spec, out, clips, views, do_bake=not args.no_bake, bake_size=args.bake_size, ao_samples=args.ao_samples)
    if not args.no_export:
        post_build(out)
    if args.product:
        product_render(out)
    if args.save:
        bpy.ops.wm.save_as_mainfile(filepath=str(out / 'bean-grenade.blend'), compress=True)
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        rec = next(r for r in manifest['scenes'] if r['view'] == view and r['clip'] == clip)
        render(bpy.data.scenes[rec['scene']], out, '%s-%s-%03d.png' % (view, clip, int(frame_)), int(frame_))
    print('[grenade] finished ' + str(out), flush=True)


if __name__ == '__main__':
    main()
