"""Contract of the bolt-action sniper rifle pack (T-334, owner 2026-10-11: "sniper verrou"): names, clips, frames,
events, bones, sockets and the key geometry shared by the model (model.py), the clips (animation.py), the build
(build.py) and the provisional manifest (scripts/props/period_arms_common/write_provisional.py). Pure Python.

The rifle: a late 19th century bolt-action sniper rifle in the spirit of the Lee-Metford / Mauser (fictional): walnut
stock with a semi pistol grip and a cheek comb, blued round barrel and receiver, turn-bolt with a 60 degree lift and a
7.5 cm pull (bolt handle on the right), five round internal magazine loaded from a stripper clip pushed down through
the open action (charger guide at the front of the rear bridge), long brass telescopic sight mounted OFFSET TO THE LEFT
(as the period sniper rifles did, so the clip still loads from the top), iron sights kept, leather sling.

Weapon space (metres): +X muzzle, +Z up, -Y right side (bolt handle), origin = the right hand grip (wrist of the
stock); the bore axis is at z = BORE_Z. Bean scale: 1.08 m overall.
"""
PACK_ID = 'bean-boltrifle'
TAG = 'BoltRifle'
DISPLAY = 'Fusil de tireur d\'élite à verrou'
SCRIPT_DIR = 'boltrifle_pack'
EXPORT_DIR = 'art/export/Weapons/BoltRifle'
FPS = 30
ROOT_BONE = 'root'
IDLE_CLIP = 'Idle'

# --------------------------------------------------------------------------------------------------- clips
CLIPS = {'Equip': 45, 'Idle': 90, 'Walk': 120, 'Sprint': 90, 'AimIn': 15, 'AimIdle': 90, 'AimOut': 12,
         'Fire': 24, 'AimFire': 24, 'BoltCycle': 27, 'Reload': 78, 'DryFire': 12, 'Inspect': 150, 'Holster': 36,
         'Discard': 40}
LOOPS = {'Idle', 'Walk', 'Sprint', 'AimIdle'}
NEW_GESTURES = ('BoltCycle',)
EVENTS = {
    'Equip': [(6, 'DrawWeapon'), (38, 'Ready')],
    'Fire': [(2, 'Fire', 'socket_muzzle')],
    'AimFire': [(2, 'Fire', 'socket_muzzle')],
    'BoltCycle': [(6, 'BoltOpen', 'socket_bolt_knob'), (11, 'CaseEject', 'socket_eject'), (19, 'BoltClose', 'socket_bolt_knob')],
    'Reload': [(10, 'BoltOpen', 'socket_bolt_knob'), (30, 'ClipIn', 'socket_clip'), (42, 'RoundsIn', 'socket_clip'),
               (52, 'ClipEject', 'socket_clip'), (62, 'BoltClose', 'socket_bolt_knob')],
    'DryFire': [(3, 'DryClick', 'socket_grip_r')],
    'Holster': [(30, 'StowWeapon')],
    'Discard': [(8, 'Empty', 'socket_bolt_knob'), (24, 'Release', 'socket_grip_r')],
}
CLIP_NOTES = {
    'Equip': 'drawn from below the view, both hands find the grip and the fore-end, a settle',
    'Idle': 'breathing (frame 0 = the hip pose the game measures: HipGripLocation / HipRotation)',
    'Walk': 'figure-of-eight sway on four steps',
    'Sprint': 'carried low and canted across the body, marked bob',
    'AimIn': 'hip -> eye at the eyepiece of the scope (socket_scope_eye on the camera axis), small settle',
    'AimIdle': 'eye at the scope, light breathing (frame 0 = the aim pose the game measures: AimMuzzleLocation / AimRotation)',
    'AimOut': 'scope -> hip',
    'Fire': 'shot from the hip: sharp kick (muzzle up, back, roll) and settle; the bolt stays closed (BoltCycle follows)',
    'AimFire': 'shot through the scope: the kick, then the eye comes back to the eyepiece',
    'BoltCycle': 'right hand to the knob, bolt lifted (BoltOpen), pulled back (CaseEject: the spent case leaves socket_eject '
                 'toward socket_eject_dir), pushed forward, turned down (BoltClose: a round chambered), hand back to the grip',
    'Reload': 'bolt opened, a stripper clip of five from the pouch seated in the charger guide (ClipIn), the rounds thumbed '
              'down into the magazine (RoundsIn: five rounds), the bolt pushed home kicks the empty clip out (ClipEject), '
              'turned down (BoltClose: a round chambered)',
    'DryFire': 'the trigger falls on an empty chamber (DryClick)',
    'Inspect': 'the scope raised to the eye, then the rifle rolled to show the bolt and the action',
    'Holster': 'put away below the view (StowWeapon)',
    'Discard': 'empty: the bolt flicked open on the empty magazine (Empty), a look, thrown to the right and down (Release); '
               'the weapon clip goes on falling after Release (the game drops the pickup there)',
}

