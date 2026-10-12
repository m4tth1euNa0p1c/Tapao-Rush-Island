"""Bean grenado handling, first (FP) and third (TP) person, 30 fps, baked on the bean skeleton and the grenade
skeleton (T-253). Metres, Blender axes: the bean looks along -Y, its right side is -X, +Z is up. Weapon local: +X the
fingers of the right hand, +Z up (the fuse), the right mitten centre at the origin.

The grenade is rigid in the right mitten (ball on the palm side): the weapon pose drives the right hand in both views
(the opposite of the musket, whose hands reach for the gun). The slow match is a bone of the weapon ("match") carried
by the left hand, as the musket's cartridge bone follows the right hand; the grenade bag ("bag") shows only in Discard.
Body and fuse are hidden (bone scale 0.0001, like the pistol's charge_visual) from the throw's Fire event, in Reload
until HammerCocked, and all along DryFire and Discard: the game hides them the same way while no grenade is loaded.

First person: authored through the camera FP_CAMERA (the game's 90 deg first person camera), weapon poses given in
that view (cm, UE axes, FRotator), FP arms placed directly (no stretch), as the musket pack (T-212).
Call of Duty feel (owner 2026-10-09): anticipation (wind-up), sharp release, follow-through, grenade kept visible.
"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix, Quaternion, Vector

from grenade_model import BAG_REST, BALL, FUSE_TIP, MATCH_LEN, MATCH_REST

FPS = 30
CLIPS = {'Equip': 45, 'Idle': 90, 'Walk': 120, 'Sprint': 90, 'AimIn': 24, 'AimIdle': 90, 'AimOut': 24, 'Fire': 60,
         'AimFire': 48, 'Reload': 75, 'DryFire': 30, 'Inspect': 150, 'Holster': 45, 'Discard': 36}
LOOPS = {'Idle', 'Walk', 'Sprint', 'AimIdle'}
GRIP_R = Vector((0.0, 0.0, 0.0))
# weapon frame -> right hand frame: fingers along the weapon's +X, back of the hand down (the ball on the palm, +Z)
R_WH = Matrix.Diagonal((1.0, -1.0, -1.0)).to_3x3()
# event frames (30 fps)
FIRE_LIGHT, FIRE_RELEASE = 14, 34
AIMFIRE_LIGHT, AIMFIRE_RELEASE = 10, 25
RELOAD_LOADED = 30
DISCARD_SHOW, DISCARD_RELEASE = 8, 30


# ----------------------------------------------------------------------------------------------------- easing

def smooth(a, b, f):
    t = max(0., min(1., (f - a) / max(1e-6, b - a)))
    return t * t * (3 - 2 * t)


def curve(f, keys):
    if f <= keys[0][0]:
        return keys[0][1]
    for (a, x), (b, y) in zip(keys, keys[1:]):
        if f <= b:
            return x + (y - x) * smooth(a, b, f)
    return keys[-1][1]


def segment(f, keys):
    """(index of the key at or before f, eased weight toward the next key)."""
    if f <= keys[0][0]:
        return 0, 0.0
    for i, (k0, k1) in enumerate(zip(keys, keys[1:])):
        if f <= k1[0]:
            return i, smooth(k0[0], k1[0], f)
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


def z_frame(z, ref):
    """Rotation whose third column is z (unit), x from ref made orthogonal."""
    z = Vector(z).normalized()
    x = Vector(ref) - z * z.dot(Vector(ref))
    if x.length < 1e-6:
        x = Vector((1, 0, 0)) - z * z.x
    x.normalize()
    return Matrix((x, z.cross(x), z)).transposed()


def blend(a, b, w):
    out = a.to_quaternion().slerp(b.to_quaternion(), w).to_matrix().to_4x4()
    out.translation = a.translation.lerp(b.translation, w)
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


FP_SHOULDER = {'r': (-4.0, 20.0, -36.0), 'l': (-4.0, -20.0, -36.0)}   # invisible shoulders of the FP arms (view cm)

# FP weapon poses: (mitten centre in the view (cm), FRotator of the weapon in the view). The ball is 5.5 cm along the
# weapon's +Z from the mitten centre, the fuse tip 10 cm further up and 1.3 cm forward.
FP_POSES = {
    'base': ((27.0, 15.0, -11.0), (20.0, -25.0, -10.0)),            # held low on the right, fuse up, ball in view
    'aim': ((26.0, 16.0, -11.0), (35.0, -40.0, -25.0)),             # arm cocked back: ball tilted to the eye, low right
    'light': ((36.0, 8.0, -9.0), (25.0, -30.0, -15.0)),             # brought to the centre to meet the match
    'light_aim': ((34.0, 8.0, -6.0), (30.0, -35.0, -20.0)),         # same, from the cocked pose (long throw)
    'wind': ((22.0, 17.0, -6.0), (45.0, -20.0, -20.0)),             # short throw: pulled back right, ball at the edge
    'release': ((40.0, 8.0, 0.0), (20.0, -35.0, 10.0)),             # arm out, fingers up-forward, ball leaves to the centre
    'follow': ((38.0, -4.0, -8.0), (-30.0, -45.0, 20.0)),           # follow-through across to the left
    'wind_long': ((19.0, 18.0, -2.0), (55.0, -30.0, -30.0)),        # long throw: wound up behind, at the right edge
    'release_long': ((42.0, 4.0, 10.0), (35.0, -30.0, 5.0)),        # overhand release above the centre
    'follow_long': ((40.0, -6.0, -4.0), (-40.0, -40.0, 15.0)),
    'holster': ((16.0, 22.0, -40.0), (-40.0, -10.0, 0.0)),          # put away below the screen
    'sprint': ((24.0, 16.0, -15.0), (10.0, -40.0, -20.0)),          # lowered, turned across the body
    'bag': ((20.0, 24.0, -40.0), (-30.0, -20.0, 0.0)),              # hand down at the bag, out of frame
    'show_bag': ((30.0, 4.0, 0.0), (10.0, -80.0, 180.0)),           # arm across, palm down at the centre, the bag hanging
    'toss': ((30.0, 16.0, -6.0), (0.0, -60.0, 140.0)),              # the bag flicked away to the right
    'inspect': ((32.0, 4.0, -6.0), (40.0, -30.0, -10.0)),
    'inspect2': ((32.0, 4.0, -6.0), (40.0, -30.0, 60.0)),
    'empty': ((26.0, 16.0, -14.0), (15.0, -25.0, -10.0)),           # empty hand at rest
}

# TP weapon poses (bean space): (fingers direction, mitten centre, roll deg, right elbow pole or None = default)
TP_POSES = {
    'base': ((0.30, -0.93, 0.20), (-0.25, -0.17, 0.42), 0, None),
    'aim': ((-0.30, -0.35, 0.89), (-0.31, 0.04, 0.72), -25, (-0.8, 0.2, 0.55)),
    'light': ((0.60, -0.75, 0.25), (-0.12, -0.34, 0.52), -10, None),
    'light_aim': ((0.60, -0.75, 0.30), (-0.12, -0.34, 0.56), -10, None),
    'wind': ((-0.30, -0.15, 0.94), (-0.34, 0.02, 0.52), -20, (-0.9, 0.2, 0.3)),
    'release': ((0.20, -0.95, -0.22), (-0.21, -0.40, 0.58), 0, None),
    'follow': ((0.45, -0.75, -0.48), (-0.12, -0.36, 0.48), 10, None),
    'wind_long': ((-0.45, 0.15, 0.88), (-0.33, 0.10, 0.78), -30, (-0.8, 0.3, 0.5)),
    'release_long': ((0.10, -0.90, 0.42), (-0.19, -0.36, 0.76), 0, (-0.6, -0.4, 0.7)),
    'follow_long': ((0.30, -0.75, -0.60), (-0.08, -0.34, 0.52), 10, None),
    'holster': ((0.10, -0.30, -0.95), (-0.28, -0.08, 0.30), 0, None),
    'sprint': ((0.20, -0.85, -0.50), (-0.27, -0.12, 0.36), 0, None),
    'bag': ((0.10, -0.40, -0.90), (-0.27, -0.05, 0.32), 0, None),
    'show_bag': ((0.30, -0.95, 0.0), (-0.12, -0.33, 0.52), 180, None),
    'toss': ((-0.60, -0.70, -0.30), (-0.38, -0.22, 0.45), 160, None),
    'inspect': ((0.30, -0.60, 0.74), (-0.10, -0.30, 0.60), -10, None),
    'inspect2': ((0.30, -0.60, 0.74), (-0.10, -0.30, 0.60), 60, None),
    'empty': ((0.20, -0.90, -0.40), (-0.27, -0.14, 0.36), 0, None),
}
TP_POLE_R = Vector((-0.45, 0.15, -1.0))
TP_POLE_L = Vector((0.45, 0.15, -1.0))

_POSES = {}


def poses(view):
    if view not in _POSES:
        if view == 'FP':
            _POSES[view] = {k: (view_pose(GRIP_R, where, rot), None) for k, (where, rot) in FP_POSES.items()}
        else:
            _POSES[view] = {k: (frame(d, o, roll), Vector(pole).normalized() if pole else None) for k, (d, o, roll, pole) in TP_POSES.items()}
    return _POSES[view]


# weapon pose keys per clip: (frame, pose)
WEAPON_KEYS = {
    'Equip': [(0, 'holster'), (36, 'base'), (45, 'base')],
    'Idle': [(0, 'base')],
    'Walk': [(0, 'base')],
    'Sprint': [(0, 'sprint')],
    'AimIn': [(0, 'base'), (24, 'aim')],
    'AimIdle': [(0, 'aim')],
    'AimOut': [(0, 'aim'), (24, 'base')],
    'Fire': [(0, 'base'), (12, 'light'), (17, 'light'), (28, 'wind'), (FIRE_RELEASE, 'release'), (43, 'follow'), (60, 'base')],
    'AimFire': [(0, 'aim'), (6, 'light_aim'), (13, 'light_aim'), (20, 'wind_long'), (AIMFIRE_RELEASE, 'release_long'), (34, 'follow_long'),
                (48, 'aim')],
    'Reload': [(0, 'base'), (14, 'bag'), (32, 'bag'), (52, 'base'), (75, 'base')],
    'DryFire': [(0, 'base'), (10, 'bag'), (14, 'bag'), (30, 'base')],
    'Inspect': [(0, 'base'), (36, 'inspect'), (72, 'inspect2'), (108, 'inspect'), (150, 'base')],
    'Holster': [(0, 'base'), (6, 'base'), (40, 'holster'), (45, 'holster')],
    'Discard': [(0, 'empty'), (DISCARD_SHOW, 'bag'), (16, 'show_bag'), (26, 'show_bag'), (DISCARD_RELEASE, 'toss'), (36, 'empty')],
}


def pose_track(view, keys, f):
    p = poses(view)
    i, w = segment(f, keys)
    a, pa = p[keys[i][1]]
    b, pb = p[keys[min(i + 1, len(keys) - 1)][1]]
    pole = None
    if pa is not None or pb is not None:
        pole = (pa or TP_POLE_R).lerp(pb or TP_POLE_R, w).normalized()
    return blend(a, b, w), pole


def weapon_pose(clip, f, view):
    """Weapon root matrix (bean space) and the right elbow pole (TP) for clip / frame."""
    M, pole = pose_track(view, WEAPON_KEYS[clip], f)
    end = CLIPS[clip]
    if clip == 'Idle':
        q = math.sin(2 * math.pi * f / end)
        M = M @ Matrix.Translation((0, .002 * q, .004 * q)) @ Matrix.Rotation(math.radians(0.8 * q), 4, 'Y')
    elif clip == 'AimIdle':
        # light breathing: the cocked arm rises and falls, the ball tilts a little
        q = math.sin(2 * math.pi * f / end)
        M = Matrix.Translation(UP * (0.005 * q)) @ M @ Matrix.Rotation(math.radians(1.5 * q), 4, 'Y')
    elif clip in ('Walk', 'Sprint'):
        cycles = 4 if clip == 'Walk' else 3
        q = 2 * math.pi * f / end * cycles
        amp = 1.0 if clip == 'Walk' else 2.4
        M = M @ Matrix.Translation((.006 * amp * math.sin(q), .005 * amp * math.cos(q), .007 * amp * math.sin(q * 2)))
        M = M @ Matrix.Rotation(math.radians((2.5 if clip == 'Walk' else 6) * math.sin(q)), 4, 'X')
        if clip == 'Sprint':
            M = M @ Matrix.Rotation(math.radians(3.0 * math.sin(q * 2)), 4, 'Y')
    elif clip == 'Reload' and 14 < f < 32:
        # rummaging in the bag, out of frame
        q = 2 * math.pi * (f - 14) / 6.0
        M = M @ Matrix.Translation((.010 * math.sin(q), 0, .008 * math.cos(q)))
    elif clip == 'DryFire':
        dip = curve(f, [(10, 0), (12, 1), (14, 0)])
        M = M @ Matrix.Translation((0, 0, -0.025 * dip))
    elif clip == 'Discard' and 18 <= f <= 26:
        # three shakes of the empty bag
        s = math.sin(2 * math.pi * 3 * (f - 18) / 8.0)
        M = Matrix.Translation(RIGHT * (0.02 * s)) @ M @ Matrix.Rotation(math.radians(14 * s), 4, 'X')
    elif clip == 'Inspect':
        q = math.sin(2 * math.pi * f / 75.0)
        M = M @ Matrix.Rotation(math.radians(6 * q * curve(f, [(36, 0), (50, 1), (100, 1), (115, 0)])), 4, 'Z')
    return M, pole


def body_visible(clip, f):
    """The ball and its fuse are in the hand."""
    if clip == 'Fire':
        return f < FIRE_RELEASE
    if clip == 'AimFire':
        return f < AIMFIRE_RELEASE
    if clip == 'Reload':
        return f >= RELOAD_LOADED
    return clip not in ('DryFire', 'Discard')


def bag_visible(clip, f):
    return clip == 'Discard' and DISCARD_SHOW <= f < DISCARD_RELEASE


# ------------------------------------------------------------------------------------------------- left hand

# FP left targets: (point view cm or None (match), elbow view cm, back of the hand view dir, match direction view dir)
LEFT_FP = {
    'rest': ((24.0, -15.0, -14.0), (10.0, -32.0, -46.0), (-0.25, -0.50, 0.83), (0.25, -0.15, 0.95)),
    'low': ((22.0, -18.0, -20.0), (8.0, -34.0, -48.0), (-0.30, -0.45, 0.84), (0.30, -0.25, 0.92)),
    'aimrest': ((27.0, -12.0, -12.0), (12.0, -30.0, -46.0), (-0.25, -0.50, 0.83), (0.25, -0.10, 0.96)),
    'sprint': ((20.0, -18.0, -22.0), (6.0, -34.0, -48.0), (-0.30, -0.45, 0.84), (0.35, -0.25, 0.90)),
    'out': ((16.0, -24.0, -44.0), (4.0, -36.0, -60.0), (-0.30, -0.45, 0.84), (0.30, -0.30, 0.90)),
    'inspectl': ((26.0, -10.0, -6.0), (10.0, -30.0, -44.0), (-0.20, -0.55, 0.81), (0.20, 0.10, 0.97)),
    'match': (None, (10.0, -28.0, -40.0), (-0.15, -0.62, 0.77), (0.35, 0.78, 0.52)),
}
# TP left targets: (mitten centre bean space or None (match), match direction bean space)
LEFT_TP = {
    'rest': ((0.25, -0.18, 0.42), (-0.25, -0.25, 0.93)),
    'low': ((0.27, -0.20, 0.36), (-0.20, -0.30, 0.93)),
    'aimrest': ((0.22, -0.27, 0.50), (-0.30, -0.40, 0.87)),
    'sprint': ((0.26, -0.22, 0.40), (-0.20, -0.30, 0.93)),
    'out': ((0.30, -0.05, 0.30), (-0.20, -0.20, 0.96)),
    'inspectl': ((0.16, -0.30, 0.56), (-0.40, -0.30, 0.87)),
    'match': (None, (-0.40, 0.25, 0.88)),      # from the left fist in front of the chest, up and back to the fuse
}
LEFT_KEYS = {
    'Equip': [(0, 'out'), (10, 'out'), (40, 'rest'), (45, 'rest')],
    'Idle': [(0, 'rest')], 'Walk': [(0, 'rest')], 'Sprint': [(0, 'sprint')],
    'AimIn': [(0, 'rest'), (24, 'aimrest')], 'AimIdle': [(0, 'aimrest')], 'AimOut': [(0, 'aimrest'), (24, 'rest')],
    'Fire': [(0, 'rest'), (12, 'match'), (17, 'match'), (27, 'low'), (44, 'low'), (60, 'rest')],
    'AimFire': [(0, 'aimrest'), (8, 'match'), (13, 'match'), (22, 'low'), (40, 'low'), (48, 'aimrest')],
    'Reload': [(0, 'rest')], 'DryFire': [(0, 'rest')],
    'Inspect': [(0, 'rest'), (40, 'rest'), (70, 'inspectl'), (110, 'rest')],
    'Holster': [(0, 'rest'), (8, 'rest'), (40, 'out'), (45, 'out')],
    'Discard': [(0, 'rest')],
}
# the match in the left mitten: its lower 5 cm inside the mitten blob (head near the mitten centre, in the FP hand
# frame: x fingers, z back of the hand), the cord along the authored view direction, the ember 8 cm out
MATCH_HEAD_L = Vector((0.012, -0.010, 0.0))
TP_MATCH_HEAD = Vector((0.0, 0.0, 0.01))
MATCH_TOUCH = 0.004   # the ember stops this short of the fuse end


def hand_frame_fp(point, elbow_cm, back):
    return axes(point - vp(*elbow_cm), vv(*back))


def resolve_left(name, view, M):
    """Concrete left hand key: FP (point, rot, match_head, match_dir), TP (point, None, match_head, match_dir)."""
    fuse_tip = M @ FUSE_TIP
    if view == 'FP':
        where, elbow, back, mdir = LEFT_FP[name]
        d = vv(*mdir)
        if where is None:
            rot = hand_frame_fp(fuse_tip, elbow, back)
            point = fuse_tip - d * (MATCH_LEN - MATCH_TOUCH) - rot @ MATCH_HEAD_L
        else:
            point = vp(*where)
            rot = hand_frame_fp(point, elbow, back)
        return point, rot, point + rot @ MATCH_HEAD_L, d
    where, d = LEFT_TP[name]
    d = Vector(d).normalized()
    if where is None:
        point = fuse_tip - d * (MATCH_LEN - MATCH_TOUCH) - TP_MATCH_HEAD
    else:
        point = Vector(where)
    return point, None, point + TP_MATCH_HEAD, d


def left_hand(clip, f, view, M):
    keys = LEFT_KEYS[clip]
    i, w = segment(f, keys)
    a = resolve_left(keys[i][1], view, M)
    b = resolve_left(keys[min(i + 1, len(keys) - 1)][1], view, M)
    point = a[0].lerp(b[0], w)
    rot = None
    if view == 'FP':
        rot = a[1].to_quaternion().slerp(b[1].to_quaternion(), w).to_matrix()
    head = a[2].lerp(b[2], w)
    d = a[3].lerp(b[3], w).normalized()
    end = CLIPS[clip]
    if clip in ('Walk', 'Sprint'):
        cycles = 4 if clip == 'Walk' else 3
        q = 2 * math.pi * f / end * cycles
        amp = 1.0 if clip == 'Walk' else 2.0
        swing = (RIGHT * (.012 * math.cos(q)) + FWD * (.010 * math.sin(q)) + UP * (.012 * math.sin(2 * q))) * amp
        point, head = point + swing, head + swing
    elif clip in ('Idle', 'AimIdle'):
        q = math.sin(2 * math.pi * f / end + 1.0)
        point, head = point + UP * (.003 * q), head + UP * (.003 * q)
    return point, rot, head, d


# ------------------------------------------------------------------------------------------------- TP torso

def torso(clip, f):
    """(twist about Z deg: + turns to the bean's left; lean forward deg) of the TP spine."""
    if clip == 'Fire':
        return curve(f, [(16, 0), (28, -14), (34, 16), (44, 8), (60, 0)]), curve(f, [(26, 0), (34, 8), (44, 4), (60, 0)])
    if clip == 'AimFire':
        return curve(f, [(10, 0), (20, -22), (25, 18), (34, 10), (48, 0)]), curve(f, [(18, 0), (25, 10), (34, 5), (48, 0)])
    if clip == 'AimIn':
        return curve(f, [(0, 0), (24, -12)]), 0.0
    if clip == 'AimIdle':
        return -12.0, 0.0
    if clip == 'AimOut':
        return curve(f, [(0, -12), (24, 0)]), 0.0
    if clip == 'Discard':
        return curve(f, [(26, 0), (30, 12), (36, 0)]), 0.0
    if clip == 'Inspect':
        return 0.0, curve(f, [(0, 0), (36, 6), (110, 6), (150, 0)])
    if clip == 'Sprint':
        return 0.0, 10.0
    return 0.0, 0.0


def head_pose(clip, f):
    """(yaw deg: + to the bean's left; look down deg) of the TP head."""
    if clip == 'Fire':
        return 0.0, curve(f, [(6, 0), (12, 16), (20, 16), (30, 0)])
    if clip == 'AimFire':
        return 0.0, curve(f, [(2, 0), (8, 14), (14, 14), (22, 0)])
    if clip == 'Inspect':
        return 0.0, curve(f, [(20, 0), (36, 14), (110, 14), (130, 0)])
    if clip == 'Discard':
        return 0.0, curve(f, [(10, 0), (16, 12), (28, 12), (34, 0)])
    if clip == 'Reload':
        return 0.0, curve(f, [(6, 0), (14, 10), (36, 10), (50, 0)])
    return 0.0, 0.0


# ------------------------------------------------------------------------------------------------- solvers

def local_rots(arm, bone, rotations):
    """Pose rotation of a bone from (axis (armature space), degrees) pairs composed in order."""
    p = arm.pose.bones[bone]
    p.rotation_mode = 'QUATERNION'
    inv = p.bone.matrix_local.to_3x3().inverted()
    q = Quaternion()
    for axis, ang in rotations:
        q = q @ Quaternion(inv @ Vector(axis), math.radians(ang))
    p.rotation_quaternion = q


FP_HAND_CENTRE = Vector((0.36, 0.0, -0.015))   # centre of the FPArm mitten (scripts/character/bean_soldier.py)


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


def solve_fp_arm(arm, matrices, side, point, rot):
    """First person: the FPArm (rigid on hand_<side>) gets its mitten centre on point, its forearm along rot's x and
    the back of the hand along rot's z; the upper arm reaches back to an invisible shoulder without stretching."""
    T = fp_arm_transform(arm, side)
    F = rot.copy()
    if side == 'l':
        F = F @ Matrix.Diagonal((1, -1, 1))
    F4 = F.to_4x4()
    F4.translation = point - F @ FP_HAND_CENTRE
    delta = F4 @ T.inverted()
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


def solve_tp_arm(arm, matrices, side, target, max_stretch=1.32, pole=None):
    """Third person: exact two-segment solve from the real shoulder, hand rigid on the forearm; the bean's arms are
    short, a soft uniform stretch (at most max_stretch) lets the mittens reach."""
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
    pole = Vector(pole) if pole is not None else (TP_POLE_L if side == 'l' else TP_POLE_R)
    perpendicular = (pole - axis * pole.dot(axis))
    if perpendicular.length < 1e-4:
        perpendicular = Vector((0, 0, -1)) - axis * (-axis.z)
    perpendicular.normalize()
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

def events(clip):
    def ev(frame_, name, socket=None):
        e = {'frame': frame_, 'name': name}
        if socket:
            e['socket'] = socket
        return e
    if clip == 'Fire':
        return [ev(FIRE_LIGHT, 'Light', 'socket_muzzle'), ev(FIRE_RELEASE, 'Fire', 'socket_body')]
    if clip == 'AimFire':
        return [ev(AIMFIRE_LIGHT, 'Light', 'socket_muzzle'), ev(AIMFIRE_RELEASE, 'Fire', 'socket_body')]
    if clip == 'Reload':
        return [ev(14, 'BagReach', 'socket_grip_r'), ev(RELOAD_LOADED, 'HammerCocked', 'socket_body'), ev(72, 'Ready')]
    if clip == 'DryFire':
        return [ev(12, 'DryClick', 'socket_grip_r')]
    if clip == 'Equip':
        return [ev(8, 'DrawWeapon'), ev(38, 'Ready')]
    if clip == 'Holster':
        return [ev(40, 'StowWeapon')]
    if clip == 'Discard':
        # Empty at frame 2, not 0: AFirearmWeapon::StartAction schedules no timer for an event at the clip start
        return [ev(2, 'Empty', 'socket_grip_r'), ev(DISCARD_RELEASE, 'Release', 'socket_bag')]
    return []


# ----------------------------------------------------------------------------------------------- animate

def scaled_about(M, head, s):
    return M @ Matrix.Translation(head) @ Matrix.Diagonal((s, s, s, 1.0)) @ Matrix.Translation(-head)


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
    rest = {n: gun.data.bones[n].matrix_local.copy() for n in bone_names}
    for f in range(end + 1):
        scene.frame_set(f)
        for pb in arm.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        # TP body: torso twist and lean for the throws, head looking at the fuse while lighting
        if view == 'TP':
            twist, lean = torso(clip, f)
            yaw, down = head_pose(clip, f)
            local_rots(arm, 'spine_02', [((0, 0, 1), twist)])
            local_rots(arm, 'spine_01', [((1, 0, 0), lean)])
            local_rots(arm, 'head', [((0, 0, 1), yaw), ((1, 0, 0), down)])
        phase = 0.0
        if clip in ('Walk', 'Sprint'):
            cyc = 4 if clip == 'Walk' else 3
            phase = 2 * math.pi * f / end * cyc
            amp = 18 if clip == 'Walk' else 28
            for side, off in (('l', 0), ('r', math.pi)):
                swing = math.sin(phase + off)
                local_rots(arm, 'thigh_' + side, [((1, 0, 0), amp * swing)])
                local_rots(arm, 'calf_' + side, [((1, 0, 0), -max(0, -swing) * (25 if clip == 'Walk' else 43))])
                local_rots(arm, 'foot_' + side, [((1, 0, 0), -amp * swing * .5)])
            arm.pose.bones['root'].location = (0, 0, .006 + .007 * (1 - math.cos(2 * phase)))
            arm.pose.bones['pelvis'].location = (0, -.027, 0)
        else:
            arm.pose.bones['root'].location = (0, 0, .003)
        M, pole = weapon_pose(clip, f, view)
        rp = M @ GRIP_R
        rrot = M.to_3x3() @ R_WH
        lp, lrot, match_head, match_dir = left_hand(clip, f, view, M)
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev = arm.evaluated_get(dg)
        bm = {p.name: p.matrix.copy() for p in ev.pose.bones}
        if clip in ('Walk', 'Sprint'):
            for side, off in (('l', 0), ('r', math.pi)):
                step = phase + off
                v = arm.data.bones['foot_' + side].head_local.copy()
                v.y = .035 * math.cos(step)
                v.z = .053 + (max(0, math.sin(step)) * (.024 if clip == 'Walk' else .040))
                solve_leg(arm, bm, side, v)
        for side, point, rot, pl in (('r', rp, rrot, pole), ('l', lp, lrot, None)):
            if view == 'FP':
                error, scale = solve_fp_arm(arm, bm, side, point, rot)
            else:
                error, scale = solve_tp_arm(arm, bm, side, point, pole=pl)
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
            rp, lp = rp + Vector((0, 0, lift)), lp + Vector((0, 0, lift))
            match_head = match_head + Vector((0, 0, lift))
        bean_samples.append(bm)
        grips.append(dict(frame=f, left=[*lp], right=[*rp], body_visible=body_visible(clip, f), bag_visible=bag_visible(clip, f)))
        # the grenade: gun_root on the right hand, ball and fuse shown or collapsed, the match from the left hand,
        # the bag hanging from the palm in Discard
        matrices = {n: M @ rest[n] for n in bone_names}
        sb = 1.0 if body_visible(clip, f) else 0.0001
        matrices['body'] = scaled_about(M, BALL, sb) @ rest['body']
        matrices['fuse'] = matrices['body'] @ rest['body'].inverted() @ rest['fuse']
        Rm = z_frame(match_dir, RIGHT if view == 'FP' else FWD).to_4x4()
        matrices['match'] = Matrix.Translation(match_head) @ Rm @ Matrix.Translation(-MATCH_REST) @ rest['match']
        if bag_visible(clip, f):
            matrices['bag'] = M @ Matrix.Translation((0, 0, 0.03)) @ Matrix.Rotation(math.pi, 4, 'X') @ Matrix.Translation(-BAG_REST) @ rest['bag']
        else:
            matrices['bag'] = scaled_about(M, BAG_REST, 0.0001) @ rest['bag']
        for n in bone_names:
            b = gun.data.bones[n]
            if b.parent and n.startswith('socket_'):
                matrices[n] = matrices[b.parent.name] @ rest[b.parent.name].inverted() @ rest[n]
        gun_samples.append(matrices)
    ba = bake_matrices(arm, bean_samples, 'A_Bean_' + view + '_Grenade_' + clip)
    ga = bake_matrices(gun, gun_samples, 'A_Grenade_' + view + '_' + clip)
    scene.frame_set(0)
    bpy.context.view_layer.update()
    return dict(bean_action=ba.name, weapon_action=ga.name, events=events(clip),
                hand_error_max_m=errors, upperarm_scale_max=max(stretches), frame_start=0, frame_end=end, fps=FPS,
                authored_method='T-253 local pack: the weapon pose drives the right hand (grenade rigid in the mitten); '
                                'FP arms placed directly through the game camera (no stretch), TP two-segment arms '
                                'with torso twist; slow match on the weapon bone "match" in the left hand; body/fuse '
                                'collapsed (scale 0.0001) when no grenade is in hand; bag shown in Discard only.',
                grip_targets=grips)


def fp_framing():
    """What the game needs to frame the FP clips like this pack's camera (DA for AGrenadeWeapon)."""
    base = view_pose(GRIP_R, *FP_POSES['base'])
    aim = view_pose(GRIP_R, *FP_POSES['aim'])
    pitch, yaw, roll = rotator_in_view(base)
    apitch, ayaw, aroll = rotator_in_view(aim)
    return dict(camera_location_m=[*FP_CAMERA], horizontal_fov_deg=90.0,
                hip_grip_location_cm=[round(v, 2) for v in to_view(base @ GRIP_R)],
                hip_rotation_pyr_deg=[round(v, 3) for v in (pitch, yaw, roll)],
                aim_muzzle_location_cm=[round(v, 2) for v in to_view(aim @ FUSE_TIP)],
                aim_rotation_pyr_deg=[round(v, 3) for v in (apitch, ayaw, aroll)],
                aim_grip_location_cm=[round(v, 2) for v in to_view(aim @ GRIP_R)],
                note='HipGripLocation / HipRotation reproduce this camera exactly (Idle frame 0 = FP base pose); '
                     'AimMuzzleLocation / AimRotation reproduce the AimIdle frame 0 pose (the muzzle is the fuse tip). '
                     'Or bAlignFirstPersonToView = false with FirstPersonViewLocation = camera_location_m: the pack camera as is.')
