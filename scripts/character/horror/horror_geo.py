"""Géométrie « Outlast » des trois Veilleurs (T-274) : Guerrier Coco, Gardien Crabe, Veilleur des Cendres.

Départ : les meilleurs modèles existants. Le Guerrier Coco reprend le modèle réaliste du 2e PC (WarriorReal, intégré le
2026-10-08) ; le Gardien Crabe et le Veilleur des Cendres, encore stylisés (tiki du 2026-10-03), sont refaits sur la
même base réaliste (RealCoco : coque irrégulière, membres organiques remaillés, mains et pieds détaillés).
Mêmes SPECS tiki (H, articulations, œuf du corps, masque) : squelette identique os pour os, réimport en place.

Direction artistique (demande du propriétaire, 2026-10-10 : « un design comme Outlast ») :
  - Guerrier Coco : bourre pourrissante lacérée (trois griffures et une entaille, chair fibreuse crème, sève ambrée
    séchée qui coule, fibres arrachées), masque fendu à l'ocre délavée, dents jaunies dont une cassée et une
    manquante, yeux injectés, membres plus maigres, bandages souillés, pagne déchiqueté ;
  - Gardien Crabe : carapace sombre bosselée et bleuie, balanes, corail incrusté, vase noire, algues qui pendent,
    masque de bois flotté rongé (pigment délavé dans les incisions, sel), dents en éclats de coquillage, yeux laiteux,
    pattes de chitine maigres et segmentées à épines, pinces dentelées, marteau en bois flotté et coquillage encroûté ;
  - Veilleur des Cendres : croûte de charbon en plaques, failles où couvent des braises (attribut Fissure), éclats
    d'obsidienne sur le crâne, masque de planches calcinées clouées, yeux de braise au fond des orbites, dents
    d'obsidienne, membres maigres de chair brûlée, griffes vitreuses, bandes de tissu roussi, massue de basalte à
    failles de braise hérissée d'obsidienne.
Motifs : uniquement inventés (traits, chevrons, points), aucun motif sacré réel ; pas de chair humaine.
"""
import math
import random

import bmesh
import bpy  # noqa: F401
from mathutils import Matrix, Vector

from . import horror_mats as HM
from . import real_geo
from .base import T, mat_bone, mat_leather
from .real_geo import ReliefMask, bm_ribbon, tiki_relief

TAU = 2 * math.pi


# ------------------------------------------------------------------ jeux de matériaux
def warrior_horror_mats(n):
    return {
        "Husk": HM.husk_rot(f"MAT-{n}_Husk"),
        "Fibre": real_geo.mat_fibre(f"MAT-{n}_Fibre", dark="140A04", light="5A3E22"),
        "Wood": HM.wood_mask_faded(f"MAT-{n}_MaskWood"),
        "Lime": HM.lime_faded(f"MAT-{n}_Lime"),
        "Dark": real_geo.mat_dark(f"MAT-{n}_Cavity"),
        "Eye": HM.eye_bloodshot(f"MAT-{n}_Eye"),
        "Teeth": HM.tooth_yellow(f"MAT-{n}_SharkTooth"),
        "Skin": HM.skin_grim(f"MAT-{n}_Skin"),
        "Nail": real_geo.mat_nail(f"MAT-{n}_Nail", light="8A7A60", dark="2A2014"),
        "Weave": real_geo.mat_weave(f"MAT-{n}_Weave", light="7A6A44", dark="2A2210"),
        "Strip": HM.pandanus_rot(f"MAT-{n}_Pandanus"),
        "Rope": HM.rotten_rope(f"MAT-{n}_Rope", dark="1A120A", light="6A5434", algae="1A1A0C"),
        "Leather": mat_leather(f"MAT-{n}_Leather"),
        "Shell": HM.cowrie_dirty(f"MAT-{n}_Cowrie"),
        "Bone": mat_bone(f"MAT-{n}_Bone", light="B8A888", dark="7A6A4A", dirt="2A2014"),
        "Shaft": real_geo.mat_wood_raw(f"MAT-{n}_SpearWood"),
        "Flint": HM.flint_sap(f"MAT-{n}_Flint"),
        "Feather": real_geo.mat_feather(f"MAT-{n}_Feather"),
        "Bandage": HM.bandage(f"MAT-{n}_Bandage"),
        "DeadX": real_geo.mat_lime(f"MAT-{n}_DeadX", base="F2EEE0"),
    }


def crab_horror_mats(n):
    return {
        "Husk": HM.crab_shell(f"MAT-{n}_Carapace"),
        "Limb": HM.crab_limb(f"MAT-{n}_Chitin"),
        "Tip": HM.crab_claw_tip(f"MAT-{n}_ClawTip"),
        "Coral": HM.coral_crust(f"MAT-{n}_Coral"),
        "Barnacle": HM.barnacle(f"MAT-{n}_Barnacle"),
        "Wood": HM.driftwood_mask(f"MAT-{n}_Driftwood"),
        "Carve": HM.salt_groove(f"MAT-{n}_SaltGroove"),
        "Dark": real_geo.mat_dark(f"MAT-{n}_Cavity", col="0A0807"),
        "Eye": HM.milky_eye(f"MAT-{n}_Eye"),
        "Teeth": HM.shell_shard(f"MAT-{n}_ShellShard"),
        "Rope": HM.rotten_rope(f"MAT-{n}_Rope"),
        "Kelp": HM.kelp(f"MAT-{n}_Kelp"),
        "Handle": HM.driftwood_handle(f"MAT-{n}_Driftwood_Handle"),
        "Conch": HM.conch(f"MAT-{n}_Conch"),
        "Peg": mat_bone(f"MAT-{n}_Peg", light="4A4238", dark="2A241C", dirt="0A0806"),
    }


def ash_horror_mats(n):
    return {
        "Husk": HM.ash_crust(f"MAT-{n}_Crust"),
        "Skin": HM.ash_skin(f"MAT-{n}_BurntSkin"),
        "Nail": HM.obsidian(f"MAT-{n}_Claw", ash_amt=0.35),
        "Obsidian": HM.obsidian(f"MAT-{n}_Obsidian"),
        "Wood": HM.char_wood(f"MAT-{n}_CharPlank"),
        "Smear": HM.ash_smear(f"MAT-{n}_AshSmear"),
        "Dark": real_geo.mat_dark(f"MAT-{n}_Cavity", col="060404"),
        "Eye": HM.ember_eye(f"MAT-{n}_EmberEye"),
        "Teeth": HM.obsidian(f"MAT-{n}_ObsidianTooth", ash_amt=0.2),
        "Rope": HM.scorched_rope(f"MAT-{n}_ScorchedRope"),
        "Cloth": HM.char_cloth(f"MAT-{n}_CharCloth"),
        "Peg": HM.obsidian(f"MAT-{n}_Peg", ash_amt=0.5),
        "Handle": HM.char_handle(f"MAT-{n}_CharHandle"),
        "Lava": HM.lava_rock(f"MAT-{n}_LavaRock"),
    }


# ------------------------------------------------------------------ primitives
def frame_of(n):
    n = Vector(n).normalized()
    a = n.orthogonal().normalized()
    return n, a, n.cross(a)


def bm_barnacle(bm, p, n, r, h, rnd, seg=9):
    """Balane : cône tronqué en plaques irrégulières, cratère creusé ; la base s'enfonce dans le support."""
    n, a, b = frame_of(n)
    p = Vector(p) - n * (0.3 * h)
    prof = [(1.0, 0.0), (0.96, 0.32), (0.72, 0.8), (0.54, 1.0), (0.44, 0.95), (0.33, 0.6)]
    ph = rnd.uniform(0, TAU)
    lean = (a * rnd.uniform(-0.2, 0.2) + b * rnd.uniform(-0.2, 0.2)) * h
    rows = []
    for rf, hf in prof:
        ring = []
        for i in range(seg):
            ang = TAU * i / seg + ph
            jit = 1.0 + 0.11 * math.sin(ang * 3 + ph) + rnd.uniform(-0.05, 0.05)
            ring.append(bm.verts.new(p + (a * math.cos(ang) + b * math.sin(ang)) * (r * rf * jit) + n * (h * hf)
                                     + lean * hf))
        rows.append(ring)
    for k in range(len(rows) - 1):
        for i in range(seg):
            i2 = (i + 1) % seg
            bm.faces.new((rows[k][i], rows[k][i2], rows[k + 1][i2], rows[k + 1][i]))
    c = bm.verts.new(p + n * (h * 0.5) + lean * 0.5)
    for i in range(seg):
        bm.faces.new((rows[-1][i], rows[-1][(i + 1) % seg], c))


def bm_coral(bm, p, n, r, rnd, count=14, branches=3):
    """Encroûtement de corail : amas de nodules bosselés et quelques courtes branches."""
    K = T["kit"]
    n, a, b = frame_of(n)
    for _ in range(count):
        ang = rnd.uniform(0, TAU)
        rad = r * math.sqrt(rnd.random())
        s = r * rnd.uniform(0.16, 0.34) * (1.0 - 0.4 * rad / r)
        c = Vector(p) + (a * math.cos(ang) + b * math.sin(ang)) * rad + n * (s * rnd.uniform(-0.1, 0.5))
        K.bm_blob(bm, c, (s, s * rnd.uniform(0.8, 1.2), s * rnd.uniform(0.55, 0.9)), rot=K.look_rot(n), seg=10, ring=7)
    for _ in range(branches):
        ang = rnd.uniform(0, TAU)
        c = Vector(p) + (a * math.cos(ang) + b * math.sin(ang)) * (r * 0.5 * rnd.random())
        d = (n + a * rnd.uniform(-0.6, 0.6) + b * rnd.uniform(-0.6, 0.6)).normalized()
        K.bm_horn(bm, c, d, r * rnd.uniform(0.6, 1.1), r * 0.15, bend=a * rnd.uniform(-1, 1) + b * rnd.uniform(-1, 1),
                  bend_amt=0.35, segs=7, n=6, r_tip=r * 0.07, power=0.6)


