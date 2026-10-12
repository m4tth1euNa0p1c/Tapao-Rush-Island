"""Bean machine gun pack (T-272, owner request 2026-10-10: "mitrailleuse lourde à manivelle type 1918, rechargement,
course, douilles éjectées, canon qui chauffe, recul... ultra réaliste type Call of Duty"): the hand-cranked heavy
machine gun Mle 1918 (machinegun_model.py), PBR textures baked from procedural materials (2K body and jacket, 1K belt
and box, heat glow mask of the barrel), the bean's 13 handling clips in third (TP) and first person (FP) at 30 fps
(animation.py), statics (pickup, ammunition box, spent case, belt link), Unreal FBX and a manifest in the format of the
weapon packs (bean-musket-anim/1), where scripts/character/import_pirate_weapons.py reads them (registry entry
"MachineGun"): build/packs/bean-machinegun/ (machinegun-manifest.json, exports/{Bases,Animations,Static}, textures/,
renders/, fp-framing.json, validation/grips/).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup --python scripts/props/machinegun_pack/build.py -- [options]
    --model-only            weapon, textures and statics (no gesture)
    --clips FireLoop,Idle   only these clips (quick look; the full pack has 13 clips per view)
    --views FP              only these views (TP,FP)
    --no-export             no FBX / manifest (look only)
    --no-bake               flat procedural materials, no texture bake (quick look of the gestures)
    --render FP:FireLoop:1,TP:Reload:60   stills into <out>/renders/ (view:clip:frame)
    --product               product renders (Cycles) into <out>/renders/
    --previews              the control renders art/_previews/machinegun_{trois_quarts,manivelle,fp_repos,fp_tir,...}.png
    --save                  also save <out>/bean-machinegun.blend
    --out DIR               pack folder (default build/packs/bean-machinegun)
Then: python scripts/props/machinegun_pack/check_pack.py, and the import (Unreal, lead action "mgimport":
scripts/props/machinegun_pack/import_machinegun_ue.py).
"""
from __future__ import annotations

import argparse
import importlib
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
# this pack first (its animation / build / export_pack shadow the musket pack's), then the long arm bake / materials,
# then the musket pack's kit
for p in (REPO / 'scripts/props/musket_pack', REPO / 'scripts/props/longarm_common', HERE):
    if str(p) in sys.path:
        sys.path.remove(str(p))
    sys.path.insert(0, str(p))

import kit as K  # noqa: E402
import bake as BAKE  # noqa: E402
import mg_materials as MM  # noqa: E402
import machinegun_model as MG  # noqa: E402
import export_pack as EX  # noqa: E402

for _m in (MM, MG, EX):
    importlib.reload(_m)


def _select_only(ob):
    """bake.select_only, tolerant of the stale (None) entries the view layer keeps right after kit.join_parts deleted
    the parts."""
    vl = bpy.context.view_layer
    vl.update()
    for o in list(vl.objects):
        if o is not None:
            o.select_set(False)
    ob.hide_set(False)
    ob.hide_viewport = False
    ob.select_set(True)
    vl.objects.active = ob


BAKE.select_only = _select_only

SOURCE = REPO / 'art/blender/characters/bean_soldier.blend'
PACK_SPEC = HERE / 'pack.json'
PREVIEWS = REPO / 'art/_previews'
STATE = {}
BAKE_SIZES = {'Body': 2048, 'Jacket': 2048, 'Belt': 1024, 'Box': 1024}


def write(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(data, indent=2, ensure_ascii=False), encoding='utf-8')


def active(obj):
    for o in bpy.context.selected_objects:
        o.select_set(False)
    obj.select_set(True)
    bpy.context.view_layer.objects.active = obj


def A():
    import animation
    importlib.reload(animation)
    return animation


# --------------------------------------------------------------------------------------------- the bean (source)

def add_grips(arm, meshes):
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
        result = []
        for side in ('l', 'r'):
            Tm = A().fp_arm_transform(arm, side)
            for src in source['fp_meshes']:
                me = src.data.copy()
                me.transform(Tm @ src.matrix_world)
                if Tm.determinant() < 0:
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


def clone_weapon(template, scene, view, clip, bind_proxy=True):
    """The gun of a clip: its armature, the full mesh for the renders and the light bind proxy for the FBX."""
    coll = bpy.data.collections.new('COL-MachineGun-' + view + '-' + clip)
    scene.collection.children.link(coll)
    arm = clone_arm(template['armature'], coll, 'SKEL-MachineGun-' + view + '-' + clip)
    meshes = clone_meshes(template['meshes'], arm, coll, view + '-' + clip)
    proxy = clone_meshes([template['bind_proxy']], arm, coll, view + '-' + clip) if bind_proxy else []
    for o in proxy:
        o.hide_render = True
    return dict(armature=arm, meshes=meshes, proxy=proxy, collection=coll)


# --------------------------------------------------------------------------------------------- the weapon template

