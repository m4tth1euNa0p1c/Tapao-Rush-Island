"""Bean musket pack, built in this repository (T-212, v2 T-246): musket model + TP/FP handling clips + Unreal FBX.

Local copy of the second PC's bean-musket-anim v0.2 pipeline (OneDrive maquettes, read only), owner's choice
2026-10-07 ("Ici, dans ce dépôt"). Writes the pack where scripts/character/import_pirate_weapons.py reads it:
build/packs/bean-musket-anim/ (musket-manifest.json, exports/{Bases,Animations,Static}, textures/, renders/,
fp-framing.json).

v2 (2026-10-09, owner: "améliorer le design de toutes les armes, animations de rechargement et de tir type Call of
Duty, modèles de munitions, armes plus réalistes"): Brown Bess style musket with baked PBR textures (looks.py), four
ammunition static meshes (ammo_model.py), CoD-style clips (animation.py: anticipation, overshoot, sharp recoil,
5.5 s ground reload, Inspect, Discard), product and control renders.

  blender -b --factory-startup --python scripts/props/musket_pack/build.py -- [options]
    --clips Idle,GroundReload   only these clips (quick look; the full pack has 26 clips per view)
    --views FP                  only these views (TP,FP)
    --model-only                musket + ammunition + bake + product renders, no clip (look-dev)
    --no-bake                   keep the procedural materials (fast look at the geometry; no textures in the pack)
    --no-export                 no FBX / manifest (look only)
    --render FP:GroundReload:30,TP:Fire:8,FPAIM:AimIdle:0   frames to render into <out>/renders/ (view:clip:frame;
                                FPAIM = the first person camera placed as the game's aim framing, sights on the axis)
    --product                   product renders of the musket and the ammunition (neutral background)
    --save                      also save <out>/bean-musket-anim.blend
    --out DIR                   pack folder (default build/packs/bean-musket-anim)

Then, editor closed and lock held: MONKE_PIRATE_WEAPONS=Musket commandlet import_pirate_weapons.py, then
setup_pirate_weapons.py (see their docstrings) and HipGripLocation / HipRotation from fp-framing.json.
"""
from __future__ import annotations

import argparse
import hashlib
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
sys.path.insert(0, str(HERE))
import animation as A  # noqa: E402
import kit as K  # noqa: E402
import looks as L  # noqa: E402
import musket_model  # noqa: E402
import ammo_model  # noqa: E402

for _m in (K, L, musket_model, ammo_model, A):
    importlib.reload(_m)

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
        # the FP arms: the FPArm parts of the bean (sleeve, cuff, lace, buttons, wrist, mitten), rigid on hand_<side>,
        # placed in the rest pose along the forearm with the mitten centre on grip_<side> (animation.fp_arm_transform)
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
    coll = bpy.data.collections.new('COL-Musket-' + view + '-' + clip)
    scene.collection.children.link(coll)
    arm = clone_arm(source['armature'], coll, 'SKEL-Musket-' + view + '-' + clip)
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


def set_engine(scene, name):
    try:
        scene.render.engine = name
    except TypeError:
        scene.render.engine = 'BLENDER_EEVEE_NEXT' if name == 'BLENDER_EEVEE' else name


def stage(scene, view, clip):
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.length_unit = 'METERS'
    scene.unit_settings.scale_length = 1
    set_engine(scene, 'BLENDER_EEVEE')
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
        # the game's first person camera: 90 deg horizontal, looking straight ahead
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


def render(scene, out, name, frame, res=960, aim=False):
    bpy.context.window.scene = scene
    scene.frame_set(frame)
    scene.render.resolution_x = res
    scene.render.resolution_y = int(res * 9 / 16)
    try:
        scene.eevee.taa_render_samples = 24
    except AttributeError:
        pass
    saved = None
    if aim:
        # the game's aim framing: the camera on the sight line, muzzle AIM_MUZZLE_CM ahead, aim field of view
        weapon = next(o for o in scene.objects if o.type == 'ARMATURE' and o.name.startswith('SKEL-Musket'))
        pb = weapon.pose.bones['root']
        # the weapon's own frame (+X muzzle, +Z up), not the bone's (Y along the bone)
        root = weapon.matrix_world @ pb.matrix @ pb.bone.matrix_local.inverted()
        eye = root @ A.aim_eye_in_weapon()
        cam = scene.camera
        saved = (cam.matrix_world.copy(), cam.data.lens, cam.data.clip_start)
        m = Matrix((root.col[1].xyz.normalized() * -1.0, root.col[2].xyz.normalized(), -root.col[0].xyz.normalized())).transposed().to_4x4()
        m.translation = eye
        cam.matrix_world = m
        cam.data.lens = 18.0 / math.tan(math.radians(A.AIM_FOV_DEG / 2))
        cam.data.clip_start = 0.10          # Unreal's near plane: the stock behind the eye is clipped
    scene.render.filepath = str(Path(out) / 'renders' / name)
    scene.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True, scene=scene.name)
    if saved:
        scene.camera.matrix_world, scene.camera.data.lens, scene.camera.data.clip_start = saved


