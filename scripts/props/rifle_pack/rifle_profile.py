"""Choreography profile of the Baker rifle (T-250): 1.15 m, reload 7 s (168 frames at 24 fps). Paper cartridge bitten,
pan primed, powder poured; then the rifle is swung up ("box" pose), the brass patch box in the butt flipped open
(bone "patchbox_lid"), a greased patch and ball taken (bone "patch"), set on the muzzle and pressed in with the thumb
(event BallAndPatch), then the long rod: three pulls, two firm pressed strokes. Aiming: rear notch and front blade on
the view axis.
"""
import math

from mathutils import Matrix, Vector

from weapon_profile import (Profile, GRIP_U, THUMB_U, SIDE_U, FORE_U, BAND_U, E_LOW, E_MOUTH, E_PAN, POUCH, MOUTH, TEAR)
from musket_ref import A
import rifle_model

V = Vector
BOX = rifle_model.PATCHBOX
HINGE = rifle_model.PATCHBOX_HINGE
PATCH_REST = rifle_model.PATCH_REST


class RifleProfile(Profile):
    name = 'Rifle'
    pack_id = 'bean-rifle-anim'
    manifest_file = 'rifle-manifest.json'
    skeleton_group = 'PirateRifle'
    display = 'carabine rayée'
    grip_r = V((0.0, 0.0, -0.002))
    grip_l = V((0.30, 0.0, 0.012))
    band_grip = V((0.52, 0.0, 0.030))
    muzzle = V((0.806, 0.0, 0.045))
    bore_z = 0.045
    pan = V((0.068, -0.027, 0.050))
    hammer_spur = V((0.030, -0.034, 0.080))
    frizzen_toe = V((0.082, -0.031, 0.084))
    rod_head = V((0.10, 0.0, 0.0))
    rod_dir = V((0.688, 0.0, 0.008)).normalized()
    rod_len = 0.70
    cartridge_rest = rifle_model.CARTRIDGE_REST
    sight_rear = V((0.165, 0.0, 0.0725))
    sight_front = V((0.765, 0.0, 0.0680))
    aim_eye_cm = 30.0
    aim_fov_deg = 58.0
    bounds_size = [1.16, 0.070, 0.225]
    recoil = dict(pitch=6.0, back=0.040, up=0.006, roll=1.5, yaw=0.6)
    reload_frames = 168
    windows = {'HalfCock': (0, 10), 'PrimePan': (10, 36), 'CloseFrizzen': (36, 41), 'Powder': (41, 66), 'BallAndPatch': (66, 100),
               'DrawRamrod': (100, 118), 'TurnRamrod': (118, 126), 'RamrodStroke': (126, 148), 'ReturnRamrod': (148, 160), 'Cock': (160, 168)}
    pose_keys = [(0, 'base'), (10, 'prime'), (41, 'prime'), (54, 'load'), (68, 'load'), (74, 'box'), (86, 'box'), (92, 'load'),
                 (158, 'load'), (166, 'base'), (168, 'base')]
    life_dips = [(22, 24, 27, 0.006), (49, 52, 56, 0.010), (78, 79, 81, 0.004), (93, 94.5, 96, 0.005), (134, 137, 140, 0.009),
                 (142, 144, 146, 0.009)]
    life_pours = [(28, 31, 34, 2.0), (57, 60, 63, 2.5)]
    ground_events = [(3, 'HalfCockStart', None), (31, 'PanPrime', 'socket_pan'), (40, 'FrizzenClose', 'socket_pan'),
                     (52, 'StockLowered', None), (58, 'PowderCharge', 'socket_muzzle'), (78, 'PatchBox', 'socket_patchbox'),
                     (93, 'BallAndPatch', 'socket_muzzle'), (104, 'RamrodDraw', 'socket_ramrod_tip'), (121, 'RamrodTurn', None),
                     (137, 'RamrodStroke', 'socket_muzzle'), (144, 'RamrodStroke', 'socket_muzzle'), (153, 'RamrodReturn', None),
                     (164, 'FullCockEnd', None)]
    rod = dict(pull_s=[(101, 0.0), (102, 0.0), (105, 0.18), (106, 0.18), (109, 0.36), (110, 0.36), (113, 0.54), (114, 0.54), (118, 0.715)],
               pull_hold=[(101, 0.68), (105, 0.68), (106, 0.50), (109, 0.50), (110, 0.32), (113, 0.32), (114, 0.14), (118, 0.14)],
               turn=(118, 126), enter=(126, 129),
               ram_depth=[(129, 0.0), (133, 0.16), (134, 0.16), (137, 0.32), (138, 0.32), (141, 0.50), (142, 0.50), (144, 0.58), (146, 0.58),
                          (147.5, 0.46), (149.5, 0.58), (150.5, 0.58), (152, 0.40), (152.5, 0.40), (154.5, 0.10), (155, 0.10), (156, -0.07)],
               ram_hold=[(129, 0.58), (133, 0.58), (134, 0.40), (137, 0.40), (138, 0.24), (141, 0.24), (142, 0.05), (152, 0.05), (152.5, 0.30),
                         (154.5, 0.30), (155, 0.35), (156, 0.35)],
               turnback=(156, 158.5), home_s=[(158.5, 1.0), (159, 1.0), (161.3, 0.0), (161.5, 0.0)], home_hold=[(158.5, 0.35), (161.5, 0.60)])
    rod_grab_before = 0.68
    rod_grab_after = 0.60
    band_grip_tp = V((0.66, 0.0, 0.030))
    cart_show, cart_hide = 16, 68
    cart_axis_keys = [(16, 'across'), (21, 'across'), (23, 'down'), (26, 'down'), (29, 'up'), (35, 'up'), (39, 'across'), (52, 'across'),
                      (56, 'barrel'), (64, 'barrel'), (68, 'barrel')]
    cart_into = None
    # the butt on the ground out to the right, the muzzle in front of the chin: both hands work at the muzzle
    tp_load = dict(anchor=V((-.06, -.22, .80)), dir=V((.67, -.21, .707)), roll=6)
    tp_extra = {'box': (BOX, V((-.22, -.22, .40)), V((.75, -.35, .45)), -70)}
    fp_poses = {
        'prime': (V((0.068, -0.027, 0.050)), (33.0, 0.5, -5.0), (14.0, -30.0, -66.0)),
        'load': (V((0.806, 0.0, 0.045)), (58.0, -3.0, 9.0), (54.0, -6.0, 8.0)),
        'box': (BOX.copy(), (27.0, 9.0, -15.0), (38.0, -30.0, -75.0)),
    }
    extra_bone_names = ['patchbox_lid', 'patch']

    def right_keys(self):
        GRIP = ('w', self.grip_r)
        spur = ('w', self.hammer_spur + V((-0.05, 0.0, -0.02)))
        above_pan = ('pinch', self.pan + V((0, -0.005, 0.030)))
        above_muzzle = ('pinch', self.muzzle + V((0.035, 0, 0)))
        toe_a = ('w', self.frizzen_toe + V((-0.03, -0.02, 0.015)))
        toe_b = ('w', self.frizzen_toe + V((-0.045, -0.02, 0.0)))
        box_over = ('w', BOX + V((0.0, -0.045, 0.025)))
        box_flip = ('w', BOX + V((0.04, -0.05, 0.0)))
        box_take = ('pinch', BOX + V((0.0, -0.012, 0.0)))
        patch_over = ('pinch', self.muzzle + V((0.035, 0, 0)))
        patch_in = ('pinch', self.muzzle + V((0.004, 0, 0)))
        rr, e_load, load_u = A.REACH['r'], A.LOAD_ELBOW['r'], (0, .6, .8)
        return [
            (0, GRIP, rr, GRIP_U), (3, spur, rr, THUMB_U), (8, GRIP, rr, GRIP_U), (11, GRIP, rr, GRIP_U),
            (16, POUCH, E_LOW, (.2, .9, -.2)), (18, POUCH, E_LOW, (.2, .9, -.2)),
            (22, MOUTH, E_MOUTH, (-.3, .6, .7)), (24, TEAR, E_MOUTH, (-.3, .6, .7)), (26, MOUTH, E_MOUTH, (-.3, .6, .7)),
            (29, above_pan, E_PAN, (0, .35, .94)), (35, above_pan, E_PAN, (0, .35, .94)),
            (38, toe_a, E_PAN, (0, .6, .8)), (41, toe_b, E_PAN, (0, .6, .8)),
            (52, above_muzzle, e_load, load_u), (56, above_muzzle, e_load, load_u), (64, above_muzzle, e_load, load_u),
            (67, ('w', self.muzzle + V((0.020, -0.012, 0.0))), e_load, load_u),
            (70, ('w', self.grip_r + V((0.0, -0.030, 0.0))), rr, GRIP_U), (74, ('w', self.grip_r + V((0.0, -0.030, 0.0))), rr, GRIP_U),
            (76, box_over, e_load, (0, .2, .98)), (79, box_flip, e_load, (0, .2, .98)),
            (82, box_take, e_load, (0, .3, .95)), (85, box_take, e_load, (0, .3, .95)),
            (90, patch_over, e_load, load_u), (93, patch_in, e_load, load_u),
            (96, ('w', self.muzzle + V((0.0, -0.012, 0.0))), e_load, load_u), (98, ('w', self.muzzle + V((0.020, -0.010, 0.0))), e_load, load_u),
            (101, ('rod', None), e_load, SIDE_U), (161, ('rod', None), e_load, SIDE_U),
            (163, spur, rr, THUMB_U), (166, spur, rr, THUMB_U), (168, GRIP, rr, GRIP_U),
        ]

    def left_keys(self, view='FP'):
        band = self.band_grip if view == 'FP' else (self.band_grip_tp or self.grip_l + V((0.03, 0.0, 0.006)))
        # third person: while the butt is swung up to the hip (box pose) the short left arm holds the fore-end low
        low = band if view == 'FP' else self.grip_l + V((0.03, 0.0, 0.006))
        rl, e_load = A.REACH['l'], A.LOAD_ELBOW['l']
        return [(0, ('w', self.grip_l), rl, FORE_U), (44, ('w', self.grip_l), rl, FORE_U), (52, ('w', band), e_load, BAND_U),
                (68, ('w', band), e_load, BAND_U), (74, ('w', low), e_load, BAND_U), (86, ('w', low), e_load, BAND_U),
                (92, ('w', band), e_load, BAND_U), (162, ('w', band), e_load, BAND_U), (166, ('w', self.grip_l), rl, FORE_U),
                (168, ('w', self.grip_l), rl, FORE_U)]

    def extra_bones(self, g, M, right_point, right_rot, view, rest):
        import choreo
        out = {}
        # the brass lid swings open about its hinge pin (vertical, rear end) while the hand flips it, shuts on its spring
        angle = 0.0 if g is None else A.curve(g, [(75, 0.0), (79, -72.0), (86, -72.0), (90, 0.0)])
        lid_rest = rest('patchbox_lid')
        pivot = Matrix.Translation(HINGE) @ Matrix.Rotation(math.radians(angle), 4, 'Z') @ Matrix.Translation(-HINGE)
        out['patchbox_lid'] = M @ pivot @ lid_rest
        # the patched ball: hidden in the butt, then pinched from the box to the muzzle, pressed in, rammed down the bore
        patch_rest = rest('patch')
        if g is None or g < 82:
            out['patch'] = M @ patch_rest
            return out
        barrel = (M.to_3x3() @ Vector((1, 0, 0))).normalized()
        if g < 96:
            across = right_rot.col[1].xyz if right_rot is not None else Vector((1, 0, 0))
            keys = [(82, across), (87, across), (90, -barrel), (96, -barrel)]
            i, w = A.segment(g, keys)
            axis = keys[i][1].lerp(keys[min(i + 1, len(keys) - 1)][1], w)
            axis = axis.normalized() if axis.length > 1e-4 else -barrel
            pos = choreo.pinch_point(right_point, right_rot)
            if g >= 93:
                into = A.smooth(93, 96, g)
                pos = pos.lerp(M @ Vector((self.muzzle.x - 0.018, 0.0, self.bore_z)), into)
            rot3 = A.axes(axis, A.UP if abs(axis.dot(A.UP)) < 0.95 else A.FWD)
        else:
            # pushed down by the rod: stays at the deepest point the rod tip reached
            keys = self.rod['ram_depth']
            depth = max([0.0, A.curve(g, keys)] + [d for fr, d in keys if fr <= g])
            pos = M @ Vector((self.muzzle.x - 0.018 - depth, 0.0, self.bore_z))
            rot3 = A.axes(-barrel, M.to_3x3() @ Vector((0, 0, 1)))
        delta = rot3.to_4x4()
        delta.translation = pos
        out['patch'] = delta @ Matrix.Translation(-PATCH_REST) @ patch_rest
        return out
