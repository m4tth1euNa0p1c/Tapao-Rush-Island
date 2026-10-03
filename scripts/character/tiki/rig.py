"""Skeleton, skinning and pose solver of the tiki creatures.

Bone names follow the Unreal mannequin and SKEL_Bean (root, pelvis, spine_01, spine_02, head, upperarm_l...),
plus `crest` (top ornament, secondary motion), `weapon_r` (held object, child of hand_r) and, for the bramble,
`prop_l` (dart in the left hand, scaled to 0 when hidden). Every creature has its own armature SKEL_<Name>;
the hierarchies match, so animations can be retargeted.

Skinning: body parts follow the spine by height (1D blend, clean bending of a wide egg body), limbs use
distance-to-bone weights restricted to their own chain, the crest and held objects are rigid.

Pose solver (used by anims.py): rotations are given in armature axes (X = the character's left, Y = back,
Z = up), applied in the parent's posed frame: x > 0 leans the top of a bone forward (and swings a hanging
limb backward), y > 0 leans it to the character's left, z > 0 turns it to the left. hold() places the
right hand so that the weapon grip reaches a point with a given direction (two-bone IK + hand orientation).
"""
import math

import bpy
from mathutils import Euler, Matrix, Quaternion, Vector

from .kit import bake_objects, fixed_texspace, join, smoothstep

SPINE = ["pelvis", "spine_01", "spine_02", "head"]


def bone_table(cr):
    H = cr.H
    s1, s2, s3 = [z * H for z in cr.s["J"]["spine"]]
    c0, c1 = [z * H for z in cr.s["J"]["crest"]]
    zh = cr.J("hip").z
    top = cr.body.zt
    B = [("root", (0, 0, 0), (0, 0, 0.12 * H), None, False),
         ("pelvis", (0, 0, zh), (0, 0, s1), "root", True),
         ("spine_01", (0, 0, s1), (0, 0, s2), "pelvis", True),
         ("spine_02", (0, 0, s2), (0, 0, s3), "spine_01", True),
         ("head", (0, 0, s3), (0, 0, top), "spine_02", True),
         ("crest", (0, 0, c0), (0, 0, c1), "head", True)]
    for s, side in ((1, "l"), (-1, "r")):
        B += [(f"upperarm_{side}", cr.J("shoulder", s), cr.J("elbow", s), "spine_02", True),
              (f"lowerarm_{side}", cr.J("elbow", s), cr.J("wrist", s), f"upperarm_{side}", True),
              (f"hand_{side}", cr.J("wrist", s), cr.J("tip", s), f"lowerarm_{side}", True),
              (f"thigh_{side}", cr.J("hip", s), cr.J("knee", s), "pelvis", True),
              (f"calf_{side}", cr.J("knee", s), cr.J("ankle", s), f"thigh_{side}", True),
              (f"foot_{side}", cr.J("ankle", s), cr.J("toe", s), f"calf_{side}", True)]
    wdir = cr.w_rest_rot.to_3x3() @ Vector((0, 0, 1))
    wdir = (wdir + Vector((0.0, 0.0, 0.002))).normalized()   # never exactly -Y (bone roll singularity)
    B.append(("weapon_r", cr.grip_r, cr.grip_r + wdir * 0.15 * H, "hand_r", True))
    if cr.prop_parts:
        B.append(("prop_l", cr.grip_l, cr.grip_l + wdir * 0.10 * H, "hand_l", True))
    return B


def build_armature(cr, coll):
    name = f"SKEL_{cr.name}"
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    coll.objects.link(arm)
    arm.show_in_front = True
    data.display_type = "STICK"
    bpy.context.view_layer.objects.active = arm
    bpy.ops.object.mode_set(mode="EDIT")
    for bname, head, tail, parent, deform in bone_table(cr):
        eb = data.edit_bones.new(bname)
        eb.head, eb.tail = Vector(head), Vector(tail)
        eb.roll = 0.0
        if parent:
            eb.parent = data.edit_bones[parent]
            eb.use_connect = False
        eb.use_deform = deform
    bpy.ops.object.mode_set(mode="OBJECT")
    for pb in arm.pose.bones:
        pb.rotation_mode = "QUATERNION"
    return arm


# ---------------------------------------------------------------- skinning

def seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
    return (p - (a + ab * t)).length


def spine_weights(z, bounds, blend):
    w = {}
    for i, (b, z0, z1) in enumerate(bounds):
        lo = 1.0 if i == 0 else smoothstep(z0 - blend, z0 + blend, z)
        hi = 1.0 if i == len(bounds) - 1 else 1.0 - smoothstep(z1 - blend, z1 + blend, z)
        w[b] = lo * hi
    tot = sum(w.values()) or 1.0
    return {b: v / tot for b, v in w.items() if v / tot > 1e-3}


