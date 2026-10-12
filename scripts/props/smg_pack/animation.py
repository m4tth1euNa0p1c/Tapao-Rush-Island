"""Bean handling of the drum sub-machine gun "Mitraillette" (T-334), first (FP) and third (TP) person, 30 fps, baked
on the bean skeleton and on the gun skeleton by period_arms_common/motion.animate. Clip names, lengths, events: spec.py.

  Equip 36 (DrawWeapon 4, Ready 30)   Holster 36 (StowWeapon 30)   Idle 90 / Walk 120 / Sprint 90 (loops)
  Fire 12 (Fire 1): the open bolt slams forward and back, a short kick
  FireLoop 30 (loop, Fire at 0, 3, 5, 8, 11, 14, 16, 19, 22, 25, 27 = 11 rounds/s): at every round the bolt is forward on
      the event frame and back on the next one (the brass knob on top runs with it), a kick with a per-round scatter, a
      periodic climb and a shudder; frame 30 = frame 0 (seamless)
  Reload 84: the gun rolled to show the drum, the left thumb presses the latch, the drum pulled down (MagOut 20),
      stowed on the belt, a full drum brought up and seated with a slap (MagIn 50), the left hand pulls the knob back
      (Charge 68) and returns to the fore-grip
  ReloadEmpty 96: the bolt is forward (fired dry); the drum pulled down (MagOut 16) and let fall (MagDrop 22: the bone is
      hidden 10 frames later; the game may drop SM_Ammo_MitrailletteDrum), a full drum seated (MagIn 54), a hard full
      charge from forward to the rear (Charge 76)
  DryFire 12 (DryClick 3)   Inspect 150   Discard 40 (Empty 6, Release 24)
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
AUTHORED = ('T-334: FP arms placed through the game camera (no stretch), TP two-segment arms from the musket pack; CoD easing; '
            'open bolt shuttling at every round (11 rounds/s), per round kick with scatter and a periodic climb; two-hand drum '
            'reload with the latch and the cocking knob.')

BZ = S.BORE_Z
GRIP_R = Vector(S.GRIP_R)
GRIP_L = Vector(S.GRIP_L)
MUZZLE = Vector((S.MUZZLE_X, 0.0, BZ))
DRUM_C = Vector(S.DRUM_C)
KNOB = Vector(S.KNOB)
LATCH = Vector(S.LATCH_PIVOT)
RECEIVER = Vector((0.090, 0.0, BZ))
SHOTS = list(S.SHOTS)
EMPTY_FRAME, RELEASE_FRAME = 6, 24
DRUM_GRAB = DRUM_C + Vector((-0.004, 0.028, -0.030))         # the left mitten on the drum: its left face, low


# ------------------------------------------------------------------------------------------- weapon poses

FP_POSES = {
    # hip: low on the right, the barrel toward the middle, rolled so the left flank (selectors, drum key) shows
    'hip': (GRIP_R, (30.0, 14.0, -21.0), (0.985, -0.12, 0.12), (-0.06, 0.28, 0.96)),
    'sprint': (GRIP_R, (28.0, 16.0, -19.0), (0.70, -0.62, -0.36), (0.30, 0.45, 0.84)),
    'holster': (GRIP_R, (18.0, 24.0, -64.0), (0.42, 0.25, -0.87), (0.88, -0.05, 0.47)),
    # reload: brought up and rolled hard to the right (left flank and underside to the eye): the drum, the latch, the well
    'reload': (RECEIVER, (37.0, -2.0, -3.0), (0.88, -0.40, 0.25), (-0.30, 0.80, 0.52)),
    # charging: the top of the receiver to the eye, the knob within reach of the left hand
    'charge': (RECEIVER, (38.0, 3.0, -12.0), (0.90, -0.33, 0.28), (0.10, -0.15, 0.98)),
    'inspect_left': (RECEIVER, (34.0, -1.0, -8.0), (0.74, -0.66, 0.15), (-0.25, 0.45, 0.86)),
    'inspect_right': (RECEIVER, (34.0, 4.0, -8.0), (0.80, -0.55, 0.22), (0.40, -0.55, 0.73)),
    'inspect_muzzle': (RECEIVER, (34.0, 10.0, -12.0), (0.45, -0.85, 0.27), (0.05, 0.25, 0.97)),
    'look': (RECEIVER, (34.0, 2.0, -10.0), (0.82, -0.52, 0.24), (-0.20, 0.45, 0.87)),
    'toss': (GRIP_R, (34.0, 40.0, -30.0), (0.30, 0.80, -0.52), (0.85, -0.20, 0.48)),
}


def fp_poses():
    return {k: MO.view_pose_dirs(*v) for k, v in FP_POSES.items()}


def tp_poses():
    at = MO.tp_pose_at
    return dict(
        # across the belly, the pistol grip at the right hip, the fore-grip in the left hand, muzzle forward left
        hip=at(GRIP_R, (-.13, -.25, .47), (.88, -.44, .12), -6),
        sprint=at(GRIP_R, (-.12, -.24, .50), (.62, -.40, -.68), 25),
        holster=at(GRIP_R, (-.30, -.20, .43), (.15, -.16, -.975), 12),
        reload=at(RECEIVER, (-.02, -.30, .58), (.85, -.38, .38), 70),
        charge=at(RECEIVER, (-.03, -.30, .58), (.86, -.38, .34), 15),
        inspect_left=at(RECEIVER, (-.02, -.31, .62), (.80, -.45, .40), 60),
        inspect_right=at(RECEIVER, (-.03, -.30, .60), (.82, -.40, .40), -50),
        inspect_muzzle=at(RECEIVER, (-.04, -.30, .58), (.60, -.78, .20), 10),
        look=at(RECEIVER, (-.03, -.30, .60), (.84, -.38, .38), 55),
        toss=at(GRIP_R, (-.36, -.24, .45), (-.30, -.50, -.81), 20),
    )


_POSES = {}


def poses(view):
    if view not in _POSES:
        _POSES[view] = fp_poses() if view == 'FP' else tp_poses()
    return _POSES[view]


def keyed(f, keys, view, over=0.05, antic=0.03):
    return MO.keyed_pose(f, keys, poses(view), over, antic)


# ------------------------------------------------------------------------------------- bolt, trigger, latch

def bolt_forward(clip, f):
    """How far forward of its rest (cocked, to the rear) the bolt is (m); negative = pulled back past the rest."""
    T = S.BOLT_TRAVEL
    if clip == 'FireLoop':
        return T if int(round(f)) % CLIPS['FireLoop'] in SHOTS else 0.0
    if clip == 'Fire':
        return curve(f, [(0, 0), (0.5, 0), (1, T), (2, 0.35 * T), (3, 0)])
    if clip == 'Reload':
        return curve(f, [(0, 0), (62, 0), (66, -0.012), (68, -0.012), (69, 0.002), (70, 0)])
    if clip == 'ReloadEmpty':
        return curve(f, [(0, T), (69, T), (74, -0.010), (76, -0.010), (77, 0.002), (78, 0)])
    if clip == 'DryFire':
        return T
    if clip == 'Discard':
        return curve(f, [(0, T), (3, T), (EMPTY_FRAME, -0.006), (8, T), (40, T)])
    if clip == 'Inspect':
        return curve(f, [(0, 0), (100, 0), (104, -0.010), (106, -0.010), (108, 0)])
    return 0.0


def trigger_angle(clip, f):
    if clip == 'FireLoop':
        return 16.0
    if clip == 'Fire':
        return curve(f, [(0, 0), (1, 16), (4, 16), (7, 0)])
    if clip == 'DryFire':
        return curve(f, [(0, 0), (3, 16), (6, 16), (9, 0)])
    return 0.0


def latch_angle(clip, f):
    if clip == 'Reload':
        return curve(f, [(0, 0), (11, 0), (14, -28), (19, -28), (22, 0)])
    if clip == 'ReloadEmpty':
        return curve(f, [(0, 0), (8, 0), (11, -28), (15, -28), (18, 0)])
    return 0.0


# ------------------------------------------------------------------------------------------- the weapon

SCATTER = [(0.5, -0.8), (-0.4, 0.6), (0.3, 1.0), (-0.6, -0.5), (0.45, 0.4), (-0.3, -0.9), (0.6, 0.7), (-0.5, 0.2), (0.2, -0.6),
           (-0.35, 0.9), (0.4, -0.3)]


def kick(clip, f):
    """Recoil envelope of the rounds at f (0..1.2) and the index of the last round."""
    shots = SHOTS if clip == 'FireLoop' else [1] if clip == 'Fire' else []
    k, idx = 0.0, -1
    n = CLIPS[clip]
    for i, s in enumerate(shots):
        d = f - s
        if clip == 'FireLoop' and d < 0:
            d += n
        if 0 <= d <= 4:
            k += curve(d, [(0, 0.0), (1, 1.0), (2, 0.40), (3, 0.10), (4, 0.0)])
        if s <= f:
            idx = i
    if clip == 'FireLoop' and idx < 0:
        idx = len(shots) - 1
    return min(k, 1.2), idx


def recoil(clip, f):
    k, i = kick(clip, f)
    if clip not in ('FireLoop', 'Fire'):
        return Matrix.Identity(4)
    yaw, roll = SCATTER[i % len(SCATTER)] if i >= 0 else (0.0, 0.0)
    n = CLIPS[clip]
    climb = hum = 0.0
    if clip == 'FireLoop':
        # the muzzle climbs and is pulled down again over the loop (periodic), a fine shudder
        climb = 0.5 - 0.5 * math.cos(2 * math.pi * f / n)
        hum = math.sin(2 * math.pi * 7 * f / n) * 0.35 + math.sin(2 * math.pi * 4 * f / n + 1.1) * 0.25
    m = Matrix.Translation((-0.009 * k, 0.0004 * hum, 0.0016 * k + 0.0004 * hum + 0.004 * climb))
    return m @ Matrix.Rotation(math.radians(-1.6 * k - 2.2 * climb), 4, 'Y') @ Matrix.Rotation(math.radians(yaw * k), 4, 'Z') \
        @ Matrix.Rotation(math.radians(roll * k + 0.3 * hum), 4, 'X')


RELOAD_KEYS = [(0, 'hip'), (8, 'reload'), (52, 'reload'), (58, 'charge'), (70, 'charge'), (80, 'hip')]
RELOAD_EMPTY_KEYS = [(0, 'hip'), (7, 'reload'), (58, 'reload'), (64, 'charge'), (79, 'charge'), (90, 'hip')]
INSPECT_KEYS = [(0, 'hip'), (20, 'inspect_left'), (58, 'inspect_left'), (76, 'inspect_right'), (96, 'inspect_right'),
                (114, 'inspect_muzzle'), (128, 'inspect_muzzle'), (146, 'hip')]


def discard_flight(p, f, view):
    toss = p['toss']
    dt = (f - RELEASE_FRAME) / FPS
    if view == 'FP':
        v = vv(0.5, 2.2, 0.3) * 2.0
        spin = vv(0.4, 0.2, -0.9)
    else:
        v = Vector((-1.4, -0.5, 0.6))
        spin = Vector((0.2, -0.9, 0.3))
    pos = toss.translation + v * dt + Vector((0, 0, -4.905 * dt * dt))
    rot = Quaternion(spin.normalized(), dt * 6.0).to_matrix().to_4x4() @ toss.to_3x3().to_4x4()
    if view == 'TP' and pos.z < 0.09:
        pos.z = 0.09
    rot.translation = pos
    if view == 'TP':
        rest = Matrix.Translation((-0.85, -0.40, 0.083)) @ Matrix.Rotation(math.radians(-65), 4, 'Z') @ Matrix.Rotation(math.radians(-90), 4, 'X')
        rot = blend(rot, rest, smooth(RELEASE_FRAME + 4, CLIPS['Discard'], f))
    return rot


def weapon_pose(clip, f, view):
    p = poses(view)
    end = CLIPS[clip]
    base = p['hip']
    if clip == 'Sprint':
        base = p['sprint']
    elif clip == 'Equip':
        base = blend(p['holster'], p['hip'], ease_os(0, end - 7, f, over=0.07))
        dip = curve(f, [(end - 8, 0), (end - 5, 1), (end - 2, -0.3), (end, 0)])
        base = base @ Matrix.Translation((0, 0, -0.010 * dip))
    elif clip == 'Holster':
        base = blend(p['hip'], p['holster'], ease_os(6, end, f, antic=0.06))
    elif clip in ('Reload', 'ReloadEmpty'):
        base = keyed(f, RELOAD_KEYS if clip == 'Reload' else RELOAD_EMPTY_KEYS, view, over=0.06, antic=0.03)
        if clip == 'Reload':
            jolt = curve(f, [(13, 0), (14, 0.5), (16, 0)]) + curve(f, [(19, 0), (20, 0.8), (23, 0)]) + curve(f, [(49, 0), (50, 1.2), (53, -0.3), (55, 0)]) \
                + curve(f, [(66, 0), (68, 1.0), (71, 0)])
        else:
            jolt = curve(f, [(15, 0), (16, 0.8), (19, 0)]) + curve(f, [(53, 0), (54, 1.2), (57, -0.3), (59, 0)]) + curve(f, [(74, 0), (76, 1.3), (79, 0)])
        base = base @ Matrix.Translation((0, 0, -0.004 * jolt)) @ Matrix.Rotation(math.radians(1.4 * jolt), 4, 'Y')
    elif clip == 'Inspect':
        base = keyed(f, INSPECT_KEYS, view, over=0.05, antic=0.03)
        pat = curve(f, [(86, 0), (88, 1), (91, 0)])
        base = base @ Matrix.Translation((0, 0, 0.004 * pat))
    elif clip == 'Discard':
        if f <= 10:
            base = blend(p['hip'], p['look'], ease_os(0, 10, f, over=0.05))
        elif f <= 16:
            base = p['look']
        elif f <= RELEASE_FRAME:
            base = blend(p['look'], p['toss'], ease_os(16, RELEASE_FRAME, f, antic=0.08))
        else:
            base = discard_flight(p, f, view)
    if clip == 'Idle':
        q = math.sin(2 * math.pi * f / end)
        q2 = math.sin(4 * math.pi * f / end + 0.7)
        base = base @ Matrix.Translation((0.0012 * q2, 0.0014 * q, 0.0035 * q)) @ Matrix.Rotation(math.radians(0.55 * q), 4, 'Y') \
            @ Matrix.Rotation(math.radians(0.35 * q2), 4, 'X')
    if clip in ('Walk', 'Sprint'):
        q = 2 * math.pi * f / end * 4
        amp = 1.0 if clip == 'Walk' else 2.2
        base = base @ Matrix.Translation((0.003 * amp * math.sin(q), 0.006 * amp * math.sin(q / 2), 0.006 * amp * math.sin(q + 0.6)))
        base = base @ Matrix.Rotation(math.radians(2.4 * amp * math.sin(q / 2 - 0.4)), 4, 'X') @ Matrix.Rotation(math.radians(1.3 * amp * math.sin(q + 0.6)), 4, 'Y')
    if clip == 'DryFire':
        j = curve(f, [(2, 0), (3, 1), (6, 0)])
        base = base @ Matrix.Translation((-0.003 * j, 0, 0)) @ Matrix.Rotation(math.radians(-0.6 * j), 4, 'Y')
    return base @ recoil(clip, f)


# ------------------------------------------------------------------------------------------------ drum

POUCH_FP = ('v', (14.0, -32.0, -56.0), Vector((.15, -.22, .40)))
POUCH_TP = Vector((.13, -.21, .40))


def drum_state(clip, f, view, M, hands):
    """Matrix delta of the drum bone (weapon space, or ('bean', W) when carried, ('hide', D) when away)."""
    if clip not in ('Reload', 'ReloadEmpty'):
        return Matrix.Identity(4)
    empty = clip == 'ReloadEmpty'
    out_f, in_f = (16, 54) if empty else (20, 50)
    unhook = curve(f, [(out_f - 6, 0), (out_f - 2, 0.012), (out_f, 0.040)])
    tilt = curve(f, [(out_f - 4, 0), (out_f, 8.0)])

    def unseated(down, deg):
        return Matrix.Translation((0, 0, -down)) @ Matrix.Translation(DRUM_C) @ Matrix.Rotation(math.radians(deg), 4, 'X') @ Matrix.Translation(-DRUM_C)

    if f <= out_f:
        return unseated(unhook, tilt)
    if empty:
        # MagDrop at 22: let go, it falls; hidden 10 frames later
        drop_f = 22
        if f <= drop_f:
            return unseated(0.040 + 0.010 * smooth(out_f, drop_f, f), 8.0)
        t = (f - drop_f) / FPS
        fall = Matrix.Translation((0.0, 0.0, -0.050 - 0.6 * t - 4.905 * t * t))
        D = fall @ Matrix.Translation(DRUM_C) @ Matrix.Rotation(math.radians(8 + 140 * t), 4, 'X') @ Matrix.Translation(-DRUM_C)
        if f > drop_f + 10 and f < in_f - 14:
            return ('hide', D)
        if f < in_f - 14:
            return D
    # carried by the left hand (away to the belt, then a full drum back) in bean space; seated again at in_f
    seat0 = in_f - 6
    if f < seat0:
        # the grab point at the fingers of the left mitten (the 'drum' hand targets put the mitten there)
        pos = hands['l'] + (hands['lrot'] @ Vector((0.030, 0.0, 0.012)) if hands.get('lrot') is not None else Vector((0, 0, 0)))
        # the drum hangs from the mitten by its grab point, tilted a little
        W = Matrix.Translation(pos) @ M.to_3x3().to_4x4() @ Matrix.Rotation(math.radians(10), 4, 'X') @ Matrix.Translation(-DRUM_GRAB)
        away = (out_f + 7 <= f <= in_f - 16) if not empty else (f <= in_f - 16)
        return ('bean_hide', W) if away else ('bean', W)
    push = curve(f, [(seat0, 0.045), (in_f - 1, 0.002), (in_f, -0.0015), (in_f + 2, 0.0)])
    return unseated(push, 6.0 * (1 - smooth(seat0, in_f, f)))


def parts(clip, f, view, M, hands):
    out = {'bolt': Matrix.Translation((bolt_forward(clip, f), 0, 0))}
    piv = Vector(S.TRIGGER_PIVOT)
    out['trigger'] = Matrix.Translation(piv) @ Matrix.Rotation(math.radians(trigger_angle(clip, f)), 4, 'Y') @ Matrix.Translation(-piv)
    out['mag_latch'] = Matrix.Translation(LATCH) @ Matrix.Rotation(math.radians(latch_angle(clip, f)), 4, 'Y') @ Matrix.Translation(-LATCH)
    out['drum'] = drum_state(clip, f, view, M, hands)
    return out


# --------------------------------------------------------------------------------------------- the hands

REACH = {'r': (8.0, 26.0, -48.0), 'l': (14.0, -30.0, -46.0)}
WORK_L = (14.0, -40.0, -38.0)
OVER_L = (20.0, -36.0, -20.0)
GRIP_U, FORE_U, DRUM_U, KNOB_U = (0, .55, .85), (0.10, -0.85, 0.50), (0.0, -0.75, -0.65), (-.10, -.30, .95)
FREE_R = ('v', (20.0, 26.0, -58.0), Vector((-.29, -.10, .36)))
FREE_L = ('v', (20.0, -26.0, -58.0), Vector((.29, -.10, .36)))


def resolver(clip, f, M):
    fwd = bolt_forward(clip, f)

    def resolve(spec, view):
        kind = spec[0]
        if kind == 'w':
            return M @ Vector(spec[1])
        if kind == 'v':
            return vp(*spec[1]) if view == 'FP' else Vector(spec[2])
        if kind == 'knob':
            return M @ (KNOB + Vector((fwd, 0.0, 0.0))), ((0.042, 0.0, 0.010), (0.0, 0.0, -0.010))
        if kind == 'latch':
            return M @ (LATCH + Vector((0.016, 0.004, -0.010))), ((0.045, 0.0, 0.0), (0.0, 0.0, 0.0))
        if kind == 'drum':
            return M @ (DRUM_GRAB + Vector((0.0, 0.0, -spec[1]))), ((0.030, 0.0, 0.012), (0.0, 0.0, 0.0))
        raise ValueError(spec)
    return resolve


def right_keys(clip, view):
    rr = REACH['r']
    G = ('w', GRIP_R)
    if clip == 'Equip':
        return [(0, FREE_R, rr, GRIP_U), (8, FREE_R, rr, GRIP_U), (22, G, rr, GRIP_U)]
    if clip == 'Holster':
        return [(0, G, rr, GRIP_U), (24, G, rr, GRIP_U), (36, FREE_R, rr, GRIP_U)]
    if clip == 'Discard':
        drop = ('v', (16.0, 30.0, -62.0), Vector((-.27, -.06, .34)))
        off = ('v', (26.0, 38.0, -42.0), Vector((-.36, -.20, .40)))
        return [(0, G, rr, GRIP_U), (RELEASE_FRAME, G, rr, GRIP_U), (30, off, rr, GRIP_U), (40, drop, rr, GRIP_U)]
    if clip in ('Fire', 'FireLoop', 'DryFire'):
        return [(0, G, rr, (0, .45, .9))]
    return [(0, G, rr, GRIP_U)]


def left_keys(clip, view):
    rl = REACH['l']
    L = ('w', GRIP_L)
    K = ('knob',)
    pouch = POUCH_FP if view == 'FP' else ('v', POUCH_FP[1], POUCH_TP)
    if clip == 'Reload':
        return [(0, L, rl, FORE_U), (6, L, rl, FORE_U), (11, ('latch',), WORK_L, DRUM_U), (14, ('latch',), WORK_L, DRUM_U),
                (17, ('drum', 0.0), WORK_L, DRUM_U), (20, ('drum', 0.040), WORK_L, DRUM_U), (28, pouch, WORK_L, DRUM_U),
                (34, pouch, WORK_L, DRUM_U), (42, ('drum', 0.045), WORK_L, DRUM_U), (48, ('drum', 0.0), WORK_L, DRUM_U),
                (50, ('drum', -0.002), WORK_L, DRUM_U), (52, ('drum', 0.0), WORK_L, DRUM_U), (60, K, OVER_L, KNOB_U),
                (70, K, OVER_L, KNOB_U), (78, L, rl, FORE_U), (84, L, rl, FORE_U)]
    if clip == 'ReloadEmpty':
        return [(0, L, rl, FORE_U), (5, L, rl, FORE_U), (9, ('latch',), WORK_L, DRUM_U), (11, ('latch',), WORK_L, DRUM_U),
                (14, ('drum', 0.0), WORK_L, DRUM_U), (16, ('drum', 0.040), WORK_L, DRUM_U), (22, ('drum', 0.050), WORK_L, DRUM_U),
                (30, pouch, WORK_L, DRUM_U), (38, pouch, WORK_L, DRUM_U), (46, ('drum', 0.045), WORK_L, DRUM_U), (52, ('drum', 0.0), WORK_L, DRUM_U),
                (54, ('drum', -0.002), WORK_L, DRUM_U), (56, ('drum', 0.0), WORK_L, DRUM_U), (66, K, OVER_L, KNOB_U),
                (79, K, OVER_L, KNOB_U), (88, L, rl, FORE_U), (96, L, rl, FORE_U)]
    if clip == 'Inspect':
        return [(0, L, rl, FORE_U), (80, L, rl, FORE_U), (85, ('drum', 0.004), WORK_L, DRUM_U), (88, ('drum', -0.002), WORK_L, DRUM_U),
                (91, ('drum', 0.004), WORK_L, DRUM_U), (96, L, rl, FORE_U), (98, K, OVER_L, KNOB_U), (108, K, OVER_L, KNOB_U),
                (114, L, rl, FORE_U), (150, L, rl, FORE_U)]
    if clip == 'Discard':
        return [(0, L, rl, FORE_U), (2, K, OVER_L, KNOB_U), (8, K, OVER_L, KNOB_U), (14, L, rl, FORE_U), (16, L, rl, FORE_U),
                (22, ('v', (22.0, -28.0, -44.0), Vector((.28, -.10, .36))), rl, FORE_U),
                (40, ('v', (16.0, -30.0, -62.0), Vector((.27, -.06, .34))), rl, FORE_U)]
    if clip == 'Equip':
        return [(0, FREE_L, rl, FORE_U), (12, FREE_L, rl, FORE_U), (28, L, rl, FORE_U)]
    if clip == 'Holster':
        return [(0, L, rl, FORE_U), (10, L, rl, FORE_U), (26, FREE_L, rl, FORE_U)]
    return [(0, L, rl, FORE_U)]


def clip_hands(clip, f, view, M):
    resolve = resolver(clip, f, M)
    right = MO.keyed_hand(right_keys(clip, view), f, view, resolve)
    left = MO.keyed_hand(left_keys(clip, view), f, view, resolve)
    return right, left


# -------------------------------------------------------------------------------------------- TP body

def tp_body(arm, clip, f):
    down = 0.0
    if clip == 'Reload':
        down = curve(f, [(0, 0), (8, 1), (72, 1), (82, 0)])
    elif clip == 'ReloadEmpty':
        down = curve(f, [(0, 0), (7, 1), (82, 1), (92, 0)])
    elif clip == 'Inspect':
        down = curve(f, [(0, 0), (20, 0.8), (135, 0.8), (148, 0)])
    elif clip == 'Discard':
        down = curve(f, [(0, 0), (8, 1), (16, 1), (RELEASE_FRAME + 2, 0.3), (40, 0)])
    MO.head_turn(arm, 30 - 18 * down, 16 * down)
    k, _ = kick(clip, f)
    if k or clip == 'FireLoop':
        hum = math.sin(2 * math.pi * 7 * f / CLIPS[clip]) if clip == 'FireLoop' else 0.0
        MO.local_rots(arm, 'spine_02', [((1, 0, 0), -2.5 * k - 0.6 * hum)])


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
        return [[0, 28]]
    if clip == 'Holster':
        return [[10, 36]]
    if clip == 'Reload':
        return [[6, 78]]
    if clip == 'ReloadEmpty':
        return [[5, 88]]
    if clip == 'Inspect':
        return [[80, 114]]
    if clip == 'Discard':
        return [[1, 40]]
    return []


def begin(clip, view):
    pass


def fp_framing():
    p = poses('FP')
    hip = p['hip']
    hp, hy, hr = MO.rotator_in_view(hip)
    return dict(camera_location_m=[*MO.FP_CAMERA], horizontal_fov_deg=90.0, grip_socket='socket_grip_r', align_bone='root',
                muzzle_socket='socket_muzzle',
                hip_grip_location_cm=[round(v, 2) for v in MO.to_view(hip @ GRIP_R)], hip_rotation_pyr_deg=[round(v, 3) for v in (hp, hy, hr)],
                muzzle_in_view_cm=[round(v, 2) for v in MO.to_view(hip @ Vector(S.SOCKETS['muzzle'][1]))],
                fore_grip_in_view_cm=[round(v, 2) for v in MO.to_view(hip @ GRIP_L)],
                aim='none: the sub-machine gun is fired from the hip (no AimIdle clip; bAimPoseValid stays false)',
                note='HipGripLocation / HipRotation reproduce this camera exactly (Idle frame 0 = hip pose, socket_grip_r = the pistol grip).')
