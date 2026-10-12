"""Bean musket handling, first (FP) and third (TP) person, baked on the bean skeleton and the musket skeleton
(T-212, v2 T-246 "animations type Call of Duty").

Local rewrite of the second PC's bean-musket-anim v0.2 animation.py (OneDrive, read only); same clip names, 24 fps.
Metres, Blender axes: the bean looks along -Y, its right side is -X, +Z is up. Weapon local: +X to the muzzle,
+Z up, -Y is the lock side (its right side).

First person (owner requests 2026-10-07 and 2026-10-09):
  - authored through a camera FP_CAMERA that stands for the game's first person camera (90 deg horizontal), every FP
    weapon pose is given in that view (cm, UE axes: X forward, Y right, Z up, rotations as FRotator pitch / yaw / roll);
  - all the non reload clips keep ONE weapon pose ("base"): the game frames it at the hip (HipGripLocation /
    HipRotation, written by the pack in fp-framing.json) and on the view axis when aiming (AimMuzzleLocation /
    AimRotation, fp_framing.aim_*: the sight line of the model on the camera axis), so Fire, Walk, AimIdle... play
    right in both framings (AFirearmWeapon measures Idle and AimIdle frame 0: a distinct authored aim pose would make the
    single Fire clip, also played aimed, jump);
  - the first person arms (FPArm sleeve, cuff, mitten: rigid on hand_l / hand_r) are placed directly: hand centre on
    its target, forearm along the line from a virtual elbow out of the frame, back of the hand oriented per gesture;
  - manipulations (reload, inspect, discard) bring the weapon up and tilt the lock toward the eye so the hands read at
    the centre of the view; moves anticipate, overshoot a little and settle (ease_os); the shot kicks hard (muzzle
    +7 deg, 3.8 cm back over 2 frames) and returns in 10 frames.
Reload (GroundReload, 5.5 s, both views, synchronised hands): half cock with the thumb, cartridge from the pouch, bite,
prime the pan, shut the pan, cast about to the loading pose (the butt thuds on the ground), powder down the barrel,
ball and paper pushed in, ramrod drawn in two pulls, turned, rammed in two sharp strokes, drawn back, turned, slid home
into its pipes, back to the hip and full cock. The left hand never leaves the musket. The cartridge is a bone of the
musket ("cartridge"), hidden in the butt when not in hand.
Inspect: the lock raised and turned to the eye, then the barrel top and muzzle. Discard (empty weapon): dry click,
a look, a sharp shake, thrown to the right and down (Release); the bean clip ends empty handed, the weapon clip keeps
falling after Release (the game drops it as a pickup at Release).
"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix, Quaternion, Vector

FPS = 24
GRIP_R = Vector((0.0, 0.0, -0.002))
CLIPS = {'Equip': 42, 'Idle': 72, 'Walk': 96, 'Sprint': 72, 'AimIn': 24, 'AimIdle': 72,
         'AimOut': 24, 'HalfCock': 24, 'PrimePan': 24, 'CloseFrizzen': 12,
         'GroundReload': 132, 'Powder': 24, 'BallAndPatch': 20, 'DrawRamrod': 24,
         'TurnRamrod': 24, 'RamrodStroke': 29, 'ReturnRamrod': 24, 'Cock': 24,
         'Present': 18, 'Fire': 24, 'Recoil': 20, 'Hold': 48, 'ReturnToPort': 24, 'Holster': 42,
         'Inspect': 96, 'Discard': 32}
# the reload's sub-gestures are windows of GroundReload (start, end frame), replayed over their own length
RELOAD_WINDOWS = {'HalfCock': (0, 9), 'PrimePan': (9, 33), 'CloseFrizzen': (33, 38), 'Powder': (38, 62),
                  'BallAndPatch': (62, 72), 'DrawRamrod': (72, 86), 'TurnRamrod': (86, 93),
                  'RamrodStroke': (93, 110), 'ReturnRamrod': (110, 122), 'Cock': (122, 132)}
RELEASE_FRAME = 22            # Discard: the musket leaves the right hand
EMPTY_FRAME = 1               # Discard: the cock falls on the empty pan

# weapon points (weapon local, metres)
GRIP_L = Vector((0.26, 0.0, 0.012))
BAND_GRIP = Vector((0.405, 0.0, 0.030))
MUZZLE = Vector((0.626, 0.0, 0.045))
BORE_Z = 0.045
SIGHT_Z = 0.0665                            # musket_model.SIGHT_Z: the sight line above the bore axis
SIGHT_REAR_X, SIGHT_FRONT_X = 0.080, 0.580
PAN = Vector((0.068, -0.027, 0.050))
HAMMER_SPUR = Vector((0.030, -0.034, 0.080))
FRIZZEN_TOE = Vector((0.082, -0.031, 0.084))
# ramrod: rest head (button end) and direction, length head -> trumpet head end
ROD_HEAD = Vector((0.10, 0.0, 0.0))
ROD_DIR = Vector((0.508, 0.0, 0.008)).normalized()
ROD_LEN = 0.52
# hammer and frizzen angles (deg, forward about the weapon's Y at their pivots): the rest pose is full cock / pan shut
HAMMER_FIRED, HAMMER_HALF = 40.0, 18.0
FRIZZEN_OPEN = 62.0
# aim framing (game, DA_PirateMusketRules): muzzle socket this far ahead of the eye, sight line on the view axis
AIM_MUZZLE_CM = 95.0
AIM_FOV_DEG = 65.0


# ----------------------------------------------------------------------------------------------------- easing

def clamp01(t):
    return max(0.0, min(1.0, t))


def smooth(a, b, f):
    t = clamp01((f - a) / max(1e-6, b - a))
    return t * t * (3 - 2 * t)


def curve(f, keys):
    if f <= keys[0][0]:
        return keys[0][1]
    for (a, x), (b, y) in zip(keys, keys[1:]):
        if f <= b:
            return x + (y - x) * smooth(a, b, f)
    return keys[-1][1]


def ease_os(a, b, f, over=0.0, antic=0.0):
    """0 -> 1 from frame a to b, Call of Duty style: a small anticipation against the move, a fast middle, a
    slight overshoot past the target and a settle."""
    t = clamp01((f - a) / max(1e-6, b - a))
    keys = [(0.0, 0.0)]
    if antic:
        keys.append((0.16, -antic))
    if over:
        keys.append((0.72, 1.0 + over))
    keys.append((1.0, 1.0))
    return curve(t, keys)


def segment(f, keys):
    """(index of the key at or before f, eased weight toward the next key)."""
    if f <= keys[0][0]:
        return 0, 0.0
    for i, (k0, k1) in enumerate(zip(keys, keys[1:])):
        if f <= k1[0]:
            return i, smooth(k0[0], k1[0], f)
    return len(keys) - 1, 0.0


def segment_os(f, keys, over=0.06, antic=0.03):
    """segment() with the Call of Duty easing on every move between two different keys."""
    if f <= keys[0][0]:
        return 0, 0.0
    for i, (k0, k1) in enumerate(zip(keys, keys[1:])):
        if f <= k1[0]:
            if k0[1] == k1[1]:
                return i, 0.0
            return i, ease_os(k0[0], k1[0], f, over, antic)
    return len(keys) - 1, 0.0


def frame(x, origin, roll=0):
    x = Vector(x).normalized()
    z = Vector((0, 0, 1))
    z = (z - x * x.dot(z)).normalized()
    y = z.cross(x)
    out = Matrix((x, y, z)).transposed().to_4x4()
    if roll:
        out = out @ Matrix.Rotation(math.radians(roll), 4, 'X')
    out.translation = Vector(origin)
    return out


def axes(x, up):
    """Rotation whose columns are x, z x x, z (z = up made orthogonal to x)."""
    x = Vector(x).normalized()
    z = Vector(up) - x * x.dot(Vector(up))
    if z.length < 1e-6:
        z = Vector((0, 0, 1)) - x * x.z
    z.normalize()
    return Matrix((x, z.cross(x), z)).transposed()


def qpow(q, t):
    axis, angle = q.to_axis_angle()
    return Quaternion(axis, angle * t)


def blend(a, b, w):
    """Pose blend; w outside [0, 1] extrapolates (overshoot) along the same shortest rotation."""
    qa, qb = a.to_quaternion(), b.to_quaternion()
    if 0.0 <= w <= 1.0:
        q = qa.slerp(qb, w)
    else:
        d = qa.inverted() @ qb
        if d.w < 0:
            d.negate()
        q = qa @ qpow(d, w)
    out = q.to_matrix().to_4x4()
    out.translation = a.translation + (b.translation - a.translation) * w
    return out


# ----------------------------------------------------------------------------------------- first person camera

FP_CAMERA = Vector((0.0, 0.0, 0.80))      # eye of the authoring camera (bean space)
FWD, RIGHT, UP = Vector((0, -1, 0)), Vector((-1, 0, 0)), Vector((0, 0, 1))
FP_LENS = 18.0                              # 36 mm sensor: 90 deg horizontal, as the game's FirstPersonCamera


def camera_matrix():
    m = Matrix((RIGHT, UP, -FWD)).transposed().to_4x4()
    m.translation = FP_CAMERA
    return m


def vp(x, y, z):
    """View point (cm; X forward, Y right, Z up) -> bean space (m)."""
    return FP_CAMERA + (FWD * x + RIGHT * y + UP * z) * 0.01


def vv(x, y, z):
    """View direction -> bean space (unit)."""
    return (FWD * x + RIGHT * y + UP * z).normalized()


def to_view(p):
    """Bean space point -> view (cm)."""
    d = (Vector(p) - FP_CAMERA) * 100.0
    return Vector((d.dot(FWD), d.dot(RIGHT), d.dot(UP)))


def ue_axes(pitch, yaw, roll):
    """X, Y, Z axes (UE view coordinates) of an FRotator, as FRotationMatrix."""
    p, y, r = (math.radians(v) for v in (pitch, yaw, roll))
    cp, sp, cy, sy, cr, sr = math.cos(p), math.sin(p), math.cos(y), math.sin(y), math.cos(r), math.sin(r)
    return (Vector((cp * cy, cp * sy, sp)), Vector((sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp)),
            Vector((-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp)))


def view_rotation(pitch, yaw, roll):
    """Weapon rotation (bean space, Blender weapon axes) for an FRotator in the view (UE weapon axes = Blender's with
    Y flipped)."""
    X, Y, Z = ue_axes(pitch, yaw, roll)
    cols = [FWD * X.x + RIGHT * X.y + UP * X.z, -(FWD * Y.x + RIGHT * Y.y + UP * Y.z), FWD * Z.x + RIGHT * Z.y + UP * Z.z]
    return Matrix(cols).transposed()


def view_pose(anchor_local, anchor_cm, rot):
    """Weapon root matrix (bean space) putting the weapon point anchor_local at anchor_cm in the view."""
    R = view_rotation(*rot)
    m = R.to_4x4()
    m.translation = vp(*anchor_cm) - R @ Vector(anchor_local)
    return m


def rotator_in_view(m):
    """FRotator (pitch, yaw, roll) of a weapon matrix in the view (inverse of view_rotation)."""
    def ue(v):
        return Vector((v.dot(FWD), v.dot(RIGHT), v.dot(UP)))
    X = ue(m.col[0].xyz)
    Y = -ue(m.col[1].xyz)
    Z = ue(m.col[2].xyz)
    pitch = math.degrees(math.asin(max(-1.0, min(1.0, X.z))))
    yaw = math.degrees(math.atan2(X.y, X.x))
    roll = math.degrees(math.atan2(-Y.z, Z.z))
    return pitch, yaw, roll


# FP weapon poses: (weapon point, where it is in the view (cm), FRotator in the view)
FP_POSES = {
    # held at the hip, low on the right, barrel toward the middle of the screen, lock rolled a little to the eye
    'base': (GRIP_R, (31.0, 14.0, -19.0), (6.0, -6.0, -12.0)),
    # priming: brought across the chest, lock turned up toward the eye, at the centre of the view
    'prime': (PAN, (33.0, 0.5, -5.0), (14.0, -30.0, -66.0)),
    # loading: butt lowered toward the ground, barrel up and away, muzzle up left of the middle
    'load': (MUZZLE, (52.0, -8.0, 12.0), (56.0, -8.0, 6.0)),
    # put away below the screen
    'holster': (GRIP_R, (18.0, 24.0, -62.0), (-55.0, 15.0, -40.0)),
    # sprint: low, turned across the body, still well in view
    'sprint': (GRIP_R, (30.0, 15.0, -17.0), (-14.0, -32.0, -42.0)),
    # inspect / discard: the lock raised to the eye
    'look': (PAN, (28.0, -2.0, -5.0), (12.0, -16.0, -78.0)),
    # inspect: barrel top and muzzle shown, canted
    'show_barrel': (BAND_GRIP, (36.0, 3.0, -7.0), (36.0, -12.0, 52.0)),
    # discard: the musket leaving the right hand, to the right and down
    'throw': (GRIP_R, (36.0, 14.0, -12.0), (-8.0, 62.0, -20.0)),
}
FP_SHOULDER = {'r': (-4.0, 20.0, -36.0), 'l': (-4.0, -20.0, -36.0)}   # invisible shoulders of the FP arms (view cm)


def fp_pose(name):
    anchor, where, rot = FP_POSES[name]
    return view_pose(anchor, where, rot)


# TP weapon poses (bean space; v0.2 poses for ready / aim / holster / inspect)
def tp_pose_at(anchor_local, anchor_world, direction, roll):
    m = frame(direction, (0, 0, 0), roll)
    m.translation = Vector(anchor_world) - m.to_3x3() @ Vector(anchor_local)
    return m


def tp_poses():
    return dict(ready=frame((.955, -.25, .16), (-.16, -.25, .51), -8),
                aim=frame((.89, -.45, .02), (-.17, -.20, .615)),
                holster=frame((.15, -.16, -.975), (-.30, -.20, .43), 12),
                inspect=frame((.88, -.35, .32), (-.16, -.25, .59), -25),
                # priming: across the chest, lock up, pan under the chin
                prime=tp_pose_at(PAN, (-.035, -.245, .585), (.80, -.42, .30), -68),
                # loading: nearly upright in front of the belly, butt just above the ground, muzzle at chin height
                load=tp_pose_at(MUZZLE, (-.05, -.30, .78), (.10, -.36, .93), 6),
                # inspect / discard: the lock lifted toward the face
                look=tp_pose_at(PAN, (-.06, -.26, .64), (.84, -.40, .36), -75),
                show_barrel=tp_pose_at(BAND_GRIP, (-.10, -.30, .66), (.55, -.45, .70), 40),
                # discard: in the right hand, swung out to the right and down
                throw=tp_pose_at(GRIP_R, (-.36, -.22, .46), (-.35, -.55, -.76), 10))


def poses(view):
    if view == 'FP':
        return {k: fp_pose(k) for k in FP_POSES}
    return tp_poses()


# --------------------------------------------------------------------------------------------- weapon pose

RELOAD_POSE_KEYS = [(0, 'base'), (8, 'prime'), (36, 'prime'), (46, 'load'), (120, 'load'), (129, 'base'), (132, 'base')]
INSPECT_KEYS = [(0, 'base'), (14, 'look'), (44, 'look'), (60, 'show_barrel'), (80, 'show_barrel'), (96, 'base')]
DISCARD_KEYS = [(0, 'base'), (8, 'look'), (18, 'look'), (RELEASE_FRAME, 'throw')]


def keyed_pose(f, keys, view, over=0.06, antic=0.03):
    p = poses(view)
    keys = [(k, 'ready' if (view == 'TP' and n == 'base') else n) for k, n in keys]
    i, w = segment_os(f, keys, over, antic)
    a = p[keys[i][1]]
    b = p[keys[min(i + 1, len(keys) - 1)][1]]
    return blend(a, b, w)


def reload_pose(f, view):
    m = keyed_pose(f, RELOAD_POSE_KEYS, view, over=0.07, antic=0.04)
    # small life: the bite tugs, the stock thuds on the ground, every ramming stroke jolts the barrel
    dip = (curve(f, [(18, 0), (20, 1), (22, 0)]) * 0.006 + curve(f, [(45, 0), (47.5, 1), (50, 0)]) * 0.012
           + curve(f, [(97, 0), (99, 1), (101, 0)]) * 0.009 + curve(f, [(102, 0), (104, 1), (106, 0)]) * 0.009)
    pour = curve(f, [(24, 0), (27, 1), (30, 0)]) * 2.0 + curve(f, [(50, 0), (54, 1), (58, 0)]) * 2.5
    return Matrix.Translation((0, 0, -dip)) @ m @ Matrix.Rotation(math.radians(pour), 4, 'X')


def discard_pose(f, view):
    """Look, shake, swing out; after RELEASE_FRAME the musket flies on its own (right and down, spinning)."""
    m = keyed_pose(min(f, RELEASE_FRAME), DISCARD_KEYS, view, over=0.05, antic=0.08)
    jolt = curve(f, [(11, 0), (12.5, 1), (14, -0.8), (15.5, 0.6), (17, -0.3), (18.5, 0)])
    m = m @ Matrix.Translation((0.0, 0.0, 0.028 * jolt)) @ Matrix.Rotation(math.radians(9.0 * jolt), 4, 'X')
    if f <= RELEASE_FRAME:
        return m
    t = (f - RELEASE_FRAME) / FPS
    if view == 'FP':
        v = vv(0.35, 1.0, -0.45) * 2.2                 # m/s, right and down in the view
    else:
        v = Vector((-1.6, -0.5, 0.6))                   # bean space: to its right, a little forward and up
    fall = v * t + Vector((0, 0, -9.81)) * 0.5 * t * t
    spin = Matrix.Rotation(math.radians(240.0 * t), 4, 'Y') @ Matrix.Rotation(math.radians(90.0 * t), 4, 'X')
    out = Matrix.Translation(fall) @ m @ spin
    if view == 'TP':
        # on the ground by the end of the clip: lying flat, lock up
        rest = Matrix.Translation((-0.95, -0.35, 0.032)) @ Matrix.Rotation(math.radians(-70), 4, 'Z') @ Matrix.Rotation(math.radians(-90), 4, 'X')
        out = blend(out, rest, smooth(RELEASE_FRAME + 4, CLIPS['Discard'], f))
    return out


def inspect_pose(f, view):
    m = keyed_pose(f, INSPECT_KEYS, view, over=0.06, antic=0.04)
    # a slow turn while the lock is shown, a slow turn of the barrel
    turn = curve(f, [(16, 0), (44, 1)]) * 10.0 - curve(f, [(62, 0), (80, 1)]) * 8.0
    return m @ Matrix.Rotation(math.radians(turn), 4, 'X')


def weapon_pose(clip, f, view):
    if clip == 'GroundReload':
        return reload_pose(f, view)
    if clip in RELOAD_WINDOWS:
        a, b = RELOAD_WINDOWS[clip]
        return reload_pose(a + (b - a) * f / CLIPS[clip], view)
    if clip == 'Inspect':
        return inspect_pose(f, view)
    if clip == 'Discard':
        return discard_pose(f, view)
    p = poses(view)
    end = CLIPS[clip]
    if view == 'FP':
        # one base pose: hip and aim framings are the game's (HipGripLocation / AimMuzzleLocation)
        base = p['base']
        if clip == 'Equip':
            base = blend(p['holster'], p['base'], ease_os(0, end - 8, f, over=0.07, antic=0.0))
        elif clip == 'Holster':
            base = blend(p['base'], p['holster'], ease_os(6, end, f, over=0.0, antic=0.06))
        elif clip == 'Sprint':
            base = p['sprint']
        elif clip in ('AimIn', 'Present'):
            # the rig carries the gun to the shoulder; a small lift with a settle
            base = base @ Matrix.Translation((0, 0, 0.008 * math.sin(math.pi * f / end))) @ Matrix.Rotation(math.radians(-1.5 * math.sin(math.pi * f / end)), 4, 'Y')
        elif clip in ('AimOut', 'ReturnToPort'):
            base = base @ Matrix.Translation((0, 0, -0.006 * math.sin(math.pi * f / end)))
    else:
        base = p['ready']
        if clip == 'AimIdle':
            base = p['aim']
        if clip in ('AimIn', 'Present'):
            base = blend(p['ready'], p['aim'], ease_os(0, end, f, over=0.06, antic=0.03))
        if clip in ('AimOut', 'ReturnToPort'):
            base = blend(p['aim'], p['ready'], ease_os(0, end, f, over=0.06, antic=0.03))
        if clip == 'Equip':
            base = blend(p['holster'], p['ready'], ease_os(0, end - 8, f, over=0.07))
        if clip == 'Holster':
            base = blend(p['ready'], p['holster'], ease_os(6, end, f, antic=0.06))
    if clip in ('Fire', 'Recoil'):
        # sharp: the shot at frame 8, the full kick at 10 (two frames), back by 20 (ten frames) with a settle
        kick = curve(f, [(0, 0), (7, 0), (8, 0.55), (10, 1), (13, 0.42), (16, 0.12), (18, -0.05), (20, 0), (end, 0)]) if clip == 'Fire' else \
            curve(f, [(0, 1), (3, 0.5), (6, 0.15), (9, -0.05), (12, 0), (end, 0)])
        base = base @ Matrix.Translation((-.038 * kick, 0, .004 * kick)) @ Matrix.Rotation(math.radians(-7 * kick), 4, 'Y')
    if clip in ('Idle', 'Hold'):
        # breathing: a slow rise with a slight pitch, a slower sway
        q = math.sin(2 * math.pi * f / end)
        s = math.sin(2 * math.pi * f / end + 1.3)
        base = base @ Matrix.Translation((0.0015 * s, .0012 * q, .004 * q)) @ Matrix.Rotation(math.radians(0.6 * q), 4, 'Y') \
            @ Matrix.Rotation(math.radians(0.4 * s), 4, 'X')
    if clip == 'AimIdle':
        q = math.sin(2 * math.pi * f / end)
        base = base @ Matrix.Translation((0, .0006 * q, .0015 * q)) @ Matrix.Rotation(math.radians(0.25 * q), 4, 'Y')
    if clip in ('Walk', 'Sprint'):
        cycles = 4
        q = 2 * math.pi * f / end * cycles
        amp = 1.0 if clip == 'Walk' else 2.0
        base = base @ Matrix.Translation((.005 * amp * math.sin(q), .006 * amp * math.cos(q), .007 * amp * math.sin(2 * q)))
        base = base @ Matrix.Rotation(math.radians((2.0 if clip == 'Walk' else 4.5) * math.sin(q)), 4, 'X') \
            @ Matrix.Rotation(math.radians((1.0 if clip == 'Walk' else 2.5) * math.sin(2 * q)), 4, 'Y')
        if clip == 'Sprint' and view == 'TP':
            base = base @ Matrix.Rotation(math.radians(12), 4, 'Y')
    return base


# ------------------------------------------------------------------------------------------ flintlock parts

def reload_frame(clip, f):
    """Frame of GroundReload a reload clip shows at f (None for the other clips)."""
    if clip == 'GroundReload':
        return f
    if clip in RELOAD_WINDOWS:
        a, b = RELOAD_WINDOWS[clip]
        return a + (b - a) * f / CLIPS[clip]
    return None


def hammer_angle(clip, f):
    g = reload_frame(clip, f)
    if g is not None:
        # fired -> pulled past half cock and let into its notch -> ... -> full cock at the end
        return curve(g, [(0, HAMMER_FIRED), (2, HAMMER_HALF - 6), (5, HAMMER_HALF), (125, HAMMER_HALF), (128, -4), (130, 0), (132, 0)])
    if clip == 'Fire':
        return curve(f, [(0, 0), (6, 0), (7, HAMMER_FIRED + 4), (9, HAMMER_FIRED), (CLIPS[clip], HAMMER_FIRED)])
    if clip == 'Recoil':
        return HAMMER_FIRED
    if clip == 'Discard':
        return curve(f, [(0, 0), (EMPTY_FRAME, HAMMER_FIRED + 3), (EMPTY_FRAME + 1, HAMMER_FIRED), (CLIPS[clip], HAMMER_FIRED)])
    return 0.0


def frizzen_angle(clip, f):
    g = reload_frame(clip, f)
    if g is not None:
        # open since the shot, shut after priming
        return curve(g, [(0, FRIZZEN_OPEN), (33, FRIZZEN_OPEN), (35, FRIZZEN_OPEN + 4), (38, 0), (132, 0)])
    if clip == 'Fire':
        return curve(f, [(0, 0), (7, 0), (8, FRIZZEN_OPEN + 6), (10, FRIZZEN_OPEN), (CLIPS[clip], FRIZZEN_OPEN)])
    if clip == 'Recoil':
        return FRIZZEN_OPEN
    if clip == 'Discard':
        return curve(f, [(0, 0), (EMPTY_FRAME, 0), (EMPTY_FRAME + 1, FRIZZEN_OPEN + 6), (EMPTY_FRAME + 3, FRIZZEN_OPEN), (CLIPS[clip], FRIZZEN_OPEN)])
    return 0.0


def trigger_angle(clip, f):
    if clip == 'Fire':
        return curve(f, [(0, 0), (4, 0), (6, 16), (9, 16), (13, 0), (CLIPS[clip], 0)])
    if clip == 'Discard':
        return curve(f, [(0, 16), (2, 16), (5, 0), (CLIPS[clip], 0)])
    return 0.0


def rod_state(g):
    """Ramrod at reload frame g: (rotation, head point) in weapon local and where the right hand holds it (rod
    coordinate from the head end, metres), or None when it is stowed in its pipes."""
    if g is None or g < 73 or g >= 122:
        return None

    def stow(s):
        """sliding along its channel under the barrel"""
        return Matrix.Identity(3), ROD_HEAD + ROD_DIR * s

    half_turn = Matrix.Rotation(math.radians(180.0), 3, 'Y')
    # along the bore, trumpet head first: the half turn about Y, then the small tilt from the channel line to the bore
    bore_rot = (half_turn @ ROD_DIR).rotation_difference(Vector((-1, 0, 0))).to_matrix() @ half_turn

    def in_bore(depth):
        """head 'depth' metres inside the muzzle (negative: above it)"""
        tip = Vector((MUZZLE.x - 0.006 - depth, 0.0, BORE_Z))
        return bore_rot, tip - bore_rot @ ROD_DIR * ROD_LEN

    def turned(start_rot, start_head, hold, angle):
        """turned by angle about the weapon's Y around the hand (rod coordinate hold)"""
        pivot = start_head + start_rot @ ROD_DIR * hold
        r = Matrix.Rotation(math.radians(angle), 3, 'Y')
        return r @ start_rot, pivot + r @ (start_head - pivot)

    def towards(a, b, w):
        return (a[0].to_quaternion().slerp(b[0].to_quaternion(), w).to_matrix(), a[1].lerp(b[1], w))

    if g < 86:
        # two sharp pulls: the hand on the head end, regrip lower, pull again
        s = curve(g, [(73, 0.0), (74, 0.0), (78, 0.25), (80, 0.25), (85.5, 0.53), (86, 0.53)])
        hold = curve(g, [(73, 0.50), (78, 0.50), (80, 0.31), (86, 0.31)])
        return (*stow(s), hold)
    out = stow(0.53)
    if g < 93:
        # turned end for end around the hand
        return (*turned(*out, 0.31, curve(g, [(86, 0.0), (92, 180.0), (93, 180.0)])), 0.31)
    if g < 97:
        # head over the muzzle, then in; the hand slides toward the free end
        over = in_bore(-0.06)
        rot, head = towards(turned(*out, 0.31, 180.0), over, smooth(93, 96, g))
        if g > 96:
            rot, head = in_bore(-0.06 + 0.06 * smooth(96, 97, g))
        return rot, head, 0.31 + (0.06 - 0.31) * smooth(93, 97, g)
    if g < 110:
        # rammed home in two sharp strokes, then drawn out
        depth = curve(g, [(97, 0.0), (99, 0.31), (101, 0.19), (103.5, 0.33), (105.5, 0.22), (106.5, 0.22), (110, -0.07)])
        return (*in_bore(depth), 0.06)
    drawn = in_bore(-0.07)
    if g < 112:
        # regrip on the middle of the rod
        return (*drawn, 0.06 + (0.26 - 0.06) * smooth(110, 112, g))
    if g < 116:
        # turned back around the hand: free end down
        return (*turned(*drawn, 0.26, -curve(g, [(112, 0.0), (116, 180.0)])), 0.26)
    # down onto the channel line beyond the muzzle, then pushed home through the pipes, the hand toward the head
    back = turned(*drawn, 0.26, -180.0)
    s0 = (back[1] - ROD_HEAD).dot(ROD_DIR)
    s = curve(g, [(116, s0), (117, s0), (121.5, 0.0), (122, 0.0)])
    rot, head = towards(back, stow(s0), smooth(116, 117, g))
    if g > 117:
        rot, head = stow(s)
    return rot, head, 0.26 + (0.45 - 0.26) * smooth(116, 120, g)


