"""The musket pack's modules, imported by path (scripts/props/musket_pack, T-212) for the long arm packs (T-250).

  A   animation.py : easing, FP camera and poses, hand solvers, baking, events (patched per weapon by choreo.apply)
  K   kit.py       : shape helpers, skeleton, join_parts, part_physics, bean() (scripts/character/bean_soldier.py)
  MB  build.py     : source_bean / clone_bean / stage / render / camera_record (its import reloads A and K: import it first)
  MX  export_pack.py : sha, select, mesh_export

The attributes listed in REQUIRED are the ones the long arm packs rely on; a musket pack rewrite that drops one of them
stops the build with a clear message instead of a stack trace deep in Blender (T-246 reworks the musket pack in
parallel: rebuild the long arm packs after it lands).
"""
from __future__ import annotations

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent
PROPS = HERE.parent
REPO = PROPS.parents[1]
MUSKET = PROPS / 'musket_pack'
if str(MUSKET) not in sys.path:
    sys.path.insert(0, str(MUSKET))

import build as MB  # noqa: E402  (reloads animation and kit at import)
import animation as A  # noqa: E402
import kit as K  # noqa: E402
import export_pack as MX  # noqa: E402

REQUIRED = {
    A: ['FPS', 'CLIPS', 'RELOAD_WINDOWS', 'GRIP_R', 'GRIP_L', 'BAND_GRIP', 'MUZZLE', 'BORE_Z', 'PAN', 'HAMMER_SPUR',
        'FRIZZEN_TOE', 'ROD_HEAD', 'ROD_DIR', 'ROD_LEN', 'HAMMER_FIRED', 'HAMMER_HALF', 'FRIZZEN_OPEN', 'FP_POSES',
        'GROUND_RELOAD_EVENTS', 'smooth', 'curve', 'segment', 'frame', 'axes', 'blend', 'camera_matrix', 'vp', 'vv',
        'to_view', 'view_pose', 'rotator_in_view', 'fp_pose', 'tp_pose_at', 'tp_poses', 'poses', 'weapon_pose',
        'reload_frame', 'reload_pose', 'hammer_angle', 'frizzen_angle', 'trigger_angle', 'rod_point', 'rod_delta',
        'right_keys', 'left_keys', 'REACH', 'LOAD_ELBOW', 'PINCH', 'TP_PINCH', 'UP', 'FWD', 'RIGHT', 'hand_frame',
        'resolve_target', 'keyed_hand', 'clip_hands', 'cartridge_world', 'local_rot', 'fp_arm_transform',
        'solve_fp_arm', 'solve_tp_arm', 'solve_leg', 'linear', 'set_matrices', 'bake_matrices', 'events', 'animate',
        'fp_framing', 'FP_CAMERA', 'FP_LENS', 'RELEASE_FRAME', 'EMPTY_FRAME', 'ease_os', 'keyed_pose', 'inspect_pose',
        'discard_pose', 'gesture_framing', 'R_GRIP', 'REACH', 'LOAD_ELBOW'],
    K: ['bean', 'loft', 'ring', 'cylinder_x', 'disc', 'flint_chip', 'part_physics', 'join_parts', 'make_skeleton',
        'DENSITY', 'V'],
    MB: ['source_bean', 'clone_bean', 'clone_arm', 'clone_meshes', 'stage', 'render', 'camera_record', 'material',
         'write', 'sha', 'active', 'SOURCE'],
    MX: ['sha', 'select', 'mesh_export'],
}
_missing = [f'{m.__name__}.{n}' for m, names in REQUIRED.items() for n in names if not hasattr(m, n)]
if _missing:
    raise SystemExit('[longarm] the musket pack changed, the long arm packs need an update: missing ' + ', '.join(_missing))
