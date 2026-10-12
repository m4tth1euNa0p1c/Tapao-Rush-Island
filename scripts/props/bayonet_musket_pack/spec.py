"""Contract of the bayonet musket pack (T-334, owner 2026-10-11: "fusil mousquet avec baïonnette et utilisation de
baïonnette"): the pirate musket of scripts/props/musket_pack (T-212, v2 T-246) with a SOCKET BAYONET fixed under the
muzzle, every musket clip kept as it is (same names, lengths and events, GroundReload included) plus two new gestures,
BayonetStab (a thrust, StabHit at the point of impact) and BayonetCharge (a running charge ending in a lunge,
ChargeHit). Pure Python: the provisional manifest and the build take their names from here; build.py checks that
MUSKET_CLIPS / MUSKET_EVENTS still match the musket pack's animation module and stops otherwise.

The bayonet (bone "bayonet", rigid, child of root; HideBoneByName("bayonet") shows the plain musket): a socket (douille)
slipped over the muzzle behind the lug, a cranked neck, a triangular blade running forward UNDER the muzzle, offset a
little to the left so the ramrod still slides along its channel, socket_bayonet_tip at the point.
Weapon space of the musket (metres): +X muzzle, +Z up, -Y lock side (right), origin = the right hand grip.
"""
PACK_ID = 'bean-bayonet-musket'
TAG = 'BayonetMusket'
DISPLAY = 'Mousquet à baïonnette'
SCRIPT_DIR = 'bayonet_musket_pack'
EXPORT_DIR = 'art/export/Weapons/BayonetMusket'
FPS = 24
ROOT_BONE = 'root'
IDLE_CLIP = 'Hold'

# the musket pack's clips (scripts/props/musket_pack/animation.py CLIPS / events(), v0.4.0-local-v2), kept as they are
MUSKET_CLIPS = {'Equip': 42, 'Idle': 72, 'Walk': 96, 'Sprint': 72, 'AimIn': 24, 'AimIdle': 72, 'AimOut': 24, 'HalfCock': 24,
                'PrimePan': 24, 'CloseFrizzen': 12, 'GroundReload': 132, 'Powder': 24, 'BallAndPatch': 20, 'DrawRamrod': 24,
                'TurnRamrod': 24, 'RamrodStroke': 29, 'ReturnRamrod': 24, 'Cock': 24, 'Present': 18, 'Fire': 24, 'Recoil': 20,
                'Hold': 48, 'ReturnToPort': 24, 'Holster': 42, 'Inspect': 96, 'Discard': 32}
MUSKET_EVENTS = {
    'Equip': [(8, 'DrawWeapon'), (34, 'Ready')],
    'HalfCock': [(8, 'HalfCockStart')],
    'PrimePan': [(18, 'PanPrime', 'socket_pan')],
    'CloseFrizzen': [(10, 'FrizzenClose', 'socket_pan')],
    'GroundReload': [(3, 'HalfCockStart'), (27, 'PanPrime', 'socket_pan'), (37, 'FrizzenClose', 'socket_pan'), (48, 'StockLowered'),
                     (55, 'PowderCharge', 'socket_muzzle'), (67, 'BallAndPatch', 'socket_muzzle'), (75, 'RamrodDraw', 'socket_ramrod_tip'),
                     (89, 'RamrodTurn'), (99, 'RamrodStroke', 'socket_muzzle'), (104, 'RamrodStroke', 'socket_muzzle'),
                     (118, 'RamrodReturn'), (130, 'FullCockEnd')],
    'Powder': [(10, 'StockLowered'), (17, 'PowderCharge', 'socket_muzzle')],
    'BallAndPatch': [(10, 'BallAndPatch', 'socket_muzzle')],
    'DrawRamrod': [(5, 'RamrodDraw', 'socket_ramrod_tip')],
    'TurnRamrod': [(10, 'RamrodTurn')],
    'RamrodStroke': [(10, 'RamrodStroke', 'socket_muzzle'), (19, 'RamrodStroke', 'socket_muzzle')],
    'ReturnRamrod': [(16, 'RamrodReturn')],
    'Cock': [(19, 'FullCockEnd')],
    'Fire': [(5, 'Trigger', 'socket_trigger'), (8, 'PanFlash', 'socket_pan'), (8, 'MuzzleFlash', 'socket_muzzle'), (8, 'Shot', 'socket_muzzle'),
             (8, 'Recoil')],
    'Holster': [(36, 'StowWeapon')],
    'Discard': [(1, 'Empty', 'socket_pan'), (22, 'Release', 'socket_grip_r')],
}
NEW_CLIPS = {'BayonetStab': 15, 'BayonetCharge': 24}
NEW_EVENTS = {
    'BayonetStab': [(3, 'StabStart'), (7, 'StabHit', 'socket_bayonet_tip')],
    'BayonetCharge': [(2, 'ChargeStart'), (16, 'ChargeHit', 'socket_bayonet_tip')],
}
NEW_GESTURES = tuple(NEW_CLIPS)
CLIPS = dict(MUSKET_CLIPS, **NEW_CLIPS)
EVENTS = dict(MUSKET_EVENTS, **NEW_EVENTS)
LOOPS = {'Idle', 'Walk', 'Sprint', 'AimIdle', 'Hold'}
CLIP_NOTES = {
    'BayonetStab': 'a short thrust: drawn back (wind-up), driven forward along the barrel to full extension (StabHit at the '
                   'point of impact, socket_bayonet_tip), held a beat, recovered to the hip; TP: the torso leans into it',
    'BayonetCharge': 'charge bayonet: the musket lowered to the hip, bayonet forward, a running stride, then a lunge '
                     '(ChargeHit at full extension), recovered; TP full body (legs run then lunge; the game keeps its legs)',
    'GroundReload': 'the musket reload, unchanged (the bayonet stays fixed: the blade runs under the muzzle, left of the ramrod)',
}

