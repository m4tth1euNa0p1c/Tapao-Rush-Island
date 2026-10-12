"""Swim clips of the player bean (T-017, MVP ADR-008): A_Bean_Swim (breaststroke loop, 1.2 s) and A_Bean_TreadWater
(treading water in place, 1.6 s), 30 fps, root fixed (no root motion: the capsule moves the bean), on SKEL_Bean.

  "C:/Program Files/Blender Foundation/Blender 5.2/blender.exe" -b --factory-startup \
      art/blender/characters/bean_soldier.blend --python scripts/character/bean_swim_clips.py -- --all

  --check   loop, water clearance (eyes, moustache, hat above WATER_LINE) and hands inside the body, per frame
  --export  art/export/Characters/Bean/Anims/A_Bean_Swim.fbx, A_Bean_TreadWater.fbx + bean_swim_clips.json
  --render  art/_previews/bean_swim_brasse.png (stroke, 4 phases, side), bean_swim_surplace.png (treading, 3/4)
  --all     the three

The bean .blend is opened read-only: this script never saves it (the rig is built by bean_soldier.py).
Poses: trunk, legs and coat tails by rotations about armature axes (convention of the maquette bean-anim-library,
kit bean_anims.key_pose); arms by two bone IK toward wrist paths given relative to the shoulders (short bean arms).
Export: settings of the library clips (factory kit anim_export.export_range): armature renamed "Armature",
FBX_SCALE_NONE with unit scale, -Y forward / Z up, deform bones only, baked keys. Unreal folds the "Armature" node
(scale 100) into the root bone: the root comes in at x100 like every A_Bean_* clip (a root at 1 would draw the bean
a hundred times too small, Creatures/PoseRoot.h).
Water line: the clips are authored for a water surface WATER_LINE above the bean root (SwimRootDepth of
DA_BeanAnimLibrary, 60 cm): UBeanPuppetComponent floats the bean there while it swims, head and hat out of the water.
"""
import argparse
import json
import math
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Quaternion, Vector
from mathutils.bvhtree import BVHTree

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art" / "export" / "Characters" / "Bean" / "Anims"
PREV = ROOT / "art" / "_previews"
REPORT = OUT / "bean_swim_clips.json"

FPS = 30
WATER_LINE = 0.60          # m above the bean root: water surface of the clips (SwimRootDepth = 60 cm)
MIN_EYE_CLEARANCE = 0.08   # eyes and moustache at least this far above the water in every frame (m)
MIN_HAT_CLEARANCE = 0.12   # lowest point of the tricorne above the water (m)
FP_CAMERA = (0.0, -0.27, 0.82)  # first person camera on UBeanPuppetComponent (FirstPersonEyeLocation 0, 27, 82 cm)
ARMATURE = "SKEL_Bean"
CLIPS = {
    # name: first frame, length (s); the last frame repeats the first (seamless loop in Unreal)
    "A_Bean_Swim": {"start": 1, "seconds": 1.2},
    "A_Bean_TreadWater": {"start": 61, "seconds": 1.6},
}
X, Y, Z = Vector((1, 0, 0)), Vector((0, 1, 0)), Vector((0, 0, 1))
TRUNK = ("pelvis", "spine_01", "spine_02", "head", "hat")
LEGS = ("thigh", "calf", "foot")


# ---------------------------------------------------------------- curves

def catmull_rom(a, b, c, d, t):
    t2, t3 = t * t, t * t * t
    return 0.5 * (2 * b + (c - a) * t + (2 * a - 5 * b + 4 * c - d) * t2 + (3 * b - a - 3 * c + d) * t3)


def cyclic(phase, keys):
    """Cyclic Catmull-Rom through keys [(phase in [0, 1), (values...)), ...] sorted by phase"""
    p = phase % 1.0
    n = len(keys)
    i = n - 1
    for j in range(n):
        if keys[j][0] <= p:
            i = j
    start = keys[i][0]
    end = keys[(i + 1) % n][0] + (1.0 if i == n - 1 else 0.0)
    pp = p if p >= start else p + 1.0
    t = (pp - start) / (end - start)
    v0, v1, v2, v3 = (keys[(i + k) % n][1] for k in (-1, 0, 1, 2))
    return tuple(catmull_rom(a, b, c, d, t) for a, b, c, d in zip(v0, v1, v2, v3))


