"""Bean rig solvers of the pistol pack (T-248): local copy of the musket pack's T-212 solvers (same approach, same
bean rest hierarchy), kept here so the pistol pack never depends on another cell's folder.

Bean space (Blender): the bean looks along -Y, its right side is -X, +Z up, metres. First person: the FPArm parts
(sleeve, cuff, mitten) are rigid on hand_<side>; they are placed directly (no stretch) with the mitten centre on
the target and the forearm along the given hand frame. Third person: analytic two-segment arms from the real
shoulders with a soft uniform stretch, hands rigid on the forearms, legs likewise.
"""
from __future__ import annotations

import math

import bpy
from mathutils import Matrix, Quaternion, Vector

FP_HAND_CENTRE = Vector((0.36, 0.0, -0.015))   # centre of the FPArm mitten (scripts/character/bean_soldier.py)
REST_HAND = {'r': Vector((-0.304, -0.012, 0.325)), 'l': Vector((0.304, -0.012, 0.325))}   # body mitten centres at rest


# ------------------------------------------------------------------------------------------------ easing

def smooth(a, b, f):
    t = max(0., min(1., (f - a) / max(1e-6, b - a)))
    return t * t * (3 - 2 * t)


def curve(f, keys):
    """Piecewise smoothstep through (frame, value) keys."""
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


def point_curve(f, keys):
    i, w = segment(f, keys)
    a = Vector(keys[i][1])
    b = Vector(keys[min(i + 1, len(keys) - 1)][1])
    return a.lerp(b, w)


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


def blend(a, b, w):
    out = a.to_quaternion().slerp(b.to_quaternion(), w).to_matrix().to_4x4()
    out.translation = a.translation.lerp(b.translation, w)
    return out


# ------------------------------------------------------------------------------------------ first person view

FP_CAMERA = Vector((0.0, 0.0, 0.80))      # eye of the authoring camera (bean space), the game's FP camera height
FWD, RIGHT, UP = Vector((0, -1, 0)), Vector((-1, 0, 0)), Vector((0, 0, 1))
FP_LENS = 18.0                              # 36 mm sensor: 90 deg horizontal, as the game's first person camera


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


# ------------------------------------------------------------------------------------------------ solvers

def local_rot(arm, bone, axis, ang):
    p = arm.pose.bones[bone]
    p.rotation_mode = 'QUATERNION'
    axis = p.bone.matrix_local.to_3x3().inverted() @ Vector(axis)
    p.rotation_quaternion = Quaternion(axis, math.radians(ang))


def fp_arm_transform(arm, side):
    """FPArm local -> bean rest space (mirrored for the left arm), mitten centre on grip_<side>."""
    grip = arm.data.bones['grip_' + side].tail_local
    d = (grip - arm.data.bones['lowerarm_' + side].head_local).normalized()
    R = frame(d, (0, 0, 0)).to_3x3()
    if side == 'l':
        R = R @ Matrix.Diagonal((1, -1, 1))
    T = R.to_4x4()
    T.translation = grip - R @ FP_HAND_CENTRE
    return T


def solve_fp_arm(arm, matrices, side, point, rot, shoulder_cm):
    """First person: the FPArm (rigid on hand_<side>) gets its mitten centre on point, its forearm along rot's x and
    the back of the hand along rot's z; the upper arm reaches back to an invisible shoulder (view cm)."""
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
    shoulder = vp(*shoulder_cm)
    direction = (elbow - shoulder).normalized()
    rest = (up.tail_local - up.head_local)
    q = rest.normalized().rotation_difference(direction)
    head = elbow - direction * rest.length
    matrices[up.name] = Matrix.Translation(head) @ q.to_matrix().to_4x4() @ Matrix.Translation(-up.head_local) @ up.matrix_local
    got = delta @ T @ FP_HAND_CENTRE
    return (got - point).length, 1.0


def solve_tp_arm(arm, matrices, side, target, max_stretch=1.32, pole=None):
    """Third person: exact two-segment solve from the real shoulder, hand rigid on the forearm; a soft uniform
    stretch (at most max_stretch) lets the short bean arms reach the weapon."""
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
    pole = Vector(pole) if pole else Vector((.45 if side == 'l' else -.45, .15, -1.0))
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


# ------------------------------------------------------------------------------------------------- baking

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
