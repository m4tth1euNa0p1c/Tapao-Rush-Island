"""Bean handling of the hand-cranked machine gun Mle 1918 (T-272), first (FP) and third (TP) person, 30 fps, baked on
the bean skeleton and on the gun skeleton. Clip names and events are the contract with the C++ (T-273):

  Idle 90 (loop)        Walk 120 (loop)          Sprint 90 (loop, the gun carried across the body)
  Equip 54              DrawWeapon 6, Ready 46     Holster 48   StowWeapon 42
  FireStart 17          Fire 1, 9, 13 (the crank spins up: 3/4 turn), CrankTick 0
  FireLoop 16 (loop)    Fire 0, 4, 8, 12, CrankTick 0 (one turn = four shots = 450 rounds/min at play rate 1)
  FireEnd 10            CrankTick 8 (the crank stops on its detent, no shot)
  Reload 150 (5 s)      CoverOpen 16, BeltOut 34, BoxOut 64, BoxIn 92, BeltIn 116, CoverClose 126, Charge 136
  Overheat 60           SteamVent 6 (the valve flap pops, the left hand jerks back off the hot gun, shakes, regrips)
  Inspect 180, DryFire 14 (DryClick 4), Discard 40 (Empty 4, Release 24)

Crank: angle phi (deg) from its rest position (handle low in front of the axle, CRANK_REST_DEG); it turns forward over
the top; a shot every quarter turn from the rest angle (phi = 0, 90, 180, 270 mod 360). FireStart takes the crank
from rest to phi = 270 (accelerating over 10 frames: shots at phi 0, 90, 180), FireLoop turns 270 -> 630 at full speed
(shots 270, 360, 450, 540: frame 16 = frame 0), FireEnd stops it on 360 = rest. So: FireStart -> FireLoop (repeat) ->
FireEnd, and FireEnd only from the end of a FireLoop turn (the C++ finishes the current turn, without shots, max
0.53 s, or accepts a jump of the crank).

Belt: a chain of bones put on the belt path every frame (machinegun_model.feed_path), one pitch per shot, jerked in
two frames after each Fire; over a FireLoop turn the belt advances four pitches and jumps back at the loop wrap, where
the engine does not interpolate (a jump inside a clip would slide the whole belt backwards between two samples).
Pockets that run past the extraction point (inside the receiver) are scaled away, the spare empty pockets take over
on the far side. Reload: the old belt is lifted out of the feedway and dropped (it hangs from the box), the box is
unhooked and let go (BoxOut: the ammo_box bone is scaled away from that frame, the game drops a SM_Ammo_MGBox from
socket_box if it wants), a new box is hooked on (BoxIn), its leader pulled up to the feed and threaded (BeltIn), the
cover slammed (CoverClose) and the cocking handle pulled (Charge: the first round advances one pitch).

Recoil: every shot kicks the gun 1.4 cm back and 1.8 deg up in one frame and settles in three (periodic in FireLoop:
the animation stays in place), with a small yaw / roll scatter per shot and a high frequency shudder; the hands are on
the gun, so the arms shake with it; in third person the shoulders take it too. Metres, Blender axes: the bean looks
along -Y, its right side is -X, +Z up. Weapon local: +X muzzle, +Z up, -Y right side (crank).
"""
from __future__ import annotations

import importlib.util
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

import machinegun_model as MG

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location('musket_animation_mg', HERE.parents[0] / 'musket_pack' / 'animation.py')
MA = importlib.util.module_from_spec(_spec)
sys.modules['musket_animation_mg'] = MA
_spec.loader.exec_module(MA)

smooth, curve, segment, frame, axes, blend, ease_os = MA.smooth, MA.curve, MA.segment, MA.frame, MA.axes, MA.blend, MA.ease_os
FP_CAMERA, FWD, RIGHT, UP, FP_LENS = MA.FP_CAMERA, MA.FWD, MA.RIGHT, MA.UP, MA.FP_LENS
camera_matrix, vp, vv, to_view, rotator_in_view = MA.camera_matrix, MA.vp, MA.vv, MA.to_view, MA.rotator_in_view
fp_arm_transform, solve_fp_arm, solve_tp_arm, solve_leg = MA.fp_arm_transform, MA.solve_fp_arm, MA.solve_tp_arm, MA.solve_leg
local_rot, set_matrices, bake_matrices, linear = MA.local_rot, MA.set_matrices, MA.bake_matrices, MA.linear

FPS = 30
CLIPS = {'Equip': 54, 'Idle': 90, 'Walk': 120, 'Sprint': 90, 'FireStart': 17, 'FireLoop': 16, 'FireEnd': 10, 'Reload': 150,
         'Overheat': 60, 'Inspect': 180, 'Holster': 48, 'DryFire': 14, 'Discard': 40}
LOOPS = {'Idle', 'Walk', 'Sprint', 'FireLoop'}
OMEGA = 360.0 / 16.0                    # crank speed in FireLoop (deg per frame)
SHOTS = {'FireStart': [1, 9, 13], 'FireLoop': [0, 4, 8, 12]}
RELEASE_FRAME, EMPTY_FRAME = 24, 4
R_EVENTS = {'CoverOpen': 16, 'BeltOut': 34, 'BoxOut': 64, 'BoxIn': 92, 'BeltIn': 116, 'CoverClose': 126, 'Charge': 136}
STEAM_VENT = 6

GRIP_R, GRIP_L = MG.GRIP_R, MG.GRIP_L
MUZZLE = Vector((MG.MUZZLE_X, 0.0, 0.0))
FEED = Vector((MG.BELT_X, MG.REC_HW, 0.033))
RECEIVER = Vector((-0.02, 0.0, 0.0))
P = MG.PITCH

# ------------------------------------------------------------------------------------------- first person poses


def view_pose_dirs(anchor_local, anchor_cm, x_dir, up_dir):
    """Weapon root matrix (bean space): the weapon point anchor_local at anchor_cm in the view, the barrel along
    x_dir and the top of the gun toward up_dir (view directions; X forward, Y right, Z up)."""
    x = vv(*x_dir)
    z = vv(*up_dir)
    z = (z - x * x.dot(z)).normalized()
    y = z.cross(x)
    R = Matrix((x, y, z)).transposed()
    m = R.to_4x4()
    m.translation = vp(*anchor_cm) - R @ Vector(anchor_local)
    return m


