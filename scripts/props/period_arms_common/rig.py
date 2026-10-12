"""Blender side of the T-334 packs: the bean (source, clones for the TP / FP scenes), the clip stage, the control renders
and the product renders. Adapted from scripts/props/musket_pack/build.py and scripts/props/machinegun_pack/build.py (the
same bean, the same FPArm first person arms, the same cameras), with the clip rate of the pack (30 fps for the rifle
and the sub-machine gun, 24 for the bayonet musket) instead of the musket's.
"""
from __future__ import annotations

import hashlib
import json
import math
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

REPO = Path(__file__).resolve().parents[3]
SOURCE = REPO / 'art/blender/characters/bean_soldier.blend'
PREVIEWS = REPO / 'art/_previews'


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(data, indent=2, ensure_ascii=False)
    for attempt in range(6):                    # Malwarebytes may lock a file just written: retry
        try:
            path.write_text(text, encoding='utf-8')
            return
        except PermissionError:
            import time
            time.sleep(1.5 * (attempt + 1))
    path.write_text(text, encoding='utf-8')


def active(obj):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


# --------------------------------------------------------------------------------------------- the bean

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


def clone_bean(source, scene, view, clip, fp_arm_transform):
    """The bean of a clip scene: the whole bean (TP) or the two FPArm parts rigid on hand_l / hand_r (FP)."""
    coll = bpy.data.collections.new('COL-Bean-' + view + '-' + clip)
    scene.collection.children.link(coll)
    arm = clone_arm(source['armature'], coll, 'SKEL-Bean-' + view + '-' + clip)
    if view == 'TP':
        result = clone_meshes(source['meshes'], arm, coll, view + '-' + clip)
    else:
        result = []
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


def clone_weapon(template, scene, view, clip, tag):
    """The weapon of a clip: its armature, the full mesh (renders) and the light bind proxy (animation FBX)."""
    coll = bpy.data.collections.new(f'COL-{tag}-{view}-{clip}')
    scene.collection.children.link(coll)
    arm = clone_arm(template['armature'], coll, f'SKEL-{tag}-{view}-{clip}')
    meshes = clone_meshes(template['meshes'], arm, coll, view + '-' + clip)
    proxy = clone_meshes([template['bind_proxy']], arm, coll, view + '-' + clip) if template.get('bind_proxy') else []
    for o in proxy:
        o.hide_render = True
    return dict(armature=arm, meshes=meshes, proxy=proxy, collection=coll)


def bind_proxy(bones, coll, bone_names, name):
    """Light bind mesh of the animation FBX: a tiny tetrahedron on every deforming bone (keeps each bone's bind pose in
    the FBX without the weapon's triangles), as the machine gun pack does."""
    import bmesh
    bm = bmesh.new()
    deform = bm.verts.layers.deform.verify()
    for i, bname in enumerate(bone_names):
        head = Vector(bones[bname][0])
        s = 0.003
        vs = [bm.verts.new(head + Vector(d) * s) for d in ((1, 1, 1), (1, -1, -1), (-1, 1, -1), (-1, -1, 1))]
        for v in vs:
            v[deform][i] = 1.0
        for tri in ((0, 1, 2), (0, 3, 1), (0, 2, 3), (1, 3, 2)):
            bm.faces.new([vs[t] for t in tri])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    for bname in bone_names:
        ob.vertex_groups.new(name=bname)
    ob.hide_render = True
    return ob


def make_skeleton(name, bones, sockets, coll, markers=None):
    """Rigid deforming bones + non deforming marker bones (grip, muzzle) + socket_* bones (Unreal sockets, hand targets);
    a socket may give its tail (its axis)."""
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    coll.objects.link(arm)
    arm.show_in_front = True
    active(arm)
    bpy.ops.object.mode_set(mode='EDIT')
    for bname, (head, tail, parent) in bones.items():
        eb = data.edit_bones.new(bname)
        eb.head, eb.tail = Vector(head), Vector(tail)
        eb.roll = 0.0
        if parent:
            eb.parent = data.edit_bones[parent]
    for mname, (parent, pos) in (markers or {}).items():
        eb = data.edit_bones.new(mname)
        eb.head, eb.tail = Vector(pos), Vector(pos) + Vector((0.03, 0, 0))
        eb.parent = data.edit_bones[parent]
        eb.use_deform = False
    for sname, spec in sockets.items():
        parent, pos = spec[0], Vector(spec[1])
        tail = Vector(spec[2]) if len(spec) > 2 else pos + Vector((0.01, 0, 0))
        eb = data.edit_bones.new('socket_' + sname)
        eb.head, eb.tail = pos, tail
        eb.parent = data.edit_bones[parent]
        eb.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm


# --------------------------------------------------------------------------------------------- stage and renders