def wave(phase, cycles=1.0, shift=0.0):
    return math.sin(2 * math.pi * (phase * cycles + shift))


# ---------------------------------------------------------------- rig

class Rig:
    """Forward kinematics of SKEL_Bean in armature space (Blender's pose formula, default inheritance), so the arm IK
    can read the posed shoulders before the keys are written"""

    def __init__(self, arm):
        self.arm = arm
        self.bones = arm.data.bones
        self.order = []
        pending = [b for b in self.bones if b.parent is None]
        while pending:
            bone = pending.pop(0)
            self.order.append(bone.name)
            pending.extend(bone.children)
        self.rest_q = {b.name: b.matrix_local.to_quaternion() for b in self.bones}
        self.length = {b.name: (b.tail_local - b.head_local).length for b in self.bones}

    def armature_rot(self, name, rots):
        """Local rotation of a bone turning it by rots [(axis, deg), ...] about armature axes (applied in order), carried
        by its parent's pose (bean_anims.key_pose)"""
        q = Quaternion()
        for axis, deg in rots:
            q = Quaternion(Vector(axis), math.radians(deg)) @ q
        rest = self.rest_q[name]
        return rest.inverted() @ q @ rest

    def armature_loc(self, name, offset):
        """Local location of a bone moved by an armature space offset"""
        return self.bones[name].matrix_local.to_3x3().inverted() @ Vector(offset)

    def pose(self, local):
        """Armature space pose matrices for local {bone: (location, quaternion)}"""
        mats = {}
        for name in self.order:
            bone = self.bones[name]
            loc, q = local.get(name, (Vector(), Quaternion()))
            basis = Matrix.Translation(loc) @ q.to_matrix().to_4x4()
            if bone.parent:
                mats[name] = mats[bone.parent.name] @ (bone.parent.matrix_local.inverted() @ bone.matrix_local) @ basis
            else:
                mats[name] = bone.matrix_local @ basis
        return mats

    def aim(self, name, local, direction):
        """Local rotation that points the bone (its Y axis) along an armature space direction, shortest swing from the
        pose its parent gives it"""
        bone = self.bones[name]
        parent = self.pose(local)[bone.parent.name]
        frame = (parent @ (bone.parent.matrix_local.inverted() @ bone.matrix_local)).to_quaternion()
        current = frame @ Y
        swing = current.rotation_difference(Vector(direction).normalized())
        return frame.inverted() @ swing @ frame

    def solve_arm(self, local, side, wrist, pole):
        """Two bone IK of one arm: the wrist on wrist (armature space), the elbow toward pole"""
        up, low, hand = f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"
        mats = self.pose(local)
        shoulder = mats[up].translation
        l1, l2 = self.length[up], self.length[low]
        to_target = Vector(wrist) - shoulder
        dist = min(max(to_target.length, abs(l1 - l2) + 1e-4), (l1 + l2) * 0.995)
        axis = to_target.normalized()
        cos_a = max(-1.0, min(1.0, (l1 * l1 + dist * dist - l2 * l2) / (2 * l1 * dist)))
        bend = Vector(pole) - axis * Vector(pole).dot(axis)
        if bend.length < 1e-6:
            bend = axis.orthogonal()
        bend.normalize()
        elbow = shoulder + axis * (l1 * cos_a) + bend * (l1 * math.sqrt(max(0.0, 1 - cos_a * cos_a)))
        reach = shoulder + axis * dist
        local[up] = (Vector(), self.aim(up, local, elbow - shoulder))
        local[low] = (Vector(), self.aim(low, local, reach - elbow))
        local[hand] = (Vector(), Quaternion())
        return dist / (l1 + l2)

    def shoulders(self, local):
        mats = self.pose(local)
        return {s: mats[f"upperarm_{s}"].translation.copy() for s in ("l", "r")}


