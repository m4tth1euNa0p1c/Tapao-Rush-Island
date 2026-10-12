"""Géométrie « réaliste » des mobs : base RealCoco (coque irrégulière, mèches, membres organiques remaillés, mains à
ongles, pieds à orteils, bracelets) et Guerrier Coco réaliste (WarriorReal).

Rapatrié de OneDrive/maquette-blender/factory/kits/coco_real_geo.py (maquette mob-coco-realiste, 2026-10-07,
celle du Guerrier Coco intégré le 2026-10-08) ; le Villageois est retiré, seuls les imports et le chargement changent.
Les variantes « horreur » (T-274 : Guerrier Coco, Gardien Crabe, Veilleur des Cendres) sont dans horror_geo.py.

Mêmes dimensions, mêmes articulations (SPECS tiki, fractions de H) que scripts/character/tiki/creatures.py : les
squelettes SKEL_<Mob> sont identiques os pour os. Conventions tiki : mètres, face -Y, gauche +X, pieds à z = 0.
"""
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

from .base import T, mat_bone, mat_leather
from .real_mats import (mat_dark, mat_feather, mat_fibre, mat_flint, mat_husk, mat_lime, mat_nail, mat_rope,
                        mat_shell, mat_skin, mat_tooth, mat_weave, mat_wet, mat_wood_carved, mat_wood_raw,
                        mat_leaf_strip)

TAU = 2 * math.pi


# ------------------------------------------------------------------ helpers bmesh avec UV
def uv_layer(bm):
    return bm.loops.layers.uv.get("UVMap") or bm.loops.layers.uv.new("UVMap")


def bm_ribbon(bm, pts, widths, normals, fold=0.0, cols=3, curl=0.0):
    """Lanière / feuille : pts (axe), largeur et normale par point ; UV u en travers [0, 1], v le long [0, 1].
    fold > 0 relève la nervure centrale (feuille pliée), curl enroule les bords vers l'arrière au bout."""
    uvl = uv_layer(bm)
    n = len(pts)
    rows = []
    for k in range(n):
        d = (pts[min(k + 1, n - 1)] - pts[max(k - 1, 0)]).normalized()
        side = d.cross(normals[k])
        if side.length < 1e-6:
            side = d.orthogonal()
        side.normalize()
        up = side.cross(d).normalized()
        w = widths[k]
        row = []
        for c in range(cols):
            t = c / (cols - 1) - 0.5
            row.append(bm.verts.new(pts[k] + side * (t * w) + up * (fold * w * (0.25 - t * t))
                                    - up * (curl * w * (k / (n - 1)) ** 2 * abs(t))))
        rows.append(row)
    for k in range(n - 1):
        for c in range(cols - 1):
            f = bm.faces.new((rows[k][c], rows[k][c + 1], rows[k + 1][c + 1], rows[k + 1][c]))
            for lp, (u, v) in zip(f.loops, ((c / (cols - 1), k / (n - 1)), ((c + 1) / (cols - 1), k / (n - 1)),
                                            ((c + 1) / (cols - 1), (k + 1) / (n - 1)), (c / (cols - 1), (k + 1) / (n - 1)))):
                lp[uvl].uv = (u, v)


def bm_eye(bm, center, gaze, r, iris_ang=0.62, nu=32, nv=16):
    """Globe oculaire orienté (pôle = regard) : faces de l'iris (mat 1, UV polaires) puis sclère (mat 0)."""
    uvl = uv_layer(bm)
    g = Vector(gaze).normalized()
    a = g.orthogonal().normalized()
    b = g.cross(a)
    c = Vector(center)
    pole = bm.verts.new(c + g * r)
    rows = []
    for j in range(1, nv):
        phi = math.pi * j / nv
        rows.append([bm.verts.new(c + r * (math.cos(phi) * g + math.sin(phi) * (math.cos(TAU * i / nu) * a +
                                                                              math.sin(TAU * i / nu) * b)))
                     for i in range(nu)])
    tail = bm.verts.new(c - g * r)

    def phi_of(j):
        return math.pi * (j + 1) / nv

    def put(f, uvs, mat):
        f.material_index = mat
        for lp, uv in zip(f.loops, uvs):
            lp[uvl].uv = uv

    for i in range(nu):
        i2 = (i + 1) % nu
        f = bm.faces.new((pole, rows[0][i], rows[0][i2]))
        put(f, ((i / nu, 0.0), (i / nu, phi_of(0) / iris_ang), (i2 / nu, phi_of(0) / iris_ang)), 1)
    for j in range(len(rows) - 1):
        for i in range(nu):
            i2 = (i + 1) % nu
            f = bm.faces.new((rows[j][i], rows[j + 1][i], rows[j + 1][i2], rows[j][i2]))
            iris = phi_of(j + 1) <= iris_ang + 1e-4
            put(f, ((i / nu, phi_of(j) / iris_ang), (i / nu, phi_of(j + 1) / iris_ang),
                    (i2 / nu, phi_of(j + 1) / iris_ang), (i2 / nu, phi_of(j) / iris_ang)), 1 if iris else 0)
    for i in range(nu):
        f = bm.faces.new((rows[-1][i], tail, rows[-1][(i + 1) % nu]))
        put(f, ((0, 1), (0, 1), (0, 1)), 0)


def bm_cowrie(bm, c, along, up, L):
    """Cauri : ellipsoïde bombé, face inférieure aplatie et pincée (la fente sombre vient de la cavité AO)."""
    along = Vector(along).normalized()
    up = Vector(up).normalized()
    side = up.cross(along).normalized()
    up = along.cross(side).normalized()
    R = Matrix((side, along, up)).transposed()
    geom = bmesh.ops.create_uvsphere(bm, u_segments=14, v_segments=9, radius=1.0)
    for v in geom["verts"]:
        p = Vector((v.co.x * 0.36 * L, v.co.y * 0.5 * L, v.co.z * 0.30 * L))
        if p.z < 0:
            p.z *= 0.35
            if abs(v.co.x) < 0.25:
                p.x *= 0.4
        v.co = R @ p + Vector(c)


