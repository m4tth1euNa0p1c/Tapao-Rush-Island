"""The five tiki creatures (reference sheets in sprite/): dimensions, palettes and builders.

Every dimension is a fraction of the body height H (top of the egg, ornaments excluded) and lives in the SPECS
dictionaries; colours are sRGB hex. Joints (`J`) give the skeleton (rig.py) and the limbs.
Each creature produces three groups of parts, skinned separately:
  body    egg, mask, ornaments, limbs (SK_<Name>)
  weapon  held object, built in grip space (origin = grip, long axis +Z), 100 % on bone weapon_r
  prop    bramble only: the dart shown in the left hand while reloading (bone prop_l, scaled 0 otherwise)
"""
import math
import random
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

from . import mats
from .kit import (Body, Mask, TAU, V, bm_blob, bm_box, bm_helix, bm_horn, bm_hull, bm_leaf, bm_patch, bm_prism,
                  bm_revolve, bm_torus, bm_tube, catmull, ccw, chaikin, ellipse, fib_sphere, lerp, mirror_poly,
                  obj_from_bm, resample, superellipse)

ART = Path(__file__).resolve().parents[3] / "art"
MODELS = ART / "models"

# ---------------------------------------------------------------- specifications

SPECS = {
    "coco": dict(
        name="CocoWarrior", label="Guerrier Coco", sheet="mob-1", role="mob", cid="CHR_Mob_CocoWarrior",
        weapon="Spear", weapon_label="lance-massue à dents de pierre", H=1.00,
        body=dict(zb=0.21, zt=1.00, zw=0.55, rx=0.33, ry=0.27, pt=2.0, qt=0.62, pb=2.2, qb=0.455),
        J=dict(hip=(0.165, 0.0, 0.27), knee=(0.165, -0.005, 0.155), ankle=(0.165, 0.0, 0.075),
               toe=(0.165, -0.17, 0.025), shoulder=(0.29, 0.0, 0.55), elbow=(0.345, -0.005, 0.455),
               wrist=(0.375, -0.012, 0.375), tip=(0.385, -0.015, 0.29),
               spine=(0.37, 0.46, 0.67), crest=(0.97, 1.13)),
        arm_r=0.058, leg_r=0.064, hand="mitten", foot="sandal", band="torus", hand_size=1.22, foot_size=1.25,
        mask=dict(cz=0.56, hw=0.20, hh=0.235, off=0.008, dome=0.014),
        death=-1,
        palette={
            "Husk": dict(kind="tex", set="characters/Bark014", dark="4A2A12", light="B07A44", keep=0.12,
                         density=3.0, bump=0.55, rough=(0.6, 0.95), tone=0.12),
            "Mask": dict(kind="clay", base="E8A33A", rough=0.42, tone=0.08, bump=0.15),
            "Carve": dict(kind="clay", base="F3E6C4", rough=0.5, tone=0.05, bump=0.1),
            "Dark": dict(kind="clay", base="1C1410", rough=0.6, bump=0.05),
            "Skin": dict(kind="clay", base="C8402E", rough=0.48, tone=0.1, sss=0.05, bump=0.12),
            "Band": dict(kind="clay", base="6BA33A", rough=0.55, tone=0.08, bump=0.12),
            "Knot": dict(kind="tex", set="characters/rough_wood", dark="2E1A0C", light="7A4A26", keep=0.15,
                         density=6.0, bump=0.5),
            "Puff": dict(kind="clay", base="F1E8D2", rough=0.75, tone=0.05, bump=0.4, grain=60.0),
            "Wood": dict(kind="tex", set="characters/bark_willow", dark="3A2412", light="8C5C32", keep=0.15,
                         density=6.0, bump=0.6),
            "Stone": dict(kind="tex", set="characters/Rock030", dark="6A6660", light="C4C0B6", keep=0.2,
                          density=5.0, bump=0.7),
            "Rope": dict(kind="tex", set="characters/Rope001", dark="7A4C0A", light="EDB532", keep=0.05,
                         density=14.0, bump=0.6),
        }),
    "crab": dict(
        name="CrabGuardian", label="Gardien Crabe", sheet="mob-2", role="mob", cid="CHR_Mob_CrabGuardian",
        weapon="ShellHammer", weapon_label="marteau-coquillage", H=1.15,
        body=dict(zb=0.17, zt=1.00, zw=0.64, rx=0.37, ry=0.29, pt=2.4, qt=0.42, pb=2.0, qb=0.5),
        J=dict(hip=(0.17, 0.0, 0.23), knee=(0.17, -0.005, 0.135), ankle=(0.17, 0.0, 0.07),
               toe=(0.17, -0.16, 0.025), shoulder=(0.33, 0.0, 0.52), elbow=(0.39, -0.01, 0.43),
               wrist=(0.41, -0.02, 0.355), tip=(0.415, -0.04, 0.245),
               spine=(0.33, 0.43, 0.66), crest=(0.95, 1.05)),
        arm_r=0.056, leg_r=0.062, hand="crab", foot="crab", band="rope", hand_size=1.3, foot_size=1.15,
        mask=dict(cz=0.64, hw=0.26, hh=0.235, off=0.008, dome=0.014),
        death=-1,
        palette={
            "Shell": dict(kind="tex", set="ground/coral_ground_02", dark="8A2A12", light="E26A3A", keep=0.04,
                          density=2.6, bump=0.8, rough=(0.35, 0.6), tone=0.1),
            "Limb": dict(kind="clay", base="D2563A", rough=0.42, tone=0.1, spots="9E3A1E", spot_amt=0.35,
                         bump=0.15),
            "Mask": dict(kind="clay", base="3FA09A", rough=0.4, tone=0.08, bump=0.12),
            "Carve": dict(kind="clay", base="F1E6C8", rough=0.45, tone=0.05, bump=0.1),
            "Dark": dict(kind="clay", base="141414", rough=0.6, bump=0.05),
            "Seam": dict(kind="clay", base="3E8C82", rough=0.45, bump=0.1),
            "Rope": dict(kind="tex", set="characters/Rope001", dark="7A4C0A", light="EDB532", keep=0.05,
                         density=14.0, bump=0.6),
            "Wood": dict(kind="tex", set="characters/bark_willow", dark="3A2412", light="8C5C32", keep=0.15,
                         density=6.0, bump=0.6),
        }),
    "ash": dict(
        name="AshWatcher", label="Veilleur des Cendres", sheet="mob-3", role="mob", cid="CHR_Mob_AshWatcher",
        weapon="ObsidianClub", weapon_label="massue d'obsidienne", H=1.30,
        body=dict(zb=0.14, zt=0.95, zw=0.50, rx=0.355, ry=0.29, pt=1.8, qt=0.75, pb=2.4, qb=0.42),
        J=dict(hip=(0.17, 0.0, 0.21), knee=(0.17, -0.005, 0.13), ankle=(0.17, 0.0, 0.065),
               toe=(0.17, -0.15, 0.022), shoulder=(0.31, 0.0, 0.47), elbow=(0.37, -0.01, 0.38),
               wrist=(0.40, -0.02, 0.30), tip=(0.41, -0.04, 0.205),
               spine=(0.31, 0.39, 0.60), crest=(0.86, 1.05)),
        arm_r=0.062, leg_r=0.066, hand="claws", foot="claws", band="rope2", hand_size=1.3, foot_size=1.2,
        mask=dict(cz=0.56, hw=0.25, hh=0.31, off=0.034, dome=0.016),
        death=1,
        palette={
            "Rock": dict(kind="lava", set="characters/Lava002", dark="121010", light="45403C", glow="FF6A2A",
                         strength=0.9, keep=0.0, density=2.2, bump=0.6, rough=(0.3, 0.6)),
            "Seam": dict(kind="emit", base="FF5A14", dark="1E0805", strength=1.6, vary=5.0),
            "Wood": dict(kind="tex", set="characters/weathered_planks", dark="3A2414", light="7E5634", keep=0.25,
                         density=2.6, bump=0.6),
            "Carve": dict(kind="clay", base="E8D8B8", rough=0.5, tone=0.06, bump=0.1),
            "Eye": dict(kind="emit", base="FF5A1A", strength=9.0),
            "Socket": dict(kind="clay", base="120C0A", rough=0.6, bump=0.05),
            "Limb": dict(kind="clay", base="7A2A26", rough=0.55, tone=0.12, spots="38110F", spot_amt=0.55,
                         spot_scale=10.0, bump=0.2),
            "Claw": dict(kind="clay", base="1A1414", rough=0.35, bump=0.05),
            "Rope": dict(kind="tex", set="characters/Rope001", dark="3A0E0C", light="8E2C22", keep=0.0,
                         density=14.0, bump=0.6),
            "Rope2": dict(kind="tex", set="characters/Rope001", dark="120A06", light="4E3626", keep=0.0,
                          density=14.0, bump=0.6),
            "Handle": dict(kind="tex", set="characters/rough_wood", dark="2A1A0E", light="6A4426", keep=0.15,
                           density=6.0, bump=0.6),
        }),
    "villager": dict(
        name="Villager", label="Villageois Inquiet", sheet="pnj-1", role="npc", cid="CHR_Npc_Villager",
        weapon="FruitBasket", weapon_label="panier de fruits", H=0.95,
        body=dict(zb=0.20, zt=1.00, zw=0.56, rx=0.39, ry=0.32, pt=2.0, qt=0.58, pb=2.3, qb=0.435),
        J=dict(hip=(0.16, 0.0, 0.25), knee=(0.16, -0.005, 0.145), ankle=(0.16, 0.0, 0.07),
               toe=(0.16, -0.16, 0.025), shoulder=(0.35, 0.0, 0.50), elbow=(0.405, -0.01, 0.41),
               wrist=(0.43, -0.02, 0.335), tip=(0.44, -0.025, 0.255),
               spine=(0.35, 0.43, 0.64), crest=(0.97, 1.10)),
        arm_r=0.056, leg_r=0.060, hand="mitten", foot="sandal", band="torus", hand_size=1.18, foot_size=1.2,
        mask=dict(cz=0.66, hw=0.29, hh=0.28, off=0.006, dome=0.012),
        death=-1,
        palette={
            "Husk": dict(kind="tex", set="characters/Bark014", dark="4A2A14", light="A66C38", keep=0.12,
                         density=3.0, bump=0.55, rough=(0.6, 0.95), tone=0.12),
            "Face": dict(kind="clay", base="F0B24A", rough=0.5, tone=0.07, bump=0.12),
            "Seam": dict(kind="clay", base="D08A30", rough=0.5, bump=0.1),
            "Sclera": dict(kind="clay", base="FAFAF6", rough=0.22, tone=0.02, bump=0.02),
            "Iris": dict(kind="clay", base="5A3418", rough=0.25, tone=0.1, bump=0.05),
            "Pupil": dict(kind="clay", base="140C08", rough=0.15, bump=0.0),
            "Shine": dict(kind="emit", base="FFFFFF", dark="FFFFFF", strength=1.5),
            "Brow": dict(kind="clay", base="5E3418", rough=0.55, bump=0.1),
            "Nose": dict(kind="clay", base="E8743A", rough=0.45, bump=0.08),
            "Mouth": dict(kind="clay", base="3A1A12", rough=0.5, bump=0.05),
            "Tongue": dict(kind="clay", base="C8504A", rough=0.4, bump=0.05),
            "Blush": dict(kind="clay", base="F08A5A", rough=0.6, bump=0.05),
            "Stripe": dict(kind="clay", base="FFFFFF", rough=0.6, bump=0.05),
            "Skin": dict(kind="clay", base="D86A3E", rough=0.48, tone=0.1, sss=0.05, bump=0.12),
            "Band": dict(kind="clay", base="6E9A3A", rough=0.55, tone=0.08, bump=0.12),
            "Skirt": dict(kind="tex", set="characters/hessian_230", dark="4A6626", light="8EAE54", keep=0.08,
                          density=5.0, bump=0.5),
            "Rope": dict(kind="tex", set="characters/Rope001", dark="6A4A22", light="CCA060", keep=0.1,
                         density=14.0, bump=0.6),
            "Patch": dict(kind="tex", set="characters/hessian_230", dark="9A8050", light="E0CC94", keep=0.1,
                          density=5.0, bump=0.5),
            "Stem": dict(kind="tex", set="characters/rough_wood", dark="3A2210", light="7A4A26", keep=0.15,
                         density=8.0, bump=0.5),
            "Leaf": dict(kind="clay", base="7AAA3A", rough=0.45, tone=0.15, bump=0.15, sss=0.08),
            "Wicker": dict(kind="tex", set="characters/Wicker002", dark="5E3A16", light="C68E4A", keep=0.3,
                           density=9.0, bump=0.8),
        }),
    "bramble": dict(
        name="BrambleStalker", label="Rôdeur des Ronces", sheet="sniper-1", role="sniper",
        cid="CHR_Mob_BrambleStalker", weapon="Blowgun", weapon_label="sarbacane", H=1.25,
        body=dict(zb=0.15, zt=1.00, zw=0.33, rx=0.31, ry=0.27, pt=1.6, qt=1.0, pb=2.0, qb=0.5),
        J=dict(hip=(0.13, 0.0, 0.25), knee=(0.13, -0.01, 0.14), ankle=(0.13, 0.0, 0.065),
               toe=(0.13, -0.15, 0.02), shoulder=(0.25, -0.07, 0.56), elbow=(0.315, -0.09, 0.43),
               wrist=(0.345, -0.11, 0.30), tip=(0.355, -0.13, 0.195),
               spine=(0.36, 0.45, 0.66), crest=(0.95, 1.10)),
        arm_r=0.048, leg_r=0.056, hand="claws", foot="claws", band="vine", hand_size=1.12, foot_size=1.05,
        mask=dict(cz=0.55, hw=0.14, hh=0.21, off=0.014, dome=0.012),
        death=-1,
        palette={
            "Under": dict(kind="tex", set="characters/Moss002", dark="16200F", light="34461E", keep=0.0,
                          density=4.0, bump=0.4),
            "Leaf": dict(kind="leaf", base="6A8A34", rough=0.55, bump=0.2),
            "Vine": dict(kind="tex", set="characters/bark_willow", dark="2A1A0E", light="624226", keep=0.1,
                         density=8.0, bump=0.6),
            "Wood": dict(kind="tex", set="characters/fine_grained_wood", dark="4A3020", light="8E6442", keep=0.3,
                         density=3.0, bump=0.5),
            "Carve": dict(kind="clay", base="E8D8B0", rough=0.5, tone=0.06, bump=0.1),
            "Eye": dict(kind="emit", base="FFD24A", strength=7.0),
            "Socket": dict(kind="clay", base="0E0A08", rough=0.6, bump=0.05),
            "Skin": dict(kind="tex", set="characters/tree_bark_03", dark="18220F", light="4A5A2C", keep=0.1,
                         density=5.0, bump=0.6),
            "Claw": dict(kind="clay", base="141812", rough=0.35, bump=0.05),
            "Bamboo": dict(kind="tex", set="characters/bamboo_veneer", dark="6A4E2A", light="BC985C", keep=0.3,
                           density=6.0, bump=0.4),
            "Cord": dict(kind="tex", set="characters/Rope001", dark="2A3A16", light="6E8E3C", keep=0.0,
                         density=14.0, bump=0.6),
            "Tuft": dict(kind="clay", base="E8DCC0", rough=0.85, tone=0.05, bump=0.5, grain=50.0),
            "LeafSolid": dict(kind="clay", base="5E7E30", rough=0.5, tone=0.15, bump=0.15, sss=0.08),
        }),
}
ORDER = ["villager", "coco", "crab", "ash", "bramble"]

