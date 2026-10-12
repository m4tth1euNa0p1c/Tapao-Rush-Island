"""Parametrises the musket's animation module (scripts/props/musket_pack/animation.py, v2 of T-246) with a weapon
profile (weapon_profile.py) for the long arm packs (T-250):

  - the whole reload is re-timed per weapon (local frames; a time warp built from the musket's current reload windows
    reaches its hammer / frizzen keys and its third person look-down) with the weapon's own grips, muzzle, pan,
    ramrod and cartridge; the right / left hand keys and the ramrod keys come from the profile;
  - Inspect and Discard reuse the musket's choreography (keyed_pose easing, look / show_barrel / throw poses given
    with the weapon's own points, Empty at EMPTY_FRAME, Release at RELEASE_FRAME, the weapon falling on after it);
  - aiming with the line of sight on the first person camera axis, weapon centred (profile.fp_poses['aim'], computed
    from the two sight points): AimIn (hip -> aim with a settle), AimIdle (light breathing on the sights), AimOut, and
    an AimFire clip (shot from the aim pose) so the game never plays the hip shot aimed;
  - Call of Duty style motion on the other clips: breathing in Idle / Hold, figure-of-eight sway in Walk, low canted
    weapon with a marked bob in Sprint, a sharp recoil (pitch snap, slide back, roll) that overshoots on its way back
    (Fire, AimFire, Recoil; per weapon profile.recoil), settles on the arrivals.

apply(profile) replaces the module attributes animation.py reads at call time; nothing in the musket pack is edited.
"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix, Quaternion, Vector

from musket_ref import A

P = None
ORIG = {}
CURRENT = {}        # gun armature, clip and view of the scene being animated (set by pack_build before A.animate)
EXTRA = {}          # frame -> {'_M': weapon root matrix, bone: armature space matrix} of the extra bones
GRIP_U, FORE_U = (0, .55, .85), (0, -.45, -.89)
RELOAD_ORDER = ['HalfCock', 'PrimePan', 'CloseFrizzen', 'Powder', 'BallAndPatch', 'DrawRamrod', 'TurnRamrod', 'RamrodStroke',
                'ReturnRamrod', 'Cock']


def rot(deg, axis):
    return Matrix.Rotation(math.radians(deg), 4, axis)


# ------------------------------------------------------------------------------------------------- timeline

def local_frame(clip, f):
    """Local reload frame shown by clip at f (None for the other clips)."""
    if clip == 'GroundReload':
        return f
    if clip in P.windows:
        a, b = P.windows[clip]
        return a + (b - a) * f / A.CLIPS[clip]
    return None


def reload_frame(clip, f):
    """What the musket's own keys (hammer, frizzen, look-down) read: the master frame of the local one."""
    g = local_frame(clip, f)
    return None if g is None else P.warp(g)


def build_warp(profile, musket_windows):
    """Local reload frames -> musket master frames, window boundary to window boundary."""
    pairs = [(profile.windows[k][0], musket_windows[k][0]) for k in RELOAD_ORDER if k in profile.windows and k in musket_windows]
    pairs.append((profile.reload_frames, musket_windows['Cock'][1]))
    return pairs


# ------------------------------------------------------------------------------------------------- poses

def tp_poses():
    tp = ORIG['tp_poses']()
    tp['load'] = A.tp_pose_at(P.muzzle, P.tp_load['anchor'], P.tp_load['dir'], P.tp_load['roll'])
    tp['base'] = tp['ready']
    tp['sprint'] = Matrix.Translation((0, 0, -0.05)) @ tp['ready'] @ rot(14, 'Y') @ rot(-10, 'X')
    for name, (anchor, where, direction, roll) in getattr(P, 'tp_extra', {}).items():
        tp[name] = A.tp_pose_at(anchor, where, direction, roll)
    return tp