def bm_shark_tooth(bm, base, down, out, h, w, t):
    """Dent de requin : triangle épais à pointe légèrement recourbée, base en racine."""
    down, out = Vector(down).normalized(), Vector(out).normalized()
    side = down.cross(out).normalized()
    base = Vector(base)
    pts = [base + side * (w * 0.5), base - side * (w * 0.5), base + out * t * 0.6, base - out * t * 0.6,
           base + down * (h * 0.5) + side * (w * 0.3) + out * t * 0.3, base + down * (h * 0.5) - side * (w * 0.3),
           base + down * h + out * (t * 0.25)]
    T["kit"].bm_hull(bm, pts)


def ragged_band(a, b, w, rnd, n=8, jag=0.25):
    """Polygone (u, v) d'un trait peint frotté de a à b, largeur w, bords irréguliers."""
    a, b = Vector(a), Vector(b)
    d = (b - a).normalized()
    pn = Vector((-d.y, d.x))
    left, right = [], []
    for k in range(n + 1):
        t = k / n
        c = a.lerp(b, t)
        ww = w * (0.55 + 0.45 * math.sin(math.pi * min(1.0, 0.1 + t)))
        left.append(tuple(c + pn * ww * (1 + jag * rnd.uniform(-1, 1))))
        right.append(tuple(c - pn * ww * (1 + jag * rnd.uniform(-1, 1))))
    return left + right[::-1]


# ------------------------------------------------------------------ jeux de matériaux
def warrior_mats(n):
    return {
        "Husk": mat_husk(f"MAT-{n}_Husk"),
        "Fibre": mat_fibre(f"MAT-{n}_Fibre"),
        "Wood": mat_wood_carved(f"MAT-{n}_MaskWood"),
        "Lime": mat_lime(f"MAT-{n}_Lime"),
        "Dark": mat_dark(f"MAT-{n}_Cavity"),
        "Eye": mat_wet(f"MAT-{n}_Eye", col="2A1A10", rough=0.06),
        "Teeth": mat_tooth(f"MAT-{n}_SharkTooth"),
        "Skin": mat_skin(f"MAT-{n}_Skin", c0="2A1208", c1="4A2412", c2="66381E", vein="1E0A04", scar="A87C66",
                         hair=0.15, ochre="8E2A12", ochre_amt=0.8),
        "Nail": mat_nail(f"MAT-{n}_Nail"),
        "Weave": mat_weave(f"MAT-{n}_Weave", light="B89A58", dark="5A4420"),
        "Strip": mat_leaf_strip(f"MAT-{n}_Pandanus", tint="BCA060", dark="5A4420", dry=0.92),
        "Rope": mat_rope(f"MAT-{n}_Rope"),
        "Leather": mat_leather(f"MAT-{n}_Leather"),
        "Shell": mat_shell(f"MAT-{n}_Cowrie"),
        "Bone": mat_bone(f"MAT-{n}_Bone"),
        "Shaft": mat_wood_raw(f"MAT-{n}_SpearWood"),
        "Flint": mat_flint(f"MAT-{n}_Flint"),
        "Feather": mat_feather(f"MAT-{n}_Feather"),
        "DeadX": mat_lime(f"MAT-{n}_DeadX", base="F2EEE0"),
    }


class ReliefMask:
    """Surface de masque + relief sculpté relief(u, v) (mètres) : pt / nrm compatibles tiki (stroke, bm_patch)."""

    def __init__(self, base, relief):
        self.base, self.relief = base, relief
        self.back = getattr(base, "back", False)

    def pt(self, u, v, extra=0.0):
        return self.base.pt(u, v, extra + self.relief(u, v))

    def nrm(self, u, v):
        e = 0.02
        n = (self.pt(u + e, v) - self.pt(u - e, v)).cross(self.pt(u, v + e) - self.pt(u, v - e))
        return n.normalized()


def seg_d(p, a, b):
    ax, ay, bx, by = a[0], a[1], b[0], b[1]
    dx, dy = bx - ax, by - ay
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / max(1e-9, dx * dx + dy * dy)))
    return math.hypot(p[0] - ax - dx * t, p[1] - ay - dy * t)


def sigm(x):
    return 1.0 / (1.0 + math.exp(-max(-60.0, min(60.0, x))))


def tiki_relief(H):
    """Relief d'un masque tiki taillé dans la masse : arcades lourdes, orbites creusées, arête du nez et narines,
    pommettes, bourrelet de la bouche, front bombé."""
    brow = [(0.06, 0.40), (0.30, 0.47), (0.55, 0.56), (0.80, 0.64)]

    def f(u, v):
        a = abs(u)
        h = 0.0
        d = min(seg_d((a, v), brow[i], brow[i + 1]) for i in range(len(brow) - 1))
        h += 0.021 * math.exp(-(d / 0.075) ** 2)
        h -= 0.013 * math.exp(-(((a - 0.40) / 0.30) ** 2 + ((v - 0.22) / 0.19) ** 2))
        h += 0.017 * math.exp(-(u / 0.075) ** 2) * sigm(v / 0.04) * sigm(-(v - 0.42) / 0.04)
        h += 0.012 * math.exp(-((u / 0.16) ** 2 + ((v - 0.03) / 0.07) ** 2))
        h += 0.009 * math.exp(-(((a - 0.68) / 0.15) ** 2 + ((v + 0.10) / 0.10) ** 2))
        dm = (abs(u / 0.70) ** 6 + abs((v + 0.55) / 0.36) ** 6) ** (1 / 6)
        h += 0.010 * math.exp(-((dm - 1.0) / 0.13) ** 2)
        h += 0.006 * max(0.0, min(1.0, (v - 0.55) / 0.35))
        return h * H

    return f


