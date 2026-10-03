"""Provisional animations of the tiki creatures (cahier §10: provisional animations accepted; §6.3: melee attack
with a visible wind-up, death).

Every action is in place (no root motion, the AI's CharacterMovement moves the actor), 30 fps, frame 0 = the
creature's base stance, so Unreal can blend between them. Cycles (idle, walk, run, aim, cower, talk) are sampled
on every frame and loop (last frame = first frame); one-shots (attack, shoot, reload, hit, death) are key poses
with Bezier easing.

Poses are written as small specs (see Animator.build): leg crouch with planted feet, spine / chest / head /
crest rotations (forward, left, turn; degrees, armature axes), left arm swing / out / flex, and the held object
placed by IK: `grip` (point in the rest body frame, carried by spine_02) or `grip_abs` (armature space), with
its direction `wdir` and secondary axis `wup`.
"""
import math

import bpy
from mathutils import Quaternion, Vector

from .rig import Pose, RigModel

TAU = 2 * math.pi

CFG = {
    "coco": dict(
        base=dict(crouch=8, spine=(3, 0, 0), larm=dict(swing=6, out=10, flex=18),
                  grip=(-0.33, -0.15, 0.43), wdir=(0, -0.15, 1.0), wup=(1, 0, 0)),
        idle=dict(n=60, breath=2.0, crest=4.0, look=6.0),
        walk=dict(n=30, stride=24, knee=40, bob=0.018, roll=7, twist=6, arm=18, lean=4, crest=7, grip_swing=0.015),
        run=dict(n=20, stride=38, knee=70, bob=0.03, roll=6, twist=8, arm=35, lean=14, crest=10, grip_swing=0.03),
        role="spear"),
    "crab": dict(
        base=dict(crouch=10, spine=(2, 0, 0), larm=dict(swing=4, out=18, flex=25),
                  grip=(-0.36, -0.14, 0.42), wdir=(0, -0.1, 1.0), wup=(1, 0, 0)),
        idle=dict(n=60, breath=1.5, crest=3.0, sway=4.0, snap=12.0),
        walk=dict(n=30, stride=20, knee=35, bob=0.015, roll=9, twist=4, arm=15, lean=2, crest=5, grip_swing=0.012),
        run=dict(n=20, stride=32, knee=60, bob=0.025, roll=8, twist=6, arm=30, lean=10, crest=8, grip_swing=0.025),
        role="hammer"),
    "ash": dict(
        base=dict(crouch=10, spine=(6, 0, 0), chest=(2, 0, 0), larm=dict(swing=8, out=14, flex=22),
                  grip=(-0.36, -0.10, 0.38), wdir=(-0.25, -0.45, 0.85), wup=(1, 0, 0)),
        idle=dict(n=72, breath=3.5, crest=2.0, look=4.0),
        walk=dict(n=36, stride=18, knee=30, bob=0.022, roll=9, twist=4, arm=12, lean=6, crest=4, grip_swing=0.012),
        run=dict(n=24, stride=28, knee=50, bob=0.03, roll=8, twist=6, arm=25, lean=14, crest=6, grip_swing=0.025),
        role="club"),
    "villager": dict(
        base=dict(crouch=4, spine=(2, 0, 0), chest=(2, 0, 0), larm=dict(swing=10, out=6, flex=30),
                  grip=(-0.40, -0.06, 0.36), wdir=(0, 0, 1.0), wup=(1, 0, 0)),
        idle=dict(n=60, breath=1.5, crest=6.0, look=22.0, tremble=1.2),
        walk=dict(n=30, stride=22, knee=38, bob=0.016, roll=8, twist=5, arm=14, lean=3, crest=9, grip_swing=0.04,
                  look=12.0),
        run=dict(n=18, stride=36, knee=70, bob=0.03, roll=6, twist=8, arm=0, lean=16, crest=14, grip_swing=0.05),
        role="villager"),
    "bramble": dict(
        base=dict(crouch=16, spine=(10, 0, 0), chest=(2, 0, 0), head=(-6, 0, 0), larm=dict(swing=20, out=15, flex=40),
                  grip=(-0.30, -0.22, 0.40), wdir=(0.15, -1.0, -0.35), wup=(0, 0, 1)),
        idle=dict(n=60, breath=1.5, crest=5.0, look=20.0),
        walk=dict(n=36, stride=22, knee=45, bob=0.012, roll=4, twist=6, arm=12, lean=14, crest=6, crouch=20,
                  grip_swing=0.01, look=10.0),
        run=dict(n=20, stride=36, knee=70, bob=0.025, roll=4, twist=8, arm=30, lean=20, crest=10, crouch=10,
                 grip_swing=0.02),
        role="sniper"),
}


