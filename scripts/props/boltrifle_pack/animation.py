"""Bean handling of the bolt-action sniper rifle (T-334), first (FP) and third (TP) person, 30 fps, baked on the bean
skeleton and on the rifle skeleton by period_arms_common/motion.animate. Clip names, lengths and events: spec.py.

  Equip 45 (DrawWeapon 6, Ready 38)   Holster 36 (StowWeapon 30)   Idle 90 / Walk 120 / Sprint 90 (loops)
  AimIn 15, AimIdle 90 (loop), AimOut 12: the eye ON THE SCOPE AXIS, EYE_RELIEF (8 cm) behind the eyepiece: in FP the
      authoring camera is the eye (socket_scope_eye on the camera), so the eyepiece sits on the view axis
  Fire 24 / AimFire 24 (Fire 2): sharp kick, the bolt stays closed
  BoltCycle 27: hand to the knob (0-3), handle lifted 60 deg (3-6, BoltOpen 6), bolt drawn 7.5 cm (6-11, CaseEject 11:
      the case leaves socket_eject toward socket_eject_dir; the game spawns SM_BoltRifle_Casing), pushed home (11-16),
      turned down (16-19, BoltClose 19: a round chambered), hand back to the grip (19-27); the rifle cants to the left
      so the bolt shows, jolts at each stop
  Reload 78: bolt opened (BoltOpen 10), a stripper clip of five from the pouch, seated in the charger guide (ClipIn
      30), the rounds thumbed down into the magazine in two pushes (RoundsIn 42), the bolt pushed home kicks the empty
      clip out (ClipEject 52), turned down (BoltClose 62), hand back
  DryFire 12 (DryClick 3)   Inspect 150   Discard 40 (Empty 8: the bolt flicked open on the empty magazine, Release 24)

Bean space: metres, the bean looks along -Y, its right side is -X, +Z up. Weapon space: +X muzzle, +Z up, -Y right side.
"""
from __future__ import annotations

import math

from mathutils import Matrix, Quaternion, Vector

import motion as MO
import spec as S
from motion import blend, curve, ease_os, smooth, vp, vv

FPS = S.FPS
CLIPS = dict(S.CLIPS)
LOOPS = set(S.LOOPS)
TP_STRETCH = 1.36
AUTHORED = ('T-334: FP arms placed through the game camera (no stretch), TP two-segment arms from the musket pack; CoD easing '
            '(anticipation, overshoot, settle); turn-bolt worked by the right hand (60 deg lift, 7.5 cm pull), stripper clip '
            'reload through the open action; aim with the eye on the scope axis.')

BZ = S.BORE_Z
GRIP_R = Vector(S.GRIP_R)
GRIP_L = Vector(S.GRIP_L)
GRIP_L_TP = Vector((0.270, 0.0, 0.012))              # the bean's short left arm: nearer on the fore-end in TP
UNDER_MAG = Vector((0.075, 0.0, -0.045))              # TP aim: the left hand under the front of the floorplate
MUZZLE = Vector((S.MUZZLE_X, 0.0, BZ))
MUZZLE_SOCKET = Vector(S.SOCKETS['muzzle'][1])        # what the game measures (MuzzleSocket = socket_muzzle)
EYE = Vector(S.SCOPE_EYE)
KNOB = Vector(S.KNOB)
AXIS = Vector((0.0, 0.0, BZ))
CLIP_C = Vector(S.BONES['stripper_clip'][0])          # centre of the seated clip (spine)
ROUNDS_C = Vector(S.BONES['clip_rounds'][0])
TOP_ROUND = Vector((S.CLIP_X + 0.024, 0.0, S.CLIP_Z0 + 4 * S.CLIP_PITCH + 0.0058))
ACTION = Vector((0.110, 0.0, BZ + 0.010))
SCOPE_MID = Vector((0.170, S.SCOPE_Y, S.SCOPE_Z))

EMPTY_FRAME, RELEASE_FRAME = 8, 24


# ------------------------------------------------------------------------------------------- weapon poses