def legs(rig, out, side, flex, abd, knee, foot):
    """flex > 0: thigh toward the belly; abd > 0: leg out; knee > 0: calf back; foot > 0: toes pointed"""
    sign = -1 if side == "l" else 1
    out[f"thigh_{side}"] = (Vector(), rig.armature_rot(f"thigh_{side}", [(X, -flex), (Y, sign * abd)]))
    out[f"calf_{side}"] = (Vector(), rig.armature_rot(f"calf_{side}", [(X, knee)]))
    out[f"foot_{side}"] = (Vector(), rig.armature_rot(f"foot_{side}", [(X, foot)]))


def trunk(rig, out, pitch, roll, head_pitch, head_yaw, offset, spine=(3.0, 2.0)):
    """pitch > 0 leans the body forward (top toward -Y) about the pelvis; roll about the forward axis; head_pitch < 0
    lifts the face; offset moves the pelvis (armature space, m)"""
    out["pelvis"] = (rig.armature_loc("pelvis", offset), rig.armature_rot("pelvis", [(X, pitch), (Y, roll)]))
    out["spine_01"] = (Vector(), rig.armature_rot("spine_01", [(X, spine[0])]))
    out["spine_02"] = (Vector(), rig.armature_rot("spine_02", [(X, spine[1])]))
    out["head"] = (Vector(), rig.armature_rot("head", [(X, head_pitch), (Z, head_yaw)]))
    out["hat"] = (Vector(), Quaternion())


def coat_tails(rig, out, lift, flutter):
    """Coat tails float up behind the bean in the water"""
    for side, sign in (("l", 1), ("r", -1)):
        out[f"coattail_{side}"] = (Vector(), rig.armature_rot(f"coattail_{side}", [(X, lift + flutter), (Y, -sign * 6)]))


def arm_target(shoulder, side, out_, fwd, up):
    """Wrist position from a shoulder: out_ away from the body (+X left, -X right), fwd toward -Y, up along +Z"""
    sign = 1 if side == "l" else -1
    return shoulder + Vector((sign * out_, -fwd, up))


# ---------------------------------------------------------------- the two clips

# breaststroke, phase 0 = arms stretched forward at the start of the pull
SWIM_TRUNK = [  # phase: (pelvis pitch, head pitch, pelvis up) -- head-up breaststroke, shoulders at the surface
    (0.00, (21.0, -11.0, 0.080)), (0.16, (19.0, -11.0, 0.086)), (0.30, (15.0, -8.0, 0.094)),
    (0.42, (17.0, -9.0, 0.088)), (0.56, (22.0, -11.0, 0.078)), (0.70, (23.5, -12.0, 0.074)),
    (0.85, (22.5, -12.0, 0.076))]
SWIM_ARM = [  # phase: wrist from the shoulder (out, fwd, up), m
    (0.00, (-0.02, 0.215, -0.04)), (0.16, (0.12, 0.16, -0.06)), (0.30, (0.07, 0.10, -0.12)),
    (0.42, (0.03, 0.16, -0.08)), (0.56, (-0.02, 0.20, -0.05)), (0.75, (-0.02, 0.215, -0.04))]
SWIM_LEG = [  # phase: (hip flexion, abduction, knee, foot), deg -- frog kick, legs trailing behind in the glide
    (0.00, (-28.0, 3.0, 5.0, 45.0)), (0.30, (-18.0, 8.0, 45.0, 25.0)), (0.45, (12.0, 20.0, 100.0, -10.0)),
    (0.56, (-5.0, 30.0, 60.0, 0.0)), (0.66, (-22.0, 12.0, 15.0, 30.0)), (0.80, (-30.0, 2.0, 5.0, 45.0))]
SWIM_BACK = 0.20   # pelvis moved back: the leaning egg stays centred on the capsule, eyes behind the first person camera


