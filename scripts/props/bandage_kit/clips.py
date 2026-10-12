"""A_Bean_Bandage_Apply (T-340): the bean bandages its left forearm, 3.0 s at 30 fps, root fixed (no root motion).

  0.00 - 0.40 s   the left forearm comes up in front of the chest, the right mitten brings the roll onto it
  0.40 s          WrapStart: the loose end is pressed on top of the forearm
  0.40 - 2.00 s   three turns: the right mitten goes over the top of the forearm and down its outer side, then swings
                  back up a little further out (a "D" loop; the left forearm rolls a little against it)
  2.00 - 2.27 s   the band is pulled tight (a tug outward)
  2.27 - 2.67 s   two pats on top of the band
  2.73 s          BandageDone: the heal is applied (DA_BandageRules), the roll is used up
  2.73 - 3.00 s   both arms back to rest (frame 90 = rest pose, like frame 0)

Geometry (measured on bean_soldier.blend, see build.py qa): the bean's arms reach 24 cm to the wrist, its sleeves are
7 cm in radius and its chest stands 23 to 29 cm in front of the shoulder line, so a full 360 degree turn of the mitten
around the forearm (the mitten must pass between forearm and chest) needs the arms stretched about 2.2 times and makes
the two sleeves cross: out of reach for this rig. The loop therefore stays on the free sides: over the top and down the
outer side (qa.loop_arc_deg; underneath, the belly bulges under the forearm and the roll would touch the coat), never
between forearm and chest. Both shoulders slide forward (shoulder_*, upperarm translation keys) so that the right mitten
reaches the left forearm with the arms stretched 1.56 at most (qa.max_stretch).
"""
from __future__ import annotations

import math

from mathutils import Matrix, Quaternion, Vector

FPS = 30
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
TAU = 2 * math.pi

# ------------------------------------------------------------------------------------------------- timing (frames)
T = {'up': 12, 'turns': 3, 'turn_frames': 16, 'tight': 8, 'pat': 12, 'done': 82, 'end': 90}

# ------------------------------------------------------------------------------------------------- pose tuning
POSE = {
    # found by a search on the real rig (hand clear of body, hats and left arm; band at chest height, mitten below the face;
    # then the widest arc at the lowest stretch)
    'lean': 4.0,             # trunk forward (deg, spread on spine_01 / spine_02)
    'twist': 0.0,            # trunk turned to the bean's left (deg)
    'nod': 7.0,              # head down (deg) - the tricorne brim stays above the loops (qa)
    'look': 10.0,            # head turned toward the left forearm (deg)
    'wrist_l': (0.0907, -0.2139, 0.5831),   # left wrist (hand_l head) in the wrap pose, armature space
    'pole_l': (3.893, 0.077, -0.357),       # left elbow out to the side
    'roll_l': 14.0,          # left forearm counter roll during each turn (deg, about its axis)
    'r_touch_extra': 0.004,  # mitten centre this far beyond touching the band
    'r_out': 0.027,          # return arc this much further out
    'arc': (0.72, 105.0),     # loop arc in the forearm frame (deg): 0 = top, 90 = outer side, 180 = underneath; beyond ~110 the
                             # roll at the fingertips reaches the coat over the belly (qa.contacts)
    'pole_r': (-1.201, 0.123, 0.197),   # right elbow out and up: its sleeve rides higher over the left arm
    'max_stretch': 1.7,
    'shoulder_fwd': 0.10,    # both shoulders forward (m, upperarm translation), ramped with the raise
    'shoulder_in': 0.0565,   # both shoulders toward the middle (m): the bean hugs its forearm
    'shoulder_up': -0.0256,
    # raise / lowering paths (armature space): round the front of the belly (it stands 31 cm out at the belt)
    'path_l': ((0.30, -0.12, 0.44), (0.24, -0.26, 0.52)),            # left wrist: out to the side, then in front
    'path_r': ((-0.30, -0.16, 0.37), (-0.14, -0.38, 0.46), (0.02, -0.40, 0.60)),   # right mitten: hip, belly, forearm
    'lift_r': (-0.03, -0.07, 0.07),  # lowering: the right mitten first lifts off the band (up and forward)
}
MITTEN_HALF = 0.047          # half thickness of the mitten (palm to back), measured on Bean_Hand_r


# ------------------------------------------------------------------------------------------------- easing
def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def keyed(f, keys, ease=smooth):
    if f <= keys[0][0]:
        return _val(keys[0][1])
    for (f0, v0), (f1, v1) in zip(keys, keys[1:]):
        if f <= f1:
            t = ease((f - f0) / max(f1 - f0, 1e-6))
            a, b = _val(v0), _val(v1)
            return a + (b - a) * t
    return _val(keys[-1][1])