FP_POSES = {
    # hip: the crank handle (its rest is top front: the right hand rides above the receiver) low on the right, the
    # jacket toward the middle of the screen, the muzzle just under the crosshair, rolled so the top cover and the belt
    # on the left flank show (poses checked by projecting the gun's key points on the 16:9 frame)
    'hip': (GRIP_R, (40.0, 28.0, -19.0), (0.985, -0.10, 0.12), (0.05, 0.30, 0.95)),
    # sprint: carried across the body, muzzle low to the left
    'sprint': (GRIP_R, (33.0, 19.0, -12.0), (0.80, -0.58, -0.20), (0.12, 0.30, 0.95)),
    # reload: brought up and turned, the top cover, the feed, the belt, the box and the cocking knob in view
    'reload': (FEED, (48.0, 6.0, -10.0), (0.86, -0.46, 0.22), (-0.12, 0.55, 0.83)),
    # put away below the screen
    'holster': (GRIP_R, (18.0, 26.0, -66.0), (0.45, 0.25, -0.86), (0.85, 0.0, 0.45)),
    # inspect: the left flank (box, belt), then rolled to show the right flank (crank), then the cone of the muzzle
    'inspect_left': (FEED, (38.0, 3.0, -12.0), (0.70, -0.68, 0.22), (-0.20, 0.55, 0.81)),
    'inspect_right': (RECEIVER, (40.0, 8.0, -10.0), (0.80, -0.55, 0.20), (0.45, 0.78, 0.36)),
    'inspect_muzzle': (RECEIVER, (40.0, 18.0, -14.0), (0.40, -0.90, 0.18), (0.02, 0.22, 0.97)),
    # discard: a look at the empty feed, then out to the right and down
    'look': (FEED, (38.0, 4.0, -13.0), (0.74, -0.62, 0.25), (-0.16, 0.58, 0.80)),
    'toss': (GRIP_R, (38.0, 46.0, -34.0), (0.25, 0.80, -0.55), (0.90, -0.15, 0.40)),
}
REACH = {'r': (8.0, 26.0, -48.0), 'l': (16.0, -30.0, -46.0)}
WORK_ELBOW = {'r': (12.0, 34.0, -40.0), 'l': (8.0, -34.0, -42.0)}
CRANK_U = (0.0, 0.55, 0.84)            # FP back of the right hand on the crank handle
FORE_U = (-0.15, -0.80, 0.58)           # FP back of the left hand on the fore-grip
FREE_R = ('v', (20.0, 26.0, -58.0), Vector((-.29, -.10, .36)))
FREE_L = ('v', (20.0, -26.0, -58.0), Vector((.29, -.10, .36)))


def fp_pose(name):
    anchor, where, x_dir, up_dir = FP_POSES[name]
    return view_pose_dirs(anchor, where, x_dir, up_dir)


def tp_poses():
    """Bean space. The bean's arms are short: the gun is held across the belly, the crank handle at the right hip and
    the LEFT HAND ON THE CARRYING HANDLE on top of the jacket (the fore-grip is out of its reach: 0.58 m from the left
    shoulder against 0.38 for the carrying handle), the muzzle forward of the left; rolled (top toward the body) so the
    box under the left flank swings clear of the belly."""
    at = MA.tp_pose_at
    return dict(hip=at(GRIP_R, (-.14, -.26, .50), (.90, -.42, .05), -50),
                sprint=at(GRIP_R, (-.12, -.26, .56), (.75, -.30, .58), -45),
                # reloading: propped up against the chest, muzzle high, so the short left arm reaches box and feed
                reload=at(GRIP_R, (-.06, -.32, .60), (.80, -.30, .50), -40),
                holster=at(GRIP_R, (-.30, -.20, .43), (.15, -.16, -.975), 12),
                inspect_left=at(GRIP_R, (-.10, -.30, .66), (.62, -.30, .72), -60),
                inspect_right=at(GRIP_R, (-.12, -.30, .62), (.78, -.36, .50), -25),
                inspect_muzzle=at(GRIP_R, (-.14, -.28, .52), (.80, -.58, .12), -45),
                look=at(GRIP_R, (-.10, -.31, .66), (.65, -.28, .70), -45),
                toss=at(GRIP_R, (-.36, -.28, .44), (-.30, -.45, -.84), 35))


def poses(view):
    if view == 'FP':
        return {k: fp_pose(k) for k in FP_POSES}
    return tp_poses()


# --------------------------------------------------------------------------------------------- crank and recoil

def crank_phi(clip, f):
    """Crank rotation from rest (deg; the handle angle is CRANK_REST_DEG - phi)."""
    if clip == 'FireStart':
        return 1.125 * f * f if f <= 10 else 112.5 + OMEGA * (f - 10)
    if clip == 'FireLoop':
        return 270.0 + OMEGA * f
    if clip == 'FireEnd':
        if f <= 8:
            return 270.0 + OMEGA * f - OMEGA * f * f / 16.0
        return 360.0 + curve(f, [(8, 0.0), (9, -2.5), (10, 0.0)])
    if clip == 'DryFire':
        return curve(f, [(0, 0.0), (2, 0.0), (4, 26.0), (5, 28.0), (9, -2.0), (11, 0.0), (14, 0.0)])
    if clip == 'Discard':
        return curve(f, [(0, 0.0), (2, 0.0), (EMPTY_FRAME, 30.0), (6, 34.0), (10, 0.0), (40, 0.0)])
    if clip == 'Inspect':
        return curve(f, [(0, 0.0), (98, 0.0), (104, 36.0), (108, 40.0), (114, -3.0), (117, 0.0), (180, 0.0)])
    return 0.0


def crank_matrix(phi):
    """Weapon space rigid transform of the crank turned by phi (about the weapon's Y axis through the axle)."""
    c = MG.CRANK_C
    return Matrix.Translation(c) @ Matrix.Rotation(math.radians(phi), 4, 'Y') @ Matrix.Translation(-c)


def handle_point(phi):
    return MG.crank_handle_point(MG.CRANK_REST_DEG - phi)


def kick(clip, f):
    """Recoil envelope (0..1) of the shots of the clip at frame f; periodic in FireLoop."""
    shots = SHOTS.get(clip)
    if not shots:
        return 0.0
    k = 0.0
    for s in shots:
        d = f - s
        if clip == 'FireLoop' and d < 0:
            d += CLIPS['FireLoop']
        if 0 <= d <= 4:
            k += curve(d, [(0, 0.0), (1, 1.0), (2, 0.42), (3, 0.12), (4, 0.0)])
    return min(k, 1.2)


def shot_index(clip, f):
    """Index of the last shot at or before f (for the scatter), -1 before the first."""
    shots = SHOTS.get(clip, [])
    idx = -1
    for i, s in enumerate(shots):
        if s <= f:
            idx = i
    if clip == 'FireLoop' and idx < 0:
        idx = len(shots) - 1
    return idx


SCATTER = [(0.55, -0.9), (-0.40, 0.7), (0.30, 1.1), (-0.65, -0.6)]   # (yaw deg, roll deg) per shot of a turn