def make_skeleton(name, bones, sockets, coll):
    """Rigid bones + non deforming socket_* bones; a socket may give its tail (its axis: socket_eject)."""
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    coll.objects.link(arm)
    arm.show_in_front = True
    active(arm)
    bpy.ops.object.mode_set(mode='EDIT')
    for bname, (head, tail, parent) in bones.items():
        eb = data.edit_bones.new(bname)
        eb.head, eb.tail = head, tail
        eb.roll = 0.0
        if parent:
            eb.parent = data.edit_bones[parent]
    for sname, spec in sockets.items():
        parent, pos = spec[0], Vector(spec[1])
        tail = Vector(spec[2]) if len(spec) > 2 else pos + Vector((0.01, 0, 0))
        eb = data.edit_bones.new('socket_' + sname)
        eb.head, eb.tail = pos, tail
        eb.parent = data.edit_bones[parent]
        eb.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm


def bake_group(scene, ob, name, folder, size, heat=False, color_samples=8, ao_samples=48, ao_distance=0.03):
    """bake.py's bake_object with the extra 'heat' channel: T_<name>_{D,N,ORM}.png (+ _Heat.png at 1024)."""
    folder = Path(folder)
    BAKE.unwrap(ob, margin=0.003 if size >= 2048 else 0.005)
    device = BAKE.gpu()
    scene.cycles.device = 'GPU' if device else 'CPU'
    print(f'[bake] {name}: {size} px, {len(ob.data.polygons)} faces, {device or "CPU"}', flush=True)
    color = BAKE.new_image(f'T_{name}_D', size, True)
    BAKE.bake_channel(scene, ob, color, 'color', color_samples)
    normal = BAKE.new_image(f'T_{name}_N', size, False)
    BAKE.bake_channel(scene, ob, normal, 'NORMAL', color_samples)
    rough = BAKE.new_image(f'T_{name}_R', size, False)
    BAKE.bake_channel(scene, ob, rough, 'ROUGHNESS', color_samples)
    metal = BAKE.new_image(f'T_{name}_M', size, False)
    BAKE.bake_channel(scene, ob, metal, 'metal', color_samples)
    ao = BAKE.new_image(f'T_{name}_AO', size, False)
    BAKE.bake_channel(scene, ob, ao, 'AO', ao_samples, ao_distance)
    orm = BAKE.new_image(f'T_{name}_ORM', size, False)
    BAKE.pack_orm(ao, rough, metal, orm)
    paths = {'BaseColor': BAKE.save(color, folder / f'T_{name}_D.png'), 'Normal': BAKE.save(normal, folder / f'T_{name}_N.png'),
             'ORM': BAKE.save(orm, folder / f'T_{name}_ORM.png')}
    heat_img = None
    if heat:
        heat_img = BAKE.new_image(f'T_{name}_Heat', 1024, True)
        BAKE.bake_channel(scene, ob, heat_img, 'heat', 4)
        paths['Heat'] = BAKE.save(heat_img, folder / f'T_{name}_Heat.png')
    for img in (rough, metal, ao):
        bpy.data.images.remove(img)
    for slot in ob.material_slots:
        node = slot.material.node_tree.nodes.get('_bake')
        if node:
            slot.material.node_tree.nodes.remove(node)
    mat = BAKE.textured_material(f'M_{name}', {'BaseColor': color, 'Normal': normal, 'ORM': orm})
    if heat_img is not None:
        node = mat.node_tree.nodes.new('ShaderNodeTexImage')
        node.label = node.name = 'Heat'
        node.image = heat_img
        node.location = (-300, -600)
        mat['heat_note'] = 'Heat: glow mask of the hot barrel (not wired here; Emissive in Unreal, EmissiveStrength driven by the game)'
    me = ob.data
    me.materials.clear()
    me.materials.append(mat)
    for poly in me.polygons:
        poly.material_index = 0
    return paths


def flat_group(ob, name):
    """--no-bake: keep the procedural materials, only give the group a unique UV layout."""
    BAKE.unwrap(ob, margin=0.004)
    return {}


def bind_proxy(bones, coll, bone_names):
    """Light bind mesh of the animation FBX: a tiny tetrahedron on every deforming bone (keeps each bone's bind pose
    in the FBX without the 60k triangles of the gun)."""
    import bmesh
    bm = bmesh.new()
    deform = bm.verts.layers.deform.verify()
    for i, name in enumerate(bone_names):
        head = Vector(bones[name][0])
        s = 0.003
        vs = [bm.verts.new(head + Vector(d) * s) for d in ((1, 1, 1), (1, -1, -1), (-1, 1, -1), (-1, -1, 1))]
        for v in vs:
            v[deform][i] = 1.0
        for tri in ((0, 1, 2), (0, 3, 1), (0, 2, 3), (1, 3, 2)):
            bm.faces.new([vs[t] for t in tri])
    me = bpy.data.meshes.new('GEO-MachineGun-BindProxy')
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new('GEO-MachineGun-BindProxy', me)
    coll.objects.link(ob)
    for name in bone_names:
        ob.vertex_groups.new(name=name)
    ob.hide_render = True
    return ob


