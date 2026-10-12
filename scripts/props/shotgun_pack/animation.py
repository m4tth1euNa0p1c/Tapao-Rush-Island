"""Bean pump shotgun handling, first (FP) and third (TP) person, 30 fps, baked on the bean skeleton and on the shotgun
skeleton (T-247). Same 16 clips, lengths, bones, sockets and events as the second PC's bean-pirate-shotgun v0.1
(the generated C++ table PirateWeaponClips.inl stays valid), plus Discard (owner, 2026-10-09).

Metres, Blender axes: the bean looks along -Y, its right side is -X, +Z up. Weapon local: +X to the muzzle, +Z up,
-Y the right side (ejection port). The weapon root is baked in bean coordinates (both actor roots at one transform).

First person is authored through the game's camera (eye at (0, 0, 0.80) in bean space, 90 deg horizontal), with the
maths and the arm solvers of the musket pack (scripts/props/musket_pack/animation.py): poses are given by where a
weapon point sits in the view (cm; X forward, Y right, Z up) and where the barrel and the top of the gun point.
  - Idle frame 0 is the hip pose; AimIdle frame 0 the aim pose: the brass bead on the view axis, weapon centred
    (the C++ measures both and frames them with HipGripLocation / HipRotation and AimMuzzleLocation / AimRotation:
    fp_framing() gives the values that reproduce this camera);
  - Call of Duty style: FirePump kicks the barrel up 8 deg and the gun back 4 cm in two frames, the pump racks hard
    after the shot and the spent shell leaves at EjectSpentShell; ReloadInsert rolls the gun on its side, loading
    port toward the eye, the shell visible in the left mitten from ShellPickup to InsertSlug; Sprint carries the gun
    low and tilted with a marked bob; Idle breathes; Inspect turns both sides; Discard racks the empty gun and
    throws it away to the right and down (Empty, Release).
"""
from __future__ import annotations

import importlib.util
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

import shotgun_model as SM

HERE = Path(__file__).resolve().parent
_spec = importlib.util.spec_from_file_location("musket_animation", HERE.parents[0] / "musket_pack" / "animation.py")
MA = importlib.util.module_from_spec(_spec)
sys.modules["musket_animation"] = MA
_spec.loader.exec_module(MA)

smooth, curve, segment, frame, axes, blend = MA.smooth, MA.curve, MA.segment, MA.frame, MA.axes, MA.blend
FP_CAMERA, FWD, RIGHT, UP, FP_LENS = MA.FP_CAMERA, MA.FWD, MA.RIGHT, MA.UP, MA.FP_LENS
camera_matrix, vp, vv, to_view, rotator_in_view = MA.camera_matrix, MA.vp, MA.vv, MA.to_view, MA.rotator_in_view
fp_arm_transform, solve_fp_arm, solve_tp_arm, solve_leg = MA.fp_arm_transform, MA.solve_fp_arm, MA.solve_tp_arm, MA.solve_leg
local_rot, set_matrices, bake_matrices, linear = MA.local_rot, MA.set_matrices, MA.bake_matrices, MA.linear

FPS = 30
CLIPS = {"Equip": 54, "Idle": 90, "Walk": 120, "Sprint": 90, "AimIn": 30, "AimIdle": 90, "AimOut": 30, "FirePump": 72,
         "AimFirePump": 72, "ReloadStart": 42, "ReloadInsert": 54, "ReloadEnd": 42, "ReloadEmpty": 240, "DryFire": 42,
         "Inspect": 180, "Holster": 54, "Discard": 40}
LOOPS = {"Idle", "Walk", "Sprint", "AimIdle"}
INSERT_STARTS = (32, 87, 142)          # ReloadEmpty: the three ReloadInsert timelines (v0.1)

# weapon points (weapon local, metres)
GRIP_R = SM.SOCKETS["grip_r"][1]
GRIP_L = SM.SOCKETS["grip_l"][1]        # on the pump: moves with it
MUZZLE = SM.SOCKETS["muzzle"][1]
LOAD_PORT = SM.SOCKETS["load_port"][1]
EJECT = SM.SOCKETS["eject_port"][1]
SIGHT = Vector(SM.SIGHT)
PUMP_TRAVEL = SM.PUMP_TRAVEL
BOLT_RATIO = SM.BOLT_TRAVEL / SM.PUMP_TRAVEL


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


