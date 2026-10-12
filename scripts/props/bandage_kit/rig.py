"""Pose solver and clip export on the bean's skeleton SKEL_Bean for the bandage kit (T-340), copied unchanged from the
capture kit (scripts/props/capture_kit/rig.py, T-337) so that both kits stay independent; one addition:
BeanRig.shoulder_offset (shoulder moved forward by a translation of upperarm_*, see shoulder()).

Conventions (bean_anims of the maquette bean-anim-library, scripts/character/bean_swim_clips.py): metres, the bean looks
along -Y, its left is +X, +Z up; trunk and legs turn about armature axes carried by the parent's pose (armature_rot);
arms by two bone IK from the posed shoulder to the wrist with a bounded uniform stretch of upper and lower arm (the
bean's arms are 24.5 cm to the wrist: the library clips stretch them too, 0.73 to 1.15), the mitten kept at its size
(local scale 1/stretch) and oriented on its own (hand_frame); legs by two bone IK to planted feet.
Export = the library's anim_export.export_range: armature only renamed "Armature", FBX_SCALE_NONE with unit scale,
-Y forward / Z up, deform bones, baked keys, frames 0..N (root at x100 in Unreal like every A_Bean_* clip).
"""
from __future__ import annotations

import math
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector

X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
REPO = Path(__file__).resolve().parents[3]
SOURCE = REPO / 'art' / 'blender' / 'characters' / 'bean_soldier.blend'


def load_bean(scene, name='BeanSoldier'):
    """Appends the bean's collection (rig + body + red coat + tricorne) from bean_soldier.blend, never saving that file"""
    with bpy.data.libraries.load(str(SOURCE), link=False) as (src, dst):
        dst.collections = [c for c in src.collections if c == 'BeanSoldier']
    coll = dst.collections[0]
    scene.collection.children.link(coll)
    for c in coll.children_recursive:
        if c.name.startswith(('SM_Bean_FPArm', 'Reference')):
            c.hide_render = True
            c.hide_viewport = True
    for c in [coll] + list(coll.children_recursive):
        if c.name.startswith('Rig'):
            c.hide_viewport = False
    arm = next(o for o in coll.all_objects if o.type == 'ARMATURE')
    arm.hide_viewport = False
    arm.animation_data_clear()
    meshes = [o for o in coll.all_objects if o.type == 'MESH' and not o.name.startswith(('REF', 'FPArm'))]
    return coll, arm, meshes