def build_template(coll, spec, out, bake=True):
    scene = bpy.context.scene
    res = MG.build_machinegun(coll)
    bone_names = list(res['bones'])
    all_parts = [o for objs in res['parts'].values() for o in objs]
    res['physics'] = K.part_physics(all_parts)
    groups = []
    textures = {}
    for gname, bones in res['groups'].items():
        parts = {b: res['parts'][b] for b in bones if res['parts'].get(b)}
        ob = K.join_parts(f'GEO-MG-{gname}', parts, coll, bone_names=bone_names)
        if bake:
            paths = bake_group(scene, ob, f'MG_{gname}', Path(out) / 'textures', BAKE_SIZES[gname], heat=(gname == 'Jacket'))
            textures.update({f'{gname}.{k}': v for k, v in paths.items()})
        else:
            flat_group(ob, gname)
        groups.append(ob)
    for cutter in res['cutters']:
        data = cutter.data
        bpy.data.objects.remove(cutter, do_unlink=True)
        if data.users == 0:
            bpy.data.meshes.remove(data)
    # one skinned mesh, four materials (M_MG_Body, M_MG_Jacket, M_MG_Belt, M_MG_Box)
    bpy.context.view_layer.update()
    for o in list(bpy.context.view_layer.objects):
        if o is not None:
            o.select_set(False)
    for o in groups:
        o.select_set(True)
    bpy.context.view_layer.objects.active = groups[0]
    bpy.ops.object.join()
    mesh = bpy.context.view_layer.objects.active
    mesh.name = 'SK_Bean_MachineGun'
    mesh.data.name = 'SK_Bean_MachineGun'
    arm = make_skeleton('SKEL-MachineGun-Template', res['bones'], res['sockets'], coll)
    mesh.parent = arm
    mod = mesh.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    proxy = bind_proxy(res['bones'], coll, bone_names)
    proxy.parent = arm
    pmod = proxy.modifiers.new('Armature', 'ARMATURE')
    pmod.object = arm
    xs = [v.co for v in mesh.data.vertices]
    lo = [min(v[i] for v in xs) for i in range(3)]
    hi = [max(v[i] for v in xs) for i in range(3)]
    tris = sum(max(0, len(p.vertices) - 2) for p in mesh.data.polygons)
    contract = dict(res['contract'])
    contract.update(bones=bone_names + ['socket_' + s for s in res['sockets']],
                    deform_bones=bone_names, socket_positions={'socket_' + s: [*Vector(v[1])] for s, v in res['sockets'].items()},
                    socket_parents={'socket_' + s: v[0] for s, v in res['sockets'].items()},
                    sockets={s: [*Vector(v[1])] for s, v in res['sockets'].items()}, params=res['params'], physics=res['physics'],
                    bounds={'min': lo, 'max': hi, 'size': [hi[i] - lo[i] for i in range(3)]}, triangle_count=tris,
                    uv_layers=[u.name for u in mesh.data.uv_layers], materials=[m.name for m in mesh.data.materials],
                    root_bone='gun_root', grip_sockets={'r': 'socket_grip_r', 'l': 'socket_grip_l'}, muzzle_socket='socket_muzzle',
                    eject_socket='socket_eject', steam_socket='socket_steam')
    print(f'[mg] template: {tris} triangles, {len(bone_names)} bones + {len(res["sockets"])} sockets, size {contract["bounds"]["size"]}, '
          f'uv {contract["uv_layers"]}, materials {contract["materials"]}', flush=True)
    return {'armature': arm, 'meshes': [mesh], 'weapon_meshes': [mesh], 'bind_proxy': proxy, 'collection': coll,
            'contract': contract, 'bounds': contract['bounds'], 'physics': res['physics'], 'texture_files': textures, 'res': res}


# --------------------------------------------------------------------------------------------- statics

