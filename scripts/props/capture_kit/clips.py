"""The capture clips (T-337) on SKEL_Bean, 30 fps, root fixed (no root motion), frames 0..N (loops: frame N = frame 0).

  A_Bean_Cage_Place      1.5 s  crouch, the folded cage set down in front, both hands pull it open, stand up
                                events CrateDown (the crate leaves the hand, lies on the ground), CagePlaced (the cage
                                stands unfolded: the game swaps the crate for SM_Capture_Cage)
  A_Bean_Cuff            1.2 s  lean forward, both mittens on the captive's wrists (captive aligned in front, see PAIR),
                                squeeze: event CuffsClosed; release and straighten up
  A_Bean_Lead_Walk       1.6 s  loop, the library's walk (A_Bean_Walk: same formula, 49 cm/s without sliding), the left
                                mitten holding the leash behind the hip
  A_BeanFR_Captive_Idle  3.0 s  loop, hunched, head down, wrists bound in front, weight shifting from foot to foot
  A_BeanFR_Captive_Walk  1.6 s  loop, short constrained steps (31 cm/s without sliding), hunched, wrists bound in front

The captive's wrists keep ONE rigid relation in every captive frame (the pair of bracelets is carried by the chest,
spine_02): SM_Steel_Handcuffs, built from that relation, fits both wrists once attached to hand_r.
"""
from __future__ import annotations

import math

from mathutils import Matrix, Quaternion, Vector

FPS = 30
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
TAU = 2 * math.pi

# ------------------------------------------------------------------------------------------------- tuning
CAPTIVE = {
    # bracelet centres (captive armature space, reference pose) and the hands' directions / thumb sides
    'cuff_centre': {'l': (0.095, -0.305, 0.47), 'r': (-0.095, -0.305, 0.47)},
    'hand_dir': {'l': (-0.30, -0.55, -0.78), 'r': (0.30, -0.55, -0.78)},
    'thumb': {'l': (-0.15, -0.55, 0.55), 'r': (0.15, -0.55, 0.55)},
    'hunch': (6.0, 7.0),        # spine_01, spine_02 forward (deg)
    'head_down': 12.0,
    'pole': {'l': (1.0, 0.35, -0.55), 'r': (-1.0, 0.35, -0.55)},
}
CUFF_ALONG = 0.031              # bracelet centre from the hand head, along the hand bone (models.CUFF['along'])
PAIR = {'distance': 0.84}       # A_Bean_Cuff: captive root this far in front of the player, facing him
CRATE_DOWN = {'centre': (-0.04, -0.40, 0.0), 'yaw_deg': 0.0}   # the crate set on the ground (bean armature space)
CAGE_SPAWN = {'centre': (0.0, -1.50, 0.0), 'yaw_deg': 90.0}    # cage base centre (DA_CaptureRules.PlaceDistanceCm 150); door (+X) faces the bean
WALK = {'period': 0.8, 'swing': 28.0, 'knee': 30.0, 'bob': 0.012, 'lean': 4.0, 'arm': 22.0}
CAPTIVE_WALK = {'period': 0.8, 'swing': 17.0, 'knee': 26.0, 'bob': 0.009, 'lean': 4.0}


# ------------------------------------------------------------------------------------------------- easing
def smooth(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3 - 2 * t)


def keyed(f, keys, ease=smooth):
    """Value at frame f through keys [(frame, value)] (floats, tuples or Vectors), eased between keys"""
    if f <= keys[0][0]:
        return _val(keys[0][1])
    for (f0, v0), (f1, v1) in zip(keys, keys[1:]):
        if f <= f1:
            t = ease((f - f0) / max(f1 - f0, 1e-6))
            a, b = _val(v0), _val(v1)
            return a + (b - a) * t if not isinstance(a, float) else a + (b - a) * t
    return _val(keys[-1][1])


def _val(v):
    if isinstance(v, (int, float)):
        return float(v)
    return Vector(v)


def sinp(phase, cycles=1.0, shift=0.0):
    return math.sin(TAU * (phase * cycles + shift))


# ------------------------------------------------------------------------------------------------- body layers
def trunk(rig, local, pelvis_loc=(0, 0, 0), pelvis=(), spine1=(), spine2=(), head=(), hat=()):
    local['pelvis'] = (rig.armature_loc('pelvis', pelvis_loc), rig.armature_rot('pelvis', list(pelvis)))
    local['spine_01'] = (Vector(), rig.armature_rot('spine_01', list(spine1)))
    local['spine_02'] = (Vector(), rig.armature_rot('spine_02', list(spine2)))
    local['head'] = (Vector(), rig.armature_rot('head', list(head)))
    local['hat'] = (Vector(), rig.armature_rot('hat', list(hat)))


