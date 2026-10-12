"""Weapon profile of a long arm pack (T-250): everything that differs from the musket in the choreography of
scripts/props/musket_pack/animation.py (v2, T-246), which choreo.apply() patches with it.

Frames are the pack's own ("local" frames, 24 fps); the musket's hammer / frizzen keys and its third person look-down
are reached through a piecewise linear time warp local -> musket master frames (built by choreo.apply from the
musket's current reload windows), so the musket's choreography is reused where the gestures are the same. Points are
weapon local (metres, +X muzzle, +Z up, -Y lock side); view points are cm in the first person camera (X forward,
Y right, Z up), FRotator (pitch, yaw, roll).
"""
from __future__ import annotations

import math

from mathutils import Vector

from musket_ref import A

V = Vector
GRIP_U, THUMB_U, SIDE_U = (0, .55, .85), (0, .75, .65), (.15, .95, .25)
FORE_U, BAND_U = (0, -.45, -.89), (-.2, -.95, .2)
E_LOW, E_MOUTH, E_PAN = (5.0, 30.0, -55.0), (10.0, 18.0, -55.0), (14.0, 30.0, -40.0)
POUCH = ('v', (16.0, 30.0, -44.0), V((-.25, -.04, .38)))
MOUTH = ('v', (24.0, 1.0, -12.5), V((-.03, -.20, .73)))
TEAR = ('v', (23.0, 2.5, -15.5), V((-.03, -.19, .70)))