def material(name, color, rough=.5, metal=0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.diffuse_color = (*color, 1)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    p.inputs['Base Color'].default_value = (*color, 1)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    return m


def set_engine(scene, engine):
    try:
        scene.render.engine = engine
    except TypeError:
        scene.render.engine = 'BLENDER_EEVEE_NEXT' if engine == 'BLENDER_EEVEE' else engine


def stage(scene, view, clip, fps, camera_matrix, lens):
    """Clip scene: metres, EEVEE, the pack's rate, a coral sand floor, a sun, the FP camera of the game (90 deg) or the
    TP orthographic camera of the weapon packs."""
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.length_unit = 'METERS'
    scene.unit_settings.scale_length = 1
    set_engine(scene, 'BLENDER_EEVEE')
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1280, 720, 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.fps = fps
    scene.render.fps_base = 1.0
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
    ob.data.materials.append(material('MAT-Stage', (.62, .55, .42), .9))
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
        cam.matrix_world = camera_matrix
        data.type = 'PERSP'
        data.sensor_fit = 'HORIZONTAL'
        data.sensor_width = 36.0
        data.lens = lens
        data.clip_start = .05                   # the game's near plane (DefaultEngine.ini NearClipPlane=5)
    else:
        cam.location = (1.58, -2.1, 1.17)
        target = Vector((-.03, -.10, .55))
        data.type = 'ORTHO'
        data.ortho_scale = 2.4
        cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    return cam


def look_at(loc, target):
    """Camera matrix at loc looking at target (Blender cameras look along their -Z, +Y up)."""
    m = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_matrix().to_4x4()
    m.translation = Vector(loc)
    return m


def camera_record(cam):
    return dict(type=cam.data.type, matrix_world=[list(row) for row in cam.matrix_world], location_m=list(cam.location),
                rotation_euler_deg=[math.degrees(v) for v in cam.matrix_world.to_euler()], lens_mm=cam.data.lens,
                horizontal_fov_degrees=math.degrees(cam.data.angle_x), sensor_width_mm=cam.data.sensor_width,
                ortho_scale_m=cam.data.ortho_scale, clip_start_m=cam.data.clip_start, view_local_axis='-Z', up_local_axis='+Y')


def render(scene, out_path, frame, res=1280, extra=None, camera=None, lens=None, samples=32):
    """A still of a clip scene at frame; extra(scene) -> [objects] adds render-only effects (flash, case in flight);
    camera (Matrix) / lens override the scene camera for this still (the scope view)."""
    bpy.context.window.scene = scene
    scene.frame_set(frame)
    scene.render.resolution_x = res
    scene.render.resolution_y = int(res * 9 / 16)
    try:
        scene.eevee.taa_render_samples = samples
    except AttributeError:
        pass
    fx = extra(scene) if extra else []
    saved = None
    if camera is not None:
        saved = (scene.camera.matrix_world.copy(), scene.camera.data.lens, scene.camera.data.clip_start)
        scene.camera.matrix_world = camera
        if lens:
            scene.camera.data.lens = lens
    out_path = Path(out_path)
    out_path.parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(out_path)
    scene.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True, scene=scene.name)
    if saved:
        scene.camera.matrix_world, scene.camera.data.lens, scene.camera.data.clip_start = saved
    for o in fx:
        bpy.data.objects.remove(o, do_unlink=True)
    print('[pa] rendered ' + str(out_path), flush=True)


def flash_objects(scene, gun_prefix, size=1.0):
    """Render-only muzzle flash at socket_muzzle (the game's is a Niagara system + a light): an emissive star of crossed
    blades, a light."""
    import bmesh
    gun = next(o for o in scene.objects if o.type == 'ARMATURE' and o.name.startswith(gun_prefix))
    pb = gun.pose.bones['socket_muzzle']
    root = gun.pose.bones['root']
    m = gun.matrix_world @ pb.matrix
    axis = (gun.matrix_world @ root.matrix).to_3x3() @ (root.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0)))
    axis.normalize()
    mat = bpy.data.materials.get('FX-Flash') or bpy.data.materials.new('FX-Flash')
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = (1.0, 0.62, 0.22, 1)
    em.inputs['Strength'].default_value = 60.0
    nt.links.new(em.outputs[0], out.inputs[0])
    bm = bmesh.new()
    up = Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))
    side = axis.cross(up).normalized()
    up = side.cross(axis).normalized()
    origin = m.translation
    for k in range(4):
        a = math.pi * k / 4
        d = side * math.cos(a) + up * math.sin(a)
        verts = [bm.verts.new(origin + d * 0.006 * size), bm.verts.new(origin + axis * 0.26 * size), bm.verts.new(origin - d * 0.006 * size),
                 bm.verts.new(origin + axis * 0.08 * size + d * 0.07 * size), bm.verts.new(origin + axis * 0.08 * size - d * 0.07 * size)]
        bm.faces.new((verts[0], verts[3], verts[1]))
        bm.faces.new((verts[2], verts[1], verts[4]))
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=0.05 * size, matrix=Matrix.Translation(origin + axis * 0.04 * size))
    me = bpy.data.meshes.new('FX-FlashMesh')
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new('FX-Flash', me)
    scene.collection.objects.link(ob)
    ld = bpy.data.lights.new('FX-FlashLight', 'POINT')
    ld.energy = 400.0 * size
    ld.color = (1.0, 0.65, 0.3)
    ld.shadow_soft_size = 0.05
    lo = bpy.data.objects.new('FX-FlashLight', ld)
    scene.collection.objects.link(lo)
    lo.location = origin + axis * 0.05
    return [ob, lo]