def walk_layer(rig, local, t, p, arms=True):
    """The library's walk (bean_anims Walk / Run formula), t in seconds; arms optional"""
    period = p['period']
    c = math.sin(TAU * t / period)
    lift_l = max(0.0, -math.sin(TAU * (t / period + 0.25)))
    lift_r = max(0.0, math.sin(TAU * (t / period + 0.25)))
    sw, kn, lean = p['swing'], p['knee'], p['lean']
    local['thigh_l'] = (Vector(), rig.armature_rot('thigh_l', [(X, -sw * c)]))
    local['calf_l'] = (Vector(), rig.armature_rot('calf_l', [(X, kn * lift_l)]))
    local['foot_l'] = (Vector(), rig.armature_rot('foot_l', [(X, -10 * c)]))
    local['thigh_r'] = (Vector(), rig.armature_rot('thigh_r', [(X, sw * c)]))
    local['calf_r'] = (Vector(), rig.armature_rot('calf_r', [(X, kn * lift_r)]))
    local['foot_r'] = (Vector(), rig.armature_rot('foot_r', [(X, 10 * c)]))
    bob = -p['bob'] * abs(math.cos(math.pi * t / period)) + p['bob'] * 0.5
    trunk(rig, local, (0, 0, bob), pelvis=[(Y, 5 * c), (Z, 6 * c)], spine1=[(X, lean * 0.5), (Z, -5 * c)],
          spine2=[(X, lean * 0.5), (Y, -3 * c)], head=[(X, -lean * 0.6), (Y, 2 * c)], hat=[(Y, -2.5 * c)])
    for side in ('l', 'r'):
        local[f'coattail_{side}'] = (Vector(), rig.armature_rot(f'coattail_{side}', [(X, 8 * abs(c))]))
    if arms:
        a = p.get('arm', 22.0)
        local['upperarm_l'] = (Vector(), rig.armature_rot('upperarm_l', [(X, a * c), (Y, -6)]))
        local['lowerarm_l'] = (Vector(), rig.armature_rot('lowerarm_l', [(X, -(10 + 8 * max(0.0, -c)))]))
        local['upperarm_r'] = (Vector(), rig.armature_rot('upperarm_r', [(X, -a * c), (Y, 6)]))
        local['lowerarm_r'] = (Vector(), rig.armature_rot('lowerarm_r', [(X, -(10 + 8 * max(0.0, c)))]))
    return c


def no_slide_speed(p):
    return 2 * (2 * 0.21 * math.sin(math.radians(p['swing']))) / p['period']


def planted_legs(rig, mats, stance=0.0, yaw=0.0):
    """Both feet on their rest spots (optionally wider by stance), knees forward"""
    for side, s in (('l', 1), ('r', -1)):
        a = rig.ankle_rest(side) + Vector((s * stance, 0, 0))
        rig.solve_leg(mats, side, a, foot_yaw=s * yaw)


# ------------------------------------------------------------------------------------------------- captive pose
class BoundHands:
    """The captive's wrists: two bracelet frames rigid with the chest (spine_02) of the reference pose"""

    def __init__(self, rig):
        self.rig = rig
        local = {}
        captive_trunk(rig, local, 0.0, 0.0, 0.0)
        mats = rig.fk(local)
        self.ref_chest = rig.carried(mats, 'spine_02')
        self.rot = {}
        self.cuff = {}
        for side in ('l', 'r'):
            frame = rig.hand_frame(CAPTIVE['hand_dir'][side], CAPTIVE['thumb'][side])
            self.rot[side] = rig.hand_rotation(side, frame)
            y = frame @ Vector((0, 1, 0))
            B = frame.to_4x4()
            B.translation = Vector(CAPTIVE['cuff_centre'][side])
            self.cuff[side] = B                      # bracelet frame: Y = ring axis (the hand bone)
        self.ref_mats = mats

    def wrist_frames(self, mats, wobble=None):
        """Bracelet frames carried by the chest of mats (and a small wobble rotation about the shoulders' midpoint)"""
        D = self.rig.carried(mats, 'spine_02') @ self.ref_chest.inverted()
        if wobble is not None:
            sh = (self.rig.shoulder(mats, 'l') + self.rig.shoulder(mats, 'r')) / 2
            D = Matrix.Translation(sh) @ wobble.to_4x4() @ Matrix.Translation(-sh) @ D
        return {side: D @ self.cuff[side] for side in ('l', 'r')}, D

    def solve(self, mats, wobble=None, max_stretch=1.3):
        frames, D = self.wrist_frames(mats, wobble)
        out = {}
        for side in ('l', 'r'):
            F = frames[side]
            R = D.to_3x3() @ self.rot[side]
            axis = F.to_3x3() @ Vector((0, 1, 0))
            wrist = F.translation - axis * CUFF_ALONG
            out[side] = self.rig.solve_arm(mats, side, wrist, R, pole=CAPTIVE['pole'][side], max_stretch=max_stretch)
        return out, frames


