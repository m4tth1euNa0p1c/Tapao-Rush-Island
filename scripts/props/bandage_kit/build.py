"""Bandage kit of T-340 (lot L5, owner request 2026-10-11: "bandages de soin : objet consommable, animation d'application
en 1re et 3e personne, soin au maximum à la fin"): models, texture, third person clip, first person keys, icon, manifest
and control renders, all procedural (Blender in the background).

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
      --python scripts/props/bandage_kit/build.py -- --all
  options: --textures (reused when present, --force-textures to redo), --models, --anims (clip + first person keys),
           --previews, --icons, --all (= all of them)

Outputs (contract with the code of T-340: ABandageItem, DA_BandageRules; names fixed by the provisional manifest):
  art/export/Props/Bandage/          SM_Bandage_Roll, SM_Bandage_Roll_Pickup, SM_Bandage_Wrapped (.fbx),
                                     Textures/T_Bandage_Linen_{D,N,ORM}.png, manifest.json (status built)
  art/export/Characters/Bandage/     A_Bean_Bandage_Apply.fbx (SK_Bean_Body_Skeleton, 30 fps, no root motion)
  art/ui/icons/bandage/              T_Icon_Bandage.png (+ _raw/, icons.json)
  art/_previews/bandage/             control renders (third person, first person, items)
bean_soldier.blend is read, never written. Nothing is imported in Unreal by this script.
"""
from __future__ import annotations

import argparse
import datetime as dt
import importlib
import json
import math
import sys
import time
from pathlib import Path

import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
sys.path.insert(0, str(HERE))
import kit as K  # noqa: E402
import textures as T  # noqa: E402
import models as M  # noqa: E402
import rig as R  # noqa: E402
import clips as C  # noqa: E402
import fp as FPm  # noqa: E402
import previews as P  # noqa: E402

for _m in (K, T, M, R, C, FPm, P):
    importlib.reload(_m)

PROPS_OUT = REPO / 'art' / 'export' / 'Props' / 'Bandage'
CLIPS_OUT = REPO / 'art' / 'export' / 'Characters' / 'Bandage'
PREV = REPO / 'art' / '_previews' / 'bandage'
ICONS = REPO / 'art' / 'ui' / 'icons' / 'bandage'
BUILD = REPO / 'build' / 'bandage_kit'
SKELETON_UE = '/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body_Skeleton'
STATE = {}

BODY = ['Bean_Body', 'Coat', 'Waistcoat', 'Collar', 'Lapel_l', 'Lapel_r', 'Belt', 'Buckle', 'CrossBelt_Fl', 'CrossBelt_Fr',
        'CrossBelt_Bl', 'CrossBelt_Br', 'Epaulette_l', 'Epaulette_r', 'Bean_Moustache_l', 'Bean_Moustache_r', 'Bean_Eye_l',
        'Bean_Eye_r', 'Bean_Hair_Cap', 'Turnback_l', 'Turnback_r', 'Pouch', 'Pouch_Flap']
HATS = ['Hat_Brim', 'Hat_Crown', 'Hat_Trim', 'Hat_Cockade', 'Hat_Cockade_Button']
HAND_R = ['Bean_Hand_r', 'Bean_Thumb_r']
SLEEVE_R = ['Sleeve_r', 'Cuff_r', 'Btn_Cuff_r0', 'Btn_Cuff_r1']
LEFT_ARM = ['Sleeve_l', 'Cuff_l', 'Btn_Cuff_l0', 'Btn_Cuff_l1', 'Bean_Hand_l', 'Bean_Thumb_l']
FP_PARTS = ['FPArm_Sleeve', 'FPArm_Cuff', 'FPArm_Lace0', 'FPArm_Btn0', 'FPArm_Lace1', 'FPArm_Btn1', 'FPArm_Lace2',
            'FPArm_Btn2', 'FPArm_Wrist', 'FPArm_Hand', 'FPArm_Thumb']
# roll in the right mitten (hand rest frame: X thumb side, Y along the fingers, Z palm): at the fingertips, on the palm side
ROLL_IN_HAND = {'along_fingers': 0.058, 'palm': 0.02}
# first person: the roll under the fingertips of the right arm mesh (Blender arm space, m), its axis across the arm
FP_ROLL = {'location_m': (0.43, 0.0, -0.035), 'yaw_deg_blender': 90.0}


def log(msg):
    print(f'[bandage] {msg}', flush=True)


def rel(p):
    return Path(p).resolve().relative_to(REPO).as_posix()


# ------------------------------------------------------------------------------------------------- models
def socket_rows(sockets):
    return {name: {'blender_m': [round(v, 4) for v in loc], 'unreal_cm': K.to_unreal_cm(loc)}
            for name, (loc, rot) in sockets.items()}


def export_asset(ob, ucx, sockets, coll):
    socks = []
    for n, (loc, rot) in sockets.items():
        s = K.socket(n, loc, ob, coll, rot)
        assert s.name == f'SOCKET_{n}', f'{ob.name}: socket name taken in the scene ({s.name})'
        socks.append(s)
    fbx = K.export_fbx([ob] + list(ucx) + socks, PROPS_OUT / f'{ob.name}.fbx')
    for s in socks:
        s.name = f'{ob.name}-{s.name}'
    stats = K.mesh_stats(ob)
    return {'files': [rel(fbx)], 'triangles': stats['triangles'], 'vertices': stats['vertices'], 'size_m': stats['size_m'],
            'bounds_min_m': stats['min_m'], 'bounds_max_m': stats['max_m'],
            'size_cm_unreal': [round(v * 100, 1) for v in stats['size_m']],
            'collision': [u.name for u in ucx], 'sockets': socket_rows(sockets),
            'materials': [m.name for m in ob.data.materials]}


def fp_arm_tree():
    return M.sleeve_tree(['FPArm_Cuff', 'FPArm_Sleeve'])