def recoil(clip, f):
    """Weapon local transform of the recoil at f (applied after the pose)."""
    k = kick(clip, f)
    if not k and clip not in ('FireLoop', 'FireStart'):
        return Matrix.Identity(4)
    i = shot_index(clip, f)
    yaw, roll = SCATTER[i % 4] if i >= 0 else (0.0, 0.0)
    n = CLIPS[clip]
    # a fine shudder while the crank turns (periodic in the loop: whole cycles over 16 frames)
    hum = 0.0
    if clip == 'FireLoop':
        hum = math.sin(2 * math.pi * 5 * f / n) * 0.35 + math.sin(2 * math.pi * 3 * f / n + 1.1) * 0.25
    elif clip == 'FireStart':
        hum = (math.sin(2 * math.pi * 5 * f / 16) * 0.35 + math.sin(2 * math.pi * 3 * f / 16 + 1.1) * 0.25) * smooth(0, 10, f)
    m = Matrix.Translation((-0.014 * k, 0.0006 * hum, 0.0022 * k + 0.0005 * hum))
    m = m @ Matrix.Rotation(math.radians(-1.8 * k), 4, 'Y') @ Matrix.Rotation(math.radians(yaw * k), 4, 'Z') \
        @ Matrix.Rotation(math.radians(roll * k + 0.25 * hum), 4, 'X')
    return m


# --------------------------------------------------------------------------------------------- weapon pose

def keyed_pose(f, keys, view, over=0.05, antic=0.03):
    p = poses(view)
    i, w = MA.segment_os(f, keys, over, antic)
    a = p[keys[i][1]]
    b = p[keys[min(i + 1, len(keys) - 1)][1]]
    return blend(a, b, w)


RELOAD_POSE_KEYS = [(0, 'hip'), (16, 'reload'), (134, 'reload'), (150, 'hip')]
INSPECT_KEYS = [(0, 'hip'), (28, 'inspect_left'), (58, 'inspect_left'), (86, 'inspect_right'), (118, 'inspect_right'),
                (144, 'inspect_muzzle'), (158, 'inspect_muzzle'), (180, 'hip')]


def discard_flight(p, f, view):
    toss = p['toss']
    dt = (f - RELEASE_FRAME) / FPS
    if view == 'FP':
        v = vv(0.6, 2.4, 0.2) * 2.2
        spin_axis = vv(0.6, 0.2, -0.75)
    else:
        v = Vector((-1.5, -0.4, 0.5))
        spin_axis = Vector((0.3, -0.9, 0.2))
    floor = 0.21                         # the box hangs 0.21 m under the bore: lying on it
    z0 = toss.translation.z
    a, b, c = -4.905, v.z, z0 - floor
    disc = b * b - 4 * a * c
    land = (-b - math.sqrt(disc)) / (2 * a) if disc > 0 else 1e9
    t = min(dt, land)
    pos = toss.translation + v * t + Vector((0, 0, -4.905 * t * t))
    if dt > land:
        pos += Vector((v.x, v.y, 0)) * min(dt - land, 0.2) * 0.25
        pos.z = floor
    rot = Quaternion(spin_axis.normalized(), min(dt, land + 0.06) * 5.0)
    m = rot.to_matrix().to_4x4() @ toss.to_3x3().to_4x4()
    m.translation = pos
    return m


def weapon_pose(clip, f, view):
    p = poses(view)
    end = CLIPS[clip]
    base = p['hip']
    if clip == 'Sprint':
        base = p['sprint']
    elif clip == 'Equip':
        base = blend(p['holster'], p['hip'], ease_os(0, 40, f, over=0.08, antic=0.0))
        # the weight lands in the hands: a dip and a recovery
        dip = curve(f, [(36, 0), (42, 1), (48, -0.25), (54, 0)])
        base = base @ Matrix.Translation((0, 0, -0.018 * dip)) @ Matrix.Rotation(math.radians(2.0 * dip), 4, 'Y')
    elif clip == 'Holster':
        base = blend(p['hip'], p['holster'], ease_os(8, end, f, over=0.0, antic=0.06))
    elif clip == 'Reload':
        base = keyed_pose(f, RELOAD_POSE_KEYS, view, over=0.06, antic=0.04)
        # the work jolts the gun: cover opened and slammed, box torn off and pushed home, the charge
        jolt = (curve(f, [(14, 0), (16, 1), (20, 0)]) * 0.6 + curve(f, [(54, 0), (57, 1), (62, 0)]) * 0.8
                + curve(f, [(88, 0), (92, 1), (97, 0)]) * 1.0 + curve(f, [(124, 0), (126, 1), (130, 0)]) * 0.9
                + curve(f, [(130, 0), (136, 1), (138, -0.6), (142, 0)]) * 1.0)
        base = base @ Matrix.Translation((-0.004 * jolt, 0, -0.006 * jolt)) @ Matrix.Rotation(math.radians(1.2 * jolt), 4, 'Y')
    elif clip == 'Inspect':
        base = keyed_pose(f, INSPECT_KEYS, view, over=0.05, antic=0.03)
        tap = curve(f, [(42, 0), (44, 1), (48, 0)])
        base = base @ Matrix.Translation((0, -0.004 * tap, -0.003 * tap)) @ Matrix.Rotation(math.radians(4.0 * math.sin(math.pi * smooth(30, 58, f))), 4, 'Z')
    elif clip == 'Overheat':
        # the boiling jacket shudders, the front sinks when the left hand lets go, comes back when it regrips
        sink = curve(f, [(0, 0), (9, 0), (16, 1), (34, 1), (44, 0)])
        boil = math.sin(f * 2.1) * curve(f, [(2, 0), (5, 1), (40, 1), (52, 0)])
        base = base @ Matrix.Rotation(math.radians(3.2 * sink), 4, 'Y') @ Matrix.Translation((0, 0, -0.012 * sink + 0.0012 * boil))
    elif clip == 'Discard':
        if f <= 8:
            base = blend(p['hip'], p['look'], ease_os(0, 8, f, over=0.05))
        elif f <= 16:
            base = p['look']
        elif f <= RELEASE_FRAME:
            base = blend(p['look'], p['toss'], ease_os(16, RELEASE_FRAME, f, antic=0.08))
        else:
            base = discard_flight(p, f, view)
        if f <= 12:
            j = curve(f, [(2, 0), (EMPTY_FRAME, 1), (7, 0.3), (10, 0)])
            base = base @ Matrix.Translation((-0.006 * j, 0, 0)) @ Matrix.Rotation(math.radians(-1.5 * j), 4, 'Y')
    if clip in ('Idle', 'FireEnd', 'DryFire'):
        # breathing (zero at frame 0: the game measures the hip framing on Idle frame 0)
        n = CLIPS['Idle']
        q = math.sin(2 * math.pi * f / n)
        q2 = math.sin(4 * math.pi * f / n)
        amp = 1.0 if clip == 'Idle' else 0.0
        base = base @ Matrix.Translation((0, .0016 * q * amp, .0040 * q * amp + .0008 * q2 * amp)) @ Matrix.Rotation(math.radians(0.7 * q * amp), 4, 'Y')
    if clip == 'DryFire':
        j = curve(f, [(2, 0), (4, 1), (8, 0)])
        base = base @ Matrix.Translation((-0.004 * j, 0, 0)) @ Matrix.Rotation(math.radians(-0.8 * j), 4, 'Y')
    if clip in ('Walk', 'Sprint'):
        cycles = 4
        q = 2 * math.pi * f / end * cycles
        amp = 1.25 if clip == 'Walk' else 2.4
        # a heavy gun: big vertical bob on each step, the muzzle swinging and lagging
        base = base @ Matrix.Translation((.004 * amp * math.sin(q), .006 * amp * math.sin(q + 0.6), .010 * amp * math.sin(q * 2)))
        base = base @ Matrix.Rotation(math.radians((2.2 if clip == 'Walk' else 5.0) * math.sin(q + 0.4)), 4, 'X') \
            @ Matrix.Rotation(math.radians((1.4 if clip == 'Walk' else 3.2) * math.sin(q * 2 + 0.5)), 4, 'Y') \
            @ Matrix.Rotation(math.radians((1.0 if clip == 'Walk' else 2.5) * math.sin(q + 1.2)), 4, 'Z')
    return base @ recoil(clip, f)