def reload_pose(f, view):
    m = A.keyed_pose(f, P.pose_keys, view, over=0.07, antic=0.04)
    dip = sum(A.curve(f, [(a, 0), (b, 1), (c, 0)]) * amp for a, b, c, amp in P.life_dips)
    pour = sum(A.curve(f, [(a, 0), (b, 1), (c, 0)]) * amp for a, b, c, amp in P.life_pours)
    return Matrix.Translation((0, 0, -dip)) @ m @ rot(pour, 'X')


def settle(m, f, f0, amp):
    """Damped bounce after an arrival at f0 (overshoot then settle), along the weapon's up."""
    v = A.curve(f, [(f0, 0), (f0 + 3, 1.0), (f0 + 6, -0.38), (f0 + 9, 0.12), (f0 + 12, 0)]) * amp
    return m @ Matrix.Translation((0, 0, v))


def breathe(m, f, end, s, aim=False):
    period = end if end <= 48 else end / 2
    q = math.sin(2 * math.pi * f / period)
    s2 = math.sin(2 * math.pi * f / end)
    if aim:
        return m @ Matrix.Translation((0, 0.0004 * s2, 0.0012 * q)) @ rot(-0.18 * q, 'Y') @ rot(0.12 * s2, 'Z')
    return m @ Matrix.Translation((0.0015 * s2 * s, 0.0015 * q * s, 0.0036 * q * s)) @ rot(-0.7 * q * s, 'Y') @ rot(0.5 * s2 * s, 'X')


def sway(m, f, end, amp):
    ph = 2 * math.pi * f / end * 4                       # four steps, as the legs of animation.animate
    x, y, z = 0.003 * amp * math.sin(ph), 0.006 * amp * math.sin(ph / 2), 0.005 * amp * math.sin(ph + 0.6)
    return (m @ Matrix.Translation((x, y, z)) @ rot(2.5 * amp * math.sin(ph / 2 - 0.4), 'X')
            @ rot(1.4 * amp * math.sin(ph + 0.6), 'Y'))


def kick_curve(f, f0):
    """Recoil envelope: anticipation, sharp snap at f0 + 2, overshoot past the rest and settle by f0 + 16."""
    return A.curve(f, [(f0 - 2, 0), (f0, -0.05), (f0 + 2, 1.0), (f0 + 5, 0.40), (f0 + 8, -0.12), (f0 + 11, 0.05), (f0 + 14, -0.015),
                       (f0 + 16, 0)])


def kick(m, k, s=1.0):
    r = P.recoil
    return (m @ Matrix.Translation((-r['back'] * k * s, 0, r['up'] * k * s)) @ rot(-r['pitch'] * k * s, 'Y')
            @ rot(r['roll'] * k * s, 'X') @ rot(r['yaw'] * k * s, 'Z'))


def weapon_pose(clip, f, view):
    if clip == 'GroundReload' or clip in P.windows or clip in ('Inspect', 'Discard', 'Equip', 'Holster', 'Present', 'ReturnToPort'):
        base = ORIG['weapon_pose'](clip, f, view)
        if clip == 'Equip':
            base = settle(base, f, A.CLIPS[clip] - 10, 0.010)
        return base
    end = A.CLIPS[clip]
    p = A.poses(view)
    hip = p['base'] if view == 'FP' else p['ready']
    aim = p['aim']
    if clip == 'AimIn':
        base = A.blend(hip, aim, A.ease_os(0, end - 6, f, over=0.05, antic=0.03))
        base = settle(base, f, end - 7, 0.006)
    elif clip == 'AimOut':
        base = A.blend(aim, hip, A.ease_os(0, end - 4, f, over=0.05, antic=0.03))
    elif clip in ('AimIdle', 'AimFire'):
        base = aim
    elif clip == 'Sprint':
        base = p['sprint']
    else:
        base = hip
    if clip in ('Idle', 'Hold'):
        base = breathe(base, f, end, 1.0)
    if clip == 'AimIdle':
        base = breathe(base, f, end, 0.25, aim=True)
    if clip == 'Walk':
        base = sway(base, f, end, 1.0)
    if clip == 'Sprint':
        base = sway(base, f, end, 2.2)
        if view == 'TP':
            base = base @ rot(12, 'Y')
    if clip in ('Fire', 'AimFire'):
        base = kick(base, kick_curve(f, 8), 1.0 if clip == 'Fire' else 0.8)
    if clip == 'Recoil':
        base = kick(base, kick_curve(f + 2, 0))
    return base