def build_models(scene, mats, rig):
    coll = bpy.data.collections.new('COL-Bandage-Models')
    scene.collection.children.link(coll)
    assets, objects = {}, {}
    ob, socks, info = M.build_roll(coll, mats)
    assets['SM_Bandage_Roll'] = dict(export_asset(ob, [], socks, coll), info=info)
    objects['SM_Bandage_Roll'] = ob
    ob, ucx, socks, info = M.build_pickup(coll, mats)
    assets['SM_Bandage_Roll_Pickup'] = dict(export_asset(ob, ucx, socks, coll), info=info)
    objects['SM_Bandage_Roll_Pickup'] = ob
    tree = M.sleeve_tree(['Cuff_l', 'Sleeve_l'])
    Mframe = M.forearm_frame(rig)
    ob, info, surf = M.build_wrapped(coll, mats, tree, Mframe)
    assets['SM_Bandage_Wrapped'] = dict(export_asset(ob, [], {}, coll), info=info)
    objects['SM_Bandage_Wrapped'] = ob
    STATE['band_frame'] = Mframe
    STATE['band_outer_radius'] = info['inner_radius_m'][1] + M.WRAP['thickness']
    # the band on the first person arm: radial scale to clear FPArm_Cuff
    total = info['covered_length_m']
    k = M.fit_scale_fp(fp_arm_tree(), FPm.FP['cuff_x'] / 100.0, FPm.FP['hand_point'][2] / 100.0,
                       lambda dx, a: surf(dx, a) + M.WRAP['gap'], total)
    STATE['fp_band_scale'] = round(max(1.0, k) * 1.01, 4)
    log(f'band: {info}, first person radial scale {STATE["fp_band_scale"]}')
    for o in coll.objects:
        if o.type == 'EMPTY' or o.name.startswith('UCX_'):
            o.hide_viewport = True
            o.hide_render = True
    STATE['models'] = objects
    return assets


def roll_in_hand(rig):
    """SM_Bandage_Roll relative to hand_r: at the fingertips on the palm side, roll axis along the thumb side, loose end
    hanging along the fingers (down at rest)"""
    F = rig.hand_rest_frame('r')
    x, y, z = Vector(F.col[0]), Vector(F.col[1]), Vector(F.col[2])
    centre = rig.grip_rest['r'] + y * ROLL_IN_HAND['along_fingers'] + z * ROLL_IN_HAND['palm']
    zr = -y
    xr = x
    yr = zr.cross(xr)
    W = Matrix((xr, yr, zr)).transposed().to_4x4()
    W.translation = centre
    return rig.rest['hand_r'].inverted() @ W, W


# ------------------------------------------------------------------------------------------------- clip
def build_clip(rig):
    roll_rel, _ = roll_in_hand(rig)
    STATE['roll_rel'] = roll_rel
    clip, samples = C.bandage_apply(rig, STATE['band_frame'], STATE['band_outer_radius'], roll_rel)
    action = rig.bake(samples, clip.name)
    path = R.export_clip(rig.arm, action, CLIPS_OUT / f'{clip.name}.fbx', clip.frames)
    STATE['clip'], STATE['samples'], STATE['action'] = clip, samples, action
    rig.arm.animation_data.action = None
    log(f'{clip.name}: {clip.frames} frames, qa {clip.qa}')
    return path


def mesh_tree(objs, extra=None):
    dg = bpy.context.evaluated_depsgraph_get()
    verts, polys = [], []
    for o in objs:
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        base = len(verts)
        mw = extra.get(o.name, o.matrix_world) if extra else o.matrix_world
        verts += [mw @ v.co for v in me.vertices]
        polys += [[base + i for i in p.vertices] for p in me.polygons]
        ev.to_mesh_clear()
    return BVHTree.FromPolygons(verts, polys) if polys else None


def _inside(tree, p, d=Vector((0.0123, 0.0071, 1.0)).normalized()):
    n, o = 0, Vector(p)
    for _ in range(64):
        hit = tree.ray_cast(o, d, 5.0)
        if hit[0] is None:
            break
        n += 1
        o = hit[0] + d * 1e-5
    return n % 2 == 1


def _verts(objs):
    dg = bpy.context.evaluated_depsgraph_get()
    out = []
    for o in objs:
        ev = o.evaluated_get(dg)
        me = ev.to_mesh()
        out += [o.matrix_world @ v.co for v in me.vertices]
        ev.to_mesh_clear()
    return out


def arms_gap(rig):
    """Smallest distance between the right forearm / upper arm axes and the left arm axes minus the two sleeve radii (7 cm x
    stretch), per frame: negative = the sleeves press into each other by that much"""
    rows = {}
    for f, m in enumerate(STATE['samples']):
        def seg(side, a, b):
            return (rig.carried(m, f'{a}_{side}') @ rig.bones[f'{a}_{side}'].head_local,
                    rig.carried(m, f'{b}_{side}') @ rig.bones[f'{b}_{side}'].head_local)

        def st(side):
            return (m[f'upperarm_{side}'].to_3x3() @ rig.rest[f'upperarm_{side}'].to_3x3().inverted()).to_scale()[0]
        r = [seg('r', 'upperarm', 'lowerarm'), seg('r', 'lowerarm', 'hand')]
        lft = [seg('l', 'upperarm', 'lowerarm'), seg('l', 'lowerarm', 'hand')]
        best = min(FPm.seg_dist(a0, a1, b0, b1) for a0, a1 in r for b0, b1 in lft)
        rows[f] = round((best - 0.07 * st('r') - 0.07 * st('l')) * 100, 2)
    work = {f: v for f, v in rows.items() if C.T['up'] <= f <= C.T['done']}
    return {'min_cm': min(rows.values()), 'min_wrap_frames_cm': min(work.values()), 'rest_frame_0_cm': rows[0],
            'worst_frame': min(rows, key=rows.get),
            'how': 'axes des os du bras droit (upperarm, lowerarm) contre ceux du bras gauche, moins 7 cm x étirement par '
                   'manche (négatif = les manches se pénètrent de tant)'}


