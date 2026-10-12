"""Animation core of the T-334 packs (bolt-action rifle, sub-machine gun): the musket pack's helpers (easing, first
person camera, FP arm placement, TP two-segment arms, legs, baking: scripts/props/musket_pack/animation.py, loaded
unchanged under its own module name) and the generic per-frame loop of the weapon packs (after the machine gun pack's
animate): the weapon pose, the hands on their targets, the TP body, the moving parts of the weapon, the sockets, then
one action for the bean and one for the weapon.

A choreography module (the pack's animation.py) gives: FPS, CLIPS, LOOPS, weapon_pose(clip, f, view) (bean space),
clip_hands(clip, f, view, M) -> ((right point, rot), (left point, rot)) (rot: FP hand frame, None in TP),
parts(clip, f, view, M, hands) -> {bone: delta} (delta = weapon space transform applied to the bone's rest, or
('bean', matrix): bean space matrix of the bone, ('hide', delta): scaled to nothing), tp_body(arm, clip, f) (head,
spine; before the arm solve), events(clip), release_intervals(clip), TP_STRETCH.
Bean space: metres, the bean looks along -Y, its right side is -X, +Z up. Weapon space: +X muzzle, +Z up, -Y right.
"""
from __future__ import annotations

import importlib.util
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

MUSKET = Path(__file__).resolve().parents[1] / 'musket_pack'
if str(MUSKET) not in sys.path:
    sys.path.append(str(MUSKET))
_spec = importlib.util.spec_from_file_location('musket_animation_pa', MUSKET / 'animation.py')
MA = importlib.util.module_from_spec(_spec)
sys.modules['musket_animation_pa'] = MA
_spec.loader.exec_module(MA)

smooth, curve, segment, segment_os, frame, axes, blend, ease_os = MA.smooth, MA.curve, MA.segment, MA.segment_os, MA.frame, MA.axes, MA.blend, MA.ease_os
FP_CAMERA, FWD, RIGHT, UP, FP_LENS = MA.FP_CAMERA, MA.FWD, MA.RIGHT, MA.UP, MA.FP_LENS
camera_matrix, vp, vv, to_view, rotator_in_view = MA.camera_matrix, MA.vp, MA.vv, MA.to_view, MA.rotator_in_view
fp_arm_transform, solve_fp_arm, solve_tp_arm, solve_leg = MA.fp_arm_transform, MA.solve_fp_arm, MA.solve_tp_arm, MA.solve_leg
local_rot, set_matrices, bake_matrices, linear, tp_pose_at = MA.local_rot, MA.set_matrices, MA.bake_matrices, MA.linear, MA.tp_pose_at
HIDDEN = Matrix.Diagonal((1e-4, 1e-4, 1e-4, 1.0))


def view_pose_dirs(anchor_local, anchor_cm, x_dir, up_dir):
    """Weapon root matrix (bean space): the weapon point anchor_local at anchor_cm in the view (cm; X forward, Y right,
    Z up), the barrel along x_dir and the top of the weapon toward up_dir (view directions)."""
    x = vv(*x_dir)
    z = vv(*up_dir)
    z = (z - x * x.dot(z)).normalized()
    y = z.cross(x)
    R = Matrix((x, y, z)).transposed()
    m = R.to_4x4()
    m.translation = vp(*anchor_cm) - R @ Vector(anchor_local)
    return m


def keyed_pose(f, keys, poses, over=0.05, antic=0.03):
    """Blend between named poses along (frame, name) keys with the Call of Duty easing."""
    i, w = segment_os(f, keys, over, antic)
    a = poses[keys[i][1]]
    b = poses[keys[min(i + 1, len(keys) - 1)][1]]
    return blend(a, b, w)


def hand_frame(point, elbow_cm, back):
    d = (point - vp(*elbow_cm)).normalized()
    return axes(d, vv(*back))