def build_statics(scene, template, out, bake=True):
    """SM_Bean_MachineGun (pickup), SM_Ammo_MGBox, SM_MG_BeltLink (from the baked gun: same materials), SM_MG_Casing
    (own 512 bake). Bottom-centre pivots, UCX hulls."""
    import animation as An
    coll = bpy.data.collections.new('COL-MG-Statics')
    scene.collection.children.link(coll)
    sk = template['meshes'][0]
    statics = {}

    def tri_count(me):
        return sum(max(0, len(p.vertices) - 2) for p in me.polygons)

    def compact(me):
        """Only the material slots the faces use (a static cut out of the gun keeps the gun's four slots)."""
        used = sorted({p.material_index for p in me.polygons})
        mats = [me.materials[i] for i in used]
        index = [used.index(p.material_index) for p in me.polygons]
        me.materials.clear()
        for m in mats:
            me.materials.append(m)
        for p, i in zip(me.polygons, index):
            p.material_index = i

    def finish(name, me, hull_boxes, view, role):
        compact(me)
        lo, hi = EX.bounds(me)
        pivot = Vector(((lo.x + hi.x) / 2, (lo.y + hi.y) / 2, lo.z))
        me.transform(Matrix.Translation(-pivot))
        ob = bpy.data.objects.new(name, me)
        coll.objects.link(ob)
        hulls = []
        for i, (a, b) in enumerate(hull_boxes):
            h = EX.ucx_box(scene, f'UCX_{name}_{i:02d}', Vector(a) - pivot, Vector(b) - pivot)
            for c in list(h.users_collection):
                c.objects.unlink(h)
            coll.objects.link(h)
            h.hide_render = True
            hulls.append(h)
        statics[name] = {'object': ob, 'hulls': hulls, 'view': view, 'role': role, 'pivot': 'bottom_center',
                         'size_m': [round(v, 4) for v in (hi - lo)], 'triangles': tri_count(me), 'pivot_offset_m': [*pivot]}
        return ob

    # the pickup: the gun at rest without the spare empty pockets and the pockets folded inside the box
    drop = list(An.OUT_BONES[:MG.OUT_SPARE]) + [b for b in An.IN_BONES if An.REST[b] < An.S_SLOT - 0.004]
    me = EX.mesh_without_groups(sk, drop, Matrix.Identity(4))
    me.name = 'SM_Bean_MachineGun'
    B0, B1 = MG.BOX_MIN, MG.BOX_MAX
    hull_boxes = [((MG.REC_X0 - 0.065, -MG.REC_HW - 0.012, MG.REC_Z0 - 0.004), (MG.TRUNNION_X1, MG.REC_HW + 0.012, MG.TOP_Z + 0.004)),
                  ((MG.JACKET_X0, -0.058, -0.058), (MG.JACKET_X1 + 0.02, 0.058, 0.090)),
                  ((MG.GLAND_X, -0.036, -0.036), (MG.MUZZLE_X + 0.001, 0.036, 0.036)),
                  ((B0.x, B0.y, B0.z), (B1.x, B1.y, MG.REC_Z0)),
                  ((MG.FOREGRIP.x - 0.018, -0.018, -0.155), (MG.FOREGRIP.x + 0.022, 0.018, -0.058))]
    finish('SM_Bean_MachineGun', me, hull_boxes, 'pickup', 'the machine gun lying on the ground (pickup, DA_Item_MachineGun.PickupMesh)')
    # the ammunition box (a full box: the pickup of the belts)
    me = EX.mesh_of_groups(sk, ['ammo_box'], Matrix.Identity(4))
    me.name = 'SM_Ammo_MGBox'
    finish('SM_Ammo_MGBox', me, [((B0.x - 0.004, B0.y - 0.004, B0.z - 0.002), (B1.x + 0.016, B1.y + 0.004, B1.z + 0.010))], 'ammo',
           'full ammunition box, 250-round canvas belt (ammunition pickup / shop; dropped empty at BoxOut)')
    # one empty belt link, flat on the ground (its pocket frame: X cartridge axis, Y along the belt, Z up)
    link = An.OUT_BONES[6]
    me = EX.mesh_of_groups(sk, [link], An.REST_INV[link])
    me.name = 'SM_MG_BeltLink'
    lo, hi = EX.bounds(me)
    finish('SM_MG_BeltLink', me, [(lo, hi)], 'fx', 'one empty canvas pocket of the belt with its brass spacer (pitch 17.5 mm)')
    # the spent case, own small bake
    parts = MG.build_casing(coll)
    ob = K.join_parts('GEO-MG-Casing', {'root': parts}, coll)
    if bake:
        paths = bake_group(scene, ob, 'MG_Casing', Path(out) / 'textures', 512, color_samples=8, ao_samples=32, ao_distance=0.004)
        template['texture_files'].update({f'Casing.{k}': v for k, v in paths.items()})
    else:
        BAKE.unwrap(ob, margin=0.01)
    me = ob.data.copy()
    me.transform(ob.matrix_world)
    bpy.data.objects.remove(ob, do_unlink=True)
    me.name = 'SM_MG_Casing'
    lo, hi = EX.bounds(me)
    finish('SM_MG_Casing', me, [(lo, hi)], 'fx', 'spent 7.7 mm case thrown at every Fire from socket_eject (mouth +X, lying, 56 mm)')
    for info in statics.values():
        info['object'].hide_render = True
    STATE['statics'] = statics
    return statics


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