def contacts_qa(rig, bean_meshes):
    """On the deformed meshes of the bean playing the clip (every frame), with the roll and the band placed as the clip
    carries them:
      clearance_cm  smallest signed distance of a group's vertices to the closed body shell Bean_Body (negative = inside
                    the body; the coat, waistcoat and belts lie 2 to 3 cm outside it: below ~2.5 cm the part touches the
                    coat), compared with the rest pose (frame 0)
      overlap       overlapping face pairs (BVH) with the hats, the left arm and the band (0 = no contact)"""
    clip = STATE['clip']
    scene = bpy.context.scene
    R.assign_action(rig.arm, STATE['action'])
    by = {o.name: o for o in bean_meshes}
    groups = {k: [by[n] for n in names if n in by] for k, names in
              (('body', BODY), ('hats', HATS), ('hand_r', HAND_R), ('sleeve_r', SLEEVE_R), ('left_arm', LEFT_ARM),
               ('cuff_l', ['Cuff_l', 'Sleeve_l']), ('left_sleeve', ['Sleeve_l', 'Cuff_l']))}
    roll = bpy.data.objects.new('QA-Roll', STATE['models']['SM_Bandage_Roll'].data)
    band = bpy.data.objects.new('QA-Band', STATE['models']['SM_Bandage_Wrapped'].data)
    for o in (roll, band):
        scene.collection.objects.link(o)
    pairs = {'hand_r~hats': ('hand_r', 'hats'), 'hand_r~left_arm': ('hand_r', 'left_arm'), 'roll~hats': ('roll', 'hats'),
             'roll~left_arm': ('roll', 'left_arm'), 'hand_r~band': ('hand_r', 'band'), 'roll~band': ('roll', 'band'),
             'sleeve_r~left_arm': ('sleeve_r', 'left_arm'), 'band~cuff_l': ('band', 'cuff_l'),
             'left_arm~hats': ('left_arm', 'hats'), 'sleeve_r~hats': ('sleeve_r', 'hats')}
    clear_groups = ('hand_r', 'roll', 'band', 'sleeve_r', 'left_sleeve')
    per = {k: {} for k in pairs}
    clear = {k: {} for k in clear_groups}
    body_obj = by['Bean_Body']
    for f in range(clip.frames + 1):
        scene.frame_set(f)
        bpy.context.view_layer.update()
        vis_r, m_r = clip.props['SM_Bandage_Roll'][f]
        vis_b, m_b = clip.props['SM_Bandage_Wrapped'][f]
        roll.matrix_world = rig.arm.matrix_world @ m_r
        # the band is attached to lowerarm_l: it scales with the stretched forearm like in Unreal
        sc = (rig.arm.matrix_world @ rig.arm.pose.bones['lowerarm_l'].matrix).to_scale()
        band.matrix_world = Matrix.Translation(m_b.translation) @ m_b.to_quaternion().to_matrix().to_4x4() @ \
            Matrix.Diagonal((sc.x, sc.y, sc.z, 1.0))
        bpy.context.view_layer.update()
        trees = {k: mesh_tree(v) for k, v in groups.items()}
        trees['roll'] = mesh_tree([roll]) if vis_r else None
        trees['band'] = mesh_tree([band]) if vis_b else None
        for key, (a, b) in pairs.items():
            ta, tb = trees.get(a), trees.get(b)
            if ta is None or tb is None:
                continue
            per[key][f] = len(ta.overlap(tb))
        shell = mesh_tree([body_obj])
        for g in clear_groups:
            objs = {'roll': [roll] if vis_r else [], 'band': [band] if vis_b else []}.get(g, groups.get(g, []))
            if not objs:
                continue
            best = 1e9
            for v in _verts(objs)[::2]:
                d = shell.find_nearest(v)[3]
                if d < best + 0.05:
                    sd = -d if _inside(shell, v) else d
                    best = min(best, sd)
            clear[g][f] = round(best * 100, 2)
    bpy.data.objects.remove(roll, do_unlink=True)
    bpy.data.objects.remove(band, do_unlink=True)
    rig.arm.animation_data.action = None
    f0, f1 = C.T['up'], C.T['done']
    summary = {'overlap': {}, 'clearance_cm': {}}
    for key, rows in per.items():
        hit = {f: v for f, v in rows.items() if v}
        summary['overlap'][key] = {'frames_checked': len(rows), 'frames_with_contact': sorted(hit),
                                   'max_overlapping_face_pairs': max(rows.values()) if rows else None,
                                   'rest_frame_0': rows.get(0)}
    for g, rows in clear.items():
        work = [v for f, v in rows.items() if f0 <= f <= f1]
        summary['clearance_cm'][g] = {'min_all_frames': min(rows.values()) if rows else None,
                                      'min_wrap_frames': min(work) if work else None,
                                      'rest_frame_0': rows.get(0),
                                      'worst_frame': min(rows, key=rows.get) if rows else None}
    summary['sleeves_gap'] = arms_gap(rig)
    STATE['contacts'] = summary
    STATE['contacts_rows'] = {'clear': clear, 'overlap': per}
    log('overlap: ' + json.dumps({k: (v['max_overlapping_face_pairs'], v['frames_with_contact'][:12]) for k, v in summary['overlap'].items()}))
    log('clearance_cm: ' + json.dumps(summary['clearance_cm']))
    return summary


# ------------------------------------------------------------------------------------------------- first person
def fp_roll_transform():
    loc = Vector(FP_ROLL['location_m'])
    Rb = Matrix.Rotation(math.radians(FP_ROLL['yaw_deg_blender']), 3, 'Z')
    Rue = FPm.M_MIRROR @ Rb @ FPm.M_MIRROR
    return loc, Rb, FPm.matrix_rotator(Rue)


def build_fp():
    loc, _Rb, rot_ue = fp_roll_transform()
    keys, qa = FPm.build_keys((loc.x * 100, -loc.y * 100, loc.z * 100))
    STATE['fp_keys'], STATE['fp_qa'] = keys, qa
    log(f'first person qa: {qa}')
    return keys, qa


def fp_rig(coll, name, mirrored):
    """An empty with copies of the SM_Bean_FPArm parts (arm space = Blender arm mesh space)"""
    root = bpy.data.objects.new('FP-' + name, None)
    coll.objects.link(root)
    for part in FP_PARTS:
        src = bpy.data.objects.get(part)
        if src is None:
            continue
        o = bpy.data.objects.new(f'FP-{name}-{part}', src.data)
        coll.objects.link(o)
        o.parent = root
        o.matrix_parent_inverse = Matrix.Identity(4)
        o.matrix_basis = src.matrix_world.copy()
        o.hide_render = False
    return root