RECEIVER = Vector((0.03, -0.02, 0.08))      # centre of the receiver's right face (ejection port)
FP_POSES = {
    # hip: low on the right, barrel toward the middle of the screen, a little cant inward
    "ready": (GRIP_R, (30.0, 14.0, -17.0), (0.975, -0.14, 0.11), (0.0, -0.14, 1.0)),
    # aim: the brass bead exactly on the view axis, 70 cm ahead; the barrel pitched up 2.2 deg so the rear of the
    # receiver (and the cocked hammer) sits 1.6 cm under the sight line: the bead stays visible
    "aim": (SIGHT, (70.0, 0.0, 0.0), (1.0, 0.0, 0.038), (0.0, 0.0, 1.0)),
    # reload: the gun across the body, muzzle up to the left, underside (loading port) toward the eye at the centre
    "reload": (LOAD_PORT, (34.0, -4.0, -9.0), (0.28, -0.76, 0.59), (0.92, 0.04, -0.39)),
    # sprint: pulled to the chest, low on the right, canted, muzzle toward the lower left corner
    "sprint": (GRIP_R, (27.0, 13.0, -22.0), (0.74, -0.66, -0.10), (0.10, -0.55, 0.83)),
    # put away below the screen
    "holster": (GRIP_R, (18.0, 24.0, -62.0), (0.40, 0.30, -0.86), (0.85, 0.0, 0.45)),
    # inspect: brought up and turned, both sides shown by the clip's roll
    "inspect": (GRIP_R, (28.0, 8.0, -16.0), (0.74, -0.63, 0.24), (0.0, 0.1, 1.0)),
    # discard: looks at the empty gun, receiver in the middle of the view, ejection port toward the eye
    "look": (RECEIVER, (34.0, 2.0, -8.0), (0.30, -0.85, 0.44), (0.34, 0.52, 0.77)),
    # discard: where the gun leaves the hand, out to the right and down
    "toss": (GRIP_R, (38.0, 44.0, -32.0), (0.25, 0.80, -0.55), (0.90, -0.15, 0.40)),
}
FP_SHOULDER = MA.FP_SHOULDER
REACH = {"r": (8.0, 26.0, -48.0), "l": (14.0, -30.0, -46.0)}
RELOAD_ELBOW = {"r": (10.0, 34.0, -40.0), "l": (6.0, -30.0, -44.0)}
PINCH = Vector((0.078, 0.0, -0.004))        # the shell just past the fingertips of the left mitten (radius 6.2 cm; hand frame: x fingers, z back)
TP_PINCH = Vector((0.0, -0.034, 0.012))


def fp_pose(name):
    anchor, where, x_dir, up_dir = FP_POSES[name]
    return view_pose_dirs(anchor, where, x_dir, up_dir)


# ------------------------------------------------------------------------------------------- third person poses

def tp_poses():
    """Bean space (v0.1 poses for ready / aim / reload / holster / inspect, adapted to Bean's short arms)."""
    return dict(ready=frame((.955, -.25, .16), (-.16, -.25, .51), -8),
                aim=frame((.855, -.515, .02), (-.17, -.20, .615)),
                reload=frame((.91, -.20, .36), (-.02, -.25, .53), -65),
                sprint=frame((.90, -.26, -.35), (-.12, -.22, .50), 8),
                holster=frame((.15, -.16, -.975), (-.30, -.20, .43), 12),
                inspect=frame((.88, -.35, .32), (-.16, -.25, .59), -25),
                look=frame((.84, -.30, .45), (-.15, -.27, .52), -42),
                toss=frame((-.30, -.45, -.84), (-.36, -.28, .44), 35))


def poses(view):
    if view == "FP":
        return {k: fp_pose(k) for k in FP_POSES}
    return tp_poses()


# --------------------------------------------------------------------------------------------- weapon pose

def insertion_local(clip, f):
    """Frame of the ReloadInsert timeline shown at f (None outside an insertion)."""
    if clip == "ReloadInsert":
        return f
    if clip == "ReloadEmpty":
        for start in INSERT_STARTS:
            if start <= f <= start + 54:
                return f - start
    return None


DISCARD_RELEASE = 24
DISCARD_EMPTY = 9


