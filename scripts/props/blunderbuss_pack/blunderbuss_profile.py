"""Choreography profile of the blunderbuss (T-250): 0.75 m, reload 5 s (120 frames at 24 fps). The shot cartridge
(paper powder tube + linen bag of pellets, bone "cartridge") is bitten, primes the pan, pours the powder, then its bag
is tipped twice into the bell (events ShotPour, BallAndPatch = the paper pushed in as wadding), rammed with the short
rod (one pull, two quick strokes). Aiming: the top of the barrel on the view axis (breech top -> bell lip).
"""
from mathutils import Vector

from weapon_profile import (Profile, GRIP_U, THUMB_U, SIDE_U, FORE_U, BAND_U, E_LOW, E_MOUTH, E_PAN, POUCH, MOUTH, TEAR)
from musket_ref import A

V = Vector


class BlunderbussProfile(Profile):
    name = 'Blunderbuss'
    pack_id = 'bean-blunderbuss-anim'
    manifest_file = 'blunderbuss-manifest.json'
    skeleton_group = 'PirateBlunderbuss'
    display = 'tromblon'
    grip_r = V((0.0, 0.0, -0.002))
    grip_l = V((0.20, 0.0, 0.012))
    band_grip = V((0.30, 0.0, 0.030))
    muzzle = V((0.486, 0.0, 0.045))
    bore_z = 0.045
    pan = V((0.062, -0.027, 0.050))
    hammer_spur = V((0.028, -0.034, 0.078))
    frizzen_toe = V((0.0745, -0.031, 0.0855))
    rod_head = V((0.10, 0.0, 0.0))
    rod_dir = V((0.366, 0.0, 0.0065)).normalized()
    rod_len = 0.374
    cartridge_rest = V((-0.170, 0.0, -0.028))
    sight_rear = V((0.045, 0.0, 0.0645))
    sight_front = V((0.475, 0.0, 0.0755))
    aim_eye_cm = 28.0
    aim_fov_deg = 70.0
    bounds_size = [0.76, 0.068, 0.158]
    recoil = dict(pitch=10.0, back=0.065, up=0.010, roll=3.0, yaw=1.2)
    reload_frames = 120
    windows = {'HalfCock': (0, 8), 'PrimePan': (8, 30), 'CloseFrizzen': (30, 34), 'Powder': (34, 56), 'BallAndPatch': (56, 72),
               'DrawRamrod': (72, 84), 'TurnRamrod': (84, 90), 'RamrodStroke': (90, 102), 'ReturnRamrod': (102, 112), 'Cock': (112, 120)}
    pose_keys = [(0, 'base'), (8, 'prime'), (34, 'prime'), (45, 'load'), (110, 'load'), (118, 'base'), (120, 'base')]
    life_dips = [(18, 20, 22, 0.006), (41, 44, 47, 0.010), (95, 97, 99, 0.008), (100, 102, 104, 0.008)]
    life_pours = [(23, 26, 29, 2.0), (47, 50, 53, 2.5)]
    ground_events = [(2, 'HalfCockStart', None), (26, 'PanPrime', 'socket_pan'), (33, 'FrizzenClose', 'socket_pan'),
                     (44, 'StockLowered', None), (50, 'PowderCharge', 'socket_muzzle'), (60, 'ShotPour', 'socket_muzzle'),
                     (66, 'BallAndPatch', 'socket_muzzle'), (74, 'RamrodDraw', 'socket_ramrod_tip'), (86, 'RamrodTurn', None),
                     (100, 'RamrodStroke', 'socket_muzzle'), (104, 'RamrodStroke', 'socket_muzzle'), (108, 'RamrodReturn', None),
                     (117, 'FullCockEnd', None)]
    rod = dict(pull_s=[(73, 0.0), (74, 0.0), (78, 0.18), (79, 0.18), (84, 0.385)],
               pull_hold=[(73, 0.35), (78, 0.35), (79, 0.20), (84, 0.20)],
               turn=(84, 90), enter=(90, 93),
               ram_depth=[(93, 0.0), (96.5, 0.15), (97.5, 0.15), (99.5, 0.28), (100.5, 0.30), (101.5, 0.30), (103, 0.22), (104.5, 0.30),
                          (105.5, 0.30), (107, -0.07)],
               ram_hold=[(93, 0.20), (96.5, 0.20), (97.5, 0.05), (105.5, 0.05), (107, 0.19)],
               turnback=(107, 109.5), home_s=[(109.5, 1.0), (110, 1.0), (112, 0.0), (112.5, 0.0)], home_hold=[(109.5, 0.19), (112.5, 0.32)])
    rod_grab_before = 0.35
    rod_grab_after = 0.31
    cart_show, cart_hide = 13, 70
    cart_axis_keys = [(13, 'across'), (17, 'across'), (18, 'down'), (21, 'down'), (23, 'up'), (29, 'up'), (32, 'across'), (44, 'across'),
                      (47, 'barrel'), (54, 'barrel'), (56, 'across'), (57.5, '-barrel'), (66, '-barrel'), (70, '-barrel')]
    cart_into = (66, 69.5)
    cart_into_depth = 0.030
    tp_load = dict(anchor=V((-.05, -.30, .70)), dir=V((.10, -.36, .93)), roll=6)
    fp_poses = {
        'prime': (V((0.062, -0.027, 0.050)), (33.0, 0.5, -5.0), (14.0, -30.0, -66.0)),
        'load': (V((0.486, 0.0, 0.045)), (50.0, -2.0, 5.0), (48.0, -6.0, 10.0)),
    }

    def right_keys(self):
        GRIP = ('w', self.grip_r)
        spur = ('w', self.hammer_spur + V((-0.05, 0.0, -0.02)))
        above_pan = ('pinch', self.pan + V((0, -0.005, 0.030)))
        above_muzzle = ('pinch', self.muzzle + V((0.035, 0, 0)))
        over_bell = ('pinch', self.muzzle + V((0.042, 0, 0.004)))
        over_bell_low = ('pinch', self.muzzle + V((0.030, 0, 0.0)))
        in_muzzle = ('pinch', self.muzzle + V((0.004, 0, 0)))
        toe_a = ('w', self.frizzen_toe + V((-0.03, -0.02, 0.015)))
        toe_b = ('w', self.frizzen_toe + V((-0.045, -0.02, 0.0)))
        rr, e_load, load_u = A.REACH['r'], A.LOAD_ELBOW['r'], (0, .6, .8)
        return [
            (0, GRIP, rr, GRIP_U), (2, spur, rr, THUMB_U), (6, GRIP, rr, GRIP_U), (9, GRIP, rr, GRIP_U),
            (13, POUCH, E_LOW, (.2, .9, -.2)), (15, POUCH, E_LOW, (.2, .9, -.2)),
            (18, MOUTH, E_MOUTH, (-.3, .6, .7)), (20, TEAR, E_MOUTH, (-.3, .6, .7)), (22, MOUTH, E_MOUTH, (-.3, .6, .7)),
            (24, above_pan, E_PAN, (0, .35, .94)), (29, above_pan, E_PAN, (0, .35, .94)),
            (31, toe_a, E_PAN, (0, .6, .8)), (34, toe_b, E_PAN, (0, .6, .8)),
            (44, above_muzzle, e_load, load_u), (47, above_muzzle, e_load, load_u), (54, above_muzzle, e_load, load_u),
            (57, over_bell, e_load, load_u), (59, over_bell_low, e_load, load_u), (61, over_bell, e_load, load_u),
            (63, over_bell_low, e_load, load_u), (65, over_bell, e_load, load_u),
            (67, in_muzzle, e_load, load_u), (70, ('w', self.muzzle + V((0.020, -0.010, 0.0))), e_load, load_u),
            (73, ('rod', None), e_load, SIDE_U), (112, ('rod', None), e_load, SIDE_U),
            (114, spur, rr, THUMB_U), (117, spur, rr, THUMB_U), (119, GRIP, rr, GRIP_U), (120, GRIP, rr, GRIP_U),
        ]

    def left_keys(self, view='FP'):
        band = self.band_grip if view == 'FP' else (self.band_grip_tp or self.grip_l + V((0.03, 0.0, 0.006)))
        rl, e_load = A.REACH['l'], A.LOAD_ELBOW['l']
        return [(0, ('w', self.grip_l), rl, FORE_U), (37, ('w', self.grip_l), rl, FORE_U), (45, ('w', band), e_load, BAND_U),
                (113, ('w', band), e_load, BAND_U), (118, ('w', self.grip_l), rl, FORE_U), (120, ('w', self.grip_l), rl, FORE_U)]