# ------------------------------------------------------------------------------------------ flintlock parts

def hammer_angle(clip, f):
    return ORIG['hammer_angle']('Fire' if clip == 'AimFire' else clip, f)


def frizzen_angle(clip, f):
    return ORIG['frizzen_angle']('Fire' if clip == 'AimFire' else clip, f)


def trigger_angle(clip, f):
    return ORIG['trigger_angle']('Fire' if clip == 'AimFire' else clip, f)


# ------------------------------------------------------------------------------------------------- ramrod

def rod_state(g):
    """Ramrod at local reload frame g: (rotation, head point) in weapon local and the rod coordinate the right hand
    holds (metres from the head end), None when stowed. Same mechanics as the musket's, keyed by the profile's
    rod phases; every phase keys the hand's hold on the rod too, so a short loader works a long rod hand over hand
    (the hand stays near the muzzle, the rod slides through it)."""
    R = P.rod
    if g is None or g < R['pull_s'][0][0] or g >= R['home_s'][-1][0]:
        return None
    L, HEAD, DIR = P.rod_len, P.rod_head, P.rod_dir

    def stow(s):
        return Matrix.Identity(3), HEAD + DIR * s

    half_turn = Matrix.Rotation(math.pi, 3, 'Y')
    bore_rot = (half_turn @ DIR).rotation_difference(Vector((-1, 0, 0))).to_matrix() @ half_turn

    def in_bore(depth):
        tip = Vector((P.muzzle.x - 0.006 - depth, 0.0, P.bore_z))
        return bore_rot, tip - bore_rot @ DIR * L

    def turned(rot0, head0, hold, angle):
        pivot = head0 + rot0 @ DIR * hold
        r = Matrix.Rotation(math.radians(angle), 3, 'Y')
        return r @ rot0, pivot + r @ (head0 - pivot)

    def towards(a, b, w):
        return (a[0].to_quaternion().slerp(b[0].to_quaternion(), w).to_matrix(), a[1].lerp(b[1], w))

    d1 = R['pull_s'][-1][0]
    t0, t1 = R['turn']
    e0, e1 = R['enter']
    hold_out = R['pull_hold'][-1][1]
    s_out = R['pull_s'][-1][1]
    if g < d1:
        return (*stow(A.curve(g, R['pull_s'])), A.curve(g, R['pull_hold']))
    out = stow(s_out)
    if g < t1:
        return (*turned(*out, hold_out, A.curve(g, [(t0, 0.0), (t1 - 1, 180.0), (t1, 180.0)])), hold_out)
    ram_hold0 = R['ram_hold'][0][1]
    if g < e1:
        over = in_bore(-0.06)
        r_, h_ = towards(turned(*out, hold_out, 180.0), over, A.smooth(e0, e1 - 1, g))
        if g > e1 - 1:
            r_, h_ = in_bore(-0.06 + 0.06 * A.smooth(e1 - 1, e1, g))
        return r_, h_, hold_out + (ram_hold0 - hold_out) * A.smooth(e0, e1, g)
    s_end = R['ram_depth'][-1][0]
    if g < s_end:
        return (*in_bore(A.curve(g, R['ram_depth'])), A.curve(g, R['ram_hold']))
    drawn = in_bore(R['ram_depth'][-1][1])
    hold_last = R['ram_hold'][-1][1]
    b0, b1 = R['turnback']
    if g < b1:
        return (*turned(*drawn, hold_last, -A.curve(g, [(b0, 0.0), (b1, 180.0)])), hold_last)
    back = turned(*drawn, hold_last, -180.0)
    s_back = (back[1] - HEAD).dot(DIR)
    h0 = R['home_s'][0][0]
    s = A.curve(g, [(fr, frac * s_back) for fr, frac in R['home_s']])
    r_, h_ = towards(back, stow(s_back), A.smooth(h0, h0 + 0.5, g))
    if g > h0 + 0.5:
        r_, h_ = stow(s)
    return r_, h_, A.curve(g, R['home_hold'])


