"""First person keys of the bandage (T-340): two SM_Bean_FPArm poses (FBeanArmPose) per frame, 30 fps, same timing and
events as A_Bean_Bandage_Apply.

Unreal side (Source/monke_island/Character/Bean/BeanFirstPersonArmsComponent.h, Weapons/MusketMath.h):
  FBeanArmPose = Location (cm, the shoulder relative to the first person camera: X forward, Y right, Z up), Rotation
  (FRotator, the arm mesh points along +X), Stretch (X scale of the arm). SetPoseOverride(Right, Left) applies the right one
  as given and the left one MIRRORED (Location.Y, Yaw and Roll negated, scale Y -1): the left pose is therefore written
  here "as a right arm" (MusketMath::MirrorArm of the real left pose), exactly what SetPoseOverride expects.
  The poses are those of MusketMath::SolveArm(Shoulder, Target, HandPoint, MaxStretch) (rotation MakeFromXZ(dir, Up), roll
  0): the targets are given too, so the code may either play the keys or call SolveArm on the targets.

The right mitten goes round the left forearm (cuff, where SM_Bandage_Wrapped sits) on its free sides: over the top, the
side toward the right arm and underneath, then swings back further out (the same "D" loop as the third person clip; a
full turn would make the right arm pass through the left one: two straight arms fixed to their shoulders).
Python side: camera space in Unreal convention (cm, X forward, Y right, Z up); Blender previews mirror Y (metres).
"""
from __future__ import annotations

import math

from mathutils import Matrix, Vector

import clips as C

FPS = C.FPS
# shoulders as MusketRules (RightShoulder / LeftShoulder), mitten point as MusketRules::ArmHandPoint (SM_Bean_FPArm, cm)
FP = {
    'right_shoulder': (-5.0, 18.0, -20.0),
    'left_shoulder': (-5.0, -18.0, -22.0),
    'hand_point': (36.0, 0.0, -1.5),
    'max_stretch': 1.6,
    'rest_right': ((-5.0, 24.0, -62.0), (40.0, -5.0, 0.0)),        # UBeanFirstPersonArmsComponent::RestPose
    'left_target': (55.25, 11.95, -4.81),    # left mitten in the wrap pose (camera cm): the band below the middle, right
    'cuff_x': 27.0,          # band centre along the arm mesh (cm, FPArm_Cuff 22.5 .. 31.1)
    'cuff_radius': 7.9,      # FPArm_Cuff outer radius (cm, measured)
    'mitten_radius': 5.2,
    'band_extra': 0.8,       # three turns of linen on the cuff
    'arc': (-0.91, 187.05),   # 0 = top (toward the camera's up), 90 = toward the right arm, 180 = underneath
    'r_out': 1.5,
    'r_extra': 1.10,         # the mitten 1 cm off the band (closer: the right arm crosses the left one more)
    'roll_l': 10.0,
    'pat_deg': 94.6,         # where the mitten tightens and pats (deg on the loop: 90 = the side toward the right arm)
}
SLEEVE_RADIUS = 8.7          # FPArm_Sleeve near the shoulder pivot (cm, measured: 6.5 at the cuff .. 8.7 at x 0)
# (x along the arm mesh, radius) of SM_Bean_FPArm (cm, measured on bean_soldier.blend): sleeve, cuff, mitten
ARM_PROFILE = ((0.0, 8.7), (8.0, 8.0), (15.0, 7.2), (22.5, 7.9), (27.0, 7.9), (31.0, 7.9), (36.0, 5.2), (41.0, 3.5))


# ------------------------------------------------------------------------------------------------- Unreal maths
def rotator_matrix(pitch, yaw, roll):
    """FRotationMatrix: rows X, Y, Z axes (Unreal frame) -> 3x3 with the axes as columns"""
    sp, cp = math.sin(math.radians(pitch)), math.cos(math.radians(pitch))
    sy, cy = math.sin(math.radians(yaw)), math.cos(math.radians(yaw))
    sr, cr = math.sin(math.radians(roll)), math.cos(math.radians(roll))
    x = Vector((cp * cy, cp * sy, sp))
    y = Vector((sr * sp * cy - cr * sy, sr * sp * sy + cr * cy, -sr * cp))
    z = Vector((-(cr * sp * cy + sr * sy), cy * sr - cr * sp * sy, cr * cp))
    return Matrix((x, y, z)).transposed()