def discard_flight(p, f, view):
    """Weapon root after Release: free flight from the toss pose, lands and rests on the floor."""
    toss = p["toss"]
    dt = (f - DISCARD_RELEASE) / FPS
    if view == "FP":
        v = vv(0.9, 2.6, 0.45) * 2.9
        spin_axis = vv(0.6, 0.2, -0.75)
    else:
        v = Vector((-1.9, -0.5, 0.9))
        spin_axis = Vector((0.3, -0.9, 0.2))
    g = Vector((0, 0, -9.81))
    floor = 0.035
    z0 = toss.translation.z
    # time of the landing (z = floor)
    a, b, c = -4.905, v.z, z0 - floor
    disc = b * b - 4 * a * c
    land = (-b - math.sqrt(disc)) / (2 * a) if disc > 0 else 1e9
    t = min(dt, land)
    pos = toss.translation + v * t + 0.5 * g * t * t
    if dt > land:
        slide = min(dt - land, 0.25)
        pos += Vector((v.x, v.y, 0)) * slide * 0.3
        pos.z = floor
    rot = Quaternion(spin_axis.normalized(), min(dt, land + 0.08) * 14.0)
    m = (rot.to_matrix().to_4x4()) @ toss.to_3x3().to_4x4()
    m.translation = pos
    return m


def weapon_pose(clip, f, view):
    p = poses(view)
    end = CLIPS[clip]
    base = p["ready"]
    if clip in ("AimIdle", "AimFirePump"):
        base = p["aim"]
    elif clip == "AimIn":
        # arrives at 26 then settles (small overshoot on the last frames)
        w = smooth(0, 26, f)
        base = blend(p["ready"], p["aim"], w)
        base = base @ Matrix.Translation((0, 0, 0.004 * math.sin(math.pi * smooth(18, 30, f))))
    elif clip == "AimOut":
        base = blend(p["aim"], p["ready"], smooth(0, 26, f))
        base = base @ Matrix.Translation((0, 0, -0.004 * math.sin(math.pi * smooth(18, 30, f))))
    elif clip == "Equip":
        base = blend(p["holster"], p["ready"], smooth(0, 44, f))
        base = base @ Matrix.Rotation(math.radians(3.0 * math.sin(math.pi * smooth(40, 52, f))), 4, "Y")
    elif clip == "Holster":
        base = blend(p["ready"], p["holster"], smooth(6, end, f))
    elif clip == "Sprint":
        base = p["sprint"]
    elif clip == "ReloadStart":
        base = blend(p["ready"], p["reload"], smooth(0, 34, f))
    elif clip == "ReloadInsert":
        base = p["reload"]
    elif clip == "ReloadEnd":
        base = blend(p["reload"], p["ready"], smooth(5, end, f))
    elif clip == "ReloadEmpty":
        base = blend(p["ready"], p["reload"], smooth(0, 30, f))
        if f > 202:
            base = blend(p["reload"], p["ready"], smooth(202, 238, f))
    elif clip == "Inspect":
        base = blend(p["ready"], p["inspect"], smooth(0, 38, f))
        roll = curve(f, [(0, 0), (38, 0), (72, -55), (112, 50), (145, 0), (180, 0)])
        base = base @ Matrix.Rotation(math.radians(roll), 4, "X")
        if f > 145:
            base = blend(base, p["ready"], smooth(145, 180, f))
    elif clip == "Discard":
        if f <= 6:
            base = blend(p["ready"], p["look"], smooth(0, 6, f))
        elif f <= 14:
            base = p["look"]
        elif f <= DISCARD_RELEASE:
            base = blend(p["look"], p["toss"], smooth(14, DISCARD_RELEASE, f))
        else:
            base = discard_flight(p, f, view)
        if f <= 16:
            # the empty rack jolts the gun
            j = curve(f, [(4, 0), (9, 1), (12, 1), (16, 0)])
            base = base @ Matrix.Translation((-0.012 * j, 0, 0)) @ Matrix.Rotation(math.radians(-3.0 * j), 4, "Y")
    if clip in ("FirePump", "AimFirePump"):
        # sharp kick: barrel up 8 deg and 4 cm back in two frames, then a damped return while the pump racks
        kick = curve(f, [(0, 0), (11, 0), (13, 1), (17, .55), (24, .18), (32, 0), (end, 0)])
        amount = 1.0 if clip == "FirePump" else 0.85
        base = base @ Matrix.Translation((-0.04 * kick * amount, 0, 0.004 * kick)) @ Matrix.Rotation(math.radians(-8.0 * kick * amount), 4, "Y")
        # the rack tugs the gun a little toward the body and back
        rack = curve(f, [(21, 0), (30, 1), (33, 1), (38, 1), (47, 0)])
        base = base @ Matrix.Translation((-0.006 * rack, 0, -0.003 * rack)) @ Matrix.Rotation(math.radians(1.5 * rack), 4, "Z")
    if clip in ("ReloadInsert", "ReloadEmpty"):
        local = insertion_local(clip, f)
        if local is not None:
            # the push of the shell into the port
            j = curve(local, [(32, 0), (36, 1), (41, 0)])
            base = base @ Matrix.Translation((0.004 * j, 0, 0.003 * j))
    if clip in ("Idle", "AimIdle"):
        q = math.sin(2 * math.pi * f / end)
        q2 = math.sin(4 * math.pi * f / end + 0.7)
        amp = 1.0 if clip == "Idle" else 0.35
        base = base @ Matrix.Translation((0, .0016 * q * amp, .0032 * q * amp + .0008 * q2 * amp)) @ Matrix.Rotation(math.radians(0.6 * q * amp), 4, "Y")
    if clip == "DryFire":
        j = curve(f, [(10, 0), (12, 1), (18, 0)])
        base = base @ Matrix.Translation((-0.004 * j, 0, 0)) @ Matrix.Rotation(math.radians(-1.0 * j), 4, "Y")
    if clip in ("Walk", "Sprint"):
        cycles = 4
        q = 2 * math.pi * f / end * cycles
        amp = 1.0 if clip == "Walk" else 2.2
        base = base @ Matrix.Translation((.005 * amp * math.sin(q), .006 * amp * math.cos(q), .007 * amp * math.sin(q * 2)))
        base = base @ Matrix.Rotation(math.radians((2.5 if clip == "Walk" else 6.0) * math.sin(q)), 4, "X")
        base = base @ Matrix.Rotation(math.radians((1.2 if clip == "Walk" else 3.0) * math.sin(q * 2)), 4, "Y")
    return base