def captive_trunk(rig, local, sway, breath, look, walk_lean=0.0):
    h1, h2 = CAPTIVE['hunch']
    trunk(rig, local, (0.011 * sway, 0.0, -0.012 + 0.002 * breath),
          pelvis=[(Y, -2.2 * sway), (Z, 2.5 * sway)],
          spine1=[(X, h1 + 0.6 * breath + walk_lean), (Y, 1.4 * sway)],
          spine2=[(X, h2 + 0.5 * breath), (Y, 0.8 * sway)],
          head=[(X, CAPTIVE['head_down'] - 1.2 * breath), (Z, look)])


# ------------------------------------------------------------------------------------------------- clips
class Clip:
    def __init__(self, name, seconds, loop, summary, target):
        self.name = name
        self.frames = int(round(seconds * FPS))
        self.loop = loop
        self.summary = summary
        self.target = target
        self.events = []
        self.props = {}          # name -> list of (visible, 4x4 armature space) per frame (previews and manifest)
        self.qa = {'max_stretch': 0.0, 'max_hand_error_m': 0.0, 'max_leg_error_m': 0.0}

    @property
    def seconds(self):
        return self.frames / FPS

    def event(self, name, frame, **kw):
        self.events.append(dict(name=name, frame=frame, time_s=round(frame / FPS, 3), **kw))

    def note(self, arm_results=(), leg_errors=()):
        frame = len(self.qa.setdefault('per_frame', []))
        worst = 0.0
        for err, st in arm_results:
            self.qa['max_stretch'] = max(self.qa['max_stretch'], round(st, 3))
            if err > self.qa['max_hand_error_m']:
                self.qa['worst_hand_frame'] = frame
            self.qa['max_hand_error_m'] = max(self.qa['max_hand_error_m'], round(err, 4))
            worst = max(worst, err)
        for e in leg_errors:
            self.qa['max_leg_error_m'] = max(self.qa['max_leg_error_m'], round(e, 4))
        self.qa['per_frame'].append(round(worst, 3))


def captive_idle(rig, bound):
    clip = Clip('A_BeanFR_Captive_Idle', 3.0, True, 'hunched, head down, wrists bound in front, weight shifting',
                'French captive (BP_BeanFR_*)')
    samples = []
    n = clip.frames
    for f in range(n + 1):
        ph = (f % n) / n
        sway = sinp(ph)
        breath = sinp(ph, 2, 0.1)
        look = 8.0 * sinp(ph, 1, 0.3) * (0.6 + 0.4 * sinp(ph, 2))
        local = {}
        captive_trunk(rig, local, sway, breath, look)
        for side in ('l', 'r'):
            local[f'coattail_{side}'] = (Vector(), rig.armature_rot(f'coattail_{side}', [(X, 2.0 * sway * (1 if side == 'l' else -1))]))
        mats = rig.fk(local)
        legs = [rig.solve_leg(mats, s, rig.ankle_rest(s), foot_yaw=4.0 * (1 if s == 'l' else -1)) for s in ('l', 'r')]
        wob = Quaternion(X, math.radians(2.0 * sinp(ph, 2, 0.35))) @ Quaternion(Z, math.radians(1.5 * sway))
        arms, _ = bound.solve(mats, wob.to_matrix())
        clip.note(arms.values(), legs)
        samples.append(mats)
    return clip, samples