# ------------------------------------------------------------------------------------------------ geometry
BORE_Z = 0.045
MUZZLE_X = 0.620                              # end of the barrel (socket_muzzle at 0.626)
BARREL_R_MUZZLE = 0.0156
SOCKET_X = (0.588, 0.624)                    # the socket (douille) over the muzzle, behind the lug at 0.574..0.586
SOCKET_R = (0.0163, 0.0188)                  # inner, outer radius
BLADE_ROOT = (0.645, 0.020, 0.019)           # blade axis root: under the muzzle, 2 cm to the left (+Y), clear of the ramrod
BLADE_LEN = 0.260
BLADE_TIP = (BLADE_ROOT[0] + BLADE_LEN, BLADE_ROOT[1], BLADE_ROOT[2])
BLADE_W, BLADE_H = 0.017, 0.011              # triangular section at the root (width across, height)

BONES = {
    'bayonet': ((0.606, 0.0, BORE_Z), (0.666, 0.0, BORE_Z), 'root'),
}
MARKERS = {'grip': ('root', (0.0, 0.0, -0.002)), 'muzzle': ('root', (MUZZLE_X + 0.006, 0.0, BORE_Z))}
SOCKETS = {
    'bayonet_tip': ('bayonet', BLADE_TIP),
}
# the musket's own bones and sockets (musket_model.build_musket), listed by the manifest after the build
MUSKET_BONES = ['root', 'hammer', 'frizzen', 'trigger', 'ramrod', 'cartridge']
MUSKET_SOCKETS = ['grip_r', 'grip_l', 'barrel', 'muzzle', 'pan', 'butt', 'sling', 'ramrod_tip', 'ramrod_end', 'flint']
CONTRACT_NAMES = {'grip': 'socket_grip_r', 'support': 'socket_grip_l', 'muzzle': 'socket_muzzle', 'bayonet_tip': 'socket_bayonet_tip',
                  'pan': 'socket_pan'}
MOVING_PARTS = {
    'bayonet': 'rigid on root in every clip (fixed bayonet); HideBoneByName("bayonet") hides the socket, neck and blade',
    'hammer / frizzen / trigger / ramrod / cartridge': 'the musket\'s, unchanged',
}
STATICS = {
    'SM_Bean_BayonetMusket': {'view': 'pickup', 'role': 'the musket with its fixed bayonet lying on the ground (pickup)'},
}
BOUNDS_PLANNED = {'size': [1.22, 0.07, 0.19]}
RULES_HINTS = {
    'FirstPersonAlignBone': 'root', 'GripSocket': 'socket_grip_r', 'MuzzleSocket': 'socket_muzzle', 'FireEvent': 'Shot',
    'HipGripLocation/HipRotation': 'fp_framing.hip_* (the musket\'s values)', 'AimMuzzleLocation/AimRotation': 'fp_framing.aim_*',
    'Melee': 'BayonetStab: damage at StabHit along socket_bayonet_tip; BayonetCharge: damage at ChargeHit',
    'Aliases': 'AimFire -> Fire (the musket family has no AimFire clip)',
}
IMPORT_ENTRY = {'pack_dir': EXPORT_DIR, 'manifest': 'pack.json', 'asset': TAG, 'display': 'Mousquet à baïonnette',
                'muzzle_sockets': ['socket_muzzle'], 'aliases': {'AimFire': 'Fire'}, 'idle': 'Hold',
                'base': 'SK_Bean_BayonetMusket', 'fp': 'SK_Bean_BayonetMusket_FP', 'pickup': 'SM_Bean_BayonetMusket'}
NOTES = [
    'The pirate musket of scripts/props/musket_pack (v0.4.0-local-v2) with a socket bayonet: every musket clip is rebuilt by '
    'the musket\'s own animation module (same choreography, lengths and events), the weapon skeleton gains the bone '
    '"bayonet" and socket_bayonet_tip.',
    'New gestures BayonetStab (15 frames, StabHit 7) and BayonetCharge (24 frames, ChargeHit 16), 24 fps like the musket; '
    'EFirearmGesture::BayonetStab / BayonetCharge to add.',
]