def pump_distance(clip, f):
    if clip in ("FirePump", "AimFirePump"):
        return curve(f, [(0, 0), (21, 0), (30, .062), (33, PUMP_TRAVEL), (38, PUMP_TRAVEL), (46, .004), (47, 0)])
    if clip == "ReloadEmpty":
        return curve(f, [(0, 0), (208, 0), (220, PUMP_TRAVEL), (226, PUMP_TRAVEL), (236, 0)])
    if clip == "Inspect":
        return curve(f, [(0, 0), (83, 0), (94, .030), (104, .030), (119, 0)])
    if clip == "Discard":
        return curve(f, [(0, 0), (4, 0), (DISCARD_EMPTY, PUMP_TRAVEL), (12, PUMP_TRAVEL), (16, 0)])
    return 0.0


def trigger_angle(clip, f):
    if clip in ("FirePump", "AimFirePump", "DryFire"):
        return curve(f, [(0, 0), (10, 0), (12, 15), (17, 15), (22, 0)])
    return 0.0


# ------------------------------------------------------------------------------------------------- hands

def hand_frame(point, elbow_cm, back):
    d = (point - vp(*elbow_cm)).normalized()
    return axes(d, vv(*back))


GRIP_U, FORE_U = (0, .55, .85), (0, -.45, -.89)
FREE_R, FREE_L = ("v", (20.0, 26.0, -58.0), Vector((-.29, -.10, .36))), ("v", (20.0, -26.0, -58.0), Vector((.29, -.10, .36)))
R_GRIP = ("w", GRIP_R)
L_GRIP = ("wp", GRIP_L)


def resolve(spec, view, M, pump):
    kind = spec[0]
    if kind == "w":
        return M @ Vector(spec[1]), False
    if kind == "wp":
        return M @ (Vector(spec[1]) - Vector((pump, 0, 0))), False
    if kind == "pinch":
        return M @ Vector(spec[1]), True
    if kind == "v":
        return (vp(*spec[1]) if view == "FP" else Vector(spec[2])), False
    raise ValueError(spec)