def rod_point(state, g_along):
    rot, head, _ = state
    return head + rot @ ROD_DIR * g_along


def rod_delta(state):
    """Local delta of the ramrod bone (weapon local): rest geometry -> its state."""
    if state is None:
        return Matrix.Identity(4)
    rot, head, _ = state
    return Matrix.Translation(head) @ rot.to_4x4() @ Matrix.Translation(-ROD_HEAD)


# ------------------------------------------------------------------------------------------------- hands

# Right hand keys of the reload: (frame, target, FP elbow (view cm), FP back of the hand (view dir)).
# Targets: ('w', weapon point) on the gun, ('v', view cm, bean point for TP) free, ('rod', None) on the ramrod,
# ('pinch', ...) the same but the cartridge between the fingers is put there instead of the hand centre.
R_GRIP = ('w', GRIP_R)
REACH = {'r': (8.0, 26.0, -48.0), 'l': (14.0, -30.0, -46.0)}
# elbows while loading: the right forearm comes in from the right edge, the left one from the bottom left
LOAD_ELBOW = {'r': (16.0, 54.0, -18.0), 'l': (20.0, -42.0, -38.0)}
GRIP_U, THUMB_U, SIDE_U, FORE_U, BAND_U = (0, .55, .85), (0, .75, .65), (.15, .95, .25), (0, -.45, -.89), (-.2, -.95, .2)
FREE_R = ('v', (20.0, 26.0, -58.0), Vector((-.29, -.10, .36)))
FREE_L = ('v', (20.0, -26.0, -58.0), Vector((.29, -.10, .36)))