def tup(v, n=3):
    v = list(v) + [0.0] * n
    return v[:n]


def merged(base, **over):
    out = dict(base)
    out.update(over)
    return out


class Animator:
    def __init__(self, cr, arm):
        self.cr = cr
        self.arm = arm
        self.rig = RigModel(arm)
        self.H = cr.H
        self.cfg = CFG[cr.key]
        self.actions = {}

    def V(self, v):
        return Vector(v) * self.H

    # ------------------------------------------------------------ pose building
    def crouch(self, p, a):
        if not a:
            return p
        L1, L2 = self.rig.length("thigh_l"), self.rig.length("calf_l")
        ar = math.radians(a)
        b = math.asin(max(-1.0, min(1.0, L1 / L2 * math.sin(ar))))
        bd = math.degrees(b)
        for side in "lr":
            p.r(f"thigh_{side}", x=-a)
            p.r(f"calf_{side}", x=a + bd)
            p.r(f"foot_{side}", x=-bd)
        p.t("pelvis", z=-((L1 + L2) - (L1 * math.cos(ar) + L2 * math.cos(b))))
        return p

    def build(self, sp):
        p = Pose()
        self.crouch(p, sp.get("crouch", 0))
        for side, extra in (sp.get("legs") or {}).items():
            x = tup(extra)
            p.r(side, x=x[0], y=x[1], z=x[2])
        if "pelvis" in sp:
            f, l, t = tup(sp["pelvis"])
            p.r("pelvis", x=f, y=l, z=t)
        if "loc" in sp:
            p.t("pelvis", *[v * self.H for v in tup(sp["loc"])])
        for bone, key in (("spine_01", "spine"), ("spine_02", "chest"), ("head", "head"), ("crest", "crest")):
            if key in sp:
                f, l, t = tup(sp[key])
                p.r(bone, x=f, y=l, z=t)
        la = sp.get("larm")
        if la:
            p.r("upperarm_l", x=-la.get("swing", 0), y=-la.get("out", 0), z=la.get("twist", 0))
            p.r("lowerarm_l", x=-la.get("flex", 0))
        ra = sp.get("rarm")
        if ra:
            p.r("upperarm_r", x=-ra.get("swing", 0), y=ra.get("out", 0))
            p.r("lowerarm_r", x=-ra.get("flex", 0))
        g = None
        if sp.get("grip_abs") is not None or sp.get("grip") is not None:
            A, P = self.rig.fk(p)
            if sp.get("grip_abs") is not None:
                g = self.V(sp["grip_abs"])
            else:
                g = self.rig.point(p, "spine_02", self.V(sp["grip"]), A, P)
            d = Vector(sp["wdir"])
            if sp.get("dir_body"):
                d = A["spine_02"] @ d
            self.rig.hold(p, g, d, Vector(sp["wup"]) if sp.get("wup") else None)
        if "lreach" in sp:
            self.rig.reach(p, "l", sp["lreach"])
        if "rpost" in sp:  # offsets added on top of the solved right arm (hit, death)
            ro = sp["rpost"]
            p.r("upperarm_r", x=-ro.get("swing", 0), y=ro.get("out", 0))
            p.r("lowerarm_r", x=-ro.get("flex", 0))
        if self.cr.prop_parts:
            p.scale["prop_l"] = sp.get("prop", 0.0)
        return p

    def base(self, **over):
        return merged(self.cfg["base"], **over)

    # ------------------------------------------------------------ recording
    def record(self, name, keyed, cyclic=False):
        arm = self.arm
        ad = arm.animation_data_create()
        act = bpy.data.actions.new(f"A_{self.cr.name}_{name}")
        act.use_fake_user = True
        ad.action = act
        prev = {}
        scale_bones = set()
        for _, pose in keyed:
            scale_bones |= set(pose.scale)
        for f, pose in keyed:
            loc = self.rig.local(pose)
            for n in self.rig.order:
                ql, tl, sc = loc[n]
                if n in prev and prev[n].dot(ql) < 0:
                    ql = -ql
                prev[n] = ql
                pb = arm.pose.bones[n]
                pb.rotation_quaternion = ql
                pb.keyframe_insert("rotation_quaternion", frame=f, group=n)
                if n in ("root", "pelvis"):
                    pb.location = tl
                    pb.keyframe_insert("location", frame=f, group=n)
                if n in scale_bones:
                    pb.scale = (sc, sc, sc)
                    pb.keyframe_insert("scale", frame=f, group=n)
        act.use_frame_range = True
        try:
            act.frame_start, act.frame_end = keyed[0][0], keyed[-1][0]
        except AttributeError:
            act.frame_range = (keyed[0][0], keyed[-1][0])
        act.use_cyclic = cyclic
        act["tiki_cyclic"] = cyclic
        for pb in arm.pose.bones:
            pb.rotation_quaternion = (1, 0, 0, 0)
            pb.location = (0, 0, 0)
            pb.scale = (1, 1, 1)
        ad.action = None
        self.actions[name] = act
        return act

    def cycle(self, name, n, fn):
        return self.record(name, [(f, self.build(fn(f / n))) for f in range(n + 1)], cyclic=True)

    def keyed(self, name, keys):
        return self.record(name, [(f, self.build(sp)) for f, sp in keys], cyclic=False)

    # ------------------------------------------------------------ generic cycles
    def idle_spec(self, ph):
        c = self.cfg["idle"]
        a = TAU * ph
        b = self.cfg["base"]
        br = c["breath"]
        sp = self.base()
        sp["spine"] = [x + y for x, y in zip(tup(b.get("spine", ())), (br * 0.5 * math.sin(a), 0, 0))]
        sp["chest"] = [x + y for x, y in zip(tup(b.get("chest", ())), (br * math.sin(a - 0.6), 0,
                                                                       c.get("look", 0) * 0.5 * math.sin(a)))]
        sp["head"] = [x + y for x, y in zip(tup(b.get("head", ())), (-br * 0.5 * math.sin(a - 1.0), 0,
                                                                     c.get("look", 0) * 0.5 * math.sin(a)))]
        if c.get("sway"):
            sp["spine"][1] += c["sway"] * math.sin(a)
            sp["head"][1] -= c["sway"] * 0.5 * math.sin(a)
        if c.get("tremble"):
            sp["chest"][1] += c["tremble"] * math.sin(a * 15)
        sp["crest"] = (c["crest"] * 0.5 * math.sin(a * 2 - 1.2), c["crest"] * math.sin(a - 1.4), 0)
        la = dict(b.get("larm", {}))
        la["swing"] = la.get("swing", 0) + 3 * math.sin(a + 0.4)
        la["out"] = la.get("out", 0) + 2 * math.sin(a)
        sp["larm"] = la
        g = list(b["grip"])
        g[2] += 0.006 * math.sin(a - 0.5)
        sp["grip"] = g
        if c.get("snap"):  # crab: the claws clack
            sp["rpost"] = {}
            sp["larm"]["flex"] = la.get("flex", 0) + c["snap"] * max(0.0, math.sin(a * 3)) ** 4
        return sp

    def loco_spec(self, ph, c):
        a = TAU * ph
        b = self.cfg["base"]
        sp = self.base(crouch=c.get("crouch", b.get("crouch", 0)))
        legs = {}
        for side, off in (("l", 0.0), ("r", math.pi)):
            s, co = math.sin(a + off), math.cos(a + off)
            sw = c["stride"] * s
            fl = c["knee"] * max(0.0, co) ** 1.3
            legs[f"thigh_{side}"] = (-sw, 0, 0)
            legs[f"calf_{side}"] = (fl, 0, 0)
            legs[f"foot_{side}"] = (0.7 * (sw - fl), 0, 0)
        sp["legs"] = legs
        roll = -c["roll"] * math.cos(a)
        tw = c["twist"] * math.sin(a)
        sp["loc"] = (0, 0, -c["bob"] * abs(math.sin(a)))
        sp["pelvis"] = (0, roll * 0.2, -tw)
        bs = tup(b.get("spine", ()))
        sp["spine"] = (bs[0] + c["lean"], bs[1] + roll * 0.55, bs[2] + tw * 0.4)
        bc = tup(b.get("chest", ()))
        look = c.get("look", 0) * math.sin(a * 0.5)
        sp["chest"] = (bc[0] + c["lean"] * 0.3, bc[1] + roll * 0.35, bc[2] + tw * 0.5 + look * 0.5)
        bh = tup(b.get("head", ()))
        sp["head"] = (bh[0] - c["lean"] * 0.4, bh[1] - roll * 0.4, bh[2] + look * 0.5)
        sp["crest"] = (c["crest"] * 0.5 * math.cos(2 * a - 1.0), c["crest"] * math.sin(2 * a - 1.0), 0)
        la = dict(b.get("larm", {}))
        la["swing"] = la.get("swing", 0) - c["arm"] * math.sin(a)
        la["flex"] = la.get("flex", 0) + 8 * max(0.0, math.sin(a))
        sp["larm"] = la
        g = list(b["grip"])
        g[1] += c.get("grip_swing", 0.0) * math.sin(a + math.pi)
        g[2] += c.get("grip_swing", 0.0) * 0.5 * abs(math.cos(a))
        sp["grip"] = g
        return sp

    # ------------------------------------------------------------ one-shots shared by every creature
    def hit(self):
        b = self.cfg["base"]
        bs, bc, bh = tup(b.get("spine", ())), tup(b.get("chest", ())), tup(b.get("head", ()))
        la = dict(b.get("larm", {}))
        keys = [(0, self.base()),
                (3, self.base(spine=(bs[0] - 10, bs[1] + 4, bs[2] + 10), chest=(bc[0] - 8, bc[1], bc[2] + 6),
                              head=(bh[0] - 12, bh[1], bh[2] + 8), crest=(-14, 8, 0),
                              larm=dict(la, out=la.get("out", 0) + 25, swing=la.get("swing", 0) - 10),
                              loc=(0, 0.02, 0))),
                (8, self.base(spine=(bs[0] + 4, bs[1], bs[2] - 3), head=(bh[0] + 3, bh[1], bh[2]), crest=(8, -4, 0))),
                (16, self.base())]
        for _, sp in keys:
            sp["dir_body"] = True
        return self.keyed("Hit", keys)

    def death(self):
        cr = self.cr
        d = cr.s.get("death", -1)
        B = cr.body
        zh = cr.J("hip").z
        lift = B.ry - zh + 0.012 * self.H
        if d > 0:
            lift += cr.mask.off + cr.mask.dome + 0.02 * self.H
        lift /= self.H
        b = self.cfg["base"]
        bs = tup(b.get("spine", ()))
        la = dict(b.get("larm", {}))
        fall = dict(pelvis=(d * 88, 0, 0), loc=(0, -d * 0.12, lift),
                    legs={"thigh_l": (-30, 0, 0), "thigh_r": (-40, 0, 0), "calf_l": (55, 0, 0), "calf_r": (70, 0, 0)},
                    spine=(d * 4, 6, 0), chest=(0, 4, 0), head=(-d * 6, 14, 22), crest=(d * 22, 10, 0),
                    larm=dict(swing=-d * 25, out=75, flex=20), rpost=dict(out=55, swing=0))
        keys = [(0, self.base()),
                (5, self.base(spine=(bs[0] + d * 12, 0, 8), chest=(d * 10, 0, 4), head=(d * 12, 0, 6), crest=(d * 15, 0, 0),
                              larm=dict(swing=la.get("swing", 0) - 20, out=50, flex=30), rpost=dict(out=20))),
                (14, self.base(crouch=24, spine=(bs[0] + d * 20, -6, 10), chest=(d * 8, -4, 4), head=(d * 10, -8, 8),
                               larm=dict(swing=-10, out=60, flex=30), rpost=dict(out=35))),
                (26, merged(self.base(crouch=0), **fall)),
                (32, merged(self.base(crouch=0), **merged(fall, pelvis=(d * 78, 0, 0), loc=(0, -d * 0.14, lift + 0.05),
                                                              crest=(-d * 16, -8, 0)))),
                (38, merged(self.base(crouch=0), **merged(fall, pelvis=(d * 90, 0, 0), crest=(d * 8, 4, 0)))),
                (46, merged(self.base(crouch=0), **merged(fall, pelvis=(d * 89, 0, 0), crest=(0, 0, 0)))),
                (60, merged(self.base(crouch=0), **merged(fall, pelvis=(d * 89, 0, 0), crest=(0, 0, 0))))]
        for _, sp in keys:
            sp["dir_body"] = True
        return self.keyed("Death", keys)

    # ------------------------------------------------------------ role actions
    def attack_spear(self):
        ready = self.base()
        wind = self.base(crouch=14, spine=(-8, 0, -18), chest=(-6, 0, -10), head=(6, 0, 14), crest=(-10, 0, 0),
                         grip=None, grip_abs=(-0.38, 0.06, 0.60), wdir=(0.05, -1, 0.12), wup=(0, 0, 1),
                         larm=dict(swing=35, out=20, flex=40))
        wind.pop("grip")
        strike = self.base(crouch=12, spine=(14, 0, 18), chest=(8, 0, 10), head=(-8, 0, -12), crest=(14, 0, 0),
                           grip_abs=(-0.22, -0.50, 0.52), wdir=(0.02, -1, -0.04), wup=(0, 0, 1), loc=(0, -0.04, 0),
                           larm=dict(swing=-30, out=25, flex=20))
        strike.pop("grip")
        hold = merged(strike, spine=(12, 0, 15), crest=(-6, 0, 0))
        return self.keyed("Attack", [(0, ready), (12, wind), (16, strike), (22, hold), (38, ready)])

    def attack_hammer(self):
        ready = self.base()
        wind = self.base(crouch=6, spine=(-12, 0, -10), chest=(-8, 0, -6), head=(8, 0, 6), crest=(-12, 0, 0),
                         grip_abs=(-0.30, 0.10, 0.82), wdir=(0, 0.55, 1.0), wup=(1, 0, 0), loc=(0, 0.02, 0.02),
                         larm=dict(swing=40, out=30, flex=30))
        wind.pop("grip")
        strike = self.base(crouch=16, spine=(18, 0, 6), chest=(10, 0, 4), head=(-10, 0, 0), crest=(16, 0, 0),
                           grip_abs=(-0.24, -0.45, 0.38), wdir=(0, -1.0, -0.75), wup=(1, 0, 0), loc=(0, -0.03, 0),
                           larm=dict(swing=-20, out=30, flex=10))
        strike.pop("grip")
        hold = merged(strike, spine=(15, 0, 5), crest=(-8, 0, 0))
        return self.keyed("Attack", [(0, ready), (14, wind), (19, strike), (27, hold), (42, ready)])

    def attack_club(self):
        ready = self.base()
        wind = self.base(crouch=14, spine=(0, 4, -30), chest=(-4, 0, -20), head=(4, 0, 25), crest=(0, 8, 0),
                         grip_abs=(-0.46, 0.18, 0.62), wdir=(-0.5, 0.6, 0.6), wup=(0, 0, 1),
                         larm=dict(swing=30, out=35, flex=40))
        wind.pop("grip")
        strike = self.base(crouch=14, spine=(8, -4, 28), chest=(6, 0, 18), head=(-4, 0, -22), crest=(0, -12, 0),
                           grip_abs=(-0.05, -0.50, 0.52), wdir=(0.75, -0.65, 0.05), wup=(0, 0, 1),
                           larm=dict(swing=-25, out=40, flex=20))
        strike.pop("grip")
        follow = merged(strike, spine=(8, -4, 40), chest=(6, 0, 25), head=(-4, 0, -30), crest=(0, 6, 0),
                        grip_abs=(0.20, -0.36, 0.46), wdir=(0.95, 0.2, -0.1), larm=dict(swing=-35, out=45, flex=15))
        return self.keyed("Attack", [(0, ready), (16, wind), (22, strike), (28, follow), (46, ready)])

    # villager --------------------------------------------------------
    def villager_run(self, ph):
        c = self.cfg["run"]
        a = TAU * ph
        sp = self.loco_spec(ph, c)
        sp["larm"] = dict(swing=120 + 25 * math.sin(a * 2), out=35 + 15 * math.cos(a * 2), flex=40 + 20 * math.sin(a * 2))
        h = tup(sp["head"])
        sp["chest"] = (tup(sp["chest"])[0], tup(sp["chest"])[1], 20.0)
        sp["head"] = (h[0] - 6, h[1], 18.0)
        return sp

    def cower_spec(self, ph):
        a = TAU * ph
        tr = 1.6 * math.sin(a * 10)
        return self.base(crouch=30, spine=(18, tr, 0), chest=(10, -tr, 0), head=(12, 0, 4 * math.sin(a)),
                         crest=(8 + 4 * math.sin(a * 10), 6 * math.sin(a * 5), 0),
                         larm=dict(swing=85, out=5, flex=110 + 4 * math.sin(a * 10)),
                         grip=(-0.30, -0.22, 0.40), wdir=(0, 0, 1))

    def talk_spec(self, ph):
        a = TAU * ph
        return self.base(spine=(2, 2 * math.sin(a), 0), chest=(2 + 3 * math.sin(a * 4), 0, 6 * math.sin(a)),
                         head=(0, 6 * math.sin(a), 0), crest=(4 * math.sin(a * 4 - 1), 5 * math.sin(a - 1.2), 0),
                         larm=dict(swing=40 + 15 * math.sin(a * 2), out=25 + 10 * math.sin(a * 2 + 1),
                                   flex=60 + 20 * math.sin(a * 2)),
                         grip=(-0.32, -0.24, 0.46), wdir=(0, 0, 1))

    # bramble ---------------------------------------------------------
    def mouth_rest(self):
        return self.cr.mask.pt(0.0, -0.46, 0.0)

    def aim_spec(self, sway=0.0, push=0.0, tilt=0.0, chest=0.0, head=0.0):
        sp = self.base(crouch=12, spine=(4, 0, 0), chest=(chest, 0, 0), head=(head, 0, 0),
                       larm=dict(swing=55, out=10, flex=60))
        p = self.build(merged(sp, grip=None))
        A, P = self.rig.fk(p)
        mouth = self.rig.point(p, "spine_02", self.mouth_rest(), A, P)
        d = Vector((math.sin(math.radians(sway)), -1.0, math.tan(math.radians(tilt)))).normalized()
        sp.pop("grip")
        sp["grip_abs"] = tuple((mouth + d * (0.045 + push)) / self.H)
        sp["wdir"] = tuple(d)
        sp["wup"] = (0, 0, 1)
        return sp

    def quiver_top(self, p):
        B = self.cr.body
        qc = B.surf(1.05, 0.34 * self.H, 0.07 * self.H)
        axis = Vector((0.12, 0.05, 1.0)).normalized()
        return self.rig.point(p, "spine_01", qc + axis * (0.13 * self.H))

    def reload(self):
        aim = self.aim_spec()
        low = self.base(crouch=12, spine=(6, 0, 4), chest=(2, 0, 4), head=(-4, 0, -6),
                        grip_abs=(-0.22, -0.30, 0.48), wdir=(0.1, -0.6, 0.8), wup=(1, 0, 0))
        low.pop("grip")
        p_low = self.build(low)
        q = self.quiver_top(p_low)
        grab = merged(low, lreach=q + Vector((0, 0, 0.05 * self.H)), prop=0.0, head=(8, 0, 20))
        grab2 = merged(grab, prop=1.0)
        A, P = self.rig.fk(p_low)
        g = self.V(low["grip_abs"])
        back = g - Vector(low["wdir"]).normalized() * 0.04
        insert = merged(low, lreach=back + Vector((0.03 * self.H, 0.0, 0.05 * self.H)), prop=1.0, head=(-2, 0, -4))
        insert2 = merged(insert, prop=0.0)
        return self.keyed("Reload", [(0, aim), (10, merged(low, prop=0.0)), (18, grab), (19, grab2), (30, insert),
                                     (36, insert), (37, insert2), (46, aim), (54, aim)])

    # ------------------------------------------------------------ all actions of a creature
    def make_all(self):
        cfg = self.cfg
        self.cycle("Idle", cfg["idle"]["n"], self.idle_spec)
        self.cycle("Walk", cfg["walk"]["n"], lambda ph: self.loco_spec(ph, cfg["walk"]))
        role = cfg["role"]
        if role == "villager":
            self.cycle("Run", cfg["run"]["n"], self.villager_run)
        else:
            self.cycle("Run", cfg["run"]["n"], lambda ph: self.loco_spec(ph, cfg["run"]))
        if role == "spear":
            self.attack_spear()
        elif role == "hammer":
            self.attack_hammer()
        elif role == "club":
            self.attack_club()
        elif role == "villager":
            self.cycle("Cower", 40, self.cower_spec)
            self.cycle("Talk", 60, self.talk_spec)
        elif role == "sniper":
            self.cycle("Aim", 40, lambda ph: self.aim_spec(sway=1.2 * math.sin(TAU * ph), chest=0.6 * math.sin(TAU * ph)))
            self.keyed("Shoot", [(0, self.aim_spec()), (2, self.aim_spec(push=0.015, chest=5, head=6)),
                                 (5, self.aim_spec(push=-0.01, tilt=4, chest=-2)), (14, self.aim_spec())])
            self.reload()
        self.hit()
        self.death()
        return self.actions