def stage(scene, view, clip):
    An = A()
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.length_unit = 'METERS'
    scene.unit_settings.scale_length = 1
    set_engine(scene, 'BLENDER_EEVEE')
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1280, 720, 100
    scene.render.image_settings.file_format = 'PNG'
    scene.render.fps = An.FPS
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
        cam.matrix_world = An.camera_matrix()
        data.type = 'PERSP'
        data.sensor_fit = 'HORIZONTAL'
        data.sensor_width = 36.0
        data.lens = An.FP_LENS
        data.clip_start = .01
    else:
        cam.location = (1.58, -2.1, 1.17)
        target = Vector((-.03, -.10, .55))
        data.type = 'ORTHO'
        data.ortho_scale = 2.4
        cam.rotation_euler = (target - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    return cam


def render(scene, out_path, frame, res=1280, flash=False, steam=False):
    bpy.context.window.scene = scene
    scene.frame_set(frame)
    scene.render.resolution_x = res
    scene.render.resolution_y = int(res * 9 / 16)
    try:
        scene.eevee.taa_render_samples = 32
    except AttributeError:
        pass
    fx = []
    if flash:
        fx = muzzle_flash(scene)
    if steam:
        fx += steam_plume(scene)
    scene.render.filepath = str(out_path)
    scene.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True, scene=scene.name)
    for o in fx:
        bpy.data.objects.remove(o, do_unlink=True)
    print('[mg] rendered ' + str(out_path), flush=True)


def muzzle_flash(scene):
    """Render-only muzzle flash (the game's is a Niagara system + light at socket_muzzle): an emissive star of
    crossed blades and a light at the muzzle, a spent case in the air under the ejection port."""
    import bmesh
    gun = next(o for o in scene.objects if o.type == 'ARMATURE' and o.name.startswith('SKEL-MachineGun'))
    pb = gun.pose.bones['socket_muzzle']
    root = gun.pose.bones['gun_root']
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
        verts = [bm.verts.new(origin + d * 0.008), bm.verts.new(origin + axis * 0.38 + d * 0.0), bm.verts.new(origin - d * 0.008),
                 bm.verts.new(origin + axis * 0.11 + d * 0.10), bm.verts.new(origin + axis * 0.11 - d * 0.10)]
        bm.faces.new((verts[0], verts[3], verts[1]))
        bm.faces.new((verts[2], verts[1], verts[4]))
    bmesh.ops.create_icosphere(bm, subdivisions=2, radius=0.075, matrix=Matrix.Translation(origin + axis * 0.06))
    me = bpy.data.meshes.new('FX-FlashMesh')
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat)
    ob = bpy.data.objects.new('FX-Flash', me)
    scene.collection.objects.link(ob)
    ld = bpy.data.lights.new('FX-FlashLight', 'POINT')
    ld.energy = 600.0
    ld.color = (1.0, 0.65, 0.3)
    ld.shadow_soft_size = 0.05
    lo = bpy.data.objects.new('FX-FlashLight', ld)
    scene.collection.objects.link(lo)
    lo.location = origin + axis * 0.06
    objs = [ob, lo]
    # a spent case falling under the port
    casing = STATE.get('statics', {}).get('SM_MG_Casing')
    if casing:
        ej = gun.matrix_world @ gun.pose.bones['socket_eject'].matrix
        c = bpy.data.objects.new('FX-Case', casing['object'].data)
        scene.collection.objects.link(c)
        c.matrix_world = Matrix.Translation(ej.translation + Vector((0.01, -0.03, -0.09))) @ Matrix.Rotation(0.9, 4, 'Z') @ Matrix.Rotation(0.5, 4, 'X')
        objs.append(c)
    return objs


def steam_plume(scene):
    """Render-only steam (the game's is NS_MG_SteamVent at socket_steam): soft translucent puffs rising from the
    valve, a few wisps from the condensing hose."""
    import bmesh
    gun = next(o for o in scene.objects if o.type == 'ARMATURE' and o.name.startswith('SKEL-MachineGun'))
    mat = bpy.data.materials.get('FX-Steam') or bpy.data.materials.new('FX-Steam')
    mat.use_nodes = True
    nt = mat.node_tree
    nt.nodes.clear()
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    mixn = nt.nodes.new('ShaderNodeMixShader')
    tr = nt.nodes.new('ShaderNodeBsdfTransparent')
    em = nt.nodes.new('ShaderNodeEmission')
    em.inputs['Color'].default_value = (0.92, 0.94, 0.96, 1)
    em.inputs['Strength'].default_value = 1.2
    mixn.inputs['Fac'].default_value = 0.42
    nt.links.new(tr.outputs[0], mixn.inputs[1])
    nt.links.new(em.outputs[0], mixn.inputs[2])
    nt.links.new(mixn.outputs[0], out.inputs[0])
    for attr, value in (('surface_render_method', 'BLENDED'), ('blend_method', 'BLEND')):
        try:
            setattr(mat, attr, value)
        except (AttributeError, TypeError):
            pass
    objs = []
    for sock, n, rise in (('socket_steam', 9, 0.05), ('socket_hose', 4, 0.03)):
        m = gun.matrix_world @ gun.pose.bones[sock].matrix
        base = m.translation
        bm = bmesh.new()
        for k in range(n):
            r = 0.018 + 0.010 * k
            c = base + Vector((0.004 * math.sin(k * 1.7), 0.003 * math.cos(k * 2.3), rise * 0.5 + rise * k * 0.9))
            bmesh.ops.create_icosphere(bm, subdivisions=2, radius=r, matrix=Matrix.Translation(c))
        me = bpy.data.meshes.new('FX-SteamMesh')
        bm.to_mesh(me)
        bm.free()
        me.materials.append(mat)
        ob = bpy.data.objects.new('FX-Steam-' + sock, me)
        scene.collection.objects.link(ob)
        objs.append(ob)
    return objs