def keyed_hand(keys, f, view, M, pump):
    """Hand centre (bean space) and hand rotation (FP; None in TP) at frame f along keys
    (frame, target, FP elbow (view cm), FP back of the hand (view dir))."""
    i, w = segment(f, keys)
    k0, k1 = keys[i], keys[min(i + 1, len(keys) - 1)]
    out = []
    for k in (k0, k1):
        p, pinch = resolve(k[1], view, M, pump)
        rot = hand_frame(p, k[2], k[3]) if view == "FP" else None
        if pinch and rot is not None:
            p = p - rot @ PINCH
        elif pinch:
            p = p - TP_PINCH
        out.append((p, rot))
    p = out[0][0].lerp(out[1][0], w)
    rot = None
    if view == "FP":
        rot = out[0][1].to_quaternion().slerp(out[1][1].to_quaternion(), w).to_matrix()
    return p, rot


def insert_left_keys(view):
    """Left hand of one ReloadInsert timeline (54 frames): releases the pump (7), picks a shell at the belt (17),
    brings it under the loading port (30), pushes it in (36), comes back on the pump (48)."""
    belt = ("v", (15.0, -27.0, -34.0), Vector((.235, -.205, .365)))
    belt_up = ("v", (17.0, -24.0, -28.0), Vector((.235, -.215, .395)))
    if view == "FP":
        below = ("pinch", LOAD_PORT + Vector((-0.010, 0.0, -0.070)))
        contact = ("pinch", LOAD_PORT + Vector((0.004, 0.0, -0.030)))
        after = ("pinch", LOAD_PORT + Vector((-0.010, 0.0, -0.080)))
    else:
        below = ("pinch", LOAD_PORT + Vector((-0.020, 0.0, -0.080)))
        contact = ("pinch", LOAD_PORT + Vector((-0.010, 0.0, -0.030)))
        after = ("pinch", LOAD_PORT + Vector((-0.020, 0.0, -0.085)))
    e, u = RELOAD_ELBOW["l"], (-0.55, -0.25, -0.80)
    return [(0, L_GRIP, REACH["l"], FORE_U), (7, L_GRIP, REACH["l"], FORE_U), (17, belt, e, (-0.1, -0.9, -0.4)), (20, belt_up, e, (-0.1, -0.9, -0.4)),
            (30, below, e, u), (36, contact, e, u), (41, after, e, u), (48, L_GRIP, REACH["l"], FORE_U), (54, L_GRIP, REACH["l"], FORE_U)]


def clip_hands(clip, f, view, M, pump):
    """((right point, rotation), (left point, rotation)) of the hands for clip / frame; rotation None in TP."""
    rk = [(0, R_GRIP, REACH["r"], GRIP_U)]
    lk = [(0, L_GRIP, REACH["l"], FORE_U)]
    local = insertion_local(clip, f)
    if local is not None:
        lk = insert_left_keys(view)
        return keyed_hand(rk, f, view, M, pump), keyed_hand(lk, local, view, M, pump)
    if clip in ("FirePump", "AimFirePump", "DryFire"):
        rk = [(0, R_GRIP, REACH["r"], GRIP_U), (11, R_GRIP, REACH["r"], GRIP_U), (13, R_GRIP, REACH["r"], (0, .40, .92)), (20, R_GRIP, REACH["r"], GRIP_U)]
    if clip == "Equip":
        rk = [(0, FREE_R, REACH["r"], GRIP_U), (10, FREE_R, REACH["r"], GRIP_U), (30, R_GRIP, REACH["r"], GRIP_U)]
        lk = [(0, FREE_L, REACH["l"], FORE_U), (14, FREE_L, REACH["l"], FORE_U), (38, L_GRIP, REACH["l"], FORE_U)]
    if clip == "Holster":
        rk = [(0, R_GRIP, REACH["r"], GRIP_U), (34, R_GRIP, REACH["r"], GRIP_U), (54, FREE_R, REACH["r"], GRIP_U)]
        lk = [(0, L_GRIP, REACH["l"], FORE_U), (11, L_GRIP, REACH["l"], FORE_U), (34, FREE_L, REACH["l"], FORE_U)]
    if clip == "Discard":
        drop_r = ("v", (16.0, 30.0, -62.0), Vector((-.27, -.06, .34)))
        drop_l = ("v", (16.0, -30.0, -62.0), Vector((.27, -.06, .34)))
        off_r = ("v", (26.0, 38.0, -42.0), Vector((-.36, -.20, .40)))
        rk = [(0, R_GRIP, REACH["r"], GRIP_U), (DISCARD_RELEASE, R_GRIP, REACH["r"], GRIP_U), (30, off_r, REACH["r"], GRIP_U), (40, drop_r, REACH["r"], GRIP_U)]
        lk = [(0, L_GRIP, REACH["l"], FORE_U), (14, L_GRIP, REACH["l"], FORE_U), (24, ("v", (22.0, -28.0, -44.0), Vector((.28, -.10, .36))), REACH["l"], FORE_U),
              (40, drop_l, REACH["l"], FORE_U)]
    return keyed_hand(rk, f, view, M, pump), keyed_hand(lk, f, view, M, pump)