# leaf colours of the bramble cloak (vertex colours, linear)
LEAF_TONES = ["4E6E2A", "6A8A34", "3A5222", "5A7A2E", "7A9A3C", "6A4A2A", "8A6A3A", "546A2C"]


def rot_x(deg):
    return Matrix.Rotation(math.radians(deg), 4, "X")


class Creature:
    """Builds one creature; parts end up in self.parts / self.weapon_parts / self.prop_parts."""

    def __init__(self, key, coll):
        self.key = key
        self.s = SPECS[key]
        self.name = self.s["name"]
        self.H = self.s["H"]
        self.coll = coll
        self.rnd = random.Random(hash(key) & 0xFFFF)
        for short, spec in self.s["palette"].items():
            mats.REGISTRY[f"M_{self.name}_{short}"] = spec
        b = {k: (v * self.H if k in ("zb", "zt", "zw", "rx", "ry") else v) for k, v in self.s["body"].items()}
        self.body = Body(**b)
        m = self.s["mask"]
        self.mask = Mask(self.body, 0.0, m["cz"] * self.H, m["hw"] * self.H, m["hh"] * self.H,
                         m["off"] * self.H, m["dome"] * self.H)
        self.parts, self.weapon_parts, self.prop_parts = [], [], []
        # weapon rest frame: grip at the palm, long axis forward (-Y) so it stands upright with the forearm
        # level (staff grip); the basket hangs (long axis +Z)
        self.w_rest_rot = Matrix.Identity(4) if key == "villager" else rot_x(90)
        self.grip_r = self.grip_point(-1)
        self.grip_l = self.grip_point(1)

    # ------------------------------------------------------------ helpers
    def m(self, short):
        return mats.make(f"M_{self.name}_{short}")

    def J(self, name, s=1):
        x, y, z = self.s["J"][name]
        return Vector((s * x * self.H, y * self.H, z * self.H))

    def h(self, v):
        return v * self.H

    def part(self, name, bm, mat, tag="body", group="body", **kw):
        ob = obj_from_bm(f"{self.name}_{name}", bm, mat if isinstance(mat, list) else self.m(mat) if isinstance(
            mat, str) else mat, self.coll, tag=tag, **kw)
        {"body": self.parts, "weapon": self.weapon_parts, "prop": self.prop_parts}[group].append(ob)
        return ob

    def hand_frame(self, s):
        wr, tip = self.J("wrist", s), self.J("tip", s)
        D = (tip - wr).normalized()
        F = Vector((0, -1, 0))
        F = (F - D * F.dot(D)).normalized()
        L = F.cross(D).normalized()
        return wr, D, F, L, (tip - wr).length

    def grip_point(self, s):
        wr, D, F, L, ln = self.hand_frame(s)
        return wr + D * (0.55 * ln) + F * self.h(0.008)

    # ------------------------------------------------------------ build
    def build(self):
        getattr(self, f"build_{self.key}")()
        return self

    # ------------------------------------------------------------ shared pieces
    def egg(self, mat, nu=64, nz=56, name="Body"):
        bm = bmesh.new()
        bm_revolve(bm, self.body, nu, nz)
        return self.part(name, bm, mat, "body", recalc=True)

    def limbs(self, skin, band, hand_mat=None, claw=None, band2=None, toe_mat=None):
        H = self.H
        for s in (1, -1):
            side = "l" if s > 0 else "r"
            # arm
            sh, el, wr = self.J("shoulder", s), self.J("elbow", s), self.J("wrist", s)
            ra = self.h(self.s["arm_r"])
            sh_in = sh - Vector((s * self.h(0.06), 0, 0))
            bm = bmesh.new()
            bm_tube(bm, [sh_in, sh, sh.lerp(el, 0.5), el, el.lerp(wr, 0.5), wr],
                    [ra * 1.08, ra * 1.06, ra, ra * 0.96, ra * 0.92, ra * 0.88], segs=16)
            self.part(f"Arm_{side}", bm, skin, f"arm_{side}", subsurf=1, recalc=True)
            # leg
            hip, kn, an = self.J("hip", s), self.J("knee", s), self.J("ankle", s)
            rl = self.h(self.s["leg_r"])
            bm = bmesh.new()
            bm_tube(bm, [hip + Vector((0, 0, self.h(0.06))), hip, hip.lerp(kn, 0.5), kn, kn.lerp(an, 0.5), an],
                    [rl, rl, rl * 0.98, rl * 0.94, rl * 0.9, rl * 0.86], segs=16)
            self.part(f"Leg_{side}", bm, skin, f"leg_{side}", subsurf=1, recalc=True)
            # hands, feet, bands
            getattr(self, f"hand_{self.s['hand']}")(s, hand_mat or skin, claw)
            getattr(self, f"foot_{self.s['foot']}")(s, skin, band, claw or toe_mat)
            self.bands(s, band, band2, ra, rl)

    def bands(self, s, band, band2, ra, rl):
        side = "l" if s > 0 else "r"
        el, wr = self.J("elbow", s), self.J("wrist", s)
        kn, an = self.J("knee", s), self.J("ankle", s)
        kind = self.s["band"]
        if kind == "torus":
            bm = bmesh.new()
            bm_torus(bm, el.lerp(wr, 0.78), wr - el, ra * 0.9 + self.h(0.006), self.h(0.014), seg=28, ring=10)
            self.part(f"Wristband_{side}", bm, band, f"arm_{side}", recalc=True, subsurf=1)
            return
        r = self.h(0.008 if kind == "vine" else 0.0075)
        bm, bm2 = bmesh.new(), bmesh.new()
        bm_helix(bm, el.lerp(wr, 0.55), el.lerp(wr, 1.0), ra * 0.9 + r * 0.7, r, 3.0, segs=8)
        bm_helix(bm2, kn.lerp(an, 0.55), kn.lerp(an, 1.0), rl * 0.88 + r * 0.7, r, 3.0, segs=8)
        if kind == "vine":
            bm_helix(bm2, self.J("hip", s).lerp(kn, 0.6), kn.lerp(an, 0.1), rl * 0.95 + r * 0.7, r, 2.5, segs=8)
        self.part(f"Wristwrap_{side}", bm, band, f"arm_{side}", recalc=True)
        self.part(f"Anklewrap_{side}", bm2, band, f"leg_{side}", recalc=True)
        if kind == "rope2":  # a second, darker rope over the first one
            bm, bm2 = bmesh.new(), bmesh.new()
            bm_helix(bm, el.lerp(wr, 0.5), el.lerp(wr, 0.9), ra * 0.92 + r * 1.7, r, 2.0, segs=8, phase=1.5)
            bm_helix(bm2, kn.lerp(an, 0.5), kn.lerp(an, 0.9), rl * 0.9 + r * 1.7, r, 2.0, segs=8, phase=1.5)
            self.part(f"Wristwrap2_{side}", bm, band2, f"arm_{side}", recalc=True)
            self.part(f"Anklewrap2_{side}", bm2, band2, f"leg_{side}", recalc=True)

    # hands -------------------------------------------------------
    def hs(self, v):
        return self.h(v) * self.s.get("hand_size", 1.0)

    def fs(self, v):
        return self.h(v) * self.s.get("foot_size", 1.0)

    def hand_mitten(self, s, skin, claw=None):
        side = "l" if s > 0 else "r"
        wr, D, F, L, ln = self.hand_frame(s)
        h = self.hs
        c = wr + D * h(0.045)
        R = Matrix((L, F, D)).transposed()
        inner = -s * L
        bm = bmesh.new()
        bm_blob(bm, c, (h(0.047), h(0.040), h(0.052)), rot=R, seg=16, ring=10)
        for k in (-1, 0, 1):
            fc = c + D * h(0.042) + F * h(0.012) + L * (k * h(0.021)) + inner * h(0.004)
            bm_blob(bm, fc, (h(0.0155), h(0.017), h(0.025)), rot=R, seg=10, ring=6)
        tc = c + F * h(0.03) + inner * h(0.03) - D * h(0.004)
        bm_blob(bm, tc, (h(0.016), h(0.016), h(0.024)), rot=R, seg=10, ring=6)
        self.part(f"Hand_{side}", bm, skin, f"hand_{side}", subsurf=1)

    def hand_crab(self, s, skin, claw=None):
        side = "l" if s > 0 else "r"
        wr, D, F, L, ln = self.hand_frame(s)
        h = self.hs
        c = wr + D * h(0.05)
        R = Matrix((L, F, D)).transposed()
        out = s * L
        bm = bmesh.new()
        bm_blob(bm, c, (h(0.054), h(0.046), h(0.064)), rot=R, seg=16, ring=10)
        bm_horn(bm, c + D * h(0.04) + out * h(0.022), (D + F * 0.55), h(0.105), h(0.03),
                bend=-out, bend_amt=0.5, segs=10, n=8)
        bm_horn(bm, c + D * h(0.035) - out * h(0.02), (D + F * 0.75), h(0.085), h(0.025),
                bend=out, bend_amt=0.45, segs=10, n=8)
        self.part(f"Claw_{side}", bm, skin, f"hand_{side}", subsurf=1, recalc=True)

    def hand_claws(self, s, skin, claw):
        side = "l" if s > 0 else "r"
        wr, D, F, L, ln = self.hand_frame(s)
        h = self.hs
        c = wr + D * h(0.045)
        R = Matrix((L, F, D)).transposed()
        inner = -s * L
        bm, bmc = bmesh.new(), bmesh.new()
        bm_blob(bm, c, (h(0.05), h(0.043), h(0.05)), rot=R, seg=16, ring=10)
        fingers = [(-1, 0.0), (0, 0.004), (1, 0.0)]
        for k, dy in fingers:
            base = c + D * h(0.03) + L * (k * h(0.024)) + F * h(dy)
            p1 = base + D * h(0.032) + F * h(0.008)
            p2 = base + D * h(0.052) + F * h(0.028)
            bm_tube(bm, [base, p1, p2], [h(0.017), h(0.015), h(0.012)], segs=8)
            bm_horn(bmc, p2 - F * h(0.004), D * 0.35 + F, h(0.03), h(0.011), bend=D, bend_amt=0.4, segs=8, n=6)
        tb = c + F * h(0.03) + inner * h(0.03)
        tp = tb + F * h(0.022) + D * h(0.012)
        bm_tube(bm, [tb, tp], [h(0.016), h(0.013)], segs=8)
        bm_horn(bmc, tp, F + D * 0.3, h(0.024), h(0.01), segs=8, n=6)
        self.part(f"Hand_{side}", bm, skin, f"hand_{side}", subsurf=1, recalc=True)
        self.part(f"HandClaws_{side}", bmc, claw, f"hand_{side}", recalc=True)

    # feet --------------------------------------------------------
    def foot_core(self, s):
        an, toe = self.J("ankle", s), self.J("toe", s)
        k = self.s.get("foot_size", 1.0)
        fy = an.y + (toe.y - an.y) * 0.5 * k
        return Vector((an.x, fy, self.fs(0.036))), Vector((an.x, an.y + (toe.y - an.y) * k, 0.0))

    def foot_sandal(self, s, skin, band, claw=None):
        side = "l" if s > 0 else "r"
        c, toe = self.foot_core(s)
        f = self.fs
        bm = bmesh.new()
        bm_blob(bm, c, (f(0.062), f(0.098), f(0.045)), cut_below=self.h(0.012), seg=16, ring=10)
        for k, dx in enumerate((-0.036, -0.012, 0.012, 0.036)):
            bm_blob(bm, (c.x + s * f(dx), toe.y + f(0.035), f(0.03)),
                    (f(0.02), f(0.024), f(0.02)), seg=10, ring=6, cut_below=self.h(0.012))
        self.part(f"Foot_{side}", bm, skin, f"foot_{side}", subsurf=1)
        bm = bmesh.new()
        bm_blob(bm, (c.x, c.y - f(0.008), self.h(0.009)), (f(0.072), f(0.118), self.h(0.011)), cut_below=0.0,
                seg=24, ring=8)
        an = self.J("ankle", s)
        bm_torus(bm, an + Vector((0, 0, self.h(0.008))), (0, 0, 1), self.h(self.s["leg_r"]) * 0.86 + self.h(0.006),
                 self.h(0.012), seg=24, ring=6)
        bm_torus(bm, (c.x, c.y - f(0.02), f(0.03)), (0, 1, 0), f(0.05), self.h(0.011), seg=24, ring=6, squash=1.3)
        self.part(f"Sandal_{side}", bm, band, f"foot_{side}", recalc=True)

    def foot_crab(self, s, skin, band, claw=None):
        side = "l" if s > 0 else "r"
        c, toe = self.foot_core(s)
        f = self.fs
        bm = bmesh.new()
        bm_blob(bm, c, (f(0.06), f(0.085), f(0.045)), cut_below=0.0, seg=16, ring=10)
        for dx in (-0.03, 0.0, 0.03):
            base = Vector((c.x + s * f(dx), toe.y + f(0.06), f(0.028)))
            bm_horn(bm, base, (s * dx * 4, -1, -0.25), f(0.06), f(0.024), bend=(0, 0, -1), bend_amt=0.3,
                    segs=10, n=6, r_tip=f(0.006))
        self.part(f"Foot_{side}", bm, skin, f"foot_{side}", subsurf=1, recalc=True)

    def foot_claws(self, s, skin, band, claw):
        side = "l" if s > 0 else "r"
        c, toe = self.foot_core(s)
        f = self.fs
        bm, bmc = bmesh.new(), bmesh.new()
        bm_blob(bm, c, (f(0.06), f(0.09), f(0.046)), cut_below=0.0, seg=16, ring=10)
        n = 3 if self.key == "bramble" else 4
        xs = (-0.03, 0.0, 0.03) if n == 3 else (-0.036, -0.012, 0.012, 0.036)
        for dx in xs:
            tc = Vector((c.x + s * f(dx), toe.y + f(0.04), f(0.024)))
            bm_blob(bm, tc, (f(0.019), f(0.026), f(0.02)), seg=10, ring=6, cut_below=0.0)
            bm_horn(bmc, tc + Vector((0, -f(0.018), -f(0.004))), (0, -1, -0.45), f(0.024), f(0.0105), segs=8, n=6)
        self.part(f"Foot_{side}", bm, skin, f"foot_{side}", subsurf=1)
        self.part(f"FootClaws_{side}", bmc, claw, f"foot_{side}", recalc=True)

    # mask features -----------------------------------------------
    def stroke(self, bm, pts_uv, r, extra=0.0, squash=0.55, n=None, closed=False, mask=None):
        """Raised clay line on the mask (carvings, brows); closed=True for loops (frames, rims)."""
        mask = mask or self.mask
        if closed:
            uv = [Vector(p) for p in pts_uv] + [Vector(pts_uv[0])]
        else:
            uv = catmull(pts_uv, n or max(10, 5 * len(pts_uv)))
        P = [mask.pt(p[0], p[1], extra + r * squash) for p in uv]
        ups = [mask.nrm(p[0], p[1]) for p in uv]
        bm_tube(bm, P, [r] * len(P), segs=8, caps=not closed, squash=squash, ups=ups)

    def angry_eye(self, cu, cv, a, b, inner, outer, k=40):
        """Ellipse clipped by the brow line from (inner u, v) to (outer u, v)."""
        out = []
        (iu, iv), (ou, ov) = inner, outer
        for u, v in ellipse(a, b, cu, cv, k):
            t = (u - iu) / (ou - iu) if ou != iu else 0.0
            line = iv + (ov - iv) * max(0.0, min(1.0, t))
            out.append((u, min(v, line)))
        return out

    def mirrored(self, poly):
        return [(-u, v) for u, v in poly]

    # ------------------------------------------------------------ COCO
    def build_coco(self):
        H = self.H
        self.egg("Husk")
        mk = self.mask
        outline = superellipse(1.0, 1.0, 4.6, k=72)
        outline = [(u * (1.0 + 0.04 * (v < 0)), v) for u, v in outline]
        bm = bmesh.new()
        bm_patch(bm, mk, outline, nr=10)
        self.part("Mask", bm, "Mask", "body", solidify=self.h(0.02))
        e = self.h(0.002)
        # eyes, mouth (dark)
        bm = bmesh.new()
        eye = self.angry_eye(0.40, 0.22, 0.25, 0.155, (0.12, 0.22), (0.70, 0.44))
        for poly in (eye, self.mirrored(eye)):
            bm_patch(bm, mk, poly, extra=e, nr=4)
        bm_patch(bm, mk, superellipse(0.62, 0.30, 5.0, 0.0, -0.55), extra=e, nr=4)
        self.part("MaskDark", bm, "Dark", "body", solidify=self.h(0.004))
        # carvings
        bm = bmesh.new()
        r = self.h(0.011)
        for sg in (1, -1):
            self.stroke(bm, [(sg * 0.12, 0.38), (sg * 0.42, 0.50), (sg * 0.72, 0.60)], r * 1.2)
            for a0, a1 in (((0.16, 0.66), (0.22, 0.84)), ((0.36, 0.68), (0.47, 0.84)), ((0.56, 0.69), (0.71, 0.80))):
                self.stroke(bm, [(sg * a0[0], a0[1]), (sg * a1[0], a1[1])], r * 0.9)
        self.stroke(bm, [(0, 0.86), (0, 0.45), (0, 0.12)], r)
        self.stroke(bm, [(-0.14, 0.0), (0, 0.13), (0.14, 0.0)], r)
        frame = resample(superellipse(0.72, 0.38, 6.0, 0.0, -0.55), 28)
        self.stroke(bm, frame, r, closed=True)
        self.part("MaskCarve", bm, "Carve", "body", recalc=True)
        # teeth
        bm = bmesh.new()
        for i in range(4):
            u0 = -0.56 + 0.28 * i
            bm_prism(bm, mk, [(u0 + 0.02, -0.27), (u0 + 0.26, -0.27), (u0 + 0.14, -0.58)], e, e + self.h(0.012))
        for i in range(3):
            u0 = -0.45 + 0.30 * i
            bm_prism(bm, mk, [(u0 + 0.02, -0.83), (u0 + 0.28, -0.83), (u0 + 0.15, -0.52)], e, e + self.h(0.012))
        self.part("Teeth", bm, "Carve", "body", bevel=self.h(0.003), recalc=True)
        # topknot and puff crown
        top = self.body.zt
        bm = bmesh.new()
        pts = [Vector((0, 0, top - self.h(0.05))), Vector((0.0, 0.0, top + self.h(0.03))),
               Vector((self.h(0.01), -self.h(0.004), top + self.h(0.08))), Vector((self.h(0.006), 0, top + self.h(0.12)))]
        bm_tube(bm, pts, [self.h(0.036), self.h(0.034), self.h(0.03), self.h(0.03)], segs=14)
        for a in (0.3, 2.4, 4.4):
            base = pts[-1] + Vector((math.cos(a), math.sin(a), 0)) * self.h(0.012)
            bm_horn(bm, base, (math.cos(a) * 0.5, math.sin(a) * 0.5, 1.0), self.h(0.03), self.h(0.012), segs=8)
        self.part("Topknot", bm, "Knot", "crest", recalc=True, subsurf=1)
        bm = bmesh.new()
        n = 10
        zr = top - self.h(0.035)
        rr = self.body.rx * self.body.f(zr) * 0.95
        for i in range(n):
            a = TAU * i / n + 0.2
            sc = 1.0 + self.rnd.uniform(-0.12, 0.12)
            bm_blob(bm, (rr * math.cos(a), rr * math.sin(a) * self.body.ry / self.body.rx, zr + self.h(0.01)),
                    (self.h(0.034) * sc, self.h(0.034) * sc, self.h(0.03) * sc), seg=10, ring=6)
        self.part("Puffs", bm, "Puff", "crest", subsurf=1)
        self.limbs("Skin", "Band")
        self.weapon_spear()

    def weapon_spear(self):
        H = self.H
        bm = bmesh.new()
        n = 14
        pts = [Vector((0.008 * math.sin(k * 1.3), 0.004 * math.cos(k * 0.9), -0.30 + 1.0 * k / (n - 1)))
               for k in range(n)]
        bm_tube(bm, pts, [0.016 - 0.002 * k / (n - 1) for k in range(n)], segs=12)
        for z in (-0.15, 0.12, 0.40):
            bm_blob(bm, (0.012, 0.0, z), (0.012, 0.012, 0.018), seg=8, ring=6)
        self.part("Spear_Shaft", bm, "Wood", "weapon", "weapon", subsurf=1, recalc=True)
        # stone blade: lens cross-section, leaf profile, studs along both edges
        bm, bms = bmesh.new(), bmesh.new()
        z0, z1 = 0.62, 1.08
        nb = 22
        pts, radii, ups = [], [], []

        def w_at(t):
            return 0.05 * math.sin(math.pi * (0.12 + 0.88 * t) ** 0.85) + 0.002

        for k in range(nb):
            t = k / (nb - 1)
            pts.append(Vector((0, 0, z0 + (z1 - z0) * t)))
            radii.append(w_at(t))
            ups.append(Vector((0, 1, 0)))
        bm_tube(bm, pts, radii, segs=16, squash=0.36, ups=ups)
        self.part("Spear_Blade", bm, "Stone", "weapon", "weapon", recalc=True, subsurf=1)
        for sgn in (1, -1):
            for i in range(6):
                t = 0.14 + 0.13 * i
                z = z0 + (z1 - z0) * t
                bm_blob(bms, (sgn * (w_at(t) + 0.004), 0, z), (0.012, 0.009, 0.012), seg=10, ring=6)
        self.part("Spear_Studs", bms, "Knot", "weapon", "weapon")
        bm = bmesh.new()
        bm_helix(bm, (0, 0, 0.56), (0, 0, 0.70), 0.022, 0.0055, 7.0, segs=8)
        bm_helix(bm, (0, 0, 0.58), (0, 0, 0.68), 0.029, 0.005, 3.0, segs=8, phase=1.2)
        self.part("Spear_Rope", bm, "Rope", "weapon", "weapon", recalc=True)
        bm = bmesh.new()
        for a in (0.6, 2.9):
            bm_horn(bm, (0.018 * math.cos(a), 0.018 * math.sin(a), 0.63), (math.cos(a), math.sin(a), 1.1), 0.075,
                    0.007, segs=8)
        self.part("Spear_Twigs", bm, "Knot", "weapon", "weapon", recalc=True)

    # ------------------------------------------------------------ CRAB
    def build_crab(self):
        H = self.H
        self.egg("Shell")
        mk = self.mask
        half = [(0, 0.78), (0.30, 0.90), (0.62, 1.0), (0.88, 0.93), (1.0, 0.55), (0.97, 0.05), (0.80, -0.45),
                (0.45, -0.85), (0, -1.0)]
        outline = resample(chaikin(mirror_poly(half), 3), 96)
        bm = bmesh.new()
        bm_patch(bm, mk, outline, nr=10)
        self.part("Mask", bm, "Mask", "body", solidify=self.h(0.02))
        e = self.h(0.002)
        bm = bmesh.new()
        eye = self.angry_eye(0.42, 0.30, 0.24, 0.16, (0.12, 0.30), (0.74, 0.52))
        for poly in (eye, self.mirrored(eye)):
            bm_patch(bm, mk, poly, extra=e, nr=4)
        bm_patch(bm, mk, superellipse(0.64, 0.34, 4.0, 0.0, -0.46), extra=e, nr=4)
        self.part("MaskDark", bm, "Dark", "body", solidify=self.h(0.004))
        bm = bmesh.new()
        r = self.h(0.013)
        for sg in (1, -1):
            self.stroke(bm, [(sg * 0.10, 0.50), (sg * 0.38, 0.64), (sg * 0.74, 0.70)], r * 1.3)
            self.stroke(bm, [(sg * 0.20, 0.66), (sg * 0.44, 0.83), (sg * 0.66, 0.91)], r)
            self.stroke(bm, [(sg * 0.66, 0.40), (sg * 0.86, 0.50)], r * 0.9)
        self.stroke(bm, [(0, 0.74), (0, 0.48), (0, 0.24)], r)
        self.stroke(bm, [(-0.11, 0.2), (0.11, 0.2)], r * 0.9)
        self.part("MaskCarve", bm, "Carve", "body", recalc=True)
        bm = bmesh.new()
        for i in range(4):
            u0 = -0.56 + 0.28 * i
            bm_prism(bm, mk, [(u0 + 0.025, -0.15), (u0 + 0.255, -0.15), (u0 + 0.255, -0.47), (u0 + 0.025, -0.47)],
                     e, e + self.h(0.014))
        for i in range(4):
            u0 = -0.50 + 0.25 * i
            bm_prism(bm, mk, [(u0 + 0.03, -0.77), (u0 + 0.22, -0.77), (u0 + 0.22, -0.58), (u0 + 0.03, -0.58)],
                     e, e + self.h(0.012))
        self.part("Teeth", bm, "Carve", "body", bevel=self.h(0.004), recalc=True)
        # horns, spikes, tubercles, back ridges
        B = self.body
        bm = bmesh.new()
        for sg in (1, -1):
            th, z = sg * 0.78, self.h(0.90)
            base = B.surf(th, z, -self.h(0.01))
            n = B.normal(th, z)
            bm_horn(bm, base, n * 0.6 + Vector((0, 0, 1.0)), self.h(0.15), self.h(0.048),
                    bend=Vector((sg, 0, 0)), bend_amt=0.35, segs=16, n=12)
            for th2, z2, ln in ((1.45, 0.80, 0.07), (1.5, 0.62, 0.06), (2.2, 0.80, 0.065), (2.6, 0.66, 0.055),
                                (1.05, 0.97, 0.05), (2.9, 0.88, 0.05)):
                th2 = sg * th2
                b2 = B.surf(th2, self.h(z2), -self.h(0.006))
                bm_horn(bm, b2, B.normal(th2, self.h(z2)) + Vector((0, 0, 0.3)), self.h(ln), self.h(0.03), segs=12,
                        n=7)
        self.part("Horns", bm, "Shell", "body", recalc=True)
        bm = bmesh.new()
        placed = 0
        tries = 0
        while placed < 46 and tries < 2000:
            tries += 1
            th = self.rnd.uniform(-math.pi, math.pi)
            z = self.h(self.rnd.uniform(0.32, 0.97))
            if abs(th) < 0.95 and abs(z - mk.cz) < mk.hh * 1.15:
                continue
            rr = self.h(self.rnd.uniform(0.010, 0.019))
            c = B.surf(th, z, -rr * 0.35)
            bm_blob(bm, c, (rr, rr, rr * 0.8), seg=8, ring=6)
            placed += 1
        self.part("Tubercles", bm, "Shell", "body")
        bm = bmesh.new()
        ridge = [B.surf(math.pi, self.h(z), self.h(0.004)) for z in (0.97, 0.85, 0.7, 0.55, 0.42)]
        bm_tube(bm, catmull(ridge, 30), self.h(0.012), segs=10, squash=0.6,
                ups=[B.normal(math.pi, p.z) for p in catmull(ridge, 30)])
        for sg in (1, -1):
            pts = [B.surf(math.pi - sg * a, self.h(z), self.h(0.004)) for a, z in ((1.25, 0.86), (0.8, 0.62),
                                                                                   (0.3, 0.42), (0.0, 0.36))]
            cp = catmull(pts, 30)
            bm_tube(bm, cp, self.h(0.009), segs=10, squash=0.6,
                    ups=[B.normal(math.atan2(p.x / B.rx, -p.y / B.ry), p.z) for p in cp])
        self.part("Seams", bm, "Seam", "body", recalc=True)
        self.limbs("Limb", "Rope")
        self.weapon_hammer()

    def weapon_hammer(self):
        bm = bmesh.new()
        n = 12
        pts = [Vector((0.006 * math.sin(k * 1.7), 0.004 * math.cos(k * 1.1), -0.20 + 0.70 * k / (n - 1)))
               for k in range(n)]
        bm_tube(bm, pts, [0.021 - 0.003 * k / (n - 1) for k in range(n)], segs=12)
        for z in (0.05, 0.28):
            bm_blob(bm, (-0.013, 0.0, z), (0.012, 0.012, 0.017), seg=12, ring=8)
        self.part("Hammer_Handle", bm, "Wood", "weapon", "weapon", subsurf=1, recalc=True)
        bm = bmesh.new()
        bm_helix(bm, (0, 0, -0.18), (0, 0, -0.05), 0.024, 0.0062, 5.5, segs=8)
        bm_helix(bm, (0, 0, 0.36), (0, 0, 0.47), 0.030, 0.0065, 4.5, segs=8)
        self.part("Hammer_Rope", bm, "Rope", "weapon", "weapon", recalc=True)
        shell = import_model(MODELS / "props" / "lambis_shell" / "lambis_shell_1k.gltf", self.coll,
                             f"{self.name}_Hammer_Shell", length=0.34, decimate=0.5)
        if shell:
            # long axis across the handle (X), opening facing down onto the handle top
            shell.location = (0.0, 0.0, 0.52)
            shell.rotation_euler = (0.0, 0.0, 0.0)
            shell["rig"] = "weapon"
            self.weapon_parts.append(shell)
            rename_materials(shell, f"M_{self.name}_Shell", tint="F0B888")

    # ------------------------------------------------------------ ASH
    def build_ash(self):
        H = self.H
        B = self.body
        rnd = self.rnd
        pts = []
        for d in fib_sphere(110, seed=7):
            z = B.zw + (d.z * (B.zt - B.zw) if d.z > 0 else d.z * (B.zw - B.zb))
            th = math.atan2(d.x, -d.y)
            p = B.surf(th, z)
            j = 1.0 + rnd.uniform(-0.035, 0.05)
            pts.append(Vector((p.x * j, p.y * j, z)))
        pts += [Vector((0, 0, B.zb)), Vector((0, 0, B.zt))]
        bm = bmesh.new()
        bm_hull(bm, pts, inset=self.h(0.0035), depth=self.h(0.0025))
        self.part("Rock", bm, [self.m("Rock"), self.m("Seam")], "body", smooth=False)
        # rock spikes on the crown
        bm = bmesh.new()
        for th, z, ht, w in ((0.0, 0.93, 0.17, 0.08), (0.75, 0.86, 0.11, 0.07), (-0.8, 0.86, 0.12, 0.07),
                             (2.3, 0.84, 0.1, 0.07), (-2.4, 0.85, 0.11, 0.075), (math.pi, 0.9, 0.12, 0.07)):
            base = B.surf(th, self.h(z), -self.h(0.03))
            up = (B.normal(th, self.h(z)) * 0.5 + Vector((0, 0, 1))).normalized()
            sp = []
            for k in range(14):
                t = rnd.uniform(0, 1)
                a = rnd.uniform(0, TAU)
                rr = self.h(w) * (1 - t) ** 1.2 * rnd.uniform(0.6, 1.0)
                side = up.orthogonal().normalized()
                side2 = up.cross(side)
                sp.append(base + up * (self.h(ht) * t) + (side * math.cos(a) + side2 * math.sin(a)) * rr)
            sp.append(base + up * self.h(ht) * 1.05)
            bm_hull(bm, sp, inset=self.h(0.0035), depth=self.h(0.003))
        self.part("Spikes", bm, [self.m("Rock"), self.m("Seam")], "head", smooth=False)
        mk = self.mask
        half = [(0, 1.0), (0.55, 1.0), (0.9, 0.93), (1.0, 0.55), (0.95, 0.0), (0.75, -0.5), (0.4, -0.85), (0, -1.0)]
        outline = resample(chaikin(mirror_poly(half), 2), 96)
        bm = bmesh.new()
        bm_patch(bm, mk, outline, nr=10)
        self.part("Mask", bm, "Wood", "body", solidify=self.h(0.035))
        e = self.h(0.002)
        bm, bme = bmesh.new(), bmesh.new()
        eye = self.angry_eye(0.42, 0.22, 0.26, 0.17, (0.12, 0.22), (0.74, 0.46))
        eye_in = self.angry_eye(0.43, 0.19, 0.15, 0.085, (0.2, 0.2), (0.66, 0.34))
        for poly, pin in ((eye, eye_in), (self.mirrored(eye), self.mirrored(eye_in))):
            bm_patch(bm, mk, poly, extra=e, nr=4)
            bm_patch(bme, mk, pin, extra=e * 2.5, nr=4, dome=self.h(0.004))
        mouth = []
        for i in range(40):
            t = TAU * i / 40
            mouth.append((0.58 * math.cos(t), -0.56 + 0.29 * math.sin(t)))
        bm_patch(bm, mk, mouth, extra=e, nr=4)
        self.part("MaskDark", bm, "Socket", "body", solidify=self.h(0.004))
        self.part("Eyes", bme, "Eye", "body")
        bm = bmesh.new()
        r = self.h(0.012)
        for k in range(3):
            y0 = 0.47 + 0.13 * k
            wdt = 0.30 - 0.07 * k
            self.stroke(bm, [(-wdt, y0), (0, y0 + 0.2), (wdt, y0)], r, n=20)
        for sg in (1, -1):
            self.stroke(bm, [(sg * 0.64, 0.06), (sg * 0.58, -0.38)], r)
            self.stroke(bm, [(sg * 0.64, 0.02), (sg * 0.82, 0.08)], r * 0.9)
            self.stroke(bm, [(sg * 0.61, -0.16), (sg * 0.80, -0.11)], r * 0.9)
        self.part("MaskCarve", bm, "Carve", "body", recalc=True)
        bm = bmesh.new()
        for i in range(5):
            u0 = -0.52 + 0.208 * i
            bm_prism(bm, mk, [(u0 + 0.01, -0.30), (u0 + 0.198, -0.30), (u0 + 0.104, -0.63)], e, e + self.h(0.013))
        for i in range(4):
            u0 = -0.42 + 0.21 * i
            bm_prism(bm, mk, [(u0 + 0.01, -0.83), (u0 + 0.2, -0.83), (u0 + 0.105, -0.55)], e, e + self.h(0.013))
        self.part("Teeth", bm, "Carve", "body", bevel=self.h(0.003), recalc=True)
        # rope headband over the top of the mask, knotted on both sides
        bm = bmesh.new()
        zb = mk.cz + mk.hh * 0.93
        ring = []
        for i in range(73):
            th = TAU * i / 72
            front = max(0.0, math.cos(th)) ** 2
            z = zb - self.h(0.015) * front + self.h(0.02) * (1 - front)
            off = self.h(0.045) + (mk.off + self.h(0.02) - self.h(0.045)) * front
            ring.append(B.surf(th, z, off))
        bm_tube(bm, ring, self.h(0.017), segs=10, caps=False)
        for sg in (1, -1):
            th = sg * 1.35
            c = B.surf(th, zb + self.h(0.01), self.h(0.07))
            bm_blob(bm, c, (self.h(0.03), self.h(0.03), self.h(0.026)), seg=14, ring=10)
            bm_blob(bm, c + Vector((sg * self.h(0.022), 0, -self.h(0.01))), (self.h(0.02),) * 3, seg=12, ring=8)
        self.part("Headband", bm, "Rope", "head", recalc=True)
        self.limbs("Limb", "Rope", claw=self.m("Claw"), band2="Rope2")
        self.weapon_club()

    def weapon_club(self):
        rnd = random.Random(31)
        bm = bmesh.new()
        n = 12
        pts = [Vector((0.007 * math.sin(k * 1.5), 0.005 * math.cos(k * 1.2), -0.22 + 0.68 * k / (n - 1)))
               for k in range(n)]
        bm_tube(bm, pts, [0.025 - 0.004 * k / (n - 1) for k in range(n)], segs=12)
        for z in (-0.08, 0.16):
            bm_blob(bm, (0.014, 0.0, z), (0.013, 0.013, 0.019), seg=12, ring=8)
        self.part("Club_Handle", bm, "Handle", "weapon", "weapon", subsurf=1, recalc=True)
        sp = []
        for d in fib_sphere(46, seed=3):
            j = rnd.uniform(0.85, 1.12)
            sp.append(Vector((d.x * 0.10 * j, d.y * 0.085 * j, 0.57 + d.z * 0.20 * j + (0.03 if d.z > 0.6 else 0))))
        bm = bmesh.new()
        bm_hull(bm, sp, inset=0.0045, depth=0.004)
        self.part("Club_Head", bm, [self.m("Rock"), self.m("Seam")], "weapon", "weapon", smooth=False)
        bm = bmesh.new()
        bm_helix(bm, (0, 0, 0.30), (0, 0, 0.43), 0.031, 0.0075, 4.5, segs=8)
        bm_helix(bm, (0, 0, 0.32), (0, 0, 0.41), 0.039, 0.007, 2.5, segs=8, phase=2.0)
        self.part("Club_Rope", bm, "Rope", "weapon", "weapon", recalc=True)

    # ------------------------------------------------------------ VILLAGER
    def build_villager(self):
        H = self.H
        B = self.body
        self.egg("Husk")
        mk = self.mask
        bm = bmesh.new()
        bm_patch(bm, mk, superellipse(1.0, 1.0, 2.5, k=72), nr=10)
        self.part("Face", bm, "Face", "body", solidify=self.h(0.016))
        e = self.h(0.0015)
        bm = bmesh.new()
        self.stroke(bm, [(0, 0.98), (0, 0.80), (0, 0.62)], self.h(0.004), squash=0.4)
        self.part("FaceSeam", bm, "Seam", "body", recalc=True)
        # eyes: domed sclera, iris looking up, pupil, two highlights
        sclera_h = self.h(0.03)
        bms, bmi, bmp, bmh = bmesh.new(), bmesh.new(), bmesh.new(), bmesh.new()

        def dome_disc(bmx, cu, cv, ru, rv, base, height, nr=6, k=40):
            ring = ellipse(ru, rv, cu, cv, k)
            c = bmx.verts.new(mk.pt(cu, cv, base + height))
            rings = []
            for j in range(1, nr + 1):
                s = j / nr
                hh = height * math.sqrt(max(0.0, 1 - s * s))
                rings.append([bmx.verts.new(mk.pt(cu + (u - cu) * s, cv + (v - cv) * s, base + hh)) for u, v in ring])
            for i in range(k):
                bmx.faces.new((c, rings[0][i], rings[0][(i + 1) % k]))
            for j in range(nr - 1):
                for i in range(k):
                    i2 = (i + 1) % k
                    bmx.faces.new((rings[j][i], rings[j + 1][i], rings[j + 1][i2], rings[j][i2]))

        def on_sclera(cu, cv, u, v):
            d = math.sqrt(((u - cu) / 0.27) ** 2 + ((v - cv) / 0.30) ** 2)
            return e + sclera_h * math.sqrt(max(0.0, 1 - min(1.0, d) ** 2))

        for sg in (1, -1):
            cu, cv = sg * 0.40, 0.12
            dome_disc(bms, cu, cv, 0.27, 0.30, e, sclera_h)
            iu, iv = cu - sg * 0.01, cv + 0.08
            dome_disc(bmi, iu, iv, 0.165, 0.185, on_sclera(cu, cv, iu, iv) - self.h(0.004), self.h(0.006))
            dome_disc(bmp, iu, iv + 0.02, 0.095, 0.105, on_sclera(cu, cv, iu, iv) + self.h(0.0015), self.h(0.003))
            for du, dv, rr in ((0.06, 0.1, 0.05), (-0.05, -0.05, 0.028)):
                dome_disc(bmh, iu + du, iv + dv, rr, rr, on_sclera(cu, cv, iu, iv) + self.h(0.004), self.h(0.002),
                          nr=3, k=16)
        self.part("Sclera", bms, "Sclera", "body")
        self.part("Iris", bmi, "Iris", "body")
        self.part("Pupil", bmp, "Pupil", "body")
        self.part("Shine", bmh, "Shine", "body")
        bm = bmesh.new()
        for sg in (1, -1):
            self.stroke(bm, [(sg * 0.15, 0.60), (sg * 0.38, 0.62), (sg * 0.64, 0.50)], self.h(0.011), squash=0.6)
        self.part("Brows", bm, "Brow", "body", recalc=True)
        bm = bmesh.new()
        dome_disc(bm, 0.0, -0.13, 0.075, 0.07, e, self.h(0.022), nr=6)
        self.part("Nose", bm, "Nose", "body")
        bm, bmt = bmesh.new(), bmesh.new()
        mouth = [(0.13 * math.cos(t) * (1.0 + 0.15 * (math.sin(t) < 0)), -0.43 + 0.105 * math.sin(t))
                 for t in (TAU * i / 40 for i in range(40))]
        bm_patch(bm, mk, mouth, extra=e, nr=4)
        bm_patch(bmt, mk, ellipse(0.07, 0.035, 0.0, -0.49), extra=e * 2, nr=3)
        self.part("Mouth", bm, "Mouth", "body", solidify=self.h(0.003))
        self.part("Tongue", bmt, "Tongue", "body")
        bm, bmw = bmesh.new(), bmesh.new()
        for sg in (1, -1):
            bm_patch(bm, mk, ellipse(0.16, 0.13, sg * 0.66, -0.22), extra=e, nr=4)
            for a, b in (((0.56, 0.0), (0.80, 0.08)), ((0.56, -0.13), (0.82, -0.05)), ((0.58, -0.26), (0.80, -0.19))):
                self.stroke(bmw, [(sg * a[0], a[1]), (sg * b[0], b[1])], self.h(0.0085), extra=e, squash=0.4)
        self.part("Blush", bm, "Blush", "body")
        self.part("Stripes", bmw, "Stripe", "body", recalc=True)
        # stem, rope ring, three leaves
        top = B.zt
        bm = bmesh.new()
        pts = [Vector((0, 0, top - self.h(0.04))), Vector((0, 0, top + self.h(0.03))),
               Vector((self.h(0.008), self.h(0.004), top + self.h(0.07)))]
        bm_tube(bm, pts, [self.h(0.032), self.h(0.03), self.h(0.026)], segs=14)
        self.part("Stem", bm, "Stem", "crest", recalc=True, subsurf=1)
        bm = bmesh.new()
        zr = top - self.h(0.012)
        bm_torus(bm, (0, 0, zr), (0, 0, 1), self.h(0.042), self.h(0.011), seg=28, ring=8)
        bm_torus(bm, (0, 0, zr + self.h(0.016)), (0, 0, 1), self.h(0.038), self.h(0.01), seg=28, ring=8)
        self.part("StemRope", bm, "Rope", "crest", recalc=True)
        bm = bmesh.new()
        st = pts[-1]
        for d, ln, wd in (((1.0, 0.15, 0.55), 0.26, 0.105), ((-1.0, 0.35, 0.45), 0.24, 0.10),
                          ((-0.2, 1.0, 0.7), 0.17, 0.075)):
            dd = Vector(d).normalized()
            bm_leaf(bm, st, dd, Vector((0, 0, 1)) - dd * 0.4, self.h(ln), self.h(wd), fold=0.25, droop=0.35, n=8)
        self.part("Leaves", bm, "Leaf", "crest", solidify=self.h(0.004))
        # skirt (flared band with a ragged hem), rope belt with knot, patch
        z_top, z_bot = self.h(0.385), self.h(0.19)
        r_top = B.f(z_top)
        bm = bmesh.new()
        nu, nz = 72, 10
        rows = []
        for j in range(nz + 1):
            t = j / nz
            z = z_top + (z_bot - z_top) * t
            flare = self.h(0.012) + self.h(0.05) * t ** 1.3
            row = []
            for i in range(nu):
                th = TAU * i / nu
                rag = self.h(0.012) * (0.5 + 0.5 * math.sin(th * 13 + 1.3) * math.cos(th * 7)) * (t == 1.0)
                zz = z + rag
                row.append(bm.verts.new(((B.rx * r_top + flare) * math.sin(th), -(B.ry * r_top + flare) * math.cos(th),
                                         zz)))
            rows.append(row)
        for j in range(nz):
            for i in range(nu):
                i2 = (i + 1) % nu
                bm.faces.new((rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]))
        self.part("Skirt", bm, "Skirt", "skirt", solidify=self.h(0.008), recalc=True)
        bm = bmesh.new()
        belt_r = (B.rx * r_top + self.h(0.016), B.ry * r_top + self.h(0.016))
        ring = [Vector((belt_r[0] * math.sin(TAU * i / 72), -belt_r[1] * math.cos(TAU * i / 72), z_top - self.h(0.004)))
                for i in range(73)]
        bm_tube(bm, ring, self.h(0.011), segs=10, caps=False)
        kth = 0.5
        kc = Vector((belt_r[0] * math.sin(kth), -belt_r[1] * math.cos(kth), z_top - self.h(0.004)))
        out = Vector((math.sin(kth), -math.cos(kth), 0)) * self.h(0.012)
        bm_blob(bm, kc + out, (self.h(0.022), self.h(0.016), self.h(0.02)), seg=14, ring=10)
        for k, dx in enumerate((-0.01, 0.012)):
            p0 = kc + out * 1.4 + Vector((self.h(dx), 0, -self.h(0.01)))
            bm_tube(bm, [p0, p0 + Vector((self.h(dx), -self.h(0.008), -self.h(0.06))),
                         p0 + Vector((self.h(dx * 1.5), -self.h(0.012), -self.h(0.11 - 0.02 * k)))],
                    [self.h(0.009), self.h(0.008), self.h(0.007)], segs=8)
        self.part("Belt", bm, "Rope", "skirt", recalc=True)
        bm = bmesh.new()
        pth, pz = -0.42, self.h(0.29)
        pc = Vector(((B.rx * r_top + self.h(0.028)) * math.sin(pth), -(B.ry * r_top + self.h(0.028)) * math.cos(pth), pz))
        rotm = Matrix.Rotation(-pth, 3, "Z") @ Matrix.Rotation(math.radians(-8), 3, "Y")
        bm_box(bm, pc, (self.h(0.08), self.h(0.006), self.h(0.075)), rot=rotm)
        self.part("Patch", bm, "Patch", "skirt", bevel=self.h(0.002))
        self.limbs("Skin", "Band")
        self.weapon_basket()

    def weapon_basket(self):
        # grip at the handle apex (origin), basket hanging below (-Z)
        bm = bmesh.new()
        z_rim, z_bot, r_top, r_bot = -0.125, -0.25, 0.135, 0.095
        nu, nz = 48, 8
        rows = [[bm.verts.new((0, 0, z_bot))] * nu]
        for j in range(1, nz + 1):
            t = j / nz
            z = z_bot + (z_rim - z_bot) * t
            r = r_bot + (r_top - r_bot) * math.sin(t * math.pi / 2) ** 0.8
            if j == 1:
                r *= 0.8
            rows.append([bm.verts.new((r * math.cos(TAU * i / nu), r * math.sin(TAU * i / nu), z)) for i in range(nu)])
        for j in range(nz):
            for i in range(nu):
                i2 = (i + 1) % nu
                q = [rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]]
                u = []
                for v in q:
                    if v not in u:
                        u.append(v)
                if len(u) >= 3:
                    bm.faces.new(u)
        self.part("Basket_Bowl", bm, "Wicker", "weapon", "weapon", solidify=0.008, recalc=True)
        bm = bmesh.new()
        bm_torus(bm, (0, 0, z_rim), (0, 0, 1), r_top, 0.011, seg=48, ring=10)
        arc = [Vector((r_top * 0.96 * math.cos(a), 0.0, z_rim + 0.125 * math.sin(a) / 1.0)) for a in
               (math.pi * k / 24 for k in range(25))]
        bm_tube(bm, arc, 0.011, segs=10)
        bm_helix(bm, (-0.025, 0, 0.0), (0.025, 0, 0.0), 0.014, 0.004, 3.0, segs=6)
        self.part("Basket_Rim", bm, "Wicker", "weapon", "weapon", recalc=True)
        ban = import_model(MODELS / "characters" / "bananas" / "bananas_1k.gltf", self.coll,
                           f"{self.name}_Basket_Bananas", length=0.19, decimate=0.12, pick="largest")
        if ban:
            ban.location = (0.02, -0.015, z_rim + 0.02)
            ban.rotation_euler = (math.radians(12), math.radians(-18), math.radians(20))
            ban["rig"] = "weapon"
            self.weapon_parts.append(ban)
            rename_materials(ban, f"M_{self.name}_Banana")
        for k, (x, y, z) in enumerate(((-0.06, 0.04, z_rim + 0.005), (-0.035, -0.06, z_rim + 0.0))):
            ap = import_model(MODELS / "characters" / "food_apple_01" / "food_apple_01_1k.gltf", self.coll,
                              f"{self.name}_Basket_Apple{k}", length=0.068, decimate=0.25)
            if ap:
                ap.location = (x, y, z)
                ap.rotation_euler = (0.3 * k, 0.2, 1.1 * k)
                ap["rig"] = "weapon"
                self.weapon_parts.append(ap)
                rename_materials(ap, f"M_{self.name}_Apple")
        bm = bmesh.new()
        for d in ((0.6, 0.6, 0.5), (-0.7, -0.2, 0.6)):
            dd = Vector(d).normalized()
            bm_leaf(bm, Vector((0.0, 0.0, z_rim + 0.01)), dd, Vector((0, 0, 1)) - dd * 0.5, 0.12, 0.05, droop=0.3, n=6)
        self.part("Basket_Leaves", bm, "Leaf", "weapon", "weapon", solidify=0.003)

    # ------------------------------------------------------------ BRAMBLE
    def build_bramble(self):
        H = self.H
        B = self.body
        rnd = self.rnd
        self.egg("Under", nu=64, nz=56)
        mk = self.mask
        half = [(0, 1.0), (0.62, 0.88), (0.95, 0.45), (1.0, 0.0), (0.85, -0.40), (0.5, -0.78), (0, -1.0)]
        outline = resample(chaikin(mirror_poly(half), 2), 96)
        bm = bmesh.new()
        bm_patch(bm, mk, outline, nr=10)
        self.part("Mask", bm, "Wood", "body", solidify=self.h(0.022))
        e = self.h(0.0015)
        bm, bme = bmesh.new(), bmesh.new()
        eye = self.angry_eye(0.42, 0.22, 0.30, 0.15, (0.1, 0.18), (0.78, 0.44))
        eye_in = [(u, v) for u, v in ellipse(0.2, 0.07, 0.44, 0.22, 32, rot=0.3)]
        for poly, pin in ((eye, eye_in), (self.mirrored(eye), self.mirrored(eye_in))):
            bm_patch(bm, mk, poly, extra=e, nr=4)
            bm_patch(bme, mk, pin, extra=e * 2.5, nr=4, dome=self.h(0.003))
        bm_patch(bm, mk, ellipse(0.2, 0.17, 0.0, -0.46, 40), extra=e, nr=4)
        self.part("MaskDark", bm, "Socket", "body", solidify=self.h(0.004))
        self.part("Eyes", bme, "Eye", "body")
        bm = bmesh.new()
        r = self.h(0.0085)
        for k in range(3):
            y0 = 0.80 - 0.13 * k
            w = 0.40 - 0.08 * k
            self.stroke(bm, [(-w, y0), (0, y0 - 0.22), (w, y0)], r, n=20)
        for sg in (1, -1):
            self.stroke(bm, [(sg * 0.70, 0.05), (sg * 0.60, -0.45)], r)
            self.stroke(bm, [(sg * 0.85, 0.0), (sg * 0.78, -0.30)], r * 0.8)
        ring = ellipse(0.25, 0.21, 0.0, -0.46, 24)
        self.stroke(bm, ring, r * 1.3, closed=True)
        self.part("MaskCarve", bm, "Carve", "body", recalc=True)
        bm = bmesh.new()
        for sg in (1, -1):
            base = mk.pt(sg * 0.24, -0.36, self.h(0.008))
            bm_horn(bm, base, (0, -0.25, -1.0), self.h(0.04), self.h(0.009), segs=8)
        self.part("Fangs", bm, "Carve", "body", recalc=True)
        # leaf cloak: layered leaves hanging down the envelope, none over the mask
        bm = bmesh.new()
        col = bm.loops.layers.float_color.new("Col")
        tones = [mats.srgb(t) for t in LEAF_TONES]
        rows = 15
        count = 0
        for j in range(rows):
            zf = 0.985 - 0.83 * j / (rows - 1)
            z = self.h(zf)
            n = int(10 + 26 * B.f(z) / 1.0)
            for i in range(n):
                th = TAU * (i + rnd.uniform(-0.35, 0.35) + 0.5 * (j % 2)) / n
                th = math.atan2(math.sin(th), math.cos(th))
                if abs(th) < 0.62 and mk.cz - mk.hh * 1.05 < z < mk.cz + mk.hh * 1.12:
                    continue
                base = B.surf(th, z, self.h(0.008 + 0.006 * rnd.random()))
                nrm = B.normal(th, z)
                down = Vector((0, 0, -1))
                down = (down - nrm * down.dot(nrm)).normalized()
                tang = nrm.cross(down).normalized()
                along = (down + tang * rnd.uniform(-0.45, 0.45) + nrm * rnd.uniform(0.05, 0.35)).normalized()
                ln = self.h(rnd.uniform(0.11, 0.19)) * (0.75 if zf > 0.85 else 1.0)
                c = list(tones[rnd.randrange(len(tones))])
                k = rnd.uniform(0.85, 1.12)
                c = [min(1.0, c[0] * k), min(1.0, c[1] * k), min(1.0, c[2] * k), 1.0]
                bm_leaf(bm, base, along, nrm, ln, ln * rnd.uniform(0.32, 0.42), fold=0.22,
                        droop=rnd.uniform(-0.05, 0.2), n=5, col=c, col_layer=col, twist=rnd.uniform(-0.4, 0.4))
                count += 1
        self.leaf_count = count
        self.part("Leaves", bm, "Leaf", "body")  # single-sided: two-sided material in Unreal
        # vines draped over the leaves, curly vine on the top
        bm = bmesh.new()
        for k in range(7):
            th0 = rnd.uniform(-math.pi, math.pi)
            if abs(th0) < 0.8:
                th0 += math.copysign(1.0, th0) * 0.8
            pts = []
            for i in range(9):
                t = i / 8
                z = self.h(0.96 - 0.62 * t)
                th = th0 + 0.25 * math.sin(t * 5 + k)
                pts.append(B.surf(th, z, self.h(0.028 + 0.012 * math.sin(t * 7 + k))))
            bm_tube(bm, catmull(pts, 40), [self.h(0.008) * (1 - 0.5 * i / 39) for i in range(40)], segs=8)
        self.part("Vines", bm, "Vine", "body", recalc=True)
        bm = bmesh.new()
        top = B.zt
        pts = [Vector((0, 0, top - self.h(0.04))), Vector((0, 0, top + self.h(0.02)))]
        for i in range(1, 30):
            t = i / 29
            a = t * TAU * 1.6
            rr = self.h(0.035) * (1 - 0.6 * t)
            pts.append(Vector((self.h(0.01) + rr * math.sin(a), -rr * 0.3 * math.cos(a),
                               top + self.h(0.02) + self.h(0.07) * t + rr * (1 - math.cos(a)) * 0.6)))
        bm_tube(bm, pts, [self.h(0.012) * (1 - 0.75 * i / (len(pts) - 1)) for i in range(len(pts))], segs=8)
        for a, ln in ((2.2, 0.06), (-0.6, 0.05)):
            bm_horn(bm, pts[4], (math.cos(a), math.sin(a), 0.9), self.h(ln), self.h(0.006), bend=(0, 0, -1),
                    bend_amt=0.5, segs=6)
        self.part("TopVine", bm, "Vine", "crest", recalc=True)
        # fern fronds: one down the middle of the back, two on the crown
        bm = bmesh.new()
        colf = bm.loops.layers.float_color.new("Col")
        for th, z, d, ln, tone in ((math.pi, 0.90, (0, 0.30, -1.0), 0.58, "5E8E2E"),
                                   (2.3, 0.93, (0.55, 0.45, -1.0), 0.40, "6E9A34"),
                                   (-2.3, 0.93, (-0.55, 0.45, -1.0), 0.40, "547E2A")):
            base = B.surf(th, self.h(z), self.h(0.03))
            bm_fern(bm, base, Vector(d), B.normal(th, self.h(z)), self.h(ln), pairs=11,
                    col=mats.srgb(tone), col_layer=colf)
        self.part("Ferns", bm, "Leaf", "body")
        # vine belt with the bamboo dart quiver on the left hip
        zb = self.h(0.40)
        fb = B.f(zb)
        bm = bmesh.new()
        ring = [Vector(((B.rx * fb + self.h(0.032)) * math.sin(TAU * i / 72),
                        -(B.ry * fb + self.h(0.032)) * math.cos(TAU * i / 72), zb)) for i in range(73)]
        bm_tube(bm, ring, self.h(0.011), segs=8, caps=False)
        self.part("Belt", bm, "Vine", "belt", recalc=True)
        th = 1.05
        qc = B.surf(th, self.h(0.34), self.h(0.07))
        axis = Vector((0.12, 0.05, 1.0)).normalized()
        bm, bmv, bmt = bmesh.new(), bmesh.new(), bmesh.new()
        a1 = axis.orthogonal().normalized()
        a2 = axis.cross(a1)
        for k in range(7):
            ang = TAU * k / 7
            off = (a1 * math.cos(ang) + a2 * math.sin(ang)) * self.h(0.026) if k else Vector()
            p0 = qc + off - axis * self.h(0.08)
            ln = self.h(0.16 + 0.02 * ((k * 3) % 4))
            bm_tube(bm, [p0, p0 + axis * ln], self.h(0.012), segs=10)
            if k in (0, 2, 5):
                tip = p0 + axis * (ln + self.h(0.035))
                bm_tube(bmt, [p0 + axis * ln * 0.8, tip], self.h(0.003), segs=6)
                bm_blob(bmt, tip, (self.h(0.012), self.h(0.012), self.h(0.016)), seg=10, ring=8)
        for t in (0.25, 0.7):
            bm_torus(bmv, qc - axis * self.h(0.08) + axis * self.h(0.16) * t, axis, self.h(0.042), self.h(0.007),
                     seg=24, ring=6)
        self.part("Quiver", bm, "Bamboo", "belt", recalc=True)
        self.part("QuiverWrap", bmv, "Vine", "belt", recalc=True)
        self.part("QuiverDarts", bmt, "Tuft", "belt", recalc=True)
        self.limbs("Skin", "Vine", claw=self.m("Claw"))
        self.weapon_blowgun()
        self.prop_dart()

    def weapon_blowgun(self):
        bm = bmesh.new()
        z0, z1 = -0.04, 1.08
        bm_tube(bm, [Vector((0, 0, z0)), Vector((0, 0, z1))], 0.019, segs=16, caps=False)
        for z in (0.16, 0.40, 0.64, 0.88):
            bm_torus(bm, (0, 0, z), (0, 0, 1), 0.0195, 0.0042, seg=24, ring=6)
        self.part("Blowgun_Tube", bm, "Bamboo", "weapon", "weapon", recalc=True)
        bm = bmesh.new()
        for z in (z0 + 0.001, z1 - 0.001):
            bm_blob(bm, (0, 0, z), (0.0165, 0.0165, 0.002), seg=16, ring=6)
        self.part("Blowgun_Bore", bm, "Socket", "weapon", "weapon")
        bm = bmesh.new()
        for a, b, t in ((0.0, 0.07, 4.0), (0.50, 0.56, 3.0), (1.0, 1.05, 3.0)):
            bm_helix(bm, (0, 0, a), (0, 0, b), 0.0215, 0.0045, t, segs=6)
        self.part("Blowgun_Cord", bm, "Cord", "weapon", "weapon", recalc=True)
        bm = bmesh.new()
        bm_leaf(bm, Vector((0.02, 0.0, 0.53)), Vector((0.5, 0.1, -1.0)), Vector((0, -1, 0)), 0.11, 0.045, droop=0.15,
                n=6)
        self.part("Blowgun_Leaf", bm, "LeafSolid", "weapon", "weapon", solidify=0.0025)

    def prop_dart(self):
        bm = bmesh.new()
        bm_tube(bm, [Vector((0, 0, -0.02)), Vector((0, 0, 0.11))], 0.0035, segs=8)
        bm_horn(bm, (0, 0, 0.11), (0, 0, 1), 0.025, 0.0035, segs=8)
        self.part("Dart_Shaft", bm, "Bamboo", "prop", "prop", recalc=True)
        bm = bmesh.new()
        bm_blob(bm, (0, 0, -0.03), (0.011, 0.011, 0.016), seg=12, ring=8)
        self.part("Dart_Tuft", bm, "Tuft", "prop", "prop")