FP_POSES = {
    # hip: low on the right, the barrel toward the middle of the screen, rolled a little so the scope shows on top
    'hip': (GRIP_R, (31.0, 15.0, -19.0), (0.985, -0.115, 0.115), (-0.06, 0.20, 0.98)),
    'sprint': (GRIP_R, (30.0, 16.0, -17.0), (0.78, -0.50, -0.36), (0.30, 0.42, 0.86)),
    'holster': (GRIP_R, (18.0, 24.0, -64.0), (0.42, 0.25, -0.87), (0.88, -0.05, 0.47)),
    # bolt cycle: lowered a little, canted to the left (right side up) so the bolt and the open action show
    'cycle': (GRIP_R, (34.0, 11.0, -19.0), (0.96, -0.16, 0.20), (0.10, -0.35, 0.93)),
    # reload: raised, the open action just left of the centre of the view, its top toward the eye, the scope (left
    # side) beyond it, the knob on the right
    'reload': (ACTION, (37.0, -4.0, -7.0), (0.95, -0.18, 0.25), (0.0, -0.38, 0.92)),
    # inspect: the scope brought to the eye from the side, then rolled to show the bolt
    'inspect_scope': (SCOPE_MID, (30.0, -1.0, -5.0), (0.70, -0.70, 0.13), (-0.25, -0.05, 0.97)),
    'inspect_bolt': (ACTION, (31.0, 3.0, -9.0), (0.72, -0.62, 0.30), (0.45, 0.20, 0.87)),
    'look': (ACTION, (32.0, 4.0, -10.0), (0.80, -0.52, 0.30), (0.22, -0.48, 0.85)),
    'toss': (GRIP_R, (34.0, 40.0, -30.0), (0.30, 0.80, -0.52), (0.85, -0.20, 0.48)),
}


def fp_poses():
    out = {k: MO.view_pose_dirs(*v) for k, v in FP_POSES.items()}
    # aim: the eye point of the scope ON the camera, the scope axis along the view (weapon +X forward, +Z up)
    out['aim'] = MO.view_pose_dirs(EYE, (0.0, 0.0, 0.0), (1.0, 0.0, 0.0), (0.0, 0.0, 1.0))
    return out


def tp_poses():
    at = MO.tp_pose_at
    return dict(
        hip=MO.frame((.955, -.25, .16), (-.16, -.25, .51), -8),
        sprint=Matrix.Translation((0, 0, -0.05)) @ MO.frame((.955, -.25, .16), (-.16, -.25, .51), -8) @ Matrix.Rotation(math.radians(14), 4, 'Y')
        @ Matrix.Rotation(math.radians(-10), 4, 'X'),
        holster=MO.frame((.15, -.16, -.975), (-.30, -.20, .43), 12),
        # aimed: the eye point of the scope just in front of the right eye, the rifle forward-left (short arms)
        aim=at(EYE, (-.045, -.27, .785), (.50, -.86, .03), 0),
        cycle=at(GRIP_R, (-.15, -.27, .52), (.93, -.32, .17), 18),
        reload=at(ACTION, (-.02, -.30, .62), (.86, -.30, .40), 35),
        inspect_scope=at(SCOPE_MID, (-.02, -.31, .68), (.86, -.42, .28), -20),
        inspect_bolt=at(ACTION, (-.04, -.29, .62), (.86, -.36, .36), 40),
        look=at(ACTION, (-.03, -.29, .62), (.86, -.34, .38), 35),
        toss=at(GRIP_R, (-.36, -.24, .45), (-.30, -.50, -.81), 20),
    )


_POSES = {}


def poses(view):
    if view not in _POSES:
        _POSES[view] = fp_poses() if view == 'FP' else tp_poses()
    return _POSES[view]


def keyed(f, keys, view, over=0.05, antic=0.03):
    return MO.keyed_pose(f, keys, poses(view), over, antic)


# ----------------------------------------------------------------------------------------------- the bolt