def resolve_target(spec, view, M, g, rod):
    kind = spec[0]
    if kind == 'w' or kind == 'pinch':
        return M @ Vector(spec[1]), kind == 'pinch'
    if kind == 'v':
        return (A.vp(*spec[1]) if view == 'FP' else Vector(spec[2])), False
    if kind == 'rod':
        if rod is None:
            before = g is None or g < P.rod['turn'][0]
            return M @ (P.rod_head + P.rod_dir * (P.rod_grab_before if before else P.rod_grab_after)), False
        return M @ A.rod_point(rod, rod[2]), False
    raise ValueError(spec)


# ------------------------------------------------------------------------------------------------- hands

def clip_hands(clip, f, view, M):
    g = local_frame(clip, f)
    if g is not None:
        rod = rod_state(g)
        right = A.keyed_hand(P.right_keys(), g, view, M, 'r', g, rod)
        left = A.keyed_hand(P.left_keys(view), g, view, M, 'l', g, rod)
        return right, left, rod
    if clip == 'AimFire':
        clip = 'Fire'
    return ORIG['clip_hands'](clip, f, view, M)


# ------------------------------------------------------------------------------------------ cartridge & extras

def cartridge_world(clip, f, view, M, right_point, right_rot, rest_matrix):
    g = local_frame(clip, f)
    extra = P.extra_bones(g, M, right_point, right_rot, view, lambda n: CURRENT['gun'].data.bones[n].matrix_local)
    EXTRA[f] = dict(extra, _M=M.copy())
    if g is None or g < P.cart_show or g >= P.cart_hide:
        return M @ rest_matrix
    across = right_rot.col[1].xyz if right_rot is not None else Vector((1, 0, 0))
    barrel = (M.to_3x3() @ Vector((1, 0, 0))).normalized()
    named = {'across': across, 'down': -A.UP, 'up': A.UP, 'barrel': barrel, '-barrel': -barrel}
    keys = [(fr, named[name]) for fr, name in P.cart_axis_keys]
    i, w = A.segment(g, keys)
    a, b = keys[i][1], keys[min(i + 1, len(keys) - 1)][1]
    axis = a.lerp(b, w)
    if axis.length < 1e-4:
        axis = b
    axis.normalize()
    pinch = pinch_point(right_point, right_rot)
    rot3 = A.axes(axis, A.UP if abs(axis.dot(A.UP)) < 0.95 else A.FWD)
    if P.cart_into and g >= P.cart_into[0]:
        into = A.smooth(P.cart_into[0], P.cart_into[1], g)
        bore = M @ Vector((P.muzzle.x - P.cart_into_depth, 0.0, P.bore_z))
        pinch = pinch.lerp(bore, into)
        rot3 = Quaternion(rot3.to_quaternion()).slerp(
            A.axes((M.to_3x3() @ Vector((-1, 0, 0))).normalized(), M.to_3x3() @ Vector((0, 0, 1))).to_quaternion(), into).to_matrix()
    delta = rot3.to_4x4()
    delta.translation = pinch
    return delta @ Matrix.Translation(-P.cartridge_rest) @ rest_matrix


def pinch_point(right_point, right_rot):
    return right_point + (right_rot @ A.PINCH if right_rot is not None else A.TP_PINCH)


# ------------------------------------------------------------------------------------------------- events

def events(clip):
    def ev(frame_, name, socket=None):
        e = {'frame': int(frame_), 'name': name}
        if socket:
            e['socket'] = socket
        return e
    if clip in ('Fire', 'AimFire'):
        return ORIG['events']('Fire')
    if clip == 'GroundReload':
        return [ev(*e) for e in P.ground_events]
    if clip in P.windows:
        a, b = P.windows[clip]
        n = A.CLIPS[clip]
        return [ev(int(round((fr - a) * n / (b - a))), name, socket) for fr, name, socket in P.ground_events if a <= fr < b]
    return ORIG['events'](clip)