def limb_bones(tag):
    kind, side = tag.split("_")
    return {"arm": ["spine_02", f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"],
            "hand": [f"lowerarm_{side}", f"hand_{side}"],
            "leg": ["pelvis", f"thigh_{side}", f"calf_{side}", f"foot_{side}"],
            "foot": [f"calf_{side}", f"foot_{side}"]}[kind]


def skin(cr, arm, coll):
    """Weights per part, then one joined mesh per group: SK_<Name> (body), SK_<Name>_<Weapon> (weapon + prop)."""
    H = cr.H
    place = {}
    for ob in cr.weapon_parts:
        place[ob] = Matrix.Translation(cr.grip_r) @ cr.w_rest_rot
    for ob in cr.prop_parts:
        place[ob] = Matrix.Translation(cr.grip_l) @ cr.w_rest_rot
    for ob, M in place.items():
        ob.matrix_world = M @ ob.matrix_world
    allp = cr.parts + cr.weapon_parts + cr.prop_parts
    tags = {ob.name: ob.get("rig", "body") for ob in allp}
    bake_objects(allp)
    bones = {b.name: (b.head_local.copy(), b.tail_local.copy()) for b in arm.data.bones}
    zs = [z * H for z in cr.s["J"]["spine"]]
    bounds = [("pelvis", -1e9, zs[0]), ("spine_01", zs[0], zs[1]), ("spine_02", zs[1], zs[2]), ("head", zs[2], 1e9)]
    blend = 0.06 * H
    for ob in allp:
        tag = tags[ob.name]
        me = ob.data
        if tag in ("body", "head", "skirt", "belt"):
            groups = {b: ob.vertex_groups.new(name=b) for b in SPINE}
            for v in me.vertices:
                for b, w in spine_weights(v.co.z, bounds, blend).items():
                    groups[b].add([v.index], w, "REPLACE")
        elif tag in ("crest", "weapon", "prop"):
            b = {"crest": "crest", "weapon": "weapon_r", "prop": "prop_l"}[tag]
            ob.vertex_groups.new(name=b).add([v.index for v in me.vertices], 1.0, "REPLACE")
        else:
            names = limb_bones(tag)
            groups = {n: ob.vertex_groups.new(name=n) for n in names}
            for v in me.vertices:
                p = v.co
                w = sorted(((1.0 / (seg_dist(p, *bones[n]) + 0.012 * H) ** 4, n) for n in names), reverse=True)[:3]
                tot = sum(x for x, _ in w)
                for x, n in w:
                    groups[n].add([v.index], x / tot, "REPLACE")
    out = {}
    body = join(cr.parts, f"SK_{cr.name}")
    out["body"] = body
    held = cr.weapon_parts + cr.prop_parts
    if held:
        out["weapon"] = join(held, f"SK_{cr.name}_{cr.s['weapon']}")
    for ob in out.values():
        fixed_texspace(ob)
        for c in list(ob.users_collection):
            c.objects.unlink(ob)
        coll.objects.link(ob)
        mod = ob.modifiers.new("Armature", "ARMATURE")
        mod.object = arm
        ob.parent = arm
        ob.matrix_parent_inverse = arm.matrix_world.inverted()
    return out


# ---------------------------------------------------------------- pose solver

class Pose:
    """Rotations (armature axes, applied in the parent's frame), translations, scales."""

    def __init__(self):
        self.rot, self.loc, self.scale = {}, {}, {}

    def copy(self):
        p = Pose()
        p.rot = {k: v.copy() for k, v in self.rot.items()}
        p.loc = {k: v.copy() for k, v in self.loc.items()}
        p.scale = dict(self.scale)
        return p

    def r(self, bone, x=0.0, y=0.0, z=0.0):
        q = Euler((math.radians(x), math.radians(y), math.radians(z)), "XYZ").to_quaternion()
        self.rot[bone] = q @ self.rot.get(bone, Quaternion())
        return self

    def t(self, bone, x=0.0, y=0.0, z=0.0):
        self.loc[bone] = self.loc.get(bone, Vector()) + Vector((x, y, z))
        return self


def frame_rotation(a0, b0, a1, b1=None):
    """Rotation taking direction a0 to a1 and, as closely as possible, b0 towards b1."""
    a0, a1 = a0.normalized(), a1.normalized()
    if b1 is None:
        return a0.rotation_difference(a1)
    b0p = b0 - a0 * b0.dot(a0)
    b1p = b1 - a1 * b1.dot(a1)
    if b0p.length < 1e-6 or b1p.length < 1e-6:
        return a0.rotation_difference(a1)
    b0p.normalize()
    b1p.normalize()
    M0 = Matrix((a0, b0p, a0.cross(b0p))).transposed()
    M1 = Matrix((a1, b1p, a1.cross(b1p))).transposed()
    return (M1 @ M0.transposed()).to_quaternion()


def two_bone(root, target, L1, L2, pole):
    d = target - root
    dist = min(max(d.length, abs(L1 - L2) + 1e-4), (L1 + L2) * 0.999)
    u = d.normalized() if d.length > 1e-9 else Vector((0, -1, 0))
    cos_a = (L1 * L1 + dist * dist - L2 * L2) / (2 * L1 * dist)
    a = math.acos(max(-1.0, min(1.0, cos_a)))
    pp = pole - u * pole.dot(u)
    if pp.length < 1e-6:
        pp = u.orthogonal()
    pp.normalize()
    d1 = u * math.cos(a) + pp * math.sin(a)
    elbow = root + d1 * L1
    wrist = root + u * dist
    return d1, (wrist - elbow).normalized()


class RigModel:
    def __init__(self, arm):
        self.arm = arm
        self.b = {}
        depth = {}
        for bone in arm.data.bones:
            d = 0
            p = bone.parent
            while p:
                d += 1
                p = p.parent
            depth[bone.name] = d
            self.b[bone.name] = dict(parent=bone.parent.name if bone.parent else None, head=bone.head_local.copy(),
                                     tail=bone.tail_local.copy(), B=bone.matrix_local.to_3x3().normalized())
        self.order = sorted(self.b, key=lambda n: depth[n])
        self.anim_bones = [n for n in self.order]

    def dir(self, n):
        d = self.b[n]
        return (d["tail"] - d["head"]).normalized()

    def length(self, n):
        d = self.b[n]
        return (d["tail"] - d["head"]).length

    def head(self, n):
        return self.b[n]["head"]

    def fk(self, pose):
        A, P = {}, {}
        for n in self.order:
            d = self.b[n]
            p = d["parent"]
            q = pose.rot.get(n, Quaternion())
            t = pose.loc.get(n, Vector())
            if p is None:
                P[n] = d["head"] + t
                A[n] = q.copy()
            else:
                P[n] = P[p] + A[p] @ (d["head"] - self.b[p]["head"] + t)
                A[n] = A[p] @ q
        return A, P

    def point(self, pose, bone, rest_point, A=None, P=None):
        """Where a rest-pose point rigidly attached to `bone` ends up in the pose."""
        if A is None:
            A, P = self.fk(pose)
        return P[bone] + A[bone] @ (Vector(rest_point) - self.b[bone]["head"])

    def reach(self, pose, side, target, pole=None, hand_rot=None):
        """Two-bone IK of an arm so that its wrist reaches `target` (armature space)."""
        up, low, hand = f"upperarm_{side}", f"lowerarm_{side}", f"hand_{side}"
        A, P = self.fk(pose)
        par = self.b[up]["parent"]
        s = 1 if side == "l" else -1
        pole = pole or Vector((s * 0.7, 0.6, -0.4))
        d1, d2 = two_bone(P[up], target, self.length(up), self.length(low), pole)
        q_up = self.dir(up).rotation_difference(A[par].inverted() @ d1)
        a_up = A[par] @ q_up
        q_low = self.dir(low).rotation_difference(a_up.inverted() @ d2)
        pose.rot[up], pose.rot[low] = q_up, q_low
        if hand_rot is not None:
            pose.rot[hand] = (a_up @ q_low).inverted() @ hand_rot
        else:
            pose.rot.pop(hand, None)
        return pose

    def hold(self, pose, grip, direction, up=None, side="r", pole=None, prop="weapon_r"):
        """Right hand (or left, with prop='prop_l') so that the held object's grip sits at `grip`, pointing along
        `direction` (its secondary axis towards `up`)."""
        hand = f"hand_{side}"
        w_rest = self.dir(prop)
        s_rest = Vector((1.0, 0.0, 0.0))
        a_hand = frame_rotation(w_rest, s_rest, Vector(direction), Vector(up) if up is not None else None)
        g_off = self.b[prop]["head"] - self.b[hand]["head"]
        wrist = Vector(grip) - a_hand @ g_off
        return self.reach(pose, side, wrist, pole, hand_rot=a_hand)

    def local(self, pose):
        """Pose -> per bone (local quaternion, local location, scale) for the pose bones."""
        out = {}
        for n in self.order:
            B = self.b[n]["B"]
            Bi = B.inverted()
            q = pose.rot.get(n)
            t = pose.loc.get(n)
            ql = (Bi @ q.to_matrix() @ B).to_quaternion() if q is not None else Quaternion()
            tl = Bi @ t if t is not None else Vector()
            out[n] = (ql, tl, pose.scale.get(n, 1.0))
        return out