def camera_record(cam):
    return dict(type=cam.data.type, matrix_world=[list(row) for row in cam.matrix_world], location_m=list(cam.location),
                rotation_euler_deg=[math.degrees(v) for v in cam.matrix_world.to_euler()], lens_mm=cam.data.lens,
                horizontal_fov_degrees=math.degrees(cam.data.angle_x), sensor_width_mm=cam.data.sensor_width,
                ortho_scale_m=cam.data.ortho_scale, clip_start_m=cam.data.clip_start, view_local_axis='-Z', up_local_axis='+Y')


def mesh_stats(ob):
    dg = bpy.context.evaluated_depsgraph_get()
    me = ob.evaluated_get(dg).to_mesh()
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    out = dict(triangles=tris, vertices=len(me.vertices),
               bounds=dict(min=[round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)], max=[round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)],
                           size=[round(max(xs) - min(xs), 4), round(max(ys) - min(ys), 4), round(max(zs) - min(zs), 4)]))
    ob.evaluated_get(dg).to_mesh_clear()
    return out


def product_scene(template, ammo):
    """Neutral studio: the musket (rest pose, static copy) and the ammunition in front of it."""
    scene = bpy.data.scenes.new('SCN-Product')
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    set_engine(scene, 'BLENDER_EEVEE')
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1600, 900, 100
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    scene.world = bpy.data.worlds.new('World-Product')
    scene.world.use_nodes = True
    w = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    w.inputs['Color'].default_value = (.30, .30, .32, 1)
    w.inputs['Strength'].default_value = 0.6
    coll = bpy.data.collections.new('COL-Product')
    scene.collection.children.link(coll)
    me = bpy.data.meshes.new('REF-ProductFloor')
    me.from_pydata([(-5, -5, 0), (5, -5, 0), (5, 5, 0), (-5, 5, 0)], [], [(0, 1, 2, 3)])
    floor = bpy.data.objects.new('REF-ProductFloor', me)
    coll.objects.link(floor)
    floor.data.materials.append(material('MAT-ProductFloor', (.36, .36, .37), .8))
    dg = bpy.context.evaluated_depsgraph_get()
    objects = []
    for ob in template['weapon_meshes']:
        copy = bpy.data.meshes.new_from_object(ob.evaluated_get(dg), depsgraph=dg, preserve_all_data_layers=True)
        gun = bpy.data.objects.new('GEO-Product-Musket', copy)
        coll.objects.link(gun)
        # lying on its left side, lock up, muzzle toward +X
        gun.matrix_world = Matrix.Translation((0.0, 0.0, 0.026)) @ Matrix.Rotation(math.radians(-90), 4, 'X')
        objects.append(gun)
    places = {'SM_Ammo_CartridgeBox': (0.02, -0.17, 0), 'SM_Ammo_PowderHorn': (-0.20, -0.17, 0),
              'SM_Ammo_MusketCartridge': (0.20, -0.15, 0), 'SM_Ammo_LeadBall': (0.26, -0.19, 0)}
    for name, ob in ammo.items():
        copy = ob.copy()
        copy.name = 'GEO-Product-' + name
        coll.objects.link(copy)
        copy.location = places.get(name, (0, -0.2, 0))
        if name == 'SM_Ammo_PowderHorn':
            copy.rotation_euler = (0, 0, math.radians(35))
        objects.append(copy)
    for name, loc, energy, size in (('Key', (0.6, -0.9, 1.1), 60, 0.9), ('Fill', (-1.0, -0.6, 0.7), 22, 1.6), ('Rim', (0.2, 0.9, 0.9), 35, 0.8)):
        data = bpy.data.lights.new('LGT-Product-' + name, 'AREA')
        data.energy = energy
        data.size = size
        light = bpy.data.objects.new(data.name, data)
        coll.objects.link(light)
        light.location = loc
        light.rotation_euler = (Vector((0.0, -0.08, 0.03)) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    data = bpy.data.cameras.new('CAM-Product')
    cam = bpy.data.objects.new(data.name, data)
    coll.objects.link(cam)
    data.lens = 50
    scene.camera = cam
    return scene, cam


def product_render(scene, cam, out, name, location, target, lens=50, res=1600):
    cam.location = location
    cam.data.lens = lens
    cam.rotation_euler = (Vector(target) - Vector(location)).to_track_quat('-Z', 'Y').to_euler()
    scene.render.resolution_x = res
    scene.render.resolution_y = int(res * 9 / 16)
    try:
        scene.eevee.taa_render_samples = 48
    except AttributeError:
        pass
    scene.render.filepath = str(Path(out) / 'renders' / name)
    scene.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True, scene=scene.name)