def matrix_rotator(Mx):
    x, y, z = Vector(Mx.col[0]), Vector(Mx.col[1]), Vector(Mx.col[2])
    pitch = math.degrees(math.atan2(x.z, math.sqrt(x.x * x.x + x.y * x.y)))
    yaw = math.degrees(math.atan2(x.y, x.x))
    sy_axis = Vector(rotator_matrix(pitch, yaw, 0.0).col[1])
    roll = math.degrees(math.atan2(z.dot(sy_axis), y.dot(sy_axis)))
    return pitch, yaw, roll


def make_from_xz(xdir, zdir=(0.0, 0.0, 1.0)):
    x = Vector(xdir).normalized()
    z0 = Vector(zdir).normalized()
    y = z0.cross(x).normalized()
    z = x.cross(y)
    return Matrix((x, y, z)).transposed()


def solve_arm(shoulder, target, hand_point, max_stretch):
    """MusketMath::SolveArm"""
    S, T = Vector(shoulder), Vector(target)
    d = T - S
    direction = d.normalized() if d.length > 1e-6 else Vector((1, 0, 0))
    Rm = make_from_xz(direction)
    pitch, yaw, roll = matrix_rotator(Rm)
    Rm = rotator_matrix(pitch, yaw, roll)
    reach = max(hand_point[0], 1.0)
    stretch = min(max(d.length / reach, 1.0), max(max_stretch, 1.0))
    hand = Vector((hand_point[0] * stretch, hand_point[1], hand_point[2]))
    loc = T - Rm @ hand
    return {'location_cm': loc, 'rotation': (pitch, yaw, roll), 'stretch': stretch, 'matrix': Rm}


def mirror(pose):
    """MusketMath::MirrorArm"""
    L = pose['location_cm']
    p, y, r = pose['rotation']
    return {'location_cm': Vector((L.x, -L.y, L.z)), 'rotation': (p, -y, -r), 'stretch': pose['stretch']}


def arm_point(pose, x_mesh, yz=(0.0, 0.0), mirrored=False):
    """A point of the arm mesh (cm, mesh space) in camera space for a pose given as applied (not the mirrored form)"""
    Rm = rotator_matrix(*pose['rotation'])
    p = Vector((x_mesh * pose['stretch'], yz[0] * (-1 if mirrored else 1), yz[1]))
    return pose['location_cm'] + Rm @ p


def blend_pose(a, b, w):
    """Location lerp, rotation slerp, stretch lerp (w = 0: a, 1: b)"""
    qa = rotator_matrix(*a['rotation']).to_quaternion()
    qb = rotator_matrix(*b['rotation']).to_quaternion()
    Rm = qa.slerp(qb, w).to_matrix()
    return {'location_cm': Vector(a['location_cm']).lerp(Vector(b['location_cm']), w), 'rotation': matrix_rotator(Rm),
            'stretch': a['stretch'] + (b['stretch'] - a['stretch']) * w, 'matrix': Rm}


def as_row(pose):
    return {'location_cm': [round(v, 3) for v in pose['location_cm']],
            'rotation_pyr_deg': [round(v, 3) for v in pose['rotation']], 'stretch': round(pose['stretch'], 4)}


# ------------------------------------------------------------------------------------------------- keys
def seg_dist(p1, q1, p2, q2, n=16):
    best = 1e9
    for i in range(n + 1):
        a = p1.lerp(q1, i / n)
        d = q2 - p2
        t = max(0.0, min(1.0, (a - p2).dot(d) / max(d.dot(d), 1e-9)))
        best = min(best, (a - (p2 + d * t)).length)
        a = p2.lerp(q2, i / n)
        d = q1 - p1
        t = max(0.0, min(1.0, (a - p1).dot(d) / max(d.dot(d), 1e-9)))
        best = min(best, (a - (p1 + d * t)).length)
    return best