class Profile:
    """Defaults shared by the long arms; the packs subclass it (blunderbuss_profile.py, rifle_profile.py)."""

    # identity
    name = 'LongArm'                 # asset tag: A_Bean_<name>_<view>_<clip>, A_<name>_<view>_<clip>, SK_Bean_Pirate<name>
    pack_id = 'bean-longarm-anim'
    manifest_file = 'longarm-manifest.json'
    skeleton_group = 'PirateLongArm'
    display = 'arme longue'
    # weapon points (weapon local)
    grip_r = V((0.0, 0.0, -0.002))
    grip_l = V((0.26, 0.0, 0.012))
    band_grip = V((0.405, 0.0, 0.030))     # FP left hand while loading (high on the fore-end)
    muzzle = V((0.626, 0.0, 0.045))
    bore_z = 0.045
    pan = V((0.068, -0.027, 0.050))
    hammer_spur = V((0.030, -0.034, 0.080))
    frizzen_toe = V((0.082, -0.031, 0.084))
    rod_head = V((0.10, 0.0, 0.0))
    rod_dir = V((0.508, 0.0, 0.008)).normalized()
    rod_len = 0.52
    cartridge_rest = V((-0.195, 0.0, -0.030))
    sight_rear = V((0.08, 0.0, 0.0675))
    sight_front = V((0.59, 0.0, 0.0665))
    aim_eye_cm = 28.0                      # eye to the rear sight when aiming (cm)
    aim_fov_deg = 65.0                     # proposed AimFieldOfView
    bounds_size = [0.92, 0.12, 0.19]
    # recoil (Fire / AimFire / Recoil): CoD style snap, overshoot and settle (choreo.kick)
    recoil = dict(pitch=7.0, back=0.045, up=0.006, roll=2.0, yaw=0.8)
    # timeline (local frames)
    reload_frames = 132
    windows = dict(A.RELOAD_WINDOWS)
    master_warp = []
    pose_keys = [(0, 'base'), (8, 'prime'), (36, 'prime'), (46, 'load'), (120, 'load'), (129, 'base'), (132, 'base')]
    life_dips = [(18, 20, 22, 0.006), (45, 47.5, 50, 0.012), (97, 99, 101, 0.009), (102, 104, 106, 0.009)]
    life_pours = [(24, 27, 30, 2.0), (50, 54, 58, 2.5)]
    ground_events = list(A.GROUND_RELOAD_EVENTS)
    # ramrod phases (local frames; s = slide along its channel, depth = tip under the muzzle, hold = where the right
    # hand holds the rod, metres from its head end; home_s in fractions of the drawn-out slide)
    rod = dict(pull_s=[(79, 0.0), (80, 0.0), (85, 0.25), (87, 0.25), (92, 0.53)],
               pull_hold=[(79, 0.50), (85, 0.50), (87, 0.31), (92, 0.31)],
               turn=(92, 100), enter=(100, 104),
               ram_depth=[(104, 0.0), (107, 0.31), (110, 0.19), (114, 0.33), (117, 0.22), (118, 0.22), (122, -0.07)],
               ram_hold=[(104, 0.06), (118, 0.06), (122, 0.26)],
               turnback=(124, 128), home_s=[(128, 1.0), (129, 1.0), (131.5, 0.0), (132, 0.0)], home_hold=[(128, 0.26), (131, 0.45)])
    rod_grab_before = 0.50
    rod_grab_after = 0.45
    band_grip_tp = None                    # TP left hand while loading (None: grip_l + 3 cm, as the musket)
    # cartridge in the right hand: visible between cart_show and cart_hide, axis keys (local frames), pushed into the
    # bore between cart_into frames down to cart_into_depth under the muzzle
    cart_show, cart_hide = 16, 73
    cart_axis_keys = [(16, 'across'), (21, 'across'), (23, 'down'), (26, 'down'), (29, 'up'), (35, 'up'), (39, 'across'), (52, 'across'),
                      (56, 'barrel'), (64, 'barrel'), (66.5, 'across'), (69, '-barrel'), (73, '-barrel')]
    cart_into = (69, 72.5)
    cart_into_depth = 0.034
    # third person load pose: the muzzle at anchor, barrel along dir
    tp_load = dict(anchor=V((-.05, -.30, .78)), dir=V((.10, -.36, .93)), roll=6)
    tp_extra = {}
    # first person poses: name -> (weapon point, view cm, FRotator); 'aim' is computed from the sights; the musket's
    # other poses (holster, sprint, look, show_barrel, throw) are kept unless listed here with the weapon's own points
    fp_poses = {}
    extra_bone_names = []

    # ------------------------------------------------------------------------------------------- derived

    def __init__(self):
        poses = dict(self.fp_poses)
        poses.setdefault('base', (self.grip_r, (31.0, 14.0, -19.0), (6.0, -6.0, -12.0)))
        poses.setdefault('prime', (self.pan, (33.0, 0.5, -5.0), (14.0, -30.0, -66.0)))
        poses.setdefault('load', (self.muzzle, (56.0, -4.0, 10.0), (54.0, -8.0, 6.0)))
        poses.setdefault('look', (self.pan, (31.0, -1.0, -4.0), (8.0, -26.0, -80.0)))
        poses.setdefault('show_barrel', (self.band_grip, (36.0, 3.0, -7.0), (36.0, -12.0, 52.0)))
        poses.setdefault('throw', (self.grip_r, (33.0, 30.0, -24.0), (-18.0, 46.0, -42.0)))
        poses.setdefault('sprint', (self.grip_r, (30.0, 14.0, -16.0), (-14.0, -40.0, -50.0)))
        poses['aim'] = (self.sight_rear, (self.aim_eye_cm, 0.0, 0.0), (self.aim_pitch(), 0.0, 0.0))
        self.fp_poses = poses
        self.clips = dict(A.CLIPS)
        self.clips['GroundReload'] = self.reload_frames
        for clip, (a, b) in self.windows.items():
            self.clips[clip] = b - a
        self.clips['AimFire'] = self.clips['Fire']
        self.clips.setdefault('Inspect', 96)
        self.clips.setdefault('Discard', 32)

    def aim_pitch(self):
        """Pitch (deg, muzzle up positive) putting the line of sight on the view axis."""
        u = (self.sight_front - self.sight_rear).normalized()
        return math.degrees(math.atan2(-u.z, u.x))

    def warp(self, f):
        """Local reload frame -> musket master frame (piecewise linear, choreo.build_warp)."""
        keys = self.master_warp
        if not keys:
            return f
        if f <= keys[0][0]:
            return keys[0][1]
        for (a, x), (b, y) in zip(keys, keys[1:]):
            if f <= b:
                return x + (y - x) * (f - a) / max(b - a, 1e-6)
        return keys[-1][1]

    # ------------------------------------------------------------------------------------------- hands

    def right_keys(self):
        """Right hand keys of the reload, local frames: (frame, target, FP elbow (view cm), FP back of the hand)."""
        raise NotImplementedError

    def left_keys(self, view='FP'):
        raise NotImplementedError

    def extra_bones(self, g, M, right_point, right_rot, view, rest):
        """Armature space matrices of the extra bones at local reload frame g (None out of the reload): {bone: matrix};
        rest(name) gives the bone's rest matrix. Default: none."""
        return {}

    def release_intervals(self, clip):
        if clip == 'Equip':
            return [[0, 30]]
        if clip == 'Holster':
            return [[30, 42]]
        if clip == 'Discard':
            return [[A.RELEASE_FRAME, self.clips['Discard']]]
        if clip == 'GroundReload':
            off = [fr for fr, target, _, _ in self.right_keys() if not (target[0] == 'w' and Vector(target[1]) == self.grip_r)]
            return [[min(off), max(off) + 2]]
        if clip in self.windows:
            return [[0, self.clips[clip]]]
        return []