# ------------------------------------------------------------------------------------------------- previews
def previews(rig, bean_meshes):
    PREV.mkdir(parents=True, exist_ok=True)
    out = []
    models = STATE['models']
    clip = STATE['clip']

    def prop(name, coll, label=None):
        ob = bpy.data.objects.new(label or ('PRV-' + name), models[name].data)
        coll.objects.link(ob)
        return ob

    # 1. items: roll in hand pose, roll on the ground, the wrapped band
    scene, coll, cam = P.new_scene('PRV-Items', sky=(0.34, 0.35, 0.37), floor=(0.40, 0.37, 0.33))
    a = prop('SM_Bandage_Roll', coll)
    a.matrix_world = Matrix.Translation((-0.12, 0.0, 0.10)) @ Matrix.Rotation(math.radians(-30), 4, 'Z')
    b = prop('SM_Bandage_Roll_Pickup', coll)
    b.matrix_world = Matrix.Translation((0.02, 0.05, 0.0)) @ Matrix.Rotation(math.radians(-25), 4, 'Z')
    c = prop('SM_Bandage_Wrapped', coll)
    c.matrix_world = Matrix.Translation((0.16, -0.05, 0.10)) @ Matrix.Rotation(math.radians(35), 4, 'Z')
    P.aim(cam, (0.02, 0.0, 0.06), 18, 28, 0.85, lens=50)
    out.append(P.render(scene, PREV / 'items_roll_pickup_wrapped.png', 0, (1280, 760)))

    # 2. third person: the bean bandaging, frames of the clip, front three quarter and side
    scene, coll, cam = P.new_scene('PRV-Apply')
    arm, meshes = P.clone_character(rig.arm, bean_meshes, coll, 'Bean-Bandage')
    R.assign_action(arm, STATE['action'])
    roll = prop('SM_Bandage_Roll', coll)
    band = prop('SM_Bandage_Wrapped', coll)

    def place(f):
        bpy.context.window.scene = scene
        scene.frame_set(f)
        vis, m = clip.props['SM_Bandage_Roll'][f]
        P.place_prop(roll, vis, m)
        vis_b, mb = clip.props['SM_Bandage_Wrapped'][f]
        bpy.context.view_layer.update()
        sc = (arm.matrix_world @ arm.pose.bones['lowerarm_l'].matrix).to_scale()
        P.place_prop(band, vis_b, Matrix.Translation(mb.translation) @ mb.to_quaternion().to_matrix().to_4x4() @
                     Matrix.Diagonal((sc.x, sc.y, sc.z, 1.0)))
    frames = [0, 6, 12, 16, 20, 24, 64, 74, 86]
    parts = []
    for f in frames:
        place(f)
        P.aim(cam, (0.02, -0.12, 0.62), -25, 12, 2.6, lens=85)
        parts.append(P.render(scene, PREV / f'_tp_{f}.png', f, (560, 560)))
    out.append(P.strip(parts, PREV / 'tp_apply_frames.png', columns=3))
    # one turn, close: front three quarter from the bean's right (where the right mitten works)
    parts = []
    for f in range(12, 28, 2):
        place(f)
        P.aim(cam, (0.05, -0.22, 0.62), -35, 18, 1.6, lens=85)
        parts.append(P.render(scene, PREV / f'_tpt_{f}.png', f, (480, 480)))
    out.append(P.strip(parts, PREV / 'tp_one_turn_closeup.png', columns=4))
    # the usual third person camera: behind, over the right shoulder
    parts = []
    for f in (12, 16, 20, 24, 74, 86):
        place(f)
        P.aim(cam, (0.0, -0.10, 0.75), 155, 22, 2.8, lens=50)
        parts.append(P.render(scene, PREV / f'_tpc_{f}.png', f, (560, 420)))
    out.append(P.strip(parts, PREV / 'tp_game_camera.png', columns=3))
    parts = []
    for f, (yaw, el) in zip((16, 20, 24, 27), ((95, 10), (95, 10), (-60, 15), (180, 25))):
        place(f)
        P.aim(cam, (0.05, -0.2, 0.62), yaw, el, 2.4, lens=60)
        parts.append(P.render(scene, PREV / f'_tps_{f}.png', f, (600, 600)))
    out.append(P.strip(parts, PREV / 'tp_apply_side_top_back.png', columns=2))
    place(clip.frames)
    band.hide_render = False
    band.hide_viewport = False
    # the band left on the forearm after the clip (rest pose)
    P.place_prop(band, True, STATE['band_frame'])
    P.aim(cam, (0.25, -0.05, 0.43), 60, 15, 1.0, lens=60)
    out.append(P.render(scene, PREV / 'tp_band_on_forearm_rest.png', clip.frames, (900, 700)))

    # 3. first person: camera at the origin looking along +X, horizontal FOV 90 (fp-framing.json)
    scene, coll, cam = P.new_scene('PRV-FP', sky=(0.55, 0.68, 0.85), floor=(0.62, 0.55, 0.42))
    for o in list(coll.objects):
        if o.name.startswith('REF-Floor'):
            o.location.z = -1.6
    cam.data.type = 'PERSP'
    cam.data.sensor_fit = 'HORIZONTAL'
    cam.data.angle = math.radians(90)
    cam.data.clip_start = 0.05
    cam.location = (0, 0, 0)
    cam.rotation_euler = (math.radians(90), 0, math.radians(-90))
    right = fp_rig(coll, 'Right', False)
    left = fp_rig(coll, 'Left', True)
    froll = prop('SM_Bandage_Roll', coll, 'PRV-FP-Roll')
    froll.parent = right
    fband = prop('SM_Bandage_Wrapped', coll, 'PRV-FP-Band')
    fband.parent = left
    loc, Rb, _ = fp_roll_transform()
    parts = []
    for f in (0, 8, 12, 16, 20, 24, 64, 72, 86):
        k = STATE['fp_keys'][f]
        right.matrix_world = FPm.blender_matrix(k['_right'])
        left.matrix_world = FPm.blender_matrix(k['_left'], mirrored=True)
        sr, sl = k['_right']['stretch'], k['_left']['stretch']
        froll.matrix_parent_inverse = Matrix.Identity(4)
        froll.matrix_basis = Matrix.Translation(loc) @ Matrix.Diagonal((1 / sr, 1.0, 1.0, 1.0)) @ Rb.to_4x4()
        froll.hide_render = f >= C.T['done']
        fband.matrix_parent_inverse = Matrix.Identity(4)
        s = STATE['fp_band_scale']
        fband.matrix_basis = Matrix.Translation((FPm.FP['cuff_x'] / 100.0, 0.0, FPm.FP['hand_point'][2] / 100.0)) @ \
            Matrix.Diagonal((1 / sl, s, s, 1.0))
        fband.hide_render = f < C.T['up']
        bpy.context.view_layer.update()
        parts.append(P.render(scene, PREV / f'_fp_{f}.png', 0, (640, 360)))
    out.append(P.strip(parts, PREV / 'fp_apply_frames.png', columns=3))
    return [rel(p) for p in out]