# ----------------------------------------------------------------------------------------------- moving parts

def cover_angle(clip, f):
    """Feed cover opening (deg; the rear edge lifts about the front hinge)."""
    if clip == 'Reload':
        return curve(f, [(0, 0), (14, 0), (16, 22), (20, 92), (22, 86), (24, 88), (120, 88), (124, 30), (126, -1.5), (128, 0.5), (130, 0), (150, 0)])
    return 0.0


def cover_matrix(angle):
    h = MG.COVER_HINGE
    return Matrix.Translation(h) @ Matrix.Rotation(math.radians(angle), 4, 'Y') @ Matrix.Translation(-h)


def cock_slide(clip, f):
    """Cocking handle travel back (m)."""
    if clip == 'Reload':
        return curve(f, [(0, 0), (129, 0), (133, 0.6 * MG.COCK_TRAVEL), (136, MG.COCK_TRAVEL), (137, MG.COCK_TRAVEL), (138.5, -0.002), (140, 0), (150, 0)])
    return 0.0


def steam_angle(clip, f):
    """Steam valve flap opening (deg about its rear hinge, the front lifts)."""
    if clip == 'Overheat':
        rattle = 7.0 * math.sin(f * 2.6) * curve(f, [(8, 0), (10, 1), (40, 1), (46, 0)])
        return curve(f, [(0, 0), (STEAM_VENT - 1, 0), (STEAM_VENT, 30), (STEAM_VENT + 2, 62), (STEAM_VENT + 4, 54), (42, 54), (47, 8), (49, -2), (51, 0), (60, 0)]) + rattle
    return 0.0


def steam_matrix(angle):
    h = MG.STEAM_HINGE
    # the flap's front lifts: rotation about -Y (the hinge axis) through the hinge pin
    return Matrix.Translation(h) @ Matrix.Rotation(math.radians(-angle), 4, 'Y') @ Matrix.Translation(-h)


BOX_C = (MG.BOX_MIN + MG.BOX_MAX) * 0.5
BOX_GRAB = Vector((BOX_C.x + 0.006, MG.BOX_MAX.y + 0.010, MG.BOX_MAX.z - 0.040))   # where the left hand holds the box


def box_matrix(clip, f, view='FP'):
    """Weapon space transform of the ammunition box, or None when it is away (scaled out). Third person: half the
    travel (the bean's short left arm follows the box)."""
    k = 1.0 if view == 'FP' else 0.35
    if clip != 'Reload':
        return Matrix.Identity(4)
    if f <= 52:
        return Matrix.Identity(4)
    if f <= R_EVENTS['BoxOut']:
        # unhooked (slides down off the rail), then pulled away down and out, tilting
        w0 = smooth(52, 56, f)
        w = smooth(56, R_EVENTS['BoxOut'], f)
        move = Vector((0.010 * w, 0.060 * w * k, -0.004 * w0 - 0.120 * w * w * k))
        tilt = Matrix.Rotation(math.radians(-28.0 * w * k), 4, 'X')
        return Matrix.Translation(move) @ Matrix.Translation(BOX_GRAB) @ tilt @ Matrix.Translation(-BOX_GRAB)
    if f < 72:
        return None
    if f <= R_EVENTS['BoxIn']:
        # the new box comes up from below in the left hand, tilted, then slides home on the rail
        w = smooth(72, 88, f)
        w2 = smooth(88, R_EVENTS['BoxIn'], f)
        move = Vector((0.006 * (1 - w), 0.070 * (1 - w) * k, -0.170 * (1 - w) * k - 0.006 * (1 - w2) * w))
        tilt = Matrix.Rotation(math.radians(-24.0 * (1 - w) * k), 4, 'X')
        return Matrix.Translation(move) @ Matrix.Translation(BOX_GRAB) @ tilt @ Matrix.Translation(-BOX_GRAB)
    jolt = curve(f, [(R_EVENTS['BoxIn'], 0), (R_EVENTS['BoxIn'] + 1, 1), (R_EVENTS['BoxIn'] + 3, 0)])
    return Matrix.Translation((0, 0, 0.0015 * jolt))


# ----------------------------------------------------------------------------------------------- the belt

F_PTS, F_S, F_MARKS = MG.feed_path()
FPATH = MG.Path(F_PTS)
S_X, S_SLOT, S_FEED = F_MARKS['extract'], F_MARKS['box_slot'], F_MARKS['feed_in']
REST = MG.belt_rest_positions()
IN_BONES = [f'belt_{k:02d}' for k in range(1, MG.N_IN + 1)]
OUT_BONES = [f'belt_out_{j:02d}' for j in range(1, MG.N_OUT + 1)]
PHYS = set(IN_BONES) | set(OUT_BONES[MG.OUT_SPARE:])
REST_FRAME = {b: MG.pocket_frame(*FPATH.at(s)) for b, s in REST.items()}
REST_INV = {b: m.inverted() for b, m in REST_FRAME.items()}
SLOT = FPATH.at(S_SLOT)[0]
HIDDEN = Matrix.Diagonal((1e-4, 1e-4, 1e-4, 1.0))
TIP_U = REST[OUT_BONES[-1]] - S_SLOT + 0.5 * P + 0.006       # the leader's brass tip beyond the last pocket


