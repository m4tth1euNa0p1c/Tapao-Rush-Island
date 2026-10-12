"""
Animations du crabe géant (SK_GiantCrab / SKEL_GiantCrab), 30 i/s.

Provenance : factory/kits/crab_anims.py du 2e PC (maquette crabe-geant v0.2.0), rapatrié le 2026-10-09 : les 21 clips
v1 gardent leurs noms, durées et notifies (DA_GiantCrabRules s'y réfère). v2 (T-260) : pièces buccales et mandibules
plus vivantes, clips neufs (contrat avec le gameplay T-261) :
  A_GiantCrab_BellySlam  le corps se soulève puis s'écrase à plat, pattes écartées, se relève (AN_BellySlam_Impact) ;
  A_GiantCrab_Roar       transition de phase : cabré sur les pattes arrière, pinces écartées, bouche ouverte (AN_Roar) ;
  A_GiantCrab_LegStomp   les pattes avant piquent le sol à tour de rôle (AN_Stomp_L, AN_Stomp_R) ;
  A_GiantCrab_Charge     course latérale rapide en boucle vers la gauche du crabe (+X), corps bas, en place ;
  A_GiantCrab_Charge_R   la même vers la droite (-X) ;
et des notifies AN_Step_leg_<s><n>_dactyl au contact de chaque pointe de patte dans les clips de locomotion.

CrabAnimator résout chaque image analytiquement (aucune cuisson de contraintes) :
  - pose du corps (os body) autour du pivot, root (root motion des clips ponctuels) ;
  - pattes : cible de pointe (sol ou espace corps) -> coxa orientée vers la cible, IK planaire 2 os mérus / propode,
    dactyle à angle imposé (crab_kit.leg_chain) ; les pointes en appui sont verrouillées au sol ;
  - pinces : poignet en IK 2 os avec pôle, main orientée, doigt mobile ouvert d'un angle (crab_kit.claw_frames) ;
  - yeux (rabattement dans l'orbite), antennes, plaques buccales, mandibules, abdomen en FK local.
Les matrices obtenues sont converties en clés d'os (location + quaternion) et écrites directement dans les F-Curves
(1 clé par image, interpolation linéaire) : le résultat est déjà en FK, l'export n'a rien à cuire.

Locomotion EN PLACE (tétrapode alterné, groupes A / B) : les pointes en appui glissent vers l'arrière exactement à la
vitesse du clip (glissement nul une fois l'acteur déplacé par le CharacterMovement). Clips ponctuels v1 avec ROOT
MOTION sur l'os root (translation + lacet), pieds verrouillés au sol ; les clips v2 sont en place.
"""

import json
import math
import os

import bpy
from mathutils import Matrix, Quaternion, Vector

import crab_kit as CK

FPS = 30
SIDES = CK.SIDES
SGN = CK.SGN
LEGS = [(s, n) for s in SIDES for n in CK.PAIRS]
Z = Vector((0, 0, 1))


def leg_name(s, n):
    return f"leg_{s}{n}"