def camera_record(cam):
    return dict(type=cam.data.type, matrix_world=[list(row) for row in cam.matrix_world], location_m=list(cam.location),
                rotation_euler_deg=[math.degrees(v) for v in cam.matrix_world.to_euler()], lens_mm=cam.data.lens,
                horizontal_fov_degrees=math.degrees(cam.data.angle_x), sensor_width_mm=cam.data.sensor_width,
                ortho_scale_m=cam.data.ortho_scale, clip_start_m=cam.data.clip_start, view_local_axis='-Z', up_local_axis='+Y')


def product_scene(template, statics, out, previews=False):
    """Product renders (Cycles): the gun three-quarter from its left with a box, cases and links on a dark bench;
    the crank close-up from its right; a side view."""
    scene = bpy.data.scenes.new('SCN-Product')
    bpy.context.window.scene = scene
    scene.unit_settings.system = 'METRIC'
    scene.render.engine = 'CYCLES'
    dev = BAKE.gpu()
    scene.cycles.device = 'GPU' if dev else 'CPU'
    scene.cycles.samples = 96
    scene.cycles.use_denoising = True
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = 1600, 900, 100
    scene.render.image_settings.file_format = 'PNG'
    try:
        scene.view_settings.view_transform = 'AgX'
        scene.view_settings.look = 'AgX - Medium High Contrast'
    except TypeError:
        pass
    scene.world = bpy.data.worlds.new('World-Product')
    scene.world.use_nodes = True
    w = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    w.inputs['Color'].default_value = (.33, .37, .43, 1)
    w.inputs['Strength'].default_value = .45
    coll = bpy.data.collections.new('COL-Product')
    scene.collection.children.link(coll)
    me = bpy.data.meshes.new('REF-Bench')
    me.from_pydata([(-4, -4, 0), (4, -4, 0), (4, 4, 0), (-4, 4, 0)], [], [(0, 1, 2, 3)])
    bench = bpy.data.objects.new('REF-Bench', me)
    coll.objects.link(bench)
    bench.data.materials.append(material('MAT-Bench', (.060, .050, .040), .72))
    for name, loc, power, size, color in (('Key', (1.0, 1.1, 1.4), 90, 1.6, (1, .93, .84)), ('Fill', (-1.2, -0.9, 0.9), 28, 2.2, (.85, .9, 1)),
                                          ('Rim', (-0.6, 1.3, 1.1), 70, 1.0, (1, .95, .9)), ('Side', (0.6, -1.4, 0.8), 35, 1.4, (1, 1, 1))):
        data = bpy.data.lights.new('LGT-Product-' + name, 'AREA')
        data.energy = power
        data.size = size
        data.color = color
        light = bpy.data.objects.new(data.name, data)
        coll.objects.link(light)
        light.location = loc
        light.rotation_euler = (Vector((0.3, 0, 0.2)) - light.location).to_track_quat('-Z', 'Y').to_euler()
    pick = statics['SM_Bean_MachineGun']
    gun = bpy.data.objects.new('PRODUCT-MachineGun', pick['object'].data)
    coll.objects.link(gun)
    gun.location = (0.0, 0.0, 0.0)
    placed = []
    for i, (name, loc, rot) in enumerate((('SM_Ammo_MGBox', (-0.12, 0.40, 0.0), 25), ('SM_MG_Casing', (0.42, 0.30, 0.0), 70),
                                          ('SM_MG_Casing', (0.47, 0.26, 0.0), 15), ('SM_MG_Casing', (0.38, 0.22, 0.0), 120),
                                          ('SM_MG_BeltLink', (0.20, 0.36, 0.0), 80), ('SM_MG_BeltLink', (0.235, 0.355, 0.0), 82))):
        ob = statics[name]['object']
        copy = bpy.data.objects.new('PRODUCT-' + name + str(i), ob.data)
        coll.objects.link(copy)
        copy.location = loc
        copy.rotation_euler = (0, 0, math.radians(rot))
        placed.append(copy)
    data = bpy.data.cameras.new('CAM-Product')
    cam = bpy.data.objects.new(data.name, data)
    coll.objects.link(cam)
    scene.camera = cam
    off = Vector(pick['pivot_offset_m'])

    def shoot(path, loc, target, lens=50):
        cam.location = loc
        cam.rotation_euler = (Vector(target) - Vector(loc)).to_track_quat('-Z', 'Y').to_euler()
        data.lens = lens
        data.clip_start = 0.01
        scene.render.filepath = str(path)
        bpy.ops.render.render(write_still=True, scene=scene.name)
        print('[mg] rendered ' + str(path), flush=True)

    # gun coordinates of the static: weapon point p -> p - pivot
    def g(p):
        return Vector(p) - off

    r = Path(out) / 'renders'
    shoot(r / 'product-weapon.png', g((1.05, 1.05, 0.62)), g((0.30, 0.02, -0.05)), 42)
    shoot(r / 'product-crank.png', g((0.10, -0.48, 0.16)), g((-0.06, -0.10, -0.02)), 55)
    shoot(r / 'product-feed.png', g((0.24, 0.50, 0.24)), g((0.05, 0.06, -0.03)), 55)
    shoot(r / 'product-muzzle.png', g((1.30, 0.30, 0.12)), g((0.80, 0.0, 0.0)), 70)
    if previews:
        PREVIEWS.mkdir(parents=True, exist_ok=True)
        shoot(PREVIEWS / 'machinegun_trois_quarts.png', g((1.05, 1.05, 0.62)), g((0.30, 0.02, -0.05)), 42)
        shoot(PREVIEWS / 'machinegun_manivelle.png', g((0.10, -0.48, 0.16)), g((-0.06, -0.10, -0.02)), 55)
        shoot(PREVIEWS / 'machinegun_alimentation.png', g((0.24, 0.50, 0.24)), g((0.05, 0.06, -0.03)), 55)
    STATE['product_scene'] = scene