def bolt_state(clip, f):
    """(lift deg, pull m) of the bolt at f."""
    T = S.BOLT_TRAVEL
    L = S.BOLT_LIFT_DEG
    if clip == 'BoltCycle':
        lift = curve(f, [(0, 0), (3, 0), (5.5, L + 4), (6, L), (16, L), (18.5, -3), (19, 0)])
        pull = curve(f, [(0, 0), (6, 0), (10.5, T + 0.002), (11, T), (11.5, T), (15.6, 0.0015), (16, 0)])
        return lift, pull
    if clip == 'Reload':
        lift = curve(f, [(0, 0), (5, 0), (9.5, L + 4), (10, L), (55, L), (61.5, -3), (62, 0)])
        pull = curve(f, [(0, 0), (10, 0), (14.5, T + 0.002), (15, T), (47, T), (54.5, 0.0015), (55, 0)])
        return lift, pull
    if clip == 'Discard':
        lift = curve(f, [(0, 0), (3, 0), (5.5, L), (40, L)])
        pull = curve(f, [(0, 0), (5.5, 0), (8, T), (40, T)])
        return lift, pull
    if clip == 'Inspect':
        lift = curve(f, [(0, 0), (86, 0), (90, L), (110, L), (114, 0)])
        pull = curve(f, [(0, 0), (90, 0), (95, 0.040), (103, 0.040), (108, 0), (110, 0)])
        return lift, pull
    return 0.0, 0.0


def bolt_matrix(lift, pull):
    """Weapon space transform of the bolt: turned about the bore axis (handle up), then drawn back along -X."""
    return Matrix.Translation((-pull, 0, 0)) @ Matrix.Translation(AXIS) @ Matrix.Rotation(math.radians(-lift), 4, 'X') @ Matrix.Translation(-AXIS)


def trigger_angle(clip, f):
    if clip in ('Fire', 'AimFire'):
        return curve(f, [(0, 0), (2, 15), (5, 15), (9, 0)])
    if clip == 'DryFire':
        return curve(f, [(0, 0), (3, 15), (6, 15), (9, 0)])
    return 0.0


# ------------------------------------------------------------------------------------------- the weapon

CYCLE_KEYS = [(0, 'hip'), (4, 'cycle'), (20, 'cycle'), (26, 'hip')]
RELOAD_KEYS = [(0, 'hip'), (7, 'reload'), (64, 'reload'), (74, 'hip')]
INSPECT_KEYS = [(0, 'hip'), (20, 'inspect_scope'), (58, 'inspect_scope'), (78, 'inspect_bolt'), (118, 'inspect_bolt'), (138, 'hip')]


def recoil(m, k, s=1.0):
    """CoD style kick (weapon local): muzzle up, back along the barrel, a little roll and yaw."""
    return (m @ Matrix.Translation((-0.050 * k * s, 0, 0.006 * k * s)) @ Matrix.Rotation(math.radians(-8.5 * k * s), 4, 'Y')
            @ Matrix.Rotation(math.radians(2.2 * k * s), 4, 'X') @ Matrix.Rotation(math.radians(0.9 * k * s), 4, 'Z'))


def aim_recoil(m, k):
    """Through the scope: the kick about the eye (the shoulder), the eye comes back to the eyepiece."""
    pivot = EYE
    R = Matrix.Rotation(math.radians(-6.5 * k), 4, 'Y') @ Matrix.Rotation(math.radians(1.6 * k), 4, 'X')
    return m @ Matrix.Translation((-0.030 * k, 0, 0.012 * k)) @ Matrix.Translation(pivot) @ R @ Matrix.Translation(-pivot)


def discard_flight(p, f, view):
    toss = p['toss']
    dt = (f - RELEASE_FRAME) / FPS
    if view == 'FP':
        v = vv(0.5, 2.2, 0.3) * 2.0
        spin = vv(0.4, 0.2, -0.9)
        floor = -10.0
    else:
        v = Vector((-1.4, -0.5, 0.6))
        spin = Vector((0.2, -0.9, 0.3))
        floor = 0.035
    pos = toss.translation + v * dt + Vector((0, 0, -4.905 * dt * dt))
    rot = Quaternion(spin.normalized(), dt * 6.0).to_matrix().to_4x4() @ toss.to_3x3().to_4x4()
    if view == 'TP' and pos.z < floor:
        pos.z = floor
    rot.translation = pos
    if view == 'TP':
        rest = Matrix.Translation((-0.95, -0.40, 0.040)) @ Matrix.Rotation(math.radians(-65), 4, 'Z') @ Matrix.Rotation(math.radians(-90), 4, 'X')
        rot = blend(rot, rest, smooth(RELEASE_FRAME + 4, CLIPS['Discard'], f))
    return rot