def belt_margins():
    """Checks of the belt's hidden ends (m): deepest loaded pocket under the box slot at the end of a FireLoop turn,
    deepest empty pocket inside the pouch."""
    deep_in = S_SLOT - (REST[IN_BONES[-1]] + 4 * P)
    pouch = F_MARKS['end'] - (REST[OUT_BONES[-1]] + 4 * P)
    first_out = REST[OUT_BONES[0]] + 4 * P - S_X
    return {'in_below_slot_at_4p_m': round(deep_in, 4), 'out_inside_pouch_at_4p_m': round(pouch, 4),
            'spare_out_reaches_extract_at_4p_m': round(first_out, 4),
            'visible_loaded_pockets': round((S_X - S_SLOT) / P, 2), 'pitch_m': P}


def conveyor(clip, f):
    """Belt advance (m) of the shots at f: one pitch jerked in over the two frames after each Fire."""
    shots = SHOTS.get(clip)
    if not shots:
        return 0.0
    return P * sum(smooth(s + 1, s + 3, f) for s in shots)


def curve_path(points, n=12):
    """A smooth Path through a few control points (Catmull-Rom)."""
    pts = [Vector(p) for p in points]
    out = []
    for i in range(len(pts) - 1):
        p0 = pts[max(i - 1, 0)]
        p1, p2 = pts[i], pts[i + 1]
        p3 = pts[min(i + 2, len(pts) - 1)]
        for k in range(n):
            t = k / n
            t2, t3 = t * t, t * t * t
            out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    out.append(pts[-1])
    return MG.Path(out)


def interior(u, Bx):
    """Inside the box below the slot (u < 0), box space -> weapon space."""
    p = SLOT + Vector((0, 0, u))
    return Bx @ p, (Bx.to_3x3() @ Vector((0, 0, 1))).normalized()


def place(path, u, Bx):
    """Point and tangent at u on a belt shape that starts at the box slot (u = 0); u < 0 runs inside the box."""
    if u < 0:
        return interior(u, Bx)
    return path.at(u)


DEEP = 0.100                             # pockets deeper than this under the box slot are folded away in the box


def fed_state(o, rule='fire', sway=None):
    """{bone: (W pocket frame in weapon space, visible)} on the fed path F with offset o. rule 'fire': loaded
    pockets past the extraction point and empty ones before it are hidden; 'belt': the physical belt only (spares
    hidden). Pockets deeper than DEEP in the box are hidden either way."""
    out = {}
    for b, s0 in REST.items():
        s = s0 + o
        point, tan = FPATH.at(s)
        W = MG.pocket_frame(point, tan)
        if sway is not None and S_SLOT < s < S_FEED:
            u = (s - S_SLOT) / (S_FEED - S_SLOT)
            W = Matrix.Translation(W.col[2].xyz * sway * math.sin(math.pi * u)) @ W
        if rule == 'fire':
            vis = (s <= S_X + 1e-6) if b in IN_BONES else (s >= S_X - 1e-6)
        else:
            vis = b in PHYS
        out[b] = (W, vis and s > S_SLOT - DEEP)
    return out


def shaped_state(path, o, Bx, keep=None):
    """{bone: (W, visible)} of the physical belt laid on a shape path starting at the (moved) box slot."""
    out = {}
    for b, s0 in REST.items():
        u = s0 - S_SLOT + o
        point, tan = place(path, u, Bx)
        W = MG.pocket_frame(point, tan, axis=(Bx.to_3x3() @ Vector((1, 0, 0))).normalized())
        vis = b in PHYS if keep is None else keep(b)
        out[b] = (W, vis and u > -DEEP)
    return out


def mix_states(a, b, w):
    if w <= 0.0:
        return a
    if w >= 1.0:
        return b
    out = {}
    for k in a:
        Wa, va = a[k]
        Wb, vb = b[k]
        out[k] = (blend(Wa, Wb, w), vb if w > 0.5 else va)
    return out


def gravity_in(M):
    return (M.to_3x3().inverted() @ Vector((0, 0, -1))).normalized()


def lifted_path(hand_w, M):
    """BeltOut: from the box slot up to the right hand (holding the belt near the extraction point), the empty part
    dangling under the hand."""
    g = gravity_in(M)
    hand = M.inverted() @ hand_w
    mid = SLOT.lerp(hand, 0.5) + Vector((0, 0.05, 0.02))
    pts = [SLOT, SLOT + Vector((0, 0, 0.03)), mid, hand]
    # on past the hand: the empty end dangling under it
    tail = [hand + g * 0.03 + Vector((0, 0.01, 0)), hand + g * 0.08 + Vector((0, 0.02, 0)), hand + g * 0.16 + Vector((0, 0.03, 0))]
    return curve_path(pts + tail)


def hanging_path(Bx, M):
    """After the drop: out of the slot, over the outer edge of the box and down its flank, hanging."""
    g = gravity_in(M)
    R = Bx.to_3x3()
    s = Bx @ SLOT
    pts = [s, s + R @ Vector((0, 0.006, 0.020)), s + R @ Vector((0, 0.030, 0.028)), s + R @ Vector((0, 0.052, 0.006)),
           s + R @ Vector((0, 0.060, -0.030)) + g * 0.01]
    last = pts[-1]
    for k in range(1, 8):
        pts.append(last + g * 0.06 * k + R @ Vector((0, 0.004 * k, 0)))
    return curve_path(pts, 10)


def new_belt_path(hand_w, M, Bx):
    """BeltIn (free part): the leader tip in the left hand, the belt coming out of the new box's slot."""
    hand = M.inverted() @ hand_w
    R = Bx.to_3x3()
    s = Bx @ SLOT
    pts = [s, s + R @ Vector((0, 0.0, 0.025)), s.lerp(hand, 0.55) + Vector((0, 0.035, 0.0)), hand]
    return curve_path(pts)


MEMO = {}
O_NEW_IN_BOX = -(TIP_U)                  # the new belt deep in its box, the leader tip at the slot
O_SHORT = -P                             # threaded, one pitch short of the extraction point (Charge advances it)


