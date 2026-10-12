"""Contract of the drum sub-machine gun pack "Mitraillette" (T-334, owner 2026-10-11: "mitraillette"): names, clips,
frames, events, bones, sockets and the key geometry shared by the model (model.py), the clips (animation.py), the build
(build.py) and the provisional manifest (scripts/props/period_arms_common/write_provisional.py). Pure Python.

The gun: a fictional 1920s sub-machine gun in the spirit of the Thompson 1921 / Suomi (old style: walnut stock, pistol
grip and vertical fore-grip, blued steel receiver, finned barrel and slotted compensator, brass fittings), open bolt
(the bolt rests to the rear, cocked; the cocking knob on top of the receiver runs with it), 50 round drum magazine
inserted from below in front of the trigger guard and held by a latch, cyclic rate about 11 rounds per second.

Weapon space (metres): +X muzzle, +Z up, -Y right side (ejection port), origin = the right hand on the pistol grip;
the bore axis at z = BORE_Z. Bean scale: 0.80 m overall.
"""
PACK_ID = 'bean-mitraillette'
TAG = 'Mitraillette'
DISPLAY = 'Mitraillette'
SCRIPT_DIR = 'smg_pack'
EXPORT_DIR = 'art/export/Weapons/Mitraillette'
FPS = 30
ROOT_BONE = 'root'
IDLE_CLIP = 'Idle'

# --------------------------------------------------------------------------------------------------- clips
SHOTS = (0, 3, 5, 8, 11, 14, 16, 19, 22, 25, 27)       # FireLoop: 11 rounds over 30 frames = 11 rounds/s at rate 1
CLIPS = {'Equip': 36, 'Idle': 90, 'Walk': 120, 'Sprint': 90, 'Fire': 12, 'FireLoop': 30, 'Reload': 84,
         'ReloadEmpty': 96, 'DryFire': 12, 'Inspect': 150, 'Holster': 36, 'Discard': 40}
LOOPS = {'Idle', 'Walk', 'Sprint', 'FireLoop'}
NEW_GESTURES = ()
EVENTS = {
    'Equip': [(4, 'DrawWeapon'), (30, 'Ready')],
    'Fire': [(1, 'Fire', 'socket_muzzle')],
    'FireLoop': [(s, 'Fire', 'socket_muzzle') for s in SHOTS],
    'Reload': [(20, 'MagOut', 'socket_drum'), (50, 'MagIn', 'socket_drum'), (68, 'Charge', 'socket_charge')],
    'ReloadEmpty': [(16, 'MagOut', 'socket_drum'), (22, 'MagDrop', 'socket_drum'), (54, 'MagIn', 'socket_drum'),
                    (76, 'Charge', 'socket_charge')],
    'DryFire': [(3, 'DryClick', 'socket_grip_r')],
    'Holster': [(30, 'StowWeapon')],
    'Discard': [(6, 'Empty', 'socket_charge'), (24, 'Release', 'socket_grip_r')],
}
CLIP_NOTES = {
    'Equip': 'drawn from below the view, right hand on the pistol grip, left hand on the fore-grip, a settle',
    'Idle': 'breathing (frame 0 = the hip pose the game measures: HipGripLocation / HipRotation)',
    'Walk': 'sway on four steps', 'Sprint': 'carried low across the body, muzzle down to the left, marked bob',
    'Fire': 'one round: the bolt slams forward and back, short kick and settle',
    'FireLoop': 'automatic fire, loop of 30 frames with 11 Fire events (11 rounds/s at play rate 1; PlayRate scales the '
                'rate): the bolt and its knob shuttle at every round, kick per round with a periodic climb and shudder',
    'Reload': 'left hand presses the latch and pulls the drum down (MagOut), stows it, brings a full drum up and seats it '
              '(MagIn), pulls the cocking knob back (Charge)',
    'ReloadEmpty': 'the empty drum pulled down (MagOut) and let fall (MagDrop: the drum bone is hidden from there, the game may '
                   'drop SM_Ammo_MitrailletteDrum from socket_drum), a full drum seated (MagIn), a hard charge (Charge)',
    'DryFire': 'the trigger pulled on an empty drum (DryClick)', 'Inspect': 'rolled to show the drum, the receiver and the compensator',
    'Holster': 'put away below the view (StowWeapon)',
    'Discard': 'empty: the knob pulled on nothing (Empty), a look, thrown to the right and down (Release)',
}

# ------------------------------------------------------------------------------------------------ geometry
BORE_Z = 0.062
MUZZLE_X = 0.482
BUTT_X = -0.318
GRIP_R = (0.0, 0.0, 0.0)                     # right hand on the pistol grip (mitten centre)
GRIP_L = (0.262, 0.0, -0.006)                # left hand on the vertical fore-grip
RECEIVER = (-0.040, 0.205)                   # rear and front of the receiver (x)
RECEIVER_Z = (0.034, 0.088)                  # bottom and top of the receiver
RECEIVER_HW = 0.0145
DRUM_C = (0.118, 0.0, -0.028)                # drum centre (axis along Y)
DRUM_R = 0.054
DRUM_HW = 0.021
BOLT_TRAVEL = 0.068                          # open bolt: from the rear (cocked, rest) forward when it fires
KNOB = (0.060, 0.0, 0.094)                   # cocking knob on top of the receiver (bolt to the rear)
LATCH_PIVOT = (0.074, 0.0, 0.030)
TRIGGER_PIVOT = (0.030, 0.0, 0.032)
EJECT = (0.122, -0.016, 0.068)
EJECT_DIR = (0.122, -0.066, 0.086)
SIGHT_REAR = (-0.012, 0.0, 0.094)
SIGHT_FRONT = (0.470, 0.0, 0.086)