def swim_pose(rig, phase):
    out = {}
    pitch, head, up = cyclic(phase, SWIM_TRUNK)
    roll = 3.0 * wave(phase)
    trunk(rig, out, pitch, roll, head, 0.0, (0.0, SWIM_BACK, up))
    for side in ("l", "r"):
        legs(rig, out, side, *cyclic(phase, SWIM_LEG))
    coat_tails(rig, out, 48.0, 10.0 * wave(phase, shift=0.35))
    shoulders = rig.shoulders(out)
    reach = {}
    for side in ("l", "r"):
        sign = 1 if side == "l" else -1
        o, f, u = cyclic(phase, SWIM_ARM)
        pole = Vector((sign * 1.0, 0.35, 0.6))  # high elbows, out and back
        reach[side] = rig.solve_arm(out, side, arm_target(shoulders[side], side, o, f, u), pole)
    return out, reach


TREAD_BACK = 0.06  # pelvis moved back: the leaning face stays behind the first person camera


def tread_pose(rig, phase):
    out = {}
    trunk(rig, out, 8.0 + 1.5 * wave(phase, 2, 0.25), 3.0 * wave(phase), -6.0, 5.0 * wave(phase, 1, 0.1),
          (0.0, TREAD_BACK, 0.03 + 0.012 * wave(phase, 2)))
    for side, shift in (("l", 0.0), ("r", 0.5)):
        theta = phase + shift  # pedalling, the legs half a turn apart
        legs(rig, out, side, 28.0 + 20.0 * wave(theta), 14.0 + 4.0 * wave(theta),
             55.0 + 30.0 * wave(theta, 1, 0.1), 10.0 - 20.0 * wave(theta, 1, 0.25))
    coat_tails(rig, out, 40.0, 6.0 * wave(phase, 2))
    shoulders = rig.shoulders(out)
    reach = {}
    for side in ("l", "r"):
        sign = 1 if side == "l" else -1
        # sculling beside the belly, two sweeps out and in per cycle
        o = 0.12 + 0.04 * wave(phase, 2)
        f = 0.09 + 0.02 * wave(phase, 2, 0.25)
        u = -0.13 + 0.01 * wave(phase, 4)
        reach[side] = rig.solve_arm(out, side, arm_target(shoulders[side], side, o, f, u), Vector((sign * 1.0, 0.6, 0.1)))
    return out, reach


POSES = {"A_Bean_Swim": swim_pose, "A_Bean_TreadWater": tread_pose}


# ---------------------------------------------------------------- keys

def frames(clip):
    spec = CLIPS[clip]
    count = int(round(spec["seconds"] * FPS))
    return spec["start"], spec["start"] + count, count


def key_clips(rig):
    arm = rig.arm
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    stats = {}
    for clip, fn in POSES.items():
        first, last, count = frames(clip)
        prev = {}
        worst_reach = 0.0
        for k in range(count + 1):
            local, reach = fn(rig, k / count)
            worst_reach = max(worst_reach, *reach.values())
            for pb in arm.pose.bones:
                loc, q = local.get(pb.name, (Vector(), Quaternion()))
                if pb.name in prev:
                    q.make_compatible(prev[pb.name])
                prev[pb.name] = q.copy()
                pb.location = loc
                pb.rotation_quaternion = q
                pb.scale = (1.0, 1.0, 1.0)
                pb.keyframe_insert("location", frame=first + k)
                pb.keyframe_insert("rotation_quaternion", frame=first + k)
                pb.keyframe_insert("scale", frame=first + k)
        stats[clip] = {"frames": [first, last], "keys": count + 1, "seconds": round(count / FPS, 3),
                       "arm_reach_max": round(worst_reach, 3)}
    return stats


# ---------------------------------------------------------------- checks

def mesh_points(obj, dg):
    ev = obj.evaluated_get(dg)
    me = ev.to_mesh()
    pts = [ev.matrix_world @ v.co for v in me.vertices]
    ev.to_mesh_clear()
    return pts