def captive_walk(rig, bound):
    p = CAPTIVE_WALK
    clip = Clip('A_BeanFR_Captive_Walk', 2 * p['period'], True, 'short constrained steps, hunched, wrists bound in front',
                'French captive (BP_BeanFR_*)')
    clip.speed_cm_s = round(no_slide_speed(p) * 100, 1)
    samples = []
    n = clip.frames
    for f in range(n + 1):
        t = (f % n) / FPS
        local = {}
        c = walk_layer(rig, local, t, p, arms=False)
        # hunched captive over the walk's trunk: forward bend, head down
        h1, h2 = CAPTIVE['hunch']
        for name, extra in (('spine_01', h1), ('spine_02', h2), ('head', CAPTIVE['head_down'] - 2.0)):
            loc, q = local[name]
            local[name] = (loc, q @ rig.armature_rot(name, [(X, extra)]))
        mats = rig.fk(local)
        wob = Quaternion(X, math.radians(3.0 * abs(c) - 1.5)) @ Quaternion(Z, math.radians(-2.0 * c))
        arms, _ = bound.solve(mats, wob.to_matrix())
        clip.note(arms.values())
        samples.append(mats)
    for step in range(4):
        clip.event('Footstep_' + ('L' if step % 2 == 0 else 'R'), int(round(step * p['period'] / 2 * FPS)))
    return clip, samples


def lead_walk(rig):
    p = WALK
    clip = Clip('A_Bean_Lead_Walk', 2 * p['period'], True, "the library's walk, the left mitten holding the leash behind",
                'player (BP_IslanderBean)')
    clip.speed_cm_s = round(no_slide_speed(p) * 100, 1)
    samples = []
    n = clip.frames
    hand_rot = None
    for f in range(n + 1):
        t = (f % n) / FPS
        local = {}
        c = walk_layer(rig, local, t, p, arms=True)
        mats = rig.fk(local)
        frame = rig.hand_frame((0.12, 0.45, -0.88), (0.25, -0.6, 0.6))
        R = rig.carried(mats, 'pelvis').to_3x3() @ rig.hand_rotation('l', frame)
        grip = rig.carried(mats, 'pelvis') @ Vector((0.245, 0.135, 0.355 + 0.012 * c))
        wrist = rig.wrist_for_grip('l', grip, R)
        res = rig.solve_arm(mats, 'l', wrist, R, pole=(1.0, 0.3, -0.2))
        clip.note([res])
        samples.append(mats)
        hand_rot = R
    for step in range(4):
        clip.event('Footstep_' + ('L' if step % 2 == 0 else 'R'), int(round(step * p['period'] / 2 * FPS)))
    clip.leash_hand = 'hand_l'
    return clip, samples


def captive_in_player_space(bound):
    """Bracelet frames of the captive standing PAIR['distance'] in front of the player, facing him (player armature space)"""
    D = PAIR['distance']
    T = Matrix.Translation((0, -D, 0)) @ Matrix.Rotation(math.pi, 4, 'Z')
    frames, _ = bound.wrist_frames(bound.ref_mats)
    return {side: T @ F for side, F in frames.items()}, T


