"""Bean naval pistol packs, built in this repository (T-248): realistic flintlock model with baked PBR textures,
TP/FP handling clips (CoD style) and Unreal FBX, single and double barrel.

Local counterpart of the second PC's bean-pirate-pistol v0.1 pipeline (OneDrive maquettes, read only) and of the
musket pack (scripts/props/musket_pack, T-212). Writes the pack where scripts/character/import_pirate_weapons.py
reads it: build/packs/bean-pirate-pistol/ (pistol-manifest.json, exports/{Bases,Animations,Static}, textures/,
renders/, fp-framing.json) and build/packs/bean-pirate-pistol-double/ (pistol-double-manifest.json...).

  blender -b --factory-startup --python scripts/props/pistol_pack/build.py -- [options]
    --variant single|double     which pack (default single)
    --clips Idle,Reload         only these clips (quick look; the full pack has 15 clips per view)
    --views FP                  only these views (TP,FP)
    --no-export                 no FBX / manifest (look only)
    --no-bake                   keep the procedural Cycles materials (look-dev only; the exports need the bake)
    --size 2048                 atlas size
    --render FP:Reload:30,TP:Idle:0   frames to render into <out>/renders/ (view:clip:frame)
    --save                      also save <out>/<pack>.blend
    --out DIR                   pack folder (default build/packs/<pack>)
"""
from __future__ import annotations

import argparse
import hashlib
import json
import math
import sys
import time
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import animation as A  # noqa: E402
import materials as MT  # noqa: E402
import meshkit as K  # noqa: E402
import pistol_model as PM  # noqa: E402

SOURCE = REPO / 'art/blender/characters/bean_soldier.blend'
SPEC = json.loads((HERE / 'pack.json').read_text(encoding='utf8'))
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


# ------------------------------------------------------------------------------------------------ the bean

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


def belt_pouch(bean, mats):
    """Small leather ball pouch at the front left hip of the TP bean, where the left hand takes the cartridge."""
    b = K.Builder(bean['collection'], 'BeltPouch', mats)
    c = Vector((.205, -.185, .340))
    bag = [((c.x, c.y, c.z - .030), .024, .020, 0), ((c.x, c.y, c.z - .012), .032, .026, 0), ((c.x, c.y, c.z + .010), .030, .024, 0),
           ((c.x, c.y, c.z + .026), .024, .019, 0), ((c.x, c.y, c.z + .034), .026, .020, 0)]
    b.loft('Bag', bag, 'leather', 'pelvis', seg=20, expo=2.0, frame=((0, 1, 0), (-1, 0, 0)))
    ring = [(c.x + .025 * math.cos(j * K.TAU / 20), c.y + .020 * math.sin(j * K.TAU / 20), c.z + .028) for j in range(21)]
    b.tube('Cord', ring, .0018, 'cord', 'pelvis', seg=6, caps=False)
    K.pack_islands(b.meshes, b.islands, 256)
    ob = K.combine(b.meshes, bean['collection'], 'GEO-BeltPouch')
    K.skin(ob, bean['armature'], 'pelvis')
    bean['meshes'].append(ob)


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
        # the FP arms: the FPArm parts of the bean (sleeve, cuff, lace, buttons, wrist, mitten), rigid on hand_<side>
        result = []
        from rig_solver import fp_arm_transform
        for side in ('l', 'r'):
            T = fp_arm_transform(arm, side)
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


def clone_weapon(template, scene, view, clip, weapon_name):
    coll = bpy.data.collections.new('COL-%s-%s-%s' % (weapon_name, view, clip))
    scene.collection.children.link(coll)
    arm = clone_arm(template['armature'], coll, 'SKEL-%s-%s-%s' % (weapon_name, view, clip))
    meshes = clone_meshes(template['meshes'], arm, coll, view + '-' + clip)
    return dict(armature=arm, meshes=meshes, collection=coll)