def ease(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * (3 - 2 * t)


def ease2(t):
    t = min(max(t, 0.0), 1.0)
    return t * t * t * (t * (6 * t - 15) + 10)


def ease_in(t):
    t = min(max(t, 0.0), 1.0)
    return t ** 2.2


def ease_out(t):
    t = min(max(t, 0.0), 1.0)
    return 1.0 - (1.0 - t) ** 2.2


def lerp(a, b, t):
    if isinstance(a, Vector):
        return a.lerp(b, t)
    if isinstance(a, (tuple, list)):
        return tuple(x + (y - x) * t for x, y in zip(a, b))
    return a + (b - a) * t


class Track:
    """Clés (image, valeur) interpolées en douceur (smoothstep) ; valeurs float, tuple ou Vector."""

    def __init__(self, keys, fn=ease):
        self.keys = sorted(keys, key=lambda k: k[0])
        self.fn = fn

    def __call__(self, f):
        k = self.keys
        if f <= k[0][0]:
            return k[0][1]
        for (f0, v0), (f1, v1) in zip(k[:-1], k[1:]):
            if f <= f1:
                t = (f - f0) / max(f1 - f0, 1e-9)
                return lerp(v0, v1, self.fn(t))
        return k[-1][1]


class Track2:
    """Clés (image, valeur[, interpolation vers cette clé]) ; interpolation par défaut : ease."""

    def __init__(self, keys):
        self.keys = sorted(keys, key=lambda k: k[0])

    def __call__(self, f):
        k = self.keys
        if f <= k[0][0]:
            return k[0][1]
        for a, b in zip(k[:-1], k[1:]):
            if f <= b[0]:
                fn = b[2] if len(b) > 2 else ease
                return lerp(a[1], b[1], fn((f - a[0]) / max(b[0] - a[0], 1e-9)))
        return k[-1][1]


def mx(s, v):
    """Coordonnées données pour le côté gauche -> côté s (miroir en X)."""
    return Vector((v[0] * SGN[s], v[1], v[2]))


# ============================================================================= poses de référence des pinces
# (côté gauche, repère corps au repos ; poignet, pôle du coude, direction de la main, haut de la main, ouverture)

def claw_preset(name, s):
    c = {
        "guard": None,
        "tuck": dict(wrist=(1.45, -1.35, 1.55), pole=(1.0, 0.3, 0.6), hand=(0.15, -0.55, 0.82), up=(0.2, -0.9, 0.2),
                     gape=10),
        "raised": dict(wrist=(1.55, -1.55, 2.15), pole=(1.0, 0.3, -0.2), hand=(0.25, -0.55, 0.80),
                       up=(-0.3, -0.8, 0.2), gape=45),
        "block": dict(wrist=(0.70, -1.78, 1.02), pole=(1.0, 0.0, -0.4), hand=(-0.80, -0.35, 0.40),
                      up=(0.0, -1.0, 0.2), gape=0),
        "spread": dict(wrist=(1.75, -1.25, 1.15), pole=(1.0, 0.3, 0.6), hand=(0.75, -0.55, -0.15),
                       up=(-0.1, 0.0, 1.0), gape=35),
        "ground": dict(wrist=(1.65, -1.55, 0.55), pole=(1.0, 0.2, 0.7), hand=(0.35, -0.75, -0.55),
                       up=(0.0, -0.2, 1.0), gape=20),
    }[name]
    if c is None:
        r = CK.CLAW_REST[s if s == "l" else "r"]
        base = dict(wrist=r["wrist"], pole=r["pole"], hand=r["hand"], up=r["up"], gape=r["gape"])
        # CLAW_REST est déjà exprimé pour le côté gauche (miroir appliqué ci-dessous)
        c = base
    return dict(wrist=mx(s, c["wrist"]), pole=mx(s, c["pole"]), hand=mx(s, c["hand"]), up=mx(s, c["up"]),
                gape=float(c["gape"]), space="body")


def claw_lerp(a, b, t):
    out = {}
    for k in ("wrist", "pole"):
        out[k] = a[k].lerp(b[k], t)
    for k in ("hand", "up"):
        v = a[k].normalized().lerp(b[k].normalized(), t)
        out[k] = v if v.length > 1e-6 else b[k]
    out["gape"] = a["gape"] + (b["gape"] - a["gape"]) * t
    out["space"] = b.get("space", "body") if t > 0.5 else a.get("space", "body")
    if a.get("space") != b.get("space"):
        out["space"] = "mixed"
        out["_a"], out["_b"], out["_t"] = a, b, t
    return out


class ClawTrack:
    def __init__(self, keys, fn=ease):
        self.keys = sorted(keys, key=lambda k: k[0])
        self.fn = fn

    def __call__(self, f):
        k = self.keys
        if f <= k[0][0]:
            return k[0][1]
        for (f0, v0), (f1, v1) in zip(k[:-1], k[1:]):
            if f <= f1:
                return claw_lerp(v0, v1, self.fn((f - f0) / max(f1 - f0, 1e-9)))
        return k[-1][1]


def claw_mod(c, **kw):
    out = dict(c)
    for k, v in kw.items():
        if k in ("wrist", "pole", "hand", "up"):
            out[k] = Vector(v)
        elif k == "d_wrist":
            out["wrist"] = c["wrist"] + Vector(v)
        else:
            out[k] = v
    return out


# ============================================================================= animateur

class CrabAnimator:
    def __init__(self, P, Lay, arm):
        self.P, self.Lay, self.arm = P, Lay, arm
        self.rest = {b.name: b.matrix_local.copy() for b in arm.data.bones}
        self.parent = {b.name: (b.parent.name if b.parent else None) for b in arm.data.bones}
        self.order = [b.name for b in arm.data.bones]          # parents avant enfants
        self.rel = {}
        for n in self.order:
            p = self.parent[n]
            self.rel[n] = (self.rest[p].inverted() @ self.rest[n]) if p else self.rest[n].copy()
        self.pivot = P.pivot.copy()
        self.feet0 = {(s, n): P.foot(s, n) for s, n in LEGS}
        self.coxa = {(s, n): P.coxa(s, n) for s, n in LEGS}
        self.L = {(s, n): P.lengths(n) for s, n in LEGS}
        self.shoulder = {s: mx(s, P.shoulder) for s in SIDES}
        self.clips = {}
        self.meta = {}
        self.diag = {}
        self.hull = self._body_hull(P)

    # ------------------------------------------------------------------ matrices
    def body_matrix(self, root, body):
        dx, dy, dz, pitch, roll, yaw = body
        pv = self.pivot
        return (root @ Matrix.Translation(pv + Vector((dx, dy, dz))) @ Matrix.Rotation(math.radians(yaw), 4, "Z")
                @ Matrix.Rotation(math.radians(-pitch), 4, "X") @ Matrix.Rotation(math.radians(-roll), 4, "Y")
                @ Matrix.Translation(-pv))

    @staticmethod
    def root_matrix(root):
        x, y, yaw = root
        return Matrix.Translation((x, y, 0.0)) @ Matrix.Rotation(math.radians(yaw), 4, "Z")

    @staticmethod
    def _body_hull(P):
        """Points de l'enveloppe rigide de la carapace (sans épines) : contour au bord, dôme, dessous."""
        o2 = CK.outline(P, 12)
        c = Vector((0.0, -0.05, 0.0))
        pts = []
        for s, z in ((1.0, (P.rim_hi + P.rim_lo) / 2), (0.62, P.top_z - 0.06), (0.80, P.ventral_z + 0.04)):
            for x, y in o2[::2]:
                pts.append(Vector((c.x + (x - c.x) * s, c.y + (y - c.y) * s, z)))
        pts += [Vector((0.0, -0.05, P.top_z)), Vector((0.0, 0.25, P.ventral_z - 0.05))]
        for s_ in SIDES:                                   # pointes des grandes épines latérales (v2)
            d = CK.mirror(Vector((1.0, -0.16, 0.20)), s_).normalized()
            base = CK.mirror(P.spine_tip, s_) - d * (P.spine_len + 0.10)
            pts.append(base + d * (CK.V2["lateral_spine_len"] + 0.10) + Vector((0.0, -0.07, 0.035)))
        return pts

    def _claw_points(self, s, Mc):
        """Points bas de la pince (espace corps) : bout du doigt fixe, du dactyle, dessous de la paume."""
        c = self.P.claw[s]
        Lp, H = c["palm"]["length_m"], c["palm"]["height_m"]
        return [Mc["hand"] @ Vector((0.0, Lp + c["fixed_finger_m"], -0.10 * H)),
                Mc["hand"] @ Vector((0.0, Lp * 0.5, -0.55 * H)),
                Mc["finger"] @ Vector((0.0, c["movable_finger_m"], -0.2 * H))]

    def ground_lift(self, B, tol=0.02):
        """v2 : soulève le corps de ce qu'il faut pour que la carapace ne traverse pas le sable (roulis des
        renversements, écrasement à plat ventre)."""
        low = min((B @ p).z for p in self.hull)
        return max(0.0, -tol - low)

    def solve(self, ps):
        """ps : pose -> {os: matrice armature}, diagnostics des pieds."""
        R = self.root_matrix(ps.get("root", (0.0, 0.0, 0.0)))
        B = self.body_matrix(R, ps.get("body", (0, 0, 0, 0, 0, 0)))
        lift = self.ground_lift(B)
        if lift > 0.0:
            B = Matrix.Translation((0.0, 0.0, lift)) @ B
        Bi = B.inverted()
        M = {"root": R @ self.rest["root"], "body": B @ self.rest["body"]}
        tips = {}
        ok_all = True
        bad = []
        # v2 : corps renversé (roulis > 90°) -> genoux pliés vers le ciel au lieu de traverser le sable
        bz = (B.to_3x3() @ Z).z
        knee_k = None if bz > 0.35 else max(-1.0, min(1.0, bz / 0.35))
        for key in LEGS:
            s, n = key
            tgt, dact, space = ps["feet"][key]
            tw = Vector(tgt) if space == "world" else B @ Vector(tgt)
            if tw.z < 0.0:                       # aucune pointe sous le sable (chutes, renversement)
                tw.z = 0.0
            tb = Bi @ tw
            joints, pn, d, ok = CK.leg_chain(self.coxa[key], tb, self.L[key], dact, knee_k=knee_k)
            # v2 : genou ou cheville sous le sable (roulis, renversement) -> l'autre côté du genou s'il est plus haut
            low = min((B @ joints[2]).z, (B @ joints[3]).z)
            if low < -0.05:
                for kk in (1.0, -1.0):
                    alt = CK.leg_chain(self.coxa[key], tb, self.L[key], dact, knee_k=kk)
                    low_alt = min((B @ alt[0][2]).z, (B @ alt[0][3]).z)
                    if low_alt > low + 0.02:
                        joints, pn, d, ok = alt
                        low = low_alt
            if not ok:
                bad.append(f"{s}{n}")
            ok_all &= ok
            frs = CK.leg_frames(joints, pn)
            for seg, fr in zip(CK.LEG_SEGS, frs):
                M[f"leg_{s}{n}_{seg}"] = B @ fr
            tip = B @ joints[4]
            tips[key] = tip
            M[f"ik_leg_{s}{n}"] = Matrix.Translation(tip) @ self.rest[f"ik_leg_{s}{n}"].to_3x3().to_4x4()
            knee = B @ joints[2]
            M[f"pole_leg_{s}{n}"] = (Matrix.Translation(knee + (B.to_3x3() @ (d * 0.4 + Z * 1.0)))
                                     @ self.rest[f"pole_leg_{s}{n}"].to_3x3().to_4x4())
        for s in SIDES:
            c = ps["claws"][s]
            cb = self._claw_body(c, Bi)
            Mc, ok = CK.claw_frames(self.P, s, self.shoulder[s], cb["wrist"], cb["pole"], cb["hand"], cb["up"],
                                    cb["gape"])
            # v2 : la pince ne s'enfonce pas de plus de 8 cm dans le sable (la pince plantée du Slam est tolérée)
            low = min((B @ q_).z for q_ in self._claw_points(s, Mc))
            if low < -0.08:
                cb = dict(cb, wrist=cb["wrist"] + Bi.to_3x3() @ Vector((0.0, 0.0, -0.06 - low)))
                Mc, ok = CK.claw_frames(self.P, s, self.shoulder[s], cb["wrist"], cb["pole"], cb["hand"], cb["up"],
                                        cb["gape"])
            for seg in CK.CLAW_SEGS:
                M[f"claw_{s}_{seg}"] = B @ Mc[seg]
        eyes = ps.get("eyes", {})
        for s in SIDES:
            retract, lx, lz, ex, ez = eyes.get(s, (0.0, 0.0, 0.0, 0.0, 0.0))
            st = f"eye_stalk_{s}"
            Ms = M["body"] @ self.rel[st] @ Matrix.Rotation(math.radians(-80.0 * SGN[s] * retract), 4, "Z") \
                @ Matrix.Rotation(math.radians(lx), 4, "X") @ Matrix.Rotation(math.radians(lz), 4, "Z")
            M[st] = Ms
            M[f"eye_{s}"] = Ms @ self.rel[f"eye_{s}"] @ Matrix.Rotation(math.radians(ex), 4, "X") \
                @ Matrix.Rotation(math.radians(ez), 4, "Z")
        ant = ps.get("ant", {})
        for s in SIDES:
            rots = ant.get(s, [(0.0, 0.0)] * 3)
            parent = M["body"]
            flip_w = max(0.0, min(1.0, (0.35 - bz) / 0.7))       # v2 : antennes rabattues quand il est renversé
            for i in range(3):
                nm = f"antenna_{s}_{i + 1:02d}"
                rx, rz = rots[i]
                rx -= (45.0, 25.0, 15.0)[i] * flip_w
                parent = parent @ self.rel[nm] @ Matrix.Rotation(math.radians(rx), 4, "X") \
                    @ Matrix.Rotation(math.radians(rz), 4, "Z")
                M[nm] = parent
            ax, az = ps.get("antennule", {}).get(s, (0.0, 0.0))
            M[f"antennule_{s}"] = M["body"] @ self.rel[f"antennule_{s}"] @ Matrix.Rotation(math.radians(ax), 4, "X") \
                @ Matrix.Rotation(math.radians(az), 4, "Z")
        M["mouth"] = M["body"] @ self.rel["mouth"] @ Matrix.Rotation(math.radians(ps.get("mouth", 0.0)), 4, "X")
        for s in SIDES:
            M[f"mandible_{s}"] = M["body"] @ self.rel[f"mandible_{s}"] @ Matrix.Rotation(
                math.radians(ps.get("mand", 0.0) * SGN[s]), 4, "Z")
        M["abdomen"] = M["body"] @ self.rel["abdomen"] @ Matrix.Rotation(math.radians(ps.get("abd", 0.0)), 4, "X")
        for n in self.order:
            if n not in M:
                M[n] = M[self.parent[n]] @ self.rel[n]
        self._bad = bad
        return M, tips, ok_all, B

    def _claw_body(self, c, Bi):
        if c.get("space") == "mixed":
            a = self._claw_body(c["_a"], Bi)
            b = self._claw_body(c["_b"], Bi)
            return claw_lerp(a, b, c["_t"])
        if c.get("space") == "world":
            R3 = Bi.to_3x3()
            return dict(wrist=Bi @ c["wrist"], pole=R3 @ c["pole"], hand=R3 @ c["hand"], up=R3 @ c["up"],
                        gape=c["gape"], space="body")
        return c

    def basis(self, M):
        out = {}
        for n in self.order:
            p = self.parent[n]
            ref = (M[p] @ self.rel[n]) if p else self.rest[n]
            b = ref.inverted() @ M[n]
            out[n] = (b.to_translation(), b.to_quaternion())
        return out

    # ------------------------------------------------------------------ helpers de pose
    def rest_feet(self, space="world", dact=-78.0):
        return {k: (self.feet0[k].copy(), dact, space) for k in LEGS}

    def base_pose(self):
        return dict(root=(0.0, 0.0, 0.0), body=(0, 0, 0, 0, 0, 0), feet=self.rest_feet(),
                    claws={s: claw_preset("guard", s) for s in SIDES}, eyes={}, ant={}, mouth=0.0, mand=0.0,
                    abd=0.0, dead=0.0)

    def idle_details(self, f, n, amp=1.0, seed=0.0):
        """Plaques buccales, mandibules, antennes, yeux : périodiques sur n images."""
        T = TAU = 2 * math.pi
        u = f / n
        mouth = amp * (5.0 + 5.0 * math.sin(T * 6 * u + seed) + 1.5 * math.sin(T * 14 * u + 1.1))
        mand = amp * (8.0 * math.sin(T * 8 * u + 0.5 + seed) + 3.0 * math.sin(T * 18 * u))
        ant = {}
        for s in SIDES:
            ph = 0.0 if s == "l" else 1.3
            ant[s] = [(6 * amp * math.sin(TAU * (2 * u) + ph + k * 0.7), 8 * amp * math.sin(TAU * (u) + ph + k * 0.9))
                      for k in range(3)]
        eyes = {s: (0.0, 4 * amp * math.sin(T * u + (0.4 if s == "l" else 2.0)),
                    12 * amp * math.sin(T * 2 * u + (0.0 if s == "l" else 1.7)), 0.0, 0.0) for s in SIDES}
        antennule = {s: (8 * amp * math.sin(T * 6 * u + (0 if s == "l" else 0.8)), 0.0) for s in SIDES}
        return dict(mouth=mouth, mand=mand, ant=ant, eyes=eyes, antennule=antennule)

    # ------------------------------------------------------------------ planificateurs d'appuis
    def gait_feet(self, f, n, direction, speed, duty=0.6, lift=0.28, phase_b=0.5):
        """Cycle en place : groupe A en appui de 0 à duty, groupe B décalé de phase_b."""
        S = speed * duty * n / FPS
        out = {}
        groups = self.P.groups
        for s, nn in LEGS:
            ph = 0.0 if leg_name(s, nn) in groups["A"] else phase_b
            c = (f / n - ph) % 1.0
            if c < duty - 1e-9:
                off = S * (0.5 - c / duty)
                z = 0.0
                dact = -78.0
            else:
                u = (c - duty) / (1 - duty)
                off = S * (-0.5 + ease2(u))
                z = lift * math.sin(math.pi * u) ** 0.85
                dact = -78.0 + 16.0 * math.sin(math.pi * u)
            out[(s, nn)] = (self.feet0[(s, nn)] + direction * off + Vector((0, 0, z)), dact, "world")
        return out

    def planned_feet(self, n, VR, steps, lift=0.26, lead=6, start=None, rest_scale=1.0):
        """Pas planifiés (clips à root motion / virages) : pointe fixe au sol hors des fenêtres de pas ;
        atterrissage = VR(image + lead) @ pied de repos. Retourne {image: {patte: (pos, dact, 'world')}}."""
        frames = {f: {} for f in range(n + 1)}
        for key in LEGS:
            rest = self.feet0[key] * 1.0
            rest = Vector((rest.x * rest_scale, rest.y, 0.0))
            cur = (start[key] if start else VR(0) @ rest).copy()
            plan = sorted(steps.get(key, []))
            segs = []
            for a, b in plan:
                land = VR(min(b + lead, n)) @ rest
                land.z = 0.0
                segs.append((a, b, cur.copy(), land))
                cur = land
            for f in range(n + 1):
                pos, dact = None, -78.0
                for a, b, p0, p1 in segs:
                    if a <= f <= b:
                        u = (f - a) / max(b - a, 1)
                        pos = p0.lerp(p1, ease2(u)) + Vector((0, 0, lift * math.sin(math.pi * u) ** 0.85))
                        dact = -78.0 + 14.0 * math.sin(math.pi * u)
                        break
                if pos is None:
                    pos = segs[0][2] if (segs and f < segs[0][0]) else None
                    if pos is None:
                        done = [sg for sg in segs if f > sg[1]]
                        pos = done[-1][3] if done else (start[key] if start else VR(0) @ rest)
                frames[f][key] = (pos.copy(), dact, "world")
        return frames

    def group_steps(self, windows_a, windows_b):
        out = {}
        for s, nn in LEGS:
            out[(s, nn)] = list(windows_a if leg_name(s, nn) in self.P.groups["A"] else windows_b)
        return out

    # ------------------------------------------------------------------ enregistrement d'un clip
    def record(self, name, n, pose_fn, loop=False, category="Combat", notifies=(), root_motion="aucun",
               peak=0.5, extra=None, dead=None):
        poses = [pose_fn(f) for f in range(n + 1)]
        mats, tips, oks, roots = [], [], [], []
        badlegs = set()
        for ps in poses:
            M, tp, ok, B = self.solve(ps)
            badlegs.update(self._bad)
            mats.append(M)
            tips.append(tp)
            oks.append(ok)
            roots.append(ps.get("root", (0.0, 0.0, 0.0)))
        if loop:
            mats[-1] = mats[0]
            tips[-1] = tips[0]
        self.clips[name] = dict(n=n, mats=mats, tips=tips, roots=roots, loop=loop,
                                dead=[ps.get("dead", 0.0) for ps in poses])
        self.meta[name] = dict(frames=n, seconds=round(n / FPS, 3), loop=loop, category=category,
                               notifies=[[a, int(b)] for a, b in notifies], root_motion=root_motion,
                               peak_frame=int(round(peak * n)) if isinstance(peak, float) else int(peak),
                               **(extra or {}))
        self.diag[name] = dict(ik_unreachable_frames=sum(1 for o in oks if not o), ik_unreachable_legs=sorted(badlegs))
        return self.clips[name]

    # ================================================================== clips
    def build_all(self):
        self.idle()
        self.walk_sideways("Walk_Sideways_L", +1)
        self.walk_sideways("Walk_Sideways_R", -1)
        self.walk_forward()
        self.turn("Turn_L", +90.0)
        self.turn("Turn_R", -90.0)
        self.swipe()
        self.slam()
        self.grab()
        self.grab_hold()
        self.throw()
        self.block()
        self.hit_react()
        self.stagger_flip()
        self.flipped_loop()
        self.recover()
        self.death()
        self.death_flipped()
        self.taunt()
        self.charge("Charge", +1)
        self.charge("Charge_R", -1)
        self.belly_slam()
        self.roar()
        self.leg_stomp()
        self.add_step_notifies()
        return self.clips

    # ------------------------------------------------------------------ Idle
    def idle(self):
        n = 90
        T = 2 * math.pi
        gape_l = Track([(0, 15), (20, 15), (28, 26), (36, 15), (90, 15)])
        gape_r = Track([(0, 15), (52, 15), (60, 24), (70, 15), (90, 15)])

        def pose(f):
            u = f / n
            ps = self.base_pose()
            sway = math.sin(T * u)
            ps["body"] = (0.02 * sway, 0.0, 0.010 * math.sin(T * 2 * u), 0.8 * math.sin(T * 2 * u + 0.6),
                          1.0 * sway, 0.6 * math.sin(T * u + 1.2))
            ps.update(self.idle_details(f, n))
            for s in SIDES:
                c = claw_preset("guard", s)
                k = 0.0 if s == "l" else 1.9
                c = claw_mod(c, d_wrist=(0.02 * math.sin(T * u + k), 0.03 * math.sin(T * 2 * u + k),
                                         0.025 * math.sin(T * u + k + 0.5)),
                             gape=(gape_l if s == "l" else gape_r)(f))
                ps["claws"][s] = c
            return ps
        self.record("Idle", n, pose, loop=True, category="Locomotion", peak=0.3,
                    extra=dict(speed_cm_s=0, blendspace=[0, 0]))

    # ------------------------------------------------------------------ marches
    def walk_sideways(self, name, sign):
        n, speed = 24, 1.0
        d = Vector((sign, 0, 0))
        T = 2 * math.pi

        def pose(f):
            u = f / n
            ps = self.base_pose()
            ps["feet"] = self.gait_feet(f, n, d, speed)
            ps["body"] = (0.0, 0.0, 0.01 * math.cos(T * 2 * u) - 0.01, 0.6 * math.sin(T * 2 * u),
                          2.0 * math.sin(T * u) * sign, 1.0 * math.sin(T * u))
            det = self.idle_details(f, n, 0.6)
            ps.update(det)
            for s in SIDES:
                lead = (SGN[s] == sign)
                c = claw_preset("guard", s)
                ps["claws"][s] = claw_mod(c, d_wrist=(0.06 * sign, 0.02 * math.sin(T * u), 0.03 * math.sin(T * 2 * u + (0 if lead else 1.5))),
                                          gape=15 + (6 if lead else 0) * (0.5 + 0.5 * math.sin(T * u)))
            return ps
        self.record(name, n, pose, loop=True, category="Locomotion", peak=0.25,
                    notifies=[("AN_Footstep_Heavy", 0), ("AN_Footstep_Heavy", 12)],
                    root_motion=f"en place ; {speed * n / FPS:.2f} m par cycle vers {'+X' if sign > 0 else '-X'} = 100 cm/s",
                    extra=dict(speed_cm_s=int(round(speed * 100)), direction="+X (gauche du crabe)" if sign > 0 else "-X",
                               blendspace=[90 if sign > 0 else -90, 100]))

    def walk_forward(self):
        n, speed = 36, 0.5
        d = Vector((0, -1, 0))
        T = 2 * math.pi

        def pose(f):
            u = f / n
            ps = self.base_pose()
            ps["feet"] = self.gait_feet(f, n, d, speed, lift=0.24)
            ps["body"] = (0.0, 0.0, 0.012 * math.cos(T * 2 * u) - 0.012, 1.2 * math.sin(T * 2 * u),
                          3.0 * math.sin(T * u), 3.0 * math.sin(T * u + 0.8))
            ps.update(self.idle_details(f, n, 0.6))
            for s in SIDES:
                c = claw_preset("guard", s)
                ps["claws"][s] = claw_mod(c, d_wrist=(0.0, -0.03 * math.sin(T * u + (0 if s == "l" else math.pi)),
                                                      0.04 * math.sin(T * 2 * u)))
            return ps
        self.record("Walk_Forward", n, pose, loop=True, category="Locomotion", peak=0.25,
                    notifies=[("AN_Footstep_Heavy", 0), ("AN_Footstep_Heavy", 18)],
                    root_motion="en place ; 0,60 m par cycle vers l'avant (-Y) = 50 cm/s",
                    extra=dict(speed_cm_s=50, direction="-Y (avant)", blendspace=[0, 50]))

    def turn(self, name, deg):
        n = 36
        yaw = Track([(0, 0.0), (n, deg)], ease)

        def VR(f):
            return self.root_matrix((0.0, 0.0, yaw(f)))
        steps = self.group_steps([(1, 8), (13, 20), (25, 32)], [(7, 14), (19, 26), (29, 35)])
        plan = self.planned_feet(n, VR, steps, lift=0.24, lead=5)

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (0.0, 0.0, yaw(f))
            ps["feet"] = plan[f]
            ps["body"] = (0, 0, -0.01 * math.sin(math.pi * f / n * 3) ** 2, 0.0, 1.5 * math.sin(math.pi * f / n * 3),
                          4.0 * math.sin(math.pi * f / n) * (1 if deg > 0 else -1))
            ps.update(self.idle_details(f, n, 0.5))
            for s in SIDES:
                ps["claws"][s] = claw_mod(claw_preset("guard", s), d_wrist=(0.05 * (1 if deg > 0 else -1), 0, 0))
            return ps
        self.record(name, n, pose, category="Locomotion", peak=0.5,
                    notifies=[("AN_Footstep_Heavy", 8), ("AN_Footstep_Heavy", 20), ("AN_Footstep_Heavy", 32)],
                    root_motion=f"lacet de {deg:+.0f}° sur root, sans translation (pivot au centre)",
                    extra=dict(root_motion_total=dict(translation_m=[0, 0, 0], yaw_deg=deg)))

    # ------------------------------------------------------------------ attaques
    def _forward_root(self, n, dist, f0, f1):
        tr = Track([(0, 0.0), (f0, 0.0), (f1, -dist), (n, -dist)], ease)
        return tr, (lambda f: self.root_matrix((0.0, tr(f), 0.0)))

    def swipe(self):
        n = 48
        tr, VR = self._forward_root(n, 0.25, 10, 24)
        plan = self.planned_feet(n, VR, self.group_steps([(10, 18)], [(18, 26)]), lift=0.18, lead=6)
        body = Track([(0, (0, 0, 0, 0, 0, 0)), (6, (0, 0, -0.02, 2, 0, 0)), (14, (0.10, 0.05, -0.15, -3, 6, 14)),
                      (18, (0.0, -0.05, -0.17, -5, 0, -4)), (25, (-0.10, -0.05, -0.15, -4, -5, -16)),
                      (34, (-0.04, 0, -0.08, 0, -2, -6)), (48, (0, 0, 0, 0, 0, 0))])

        def arc(a_deg, z=0.70, r=1.22):
            a = math.radians(a_deg)
            sh = self.shoulder["l"]
            return dict(wrist=Vector((sh.x + r * math.cos(a) * 0.95, sh.y - r * math.sin(a), z)),
                        pole=Vector((0.3, 0.6, 1.0)), hand=Vector((math.cos(a), -math.sin(a), -0.22)),
                        up=Vector((0, 0, 1)), gape=40.0, space="body")
        g = claw_preset("guard", "l")
        wind = dict(wrist=Vector((2.05, -0.75, 1.15)), pole=Vector((0.2, 1.0, 0.6)), hand=Vector((0.85, 0.25, 0.1)),
                    up=Vector((0, 0, 1)), gape=45.0, space="body")
        keys = [(0, g), (6, g), (14, wind), (16, arc(-5)), (19, arc(55)), (22, arc(110)), (25, arc(160)),
                (32, claw_mod(arc(150), gape=10.0)), (40, claw_mod(g, gape=12.0)), (48, g)]
        clL = ClawTrack(keys)
        clR = ClawTrack([(0, claw_preset("guard", "r")), (8, claw_preset("tuck", "r")), (30, claw_preset("tuck", "r")),
                         (44, claw_preset("guard", "r")), (48, claw_preset("guard", "r"))])
        snap = Track([(0, 0.0), (19, 0.0), (21, 1.0), (24, 1.0), (30, 0.0)])

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (0.0, tr(f), 0.0)
            ps["feet"] = plan[f]
            ps["body"] = body(f)
            ps.update(self.idle_details(f, n, 0.8))
            c = clL(f)
            c = claw_mod(c, gape=c["gape"] * (1 - 0.8 * snap(f)))
            ps["claws"] = {"l": c, "r": clR(f)}
            ps["eyes"] = {s: (0.0, -6.0, -8.0 * SGN[s], 0.0, 0.0) for s in SIDES}
            return ps
        self.record("Attack_ClawSwipe", n, pose, peak=20,
                    notifies=[("AN_Attack_Windup", 6), ("AN_HitWindow_Begin", 16), ("AN_Claw_Whoosh", 17),
                              ("AN_HitWindow_End", 25)],
                    root_motion="0,25 m vers l'avant (-Y), images 10 à 24",
                    extra=dict(root_motion_total=dict(translation_m=[0, -0.25, 0], yaw_deg=0)))

    def slam(self):
        n = 60
        tr, VR = self._forward_root(n, 0.40, 22, 32)
        plan = self.planned_feet(n, VR, self.group_steps([(22, 30)], [(46, 54)]), lift=0.2, lead=4)
        body = Track([(0, (0, 0, 0, 0, 0, 0)), (10, (0, 0.08, 0.10, 9, -3, -4)), (18, (0, 0.12, 0.14, 12, -4, -6)),
                      (26, (0, 0.12, 0.15, 13, -4, -6)), (31, (0, -0.10, -0.26, -9, -6, -3)),
                      (44, (0, -0.06, -0.22, -7, -5, -2)), (52, (0, 0, -0.08, -2, -1, 0)), (60, (0, 0, 0, 0, 0, 0))])
        g = claw_preset("guard", "r")
        up_pose = dict(wrist=Vector((-1.05, -1.45, 2.05)), pole=Vector((-1.0, 0.4, -0.3)),
                       hand=Vector((0.05, -0.30, 0.95)), up=Vector((0.0, -0.95, -0.25)), gape=40.0, space="body")
        # point d'impact (repère de départ du clip) : pointe des doigts dans le sable
        tip_w = Vector((-0.62, -3.30, -0.06))
        d = Vector((0.05, -0.42, -0.905)).normalized()
        c = self.P.claw["r"]
        reach = c["palm"]["length_m"] + c["fixed_finger_m"]
        wrist_w = tip_w - d * reach

        def strike(f_root_y, gape):
            return dict(wrist=wrist_w.copy(), pole=Vector((-1.0, 0.3, 0.6)), hand=d.copy(), up=Vector((0, -0.9, 0.42)),
                        gape=gape, space="world")
        hit = strike(-0.40, 4.0)
        # le poignet est donné en repère root (world du clip moins la root motion) : le solveur le reconvertit
        keys = [(0, g), (10, claw_mod(up_pose, gape=20.0)), (18, up_pose), (22, claw_mod(up_pose, d_wrist=(0, 0.02, 0.03))),
                (26, claw_mod(up_pose, d_wrist=(0, 0.05, 0.06), gape=45.0)), (31, hit), (37, hit),
                (41, hit), (44, hit), (49, claw_mod(up_pose, d_wrist=(0.1, -0.35, -0.7), gape=15.0)), (60, g)]
        clR = ClawTrack(keys)
        trem = Track([(0, 0.0), (17, 0.0), (19, 1.0), (26, 1.0), (27, 0.0), (32, 0.0), (33, 1.0), (43, 1.0), (45, 0.0)])
        clL = ClawTrack([(0, claw_preset("guard", "l")), (12, claw_preset("spread", "l")), (36, claw_preset("spread", "l")),
                         (54, claw_preset("guard", "l")), (60, claw_preset("guard", "l"))])

        def pose(f):
            ps = self.base_pose()
            ry = tr(f)
            ps["root"] = (0.0, ry, 0.0)
            ps["feet"] = plan[f]
            b = body(f)
            k = trem(f)
            ps["body"] = (b[0] + 0.012 * k * math.sin(f * 2.7), b[1], b[2] + 0.01 * k * math.sin(f * 3.3), b[3], b[4], b[5])
            ps.update(self.idle_details(f, n, 0.7))
            cr = clR(f)
            if cr.get("space") == "world":
                cr = dict(cr, wrist=cr["wrist"] + Vector((0, ry, 0)) - Vector((0, ry, 0)))
            # extraction : petites secousses pendant que la pince est plantée
            if 33 <= f <= 43 and cr.get("space") == "world":
                cr = dict(cr, wrist=cr["wrist"] + Vector((0.015 * math.sin(f * 2.1), 0, 0.02 * abs(math.sin(f * 1.7)))))
            ps["claws"] = {"l": clL(f), "r": self._root_rel(cr, ry)}
            ps["eyes"] = {s: (0.0, -10.0 * (1 - k * 0.5), 0.0, 0.0, 0.0) for s in SIDES}
            return ps
        self.record("Attack_ClawSlam", n, pose, peak=31,
                    notifies=[("AN_Attack_Windup", 10), ("AN_HitWindow_Begin", 28), ("AN_GroundPound_Impact", 31),
                              ("AN_ClawStuck_Begin", 31), ("AN_HitWindow_End", 32), ("AN_ClawStuck_End", 44)],
                    root_motion="0,40 m vers l'avant (-Y), images 22 à 32",
                    extra=dict(root_motion_total=dict(translation_m=[0, -0.40, 0], yaw_deg=0),
                               impact_tip_clip_space_m=[round(v, 3) for v in tip_w]))

    def _root_rel(self, c, ry):
        """Pince donnée en repère du clip (world) : le solveur travaille en espace armature = world (root motion
        incluse dans la matrice du corps), rien à corriger ; fonction gardée pour la lisibilité."""
        return c

    def grab_target(self):
        """Point de saisie (SOCKET_ClawGrab_R) dans le repère de départ du clip, à l'image 20 : taille de Bean."""
        return Vector((-0.30, -2.98, 0.50))

    def grab_claw(self, socket_w, dir_w, gape, lift_up=None):
        """Poignet de la pince droite pour placer SOCKET_ClawGrab_R en socket_w, pince roulée (ouverture horizontale,
        dactyle vers le centre : axe X de la main vers le haut)."""
        c = self.P.claw["r"]
        Lp, H, Lf = c["palm"]["length_m"], c["palm"]["height_m"], c["fixed_finger_m"]
        u = 0.6
        fz0 = -0.20 * H
        z_ff = fz0 + 0.16 * H * u ** 1.6 + 0.05 * H * math.sin(u * math.pi) + 0.30 * H * (1 - u) ** 0.75
        ys, zs = Lp - 0.10 + u * (Lf + 0.10), z_ff + 0.12 * H
        d = Vector(dir_w).normalized()
        upz = Vector((1.0, 0.0, 0.0)) if lift_up is None else Vector(lift_up)
        zh = (upz - d * upz.dot(d)).normalized()
        wrist = Vector(socket_w) - d * ys - zh * zs
        return dict(wrist=wrist, pole=Vector((-1.0, 0.6, 0.4)), hand=d, up=zh, gape=gape, space="world")

    def grab(self):
        n = 54
        tr, VR = self._forward_root(n, 0.60, 8, 20)
        plan = self.planned_feet(n, VR, self.group_steps([(9, 16)], [(14, 21)]), lift=0.16, lead=5)
        body = Track([(0, (0, 0, 0, 0, 0, 0)), (6, (0, 0.04, -0.12, 4, 0, 0)), (14, (0, -0.04, -0.45, -6, -3, -3)),
                      (20, (0, -0.06, -0.45, -8, -4, -4)), (24, (0, -0.04, -0.42, -6, -3, -3)),
                      (36, (0, 0, -0.30, -2, -1, 0)), (54, (0, 0, 0, 0, 0, 0))])
        S = self.grab_target()
        open_c = self.grab_claw(S + Vector((-0.05, 0.75, 0.15)), (0.10, -1.0, -0.05), 55.0)
        reach_c = self.grab_claw(S + Vector((0.0, 0.08, 0.02)), (0.12, -1.0, -0.06), 55.0)
        at_c = self.grab_claw(S, (0.12, -1.0, -0.06), 55.0)
        closed = claw_mod(at_c, gape=18.0)
        g = claw_preset("guard", "r")
        miss_c = claw_mod(self.grab_claw(S + Vector((0.0, 0.25, 0.25)), (0.1, -1.0, 0.05), 0.0))
        clR = ClawTrack([(0, g), (6, claw_mod(g, gape=35.0)), (14, open_c), (19, reach_c), (20, at_c), (21, closed),
                         (24, closed), (32, miss_c), (44, claw_mod(g, gape=8.0)), (54, g)])
        clL = ClawTrack([(0, claw_preset("guard", "l")), (10, claw_preset("spread", "l")), (30, claw_preset("spread", "l")),
                         (50, claw_preset("guard", "l")), (54, claw_preset("guard", "l"))])

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (0.0, tr(f), 0.0)
            ps["feet"] = plan[f]
            ps["body"] = body(f)
            ps.update(self.idle_details(f, n, 0.8))
            ps["claws"] = {"l": clL(f), "r": clR(f)}
            ps["mouth"] = 6.0 + 8.0 * ease((f - 14) / 8.0) * (1 - ease((f - 30) / 10.0))
            return ps
        self.record("Attack_Grab", n, pose, peak=20,
                    notifies=[("AN_Attack_Windup", 6), ("AN_Grab_Check", 20), ("AN_Grab_Attach", 21)],
                    root_motion="fente de 0,60 m vers l'avant (-Y), images 8 à 20",
                    extra=dict(root_motion_total=dict(translation_m=[0, -0.60, 0], yaw_deg=0),
                               montage_sections={"Grab": [0, 24], "Miss": [24, 54]},
                               grab_socket_clip_space_m=[round(v, 3) for v in S], grab_frame=21))

    def hold_socket(self, f, n=30):
        u = f / n
        return Vector((-0.28 + 0.02 * math.sin(2 * math.pi * u), -2.12, 1.60 + 0.03 * math.sin(2 * math.pi * 2 * u)))

    def grab_hold(self):
        n = 30
        T = 2 * math.pi
        squeeze = Track([(0, 18.0), (12, 18.0), (15, 11.0), (19, 18.0), (30, 18.0)])

        def pose(f):
            u = f / n
            ps = self.base_pose()
            ps["body"] = (0.0, -0.02, 0.04 + 0.01 * math.sin(T * u), 3.0 + 1.0 * math.sin(T * u), -1.5, -2.0)
            det = self.idle_details(f, n, 1.0)
            ps.update(det)
            ps["mouth"] = 8.0 + 6.0 * math.sin(T * 2 * u)
            ps["mand"] = 10.0 * math.sin(T * 3 * u)
            S = self.hold_socket(f, n)
            ps["claws"] = {"r": self.grab_claw(S, (0.25, -0.95, 0.12), squeeze(f)),
                           "l": claw_mod(claw_preset("spread", "l"), d_wrist=(0, -0.05, 0.1 + 0.03 * math.sin(T * u)))}
            ps["eyes"] = {s: (0.0, -8.0, -6.0 * SGN[s], 0.0, 0.0) for s in SIDES}
            return ps
        self.record("Grab_Hold", n, pose, loop=True, peak=15, notifies=[("AN_Squeeze_Tick", 15)],
                    extra=dict(hold_socket_m=[round(v, 3) for v in self.hold_socket(0)]))

    def throw(self):
        n = 48
        yaw = Track([(0, 0.0), (16, -30.0), (19, -30.0), (26, 4.0), (34, 0.0), (48, 0.0)])
        S0 = self.hold_socket(0)
        sock = Track([(0, S0), (16, Vector((-1.15, -1.55, 2.6))), (19, Vector((-1.2, -1.45, 2.62))),
                      (26, Vector((-0.35, -2.55, 2.35))), (30, Vector((0.05, -2.6, 1.9)))])
        gape = Track([(0, 18.0), (25, 18.0), (26, 52.0), (28, 52.0), (30, 0.0), (36, 4.0), (48, 15.0)])
        body = Track([(0, (0.0, -0.02, 0.04, 3.0, -1.5, -2.0)), (16, (0.05, 0.05, 0.12, 9.0, -6.0, -6.0)),
                      (24, (-0.02, -0.12, 0.02, -4.0, 4.0, 6.0)), (30, (0, -0.08, -0.04, -5.0, 2.0, 2.0)),
                      (48, (0, 0, 0, 0, 0, 0))])
        g = claw_preset("guard", "r")
        dirs = Track([(0, Vector((0.25, -0.95, 0.12))), (16, Vector((-0.2, -0.3, 0.93))), (19, Vector((-0.25, -0.2, 0.95))),
                      (26, Vector((0.2, -0.85, 0.5))), (30, Vector((0.3, -0.9, -0.1)))])

        def VR(f):
            return self.root_matrix((0.0, 0.0, yaw(f)))
        plan = self.planned_feet(n, VR, self.group_steps([(3, 10), (27, 34)], [(9, 16), (33, 40)]), lift=0.14, lead=4)
        # les pieds restent plantés pendant la torsion : planned_feet garde la position de départ jusqu'au 1er pas

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (0.0, 0.0, yaw(f))
            ps["feet"] = plan[f]
            ps["body"] = body(f)
            ps.update(self.idle_details(f, n, 0.8))
            ps["mouth"] = 10.0 * (1 - ease((f - 26) / 10))
            if f <= 30:
                R = self.root_matrix((0.0, 0.0, yaw(f)))
                sw = R @ sock(f)
                dw = R.to_3x3() @ dirs(f)
                ps["claws"]["r"] = self.grab_claw(sw, dw, gape(f), lift_up=R.to_3x3() @ Vector((1, 0, 0)))
            else:
                R = self.root_matrix((0.0, 0.0, yaw(f)))
                last = self.grab_claw(R @ sock(30), R.to_3x3() @ dirs(30), gape(f),
                                      lift_up=R.to_3x3() @ Vector((1, 0, 0)))
                ps["claws"]["r"] = claw_lerp(last, g, ease((f - 30) / 18.0))
                ps["claws"]["r"]["gape"] = gape(f)
            ps["claws"]["l"] = ClawTrack([(0, claw_preset("spread", "l")), (16, claw_preset("raised", "l")),
                                          (28, claw_preset("spread", "l")), (48, claw_preset("guard", "l"))])(f)
            return ps
        self.record("Throw", n, pose, peak=26, notifies=[("AN_Throw_Release", 26), ("AN_Claw_Snap", 30)],
                    root_motion="lacet de -30° (images 0 à 16) puis retour à 0° (26 à 34)",
                    extra=dict(root_motion_total=dict(translation_m=[0, 0, 0], yaw_deg=0),
                               release=dict(frame=26, speed_m_s=6.0, pitch_deg=35, distance_m=4.5)))

    def block(self):
        low = (0, 0, -0.45, -4, 0, 0)

        def blk(s):
            c = claw_preset("block", s)
            if s == "r":
                c = claw_mod(c, d_wrist=(0, 0.10, -0.06))
            return c

        def mk(name, n, frm, to, notif, loop=False):
            bt = Track([(0, frm[0]), (n, to[0])])
            ct = {s: ClawTrack([(0, frm[1][s]), (n, to[1][s])]) for s in SIDES}
            et = Track([(0, frm[2]), (n, to[2])])
            T = 2 * math.pi

            def pose(f):
                ps = self.base_pose()
                b = bt(f)
                if loop:
                    u = f / n
                    b = (b[0], b[1], b[2] + 0.012 * math.sin(T * u), b[3] + 0.8 * math.sin(T * u), b[4], b[5])
                ps["body"] = b
                ps.update(self.idle_details(f, n, 0.5))
                ps["claws"] = {s: ct[s](f) for s in SIDES}
                if loop:
                    u = f / n
                    for s in SIDES:
                        ps["claws"][s] = claw_mod(ps["claws"][s], d_wrist=(0, 0, 0.01 * math.sin(T * u + SGN[s])))
                r = et(f)
                ps["eyes"] = {s: (r, 0.0, 0.0, 0.0, 0.0) for s in SIDES}
                return ps
            self.record(name, n, pose, loop=loop, peak=n if not loop else 0.5, notifies=notif,
                        category="Defense")
        guard = ((0, 0, 0, 0, 0, 0), {s: claw_preset("guard", s) for s in SIDES}, 0.0)
        blocked = (low, {s: blk(s) for s in SIDES}, 1.0)
        mk("Block_Start", 12, guard, blocked, [("AN_BlockWindow_Begin", 6)])
        mk("Block_Loop", 30, blocked, blocked, [], loop=True)
        mk("Block_End", 12, blocked, guard, [("AN_BlockWindow_End", 4)])

    def hit_react(self):
        n = 24
        tr = Track([(0, 0.0), (5, 0.12), (14, 0.20), (24, 0.20)])

        def VR(f):
            return self.root_matrix((0.0, tr(f), 0.0))
        plan = self.planned_feet(n, VR, self.group_steps([(4, 11)], [(9, 16)]), lift=0.14, lead=6)
        body = Track([(0, (0, 0, 0, 0, 0, 0)), (3, (0, 0.10, 0.06, 12, 5, 4)), (8, (0, 0.06, 0.02, 6, -3, -2)),
                      (16, (0, 0.01, -0.02, -1, 0, 0)), (24, (0, 0, 0, 0, 0, 0))])
        eye = Track([(0, 0.0), (2, 1.0), (10, 1.0), (20, 0.0), (24, 0.0)])
        cl = {s: ClawTrack([(0, claw_preset("guard", s)), (4, claw_preset("spread", s)), (12, claw_preset("spread", s)),
                            (24, claw_preset("guard", s))]) for s in SIDES}

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (0.0, tr(f), 0.0)
            ps["feet"] = plan[f]
            ps["body"] = body(f)
            ps.update(self.idle_details(f, n, 1.2))
            ps["eyes"] = {s: (eye(f), 0.0, 0.0, 0.0, 0.0) for s in SIDES}
            ps["claws"] = {s: cl[s](f) for s in SIDES}
            ps["mouth"] = 14.0 * math.sin(math.pi * min(f / 12.0, 1.0))
            return ps
        self.record("Hit_React", n, pose, peak=3, category="Reaction", notifies=[("AN_HitFlash", 1)],
                    root_motion="recul de 0,20 m (+Y)",
                    extra=dict(root_motion_total=dict(translation_m=[0, 0.20, 0], yaw_deg=0)))

    # ------------------------------------------------------------------ renversement
    def flipped_body(self, f=0, n=60, amp=1.0):
        T = 2 * math.pi
        u = f / n
        # dos au sol : pivot à 0,42 m (le haut du dos, 0,35 au-dessus du pivot, touche le sable)
        return (0.0, 0.0, 0.42 - self.pivot.z + 0.015 * amp * math.sin(T * 2 * u), 1.5 * amp * math.sin(T * u),
                -180.0 + 5.0 * amp * math.sin(T * u + 0.4), 2.0 * amp * math.sin(T * u + 1.1))

    def flail_feet(self, f, n, amp=1.0, fold=0.0):
        """Pieds en espace corps (pattes en l'air quand le crabe est sur le dos)."""
        T = 2 * math.pi
        out = {}
        for i, (s, nn) in enumerate(LEGS):
            r = self.feet0[(s, nn)]
            ph = i * 1.37
            u = f / n
            base = Vector((r.x * (0.78 - 0.45 * fold), r.y * (0.85 - 0.4 * fold), 0.55 + 0.55 * fold))
            wig = Vector((0.12 * math.sin(T * 2 * u + ph), 0.18 * math.sin(T * 2 * u + ph + 1.0),
                          0.22 * math.sin(T * 3 * u + ph))) * amp * (1 - fold * 0.8)
            out[(s, nn)] = (base + wig, -60.0 - 20 * fold, "body")
        return out

    def stagger_flip(self):
        n = 66
        lat = Track([(0, 0.0), (10, 0.05), (30, 0.7), (40, 1.0), (66, 1.0)])

        def VR(f):
            return self.root_matrix((lat(f), 0.0, 0.0))
        body = Track([(0, (0, 0, 0, 0, 0, 0)), (6, (0.0, 0.05, 0.10, 14, 0, 0)), (12, (0.0, 0.08, 0.25, 26, -8, 4)),
                      (20, (-0.15, 0.05, 0.10, 20, -60, 6)), (30, (-0.10, 0.0, -0.50, 8, -130, 4)),
                      (40, self.flipped_body(0, amp=0.0)[:4] + (-180.0, 0.0)),
                      (45, (0, 0, 0.42 - self.pivot.z + 0.12, 2.0, -184.0, 1.0)),
                      (52, (0, 0, 0.42 - self.pivot.z, 0.0, -179.0, 0.0)),
                      (66, self.flipped_body(0))])
        # pieds : au sol jusqu'à 12, puis en l'air (espace corps)
        steps = self.planned_feet(n, VR, {}, lift=0.0)
        g = {s: claw_preset("guard", s) for s in SIDES}
        sp = {s: claw_preset("spread", s) for s in SIDES}
        rz = {s: claw_preset("raised", s) for s in SIDES}

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (lat(f), 0.0, 0.0)
            b = body(f)
            ps["body"] = b
            ps.update(self.idle_details(f, n, 1.4))
            w = ease((f - 10) / 14.0)
            fl = self.flail_feet(f, 60, amp=1.0 + 0.5 * (f > 40))
            feet = {}
            M = self.body_matrix(self.root_matrix(ps["root"]), b)
            for k in LEGS:
                pw, dw, _ = steps[f][k]
                pb, db, _ = fl[k]
                if w <= 0:
                    feet[k] = (pw, dw, "world")
                elif w >= 1:
                    feet[k] = (pb, db, "body")
                else:
                    pwb = M.inverted() @ pw
                    feet[k] = (pwb.lerp(pb, w), dw + (db - dw) * w, "body")
            ps["feet"] = feet
            cw = ease((f - 4) / 10.0)
            for s in SIDES:
                c = claw_lerp(g[s], rz[s], cw) if f < 22 else claw_lerp(rz[s], sp[s], ease((f - 22) / 18.0))
                ps["claws"][s] = c
            ps["eyes"] = {s: (ease((f - 8) / 6.0), 0.0, 0.0, 0.0, 0.0) for s in SIDES}
            ps["abd"] = 12.0 * ease((f - 40) / 20.0)
            return ps
        self.record("Stagger_Flip", n, pose, peak=40, category="Reaction",
                    notifies=[("AN_Stagger_Stumble", 8), ("AN_BodyFall", 40), ("AN_CameraShake", 40)],
                    root_motion="1,0 m latéral (+X) ; la bascule de 180° en roulis est portée par l'os body (root reste à plat)",
                    extra=dict(root_motion_total=dict(translation_m=[1.0, 0, 0], yaw_deg=0)))

    def flipped_claw(self, s, f, n):
        T = 2 * math.pi
        u = f / n
        # pinces qui griffent le sable de chaque côté (repère monde du clip)
        k = 0.0 if s == "l" else math.pi
        x = SGN[s] * (1.45 + 0.1 * math.sin(T * u + k))
        y = -1.55 + 0.35 * math.sin(T * 2 * u + k)
        z = 0.62 + 0.18 * max(0.0, math.sin(T * 2 * u + k + 1.2))
        return dict(wrist=Vector((x, y, z)), pole=Vector((SGN[s] * 1.0, 0.0, 1.0)),
                    hand=Vector((SGN[s] * 0.5, -0.80, -0.22)), up=Vector((0.0, 0.0, -1.0)),
                    gape=20.0 + 20.0 * max(0.0, math.sin(T * 2 * u + k)), space="world")

    def flipped_loop(self):
        n = 60

        def pose(f):
            ps = self.base_pose()
            ps["body"] = self.flipped_body(f, n)
            ps.update(self.idle_details(f, n, 1.5))
            ps["feet"] = self.flail_feet(f, n, amp=1.4)
            ps["claws"] = {s: self.flipped_claw(s, f, n) for s in SIDES}
            ps["eyes"] = {s: (1.0, 0.0, 0.0, 0.0, 0.0) for s in SIDES}
            ps["abd"] = 12.0 + 4.0 * math.sin(2 * math.pi * f / n)
            return ps
        self.record("Flipped_Loop", n, pose, loop=True, peak=0.5, category="Reaction",
                    extra=dict(belly_height_m=0.78, weakness_duration_s=5.0))

    def recover(self):
        n = 60
        lat = Track([(0, 0.0), (20, 0.0), (44, -0.4), (60, -0.4)])

        def VR(f):
            return self.root_matrix((lat(f), 0.0, 0.0))
        fb = self.flipped_body(0)
        body = Track([(0, fb), (14, (-0.1, 0, fb[2] + 0.05, 0, -170.0, 0)), (24, (-0.25, 0, fb[2] + 0.35, 6, -120.0, 2)),
                      (34, (-0.15, 0, -0.55, 8, -50.0, 2)), (44, (0, 0, -0.18, 4, -4.0, 0)), (56, (0, 0, 0.02, 0, 0, 0)),
                      (60, (0, 0, 0, 0, 0, 0))])
        land = self.planned_feet(n, VR, {}, lift=0.0)
        endfeet = {k: VR(n) @ Vector((self.feet0[k].x, self.feet0[k].y, 0.0)) for k in LEGS}

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (lat(f), 0.0, 0.0)
            b = body(f)
            ps["body"] = b
            ps.update(self.idle_details(f, n, 1.0))
            M = self.body_matrix(self.root_matrix(ps["root"]), b)
            fl = self.flail_feet(f, 60, amp=max(0.0, 1.4 - f / 20.0))
            w = ease((f - 30) / 14.0)
            feet = {}
            for k in LEGS:
                pb, db, _ = fl[k]
                pw = endfeet[k]
                if w <= 0:
                    feet[k] = (pb, db, "body")
                elif w >= 1:
                    feet[k] = (pw, -78.0, "world")
                else:
                    feet[k] = ((M.inverted() @ pw).lerp(pb, 1 - w), db + (-78.0 - db) * w, "body")
            ps["feet"] = feet
            for s in SIDES:
                push = dict(wrist=Vector((SGN[s] * 1.25 + (0.6 if s == "r" else 0.0), -1.4, 0.15)),
                            pole=Vector((SGN[s], 0.0, 1.0)), hand=Vector((SGN[s] * 0.3, -0.8, -0.5)),
                            up=Vector((0, 0, 1)), gape=10.0, space="world")
                a = self.flipped_claw(s, f, 60)
                if f < 14:
                    c = claw_lerp(a, push, ease(f / 14.0))
                elif f < 34:
                    c = push
                else:
                    c = claw_lerp(push, claw_preset("guard", s), ease((f - 34) / 22.0))
                ps["claws"][s] = c
            ps["eyes"] = {s: (1.0 - ease((f - 40) / 14.0), 0.0, 0.0, 0.0, 0.0) for s in SIDES}
            ps["abd"] = 12.0 * (1 - ease(f / 30.0))
            return ps
        self.record("Recover", n, pose, peak=30, category="Reaction", notifies=[("AN_Land", 44), ("AN_StaggerEnd", 56)],
                    root_motion="0,40 m latéral (-X, retour)",
                    extra=dict(root_motion_total=dict(translation_m=[-0.4, 0, 0], yaw_deg=0)))

    # ------------------------------------------------------------------ morts
    def death(self):
        n = 90
        tr = Track([(0, 0.0), (20, 0.15), (34, 0.30), (90, 0.30)])
        ground = 0.40 - self.pivot.z
        body = Track([(0, (0, 0, 0, 0, 0, 0)), (8, (0, 0.03, 0.04, 6, 2, 0)), (20, (0.05, 0, -0.45, -3, 6, 4)),
                      (34, (0.08, 0, ground, -6, 4, 5)), (38, (0.08, 0, ground + 0.03, -5, 4, 5)),
                      (44, (0.08, 0, ground, -6, 4, 5)), (90, (0.08, 0, ground, -6, 4, 5))])
        twitch = Track([(0, 0.0), (58, 0.0), (60, 1.0), (63, 0.0), (68, 0.6), (71, 0.0)])
        dead = Track([(0, 0.0), (30, 0.0), (50, 1.0), (90, 1.0)])

        def splay(k, w):
            r = self.feet0[k]
            return Vector((r.x * (1.0 + 0.16 * w), r.y * (1.0 + 0.10 * w), 0.0))

        def pose(f):
            ps = self.base_pose()
            ps["root"] = (0.0, tr(f), 0.0)
            ps["body"] = body(f)
            R = self.root_matrix(ps["root"])
            w = ease((f - 10) / 26.0)
            tw = twitch(f)
            feet = {}
            for i, k in enumerate(LEGS):
                p = R @ splay(k, w)
                if tw > 0 and i % 3 == 0:
                    p = p + Vector((0, 0, 0.12 * tw))
                feet[k] = (p, -78.0 + 40.0 * w, "world")
            ps["feet"] = feet
            ps.update(self.idle_details(f, n, max(0.0, 1.0 - f / 40.0)))
            for s in SIDES:
                gnd = dict(wrist=Vector((SGN[s] * 1.55, -1.85, 0.48)), pole=Vector((SGN[s], 0.2, 1.0)),
                           hand=Vector((SGN[s] * 0.35, -0.85, -0.35)), up=Vector((0, 0, 1)), gape=25.0, space="world")
                gnd["wrist"] = R @ gnd["wrist"]
                c = claw_lerp(claw_preset("guard", s), claw_preset("spread", s), ease(f / 14.0)) if f < 14 else \
                    claw_lerp(claw_preset("spread", s), gnd, ease((f - 14) / 22.0))
                if s == "r" and tw > 0:
                    c = claw_mod(c, gape=c["gape"] + 15 * tw)
                ps["claws"][s] = c
            droop = ease((f - 30) / 30.0)
            ps["eyes"] = {s: (0.35 * droop, 50.0 * droop, 0.0, 20.0 * droop, 0.0) for s in SIDES}
            ps["dead"] = dead(f)
            return ps
        self.record("Death", n, pose, peak=50, category="Death",
                    notifies=[("AN_DeathStart", 0), ("AN_BodyFall", 34), ("AN_DeadEyes", 40), ("AN_DeathEnd", 80)],
                    root_motion="0,30 m vers l'arrière (+Y)",
                    extra=dict(root_motion_total=dict(translation_m=[0, 0.30, 0], yaw_deg=0),
                               curves={"DeadEyes": [[0, 0.0], [30, 0.0], [50, 1.0], [90, 1.0]]}))

    def death_flipped(self):
        n = 60
        dead = Track([(0, 0.0), (8, 0.0), (20, 1.0), (60, 1.0)])

        def pose(f):
            ps = self.base_pose()
            w = ease(f / 40.0)
            fb = self.flipped_body(f, 60, amp=1.0 - w)
            ps["body"] = (fb[0], fb[1], fb[2] - 0.02 * w, fb[3], fb[4], fb[5])
            ps.update(self.idle_details(f, n, max(0.0, 1.0 - f / 20.0)))
            ps["feet"] = self.flail_feet(f, 60, amp=1.4 * (1 - w), fold=w)
            for s in SIDES:
                a = self.flipped_claw(s, f, 60)
                folded = dict(wrist=Vector((SGN[s] * 0.75, -1.5, 1.55)), pole=Vector((SGN[s], 0.3, -0.5)),
                              hand=Vector((-SGN[s] * 0.8, -0.2, -0.55)), up=Vector((0, -1, 0)), gape=5.0, space="body")
                ps["claws"][s] = claw_lerp(a, folded, ease(f / 34.0)) if f < 34 else folded
            ps["eyes"] = {s: (1.0 - 0.6 * w, 30.0 * w, 0.0, 15.0 * w, 0.0) for s in SIDES}
            ps["abd"] = 12.0 * (1 - w) + 4.0
            ps["dead"] = dead(f)
            return ps
        self.record("Death_Flipped", n, pose, peak=40, category="Death",
                    notifies=[("AN_DeathStart", 0), ("AN_DeadEyes", 20), ("AN_DeathEnd", 55)],
                    extra=dict(curves={"DeadEyes": [[0, 0.0], [8, 0.0], [20, 1.0], [60, 1.0]]}))

    def taunt(self):
        n = 60
        body = Track([(0, (0, 0, 0, 0, 0, 0)), (12, (0, 0.05, 0.20, 12, 0, 0)), (46, (0, 0.05, 0.20, 12, 0, 0)),
                      (60, (0, 0, 0, 0, 0, 0))])
        clack = Track([(0, 0.0), (16, 0.0), (18, 1.0), (21, 0.0), (24, 0.0), (26, 1.0), (29, 0.0), (32, 0.0),
                       (34, 1.0), (37, 0.0), (40, 0.0), (42, 1.0), (45, 0.0)])

        def pose(f):
            ps = self.base_pose()
            b = body(f)
            ps["body"] = (b[0], b[1], b[2] + 0.02 * clack(f), b[3], b[4] + 2.0 * math.sin(f * 0.4) * (12 < f < 46), b[5])
            ps.update(self.idle_details(f, n, 1.3))
            w = ease(f / 12.0) * (1 - ease((f - 46) / 14.0))
            for s in SIDES:
                c = claw_lerp(claw_preset("guard", s), claw_preset("raised", s), w)
                c = claw_mod(c, gape=(55.0 - 55.0 * clack(f)) * w + 15.0 * (1 - w),
                             d_wrist=(0, 0, 0.06 * clack(f) * w))
                ps["claws"][s] = c
            ps["eyes"] = {s: (0.0, -15.0 * w, 10.0 * SGN[s] * w, 0.0, 0.0) for s in SIDES}
            ps["mouth"] = 6.0 + 10.0 * w
            return ps
        self.record("Taunt_ClawClack", n, pose, peak=18, category="Combat",
                    notifies=[("AN_Claw_Snap", 18), ("AN_Claw_Snap", 26), ("AN_Claw_Snap", 34), ("AN_Claw_Snap", 42)],
                    extra=dict(optional=True))

    # ================================================================== clips v2 (T-260)
    def splay_target(self, key, factor, drop):
        """Pied écarté radialement depuis la coxa, au sol, limité à 90 % de la portée quand le corps est descendu de
        drop (m)."""
        cx = self.coxa[key]
        foot = self.feet0[key]
        d = Vector((foot.x - cx.x, foot.y - cx.y, 0.0))
        L = self.L[key]
        reach = L["coxa"] + L["merus"] + L["propodus"] + 0.55 * L["dactyl"]
        h = max(cx.z - drop, 0.05)
        horiz = math.sqrt(max((0.90 * reach) ** 2 - h * h, 0.1))
        return Vector((cx.x, cx.y, 0.0)) + d.normalized() * min(d.length * factor, horiz)

    def gait_feet_v2(self, f, n, direction, speed, duty, lift, pull=1.0):
        """Cycle en place (comme gait_feet) avec pieds de repos rapprochés de la coxa (pull < 1 : marge d'extension
        pour une foulée longue) et pointes rasantes en vol."""
        S = speed * duty * n / FPS
        out = {}
        groups = self.P.groups
        for s, nn in LEGS:
            ph = 0.0 if leg_name(s, nn) in groups["A"] else 0.5
            c = (f / n - ph) % 1.0
            cx = self.coxa[(s, nn)]
            r = self.feet0[(s, nn)]
            rest = Vector((cx.x + (r.x - cx.x) * pull, cx.y + (r.y - cx.y) * pull, 0.0))
            if c < duty - 1e-9:
                off = S * (0.5 - c / duty)
                z = 0.0
                dact = -78.0
            else:
                u = (c - duty) / (1 - duty)
                off = S * (-0.5 + ease2(u))
                z = lift * math.sin(math.pi * u) ** 0.7
                dact = -78.0 + 22.0 * math.sin(math.pi * u)
            out[(s, nn)] = (rest + direction * off + Vector((0, 0, z)), dact, "world")
        return out

    def charge(self, name, sign):
        """Course latérale rapide, corps bas, en place (vitesse sans glissement : speed_cm_s du manifeste)."""
        n, speed, duty, lift, low = 12, 4.5, 0.5, 0.32, -0.30
        d = Vector((sign, 0, 0))
        T = 2 * math.pi

        def pose(f):
            u = f / n
            ps = self.base_pose()
            ps["feet"] = self.gait_feet_v2(f, n, d, speed, duty, lift, pull=0.86)
            ps["body"] = (0.0, 0.0, low + 0.035 * math.cos(T * 2 * u), 1.5 * math.sin(T * 2 * u),
                          4.0 * sign + 1.5 * math.sin(T * u) * sign, 2.0 * math.sin(T * u))
            det = self.idle_details(f, n, 1.2)
            ps.update(det)
            ps["mouth"] = 9.0 + 6.0 * math.sin(T * 2 * u)
            for s in SIDES:
                lead = (SGN[s] == sign)
                c = claw_preset("guard", s)
                ps["claws"][s] = claw_mod(c, d_wrist=(0.10 * sign, 0.18, -0.18 + 0.04 * math.sin(T * 2 * u + (0 if lead else 1.6))),
                                          gape=10 + (10 if lead else 4) * (0.5 + 0.5 * math.sin(T * u)))
            ps["eyes"] = {s: (0.0, -6.0, 18.0 * sign, 0.0, 0.0) for s in SIDES}
            return ps
        self.record(name, n, pose, loop=True, category="Locomotion", peak=0.25,
                    root_motion=f"en place ; {speed * duty * n / FPS:.2f} m par appui, {speed:.1f} m/s vers "
                                f"{'+X (gauche du crabe)' if sign > 0 else '-X (droite du crabe)'}",
                    extra=dict(speed_cm_s=int(round(speed * 100)), direction="+X (gauche du crabe)" if sign > 0 else "-X",
                               duty_factor=duty, body_low_m=low, v2=True))

    def belly_slam(self):
        """Se soulève (22-26), retombe à plat ventre (impact image 30), pattes écartées, reste au sol jusqu'à 44,
        se relève (60). Pas de root motion."""
        n = 60
        drop = 1.16
        body = Track2([(0, (0, 0, 0, 0, 0, 0)), (10, (0, 0.03, -0.16, -4, 0, 0)),
                       (22, (0, 0.06, 0.30, 7, 0, 0)), (26, (0, 0.06, 0.35, 9, 0, 0), ease_out),
                       (30, (0, 0.0, -drop, -2, 0, 0), ease_in), (32, (0, 0.0, -drop + 0.05, -1, 1.5, 0), ease_out),
                       (35, (0, 0, -drop, -2, -1, 0), ease_in), (44, (0, 0, -drop + 0.01, -1, 0, 0)),
                       (54, (0, 0, -0.30, 2, 0, 0)), (60, (0, 0, 0, 0, 0, 0))])
        trem = Track([(0, 0.0), (29, 0.0), (30, 1.0), (40, 0.3), (44, 0.0), (60, 0.0)])
        splay = {k: self.splay_target(k, 1.32, drop) for k in LEGS}
        A = self.P.groups["A"]
        g = {s: claw_preset("guard", s) for s in SIDES}
        sp = {s: claw_preset("spread", s) for s in SIDES}
        rz = {s: claw_preset("raised", s) for s in SIDES}

        def ground_claw(s):
            return dict(wrist=mx(s, (1.95, -1.80, 0.55)), pole=mx(s, (1.0, 0.2, 1.0)), hand=mx(s, (0.40, -0.88, -0.18)),
                        up=Vector((0, 0, 1)), gape=26.0, space="world")
        cl = {s: ClawTrack([(0, g[s]), (12, sp[s]), (24, rz[s]), (30, ground_claw(s)), (46, ground_claw(s)),
                            (58, g[s]), (60, g[s])]) for s in SIDES}
        eyes = Track([(0, 0.0), (26, 0.0), (30, 1.0), (46, 1.0), (56, 0.0), (60, 0.0)])
        mouth = Track([(0, 4.0), (16, 6.0), (24, 26.0), (30, 10.0), (44, 8.0), (60, 4.0)])

        def feet(f):
            out = {}
            for k in LEGS:
                r = Vector((self.feet0[k].x, self.feet0[k].y, 0.0))
                late = 0 if leg_name(*k) in A else 6
                if f <= 24:
                    p, dact = r, -78.0
                elif f <= 30:
                    u = (f - 24) / 6.0
                    p = r.lerp(splay[k], ease2(u)) + Vector((0, 0, 0.20 * math.sin(math.pi * u)))
                    dact = -78.0 + 38.0 * ease(u)
                elif f <= 46 + late:
                    p, dact = splay[k].copy(), -40.0
                elif f <= 52 + late:
                    u = (f - 46 - late) / 6.0
                    p = splay[k].lerp(r, ease2(u)) + Vector((0, 0, 0.16 * math.sin(math.pi * u)))
                    dact = -40.0 - 38.0 * ease(u)
                else:
                    p, dact = r, -78.0
                out[k] = (p, dact, "world")
            return out

        def pose(f):
            ps = self.base_pose()
            b = body(f)
            k_ = trem(f)
            ps["body"] = (b[0] + 0.01 * k_ * math.sin(f * 2.9), b[1], b[2] + 0.012 * k_ * math.sin(f * 3.7), b[3],
                          b[4] + 1.2 * k_ * math.sin(f * 2.3), b[5])
            ps["feet"] = feet(f)
            ps.update(self.idle_details(f, n, 1.2))
            ps["claws"] = {s: cl[s](f) for s in SIDES}
            e = eyes(f)
            ps["eyes"] = {s: (e, -8.0, 0.0, 0.0, 0.0) for s in SIDES}
            ps["mouth"] = mouth(f) + 3.0 * math.sin(f * 1.9)
            ps["mand"] = 10.0 * math.sin(f * 1.3) + (14.0 if 20 <= f <= 30 else 0.0)
            return ps
        self.record("BellySlam", n, pose, peak=30, category="Combat",
                    notifies=[("AN_Attack_Windup", 12), ("AN_BellySlam_Impact", 30), ("AN_CameraShake", 30)],
                    extra=dict(impact_frame=30, flat_frames=[30, 44], body_drop_m=drop, v2=True,
                               root_motion_total=dict(translation_m=[0, 0, 0], yaw_deg=0)))

    def roar(self):
        """Transition de phase : se cabre sur les pattes arrière (pattes avant levées), pinces grandes ouvertes et
        écartées, bouche et mandibules ouvertes, tremblement (AN_Roar à l'image 22)."""
        n = 75
        T = 2 * math.pi
        body = Track2([(0, (0, 0, 0, 0, 0, 0)), (8, (0, 0.04, -0.07, -3, 0, 0)),
                       (20, (0, 0.30, 0.28, 27, 0, 0)), (24, (0, 0.32, 0.31, 30, 0, 0), ease_out),
                       (52, (0, 0.31, 0.29, 29, 0, 0)), (66, (0, 0.04, -0.05, -2, 0, 0)), (75, (0, 0, 0, 0, 0, 0))])
        tuck = Track([(0, 0.0), (3, 0.0), (11, 1.0), (60, 1.0), (70, 0.0), (75, 0.0)])
        trem = Track([(0, 0.0), (20, 0.0), (23, 1.0), (52, 1.0), (57, 0.0), (75, 0.0)])
        lift = Track([(0, 0.0), (6, 0.0), (20, 1.0), (54, 1.0), (68, 0.0), (75, 0.0)])
        wide = {s: dict(wrist=mx(s, (1.95, -1.15, 2.30)), pole=mx(s, (1.0, 0.4, -0.3)), hand=mx(s, (0.50, -0.55, 0.67)),
                        up=mx(s, (-0.3, -0.8, 0.3)), gape=55.0, space="body") for s in SIDES}
        g = {s: claw_preset("guard", s) for s in SIDES}
        cl = {s: ClawTrack([(0, g[s]), (8, claw_preset("tuck", s)), (22, wide[s]), (54, wide[s]), (70, g[s]),
                            (75, g[s])]) for s in SIDES}
        mouth = Track([(0, 4.0), (14, 8.0), (22, 34.0), (54, 30.0), (66, 6.0), (75, 4.0)])
        mand = Track([(0, 0.0), (14, 0.0), (22, 24.0), (54, 22.0), (66, 0.0), (75, 0.0)])

        def pose(f):
            ps = self.base_pose()
            b = body(f)
            k_ = trem(f)
            bb = (b[0] + 0.012 * k_ * math.sin(f * 2.6), b[1], b[2] + 0.01 * k_ * math.sin(f * 3.1),
                  b[3] + 1.2 * k_ * math.sin(f * 2.2), b[4] + 1.6 * k_ * math.sin(f * 1.7), b[5])
            ps["body"] = bb
            B = self.body_matrix(self.root_matrix((0.0, 0.0, 0.0)), bb)
            w = ease(lift(f))
            feet = {}
            for k in LEGS:
                s, nn = k
                r = Vector((self.feet0[k].x, self.feet0[k].y, 0.0))
                if nn >= 3:
                    cx = self.coxa[k]
                    cxz = Vector((cx.x, cx.y, 0.0))
                    tk = tuck(f)
                    step = (3 < f < 11) or (60 < f < 70)
                    u = (f - 3) / 8.0 if f < 30 else (f - 60) / 10.0
                    lift_z = 0.16 * math.sin(math.pi * min(max(u, 0.0), 1.0)) if step else 0.0
                    p_ = r.lerp(cxz + (r - cxz) * 0.86 + Vector((0.0, 0.10, 0.0)), ease(tk)) + Vector((0, 0, lift_z))
                    feet[k] = (p_, -78.0, "world")
                    continue
                up_b = Vector((self.feet0[k].x * (0.82 if nn == 1 else 0.92), self.feet0[k].y - (0.30 if nn == 1 else 0.10),
                               0.95 if nn == 1 else 0.55))
                fl = Vector((0.10 * math.sin(T * f / 13 + nn * 1.1 + SGN[s]), 0.12 * math.sin(T * f / 10 + nn * 1.3),
                             0.14 * math.sin(T * f / 8 + nn * 0.7 + SGN[s]))) * k_
                tw = B @ (up_b + fl)
                feet[k] = (r.lerp(tw, w), -78.0 + 40.0 * w, "world")
            ps["feet"] = feet
            ps.update(self.idle_details(f, n, 1.0 + 1.2 * k_))
            for s in SIDES:
                c = cl[s](f)
                c = claw_mod(c, d_wrist=(0.03 * k_ * math.sin(f * 2.4 + SGN[s]), 0.0, 0.035 * k_ * math.sin(f * 1.9)),
                             gape=c["gape"] + 8.0 * k_ * math.sin(f * 1.4 + SGN[s]))
                ps["claws"][s] = c
            ps["mouth"] = mouth(f) + 4.0 * k_ * math.sin(f * 2.1)
            ps["mand"] = mand(f) + 6.0 * k_ * math.sin(f * 2.9)
            ps["eyes"] = {s: (0.0, -22.0 * w, 12.0 * SGN[s] * w, 0.0, 0.0) for s in SIDES}
            ps["abd"] = 6.0 * w
            return ps
        self.record("Roar", n, pose, peak=22, category="Combat", notifies=[("AN_Roar", 22), ("AN_CameraShake", 23)],
                    extra=dict(roar_frame=22, v2=True, root_motion_total=dict(translation_m=[0, 0, 0], yaw_deg=0)))

    def leg_stomp(self):
        """Les pattes avant piquent le sol à tour de rôle : gauche (impact image 13), droite (image 27)."""
        n = 45
        strikes = {("l", 1): (2, 8, 13, 22), ("r", 1): (15, 21, 27, 37)}
        body = Track2([(0, (0, 0, 0, 0, 0, 0)), (8, (0.06, 0.02, 0.10, 7, 4, 2)),
                       (13, (0.02, -0.08, -0.10, -6, -2, 0), ease_in), (21, (-0.06, 0.02, 0.10, 7, -4, -2)),
                       (27, (-0.02, -0.08, -0.10, -6, 2, 0), ease_in), (37, (0, -0.02, -0.02, -1, 0, 0)),
                       (45, (0, 0, 0, 0, 0, 0))])
        g = {s: claw_preset("guard", s) for s in SIDES}
        hi = {s: claw_mod(claw_lerp(claw_preset("guard", s), claw_preset("raised", s), 0.55), gape=38.0) for s in SIDES}
        cl = {s: ClawTrack([(0, g[s]), (8, hi[s]), (38, hi[s]), (45, g[s])]) for s in SIDES}
        points = {}

        def foot(k, f):
            r = Vector((self.feet0[k].x, self.feet0[k].y, 0.0))
            if k not in strikes:
                return r, -78.0
            a, b, c, e = strikes[k]
            cx = self.coxa[k]
            cxz = Vector((cx.x, cx.y, 0.0))
            target = cxz + (r - cxz) * 0.80 + Vector((0.0, -0.18, 0.0))
            cock = cxz + (r - cxz) * 0.72 + Vector((0.0, -0.12, 0.78))
            points[k] = target
            if f < a:
                return r, -78.0
            if f <= b:
                u = ease((f - a) / (b - a))
                return r.lerp(cock, u), -78.0 + 50.0 * u
            if f <= c:
                u = ease_in((f - b) / (c - b))
                return cock.lerp(target, u), -28.0 - 62.0 * u
            if f <= c + 5:
                return target + Vector((0.0, 0.0, 0.0)), -90.0
            if f <= e:
                u = ease((f - c - 5) / (e - c - 5))
                return target.lerp(r, u) + Vector((0, 0, 0.20 * math.sin(math.pi * u))), -90.0 + 12.0 * u
            return r, -78.0

        def pose(f):
            ps = self.base_pose()
            ps["body"] = body(f)
            ps["feet"] = {k: (*foot(k, f), "world") for k in LEGS}
            ps.update(self.idle_details(f, n, 1.3))
            ps["claws"] = {s: cl[s](f) for s in SIDES}
            ps["mouth"] = 10.0 + 8.0 * math.sin(f * 0.9)
            ps["eyes"] = {s: (0.0, -10.0, 0.0, 0.0, 0.0) for s in SIDES}
            return ps
        for k in strikes:
            foot(k, 0)
        self.record("LegStomp", n, pose, peak=13, category="Combat",
                    notifies=[("AN_Stomp_L", 13), ("AN_Stomp_R", 27)],
                    extra=dict(v2=True, stomp_points_clip_space_m={f"{s.upper()}{nn}": [round(v, 3) for v in p]
                                                                   for (s, nn), p in points.items()},
                               root_motion_total=dict(translation_m=[0, 0, 0], yaw_deg=0)))

    LOCOMOTION_STEPS = ("Walk_Sideways_L", "Walk_Sideways_R", "Walk_Forward", "Turn_L", "Turn_R", "Charge",
                        "Charge_R")

    def step_contacts(self, name, eps=0.004):
        """Images de contact de chaque pointe (passage au-dessus -> au sol)."""
        c = self.clips[name]
        n = c["n"]
        out = []
        for key in LEGS:
            zs = [c["tips"][f][key].z for f in range(n + 1)]
            for f in range(1, n + 1):
                if zs[f - 1] > eps and zs[f] <= eps:
                    fr = 0 if (c["loop"] and f == n) else f
                    out.append(dict(bone=f"leg_{key[0]}{key[1]}_dactyl", socket=f"SOCKET_LegTip_{key[0].upper()}{key[1]}",
                                    frame=fr, time_s=round(fr / FPS, 3)))
        return sorted(out, key=lambda d: (d["frame"], d["bone"]))

    def add_step_notifies(self):
        """AN_Step_<os de pointe> dans les clips de locomotion ; liste des contacts de tous les clips au manifeste."""
        for name in self.clips:
            contacts = self.step_contacts(name)
            self.meta[name]["step_contacts"] = contacts
            if name in self.LOCOMOTION_STEPS:
                self.meta[name]["notifies"] += [[f"AN_Step_{c['bone']}", c["frame"]] for c in contacts]
                self.meta[name]["notifies"].sort(key=lambda x: x[1])


    # ================================================================== contrôles
    def check(self):
        """Glissement des pieds en appui, pointes sous le sol, bouclage."""
        out = {}
        for name, c in self.clips.items():
            n = c["n"]
            m = self.meta[name]
            speed = m.get("speed_cm_s", 0) / 100.0
            direction = Vector((1, 0, 0)) if name.endswith("_L") else Vector((-1, 0, 0)) if name.endswith("_R") \
                else Vector((0, -1, 0))
            if name == "Charge":
                direction = Vector((1, 0, 0))
            slip = 0.0
            minz = 9.0
            for f in range(n):
                for k in LEGS:
                    p0, p1 = c["tips"][f][k], c["tips"][f + 1][k]
                    minz = min(minz, p0.z)
                    if p0.z < 0.003 and p1.z < 0.003:
                        v = (p1 - p0)
                        if m["category"] == "Locomotion" and name.startswith(("Walk", "Charge")):
                            v = v + direction * speed / FPS
                        if name in ("Death", "Stagger_Flip", "Recover", "BellySlam", "LegStomp", "Roar"):
                            continue
                        slip = max(slip, v.length)
            loop_err = 0.0
            if c["loop"]:
                a, b = c["mats"][0], c["mats"][-1]
                loop_err = max((a[k].translation - b[k].translation).length for k in a)
            out[name] = dict(max_stance_slip_m_per_frame=round(slip, 5), min_tip_z_m=round(minz, 4),
                             loop_error=round(loop_err, 6), **self.diag.get(name, {}))
        return out


# ============================================================================= écriture des actions

def _fcurve_bag(action):
    curves = []
    for layer in getattr(action, "layers", []):
        for strip in layer.strips:
            for bag in getattr(strip, "channelbags", []):
                curves.extend(bag.fcurves)
    return curves


def write_action(arm, an, name, clip, offset=0):
    """Clés d'os (location + quaternion) image par image, interpolation linéaire."""
    act = bpy.data.actions.get(name)
    if act is not None:
        bpy.data.actions.remove(act)
    act = bpy.data.actions.new(name)
    act.use_fake_user = True
    ad = arm.animation_data_create()
    ad.action = act
    n = clip["n"]
    bases = [an.basis(M) for M in clip["mats"]]
    frames = [offset + f for f in range(n + 1)]
    for bname in an.order:
        locs = [b[bname][0] for b in bases]
        qs = []
        prev = None
        for b in bases:
            q = b[bname][1].copy()
            if prev is not None and prev.dot(q) < 0:
                q.negate()
            qs.append(q)
            prev = q
        dp = f'pose.bones["{bname}"]'
        for path, vals, dim in ((dp + ".location", locs, 3), (dp + ".rotation_quaternion", qs, 4)):
            for i in range(dim):
                fc = act.fcurve_ensure_for_datablock(arm, path, index=i, group_name=bname)
                kp = fc.keyframe_points
                kp.add(len(frames))
                co = []
                for f, v in zip(frames, vals):
                    co += [f, v[i]]
                kp.foreach_set("co", co)
                kp.foreach_set("interpolation", [1] * len(frames))
                fc.update()
    act.frame_range = (offset, offset + n)
    act.use_frame_range = True
    act["fps"] = FPS
    act["loop"] = clip["loop"]
    ad.action = None
    return act


def write_dead_eyes(mesh_obs, name, clip):
    """Action de forme DeadEyes (pour la vitrine Blender ; dans Unreal : courbe DeadEyes du manifeste)."""
    vals = clip["dead"]
    if max(vals) <= 0:
        return None
    acts = []
    for ob in mesh_obs:
        key = ob.data.shape_keys
        if key is None:
            continue
        an = f"SK_{name}_{ob.name}"
        act = bpy.data.actions.get(an) or bpy.data.actions.new(an)
        act.use_fake_user = True
        ad = key.animation_data_create()
        ad.action = act
        fc = act.fcurve_ensure_for_datablock(key, 'key_blocks["DeadEyes"].value')
        fc.keyframe_points.add(len(vals))
        co = []
        for f, v in enumerate(vals):
            co += [f, v]
        fc.keyframe_points.foreach_set("co", co)
        fc.keyframe_points.foreach_set("interpolation", [1] * len(vals))
        fc.update()
        act.frame_range = (0, len(vals) - 1)
        ad.action = None
        acts.append(act)
    return acts


ORDER_V1 = ["Idle", "Walk_Sideways_L", "Walk_Sideways_R", "Walk_Forward", "Turn_L", "Turn_R", "Taunt_ClawClack",
            "Attack_ClawSwipe", "Attack_ClawSlam", "Attack_Grab", "Grab_Hold", "Throw", "Block_Start", "Block_Loop",
            "Block_End", "Hit_React", "Stagger_Flip", "Flipped_Loop", "Recover", "Death", "Death_Flipped"]
ORDER_V2 = ["BellySlam", "Roar", "LegStomp", "Charge", "Charge_R"]
ORDER = ORDER_V1 + ORDER_V2


def set_action(arm, act):
    ad = arm.animation_data_create()
    ad.action = act
    if act is not None and hasattr(ad, "action_slot") and act.slots:
        try:
            ad.action_slot = act.slots[0]
        except (TypeError, RuntimeError):
            pass


def build_actions(st):
    """Crée l'animateur, génère tous les clips et leurs actions A_GiantCrab_*."""
    an = CrabAnimator(st["P"], st["Lay"], st["arm"])
    an.build_all()
    actions = {}
    for name in ORDER:
        actions[name] = write_action(st["arm"], an, "A_GiantCrab_" + name, an.clips[name])
        write_dead_eyes([st["meshes"][0]], name, an.clips[name])
    for ob in st["meshes"].values():                 # valeur de repos de la forme (aussi exportée dans le FBX)
        if ob.data.shape_keys:
            ob.data.shape_keys.key_blocks["DeadEyes"].value = 0.0
    st["animator"] = an
    st["actions"] = actions
    return an, actions


def export_clips(st, out_dir, rig_module):
    """Une FBX par clip (armature seule, os déformants, 30 i/s), comme anim_export.export_range."""
    arm = st["arm"]
    scene = bpy.context.scene
    scene.render.fps = FPS
    ad = arm.animation_data_create()
    use_nla = ad.use_nla
    ad.use_nla = False
    files = {}
    try:
        for name in ORDER:
            act = st["actions"][name]
            set_action(arm, act)
            n = st["animator"].clips[name]["n"]
            path = os.path.join(out_dir, f"A_GiantCrab_{name}.fbx")
            rig_module.export_fbx([arm], path, anim=True, frames=(0, n))
            files[name] = path
    finally:
        set_action(arm, None)
        ad.use_nla = use_nla
    return files


def clip_records(st, rel_root):
    an = st["animator"]
    out = []
    for name in ORDER:
        m = an.meta[name]
        c = an.clips[name]
        roots = c["roots"]
        rm = m.get("root_motion", "aucun")
        has_rm = any(abs(r[0]) > 1e-6 or abs(r[1]) > 1e-6 or abs(r[2]) > 1e-6 for r in roots)
        rec = dict(name=name, asset=f"A_GiantCrab_{name}", file=f"{rel_root}/A_GiantCrab_{name}.fbx",
                   frames=m["frames"], seconds=m["seconds"], fps=FPS, loop=m["loop"], category=m["category"],
                   enable_root_motion=has_rm, root_motion=rm,
                   root_motion_per_frame=[[round(r[0], 4), round(r[1], 4), round(r[2], 3)] for r in roots] if has_rm else None,
                   notifies=[dict(name=a, frame=b, time_s=round(b / FPS, 3)) for a, b in m["notifies"]],
                   peak_frame=m["peak_frame"])
        for k in ("speed_cm_s", "direction", "blendspace", "montage_sections", "curves", "release",
                  "root_motion_total", "grab_socket_clip_space_m", "grab_frame", "hold_socket_m",
                  "impact_tip_clip_space_m", "belly_height_m", "optional", "impact_frame", "flat_frames",
                  "body_drop_m", "belly_min_z_m", "roar_frame", "stomp_points_clip_space_m", "step_contacts",
                  "duty_factor", "body_low_m", "v2"):
            if k in m:
                rec[k] = m[k]
        if "curves" in rec:
            rec["curves"] = {k: [[round(f / FPS, 3), v] for f, v in keys] for k, keys in rec["curves"].items()}
        out.append(rec)
    return out