def weapon_pose(clip, f, view):
    p = poses(view)
    end = CLIPS[clip]
    base = p['hip']
    if clip == 'Sprint':
        base = p['sprint']
    elif clip == 'Equip':
        base = blend(p['holster'], p['hip'], ease_os(0, end - 9, f, over=0.07))
        dip = curve(f, [(end - 10, 0), (end - 6, 1), (end - 3, -0.3), (end, 0)])
        base = base @ Matrix.Translation((0, 0, -0.010 * dip))
    elif clip == 'Holster':
        base = blend(p['hip'], p['holster'], ease_os(6, end, f, antic=0.06))
    elif clip == 'AimIn':
        base = blend(p['hip'], p['aim'], ease_os(0, end - 3, f, over=0.04, antic=0.02))
        settle = curve(f, [(end - 4, 0), (end - 2, 1), (end, 0)])
        base = base @ Matrix.Translation((0, 0, -0.002 * settle))
    elif clip == 'AimOut':
        base = blend(p['aim'], p['hip'], ease_os(0, end, f, over=0.04, antic=0.02))
    elif clip in ('AimIdle', 'AimFire'):
        base = p['aim']
    elif clip == 'BoltCycle':
        base = keyed(f, CYCLE_KEYS, view, over=0.06, antic=0.03)
        jolt = curve(f, [(5, 0), (6, 1), (8, 0)]) * 0.5 + curve(f, [(10, 0), (11, 1), (13, -0.3), (14, 0)]) * 0.8 \
            + curve(f, [(15, 0), (16, -0.9), (18, 0.2), (19, 0)]) + curve(f, [(18, 0), (19, 0.6), (21, 0)])
        base = base @ Matrix.Translation((-0.004 * jolt, 0, -0.002 * jolt)) @ Matrix.Rotation(math.radians(0.9 * jolt), 4, 'Y')
    elif clip == 'Reload':
        base = keyed(f, RELOAD_KEYS, view, over=0.06, antic=0.03)
        jolt = curve(f, [(9, 0), (10, 0.5), (12, 0)]) + curve(f, [(14, 0), (15, 0.7), (17, 0)]) + curve(f, [(29, 0), (30, 0.6), (32, 0)]) \
            + curve(f, [(36, 0), (37, 0.8), (39, 0)]) + curve(f, [(41, 0), (42, 0.9), (44, 0)]) + curve(f, [(53, 0), (55, -1.0), (57, 0)]) \
            + curve(f, [(61, 0), (62, 0.7), (64, 0)])
        base = base @ Matrix.Translation((0, 0, -0.004 * jolt)) @ Matrix.Rotation(math.radians(1.2 * jolt), 4, 'Y')
    elif clip == 'Inspect':
        base = keyed(f, INSPECT_KEYS, view, over=0.05, antic=0.03)
        turn = curve(f, [(22, 0), (56, 1)]) * 9.0 - curve(f, [(80, 0), (116, 1)]) * 7.0
        base = base @ Matrix.Rotation(math.radians(turn), 4, 'X')
    elif clip == 'Discard':
        if f <= 10:
            base = blend(p['hip'], p['look'], ease_os(0, 10, f, over=0.05))
        elif f <= 16:
            base = p['look']
        elif f <= RELEASE_FRAME:
            base = blend(p['look'], p['toss'], ease_os(16, RELEASE_FRAME, f, antic=0.08))
        else:
            base = discard_flight(p, f, view)
    # loops and shots
    if clip == 'Idle':
        q = math.sin(2 * math.pi * f / end)
        q2 = math.sin(4 * math.pi * f / end + 0.7)
        base = base @ Matrix.Translation((0.0012 * q2, 0.0014 * q, 0.0035 * q)) @ Matrix.Rotation(math.radians(0.55 * q), 4, 'Y') \
            @ Matrix.Rotation(math.radians(0.35 * q2), 4, 'X')
    if clip == 'AimIdle':
        # breathing on the scope: a slow figure of eight of a few tenths of a degree about the eye
        q = math.sin(2 * math.pi * f / end)
        q2 = math.sin(4 * math.pi * f / end + 0.9)
        R = Matrix.Rotation(math.radians(0.20 * q), 4, 'Y') @ Matrix.Rotation(math.radians(0.14 * q2), 4, 'Z')
        base = base @ Matrix.Translation(EYE) @ R @ Matrix.Translation(-EYE) @ Matrix.Translation((0, 0, 0.0006 * q))
    if clip in ('Walk', 'Sprint'):
        q = 2 * math.pi * f / end * 4
        amp = 1.0 if clip == 'Walk' else 2.2
        base = base @ Matrix.Translation((0.003 * amp * math.sin(q), 0.006 * amp * math.sin(q / 2), 0.006 * amp * math.sin(q + 0.6)))
        base = base @ Matrix.Rotation(math.radians(2.4 * amp * math.sin(q / 2 - 0.4)), 4, 'X') @ Matrix.Rotation(math.radians(1.3 * amp * math.sin(q + 0.6)), 4, 'Y')
        if clip == 'Sprint' and view == 'TP':
            base = base @ Matrix.Rotation(math.radians(10), 4, 'Y')
    if clip == 'Fire':
        base = recoil(base, MO.kick_curve(f, 2, settle=14))
    if clip == 'AimFire':
        base = aim_recoil(base, MO.kick_curve(f, 2, settle=16))
    if clip == 'DryFire':
        j = curve(f, [(2, 0), (3, 1), (6, 0)])
        base = base @ Matrix.Translation((-0.003 * j, 0, 0)) @ Matrix.Rotation(math.radians(-0.6 * j), 4, 'Y')
    return base