# ------------------------------------------------------------------------------------------------ shells

def load_shell_world(clip, f, view, M, left_point, left_rot):
    """Pose of the full shell on the bone shell_load (bean space) while it is in the left hand, else None."""
    local = insertion_local(clip, f)
    if local is None or local < 17 or local > 36:
        return None
    pinch = left_point + (left_rot @ PINCH if left_rot is not None else TP_PINCH)
    # held along the weapon's barrel axis, ready to be pushed in (crimp first, toward +X)
    x = (M.to_3x3() @ Vector((1, 0, 0))).normalized()
    up = M.to_3x3() @ Vector((0, 0, 1))
    rot = axes(x, up)
    if local >= 30:
        target = M @ (LOAD_PORT + Vector((0.004, 0, 0.006)))
        w = smooth(30, 36, local)
        pinch = pinch.lerp(target, w)
    m = rot.to_4x4()
    m.translation = pinch
    return m


def eject_shell_world(clip, f, view, M30, floor=0.016):
    """Ballistic spent shell from the ejection port (frame 30 of the shot clips), one damped bounce, then rest."""
    if clip not in ("FirePump", "AimFirePump") or f < 30:
        return None
    origin = M30 @ EJECT
    velocity = M30.to_3x3() @ Vector((0.30, -1.35, 0.95))
    dt = (f - 30) / FPS
    root = (velocity.z + math.sqrt(velocity.z ** 2 + 19.62 * max(0.0, origin.z - floor))) / 9.81
    if dt <= root:
        pos = origin + velocity * dt + Vector((0, 0, -4.905 * dt * dt))
    else:
        hdt = dt - root
        pos = origin + velocity * root + Vector((0, 0, -4.905 * root * root))
        pos += Vector((velocity.x, velocity.y, 0)) * min(hdt, .34) * .28
        pos.z = floor + max(0.0, .105 * math.sin(min(1.0, hdt / .28) * math.pi))
    rot = M30.to_quaternion() @ Quaternion(Vector((.5, .3, 1)).normalized(), min(dt, root + .28) * 18)
    m = rot.to_matrix().to_4x4()
    m.translation = pos
    return m


# ------------------------------------------------------------------------------------------------ events

def events(clip):
    def ev(frame_, name, socket=None):
        e = {"frame": frame_, "name": name}
        if socket:
            e["socket"] = socket
        return e
    if clip in ("FirePump", "AimFirePump"):
        return [ev(12, "Fire", "socket_muzzle"), ev(21, "PumpBackStart"), ev(30, "EjectSpentShell", "socket_eject_port"), ev(33, "PumpBackEnd"),
                ev(38, "PumpForwardStart"), ev(46, "ChamberSlug"), ev(47, "PumpForwardEnd")]
    if clip == "ReloadInsert":
        return [ev(7, "LeftHandRelease"), ev(17, "ShellPickup"), ev(36, "InsertSlug", "socket_load_port"), ev(48, "LeftHandRegrab")]
    if clip == "ReloadEmpty":
        result = []
        for start in INSERT_STARTS:
            result += [dict(e, frame=e["frame"] + start) for e in events("ReloadInsert")]
        return result + [ev(208, "PumpBackStart"), ev(220, "PumpBackEnd"), ev(235, "ChamberSlug"), ev(236, "PumpForwardEnd")]
    if clip == "DryFire":
        return [ev(12, "DryClick")]
    if clip == "Equip":
        return [ev(8, "DrawWeapon"), ev(44, "Ready")]
    if clip == "Holster":
        return [ev(48, "StowWeapon")]
    if clip == "Discard":
        return [ev(DISCARD_EMPTY, "Empty"), ev(DISCARD_RELEASE, "Release")]
    return []