def keyed_hand(keys, f, view, resolve):
    """Hand centre (bean space) and FP hand rotation (None in TP) at frame f along keys (frame, target, FP elbow (view
    cm), FP back of the hand (view dir)); resolve(target, view) -> bean space point, or (point, (fp_offset, tp_offset)):
    the hand centre is then moved by -rot @ fp_offset (hand frame: x fingers, y thumb side, z back of the hand) in FP
    and by -tp_offset (bean space) in TP, so the grasped point sits at the fingers (a knob, a clip)."""
    i, w = segment(f, keys)
    k0, k1 = keys[i], keys[min(i + 1, len(keys) - 1)]
    out = []
    for k in (k0, k1):
        p = resolve(k[1], view)
        off = None
        if isinstance(p, tuple):
            p, off = p
        rot = hand_frame(p, k[2], k[3]) if view == 'FP' else None
        if off is not None:
            p = p - (rot @ Vector(off[0]) if rot is not None else Vector(off[1]))
        out.append((p, rot))
    p = out[0][0].lerp(out[1][0], w)
    rot = None
    if view == 'FP':
        rot = out[0][1].to_quaternion().slerp(out[1][1].to_quaternion(), w).to_matrix()
    return p, rot


def kick_curve(f, f0, settle=12):
    """Recoil envelope: anticipation, sharp snap at f0 + 1, overshoot past the rest and settle."""
    s = settle / 12.0
    return curve(f, [(f0 - 1, 0), (f0, 0.15), (f0 + 1, 1.0), (f0 + 1 + 2 * s, 0.42), (f0 + 1 + 4 * s, -0.12), (f0 + 1 + 7 * s, 0.04),
                     (f0 + 1 + 10 * s, -0.01), (f0 + 1 + 12 * s, 0)])


def walk_legs(arm, clip, f, end):
    """The legs of Walk / Sprint (as the musket and the machine gun): four steps over the clip; returns the phase."""
    phase = 2 * math.pi * f / end * 4
    amp = 18 if clip == 'Walk' else 28
    for side, off in (('l', 0), ('r', math.pi)):
        swing = math.sin(phase + off)
        local_rot(arm, 'thigh_' + side, (1, 0, 0), amp * swing)
        local_rot(arm, 'calf_' + side, (1, 0, 0), -max(0, -swing) * (25 if clip == 'Walk' else 43))
        local_rot(arm, 'foot_' + side, (1, 0, 0), -amp * swing * .5)
    arm.pose.bones['root'].location = (0, 0, .006 + .007 * (1 - math.cos(2 * phase)))
    arm.pose.bones['pelvis'].location = (0, -.027, 0)
    return phase


def local_rots(arm, bone, rotations):
    """Several rotations (armature space axis, degrees) composed on one bone, the first applied last."""
    p = arm.pose.bones[bone]
    p.rotation_mode = 'QUATERNION'
    q = Quaternion()
    inv = p.bone.matrix_local.to_3x3().inverted()
    for axis, ang in rotations:
        q = q @ Quaternion(inv @ Vector(axis), math.radians(ang))
    p.rotation_quaternion = q


def head_turn(arm, yaw, down=0.0):
    """TP head: yaw about the vertical (the musket family looks 30 deg toward its left, along the gun), then down."""
    local_rot(arm, 'head', (0, 0, 1), yaw)
    if down:
        pb = arm.pose.bones['head']
        pb.rotation_quaternion = pb.rotation_quaternion @ Quaternion(pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0)),
                                                                     math.radians(down))