def check_clips(rig):
    """Per frame: loop, eyes / moustache / hat above the water line, hand points inside the body"""
    scene = bpy.context.scene
    objs = bpy.data.objects
    eyes = [objs[n] for n in ("Bean_Eye_l", "Bean_Eye_r")]
    moustache = [objs[n] for n in ("Bean_Moustache_l", "Bean_Moustache_r") if n in objs]
    hat = [ob for ob in bpy.data.collections["SM_Bean_Hat_Tricorne"].all_objects if ob.type == "MESH"]
    hands = [objs[n] for n in ("Bean_Hand_l", "Bean_Hand_r", "Bean_Thumb_l", "Bean_Thumb_r") if n in objs]
    body = objs["Bean_Body"]
    result = {}
    for clip in POSES:
        first, last, _ = frames(clip)
        eye_min = mou_min = hat_min = 9.0
        inside_max = 0
        camera_in_head = 0
        camera_gap_min = 9.0
        worst = {}
        for f in range(first, last + 1):
            scene.frame_set(f)
            dg = bpy.context.evaluated_depsgraph_get()
            e = min(p.z for ob in eyes for p in mesh_points(ob, dg))
            m = min((p.z for ob in moustache for p in mesh_points(ob, dg)), default=9.0)
            h = min(p.z for ob in hat for p in mesh_points(ob, dg))
            ev = body.evaluated_get(dg)
            me = ev.to_mesh()
            tree = BVHTree.FromPolygons([ev.matrix_world @ v.co for v in me.vertices], [tuple(p.vertices) for p in me.polygons])
            ev.to_mesh_clear()
            inside = 0
            for ob in hands:
                for p in mesh_points(ob, dg)[::3]:
                    loc, normal, _i, dist = tree.find_nearest(p)
                    if loc is not None and (p - loc).dot(normal) < 0 and dist > 0.01:
                        inside += 1
            # first person: the camera rides the bean root, not the head; it should stay in front of the face
            cam = Vector(FP_CAMERA)
            loc, normal, _i, dist = tree.find_nearest(cam)
            if loc is not None:
                if (cam - loc).dot(normal) < 0:
                    camera_in_head += 1
                else:
                    camera_gap_min = min(camera_gap_min, dist)
            if e < eye_min:
                worst["eyes"] = f
            if h < hat_min:
                worst["hat"] = f
            if inside > inside_max:
                worst["hands_inside"] = f
            eye_min, mou_min, hat_min, inside_max = min(eye_min, e), min(mou_min, m), min(hat_min, h), max(inside_max, inside)
        # loop: the first and last frames carry the same pose (keys read back from the evaluated armature)
        scene.frame_set(first)
        start = {pb.name: (pb.matrix_basis.to_quaternion(), pb.location.copy()) for pb in rig.arm.pose.bones}
        scene.frame_set(last)
        loop_deg = loop_m = 0.0
        for pb in rig.arm.pose.bones:
            qa, la = start[pb.name]
            loop_deg = max(loop_deg, math.degrees(qa.rotation_difference(pb.matrix_basis.to_quaternion()).angle))
            loop_m = max(loop_m, (la - pb.location).length)
        result[clip] = {
            "water_line_m": WATER_LINE,
            "eyes_above_water_min_m": round(eye_min - WATER_LINE, 3),
            "moustache_above_water_min_m": round(mou_min - WATER_LINE, 3) if moustache else None,
            "hat_above_water_min_m": round(hat_min - WATER_LINE, 3),
            "hand_points_inside_body_max": inside_max,
            "fp_camera_frames_inside_head": camera_in_head,
            "fp_camera_gap_to_face_min_m": round(camera_gap_min, 3) if camera_gap_min < 9.0 else None,
            "worst_frames": worst,
            "loop_mismatch_deg": round(loop_deg, 3),
            "loop_mismatch_m": round(loop_m, 5),
            "passed": eye_min - WATER_LINE >= MIN_EYE_CLEARANCE and (not moustache or mou_min - WATER_LINE >= MIN_EYE_CLEARANCE * 0.5)
                      and hat_min - WATER_LINE >= MIN_HAT_CLEARANCE and loop_deg < 0.5 and loop_m < 0.001
                      and camera_in_head == 0,
        }
    return result