def reload_belt(f, M, view, hands):
    """{bone: (W, visible)} of the Reload clip at frame f; hands = {'r': point, 'l': point} (bean space)."""
    Bx = box_matrix('Reload', f, view)
    if f <= 28:
        return fed_state(0.0, rule='belt')
    if f <= 38:
        if f == 38:
            MEMO['release'] = (hands['r'].copy(), M.copy())
        lift = shaped_state(lifted_path(hands['r'], M), 0.0, Matrix.Identity(4))
        return mix_states(fed_state(0.0, rule='belt'), lift, smooth(28, 31, f))
    if f <= R_EVENTS['BoxOut']:
        hang = shaped_state(hanging_path(Bx, M), 0.0, Bx)
        if f <= 46:
            hand, M38 = MEMO.get('release', (hands['r'], M))
            # the belt let go at frame 38: its lifted shape (as it was in the hand) falls into the hanging one
            lift = shaped_state(lifted_path(hand, M38), 0.0, Matrix.Identity(4))
            return mix_states(lift, hang, smooth(38, 46, f))
        return hang
    if Bx is None:
        return {b: (Matrix.Identity(4), False) for b in REST}
    if f <= 94:
        return shaped_state(MG.Path([Bx @ SLOT, Bx @ SLOT + Bx.to_3x3() @ Vector((0, 0, 0.01))]), O_NEW_IN_BOX, Bx)
    if f <= 108:
        path = new_belt_path(hands['l'], M, Bx)
        o = path.length - TIP_U
        st = shaped_state(path, o, Bx)
        if f > 104:
            # the free shape hands over to the fed path as the leader enters the feed slot
            o_fed = (S_FEED + 0.004) - (REST[OUT_BONES[-1]] + 0.5 * P + 0.006)
            st = mix_states(st, fed_state(o_fed, rule='belt'), smooth(104, 108, f))
        return st
    o_fed0 = (S_FEED + 0.004) - (REST[OUT_BONES[-1]] + 0.5 * P + 0.006)
    if f <= 122:
        # threaded in three pushes
        w = curve(f, [(108, 0.0), (111, 0.36), (112, 0.36), (115, 0.70), (116, 0.70), (120, 1.0), (122, 1.0)])
        return fed_state(o_fed0 + (O_SHORT - o_fed0) * w, rule='belt')
    if f <= 134:
        return fed_state(O_SHORT, rule='belt')
    return fed_state(O_SHORT * (1.0 - smooth(134, 137, f)), rule='belt')


def belt_bones(clip, f, M, view, hands):
    if clip == 'Reload':
        return reload_belt(f, M, view, hands)
    if clip in ('FireStart', 'FireLoop'):
        return fed_state(conveyor(clip, f), rule='fire')
    sway = None
    if clip in ('Walk', 'Sprint'):
        q = 2 * math.pi * f / CLIPS[clip] * 4
        sway = (0.004 if clip == 'Walk' else 0.009) * math.sin(q * 2 + 0.9)
    elif clip == 'Equip':
        sway = 0.008 * math.sin(math.pi * smooth(34, 54, f) * 2) * (1 - smooth(48, 54, f))
    elif clip == 'Overheat':
        sway = 0.002 * math.sin(f * 2.1) * curve(f, [(2, 0), (5, 1), (40, 1), (52, 0)])
    return fed_state(0.0, rule='fire', sway=sway)


# ------------------------------------------------------------------------------------------------- hands

def hand_frame(point, elbow_cm, back):
    d = (point - vp(*elbow_cm)).normalized()
    return axes(d, vv(*back))


def resolve(spec, view, M, ctx):
    """A hand target at this frame: ('crank',) the crank handle, ('w', p) a weapon point, ('cover', p) on the feed
    cover, ('cock', p) on the cocking knob, ('box', p) on the box, ('belt', bone) a belt pocket, ('v', cm, bean point)
    free in the view / body space."""
    kind = spec[0]
    if kind == 'crank':
        return M @ (crank_matrix(ctx['phi']) @ handle_point(0.0))
    if kind == 'w':
        return M @ Vector(spec[1])
    if kind == 'cover':
        return M @ (cover_matrix(ctx['cover']) @ Vector(spec[1]))
    if kind == 'cock':
        return M @ (Vector(spec[1]) - Vector((ctx['cock'], 0, 0)))
    if kind == 'box':
        Bx = ctx['box'] if ctx['box'] is not None else ctx.get('box_last', Matrix.Identity(4))
        return M @ (Bx @ Vector(spec[1]))
    if kind == 'pt':
        return M @ Vector(spec[1])
    if kind == 'v':
        return vp(*spec[1]) if view == 'FP' else Vector(spec[2])
    raise ValueError(spec)


def keyed_hand(keys, f, view, M, ctx):
    """Hand centre (bean space) and FP hand rotation (None in TP) at frame f along keys
    (frame, target, FP elbow (view cm), FP back of the hand (view dir))."""
    i, w = segment(f, keys)
    k0, k1 = keys[i], keys[min(i + 1, len(keys) - 1)]
    out = []
    for k in (k0, k1):
        p = resolve(k[1], view, M, ctx)
        rot = hand_frame(p, k[2], k[3]) if view == 'FP' else None
        out.append((p, rot))
    p = out[0][0].lerp(out[1][0], w)
    rot = None
    if view == 'FP':
        rot = out[0][1].to_quaternion().slerp(out[1][1].to_quaternion(), w).to_matrix()
    return p, rot


CRANK = ('crank',)
FORE = ('w', GRIP_L)
CARRY = ('w', MG.CARRY_GRIP)
LATCH = ('cover', Vector((MG.COVER_X0 + 0.004, 0.0, MG.TOP_Z + 0.012)))
KNOB = ('cock', MG.COCK_KNOB + Vector((0.0, 0.010, 0.0)))
BOX_H = ('box', BOX_GRAB)
SLOT_H = ('box', SLOT + Vector((0, 0.004, 0.022)))
BELT_GRAB = ('pt', Vector((MG.BELT_X, 0.0, 0.050)))