def build_keys(roll_offset_cm):
    """Per frame poses (0..90). roll_offset_cm: the roll centre in the right arm mesh (cm), for the qa"""
    P = FP
    n = C.T['end']
    f_up = C.T['up']
    f_loops_end = f_up + C.T['turns'] * C.T['turn_frames']
    f_tight_end = f_loops_end + C.T['tight']
    f_done = C.T['done']
    hp = P['hand_point']
    Lsh, Rsh = Vector(P['left_shoulder']), Vector(P['right_shoulder'])
    rest_r_loc, rest_r_rot = P['rest_right']
    rest_r = {'location_cm': Vector(rest_r_loc), 'rotation': rest_r_rot, 'stretch': 1.0}
    rest_r_target = arm_point(rest_r, hp[0], (hp[1], hp[2]))
    keys = []
    qa = {'min_right_mitten_to_left_cuff_cm': 1e9, 'min_right_mitten_to_left_mitten_cm': 1e9,
          'min_roll_to_left_arm_cm': 1e9, 'min_arm_axes_distance_cm': 1e9, 'min_camera_distance_cm': 1e9,
          'max_stretch_right': 0.0, 'max_stretch_left': 0.0}
    a0, a1 = P['arc']
    for f in range(n + 1):
        w_l = C.keyed(f, [(0, 0.0), (f_up - 1, 1.0), (f_done + 1, 1.0), (n, 0.0)])
        lt = Vector(P['left_target'])
        if f_up <= f < f_loops_end:
            u = ((f - f_up) % C.T['turn_frames']) / C.T['turn_frames']
            lt = lt + Vector((0.0, 0.6 * math.sin(C.TAU * u), 0.8 * math.sin(C.TAU * u + 0.6)))
        left = solve_arm(Lsh, lt, hp, P['max_stretch'])
        # the left forearm (cuff) axis and centre in camera space
        Rl = left['matrix']
        a = (Rl @ Vector((1, 0, 0))).normalized()
        c = left['location_cm'] + Rl @ Vector((P['cuff_x'] * left['stretch'], 0.0, hp[2]))
        top = Vector((0, 0, 1)) - a * a.z
        top.normalize()
        side = a.cross(top)                     # toward +Y (the right arm) when the forearm points forward
        if side.y < 0:
            side = -side
        r_touch = P['cuff_radius'] + P['band_extra'] + P['mitten_radius'] + P['r_extra']
        p0 = math.radians(a0)
        start_target = c + (top * math.cos(p0) + side * math.sin(p0)) * r_touch
        if f < f_up:
            rt = start_target
        elif f < f_loops_end:
            u = ((f - f_up) % C.T['turn_frames']) / C.T['turn_frames']
            mid, amp = (a0 + a1) / 2, (a1 - a0) / 2
            phi = math.radians(mid - amp * math.cos(C.TAU * u))
            back = max(0.0, -math.sin(C.TAU * u))
            rt = c + (top * math.cos(phi) + side * math.sin(phi)) * (r_touch + P['r_out'] * back * back)
        elif f < f_tight_end:
            u = (f - f_loops_end) / C.T['tight']
            phi = math.radians(a0 + (P['pat_deg'] - a0) * C.smooth(u * 2))
            tug = math.sin(math.pi * u)
            rt = c + (top * math.cos(phi) + side * math.sin(phi)) * (r_touch + 3.0 * tug) + a * 1.2 * tug
        elif f < f_done:
            u = (f - f_tight_end) / max(f_done - f_tight_end, 1)
            pd = math.radians(P['pat_deg'])
            rt = c + (top * math.cos(pd) + side * math.sin(pd)) * (r_touch + 2.5 * abs(math.sin(2 * math.pi * u)))
        else:
            pd = math.radians(P['pat_deg'])
            rt = c + (top * math.cos(pd) + side * math.sin(pd)) * r_touch
        # raise and lower in target space: shoulder and mitten target both slide from / to the arms component's RestPose
        # (the arm keeps pointing at its mitten: no sweep across the lens); frames 0 and 90 are the RestPose exactly
        w_in = 1.0 - C.keyed(f, [(0, 0.0), (f_up, 1.0)]) if f < f_up else 0.0
        w_out = C.keyed(f, [(f_done, 0.0), (n, 1.0)]) if f > f_done else 0.0
        w_rest = max(w_in, w_out)
        r_sh, l_sh = Rsh, Lsh
        if w_rest > 0.0:
            rest_l_loc = Vector((rest_r_loc[0], -rest_r_loc[1], rest_r_loc[2]))
            r_sh = Rsh.lerp(Vector(rest_r_loc), w_rest)
            l_sh = Lsh.lerp(rest_l_loc, w_rest)
            rt = rt.lerp(rest_r_target, w_rest)
            lt = lt.lerp(Vector((rest_r_target.x, -rest_r_target.y, rest_r_target.z)), w_rest)
            left = solve_arm(l_sh, lt, hp, P['max_stretch'])
        right = solve_arm(r_sh, rt, hp, P['max_stretch'])
        left_mirrored = mirror(left)
        if f in (0, n):
            right = dict(rest_r, matrix=rotator_matrix(*rest_r['rotation']))
            left_mirrored = {'location_cm': Vector(rest_r_loc), 'rotation': rest_r_rot, 'stretch': 1.0}
            left = mirror(left_mirrored)
            left['matrix'] = rotator_matrix(*left['rotation'])
        keys.append({'frame': f, 'time_s': round(f / FPS, 4), 'right': as_row(right), 'left': as_row(left_mirrored),
                     'right_target_cm': [round(v, 2) for v in rt], 'left_target_cm': [round(v, 2) for v in lt],
                     '_right': right, '_left': left, '_c': c, '_a': a})
        # qa (frames where the right mitten works on the forearm)
        if f_up <= f < f_done:
            rm = arm_point(right, hp[0], (hp[1], hp[2]))
            lm = arm_point(left, hp[0], (hp[1], hp[2]), mirrored=True)
            l_cuff0 = arm_point(left, 22.5, (0.0, hp[2]), mirrored=True)
            l_cuff1 = arm_point(left, 31.0, (0.0, hp[2]), mirrored=True)
            d = seg_dist(rm, rm, l_cuff0, l_cuff1) - P['cuff_radius'] - P['band_extra'] - P['mitten_radius']
            qa['min_right_mitten_to_left_cuff_cm'] = min(qa['min_right_mitten_to_left_cuff_cm'], round(d, 2))
            d = (rm - lm).length - 2 * P['mitten_radius']
            qa['min_right_mitten_to_left_mitten_cm'] = min(qa['min_right_mitten_to_left_mitten_cm'], round(d, 2))
            roll = arm_point(right, roll_offset_cm[0], (roll_offset_cm[1], roll_offset_cm[2]))
            d = min(seg_dist(roll, roll, l_cuff0, l_cuff1) - P['cuff_radius'] - P['band_extra'],
                    (roll - lm).length - P['mitten_radius']) - 3.0
            qa['min_roll_to_left_arm_cm'] = min(qa['min_roll_to_left_arm_cm'], round(d, 2))
            # the two arms (axes from the camera plane forward), minus their radii (cuff / sleeve)
            r0 = arm_point(right, 0.0)
            r1 = arm_point(right, 31.0)
            l0 = arm_point(left, 0.0, mirrored=True)
            l1 = arm_point(left, 31.0, mirrored=True)
            d = seg_dist(r0, r1, l0, l1) - 2 * P['cuff_radius']
            qa['min_arm_axes_distance_cm'] = min(qa['min_arm_axes_distance_cm'], round(d, 2))
            qa['min_roll_to_camera_cm'] = min(qa.get('min_roll_to_camera_cm', 1e9), round(roll.length - 3.0, 2))
        # every frame: the visible arm (axis samples minus the measured radii) and the mittens to the camera
        for pose, mir in ((right, False), (left, True)):
            for x, r in ARM_PROFILE:
                pt = arm_point(pose, x, (0.0, hp[2]), mirrored=mir)
                qa['min_camera_distance_cm'] = min(qa['min_camera_distance_cm'], round(pt.length - r, 2))
        qa['max_stretch_right'] = max(qa['max_stretch_right'], round(right['stretch'], 3))
        qa['max_stretch_left'] = max(qa['max_stretch_left'], round(left['stretch'], 3))
    return keys, qa


# ------------------------------------------------------------------------------------------------- Blender previews
M_MIRROR = Matrix.Diagonal((1.0, -1.0, 1.0))


def blender_matrix(pose, mirrored=False):
    """Object matrix (Blender camera space, metres) of an arm mesh for a pose as applied in Unreal"""
    Rm = rotator_matrix(*pose['rotation'])
    Rb = M_MIRROR @ Rm @ M_MIRROR
    L = pose['location_cm']
    T = Matrix.Translation((L.x / 100.0, -L.y / 100.0, L.z / 100.0))
    S = Matrix.Diagonal((pose['stretch'], -1.0 if mirrored else 1.0, 1.0, 1.0))
    return T @ Rb.to_4x4() @ S