# ---------------------------------------------------------------- export

def export_clip(arm, clip):
    """The library's export_range: armature only, renamed Armature, deform bones, baked keys over the clip frames"""
    scene = bpy.context.scene
    first, last, _ = frames(clip)
    keep = (scene.frame_start, scene.frame_end)
    scene.frame_start, scene.frame_end = first, last
    scene.frame_set(first)  # the nodes' static transforms: the clip's first pose (all channels are keyed anyway)
    view_layer = bpy.context.view_layer
    for ob in view_layer.objects:
        ob.select_set(False)
    arm.select_set(True)
    view_layer.objects.active = arm
    name = arm.name
    clash = bpy.data.objects.get("Armature")
    if clash is not None and clash is not arm:
        clash.name = "Armature_tmp"
    arm.name = "Armature"  # Unreal drops an armature node named "Armature"; any other name becomes an extra root bone
    path = OUT / f"{clip}.fbx"
    OUT.mkdir(parents=True, exist_ok=True)
    try:
        bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={"ARMATURE"},
                                 apply_unit_scale=True, apply_scale_options="FBX_SCALE_NONE", global_scale=1.0,
                                 axis_forward="-Y", axis_up="Z", mesh_smooth_type="FACE", use_mesh_modifiers=True,
                                 add_leaf_bones=False, primary_bone_axis="Y", secondary_bone_axis="X",
                                 use_armature_deform_only=True, bake_anim=True, bake_anim_use_all_actions=False,
                                 bake_anim_use_nla_strips=False, bake_anim_force_startend_keying=True,
                                 bake_anim_simplify_factor=0.0)
    finally:
        arm.name = name
        if clash is not None and clash is not arm:
            clash.name = "Armature"
        scene.frame_start, scene.frame_end = keep
    print(f"[bean_swim] export {path} ({path.stat().st_size} bytes)")
    return path


# ---------------------------------------------------------------- previews

def preview_scene():
    scene = bpy.context.scene
    for coll in ("SM_Bean_FPArm", "Reference"):
        if coll in bpy.data.collections:
            bpy.data.collections[coll].hide_render = True
    for eng in ("BLENDER_WORKBENCH", "BLENDER_EEVEE_NEXT", "BLENDER_EEVEE"):
        try:
            scene.render.engine = eng
            break
        except TypeError:
            continue
    if scene.render.engine == "BLENDER_WORKBENCH":
        shading = scene.display.shading
        shading.light = "STUDIO"
        shading.color_type = "MATERIAL"
        shading.show_shadows = True
        shading.show_cavity = True
    scene.render.film_transparent = False
    world = bpy.data.worlds.new("W_SwimPreview")
    world.color = (0.80, 0.86, 0.90)
    scene.world = world
    scene.render.resolution_x, scene.render.resolution_y = 560, 720
    scene.render.image_settings.file_format = "PNG"
    # water surface at the clips' water line, see-through (wider than the frame)
    mat = bpy.data.materials.new("M_SwimPreview_Water")
    mat.diffuse_color = (0.04, 0.32, 0.50, 0.50)
    for attr, value in (("surface_render_method", "BLENDED"), ("blend_method", "BLEND")):
        try:
            setattr(mat, attr, value)
        except (AttributeError, TypeError):
            pass
    bpy.ops.mesh.primitive_plane_add(size=40.0, location=(0.0, 0.0, WATER_LINE))
    water = bpy.context.active_object
    water.name = "REF_Water"
    water.data.materials.append(mat)
    cam = bpy.data.objects.new("CAM_Swim", bpy.data.cameras.new("CAM_Swim"))
    cam.data.type = "ORTHO"
    cam.data.ortho_scale = 1.45
    scene.collection.objects.link(cam)
    scene.camera = cam
    return cam


def place_camera(cam, yaw_deg, elevation_deg, target=(0.0, 0.0, 0.55), distance=12.0):
    yaw, elev = math.radians(yaw_deg), math.radians(elevation_deg)
    direction = Vector((math.sin(yaw) * math.cos(elev), -math.cos(yaw) * math.cos(elev), math.sin(elev)))
    cam.location = Vector(target) + direction * distance
    cam.rotation_euler = (-direction).to_track_quat("-Z", "Y").to_euler()