def _val(v):
    if isinstance(v, (int, float)):
        return float(v)
    return Vector(v)


def trunk(rig, local, pelvis_loc=(0, 0, 0), pelvis=(), spine1=(), spine2=(), head=(), hat=()):
    local['pelvis'] = (rig.armature_loc('pelvis', pelvis_loc), rig.armature_rot('pelvis', list(pelvis)))
    local['spine_01'] = (Vector(), rig.armature_rot('spine_01', list(spine1)))
    local['spine_02'] = (Vector(), rig.armature_rot('spine_02', list(spine2)))
    local['head'] = (Vector(), rig.armature_rot('head', list(head)))
    local['hat'] = (Vector(), rig.armature_rot('hat', list(hat)))


class Clip:
    def __init__(self, name, seconds, loop, summary, target):
        self.name = name
        self.frames = int(round(seconds * FPS))
        self.loop = loop
        self.summary = summary
        self.target = target
        self.events = []
        self.props = {}
        self.qa = {'max_stretch': 0.0, 'max_stretch_l': 0.0, 'max_stretch_r': 0.0, 'max_hand_error_m': 0.0,
                   'max_leg_error_m': 0.0}

    @property
    def seconds(self):
        return self.frames / FPS

    def event(self, name, frame, **kw):
        self.events.append(dict(name=name, frame=frame, time_s=round(frame / FPS, 3), **kw))

    def note(self, arm_results=(), leg_errors=()):
        for side, (err, st) in arm_results:
            self.qa['max_stretch'] = max(self.qa['max_stretch'], round(st, 3))
            self.qa[f'max_stretch_{side}'] = max(self.qa[f'max_stretch_{side}'], round(st, 3))
            self.qa['max_hand_error_m'] = max(self.qa['max_hand_error_m'], round(err, 4))
        for e in leg_errors:
            self.qa['max_leg_error_m'] = max(self.qa['max_leg_error_m'], round(e, 4))


# ------------------------------------------------------------------------------------------------- helpers
def bez(p0, p1, p2, t):
    """Quadratic Bezier: the raise and the lowering go round the front of the belly instead of through it"""
    t = max(0.0, min(1.0, t))
    return p0 * (1 - t) ** 2 + p1 * (2 * t * (1 - t)) + p2 * (t * t)


def bez3(p0, p1, p2, p3, t):
    t = max(0.0, min(1.0, t))
    u = 1 - t
    return p0 * u ** 3 + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * t ** 3


def spline(points, t):
    """Uniform Catmull-Rom through points (Vectors), t in 0..1 over the whole path (ends clamped)"""
    t = max(0.0, min(1.0, t))
    n = len(points) - 1
    x = t * n
    i = min(int(x), n - 1)
    u = x - i
    p0 = points[max(i - 1, 0)]
    p1, p2 = points[i], points[i + 1]
    p3 = points[min(i + 2, n)]
    return 0.5 * ((2 * p1) + (-p0 + p2) * u + (2 * p0 - 5 * p1 + 4 * p2 - p3) * u * u + (-p0 + 3 * p1 - 3 * p2 + p3) * u ** 3)


def loop_basis(F):
    """Forearm frame F (4x4, band frame posed: X = forearm axis) -> centre, axis, top, outer (unit vectors)"""
    c = F.translation.copy()
    a = (F.to_3x3() @ X).normalized()
    top = Z - a * Z.dot(a)
    top.normalize()
    outer = a.cross(top)
    away = Vector((c.x, c.y, 0.0))
    if outer.dot(away) < 0:
        outer = -outer
    return c, a, top, outer


def loop_point(c, a, top, outer, phi_deg, radius):
    p = math.radians(phi_deg)
    rad = top * math.cos(p) + outer * math.sin(p)
    return c + rad * radius, rad


def mitten_rotation(rig, rad, tangent, a):
    """Right mitten: palm toward the forearm axis (-rad), fingers along the motion (tangent), thumb side along the axis"""
    fingers = tangent - rad * tangent.dot(rad)
    if fingers.length < 1e-6:
        fingers = a.cross(rad)
    fingers.normalize()
    thumb = rad.cross(fingers)          # in the tangent plane, along +/- the forearm axis
    if thumb.dot(-a) < 0:
        thumb = -thumb
    return rig.hand_rotation('r', rig.hand_frame(fingers, thumb))


def rest_rotation(rig, mats, side):
    return mats[f'hand_{side}'].to_3x3() @ rig.rest[f'hand_{side}'].to_3x3().inverted()