def right_keys():
    pouch = ('v', (16.0, 30.0, -44.0), Vector((-.25, -.04, .38)))
    mouth = ('v', (24.0, 1.0, -12.5), Vector((-.03, -.20, .73)))
    tear = ('v', (23.0, 2.5, -15.5), Vector((-.03, -.19, .70)))
    above_pan = ('pinch', PAN + Vector((0, -0.005, 0.030)))
    above_muzzle = ('pinch', MUZZLE + Vector((0.035, 0, 0)))
    in_muzzle = ('pinch', MUZZLE + Vector((0.004, 0, 0)))
    spur = ('w', HAMMER_SPUR + Vector((-0.05, 0.0, -0.02)))
    e_low, e_mouth, e_pan, e_load = (5.0, 30.0, -55.0), (10.0, 18.0, -55.0), (14.0, 30.0, -40.0), LOAD_ELBOW['r']
    return [
        (0, R_GRIP, REACH['r'], GRIP_U),
        (2, spur, REACH['r'], THUMB_U),                                    # thumb pulls the hammer to half cock
        (6, spur, REACH['r'], THUMB_U),
        (9, R_GRIP, REACH['r'], GRIP_U),
        (10, R_GRIP, REACH['r'], GRIP_U),
        (14, pouch, e_low, (.2, .9, -.2)),                                 # a cartridge from the pouch
        (16, pouch, e_low, (.2, .9, -.2)),
        (19, mouth, e_mouth, (-.3, .6, .7)),                               # bitten open
        (20.5, tear, e_mouth, (-.3, .6, .7)),
        (22, mouth, e_mouth, (-.3, .6, .7)),
        (25, above_pan, e_pan, (0, .35, .94)),                             # powder into the pan
        (30, above_pan, e_pan, (0, .35, .94)),
        (33, ('w', FRIZZEN_TOE + Vector((-0.03, -0.02, 0.015))), e_pan, (0, .6, .8)),     # pan shut
        (37, ('w', FRIZZEN_TOE + Vector((-0.045, -0.02, 0.0))), e_pan, (0, .6, .8)),
        (46, above_muzzle, e_load, (0, .6, .8)),                           # cast about, to the muzzle
        (50, above_muzzle, e_load, (0, .6, .8)),
        (58, above_muzzle, e_load, (0, .6, .8)),                           # powder down the barrel
        (63, in_muzzle, e_load, (0, .6, .8)),                              # ball and paper pushed in
        (67, ('w', MUZZLE + Vector((0.012, -0.004, 0.0))), e_load, (0, .6, .8)),
        (70, ('w', MUZZLE + Vector((0.020, -0.010, 0.0))), e_load, (0, .6, .8)),
        (73, ('rod', None), e_load, SIDE_U),                               # ramrod: two pulls, turn, ram
        (121, ('rod', None), e_load, SIDE_U),
        (124, spur, REACH['r'], THUMB_U),                                  # full cock
        (129, spur, REACH['r'], THUMB_U),
        (131, R_GRIP, REACH['r'], GRIP_U),
        (132, R_GRIP, REACH['r'], GRIP_U),
    ]