# ------------------------------------------------------------------------------------------------- icon
def icon():
    """T_Icon_Bandage: the roll on the ground with its unrolled end, three quarter view from above, 512 x 512 RGBA,
    transparent, soft dark shadow (as scripts/ui/render_capture_icons.py + finish_capture_icons.py)"""
    ICONS.mkdir(parents=True, exist_ok=True)
    (ICONS / '_raw').mkdir(exist_ok=True)
    scene = bpy.data.scenes.new('ICON-Bandage')
    keep = bpy.context.window.scene
    bpy.context.window.scene = scene          # matrices are only updated for the window's scene
    P.set_engine(scene)
    scene.render.resolution_x = scene.render.resolution_y = 512
    scene.render.resolution_percentage = 100
    scene.render.film_transparent = True
    scene.render.image_settings.file_format = 'PNG'
    scene.render.image_settings.color_mode = 'RGBA'
    try:
        scene.view_settings.view_transform = 'AgX'
        scene.view_settings.look = 'AgX - Medium High Contrast'
    except TypeError:
        pass
    try:
        scene.eevee.taa_render_samples = 64
    except AttributeError:
        pass
    world = bpy.data.worlds.new('W-Icon')
    world.use_nodes = True
    bg = next(n for n in world.node_tree.nodes if n.type == 'BACKGROUND')
    bg.inputs['Color'].default_value = (0.55, 0.57, 0.6, 1)
    bg.inputs['Strength'].default_value = 0.6
    scene.world = world
    pivot = bpy.data.objects.new('ICON-Pivot', None)
    scene.collection.objects.link(pivot)
    ob = bpy.data.objects.new('ICON-Roll', STATE['models']['SM_Bandage_Roll_Pickup'].data)
    scene.collection.objects.link(ob)
    ob.parent = pivot
    pts = [Vector(c) for c in ob.bound_box]
    centre = sum(pts, Vector()) / 8
    ob.location = -centre
    turn = (52, 0, -38)
    pivot.rotation_euler = [math.radians(v) for v in turn]
    bpy.context.view_layer.update()
    wp = [ob.matrix_world @ v.co for v in ob.data.vertices]
    xs, zs = [p.x for p in wp], [p.z for p in wp]
    extent = max(max(xs) - min(xs), max(zs) - min(zs))
    cd = bpy.data.cameras.new('CAM-Icon')
    cd.type = 'ORTHO'
    cd.ortho_scale = extent * 1.10
    cam = bpy.data.objects.new('CAM-Icon', cd)
    scene.collection.objects.link(cam)
    cam.location = ((min(xs) + max(xs)) / 2, -3.0, (min(zs) + max(zs)) / 2)
    cam.rotation_euler = (math.radians(90), 0, 0)
    scene.camera = cam
    s = extent

    def light(name, location, energy, size, color=(1, 1, 1)):
        d = bpy.data.lights.new(name, 'AREA')
        d.energy, d.size, d.color = energy, size, color
        o = bpy.data.objects.new(name, d)
        scene.collection.objects.link(o)
        o.location = location
        o.rotation_euler = (-Vector(location)).to_track_quat('-Z', 'Y').to_euler()
    light('KEY', (-1.6 * s - 0.4, -2.2 * s - 0.6, 2.0 * s + 0.6), 420 * s * s + 60, 1.6 * s + 0.3, (1.0, 0.96, 0.9))
    light('FILL', (2.0 * s + 0.4, -1.6 * s - 0.5, 0.6 * s + 0.2), 160 * s * s + 25, 2.0 * s + 0.4, (0.85, 0.9, 1.0))
    light('RIM', (0.4 * s, 2.4 * s + 0.6, 1.6 * s + 0.4), 300 * s * s + 40, 1.2 * s + 0.3)
    raw = ICONS / '_raw' / 'T_Icon_Bandage.png'
    P.render(scene, raw, 0, (512, 512))
    # soft shadow (numpy: max filter 5, gaussian ~6 px, 55 %, offset (3, 5)) like finish_capture_icons.py
    img = bpy.data.images.load(str(raw), check_existing=False)
    w, h = img.size
    px = np.empty(w * h * 4, dtype=np.float32)
    img.pixels.foreach_get(px)
    px = px.reshape(h, w, 4)
    alpha = px[..., 3]
    halo = alpha.copy()
    for axis in (0, 1):
        halo = np.max([np.roll(halo, d, axis=axis) for d in range(-2, 3)], axis=0)
    for _ in range(3):
        for axis in (0, 1):
            halo = np.mean([np.roll(halo, d, axis=axis) for d in range(-4, 5)], axis=0)
    halo *= 0.55
    shifted = np.zeros_like(halo)                             # 3 px right, 5 px down (Blender rows start at the bottom)
    shifted[:-5, 3:] = halo[5:, :-3]
    halo = shifted
    out = np.zeros_like(px)
    out[..., 0], out[..., 1], out[..., 2] = 8 / 255, 6 / 255, 4 / 255
    out[..., 3] = halo
    a_top = alpha[..., None]
    rgb = px[..., :3] * a_top + out[..., :3] * out[..., 3:4] * (1 - a_top)
    a_out = a_top[..., 0] + out[..., 3] * (1 - a_top[..., 0])
    res = np.zeros_like(px)
    res[..., :3] = np.where(a_out[..., None] > 1e-5, rgb / np.maximum(a_out[..., None], 1e-5), 0.0)
    res[..., 3] = a_out
    fin = bpy.data.images.new('T_Icon_Bandage', w, h, alpha=True)
    fin.pixels.foreach_set(res.ravel())
    fin.filepath_raw = str(ICONS / 'T_Icon_Bandage.png')
    fin.file_format = 'PNG'
    fin.save()
    ys_, xs_ = np.nonzero(alpha > 0.02)
    coverage = [int(xs_.max() - xs_.min() + 1), int(ys_.max() - ys_.min() + 1)] if len(xs_) else None
    bpy.data.images.remove(img)
    bpy.data.images.remove(fin)
    rows = [{'name': 'Bandage', 'mesh': 'SM_Bandage_Roll_Pickup', 'turn_deg': list(turn),
             'raw': rel(raw), 'png': rel(ICONS / 'T_Icon_Bandage.png'), 'extent_m': round(extent, 3),
             'coverage_px': coverage}]
    K.write_json(ICONS / 'icons.json', rows)
    bpy.context.window.scene = keep
    log(f'icon: {rows}')
    return rows