# ---------------------------------------------------------------------------------------------- the clip

POUCH_FP = ('v', (16.0, 30.0, -52.0), Vector((-.16, -.24, .40)))
POUCH_TP = Vector((-.13, -.21, .40))
HAND_ON_CLIP = Vector((0.030, -0.042, 0.022))         # weapon frame: the mitten pinches the clip from its right, a little
                                                      # forward (the brass heads of the rounds stay in view, left of it)
CLIP_TILT = Matrix.Rotation(math.radians(25), 4, 'Y') @ Matrix.Rotation(math.radians(-15), 4, 'X')


def clip_carrier(f, view, M):
    """Bean space point of the clip spine centre during Reload (frames 21..30) and the clip's rotation."""
    seated = M @ CLIP_C
    above = M @ (CLIP_C + Vector((-0.004, -0.010, 0.075)))
    pouch = vp(*POUCH_FP[1]) if view == 'FP' else POUCH_TP
    if f <= 21:
        pos = pouch
    elif f <= 27:
        pos = pouch.lerp(above, ease_os(21, 27, f, over=0.04))
    else:
        pos = above.lerp(seated, smooth(27, 30, f))
    tilt = 1.0 - smooth(25, 30, f)
    R = Quaternion().slerp(CLIP_TILT.to_quaternion(), tilt).to_matrix().to_4x4()
    return pos, M.to_3x3().to_4x4() @ R


def clip_parts(clip, f, view, M):
    """{stripper_clip, clip_rounds}: the strip hidden outside Reload; the rounds in the magazine (pushed down)."""
    down = Matrix.Translation((0, 0, -S.PUSH_DEPTH))
    if clip == 'Discard':
        return {'stripper_clip': ('hide', Matrix.Identity(4)), 'clip_rounds': ('hide', down)}
    if clip != 'Reload':
        return {'stripper_clip': ('hide', Matrix.Identity(4)), 'clip_rounds': down}
    if f < 21:
        return {'stripper_clip': ('hide', Matrix.Identity(4)), 'clip_rounds': ('hide', Matrix.Identity(4))}
    if f < 30:
        pos, R = clip_carrier(f, view, M)
        W = Matrix.Translation(pos) @ R @ Matrix.Translation(-CLIP_C)
        return {'stripper_clip': ('bean', W), 'clip_rounds': ('bean', W)}
    push = curve(f, [(30, 0), (34, 0), (37, 0.62), (39, 0.62), (42, 1.0)])
    rounds = Matrix.Translation((0, 0, -S.PUSH_DEPTH * push))
    if f < 52:
        return {'stripper_clip': Matrix.Identity(4), 'clip_rounds': rounds}
    # ClipEject: the bolt kicks the empty strip up and to the right, it tumbles out of the action
    t = (f - 52) / FPS
    pos = Vector((-0.4 * t, -1.1 * t, 2.0 * t - 4.905 * t * t))          # weapon space: up, to the right, a little back
    D = Matrix.Translation(CLIP_C + pos) @ Matrix.Rotation(-7.0 * t, 4, 'X') @ Matrix.Rotation(3.0 * t, 4, 'Y') @ Matrix.Translation(-CLIP_C)
    if f > 60:
        return {'stripper_clip': ('hide', D), 'clip_rounds': rounds}
    return {'stripper_clip': D, 'clip_rounds': rounds}