def left_keys(view='FP'):
    # third person: the short left arm holds the loading musket lower, on the fore-end
    band = BAND_GRIP if view == 'FP' else GRIP_L + Vector((0.03, 0.0, 0.006))
    return [
        (0, ('w', GRIP_L), REACH['l'], FORE_U),
        (40, ('w', GRIP_L), REACH['l'], FORE_U),
        (46, ('w', band), LOAD_ELBOW['l'], BAND_U),
        (122, ('w', band), LOAD_ELBOW['l'], BAND_U),
        (129, ('w', GRIP_L), REACH['l'], FORE_U),
        (132, ('w', GRIP_L), REACH['l'], FORE_U),
    ]


# the cartridge between the thumb and the mitten: offset in the hand frame (x fingers, y thumb side, z back); in
# third person (no authored frame for the body mitten) it sits on the front top of the mitten
PINCH = Vector((0.050, 0.020, -0.004))
TP_PINCH = Vector((0.0, -0.022, 0.026))


def hand_frame(view, side, point, elbow_cm, back, bean_hand=None):
    """Rotation of the hand frame (x: forearm toward the fingers, z: back of the hand) at point (bean space)."""
    if view == 'FP':
        d = (point - vp(*elbow_cm)).normalized()
        return axes(d, vv(*back))
    return bean_hand