# ------------------------------------------------------------------------------------------------- reimport check
def reimport_check():
    """Imports every FBX written back into an empty scene: names, sockets, UCX, sizes (cm in the file), the clip's frame
    range and bones"""
    report = {}
    keep = bpy.context.window.scene
    scene = bpy.data.scenes.new('CHK-Reimport')
    bpy.context.window.scene = scene
    for name in ('SM_Bandage_Roll', 'SM_Bandage_Roll_Pickup', 'SM_Bandage_Wrapped'):
        before = set(bpy.data.objects)
        bpy.ops.import_scene.fbx(filepath=str(PROPS_OUT / f'{name}.fbx'))
        new = [o for o in bpy.data.objects if o not in before]
        mesh = next((o for o in new if o.type == 'MESH' and o.name.startswith(name)), None)
        row = {'objects': sorted(o.name for o in new)}
        if mesh is not None:
            bpy.context.view_layer.update()
            pts = [mesh.matrix_world @ v.co for v in mesh.data.vertices]
            row['size_m_after_import'] = [round(max(p[i] for p in pts) - min(p[i] for p in pts), 4) for i in range(3)]
            row['min_z_m'] = round(min(p.z for p in pts), 4)
            row['sockets'] = sorted(o.name for o in new if o.name.startswith('SOCKET_'))
            row['ucx'] = sorted(o.name for o in new if o.name.startswith('UCX_'))
            row['materials'] = [m.name.split('.')[0] for m in mesh.data.materials]
        report[name] = row
        for o in new:
            bpy.data.objects.remove(o, do_unlink=True)
    before = set(bpy.data.objects)
    acts = set(bpy.data.actions)
    bpy.ops.import_scene.fbx(filepath=str(CLIPS_OUT / 'A_Bean_Bandage_Apply.fbx'))
    new = [o for o in bpy.data.objects if o not in before]
    arm = next((o for o in new if o.type == 'ARMATURE'), None)
    na = [a for a in bpy.data.actions if a not in acts]
    row = {'objects': sorted(o.name for o in new)}
    if arm is not None:
        row['bones'] = len(arm.data.bones)
        row['bone_names_ok'] = all(b in arm.data.bones for b in ('root', 'pelvis', 'hand_r', 'lowerarm_l', 'hat'))
    if na:
        fr = na[0].frame_range
        row['frame_range'] = [round(fr[0], 2), round(fr[1], 2)]
    report['A_Bean_Bandage_Apply'] = row
    for o in new:
        bpy.data.objects.remove(o, do_unlink=True)
    for a in na:
        bpy.data.actions.remove(a)
    bpy.context.window.scene = keep
    bpy.data.scenes.remove(scene)
    log(f'reimport: {json.dumps(report)}')
    return report


# ------------------------------------------------------------------------------------------------- manifest
def attachments_block(rig):
    rest = rig.fk({})
    _rel, roll_world = roll_in_hand(rig)
    roll_row = R.relative_to_bone(rig, rest, 'hand_r', roll_world)
    band_row = R.relative_to_bone(rig, rest, 'lowerarm_l', STATE['band_frame'])
    loc, Rb, rot_ue = fp_roll_transform()
    s = STATE['fp_band_scale']
    return {
        'note': 'transformations relatives à un os : espace os Blender (exact) et conversion Unreal du projet '
                '(location cm (x, -y, z), FRotator(-ey, -ez, ex) comme TorchRules::GripInHand) À VÉRIFIER dans l\'éditeur',
        'roll_in_hand_r': dict(roll_row, mesh='SM_Bandage_Roll', socket_at_mitten='grip',
                               what='origine du maillage (= socket grip, centre du rouleau) relative à hand_r : au bout '
                                    'des doigts côté paume, axe du rouleau côté pouce, bout libre le long des doigts',
                               use='objet tenu (main droite) et pendant A_Bean_Bandage_Apply ; caché à BandageDone '
                                   '(rouleau consommé)'),
        'wrapped_on_lowerarm_l': dict(band_row, mesh='SM_Bandage_Wrapped',
                                      what='origine du maillage (centre de la bande sur l\'axe de l\'avant-bras) relative à '
                                           'lowerarm_l ; ajustée sur la manchette en pose de repos',
                                      use='attacher à lowerarm_l (KeepRelative avec cette transformation, échelle héritée '
                                          'de l\'os : la bande suit le bras étiré pendant le clip) ; visible dès '
                                          'WrapStart (ou à la fin du 1er tour, clip.turns) et après le soin le temps voulu '
                                          '(DA_BandageRules)'),
        'first_person': {
            'wrapped_on_left_fp_arm': {'mesh': 'SM_Bandage_Wrapped', 'parent': 'bras gauche SM_Bean_FPArm '
                                       '(UBeanFirstPersonArmsComponent, composant gauche, échelle Y -1)',
                                       'relative_location_cm': [FPm.FP['cuff_x'], 0.0, FPm.FP['hand_point'][2]],
                                       'relative_rotation_pyr_deg': [0.0, 0.0, 0.0],
                                       'relative_scale': [1.0, s, s],
                                       'note': f'échelle radiale {s} pour passer sur FPArm_Cuff (rayon 7,9 cm) ; le bras '
                                               'est étiré en X par Stretch : mettre l\'échelle X relative à 1/Stretch '
                                               'chaque image pour garder la largeur de la bande, ou l\'ignorer (écart '
                                               'max Stretch gauche, first_person.qa)'},
            'roll_in_right_fp_hand': {'mesh': 'SM_Bandage_Roll', 'parent': 'bras droit SM_Bean_FPArm',
                                      'relative_location_cm': [round(loc.x * 100, 2), round(-loc.y * 100, 2), round(loc.z * 100, 2)],
                                      'relative_rotation_pyr_deg': [round(v, 3) for v in rot_ue],
                                      'note': 'sous le bout des doigts de la mitaine, axe du rouleau en travers du bras ; '
                                              'échelle X relative 1/Stretch du bras droit pour ne pas déformer le rouleau'}},
    }