def animate(scene, bean, weapon, clip, view, C, tag):
    arm = bean['armature']
    gun = weapon['armature']
    end = C.CLIPS[clip]
    scene.frame_start = 0
    scene.frame_end = end
    scene.render.fps = C.FPS
    bpy.context.window.scene = scene
    bean_samples, gun_samples, grips = [], [], []
    errors = {'l': 0., 'r': 0.}
    stretches = []
    bone_names = [pb.name for pb in gun.pose.bones]
    rest = {n: gun.data.bones[n].matrix_local.copy() for n in bone_names}
    if hasattr(C, 'begin'):
        C.begin(clip, view)
    for f in range(end + 1):
        scene.frame_set(f)
        for pb in arm.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        phase = 0.0
        if clip in ('Walk', 'Sprint'):
            phase = walk_legs(arm, clip, f, end)
        else:
            arm.pose.bones['root'].location = (0, 0, .003)
        if view == 'TP':
            C.tp_body(arm, clip, f)
        M = C.weapon_pose(clip, f, view)
        (rp, rrot), (lp, lrot) = C.clip_hands(clip, f, view, M)
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev_arm = arm.evaluated_get(dg)
        bm = {p.name: p.matrix.copy() for p in ev_arm.pose.bones}
        if clip in ('Walk', 'Sprint'):
            for side, off in (('l', 0), ('r', math.pi)):
                step = phase + off
                v = arm.data.bones['foot_' + side].head_local.copy()
                v.y = .035 * math.cos(step)
                v.z = .053 + (max(0, math.sin(step)) * (.024 if clip == 'Walk' else .040))
                solve_leg(arm, bm, side, v)
        for side, point, rot in (('r', rp, rrot), ('l', lp, lrot)):
            if view == 'FP':
                error, scale = solve_fp_arm(arm, bm, side, point, rot)
            else:
                error, scale = solve_tp_arm(arm, bm, side, point, max_stretch=C.TP_STRETCH)
            errors[side] = max(errors[side], error)
            stretches.append(scale)
        if view == 'TP' and clip in ('Walk', 'Sprint'):
            set_matrices(arm, bm)
            bpy.context.view_layer.update()
            dg = bpy.context.evaluated_depsgraph_get()
            minimum = 1e6
            for ob in bean['meshes']:
                if 'Boot' not in ob.name:
                    continue
                evob = ob.evaluated_get(dg)
                me = evob.to_mesh()
                minimum = min(minimum, min((evob.matrix_world @ v.co).z for v in me.vertices))
                evob.to_mesh_clear()
            lift = max(0., .003 - minimum)
            T = Matrix.Translation((0, 0, lift))
            bm = {name: T @ matrix for name, matrix in bm.items()}
            M = T @ M
            rp = rp + Vector((0, 0, lift))
            lp = lp + Vector((0, 0, lift))
        bean_samples.append(bm)
        grips.append(dict(frame=f, left=[*lp], right=[*rp]))
        mats = {n: M @ rest[n] for n in bone_names}
        for n, D in C.parts(clip, f, view, M, {'r': rp, 'l': lp, 'rrot': rrot, 'lrot': lrot}).items():
            if n not in mats:
                continue
            if isinstance(D, tuple) and D[0] == 'bean':
                mats[n] = D[1] @ rest[n]
            elif isinstance(D, tuple) and D[0] == 'hide':
                mats[n] = M @ D[1] @ rest[n] @ HIDDEN
            elif isinstance(D, tuple) and D[0] == 'bean_hide':
                mats[n] = D[1] @ rest[n] @ HIDDEN
            else:
                mats[n] = M @ D @ rest[n]
        for n in bone_names:
            b = gun.data.bones[n]
            if b.parent and (n.startswith('socket_') or not b.use_deform):
                mats[n] = mats[b.parent.name] @ rest[b.parent.name].inverted() @ rest[n]
        gun_samples.append(mats)
    ba = bake_matrices(arm, bean_samples, f'A_Bean_{tag}_{view}_{clip}')
    ga = bake_matrices(gun, gun_samples, f'A_{tag}_{view}_{clip}')
    scene.frame_set(0)
    bpy.context.view_layer.update()
    return dict(bean_action=ba.name, weapon_action=ga.name, events=C.events(clip), hand_error_max_m=errors,
                upperarm_scale_max=max(stretches), frame_start=0, frame_end=end, fps=C.FPS, loop=clip in C.LOOPS,
                authored_method=C.AUTHORED, grip_targets=grips)