def case_object(scene, gun_prefix, case_mesh, t=0.06, speed=(0.0, -2.2, 1.6)):
    """Render-only spent case in flight from socket_eject toward socket_eject_dir (t seconds after the ejection)."""
    gun = next(o for o in scene.objects if o.type == 'ARMATURE' and o.name.startswith(gun_prefix))
    ej = (gun.matrix_world @ gun.pose.bones['socket_eject'].matrix).translation
    dr = (gun.matrix_world @ gun.pose.bones['socket_eject_dir'].matrix).translation
    d = (dr - ej).normalized()
    p = ej + d * 2.4 * t + Vector((0, 0, -4.9 * t * t))
    c = bpy.data.objects.new('FX-Case', case_mesh)
    scene.collection.objects.link(c)
    c.matrix_world = Matrix.Translation(p) @ Matrix.Rotation(0.9, 4, 'Z') @ Matrix.Rotation(0.5, 4, 'X')
    return [c]


def studio(name, engine='CYCLES', samples=96, res=(1600, 900), dark=True):
    """Product scene: a bench, four area lights, a camera (Cycles by default, GPU when there is one)."""
    scene = bpy.data.scenes.new(name)
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    set_engine(scene, engine)
    if engine == 'CYCLES':
        try:
            import pa_bake
            dev = pa_bake.gpu()
        except Exception:  # noqa: BLE001
            dev = None
        scene.cycles.device = 'GPU' if dev else 'CPU'
        scene.cycles.samples = samples
        scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = res[0], res[1], 100
    scene.render.image_settings.file_format = 'PNG'
    try:
        scene.view_settings.view_transform = 'AgX'
        scene.view_settings.look = 'AgX - Medium High Contrast'
    except TypeError:
        pass
    scene.world = bpy.data.worlds.new('World-' + name)
    scene.world.use_nodes = True
    w = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    w.inputs['Color'].default_value = (.33, .37, .43, 1)
    w.inputs['Strength'].default_value = .45
    coll = bpy.data.collections.new('COL-' + name)
    scene.collection.children.link(coll)
    me = bpy.data.meshes.new('REF-Bench')
    me.from_pydata([(-4, -4, 0), (4, -4, 0), (4, 4, 0), (-4, 4, 0)], [], [(0, 1, 2, 3)])
    bench = bpy.data.objects.new('REF-Bench-' + name, me)
    coll.objects.link(bench)
    bench.data.materials.append(material('MAT-Bench', (.060, .050, .040) if dark else (.30, .28, .25), .72))
    for lname, loc, power, size, color in (('Key', (1.0, 1.1, 1.4), 90, 1.6, (1, .93, .84)), ('Fill', (-1.2, -0.9, 0.9), 28, 2.2, (.85, .9, 1)),
                                           ('Rim', (-0.6, 1.3, 1.1), 70, 1.0, (1, .95, .9)), ('Side', (0.6, -1.4, 0.8), 35, 1.4, (1, 1, 1))):
        data = bpy.data.lights.new(f'LGT-{name}-{lname}', 'AREA')
        data.energy = power
        data.size = size
        data.color = color
        light = bpy.data.objects.new(data.name, data)
        coll.objects.link(light)
        light.location = loc
        light.rotation_euler = (Vector((0.3, 0, 0.2)) - light.location).to_track_quat('-Z', 'Y').to_euler()
    data = bpy.data.cameras.new('CAM-' + name)
    cam = bpy.data.objects.new(data.name, data)
    coll.objects.link(cam)
    scene.camera = cam
    return scene, cam, coll


def shoot(scene, cam, path, loc, target, lens=50):
    cam.location = loc
    cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
    cam.data.lens = lens
    cam.data.clip_start = 0.01
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True, scene=scene.name)
    print('[pa] rendered ' + str(path), flush=True)