BONES = {
    'root': ((0.0, 0.0, 0.0), (0.10, 0.0, 0.0), None),
    'bolt': ((0.060, 0.0, BORE_Z), (0.160, 0.0, BORE_Z), 'root'),
    'trigger': (TRIGGER_PIVOT, (0.028, 0.0, 0.006), 'root'),
    'drum': (DRUM_C, (DRUM_C[0], 0.0, DRUM_C[2] + 0.04), 'root'),
    'mag_latch': (LATCH_PIVOT, (LATCH_PIVOT[0] + 0.015, 0.0, LATCH_PIVOT[2]), 'root'),
}
MARKERS = {'grip': ('root', GRIP_R), 'muzzle': ('root', (MUZZLE_X, 0.0, BORE_Z))}
SOCKETS = {
    'grip_r': ('root', GRIP_R),
    'grip_l': ('root', GRIP_L),
    'muzzle': ('root', (MUZZLE_X + 0.002, 0.0, BORE_Z)),
    'eject': ('root', EJECT),
    'eject_dir': ('root', EJECT_DIR),
    'drum': ('drum', DRUM_C),
    'charge': ('bolt', KNOB),
    'latch': ('mag_latch', (LATCH_PIVOT[0] + 0.012, 0.0, LATCH_PIVOT[2] - 0.004)),
    'sight_rear': ('root', SIGHT_REAR),
    'sight_front': ('root', SIGHT_FRONT),
    'butt': ('root', (BUTT_X - 0.004, 0.0, 0.012)),
}
CONTRACT_NAMES = {'grip': 'socket_grip_r', 'support': 'socket_grip_l', 'muzzle': 'socket_muzzle', 'eject': 'socket_eject',
                  'drum': 'socket_drum', 'charge': 'socket_charge'}
MOVING_PARTS = {
    'bolt': 'open bolt: rests to the rear (cocked); at every Fire it is forward on the event frame and back on the next '
            'one (the knob on top runs with it); Charge pulls it back past the rest and lets it catch',
    'trigger': 'pivots back while firing', 'mag_latch': 'pressed forward by the left thumb before MagOut',
    'drum': 'rigid drum; Reload / ReloadEmpty move it with the left hand (hidden by scale 0 while out of the gun and out of view)',
}
STATICS = {
    'SM_Bean_Mitraillette': {'view': 'pickup', 'role': 'the gun at rest lying on the ground with its drum (pickup)'},
    'SM_Ammo_MitrailletteDrum': {'view': 'ammo', 'role': 'a full 50 round drum (ammunition pickup; also the drum dropped at MagDrop)'},
    'SM_Mitraillette_Casing': {'view': 'fx', 'role': 'spent pistol case thrown at every Fire from socket_eject (mouth +X)'},
}
BOUNDS_PLANNED = {'size': [0.80, 0.06, 0.20]}
RULES_HINTS = {
    'FirstPersonAlignBone': 'root', 'GripSocket': 'socket_grip_r', 'MuzzleSocket': 'socket_muzzle', 'FireEvent': 'Fire',
    'bAllowAiming': 'false (no aim clip, fired from the hip like the machine gun)', 'Capacity': '50 (drum)',
    'RateOfFire': 'FireLoop: 11 Fire events per 1.0 s loop (play rate 1)',
    'HipGripLocation/HipRotation': 'fp_framing.hip_*',
    'ShellEject': 'SM_Mitraillette_Casing at every Fire from socket_eject toward socket_eject_dir',
}
IMPORT_ENTRY = {'pack_dir': EXPORT_DIR, 'manifest': 'pack.json', 'asset': TAG, 'display': 'Mitraillette',
                'muzzle_sockets': ['socket_muzzle'], 'base': 'SK_Bean_Mitraillette', 'fp': 'SK_Bean_Mitraillette_FP',
                'pickup': 'SM_Bean_Mitraillette', 'idle': 'Idle'}
NOTES = [
    'Fictional 1920s drum sub-machine gun (Thompson 1921 / Suomi spirit): walnut stock, pistol grip and vertical fore-grip, '
    'blued receiver, finned barrel, slotted compensator, brass knob, latch and drum key; open bolt; 50 round drum from below.',
    'FireLoop: 30 frames, 11 Fire events (frames 0, 3, 5, 8, 11, 14, 16, 19, 22, 25, 27): 11 rounds/s at play rate 1; the '
    'loop is seamless (frame 30 = frame 0); cut it anywhere (the C++ can blend to Idle when the trigger is released).',
    'Reload: MagOut, MagIn, Charge (the drum kept); ReloadEmpty: MagOut, MagDrop (drum let fall, hidden), MagIn, Charge.',
    'No aim clips (fired from the hip like the machine gun); Discard added like every v2 pack.',
]