def bm_strand(bm, root, out, length, width, rnd, n=8, swing=0.18):
    """Lanière qui pend (algue, bande effilochée) : part vers out puis tombe ; UV v le long."""
    out = Vector(out)
    out.z = 0.0
    out = out.normalized() if out.length > 1e-6 else Vector((0, -1, 0))
    side = out.cross(Vector((0, 0, 1))).normalized()
    pts, wid, nrm = [], [], []
    ph = rnd.uniform(0, TAU)
    for k in range(n):
        t = k / (n - 1)
        p = (Vector(root) + out * (length * swing * math.sin(math.pi * 0.5 * min(1.0, t * 2.2)))
             + Vector((0, 0, -length * t)) + side * (0.05 * length * math.sin(t * 4.0 + ph)))
        pts.append(p)
        wid.append(width * (0.55 + 0.45 * math.sin(math.pi * min(1.0, 0.18 + t * 0.9))) * rnd.uniform(0.85, 1.1))
        nrm.append(out)
    bm_ribbon(bm, pts, wid, nrm, fold=0.12, cols=3, curl=rnd.uniform(0.0, 0.35))
    return pts[-1]


def bm_wrap(bm, a, b, R, width, turns, rnd, wobble=0.05):
    """Bande enroulée en hélice autour de a-b (bandage, tissu) : ruban posé sur le cylindre de rayon R."""
    a, b = Vector(a), Vector(b)
    ax = b - a
    L = ax.length
    ax.normalize()
    u = ax.orthogonal().normalized()
    v = ax.cross(u)
    n = max(14, int(turns * 20))
    pts, wid, nrm = [], [], []
    ph = rnd.uniform(0, TAU)
    for k in range(n + 1):
        t = k / n
        ang = TAU * turns * t + ph
        radial = u * math.cos(ang) + v * math.sin(ang)
        rr = R * (1.0 + wobble * math.sin(ang * 3 + ph))
        pts.append(a + ax * (L * t) + radial * rr)
        wid.append(width * rnd.uniform(0.85, 1.12))
        nrm.append(radial)
    bm_ribbon(bm, pts, wid, nrm, fold=0.0, cols=3)
    return pts[-1], nrm[-1]


def bm_shard(bm, base, d, length, w, rnd, pts=9, flat=0.55):
    """Éclat vitreux (obsidienne, coquillage) : enveloppe convexe effilée, facettes nettes."""
    d = Vector(d).normalized()
    a = d.orthogonal().normalized()
    b = d.cross(a)
    P = []
    for k in range(pts):
        t = rnd.uniform(0.0, 0.85)
        ang = rnd.uniform(0, TAU)
        rr = w * (1.0 - t) ** 1.1 * rnd.uniform(0.55, 1.0)
        P.append(Vector(base) + d * (length * t) + (a * math.cos(ang) + b * math.sin(ang) * flat) * rr)
    P.append(Vector(base) + d * length + a * rnd.uniform(-0.15, 0.15) * w)
    T["kit"].bm_hull(bm, P)


def poly_span(poly, u):
    """(bas, haut) de la droite verticale u dans le polygone (u, v)."""
    vs = []
    n = len(poly)
    for i in range(n):
        (u1, v1), (u2, v2) = poly[i], poly[(i + 1) % n]
        if (u1 - u) * (u2 - u) <= 0 and abs(u2 - u1) > 1e-9:
            vs.append(v1 + (v2 - v1) * (u - u1) / (u2 - u1))
    return (min(vs), max(vs)) if vs else None


def crab_relief(H):
    """Relief du masque de bois flotté du Gardien Crabe : orbites hautes et creusées, arcades lourdes, arête du nez,
    lèvre de la bouche ; surface rongée (creux de vers marins)."""
    base = tiki_relief(H)

    def f(u, v):
        return base(u, v - 0.06)

    return f


def ash_relief(H):
    return lambda u, v: 0.0