class BeanRig:
    """FK / IK of SKEL_Bean in armature space"""

    def __init__(self, arm, meshes=None):
        self.arm = arm
        self.bones = arm.data.bones
        self.order = []
        pending = [b for b in self.bones if b.parent is None]
        while pending:
            b = pending.pop(0)
            self.order.append(b.name)
            pending.extend(b.children)
        self.rest = {b.name: b.matrix_local.copy() for b in self.bones}
        self.rest_q = {n: m.to_quaternion() for n, m in self.rest.items()}
        # mitten centre (rest, armature space) of each hand: the grip point
        self.grip_rest = {}
        for side in ('l', 'r'):
            ob = bpy.data.objects.get(f'Bean_Hand_{side}')
            if ob is not None:
                pts = [ob.matrix_world @ Vector(c) for c in ob.bound_box]
                self.grip_rest[side] = sum(pts, Vector()) / 8
            else:
                b = self.bones[f'hand_{side}']
                self.grip_rest[side] = b.head_local.lerp(b.tail_local, 0.6)
        self.l1 = {s: (self.bones[f'lowerarm_{s}'].head_local - self.bones[f'upperarm_{s}'].head_local).length for s in 'lr'}
        self.l2 = {s: (self.bones[f'hand_{s}'].head_local - self.bones[f'lowerarm_{s}'].head_local).length for s in 'lr'}

    # ------------------------------------------------------------------ local pose helpers
    def armature_rot(self, name, rots):
        q = Quaternion()
        for axis, deg in rots:
            q = Quaternion(Vector(axis), math.radians(deg)) @ q
        rest = self.rest_q[name]
        return rest.inverted() @ q @ rest

    def armature_loc(self, name, offset):
        return self.rest[name].to_3x3().inverted() @ Vector(offset)

    def fk(self, local):
        """Armature space matrices for local {bone: (location, quaternion)}"""
        mats = {}
        for name in self.order:
            bone = self.bones[name]
            loc, q = local.get(name, (Vector(), Quaternion()))
            basis = Matrix.Translation(loc) @ q.to_matrix().to_4x4()
            if bone.parent:
                mats[name] = mats[bone.parent.name] @ (self.rest[bone.parent.name].inverted() @ self.rest[name]) @ basis
            else:
                mats[name] = self.rest[name] @ basis
        return mats

    def carried(self, mats, name):
        """Delta of a bone (posed @ rest^-1): carries rest points of its children"""
        return mats[name] @ self.rest[name].inverted()

    def shoulder(self, mats, side):
        """Shoulder (upperarm head) carried by the chest; shoulder_offset[side] (armature rest axes, metres) moves it, the
        bandage clip uses it to bring both arms forward (translation key on upperarm_*; the sleeve, cuff and epaulette are
        skinned to the arm bones at 95 %, so the whole arm slides on the egg)"""
        off = getattr(self, 'shoulder_offset', {}).get(side)
        head = self.bones[f'upperarm_{side}'].head_local
        if off is not None:
            head = head + Vector(off)
        return self.carried(mats, 'spine_02') @ head

    # ------------------------------------------------------------------ hands
    def hand_rest_frame(self, side):
        """Rest frame of a hand: Y along the bone (wrist -> mitten tip), X toward the thumb side (forward, -Y)"""
        b = self.bones[f'hand_{side}']
        y = (b.tail_local - b.head_local).normalized()
        t = Vector((0, -1, 0))
        x = (t - y * t.dot(y)).normalized()
        z = x.cross(y)
        return Matrix((x, y, z)).transposed()

    def hand_frame(self, direction, thumb):
        """Rotation (3x3) of a hand whose bone points along direction with the thumb side toward thumb"""
        y = Vector(direction).normalized()
        t = Vector(thumb)
        x = t - y * t.dot(y)
        if x.length < 1e-6:
            x = Vector((0, -1, 0)) - y * (-y.y)
        x.normalize()
        z = x.cross(y)
        return Matrix((x, y, z)).transposed()

    def hand_rotation(self, side, frame):
        """Armature space rotation taking the rest hand to frame (a hand_frame)"""
        return frame @ self.hand_rest_frame(side).inverted()

    def wrist_for_grip(self, side, grip, rotation):
        """Wrist (hand head) putting the mitten centre on grip for the hand rotation (3x3, from hand_rotation)"""
        b = self.bones[f'hand_{side}']
        return Vector(grip) - rotation @ (self.grip_rest[side] - b.head_local)

    def grip_of(self, mats, side):
        return self.carried(mats, f'hand_{side}') @ self.grip_rest[side]

    def solve_arm(self, mats, side, wrist, rotation=None, pole=None, max_stretch=1.25):
        """Two bone IK: the wrist (head of hand_<side>) on wrist; uniform stretch of the upper and lower arm when needed
        (up to max_stretch); the hand turned by rotation (3x3 armature space, from hand_rotation) or kept on the forearm.
        Returns (error m, stretch)"""
        up, low, hand = f'upperarm_{side}', f'lowerarm_{side}', f'hand_{side}'
        B = self.bones
        sh = self.shoulder(mats, side)
        l1, l2 = self.l1[side], self.l2[side]
        target = Vector(wrist)
        d = target - sh
        dist = d.length
        scale = min(max_stretch, max(1.0, dist / ((l1 + l2) * 0.985)))
        a1, a2 = l1 * scale, l2 * scale
        reach = min(dist, (a1 + a2) * 0.9995)
        axis = d.normalized()
        sign = 1 if side == 'l' else -1
        pole = Vector(pole) if pole is not None else Vector((sign * 0.9, 0.25, -0.6))
        perp = pole - axis * pole.dot(axis)
        if perp.length < 1e-6:
            perp = axis.orthogonal()
        perp.normalize()
        along = (a1 * a1 - a2 * a2 + reach * reach) / (2 * max(reach, 1e-8))
        along = max(-a1, min(a1, along))
        height = math.sqrt(max(0.0, a1 * a1 - along * along))
        elbow = sh + axis * along + perp * height
        hand_at = sh + axis * reach
        v1 = B[low].head_local - B[up].head_local
        v2 = B[hand].head_local - B[low].head_local
        S = Matrix.Diagonal((scale, scale, scale, 1.0))
        # keep the arm's twist from the parent (shortest swing from the carried rest direction)
        P = self.carried(mats, 'spine_02').to_3x3()
        Pq = P.to_quaternion()
        c1 = (Pq @ v1).normalized()
        q1 = c1.rotation_difference((elbow - sh).normalized()) @ Pq
        c2 = (q1 @ v2).normalized()
        q2 = c2.rotation_difference((hand_at - elbow).normalized()) @ q1
        mats[up] = Matrix.Translation(sh) @ q1.to_matrix().to_4x4() @ S @ Matrix.Translation(-B[up].head_local) @ self.rest[up]
        mats[low] = Matrix.Translation(elbow) @ q2.to_matrix().to_4x4() @ S @ Matrix.Translation(-B[low].head_local) @ self.rest[low]
        R = rotation if rotation is not None else q2.to_matrix()
        mats[hand] = Matrix.Translation(hand_at) @ R.to_4x4() @ Matrix.Translation(-B[hand].head_local) @ self.rest[hand]
        return (hand_at - target).length, scale

    # ------------------------------------------------------------------ legs
    def solve_leg(self, mats, side, ankle, foot_yaw=0.0, pole=None):
        """Thigh and calf to put the ankle (head of foot_<side>) on ankle; the foot flat, turned by foot_yaw (deg)"""
        th, ca, ft = f'thigh_{side}', f'calf_{side}', f'foot_{side}'
        B = self.bones
        hip = self.carried(mats, 'pelvis') @ B[th].head_local
        l1 = (B[ca].head_local - B[th].head_local).length
        l2 = (B[ft].head_local - B[ca].head_local).length
        d = Vector(ankle) - hip
        dist = min(d.length, (l1 + l2) * 0.9995)
        axis = d.normalized()
        pole = Vector(pole) if pole is not None else Vector((0.12 if side == 'l' else -0.12, -1.0, 0.0))
        perp = (pole - axis * pole.dot(axis)).normalized()
        along = max(-l1, min(l1, (l1 * l1 - l2 * l2 + dist * dist) / (2 * max(dist, 1e-8))))
        knee = hip + axis * along + perp * math.sqrt(max(0.0, l1 * l1 - along * along))
        at = hip + axis * dist
        v1 = B[ca].head_local - B[th].head_local
        v2 = B[ft].head_local - B[ca].head_local
        Pq = self.carried(mats, 'pelvis').to_quaternion()
        q1 = (Pq @ v1).normalized().rotation_difference((knee - hip).normalized()) @ Pq
        q2 = (q1 @ v2).normalized().rotation_difference((at - knee).normalized()) @ q1
        mats[th] = Matrix.Translation(hip) @ q1.to_matrix().to_4x4() @ Matrix.Translation(-B[th].head_local) @ self.rest[th]
        mats[ca] = Matrix.Translation(knee) @ q2.to_matrix().to_4x4() @ Matrix.Translation(-B[ca].head_local) @ self.rest[ca]
        R = Matrix.Rotation(math.radians(foot_yaw), 4, 'Z')
        mats[ft] = Matrix.Translation(at) @ R @ Matrix.Translation(-B[ft].head_local) @ self.rest[ft]
        return (at - Vector(ankle)).length

    def ankle_rest(self, side):
        return self.bones[f'foot_{side}'].head_local.copy()

    # ------------------------------------------------------------------ keys
    def set_pose(self, mats):
        for pb in self.arm.pose.bones:
            kw = {}
            if pb.parent:
                kw = dict(parent_matrix=mats[pb.parent.name], parent_matrix_local=self.rest[pb.parent.name])
            pb.matrix_basis = pb.bone.convert_local_to_pose(mats[pb.name], self.rest[pb.name], invert=True, **kw)

    def bake(self, samples, name):
        """One action keyed on every bone at frames 0..len-1 (location, quaternion, scale), linear"""
        arm = self.arm
        for pb in arm.pose.bones:
            for c in list(pb.constraints):
                pb.constraints.remove(c)
            pb.rotation_mode = 'QUATERNION'
        arm.animation_data_create()
        old = bpy.data.actions.get(name)
        if old is not None:
            bpy.data.actions.remove(old)
        action = bpy.data.actions.new(name)
        action.use_fake_user = True
        arm.animation_data.action = action
        prev = {}
        for f, mats in enumerate(samples):
            self.set_pose(mats)
            for pb in arm.pose.bones:
                q = pb.rotation_quaternion.copy()
                if pb.name in prev:
                    q.make_compatible(prev[pb.name])
                    pb.rotation_quaternion = q
                prev[pb.name] = q
                for prop in ('location', 'rotation_quaternion', 'scale'):
                    pb.keyframe_insert(prop, frame=f, group=pb.name)
        action.use_frame_range = True
        action.frame_start = 0
        action.frame_end = len(samples) - 1
        for fc in _fcurves(action):
            for k in fc.keyframe_points:
                k.interpolation = 'LINEAR'
        return action