def build_assets(spec, out, bake=True):
    """Musket template (skeletal) and ammunition, baked; everything else clones them."""
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    weapon_coll = bpy.data.collections.new('COL-Weapon-Template')
    root_scene.collection.children.link(weapon_coll)
    built = K.build_skeletal_musket(musket_model.build_musket, weapon_coll, dict(spec.get('musket_params', {})))
    ammo_coll = bpy.data.collections.new('COL-Ammo-Template')
    root_scene.collection.children.link(ammo_coll)
    ammo = ammo_model.build_all(ammo_coll)
    textures = {}
    if bake:
        t0 = time.time()
        textures = L.bake_all([built['object']] + list(ammo.values()), Path(out) / 'textures')
        print('[musket] baked %d atlases in %.0f s' % (len(textures), time.time() - t0), flush=True)
    stats = mesh_stats(built['object'])
    template = {'armature': built['armature'], 'meshes': [built['object']], 'collection': weapon_coll,
                'contract': {'bones': list(built['bones']) + ['socket_' + x for x in built['sockets']],
                             'sockets': {k: list(v[1]) for k, v in built['sockets'].items()}, 'params': built['params'],
                             'bounds': stats['bounds'], 'triangles': stats['triangles'], 'physics': built['physics'],
                             'sights': built['sights']},
                'bounds': stats['bounds'], 'weapon_meshes': [built['object']]}
    STATE.update(template=template, ammo=ammo, ammo_collection=ammo_coll, textures=textures, root_scene=root_scene)
    return template, ammo


def generate(spec, out, clips=None, views=('TP', 'FP'), bake=True):
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    src = source_bean(root_scene)
    template, ammo = build_assets(spec, out, bake)
    records = []
    clips = list(clips) if clips else list(A.CLIPS)
    for view in views:
        for clip in clips:
            print('[musket] ' + view + ' ' + clip, flush=True)
            scene = bpy.data.scenes.new('MUSKET-' + view + '-' + clip)
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
                                grip_contract=dict(bean_tail_bones={'l': 'grip_l', 'r': 'grip_r'},
                                                   weapon_head_bones={'l': 'socket_grip_l', 'r': 'socket_grip_r'},
                                                   release_intervals=A.release_intervals(clip)), qa=qa))
            STATE.setdefault('targets', {})['%s-%s' % (view, clip)] = targets
            print('[musket] hand error %s, arm scale max %.3f' % (qa['hand_error_max_m'], qa['upperarm_scale_max']), flush=True)
    bpy.context.window.scene = root_scene
    root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(template['collection'])
    root_scene.collection.children.unlink(STATE['ammo_collection'])
    root_scene.name = 'SCN-bean-musket-anim'
    reload_end = A.CLIPS['GroundReload']
    manifest = dict(format='bean-musket-anim/1', version=spec['version'], fps=A.FPS, local_build=True,
                    built_by='scripts/props/musket_pack/build.py (T-212, v2 T-246)',
                    source=dict(blend=SOURCE.relative_to(REPO).as_posix(), sha256=sha(SOURCE)),
                    views=list(views), scenes=records, weapon_contract=template['contract'], assets=[],
                    requested_clips=list(A.CLIPS), delivered_clips=clips,
                    not_delivered_clips=sorted(set(A.CLIPS) - set(clips)),
                    fp_framing=A.fp_framing(),
                    reload_sequence={'clip': 'GroundReload', 'view_variants': list(views), 'fps': A.FPS, 'start_frame': 0,
                                     'end_frame': reload_end, 'duration_s': round(reload_end / A.FPS, 3), 'windows_frames': A.RELOAD_WINDOWS,
                                     'events': A.events('GroundReload')},
                    textures={aid: {label: Path(p).relative_to(Path(out)).as_posix() for label, p in d.items()} for aid, d in STATE['textures'].items()},
                    notes=['v2 (T-246): Brown Bess style musket with baked PBR textures (textures/: BaseColor sRGB, Normal DirectX '
                           'green down, ORM = R occlusion / G roughness / B metallic, linear); same bones, sockets and rest lines as '
                           'T-212, so the clips, the game sockets and the hip framing stay valid.',
                           'Ammunition static meshes in exports/Static (assets view "ammo": pivot bottom centre, UCX_<Name>_00 '
                           'convex collision in the FBX, their own baked atlas).',
                           'Clips: CoD-style anticipation / overshoot / settle, sharp recoil (+7 deg, 3.8 cm back over 2 frames, '
                           'back in 10), GroundReload shortened to %d frames (%.2f s) with every phase clip still delivered, new '
                           'Inspect (lock then barrel) and Discard (empty click, shake, thrown right and down: events Empty, '
                           'Release) clips. Discard needs EFirearmGesture::Discard before PirateWeaponClips.inl is regenerated.'
                           % (reload_end, reload_end / A.FPS),
                           'First person: one base weapon pose for every non reload clip; the game frames it with '
                           'HipGripLocation / HipRotation (fp_framing.hip_*) and AimMuzzleLocation / AimRotation (fp_framing.aim_*: '
                           'the sight line, rear notch to front sight, on the camera axis; written for DA_PirateMusketRules).',
                           'Weapon skeleton keeps the bone "cartridge" (paper cartridge, hidden inside the butt at rest).',
                           'Weapon root is baked in bean coordinates: both actor roots at the same transform.'])
    STATE.update(manifest=manifest, source=src)
    return manifest