# ---------------------------------------------------------------- imported CC0 models

def import_model(path, coll, name, length, decimate=1.0, pick=None):
    """Import a Poly Haven glTF, join it, centre it and scale its longest side to `length` (meters)."""
    path = Path(path)
    if not path.exists():
        print(f"[tiki] WARNING: missing model {path} (run fetch_character_assets.py)")
        return None
    before = set(bpy.data.objects)
    bpy.ops.import_scene.gltf(filepath=str(path))
    new = [o for o in bpy.data.objects if o not in before]
    meshes = [o for o in new if o.type == "MESH"]
    if not meshes:
        return None
    if pick == "largest":
        meshes.sort(key=lambda o: o.dimensions.length, reverse=True)
        keep = meshes[:1]
    else:
        keep = meshes
    names = [o.name for o in new]
    for o in new:
        for c in list(o.users_collection):
            c.objects.unlink(o)
        coll.objects.link(o)
    bpy.ops.object.select_all(action="DESELECT")
    for o in keep:
        o.select_set(True)
    bpy.context.view_layer.objects.active = keep[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    if len(keep) > 1:
        bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    for n in names:
        o = bpy.data.objects.get(n)
        if o is not None and o is not ob:
            bpy.data.objects.remove(o)
    ob.name = ob.data.name = name
    me = ob.data
    lo = Vector((min(v.co[i] for v in me.vertices) for i in range(3)))
    hi = Vector((max(v.co[i] for v in me.vertices) for i in range(3)))
    size = hi - lo
    k = length / max(size)
    ctr = (lo + hi) * 0.5
    for v in me.vertices:
        v.co = (v.co - ctr) * k
    if decimate < 1.0:
        mod = ob.modifiers.new("Decimate", "DECIMATE")
        mod.ratio = decimate
    bpy.ops.object.select_all(action="DESELECT")
    return ob


def rename_materials(ob, prefix, tint=None):
    """Prefix the imported materials; tint multiplies their base colour texture (palette match)."""
    for i, m in enumerate(ob.data.materials):
        if not m:
            continue
        if not m.name.startswith("M_"):
            m.name = prefix if i == 0 else f"{prefix}_{i}"
        if tint and m.use_nodes:
            nt = m.node_tree
            bsdf = next((n for n in nt.nodes if n.type == "BSDF_PRINCIPLED"), None)
            if bsdf is None or not bsdf.inputs["Base Color"].links:
                continue
            src = bsdf.inputs["Base Color"].links[0].from_socket
            mix = nt.nodes.new("ShaderNodeMix")
            mix.data_type, mix.blend_type = "RGBA", "MULTIPLY"
            mats.sock(mix.inputs, "Factor_Float").default_value = 1.0
            nt.links.new(src, mats.sock(mix.inputs, "A_Color"))
            mats.sock(mix.inputs, "B_Color").default_value = mats.srgb(tint)
            nt.links.new(mats.sock(mix.outputs, "Result_Color"), bsdf.inputs["Base Color"])


def bm_fern(bm, base, along, normal, length, pairs=11, col=None, col_layer=None):
    """Fern frond: drooping rachis with pairs of leaflets, longest near the base."""
    along = Vector(along).normalized()
    side = along.cross(Vector(normal)).normalized()
    up = side.cross(along).normalized()
    pts = [Vector(base) + along * (length * t) - up * (0.12 * length * t * t) for t in (k / 10 for k in range(11))]
    bm_tube(bm, pts, [length * 0.011 * (1 - 0.7 * k / 10) for k in range(11)], segs=6)
    if col_layer is not None and col is not None:
        dark = [x * 0.6 for x in col[:3]] + [1.0]
        for f in bm.faces[-60:]:
            for loop in f.loops:
                loop[col_layer] = dark
    for i in range(pairs):
        t = 0.08 + 0.86 * i / (pairs - 1)
        p = Vector(base) + along * (length * t) - up * (0.12 * length * t * t)
        ln = length * 0.24 * (1.0 - 0.72 * t)
        for sgn in (1, -1):
            d = (side * sgn + along * 0.55).normalized()
            bm_leaf(bm, p, d, up, ln, ln * 0.34, fold=0.12, droop=0.18, n=4, col=col, col_layer=col_layer)