def resolve_target(spec, view, M, g, rod):
    kind = spec[0]
    if kind == 'w' or kind == 'pinch':
        return M @ Vector(spec[1]), kind == 'pinch'
    if kind == 'v':
        return (vp(*spec[1]) if view == 'FP' else Vector(spec[2])), False
    if kind == 'rod':
        if rod is None:
            # stowed: on the head end before the draw, where the push home ends after it
            return M @ (ROD_HEAD + ROD_DIR * (0.50 if g is None or g < 93 else 0.45)), False
        return M @ rod_point(rod, rod[2]), False
    raise ValueError(spec)


def keyed_hand(keys, f, view, M, side, g, rod):
    """Hand centre (bean space) and hand rotation (FP; None in TP) at frame f along the keys."""
    i, w = segment(f, keys)
    k0, k1 = keys[i], keys[min(i + 1, len(keys) - 1)]
    out = []
    for k in (k0, k1):
        p, pinch = resolve_target(k[1], view, M, g, rod)
        rot = hand_frame(view, side, p, k[2], k[3]) if view == 'FP' else None
        if pinch and rot is not None:
            p = p - rot @ PINCH
        elif pinch:
            p = p - TP_PINCH
        out.append((p, rot))
    p = out[0][0].lerp(out[1][0], w)
    rot = None
    if view == 'FP':
        rot = out[0][1].to_quaternion().slerp(out[1][1].to_quaternion(), w).to_matrix()
    return p, rot


def clip_hands(clip, f, view, M):
    """(right, left) = ((point, rotation), (point, rotation)) of the hands on the musket for clip / frame."""
    g = reload_frame(clip, f)
    rod = rod_state(g)
    if g is not None:
        right = keyed_hand(right_keys(), g, view, M, 'r', g, rod)
        left = keyed_hand(left_keys(view), g, view, M, 'l', g, rod)
        return right, left, rod
    rk = [(0, R_GRIP, REACH['r'], GRIP_U)]
    lk = [(0, ('w', GRIP_L), REACH['l'], FORE_U)]
    if clip == 'Fire':
        # the index squeezes: the hand rolls a little on the wrist
        rk = [(0, R_GRIP, REACH['r'], GRIP_U), (5, R_GRIP, REACH['r'], (0, .45, .9)), (12, R_GRIP, REACH['r'], GRIP_U)]
    if clip == 'Equip':
        rk = [(0, FREE_R, REACH['r'], GRIP_U), (12, FREE_R, REACH['r'], GRIP_U), (30, R_GRIP, REACH['r'], GRIP_U)]
        lk = [(0, FREE_L, REACH['l'], FORE_U), (14, FREE_L, REACH['l'], FORE_U), (34, ('w', GRIP_L), REACH['l'], FORE_U)]
    if clip == 'Holster':
        rk = [(0, R_GRIP, REACH['r'], GRIP_U), (30, R_GRIP, REACH['r'], GRIP_U), (42, FREE_R, REACH['r'], GRIP_U)]
        lk = [(0, ('w', GRIP_L), REACH['l'], FORE_U), (10, ('w', GRIP_L), REACH['l'], FORE_U), (32, FREE_L, REACH['l'], FORE_U)]
    if clip == 'Inspect':
        # the left hand slides to the fore-end while the barrel is shown
        lk = [(0, ('w', GRIP_L), REACH['l'], FORE_U), (46, ('w', GRIP_L), REACH['l'], FORE_U), (60, ('w', BAND_GRIP), LOAD_ELBOW['l'], BAND_U),
              (80, ('w', BAND_GRIP), LOAD_ELBOW['l'], BAND_U), (96, ('w', GRIP_L), REACH['l'], FORE_U)]
    if clip == 'Discard':
        after_r = ('v', (22.0, 26.0, -44.0), Vector((-.28, -.12, .40)))
        gone_r = ('v', (18.0, 24.0, -56.0), Vector((-.30, -.08, .34)))
        after_l = ('v', (24.0, -24.0, -40.0), Vector((.27, -.14, .42)))
        gone_l = ('v', (18.0, -24.0, -56.0), Vector((.30, -.08, .34)))
        rk = [(0, R_GRIP, REACH['r'], GRIP_U), (RELEASE_FRAME, R_GRIP, REACH['r'], GRIP_U), (RELEASE_FRAME + 5, after_r, REACH['r'], (0, .3, .95)),
              (CLIPS['Discard'], gone_r, REACH['r'], GRIP_U)]
        # the left hand lets go before the swing to the right (the bean's left arm cannot follow it)
        lk = [(0, ('w', GRIP_L), REACH['l'], FORE_U), (13, ('w', GRIP_L), REACH['l'], FORE_U),
              (19, after_l, REACH['l'], (0, -.3, -.95)), (CLIPS['Discard'], gone_l, REACH['l'], FORE_U)]
    return keyed_hand(rk, f, view, M, 'r', None, None), keyed_hand(lk, f, view, M, 'l', None, None), None