# ------------------------------------------------------------------------------------------------ animate

def animate(scene, bean, weapon, clip, view):
    arm = bean["armature"]
    gun = weapon["armature"]
    end = CLIPS[clip]
    scene.frame_start = 0
    scene.frame_end = end
    scene.render.fps = FPS
    bpy.context.window.scene = scene
    bean_samples, gun_samples, grips = [], [], []
    errors = {"l": 0., "r": 0.}
    stretches = []
    bone_names = [pb.name for pb in gun.pose.bones]
    M30 = weapon_pose(clip, 30, view) if clip in ("FirePump", "AimFirePump") else None
    for f in range(end + 1):
        scene.frame_set(f)
        for pb in arm.pose.bones:
            pb.matrix_basis = Matrix.Identity(4)
        local = insertion_local(clip, f)
        if view == "TP":
            aim_weight = 1. if clip in ("AimIdle", "AimFirePump") else smooth(0, end, f) if clip == "AimIn" else \
                1. - smooth(0, end, f) if clip == "AimOut" else 0.
            look_down = 0.0
            if local is not None:
                look_down = curve(local, [(0, 0), (8, 1), (44, 1), (52, 0)])
            if clip == "Discard":
                look_down = curve(f, [(0, 0), (5, 1), (14, 1), (24, 0.3), (32, 0)])
            if clip in ("ReloadStart", "ReloadEnd", "ReloadEmpty") and local is None:
                look_down = smooth(0, 30, f) if clip in ("ReloadStart", "ReloadEmpty") else 1. - smooth(5, end, f)
                if clip == "ReloadEmpty" and f > 202:
                    look_down = 1. - smooth(202, 238, f)
            local_rot(arm, "head", (0, 0, 1), 30 + 25 * aim_weight - 20 * look_down)
            if look_down:
                pb = arm.pose.bones["head"]
                pb.rotation_quaternion = pb.rotation_quaternion @ Quaternion(
                    pb.bone.matrix_local.to_3x3().inverted() @ Vector((1, 0, 0)), math.radians(14 * look_down))
            if clip == "Sprint":
                local_rot(arm, "spine_01", (1, 0, 0), 9.0)
            if clip in ("FirePump", "AimFirePump"):
                # the shoulders take the recoil
                kick = curve(f, [(11, 0), (13, 1), (20, 0.3), (30, 0)])
                local_rot(arm, "spine_02", (1, 0, 0), -4.0 * kick)
        phase = 0.0
        if clip in ("Walk", "Sprint"):
            cyc = 4
            phase = 2 * math.pi * f / end * cyc
            amp = 18 if clip == "Walk" else 28
            for side, off in (("l", 0), ("r", math.pi)):
                swing = math.sin(phase + off)
                local_rot(arm, "thigh_" + side, (1, 0, 0), amp * swing)
                local_rot(arm, "calf_" + side, (1, 0, 0), -max(0, -swing) * (25 if clip == "Walk" else 43))
                local_rot(arm, "foot_" + side, (1, 0, 0), -amp * swing * .5)
            arm.pose.bones["root"].location = (0, 0, .006 + .007 * (1 - math.cos(2 * phase)))
            arm.pose.bones["pelvis"].location = (0, -.027, 0)
        else:
            arm.pose.bones["root"].location = (0, 0, .003)
        M = weapon_pose(clip, f, view)
        pump = pump_distance(clip, f)
        (rp, rrot), (lp, lrot) = clip_hands(clip, f, view, M, pump)
        bpy.context.view_layer.update()
        dg = bpy.context.evaluated_depsgraph_get()
        ev = arm.evaluated_get(dg)
        bm = {p.name: p.matrix.copy() for p in ev.pose.bones}
        if clip in ("Walk", "Sprint"):
            for side, off in (("l", 0), ("r", math.pi)):
                step = phase + off
                v = arm.data.bones["foot_" + side].head_local.copy()
                v.y = .035 * math.cos(step)
                v.z = .053 + (max(0, math.sin(step)) * (.024 if clip == "Walk" else .040))
                solve_leg(arm, bm, side, v)
        for side, point, rot in (("r", rp, rrot), ("l", lp, lrot)):
            if view == "FP":
                error, scale = solve_fp_arm(arm, bm, side, point, rot)
            else:
                error, scale = solve_tp_arm(arm, bm, side, point)
            errors[side] = max(errors[side], error)
            stretches.append(scale)
        if view == "TP" and clip in ("Walk", "Sprint"):
            set_matrices(arm, bm)
            bpy.context.view_layer.update()
            dg = bpy.context.evaluated_depsgraph_get()
            minimum = 1e6
            for ob in bean["meshes"]:
                if "Boot" not in ob.name:
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
        grips.append(dict(frame=f, left=[*lp], right=[*rp], left_on_pump=local is None or not (7 < local < 48)))
        # the shotgun: rigid parts on the root pose; pump, bolt and trigger animated; shells from the hand / the port
        matrices = {n: M @ gun.data.bones[n].matrix_local for n in bone_names}
        for n in ("pump", "socket_grip_l", "socket_pump_grip"):
            if n in matrices:
                matrices[n] = M @ Matrix.Translation((-pump, 0, 0)) @ gun.data.bones[n].matrix_local
        if "bolt" in matrices:
            matrices["bolt"] = M @ Matrix.Translation((-pump * BOLT_RATIO, 0, 0)) @ gun.data.bones["bolt"].matrix_local
        if "trigger" in matrices:
            B = gun.data.bones["trigger"]
            head = B.head_local
            P = Matrix.Translation(head) @ Matrix.Rotation(math.radians(trigger_angle(clip, f)), 4, "Y") @ Matrix.Translation(-head)
            matrices["trigger"] = M @ P @ B.matrix_local
        hidden = Matrix.Diagonal((.0001, .0001, .0001, 1))
        if "shell_load" in matrices:
            B = gun.data.bones["shell_load"]
            held = load_shell_world(clip, f, view, M, lp, lrot)
            matrices["shell_load"] = (held @ Matrix.Translation(-LOAD_PORT) @ B.matrix_local) if held else M @ B.matrix_local @ hidden
        if "shell_eject" in matrices:
            B = gun.data.bones["shell_eject"]
            flying = eject_shell_world(clip, f, view, M30) if M30 is not None else None
            matrices["shell_eject"] = (flying @ Matrix.Translation(-EJECT) @ B.matrix_local) if flying else M @ B.matrix_local @ hidden
        for n in bone_names:
            b = gun.data.bones[n]
            if b.parent and n.startswith("socket_") and n not in ("socket_grip_l", "socket_pump_grip"):
                matrices[n] = matrices[b.parent.name] @ b.parent.matrix_local.inverted() @ b.matrix_local
        gun_samples.append(matrices)
    ba = bake_matrices(arm, bean_samples, "A_Bean_" + view + "_Shotgun_" + clip)
    ga = bake_matrices(gun, gun_samples, "A_Shotgun_" + view + "_" + clip)
    scene.frame_set(0)
    bpy.context.view_layer.update()
    return dict(bean_action=ba.name, weapon_action=ga.name, events=events(clip),
                hand_error_max_m=errors, upperarm_scale_max=max(stretches), frame_start=0, frame_end=end, fps=FPS,
                authored_method="T-247 local pack: FP arms placed through the game camera (no stretch), TP two-segment arms; "
                                "hip and aim poses measured by the game on Idle / AimIdle frame 0; pump, bolt, trigger and the two "
                                "shells keyed per frame.",
                grip_targets=grips)


def fp_framing():
    """What the game needs to frame the FP clips like this pack's camera (DA_ShotgunRules)."""
    hip = fp_pose("ready")
    aim = fp_pose("aim")
    hp, hy, hr = rotator_in_view(hip)
    ap, ay, ar = rotator_in_view(aim)
    return dict(camera_location_m=[*FP_CAMERA], horizontal_fov_deg=90.0,
                hip_grip_location_cm=[round(v, 2) for v in to_view(hip @ GRIP_R)], hip_rotation_pyr_deg=[round(v, 3) for v in (hp, hy, hr)],
                aim_muzzle_location_cm=[round(v, 2) for v in to_view(aim @ MUZZLE)], aim_rotation_pyr_deg=[round(v, 3) for v in (ap, ay, ar)],
                aim_sight_location_cm=[round(v, 2) for v in to_view(aim @ SIGHT)],
                note="HipGripLocation / HipRotation and AimMuzzleLocation / AimRotation reproduce this camera exactly "
                     "(Idle frame 0 = hip pose, AimIdle frame 0 = aim pose with the brass bead on the view axis).")