# ------------------------------------------------------------------------------------------------ geometry
BORE_Z = 0.040
MUZZLE_X = 0.746
BUTT_X = -0.345
GRIP_R = (0.0, 0.0, -0.002)                 # right hand (mitten centre) on the wrist
GRIP_L = (0.360, 0.0, 0.010)                # left hand under the fore-end
# telescopic sight, offset to the LEFT (+Y): axis, eyepiece rear end, objective front end, eye relief
SCOPE_Y, SCOPE_Z = 0.026, 0.076
SCOPE_X0, SCOPE_X1 = 0.020, 0.330
EYE_RELIEF = 0.080
SCOPE_EYE = (SCOPE_X0 - EYE_RELIEF, SCOPE_Y, SCOPE_Z)
# iron sights (kept under the scope's left offset): rear notch and front blade tops
IRON_REAR = (0.262, 0.0, BORE_Z + 0.0175)
IRON_FRONT = (0.733, 0.0, BORE_Z + 0.0175)
# bolt: closed face, travel, lift, handle root and knob centre (closed)
BOLT_FACE_X = 0.160
BOLT_REAR_X = 0.034
BOLT_R = 0.0085
BOLT_TRAVEL = 0.075
BOLT_LIFT_DEG = 60.0
BOLT_HANDLE_X = 0.047
KNOB = (0.049, -0.058, BORE_Z - 0.010)
# receiver: rear bridge (charger guide at its front), magazine well, ring
REAR_BRIDGE = (0.034, 0.083)
WELL = (0.085, 0.155)
RING = (0.155, 0.240)
RECEIVER_R = 0.016
# stripper clip (five rounds standing in a column in the guide), rounds
ROUND_LEN = 0.062
ROUND_R = 0.00525
CLIP_X = 0.0855                              # the strip (spine), just ahead of the open bolt face
CLIP_Z0 = 0.056                              # centre of the lowest round in the guide
CLIP_PITCH = 0.0118
PUSH_DEPTH = 0.0775                          # the column pushed down into the magazine (top round just under the bolt)
TRIGGER_PIVOT = (0.024, 0.0, -0.026)
EJECT = (0.120, -0.014, 0.054)
EJECT_DIR = (0.120, -0.060, 0.082)

BONES = {
    'root': ((0.0, 0.0, 0.0), (0.10, 0.0, 0.0), None),
    'bolt': ((BOLT_HANDLE_X, 0.0, BORE_Z), (BOLT_HANDLE_X + 0.10, 0.0, BORE_Z), 'root'),
    'trigger': (TRIGGER_PIVOT, (0.020, 0.0, -0.050), 'root'),
    'stripper_clip': ((CLIP_X, 0.0, CLIP_Z0 + 2 * CLIP_PITCH), (CLIP_X, 0.0, CLIP_Z0 + 2 * CLIP_PITCH + 0.03), 'root'),
    'clip_rounds': ((CLIP_X + ROUND_LEN * 0.5, 0.0, CLIP_Z0 + 2 * CLIP_PITCH), (CLIP_X + ROUND_LEN * 0.5, 0.0, CLIP_Z0 + 2 * CLIP_PITCH + 0.03), 'root'),
}
MARKERS = {'grip': ('root', GRIP_R), 'muzzle': ('root', (MUZZLE_X, 0.0, BORE_Z))}
SOCKETS = {
    'grip_r': ('root', GRIP_R),
    'grip_l': ('root', GRIP_L),
    'muzzle': ('root', (MUZZLE_X + 0.002, 0.0, BORE_Z)),
    'eject': ('root', EJECT),
    'eject_dir': ('root', EJECT_DIR),
    'scope_eye': ('root', SCOPE_EYE),
    'scope_lens': ('root', (SCOPE_X1 + 0.014, SCOPE_Y, SCOPE_Z)),
    'sight_rear': ('root', IRON_REAR),
    'sight_front': ('root', IRON_FRONT),
    'bolt_knob': ('bolt', KNOB),
    'clip': ('stripper_clip', (CLIP_X, 0.0, CLIP_Z0 + 4 * CLIP_PITCH + 0.008)),
    'butt': ('root', (BUTT_X - 0.004, 0.0, -0.040)),
}
CONTRACT_NAMES = {'grip': 'socket_grip_r', 'support': 'socket_grip_l', 'muzzle': 'socket_muzzle', 'eject': 'socket_eject',
                  'scope_eye': 'socket_scope_eye'}