# ------------------------------------------------------------------------------------------- cartridge

def cartridge_world(clip, f, view, M, right_point, right_rot, rest_matrix):
    """Pose matrix of the cartridge bone (armature space = bean space), from the right hand when held."""
    from musket_model import CARTRIDGE_REST
    g = reload_frame(clip, f)
    if g is None or g < 14 or g >= 68:
        return M @ rest_matrix
    # axis (+X of the cartridge = ball end) per moment: across the hand, torn end up to the mouth, torn end down over
    # the pan and over the muzzle, then ball first into the bore
    across = right_rot.col[1].xyz if right_rot is not None else Vector((1, 0, 0))
    barrel = (M.to_3x3() @ Vector((1, 0, 0))).normalized()          # the barrel points up while loading
    keys = [(14, across), (18, across), (20, -UP), (22, -UP), (25, UP), (30, UP),
            (34, across), (46, across), (50, barrel), (58, barrel), (60.5, across), (63, -barrel), (68, -barrel)]
    i, w = segment(g, keys)
    a, b = keys[i][1], keys[min(i + 1, len(keys) - 1)][1]
    axis = a.lerp(b, w)
    if axis.length < 1e-4:
        axis = b
    axis.normalize()
    pinch = right_point + (right_rot @ PINCH if right_rot is not None else TP_PINCH)
    rot = axes(axis, UP if abs(axis.dot(UP)) < 0.95 else FWD)
    # pushing ball and paper: the cartridge slides from the fingers down into the bore
    if g >= 63:
        into = smooth(63, 66.5, g)
        bore = M @ Vector((MUZZLE.x - 0.034, 0.0, BORE_Z))
        pinch = pinch.lerp(bore, into)
        rot = Quaternion(rot.to_quaternion()).slerp(
            axes((M.to_3x3() @ Vector((-1, 0, 0))).normalized(), M.to_3x3() @ Vector((0, 0, 1))).to_quaternion(), into).to_matrix()
    delta = rot.to_4x4()
    delta.translation = pinch
    return delta @ Matrix.Translation(-CARTRIDGE_REST) @ rest_matrix


# ------------------------------------------------------------------------------------------------- solvers

def local_rot(arm, bone, axis, ang):
    p = arm.pose.bones[bone]
    p.rotation_mode = 'QUATERNION'
    axis = p.bone.matrix_local.to_3x3().inverted() @ Vector(axis)
    p.rotation_quaternion = Quaternion(axis, math.radians(ang))


def fp_arm_transform(arm, side):
    """T of build.clone_bean: FPArm local -> bean rest space (mirrored for the left arm)."""
    grip = arm.data.bones['grip_' + side].tail_local
    d = (grip - arm.data.bones['lowerarm_' + side].head_local).normalized()
    R = frame(d, (0, 0, 0)).to_3x3()
    if side == 'l':
        R = R @ Matrix.Diagonal((1, -1, 1))
    T = R.to_4x4()
    T.translation = grip - R @ FP_HAND_CENTRE
    return T


FP_HAND_CENTRE = Vector((0.36, 0.0, -0.015))   # centre of the FPArm mitten (scripts/character/bean_soldier.py)


def solve_fp_arm(arm, matrices, side, point, rot):
    """First person: the FPArm (rigid on hand_<side>) gets its mitten centre on point, its forearm along rot's x and
    the back of the hand along rot's z; forearm and hand rigid together, the upper arm reaches back to an invisible
    shoulder without stretching."""
    T = fp_arm_transform(arm, side)
    F = rot.copy()
    if side == 'l':
        F = F @ Matrix.Diagonal((1, -1, 1))
    F4 = F.to_4x4()
    F4.translation = point - F @ FP_HAND_CENTRE
    delta = F4 @ T.inverted()                      # rest space -> posed, proper rigid (mirrors cancel)
    for name in ('lowerarm_' + side, 'hand_' + side, 'grip_' + side):
        matrices[name] = delta @ arm.data.bones[name].matrix_local
    up = arm.data.bones['upperarm_' + side]
    elbow = delta @ arm.data.bones['lowerarm_' + side].head_local
    shoulder = vp(*FP_SHOULDER[side])
    direction = (elbow - shoulder).normalized()
    rest = (up.tail_local - up.head_local)
    q = rest.normalized().rotation_difference(direction)
    head = elbow - direction * rest.length
    matrices[up.name] = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-up.head_local) @ up.matrix_local
    got = delta @ T @ FP_HAND_CENTRE
    return (got - point).length, 1.0


def solve_tp_arm(arm, matrices, side, target, max_stretch=1.32):
    """Third person: exact two-segment solve from the real shoulder, hand rigid on the forearm; arms of the bean are
    short, a soft stretch (uniform, at most max_stretch) lets the mittens reach the musket."""
    up = arm.data.bones['upperarm_' + side]
    low = arm.data.bones['lowerarm_' + side]
    grip = arm.data.bones['grip_' + side]
    core = matrices[up.parent.name] @ up.parent.matrix_local.inverted()
    sh = core @ up.head_local
    v1 = up.tail_local - up.head_local
    v2 = grip.tail_local - low.head_local
    delta = Vector(target) - sh
    dist = delta.length
    scale = min(max_stretch, max(1., dist / (v1.length + v2.length) * 1.004))
    l1, l2 = v1.length * scale, v2.length * scale
    axis = delta.normalized()
    pole = Vector((.45 if side == 'l' else -.45, .15, -1.0))
    perpendicular = (pole - axis * pole.dot(axis)).normalized()
    reach = min(dist, l1 + l2 - 1e-5)
    along = max(-l1, min(l1, (l1 * l1 - l2 * l2 + reach * reach) / (2 * max(reach, 1e-8))))
    height = math.sqrt(max(0, l1 * l1 - along * along))
    elbow = sh + axis * along + perpendicular * height
    hand_at = sh + axis * reach
    q1 = v1.normalized().rotation_difference((elbow - sh).normalized())
    q2 = v2.normalized().rotation_difference((hand_at - elbow).normalized())
    S = Matrix.Diagonal((scale, scale, scale, 1))
    D1 = Matrix.Translation(sh) @ q1.to_matrix().to_4x4() @ S @ Matrix.Translation(-up.head_local)
    D2 = Matrix.Translation(elbow) @ q2.to_matrix().to_4x4() @ S @ Matrix.Translation(-low.head_local)
    matrices[up.name] = D1 @ up.matrix_local
    for name in ('lowerarm_' + side, 'hand_' + side, 'grip_' + side):
        matrices[name] = D2 @ arm.data.bones[name].matrix_local
    got = matrices[grip.name] @ grip.matrix_local.inverted() @ grip.tail_local
    return (got - Vector(target)).length, scale


def shoulder_point(arm, matrices, side):
    up = arm.data.bones['upperarm_' + side]
    return matrices[up.parent.name] @ up.parent.matrix_local.inverted() @ up.head_local


def tp_reach(arm, side, stretch=1.12):
    """Comfortable reach of a third person arm, shoulder to mitten centre (m)."""
    up = arm.data.bones['upperarm_' + side]
    low = arm.data.bones['lowerarm_' + side]
    grip = arm.data.bones['grip_' + side]
    return ((up.tail_local - up.head_local).length + (grip.tail_local - low.head_local).length) * stretch