# ------------------------------------------------------------------ classes
def make_classes():
    RealCoco, WarriorReal = real_geo.make_classes()
    K = T["kit"]

    class HorrorMixin:
        """Outils communs : plaies, bandes, pièces qui pendent, attributs de sommets de la coque."""

        def set_attr(self, ob, name, values):
            me = ob.data
            a = me.attributes.get(name) or me.attributes.new(name, "FLOAT", "POINT")
            a.data.foreach_set("value", values)

        def shell_points(self):
            me = self.shell_ob.data
            return [v.co.copy() for v in me.vertices]

        def theta_of(self, p):
            return math.atan2(p.x / max(self.body.rx, 1e-6), -p.y / max(self.body.ry, 1e-6))

        def cut(self, name, bm, targets):
            """Découpe booléenne contrôlée (voir check_booleans)."""
            ob = self.cutter(name, bm, targets)
            for t in targets:
                self.check_booleans(t)
            return ob

        def check_booleans(self, t):
            """Les plaques solidifiées sur l'œuf se recoupent légèrement : EXACT seul les vide (constaté sur les trois
            masques), FLOAT oublie parfois un trou (bouche du Veilleur). On garde le premier réglage plausible :
            EXACT avec auto-intersection, puis FLOAT, puis MANIFOLD ; plausible = au moins 70 % des faces d'avant,
            un changement réel et aucun sommet hors de la boîte d'avant (+ 1 cm)."""
            def measure():
                bpy.context.view_layer.update()
                dg = bpy.context.evaluated_depsgraph_get()
                me = t.evaluated_get(dg).data
                co = [v.co for v in me.vertices]
                lo = [min(c[i] for c in co) for i in range(3)] if co else [0, 0, 0]
                hi = [max(c[i] for c in co) for i in range(3)] if co else [0, 0, 0]
                return len(me.polygons), lo, hi

            bools = [m for m in t.modifiers if m.type == "BOOLEAN"]
            for m in bools:
                m.show_viewport = False
            ref, lo0, hi0 = measure()
            for m in bools:
                m.show_viewport = True
            for solver, self_x in (("EXACT", True), ("FLOAT", False), ("MANIFOLD", False)):
                for m in bools:
                    try:
                        m.solver = solver
                        if solver == "EXACT":
                            m.use_self = self_x
                    except (TypeError, AttributeError):
                        continue
                got, lo, hi = measure()
                inside = all(lo[i] >= lo0[i] - 0.01 and hi[i] <= hi0[i] + 0.01 for i in range(3))
                inside = inside and all(hi0[i] - lo0[i] < 2.0 for i in range(3))
                if got >= 0.7 * ref and got != ref and inside:
                    print(f"[horror] {t.name}: découpe {solver}{' (auto)' if self_x else ''} ({ref} -> {got} faces)")
                    return solver
                print(f"[horror] {t.name}: découpe {solver} rejetée ({ref} -> {got} faces, boîte {'ok' if inside else 'hors'})")
            raise RuntimeError(f"{t.name}: aucune découpe booléenne correcte")

        def bandage(self, s, a, b, R, turns, mat="Bandage", tag=None, width=None, tail=True):
            side = "l" if s > 0 else "r"
            rnd = self.rnd
            bm = bmesh.new()
            end, out = bm_wrap(bm, a, b, R, width or self.h(0.022), turns, rnd)
            if tail:
                bm_strand(bm, end, out, self.h(rnd.uniform(0.07, 0.11)), self.h(0.016), rnd, n=7, swing=0.25)
            ob = self.part(f"Wrap_{side}_{len(self.parts)}", bm, mat, tag or f"arm_{side}", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = self.h(0.0025)
            return ob

    # ======================================================================== GUERRIER COCO
    class WarriorHorror(HorrorMixin, WarriorReal):
        MATS = staticmethod(warrior_horror_mats)

        def __init__(self, coll):
            super().__init__(coll)
            self.rnd.seed(20261010)
            self.wounds = self.make_wounds()
            # la bourre lit ses plaies dans le shader (distance aux tracés) : matériau refait avec les tracés ; le
            # masque, ses coulures sous les orbites (centres des yeux du modèle réaliste : u = ±0,38, v = 0,24)
            self.custom["Husk"] = HM.husk_rot(f"MAT-{self.name}_Husk", wounds=self.wounds)
            rel, fz = tiki_relief(self.H), 0.6 * self.h(0.032)
            eyes = [self.mask.pt(sg * 0.40, 0.06, fz + rel(sg * 0.40, 0.06)) for sg in (1, -1)]
            self.custom["Wood"] = HM.wood_mask_faded(f"MAT-{self.name}_MaskWood", eyes=eyes)

        def make_wounds(self):
            """Tracés des plaies (espace objet, mètres) : trois griffures parallèles en travers du flanc avant gauche,
            une entaille profonde dans le dos, deux coupures sur le flanc droit."""
            B, H = self.body, self.H

            def path(pts, off=0.0):
                return [B.surf(th, z * H, off) for th, z in pts]

            out = []
            for k in range(3):                                          # griffures en travers du flanc avant gauche
                dz = -0.075 * k
                out.append(dict(pts=path([(1.32 + 0.03 * k, 0.70 + dz), (1.12, 0.62 + dz), (0.92, 0.535 + dz),
                                          (0.74, 0.46 + dz)]), w=0.013 * H, depth=0.007 * H))
            out.append(dict(pts=path([(math.pi - 0.8, 0.84), (math.pi - 0.48, 0.73), (math.pi - 0.16, 0.60),
                                      (math.pi + 0.14, 0.47)]), w=0.026 * H, depth=0.012 * H))
            out.append(dict(pts=path([(-1.2, 0.66), (-1.42, 0.57)]), w=0.012 * H, depth=0.005 * H))
            out.append(dict(pts=path([(-0.98, 0.50), (-0.84, 0.43), (-0.74, 0.36)]), w=0.014 * H, depth=0.006 * H))
            out.append(dict(pts=path([(-1.85, 0.46), (-2.05, 0.41), (-2.25, 0.385)]), w=0.012 * H, depth=0.006 * H))
            return out

        def wound_disp(self, th, z, p):
            d = 0.0
            for w in self.wounds:
                P = w["pts"]
                best = min(_seg_dist(p, P[i], P[i + 1]) for i in range(len(P) - 1))
                r = best / (w["w"] * 1.4)
                if r < 1.0:
                    d -= w["depth"] * (1.0 - r * r)
                elif r < 1.8:
                    d += 0.25 * w["depth"] * math.sin(math.pi * (r - 1.0) / 0.8)    # bord arraché, relevé
            return d

        def build(self):
            H, B = self.H, self.body
            m = self.s["mask"]
            hwm, hhm = m["hw"] * H, m["hh"] * H
            cz = m["cz"] * H

            def under_mask(th, z):
                p = B.surf(th, z)
                u, v = p.x / hwm, (z - cz) / hhm
                return p.y < 0 and abs(u) < 1.08 and abs(v) < 1.08

            self.shell(88, 66, disp=self.wound_disp)
            self.fibres(96, skip=under_mask)
            self.torn_fibres()
            self.mask_horror()
            self.topknot()
            self.crown()
            self.necklace()
            self.pagne_torn()
            for s in (1, -1):
                self.limb(s, muscle=0.78)
                if s < 0:
                    self.hand_fist(s, rs=0.018)
                else:
                    self.hand_open(s)
                self.foot_real(s)
                self.armband(s)
                if s < 0:
                    self.wrist_band(s)
            self.bandages()
            self.spear()
            return self

        def torn_fibres(self):
            """Mèches de bourre arrachées qui pendent au bord des plaies."""
            rnd = self.rnd
            bm = bmesh.new()
            for w in self.wounds:
                P = w["pts"]
                for i in range(len(P) - 1):
                    for _ in range(4):
                        t = rnd.random()
                        c = P[i].lerp(P[i + 1], t)
                        th = self.theta_of(c)
                        nn = self.body.normal(th, c.z)
                        along = (P[i + 1] - P[i]).normalized()
                        side = nn.cross(along).normalized() * (1 if rnd.random() < 0.5 else -1)
                        base = c + side * w["w"] * 1.3
                        ln = self.h(rnd.uniform(0.02, 0.05))
                        pts = [base, base + nn * ln * 0.4 - side * ln * 0.2, base + nn * ln * 0.6 + Vector((0, 0, -ln * 0.6))]
                        K.bm_tube(bm, K.catmull(pts, 6), [self.h(0.0022) * (1 - 0.8 * j / 5) for j in range(6)], segs=4)
            self.part("TornCoir", bm, "Fibre", "body", recalc=True)

        def mask_horror(self):
            """Masque du réaliste, fendu de haut en bas sur la droite, coin ébréché ; dents jaunies dont une cassée et
            une manquante."""
            orig = real_geo.bm_shark_tooth
            count = [0]

            def tooth(bm, base, down, out, h, w, t):
                count[0] += 1
                if count[0] == 2:                     # dent manquante (haut, deuxième)
                    return
                if count[0] == 4:                     # dent cassée
                    h *= 0.45
                if count[0] == 7:
                    h *= 0.7
                orig(bm, base, down, out, h, w, t)

            real_geo.bm_shark_tooth = tooth
            try:
                self.mask_wood()
            finally:
                real_geo.bm_shark_tooth = orig
            # fente : coin taillé en dents de scie du bord haut jusqu'à l'orbite droite
            base = self.mask
            plate = next(p for p in self.parts if p.name.split("_", 1)[-1].split(".")[0] == "Mask")
            crack = [(-0.30, 1.05), (-0.27, 0.86), (-0.33, 0.70), (-0.29, 0.55), (-0.34, 0.42)]
            poly = [(u - 0.012, v) for u, v in crack] + [(u + 0.012 + 0.006 * math.sin(i * 2.1), v)
                                                          for i, (u, v) in enumerate(crack[::-1])]
            bm = bmesh.new()
            K.bm_prism(bm, base, poly, -self.h(0.05), self.h(0.08))
            chip = [(0.78, 1.05), (1.05, 1.05), (1.05, 0.72), (0.93, 0.80), (0.86, 0.92)]
            K.bm_prism(bm, base, chip, -self.h(0.05), self.h(0.08))
            self.cut("MaskCrack", bm, [plate])

        def pagne_torn(self):
            """Pagne pourri : lanières de longueurs inégales, certaines arrachées, ourlet effiloché."""
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
                if i % 9 == 4 or i % 13 == 7:
                    continue                          # lanières arrachées
                th = TAU * (i + rnd.uniform(-0.35, 0.35)) / n
                ln = rnd.uniform(0.45, 1.25)
                wd = rnd.uniform(0.6, 1.2)
                layer = self.h(0.006) * (i % 2)
                pts, wid, nrm = [], [], []
                for k in range(7):
                    t = k / 6
                    z = zt_ - (zt_ - zb_) * t * ln
                    r = B.rx * B.f(zt_) + self.h(0.016) + layer + self.h(0.05) * t ** 1.2
                    ry = B.ry * B.f(zt_) + self.h(0.016) + layer + self.h(0.05) * t ** 1.2
                    tw = th + 0.05 * math.sin(i * 1.7) * t
                    pts.append(Vector((r * math.sin(tw), -ry * math.cos(tw), z)))
                    wid.append(self.h(0.034) * wd * (1.0 - 0.6 * t) * (0.6 + 0.4 * rnd.random()))
                    nrm.append(Vector((math.sin(th), -math.cos(th), 0.0)))
                bm_ribbon(bm, pts, wid, nrm, fold=0.15, curl=rnd.uniform(0.1, 0.8))
            ob = self.part("PagneStrips", bm, "Strip", "skirt", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = self.h(0.003)

        def bandages(self):
            """Avant-bras gauche et tibia droit bandés de lin souillé ; chiffon noué au genou gauche."""
            ra = self.h(self.s["arm_r"])
            rl = self.h(self.s["leg_r"])
            el, wr = self.J("elbow", 1), self.J("wrist", 1)
            self.bandage(1, el.lerp(wr, 0.18), el.lerp(wr, 0.86), ra * 1.06, 4.2)
            kn, an = self.J("knee", -1), self.J("ankle", -1)
            self.bandage(-1, kn.lerp(an, 0.12), kn.lerp(an, 0.80), rl * 1.0, 3.6, tag="leg_r")
            kl = self.J("knee", 1)
            self.bandage(1, kl + Vector((0, 0, self.h(0.02))), kl - Vector((0, 0, self.h(0.02))), rl * 1.08, 1.2,
                         tag="leg_l", width=self.h(0.03))

    # ======================================================================== GARDIEN CRABE
    class CrabHorror(HorrorMixin, RealCoco):
        MATS = staticmethod(crab_horror_mats)

        def __init__(self, coll):
            super().__init__("crab", coll, 20261010)
            rnd = self.rnd
            H, m = self.H, self.s["mask"]
            self.bumps = []
            while len(self.bumps) < 90:
                th = rnd.uniform(-math.pi, math.pi)
                z = H * rnd.uniform(0.30, 0.98)
                if abs(th) < 0.95 and abs(z - m["cz"] * H) < m["hh"] * H * 1.15:
                    continue
                self.bumps.append((th, z, H * rnd.uniform(0.014, 0.034), H * rnd.uniform(0.004, 0.016)))
            self.dents = [(rnd.uniform(1.0, 5.3), H * rnd.uniform(0.35, 0.9), H * rnd.uniform(0.02, 0.04))
                          for _ in range(7)]

        def husk_noise(self, th, z):
            H = self.H
            return (math.sin(th * 5 + z * 11) * 0.5 + math.sin(th * 9 - z * 19 + 1.1) * 0.3 +
                    math.sin(th * 17 + z * 33) * 0.2) * 0.0045 * H

        def carapace_disp(self, th, z, p):
            B, H = self.body, self.H
            rr = B.rx * B.f(z)
            d = 0.0
            for bt, bz, br, bh in self.bumps:
                dth = math.atan2(math.sin(th - bt), math.cos(th - bt))
                dd2 = (rr * dth) ** 2 + (z - bz) ** 2
                if dd2 < 9 * br * br:
                    d += bh * math.exp(-dd2 / (br * br))
            # crête dorsale et deux crêtes latérales (anciennes coutures turquoise devenues chitine)
            dth = math.atan2(math.sin(th - math.pi), math.cos(th - math.pi))
            if z > 0.40 * H:
                d += 0.010 * H * math.exp(-((rr * dth) / (0.022 * H)) ** 2)
            for sg in (1, -1):
                a = math.pi - sg * (1.25 * max(0.0, min(1.0, (z - 0.36 * H) / (0.5 * H))))
                da = math.atan2(math.sin(th - a), math.cos(th - a))
                if 0.36 * H < z < 0.88 * H:
                    d += 0.007 * H * math.exp(-((rr * da) / (0.018 * H)) ** 2)
            for dt, dz, dr in self.dents:                     # coups et éclats anciens
                dth2 = math.atan2(math.sin(th - dt), math.cos(th - dt))
                dd2 = (rr * dth2) ** 2 + (z - dz) ** 2
                if dd2 < 4 * dr * dr:
                    d -= 0.006 * H * math.exp(-dd2 / (dr * dr))
            return d

        def build(self):
            self.shell(112, 86, disp=self.carapace_disp)
            self.silt()
            self.horns()
            self.mask_driftwood()
            self.encrust()
            for s in (1, -1):
                self.crab_arm(s)
                self.chela(s, grip=s < 0)
                self.crab_leg(s)
                self.crab_foot(s)
                self.rope_bands(s)
            self.hammer()
            return self

        def silt(self):
            H = self.H
            vals = []
            for p in self.shell_points():
                n = 0.5 + 0.5 * math.sin(p.x * 9 + p.y * 5) * math.sin(p.z * 7 + p.x * 3)
                t = max(0.0, min(1.0, (0.46 * H + 0.05 * H * n - p.z) / (0.24 * H)))
                vals.append(t * t * (3 - 2 * t))
            self.set_attr(self.shell_ob, "Silt", vals)

        def horns(self):
            B, rnd = self.body, self.rnd
            bm, bmt = bmesh.new(), bmesh.new()
            for sg in (1, -1):
                th, z = sg * 0.78, self.h(0.90)
                base = B.surf(th, z, -self.h(0.012))
                n = B.normal(th, z)
                d = n * 0.6 + Vector((0, 0, 1.0))
                ln = self.h(0.16 if sg > 0 else 0.11)                 # la corne droite est brisée
                K.bm_horn(bm, base, d, ln, self.h(0.05), bend=Vector((sg, 0.3, 0)), bend_amt=0.35, segs=16, n=14,
                          r_tip=self.h(0.004 if sg > 0 else 0.016))
                if sg > 0:
                    tip = base + d.normalized() * ln + Vector((sg, 0.3, 0)).normalized() * 0.35 * ln
                    K.bm_horn(bmt, tip - d.normalized() * self.h(0.012), d, self.h(0.02), self.h(0.008), segs=8, n=4,
                              r_tip=self.h(0.001))
                for th2, z2, ln2, broken in ((1.45, 0.80, 0.07, 0), (1.5, 0.62, 0.06, 1), (2.2, 0.80, 0.065, 0),
                                             (2.6, 0.66, 0.055, 1), (1.05, 0.97, 0.05, 0), (2.9, 0.88, 0.05, 0)):
                    th2 = sg * th2
                    b2 = B.surf(th2, self.h(z2), -self.h(0.006))
                    dd = B.normal(th2, self.h(z2)) + Vector((0, 0, 0.3))
                    K.bm_horn(bm, b2, dd, self.h(ln2) * rnd.uniform(0.8, 1.15) * (0.55 if broken else 1.0),
                              self.h(0.026), segs=12, n=7, r_tip=self.h(0.012 if broken else 0.002))
            self.part("Horns", bm, "Husk", "body", recalc=True)
            self.part("HornTips", bmt, "Tip", "body", recalc=True)

        def mask_driftwood(self):
            base, rnd, H = self.mask, self.rnd, self.H
            mk = ReliefMask(base, crab_relief(H))
            thick = self.h(0.030)
            fz = thick * 0.6
            half = [(0, 0.78), (0.30, 0.90), (0.62, 1.0), (0.88, 0.93), (1.0, 0.55), (0.97, 0.05), (0.80, -0.45),
                    (0.45, -0.85), (0, -1.0)]
            outline = K.resample(K.chaikin(K.mirror_poly(half), 3), 140)
            out = []
            for i, (u, v) in enumerate(outline):              # bord rongé, deux morsures
                k = 1.0 - 0.03 * (0.5 + 0.5 * math.sin(i * 1.7) * math.sin(i * 0.43))
                if (u - 0.85) ** 2 + (v + 0.30) ** 2 < 0.05:
                    k -= 0.10
                if (u + 0.55) ** 2 + (v - 0.92) ** 2 < 0.03:
                    k -= 0.07
                out.append((u * k, v * k))
            bm = bmesh.new()
            K.bm_patch(bm, mk, out, extra=fz, nr=26)
            plate = self.part("Mask", bm, "Wood", "body", solidify=thick)
            plate.modifiers["Solidify"].use_even_offset = True
            eye = self.angry_eye(0.42, 0.30, 0.24, 0.16, (0.12, 0.30), (0.74, 0.52))
            mouth = K.superellipse(0.62, 0.32, 4.0, 0.0, -0.46, k=48)
            bm = bmesh.new()
            for poly in (eye, self.mirrored(eye), mouth):
                K.bm_prism(bm, base, poly, -thick, fz + self.h(0.06))
            self.cut("MaskCutter", bm, [plate])
            bm = bmesh.new()
            for poly, sc in ((eye, 1.3), (self.mirrored(eye), 1.3), (mouth, 1.12)):
                c = (sum(p[0] for p in poly) / len(poly), sum(p[1] for p in poly) / len(poly))
                big = [(c[0] + (u - c[0]) * sc, c[1] + (v - c[1]) * sc) for u, v in poly]
                K.bm_patch(bm, base, big, extra=-self.h(0.006), nr=4, center=c)
            self.part("MaskCavity", bm, "Dark", "body")
            bm = bmesh.new()
            for sg in (1, -1):                                 # yeux laiteux, globuleux, au fond des orbites
                c = base.pt(sg * 0.42, 0.28, -self.h(0.012))
                K.bm_blob(bm, c, (self.h(0.026), self.h(0.019), self.h(0.021)), seg=18, ring=12)
            self.part("Eyes", bm, "Eye", "body")
            # dents : éclats de coquillage plantés dans les bords de la bouche (deux manquent)
            bm = bmesh.new()
            for i in range(6):
                if i == 4:
                    continue
                u = -0.48 + 0.19 * i + rnd.uniform(-0.03, 0.03)
                b0 = mk.pt(u, -0.17, fz - thick * 0.5)
                dn = (base.pt(u, -0.6, 0) - base.pt(u, -0.17, 0)).normalized()
                bm_shard(bm, b0, dn + base.nrm(u, -0.2) * 0.15, self.h(rnd.uniform(0.06, 0.095)), self.h(0.024), rnd)
            for i in range(5):
                if i == 1:
                    continue
                u = -0.40 + 0.2 * i + rnd.uniform(-0.03, 0.03)
                b0 = mk.pt(u, -0.76, fz - thick * 0.5)
                upv = (base.pt(u, -0.4, 0) - base.pt(u, -0.76, 0)).normalized()
                bm_shard(bm, b0, upv + base.nrm(u, -0.76) * 0.1, self.h(rnd.uniform(0.05, 0.075)), self.h(0.02), rnd)
            self.part("Teeth", bm, "Teeth", "body", smooth=False, recalc=True)
            # incisions incrustées de sel : traits brisés (motif inventé)
            bm = bmesh.new()
            r = self.h(0.0065)
            ex = fz - r * 0.55
            for sg in (1, -1):
                self.stroke(bm, [(sg * 0.10, 0.52), (sg * 0.38, 0.66)], r * 1.1, extra=ex, squash=0.35, mask=mk)
                self.stroke(bm, [(sg * 0.48, 0.70), (sg * 0.74, 0.74)], r, extra=ex, squash=0.35, mask=mk)
                self.stroke(bm, [(sg * 0.22, 0.70), (sg * 0.40, 0.86)], r * 0.9, extra=ex, squash=0.35, mask=mk)
                self.stroke(bm, [(sg * 0.66, 0.40), (sg * 0.84, 0.48)], r * 0.85, extra=ex, squash=0.35, mask=mk)
                for k in range(3):                             # points sous les yeux
                    c = (sg * (0.30 + 0.11 * k), 0.02 - 0.03 * k)
                    self.stroke(bm, [c, (c[0] + 0.012, c[1] - 0.01)], r * 0.9, extra=ex, squash=0.35, mask=mk)
            self.stroke(bm, [(0, 0.80), (0, 0.62)], r, extra=ex, squash=0.35, mask=mk)
            self.stroke(bm, [(0, 0.50), (0, 0.32)], r, extra=ex, squash=0.35, mask=mk)
            self.part("MaskCarve", bm, "Carve", "body", recalc=True)
            # ligatures pourries autour de la tête et chevilles d'os
            B = self.body
            bm, bmp = bmesh.new(), bmesh.new()
            for v0 in (0.62, -0.40):
                z = base.cz + v0 * base.hh
                th0 = B.theta_at(base.hw * 1.0, z)
                pts = [B.surf(th0 + (TAU - 2 * th0) * k / 44, z + self.h(0.005) * math.sin(k * 0.9), self.h(0.006))
                       for k in range(45)]
                K.bm_tube(bm, pts, self.h(0.0055), segs=6, caps=True)
                for sg in (1, -1):
                    K.bm_blob(bmp, mk.pt(sg * 0.96, v0, fz), (self.h(0.008),) * 3, seg=10, ring=6)
            self.part("MaskLashing", bm, "Rope", "body", recalc=True)
            self.part("MaskPegs", bmp, "Peg", "body", recalc=True)
            # balanes sur le masque
            bm = bmesh.new()
            for (u, v) in ((0.80, -0.55), (0.68, -0.68), (0.88, -0.38), (-0.86, 0.70), (-0.72, 0.80)):
                rr = self.h(rnd.uniform(0.016, 0.024))
                bm_barnacle(bm, mk.pt(u, v, fz), base.nrm(u, v), rr, rr * 0.9, rnd, seg=10)
            ob = self.part("MaskBarnacles", bm, "Barnacle", "body", recalc=True)
            ob["budget_weight"] = 3.0
            # algues qui pendent des ligatures
            bm = bmesh.new()
            for sg, v0 in ((1, -0.40), (-1, 0.62), (-1, -0.40)):
                z = base.cz + v0 * base.hh
                th = sg * (B.theta_at(base.hw * 1.0, z) + 0.15)
                root = B.surf(th, z, self.h(0.012))
                bm_strand(bm, root, B.normal(th, z), self.h(rnd.uniform(0.12, 0.2)), self.h(0.028), rnd)
            ob = self.part("MaskKelp", bm, "Kelp", "body", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = self.h(0.002)

        def encrust(self):
            """Balanes en colonies sur le dos, les épaules et autour des cornes ; corail sur le bas du dos et un flanc ;
            algues accrochées aux cornes."""
            B, rnd, H = self.body, self.rnd, self.H
            m = self.s["mask"]
            bm = bmesh.new()
            centers = [(2.2, 0.82), (-2.0, 0.74), (math.pi, 0.90), (1.3, 0.70), (-1.15, 0.88), (2.8, 0.55),
                       (-2.7, 0.62), (0.9, 0.95), (math.pi - 0.4, 0.66), (-1.6, 0.52), (1.75, 0.46)]
            for th0, z0 in centers:
                for _ in range(rnd.randint(4, 8)):
                    th = th0 + rnd.uniform(-0.22, 0.22)
                    z = H * (z0 + rnd.uniform(-0.05, 0.05))
                    if abs(th) < 0.95 and abs(z - m["cz"] * H) < m["hh"] * H * 1.1:
                        continue
                    rr = H * rnd.uniform(0.018, 0.038)
                    bm_barnacle(bm, B.surf(th, z, -rr * 0.1), B.normal(th, z), rr, rr * rnd.uniform(0.7, 1.1), rnd)
            for sg in (1, -1):
                th, z = sg * 0.78, self.h(0.90)
                for k in range(4):
                    a = th + rnd.uniform(-0.18, 0.18)
                    zz = z + self.h(rnd.uniform(-0.05, 0.02))
                    rr = H * rnd.uniform(0.008, 0.014)
                    bm_barnacle(bm, B.surf(a, zz, 0.0), B.normal(a, zz), rr, rr, rnd)
            ob = self.part("Barnacles", bm, "Barnacle", "body", recalc=True)
            ob["budget_weight"] = 3.0
            bm = bmesh.new()
            for th, z, r in ((math.pi + 0.5, 0.42, 0.09), (1.95, 0.50, 0.07), (math.pi - 0.9, 0.36, 0.065),
                             (-1.5, 0.95, 0.05), (-2.3, 0.40, 0.06)):
                bm_coral(bm, B.surf(th, H * z, -H * 0.004), B.normal(th, H * z), H * r, rnd, count=18, branches=2)
            ob = self.part("Coral", bm, "Coral", "body", recalc=True)
            ob["budget_weight"] = 2.0
            bm = bmesh.new()
            for sg in (1, -1):
                th, z = sg * 0.86, self.h(0.93)
                for _ in range(3):
                    root = B.surf(th + rnd.uniform(-0.15, 0.15), z + self.h(rnd.uniform(-0.03, 0.02)), self.h(0.02))
                    bm_strand(bm, root, B.normal(th, z), self.h(rnd.uniform(0.14, 0.26)), self.h(0.032), rnd)
            for th, z in ((math.pi - 0.3, 0.75), (math.pi + 0.6, 0.82), (2.0, 0.62)):
                root = B.surf(th, self.h(z), self.h(0.01))
                bm_strand(bm, root, B.normal(th, self.h(z)), self.h(rnd.uniform(0.2, 0.3)), self.h(0.034), rnd)
            ob = self.part("Kelp", bm, "Kelp", "head", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = self.h(0.002)

        def crab_arm(self, s):
            """Bras de chitine maigre : segments, articulation noueuse, épines sur l'arête extérieure."""
            side = "l" if s > 0 else "r"
            rnd = self.rnd
            sh, el, wr = self.J("shoulder", s), self.J("elbow", s), self.J("wrist", s)
            ra = self.h(self.s["arm_r"]) * 0.80
            sh_in = sh - Vector((s * self.h(0.07), 0, -self.h(0.01)))
            bm = bmesh.new()
            pts = [sh_in, sh, sh.lerp(el, 0.45), el.lerp(sh, 0.12), el, el.lerp(wr, 0.25), el.lerp(wr, 0.7), wr]
            K.bm_tube(bm, pts, [ra * 1.2, ra * 1.1, ra * 0.82, ra * 0.92, ra * 1.0, ra * 0.78, ra * 0.86, ra * 0.95],
                      segs=18)
            K.bm_blob(bm, el + Vector((0, self.h(0.01), 0)), (ra * 1.05, ra * 1.05, ra * 1.0), seg=14, ring=10)
            ax = (el - sh).normalized()
            K.bm_torus(bm, sh.lerp(el, 0.78), ax, ra * 0.86, ra * 0.16, seg=16, ring=6)
            fa = (wr - el).normalized()
            K.bm_torus(bm, el.lerp(wr, 0.42), fa, ra * 0.8, ra * 0.15, seg=16, ring=6)
            out = (Vector((s, 0, 0)) - fa * fa.dot(Vector((s, 0, 0)))).normalized()
            back = Vector((0, 1, 0))
            for k, t in enumerate((0.25, 0.5, 0.75)):
                b = el.lerp(wr, t) + (out * 0.7 + back * 0.5).normalized() * ra * 0.8
                K.bm_horn(bm, b, out + back * 0.6 + fa * 0.3, ra * rnd.uniform(0.5, 0.8), ra * 0.22, segs=8, n=5,
                          r_tip=ra * 0.03)
            for t in (0.3, 0.62):
                b = sh.lerp(el, t) + (out * 0.6 + back * 0.6).normalized() * ra * 0.85
                K.bm_horn(bm, b, out + back * 0.7, ra * 0.55, ra * 0.2, segs=8, n=5, r_tip=ra * 0.03)
            self.organic(self.part(f"Arm_{side}", bm, "Limb", f"arm_{side}", recalc=True), voxel=0.0055, smooth=5)

        def crab_leg(self, s):
            side = "l" if s > 0 else "r"
            rnd = self.rnd
            hip, kn, an = self.J("hip", s), self.J("knee", s), self.J("ankle", s)
            rl = self.h(self.s["leg_r"]) * 0.82
            bm = bmesh.new()
            K.bm_tube(bm, [hip + Vector((0, 0, self.h(0.07))), hip, hip.lerp(kn, 0.5), kn, kn.lerp(an, 0.5), an],
                      [rl * 1.15, rl * 1.12, rl * 0.86, rl * 1.0, rl * 0.78, rl * 0.86], segs=18)
            K.bm_blob(bm, kn + Vector((0, -rl * 0.3, 0)), (rl * 0.95, rl * 0.85, rl * 0.9), seg=14, ring=10)
            K.bm_torus(bm, hip.lerp(kn, 0.75), kn - hip, rl * 0.82, rl * 0.16, seg=16, ring=6)
            out = Vector((s, 0, 0))
            for t in (0.3, 0.65):
                b = kn.lerp(an, t) + (out * 0.7 + Vector((0, 0.6, 0))).normalized() * rl * 0.8
                K.bm_horn(bm, b, out + Vector((0, 0.8, 0.2)), rl * rnd.uniform(0.5, 0.75), rl * 0.2, segs=8, n=5,
                          r_tip=rl * 0.03)
            self.organic(self.part(f"Leg_{side}", bm, "Limb", f"leg_{side}", recalc=True), voxel=0.0055, smooth=5)

        def chela(self, s, grip=False):
            """Pince : propode bombé et dentelé, doigt fixe et dactyle courbes à dents ; la droite serre le manche."""
            side = "l" if s > 0 else "r"
            rnd = self.rnd
            wr, D, F, L, ln = self.hand_frame(s)
            hs = self.hs
            out = s * L
            bm, bmt = bmesh.new(), bmesh.new()
            if grip:
                c = self.grip_r
                A = (self.w_rest_rot.to_3x3() @ Vector((0, 0, 1))).normalized()
                Dp = (D - A * D.dot(A)).normalized()
                Bk = A.cross(Dp).normalized()
                rs = 0.021
                palm_c = c - Dp * (rs + hs(0.034))
                R = Matrix((Bk, A, Dp)).transposed()
                K.bm_blob(bm, palm_c, (hs(0.044), hs(0.05), hs(0.06)), rot=R, seg=18, ring=12)
                K.bm_tube(bm, [wr - D * hs(0.01), palm_c], [hs(0.036), hs(0.042)], segs=14)
                for sg in (1, -1):
                    arc = []
                    for j in range(8):
                        th = math.radians(180 - 165 * j / 7) * sg
                        rr = rs + hs(0.016) * (1.0 - 0.3 * j / 7)
                        arc.append(c + (Dp * math.cos(th) + Bk * math.sin(th)) * rr + A * (sg * hs(0.006)))
                    C = K.catmull(arc, 14)
                    K.bm_tube(bm, C, [hs(0.022) * (1.0 - 0.75 * j / 13) for j in range(14)], segs=10)
                    for j in (3, 6, 9):                        # dents internes
                        inward = (c - C[j]).normalized()
                        K.bm_horn(bm, C[j] + inward * hs(0.012), inward, hs(0.012), hs(0.006), segs=6, n=3)
                    tip = C[-1]
                    K.bm_horn(bmt, tip - (C[-1] - C[-3]).normalized() * hs(0.012), (C[-1] - C[-3]), hs(0.02),
                              hs(0.007), segs=8, n=4, r_tip=hs(0.0012))
            else:
                c = wr + D * hs(0.06)
                R = Matrix((L, F, D)).transposed()
                K.bm_blob(bm, c, (hs(0.05), hs(0.046), hs(0.075)), rot=R, seg=18, ring=12)
                K.bm_tube(bm, [wr - D * hs(0.01), c], [hs(0.036), hs(0.044)], segs=14)
                for k in range(5):                             # tubercules sur la face externe
                    p = c + D * hs(-0.03 + 0.016 * k) + out * hs(0.044) + F * hs(rnd.uniform(-0.01, 0.01))
                    K.bm_blob(bm, p, (hs(0.008),) * 3, seg=8, ring=6)
                fingers = [(out * hs(0.022) + F * hs(0.004), D + F * 0.30 - out * 0.10, -out, 0.38, hs(0.105)),
                           (-out * hs(0.018) + F * hs(0.012), D + F * 0.75 + out * 0.45, out, 0.45, hs(0.095))]
                for off, d, bend, bend_amt, length in fingers:
                    b0 = c + D * hs(0.06) + off
                    K.bm_horn(bm, b0, d, length, hs(0.024), bend=bend, bend_amt=bend_amt, segs=12, n=10,
                              r_tip=hs(0.004))
                    dn = Vector(d).normalized()
                    for t in (0.25, 0.45, 0.65):               # dents du bord interne
                        p = b0 + dn * (length * t) + Vector(bend).normalized() * (bend_amt * length * t * t)
                        K.bm_horn(bm, p + Vector(bend).normalized() * hs(0.012), Vector(bend), hs(0.012), hs(0.006),
                                  segs=6, n=3)
                    tip = b0 + dn * length + Vector(bend).normalized() * (bend_amt * length)
                    K.bm_horn(bmt, tip - dn * hs(0.012), dn + Vector(bend).normalized() * 0.5, hs(0.02), hs(0.007),
                              segs=8, n=4, r_tip=hs(0.0012))
            self.organic(self.part(f"Claw_{side}", bm, "Limb", f"hand_{side}", recalc=True), voxel=0.0045, smooth=4)
            self.part(f"ClawTips_{side}", bmt, "Tip", f"hand_{side}", recalc=True)

        def crab_foot(self, s):
            side = "l" if s > 0 else "r"
            c, toe = self.foot_core(s)
            f = self.fs
            bm, bmt = bmesh.new(), bmesh.new()
            K.bm_blob(bm, c, (f(0.052), f(0.072), f(0.042)), cut_below=0.0, seg=16, ring=10)
            K.bm_tube(bm, [self.J("ankle", s) + Vector((0, 0, self.h(0.02))), c + Vector((0, f(0.01), f(0.02)))],
                      [self.h(self.s["leg_r"]) * 0.72, f(0.04)], segs=14)
            for dx in (-0.032, 0.0, 0.032):
                base = Vector((c.x + s * f(dx), toe.y + f(0.07), f(0.026)))
                d = Vector((s * dx * 4, -1, -0.32))
                K.bm_horn(bm, base, d, f(0.075), f(0.022), bend=(0, 0, -1), bend_amt=0.3, segs=10, n=8, r_tip=f(0.005))
                tip = base + d.normalized() * f(0.075) + Vector((0, 0, -1)) * 0.3 * f(0.075)
                K.bm_horn(bmt, tip - d.normalized() * f(0.01), d + Vector((0, 0, -0.6)), f(0.018), f(0.006), segs=8,
                          n=4, r_tip=f(0.001))
            K.bm_horn(bm, Vector((c.x, c.y + f(0.05), f(0.025))), (0, 1, -0.4), f(0.045), f(0.016), segs=8, n=5,
                      r_tip=f(0.003))
            self.organic(self.part(f"Foot_{side}", bm, "Limb", f"foot_{side}", recalc=True), voxel=0.0045, smooth=4)
            self.part(f"FootTips_{side}", bmt, "Tip", f"foot_{side}", recalc=True)

        def rope_bands(self, s):
            side = "l" if s > 0 else "r"
            rnd = self.rnd
            el, wr = self.J("elbow", s), self.J("wrist", s)
            kn, an = self.J("knee", s), self.J("ankle", s)
            ra, rl = self.h(self.s["arm_r"]) * 0.8, self.h(self.s["leg_r"]) * 0.82
            r = self.h(0.0075)
            bm, bm2 = bmesh.new(), bmesh.new()
            K.bm_helix(bm, el.lerp(wr, 0.55), el.lerp(wr, 0.95), ra * 0.95 + r * 0.7, r, 3.0, segs=7)
            K.bm_helix(bm2, kn.lerp(an, 0.55), kn.lerp(an, 0.95), rl * 0.9 + r * 0.7, r, 3.0, segs=7)
            self.part(f"Wristwrap_{side}", bm, "Rope", f"arm_{side}", recalc=True)
            self.part(f"Anklewrap_{side}", bm2, "Rope", f"leg_{side}", recalc=True)
            bm = bmesh.new()
            p = el.lerp(wr, 0.8) + Vector((s, 0.4, 0)).normalized() * (ra + r)
            bm_strand(bm, p, Vector((s, 0.3, 0)), self.h(rnd.uniform(0.10, 0.15)), self.h(0.024), rnd, n=7)
            ob = self.part(f"WristKelp_{side}", bm, "Kelp", f"arm_{side}", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = self.h(0.002)

        def hammer(self):
            """Marteau : manche de bois flotté tordu et noueux, coquillage encroûté de balanes et de corail, ligatures
            pourries, une algue qui pend (espace de prise : origine = poignée, +Z)."""
            rnd = random.Random(5)
            bm = bmesh.new()
            n = 16
            pts = [Vector((0.009 * math.sin(k * 1.3) + 0.004 * math.sin(k * 3.1), 0.006 * math.cos(k * 0.9),
                           -0.22 + 0.76 * k / (n - 1))) for k in range(n)]
            radii = [(0.021 - 0.004 * k / (n - 1)) * (1.0 + 0.10 * math.sin(k * 2.3) + rnd.uniform(-0.04, 0.04))
                     for k in range(n)]
            K.bm_tube(bm, pts, radii, segs=12)
            for z in (-0.12, 0.16, 0.33):
                a = rnd.uniform(0, TAU)
                K.bm_blob(bm, (0.014 * math.cos(a), 0.014 * math.sin(a), z), (0.012, 0.012, 0.018), seg=10, ring=6)
            self.part("Hammer_Handle", bm, "Handle", "weapon", "weapon", recalc=True)
            bm = bmesh.new()
            K.bm_helix(bm, (0, 0, -0.10), (0, 0, 0.07), 0.023, 0.0058, 6.0, segs=7)
            K.bm_helix(bm, (0, 0, 0.40), (0, 0, 0.50), 0.027, 0.0062, 4.5, segs=7)
            self.part("Hammer_Rope", bm, "Rope", "weapon", "weapon", recalc=True)
            creatures = T["creatures"]
            shell = creatures.import_model(creatures.MODELS / "props" / "lambis_shell" / "lambis_shell_1k.gltf",
                                           self.coll, f"{self.name}_Hammer_Shell", length=0.34, decimate=0.5)
            top = Vector((0, 0, 0.56))
            if shell:
                shell.location = (0.0, 0.0, 0.52)
                shell.rotation_euler = (0.0, 0.0, 0.0)
                shell["rig"] = "weapon"
                shell.data.materials.clear()
                shell.data.materials.append(self.m("Conch"))
                for poly in shell.data.polygons:
                    poly.material_index = 0
                self.weapon_parts.append(shell)
            bm = bmesh.new()
            for k in range(9):
                a = rnd.uniform(0, TAU)
                p = top + Vector((0.13 * rnd.uniform(-1, 1), 0.05 * math.sin(a), 0.03 + 0.03 * math.cos(a)))
                nrm = Vector((p.x * 0.3, math.sin(a), math.cos(a) + 0.3))
                rr = rnd.uniform(0.008, 0.016)
                bm_barnacle(bm, p, nrm, rr, rr, rnd)
            self.part("Hammer_Barnacles", bm, "Barnacle", "weapon", "weapon", recalc=True)
            bm = bmesh.new()
            bm_coral(bm, top + Vector((-0.10, 0.0, 0.05)), (0, 0.3, 1), 0.035, rnd, count=12, branches=2)
            self.part("Hammer_Coral", bm, "Coral", "weapon", "weapon", recalc=True)
            bm = bmesh.new()
            bm_strand(bm, Vector((0.02, 0.02, 0.47)), Vector((1, 0.4, 0)), 0.16, 0.02, rnd, n=8)
            ob = self.part("Hammer_Kelp", bm, "Kelp", "weapon", "weapon", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = 0.002

    # ======================================================================== VEILLEUR DES CENDRES
    class AshHorror(HorrorMixin, RealCoco):
        MATS = staticmethod(ash_horror_mats)

        def __init__(self, coll):
            super().__init__("ash", coll, 20261010)
            B, H, rnd = self.body, self.H, self.rnd
            self.seeds = []
            for d in K.fib_sphere(74, seed=11):
                z = B.zw + (d.z * (B.zt - B.zw) if d.z > 0 else d.z * (B.zw - B.zb))
                th = math.atan2(d.x, -d.y)
                p = B.surf(th, z)
                self.seeds.append((p, B.normal(th, z), H * rnd.uniform(-0.003, 0.006)))
            self.live = {}
            # flèche de chaque plaque (demi-distance à la graine voisine, rayon de courbure ~0,38 H)
            self.sag = []
            for i, (c, _, _) in enumerate(self.seeds):
                rc = 0.5 * min((c - c2).length for j, (c2, _, _) in enumerate(self.seeds) if j != i)
                self.sag.append(rc * rc / (2 * 0.38 * H))

        def husk_noise(self, th, z):
            return (math.sin(th * 11 + z * 29) * 0.5 + math.sin(th * 23 - z * 51 + 0.7) * 0.5) * 0.0018 * self.H

        def nearest2(self, p):
            best = [(1e9, -1), (1e9, -1)]
            for i, (c, _, _) in enumerate(self.seeds):
                d = (p - c).length_squared
                if d < best[1][0]:
                    if d < best[0][0]:
                        best = [(d, i), best[0]]
                    else:
                        best[1] = (d, i)
            return math.sqrt(best[0][0]), best[0][1], math.sqrt(best[1][0]), best[1][1]

        def crust_disp(self, th, z, p):
            H = self.H
            d1, i1, d2, i2 = self.nearest2(p)
            c, nc, ph = self.seeds[i1]
            edge = d2 - d1
            gw = 0.018 * H
            g = 1.0 - min(1.0, edge / gw)
            g = g * g * (3 - 2 * g)
            # plaque aplanie autour du plan moyen de la cellule (centre rentré, bords sortis d'autant) : la coque reste
            # à ±1,5 cm de l'œuf et le masque, posé 4,4 cm devant, n'est jamais traversé
            facet = ((c - p).dot(nc) - 0.5 * self.sag[i1]) * 0.75
            return facet + ph * (1.0 - g) - 0.016 * H * g

        def fissures(self):
            """Attribut Fissure (1 au fond des failles vives) : la moitié des failles couvent des braises."""
            H, rnd = self.H, random.Random(7)
            vals = []
            for p in self.shell_points():
                d1, i1, d2, i2 = self.nearest2(p)
                key = (min(i1, i2), max(i1, i2))
                if key not in self.live:
                    self.live[key] = rnd.random() < 0.55
                g = max(0.0, 1.0 - (d2 - d1) / (0.016 * H))
                vals.append(g * g if self.live[key] else 0.0)
            self.set_attr(self.shell_ob, "Fissure", vals)

        def build(self):
            self.shell(150, 112, disp=self.crust_disp)
            self.shell_ob["mask_part"] = True                  # budget : les failles gardent leur densité
            self.fissures()
            self.crown_shards()
            self.plank_mask()
            for s in (1, -1):
                self.limb(s, muscle=0.66)
                if s < 0:
                    self.hand_fist(s, rs=0.023)
                else:
                    self.hand_open(s)
                self.foot_real(s, sandal=False)
                self.wraps(s)
            self.club()
            return self

        def nail(self, bm, tip, d, back, r):
            """Griffes d'obsidienne : longues, recourbées vers la paume."""
            d = Vector(d).normalized()
            back = Vector(back).normalized()
            K.bm_horn(bm, Vector(tip) - d * r * 0.6 + back * r * 0.2, d, r * 2.6, r * 0.75, bend=-back, bend_amt=0.45,
                      segs=8, n=6, r_tip=r * 0.06)

        def crown_shards(self):
            B, rnd = self.body, self.rnd
            bm = bmesh.new()
            for th, z, ht, w in ((0.0, 0.93, 0.19, 0.06), (0.75, 0.86, 0.12, 0.05), (-0.8, 0.86, 0.15, 0.055),
                                 (2.3, 0.84, 0.10, 0.05), (-2.4, 0.85, 0.08, 0.05), (math.pi, 0.9, 0.13, 0.05),
                                 (1.5, 0.80, 0.07, 0.04), (-1.6, 0.79, 0.06, 0.035), (0.35, 0.91, 0.07, 0.035)):
                base = B.surf(th, self.h(z), -self.h(0.03))
                up = (B.normal(th, self.h(z)) * 0.55 + Vector((0, 0, 1))).normalized()
                bm_shard(bm, base, up + Vector((rnd.uniform(-0.2, 0.2), rnd.uniform(-0.2, 0.2), 0)), self.h(ht),
                         self.h(w), rnd, pts=11)
                for _ in range(2):
                    b2 = base + Vector((rnd.uniform(-1, 1), rnd.uniform(-1, 1), 0)).normalized() * self.h(w * 0.9)
                    bm_shard(bm, b2, up + Vector((rnd.uniform(-0.5, 0.5), rnd.uniform(-0.5, 0.5), 0)),
                             self.h(ht * rnd.uniform(0.3, 0.55)), self.h(w * 0.55), rnd, pts=8)
            for s in (1, -1):                                   # éclats sur les épaules
                for k in range(3):
                    th = s * (1.25 + 0.18 * k)
                    z = self.h(0.62 + 0.05 * k)
                    base = B.surf(th, z, -self.h(0.01))
                    bm_shard(bm, base, B.normal(th, z) + Vector((0, 0, 0.6)), self.h(rnd.uniform(0.04, 0.07)),
                             self.h(0.025), rnd, pts=8)
            self.part("Shards", bm, "Obsidian", "head", smooth=False, recalc=True)

        def plank_mask(self):
            """Masque de planches calcinées : une seule plaque (bord rongé par le feu) dont le relief creuse trois
            joints entre quatre planches décalées en profondeur, clous d'obsidienne le long des joints."""
            base, rnd, H = self.mask, self.rnd, self.H
            seams = (-0.5, 0.0, 0.5)
            offs = (0.004, -0.003, 0.005, -0.002)

            def relief(u, v):
                k = max(0, min(3, int((u + 1.0) / 0.5)))
                h = offs[k] * H
                for s0 in seams:
                    h -= 0.012 * H * math.exp(-((u - s0) / 0.03) ** 2)
                return h

            mk = ReliefMask(base, relief)
            thick = self.h(0.034)
            fz = thick * 0.55
            half = [(0, 1.0), (0.55, 1.0), (0.9, 0.93), (1.0, 0.55), (0.95, 0.0), (0.75, -0.5), (0.4, -0.85),
                    (0, -1.0)]
            outline = K.resample(K.chaikin(K.mirror_poly(half), 2), 160)
            B = self.body
            out = []
            for i, (u, v) in enumerate(outline):                # bord rongé par le feu, plus bas que haut
                k = 1.0 - 0.03 * (0.5 + 0.5 * math.sin(i * 0.61) * math.sin(i * 0.23))
                if v < -0.4:
                    k -= 0.04 * (0.5 + 0.5 * math.sin(i * 0.9))
                if (u - 0.62) ** 2 + (v + 0.72) ** 2 < 0.04:
                    k -= 0.08
                u, v = u * k, v * k
                # la plaque ne déborde jamais du profil de l'œuf (sinon elle se replie sur le flanc et la découpe
                # booléenne la détruit) : demi-largeur bornée à 88 % du rayon du corps à cette hauteur
                umax = 0.88 * (B.rx * B.f(base.cz + v * base.hh) + base.off) / base.hw
                out.append((max(-umax, min(umax, u)), v))
            bm = bmesh.new()
            K.bm_patch(bm, mk, out, extra=fz, nr=28)
            plate = self.part("Mask", bm, "Wood", "body", solidify=thick)
            plate.modifiers["Solidify"].use_even_offset = False    # bord borné : angles minces, pas de pointes
            planks = [plate]
            edges = [-1.0, -0.5, 0.0, 0.5, 1.0]
            eye = self.angry_eye(0.42, 0.22, 0.26, 0.17, (0.12, 0.22), (0.74, 0.46))
            mouth = [(0.46 * math.cos(TAU * i / 40), -0.50 + 0.20 * math.sin(TAU * i / 40)) for i in range(40)]
            bm = bmesh.new()
            for poly in (eye, self.mirrored(eye), mouth):
                K.bm_prism(bm, base, poly, -thick * 1.5, fz + self.h(0.06))
            self.cut("MaskCutter", bm, planks)
            bm = bmesh.new()
            for poly, sc in ((eye, 1.25), (self.mirrored(eye), 1.25), (mouth, 1.1)):
                c = (sum(p[0] for p in poly) / len(poly), sum(p[1] for p in poly) / len(poly))
                big = [(c[0] + (u - c[0]) * sc, c[1] + (v - c[1]) * sc) for u, v in poly]
                K.bm_patch(bm, base, big, extra=-self.h(0.012), nr=4, center=c)
            self.part("MaskCavity", bm, "Dark", "body")
            bm = bmesh.new()
            for sg in (1, -1):                                 # braises au fond des orbites
                c = base.pt(sg * 0.43, 0.19, -self.h(0.014))
                K.bm_blob(bm, c, (self.h(0.022), self.h(0.014), self.h(0.016)), seg=16, ring=10)
            self.part("Eyes", bm, "Eye", "body")
            bm = bmesh.new()                                   # dents d'obsidienne, irrégulières
            for i in range(6):
                u = -0.38 + 0.152 * i + rnd.uniform(-0.02, 0.02)
                b0 = mk.pt(u, -0.31, fz - thick * 0.4)
                dn = (base.pt(u, -0.7, 0) - base.pt(u, -0.31, 0)).normalized()
                bm_shard(bm, b0, dn, self.h(rnd.uniform(0.045, 0.085)), self.h(0.02), rnd)
            for i in range(5):
                u = -0.30 + 0.15 * i + rnd.uniform(-0.02, 0.02)
                b0 = mk.pt(u, -0.69, fz - thick * 0.4)
                upv = (base.pt(u, -0.35, 0) - base.pt(u, -0.69, 0)).normalized()
                bm_shard(bm, b0, upv, self.h(rnd.uniform(0.04, 0.07)), self.h(0.018), rnd)
            self.part("Teeth", bm, "Teeth", "body", smooth=False, recalc=True)
            bm = bmesh.new()                                   # traces de cendre délavées (chevrons inventés)
            r = self.h(0.007)
            for k in range(3):
                y0 = 0.47 + 0.13 * k
                wdt = 0.30 - 0.07 * k
                pts = [(-wdt, y0), (-wdt * 0.5, y0 + 0.1), (0, y0 + 0.2), (wdt * 0.5, y0 + 0.1), (wdt, y0)]
                for j in range(len(pts) - 1):
                    if rnd.random() < 0.35:
                        continue
                    self.stroke(bm, [pts[j], pts[j + 1]], r * rnd.uniform(0.6, 1.0), extra=fz - r * 0.4, squash=0.3,
                                mask=mk)
            for sg in (1, -1):
                self.stroke(bm, [(sg * 0.64, 0.06), (sg * 0.60, -0.16)], r, extra=fz, squash=0.3, mask=mk)
                self.stroke(bm, [(sg * 0.64, 0.02), (sg * 0.82, 0.08)], r * 0.9, extra=fz, squash=0.3, mask=mk)
            self.part("MaskSmear", bm, "Smear", "body", recalc=True)
            bm = bmesh.new()                                   # clous d'obsidienne de part et d'autre des joints
            for k in range(4):
                uc = (edges[k] + edges[k + 1]) * 0.5
                sp = poly_span(outline, uc)
                for v in (sp[1] - 0.12, sp[0] + 0.18):
                    K.bm_blob(bm, mk.pt(uc + rnd.uniform(-0.05, 0.05), v, fz + thick * 0.02), (self.h(0.009),) * 3,
                              seg=8, ring=6)
            self.part("MaskPegs", bm, "Peg", "body", recalc=True)
            B = self.body                                      # bandeau de corde roussie
            bm = bmesh.new()
            zb = base.cz + base.hh * 0.93
            ring = []
            for i in range(73):
                th = TAU * i / 72
                front = max(0.0, math.cos(th)) ** 2
                z = zb - self.h(0.015) * front + self.h(0.02) * (1 - front)
                off = self.h(0.045) + (base.off + self.h(0.02) - self.h(0.045)) * front
                ring.append(B.surf(th, z, off))
            K.bm_tube(bm, ring, self.h(0.017), segs=10, caps=False)
            for sg in (1, -1):
                th = sg * 1.35
                c = B.surf(th, zb + self.h(0.01), self.h(0.07))
                K.bm_blob(bm, c, (self.h(0.03), self.h(0.03), self.h(0.026)), seg=14, ring=10)
            self.part("Headband", bm, "Rope", "head", recalc=True)
            bm = bmesh.new()
            for sg in (1, -1):
                th = sg * 1.35
                c = B.surf(th, zb, self.h(0.08))
                bm_strand(bm, c, B.normal(th, zb), self.h(rnd.uniform(0.10, 0.16)), self.h(0.02), rnd)
            ob = self.part("HeadbandTails", bm, "Cloth", "head", recalc=True)
            ob.modifiers.new("Solidify", "SOLIDIFY").thickness = self.h(0.002)

        def wraps(self, s):
            """Corde roussie aux poignets et aux chevilles, bandes de tissu brûlé sur l'avant-bras gauche et les
            tibias."""
            side = "l" if s > 0 else "r"
            ra = self.h(self.s["arm_r"]) * (0.92 + 0.12 * 0.66)
            rl = self.h(self.s["leg_r"]) * (0.95 + 0.1 * 0.66)
            el, wr = self.J("elbow", s), self.J("wrist", s)
            kn, an = self.J("knee", s), self.J("ankle", s)
            r = self.h(0.0075)
            bm, bm2 = bmesh.new(), bmesh.new()
            K.bm_helix(bm, el.lerp(wr, 0.62), el.lerp(wr, 0.96), ra * 0.92 + r * 0.7, r, 3.0, segs=7)
            K.bm_helix(bm2, kn.lerp(an, 0.65), kn.lerp(an, 0.98), rl * 0.85 + r * 0.7, r, 2.5, segs=7)
            self.part(f"Wristwrap_{side}", bm, "Rope", f"arm_{side}", recalc=True)
            self.part(f"Anklewrap_{side}", bm2, "Rope", f"leg_{side}", recalc=True)
            if s > 0:
                self.bandage(s, el.lerp(wr, 0.10), el.lerp(wr, 0.55), ra * 1.05, 2.6, mat="Cloth")
            self.bandage(s, kn.lerp(an, 0.10), kn.lerp(an, 0.55), rl * 1.02, 2.2, mat="Cloth", tag=f"leg_{side}",
                         tail=s < 0)

        def club(self):
            """Massue : manche calciné, tête de basalte à failles de braise hérissée d'éclats d'obsidienne, corde
            roussie (espace de prise : origine = poignée, +Z)."""
            rnd = random.Random(31)
            bm = bmesh.new()
            n = 14
            pts = [Vector((0.008 * math.sin(k * 1.5), 0.006 * math.cos(k * 1.2), -0.24 + 0.70 * k / (n - 1)))
                   for k in range(n)]
            K.bm_tube(bm, pts, [(0.025 - 0.004 * k / (n - 1)) * (1.0 + rnd.uniform(-0.05, 0.05)) for k in range(n)],
                      segs=12)
            for z in (-0.10, 0.14):
                K.bm_blob(bm, (0.015, 0.0, z), (0.013, 0.013, 0.019), seg=10, ring=6)
            self.part("Club_Handle", bm, "Handle", "weapon", "weapon", recalc=True)
            sp = []
            for d in K.fib_sphere(64, seed=3):
                j = rnd.uniform(0.82, 1.14)
                sp.append(Vector((d.x * 0.10 * j, d.y * 0.088 * j, 0.57 + d.z * 0.20 * j + (0.03 if d.z > 0.6 else 0))))
            bm = bmesh.new()
            K.bm_hull(bm, sp)
            self.part("Club_Head", bm, "Lava", "weapon", "weapon", smooth=False, recalc=True)
            bm = bmesh.new()
            for k in range(7):
                a = TAU * k / 7 + rnd.uniform(-0.3, 0.3)
                z = 0.57 + rnd.uniform(-0.12, 0.14)
                c = Vector((0.085 * math.cos(a), 0.075 * math.sin(a), z))
                bm_shard(bm, c, Vector((math.cos(a), math.sin(a), rnd.uniform(-0.2, 0.5))), rnd.uniform(0.05, 0.09),
                         0.022, rnd, pts=9)
            bm_shard(bm, Vector((0, 0, 0.74)), Vector((0.1, 0, 1)), 0.08, 0.03, rnd, pts=10)
            self.part("Club_Shards", bm, "Obsidian", "weapon", "weapon", smooth=False, recalc=True)
            bm = bmesh.new()
            K.bm_helix(bm, (0, 0, 0.30), (0, 0, 0.43), 0.031, 0.0075, 4.5, segs=7)
            K.bm_helix(bm, (0, 0, -0.09), (0, 0, 0.08), 0.027, 0.0065, 6.0, segs=7)
            self.part("Club_Rope", bm, "Rope", "weapon", "weapon", recalc=True)

    return dict(coco=WarriorHorror, crab=CrabHorror, ash=AshHorror)


def _seg_dist(p, a, b):
    ab = b - a
    t = max(0.0, min(1.0, (p - a).dot(ab) / max(ab.length_squared, 1e-12)))
    return (p - (a + ab * t)).length