def clip_hands(clip, f, view, M, ctx):
    FORE = globals()['FORE'] if view == 'FP' else CARRY    # TP: the left hand on the carrying handle
    rr, rl = REACH['r'], REACH['l']
    wr, wl = WORK_ELBOW['r'], WORK_ELBOW['l']
    rk = [(0, CRANK, rr, CRANK_U)]
    lk = [(0, FORE, rl, FORE_U)]
    if clip == 'Equip':
        rk = [(0, FREE_R, rr, CRANK_U), (10, FREE_R, rr, CRANK_U), (28, CRANK, rr, CRANK_U)]
        lk = [(0, FREE_L, rl, FORE_U), (14, FREE_L, rl, FORE_U), (34, FORE, rl, FORE_U)]
    elif clip == 'Holster':
        rk = [(0, CRANK, rr, CRANK_U), (30, CRANK, rr, CRANK_U), (46, FREE_R, rr, CRANK_U)]
        lk = [(0, FORE, rl, FORE_U), (10, FORE, rl, FORE_U), (30, FREE_L, rl, FORE_U)]
    elif clip == 'Reload':
        lift_a = ('pt', Vector((MG.BELT_X, 0.060, 0.120)))
        lift_b = ('pt', Vector((MG.BELT_X + 0.01, 0.130, 0.090)))
        rk = [(0, CRANK, rr, CRANK_U), (4, CRANK, rr, CRANK_U), (12, LATCH, wr, (0.0, 0.2, 0.98)), (14, LATCH, wr, (0.0, 0.2, 0.98)),
              (20, LATCH, wr, (0.0, 0.3, 0.95)), (26, BELT_GRAB, wr, (0.0, 0.4, 0.92)), (28, BELT_GRAB, wr, (0.0, 0.4, 0.92)),
              (33, lift_a, wr, (0.0, 0.5, 0.86)), (38, lift_b, wr, (-0.2, 0.5, 0.84)), (42, ('v', (24.0, 20.0, -30.0), Vector((-.26, -.20, .50))), wr, CRANK_U),
              (52, CRANK, rr, CRANK_U), (112, CRANK, rr, CRANK_U), (118, LATCH, wr, (0.0, 0.2, 0.98)), (120, LATCH, wr, (0.0, 0.2, 0.98)),
              (126, LATCH, wr, (0.0, 0.2, 0.98)), (128, LATCH, wr, (0.0, 0.2, 0.98)), (134, CRANK, rr, CRANK_U), (150, CRANK, rr, CRANK_U)]
        down_l = ('v', (14.0, -24.0, -66.0), Vector((.24, -.12, .30)))
        lk = [(0, FORE, rl, FORE_U), (42, FORE, rl, FORE_U), (50, BOX_H, wl, (-0.3, -0.9, 0.2)), (64, BOX_H, wl, (-0.3, -0.9, 0.2)),
              (68, down_l, wl, FORE_U), (72, down_l, wl, FORE_U), (74, BOX_H, wl, (-0.3, -0.9, 0.2)), (92, BOX_H, wl, (-0.3, -0.9, 0.2)),
              (96, SLOT_H, wl, (-0.2, -0.6, 0.75)), (98, SLOT_H, wl, (-0.2, -0.6, 0.75))]
        # pulling the leader up to the feed slot, then three pushes into it
        feed_out = ('pt', Vector((MG.BELT_X, MG.REC_HW + 0.032, 0.040)))
        feed_in = ('pt', Vector((MG.BELT_X, MG.REC_HW + 0.016, 0.036)))
        lk += [(104, ('pt', Vector((MG.BELT_X, MG.REC_HW + 0.060, 0.050))), wl, (-0.2, -0.6, 0.75)), (108, feed_out, wl, (-0.1, -0.7, 0.7)),
               (111, feed_in, wl, (-0.1, -0.7, 0.7)), (112, feed_out, wl, (-0.1, -0.7, 0.7)), (115, feed_in, wl, (-0.1, -0.7, 0.7)),
               (116, feed_out, wl, (-0.1, -0.7, 0.7)), (120, feed_in, wl, (-0.1, -0.7, 0.7)), (122, feed_out, wl, (-0.1, -0.7, 0.7)),
               (128, KNOB, wl, (-0.1, -0.8, 0.6)), (137, KNOB, wl, (-0.1, -0.8, 0.6)), (140, ('pt', MG.COCK_KNOB + Vector((-0.06, 0.05, 0.01))), wl, FORE_U),
               (148, FORE, rl, FORE_U), (150, FORE, rl, FORE_U)]
    elif clip == 'Overheat':
        burnt = ('v', (16.0, -36.0, -44.0), Vector((.30, -.18, .36)))
        lk = [(0, FORE, rl, FORE_U), (8, FORE, rl, FORE_U), (13, burnt, rl, FORE_U), (36, burnt, rl, FORE_U), (46, FORE, rl, FORE_U), (60, FORE, rl, FORE_U)]
    elif clip == 'Inspect':
        if view == 'FP':
            tap = ('box', Vector((BOX_C.x, MG.BOX_MAX.y + 0.016, BOX_C.z)))
            hit = ('box', Vector((BOX_C.x, MG.BOX_MAX.y + 0.008, BOX_C.z)))
        else:
            # third person: the box is out of the short arm's reach, a slap on the jacket instead
            tap = ('w', Vector((0.34, 0.035, 0.075)))
            hit = ('w', Vector((0.34, 0.030, 0.062)))
        lk = [(0, FORE, rl, FORE_U), (34, FORE, rl, FORE_U), (41, tap, wl, (-0.3, -0.9, 0.2)), (44, hit, wl, (-0.3, -0.9, 0.2)),
              (48, tap, wl, (-0.3, -0.9, 0.2)), (56, FORE, rl, FORE_U), (180, FORE, rl, FORE_U)]
    elif clip == 'Discard':
        drop_r = ('v', (16.0, 30.0, -62.0), Vector((-.27, -.06, .34)))
        drop_l = ('v', (16.0, -30.0, -62.0), Vector((.27, -.06, .34)))
        off_r = ('v', (26.0, 38.0, -42.0), Vector((-.36, -.20, .40)))
        rk = [(0, CRANK, rr, CRANK_U), (RELEASE_FRAME, CRANK, rr, CRANK_U), (30, off_r, rr, CRANK_U), (40, drop_r, rr, CRANK_U)]
        lk = [(0, FORE, rl, FORE_U), (14, FORE, rl, FORE_U), (22, ('v', (22.0, -28.0, -44.0), Vector((.28, -.10, .36))), rl, FORE_U), (40, drop_l, rl, FORE_U)]
    right = keyed_hand(rk, f, view, M, ctx)
    left = keyed_hand(lk, f, view, M, ctx)
    if clip == 'Overheat':
        # the scalded hand shakes
        shake = curve(f, [(12, 0), (15, 1), (32, 1), (36, 0)])
        p, r = left
        p = p + Vector((0.012 * math.sin(f * 2.4), 0.0, 0.010 * math.sin(f * 3.1 + 0.6))) * shake
        if r is not None:
            r = (Quaternion(Vector((1, 0, 0)), 0.35 * math.sin(f * 2.4) * shake).to_matrix() @ r)
        left = (p, r)
    return right, left


# ------------------------------------------------------------------------------------------------ events

def events(clip):
    def ev(frame_, name, socket=None):
        e = {'frame': frame_, 'name': name}
        if socket:
            e['socket'] = socket
        return e
    if clip == 'FireStart':
        return [ev(0, 'CrankTick', 'socket_crank_handle')] + [ev(s, 'Fire', 'socket_muzzle') for s in SHOTS['FireStart']]
    if clip == 'FireLoop':
        return [ev(0, 'CrankTick', 'socket_crank_handle')] + [ev(s, 'Fire', 'socket_muzzle') for s in SHOTS['FireLoop']]
    if clip == 'FireEnd':
        return [ev(8, 'CrankTick', 'socket_crank_handle')]
    if clip == 'Reload':
        sockets = {'CoverOpen': 'socket_cover_latch', 'BeltOut': 'socket_feed', 'BoxOut': 'socket_box', 'BoxIn': 'socket_box',
                   'BeltIn': 'socket_feed', 'CoverClose': 'socket_cover_latch', 'Charge': 'socket_cocking'}
        return [ev(fr, name, sockets[name]) for name, fr in sorted(R_EVENTS.items(), key=lambda kv: kv[1])]
    if clip == 'Overheat':
        return [ev(STEAM_VENT, 'SteamVent', 'socket_steam')]
    if clip == 'Equip':
        return [ev(6, 'DrawWeapon'), ev(46, 'Ready')]
    if clip == 'Holster':
        return [ev(42, 'StowWeapon')]
    if clip == 'DryFire':
        return [ev(4, 'DryClick', 'socket_crank_handle')]
    if clip == 'Discard':
        return [ev(EMPTY_FRAME, 'Empty', 'socket_crank_handle'), ev(RELEASE_FRAME, 'Release', 'socket_grip_r')]
    return []


