"""Bean naval pistol handling, first (FP) and third (TP) person, baked on the bean skeleton and the pistol skeleton
(T-248, Call-of-Duty style: anticipation / overshoot / settle, 30 fps).

Fifteen gestures (the fourteen of the v0.1 pack + Discard, owner 2026-10-09), same bone, socket and event names as
v0.1 so the C++ (EFirearmGesture, PirateWeaponClips.inl regenerated from the manifest) and the importer keep working.

First person: authored through the game's first person camera (rig_solver.FP_CAMERA, 90 deg horizontal), every FP
weapon pose given in view coordinates (cm, X forward / Y right / Z up, FRotator pitch / yaw / roll). The pistol is
one-handed: the right FP arm holds the grip, the left arm stays below the frame except in Reload, Inspect and Discard.
Aim clips put the sight line (rear notch -> front bead) on the camera axis, weapon centred (owner request).
Third person: weapon poses in bean space, analytic two-segment arms, head looking at the pistol when it is handled.
"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix, Quaternion, Vector

import pistol_model as PM
from rig_solver import (FWD, RIGHT, UP, FP_CAMERA, FP_LENS, REST_HAND, axes, bake_matrices, blend, camera_matrix, curve,
                        frame, local_rot, point_curve, rotator_in_view, segment, set_matrices, shoulder_point, smooth,
                        solve_fp_arm, solve_leg, solve_tp_arm, to_view, view_pose, vp, vv)

FPS = 30
CLIPS = {'Equip': 36, 'Idle': 90, 'Walk': 120, 'Sprint': 90, 'AimIn': 12, 'AimIdle': 90, 'AimOut': 12, 'Fire': 36,
         'AimFire': 36, 'Reload': 135, 'DryFire': 24, 'Inspect': 150, 'Holster': 36, 'Cock': 30, 'Discard': 36}
LOOPS = {'Idle', 'Walk', 'Sprint', 'AimIdle'}
V01_CLIPS = ['Equip', 'Idle', 'Walk', 'Sprint', 'AimIn', 'AimIdle', 'AimOut', 'Fire', 'AimFire', 'Reload', 'DryFire',
             'Inspect', 'Holster', 'Cock']

GRIP_R = Vector((0.0, 0.0, 0.0))
SIGHT = Vector((0.040, 0.0, PM.SIGHT_LINE_Z))            # rear notch: on the camera axis when aiming
COMB = Vector((0.014, -0.030, 0.119))                      # thumb piece of the (right) cock at full cock
ROD_TIP0 = Vector((PM.ROD_X1, 0.0, PM.ROD_Z))
ROD_LEN = PM.ROD_X1 - PM.ROD_X0
REACH = {'r': (14.0, 18.0, -50.0), 'l': (4.0, -26.0, -50.0)}          # FP elbows (view cm) for the ordinary hold
FP_SHOULDER = {'r': (-4.0, 20.0, -36.0), 'l': (-4.0, -20.0, -36.0)}   # invisible shoulders of the FP arms
FP_LEFT_FREE = (24.0, -26.0, -44.0)                                   # left mitten below the frame
FP_PINCH = Vector((0.085, 0.012, 0.004))                              # cartridge in the hand frame (fingers, thumb, back): past the mitten
TP_PINCH_ALONG = 0.085                                                # TP: past the mitten along the forearm direction
TP_SHOULDER_L = Vector((0.2, 0.0, 0.625))

# ------------------------------------------------------------------------------------------------- poses

# FP: (anchor weapon point, where in the view (cm), FRotator)
FP_POSES = {
    'ready': (GRIP_R, (26.0, 10.0, -15.0), (3.0, -6.0, -5.0)),
    'aim': (SIGHT, (28.0, 0.0, 0.0), (0.0, 0.0, 0.0)),
    'reload': (GRIP_R, (26.0, 5.0, -16.0), (35.0, -42.0, -72.0)),
    'sprint': (GRIP_R, (26.0, 12.0, -14.0), (-35.0, -28.0, -24.0)),
    'holster': (GRIP_R, (14.0, 22.0, -58.0), (-70.0, 20.0, -30.0)),
    'inspect_a': (GRIP_R, (24.0, 3.0, -11.0), (22.0, -30.0, -78.0)),
    'inspect_b': (GRIP_R, (24.0, 6.0, -10.0), (15.0, -95.0, 10.0)),
    'look': (GRIP_R, (25.0, 3.0, -10.0), (32.0, -28.0, -62.0)),
    'cock': (GRIP_R, (27.0, 8.0, -14.0), (16.0, -16.0, -44.0)),
}


def tp_pose_at(anchor_local, anchor_world, direction, roll):
    m = frame(direction, (0, 0, 0), roll)
    m.translation = Vector(anchor_world) - m.to_3x3() @ Vector(anchor_local)
    return m


TP_POSES = {
    'ready': (GRIP_R, (-.225, -.20, .50), (.10, -.98, .15), -8),
    'aim': (GRIP_R, (-.19, -.33, .66), (.03, -.999, 0.0), 0),
    'reload': (GRIP_R, (-.13, -.25, .42), (.45, -.65, .60), -62),
    'sprint': (GRIP_R, (-.25, -.12, .38), (.25, -.55, -.80), 0),
    'holster': (GRIP_R, (-.26, -.08, .30), (.10, -.20, -.97), 10),
    'inspect_a': (GRIP_R, (-.14, -.28, .60), (.75, -.45, .35), -45),
    'inspect_b': (GRIP_R, (-.12, -.30, .60), (-.55, -.60, .35), 120),
    'look': (GRIP_R, (-.10, -.27, .63), (.50, -.60, .60), -70),
    'cock': (GRIP_R, (-.20, -.22, .54), (.30, -.90, .30), -40),
}


def local_delta(M, pitch=0.0, yaw=0.0, roll=0.0):
    """Small weapon-local rotation (deg, UE sense: pitch up, yaw right, roll right side down)."""
    out = M
    if yaw:
        out = out @ Matrix.Rotation(math.radians(-yaw), 4, 'Z')
    if pitch:
        out = out @ Matrix.Rotation(math.radians(-pitch), 4, 'Y')
    if roll:
        out = out @ Matrix.Rotation(math.radians(roll), 4, 'X')
    return out


def pose(view, name, dpos=(0, 0, 0), drot=(0, 0, 0)):
    """Base pose of a view with a translation (view-like cm: forward, right, up) and a local rotation delta."""
    if view == 'FP':
        anchor, where, rot = FP_POSES[name]
        M = view_pose(anchor, where, rot)
    else:
        M = tp_pose_at(*TP_POSES[name])
    d = (FWD * dpos[0] + RIGHT * dpos[1] + UP * dpos[2]) * 0.01
    M = Matrix.Translation(d) @ M
    return local_delta(M, *drot)


def track(view, keys, f):
    """keys = [(frame, pose name | (name, dpos, drot) | Matrix)], smoothstep between consecutive keys."""
    def resolve(k):
        if isinstance(k, Matrix):
            return k
        if isinstance(k, str):
            return pose(view, k)
        return pose(view, *k)
    i, w = segment(f, keys)
    a = resolve(keys[i][1])
    if w <= 0.0 or i + 1 >= len(keys):
        return a
    return blend(a, resolve(keys[i + 1][1]), w)


# --------------------------------------------------------------------------------------------- weapon pose

def weapon_pose(clip, f, view):
    end = CLIPS[clip]
    bob = 1.0 if view == 'FP' else 0.8
    if clip == 'Equip':
        # anticipation at the hip, fast rise, overshoot above ready, settle (Ready at 30)
        M = track(view, [(0, 'holster'), (6, ('holster', (0, 0, -3), (0, 0, 0))), (22, ('ready', (1.5, 0.5, 2.0), (6, 0, -4))),
                         (30, 'ready'), (end, 'ready')], f)
    elif clip == 'Holster':
        M = track(view, [(0, 'ready'), (5, ('ready', (0, 0, 1.5), (4, 0, 0))), (26, 'holster'), (end, ('holster', (0, 2, -6), (0, 0, 0)))], f)
    elif clip == 'AimIn':
        M = track(view, [(0, 'ready'), (9, ('aim', (0.4, 0, 0.5), (0.6, 0, 0))), (end, 'aim')], f)
    elif clip == 'AimOut':
        M = track(view, [(0, 'aim'), (8, ('ready', (-0.5, 0.5, -0.8), (-1.5, 0, -1))), (end, 'ready')], f)
    elif clip in ('AimIdle', 'AimFire'):
        M = pose(view, 'aim')
    elif clip == 'Sprint':
        M = pose(view, 'sprint')
    elif clip == 'Reload':
        M = track(view, [(0, 'ready'), (4, ('ready', (0.5, 0, -1), (-4, 0, 0))), (14, ('reload', (0, 0, 0.8), (3, 0, -4))), (20, 'reload'),
                         (118, 'reload'), (124, ('reload', (0, 0, 0), (0, 0, 0))), (132, ('ready', (0.5, 0, 0.6), (3, 0, -2))),
                         (end, 'ready')], f)
        # the stock dips when the ball is pushed and at every ramming stroke; the cocking tug
        dip = (curve(f, [(55, 0), (58, 1), (62, 0)]) * 0.9 + curve(f, [(91, 0), (94, 1), (97, 0)]) * 1.2
               + curve(f, [(97, 0), (100, 1), (103, 0)]) * 1.2 + curve(f, [(117, 0), (120, 1), (124, 0)]) * 0.7)
        M = Matrix.Translation(UP * (-dip * 0.01)) @ local_delta(M, -2.5 * dip, 0, 0)
    elif clip == 'Inspect':
        M = track(view, [(0, 'ready'), (4, ('ready', (0, 0, -0.6), (-3, 0, 0))), (26, ('inspect_a', (0, 0, 0.6), (2, 0, -4))), (34, 'inspect_a'),
                         (62, 'inspect_a'), (92, 'inspect_b'), (122, 'inspect_b'), (144, ('ready', (0.4, 0, 0.5), (2, 0, -2))),
                         (end, 'ready')], f)
        # slow drift while looking, a little turn of the wrist
        M = local_delta(M, 1.5 * math.sin(f / 9.0) * smooth(30, 62, f) * (1 - smooth(62, 92, f)), 0, 2.0 * math.sin(f / 7.0) * smooth(100, 122, f))
    elif clip == 'Cock':
        M = track(view, [(0, 'ready'), (8, 'cock'), (16, ('cock', (0, 0, -0.5), (-2, 0, 0))), (18, ('cock', (0, 0, 0.3), (1.5, 0, 2))),
                         (28, 'ready'), (end, 'ready')], f)
    elif clip == 'Discard':
        M = discard_pose(f, view)
    else:
        M = pose(view, 'ready')
    # life: breathing, gait, shot
    if clip in ('Idle', 'AimIdle'):
        q = math.sin(math.tau * f / end)
        q2 = math.sin(math.tau * f / end * 2 + 1.1)
        amp = 0.55 if clip == 'Idle' else 0.22
        M = Matrix.Translation(UP * (amp * q * 0.01 * bob) + RIGHT * (0.15 * q2 * 0.01)) @ local_delta(M, 0.35 * q, 0.15 * q2, 0.9 * q if clip == 'Idle' else 0.0)
    if clip in ('Walk', 'Sprint'):
        cycles = 4 if clip == 'Walk' else 3
        q = math.tau * f / end * cycles
        amp = 1.0 if clip == 'Walk' else 2.2
        M = (Matrix.Translation(RIGHT * (1.1 * amp * math.sin(q) * 0.01 * bob) + UP * (0.9 * amp * abs(math.sin(q)) * -0.01 * bob)
                                + FWD * (0.3 * amp * math.sin(q * 2) * 0.01))
             @ local_delta(M, 2.0 * amp * math.sin(q * 2 + 0.6), 1.2 * amp * math.sin(q), 2.5 * amp * math.sin(q)))
    if clip in ('Fire', 'AimFire'):
        kick = curve(f, [(0, 0), (2, 0), (4, 1), (9, .32), (15, -.08), (22, 0), (end, 0)])
        back, up_, pitch = ((0.040, 0.004, 12.0) if clip == 'Fire' else (0.030, 0.003, 8.5))
        M = M @ Matrix.Translation((-back * kick, 0, up_ * kick)) @ Matrix.Rotation(math.radians(-pitch * kick), 4, 'Y')
        M = local_delta(M, 0, 0.8 * kick if clip == 'Fire' else 0.3 * kick, 2.0 * kick if clip == 'Fire' else 0.5 * kick)
    if clip == 'DryFire':
        j = curve(f, [(0, 0), (4, 0), (6, 1), (12, 0)])
        M = M @ Matrix.Translation((-0.004 * j, 0, 0)) @ Matrix.Rotation(math.radians(-1.5 * j), 4, 'Y')
    return M


def discard_pose(f, view):
    """Empty: look at the pistol, shake it, throw it to the right and down (Release at 24), it falls on."""
    if f <= 24:
        M = track(view, [(0, 'ready'), (9, 'look'), (11, 'look'), (20, 'look'), (24, ('look', (3, 10, 4), (10, 25, 20)))], f)
        shake = (curve(f, [(11, 0), (12.5, 1), (14, -1), (15.5, 1), (17, -1), (18.5, 0.6), (20, 0)]))
        return Matrix.Translation(UP * (0.012 * shake)) @ local_delta(M, 0, 4 * shake, 18 * shake)
    t = (f - 24) / FPS
    M24 = track(view, [(24, ('look', (3, 10, 4), (10, 25, 20)))], 24)
    if view == 'FP':
        v, g = Vector((80.0, 240.0, 150.0)), Vector((0, 0, -981.0))
        d = (v * t + g * (0.5 * t * t))
        offset = (FWD * d.x + RIGHT * d.y + UP * d.z) * 0.01
        tumble = 420.0 * t
        return Matrix.Translation(offset) @ local_delta(M24, -tumble, 0, 0.6 * tumble)
    v, g = Vector((-1.5, -0.35, 1.3)), Vector((0, 0, -9.81))
    d = v * t + g * (0.5 * t * t)
    start = M24.translation.z
    landed = None
    # stop on the ground: lowest point of the pistol about 9 cm under its grip origin
    for k in range(1, 200):
        tt = k / 100.0
        if tt > t:
            break
        if start + (v * tt + g * (0.5 * tt * tt)).z - 0.09 <= 0.0:
            landed = tt
            break
    if landed is not None:
        d = v * landed + g * (0.5 * landed * landed)
        tumble = 380.0 * landed
        M = Matrix.Translation(d) @ local_delta(M24, -tumble, 0, 0)
        # lies flat on the ground on its lock side
        flat = tp_pose_at(GRIP_R, (M.translation.x, M.translation.y, 0.055), (-.95, -.2, 0.0), -80)
        return blend(M, flat, smooth(landed, landed + 0.12, t))
    return Matrix.Translation(d) @ local_delta(M24, -380.0 * t, 0, 0)


# ------------------------------------------------------------------------------------------- lock parts

def hammer_angle(clip, f, lock=1):
    """Forward rotation (deg) of the cock from full cock; lock 1 = right (Fire), 2 = left (AimFire of the double)."""
    end = CLIPS[clip]
    fires = (clip == 'Fire' and lock == 1) or (clip == 'AimFire' and lock == 2) or clip == 'DryFire'
    if fires:
        hit = 2 if clip != 'DryFire' else 6
        return curve(f, [(0, 0), (hit - 2, 0), (hit, PM.HAMMER_FIRED + 4), (hit + 2, PM.HAMMER_FIRED), (end, PM.HAMMER_FIRED)])
    if clip in ('Fire', 'AimFire'):
        return 0.0
    if clip == 'Reload':
        # down since the shot, pulled back slowly by the thumb, snaps into full cock at HammerCocked (120)
        return curve(f, [(0, PM.HAMMER_FIRED), (112, PM.HAMMER_FIRED), (119, 2.0), (120, -3.0), (122, 0.0), (end, 0.0)])
    if clip == 'Cock':
        return curve(f, [(0, PM.HAMMER_FIRED), (8, PM.HAMMER_FIRED), (15, 3.0), (16, -3.0), (18, 0.0), (end, 0.0)])
    if clip == 'Discard':
        return PM.HAMMER_FIRED + curve(f, [(0, 0), (1, 4), (3, 0)])
    return 0.0


def frizzen_angle(clip, f, lock=1):
    end = CLIPS[clip]
    fires = (clip == 'Fire' and lock == 1) or (clip == 'AimFire' and lock == 2) or clip == 'DryFire'
    if fires:
        hit = 2 if clip != 'DryFire' else 6
        return curve(f, [(0, 0), (hit, 0), (hit + 1, PM.FRIZZEN_OPEN + 8), (hit + 3, PM.FRIZZEN_OPEN), (end, PM.FRIZZEN_OPEN)])
    if clip in ('Fire', 'AimFire'):
        return 0.0
    if clip == 'Reload':
        return curve(f, [(0, PM.FRIZZEN_OPEN), (26, PM.FRIZZEN_OPEN), (30, 0.0), (end, 0.0)])
    if clip == 'Cock':
        return curve(f, [(0, PM.FRIZZEN_OPEN), (2, PM.FRIZZEN_OPEN), (7, 0.0), (end, 0.0)])
    if clip == 'Discard':
        return PM.FRIZZEN_OPEN
    return 0.0


def trigger_angle(clip, f):
    if clip in ('Fire', 'AimFire'):
        return curve(f, [(0, 0), (0.5, 14), (2, 14), (6, 14), (12, 0)])
    if clip == 'DryFire':
        return curve(f, [(0, 0), (3, 0), (6, 14), (9, 14), (15, 0)])
    if clip == 'Discard':
        return curve(f, [(0, 0), (1, 14), (3, 14), (8, 0)])
    return 0.0


# ------------------------------------------------------------------------------------------------ ramrod

def rod_state(clip, f, load_y=0.0):
    """Ramrod during Reload: (tip point, unit direction rear -> tip, hold distance from the tip) in weapon space;
    None when stowed. Pulled by the tip (two pulls), regrip, flipped over the barrel, two ramming strokes, drawn
    out, flipped back, slid home."""
    if clip != 'Reload' or f < 70 or f >= 112:
        return None
    bore = Vector((PM.MUZZLE_X, load_y, PM.BORE_Z))

    def rod(theta, pivot, h):
        d = Vector((math.cos(math.radians(theta)), 0.0, math.sin(math.radians(theta))))
        return pivot + d * h, d, h

    if f < 80:
        s = curve(f, [(70, 0.0), (74, 0.10), (76, 0.10), (80, 0.195)])
        h = curve(f, [(70, 0.03), (74, 0.03), (76, 0.09), (80, 0.09)])
        tip = ROD_TIP0 + Vector((s, 0, 0))
        return tip, Vector((1, 0, 0)), h
    out_tip = ROD_TIP0 + Vector((0.195, 0, 0))
    if f < 84:
        return out_tip, Vector((1, 0, 0)), 0.09 + (0.165 - 0.09) * smooth(80, 84, f)
    pivot_out = out_tip - Vector((0.165, 0, 0))
    pivot_in = bore + Vector((0.035 + 0.165, 0, 0.0))
    if f < 90:
        w = smooth(84, 90, f)
        return rod(180.0 * w, pivot_out.lerp(pivot_in, w), 0.165)
    if f < 104:
        depth = curve(f, [(90, -0.035), (94, 0.12), (96, 0.07), (100, 0.16), (101.5, 0.16), (104, -0.04)])
        tip = bore - Vector((depth, 0, 0))
        return tip, Vector((-1, 0, 0)), 0.165
    if f < 108:
        w = smooth(104, 108, f)
        pivot_drawn = bore + Vector((0.04 + 0.165, 0, 0))
        return rod(180.0 * (1 - w), pivot_drawn.lerp(pivot_out, w), 0.165)
    s = curve(f, [(108, 0.195), (110, 0.10), (112, 0.0)])
    h = curve(f, [(108, 0.165), (110, 0.165), (110.5, 0.025), (112, 0.025)])
    return ROD_TIP0 + Vector((s, 0, 0)), Vector((1, 0, 0)), h


def rod_delta(state):
    if state is None:
        return Matrix.Identity(4)
    tip, d, _ = state
    q = Vector((1, 0, 0)).rotation_difference(d)
    return Matrix.Translation(tip) @ q.to_matrix().to_4x4() @ Matrix.Translation(-ROD_TIP0)


# ------------------------------------------------------------------------------------------------- hands

def hand_frame(view, point, elbow_cm, back):
    """FP hand rotation (x: forearm toward the fingers, z: back of the hand); None in TP."""
    if view != 'FP':
        return None
    d = (point - vp(*elbow_cm)).normalized()
    return axes(d, Vector(back))


def right_hand(clip, f, view, M):
    """Right hand on the grip (point, FP rotation); after Release of Discard it lets go."""
    M3 = M.to_3x3()
    if clip == 'Discard' and f > 24:
        if view == 'FP':
            p = point_curve(f, [(24, vp(*to_view(M @ GRIP_R))), (28, vp(27, 22, -12)), (36, vp(14, 30, -42))])
            return p, hand_frame(view, p, REACH['r'], vv(0, 0.3, 1))
        p = point_curve(f, [(24, M @ GRIP_R), (30, Vector((-.42, -.18, .42))), (36, REST_HAND['r'])])
        return p, None
    p = M @ GRIP_R
    if view != 'FP':
        return p, None
    back = (M3 @ Vector((-0.15, -1.0, 0.3))).normalized()
    elbow = REACH['r']
    if clip in ('AimIdle', 'AimFire', 'AimIn', 'AimOut'):
        elbow = (6.0, 12.0, -40.0)
    return p, hand_frame(view, p, elbow, back)


def tp_pinch(point):
    return (point - TP_SHOULDER_L).normalized() * TP_PINCH_ALONG


def pinch_point(point, rot, view):
    return point + (rot @ FP_PINCH if rot is not None else tp_pinch(point))


def left_keys(view, M, clip, f, rod, load_y):
    """Target of the left hand: ('free', point) off the weapon, ('pinch', point) cartridge at the fingers,
    ('rod',) on the ramrod, ('w', point) on a weapon point; with the FP back-of-hand direction."""
    M3 = M.to_3x3()
    bar = M3 @ Vector((1, 0, 0))
    muzzle = M @ Vector((PM.MUZZLE_X, load_y, PM.BORE_Z))
    if view == 'FP':
        cv = to_view(M @ COMB)
        comb = vp(cv.x - 3.0, cv.y - 9.0, cv.z + 5.5)
    else:
        comb = M @ COMB + Vector((0.065, -0.025, 0.05))
    if view == 'FP':
        free = vp(*FP_LEFT_FREE)
        pouch = vp(13.0, -17.0, -33.0)
        mouth = vp(15.0, -3.0, -4.5)
        tear = vp(18.0, -6.0, -8.0)
        back_up = vv(-0.25, -0.5, 0.8)
    else:
        free = REST_HAND['l']
        pouch = Vector((.205, -.185, .347))
        mouth = Vector((.02, -.215, .705))
        tear = Vector((.06, -.25, .68))
        back_up = UP
    over = muzzle + bar * 0.030 + (UP * 0.012)
    into = muzzle + bar * 0.004
    if clip == 'Reload':
        keys = [(0, 'free', free, back_up), (6, 'free', free, back_up), (14, 'free', pouch, back_up), (18, 'free', pouch, back_up),
                (27, 'pinch', mouth, vv(-0.6, -0.3, 0.75) if view == 'FP' else UP), (30, 'pinch', mouth, vv(-0.6, -0.3, 0.75) if view == 'FP' else UP),
                (32, 'pinch', tear, vv(-0.5, -0.4, 0.75) if view == 'FP' else UP), (38, 'pinch', over, back_up), (41, 'pinch', over + bar * 0.008, back_up),
                (43, 'pinch', over, back_up), (45, 'pinch', over + bar * 0.008, back_up), (47, 'pinch', over, back_up), (50, 'pinch', over, back_up),
                (54, 'pinch', into + bar * 0.012, back_up), (58, 'pinch', into, back_up), (61, 'w', into + bar * 0.01, back_up),
                (66, 'w', muzzle + bar * 0.02 - UP * 0.035, back_up), (70, 'rod', None, back_up), (112, 'rod', None, back_up),
                (116, 'w', comb + UP * 0.01, vv(0.2, -0.4, 0.9) if view == 'FP' else UP), (120, 'w', comb - UP * 0.004, vv(0.2, -0.4, 0.9) if view == 'FP' else UP),
                (122, 'w', comb + UP * 0.01, back_up), (135, 'free', free, back_up)]
    elif clip == 'Inspect':
        hold = M @ Vector((0.19, 0.0, PM.BORE_Z - 0.03))
        keys = [(0, 'free', free, back_up), (52, 'free', free, back_up), (66, 'w', hold, back_up), (122, 'w', hold, back_up), (140, 'free', free, back_up),
                (150, 'free', free, back_up)]
    else:
        keys = [(0, 'free', free, back_up)]
        if clip in ('Walk', 'Sprint') and view == 'TP':
            cycles = 4 if clip == 'Walk' else 3
            q = math.tau * f / CLIPS[clip] * cycles
            swing = free + Vector((0.01 * math.cos(q), (0.055 if clip == 'Walk' else 0.09) * math.sin(q), 0.015 * (1 - math.cos(q))))
            keys = [(0, 'free', swing, back_up)]
    i, w = segment(f, [(k[0], None) for k in keys])
    k0, k1 = keys[i], keys[min(i + 1, len(keys) - 1)]
    out = []
    for k in (k0, k1):
        kind, p, back = k[1], k[2], k[3]
        if kind == 'rod':
            if rod is None:
                p = M @ (ROD_TIP0 - Vector((0.03, 0, 0)))
            else:
                tip, d, h = rod
                p = M @ (tip - d * h)
            back = (M3 @ UP + back_up).normalized() if view == 'FP' else UP
        out.append((kind, p, back))
    p = out[0][1].lerp(out[1][1], w)
    back = out[0][2].lerp(out[1][2], w).normalized()
    kind = out[1][0] if w > 0.5 else out[0][0]
    elbow = REACH['l']
    if clip == 'Reload' and 34 <= f < 60:
        elbow = (6.0, -40.0, -22.0)
    elif clip == 'Reload' and 60 <= f <= 112:
        elbow = (8.0, -38.0, -30.0)
    elif clip == 'Inspect':
        elbow = (6.0, -38.0, -34.0)
    rot = hand_frame(view, p, elbow, back)
    if kind == 'pinch':
        p = p - (rot @ FP_PINCH if rot is not None else tp_pinch(p))
    return p, rot, kind


# ---------------------------------------------------------------------------------------------- cartridge

def cartridge_pose(clip, f, view, M, left_point, left_rot, load_y, rest_matrix):
    """Pose of the charge_visual bone (bean space) or None when hidden: at the left fingers from PaperPickup (18)
    to PaperTransfer (58), then slides into the bore."""
    if clip != 'Reload' or f < 18 or f > 62:
        return None
    M3 = M.to_3x3()
    bar = (M3 @ Vector((1, 0, 0))).normalized()
    up = UP if view == 'TP' else UP
    bite = (FWD * 0.6 + up * 0.8).normalized()
    keys = [(18, up), (26, bite), (32, bite), (37, up), (50, up), (54, -bar), (62, -bar)]
    i, w = segment(f, keys)
    axis = keys[i][1].lerp(keys[min(i + 1, len(keys) - 1)][1], w)
    if axis.length < 1e-4:
        axis = keys[min(i + 1, len(keys) - 1)][1]
    axis.normalize()
    pinch = pinch_point(left_point, left_rot, view)
    muzzle = M @ Vector((PM.MUZZLE_X, load_y, PM.BORE_Z))
    if f >= 58:
        into = smooth(58, 62, f)
        pinch = pinch.lerp(muzzle - bar * 0.065, into)
    rot = axes(axis, up if abs(axis.dot(up)) < 0.95 else FWD)
    delta = rot.to_4x4()
    delta.translation = pinch
    return delta @ Matrix.Translation(-Vector((PM.MUZZLE_X, load_y, PM.BORE_Z))) @ rest_matrix


# ------------------------------------------------------------------------------------------------- events

def events(clip):
    def ev(frame_, name, socket=None):
        e = {'frame': frame_, 'name': name}
        if socket:
            e['socket'] = socket
        return e
    if clip in ('Fire', 'AimFire'):
        return [ev(2, 'Fire', 'socket_muzzle'), ev(2, 'HammerDrop')]
    if clip == 'Reload':
        return [ev(6, 'AssistLeft'), ev(18, 'PaperPickup'), ev(58, 'PaperTransfer', 'socket_load_port'), ev(70, 'RamrodGesture'),
                ev(112, 'RamrodReturned', 'socket_ramrod_grip'), ev(120, 'HammerCocked'), ev(135, 'Ready')]
    if clip == 'DryFire':
        return [ev(6, 'DryClick'), ev(6, 'HammerDrop')]
    if clip == 'Cock':
        return [ev(16, 'CockHammer', 'socket_hammer')]
    if clip == 'Equip':
        return [ev(6, 'DrawWeapon'), ev(30, 'Ready')]
    if clip == 'Holster':
        return [ev(30, 'StowWeapon')]
    if clip == 'Discard':
        return [ev(0, 'Empty'), ev(24, 'Release')]
    return []


# ------------------------------------------------------------------------------------------------ animate

def animate(scene, bean, weapon, clip, view, variant='single'):
    arm = bean['armature']
    gun = weapon['armature']
    end = CLIPS[clip]
    load_y = PM.BARREL_Y[variant][0]
    scene.frame_start = 0
    scene.frame_end = end
    scene.render.fps = FPS
    bpy.context.window.scene = scene
    bean_samples, gun_samples, grips = [], [], []
    errors = {'l': 0., 'r': 0.}
    stretches = []
    bone_names = [pb.name for pb in gun.pose.bones]
    charge_rest = gun.data.bones['charge_visual'].matrix_local
    for f in range(end + 1):
        scene.frame_set(f)
        for pb in arm.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        M = weapon_pose(clip, f, view)
        rod = rod_state(clip, f, load_y)
        # TP body: head follows the pistol when handled, walk cycle on the legs
        phase = 0.0
        if view == 'TP':
            aim = 1. if clip in ('AimIdle', 'AimFire') else smooth(0, end, f) if clip == 'AimIn' else 1. - smooth(0, end, f) if clip == 'AimOut' else 0.
            look = 0.0
            if clip == 'Reload':
                look = curve(f, [(0, 0), (12, 1), (124, 1), (135, 0)])
            elif clip == 'Inspect':
                look = curve(f, [(0, 0), (20, 1), (130, 1), (150, 0)])
            elif clip == 'Discard':
                look = curve(f, [(0, 0), (8, 1), (22, 1), (30, 0)])
            elif clip == 'Cock':
                look = curve(f, [(0, 0), (8, .6), (24, .6), (30, 0)])
            local_rot(arm, 'head', (0, 0, 1), -4 + 6 * aim - 14 * look)
            if look:
                pb = arm.pose.bones['head']
                pb.rotation_quaternion = pb.rotation_quaternion @ Quaternion(pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0)),
                                                                             math.radians(14 * look))
            arm.pose.bones['root'].location = (0, 0, .003)
            if clip in ('Walk', 'Sprint'):
                cyc = 4 if clip == 'Walk' else 3
                phase = math.tau * f / end * cyc
                arm.pose.bones['root'].location = (0, 0, .006 + (.007 if clip == 'Walk' else .012) * (1 - math.cos(2 * phase)))
                arm.pose.bones['pelvis'].location = (0, -.027 if clip == 'Walk' else -.045, 0)
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev = arm.evaluated_get(dg)
        bm = {p.name: p.matrix.copy() for p in ev.pose.bones}
        rp, rrot = right_hand(clip, f, view, M)
        lp, lrot, lkind = left_keys(view, M, clip, f, rod, load_y)
        if view == 'TP' and lkind in ('rod', 'w', 'pinch') and clip == 'Reload':
            # short bean arms: bring the pistol toward the left shoulder when the working hand is out of reach
            d = lp - shoulder_point(arm, bm, 'l')
            reach = 0.47
            if d.length > reach:
                M = Matrix.Translation(-d.normalized() * (d.length - reach)) @ M
                rp, rrot = right_hand(clip, f, view, M)
                lp, lrot, lkind = left_keys(view, M, clip, f, rod, load_y)
        if clip in ('Walk', 'Sprint') and view == 'TP':
            for side, off in (('l', 0), ('r', math.pi)):
                step = phase + off
                v = arm.data.bones['foot_' + side].head_local.copy()
                v.y = (.035 if clip == 'Walk' else .055) * math.cos(step)
                v.z = .053 + max(0, math.sin(step)) * (.024 if clip == 'Walk' else .045)
                solve_leg(arm, bm, side, v)
        for side, point, rot in (('r', rp, rrot), ('l', lp, lrot)):
            if view == 'FP':
                error, scale = solve_fp_arm(arm, bm, side, point, rot, FP_SHOULDER[side])
            else:
                error, scale = solve_tp_arm(arm, bm, side, point)
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
            rp, lp = rp + Vector((0, 0, lift)), lp + Vector((0, 0, lift))
        bean_samples.append(bm)
        grips.append(dict(frame=f, left=[*lp], right=[*rp], left_kind=lkind))
        # the pistol: rigid parts on the root pose; cock, frizzen, trigger, ramrod and cartridge animated
        matrices = {n: M @ gun.data.bones[n].matrix_local for n in bone_names}
        for part, angle in (('hammer', hammer_angle(clip, f, 1)), ('frizzen', frizzen_angle(clip, f, 1)),
                            ('hammer2', hammer_angle(clip, f, 2)), ('frizzen2', frizzen_angle(clip, f, 2))):
            if part in matrices:
                B = gun.data.bones[part].matrix_local
                head = gun.data.bones[part].head_local
                P = Matrix.Translation(head) @ Matrix.Rotation(math.radians(angle), 4, 'Y') @ Matrix.Translation(-head)
                matrices[part] = M @ P @ B
        B = gun.data.bones['trigger'].matrix_local
        head = gun.data.bones['trigger'].head_local
        P = Matrix.Translation(head) @ Matrix.Rotation(math.radians(-trigger_angle(clip, f)), 4, 'Y') @ Matrix.Translation(-head)
        matrices['trigger'] = M @ P @ B
        matrices['ramrod'] = M @ rod_delta(rod) @ gun.data.bones['ramrod'].matrix_local
        charge = cartridge_pose(clip, f, view, M, lp, lrot, load_y, charge_rest)
        matrices['charge_visual'] = charge if charge is not None else M @ charge_rest @ Matrix.Diagonal((.0001, .0001, .0001, 1))
        for n in bone_names:
            b = gun.data.bones[n]
            if b.parent and n.startswith('socket_'):
                matrices[n] = matrices[b.parent.name] @ b.parent.matrix_local.inverted() @ b.matrix_local
        gun_samples.append(matrices)
    clip_weapon = 'DoublePistol' if variant == 'double' else 'Pistol'
    ba = bake_matrices(arm, bean_samples, 'A_Bean_%s_%s_%s' % (view, clip_weapon, clip))
    ga = bake_matrices(gun, gun_samples, 'A_%s_%s_%s' % (clip_weapon, view, clip))
    scene.frame_set(0)
    bpy.context.view_layer.update()
    return dict(bean_action=ba.name, weapon_action=ga.name, events=events(clip), hand_error_max_m=errors,
                upperarm_scale_max=max(stretches), frame_start=0, frame_end=end, fps=FPS,
                authored_method='T-248 local pack: FP arms placed through the game camera (no stretch), TP two-segment arms; '
                                'CoD style keys (anticipation, overshoot, settle); one-handed pistol, left hand assists '
                                'Reload / Inspect; cartridge and ramrod animated; aim on the sight line.',
                primary_hand='right', left_hand='free except Reload, Inspect',
                hammer_end_state='down' if clip in ('Fire', 'AimFire', 'DryFire', 'Discard') else 'cocked',
                grip_targets=grips)


# ------------------------------------------------------------------------------------------- game framing

def fp_framing():
    """What the game needs to frame the FP clips like this pack's camera (DA_PistolRules: HipGripLocation /
    HipRotation from Idle frame 0, AimMuzzleLocation / AimRotation from AimIdle: sight line on the view axis)."""
    ready = pose('FP', 'ready')
    aim = pose('FP', 'aim')
    hp, hy, hr = rotator_in_view(ready)
    ap, ay, ar = rotator_in_view(aim)
    return dict(camera_location_m=[*FP_CAMERA], horizontal_fov_deg=90.0,
                hip_grip_location_cm=[round(v, 2) for v in to_view(ready @ GRIP_R)], hip_rotation_pyr_deg=[round(v, 3) for v in (hp, hy, hr)],
                aim_muzzle_location_cm=[round(v, 2) for v in to_view(aim @ Vector((PM.MUZZLE_X, 0.0, PM.BORE_Z)))],
                aim_rotation_pyr_deg=[round(v, 3) for v in (ap, ay, ar)],
                sight_line_height_above_bore_m=round(PM.SIGHT_LINE_Z - PM.BORE_Z, 4),
                note='HipGripLocation / HipRotation reproduce the Idle pose of this camera; AimMuzzleLocation / AimRotation put the '
                     'rear notch and the front bead on the view axis (the bore sits 1.55 cm under it).')