# ------------------------------------------------------------------------------------------- post passes

def bake_extra(gun, action_name):
    """Keys the extra bones of the weapon (rigid on the root by animation.animate) from the matrices recorded by
    cartridge_world, in the action animate just baked."""
    if not P.extra_bone_names or not EXTRA:
        return
    act = bpy.data.actions[action_name]
    for f in sorted(EXTRA):
        mats = EXTRA[f]
        M = mats['_M']
        for name in P.extra_bone_names:
            if name not in mats:
                continue
            pb = gun.pose.bones[name]
            parent = pb.parent
            pb.matrix_basis = pb.bone.convert_local_to_pose(mats[name], pb.bone.matrix_local, parent_matrix=M @ parent.bone.matrix_local,
                                                            parent_matrix_local=parent.bone.matrix_local, invert=True)
            for prop in ('location', 'rotation_quaternion', 'scale'):
                pb.keyframe_insert(prop, frame=f, group=pb.name)
    A.linear(act)


def verify_extra(scene, gun):
    """Largest deviation (m) between the recorded extra bone matrices and the baked pose, over three frames."""
    if not P.extra_bone_names or not EXTRA:
        return None
    frames = sorted(EXTRA)
    worst = 0.0
    for f in (frames[len(frames) // 3], frames[len(frames) // 2], frames[-1]):
        scene.frame_set(f)
        bpy.context.view_layer.update()
        for name in P.extra_bone_names:
            if name in EXTRA[f]:
                worst = max(worst, (gun.pose.bones[name].matrix.translation - EXTRA[f][name].translation).length)
    return round(worst, 5)


def head_pass(arm, action_name, clip, view, end):
    """Third person head of AimFire: down the sights, as AimIdle (animation.animate knows no AimFire)."""
    if view != 'TP' or clip != 'AimFire':
        return
    act = bpy.data.actions[action_name]
    pb = arm.pose.bones['head']
    for f in range(end + 1):
        A.local_rot(arm, 'head', (0, 0, 1), 55.0)
        pb.keyframe_insert('rotation_quaternion', frame=f, group='head')
    A.linear(act)


# ------------------------------------------------------------------------------------------------- framing

def fp_framing():
    """What the game needs to frame the FP clips like this pack's camera (DA_Pirate<Weapon>Rules)."""
    base = A.fp_pose('base')
    pitch, yaw, roll = A.rotator_in_view(base)
    grip = A.to_view(base @ P.grip_r)
    aim = A.fp_pose('aim')
    a_pitch, a_yaw, a_roll = A.rotator_in_view(aim)
    muzzle = A.to_view(aim @ P.muzzle)
    w0, w1 = P.windows['PrimePan'], P.windows['RamrodStroke']
    gestures = {
        'Idle': A.gesture_framing('Idle (hip, frame 0 = base pose; Walk, Fire, Hold, Equip end...)', base),
        'AimIdle': A.gesture_framing('AimIdle frame 0 (authored aim pose: sight line on the view axis; AimIn / AimOut / AimFire)', aim),
        'Sprint': A.gesture_framing('Sprint', A.fp_pose('sprint')),
        'GroundReload@prime': A.gesture_framing(f'GroundReload frame {w0[0] + 12} (prime)', A.reload_pose(w0[0] + 12, 'FP')),
        'GroundReload@load': A.gesture_framing(f'GroundReload frame {P.windows["Powder"][0] + 14} (load)',
                                               A.reload_pose(P.windows['Powder'][0] + 14, 'FP')),
        'GroundReload@ram': A.gesture_framing(f'GroundReload frame {w1[0] + 6} (ramrod stroke)', A.reload_pose(w1[0] + 6, 'FP')),
        'Inspect@30': A.gesture_framing('Inspect frame 30 (lock)', A.inspect_pose(30, 'FP')),
        'Inspect@70': A.gesture_framing('Inspect frame 70 (barrel)', A.inspect_pose(70, 'FP')),
        'Discard@Release': A.gesture_framing(f'Discard frame {A.RELEASE_FRAME} (Release)', A.discard_pose(A.RELEASE_FRAME, 'FP')),
    }
    if 'box' in P.windows or 'box' in P.fp_poses:
        gestures['GroundReload@box'] = A.gesture_framing('GroundReload (patch box pose)', A.fp_pose('box'))
    return dict(camera_location_m=[*A.FP_CAMERA], horizontal_fov_deg=90.0,
                hip_grip_location_cm=[round(v, 2) for v in grip], hip_rotation_pyr_deg=[round(v, 3) for v in (pitch, yaw, roll)],
                aim_muzzle_location_cm=[round(v, 2) for v in muzzle], aim_rotation_pyr_deg=[round(v, 3) for v in (a_pitch, a_yaw, a_roll)],
                aim_field_of_view_deg=P.aim_fov_deg,
                sight_line=dict(rear_m=[*P.sight_rear], front_m=[*P.sight_front], eye_to_rear_cm=P.aim_eye_cm,
                                rear_in_view_cm=[round(v, 2) for v in A.to_view(aim @ P.sight_rear)],
                                front_in_view_cm=[round(v, 2) for v in A.to_view(aim @ P.sight_front)],
                                pitch_deg=round(P.aim_pitch(), 3)),
                gestures=gestures,
                note='HipGripLocation / HipRotation reproduce this camera exactly (Idle frame 0 = FP base pose). The aim clips '
                     '(AimIn, AimIdle, AimFire, AimOut) are AUTHORED with the line of sight (socket_sight_rear -> '
                     'socket_sight_front) on the view axis, weapon centred: AimMuzzleLocation / AimRotation set to aim_* keep '
                     'the game framing at zero offset (AFirearmWeapon measures AimIdle frame 0). The shot aimed is the '
                     'AimFire clip (no alias of Fire). setup_pirate_weapons.py writes the hip values only: copy aim_* by hand.')


# ------------------------------------------------------------------------------------------------- apply

PATCHED = ['weapon_pose', 'reload_frame', 'reload_pose', 'hammer_angle', 'frizzen_angle', 'trigger_angle', 'resolve_target',
           'clip_hands', 'cartridge_world', 'events', 'tp_poses', 'rod_state', 'fp_framing']


def apply(profile):
    """Installs the profile in the musket's animation module (idempotent for the same profile)."""
    global P
    if not ORIG:
        for name in PATCHED:
            ORIG[name] = getattr(A, name)
        ORIG['RELOAD_WINDOWS'] = dict(A.RELOAD_WINDOWS)
        ORIG['FP_POSES'] = dict(A.FP_POSES)
        ORIG['CLIPS'] = dict(A.CLIPS)
    P = profile
    P.master_warp = build_warp(P, ORIG['RELOAD_WINDOWS'])
    A.CLIPS = dict(P.clips)
    A.RELOAD_WINDOWS = dict(P.windows)
    A.GRIP_R, A.GRIP_L, A.BAND_GRIP = P.grip_r.copy(), P.grip_l.copy(), P.band_grip.copy()
    A.MUZZLE, A.BORE_Z, A.PAN = P.muzzle.copy(), P.bore_z, P.pan.copy()
    A.HAMMER_SPUR, A.FRIZZEN_TOE = P.hammer_spur.copy(), P.frizzen_toe.copy()
    A.ROD_HEAD, A.ROD_DIR, A.ROD_LEN = P.rod_head.copy(), P.rod_dir.copy(), P.rod_len
    A.R_GRIP = ('w', P.grip_r.copy())
    A.FP_POSES = dict(ORIG['FP_POSES'])
    A.FP_POSES.update(P.fp_poses)
    A.GROUND_RELOAD_EVENTS = list(P.ground_events)
    for name in PATCHED:
        setattr(A, name, globals()[name])