def solve_leg(arm, matrices, side, target):
    up = arm.data.bones['thigh_' + side]
    low = arm.data.bones['calf_' + side]
    foot = arm.data.bones['foot_' + side]
    core = matrices[up.parent.name] @ up.parent.matrix_local.inverted()
    sh = core @ up.head_local
    v1 = up.tail_local - up.head_local
    v2 = low.tail_local - low.head_local
    delta = target - sh
    dist = delta.length
    scale = max(1., dist / (v1.length + v2.length) * 1.002)
    l1, l2 = v1.length * scale, v2.length * scale
    axis = delta.normalized()
    pole = Vector((0, -1, 0))
    perpendicular = (pole - axis * pole.dot(axis)).normalized()
    along = max(-l1, min(l1, (l1 * l1 - l2 * l2 + dist * dist) / (2 * max(dist, 1e-8))))
    knee = sh + axis * along + perpendicular * math.sqrt(max(0, l1 * l1 - along * along))
    q1 = v1.normalized().rotation_difference((knee - sh).normalized())
    q2 = v2.normalized().rotation_difference((target - knee).normalized())
    S = Matrix.Diagonal((scale, scale, scale, 1))
    matrices[up.name] = Matrix.Translation(sh) @ q1.to_matrix().to_4x4() @ S @ Matrix.Translation(-up.head_local) @ up.matrix_local
    matrices[low.name] = Matrix.Translation(knee) @ q2.to_matrix().to_4x4() @ S @ Matrix.Translation(-low.head_local) @ low.matrix_local
    matrices[foot.name] = Matrix.Translation(target - foot.head_local) @ foot.matrix_local


# ---------------------------------------------------------------------------------------------- baking

def linear(action):
    for layer in action.layers:
        for strip in layer.strips:
            for bag in getattr(strip, 'channelbags', []):
                for fc in bag.fcurves:
                    for k in fc.keyframe_points:
                        k.interpolation = 'LINEAR'


def set_matrices(arm, matrices):
    for pb in arm.pose.bones:
        kw = {}
        if pb.parent:
            kw = dict(parent_matrix=matrices[pb.parent.name], parent_matrix_local=pb.parent.bone.matrix_local)
        pb.matrix_basis = pb.bone.convert_local_to_pose(matrices[pb.name], pb.bone.matrix_local, invert=True, **kw)


def bake_matrices(arm, samples, name):
    for pb in arm.pose.bones:
        for c in list(pb.constraints):
            pb.constraints.remove(c)
        pb.rotation_mode = 'QUATERNION'
    arm.animation_data_clear()
    action = bpy.data.actions.new(name)
    action.use_fake_user = True
    arm.animation_data_create()
    arm.animation_data.action = action
    for f, mats in enumerate(samples):
        set_matrices(arm, mats)
        for pb in arm.pose.bones:
            for prop in ('location', 'rotation_quaternion', 'scale'):
                pb.keyframe_insert(prop, frame=f, group=pb.name)
    action.use_frame_range = True
    action.frame_start = 0
    action.frame_end = len(samples) - 1
    linear(action)
    return action


# ------------------------------------------------------------------------------------------------ events

GROUND_RELOAD_EVENTS = [
    (3, 'HalfCockStart', None), (27, 'PanPrime', 'socket_pan'), (37, 'FrizzenClose', 'socket_pan'),
    (48, 'StockLowered', None), (55, 'PowderCharge', 'socket_muzzle'), (67, 'BallAndPatch', 'socket_muzzle'),
    (75, 'RamrodDraw', 'socket_ramrod_tip'), (89, 'RamrodTurn', None), (99, 'RamrodStroke', 'socket_muzzle'),
    (104, 'RamrodStroke', 'socket_muzzle'), (118, 'RamrodReturn', None), (130, 'FullCockEnd', None)]


def events(clip):
    def ev(frame_, name, socket=None):
        e = {'frame': frame_, 'name': name}
        if socket:
            e['socket'] = socket
        return e
    if clip == 'Fire':
        return [ev(5, 'Trigger', 'socket_trigger'), ev(8, 'PanFlash', 'socket_pan'), ev(8, 'MuzzleFlash', 'socket_muzzle'),
                ev(8, 'Shot', 'socket_muzzle'), ev(8, 'Recoil')]
    if clip == 'GroundReload':
        return [ev(*e) for e in GROUND_RELOAD_EVENTS]
    if clip in RELOAD_WINDOWS:
        a, b = RELOAD_WINDOWS[clip]
        n = CLIPS[clip]
        return [ev(int(round((fr - a) * n / (b - a))), name, socket) for fr, name, socket in GROUND_RELOAD_EVENTS if a <= fr < b]
    if clip == 'Equip':
        return [ev(8, 'DrawWeapon'), ev(34, 'Ready')]
    if clip == 'Holster':
        return [ev(36, 'StowWeapon')]
    if clip == 'Discard':
        return [ev(EMPTY_FRAME, 'Empty', 'socket_pan'), ev(RELEASE_FRAME, 'Release', 'socket_grip_r')]
    return []


def release_intervals(clip):
    """Frames where a hand is off its grip (grip contract of the manifest)."""
    if clip == 'Equip':
        return [[0, 30]]
    if clip == 'Holster':
        return [[30, 42]]
    if clip == 'GroundReload':
        return [[2, 9], [9, CLIPS['GroundReload']]]
    if clip in RELOAD_WINDOWS:
        return [[0, CLIPS[clip]]]
    if clip == 'Discard':
        return [[RELEASE_FRAME, CLIPS['Discard']]]
    return []


# ----------------------------------------------------------------------------------------------- animate

def head_look(clip, f, g):
    """TP: how much the head looks down at the gun (0..1)."""
    if g is not None:
        return curve(g, [(0, 0), (8, 1), (38, 1), (46, .5), (122, .5), (129, 0)])
    if clip == 'Inspect':
        return curve(f, [(0, 0), (12, 1), (82, 1), (96, 0)])
    if clip == 'Discard':
        return curve(f, [(0, 0), (6, 1), (18, 1), (RELEASE_FRAME + 2, 0.3), (32, 0)])
    return 0.0