# ------------------------------------------------------------------------------------------------ the stage

def material(name, color, rough=.5, metal=0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = K.principled(m)
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


def presentation_flash(scene, weapon, clip, view, variant):
    """Muzzle flame for the preview renders only (REF object, never exported)."""
    if clip not in ('Fire', 'AimFire'):
        return []
    y = PM.BARREL_Y[variant][1 if (clip == 'AimFire' and variant == 'double') else 0]
    verts, faces = [], []
    seg, ring = 16, 8
    for i in range(ring + 1):
        phi = math.pi * i / ring
        for j in range(seg + 1):
            th = K.TAU * j / seg
            verts.append((.10 * math.cos(phi) + .085, .030 * math.sin(phi) * math.cos(th), .030 * math.sin(phi) * math.sin(th)))
    for i in range(ring):
        for j in range(seg):
            a = i * (seg + 1) + j
            faces.append((a, a + 1, a + seg + 2, a + seg + 1))
    me = bpy.data.meshes.new('FX-MuzzleMesh')
    me.from_pydata(verts, [], faces)
    ob = bpy.data.objects.new('REF-MuzzleFlash-%s-%s' % (view, clip), me)
    scene.collection.objects.link(ob)
    mat = material('MAT-Pistol-PreviewFlash', (1, .3, .02), .4)
    p = K.principled(mat)
    p.inputs['Emission Color'].default_value = (1, .45, .05, 1)
    p.inputs['Emission Strength'].default_value = 12
    me.materials.append(mat)
    ob.rotation_mode = 'QUATERNION'
    for f in range(A.CLIPS[clip] + 1):
        M = A.weapon_pose(clip, f, view)
        M.translation = M @ Vector((PM.MUZZLE_X, y, PM.BORE_Z))
        ob.matrix_world = M
        s = 1.0 if f == 2 else .7 if f == 3 else .3 if f == 4 else .000001
        ob.scale = (s, s * 1.1, s * 1.1)
        for prop in ('location', 'rotation_quaternion', 'scale'):
            ob.keyframe_insert(prop, frame=f)
    from rig_solver import linear
    linear(ob.animation_data.action)
    return [ob.name]


def render(scene, out, name, frame, res=960):
    bpy.context.window.scene = scene
    scene.frame_set(frame)
    scene.render.resolution_x = res
    scene.render.resolution_y = int(res * 9 / 16)
    try:
        scene.eevee.taa_render_samples = 24
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


def camera_transform_v01(cam):
    """The v0.1 manifest field, kept for setup_pirate_weapons.fp_view fallbacks."""
    return {'location': list(cam.location), 'rotation_degrees': [math.degrees(x) for x in cam.rotation_euler], 'type': cam.data.type,
            'lens_mm': cam.data.lens, 'ortho_scale': cam.data.ortho_scale, 'clip_start_m': cam.data.clip_start, 'clip_end_m': cam.data.clip_end}


def left_assist_intervals(clip):
    if clip == 'Reload':
        return [[6, 135]]
    if clip == 'Inspect':
        return [[52, 140]]
    return []


# ------------------------------------------------------------------------------------------------- build

def generate(variant_key, clips=None, views=('TP', 'FP'), bake=True, size=2048, out=None):
    variant = SPEC['variants'][variant_key]
    weapon_name = variant['weapon']
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    mats = MT.create_materials('Pistol')
    src = source_bean(root_scene)
    belt_pouch(src, mats)
    weapon_coll = bpy.data.collections.new('COL-Weapon-Template')
    root_scene.collection.children.link(weapon_coll)
    t0 = time.time()
    template = PM.create_weapon(weapon_coll, mats, weapon_name, variant_key)
    PM.finalize(template, size)
    print('[pistol] %s model: %d tris, bounds %s (%.1fs)' % (variant_key, template['contract']['triangle_count'],
                                                             [round(v, 4) for v in template['bounds']['size']], time.time() - t0), flush=True)
    ammo = None
    textures = {}
    if variant_key == 'single':
        ammo_coll = bpy.data.collections.new('COL-Ammo-Template')
        root_scene.collection.children.link(ammo_coll)
        ammo = PM.create_ammo(ammo_coll, mats, SPEC['bake']['ammo_size'])
    if bake:
        MT.cycles_gpu(root_scene)
        bk = SPEC['bake']
        paths = MT.bake_atlas(root_scene, template['meshes'], size, out / 'textures', variant['texture'], bk['color_samples'], bk['ao_samples'],
                              bk['margin'])
        MT.apply_baked(template['meshes'], MT.baked_material(variant['material'], paths))
        textures[variant['material']] = {k: str(v.relative_to(out)) for k, v in paths.items()}
        if ammo:
            objs = [v[0] for v in ammo.values()]
            paths = MT.bake_atlas(root_scene, objs, bk['ammo_size'], out / 'textures', 'T_Ammo_Pistol', bk['color_samples'], bk['ao_samples'], 8)
            MT.apply_baked(objs, MT.baked_material('M_Ammo_Pistol', paths))
            textures['M_Ammo_Pistol'] = {k: str(v.relative_to(out)) for k, v in paths.items()}
    if ammo:
        root_scene.collection.children.unlink(ammo_coll)
    records = []
    clips = list(clips) if clips else list(A.CLIPS)
    for view in views:
        for clip in clips:
            t1 = time.time()
            scene = bpy.data.scenes.new('PISTOL-' + view + '-' + clip)
            bpy.context.window.scene = scene
            bean = clone_bean(src, scene, view, clip)
            weapon = clone_weapon(template, scene, view, clip, weapon_name)
            cam = stage(scene, view, clip)
            qa = A.animate(scene, bean, weapon, clip, view, variant_key)
            fx = presentation_flash(scene, weapon, clip, view, variant_key)
            targets = qa.pop('grip_targets')
            records.append(dict(scene=scene.name, view=view, clip=clip, start=0, end=A.CLIPS[clip], fps=A.FPS, camera=cam.name,
                                camera_data=camera_record(cam), camera_transform=camera_transform_v01(cam),
                                actors=dict(bean=dict(armature=bean['armature'].name, meshes=[o.name for o in bean['meshes']],
                                                      action=qa['bean_action'], location=[0, 0, 0], rotation_degrees=[0, 0, 0], scale=[1, 1, 1]),
                                            weapon=dict(armature=weapon['armature'].name, meshes=[o.name for o in weapon['meshes']],
                                                        action=qa['weapon_action'], location=[0, 0, 0], rotation_degrees=[0, 0, 0], scale=[1, 1, 1])),
                                events=qa['events'], preview_fx=fx,
                                grip_contract=dict(bean_tail_bones={'r': 'grip_r'}, weapon_head_bones={'r': 'socket_grip_r'},
                                                   right_grip_always=clip != 'Discard', left_hand='free except Reload, Inspect',
                                                   left_assist_intervals=left_assist_intervals(clip),
                                                   release_intervals=[[0, A.CLIPS[clip]]] if clip != 'Discard' else [[0, 24]],
                                                   right_release_frame=24 if clip == 'Discard' else None),
                                qa=qa))
            STATE.setdefault('targets', {})['%s-%s' % (view, clip)] = targets
            print('[pistol] %s %s: hand error %s, arm scale max %.3f (%.1fs)' % (view, clip, {k: round(v, 6) for k, v in qa['hand_error_max_m'].items()},
                                                                                 qa['upperarm_scale_max'], time.time() - t1), flush=True)
    bpy.context.window.scene = root_scene
    root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = 'SCN-' + variant['pack']
    contract = dict(template['contract'])
    manifest = dict(format=variant['format'], version=SPEC['version'], fps=A.FPS, local_build=True, variant=variant_key,
                    built_by='scripts/props/pistol_pack/build.py (T-248)',
                    source=dict(blend=SOURCE.relative_to(REPO).as_posix(), sha256=sha(SOURCE)),
                    views=list(views), scenes=records, weapon_contract=contract, assets=[],
                    requested_clips=list(A.CLIPS), delivered_clips=clips, not_delivered_clips=sorted(set(A.CLIPS) - set(clips)),
                    v01_clips=A.V01_CLIPS, new_clips=sorted(set(A.CLIPS) - set(A.V01_CLIPS)), loop_clips=sorted(A.LOOPS),
                    fp_framing=A.fp_framing(), baked_textures=textures,
                    notes=['Pistol held and fired by the right hand; the left hand is below the FP frame except in Reload and Inspect '
                           '(it is never collapsed by scale any more).',
                           'First person authored through the game camera (0, 0, 0.80 m, 90 deg): HipGripLocation / HipRotation and '
                           'AimMuzzleLocation / AimRotation of fp_framing reproduce it (sight line on the view axis when aiming).',
                           'Fire / AimFire / DryFire / Discard finish with the cock down and the frizzen open; Cock or Reload resets them.',
                           'Double pistol: Fire drops the right cock (hammer, socket_muzzle), AimFire the left one (hammer2, socket_muzzle2); '
                           'the ramrod loads the right barrel (socket_load_port).',
                           'Discard (owner 2026-10-09): Empty at 0, the pistol leaves the right hand at Release (24), the weapon clip '
                           'continues the fall to frame 36 (FP: out of the frame; TP: on the ground at the right foot); the bean ends empty-handed.',
                           'Muzzle flash is a preview object only (REF-MuzzleFlash-*), never exported.',
                           'Weapon gun_root is baked in bean coordinates: place both actor roots at the same transform.',
                           'No Unreal runtime validation has been performed.'])
    STATE.update(manifest=manifest, template=template, source=src, root_scene=root_scene, variant=variant, ammo=ammo)
    return manifest


def post_build(out):
    manifest = STATE['manifest']
    from export_pack import export_pack
    variant = STATE['variant']
    manifest['assets'] = export_pack(Path(out), manifest, variant, STATE['source'], STATE['template'], STATE['ammo'])
    write(Path(out) / variant['manifest'], manifest)
    write(Path(out) / 'fp-framing.json', manifest['fp_framing'])
    for name, targets in STATE.get('targets', {}).items():
        write(Path(out) / 'validation' / 'grips' / (name + '.json'), targets)
    bpy.context.window.scene = STATE['root_scene']


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--variant', default='single')
    p.add_argument('--clips', default='')
    p.add_argument('--views', default='TP,FP')
    p.add_argument('--no-export', action='store_true')
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--size', type=int, default=SPEC['bake']['size'])
    p.add_argument('--render', default='')
    p.add_argument('--save', action='store_true')
    p.add_argument('--out', default='')
    args = p.parse_args(argv)
    variant = SPEC['variants'][args.variant]
    out = Path(args.out) if args.out else REPO / 'build' / 'packs' / variant['pack']
    out.mkdir(parents=True, exist_ok=True)
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    clips = [c for c in args.clips.split(',') if c] or None
    views = tuple(v for v in args.views.split(',') if v)
    t0 = time.time()
    manifest = generate(args.variant, clips, views, not args.no_bake, args.size, out)
    if not args.no_export:
        post_build(out)
    if args.save:
        bpy.ops.wm.save_as_mainfile(filepath=str(out / (variant['pack'] + '.blend')), compress=True)
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        rec = next(r for r in manifest['scenes'] if r['view'] == view and r['clip'] == clip)
        render(bpy.data.scenes[rec['scene']], out, '%s-%s-%03d.png' % (view, clip, int(frame_)), int(frame_))
    print('[pistol] finished %s in %.0fs' % (out, time.time() - t0), flush=True)


if __name__ == '__main__':
    main()