def cuff(rig, bound, cuffs_open_rel):
    """A_Bean_Cuff: the right mitten (open cuffs in it) on the captive's left wrist, the left mitten on its right wrist"""
    clip = Clip('A_Bean_Cuff', 1.2, False, "lean forward, both mittens on the captive's wrists, click, release",
                'player (BP_IslanderBean)')
    frames_c, T = captive_in_player_space(bound)
    # the captive's left cuff is on the player's right (-X): right mitten there, from the player's side (+Y)
    grab = {'r': frames_c['l'].translation + Vector((0.0, 0.068, 0.014)),
            'l': frames_c['r'].translation + Vector((0.0, 0.068, 0.014))}
    near = {s: g + Vector((0.0, 0.05, 0.03)) for s, g in grab.items()}
    squeeze = {s: g + Vector((0.0, -0.016, -0.006)) for s, g in grab.items()}
    n = clip.frames
    F_GRAB, F_CLICK, F_REL = 12, 21, 26
    samples = []
    rest_local = {}
    rest_mats = rig.fk(rest_local)
    rest_grip = {s: rig.grip_of(rest_mats, s) for s in ('l', 'r')}
    rest_rot = {s: Matrix.Identity(3) for s in ('l', 'r')}
    grab_frame = {'r': rig.hand_frame((0.05, -0.78, -0.62), (0.55, -0.2, 0.8)),
                  'l': rig.hand_frame((-0.05, -0.78, -0.62), (-0.55, -0.2, 0.8))}
    grab_rot = {s: rig.hand_rotation(s, grab_frame[s]) for s in ('l', 'r')}
    for f in range(n + 1):
        lean = keyed(f, [(0, 0.0), (F_GRAB, 1.0), (F_CLICK + 2, 1.0), (n - 4, 0.0)])
        local = {}
        trunk(rig, local, (0.0, -0.02 * lean, -0.026 * lean), pelvis=[(X, 6 * lean)],
              spine1=[(X, 12 * lean)], spine2=[(X, 9 * lean)], head=[(X, -30 * lean)])
        mats = rig.fk(local)
        legs = [rig.solve_leg(mats, s, rig.ankle_rest(s), foot_yaw=6.0 * (1 if s == 'l' else -1)) for s in ('l', 'r')]
        arms = []
        for side in ('r', 'l'):
            delay = 0 if side == 'r' else 2
            g = keyed(f, [(0, rest_grip[side]), (F_GRAB - 3 + delay, near[side]), (F_GRAB + 3 + delay, grab[side]),
                          (F_CLICK - 1, grab[side]), (F_CLICK, squeeze[side]), (F_CLICK + 2, grab[side]),
                          (F_REL, near[side]), (n, rest_grip[side])])
            w = keyed(f, [(0, 0.0), (F_GRAB - 2, 1.0), (F_REL + 1, 1.0), (n, 0.0)])
            R = Quaternion().slerp(grab_rot[side].to_quaternion(), w).to_matrix()
            wrist = rig.wrist_for_grip(side, g, R)
            arms.append(rig.solve_arm(mats, side, wrist, R, pole=((-1.0 if side == 'r' else 1.0), 0.2, -0.8), max_stretch=1.3))
        clip.note(arms, legs)
        samples.append(mats)
        # props: the open cuffs in the right mitten until the click, the closed cuffs on the captive afterwards
        hand = mats['hand_r']
        clip.props.setdefault('SM_Steel_Handcuffs_Open', []).append((f < F_CLICK, hand @ cuffs_open_rel))
    clip.event('GrabWrists', F_GRAB + 3)
    clip.event('CuffsClosed', F_CLICK)
    clip.pair = {'captive_root': T, 'grab': {s: list(v) for s, v in grab.items()}}
    return clip, samples