# ------------------------------------------------------------------------------------------------- the clip
def bandage_apply(rig, band_frame_rest, band_outer_radius, roll_rel, pose=None):
    """band_frame_rest: the band frame (4x4) at rest, armature space (models.forearm_frame); band_outer_radius: the
    outside of the three turns (m, rest); roll_rel: SM_Bandage_Roll relative to hand_r (4x4)"""
    P = dict(POSE, **(pose or {}))
    rig.shoulder_offset = {}
    clip = Clip('A_Bean_Bandage_Apply', T['end'] / FPS, False,
                "left forearm up in front of the chest, the right mitten winds the band three times (over the top and "
                "down the outer side), pulls it tight, pats it; back to rest", 'player (BP_IslanderBean)')
    n = clip.frames
    f_up = T['up']
    f_loops_end = f_up + T['turns'] * T['turn_frames']
    f_tight_end = f_loops_end + T['tight']
    f_pat_end = f_tight_end + T['pat']
    f_done = T['done']
    rest_mats = rig.fk({})
    rest_grip = {s: rig.grip_of(rest_mats, s) for s in 'lr'}
    rest_wrist = {s: rig.carried(rest_mats, f'hand_{s}') @ rig.bones[f'hand_{s}'].head_local for s in 'lr'}
    band_in_lower = rig.rest['lowerarm_l'].inverted() @ band_frame_rest
    samples = []
    loop_phis = []
    hand_track = []
    a0, a1 = P['arc']
    for f in range(n + 1):
        # ---------------- trunk: lean, twist, head toward the forearm
        w_body = keyed(f, [(0, 0.0), (f_up, 1.0), (f_done, 1.0), (n, 0.0)])
        local = {}
        lean, tw = P['lean'] * w_body, P['twist'] * w_body
        trunk(rig, local, (0.0, -0.004 * w_body, -0.006 * w_body), pelvis=[(Z, tw * 0.3)],
              spine1=[(X, lean * 0.5), (Z, tw * 0.35)], spine2=[(X, lean * 0.5), (Z, tw * 0.35)],
              head=[(X, P['nod'] * w_body), (Z, P['look'] * w_body)], hat=[])
        mats = rig.fk(local)
        legs = [rig.solve_leg(mats, s, rig.ankle_rest(s), foot_yaw=4.0 * w_body * (1 if s == 'l' else -1)) for s in 'lr']
        rig.shoulder_offset = {s: Vector((-sg * P['shoulder_in'], -P['shoulder_fwd'], P['shoulder_up'])) * w_body
                               for s, sg in (('l', 1.0), ('r', -1.0))}
        # ---------------- left arm: up to the wrap pose, small counter roll per turn, back down
        w_l = keyed(f, [(0, 0.0), (f_up - 1, 1.0), (f_done + 3, 1.0), (n, 0.0)])
        wrap_w = Vector(P['wrist_l'])
        wrist_l = spline([rest_wrist['l']] + [Vector(v) for v in P['path_l']] + [wrap_w], w_l)
        if f_up <= f < f_loops_end:
            u = ((f - f_up) % T['turn_frames']) / T['turn_frames']
            wrist_l = wrist_l + Vector((0.0, -0.006 * math.sin(TAU * u), 0.005 * math.sin(TAU * u + 0.6)))
        # the left mitten follows the forearm (no wrist bend: the cuff, skinned a third to hand_l, stays round under the
        # band), rolled a little about the forearm during each turn
        res_l = rig.solve_arm(mats, 'l', wrist_l, None, pole=P['pole_l'], max_stretch=P['max_stretch'])
        if f_up <= f < f_loops_end:
            u = ((f - f_up) % T['turn_frames']) / T['turn_frames']
            fore = (rig.carried(mats, 'hand_l') @ rig.bones['hand_l'].head_local -
                    rig.carried(mats, 'lowerarm_l') @ rig.bones['lowerarm_l'].head_local).normalized()
            Rl = Quaternion(fore, math.radians(P['roll_l'] * math.sin(TAU * u))).to_matrix() @ rest_rotation(rig, mats, 'l')
            res_l = rig.solve_arm(mats, 'l', wrist_l, Rl, pole=P['pole_l'], max_stretch=P['max_stretch'])
        # band frame carried by the posed forearm
        F = mats['lowerarm_l'] @ band_in_lower
        c, a, top, outer = loop_basis(F)
        stretch_l = res_l[1]
        r_touch = band_outer_radius * stretch_l + MITTEN_HALF + P['r_touch_extra']
        # ---------------- right mitten
        if f < f_up:
            # from rest to the top of the forearm (slightly on the near side: arc start)
            g_top, rad_top = loop_point(c, a, top, outer, a0, r_touch)
            w = keyed(f, [(0, 0.0), (f_up, 1.0)])
            g = spline([rest_grip['r']] + [Vector(v) for v in P['path_r']] + [g_top], w)
            tangent = (outer * math.cos(math.radians(a0)) - top * math.sin(math.radians(a0)))
            R_target = mitten_rotation(rig, rad_top, tangent, a)
            Rr = Quaternion().slerp(R_target.to_quaternion(), smooth(w * 1.3)).to_matrix()
            phi = None
        elif f < f_loops_end:
            u = ((f - f_up) % T['turn_frames']) / T['turn_frames']
            mid, amp = (a0 + a1) / 2, (a1 - a0) / 2
            phi = mid - amp * math.cos(TAU * u)
            back = max(0.0, -math.sin(TAU * u))
            radius = r_touch + P['r_out'] * back * back
            g, rad = loop_point(c, a, top, outer, phi, radius)
            # fingers (and the roll at their tips) always toward the outer side, also on the way back, and no further down
            # than 100 deg: never toward the chest or the belly
            pt = math.radians(min(phi, 100.0))
            tangent = (outer * math.cos(pt) - top * math.sin(pt))
            Rr = mitten_rotation(rig, rad, tangent, a)
        elif f < f_tight_end:
            u = (f - f_loops_end) / T['tight']
            tug = math.sin(math.pi * u)
            phi = a0 + (90.0 - a0) * smooth(u * 2)
            g, rad = loop_point(c, a, top, outer, phi, r_touch + 0.035 * tug)
            g = g + a * 0.012 * tug
            tangent = outer * math.cos(math.radians(phi)) - top * math.sin(math.radians(phi))
            Rr = mitten_rotation(rig, rad, tangent, a)
        elif f < f_done:
            u = (f - f_tight_end) / max(f_done - f_tight_end, 1)
            phi = 0.0
            lift = 0.03 * abs(math.sin(2 * math.pi * min(u, 1.0)))
            g, rad = loop_point(c, a, top, outer, 0.0, r_touch + lift)
            tangent = outer
            Rr = mitten_rotation(rig, rad, tangent, a)
        else:
            w = keyed(f, [(f_done, 0.0), (n, 1.0)])
            g_top, rad_top = loop_point(c, a, top, outer, 0.0, r_touch)
            Rt = mitten_rotation(rig, rad_top, outer, a)
            pts = [g_top, g_top + Vector(P['lift_r'])] + [Vector(v) for v in reversed(P['path_r'])] + [rest_grip['r']]
            g = spline(pts, w)
            Rr = Rt.to_quaternion().slerp(Quaternion(), w).to_matrix()
        wrist_r = rig.wrist_for_grip('r', g, Rr)
        res_r = rig.solve_arm(mats, 'r', wrist_r, Rr, pole=P['pole_r'], max_stretch=P['max_stretch'])
        clip.note([('l', res_l), ('r', res_r)], legs)
        samples.append(mats)
        loop_phis.append(phi)
        hand_track.append({'grip': rig.grip_of(mats, 'r'), 'centre': c, 'axis': a, 'top': top, 'outer': outer,
                           'r_touch': r_touch})
        clip.props.setdefault('SM_Bandage_Roll', []).append((f < f_done, mats['hand_r'] @ roll_rel))
        clip.props.setdefault('SM_Bandage_Wrapped', []).append((f >= f_up, F))
    rig.shoulder_offset = {}
    clip.event('WrapStart', f_up)
    clip.event('BandageDone', f_done)
    clip.turns = [{'turn': k + 1, 'start_s': round((f_up + k * T['turn_frames']) / FPS, 3),
                   'end_s': round((f_up + (k + 1) * T['turn_frames']) / FPS, 3)} for k in range(T['turns'])]
    clip.phases = {'raise_s': [0.0, round(f_up / FPS, 3)], 'turns_s': [round(f_up / FPS, 3), round(f_loops_end / FPS, 3)],
                   'tighten_s': [round(f_loops_end / FPS, 3), round(f_tight_end / FPS, 3)],
                   'pats_s': [round(f_tight_end / FPS, 3), round(f_done / FPS, 3)],
                   'lower_s': [round(f_done / FPS, 3), round(n / FPS, 3)]}
    # measured arc of the mitten around the forearm (in the forearm frame, from the grip actually reached)
    angles = []
    for h in hand_track[f_up:f_loops_end]:
        d = h['grip'] - h['centre']
        d = d - h['axis'] * d.dot(h['axis'])
        ang = math.degrees(math.atan2(d.dot(h['outer']), d.dot(h['top'])))
        angles.append((ang - 90.0 + 180.0) % 360.0 - 180.0 + 90.0)       # unwrapped around the outer side (90)
    clip.qa['loop_arc_deg'] = [round(min(angles), 1), round(max(angles), 1)] if angles else None
    clip.qa['loop_arc_span_deg'] = round(max(angles) - min(angles), 1) if angles else None
    clip.hand_track = hand_track
    clip.pose = P
    return clip, samples