def parts(clip, f, view, M, hands):
    lift, pull = bolt_state(clip, f)
    out = {'bolt': bolt_matrix(lift, pull)}
    tr = trigger_angle(clip, f)
    piv = Vector(S.TRIGGER_PIVOT)
    out['trigger'] = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(tr), 4, 'Y') @ Matrix.Translation(-piv)
    out.update(clip_parts(clip, f, view, M))
    return out


# --------------------------------------------------------------------------------------------- the hands

REACH = {'r': (8.0, 26.0, -48.0), 'l': (14.0, -30.0, -46.0)}
WORK_R = (10.0, 44.0, -32.0)                          # the right forearm in from the right edge while it works the bolt
CLIP_R = (22.0, 52.0, -10.0)                          # ... and level with the action while it loads the clip
WORK_L = (18.0, -34.0, -40.0)
GRIP_U, FORE_U, KNOB_U, THUMB_U = (0, .55, .85), (0, -.45, -.89), (-.15, .55, .82), (-.10, .40, .91)
FREE_R = ('v', (20.0, 26.0, -58.0), Vector((-.29, -.10, .36)))
FREE_L = ('v', (20.0, -26.0, -58.0), Vector((.29, -.10, .36)))


def resolver(clip, f, M):
    lift, pull = bolt_state(clip, f)
    B = bolt_matrix(lift, pull)
    push = curve(f, [(30, 0), (34, 0), (37, 0.62), (39, 0.62), (42, 1.0)]) if clip == 'Reload' else 0.0

    def resolve(spec, view):
        kind = spec[0]
        if kind == 'w':
            return M @ Vector(spec[1])
        if kind == 'knob':
            # the knob at the fingers: the mitten behind it (FP: along the forearm; TP: the body mitten just below)
            return M @ (B @ KNOB), ((0.045, 0.0, 0.004), (0.0, 0.0, -0.012))
        if kind == 'v':
            return vp(*spec[1]) if view == 'FP' else Vector(spec[2])
        if kind == 'clip':
            pos, R = clip_carrier(f, view, M)
            return pos + R.to_3x3() @ HAND_ON_CLIP
        if kind == 'thumb':
            # the thumb on the top round, pushing it down with the column
            return M @ (TOP_ROUND + Vector((0.020, -0.024, 0.034 - S.PUSH_DEPTH * push)))
        raise ValueError(spec)
    return resolve