# --------------------------------------------------------------------------------------------- the clips

def generate(spec, out, clips=None, views=('TP', 'FP'), bake=True, model_only=False):
    An = A()
    root_scene = bpy.context.scene
    bpy.context.window.scene = root_scene
    weapon_coll = bpy.data.collections.new('COL-Weapon-Template')
    root_scene.collection.children.link(weapon_coll)
    template = build_template(weapon_coll, spec, out, bake)
    statics = build_statics(root_scene, template, out, bake)
    records = []
    manifest = dict(format='bean-musket-anim/1', pack='bean-machinegun', weapon='MachineGun', version=spec['version'], fps=An.FPS,
                    local_build=True, built_by='scripts/props/machinegun_pack/build.py (T-272)',
                    source=dict(blend=SOURCE.relative_to(REPO).as_posix(), sha256=EX.sha(SOURCE)),
                    views=list(views), scenes=records, weapon_contract=template['contract'], assets=[], loops=sorted(An.LOOPS),
                    belt_check=An.belt_margins())
    print('[mg] belt margins', manifest['belt_check'], flush=True)
    if model_only:
        manifest.update(requested_clips=list(An.CLIPS), delivered_clips=[], not_delivered_clips=list(An.CLIPS))
        STATE.update(manifest=manifest, template=template, source=None, root_scene=root_scene)
        return manifest
    src = source_bean(root_scene)
    clips = list(clips) if clips else list(An.CLIPS)
    for view in views:
        for clip in clips:
            print('[mg] ' + view + ' ' + clip, flush=True)
            scene = bpy.data.scenes.new('MG-' + view + '-' + clip)
            bpy.context.window.scene = scene
            bean = clone_bean(src, scene, view, clip)
            weapon = clone_weapon(template, scene, view, clip)
            cam = stage(scene, view, clip)
            qa = An.animate(scene, bean, weapon, clip, view)
            targets = qa.pop('grip_targets')
            records.append(dict(scene=scene.name, view=view, clip=clip, start=0, end=An.CLIPS[clip], fps=An.FPS, loop=clip in An.LOOPS,
                                camera=cam.name, camera_data=camera_record(cam),
                                actors=dict(bean=dict(armature=bean['armature'].name, meshes=[o.name for o in bean['meshes']],
                                                      action=qa['bean_action']),
                                            weapon=dict(armature=weapon['armature'].name, meshes=[o.name for o in weapon['proxy']],
                                                        render_meshes=[o.name for o in weapon['meshes']], action=qa['weapon_action'])),
                                events=qa['events'], preview_fx=[],
                                grip_contract=dict(bean_tail_bones={'l': 'grip_l', 'r': 'grip_r'},
                                                   weapon_head_bones={'l': 'socket_grip_l', 'r': 'socket_grip_r'},
                                                   release_intervals=An.release_intervals(clip)), qa=qa))
            STATE.setdefault('targets', {})[f'{view}-{clip}'] = targets
            print('[mg] hand error %s, arm scale max %.3f' % (qa['hand_error_max_m'], qa['upperarm_scale_max']), flush=True)
    bpy.context.window.scene = root_scene
    root_scene.collection.children.unlink(src['collection'])
    root_scene.collection.children.unlink(weapon_coll)
    root_scene.name = 'SCN-bean-machinegun'
    manifest.update(requested_clips=list(An.CLIPS), delivered_clips=clips, not_delivered_clips=sorted(set(An.CLIPS) - set(clips)),
                    fp_framing=An.fp_framing(),
                    fire_sequence={'clips': ['FireStart', 'FireLoop', 'FireEnd'], 'fps': An.FPS, 'shots_per_turn': 4,
                                   'rounds_per_minute_at_rate_1': round(60 * An.FPS * 4 / An.CLIPS['FireLoop']),
                                   'crank_phi_deg': {'FireStart': [0, 270], 'FireLoop': [270, 630], 'FireEnd': [270, 360]},
                                   'note': 'FireStart then FireLoop repeated, FireEnd only from the end of a FireLoop turn (its frame 0 '
                                           'crank angle); PlayRate scales the rate of fire (events and belt together).'},
                    reload_sequence={'clip': 'Reload', 'fps': An.FPS, 'frames': An.CLIPS['Reload'], 'duration_s': An.CLIPS['Reload'] / An.FPS,
                                     'events': An.events('Reload')},
                    overheat={'clip': 'Overheat', 'frames': An.CLIPS['Overheat'], 'events': An.events('Overheat'),
                              'heat_texture': 'textures/T_MG_Jacket_Heat.png (glow mask of the barrel and the cone)'},
                    discard={'clip': 'Discard', 'frames': An.CLIPS['Discard'], 'events': An.events('Discard'),
                             'release_frame': An.RELEASE_FRAME, 'empty_frame': An.EMPTY_FRAME},
                    notes=spec.get('notes', []))
    STATE.update(manifest=manifest, template=template, source=src, root_scene=root_scene)
    return manifest