MOVING_PARTS = {
    'bolt': 'turn-bolt: rotates up to 60 deg about the bore axis (BoltOpen), slides back 7.5 cm (CaseEject at the rear), '
            'forward, turned down (BoltClose); carries the handle, the knob (socket_bolt_knob) and the cocking piece',
    'trigger': 'pivots back 15 deg at Fire / DryClick',
    'stripper_clip': 'the steel strip of the clip; hidden (scale 0) outside Reload; Reload: from the pouch to the guide '
                     '(ClipIn), kicked out by the bolt (ClipEject), flies up and right, hidden once out of view',
    'clip_rounds': 'the five rounds as one rigid column; Reload: in the clip, thumbed down by PUSH_DEPTH into the magazine '
                   '(RoundsIn); every other clip shows them in the magazine (the top round under the open bolt)',
}
STATICS = {
    'SM_Bean_BoltRifle': {'view': 'pickup', 'role': 'the rifle at rest lying on the ground (pickup, DA_Item_BoltRifle.PickupMesh)'},
    'SM_Ammo_StripperClip': {'view': 'ammo', 'role': 'a stripper clip of five rounds (ammunition pickup: 5 rounds)'},
    'SM_Ammo_RifleClipBox': {'view': 'ammo', 'role': 'a cardboard box of four clips (20 rounds, shop / loot)'},
    'SM_BoltRifle_Casing': {'view': 'fx', 'role': 'spent case thrown at CaseEject from socket_eject (mouth +X, lying)'},
    'SM_BoltRifle_EmptyClip': {'view': 'fx', 'role': 'empty stripper clip thrown at ClipEject (the clip bone is hidden then)'},
}
BOUNDS_PLANNED = {'size': [1.10, 0.075, 0.20]}
RULES_HINTS = {
    'FirstPersonAlignBone': 'root', 'GripSocket': 'socket_grip_r', 'MuzzleSocket': 'socket_muzzle', 'FireEvent': 'Fire',
    'bAllowAiming': 'true (scope); the aim clips put socket_scope_eye on the camera axis, see fp_framing.aim_*',
    'AimFieldOfView': 'fp_framing.aim_field_of_view_deg', 'HipGripLocation/HipRotation': 'fp_framing.hip_*',
    'AimMuzzleLocation/AimRotation': 'fp_framing.aim_*', 'Capacity': '5 (magazine) + 1 in the chamber after BoltClose',
    'ShellEject': 'SM_BoltRifle_Casing at CaseEject from socket_eject toward socket_eject_dir',
}
IMPORT_ENTRY = {'pack_dir': EXPORT_DIR, 'manifest': 'pack.json', 'asset': TAG, 'display': 'Fusil à verrou',
                'muzzle_sockets': ['socket_muzzle'], 'base': 'SK_Bean_BoltRifle', 'fp': 'SK_Bean_BoltRifle_FP',
                'pickup': 'SM_Bean_BoltRifle', 'idle': 'Idle'}
NOTES = [
    'Bolt-action sniper rifle, late 19th century in the spirit of the Lee-Metford / Mauser (fictional): walnut stock, '
    'blued steel, 60 deg bolt lift, five round magazine loaded from a stripper clip, long brass scope offset to the left.',
    'Aim: AimIn / AimIdle / AimFire / AimOut are authored with the eye ON THE SCOPE AXIS, EYE_RELIEF behind the eyepiece '
    '(socket_scope_eye on the first person camera axis); fp_framing.aim_* reproduces that camera (zero offset in the game).',
    'The lenses are a separate material slot (M_BoltRifle_Lens, not baked): swap it for a scope render material if wanted.',
    'BoltCycle is a new gesture (EFirearmGesture::BoltCycle to add): Fire then BoltCycle chambers the next round.',
    'Events of Reload: BoltOpen, ClipIn, RoundsIn (count the five rounds here), ClipEject, BoltClose (a round chambered).',
]