def animate(scene, bean, weapon, clip, view):
    arm = bean['armature']
    gun = weapon['armature']
    end = CLIPS[clip]
    scene.frame_start = 0
    scene.frame_end = end
    scene.render.fps = FPS
    bpy.context.window.scene = scene
    bean_samples, gun_samples, grips = [], [], []
    errors = {'l': 0., 'r': 0.}
    stretches = []
    bone_names = [pb.name for pb in gun.pose.bones]
    for f in range(end + 1):
        scene.frame_set(f)
        for pb in arm.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        g = reload_frame(clip, f)
        # TP body: the head looks at the gun (down while loading), walk cycle on the legs
        if view == 'TP':
            aim_weight = 1. if clip == 'AimIdle' else smooth(0, end, f) if clip in ('AimIn', 'Present') else \
                1. - smooth(0, end, f) if clip in ('AimOut', 'ReturnToPort') else 0.
            look_down = head_look(clip, f, g)
            local_rot(arm, 'head', (0, 0, 1), 30 + 25 * aim_weight - 22 * look_down)
            if look_down:
                pb = arm.pose.bones['head']
                # about the bean's left axis, forward: the face (at -Y) looks down at the lock and the muzzle
                pb.rotation_quaternion = pb.rotation_quaternion @ Quaternion(
                    pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0)), math.radians(12 * look_down))
        phase = 0.0
        if clip in ('Walk', 'Sprint'):
            cyc = 4
            phase = 2 * math.pi * f / end * cyc
            amp = 18 if clip == 'Walk' else 28
            for side, off in (('l', 0), ('r', math.pi)):
                swing = math.sin(phase + off)
                local_rot(arm, 'thigh_' + side, (1, 0, 0), amp * swing)
                local_rot(arm, 'calf_' + side, (1, 0, 0), -max(0, -swing) * (25 if clip == 'Walk' else 43))
                local_rot(arm, 'foot_' + side, (1, 0, 0), -amp * swing * .5)
            arm.pose.bones['root'].location = (0, 0, .006 + .007 * (1 - math.cos(2 * phase)))
            arm.pose.bones['pelvis'].location = (0, -.027, 0)
        else:
            arm.pose.bones['root'].location = (0, 0, .003)
        M = weapon_pose(clip, f, view)
        (rp, rrot), (lp, lrot), rod = clip_hands(clip, f, view, M)
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev = arm.evaluated_get(dg)
        bm = {p.name: p.matrix.copy() for p in ev.pose.bones}
        if view == 'TP' and rod is not None:
            # the bean's arms are short: while the ramrod works high above the muzzle, the left hand brings the musket
            # down to the right hand instead of the right arm stretching out of shape
            # (straight down, so the left hand on the barrel keeps within its own reach)
            d = rp - shoulder_point(arm, bm, 'r')
            reach = tp_reach(arm, 'r')
            if d.length > reach:
                flat = d.x * d.x + d.y * d.y
                drop = d.z - math.sqrt(reach * reach - flat) if flat < reach * reach else d.z
                # at most 18 cm: the left hand, lower on the fore-end in third person, keeps the barrel
                M = Matrix.Translation((0, 0, -min(0.18, max(0.0, drop)))) @ M
                (rp, rrot), (lp, lrot), rod = clip_hands(clip, f, view, M)
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
                error, scale = solve_tp_arm(arm, bm, side, point)
            errors[side] = max(errors[side], error)
            stretches.append(scale)
        if view == 'TP' and clip in ('Walk', 'Sprint'):
            # skin-weighted boots can dip below their ankle targets: measure the soles, lift both actors together
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
        # the musket: rigid parts on the root pose, flintlock and ramrod animated, the cartridge from the right hand
        matrices = {n: M @ gun.data.bones[n].matrix_local for n in bone_names}
        for part, angle in (('hammer', hammer_angle(clip, f)), ('frizzen', frizzen_angle(clip, f))):
            if part in matrices:
                B = gun.data.bones[part].matrix_local
                head = gun.data.bones[part].head_local
                P = Matrix.Translation(head) @ Matrix.Rotation(math.radians(angle), 4, 'Y') @ Matrix.Translation(-head)
                matrices[part] = M @ P @ B
        if 'trigger' in matrices:
            B = gun.data.bones['trigger'].matrix_local
            matrices['trigger'] = M @ B @ Matrix.Rotation(math.radians(trigger_angle(clip, f)), 4, 'X')
        if 'ramrod' in matrices:
            matrices['ramrod'] = M @ rod_delta(rod) @ gun.data.bones['ramrod'].matrix_local
        if 'cartridge' in matrices:
            matrices['cartridge'] = cartridge_world(clip, f, view, M, rp, rrot, gun.data.bones['cartridge'].matrix_local)
        # sockets follow their parent's delta
        for n in bone_names:
            b = gun.data.bones[n]
            if b.parent and n.startswith('socket_'):
                matrices[n] = matrices[b.parent.name] @ b.parent.matrix_local.inverted() @ b.matrix_local
        gun_samples.append(matrices)
    ba = bake_matrices(arm, bean_samples, 'A_Bean_Musket_' + view + '_' + clip)
    ga = bake_matrices(gun, gun_samples, 'A_Musket_' + view + '_' + clip)
    scene.frame_set(0)
    bpy.context.view_layer.update()
    return dict(bean_action=ba.name, weapon_action=ga.name, events=events(clip),
                hand_error_max_m=errors, upperarm_scale_max=max(stretches), frame_start=0, frame_end=end, fps=FPS,
                authored_method='T-246 local pack v2: FP arms placed directly through the game camera (no stretch), TP '
                                'two-segment arms; one FP base pose framed by the game (hip and aim); CoD easing '
                                '(anticipation, overshoot, settle), sharp recoil; synchronised two-hand 5.5 s reload with '
                                'cartridge and sliding ramrod; Inspect and Discard.',
                grip_targets=grips)


# ------------------------------------------------------------------------------------------- framing data

def aim_eye_in_weapon():
    """The eye in weapon local coordinates for the aim framing: on the sight line, AIM_MUZZLE_CM behind the muzzle."""
    return Vector((MUZZLE.x - AIM_MUZZLE_CM / 100.0, 0.0, SIGHT_Z))


def gesture_framing(name, M):
    """Where a weapon pose M (bean space) sits in the view and where the eye is in the weapon (cm / m)."""
    pitch, yaw, roll = rotator_in_view(M)
    eye = M.inverted() @ FP_CAMERA
    return dict(gesture=name, grip_in_view_cm=[round(v, 2) for v in to_view(M @ GRIP_R)],
                muzzle_in_view_cm=[round(v, 2) for v in to_view(M @ MUZZLE)],
                rotation_pyr_deg=[round(v, 3) for v in (pitch, yaw, roll)],
                eye_in_weapon_m=[round(v, 4) for v in eye])


def fp_framing():
    """What the game needs to frame the FP clips like this pack's camera (DA_PirateMusketRules)."""
    base = fp_pose('base')
    pitch, yaw, roll = rotator_in_view(base)
    grip = to_view(base @ GRIP_R)
    sight_h = SIGHT_Z - BORE_Z
    gestures = {
        'Idle': gesture_framing('Idle (hip, frame 0 = base pose, every non reload clip)', base),
        'Sprint': gesture_framing('Sprint', fp_pose('sprint')),
        'GroundReload@20': gesture_framing('GroundReload frame 20 (prime)', reload_pose(20, 'FP')),
        'GroundReload@60': gesture_framing('GroundReload frame 60 (load)', reload_pose(60, 'FP')),
        'GroundReload@100': gesture_framing('GroundReload frame 100 (ramrod stroke)', reload_pose(100, 'FP')),
        'Inspect@30': gesture_framing('Inspect frame 30 (lock)', inspect_pose(30, 'FP')),
        'Inspect@70': gesture_framing('Inspect frame 70 (barrel)', inspect_pose(70, 'FP')),
        'Discard@14': gesture_framing('Discard frame 14 (shake)', discard_pose(14, 'FP')),
        'Discard@22': gesture_framing('Discard frame 22 (Release)', discard_pose(RELEASE_FRAME, 'FP')),
    }
    gestures['AimIdle'] = dict(gesture='AimIdle / AimIn / AimOut / Fire aimed (game aim framing, not an authored pose)',
                               grip_in_view_cm=[round(AIM_MUZZLE_CM - (MUZZLE.x - GRIP_R.x) * 100, 2), 0.0, round(-(sight_h + (BORE_Z - GRIP_R.z)) * 100, 2)],
                               muzzle_in_view_cm=[AIM_MUZZLE_CM, 0.0, round(-sight_h * 100, 2)], rotation_pyr_deg=[0.0, 0.0, 0.0],
                               eye_in_weapon_m=[round(v, 4) for v in aim_eye_in_weapon()])
    return dict(camera_location_m=[*FP_CAMERA], horizontal_fov_deg=90.0,
                hip_grip_location_cm=[round(v, 2) for v in grip], hip_rotation_pyr_deg=[round(v, 3) for v in (pitch, yaw, roll)],
                aim_muzzle_location_cm=[AIM_MUZZLE_CM, 0.0, round(-sight_h * 100, 2)], aim_rotation_pyr_deg=[0.0, 0.0, 0.0],
                aim_field_of_view_deg=AIM_FOV_DEG,
                sight_line=dict(rear_notch_m=[SIGHT_REAR_X, 0.0, SIGHT_Z], front_sight_m=[SIGHT_FRONT_X, 0.0, SIGHT_Z],
                                height_above_bore_m=round(sight_h, 4), eye_to_rear_notch_cm=round(AIM_MUZZLE_CM - (MUZZLE.x - SIGHT_REAR_X) * 100, 2),
                                eye_to_front_sight_cm=round(AIM_MUZZLE_CM - (MUZZLE.x - SIGHT_FRONT_X) * 100, 2)),
                gestures=gestures,
                note='HipGripLocation / HipRotation reproduce this camera exactly (Idle frame 0 = FP base pose). '
                     'AimMuzzleLocation / AimRotation (aim_*) put the sight line (rear notch -> front sight, parallel to the '
                     'bore, height_above_bore_m above it) exactly on the camera axis, weapon centred, with the muzzle socket '
                     'AIM_MUZZLE_CM ahead: AimIdle keeps the base pose (the game aligns Idle and AimIdle frame 0), so the '
                     'single Fire clip plays aimed without a jump. setup_pirate_weapons.py writes the hip values only: '
                     'copy aim_* into DA_PirateMusketRules by hand (AimMuzzleLocation, AimRotation).')
