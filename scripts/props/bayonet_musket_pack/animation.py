"""Clips of the bayonet musket (T-334): every musket clip by the musket pack's OWN animation module
(scripts/props/musket_pack/animation.py, loaded as it is, unchanged: same choreography, lengths, events), plus the two
bayonet gestures, installed in that module by install():

  BayonetStab (15 frames, 24 fps)    StabStart 3 (whoosh), StabHit 7 (full extension, socket_bayonet_tip)
      the musket drawn back a little (wind-up, frames 0-3), driven forward along its barrel (3-7, CoD easing with a
      punch at the end), held a beat (7-9), recovered to the hip with a settle (9-15). TP: the torso leans into the
      thrust, the left foot steps in.
  BayonetCharge (24 frames)          ChargeStart 2, ChargeHit 16 (full extension)
      the musket lowered to the charge position (hip level, bayonet forward, frames 0-5), a running stride with the
      marked bob of a charge (5-13), a lunge (13-16) to full extension, recovered (16-24). TP: legs run then lunge
      (FK; the game keeps its own legs and copies the upper body), the torso low and forward.

Both hands stay on the musket (right hand on the wrist, left on the fore-end): the musket's default hand keys.
The weapon pose is given like the musket's (FP: anchor point, place in the view (cm), FRotator in the view; TP: weapon
point, bean space point, barrel direction, roll).
"""
from __future__ import annotations

import importlib.util
import math
import sys
from pathlib import Path

from mathutils import Matrix, Vector

import spec as S

MUSKET = Path(__file__).resolve().parents[1] / 'musket_pack'
if str(MUSKET) not in sys.path:
    sys.path.append(str(MUSKET))          # musket_model, kit, looks (imported by name by the musket's modules)
_spec = importlib.util.spec_from_file_location('musket_animation_bm', MUSKET / 'animation.py')
MA = importlib.util.module_from_spec(_spec)
sys.modules['musket_animation_bm'] = MA
_spec.loader.exec_module(MA)

NEW = dict(S.NEW_CLIPS)
ORIG = {}
CURRENT = {}            # the bean armature of the scene being animated (install() side passes on the TP body)


def check_musket():
    """The musket clips must still be the ones of the contract (spec.MUSKET_CLIPS / MUSKET_EVENTS)."""
    problems = []
    for clip, n in S.MUSKET_CLIPS.items():
        if MA.CLIPS.get(clip) != n:
            problems.append(f'{clip}: {MA.CLIPS.get(clip)} frames, contract {n}')
        got = [(e['frame'], e['name'], e.get('socket')) for e in MA.events(clip)]
        want = [(e[0], e[1], e[2] if len(e) > 2 else None) for e in S.MUSKET_EVENTS.get(clip, [])]
        if got != want:
            problems.append(f'{clip}: events {got} != contract {want}')
    extra = sorted(set(MA.CLIPS) - set(S.MUSKET_CLIPS))
    if extra:
        problems.append(f'musket clips not in the contract: {extra}')
    if problems:
        raise SystemExit('[bayonet] the musket pack changed, update spec.MUSKET_CLIPS / MUSKET_EVENTS: ' + '; '.join(problems))


# --------------------------------------------------------------------------------------------------- poses

FP_NEW = {
    # wind-up: pulled back and down to the right, the point lifted a little
    'stab_wind': (MA.GRIP_R, (24.0, 19.0, -23.0), (9.0, -14.0, -20.0)),
    # full thrust: driven 22 cm forward, still on the diagonal of the hip pose, the point low at the centre of the view
    'stab': (MA.GRIP_R, (46.0, 17.0, -15.0), (5.5, -12.0, -14.0)),
    # charge bayonet: low at the hip, bayonet forward and up toward the centre of the view
    'charge': (MA.GRIP_R, (30.0, 15.0, -27.0), (12.0, -14.0, -12.0)),
    # lunge at the end of the charge: driven forward and a little down
    'lunge': (MA.GRIP_R, (52.0, 16.0, -19.0), (9.0, -11.5, -12.0)),
}


def tp_new():
    at = MA.tp_pose_at
    g = MA.GRIP_R
    return {
        # the bean's arms are short: the musket stays on the diagonal toward its left (as the musket's TP poses), the thrust
        # runs along the barrel; the leaned torso brings the shoulders forward (both hands within reach, see qa)
        'guard': at(g, (-.14, -.24, .50), (.72, -.69, .05), -10),
        'stab_wind': at(g, (-.18, -.16, .49), (.72, -.69, .07), -12),
        'stab': at(g, (-.11, -.29, .50), (.72, -.69, .05), -8),
        'charge': at(g, (-.15, -.22, .45), (.70, -.70, .10), -10),
        'lunge': at(g, (-.09, -.30, .46), (.70, -.71, .05), -6),
    }


def new_pose(name, view):
    if view == 'FP':
        anchor, where, rot = FP_NEW[name]
        return MA.view_pose(anchor, where, rot)
    return tp_new()[name]


def base_pose(view):
    return MA.fp_pose('base') if view == 'FP' else MA.tp_poses()['ready']


def keyed(f, keys, view, over=0.06, antic=0.03):
    """Blend between named poses along (frame, name) keys with the musket's Call of Duty easing."""
    i, w = MA.segment_os(f, keys, over, antic)
    a = keys[i][1]
    b = keys[min(i + 1, len(keys) - 1)][1]
    pa = base_pose(view) if a == 'base' else new_pose(a, view)
    pb = base_pose(view) if b == 'base' else new_pose(b, view)
    return MA.blend(pa, pb, w)


STAB_KEYS = [(0, 'base'), (3, 'stab_wind'), (7, 'stab'), (9, 'stab'), (15, 'base')]
CHARGE_KEYS = [(0, 'base'), (5, 'charge'), (13, 'charge'), (16, 'lunge'), (18, 'lunge'), (24, 'base')]