def right_keys(clip, view):
    rr = REACH['r']
    G = ('w', GRIP_R)
    K = ('knob',)
    if clip == 'BoltCycle':
        return [(0, G, rr, GRIP_U), (3, K, WORK_R, KNOB_U), (19, K, WORK_R, KNOB_U), (25, G, rr, GRIP_U), (27, G, rr, GRIP_U)]
    if clip == 'Reload':
        pouch = POUCH_FP if view == 'FP' else ('v', POUCH_FP[1], POUCH_TP)
        return [(0, G, rr, GRIP_U), (5, K, WORK_R, KNOB_U), (15, K, WORK_R, KNOB_U), (21, pouch, (4.0, 34.0, -52.0), (-.2, .8, .5)),
                (22, ('clip',), (4.0, 34.0, -52.0), (-.2, .8, .5)), (27, ('clip',), CLIP_R, THUMB_U), (30, ('clip',), CLIP_R, THUMB_U),
                (34, ('thumb',), CLIP_R, THUMB_U), (37, ('thumb',), CLIP_R, THUMB_U), (39, ('thumb',), CLIP_R, THUMB_U),
                (42, ('thumb',), CLIP_R, THUMB_U), (47, K, WORK_R, KNOB_U), (62, K, WORK_R, KNOB_U), (70, G, rr, GRIP_U), (78, G, rr, GRIP_U)]
    if clip == 'Discard':
        drop = ('v', (16.0, 30.0, -62.0), Vector((-.27, -.06, .34)))
        off = ('v', (26.0, 38.0, -42.0), Vector((-.36, -.20, .40)))
        return [(0, G, rr, GRIP_U), (3, K, WORK_R, KNOB_U), (9, K, WORK_R, KNOB_U), (13, G, rr, GRIP_U), (RELEASE_FRAME, G, rr, GRIP_U),
                (30, off, rr, GRIP_U), (40, drop, rr, GRIP_U)]
    if clip == 'Inspect':
        return [(0, G, rr, GRIP_U), (82, G, rr, GRIP_U), (86, K, WORK_R, KNOB_U), (114, K, WORK_R, KNOB_U), (120, G, rr, GRIP_U),
                (150, G, rr, GRIP_U)]
    if clip == 'Equip':
        return [(0, FREE_R, rr, GRIP_U), (10, FREE_R, rr, GRIP_U), (28, G, rr, GRIP_U)]
    if clip == 'Holster':
        return [(0, G, rr, GRIP_U), (24, G, rr, GRIP_U), (36, FREE_R, rr, GRIP_U)]
    if clip in ('Fire', 'AimFire', 'DryFire'):
        return [(0, G, rr, GRIP_U), (2, G, rr, (0, .42, .9)), (8, G, rr, GRIP_U)]
    return [(0, G, rr, GRIP_U)]


def left_keys(clip, view):
    rl = REACH['l']
    L = ('w', GRIP_L if view == 'FP' else GRIP_L_TP)
    if view == 'TP' and clip in ('AimIn', 'AimIdle', 'AimOut', 'AimFire'):
        U = ('w', UNDER_MAG)
        if clip == 'AimIn':
            return [(0, L, rl, FORE_U), (3, L, rl, FORE_U), (9, U, rl, FORE_U), (CLIPS[clip], U, rl, FORE_U)]
        if clip == 'AimOut':
            return [(0, U, rl, FORE_U), (4, U, rl, FORE_U), (CLIPS[clip], L, rl, FORE_U)]
        return [(0, U, rl, FORE_U)]
    if clip == 'Equip':
        return [(0, FREE_L, rl, FORE_U), (14, FREE_L, rl, FORE_U), (32, L, rl, FORE_U)]
    if clip == 'Holster':
        return [(0, L, rl, FORE_U), (10, L, rl, FORE_U), (28, FREE_L, rl, FORE_U)]
    if clip == 'Discard':
        return [(0, L, rl, FORE_U), (14, L, rl, FORE_U), (22, ('v', (22.0, -28.0, -44.0), Vector((.28, -.10, .36))), rl, FORE_U),
                (40, ('v', (16.0, -30.0, -62.0), Vector((.27, -.06, .34))), rl, FORE_U)]
    return [(0, L, rl, FORE_U)]


def clip_hands(clip, f, view, M):
    resolve = resolver(clip, f, M)
    right = MO.keyed_hand(right_keys(clip, view), f, view, resolve)
    left = MO.keyed_hand(left_keys(clip, view), f, view, resolve)
    return right, left


# -------------------------------------------------------------------------------------------- TP body