def manifest(assets, tex, clip_path):
    clip = STATE.get('clip')
    attachments = attachments_block(STATE['rig']) if clip else {}
    fp_keys = STATE.get('fp_keys', [])
    contacts = STATE.get('contacts', {})
    clip_row = {}
    if clip:
        clip_row = {
            'file': rel(clip_path), 'skeleton': SKELETON_UE, 'target': clip.target, 'summary': clip.summary,
            'fps': C.FPS, 'frames': clip.frames, 'keys': clip.frames + 1, 'seconds': round(clip.seconds, 3),
            'loop': False, 'root_motion': False, 'events': clip.events, 'turns': clip.turns, 'phases_s': clip.phases,
            'end_pose': 'image 90 = pose de repos (comme l\'image 0)',
            'qa': dict({k: v for k, v in clip.qa.items()}, contacts=contacts,
                       contacts_how='maillages déformés du bean (bean_soldier.blend) jouant le clip, rouleau et bande placés '
                                    'comme les attachments, à chaque image : clearance_cm = plus petite distance signée '
                                    'des sommets à la coque fermée Bean_Body (négatif = dans le corps ; manteau, gilet et '
                                    'baudriers sont 2 à 3 cm au-dessus : sous ~2,5 cm la pièce touche le manteau), '
                                    'overlap = paires de faces qui se croisent (BVH) avec chapeaux, bras gauche, bande ; '
                                    'rest_frame_0 = valeur en pose de repos (contacts déjà présents dans le modèle)'),
            'geometry_limit': (
                'un tour complet de 360 degrés de la mitaine autour de l\'avant-bras est hors de portée de ce rig : '
                'bras de 24 cm jusqu\'au poignet, manches de 7 cm de rayon, poitrine 23 à 29 cm devant la ligne des '
                'épaules ; la mitaine (6 cm de rayon) devrait passer entre avant-bras et poitrine, ce qui demande '
                'd\'étirer les bras d\'environ 2,2 (l\'étirement uniforme grossit aussi la manche) et fait croiser les '
                'deux manches. La boucle reste sur les côtés libres : par-dessus puis le long du côté extérieur '
                '(qa.loop_arc_deg dans le repère de l\'avant-bras, 0 = dessus, 90 = extérieur, 180 = dessous ; dessous, '
                'le ventre déborde et le rouleau toucherait le manteau), puis revient par l\'extérieur ; l\'avant-bras '
                'roule un peu contre elle. Les deux épaules avancent (10 cm), rentrent (6 cm) et descendent (1,7 cm) '
                'par des clés de translation sur upperarm_l / upperarm_r (manches et épaulettes pondérées à 95 % sur les '
                'os du bras : tout le bras glisse sur l\'oeuf). En 1re personne l\'arc va du dessus au dessous par le '
                'côté droit (first_person.qa).'),
            'unreal_import': 'commandlet comme les clips de capture (options de scripts/character/import_bean_library.py '
                             'task_for : skeleton = SK_Bean_Body_Skeleton, FBXALIT_EXPORTED_TIME, remove_redundant_keys '
                             'False) ; cible conseillée /Game/MonkeIsland/Bandage/Animations/ ; notifies WrapStart et '
                             'BandageDone à poser aux temps events[].time_s',
            'export': 'réglages des clips de la bibliothèque (rig.export_clip) : armature seule renommée Armature, '
                      'FBX_SCALE_NONE + apply_unit_scale, -Y avant / Z haut, os déformants, toutes clés cuites, os root à '
                      'x100 comme tous les A_Bean_* ; bras étirés uniformément (qa.max_stretch), mitaines gardées à leur '
                      'taille'}
    fp_rows = [{k: v for k, v in key.items() if not k.startswith('_')} for key in fp_keys]
    return {
        'format': 'bandage-kit/1', 'task': 'T-340 (lot L5), art pour l\'agent du code (ABandageItem, DA_BandageRules)',
        'status': 'built',
        'note': 'régénéré par scripts/props/bandage_kit/build.py (Blender -b) : noms d\'assets, de sockets et d\'événements '
                'du manifeste provisoire conservés ; durée, temps des événements et transformations d\'attache exacts ; '
                'rien n\'a été importé ni testé dans Unreal (runtime_tested false)',
        'built_by': 'scripts/props/bandage_kit/build.py',
        'built_at': dt.datetime.now().isoformat(timespec='seconds'), 'blender': bpy.app.version_string,
        'units': {
            'blender': 'mètres, Z haut, avant de l\'objet = +X',
            'unreal': 'cm ; Unreal = (x*100, -y*100, z*100) depuis Blender',
            'fbx': 'FBX_SCALE_NONE, global_scale 1, apply_unit_scale, -Y avant / Z haut (comme scripts/props/barque.py et '
                   'le kit de capture) : import échelle 1',
            'sockets': 'empties SOCKET_<nom> ; remettre relative_scale à 1 après l\'import',
            'collision': 'UCX_SM_Bandage_Roll_Pickup_00 (boîte) dans le FBX de l\'objet au sol : importer sans collision '
                         'auto ; rouleau tenu et bande sans collision (attachés, NoCollision)'},
        'assets': {
            'SM_Bandage_Roll': dict(assets.get('SM_Bandage_Roll', {}), role='rouleau de bande de lin tenu en main, un bout '
                                    'qui pend', pivot='centre du rouleau = socket grip',
                                    socket_roles={'grip': 'centre du rouleau (dans la mitaine droite)',
                                                  'tail': 'bout de la bande qui pend'}),
            'SM_Bandage_Roll_Pickup': dict(assets.get('SM_Bandage_Roll_Pickup', {}), role='le même rouleau couché au sol '
                                           '(objet ramassable), bout à plat', pivot='centre du dessous (Z = 0)'),
            'SM_Bandage_Wrapped': dict(assets.get('SM_Bandage_Wrapped', {}), role='bande enroulée (3 tours) autour de '
                                       'l\'avant-bras gauche', pivot='centre de la bande, axe de l\'avant-bras = +X',
                                       attach='sur l\'os lowerarm_l (attachments.wrapped_on_lowerarm_l) ; version 1re '
                                              'personne sur le bras gauche SM_Bean_FPArm '
                                              '(attachments.first_person.wrapped_on_left_fp_arm)')},
        'materials': {
            'M_Bandage_Linen': {'textures': {k: v for k, v in tex['Linen']['files'].items() if k in ('D', 'N', 'ORM')},
                                'source': tex['Linen']['source_url'], 'license': 'CC0 1.0',
                                'look': 'lin écru (toile tissée fine), mat'},
            'convention': 'D sRGB ; N DirectX (TC_Normalmap sans flip) ; ORM linéaire (TC_Masks, sRGB off) R = AO, G = '
                          'rugosité, B = métal (0) ; textures en répétition (UV en mètres / 0,12 m par tuile) : brancher '
                          'sur M_Character_Baked ou un maître équivalent ; les FBX ne portent que le nom d\'emplacement '
                          'M_Bandage_Linen'},
        'clip': {'A_Bean_Bandage_Apply': clip_row},
        'attachments': attachments,
        'first_person': {
            'format': 'FBeanArmPose (Location cm = épaule relative à la caméra, Rotation, Stretch), comme '
                      'MusketMath::SolveArm ; clés temporelles des deux bras ; gauche donnée en pose miroir pour '
                      'UBeanFirstPersonArmsComponent::SetPoseOverride',
            'fps': C.FPS, 'seconds': round(C.T['end'] / C.FPS, 3), 'events': clip.events if clip else [],
            'interpolation': 'une clé par image (30 i/s) : interpoler linéairement entre deux clés (pas de SmoothStep '
                             'MusketMath::FindKeys, qui marquerait un arrêt à chaque image)',
            'solve': {'right_shoulder_cm': list(FPm.FP['right_shoulder']), 'left_shoulder_cm': list(FPm.FP['left_shoulder']),
                      'hand_point_cm': list(FPm.FP['hand_point']), 'max_stretch': FPm.FP['max_stretch'],
                      'note': 'épaules et point de mitaine de MusketRules ; right / left = SolveArm(épaule, *_target_cm) '
                              '(left passé par MirrorArm) ; au-delà de MaxStretch, SolveArm fait glisser l\'épaule le long '
                              'du bras (Location des clés gauches) ; montée (0..WrapStart) et descente (BandageDone..fin) : '
                              'épaule et cible glissent vers la RestPose du composant (le bras vise toujours sa mitaine, '
                              'sans balayer l\'objectif) ; image 0 et image 90 = RestPose exacte des deux bras'},
            'rest_pose': {'location_cm': list(FPm.FP['rest_right'][0]), 'rotation_pyr_deg': list(FPm.FP['rest_right'][1])},
            'qa': STATE.get('fp_qa'),
            'qa_how': 'formes analytiques (tubes et sphères aux rayons mesurés de SM_Bean_FPArm : manchette 7,9 cm, mitaine '
                      '5,2 cm, rouleau 3 cm) entre WrapStart et BandageDone ; min_arm_axes_distance_cm < 0 = les deux '
                      'bras se touchent ; min_camera_distance_cm = surface la plus proche de la caméra',
            'keys': fp_rows},
        'icons': {'T_Icon_Bandage': 'art/ui/icons/bandage/T_Icon_Bandage.png',
                  'format': '512 x 512 RGBA, fond transparent, ombre douce (comme art/ui/icons/capture)',
                  'made_by': 'scripts/props/bandage_kit/build.py --icons (Blender, ombre en numpy)'},
        'previews': STATE.get('previews', []),
        'reimport_check': STATE.get('reimport'),
        'runtime_tested': False,
        'proof_level': 'Blender en arrière-plan seulement : rien importé ni testé dans Unreal',
    }