def assign_action(obj, action):
    """Action on obj with its slot (Blender 4.4+ slotted actions: an action keyed on another armature needs its slot
    assigned explicitly, else the clone stays in rest pose)"""
    obj.animation_data_create()
    obj.animation_data.action = action
    if hasattr(obj.animation_data, 'action_slot') and getattr(action, 'slots', None):
        if obj.animation_data.action_slot is None or obj.animation_data.action_slot not in list(action.slots):
            obj.animation_data.action_slot = action.slots[0]
    return obj


def _fcurves(action):
    if hasattr(action, 'fcurves') and len(getattr(action, 'fcurves', [])):
        return list(action.fcurves)
    out = []
    for layer in getattr(action, 'layers', []):
        for strip in layer.strips:
            for bag in getattr(strip, 'channelbags', []):
                out.extend(bag.fcurves)
    return out


def export_clip(arm, action, path, frames):
    """The library's export_range: armature only, renamed Armature, deform bones, FBX_SCALE_NONE, baked keys"""
    scene = bpy.context.scene
    assign_action(arm, action)
    keep = (scene.frame_start, scene.frame_end)
    scene.frame_start, scene.frame_end = 0, frames
    scene.frame_set(0)
    vl = bpy.context.view_layer
    for o in vl.objects:
        o.select_set(False)
    arm.hide_set(False)
    arm.select_set(True)
    vl.objects.active = arm
    name = arm.name
    clash = bpy.data.objects.get('Armature')
    if clash is not None and clash is not arm:
        clash.name = 'Armature_tmp'
    arm.name = 'Armature'
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    try:
        for attempt in range(6):
            try:
                bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'ARMATURE'},
                                         apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', global_scale=1.0,
                                         axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE', use_mesh_modifiers=True,
                                         add_leaf_bones=False, primary_bone_axis='Y', secondary_bone_axis='X',
                                         use_armature_deform_only=True, bake_anim=True, bake_anim_use_all_actions=False,
                                         bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                                         bake_anim_simplify_factor=0.0)
                break
            except RuntimeError:
                if attempt == 5:
                    raise
                import time
                time.sleep(1.0)
    finally:
        arm.name = name
        if clash is not None and clash is not arm:
            clash.name = 'Armature'
        scene.frame_start, scene.frame_end = keep
    return path