# ------------------------------------------------------------------ classes
def make_classes():
    Creature = T["creatures"].Creature
    K = T["kit"]

    class RealCoco(Creature):
        MATS = None

        def __init__(self, key, coll, seed):
            super().__init__(key, coll)
            self.rnd.seed(seed)
            self.custom = type(self).MATS(self.name)
            self.cutters = []
            self.eye_points = []

        def m(self, short):
            if short in self.custom:
                return self.custom[short]
            return super().m(short)

        # -------------------------------------------------------- utilitaires
        def cutter(self, name, bm, targets):
            me = bpy.data.meshes.new(f"{self.name}_{name}")
            bm.to_mesh(me)
            bm.free()
            ob = bpy.data.objects.new(me.name, me)
            self.coll.objects.link(ob)
            ob.display_type = "WIRE"
            ob.hide_render = True
            self.cutters.append(ob)
            for part in targets:
                mod = part.modifiers.new("Cut_" + name, "BOOLEAN")
                mod.operation = "DIFFERENCE"
                mod.solver = "EXACT"
                mod.object = ob
                mod.material_mode = "TRANSFER"
            return ob

        def organic(self, ob, voxel=0.0075, smooth=12):
            """Fusionne tubes et masses musculaires en une seule peau (remaillage voxel + lissage)."""
            r = ob.modifiers.new("Remesh", "REMESH")
            r.mode = "VOXEL"
            r.voxel_size = voxel
            r.use_smooth_shade = True
            sm = ob.modifiers.new("Smooth", "SMOOTH")
            sm.factor = 0.7
            sm.iterations = smooth
            ob["organic"] = True
            return ob

        def husk_noise(self, th, z):
            H = self.H
            return (math.sin(th * 7 + z * 23) * 0.5 + math.sin(th * 13 - z * 41 + 1.3) * 0.3 +
                    math.sin(th * 29 + z * 77 + 0.4) * 0.2) * 0.0035 * H

        def shell(self, nu=88, nz=66, disp=None):
            """Œuf de bourre irrégulier ; disp(th, z, p) -> déplacement additionnel le long de la normale."""
            B = self.body
            bm = bmesh.new()
            K.bm_revolve(bm, B, nu, nz)
            for v in bm.verts:
                p = v.co.copy()
                if p.z <= B.zb + 1e-5 or p.z >= B.zt - 1e-5:
                    continue
                th = math.atan2(p.x, -p.y)
                nn = B.normal(th, p.z)
                d = self.husk_noise(th, p.z)
                if disp:
                    d += disp(th, p.z, p)
                v.co = p + nn * d
            self.shell_ob = self.part("Body", bm, "Husk", "body", recalc=True)
            return self.shell_ob

        def fibres(self, n=84, skip=None, z0=(0.27, 0.36), z1=(0.86, 0.97), long_n=110):
            H, B, rnd = self.H, self.body, self.rnd
            bm = bmesh.new()

            def flush(pts):
                if len(pts) >= 3:
                    rr = H * rnd.uniform(0.0016, 0.003)
                    K.bm_tube(bm, pts, [rr * (0.4 + 0.6 * math.sin(math.pi * k / (len(pts) - 1)))
                                        for k in range(len(pts))], segs=4)

            for i in range(n):
                th = TAU * (i + rnd.uniform(-0.3, 0.3)) / n
                za = H * rnd.uniform(*z0)
                zb = H * rnd.uniform(*z1)
                pts = []
                for j in range(15):
                    z = za + (zb - za) * j / 14
                    t2 = th + 0.03 * math.sin(j * 0.7 + i)
                    if skip and skip(t2, z):
                        flush(pts)
                        pts = []
                        continue
                    pts.append(B.surf(t2, z, H * 0.0012 + self.husk_noise(t2, z)))
                flush(pts)
            self.part("HuskFibres", bm, "Husk", "body", recalc=True)
            bm = bmesh.new()
            for i in range(long_n):
                th = rnd.uniform(0.0, TAU)
                z = H * rnd.uniform(0.28, 0.96)
                if skip and skip(th, z):
                    continue
                p, nn = B.surf(th, z, 0.0008 * H + self.husk_noise(th, z)), B.normal(th, z)
                side = nn.cross(Vector((0, 0, 1))).normalized()
                long_ = rnd.random() < 0.3
                k = rnd.uniform(0.8, 1.5) * (2.6 if long_ else 1.0)
                droop = Vector((0, 0, -0.02 * H * k)) if long_ else Vector((0, 0, 0.012 * H * k))
                pts = [p, p + nn * 0.010 * H * k + droop * 0.4, p + nn * 0.014 * H * k + side * 0.008 * H + droop]
                K.bm_tube(bm, K.catmull(pts, 6), [0.0022 * H * (1 - 0.85 * j / 5) for j in range(6)], segs=4)
            self.part("LooseCoir", bm, "Fibre", "body", recalc=True)

        # -------------------------------------------------------- membres
        def finger(self, bm, pts, r, taper=0.25):
            P = [Vector(p) for p in pts]
            C = K.catmull(P, 10)
            K.bm_tube(bm, C, [r * (1.0 - taper * j / 9) for j in range(10)], segs=8)
            for p in P[1:-1]:
                K.bm_blob(bm, p, (r * 1.08, r * 1.08, r * 1.08), seg=10, ring=6)
            d = (C[-1] - C[-3]).normalized()
            return C[-1], d

        def nail(self, bm, tip, d, back, r):
            """Ongle : coque bombée sur le dos de la dernière phalange."""
            d, back = Vector(d).normalized(), Vector(back).normalized()
            side = d.cross(back).normalized()
            back = side.cross(d).normalized()
            c = Vector(tip) - d * r * 0.7 + back * r * 0.62
            R = Matrix((side, d, back)).transposed()
            K.bm_blob(bm, c, (r * 0.78, r * 0.95, r * 0.28), rot=R, seg=10, ring=6)

        def limb(self, s, muscle=1.0):
            side = "l" if s > 0 else "r"
            sh, el, wr = self.J("shoulder", s), self.J("elbow", s), self.J("wrist", s)
            ra = self.h(self.s["arm_r"]) * (0.92 + 0.12 * muscle)
            sh_in = sh - Vector((s * self.h(0.07), 0, -self.h(0.01)))
            bm = bmesh.new()
            pts = [sh_in, sh, sh.lerp(el, 0.35), sh.lerp(el, 0.7), el, el.lerp(wr, 0.3), el.lerp(wr, 0.6), wr]
            K.bm_tube(bm, pts, [ra * 1.10, ra * 1.16, ra * 1.06, ra * 0.98, ra * 0.88, ra * (1.0 + 0.2 * muscle),
                                ra * (0.98 + 0.16 * muscle), ra * 0.82], segs=20)
            ax = (el - sh).normalized()
            out = (Vector((s, 0, 0)) - ax * ax.dot(Vector((s, 0, 0)))).normalized()
            fw = Vector((0, -1, 0))
            mm = muscle
            K.bm_blob(bm, sh + Vector((s * ra * 0.2, 0, ra * 0.2)), (ra * (0.85 + 0.15 * mm), ra * 0.95, ra * 1.0),
                      rot=K.look_rot(ax), seg=20, ring=12)                                         # deltoïde
            K.bm_blob(bm, sh.lerp(el, 0.5) + fw * ra * 0.30, (ra * 0.72 * mm, ra * 0.62 * mm, ra * 1.5),
                      rot=K.look_rot(ax), seg=16, ring=10)                                         # biceps
            K.bm_blob(bm, sh.lerp(el, 0.45) - fw * ra * 0.30, (ra * 0.74 * mm, ra * 0.66 * mm, ra * 1.5),
                      rot=K.look_rot(ax), seg=16, ring=10)                                         # triceps
            fa = (wr - el).normalized()
            K.bm_blob(bm, el.lerp(wr, 0.30) + out * ra * 0.25, (ra * 0.82 * mm, ra * 0.8 * mm, ra * 1.6),
                      rot=K.look_rot(fa), seg=16, ring=10)                                         # avant-bras
            K.bm_blob(bm, el.lerp(wr, 0.25) - out * ra * 0.2 + fw * ra * 0.2, (ra * 0.6 * mm, ra * 0.6, ra * 1.3),
                      rot=K.look_rot(fa), seg=16, ring=10)
            self.organic(self.part(f"Arm_{side}", bm, "Skin", f"arm_{side}", recalc=True))
            hip, kn, an = self.J("hip", s), self.J("knee", s), self.J("ankle", s)
            rl = self.h(self.s["leg_r"]) * (0.95 + 0.1 * muscle)
            bm = bmesh.new()
            K.bm_tube(bm, [hip + Vector((0, 0, self.h(0.07))), hip, hip.lerp(kn, 0.5), kn, kn.lerp(an, 0.45), an],
                      [rl * 1.08, rl * 1.10, rl * 1.02, rl * 0.90, rl * 0.98, rl * 0.80], segs=20)
            la = (kn - hip).normalized()
            K.bm_blob(bm, hip.lerp(kn, 0.45) + Vector((0, -rl * 0.25, 0)), (rl * 0.9 * mm, rl * 0.8, rl * 1.5),
                      rot=K.look_rot(la), seg=16, ring=10)
            K.bm_blob(bm, kn.lerp(an, 0.35) + Vector((0, rl * 0.3, 0)), (rl * 0.75 * mm, rl * 0.8, rl * 1.3),
                      rot=K.look_rot((an - kn).normalized()), seg=16, ring=10)
            K.bm_blob(bm, kn + Vector((0, -rl * 0.5, 0)), (rl * 0.5, rl * 0.35, rl * 0.45), seg=12, ring=8)  # genou
            self.organic(self.part(f"Leg_{side}", bm, "Skin", f"leg_{side}", recalc=True))

        def hand_open(self, s):
            side = "l" if s > 0 else "r"
            wr, D, F, L, ln = self.hand_frame(s)
            hs = self.hs
            c = wr + D * hs(0.045)
            R = Matrix((L, F, D)).transposed()
            inner = -s * L
            bm, bmn = bmesh.new(), bmesh.new()
            K.bm_blob(bm, c, (hs(0.050), hs(0.034), hs(0.050)), rot=R, seg=18, ring=12)
            K.bm_tube(bm, [wr - D * hs(0.012), c], [hs(0.034), hs(0.038)], segs=14)
            rf = hs(0.0135)
            for k in (-1.5, -0.5, 0.5, 1.5):
                base = c + D * hs(0.038) + L * (k * hs(0.021)) + F * hs(0.002)
                ln_k = 1.0 - 0.12 * abs(k) - (0.1 if k * s > 1 else 0.0)
                p1 = base + D * hs(0.026 * ln_k) + F * hs(0.008)
                p2 = p1 + D * hs(0.020 * ln_k) + F * hs(0.014)
                p3 = p2 + D * hs(0.012 * ln_k) + F * hs(0.014)
                tip, d = self.finger(bm, [base, p1, p2, p3], rf * (1.0 - 0.06 * abs(k)))
                self.nail(bmn, tip, d, -F, rf * 0.95)
            tb = c + F * hs(0.022) + inner * hs(0.034) - D * hs(0.01)
            tp = tb + F * hs(0.024) + D * hs(0.020) + inner * hs(0.008)
            tip, d = self.finger(bm, [tb, tb.lerp(tp, 0.5) + inner * hs(0.004), tp], rf * 1.12)
            self.nail(bmn, tip, d, inner, rf)
            self.organic(self.part(f"Hand_{side}", bm, "Skin", f"hand_{side}", recalc=True), voxel=0.005, smooth=5)
            self.part(f"Nails_{side}", bmn, "Nail", f"hand_{side}", recalc=True)

        def hand_fist(self, s, axis=None, rs=0.018):
            """Main refermée sur une hampe (axe A au repos), 4 doigts enroulés, pouce verrouillé, ongles."""
            side = "l" if s > 0 else "r"
            wr, D, F, L, ln = self.hand_frame(s)
            hs = self.hs
            c = self.grip_r if s < 0 else self.grip_l
            A = Vector(axis) if axis is not None else (self.w_rest_rot.to_3x3() @ Vector((0, 0, 1)))
            A.normalize()
            Dp = (D - A * D.dot(A)).normalized()
            Bk = A.cross(Dp).normalized()
            if Bk.dot(s * L) < 0:
                Bk = -Bk
            rf = hs(0.0135)
            bm, bmn = bmesh.new(), bmesh.new()
            palm_c = c - Dp * (rs + hs(0.018)) + Bk * hs(0.010)
            R = Matrix((Bk, A, Dp)).transposed()
            K.bm_blob(bm, palm_c, (hs(0.034), hs(0.048), hs(0.036)), rot=R, seg=18, ring=12)
            K.bm_tube(bm, [wr - Dp * hs(0.01), palm_c], [hs(0.034), hs(0.036)], segs=14)
            for k in range(4):
                off = A * ((k - 1.5) * hs(0.0215))
                arc = []
                for j in range(7):
                    th = math.radians(115 - 250 * j / 6)
                    rr = rs + rf * (1.0 if j < 6 else 0.9)
                    arc.append(c + off + (Dp * math.cos(th) + Bk * math.sin(th)) * rr)
                arc[0] = arc[0] + Bk * hs(0.005)
                tip, d = self.finger(bm, arc, rf * (1.0 - 0.06 * abs(k - 1.5)), taper=0.15)
                outw = (tip - (c + off)).normalized()
                self.nail(bmn, tip, d, outw, rf * 0.9)
            tb = palm_c - Bk * hs(0.026) + A * hs(0.048)
            tm = c + A * hs(0.052) - Bk * (rs + rf)
            tt = c + A * hs(0.03) + Dp * (rs + rf * 0.8) - Bk * rf * 0.5
            tip, d = self.finger(bm, [tb, tm, tt], rf * 1.1)
            outw = (tip - c).normalized()
            self.nail(bmn, tip, d, outw - A * outw.dot(A), rf)
            self.organic(self.part(f"Hand_{side}", bm, "Skin", f"hand_{side}", recalc=True), voxel=0.005, smooth=5)
            self.part(f"Nails_{side}", bmn, "Nail", f"hand_{side}", recalc=True)

        def foot_real(self, s, sandal=True):
            side = "l" if s > 0 else "r"
            c, toe = self.foot_core(s)
            f = self.fs
            bm, bmn = bmesh.new(), bmesh.new()
            K.bm_blob(bm, c + Vector((0, f(0.01), 0)), (f(0.056), f(0.088), f(0.044)), cut_below=self.h(0.012),
                      seg=18, ring=12)
            K.bm_blob(bm, c + Vector((0, f(0.06), f(0.01))), (f(0.044), f(0.045), f(0.042)), cut_below=self.h(0.012),
                      seg=14, ring=8)                                            # talon
            K.bm_tube(bm, [self.J("ankle", s) + Vector((0, 0, self.h(0.03))), c + Vector((0, f(0.02), f(0.02)))],
                      [self.h(self.s["leg_r"]) * 0.8, f(0.04)], segs=14)
            for k, dx in enumerate((-0.034, -0.012, 0.010, 0.030)):
                big = k == 0
                rr = f(0.017 if big else 0.0135)
                tc = Vector((c.x + s * f(dx) * (1.0 if not big else 1.05), toe.y + f(0.045 - 0.006 * k),
                             self.h(0.012) + rr))
                tt = tc + Vector((s * f(dx) * 0.2, -f(0.026 - 0.003 * k), -rr * 0.35))
                tip, d = self.finger(bm, [tc + Vector((0, f(0.03), 0.004)), tc, tt], rr, taper=0.1)
                self.nail(bmn, tip, d, Vector((0, 0, 1)), rr * 0.95)
            self.organic(self.part(f"Foot_{side}", bm, "Skin", f"foot_{side}", recalc=True), voxel=0.0055, smooth=5)
            self.part(f"ToeNails_{side}", bmn, "Nail", f"foot_{side}", recalc=True)
            if not sandal:
                return
            bm = bmesh.new()
            K.bm_blob(bm, (c.x, c.y - f(0.008), self.h(0.008)), (f(0.068), f(0.115), self.h(0.009)), cut_below=0.0,
                      seg=24, ring=8)
            self.part(f"SandalSole_{side}", bm, "Weave", f"foot_{side}", recalc=True)
            bm = bmesh.new()
            an = self.J("ankle", s)
            K.bm_torus(bm, an + Vector((0, 0, self.h(0.012))), (0, 0, 1),
                       self.h(self.s["leg_r"]) * 0.82 + self.h(0.006), self.h(0.007), seg=24, ring=6)
            K.bm_torus(bm, (c.x, c.y - f(0.025), f(0.026)), (0, 1, 0.3), f(0.046), self.h(0.0065), seg=24, ring=6,
                       squash=1.3)
            for sg in (1, -1):
                p0 = Vector((c.x + sg * f(0.05), c.y + f(0.02), self.h(0.012)))
                K.bm_tube(bm, [p0, p0.lerp(an, 0.5) + Vector((0, 0, self.h(0.01))),
                               an + Vector((sg * f(0.02), 0, self.h(0.012)))], self.h(0.005), segs=6, squash=0.5)
            self.part(f"SandalStraps_{side}", bm, "Leather", f"foot_{side}", recalc=True)

        def wrist_band(self, s, mat="Weave", t=0.80, extra=1.0):
            side = "l" if s > 0 else "r"
            el, wr = self.J("elbow", s), self.J("wrist", s)
            ra = self.h(self.s["arm_r"])
            bm = bmesh.new()
            K.bm_torus(bm, el.lerp(wr, t), wr - el, ra * 0.92 * extra + self.h(0.006), self.h(0.012), seg=28, ring=10,
                       squash=1.6)
            self.part(f"Wristband_{side}", bm, mat, f"arm_{side}", recalc=True)
            bm = bmesh.new()
            r = self.h(0.0045)
            K.bm_helix(bm, el.lerp(wr, t - 0.22), el.lerp(wr, t - 0.08), ra * 1.0 * extra + r, r, 2.5, segs=6)
            self.part(f"WristLace_{side}", bm, "Leather", f"arm_{side}", recalc=True)

    # ======================================================================== GUERRIER COCO
    class WarriorReal(RealCoco):
        MATS = staticmethod(warrior_mats)

        def __init__(self, coll):
            super().__init__("coco", coll, 20261008)

        def build(self):
            H, B = self.H, self.body
            m = self.s["mask"]
            hwm, hhm = m["hw"] * H, m["hh"] * H
            cz = m["cz"] * H

            def under_mask(th, z):
                p = B.surf(th, z)
                u, v = p.x / hwm, (z - cz) / hhm
                return p.y < 0 and abs(u) < 1.08 and abs(v) < 1.08

            self.shell(88, 66)
            self.fibres(84, skip=under_mask)
            self.mask_wood()
            self.topknot()
            self.crown()
            self.necklace()
            self.pagne()
            for s in (1, -1):
                self.limb(s, muscle=1.15)
                if s < 0:
                    self.hand_fist(s, rs=0.018)
                else:
                    self.hand_open(s)
                self.foot_real(s)
                self.wrist_band(s)
                self.armband(s)
            self.spear()
            return self

        # -------------------------------------------------------- masque en bois sculpté
        def mask_wood(self):
            base, rnd = self.mask, self.rnd
            mk = ReliefMask(base, tiki_relief(self.H))
            self.mask_relief = mk
            thick = self.h(0.032)
            fz = thick * 0.6                     # face avant de la planche (le dos s'enfonce de 0,4 x thick)
            self.mask_front = fz
            outline = K.resample(K.superellipse(1.0, 1.0, 4.6, k=96), 128)
            outline = [(u * (1.0 + 0.04 * (v < 0)), v) for u, v in outline]
            bm = bmesh.new()
            K.bm_patch(bm, mk, outline, extra=fz, nr=26)
            plate = self.part("Mask", bm, "Wood", "body", solidify=thick)
            plate.modifiers["Solidify"].use_even_offset = True
            eye = self.angry_eye(0.40, 0.22, 0.25, 0.155, (0.12, 0.22), (0.70, 0.44))
            mouth = K.superellipse(0.60, 0.27, 5.0, 0.0, -0.55, k=48)
            bm = bmesh.new()
            for poly in (eye, self.mirrored(eye), mouth):
                K.bm_prism(bm, base, poly, -thick, fz + self.h(0.06))
            self.cutter("MaskCutter", bm, [plate])
            # fond sombre (orbites, bouche) juste devant la coque, et yeux luisants enfoncés
            bm = bmesh.new()
            for poly, sc in ((eye, 1.3), (self.mirrored(eye), 1.3), (mouth, 1.15)):
                c = (sum(p[0] for p in poly) / len(poly), sum(p[1] for p in poly) / len(poly))
                big = [(c[0] + (u - c[0]) * sc, c[1] + (v - c[1]) * sc) for u, v in poly]
                K.bm_patch(bm, base, big, extra=-self.h(0.0065), nr=4, center=c)
            self.part("MaskCavity", bm, "Dark", "body")
            bm = bmesh.new()
            self.eye_points = []
            for sg in (1, -1):
                c = base.pt(sg * 0.38, 0.24, -self.h(0.006))
                K.bm_blob(bm, c, (self.h(0.028), self.h(0.018), self.h(0.022)), seg=16, ring=10)
                self.eye_points.append(c)
            self.part("Eyes", bm, "Eye", "body")
            # gravures incisées remplies de chaux (disposition de l'original), à peine saillantes
            bm = bmesh.new()
            r = self.h(0.0055)
            ex = fz - r * 0.15
            for sg in (1, -1):
                self.stroke(bm, [(sg * 0.12, 0.42), (sg * 0.42, 0.53), (sg * 0.74, 0.63)], r * 1.1, extra=ex,
                            squash=0.35, mask=mk)
                for a0, a1 in (((0.16, 0.72), (0.22, 0.89)), ((0.36, 0.74), (0.47, 0.89)), ((0.56, 0.75), (0.71, 0.86))):
                    self.stroke(bm, [(sg * a0[0], a0[1]), (sg * a1[0], a1[1])], r * 0.85, extra=ex, squash=0.35, mask=mk)
                self.stroke(bm, [(sg * 0.66, -0.08), (sg * 0.80, -0.30)], r * 0.8, extra=ex, squash=0.35, mask=mk)
            self.stroke(bm, [(0, 0.92), (0, 0.66), (0, 0.46)], r, extra=ex, squash=0.35, mask=mk)
            self.stroke(bm, [(-0.15, -0.01), (0, 0.10), (0.15, -0.01)], r, extra=ex, squash=0.35, mask=mk)
            frame = K.resample(K.superellipse(0.70, 0.36, 6.0, 0.0, -0.55), 48)
            self.stroke(bm, frame, r * 1.15, extra=ex, closed=True, squash=0.35, mask=mk)
            self.part("MaskCarve", bm, "Lime", "body", recalc=True)
            # dents de requin plantées dans les bords de la bouche
            bm = bmesh.new()
            for i in range(5):
                u = -0.46 + 0.23 * i + rnd.uniform(-0.02, 0.02)
                b0 = mk.pt(u, -0.30, fz - thick * 0.5)
                down = base.pt(u, -0.6, 0) - base.pt(u, -0.3, 0)
                bm_shark_tooth(bm, b0, down, base.nrm(u, -0.3), self.h(0.07) * rnd.uniform(0.85, 1.1),
                               self.h(0.05), self.h(0.012))
            for i in range(4):
                u = -0.36 + 0.24 * i + rnd.uniform(-0.02, 0.02)
                b0 = mk.pt(u, -0.80, fz - thick * 0.5)
                up = base.pt(u, -0.5, 0) - base.pt(u, -0.8, 0)
                bm_shark_tooth(bm, b0, up, base.nrm(u, -0.8), self.h(0.06) * rnd.uniform(0.85, 1.1),
                               self.h(0.045), self.h(0.011))
            self.part("Teeth", bm, "Teeth", "body", smooth=False, recalc=True)
            # chevilles et ligatures qui tiennent le masque sur la coque
            bm, bmp = bmesh.new(), bmesh.new()
            B = self.body
            for v0 in (0.55, -0.45):
                z = base.cz + v0 * base.hh
                th0 = B.theta_at(base.hw * 1.02, z)
                pts = [B.surf(th0 + (TAU - 2 * th0) * k / 40, z + self.h(0.004) * math.sin(k * 0.9), self.h(0.004))
                       for k in range(41)]
                K.bm_tube(bm, pts, self.h(0.0045), segs=6, caps=True)
                for sg in (1, -1):
                    K.bm_blob(bmp, mk.pt(sg * 0.97, v0, fz), (self.h(0.012),) * 3, seg=10, ring=6)
            self.part("MaskLashing", bm, "Rope", "body", recalc=True)
            self.part("MaskPegs", bmp, "Bone", "body", recalc=True)

        # -------------------------------------------------------- chignon de coir et épingles d'os (os crest)
        def topknot(self):
            rnd = self.rnd
            top = self.body.zt
            bm = bmesh.new()
            for i in range(40):
                a = TAU * i / 40 + rnd.uniform(-0.1, 0.1)
                r0 = self.h(0.03) * rnd.uniform(0.2, 1.0)
                base = Vector((r0 * math.cos(a), r0 * math.sin(a), top - self.h(0.03)))
                tip = base + Vector((0.4 * math.cos(a) * self.h(0.03), 0.4 * math.sin(a) * self.h(0.03),
                                     self.h(rnd.uniform(0.10, 0.15))))
                mid = base.lerp(tip, 0.55) - Vector((math.cos(a), math.sin(a), 0)) * self.h(0.012)
                K.bm_tube(bm, K.catmull([base, mid, tip], 7), [self.h(0.0075) * (1 - 0.8 * j / 6) for j in range(7)],
                          segs=5)
            self.part("Topknot", bm, "Fibre", "crest", recalc=True)
            bm = bmesh.new()
            K.bm_helix(bm, (0, 0, top + self.h(0.005)), (0, 0, top + self.h(0.045)), self.h(0.03), self.h(0.0055), 4.0,
                       segs=6)
            self.part("TopknotWrap", bm, "Rope", "crest", recalc=True)
            bm = bmesh.new()
            for a in (0.3, 2.4, 4.4):
                base = Vector((math.cos(a) * self.h(0.012), math.sin(a) * self.h(0.012), top + self.h(0.06)))
                K.bm_horn(bm, base - Vector((math.cos(a), math.sin(a), 1.4)).normalized() * self.h(0.04),
                          (math.cos(a) * 0.6, math.sin(a) * 0.6, 1.0), self.h(0.10), self.h(0.007), segs=8, n=8,
                          r_tip=self.h(0.001))
            self.part("BonePins", bm, "Bone", "crest", recalc=True)

        def crown(self):
            """Couronne de cauris (remplace les pompons blancs de l'original)."""
            B = self.body
            top = B.zt
            zr = top - self.h(0.035)
            rr = B.rx * B.f(zr) * 0.95
            bm, bmc = bmesh.new(), bmesh.new()
            for i in range(10):
                a = TAU * i / 10 + 0.2
                sc = 1.0 + self.rnd.uniform(-0.1, 0.1)
                c = Vector((rr * math.cos(a), rr * math.sin(a) * B.ry / B.rx, zr + self.h(0.012)))
                bm_cowrie(bm, c, Vector((-math.sin(a), math.cos(a), 0.0)), Vector((math.cos(a), math.sin(a), 0.6)),
                          self.h(0.07) * sc)
            ring = [Vector((rr * 0.97 * math.cos(TAU * i / 64), rr * 0.97 * math.sin(TAU * i / 64) * B.ry / B.rx,
                            zr + self.h(0.006))) for i in range(65)]
            K.bm_tube(bmc, ring, self.h(0.004), segs=6, caps=False)
            self.part("Crown", bm, "Shell", "crest", recalc=True)
            self.part("CrownCord", bmc, "Rope", "crest", recalc=True)

        def necklace(self):
            """Collier de coquillages sous le masque : cordon incliné, cauris, dent de requin."""
            B = self.body
            bm, bms, bmt = bmesh.new(), bmesh.new(), bmesh.new()

            def zc(th):
                return self.h(0.305) + self.h(0.075) * (1 - math.cos(th)) * 0.5

            pts = [B.surf(TAU * k / 72, zc(TAU * k / 72), self.h(0.007)) for k in range(73)]
            K.bm_tube(bm, pts, self.h(0.0045), segs=6, caps=False)
            for th in (-0.75, -0.5, -0.25, 0.25, 0.5, 0.75):
                p = B.surf(th, zc(th) - self.h(0.03), self.h(0.012))
                bm_cowrie(bms, p, Vector((0, 0, -1)), B.normal(th, zc(th)), self.h(0.045))
                K.bm_tube(bm, [B.surf(th, zc(th), self.h(0.007)), p + Vector((0, 0, self.h(0.02)))],
                          self.h(0.0025), segs=5)
            p = B.surf(0.0, zc(0.0) - self.h(0.025), self.h(0.014))
            bm_shark_tooth(bmt, p + Vector((0, 0, self.h(0.01))), (0, 0, -1), B.normal(0, zc(0)), self.h(0.085),
                           self.h(0.055), self.h(0.012))
            K.bm_torus(bm, p + Vector((0, 0, self.h(0.012))), (0, 1, 0), self.h(0.012), self.h(0.003), seg=12, ring=5)
            self.part("Necklace", bm, "Rope", "body", recalc=True)
            self.part("NecklaceShells", bms, "Shell", "body", recalc=True)
            self.part("NecklaceTooth", bmt, "Teeth", "body", smooth=False, recalc=True)

        def pagne(self):
            """Pagne : ceinture tressée sur le bas de la coque, lanières de pandanus séché qui pendent (UV)."""
            B, rnd = self.body, self.rnd
            zt_, zb_ = self.h(0.27), self.h(0.12)
            bm = bmesh.new()
            ring = [B.surf(TAU * i / 72, zt_, self.h(0.01)) for i in range(73)]
            K.bm_tube(bm, ring, self.h(0.016), segs=10, caps=False, squash=0.55,
                      ups=[B.normal(TAU * i / 72, zt_) for i in range(73)])
            self.part("PagneBelt", bm, "Weave", "skirt", recalc=True)
            bm = bmesh.new()
            n = 52
            for i in range(n):
                th = TAU * (i + rnd.uniform(-0.35, 0.35)) / n
                ln = rnd.uniform(0.72, 1.22)
                wd = rnd.uniform(0.75, 1.25)
                layer = self.h(0.006) * (i % 2)
                pts, wid, nrm = [], [], []
                for k in range(7):
                    t = k / 6
                    z = zt_ - (zt_ - zb_) * t * ln
                    r = B.rx * B.f(zt_) + self.h(0.016) + layer + self.h(0.05) * t ** 1.2
                    ry = B.ry * B.f(zt_) + self.h(0.016) + layer + self.h(0.05) * t ** 1.2
                    tw = th + 0.03 * math.sin(i * 1.7) * t
                    pts.append(Vector((r * math.sin(tw), -ry * math.cos(tw), z)))
                    wid.append(self.h(0.036) * wd * (1.0 - 0.45 * t))
                    nrm.append(Vector((math.sin(th), -math.cos(th), 0.0)))
                bm_ribbon(bm, pts, wid, nrm, fold=0.15, curl=rnd.uniform(0.0, 0.5))
            ob = self.part("PagneStrips", bm, "Strip", "skirt", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = self.h(0.003)

        def armband(self, s):
            side = "l" if s > 0 else "r"
            sh, el = self.J("shoulder", s), self.J("elbow", s)
            ra = self.h(self.s["arm_r"])
            bm = bmesh.new()
            r = self.h(0.0045)
            K.bm_helix(bm, sh.lerp(el, 0.42), sh.lerp(el, 0.58), ra * 1.18 + r, r, 3.0, segs=6)
            self.part(f"Armband_{side}", bm, "Leather", f"arm_{side}", recalc=True)

        # -------------------------------------------------------- lance (espace de prise : origine = poignée, +Z)
        def spear(self):
            rnd = self.rnd
            bm = bmesh.new()
            n = 26
            z0, z1 = -0.30, 0.70
            pts = [Vector((0.007 * math.sin(k * 0.9) + 0.003 * math.sin(k * 2.7), 0.005 * math.cos(k * 0.7),
                           z0 + (z1 - z0) * k / (n - 1))) for k in range(n)]
            radii = [(0.017 - 0.003 * k / (n - 1)) * (1.0 + 0.08 * math.sin(k * 2.3) + rnd.uniform(-0.03, 0.03))
                     for k in range(n)]
            K.bm_tube(bm, pts, radii, segs=10)
            for z in (-0.18, 0.05, 0.36):
                a = rnd.uniform(0, TAU)
                K.bm_blob(bm, (0.012 * math.cos(a), 0.012 * math.sin(a), z), (0.011, 0.011, 0.016), seg=8, ring=6)
            self.part("Spear_Shaft", bm, "Shaft", "weapon", "weapon", recalc=True)
            bm = bmesh.new()
            K.bm_horn(bm, (0, 0, -0.30), (0, 0.02, -1), 0.06, 0.012, segs=8, n=6, r_tip=0.002)
            self.part("Spear_Butt", bm, "Bone", "weapon", "weapon", recalc=True)
            # lame de silex en feuille, taillée par éclats (enveloppes convexes)
            bm = bmesh.new()
            zb, zt = 0.62, 1.06
            nb = 13
            for k in range(nb - 1):
                pts = []
                for t in (k / (nb - 1), (k + 1) / (nb - 1)):
                    z = zb + (zt - zb) * t
                    w = 0.048 * math.sin(math.pi * (0.10 + 0.90 * t) ** 0.8) + 0.002
                    th = 0.011 * (1.0 - t) ** 0.5 + 0.0015
                    pts += [Vector((w, 0.0, z)), Vector((-w, 0.0, z)), Vector((0.0, th, z)), Vector((0.0, -th, z)),
                            Vector((w * 0.5, th * 0.78 * rnd.uniform(0.8, 1.1), z)),
                            Vector((-w * 0.5, -th * 0.78 * rnd.uniform(0.8, 1.1), z)),
                            Vector((-w * 0.5, th * 0.7, z)), Vector((w * 0.5, -th * 0.7, z))]
                K.bm_hull(bm, pts)
            K.bm_hull(bm, [Vector((0, 0, zt + 0.024))] + [Vector((0.005 * math.cos(a), 0.0025 * math.sin(a), zt - 0.01))
                                                          for a in (0, 1.6, 3.1, 4.7)])
            self.part("Spear_Blade", bm, "Flint", "weapon", "weapon", smooth=False, recalc=True)
            # dents de requin serties le long des bords (les « dents de pierre » de l'original)
            bm = bmesh.new()
            for sgn in (1, -1):
                for i in range(6):
                    t = 0.14 + 0.13 * i
                    z = zb + (zt - zb) * t
                    w = 0.048 * math.sin(math.pi * (0.10 + 0.90 * t) ** 0.8)
                    bm_shark_tooth(bm, Vector((sgn * (w - 0.004), 0, z)), (sgn, 0, 0.35), (0, 1, 0), 0.026, 0.018,
                                   0.006)
            self.part("Spear_Teeth", bm, "Teeth", "weapon", "weapon", smooth=False, recalc=True)
            bm = bmesh.new()
            K.bm_helix(bm, (0, 0, 0.555), (0, 0, 0.665), 0.0195, 0.0048, 7.0, segs=6)
            K.bm_helix(bm, (0, 0, -0.08), (0, 0, 0.10), 0.0185, 0.0048, 8.0, segs=6)
            self.part("Spear_Coir", bm, "Rope", "weapon", "weapon", recalc=True)
            bm = bmesh.new()
            K.bm_helix(bm, (0, 0, 0.575), (0, 0, 0.645), 0.026, 0.004, 2.0, segs=6, phase=1.2)
            for tail in ([Vector((0.022, -0.004, 0.60)), Vector((0.03, -0.01, 0.52)), Vector((0.034, -0.004, 0.44))],
                         [Vector((0.018, 0.01, 0.59)), Vector((0.026, 0.018, 0.53)), Vector((0.022, 0.02, 0.47))]):
                C = K.catmull(tail, 10)
                K.bm_tube(bm, C, [0.0035 - 0.0012 * k / 9 for k in range(10)], segs=5, squash=0.5)
            self.part("Spear_Leather", bm, "Leather", "weapon", "weapon", recalc=True)
            # deux plumes noires attachées sous la lame
            bm, bmr = bmesh.new(), bmesh.new()
            for a, ln in ((0.5, 0.16), (2.6, 0.14)):
                base = Vector((0.03 * math.cos(a), 0.03 * math.sin(a), 0.56))
                d = Vector((0.25 * math.cos(a), 0.25 * math.sin(a), -1.0)).normalized()
                pts = [base + d * (ln * k / 7) + Vector((0.004 * math.sin(k), 0, 0)) for k in range(8)]
                nrm = [Vector((math.cos(a + 1.57), math.sin(a + 1.57), 0))] * 8
                wid = [0.004 + 0.03 * math.sin(math.pi * min(1.0, 0.15 + k / 7)) for k in range(8)]
                bm_ribbon(bm, pts, wid, nrm, fold=0.2)
                K.bm_tube(bmr, pts, 0.0015, segs=4)
            ob = self.part("Spear_Feathers", bm, "Feather", "weapon", "weapon")
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = 0.0015
            self.part("Spear_Quills", bmr, "Bone", "weapon", "weapon")

    return RealCoco, WarriorReal