def post_build(out, model_only=False):
    manifest = STATE['manifest']
    importlib.reload(EX)
    manifest['assets'] = EX.export_pack(Path(out), manifest, STATE['template'], STATE['statics'], model_only)
    write(Path(out) / 'machinegun-manifest.json', manifest)
    if 'fp_framing' in manifest:
        write(Path(out) / 'fp-framing.json', manifest['fp_framing'])
    for name, targets in STATE.get('targets', {}).items():
        write(Path(out) / 'validation' / 'grips' / (name + '.json'), targets)
    bpy.context.window.scene = STATE['root_scene']


def scene_of(manifest, view, clip):
    rec = next(r for r in manifest['scenes'] if r['view'] == view and r['clip'] == clip)
    return bpy.data.scenes[rec['scene']]


def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    p = argparse.ArgumentParser()
    p.add_argument('--clips', default='')
    p.add_argument('--views', default='TP,FP')
    p.add_argument('--no-export', action='store_true')
    p.add_argument('--no-bake', action='store_true')
    p.add_argument('--model-only', action='store_true')
    p.add_argument('--product', action='store_true')
    p.add_argument('--previews', action='store_true')
    p.add_argument('--render', default='')
    p.add_argument('--save', action='store_true')
    p.add_argument('--out', default=str(REPO / 'build' / 'packs' / 'bean-machinegun'))
    args = p.parse_args(argv)
    spec = json.loads(PACK_SPEC.read_text(encoding='utf8'))
    for ob in list(bpy.data.objects):
        bpy.data.objects.remove(ob, do_unlink=True)
    clips = [c for c in args.clips.split(',') if c] or None
    views = tuple(v for v in args.views.split(',') if v)
    out = Path(args.out).resolve()          # Blender saves relative image / render paths elsewhere
    out.mkdir(parents=True, exist_ok=True)
    manifest = generate(spec, out, clips, views, bake=not args.no_bake, model_only=args.model_only)
    if not args.no_export:
        post_build(out, args.model_only)
    if args.product or args.previews:
        product_scene(STATE['template'], STATE['statics'], out, previews=args.previews)
    for item in [r for r in args.render.split(',') if r]:
        view, clip, frame_ = item.split(':')
        flash = clip in ('FireLoop', 'FireStart') and int(frame_) in (0, 1, 4, 5, 8, 9, 12, 13)
        steam = clip == 'Overheat' and 8 <= int(frame_) <= 44
        render(scene_of(manifest, view, clip), out / 'renders' / ('%s-%s-%03d.png' % (view, clip, int(frame_))), int(frame_), flash=flash, steam=steam)
    if args.previews and not args.model_only:
        # FireLoop 5: the kick of the second round, the crank handle up in front (the right hand in view)
        shots = {'machinegun_fp_repos.png': ('FP', 'Idle', 0, False, False), 'machinegun_fp_tir.png': ('FP', 'FireLoop', 5, True, False),
                 'machinegun_fp_rechargement.png': ('FP', 'Reload', 100, False, False),
                 'machinegun_fp_surchauffe.png': ('FP', 'Overheat', 22, False, True), 'machinegun_tp.png': ('TP', 'FireLoop', 5, True, False),
                 'machinegun_tp_rechargement.png': ('TP', 'Reload', 60, False, False)}
        for name, (view, clip, frame_, flash, steam) in shots.items():
            if any(r['view'] == view and r['clip'] == clip for r in manifest['scenes']):
                render(scene_of(manifest, view, clip), PREVIEWS / name, frame_, flash=flash, steam=steam)
    if args.save:
        bpy.context.window.scene = STATE['root_scene']
        bpy.ops.wm.save_as_mainfile(filepath=str(out / 'bean-machinegun.blend'), compress=True)
    print('[mg] finished ' + str(out), flush=True)


if __name__ == '__main__':
    main()