def relative_to_bone(rig, mats, bone, world):
    """A transform (4x4, armature space) relative to a posed bone, in Blender bone space and in the convention of the
    project's Unreal bone offsets (TorchRules::GripInHand from attach_torch.json, CutlassRules::LibraryGripInHand):
    location cm (x, -y, z), FRotator(pitch = -euler y, yaw = -euler z, roll = euler x) of the Blender XYZ euler"""
    rel = mats[bone].inverted() @ world
    t, q, s = rel.decompose()
    e = q.to_euler('XYZ')
    return {'bone': bone,
            'blender_bone_space': {'location_m': [round(v, 5) for v in t], 'quaternion_wxyz': [round(v, 6) for v in q],
                                   'rotation_euler_deg_xyz': [round(math.degrees(v), 3) for v in e]},
            'unreal_guess': {'location_cm': [round(t.x * 100, 3), round(-t.y * 100, 3), round(t.z * 100, 3)],
                             'rotator_pitch_yaw_roll_deg': [round(-math.degrees(e.y), 3), round(-math.degrees(e.z), 3),
                                                            round(math.degrees(e.x), 3)],
                             'convention': 'like TorchRules::GripInHand (attach_torch.json): to check in the editor'}}


def character_space(m, forward=(1.0, 0.0, 0.0)):
    """Armature space (Blender, bean facing -Y) -> the character's Unreal frame: X forward, Y right, Z up, cm, yaw deg.
    forward = the object's own forward axis in Blender: +X for the props, -Y for a bean (another character)"""
    t = m.to_translation()
    fwd = m.to_3x3() @ Vector(forward)
    # Blender -Y is the bean's forward (Unreal +X); Blender -X is its right (Unreal +Y)
    loc = [round(-t.y * 100, 2), round(-t.x * 100, 2), round(t.z * 100, 2)]
    # the object's forward in the character frame (forward = Blender -Y, right = Blender -X)
    yaw_ue = math.degrees(math.atan2(-fwd.x, -fwd.y))
    yaw_ue = 180.0 if abs(abs(yaw_ue) - 180.0) < 1e-6 else yaw_ue
    return {'location_cm': loc, 'yaw_deg': round(yaw_ue + 0.0, 2)}