def tp_body(arm, clip, f):
    end = CLIPS[clip]
    aim = 1.0 if clip in ('AimIdle', 'AimFire') else smooth(0, end, f) if clip == 'AimIn' else 1.0 - smooth(0, end, f) if clip == 'AimOut' else 0.0
    down = 0.0
    if clip == 'BoltCycle':
        down = curve(f, [(0, 0), (4, 0.6), (20, 0.6), (26, 0)])
    elif clip == 'Reload':
        down = curve(f, [(0, 0), (8, 1), (64, 1), (74, 0)])
    elif clip == 'Inspect':
        down = curve(f, [(0, 0), (20, 0.8), (130, 0.8), (145, 0)])
    elif clip == 'Discard':
        down = curve(f, [(0, 0), (8, 1), (16, 1), (RELEASE_FRAME + 2, 0.3), (40, 0)])
    MO.head_turn(arm, 30 + 2 * aim - 18 * down, 16 * down + 6 * aim)
    k = MO.kick_curve(f, 2, settle=14) if clip in ('Fire', 'AimFire') else 0.0
    if aim or k:
        MO.local_rots(arm, 'spine_02', [((0, 0, 1), 8 * aim), ((1, 0, 0), -4.0 * k)])


# ------------------------------------------------------------------------------------------- events, data

def events(clip):
    out = []
    for e in S.EVENTS.get(clip, []):
        d = {'frame': e[0], 'name': e[1]}
        if len(e) > 2:
            d['socket'] = e[2]
        out.append(d)
    return out


def release_intervals(clip):
    if clip == 'Equip':
        return [[0, 32]]
    if clip == 'Holster':
        return [[10, 36]]
    if clip == 'BoltCycle':
        return [[1, 25]]
    if clip == 'Reload':
        return [[2, 72]]
    if clip == 'Inspect':
        return [[84, 118]]
    if clip == 'Discard':
        return [[2, 40]]
    return []


def begin(clip, view):
    pass


def fp_framing():
    """What the game needs to frame the FP clips like this pack's camera (DA_BoltRifleRules)."""
    p = poses('FP')
    hip, aim = p['hip'], p['aim']
    hp, hy, hr = MO.rotator_in_view(hip)
    ap, ay, ar = MO.rotator_in_view(aim)
    eye_view = MO.to_view(aim @ EYE)
    return dict(camera_location_m=[*MO.FP_CAMERA], horizontal_fov_deg=90.0, grip_socket='socket_grip_r', align_bone='root',
                muzzle_socket='socket_muzzle',
                hip_grip_location_cm=[round(v, 2) for v in MO.to_view(hip @ GRIP_R)], hip_rotation_pyr_deg=[round(v, 3) for v in (hp, hy, hr)],
                aim_muzzle_location_cm=[round(v, 2) for v in MO.to_view(aim @ MUZZLE_SOCKET)], aim_rotation_pyr_deg=[round(v, 3) for v in (ap, ay, ar)],
                aim_field_of_view_deg=55.0,
                scope=dict(eye_socket='socket_scope_eye', eye_in_view_cm=[round(v, 3) for v in eye_view],
                           eyepiece_in_view_cm=[round(v, 2) for v in MO.to_view(aim @ Vector((S.SCOPE_X0, S.SCOPE_Y, S.SCOPE_Z)))],
                           objective_in_view_cm=[round(v, 2) for v in MO.to_view(aim @ Vector((S.SCOPE_X1, S.SCOPE_Y, S.SCOPE_Z)))],
                           eye_relief_cm=S.EYE_RELIEF * 100, eyepiece_radius_cm=1.2,
                           eyepiece_half_angle_deg=round(math.degrees(math.atan(0.012 / S.EYE_RELIEF)), 2),
                           near_clip_cm=5.0, lens_material='M_BoltRifle_Lens',
                           note='AimIdle frame 0 has the eye (socket_scope_eye) exactly on the camera and the scope axis along the '
                                'view: the eyepiece is centred on the crosshair 8 cm ahead (beyond the 5 cm near plane). '
                                'AimFieldOfView 55 is a proposal (the eyepiece fills half the view); a narrower one (zoom) needs a scope overlay or a render material on M_BoltRifle_Lens, otherwise the dark lens fills the screen.'),
                muzzle_in_view_hip_cm=[round(v, 2) for v in MO.to_view(hip @ MUZZLE_SOCKET)],
                note='HipGripLocation / HipRotation reproduce this camera exactly (Idle frame 0 = hip pose). AimMuzzleLocation / '
                     'AimRotation (aim_*) reproduce the authored aim (AimIdle frame 0): eye on the scope axis. Copy them into '
                     'DA_BoltRifleRules (and bAllowAiming true for the scope).')