def post_build(out):
    manifest = STATE['manifest']
    from export_pack import export_pack
    manifest['assets'] = export_pack(Path(out), manifest, STATE['source'], STATE['template'], STATE['ammo'])
    write(Path(out) / 'musket-manifest.json', manifest)
    write(Path(out) / 'fp-framing.json', manifest['fp_framing'])
    for name, targets in STATE.get('targets', {}).items():
        write(Path(out) / 'validation' / 'grips' / (name + '.json'), targets)
    bpy.context.window.scene = STATE['root_scene']


def product_renders(out):
    scene, cam = product_scene(STATE['template'], STATE['ammo'])
    # the musket lies lock up: the view from above is its classic side view; the lock sits at y ~ +0.05
    product_render(scene, cam, out, 'product-musket.png', (0.16, -0.02, 1.42), (0.16, 0.0, 0.0), lens=50)
    product_render(scene, cam, out, 'product-musket-lock.png', (0.11, -0.16, 0.30), (0.055, 0.045, 0.05), lens=75)
    product_render(scene, cam, out, 'product-musket-3-4.png', (0.60, -0.70, 0.55), (0.12, 0.0, 0.03), lens=50)
    product_render(scene, cam, out, 'product-ammo.png', (0.05, -0.55, 0.30), (0.03, -0.17, 0.03), lens=60)
    bpy.context.window.scene = STATE['root_scene']


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--clips', default='')
    p.add_argument('--views', default='TP,FP')
    p.add_argument('--model-only', action='store_true')
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--no-export', action='store_true')
    p.add_argument('--render', default='')
    p.add_argument('--product', action='store_true')
    p.add_argument('--save', action='store_true')
    p.add_argument('--out', default=str(REPO / 'build' / 'packs' / 'bean-musket-anim'))
    args = p.parse_args(argv)
    spec = json.loads(PACK_SPEC.read_text(encoding='utf8'))
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    clips = [c for c in args.clips.split(',') if c] or None
    views = tuple(v for v in args.views.split(',') if v)
    out = Path(args.out).resolve()        # Blender resolves relative paths against the drive root, never the cwd
    out.mkdir(parents=True, exist_ok=True)
    t0 = time.time()
    if args.model_only:
        build_assets(spec, out, bake=not args.no_bake)
        product_renders(out)
        if args.save:
            bpy.ops.wm.save_as_mainfile(filepath=str(out / 'bean-musket-model.blend'), compress=True)
        print('[musket] model only, %.0f s' % (time.time() - t0), flush=True)
        return
    manifest = generate(spec, out, clips, views, bake=not args.no_bake)
    if not args.no_export:
        post_build(out)
    if args.product:
        product_renders(out)
    if args.save:
        bpy.ops.wm.save_as_mainfile(filepath=str(out / 'bean-musket-anim.blend'), compress=True)
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        rec = next(r for r in manifest['scenes'] if r['view'] == ('FP' if view == 'FPAIM' else view) and r['clip'] == clip)
        render(bpy.data.scenes[rec['scene']], out, '%s-%s-%03d.png' % (view, clip, int(frame_)), int(frame_), aim=(view == 'FPAIM'))
    print('[musket] finished %s in %.0f s' % (out, time.time() - t0), flush=True)


if __name__ == '__main__':
    main()