def render_frame(path, frame):
    scene = bpy.context.scene
    scene.frame_set(frame)
    scene.render.filepath = str(path)
    bpy.ops.render.render(write_still=True)
    return path


def strip(paths, out_path):
    """Images side by side (same size) into one PNG"""
    import numpy as np
    images = [bpy.data.images.load(str(p)) for p in paths]
    w, h = images[0].size
    tiles = []
    for img in images:
        buf = np.empty(w * h * 4, dtype=np.float32)
        img.pixels.foreach_get(buf)
        tiles.append(buf.reshape(h, w, 4))
    joined = np.concatenate(tiles, axis=1)
    total = bpy.data.images.new("strip", width=w * len(images), height=h, alpha=False)
    total.pixels.foreach_set(joined.ravel())
    total.filepath_raw = str(out_path)
    total.file_format = "PNG"
    total.save()
    for img in images:
        bpy.data.images.remove(img)
    for p in paths:
        Path(p).unlink(missing_ok=True)
    return out_path


def render_previews():
    PREV.mkdir(parents=True, exist_ok=True)
    cam = preview_scene()
    first, _last, count = frames("A_Bean_Swim")
    parts = []
    for n, phase in enumerate((0.0, 0.16, 0.30, 0.45)):  # stretched, outsweep, insweep, recovery
        place_camera(cam, 74.0, 10.0)
        parts.append(render_frame(PREV / f"_bean_swim_part{n}.png", first + int(round(phase * count))))
    brasse = strip(parts, PREV / "bean_swim_brasse.png")
    first, _last, count = frames("A_Bean_TreadWater")
    parts = []
    for n, phase in enumerate((0.0, 0.25)):
        place_camera(cam, 35.0, 14.0, target=(0.0, 0.0, 0.6))
        parts.append(render_frame(PREV / f"_bean_tread_part{n}.png", first + int(round(phase * count))))
    surplace = strip(parts, PREV / "bean_swim_surplace.png")
    print(f"[bean_swim] previews {brasse}, {surplace}")
    return [str(brasse.relative_to(ROOT)), str(surplace.relative_to(ROOT))]


# ---------------------------------------------------------------- main

def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--all", action="store_true")
    a = ap.parse_args(argv)
    if a.all:
        a.check = a.export = a.render = True

    arm = bpy.data.objects.get(ARMATURE)
    assert arm is not None and arm.type == "ARMATURE", f"{ARMATURE} missing: open art/blender/characters/bean_soldier.blend"
    scene = bpy.context.scene
    scene.render.fps, scene.render.fps_base = FPS, 1.0
    if arm.animation_data and arm.animation_data.action:
        arm.animation_data.action = None
    rig = Rig(arm)
    report = {"format": "bean-swim-clips/1", "task": "T-017", "fps": FPS, "water_line_cm": round(WATER_LINE * 100, 1),
              "skeleton": f"{ARMATURE} (art/blender/characters/bean_soldier.blend) -> SK_Bean_Body_Skeleton",
              "root_motion": False, "clips": key_clips(rig)}
    if a.check:
        report["check"] = check_clips(rig)
        for clip, res in report["check"].items():
            print(f"[bean_swim] check {clip}: {json.dumps(res)}")
    if a.export:
        report["files"] = {clip: str(export_clip(arm, clip).relative_to(ROOT)).replace("\\", "/") for clip in POSES}
    if a.render:
        report["previews"] = [p.replace("\\", "/") for p in render_previews()]
    if a.export:
        REPORT.write_text(json.dumps(report, indent=1, ensure_ascii=False), encoding="utf-8")
        print(f"[bean_swim] report {REPORT}")
    if a.check and not all(res["passed"] for res in report["check"].values()):
        print("[bean_swim] CHECK FAILED")
        sys.exit(1)


if __name__ == "__main__":
    main()