def release_intervals(clip):
    if clip == 'Equip':
        return [[0, 34]]
    if clip == 'Holster':
        return [[10, 48]]
    if clip == 'Reload':
        return [[4, 52], [42, 148], [112, 134]]
    if clip == 'Overheat':
        return [[8, 46]]
    if clip == 'Inspect':
        return [[34, 56]]
    if clip == 'Discard':
        return [[14, 40]]
    return []


# ------------------------------------------------------------------------------------------------ animate

def head_look(clip, f):
    if clip == 'Reload':
        return curve(f, [(0, 0), (12, 1), (138, 1), (150, 0)])
    if clip == 'Inspect':
        return curve(f, [(0, 0), (20, 1), (160, 1), (180, 0)])
    if clip == 'Overheat':
        return curve(f, [(0, 0), (8, 0.8), (44, 0.8), (56, 0)])
    if clip == 'Discard':
        return curve(f, [(0, 0), (6, 1), (16, 1), (RELEASE_FRAME + 2, 0.3), (40, 0)])
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
    rest = {n: gun.data.bones[n].matrix_local.copy() for n in bone_names}
    box_last = Matrix.Identity(4)
    MEMO.clear()
    for f in range(end + 1):
        scene.frame_set(f)
        for pb in arm.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        if view == 'TP':
            look = head_look(clip, f)
            local_rot(arm, 'head', (0, 0, 1), 30 - 22 * look)
            if look:
                pb = arm.pose.bones['head']
                pb.rotation_quaternion = pb.rotation_quaternion @ Quaternion(pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0)),
                                                                             math.radians(14 * look))
            # a heavy gun: the bean leans back a little, more when sprinting with it across the chest
            local_rot(arm, 'spine_01', (1, 0, 0), -4.0 if clip != 'Sprint' else 6.0)
            k = kick(clip, f)
            if k:
                local_rot(arm, 'spine_02', (1, 0, 0), -3.5 * k)
        phase = 0.0
        if clip in ('Walk', 'Sprint'):
            phase = 2 * math.pi * f / end * 4
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
        Bx = box_matrix(clip, f, view)
        if Bx is not None:
            box_last = Bx
        ctx = {'phi': crank_phi(clip, f), 'cover': cover_angle(clip, f), 'cock': cock_slide(clip, f), 'box': Bx, 'box_last': box_last}
        (rp, rrot), (lp, lrot) = clip_hands(clip, f, view, M, ctx)
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev_arm = arm.evaluated_get(dg)
        bm = {p.name: p.matrix.copy() for p in ev_arm.pose.bones}
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
                error, scale = solve_tp_arm(arm, bm, side, point, max_stretch=1.40)
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
            rp = rp + Vector((0, 0, lift))
            lp = lp + Vector((0, 0, lift))
        bean_samples.append(bm)
        grips.append(dict(frame=f, left=[*lp], right=[*rp]))
        # the gun: rigid on the pose; crank, cover, cocking knob, steam flap, box and belt animated
        mats = {n: M @ rest[n] for n in bone_names}
        part = {'crank': crank_matrix(ctx['phi']), 'feed_cover': cover_matrix(ctx['cover']),
                'cocking_handle': Matrix.Translation((-ctx['cock'], 0, 0)), 'steam_cap': steam_matrix(steam_angle(clip, f))}
        for n, D in part.items():
            if n in mats:
                mats[n] = M @ D @ rest[n]
        if 'ammo_box' in mats:
            mats['ammo_box'] = (M @ Bx @ rest['ammo_box']) if Bx is not None else (M @ box_last @ rest['ammo_box'] @ HIDDEN)
        for n, (W, vis) in belt_bones(clip, f, M, view, {'r': rp, 'l': lp}).items():
            if n in mats:
                D = W @ REST_INV[n]
                mats[n] = M @ D @ rest[n] if vis else M @ D @ rest[n] @ HIDDEN
        # sockets follow their parent's motion
        for n in bone_names:
            b = gun.data.bones[n]
            if n.startswith('socket_') and b.parent:
                mats[n] = mats[b.parent.name] @ rest[b.parent.name].inverted() @ rest[n]
        gun_samples.append(mats)
    ba = bake_matrices(arm, bean_samples, 'A_Bean_MachineGun_' + view + '_' + clip)
    ga = bake_matrices(gun, gun_samples, 'A_MachineGun_' + view + '_' + clip)
    scene.frame_set(0)
    bpy.context.view_layer.update()
    return dict(bean_action=ba.name, weapon_action=ga.name, events=events(clip), hand_error_max_m=errors,
                upperarm_scale_max=max(stretches), frame_start=0, frame_end=end, fps=FPS, loop=clip in LOOPS,
                authored_method='T-272: FP arms placed through the game camera (no stretch), TP two-segment arms; hip pose '
                                'measured by the game on Idle frame 0; crank turned by the right hand (a shot every quarter '
                                'turn), belt conveyor on a chain of bones, per-shot recoil, two-hand box and belt reload.',
                grip_targets=grips)


def fp_framing():
    """What the game needs to frame the FP clips like this pack's camera (DA_MachineGunRules). No aim pose: the aim is
    switched off in the game."""
    hip = fp_pose('hip')
    hp, hy, hr = rotator_in_view(hip)
    return dict(camera_location_m=[*FP_CAMERA], horizontal_fov_deg=90.0, grip_socket='socket_grip_r', align_bone='gun_root',
                hip_grip_location_cm=[round(v, 2) for v in to_view(hip @ GRIP_R)], hip_rotation_pyr_deg=[round(v, 3) for v in (hp, hy, hr)],
                muzzle_in_view_cm=[round(v, 2) for v in to_view(hip @ MUZZLE)],
                fore_grip_in_view_cm=[round(v, 2) for v in to_view(hip @ GRIP_L)],
                aim='none: the machine gun is fired from the hip (no AimIdle clip; bAimPoseValid stays false)',
                note='HipGripLocation / HipRotation reproduce this camera exactly (Idle frame 0 = hip pose, socket_grip_r = the crank '
                     'handle at rest). Or bAlignFirstPersonToView = false with FirstPersonViewLocation = camera_location_m.')