# ------------------------------------------------------------------------------------------------- main
def main():
    argv = sys.argv[sys.argv.index('--') + 1:] if '--' in sys.argv else []
    ap = argparse.ArgumentParser()
    for flag in ('textures', 'force-textures', 'models', 'anims', 'previews', 'icons', 'all'):
        ap.add_argument('--' + flag, action='store_true')
    a = ap.parse_args(argv)
    if a.all:
        a.textures = a.models = a.anims = a.previews = a.icons = True
    t0 = time.time()
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = 'SCN-BandageKit'
    scene.unit_settings.system = 'METRIC'
    scene.unit_settings.scale_length = 1.0
    scene.render.fps = C.FPS
    tex = T.build_all(force=a.force_textures)
    mats = {'M_Bandage_Linen': K.material('M_Bandage_Linen', T.texture_paths)}
    bean_coll, arm, bean_meshes = R.load_bean(scene)
    rig = R.BeanRig(arm, bean_meshes)
    STATE['rig'] = rig
    assets = build_models(scene, mats, rig)
    log(f'models: { {k: v["triangles"] for k, v in assets.items()} } in {time.time() - t0:.0f} s')
    clip_path = None
    if a.anims or a.previews:
        clip_path = build_clip(rig)
        contacts_qa(rig, bean_meshes)
        build_fp()
    if a.icons:
        icon()
    if a.previews:
        STATE['previews'] = previews(rig, bean_meshes)
        log(f'previews: {STATE["previews"]}')
    if a.anims:
        STATE['reimport'] = reimport_check()
    if a.models or a.anims:
        K.write_json(PROPS_OUT / 'manifest.json', manifest(assets, tex, clip_path))
        log(f'manifest written ({rel(PROPS_OUT / "manifest.json")})')
    log(f'finished in {time.time() - t0:.0f} s')


if __name__ == '__main__':
    main()