def cage_place(rig, crate_rel):
    """A_Bean_Cage_Place: the folded cage (SM_Capture_Cage_Folded, held by its handle in the right mitten, crate_rel = its
    transform relative to hand_r) set down in front, pulled open with both mittens, the cage stands (CagePlaced)"""
    clip = Clip('A_Bean_Cage_Place', 1.5, False, 'crouch, set the folded cage down in front, pull it open, stand up',
                'player (BP_IslanderBean)')
    n = clip.frames
    F_DOWN, F_GRAB2, F_OPEN, F_UP = 12, 17, 28, 41
    c = Vector(CRATE_DOWN['centre'])
    down = Matrix.Translation(c) @ Matrix.Rotation(math.radians(CRATE_DOWN['yaw_deg']), 4, 'Z')
    # crate: long axis (+X) along the bean's left-right once down; its handle grip 0.25 m above the ground
    grip_down = down @ Vector((0.0, 0.0, 0.2502))
    ends = {'r': down @ Vector((-0.20, 0.0, 0.215)), 'l': down @ Vector((0.20, 0.0, 0.215))}
    spread = {'r': down @ Vector((-0.30, -0.03, 0.36)), 'l': down @ Vector((0.30, -0.03, 0.36))}
    rest_mats = rig.fk({})
    rest_grip = {s: rig.grip_of(rest_mats, s) for s in ('l', 'r')}
    # the right mitten's turn that carries the crate long axis forward at the side, then sideways in front
    carry_frame = rig.hand_frame((0.0, 0.0, -1.0), (0.0, -1.0, 0.0))
    put_frame = rig.hand_frame((0.0, -0.25, -0.97), (1.0, 0.0, 0.0))
    carry_rot = rig.hand_rotation('r', carry_frame)
    put_rot = rig.hand_rotation('r', put_frame)
    pull_frame = {'r': rig.hand_frame((-0.2, -0.45, -0.87), (0.9, 0.0, 0.3)),
                  'l': rig.hand_frame((0.2, -0.45, -0.87), (-0.9, 0.0, 0.3))}
    pull_rot = {s: rig.hand_rotation(s, pull_frame[s]) for s in ('l', 'r')}
    samples = []
    held = None
    for f in range(n + 1):
        crouch = keyed(f, [(0, 0.0), (F_DOWN - 2, 1.0), (F_OPEN - 3, 0.85), (F_OPEN + 2, 0.7), (F_UP, 0.0)])
        local = {}
        trunk(rig, local, (0.0, 0.03 * crouch, -0.13 * crouch), pelvis=[(X, 12 * crouch)],
              spine1=[(X, 22 * crouch)], spine2=[(X, 16 * crouch)], head=[(X, -10 * crouch)])
        for side in ('l', 'r'):
            local[f'coattail_{side}'] = (Vector(), rig.armature_rot(f'coattail_{side}', [(X, -14 * crouch)]))
        mats = rig.fk(local)
        legs = [rig.solve_leg(mats, s, rig.ankle_rest(s) + Vector(((0.03 if s == 'l' else -0.03) * crouch, 0, 0)),
                              foot_yaw=10.0 * crouch * (1 if s == 'l' else -1)) for s in ('l', 'r')]
        arms = []
        # right mitten: carries the crate down (F_DOWN), stays on the handle, then the right end, the spread, rest
        if f <= F_DOWN:
            g = keyed(f, [(0, rest_grip['r'] + Vector((0.0, -0.01, 0.0))), (F_DOWN, grip_down)])
            w = keyed(f, [(0, 0.0), (F_DOWN, 1.0)])
            R = carry_rot.to_quaternion().slerp(put_rot.to_quaternion(), w).to_matrix()
        else:
            g = keyed(f, [(F_DOWN, grip_down), (F_DOWN + 2, grip_down + Vector((0, 0.02, 0.03))), (F_GRAB2, ends['r']),
                          (F_OPEN - 4, ends['r'] + Vector((0, 0, 0.02))), (F_OPEN, spread['r']), (F_UP, rest_grip['r'])])
            w = keyed(f, [(F_DOWN + 1, 0.0), (F_GRAB2, 1.0), (F_OPEN + 2, 1.0), (F_UP, 0.0)])
            Ra = put_rot.to_quaternion().slerp(pull_rot['r'].to_quaternion(), min(1.0, w * 2)) if f < F_OPEN else \
                Quaternion().slerp(pull_rot['r'].to_quaternion(), w)
            R = Ra.to_matrix()
        wrist = rig.wrist_for_grip('r', g, R)
        arms.append(rig.solve_arm(mats, 'r', wrist, R, pole=(-1.0, 0.3, -0.5), max_stretch=1.3))
        # left mitten: free, joins on the left end, spread, rest
        gl = keyed(f, [(0, rest_grip['l']), (F_DOWN - 3, rest_grip['l'] + Vector((0.0, -0.10, 0.04))),
                       (F_GRAB2 + 1, ends['l']), (F_OPEN - 4, ends['l'] + Vector((0, 0, 0.02))), (F_OPEN, spread['l']),
                       (F_UP + 2, rest_grip['l'])])
        wl = keyed(f, [(0, 0.0), (F_GRAB2 + 1, 1.0), (F_OPEN + 2, 1.0), (F_UP + 2, 0.0)])
        Rl = Quaternion().slerp(pull_rot['l'].to_quaternion(), wl).to_matrix()
        wrist_l = rig.wrist_for_grip('l', gl, Rl)
        arms.append(rig.solve_arm(mats, 'l', wrist_l, Rl, pole=(1.0, 0.3, -0.5), max_stretch=1.3))
        clip.note(arms, legs)
        samples.append(mats)
        if f <= F_DOWN:
            held = mats['hand_r'] @ crate_rel
            clip.props.setdefault('SM_Capture_Cage_Folded', []).append((True, held))
        else:
            clip.props['SM_Capture_Cage_Folded'].append((f < F_OPEN, down))
        cage = Matrix.Translation(CAGE_SPAWN['centre']) @ Matrix.Rotation(math.radians(CAGE_SPAWN['yaw_deg']), 4, 'Z')
        clip.props.setdefault('SM_Capture_Cage', []).append((f >= F_OPEN, cage))
    clip.event('CrateDown', F_DOWN)
    clip.event('CagePlaced', F_OPEN)
    clip.crate_down = down
    clip.cage_spawn = Matrix.Translation(CAGE_SPAWN['centre']) @ Matrix.Rotation(math.radians(CAGE_SPAWN['yaw_deg']), 4, 'Z')
    return clip, samples