def weapon_pose(clip, f, view):
    if clip not in NEW:
        return ORIG['weapon_pose'](clip, f, view)
    if clip == 'BayonetStab':
        m = keyed(f, STAB_KEYS, view, over=0.08, antic=0.0)
        # the impact: a short jolt back along the barrel and the point dipping, then the recovery
        hit = MA.curve(f, [(6, 0), (7, 1.0), (8, -0.45), (9.5, 0.15), (11, 0)])
        return m @ Matrix.Translation((-0.012 * hit, 0, -0.004 * hit)) @ Matrix.Rotation(math.radians(1.5 * hit), 4, 'Y')
    m = keyed(f, CHARGE_KEYS, view, over=0.07, antic=0.0)
    # running: a heavy bob and sway of the lowered musket while the bean runs (two strides between 5 and 13)
    run = MA.curve(f, [(3, 0), (6, 1), (12, 1), (14, 0)])
    ph = 2 * math.pi * (f - 5) / 8.0
    bob = Matrix.Translation((0.004 * math.sin(ph) * run, 0.006 * math.cos(ph) * run, 0.012 * math.sin(2 * ph) * run))
    m = m @ bob @ Matrix.Rotation(math.radians(2.5 * math.sin(ph) * run), 4, 'X') @ Matrix.Rotation(math.radians(1.8 * math.sin(2 * ph) * run), 4, 'Y')
    hit = MA.curve(f, [(15, 0), (16, 1.0), (17, -0.4), (19, 0.12), (21, 0)])
    return m @ Matrix.Translation((-0.015 * hit, 0, -0.005 * hit)) @ Matrix.Rotation(math.radians(2.0 * hit), 4, 'Y')


# ------------------------------------------------------------------------------------------- TP body passes

def body_pass(clip, f):
    """TP: spine lean and legs of the bayonet gestures, set on the bean before the arm solve (the arms are solved from
    the leaned shoulders)."""
    arm = CURRENT.get('arm')
    if arm is None:
        return
    lr = MA.local_rot
    if clip == 'BayonetStab':
        lean = MA.curve(f, [(0, 0), (3, -4), (7, 21), (9, 19), (15, 0)])
        lr(arm, 'spine_01', (1, 0, 0), lean * 0.55)
        lr(arm, 'spine_02', (1, 0, 0), lean * 0.45)
        step = MA.curve(f, [(2, 0), (6, 1), (10, 1), (15, 0)])
        lr(arm, 'thigh_l', (1, 0, 0), 16 * step)
        lr(arm, 'calf_l', (1, 0, 0), -10 * step)
        lr(arm, 'foot_l', (1, 0, 0), -6 * step)
        lr(arm, 'thigh_r', (1, 0, 0), -10 * step)
        arm.pose.bones['pelvis'].location = (0, -0.018 * step, -0.012 * step)
        return
    # BayonetCharge: the run (two strides 5..13), the lunge (13..18), the recovery
    run = MA.curve(f, [(2, 0), (5, 1), (12, 1), (14, 0)])
    ph = 2 * math.pi * (f - 5) / 8.0
    lunge = MA.curve(f, [(12, 0), (16, 1), (18, 1), (24, 0)])
    lean = 12 * MA.curve(f, [(0, 0), (5, 1), (13, 1), (24, 0)]) + 14 * lunge
    lr(arm, 'spine_01', (1, 0, 0), lean * 0.55)
    lr(arm, 'spine_02', (1, 0, 0), lean * 0.45)
    for side, off in (('l', 0.0), ('r', math.pi)):
        swing = math.sin(ph + off) * run
        thigh = 30 * swing + (34 if side == 'l' else -24) * lunge
        calf = -max(0.0, -math.sin(ph + off)) * 48 * run - (22 if side == 'l' else 8) * lunge
        lr(arm, 'thigh_' + side, (1, 0, 0), thigh)
        lr(arm, 'calf_' + side, (1, 0, 0), calf)
        lr(arm, 'foot_' + side, (1, 0, 0), -0.5 * 30 * swing + (-10 if side == 'l' else 8) * lunge)
    arm.pose.bones['root'].location = (0, 0, 0.003 + 0.012 * (1 - math.cos(2 * ph)) * run * 0.5)
    arm.pose.bones['pelvis'].location = (0, -0.02 * run - 0.03 * lunge, -0.035 * lunge)


def head_look(clip, f, g):
    if clip in NEW:
        body_pass(clip, f)
        return 0.0
    return ORIG['head_look'](clip, f, g)


def events(clip):
    if clip in NEW:
        out = []
        for e in S.NEW_EVENTS[clip]:
            d = {'frame': e[0], 'name': e[1]}
            if len(e) > 2:
                d['socket'] = e[2]
            out.append(d)
        return out
    return ORIG['events'](clip)


def release_intervals(clip):
    if clip in NEW:
        return []
    return ORIG['release_intervals'](clip)


def install():
    """Adds the bayonet gestures to the musket's animation module (idempotent)."""
    if ORIG:
        return MA
    check_musket()
    for name in ('weapon_pose', 'head_look', 'events', 'release_intervals'):
        ORIG[name] = getattr(MA, name)
    MA.CLIPS.update(NEW)
    MA.weapon_pose = weapon_pose
    MA.head_look = head_look
    MA.events = events
    MA.release_intervals = release_intervals
    return MA


def tip_in_view(clip, f):
    """Where the bayonet point is in the first person view (cm) at f (control value for the manifest)."""
    M = weapon_pose(clip, f, 'FP')
    return [round(v, 2) for v in MA.to_view(M @ Vector(S.BLADE_TIP))]
