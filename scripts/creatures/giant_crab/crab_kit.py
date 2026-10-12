"""
Crabe géant « Tupa Nui » v2 (T-260, lot L5) : gabarit du squelette, cinématique, géométrie, UV paramétriques, LOD.

Provenance : factory/kits/crab_kit.py du 2e PC (maquette crabe-geant v0.2.0 du 2026-10-07), rapatrié dans le dépôt le
2026-10-09. Sont repris SANS CHANGEMENT : Params, Layout (repères de repos des 58 os déformants), leg_chain /
leg_frames / two_bone / claw_frames (cinématique partagée avec crab_anims), le contour et les sillons de la carapace.
Le squelette SKEL_GiantCrab, l'échelle (dos 1,95 m, ventre 1,25 m, envergure 5,9 m) et les sockets restent donc ceux
de la v1 (Physics Asset et clips existants compatibles).

Géométrie v2 (demande du propriétaire du 2026-10-09 : « modéliser ses pattes », « ultra terrifiant ») :
  - pattes en plaques rigides : coxa, basis-ischion, mérus, carpe, propode, dactyle (6 segments sur les 4 os de la
    patte : basis-ischion sur l'os du mérus, carpe sur l'os du propode), membranes articulaires plissées pondérées en
    dégradé entre les deux os, carènes dentelées, épines, touffes de soies, dactyles en crochets acérés ; balanes,
    coraux et cicatrices sur les pattes arrière ;
  - pinces plus massives, dents irrégulières et cassées, rangées d'épines, tubercules ;
  - carapace hérissée sur le bord frontal (rostre, épines sus-orbitaires et antérolatérales, grandes épines
    latérales), tubercules dorsaux, cicatrices (griffures, fissure), balanes et coraux sur le bord postérieur ;
  - plus d'or ni de perles (registre Tamatoa évité, palette sombre) ;
  - yeux composés ambrés (matériau à part, émissif) ; pièces buccales et mandibules dentées.
UV : chaque pièce a un rectangle dans l'atlas de son matériau (Body, Legs, Eyes) avec une paramétrisation analytique
(u autour, v le long ; projections planes pour le dessus et le dessous de la carapace) : les LOD régénérés à plus
basse résolution gardent exactement les mêmes UV, donc les mêmes textures cuites.
Unités : mètres, Z haut, avant -Y, gauche +X.
"""

import math

import bpy
import numpy as np
from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree

from artifacts import geo as AG
from crab_nodes import lin

TAU = 2 * math.pi
Z = Vector((0, 0, 1))
SIDES = ("l", "r")
SGN = {"l": 1.0, "r": -1.0}
PAIRS = (1, 2, 3, 4)
LEG_SEGS = ("coxa", "merus", "propodus", "dactyl")
CLAW_SEGS = ("arm", "forearm", "hand", "finger")

# matériaux (emplacements du SkeletalMesh, dans cet ordre) : un atlas cuit par emplacement. Les noms d'emplacement
# reprennent ceux de la v1 (MI_GiantCrab_Chitin, MI_GiantCrab_Eye : le réimport les retrouve, le gameplay T-261 cherche
# l'emplacement MI_GiantCrab_Eye pour les yeux rouges) ; les instances Unreal sont MI_GiantCrab_Body / _Legs / _Eyes.
SLOT_BODY, SLOT_LEGS, SLOT_EYE = 0, 1, 2
MAT_SLOTS = ["MI_GiantCrab_Chitin", "MI_GiantCrab_Legs", "MI_GiantCrab_Eye"]
UE_MI = {"MI_GiantCrab_Chitin": "MI_GiantCrab_Body", "MI_GiantCrab_Legs": "MI_GiantCrab_Legs",
         "MI_GiantCrab_Eye": "MI_GiantCrab_Eyes"}

# couleurs (sRGB) : chitine noir violacé et sang de bœuf, épines os, laque humide
PALETTE = {
    "black": "#1F1119",          # dos, faces dorsales des pattes
    "violet": "#28132F",         # reflets violets dans le noir
    "oxblood": "#4C1017",        # sang de bœuf : bords, articulations, dessous des pattes
    "oxblood_dark": "#2B080D",
    "belly": "#77675B",          # dessous os sale
    "belly_stain": "#4B1A1D",
    "bone": "#CDBE98",           # pointes d'épines, dents (ivoire jauni)
    "bone_dark": "#8C7C64",
    "tip": "#0B0708",            # pointes des dactyles et des doigts
    "membrane": "#71101A",       # membranes articulaires (points faibles)
    "membrane_dark": "#2C0508",
    "scar": "#6B4B47",           # cicatrices
    "barnacle": "#A39D8E",
    "barnacle_dark": "#36312B",
    "coral_bleached": "#CBBDA7",
    "coral_red": "#5E1F2E",
    "setae": "#2D1A12",
    "mouth": "#3B0812",          # fond de la cavité buccale
    "veil": "#B9B3AB",           # voile laiteux (forme DeadEyes)
    "eye_amber": "#E0701A",      # œil composé (l'émission fait la lueur)
    "eye_dark": "#2A1104",
    "eye_pupil": "#0C0503",      # pseudo-pupille
}

# paramètres visuels v2 (les dimensions du squelette restent dans crab_spec.json > params)
V2 = dict(
    lod_q={0: 1.0, 1: 0.55, 2: 0.32, 3: 0.17},
    lod_tris_max={0: 110000, 1: 45000, 2: 18000, 3: 7000},
    claw_mass=1.22,              # épaisseur des bras, avant-bras et paumes des pinces (visuel, sockets inchangés)
    lateral_spine_len=0.46,      # grande épine latérale de la carapace (v1 : 0,22 m)
    dorsal_spines=105,            # tubercules épineux du dessus de la carapace (LOD0)
    margin_spines=7,             # épines antérolatérales par côté
    atlas_px=2048, eye_px=512, pad_px=8,
    seed=20261009,
)


def V3(v):
    return Vector((float(v[0]), float(v[1]), float(v[2])))


def mirror(v, side):
    return Vector((v[0] * SGN[side], v[1], v[2]))


def smooth(a, b, x):
    t = np.clip((np.asarray(x, float) - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def bump(x, at, width):
    return np.exp(-((np.asarray(x, float) - at) / width) ** 2)


def frame(head, tail, up):
    """Repère d'os Blender : Y = tête -> queue, Z = up orthogonalisé, X = Y x Z (comme EditBone.align_roll)."""
    head, tail, up = V3(head), V3(tail), V3(up)
    y = (tail - head).normalized()
    z = up - y * up.dot(y)
    if z.length < 1e-6:
        z = Vector((0, 0, 1)) - y * y.z
        if z.length < 1e-6:
            z = Vector((0, -1, 0)) - y * (-y.y)
    z.normalize()
    x = y.cross(z)
    M = Matrix((x, y, z)).transposed().to_4x4()
    M.translation = head
    return M


def rot_axis(M, axis, deg):
    """M @ rotation locale autour d'un axe local ('X', 'Y', 'Z')."""
    return M @ Matrix.Rotation(math.radians(deg), 4, axis)


# ============================================================================= paramètres (v1, inchangé)

class Params:
    """Lecture typée de spec['params'] + grandeurs dérivées."""

    def __init__(self, P):
        self.P = P
        b = P["body"]
        self.pivot = V3(b["bone_pivot"])
        c = b["carapace"]
        self.cara = c
        self.width = c["width_m"]
        self.length = c["length_m"]
        self.front_y = c["front_edge_y"]
        self.rear_y = c["rear_edge_y"]
        self.ventral_z = c["ventral_z"]
        self.top_z = c["dorsal_top_z"]
        self.rim_lo, self.rim_hi = c["rim_z"]
        sp = c["anterolateral_spines"]
        self.spine_tip = V3(sp["tip_L"])
        self.spine_len = sp["length_m"]
        self.spine_d = sp["base_diameter_m"]
        f = b["face"]
        self.mouth_size = f["mouth_plates"]["size_m"]
        self.mouth_c = V3(f["mouth_plates"]["center"])
        self.mand = V3(f["mandibles"]["L"])
        self.mand_len = f["mandibles"]["length_m"]
        e = b["eyes"]
        self.orbit = V3(e["orbit_L"])
        self.stalk_len = e["stalk_length_m"]
        self.stalk_d = e["stalk_diameter_m"]
        self.eye_d = e["eye_diameter_m"]
        a = b["antennae"]
        self.ant_base = V3(a["base_L"])
        self.ant_len = a["length_m"]
        self.ant_d = a["base_diameter_m"]
        ab = b["abdomen"]
        self.abd_size = ab["size_m"]
        self.abd_c = V3(ab["center"])
        cl = P["claws"]
        self.shoulder = V3(cl["shoulder_L"])
        self.claw = {"r": cl["R_crusher"], "l": cl["L_cutter"]}
        lg = P["legs"]
        self.leg_len = lg["base_lengths_m"]
        self.pair_scale = {int(k): v for k, v in lg["pair_scale"].items()}
        self.sections = lg["sections_m"]
        self.coxa_L = {int(k): V3(v) for k, v in lg["coxa_L"].items()}
        self.feet_L = {int(k): V3(v) for k, v in lg["rest_feet_L"].items()}
        self.groups = lg["gait_groups"]
        o = P["ornaments"]
        self.pearls = o["pearls"]
        self.gold = o["gold"]
        self.colors = {}

    def lengths(self, n):
        s = self.pair_scale[n]
        return {k: self.leg_len[k] * s for k in LEG_SEGS}

    def coxa(self, side, n):
        return mirror(self.coxa_L[n], side)

    def foot(self, side, n):
        return mirror(self.feet_L[n], side)

    def claw_len(self, side):
        c = self.claw[side]
        return c["arm"]["length_m"], c["forearm"]["length_m"], c["palm"]["length_m"], c["movable_finger_m"]


def spec_colors(spec):
    """Couleurs linéaires des matériaux déclarés dans la spec (chitine_dos, chitine_magenta, ventre...)."""
    out = {}
    for k, m in spec.get("materials", {}).items():
        out[k] = np.array(m["base_color"][:3], float)
    return out


# ============================================================================= cinématique

def leg_chain(cb, foot, L, dact_deg=-78.0, up=Z, yaw_dir=None, knee_up=True, knee_k=None):
    """Patte en espace corps : coxa horizontale vers le pied, IK planaire 2 os (mérus, propode), dactyle à angle
    absolu imposé dans le plan de la patte. Retourne (joints [cb, ct, genou, cheville, pointe], normale du plan, ok)."""
    cb, foot, up = V3(cb), V3(foot), V3(up).normalized()
    if yaw_dir is None:
        d = foot - cb
        d = d - up * d.dot(up)
    else:
        d = V3(yaw_dir)
    d.normalize()
    ct = cb + d * L["coxa"]
    a = math.radians(dact_deg)
    ddir = d * math.cos(a) + up * math.sin(a)
    ankle_t = foot - ddir * L["dactyl"]
    rel = ankle_t - ct
    h, v = rel.dot(d), rel.dot(up)
    D = math.hypot(h, v)
    L1, L2 = L["merus"], L["propodus"]
    ok = True
    Dc = min(max(D, abs(L1 - L2) + 1e-4), L1 + L2 - 1e-4)
    if Dc != D:
        ok = False
    phi = math.atan2(v, h)
    alpha = math.acos(max(-1.0, min(1.0, (L1 * L1 + Dc * Dc - L2 * L2) / (2 * L1 * Dc))))
    ang = phi + alpha if knee_up else phi - alpha
    if knee_k is not None:                    # v2 : côté du genou progressif (corps renversé : genou vers le ciel)
        ang = phi + alpha * knee_k
    knee = ct + (d * math.cos(ang) + up * math.sin(ang)) * L1
    ankle = knee + (ankle_t - knee).normalized() * L2
    tip = ankle + ddir * L["dactyl"]
    pn = d.cross(up).normalized()
    return [cb, ct, knee, ankle, tip], pn, d, ok


def leg_frames(joints, pn, up=Z):
    """Repères des 4 segments (coxa, mérus, propode, dactyle) : X = normale du plan, Z = côté supérieur."""
    out = []
    for i in range(4):
        a, b = joints[i], joints[i + 1]
        u = (b - a).normalized()
        zz = pn.cross(u) if i > 0 else V3(up)
        out.append(frame(a, b, zz))
    return out


def two_bone(root, target, L1, L2, pole):
    root, target, pole = V3(root), V3(target), V3(pole)
    d = target - root
    D = d.length
    Dc = min(max(D, abs(L1 - L2) + 1e-4), L1 + L2 - 1e-4)
    w = d.normalized()
    q = pole - w * pole.dot(w)
    if q.length < 1e-6:
        q = Z - w * w.z
    q.normalize()
    alpha = math.acos(max(-1.0, min(1.0, (L1 * L1 + Dc * Dc - L2 * L2) / (2 * L1 * Dc))))
    elbow = root + (w * math.cos(alpha) + q * math.sin(alpha)) * L1
    wrist = elbow + (root + w * Dc - elbow).normalized() * L2
    return elbow, wrist, w.cross(q).normalized(), D <= L1 + L2


def claw_frames(P, side, shoulder, wrist_t, pole, hand_dir, hand_up, gape_deg):
    """Pince : bras + avant-bras en IK 2 os (pôle), main orientée (direction, haut), doigt mobile ouvert de gape_deg.
    Retourne {os: Matrix} en espace corps et la position de la pointe du doigt mobile."""
    L1, L2, Lp, Lm = P.claw_len(side)
    elbow, wrist, pn, ok = two_bone(shoulder, wrist_t, L1, L2, pole)
    fa_dir = (wrist - elbow).normalized()
    M = {}
    M["arm"] = frame(shoulder, elbow, pn.cross((elbow - shoulder).normalized()))
    M["forearm"] = frame(elbow, wrist, pn.cross(fa_dir))
    h = V3(hand_dir).normalized()
    M["hand"] = frame(wrist, wrist + h * Lp, hand_up)
    hinge = finger_hinge(P, side)
    F = M["hand"] @ Matrix.Translation(hinge) @ Matrix.Rotation(math.radians(gape_deg), 4, "X")
    M["finger"] = F
    return M, ok


def finger_hinge(P, side):
    c = P.claw[side]
    Lp, H = c["palm"]["length_m"], c["palm"]["height_m"]
    return Vector((0.0, Lp - 0.07, 0.30 * H))


# ============================================================================= gabarit des os (repos)

CLAW_REST = {
    # poignet (repère corps, côté gauche ; miroir à droite), pôle du coude, direction et haut de la main, ouverture
    "l": dict(wrist=(1.10, -1.88, 1.20), pole=(1.0, 0.2, 0.55), hand=(-0.72, -0.66, -0.20), up=(-0.10, -0.05, 1.0),
              gape=15.0),
    "r": dict(wrist=(1.16, -1.84, 1.16), pole=(1.0, 0.2, 0.55), hand=(-0.62, -0.75, -0.24), up=(-0.10, -0.05, 1.0),
              gape=15.0),
}
EYE_REST_DIR = (0.30, -0.42, 0.86)
ANT_REST_DIRS = ((0.25, -0.85, 0.45), (0.55, -0.55, 0.62), (0.80, -0.05, 0.60))


def claw_rest(P, side):
    r = CLAW_REST[side]
    s = SGN[side]
    sh = P.shoulder.copy()
    sh.x *= s
    m = lambda v: Vector((v[0] * s, v[1], v[2]))  # noqa: E731
    return dict(shoulder=sh, wrist=m(r["wrist"]), pole=m(r["pole"]), hand=m(r["hand"]), up=m(r["up"]),
                gape=r["gape"])


class Layout:
    """Repères de repos de tous les os. bones[nom] = dict(M=Matrix 4x4 armature, length, parent, deform)."""

    def __init__(self, P):
        self.P = P
        self.bones = {}
        self.legs = {}
        self.order = []
        pv = P.pivot
        self.add("root", frame((0, 0, 0), (0, 0, 0.5), (0, -1, 0)), 0.5, None)
        self.add("body", frame(pv, pv + Vector((0, -0.6, 0)), Z), 0.6, "root")
        for s in SIDES:
            o = mirror(P.orbit, s)
            d = mirror(Vector(EYE_REST_DIR), s).normalized()
            Ms = frame(o, o + d * P.stalk_len, Vector((0, -1, 0.2)))
            self.add(f"eye_stalk_{s}", Ms, P.stalk_len, "body")
            e0 = o + d * P.stalk_len
            self.add(f"eye_{s}", frame(e0, e0 + d * P.eye_d, Vector((0, -1, 0.2))), P.eye_d, f"eye_stalk_{s}")
        for s in SIDES:
            p = mirror(P.ant_base, s)
            parent = "body"
            seg = P.ant_len / 3
            for i, dd in enumerate(ANT_REST_DIRS):
                d = mirror(Vector(dd), s).normalized()
                q = p + d * seg
                name = f"antenna_{s}_{i + 1:02d}"
                self.add(name, frame(p, q, Z), seg, parent)
                parent, p = name, q
            ab = mirror(Vector((0.075, -1.165, 1.585)), s)
            d = mirror(Vector((0.18, -0.55, 0.82)), s).normalized()
            self.add(f"antennule_{s}", frame(ab, ab + d * 0.25, Vector((0, -1, 0))), 0.25, "body")
        mc = P.mouth_c
        top = Vector((0.0, mc.y + 0.02, mc.z + P.mouth_size[2] / 2))
        self.add("mouth", frame(top, top + Vector((0, -0.02, -P.mouth_size[2])), Vector((0, -1, 0))),
                 P.mouth_size[2], "body")
        for s in SIDES:
            h = mirror(P.mand, s)
            t = h + mirror(Vector((-0.12, 0.03, -0.05)), s).normalized() * P.mand_len
            self.add(f"mandible_{s}", frame(h, t, Z), P.mand_len, "body")
        ac = P.abd_c
        L = P.abd_size[1]
        self.add("abdomen", frame((0, ac.y + L / 2, ac.z + 0.01), (0, ac.y - L / 2, ac.z + 0.01), Z), L, "body")
        for s in SIDES:
            r = claw_rest(P, s)
            M, ok = claw_frames(P, s, r["shoulder"], r["wrist"], r["pole"], r["hand"], r["up"], r["gape"])
            L1, L2, Lp, Lm = P.claw_len(s)
            parent = "body"
            for seg, ln in zip(CLAW_SEGS, (L1, L2, Lp, Lm)):
                self.add(f"claw_{s}_{seg}", M[seg], ln, parent)
                parent = f"claw_{s}_{seg}"
        for s in SIDES:
            for n in PAIRS:
                Lg = P.lengths(n)
                joints, pn, d, ok = leg_chain(P.coxa(s, n), P.foot(s, n), Lg)
                fr = leg_frames(joints, pn)
                self.legs[(s, n)] = dict(joints=joints, pn=pn, yaw=d, L=Lg, ok=ok)
                parent = "body"
                for seg, Mf in zip(LEG_SEGS, fr):
                    name = f"leg_{s}{n}_{seg}"
                    self.add(name, Mf, Lg[seg], parent)
                    parent = name
        # os non déformants (contrôle IK)
        for s in SIDES:
            for n in PAIRS:
                j = self.legs[(s, n)]["joints"]
                self.add(f"ik_leg_{s}{n}", frame(j[4], j[4] + Vector((0, 0, 0.25)), Vector((0, -1, 0))), 0.25,
                         "root", deform=False)
                pole = j[2] + (self.legs[(s, n)]["yaw"] * 0.4 + Z * 1.0)
                self.add(f"pole_leg_{s}{n}", frame(pole, pole + Vector((0, 0, 0.15)), Vector((0, -1, 0))), 0.15,
                         "root", deform=False)
            hand = self.bones[f"claw_{s}_hand"]
            ht = hand["M"].translation
            self.add(f"ik_claw_{s}", frame(ht, ht + Vector((0, -0.25, 0)), Z), 0.25, "body", deform=False)
            el = self.bones[f"claw_{s}_forearm"]["M"].translation
            pole = el + mirror(Vector((0.8, 0.1, 0.4)), s)
            self.add(f"pole_claw_{s}", frame(pole, pole + Vector((0, 0, 0.15)), Vector((0, -1, 0))), 0.15,
                     "body", deform=False)

    def add(self, name, M, length, parent, deform=True):
        self.bones[name] = dict(M=M.copy(), length=float(length), parent=parent, deform=deform)
        self.order.append(name)

    def M(self, name):
        return self.bones[name]["M"]

    def tail(self, name):
        b = self.bones[name]
        return b["M"] @ Vector((0, b["length"], 0))

    def deform_bones(self):
        return [n for n in self.order if self.bones[n]["deform"]]



# ============================================================================= primitives v2 (UV paramétriques)

def ring_mesh(R, pole0=None, pole1=None):
    """R (k, m, 3) : k anneaux de m points. Quads entre anneaux successifs (bouclés autour), éventails vers les pôles.
    UV par coin de face dans [0,1]² : u = j/m (la colonne de fermeture prend u = 1), v = abscisse curviligne des
    centres d'anneaux (pôles compris) normalisée. Retourne V (n,3), F, UV, taille physique (périmètre, longueur)."""
    R = np.asarray(R, float)
    k, m, _ = R.shape
    chain = ([np.asarray(pole0, float)] if pole0 is not None else []) + list(R.mean(1)) + \
        ([np.asarray(pole1, float)] if pole1 is not None else [])
    chain = np.array(chain)
    d = np.r_[0.0, np.cumsum(np.linalg.norm(np.diff(chain, axis=0), axis=1))]
    tot = max(float(d[-1]), 1e-6)
    vv = d / tot
    off = 1 if pole0 is not None else 0
    vr = vv[off:off + k]
    V = list(R.reshape(-1, 3))
    F, UV = [], []
    for i in range(k - 1):
        for j in range(m):
            j2 = (j + 1) % m
            u0, u1 = j / m, (j + 1) / m
            F.append((i * m + j, i * m + j2, (i + 1) * m + j2, (i + 1) * m + j))
            UV.append(((u0, vr[i]), (u1, vr[i]), (u1, vr[i + 1]), (u0, vr[i + 1])))
    if pole0 is not None:
        p = len(V)
        V.append(np.asarray(pole0, float))
        for j in range(m):
            u0, u1 = j / m, (j + 1) / m
            F.append((p, (j + 1) % m, j))
            UV.append((((u0 + u1) / 2, 0.0), (u1, vr[0]), (u0, vr[0])))
    if pole1 is not None:
        p = len(V)
        V.append(np.asarray(pole1, float))
        base = (k - 1) * m
        for j in range(m):
            u0, u1 = j / m, (j + 1) / m
            F.append((p, base + j, base + (j + 1) % m))
            UV.append((((u0 + u1) / 2, 1.0), (u0, vr[-1]), (u1, vr[-1])))
    perim = float(np.mean([np.sum(np.linalg.norm(np.roll(r, -1, 0) - r, axis=1)) for r in R]))
    return np.array(V), F, UV, (max(perim, 1e-3), tot)


RIDGE_ANGLES = (0.80, math.pi - 0.80, -0.80, -(math.pi - 0.80))


def section_pts(nseg, w, t, expo=2.3, keel_t=0.0, keel_b=0.0, keel_w=0.30, ridge=0.0):
    """Section de plaque : superellipse (X = épaisseur t, Z = hauteur w) avec carènes dorsale (+Z) et ventrale (-Z)
    et, si ridge > 0, quatre arêtes longitudinales entre les faces latérales et les carènes."""
    a = np.arange(nseg) * TAU / nseg
    ca, sa = np.cos(a), np.sin(a)
    e = 2.0 / expo
    cx = np.sign(ca) * np.abs(ca) ** e
    sz = np.sign(sa) * np.abs(sa) ** e
    dt = np.angle(np.exp(1j * (a - math.pi / 2)))
    db = np.angle(np.exp(1j * (a + math.pi / 2)))
    kz = 1.0 + keel_t * np.exp(-(dt / keel_w) ** 2) + keel_b * np.exp(-(db / keel_w) ** 2)
    rr = np.ones_like(a)
    if ridge:
        for ak in RIDGE_ANGLES:
            rr += ridge * np.exp(-(np.angle(np.exp(1j * (a - ak))) / 0.20) ** 2)
    return t / 2 * cx * rr, w / 2 * sz * kz * rr


def section_point(a, w, t, expo=2.3, keel_t=0.0, keel_b=0.0, keel_w=0.30, ridge=0.0):
    """Un point de la section à l'angle a et sa normale approchée (repère local X, Z)."""
    ca, sa = math.cos(a), math.sin(a)
    e = 2.0 / expo
    cx = math.copysign(abs(ca) ** e, ca)
    sz = math.copysign(abs(sa) ** e, sa)
    dt = math.atan2(math.sin(a - math.pi / 2), math.cos(a - math.pi / 2))
    db = math.atan2(math.sin(a + math.pi / 2), math.cos(a + math.pi / 2))
    kz = 1.0 + keel_t * math.exp(-(dt / keel_w) ** 2) + keel_b * math.exp(-(db / keel_w) ** 2)
    rr = 1.0 + sum(ridge * math.exp(-(math.atan2(math.sin(a - ak), math.cos(a - ak)) / 0.20) ** 2)
                   for ak in RIDGE_ANGLES) if ridge else 1.0
    x, z = t / 2 * cx * rr, w / 2 * sz * kz * rr
    n = np.array([ca / max(t, 1e-4), 0.0, sa / max(w, 1e-4)])
    return x, z, n / np.linalg.norm(n)


def plate_segment(y0, y1, prof, nseg, nrow, cap0=0.0, cap1=0.0, cap_rows=2, expo=2.3, tip=False, keel_w=0.30,
                  ridge=0.0):
    """Segment rigide le long de +Y (repère d'os) de y0 à y1. prof(tt) -> (w, t, xc, zc, keel_t, keel_b) avec
    tt dans [0, 1]. Calottes arrondies qui débordent de cap0 / cap1 dans l'articulation ; tip : pointe en (0, y1, 0)."""
    L = y1 - y0
    angs = (32.0, 62.0) if cap_rows >= 2 else ((48.0,) if cap_rows == 1 else ())
    rings = []

    def ring(tt, y, s=1.0):
        w, t, xc, zc, kt, kb = prof(tt)
        x, z = section_pts(nseg, w * s, t * s, expo, kt, kb, keel_w, ridge * s)
        return np.stack([xc + x, np.full(nseg, y), zc + z], 1)

    if cap0 > 0:
        for ang in angs:
            rings.append(ring(0.0, y0 - cap0 * math.cos(math.radians(ang)), math.sin(math.radians(ang))))
    ts = np.linspace(0.0, 1.0, max(nrow, 2))
    if tip:
        ts = ts[:-1]
    for tt in ts:
        rings.append(ring(float(tt), y0 + float(tt) * L))
    if cap1 > 0 and not tip:
        for ang in angs[::-1]:
            rings.append(ring(1.0, y1 + cap1 * math.cos(math.radians(ang)), math.sin(math.radians(ang))))
    _, _, xc0, zc0, _, _ = prof(0.0)
    _, _, xc1, zc1, _, _ = prof(1.0)
    pole0 = (xc0, y0 - cap0, zc0)
    pole1 = (0.0, y1, 0.0) if tip else (xc1, y1 + cap1, zc1)
    return ring_mesh(np.array(rings), pole0, pole1)


def _basis(y, ref=None):
    y = np.asarray(y, float)
    y = y / max(np.linalg.norm(y), 1e-9)
    if ref is None:
        ref = np.array([0, 0, 1.0]) if abs(y[2]) < 0.9 else np.array([1.0, 0, 0])
    x = np.cross(y, ref)
    if np.linalg.norm(x) < 1e-6:
        x = np.cross(y, np.array([1.0, 0, 0]))
    x /= np.linalg.norm(x)
    z = np.cross(x, y)
    return x, y, z


def spine_mesh(base, tip, r0, nseg, rows=3, bend=None, flat=1.0, trunc=1.0, jag=0.0, rng=None, expo=1.0):
    """Épine conique de base à pointe : courbure (bend, vecteur ajouté en t²), section aplatie sur x (flat), troncature
    (trunc < 1 : dent cassée, bord déchiqueté de jag). Retourne V, F, UV, taille, t le long (par sommet)."""
    base, tip = np.asarray(base, float), np.asarray(tip, float)
    ax = tip - base
    x, y, z = _basis(ax)
    bend = np.zeros(3) if bend is None else np.asarray(bend, float)
    a = np.arange(nseg) * TAU / nseg
    rings, ts = [], []
    n_r = rows if trunc >= 1.0 else rows + 1
    for i in range(n_r):
        t = (i / rows) * min(trunc, 1.0) if trunc < 1.0 else i / rows
        if trunc < 1.0 and i == n_r - 1:
            t = trunc
        c = base + ax * t + bend * t * t
        r = r0 * (1 - t) ** expo
        ring = c + np.outer(np.cos(a) * r * flat, x) + np.outer(np.sin(a) * r, z)
        if trunc < 1.0 and i == n_r - 1 and jag > 0 and rng is not None:
            ring = ring + np.outer(rng.uniform(-jag, jag, nseg) * r0, y)
        rings.append(ring)
        ts.append(np.full(nseg, t))
    if trunc < 1.0:
        top = base + ax * trunc * 0.97 + bend * trunc * trunc
    else:
        top = tip + bend
    V, F, UV, size = ring_mesh(np.array(rings), base - y * r0 * 0.25, top)
    tv = np.concatenate(ts + [[0.0], [min(trunc, 1.0)]])
    return V, F, UV, size, tv


def blob_mesh(center, radii, nu, nv, M=None):
    """Ellipsoïde (pôles en ±Z local) ; M = matrice 4x4 optionnelle."""
    th = np.linspace(0, math.pi, nv + 1)[1:-1]
    a = np.arange(nu) * TAU / nu
    rings = [np.stack([np.sin(t) * np.cos(a), np.sin(t) * np.sin(a), np.full(nu, np.cos(t))], 1) * radii + center
             for t in th]
    pole0 = np.asarray(center, float) + np.array([0, 0, radii[2]])
    pole1 = np.asarray(center, float) - np.array([0, 0, radii[2]])
    V, F, UV, size = ring_mesh(np.array(rings), pole0, pole1)
    if M is not None:
        Mn = np.array(M, float)
        V = V @ Mn[:3, :3].T + Mn[:3, 3]
    return V, F, UV, size


def bristle(base, tip, r, ref):
    """Soie : pyramide à 3 faces sans base (la base est noyée dans la cuticule)."""
    base, tip = np.asarray(base, float), np.asarray(tip, float)
    x, y, z = _basis(tip - base, ref)
    a = np.array([0.0, TAU / 3, 2 * TAU / 3])
    V = [base + r * (math.cos(t) * x + math.sin(t) * z) for t in a] + [tip]
    F = [(0, 1, 3), (1, 2, 3), (2, 0, 3)]
    UV = [((0.0, 0.0), (1 / 3, 0.0), (1 / 6, 1.0)), ((1 / 3, 0.0), (2 / 3, 0.0), (0.5, 1.0)),
          ((2 / 3, 0.0), (1.0, 0.0), (5 / 6, 1.0))]
    # faces vers l'extérieur de l'axe
    out = []
    for f, uv in zip(F, UV):
        p = [V[i] for i in f]
        nrm = np.cross(p[1] - p[0], p[2] - p[0])
        c = (p[0] + p[1] + p[2]) / 3
        axis_pt = base + y * np.dot(c - base, y)
        if np.dot(nrm, c - axis_pt) < 0:
            f, uv = tuple(reversed(f)), tuple(reversed(uv))
        out.append((f, uv))
    return np.array(V), [o[0] for o in out], [o[1] for o in out], (TAU * r, np.linalg.norm(tip - base))


def signed_volume(V, F):
    tot = 0.0
    for f in F:
        a = V[f[0]]
        for k in range(1, len(f) - 1):
            tot += float(np.dot(a, np.cross(V[f[k]], V[f[k + 1]])))
    return tot / 6.0


def tri_fan(F):
    T = []
    for f in F:
        for k in range(1, len(f) - 1):
            T.append((f[0], f[k], f[k + 1]))
    return np.array(T, np.int64).reshape(-1, 3)


def vnormals(V, F):
    T = tri_fan(F)
    N = np.zeros_like(V)
    if len(T):
        fn = np.cross(V[T[:, 1]] - V[T[:, 0]], V[T[:, 2]] - V[T[:, 0]])
        for k in range(3):
            np.add.at(N, T[:, k], fn)
    return N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-12)


def seg_n(base, q, lo=6):
    return max(lo, int(round(base * q / 4.0)) * 4)


# ============================================================================= pièce de maillage

class Part:
    """Pièce de maillage : sommets (n,3), faces, UV par coin de face (paramètres [0,1]² du rectangle de sa clé),
    emplacement de matériau, poids par os, couleurs et masques par sommet :
      base (n,3) albédo linéaire ; mask (n,4) : cavité (calculée à l'assemblage), usure, cicatrice, aléa ;
      mat (n,4) : membrane humide, incrustation (balane / corail, rugueux), métal, brillance (laque des pointes)."""

    __slots__ = ("V", "F", "UV", "FK", "keys", "slot", "W", "base", "mask", "mat", "kind")

    def __init__(self, V, F, UV, slot, weights, key=None, size=None, base=None, mask=None, mat=None, kind=None,
                 orient=True, FK=None, keys=None):
        self.V = np.asarray(V, float).reshape(-1, 3)
        self.F = [tuple(int(i) for i in f) for f in F]
        self.UV = [tuple((float(c[0]), float(c[1])) for c in uv) for uv in UV]
        if keys is None:
            self.keys = [(key, size)]
            self.FK = [0] * len(self.F)
        else:
            self.keys = list(keys)
            self.FK = list(FK)
        self.slot = slot
        n = len(self.V)
        if isinstance(weights, str):
            weights = {weights: np.ones(n)}
        self.W = {k: np.asarray(v, float) for k, v in weights.items()}
        self.base = np.tile(lin(PALETTE["black"])[:3], (n, 1)) if base is None else np.asarray(base, float).reshape(n, 3)
        self.mask = np.zeros((n, 4)) if mask is None else np.asarray(mask, float).reshape(n, 4)
        self.mat = np.zeros((n, 4)) if mat is None else np.asarray(mat, float).reshape(n, 4)
        self.kind = kind
        if orient and self.F and signed_volume(self.V, self.F) < 0:
            self.flip()

    def flip(self):
        self.F = [tuple(reversed(f)) for f in self.F]
        self.UV = [tuple(reversed(u)) for u in self.UV]

    def transform(self, M):
        M = np.array(M, float)
        self.V = self.V @ M[:3, :3].T + M[:3, 3]
        if np.linalg.det(M[:3, :3]) < 0:
            self.flip()
        return self

    def normals(self):
        return vnormals(self.V, self.F)


def merge_parts(items, slot, weights_fn, key):
    """Plusieurs petites pièces (épines d'une rangée, soies d'une patte...) en une seule pièce, un rectangle d'atlas
    partagé en colonnes : l'élément d'indice i (indice du LOD0) occupe u dans [i/K, (i+1)/K].
    items : liste de (indice, K, V, F, UV, taille, extras) ; weights_fn(V) -> dict os -> poids."""
    Vs, Fs, UVs = [], [], []
    off = 0
    wsum, hmax, K = 0.0, 0.0, 1
    extras = []
    for idx, K, V, F, UV, size, ex in items:
        Vs.append(V)
        Fs += [tuple(i + off for i in f) for f in F]
        UVs += [tuple(((idx + c[0]) / K, c[1]) for c in uv) for uv in UV]
        off += len(V)
        wsum += size[0]
        hmax = max(hmax, size[1])
        extras.append(ex)
    if not Vs:
        return None, []
    V = np.vstack(Vs)
    # taille du rectangle : largeur = somme des périmètres de la rangée complète (LOD0), hauteur = plus grande longueur
    width = wsum / len(items) * K
    return Part(V, Fs, UVs, slot, weights_fn(V), key, (width, hmax), orient=False), extras


def oriented(V, F, UV):
    """Faces d'une pièce fermée tournées vers l'extérieur (volume signé positif)."""
    if F and signed_volume(np.asarray(V, float), F) < 0:
        return [tuple(reversed(f)) for f in F], [tuple(reversed(u)) for u in UV]
    return F, UV


# ============================================================================= couleurs

class Pal:
    def __init__(self):
        self.c = {k: np.array(lin(v)[:3]) for k, v in PALETTE.items()}

    def __getitem__(self, k):
        return self.c[k]

    def mix(self, a, b, t):
        t = np.clip(np.asarray(t, float), 0, 1)[:, None]
        ca = self.c[a] if isinstance(a, str) else np.asarray(a, float)
        cb = self.c[b] if isinstance(b, str) else np.asarray(b, float)
        return ca * (1 - t) + cb * t


def chitin_color(pal, p, nz, ox=0.0, seed=0):
    """Chitine : noir violacé marbré de sang de bœuf ; dessous (nz < 0) plus rouge ; ox = biais rouge (bords,
    articulations), scalaire ou tableau."""
    p = np.asarray(p, float)
    n1 = AG.fbm(p * 1.9 + seed * 0.37, 3, seed=seed + 1)
    n2 = AG.fbm(p * 6.5 + 3.3 + seed * 0.11, 3, seed=seed + 2)
    red = np.clip(-0.08 + 0.42 * smooth(0.25, -0.75, nz) + ox + (n1 - 0.5) * 0.95, 0, 1) ** 1.4
    vio = np.clip((n2 - 0.42) * 2.0, 0, 1) * (1 - red) * 0.55
    c = pal["black"][None] * (1 - red[:, None]) + pal["oxblood"][None] * red[:, None]
    return c * (1 - vio[:, None]) + pal["violet"][None] * vio[:, None]


def spine_colors(pal, base_col, t, tip0=0.45, tip1=0.95):
    """Épine : base de la couleur locale -> os vers la pointe."""
    k = smooth(tip0, tip1, t)
    c = np.asarray(base_col, float) * (1 - k[:, None]) + pal["bone"][None] * k[:, None]
    dk = smooth(0.25, tip0, t) * (1 - k) * 0.35            # anneau sombre avant l'os
    return c * (1 - dk[:, None]) + pal["oxblood_dark"][None] * dk[:, None]


# ============================================================================= carapace

def outline(P, n_half, teeth=((0.62, 0.050), (0.78, 0.058), (0.94, 0.062), (1.10, 0.060), (1.24, 0.052))):
    """Contour vu de dessus (repris de la v1) ; dents antérolatérales plus nombreuses et plus aiguës (v2)."""
    fy, ry = P.front_y, P.rear_y
    hw = P.width / 2
    ctrl = [(0.0, fy), (0.22, fy - 0.012), (0.36, fy + 0.012), (0.50, fy + 0.055), (0.62, fy + 0.10),
            (0.86, fy + 0.22), (1.07, fy + 0.36), (1.24, fy + 0.49), (hw, fy + 0.60), (hw - 0.03, fy + 0.80),
            (1.24, fy + 1.12), (1.10, fy + 1.42), (0.90, fy + 1.72), (0.64, fy + 1.98), (0.34, ry - 0.04),
            (0.0, ry)]
    pts = [Vector((x, y, 0)) for x, y in ctrl]
    mir = [Vector((-p.x, p.y, 0)) for p in reversed(pts[1:-1])]
    loop = pts + mir
    dense = []
    m = len(loop)
    for i in range(m):
        p0, p1, p2, p3 = loop[i - 1], loop[i], loop[(i + 1) % m], loop[(i + 2) % m]
        for k in range(16):
            t = k / 16
            t2, t3 = t * t, t * t * t
            dense.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2
                                + (-p0 + 3 * p1 - 3 * p2 + p3) * t3))
    half = dense[: (len(ctrl) - 1) * 16 + 1]
    seg = [0.0]
    for a, b in zip(half[:-1], half[1:]):
        seg.append(seg[-1] + (b - a).length)
    tot = seg[-1]
    out = []
    j = 0
    for i in range(n_half + 1):
        s = tot * i / n_half
        while j < len(seg) - 2 and seg[j + 1] < s:
            j += 1
        t = (s - seg[j]) / max(seg[j + 1] - seg[j], 1e-9)
        out.append(half[j].lerp(half[j + 1], t))
    pts = np.array([[p.x, p.y] for p in out])
    pts[0, 0] = 0.0
    pts[-1, 0] = 0.0
    for tx, amp in teeth:
        d = np.abs(pts[:, 0] - tx)
        front = pts[:, 1] < (fy + 0.62)
        bump_ = amp * np.exp(-(d / 0.032) ** 2) * front
        nrm = np.stack([np.ones(len(pts)) * 0.6, -np.ones(len(pts)) * 0.8], 1)
        pts += nrm * bump_[:, None]
    full = np.vstack([pts, np.stack([-pts[-2:0:-1, 0], pts[-2:0:-1, 1]], 1)])
    return full


GROOVES = [
    ([(0.0, -0.10), (0.24, -0.13), (0.50, -0.24), (0.74, -0.40), (0.96, -0.55)], 0.032, 0.050),   # cervical
    ([(0.24, -0.13), (0.30, 0.12), (0.36, 0.40), (0.26, 0.70), (0.10, 0.88)], 0.026, 0.045),       # branchio-cardiaque
    ([(0.24, -0.13), (0.20, -0.42), (0.12, -0.70), (0.0, -0.92)], 0.022, 0.040),                   # gastrique (V)
    ([(0.50, -0.24), (0.74, -0.06), (0.94, 0.16), (1.02, 0.38)], 0.018, 0.040),                    # branchial
    ([(0.46, -1.06), (0.62, -1.00), (0.80, -0.94)], 0.030, 0.035),                                 # rainure orbitaire
]
LOBES = [((0.0, -0.52), 0.045, 0.30), ((0.0, 0.30), 0.035, 0.26), ((0.72, 0.02), 0.045, 0.40),
         ((-0.72, 0.02), 0.045, 0.40), ((0.66, -0.62), 0.028, 0.22), ((-0.66, -0.62), 0.028, 0.22),
         ((0.0, 0.80), 0.020, 0.25)]
# cicatrices du dessus (v2) : (points x, y), profondeur, demi-largeur
SCARS = [
    ([(0.30, -0.74), (0.60, -0.46), (0.94, -0.14)], 0.020, 0.032),          # trois griffures parallèles
    ([(0.20, -0.62), (0.52, -0.33), (0.86, -0.02)], 0.024, 0.036),
    ([(0.12, -0.49), (0.42, -0.21), (0.74, 0.10)], 0.018, 0.030),
    ([(-0.12, 0.84), (-0.30, 0.56), (-0.36, 0.33), (-0.58, 0.14), (-0.74, -0.04)], 0.016, 0.030),   # fissure
    ([(-0.52, -0.96), (-0.76, -0.84), (-1.00, -0.66)], 0.020, 0.030),       # entaille au bord avant droit
]


def groove_lines():
    out = []
    for pts, depth, w in GROOVES:
        a = np.array(pts, float)
        out.append((a, depth, w))
        b = a.copy()
        b[:, 0] *= -1
        out.append((b, depth, w))
    return out


def dist_to_polyline(p, line):
    best = np.full(len(p), 9.0)
    for a, b in zip(line[:-1], line[1:]):
        ab = b - a
        t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-12), 0, 1)
        d = np.linalg.norm(p - (a + t[:, None] * ab), axis=1)
        best = np.minimum(best, d)
    return best


def carapace(P, pal, q=1.0, seed=7):
    """Carapace fermée (v1 : dôme bas, sillons, lobes, front ridé, sternum) sans or, sillons plus creusés, granulation
    plus forte, cicatrices ; UV : dessus et dessous en projection plane, paroi (bord, front, flancs) en 4 quarts
    développés (u = position sur le contour, v = z)."""
    n_half = max(16, int(round(20 * q)) * 4)
    out2 = outline(P, n_half)
    N = len(out2)
    C = np.array([0.0, -0.05])
    rel = out2 - C
    n_top = max(5, int(round(28 * q)))
    n_rim = max(3, int(round(8 * q)))
    n_bot = max(5, int(round(20 * q)))
    n_face = max(4, int(round(15 * q)))
    s_top = (np.linspace(1.0, 0.05, n_top + 1) ** 0.85)[1:]
    s_bot = np.concatenate([np.linspace(1.0, 0.72, n_face + 1)[1:], np.linspace(0.72, 0.05, n_bot - n_face + 1)[1:]])
    hi, lo = P.rim_hi, P.rim_lo
    lip = 0.045
    rings = [("top", s, None) for s in s_top[::-1]]
    for k in range(n_rim):
        a = math.pi * k / (n_rim - 1)
        rings.append(("rim", 1.0 + lip * math.sin(a) / 1.35, (hi + lo) / 2 + (hi - lo) / 2 * math.cos(a)))
    rings += [("bot", s, None) for s in s_bot]
    R = len(rings)
    V = np.zeros((R, N, 3))
    region = np.zeros((R, N))
    sval = np.zeros((R, N))
    for r, (kind, s, z) in enumerate(rings):
        xy = C + rel * s
        V[r, :, :2] = xy
        sval[r] = s
        if kind == "top":
            dome = (P.top_z - hi) * (1 - min(s, 1.0) ** 2.3) ** 0.72
            gut = -0.022 * math.exp(-((s - 0.90) / 0.05) ** 2)
            V[r, :, 2] = hi + dome + gut
            region[r] = 0
        elif kind == "rim":
            V[r, :, 2] = z
            region[r] = 1
        else:
            t = smooth(0.70, 1.0, s)
            V[r, :, 2] = P.ventral_z + (lo - P.ventral_z) * t ** 0.85
            region[r] = 2 if s > 0.72 else 3
    xy = V[:, :, :2].reshape(-1, 2)
    zz = V[:, :, 2].reshape(-1)
    reg = region.reshape(-1)
    fy = P.front_y
    is_face = (reg == 2) & (xy[:, 1] < fy + 0.42) & (np.abs(xy[:, 0]) < 0.62)
    if is_face.any():
        k = smooth(0.62, 0.30, np.abs(xy[:, 0])) * smooth(fy + 0.42, fy + 0.18, xy[:, 1]) * is_face
        target_y = fy + 0.05 + (P.rim_lo - zz) * 0.33
        xy[:, 1] = xy[:, 1] * (1 - k) + np.minimum(xy[:, 1], target_y) * k
    face_k = (smooth(0.62, 0.36, np.abs(xy[:, 0])) * smooth(fy + 0.36, fy + 0.10, xy[:, 1]) * (reg >= 2)
              * smooth(P.ventral_z - 0.01, P.ventral_z + 0.06, zz))
    p3 = np.stack([xy[:, 0], xy[:, 1], zz], 1)
    top = reg == 0
    rimband = reg == 1
    disp = np.zeros(len(p3))
    for line, depth, w in groove_lines():
        d = dist_to_polyline(xy, line)
        disp -= depth * 1.35 * np.exp(-(d / w) ** 2) * top
    for (cx, cy), amp, rad in LOBES:
        d2 = (xy[:, 0] - cx) ** 2 + (xy[:, 1] - cy) ** 2
        disp += amp * 1.3 * np.exp(-d2 / rad ** 2) * top
    for s_ in SIDES:
        o = mirror(P.orbit, s_)
        d2 = (xy[:, 0] - o.x) ** 2 + (xy[:, 1] - o.y) ** 2
        disp -= 0.05 * np.exp(-d2 / 0.09 ** 2) * (top | rimband)
    n1 = AG.fbm(p3 * 3.2, 4, seed=seed)
    n2 = AG.fbm(p3 * 11.0, 3, seed=seed + 3)
    F1 = AG.worley(p3 * 14.0, seed=seed + 5)
    gran = (1 - smooth(0.0, 0.55, F1))
    disp += ((n1 - 0.5) * 0.034 + (n2 - 0.5) * 0.010 + gran * 0.010) * (top | rimband)
    # cicatrices : sillon creusé, lèvres en bourrelet
    scar = np.zeros(len(p3))
    for pts, depth, w in SCARS:
        line = np.array(pts, float)
        d = dist_to_polyline(xy, line)
        disp -= depth * np.exp(-(d / w) ** 2) * top
        disp += depth * 0.35 * np.exp(-((d - 1.7 * w) / (0.55 * w)) ** 2) * top
        scar = np.maximum(scar, np.exp(-(d / (2.0 * w)) ** 2) * top)
    # face ridée (v1)
    fz = (zz - P.ventral_z) / max(P.rim_lo - P.ventral_z, 1e-6)
    ph = (fz * 4.3 + 0.9 * np.abs(xy[:, 0]) ** 1.3 + 0.18 * np.sin(xy[:, 0] * 9.0 + 0.7)
          + 1.6 * (AG.fbm(p3 * 3.5, 3, seed=seed + 17) - 0.5))
    wav = np.sin(TAU * ph)
    lump = AG.fbm(p3 * 7.0, 3, seed=seed + 19)
    folds = 0.028 * np.sign(wav) * np.abs(wav) ** 0.6 * (0.35 + 0.9 * lump) + 0.02 * (lump - 0.5)
    crease = -0.03 * np.exp(-(xy[:, 0] / 0.05) ** 2) * smooth(0.15, 0.6, fz)
    brow = 0.034 * np.exp(-((fz - 0.86) / 0.08) ** 2)
    face_disp = (folds + crease + brow + (n2 - 0.5) * 0.02) * face_k
    stern = reg == 3
    disp_under = np.zeros(len(p3))
    disp_under -= 0.02 * smooth(0.5, 0.0, sval.reshape(-1)) * stern
    for k_ in range(4):
        yk = -0.42 + 0.42 * k_
        disp_under += 0.016 * np.exp(-((xy[:, 1] - yk) / 0.022) ** 2) * stern
    disp_under += 0.012 * np.exp(-(xy[:, 0] / 0.02) ** 2) * stern * smooth(0.15, 0.4, sval.reshape(-1))
    out = p3.copy()
    out[:, 2] += disp * (top | rimband) + disp_under
    if top.any():
        zmax = out[top, 2].max()
        k_ = (P.top_z - hi) / max(zmax - hi, 1e-6)
        out[top, 2] = hi + (out[top, 2] - hi) * k_
    radial = np.zeros_like(out)
    radial[:, :2] = xy - C
    radial /= np.maximum(np.linalg.norm(radial, axis=1, keepdims=True), 1e-9)
    out += radial * face_disp[:, None]
    # --- maillage (anneaux x contour) + pôles
    F = []
    for i in range(R - 1):
        for j in range(N):
            j2 = (j + 1) % N
            F.append((i * N + j, i * N + j2, (i + 1) * N + j2, (i + 1) * N + j))
    Vf = list(out)
    ptop = len(Vf)
    Vf.append(out[:N].mean(0) + np.array([0, 0, 0.004]))
    F += [(ptop, (j + 1) % N, j) for j in range(N)]
    pbot = len(Vf)
    Vf.append(out[(R - 1) * N:].mean(0) - np.array([0, 0, 0.004]))
    base_i = (R - 1) * N
    F += [(pbot, base_i + j, base_i + (j + 1) % N) for j in range(N)]
    Vf = np.array(Vf)
    nV = len(Vf)
    regv = np.concatenate([reg, [0, 3]])
    s_all = np.concatenate([sval.reshape(-1), [0.0, 0.0]])
    scar_v = np.concatenate([scar, [0.0, 0.0]])
    folds_v = np.concatenate([folds * face_k, [0.0, 0.0]])
    # --- UV : classes de faces (normale), dessus / dessous en projection plane, paroi développée par quarts
    idx_of = np.concatenate([np.tile(np.arange(N), R), [-1, -1]])
    T = np.array([f[:3] for f in F])
    fn = np.cross(Vf[T[:, 1]] - Vf[T[:, 0]], Vf[T[:, 2]] - Vf[T[:, 0]])
    fn /= np.maximum(np.linalg.norm(fn, axis=1, keepdims=True), 1e-12)
    # orientation : la normale du dessus doit monter
    flip = signed_volume(Vf, F) < 0
    if flip:
        F = [tuple(reversed(f)) for f in F]
        fn = -fn
    zr = (P.ventral_z - 0.10, P.top_z - 0.05)
    perim = float(np.sum(np.linalg.norm(np.roll(out2, -1, 0) - out2, axis=1)))
    top_box = (-P.width / 2 - 0.12, P.front_y - 0.12, P.width + 0.24, P.length + 0.24)
    keys = [("cara_top", (top_box[2], top_box[3])), ("cara_bot", (top_box[2] * 0.55, top_box[3] * 0.55))]
    keys += [(f"cara_wall_{k}", (perim / 4 * 1.0, zr[1] - zr[0])) for k in range(4)]
    UV, FK = [], []
    q8 = N // 8
    for fi, f in enumerate(F):
        nz = fn[fi, 2]
        if nz > 0.5 or nz < -0.5:
            topk = nz > 0
            UV.append(tuple(((Vf[i, 0] - top_box[0]) / top_box[2], (Vf[i, 1] - top_box[1]) / top_box[3]) for i in f))
            FK.append(0 if topk else 1)
            continue
        js = [idx_of[i] for i in f if idx_of[i] >= 0]
        j0 = min(js)
        if max(js) - j0 > N // 2:                    # face à cheval sur la fermeture du contour
            j0 = max(js)
        quad = ((j0 + q8) % N) // (N // 4)
        start = (quad * (N // 4) - q8) % N
        uv = []
        for i in f:
            j = idx_of[i]
            if j < 0:
                j = j0
            uu = ((j - start) % N)
            if uu > N // 4 + 2:
                uu -= N
            uv.append((uu / (N / 4), (Vf[i, 2] - zr[0]) / (zr[1] - zr[0])))
        UV.append(tuple(uv))
        FK.append(2 + quad)
    # --- couleurs : dessus noir violacé marbré, bords sang de bœuf, front sombre ridé, bouche, dessous os sale
    nrm = vnormals(Vf, F)
    ox = np.where(regv == 1, 0.55, 0.0) + np.where(regv == 0, smooth(0.80, 1.0, s_all) * 0.45, 0.0)
    ox = ox + np.where(regv == 2, 0.25 + 0.3 * np.clip(folds_v * 25, 0, 1), 0.0)
    col = chitin_color(pal, Vf, nrm[:, 2], ox, seed=seed)
    # cavité buccale (derrière les pièces buccales) : rouge noir humide
    mc = P.mouth_c
    mouth = (smooth(0.34, 0.24, np.abs(Vf[:, 0])) * smooth(mc.z - 0.20, mc.z - 0.13, Vf[:, 2])
             * smooth(mc.z + 0.20, mc.z + 0.12, Vf[:, 2]) * smooth(mc.y + 0.30, mc.y + 0.12, Vf[:, 1])
             * (regv >= 2))
    col = col * (1 - mouth[:, None]) + pal["mouth"][None] * mouth[:, None]
    bvar = AG.fbm(Vf * 4.5, 3, seed=seed + 29)
    stain = smooth(0.55, 0.75, AG.fbm(Vf * 2.2 + 5.0, 3, seed=seed + 31))
    belly = pal.mix("belly", "belly_stain", stain * 0.85) * (0.82 + 0.3 * bvar[:, None])
    bk = np.where(regv == 3, 1.0, 0.0) + np.where(regv == 2, smooth(0.95, 0.74, s_all) * 0.7, 0.0)
    bk = np.clip(bk * (1 - smooth(-0.2, 0.3, nrm[:, 2])), 0, 1)
    col = col * (1 - bk[:, None]) + belly * bk[:, None]
    mask = np.zeros((nV, 4))
    mask[:, 2] = scar_v
    mask[:, 3] = bvar
    mat = np.zeros((nV, 4))
    mat[:, 3] = mouth                                   # brillance humide de la cavité
    mat[:, 0] = mouth * 0.6
    part = Part(Vf, F, UV, SLOT_BODY, "body", base=col, mask=mask, mat=mat, orient=False, FK=FK, keys=keys)
    return part, dict(region=regv, s=s_all, outline=out2, center=C, n_half=n_half)


def surface_bvh(part):
    return BVHTree.FromPolygons(part.V.tolist(), [list(f) for f in part.F], all_triangles=False, epsilon=0.0)


def spine_group(items, slot, bone, key, pal, nseg, rows=3, tip0=0.45):
    """items : (indice LOD0, K, base, pointe, rayon, couleur de base, options) -> une pièce (un rectangle d'atlas)."""
    built = []
    for idx, K, base, tip, r0, col, opt in items:
        V, F, UV, size, tv = spine_mesh(base, tip, r0, nseg, rows=rows, bend=opt.get("bend"), flat=opt.get("flat", 1.0),
                                        trunc=opt.get("trunc", 1.0), jag=opt.get("jag", 0.0), rng=opt.get("rng"),
                                        expo=opt.get("expo", 0.9))
        F, UV = oriented(V, F, UV)
        built.append((idx, K, V, F, UV, size, (tv, col, opt)))
    part, extras = merge_parts(built, slot, lambda V: {bone: np.ones(len(V))}, key)
    if part is None:
        return None
    cols, tips, glossy = [], [], []
    for (tv, col, opt) in extras:
        if opt.get("plain"):
            c = np.tile(col, (len(tv), 1))
        else:
            c = spine_colors(pal, np.tile(col, (len(tv), 1)), tv, tip0=opt.get("tip0", tip0))
        if opt.get("trunc", 1.0) < 1.0:                # dent cassée : cassure brute plus sombre
            k = smooth(opt["trunc"] * 0.8, opt["trunc"], tv)
            c = c * (1 - k[:, None]) + pal["bone_dark"][None] * k[:, None]
        cols.append(c)
        tips.append(tv)
        glossy.append(np.full(len(tv), opt.get("gloss", 0.3)))
    part.base = np.vstack(cols)
    part.mat[:, 3] = np.concatenate(glossy) * smooth(0.5, 1.0, np.concatenate(tips))
    part.mask[:, 1] = smooth(0.6, 1.0, np.concatenate(tips)) * 0.8       # usure claire des pointes
    return part


def carapace_spines(P, pal, q, lod, cara, cdata, seed):
    """Bord frontal hérissé (rostre, sus-orbitaires, antérolatérales), grandes épines latérales, tubercules dorsaux."""
    parts = []
    rng = np.random.default_rng(seed + 401)
    bvh = surface_bvh(cara)
    nseg = seg_n(10, q, 5)
    hi, lo = P.rim_hi, P.rim_lo
    zrim = (hi + lo) / 2 + 0.03
    fy = P.front_y

    def col_at(p):
        return chitin_color(pal, np.array([p]), np.array([0.3]), 0.45, seed=seed)[0]

    def surf(p, d):
        """Point de surface : rayon depuis l'extérieur le long de -d."""
        o = Vector(p) + Vector(d) * 0.6
        hit = bvh.ray_cast(o, -Vector(d), 1.2)
        if hit[0] is None:
            return np.asarray(p, float), np.asarray(d, float)
        return np.array(hit[0]), np.array(hit[1])

    # grandes épines latérales (une pièce chacune)
    for s in SIDES:
        tip_v1 = np.array(mirror(P.spine_tip, s))
        d = np.array(mirror(Vector((1.0, -0.16, 0.20)), s).normalized())
        base = tip_v1 - d * (P.spine_len + 0.10)
        tip = base + d * (V2["lateral_spine_len"] + 0.10)
        items = [(0, 1, base, tip, 0.085, col_at(base), dict(bend=np.array([0.0, -0.07, 0.035]), expo=0.85))]
        parts.append(spine_group(items, SLOT_BODY, "body", f"cara_spine_lat_{s}", pal, seg_n(14, q, 6), rows=6))
    # rostre et épines sus-orbitaires (LOD0..2)
    front = []
    for s in SIDES:
        g = SGN[s]
        front += [((0.10 * g, fy - 0.01, zrim - 0.02), (0.10 * g, -0.95, -0.30), 0.21, 0.040),
                  ((0.24 * g, fy + 0.0, zrim), (0.22 * g, -0.95, -0.10), 0.11, 0.026),
                  ((0.33 * g, fy + 0.02, zrim + 0.01), (0.30 * g, -0.92, 0.05), 0.09, 0.022),
                  ((0.36 * g, fy + 0.07, hi + 0.035), (0.25 * g, -0.70, 0.67), 0.12, 0.026),
                  ((0.52 * g, fy + 0.11, hi + 0.03), (0.40 * g, -0.62, 0.68), 0.10, 0.024)]
    if lod <= 2:
        keep = range(len(front)) if lod <= 1 else [0, 5]
        items = []
        for i in keep:
            p, d, L, r = front[i]
            d = np.array(d) / np.linalg.norm(d)
            sp, _ = surf(np.array(p), d)
            base = sp - d * L * 0.25
            items.append((i, len(front), base, base + d * L, r, col_at(base), dict(bend=np.array([0, 0, -0.012]))))
        parts.append(spine_group(items, SLOT_BODY, "body", "cara_spine_front", pal, nseg))
    # épines antérolatérales le long du contour
    if lod <= 1:
        o2 = cdata["outline"]
        n_half = cdata["n_half"]
        side_pts = o2[: n_half + 1]
        sel = np.nonzero((side_pts[:, 0] > 0.50) & (side_pts[:, 0] < 1.26) & (side_pts[:, 1] < fy + 0.58))[0]
        m = V2["margin_spines"]
        picks = np.linspace(sel[0], sel[-1], m + 2)[1:-1].astype(int) if len(sel) > m else sel
        for s in SIDES:
            g = SGN[s]
            items = []
            for k, ii in enumerate(picks):
                if lod == 1 and k % 2 == 1:
                    continue
                p = side_pts[ii].copy()
                tng = side_pts[min(ii + 1, len(side_pts) - 1)] - side_pts[max(ii - 1, 0)]
                nrm2 = np.array([tng[1], -tng[0]])
                nrm2 /= np.linalg.norm(nrm2)
                if nrm2[0] < 0:
                    nrm2 = -nrm2
                f = k / max(len(picks) - 1, 1)
                d = np.array([nrm2[0] * g, nrm2[1] - 0.35, 0.28])
                d /= np.linalg.norm(d)
                L = 0.10 + 0.12 * f + rng.uniform(-0.01, 0.02)
                p3 = np.array([p[0] * g, p[1], zrim])
                sp, _ = surf(p3, d)
                base = sp - d * L * 0.22
                items.append((k, len(picks), base, base + d * L, 0.028 + 0.016 * f, col_at(base),
                              dict(bend=np.array([0, -0.015, 0.01]))))
            parts.append(spine_group(items, SLOT_BODY, "body", f"cara_spine_margin_{s}", pal, nseg))
    # grandes épines sur les lobes (gastrique, branchiaux, cardiaque)
    if lod <= 2:
        majors = [(0.0, -0.55), (0.30, -0.70), (-0.30, -0.70), (0.72, -0.10), (-0.72, -0.10), (0.55, 0.30),
                  (-0.55, 0.30), (0.0, 0.25), (0.22, 0.62), (-0.22, 0.62)]
        items = []
        for i, (x, y) in enumerate(majors):
            L = 0.13 + 0.05 * rng.random()
            if lod == 2 and i % 3:
                continue
            hit = bvh.ray_cast(Vector((x, y, 3.0)), Vector((0, 0, -1)), 4.0)
            if hit[0] is None:
                continue
            p, n = np.array(hit[0]), np.array(hit[1])
            d = n + np.array([0.0, 0.35, 0.0])
            d /= np.linalg.norm(d)
            base = p - d * L * 0.22
            items.append((i, len(majors), base, base + d * L, 0.040, col_at(base), dict(bend=np.array([0, 0.02, 0]))))
        parts.append(spine_group(items, SLOT_BODY, "body", "cara_spine_major", pal, seg_n(9, q, 5), rows=4))
    # tubercules épineux dorsaux (échantillonnage de Poisson sur le dessus)
    if lod <= 1:
        n_target = V2["dorsal_spines"]
        pts = []
        tries = 0
        orbit = [np.array([mirror(P.orbit, s).x, mirror(P.orbit, s).y]) for s in SIDES]
        while len(pts) < n_target and tries < 20000:
            tries += 1
            x = rng.uniform(-1.25, 1.25)
            y = rng.uniform(fy + 0.10, P.rear_y - 0.12)
            r2 = np.array([x, y])
            if any(np.linalg.norm(r2 - o) < 0.20 for o in orbit):
                continue
            if any(np.linalg.norm(r2 - p_) < 0.145 for p_ in pts):
                continue
            hit = bvh.ray_cast(Vector((x, y, 3.0)), Vector((0, 0, -1)), 4.0)
            if hit[0] is None or hit[1].z < 0.55:
                continue
            pts.append(r2)
        groups = 6
        for gi in range(groups):
            items = []
            mine = [i for i in range(len(pts)) if i % groups == gi]
            K = len(mine)
            for k, i in enumerate(mine):
                if lod == 1 and k % 2 == 1:
                    continue
                x, y = pts[i]
                hit = bvh.ray_cast(Vector((x, y, 3.0)), Vector((0, 0, -1)), 4.0)
                p = np.array(hit[0])
                n = np.array(hit[1])
                d = n + np.array([0.0, 0.30, 0.0]) + np.array([x, 0, 0]) * 0.12
                d /= np.linalg.norm(d)
                frontk = float(smooth(0.3, -0.9, y))
                L = (0.045 + 0.05 * rng.random()) * (1 + 0.8 * frontk)
                base = p - d * L * 0.25
                items.append((k, K, base, base + d * L, L * 0.34, col_at(base), dict()))
            sp = spine_group(items, SLOT_BODY, "body", f"cara_spine_dorsal_{gi}", pal, max(5, nseg - 2))
            if sp is not None:
                parts.append(sp)
    return [p for p in parts if p is not None]


# ============================================================================= pattes (v2)

def membrane_part(M, ya, yb, w, t, nseg, nrow, boneA, boneB, key, pal, folds=2, slot=SLOT_LEGS, seed=0):
    """Membrane articulaire plissée (soufflet) dans le repère M, de ya à yb le long de Y ; poids en dégradé de boneA
    (côté ya) à boneB (côté yb)."""
    def prof(tt):
        f = 1.0 + 0.09 * math.sin(tt * math.pi * 2 * folds + 0.5) - 0.06 * math.sin(math.pi * tt)
        return w * f, t * f, 0.0, 0.0, 0.0, 0.0
    V, F, UV, size = plate_segment(ya, yb, prof, nseg, nrow, cap0=0.0, cap1=0.0, expo=2.0)
    yl = V[:, 1].copy()
    s = smooth(ya + 0.15 * (yb - ya), yb - 0.15 * (yb - ya), yl)
    W = {boneA: 1 - s, boneB: s} if boneA != boneB else {boneA: np.ones(len(V))}
    if boneA != boneB:
        W = {k: v for k, v in W.items()}
    p = Part(V, F, UV, slot, W, key, (size[0] * 0.8, size[1] * 0.8))
    p.transform(M)
    fold = 0.5 + 0.5 * np.sin((yl - ya) / max(yb - ya, 1e-6) * math.pi * 2 * folds + 0.5)
    n = AG.fbm(p.V * 9.0 + seed, 2, seed=seed + 61)
    p.base = pal.mix("membrane", "membrane_dark", np.clip(0.25 + 0.6 * fold + (n - 0.5) * 0.5, 0, 1))
    p.mat[:, 0] = 1.0
    p.mat[:, 3] = 0.6
    return p


def leg_parts(P, Lay, pal, q, side, n, lod, seed):
    """Patte ambulatoire : coxa, basis-ischion, mérus, carpe, propode, dactyle en plaques rigides, membranes plissées,
    carènes dentelées, épines, soies ; balanes, corail et cicatrices sur les pattes arrière."""
    parts = []
    rng = np.random.default_rng(seed * 101 + n * 7 + (0 if side == "l" else 3))
    sc = P.pair_scale[n]
    k = sc ** 0.7
    Lg = P.lengths(n)
    S = P.sections
    nseg = seg_n(16, q)
    rear = n >= 3

    def nrow(L):
        return max(3, int(round(L * 21 * q))) + 1

    bones = {s: f"leg_{side}{n}_{s}" for s in LEG_SEGS}
    M = {s: np.array(Lay.M(bones[s])) for s in LEG_SEGS}
    tag = f"leg_{side}{n}"
    Wc, Tc = S["coxa"][0] * k, S["coxa"][1] * k * 0.92
    Wm, Tm = S["merus"][0] * k * 1.04, S["merus"][1] * k * 0.78         # plaques comprimées latéralement
    Wp, Tp = S["propodus"][0] * k * 1.04, S["propodus"][1] * k * 0.80
    Wd = S["dactyl"][0] * k
    Lc, Lm, Lp, Ld = Lg["coxa"], Lg["merus"], Lg["propodus"], Lg["dactyl"]
    cap_rows = 2 if lod < 2 else 1
    KW, EXPO, RIDGE = 0.22, 2.9, 0.06

    def prof_coxa(tt):
        b = 0.06 * bump(tt, 0.55, 0.12)
        return Wc * (1.04 - 0.06 * tt + b), Tc * (1.0 - 0.05 * tt + b), 0.0, 0.0, 0.03, 0.0

    def prof_basis(tt):
        g = -0.05 * bump(tt, 0.48, 0.07)                       # suture basis / ischion
        return Wm * (0.80 + 0.10 * tt + g), Tm * (0.82 + 0.08 * tt + g), 0.0, 0.0, 0.05, 0.03

    def prof_merus(tt):
        bl = bump(tt, 0.42, 0.30)
        fl = smooth(0.88, 1.0, tt)
        return (Wm * (0.93 + 0.16 * bl) * (1 - 0.14 * tt) * (1 + 0.07 * fl),
                Tm * (0.95 + 0.10 * bl) * (1 - 0.12 * tt) * (1 + 0.05 * fl),
                0.0, 0.03 * Lm * math.sin(math.pi * tt), 0.22, 0.15)

    def prof_carpus(tt):
        bl = bump(tt, 0.5, 0.32)
        return Wp * (1.04 + 0.16 * bl), Tp * (1.05 + 0.14 * bl), 0.0, 0.0, 0.18, 0.08

    def prof_prop(tt):
        bl = bump(tt, 0.35, 0.30)
        return (Wp * (0.98 + 0.10 * bl) * (1 - 0.20 * tt) * (1 + 0.05 * smooth(0.9, 1.0, tt)),
                Tp * (0.98 + 0.08 * bl) * (1 - 0.16 * tt), 0.0, -0.02 * Lp * math.sin(math.pi * tt), 0.20, 0.14)

    def prof_dact(tt):
        f = max(1.0 - tt ** 1.5, 0.0) ** 0.75
        b = 1.0 + 0.10 * bump(tt, 0.2, 0.25)
        return (Wd * 1.05 * b * f + 0.003, Wd * 0.74 * b * f + 0.003, 0.0,
                0.075 * Ld * math.sin(math.pi * tt ** 0.85), 0.16, 0.14)

    segs = [("coxa", "coxa", 0.0, Lc, prof_coxa, 0.09, 0.05, False),
            ("basis", "merus", 0.012 * Lm, 0.165 * Lm, prof_basis, 0.035, 0.03, False),
            ("merus", "merus", 0.19 * Lm, Lm, prof_merus, 0.035, 0.10, False),
            ("carpus", "propodus", 0.0, 0.24 * Lp, prof_carpus, 0.10, 0.03, False),
            ("propodus", "propodus", 0.275 * Lp, Lp, prof_prop, 0.03, 0.085, False),
            ("dactyl", "dactyl", 0.0, Ld, prof_dact, 0.07, 0.0, True)]
    profs = {}
    for key, bone, y0, y1, prof, c0, c1, tip in segs:
        profs[key] = (bone, y0, y1, prof)
        V, F, UV, size = plate_segment(y0, y1, prof, nseg, nrow(y1 - y0), cap0=c0, cap1=c1, cap_rows=cap_rows,
                                       tip=tip, expo=EXPO, keel_w=KW, ridge=0.0 if key in ("coxa", "basis") else RIDGE)
        yl = V[:, 1].copy()
        xl, zl = V[:, 0].copy(), V[:, 2].copy()
        p = Part(V, F, UV, SLOT_LEGS, bones[bone], f"{tag}_{key}", size)
        p.transform(M[bone])
        nr = p.normals()
        tt = np.clip((yl - y0) / max(y1 - y0, 1e-6), 0, 1)
        joint = np.maximum(smooth(0.12, 0.0, tt), smooth(0.88, 1.0, tt)) * 0.30
        vent = smooth(0.1, -0.9, zl / np.maximum(np.abs(zl).max(), 1e-6))      # face ventrale du segment
        col = chitin_color(pal, p.V, nr[:, 2], joint + 0.30 * vent, seed=seed + n)
        if key == "dactyl":
            tipk = smooth(0.66, 0.90, tt)
            col = col * (1 - tipk[:, None]) + pal["tip"][None] * tipk[:, None]
            p.mat[:, 3] = tipk
            p.mask[:, 1] = smooth(0.75, 1.0, tt) * 0.4
        p.base = col
        p.mask[:, 3] = rng.random()
        # cicatrices (pattes arrière : griffures en diagonale sur le mérus et le propode)
        if rear and key in ("merus", "propodus"):
            ang = np.arctan2(zl, xl)
            arc = ang * (Wm if key == "merus" else Wp) * 0.45
            yy = yl - y0
            sc_m = np.zeros(len(V))
            for a0, a1, b0, b1, wdt in ((0.20, 0.62, -0.10, 0.18, 0.018), (0.48, 0.80, -0.16, 0.06, 0.014)):
                A = np.array([a0 * (y1 - y0), b0])
                B = np.array([a1 * (y1 - y0), b1])
                pts2 = np.stack([yy, arc], 1)
                d = dist_to_polyline(pts2, np.array([A, B]))
                sc_m = np.maximum(sc_m, np.exp(-(d / wdt) ** 2))
            p.mask[:, 2] = sc_m * (1.0 if (n + (0 if side == "l" else 1)) % 2 == 0 else 0.6)
        parts.append(p)
    # membranes articulaires (LOD3 : aucune)
    if lod <= 2:
        mseg, mrow = max(8, nseg), (6 if lod == 0 else 4)
        mems = [("body", "coxa", "coxa", -0.07, 0.02, Wc * 0.92, Tc * 0.92, "mem_body"),
                ("coxa", "merus", "merus", -0.045, 0.02, Wm * 0.74, Tm * 0.76, "mem_coxa"),
                ("merus", "merus", "merus", 0.158 * Lm, 0.197 * Lm, Wm * 0.80, Tm * 0.80, "mem_basis"),
                ("merus", "propodus", "propodus", -0.08, 0.035, Wp * 0.92, Tp * 0.92, "mem_knee"),
                ("propodus", "propodus", "propodus", 0.232 * Lp, 0.283 * Lp, Wp * 0.88, Tp * 0.88, "mem_carpus"),
                ("propodus", "dactyl", "dactyl", -0.06, 0.03, Wd * 0.95, Wd * 0.80, "mem_ankle")]
        for a, b, fr, ya, yb, w, t, key in mems:
            if lod == 2 and key not in ("mem_knee", "mem_coxa"):
                continue
            ba = "body" if a == "body" else bones[a]
            parts.append(membrane_part(M[fr], ya, yb, w, t, mseg, mrow, ba, bones[b], f"{tag}_{key}", pal,
                                       seed=seed + n))
    # épines (LOD0 / LOD1) : carène dorsale dentelée du mérus, épines ventrales, carpe, propode, coxa
    snseg = seg_n(7, q, 5) if lod == 0 else 5

    def keel_point(key, tt, top=True):
        bone, y0, y1, prof = profs[key]
        w, t, xc, zc, kt, kb = prof(tt)
        z = zc + (w / 2 * (1 + kt) if top else -w / 2 * (1 + kb))
        return np.array([xc, y0 + tt * (y1 - y0), z]), bone

    def side_point(key, tt, ang):
        bone, y0, y1, prof = profs[key]
        w, t, xc, zc, kt, kb = prof(tt)
        x, z, nl = section_point(ang, w, t, EXPO, kt, kb, KW, RIDGE)
        return np.array([xc + x, y0 + tt * (y1 - y0), zc + z]), nl, bone

    def rowspec(key, count, t0, t1, top, dir_, Lr, rr, jitter=0.03):
        out = []
        for i, tt in enumerate(np.linspace(t0, t1, count)):
            tt = float(np.clip(tt + rng.uniform(-jitter, jitter), 0.02, 0.98))
            p, bone = keel_point(key, tt, top)
            d = np.array(dir_, float) + rng.normal(0, 0.10, 3) * np.array([1.0, 0.4, 0.2])
            d /= np.linalg.norm(d)
            L = rng.uniform(*Lr) * sc
            out.append((i, count, p, d, L, rr * L, bone))
        return out

    def siderow(key, count, t0, t1, ang, Lr, rr):
        out = []
        for i, tt in enumerate(np.linspace(t0, t1, count)):
            tt = float(np.clip(tt + rng.uniform(-0.03, 0.03), 0.04, 0.96))
            a_ = ang + rng.uniform(-0.12, 0.12)
            p, nl, bone = side_point(key, tt, a_)
            d = nl + np.array([0.0, 0.55, 0.0]) + rng.normal(0, 0.10, 3)
            d /= np.linalg.norm(d)
            L = rng.uniform(*Lr) * sc
            out.append((i, count, p, d, L, rr * L, bone))
        return out

    rows = [("serr", rowspec("merus", 13, 0.05, 0.95, True, (0, 0.60, 0.80), (0.060, 0.110), 0.33), 2),
            ("mspv", rowspec("merus", 6, 0.18, 0.88, False, (0, 0.45, -0.89), (0.080, 0.150), 0.30), 2),
            ("msla", siderow("merus", 5, 0.15, 0.85, 0.35, (0.030, 0.055), 0.40), 2),
            ("mslp", siderow("merus", 5, 0.20, 0.90, math.pi - 0.35, (0.030, 0.055), 0.40), 2),
            ("carp", rowspec("carpus", 3, 0.25, 0.80, True, (0, 0.35, 0.94), (0.100, 0.160), 0.30), 1),
            ("prop", rowspec("propodus", 8, 0.10, 0.92, True, (0, 0.55, 0.84), (0.050, 0.090), 0.33), 2),
            ("prpv", rowspec("propodus", 4, 0.20, 0.80, False, (0, 0.50, -0.86), (0.040, 0.070), 0.33), 2),
            ("dacv", rowspec("dactyl", 4, 0.10, 0.45, False, (0, 0.55, -0.84), (0.025, 0.040), 0.38), 2),
            ("coxs", rowspec("coxa", 2, 0.3, 0.7, True, (0, 0.2, 0.98), (0.05, 0.07), 0.38), 2)]
    if lod <= 1:
        for key, spec, step in rows:
            items = []
            for i, K, p, d, L, r0, bone in spec:
                if lod == 1 and i % step == 1:
                    continue
                Mb = M[bone]
                bw = Mb[:3, :3] @ (p - d * L * 0.3) + Mb[:3, 3]
                tw = Mb[:3, :3] @ (p + d * L * 0.7) + Mb[:3, 3]
                col = chitin_color(pal, np.array([bw]), np.array([0.0]), 0.35, seed=seed)[0]
                items.append((i, K, bw, tw, r0, col, dict(bend=Mb[:3, :3] @ np.array([0, 0.25, 0]) * L * 0.3)))
            sp = spine_group(items, SLOT_LEGS, bones[spec[0][6]], f"{tag}_sp_{key}", pal, snseg)
            if sp is not None:
                parts.append(sp)
    # soies : touffes sur la carène ventrale du propode et les deux carènes du dactyle
    if lod <= 1:
        tufts = []
        for i, tt in enumerate((0.25, 0.45, 0.65, 0.85)):
            tufts.append(("propodus", tt, False, (0, 0.5, -0.86)))
        for tt in (0.15, 0.32, 0.50):
            tufts.append(("dactyl", tt, True, (0, 0.4, 0.92)))
            tufts.append(("dactyl", tt, False, (0, 0.4, -0.92)))
        built = []
        nb = 7
        K = len(tufts) * nb
        draws = [[(rng.normal(0, 0.32, 3), rng.uniform(0.045, 0.085), rng.normal(0, 0.008, 3)) for _ in range(nb)]
                 for _ in tufts]                          # tirages du LOD0, identiques à tous les LOD
        for ti, (key, tt, top, d0) in enumerate(tufts):
            if lod == 1 and ti % 2 == 1:
                continue
            p, bone = keel_point(key, tt, top)
            Mb = M[bone]
            for b in range(nb if lod == 0 else 4):
                jd, jl, jb = draws[ti][b]
                d = np.array(d0) + jd
                d /= np.linalg.norm(d)
                L = jl * sc
                base = p + jb - d * 0.006
                bw = Mb[:3, :3] @ base + Mb[:3, 3]
                tw = Mb[:3, :3] @ (base + d * L) + Mb[:3, 3]
                ref = Mb[:3, :3] @ np.array([1.0, 0, 0])
                V, F, UV, size = bristle(bw, tw, 0.0035, ref)
                built.append((ti * nb + b, K, V, F, UV, size, bone))
        if built:
            weights = {}
            Vs = []
            for item in built:
                Vs.append((item[6], len(item[2])))
            part, extras = merge_parts([b[:6] + (b[6],) for b in built], SLOT_LEGS, lambda V: {}, f"{tag}_setae")
            wts = {}
            off = 0
            total = len(part.V)
            for bone, cnt in Vs:
                arr = wts.setdefault(bones[bone], np.zeros(total))
                arr[off:off + cnt] = 1.0
                off += cnt
            part.W = wts
            tv = np.tile([0.0, 0.0, 0.0, 1.0], len(built))
            part.base = pal.mix("setae", "bone_dark", tv * 0.5)
            parts.append(part)
    # balanes, corail (pattes arrière)
    if rear and lod <= 1:
        barn = []
        spots = [("merus", 0.30, 4), ("merus", 0.62, 5), ("merus", 0.82, 3), ("propodus", 0.45, 3)]
        idx = 0
        Kb = sum(c for _, _, c in spots)
        for key, tc, cnt in spots:
            bone, y0, y1, prof = profs[key]
            ac = rng.choice([0.0, math.pi]) + rng.uniform(-0.5, 0.5)
            for j in range(cnt):
                tt = float(np.clip(tc + rng.normal(0, 0.05), 0.08, 0.92))
                a = ac + rng.normal(0, 0.35)
                w, t, xc, zc, kt, kb = prof(tt)
                x, z, nrm2 = section_point(a, w, t, EXPO, kt, kb, KW, RIDGE)
                pl = np.array([xc + x, y0 + tt * (y1 - y0), zc + z])
                r = rng.uniform(0.028, 0.055) * sc
                if lod == 1 and j % 2 == 1:
                    idx += 1
                    continue
                barn.append((idx, Kb, pl, nrm2, r, bone))
                idx += 1
        if barn:
            built = []
            for i, K, pl, nl, r, bone in barn:
                Mb = M[bone]
                pw = Mb[:3, :3] @ pl + Mb[:3, 3]
                nw = Mb[:3, :3] @ nl
                V, F, UV, size = barnacle_mesh(pw, nw, r, max(6, seg_n(9, q, 6)), rng)
                built.append((i, K, V, F, UV, size, bone))
            part, _ = merge_parts([b[:6] + (b[6],) for b in built], SLOT_LEGS, lambda V: {}, f"{tag}_barn")
            wts, off = {}, 0
            for b in built:
                arr = wts.setdefault(bones[b[6]], np.zeros(len(part.V)))
                arr[off:off + len(b[2])] = 1.0
                off += len(b[2])
            part.W = wts
            part.base, part.mat = barnacle_colors(pal, part.V, built)
            parts.append(part)
        if lod == 0 and n == 4:
            bone, y0, y1, prof = profs["merus"]
            w, t, xc, zc, kt, kb = prof(0.55)
            x, z, nrm2 = section_point(1.2, w, t, EXPO, kt, kb, KW, RIDGE)
            pl = np.array([xc + x, y0 + 0.55 * (y1 - y0), zc + z])
            Mb = M[bone]
            cp = coral_part(Mb[:3, :3] @ pl + Mb[:3, 3], Mb[:3, :3] @ nrm2, 0.10 * sc, bones[bone], f"{tag}_coral",
                            pal, rng, q)
            parts.append(cp)
    return parts


def barnacle_mesh(p, n, r, nseg, rng):
    """Balane en cône volcanique : pied noyé, flancs côtelés, cratère (opercule sombre)."""
    p, n = np.asarray(p, float), np.asarray(n, float)
    x, y, z = _basis(n)
    h = r * rng.uniform(1.0, 1.5)
    a = np.arange(nseg) * TAU / nseg
    rib = 1.0 + 0.10 * np.cos(a * 6 + rng.uniform(0, 6))
    prof = [(-0.25 * r, 1.05), (0.0, 1.0), (0.45 * h, 0.80), (0.85 * h, 0.58), (1.0 * h, 0.50), (0.92 * h, 0.36),
            (0.72 * h, 0.22)]
    rings = []
    for hh, rr in prof:
        rings.append(p + np.outer(np.cos(a) * r * rr * rib, x) + np.outer(np.sin(a) * r * rr * rib, z)
                     + y * hh)
    V, F, UV, size = ring_mesh(np.array(rings), p - y * 0.4 * r, p + y * 0.62 * h)
    F, UV = oriented(V, F, UV)
    return V, F, UV, size


def barnacle_colors(pal, Vall, built):
    cols, mats = [], []
    for b in built:
        nv = len(b[2])
        V = b[2]
        p0 = V[:len(V)].mean(0)
        hh = np.linalg.norm(V - V[0], axis=1)
        crater = np.zeros(nv)
        # anneaux 5 et 6 (cratère) et pôle supérieur : opercule sombre
        nseg = (nv - 2) // 7
        crater[5 * nseg:7 * nseg] = 1.0
        crater[-1] = 1.0
        nz = AG.fbm(V * 30.0, 2, seed=5)
        c = pal.mix("barnacle", "barnacle_dark", np.clip(crater * 0.9 + (nz - 0.5) * 0.4, 0, 1))
        cols.append(c)
        m = np.zeros((nv, 4))
        m[:, 1] = 1.0
        mats.append(m)
    return np.vstack(cols), np.vstack(mats)


def coral_part(p, n, size, bone, key, pal, rng, q):
    """Petit corail branchu (3 à 5 branches courbes)."""
    items = []
    nb = 5
    for b in range(nb):
        d = np.asarray(n, float) + rng.normal(0, 0.55, 3)
        d /= np.linalg.norm(d)
        L = size * rng.uniform(0.6, 1.0)
        base = np.asarray(p, float) - d * 0.01
        items.append((b, nb, base, base + d * L, 0.016, pal["coral_bleached"] if b % 2 else pal["coral_red"],
                      dict(bend=rng.normal(0, 0.02, 3), plain=True, expo=0.55)))
    part = spine_group(items, SLOT_LEGS if bone.startswith("leg") else SLOT_BODY, bone, key, pal, seg_n(6, q, 5), rows=3)
    nz = AG.fbm(part.V * 40.0, 2, seed=9)
    part.base = part.base * (0.8 + 0.4 * nz[:, None])
    part.mat[:, 1] = 1.0
    part.mat[:, 3] = 0.0
    return part


# ============================================================================= pinces (v2)

def claw_parts(P, Lay, pal, q, side, lod, seed):
    """Pince : mérus (bras) et carpe (avant-bras) en plaques épineuses, paume massive à carène dentelée et tubercules,
    doigt fixe et dactyle à dents irrégulières (molaires à droite, dents de scie à gauche, quelques-unes cassées),
    membranes plissées (épaule, coude = point faible, poignet, charnière du doigt)."""
    parts = []
    rng = np.random.default_rng(seed * 31 + (11 if side == "l" else 17))
    c = P.claw[side]
    crusher = side == "r"
    m = V2["claw_mass"]
    nseg = seg_n(20, q)
    names = {s: f"claw_{side}_{s}" for s in CLAW_SEGS}
    M = {s: np.array(Lay.M(names[s])) for s in CLAW_SEGS}
    tag = f"claw_{side}"
    cap_rows = 2 if lod < 2 else 1

    def nrow(L):
        return max(3, int(round(L * 23 * q))) + 1

    def outward_sign(Mb):
        # signe local x de la face extérieure (opposée au plan médian du crabe)
        return SGN[side] * (1.0 if Mb[0, 0] >= 0 else -1.0)

    def color(p, ox=0.0, seed_=0):
        nr = p.normals()
        return chitin_color(pal, p.V, nr[:, 2], ox, seed=seed + seed_)

    # ---------------------------------------------------------------- bras (mérus) et avant-bras (carpe)
    La, da = c["arm"]["length_m"], c["arm"]["diameter_m"]
    Lf_, df = c["forearm"]["length_m"], c["forearm"]["diameter_m"]

    def prof_arm(tt):
        bl = bump(tt, 0.55, 0.35)
        return (da * 0.98 * m * (0.92 + 0.16 * bl), da * 0.80 * m * (0.92 + 0.14 * bl), 0.0,
                0.02 * La * math.sin(math.pi * tt), 0.14, 0.06)

    def prof_fa(tt):
        bl = bump(tt, 0.5, 0.33)
        return df * 0.74 * m * (1 + 0.30 * bl), df * 0.68 * m * (1 + 0.28 * bl), 0.0, 0.0, 0.10, 0.05

    profs = {"arm": (0.0, La, prof_arm), "forearm": (0.0, Lf_, prof_fa)}
    for seg, (y0, y1, prof) in profs.items():
        V, F, UV, size = plate_segment(y0, y1, prof, nseg, nrow(y1 - y0), cap0=0.13 if seg == "arm" else 0.14,
                                       cap1=0.12, cap_rows=cap_rows, expo=2.5)
        zl = V[:, 2].copy()
        p = Part(V, F, UV, SLOT_BODY, names[seg], f"{tag}_{seg}", size)
        p.transform(M[seg])
        vent = smooth(0.1, -0.9, zl / np.maximum(np.abs(zl).max(), 1e-6))
        p.base = color(p, 0.15 + 0.35 * vent, 3)
        p.mask[:, 3] = rng.random()
        parts.append(p)

    # ---------------------------------------------------------------- paume (propode) + doigt fixe
    Lp, H, T = c["palm"]["length_m"], c["palm"]["height_m"], c["palm"]["thickness_m"]
    Lf = c["fixed_finger_m"]
    mh, mt = m * 0.96, m
    k_ = nrow(Lp) + 4
    ys = np.linspace(-0.04, Lp, k_)
    tpar = (ys + 0.04) / (Lp + 0.04)
    hz = H / 2 * (0.62 + 0.38 * np.sin(np.clip(tpar * 1.4, 0, 1) * math.pi / 2) ** 0.7) * (1 - 0.12 * smooth(0.75, 1.0, tpar)) * mh
    tx = T / 2 * (0.75 + 0.25 * np.sin(np.clip(tpar * 1.4, 0, 1) * math.pi / 2)) * mt
    zc = -0.04 * H * np.sin(tpar * math.pi)
    pns = nseg + 4
    rings = []
    for yy, s_ in ((-0.10, 0.35), (-0.075, 0.75)):
        x, z = section_pts(pns, 2 * hz[0] * s_, 2 * tx[0] * s_, 2.6, 0.07, 0.04)
        rings.append(np.stack([x, np.full(pns, yy), zc[0] + z], 1))
    for yy, h_, t_, z_ in zip(ys, hz, tx, zc):
        x, z = section_pts(pns, 2 * h_, 2 * t_, 2.6, 0.07, 0.04)
        rings.append(np.stack([x, np.full(pns, yy), z_ + z], 1))
    V, F, UV, size = ring_mesh(np.array(rings), (0, -0.115, 0), (0, Lp + 0.03, zc[-1]))
    xl, zl = V[:, 0].copy(), V[:, 2].copy()
    palm = Part(V, F, UV, SLOT_BODY, names["hand"], f"{tag}_palm", size)
    palm.transform(M["hand"])
    up = zl / max(np.abs(zl).max(), 1e-6)
    palm.base = color(palm, 0.10 + 0.40 * smooth(0.1, -0.9, up), 5)
    palm.mask[:, 3] = rng.random()
    parts.append(palm)
    # doigt fixe (pollex)
    fz0 = -0.20 * H
    n_f = max(6, nrow(Lf + 0.1))
    tt = np.linspace(0, 1, n_f)
    Cf = np.stack([np.zeros(n_f), Lp - 0.10 + tt * (Lf + 0.10),
                   fz0 + 0.16 * H * tt ** 1.6 + 0.05 * H * np.sin(tt * math.pi)], 1)
    hf = (0.30 * H * (1 - tt) ** 0.75 + 0.004) * mh ** 0.5
    tf = (0.55 * T / 2 * (1 - tt) ** 0.7 + 0.004) * mt ** 0.5
    fns = max(8, nseg - 4)
    rings = []
    for i in range(n_f - 1):
        x, z = section_pts(fns, 2 * hf[i], 2 * tf[i], 2.2, 0.10, 0.05)
        rings.append(np.stack([x, np.full(fns, Cf[i, 1]), Cf[i, 2] + z], 1))
    V, F, UV, size = ring_mesh(np.array(rings), Cf[0] - np.array([0, 0.05, 0]), Cf[-1] + np.array([0, 0.004, 0]))
    yl = V[:, 1].copy()
    pf = Part(V, F, UV, SLOT_BODY, names["hand"], f"{tag}_pollex", size)
    pf.transform(M["hand"])
    tk = np.clip((yl - (Lp - 0.10)) / (Lf + 0.10), 0, 1)
    col = color(pf, 0.30 + 0.3 * tk, 7)
    tipk = smooth(1 - 0.22 / (Lf + 0.1), 1.0, tk)
    pf.base = col * (1 - tipk[:, None]) + pal["tip"][None] * tipk[:, None]
    pf.mat[:, 3] = tipk
    parts.append(pf)

    def ff_edge(u):
        z = fz0 + 0.16 * H * u ** 1.6 + 0.05 * H * math.sin(u * math.pi)
        h = (0.30 * H * (1 - u) ** 0.75 + 0.004) * mh ** 0.5 * 1.10
        return Lp - 0.10 + u * (Lf + 0.10), z + h

    # ---------------------------------------------------------------- dactyle (doigt mobile), repère de son os
    Lm = c["movable_finger_m"]
    hinge = finger_hinge(P, side)
    target_z = (fz0 + 0.16 * H + 0.05 * H * math.sin(math.pi)) - hinge.z + 0.10 * H
    n_m = max(6, nrow(Lm))
    tt = np.linspace(0, 1, n_m)
    Cm = np.stack([np.zeros(n_m), tt * Lm, target_z * tt ** 1.5 + 0.06 * H * np.sin(tt * math.pi)], 1)
    hm = (0.27 * H * (1 - tt) ** 0.7 + 0.004) * mh ** 0.5
    tm = (0.52 * T / 2 * (1 - tt) ** 0.65 + 0.004) * mt ** 0.5
    rings = []
    for i in range(n_m - 1):
        x, z = section_pts(fns, 2 * hm[i], 2 * tm[i], 2.2, 0.06, 0.10)
        rings.append(np.stack([x, np.full(fns, Cm[i, 1]), Cm[i, 2] + z], 1))
    V, F, UV, size = ring_mesh(np.array(rings), Cm[0] - np.array([0, 0.05, 0]), Cm[-1] + np.array([0, 0.004, 0]))
    yl = V[:, 1].copy()
    pm = Part(V, F, UV, SLOT_BODY, names["finger"], f"{tag}_dactyl", size)
    pm.transform(M["finger"])
    tk = np.clip(yl / Lm, 0, 1)
    col = color(pm, 0.20 + 0.3 * tk, 9)
    tipk = smooth(1 - 0.22 / Lm, 1.0, tk)
    pm.base = col * (1 - tipk[:, None]) + pal["tip"][None] * tipk[:, None]
    pm.mat[:, 3] = tipk
    parts.append(pm)

    def mf_edge(u):
        z = target_z * u ** 1.5 + 0.06 * H * math.sin(u * math.pi)
        h = (0.27 * H * (1 - u) ** 0.7 + 0.004) * mh ** 0.5 * 1.10
        return u * Lm, z - h

    # ---------------------------------------------------------------- dents (irrégulières, certaines cassées)
    nt = 6 if crusher else 12
    broken = set(rng.choice(nt, 2 if crusher else 3, replace=False).tolist())
    teeth_spec = []
    for finger, edge in (("hand", ff_edge), ("finger", mf_edge)):
        sgn_z = 1.0 if finger == "hand" else -1.0
        for i in range(nt):
            u = 0.08 + 0.74 * i / max(nt - 1, 1) + rng.uniform(-0.025, 0.025)
            y, z = edge(min(max(u, 0.02), 0.9))
            size_k = rng.uniform(0.65, 1.35) * (1.25 if i == 0 else 1.0)
            teeth_spec.append((finger, i, y, z, sgn_z, size_k, i in broken, rng.uniform(-0.012, 0.012),
                               rng.integers(0, 1 << 30)))
    keep_n = {0: nt, 1: nt // 2}.get(lod, 0)
    for finger in ("hand", "finger"):
        items_b, items_c = [], []
        Mb = M[finger]
        for (fg, i, y, z, sz, size_k, brk, xo, sd) in teeth_spec:
            if fg != finger or (lod == 1 and i % 2 == 1) or lod >= 2:
                continue
            trng = np.random.default_rng(int(sd))
            if crusher:
                r = (0.040 * size_k, 0.050 * size_k, 0.036 * size_k)
                cen = np.array([xo, y, z - sz * r[2] * 0.35])
                V, F, UV, size = blob_mesh(cen, r, max(6, seg_n(8, q, 6)), max(5, seg_n(6, q, 4)))
                lump = AG.fbm((V - cen) * 22.0 + float(sd % 997), 2, seed=int(sd % 89))
                V = cen + (V - cen) * (0.82 + 0.36 * lump)[:, None]
                if brk:                                            # molaire cassée : sommet arasé, déchiqueté
                    lim = cen[2] + sz * r[2] * 0.25
                    jag = trng.uniform(-0.006, 0.006, len(V))
                    V[:, 2] = np.where(sz * (V[:, 2] - lim) > 0, lim + jag, V[:, 2])
                F, UV = oriented(V, F, UV)
                t_ = np.clip(sz * (V[:, 2] - (cen[2] - sz * r[2])) / (2 * r[2]), 0, 1)
                items_b.append((i, nt, V, F, UV, size, (t_, brk)))
            else:
                h = 0.045 * size_k
                base = np.array([xo, y, z - sz * 0.010])
                tip = base + np.array([0.0, 0.016, sz * h])
                opt = dict(flat=0.55, rng=trng)
                if brk:
                    opt.update(trunc=0.45, jag=0.30)
                items_c.append((i, nt, Mb[:3, :3] @ base + Mb[:3, 3], Mb[:3, :3] @ tip + Mb[:3, 3], 0.020 * size_k,
                                pal["bone"], dict(opt, tip0=-0.5, gloss=0.4)))
        if items_b:
            part, extras = merge_parts(items_b, SLOT_BODY, lambda V: {names[finger]: np.ones(len(V))},
                                       f"{tag}_teeth_{finger}")
            part.transform(Mb)
            tv = np.concatenate([e[0] for e in extras])
            brk = np.concatenate([np.full(len(e[0]), 1.0 if e[1] else 0.0) for e in extras])
            stain = AG.fbm(part.V * 25.0, 2, seed=13)
            cbone = pal.mix("bone_dark", "bone", smooth(0.25, 0.95, tv) * (0.55 + 0.45 * stain))
            part.base = cbone * (1 - 0.40 * brk[:, None] * smooth(0.6, 1.0, tv)[:, None])
            part.mat[:, 3] = 0.35
            parts.append(part)
        if items_c:
            sp = spine_group(items_c, SLOT_BODY, names[finger], f"{tag}_teeth_{finger}", pal, 5, rows=2)
            parts.append(sp)

    # ---------------------------------------------------------------- épines et tubercules
    def keel_pt(seg, tt, ang):
        y0, y1, prof = profs[seg]
        w, t, xc, zc, kt, kb = prof(tt)
        x, z, nl = section_point(ang, w, t, 2.5, kt, kb)
        return np.array([xc + x, y0 + tt * (y1 - y0), zc + z]), nl

    rows = []
    if lod <= 2:
        o_arm = outward_sign(M["arm"])
        o_fa = outward_sign(M["forearm"])
        rows.append(("arm", "sp_arm_d", [(tt, math.pi / 2, (0, 0.45, 0.89), (0.09, 0.14), 0.28)
                                         for tt in np.linspace(0.10, 0.90, 7)], {0: 7, 1: 4, 2: 2}))
        rows.append(("arm", "sp_arm_v", [(tt, -math.pi / 2, (0, 0.40, -0.92), (0.07, 0.11), 0.30)
                                         for tt in np.linspace(0.2, 0.8, 4)], {0: 4, 1: 2, 2: 0}))
        rows.append(("arm", "sp_arm_s", [(tt, (0.0 if o_arm > 0 else math.pi) + 0.4, (o_arm * 0.9, 0.4, 0.2),
                                          (0.035, 0.055), 0.38) for tt in np.linspace(0.2, 0.8, 5)], {0: 5, 1: 0, 2: 0}))
        inner_a = math.pi if o_fa > 0 else 0.0                     # face intérieure de l'avant-bras
        rows.append(("forearm", "sp_fa_in", [(tt, inner_a + da_, (-o_fa * 0.85, 0.45, 0.25), (0.15, 0.21), 0.24)
                                             for tt, da_ in ((0.30, 0.45), (0.52, 0.20), (0.72, -0.05))],
                     {0: 3, 1: 2, 2: 1}))
        rows.append(("forearm", "sp_fa_d", [(tt, math.pi / 2 + 0.2, (0, 0.30, 0.95), (0.06, 0.09), 0.30)
                                            for tt in (0.2, 0.38, 0.56, 0.74)], {0: 4, 1: 2, 2: 0}))
    snseg = seg_n(7, q, 5) if lod == 0 else 5
    for seg, key, spec, keep in rows:
        items = []
        K = len(spec)
        for i, (tt, ang, d0, Lr, rr) in enumerate(spec):
            L = rng.uniform(*Lr)
            d = np.array(d0, float) + rng.normal(0, 0.08, 3)
            d /= np.linalg.norm(d)
            if i >= keep.get(lod, 0):
                continue
            p, _ = keel_pt(seg, tt, ang)
            Mb = M[seg]
            bw = Mb[:3, :3] @ (p - d * L * 0.28) + Mb[:3, 3]
            tw = Mb[:3, :3] @ (p + d * L * 0.72) + Mb[:3, 3]
            col = chitin_color(pal, np.array([bw]), np.array([0.2]), 0.3, seed=seed)[0]
            items.append((i, K, bw, tw, rr * L, col, dict(bend=Mb[:3, :3] @ np.array([0, 0.3, 0]) * L * 0.25)))
        sp = spine_group(items, SLOT_BODY, names[seg], f"{tag}_{key}", pal, snseg)
        if sp is not None:
            parts.append(sp)
    # paume : carène dorsale dentelée (6 épines), tubercules sur la face extérieure, épines du dactyle
    Mh = M["hand"]
    oh = outward_sign(Mh)
    if lod <= 1:
        items = []
        for i, yy in enumerate(np.linspace(0.08, 0.92, 8) * Lp):
            L = rng.uniform(0.065, 0.11)
            d = np.array([oh * 0.25, 0.42, 0.87]) + rng.normal(0, 0.08, 3)
            d /= np.linalg.norm(d)
            if lod == 1 and i % 2 == 1:
                continue
            j = int(np.argmin(np.abs(ys - yy)))
            top = np.array([0.0, yy, zc[j] + hz[j] * 1.07])
            bw = Mh[:3, :3] @ (top - d * L * 0.3) + Mh[:3, 3]
            tw = Mh[:3, :3] @ (top + d * L * 0.7) + Mh[:3, 3]
            col = chitin_color(pal, np.array([bw]), np.array([0.5]), 0.2, seed=seed)[0]
            items.append((i, 8, bw, tw, 0.30 * L, col, dict()))
        parts.append(spine_group(items, SLOT_BODY, names["hand"], f"{tag}_sp_palm", pal, snseg))
        items = []
        ntub = 24
        for i in range(ntub):
            yy = rng.uniform(0.06, 0.86) * Lp
            zz = rng.uniform(-0.62, 0.62)
            L = rng.uniform(0.028, 0.052)
            jit = rng.normal(0, 0.15, 3)
            if lod == 1:
                continue
            j = int(np.argmin(np.abs(ys - yy)))
            ang = (0.0 if oh > 0 else math.pi) + zz * 1.1
            x, z, nl = section_point(ang, 2 * hz[j], 2 * tx[j], 2.6, 0.07, 0.04)
            pl = np.array([x, yy, zc[j] + z])
            d = nl + jit + np.array([0, 0.2, 0])
            d /= np.linalg.norm(d)
            bw = Mh[:3, :3] @ (pl - d * L * 0.3) + Mh[:3, 3]
            tw = Mh[:3, :3] @ (pl + d * L * 0.7) + Mh[:3, 3]
            col = chitin_color(pal, np.array([bw]), np.array([0.3]), 0.1, seed=seed)[0]
            items.append((i, ntub, bw, tw, 0.42 * L, col, dict(expo=1.3)))
        tub = spine_group(items, SLOT_BODY, names["hand"], f"{tag}_tubercles", pal, 5, rows=2)
        if tub is not None:
            parts.append(tub)
        items = []
        Mf = M["finger"]
        for i, u in enumerate((0.14, 0.28, 0.42, 0.56, 0.70)):
            L = rng.uniform(0.04, 0.065)
            if lod == 1:
                continue
            z = target_z * u ** 1.5 + 0.06 * H * math.sin(u * math.pi) + (0.27 * H * (1 - u) ** 0.7) * mh ** 0.5
            pl = np.array([0.0, u * Lm, z])
            d = np.array([0.0, 0.5, 0.86])
            bw = Mf[:3, :3] @ (pl - d * L * 0.3) + Mf[:3, 3]
            tw = Mf[:3, :3] @ (pl + d * L * 0.7) + Mf[:3, 3]
            items.append((i, 5, bw, tw, 0.32 * L, pal["black"], dict()))
        fsp = spine_group(items, SLOT_BODY, names["finger"], f"{tag}_sp_finger", pal, 5)
        if fsp is not None:
            parts.append(fsp)
    # ---------------------------------------------------------------- membranes
    if lod <= 2:
        mseg, mrow = max(8, nseg - 4), (6 if lod == 0 else 4)
        mems = [("body", "arm", "arm", -0.12, 0.0, da * 0.66 * m, da * 0.60 * m, "mem_shoulder"),
                ("arm", "forearm", "forearm", -0.09, 0.02, df * 0.62 * m, df * 0.58 * m, "mem_elbow"),
                ("forearm", "hand", "hand", -0.14, -0.06, df * 0.55 * m, df * 0.50 * m, "mem_wrist"),
                ("hand", "finger", "finger", -0.06, 0.03, H * 0.22, T * 0.30, "mem_finger")]
        for a, b, fr, ya, yb, w, t, key in mems:
            if lod == 2 and key != "mem_elbow":
                continue
            ba = "body" if a == "body" else names[a]
            parts.append(membrane_part(M[fr], ya, yb, w, t, mseg, mrow, ba, names[b], f"{tag}_{key}", pal,
                                       slot=SLOT_BODY, seed=seed + 3))
    return [p for p in parts if p is not None]


# ============================================================================= tête, bouche, abdomen (v2)

def head_parts(P, Lay, pal, q, lod, seed):
    parts = []
    rng = np.random.default_rng(seed * 7 + 5)
    nseg = seg_n(12, q)
    for s in SIDES:
        g = SGN[s]
        # pédoncule oculaire : deux anneaux, épine sus-oculaire
        Ms = np.array(Lay.M(f"eye_stalk_{s}"))
        d = P.stalk_d
        L = P.stalk_len

        def prof_st(tt):
            ring = 1.0 - 0.10 * bump(tt, 0.38, 0.05) - 0.10 * bump(tt, 0.70, 0.05)
            return d * 1.55 * (1 - 0.25 * tt) * ring, d * 1.45 * (1 - 0.25 * tt) * ring, 0.0, 0.0, 0.05, 0.0
        V, F, UV, size = plate_segment(0.0, L, prof_st, nseg, max(4, int(14 * q)), cap0=0.05, cap1=0.025)
        p = Part(V, F, UV, SLOT_BODY, f"eye_stalk_{s}", f"stalk_{s}", size)
        p.transform(Ms)
        p.base = chitin_color(pal, p.V, p.normals()[:, 2], 0.35, seed=seed)
        parts.append(p)
        if lod <= 1:
            base = Ms[:3, :3] @ np.array([0.0, 0.80 * L, d * 0.62]) + Ms[:3, 3]
            dd = Ms[:3, :3] @ np.array([0.0, 0.55, 0.84])
            parts.append(spine_group([(0, 1, base - dd * 0.012, base + dd * 0.07, 0.016,
                                       chitin_color(pal, np.array([base]), np.array([0.5]), 0.3)[0], dict())],
                                     SLOT_BODY, f"eye_stalk_{s}", f"stalk_spine_{s}", pal, 5))
        # œil composé (matériau émissif à part) et voile (forme DeadEyes)
        Me = np.array(Lay.M(f"eye_{s}"))
        r = P.eye_d / 2 * 1.12
        V, F, UV, size = blob_mesh((0, r * 0.55, 0), (r, r * 1.1, r), seg_n(16, q, 8), max(6, seg_n(10, q, 6)))
        eye = Part(V, F, UV, SLOT_EYE, f"eye_{s}", f"eye_{s}", size, kind="eye")
        eye.transform(Me)
        nrm = eye.normals()
        look = np.array([0.30 * g, -0.92, 0.25])
        look /= np.linalg.norm(look)
        dotl = nrm @ look
        axis = Me[:3, 1] / np.linalg.norm(Me[:3, 1])
        rim = smooth(0.15, 0.55, -(nrm @ axis))                  # base de l'œil (côté pédoncule)
        eye.base = pal.mix("eye_amber", "eye_dark", rim)
        eye.mat[:, 0] = smooth(math.cos(math.radians(24)), math.cos(math.radians(12)), dotl)   # pseudo-pupille
        eye.mat[:, 1] = rim
        parts.append(eye)
        V, F, UV, size = blob_mesh((0, r * 0.62, 0), (r * 1.06, r * 1.13, r * 1.06), seg_n(16, q, 8),
                                   max(6, seg_n(10, q, 6)))
        veil = Part(V, F, UV, SLOT_BODY, f"eye_{s}", f"veil_{s}", size, kind="veil")
        veil.transform(Me)
        veil.base = np.tile(pal["veil"], (len(veil.V), 1))
        veil.mat[:, 3] = 0.2
        parts.append(veil)
        # antennes (3 os) et antennules
        rad = P.ant_d / 2
        for i in (range(3) if lod < 3 else ()):
            name = f"antenna_{s}_{i + 1:02d}"
            La = Lay.bones[name]["length"]
            r0 = rad * (1 - 0.3 * i)
            r1 = rad * (1 - 0.3 * (i + 1)) + 0.002

            def prof_a(tt, r0=r0, r1=r1):
                rr = (r0 + (r1 - r0) * tt) * (1 + 0.12 * max(0.0, math.sin(tt * TAU * 4)))
                return 2 * rr, 2 * rr, 0.0, 0.0, 0.0, 0.0
            V, F, UV, size = plate_segment(0.0, La, prof_a, max(6, nseg // 2), max(4, int(16 * q)), cap0=0.012,
                                           cap1=0.008, expo=2.0)
            yl = V[:, 1].copy()
            p = Part(V, F, UV, SLOT_BODY, name, name, size)
            p.transform(np.array(Lay.M(name)))
            ann = 0.5 + 0.5 * np.sin(yl / La * TAU * 4)
            p.base = pal.mix("black", "oxblood", 0.25 + 0.5 * ann)
            parts.append(p)
        if lod < 2:
            name = f"antennule_{s}"
            Ma = np.array(Lay.M(name))
            V, F, UV, size = plate_segment(0.0, 0.25, lambda tt: (0.035 - 0.023 * tt, 0.035 - 0.023 * tt, 0, 0, 0, 0),
                                           max(5, nseg // 2), 5, cap0=0.015, cap1=0.008, expo=2.0)
            p = Part(V, F, UV, SLOT_BODY, name, name, size)
            p.transform(Ma)
            p.base = pal.mix("black", "oxblood", np.full(len(p.V), 0.5))
            parts.append(p)
    # pièces buccales : maxillipèdes (os mouth : Y vers le bas, Z vers l'avant), franges de soies, mandibules dentées
    Mm = np.array(Lay.M("mouth"))
    W, Dp, Hh = P.mouth_size
    nr = max(5, int(12 * q))
    for kx in (1, -1):
        ys = np.linspace(0.02, Hh, nr)
        t = (ys - 0.02) / (Hh - 0.02)
        Cp = np.stack([kx * (W / 4 - 0.012) * (1 - 0.25 * t), ys, -0.03 + 0.015 * np.sin(t * math.pi)], 1)
        hw = (W / 4 - 0.006) * (1.0 - 0.45 * t ** 1.6)
        step = 0.012 * smooth(0.40, 0.46, t)
        th = Dp * 0.26 + step - 0.008 * smooth(0.85, 1.0, t)
        pns = max(8, int(14 * q))
        rings = []
        for i in range(nr):
            x, z = section_pts(pns, 2 * th[i], 2 * hw[i], 3.5)
            rings.append(np.stack([Cp[i, 0] + x, np.full(pns, Cp[i, 1]), Cp[i, 2] + z], 1))
        V, F, UV, size = ring_mesh(np.array(rings), Cp[0] - np.array([0, 0.015, 0]), Cp[-1] + np.array([0, 0.015, 0]))
        yl = V[:, 1].copy()
        p = Part(V, F, UV, SLOT_BODY, "mouth", f"maxilliped_{'l' if kx > 0 else 'r'}", size)
        p.transform(Mm)
        tt = np.clip(yl / Hh, 0, 1)
        p.base = pal.mix("oxblood_dark", "black", 0.3 + 0.5 * tt) * (1 - 0.4 * (0.5 + 0.5 * np.sin(tt * 18))[:, None] * 0.5)
        p.mat[:, 3] = 0.5
        parts.append(p)
        if lod == 0:                                       # frange de soies sur le bord intérieur et le bas
            built = []
            K = 14
            for b in range(K):
                u = b / (K - 1)
                y = 0.05 + u * (Hh - 0.06)
                x0 = kx * 0.006
                base = np.array([x0, y, -0.03 + 0.015 * math.sin(u * math.pi) + 0.012])
                d = np.array([-kx * 0.25, 0.55 + 0.4 * u, 0.55]) + rng.normal(0, 0.15, 3)
                d /= np.linalg.norm(d)
                Lb = rng.uniform(0.04, 0.07)
                bw = Mm[:3, :3] @ base + Mm[:3, 3]
                tw = Mm[:3, :3] @ (base + d * Lb) + Mm[:3, 3]
                V, F, UV, size = bristle(bw, tw, 0.003, Mm[:3, :3] @ np.array([1.0, 0, 0]))
                built.append((b, K, V, F, UV, size, None))
            part, _ = merge_parts(built, SLOT_BODY, lambda V: {"mouth": np.ones(len(V))},
                                  f"mouth_setae_{'l' if kx > 0 else 'r'}")
            part.base = pal.mix("setae", "bone_dark", np.tile([0, 0, 0, 0.8], K))
            parts.append(part)
    for s in SIDES:
        name = f"mandible_{s}"
        Md = np.array(Lay.M(name))
        Ld = Lay.bones[name]["length"]
        V, F, UV, size = plate_segment(0.0, Ld, lambda tt: (0.075 - 0.04 * tt, 0.050 - 0.028 * tt, 0, 0, 0.12, 0.05),
                                       max(6, nseg // 2), max(4, int(8 * q)), cap0=0.03, cap1=0.015, expo=2.6)
        p = Part(V, F, UV, SLOT_BODY, name, name, size)
        p.transform(Md)
        p.base = pal.mix("black", "oxblood_dark", np.full(len(p.V), 0.4))
        p.mat[:, 3] = 0.6
        parts.append(p)
        if lod <= 1:
            items = []
            for i, u in enumerate((0.35, 0.55, 0.72, 0.88)):
                base = np.array([0.0, u * Ld, -(0.075 - 0.04 * u) / 2 * 0.9])
                dd = np.array([0.0, 0.35, -0.94])
                bw = Md[:3, :3] @ base + Md[:3, 3]
                tw = Md[:3, :3] @ (base + dd * (0.035 - 0.012 * u)) + Md[:3, 3]
                items.append((i, 4, bw, tw, 0.012, pal["bone_dark"], dict(gloss=0.6)))
            parts.append(spine_group(items, SLOT_BODY, name, f"mandible_teeth_{s}", pal, 5, rows=2, tip0=0.2))
    # abdomen replié sous le ventre (v1) et membrane du pourtour
    Ma = np.array(Lay.M("abdomen"))
    Wd, Ln, Th = P.abd_size
    nr = max(4, int(26 * q))
    ys = np.linspace(0, Ln, nr)
    t = ys / Ln
    half_w = Wd / 2 * (1 - 0.62 * t ** 1.2)
    nsg = 6
    ph = (t * nsg) % 1.0
    groove = np.exp(-((np.minimum(ph, 1 - ph)) / 0.07) ** 2) if q >= 0.5 else np.zeros(nr)
    pns = max(10, nseg + 4)
    rings = []
    for i in range(nr):
        x, z = section_pts(pns, 2 * (Th / 2 - 0.018 * groove[i]), 2 * half_w[i] * (1 - 0.06 * groove[i]), 3.2)
        rings.append(np.stack([x, np.full(pns, ys[i]), -Th / 2 - 0.01 * math.sin(t[i] * math.pi) + z], 1))
    V, F, UV, size = ring_mesh(np.array(rings), None, None)
    F_, UV_ = list(F), list(UV)
    # bouchons plats aux extrémités
    rings = np.array(rings)
    V = np.vstack([V, rings[0].mean(0), rings[-1].mean(0)])
    p0, p1 = len(V) - 2, len(V) - 1
    for j in range(pns):
        u0, u1 = j / pns, (j + 1) / pns
        F_.append((p0, (j + 1) % pns, j))
        UV_.append((((u0 + u1) / 2, 0.0), (u1, 0.0), (u0, 0.0)))
        F_.append((p1, (nr - 1) * pns + j, (nr - 1) * pns + (j + 1) % pns))
        UV_.append((((u0 + u1) / 2, 1.0), (u0, 1.0), (u1, 1.0)))
    yl = V[:, 1].copy()
    xl = V[:, 0].copy()
    p = Part(V, F_, UV_, SLOT_BODY, "abdomen", "abdomen", (size[0] * 0.6, size[1] * 0.6))
    p.transform(Ma)
    tv = np.clip(yl / Ln, 0, 1)
    gv = np.exp(-((np.minimum((tv * nsg) % 1, 1 - (tv * nsg) % 1)) / 0.08) ** 2)
    stain = smooth(0.5, 0.75, AG.fbm(p.V * 3.0, 3, seed=seed + 77))
    edge = np.abs(xl) / np.maximum(Wd / 2 * (1 - 0.62 * tv ** 1.2), 1e-6)
    col = pal.mix("belly", "belly_stain", np.clip(stain * 0.8 + gv * 0.5 + smooth(0.7, 1.0, edge) * 0.6, 0, 1))
    p.base = col
    p.mask[:, 0] = gv * 0.8
    parts.append(p)
    rings2 = []
    for i in range(nr):
        x, z = section_pts(max(8, nseg), 2 * Th / 2 * 0.6, 2 * (half_w[i] + 0.025), 4.0)
        rings2.append(np.stack([x, np.full(len(x), ys[i]), -Th / 2 - 0.01 * math.sin(t[i] * math.pi) + 0.012 + z], 1))
    V, F, UV, size = ring_mesh(np.array(rings2), np.array(rings2)[0].mean(0), np.array(rings2)[-1].mean(0))
    w_ = np.full(len(V), 0.5)
    mem = Part(V, F, UV, SLOT_BODY, {"abdomen": w_, "body": 1 - w_}, "abdomen_membrane", (size[0] * 0.35, size[1] * 0.35))
    mem.transform(Ma)
    mem.base = pal.mix("membrane", "membrane_dark", np.full(len(mem.V), 0.5))
    mem.mat[:, 0] = 1.0
    parts.append(mem)
    return parts


def carapace_reef(P, pal, q, lod, cara, seed):
    """Balanes en grappes et petits coraux sur le bord postérieur de la carapace (LOD0 : 18 + 3, LOD1 : 9 + 1)."""
    if lod >= 2:
        return []
    rng = np.random.default_rng(seed + 503)
    bvh = surface_bvh(cara)
    clusters = [(-0.55, 0.62), (0.15, 0.86), (0.70, 0.52), (-0.95, 0.20)]
    spots = []
    for ci, (cx, cy) in enumerate(clusters):
        for k in range(5 if ci < 2 else 4):
            x = cx + rng.normal(0, 0.10)
            y = cy + rng.normal(0, 0.08)
            r = rng.uniform(0.022, 0.045)
            spots.append((x, y, r))
    built = []
    K = len(spots)
    for i, (x, y, r) in enumerate(spots):
        if lod == 1 and i % 2 == 1:
            continue
        hit = bvh.ray_cast(Vector((x, y, 3.0)), Vector((0, 0, -1)), 4.0)
        if hit[0] is None:
            continue
        V, F, UV, size = barnacle_mesh(np.array(hit[0]) - np.array(hit[1]) * 0.004, np.array(hit[1]), r,
                                       max(6, seg_n(9, q, 6)), np.random.default_rng(seed + i))
        built.append((i, K, V, F, UV, size, "body"))
    parts = []
    if built:
        part, _ = merge_parts([b[:6] + (b[6],) for b in built], SLOT_BODY, lambda V: {"body": np.ones(len(V))},
                              "cara_barnacles")
        part.base, part.mat = barnacle_colors(pal, part.V, built)
        parts.append(part)
    for j, (cx, cy) in enumerate(((-0.35, 0.80), (0.45, 0.70), (-0.80, 0.42))):
        if lod == 1 and j > 0:
            continue
        hit = bvh.ray_cast(Vector((cx, cy, 3.0)), Vector((0, 0, -1)), 4.0)
        if hit[0] is None:
            continue
        parts.append(coral_part(np.array(hit[0]), np.array(hit[1]), 0.12, "body", f"cara_coral_{j}", pal,
                                np.random.default_rng(seed + 900 + j), q))
    return parts


# ============================================================================= maillage complet d'un LOD

def build_meshes(P, Lay, pal, lod=0, seed=None):
    seed = V2["seed"] if seed is None else seed
    q = V2["lod_q"][lod]
    parts = []
    cara, cdata = carapace(P, pal, q, seed=7)
    parts.append(cara)
    parts += carapace_spines(P, pal, q, lod, cara, cdata, seed)
    parts += carapace_reef(P, pal, q, lod, cara, seed)
    for s in SIDES:
        for n in PAIRS:
            parts += leg_parts(P, Lay, pal, q, s, n, lod, seed)
        parts += claw_parts(P, Lay, pal, q, s, lod, seed)
    parts += head_parts(P, Lay, pal, q, lod, seed)
    return [p for p in parts if p is not None]


# ============================================================================= atlas UV et assemblage

class Atlas:
    """Rectangles d'un atlas carré (rangées, plus grand côté à l'horizontale) ; densité de texels uniforme : on
    cherche par dichotomie la plus grande échelle (UV par mètre) qui fait tout tenir avec la marge de cuisson."""

    def __init__(self, px, pad_px):
        self.px = px
        self.pad = pad_px / px
        self.rects = {}
        self.scale = None

    def _shelf(self, sizes, s):
        items = sorted(sizes, key=lambda it: (-min(it[1], it[2]), -max(it[1], it[2])))
        x = y = shelf = 0.0
        rects = {}
        pad = self.pad
        for key, w, h in items:
            rot = h > w
            W, H = (h * s, w * s) if rot else (w * s, h * s)
            if W + 2 * pad > 1.0:
                return None
            if x + W + 2 * pad > 1.0:
                y += shelf
                x = shelf = 0.0
            if y + H + 2 * pad > 1.0:
                return None
            rects[key] = (x + pad, y + pad, W, H, rot)
            x += W + 2 * pad
            shelf = max(shelf, H + 2 * pad)
        return rects

    def pack(self, sizes):
        lo, hi = 0.005, 8.0
        best = None
        for _ in range(48):
            mid = (lo + hi) / 2
            r = self._shelf(sizes, mid)
            if r is None:
                hi = mid
            else:
                lo, best = mid, (mid, r)
        self.scale, self.rects = best
        used = sum(r[2] * r[3] for r in self.rects.values())
        return dict(px_per_m=round(self.scale * self.px, 1), rects=len(self.rects), fill=round(used, 3))

    def uv(self, key, u, v):
        x, y, W, H, rot = self.rects[key]
        if rot:
            return x + v * W, y + u * H
        return x + u * W, y + v * H


def atlas_layout(parts):
    """Atlas de chaque emplacement à partir des pièces du LOD0 (clé -> taille physique)."""
    sizes = {SLOT_BODY: {}, SLOT_LEGS: {}, SLOT_EYE: {}}
    for p in parts:
        for key, size in p.keys:
            old = sizes[p.slot].get(key)
            w, h = size
            sizes[p.slot][key] = (max(w, old[0]), max(h, old[1])) if old else (w, h)
    out, report = {}, {}
    for slot, d in sizes.items():
        px = V2["eye_px"] if slot == SLOT_EYE else V2["atlas_px"]
        a = Atlas(px, V2["pad_px"] * (0.5 if slot == SLOT_EYE else 1.0))
        report[MAT_SLOTS[slot]] = a.pack([(k, w, h) for k, (w, h) in d.items()])
        out[slot] = a
    return out, report


def assemble(name, parts, coll, mats, bones, atlases):
    """Pièces -> objet maillage : emplacements, UV de l'atlas, attributs (CrabBase, CrabMask, CrabMat, EyeVeil),
    cavité et usure géométriques, groupes de sommets (un par os déformant)."""
    Vs, Fs, FMs, loops_uv = [], [], [], []
    bases, masks, matsv, veil = [], [], [], []
    W = {b: [] for b in bones}
    off = 0
    for p in parts:
        n = len(p.V)
        Vs.append(p.V)
        atlas = atlases[p.slot]
        for fi, f in enumerate(p.F):
            Fs.append(tuple(i + off for i in f))
            key = p.keys[p.FK[fi]][0]
            if key not in atlas.rects:
                raise KeyError(f"clé d'atlas absente du LOD0 : {key}")
            for (u, v) in p.UV[fi]:
                loops_uv.append(atlas.uv(key, u, v))
        FMs += [p.slot] * len(p.F)
        bases.append(p.base)
        masks.append(p.mask)
        matsv.append(p.mat)
        veil.append(np.full(n, 1.0 if p.kind == "veil" else 0.0))
        for b in bones:
            W[b].append(p.W.get(b, np.zeros(n)))
        unknown = set(p.W) - set(bones)
        if unknown:
            raise KeyError(f"os inconnus {unknown}")
        off += n
    V = np.vstack(Vs)
    me = bpy.data.meshes.new(name)
    me.from_pydata(V.tolist(), [], Fs)
    me.update(calc_edges=True)
    for m in mats:
        me.materials.append(m)
    me.polygons.foreach_set("material_index", np.array(FMs, np.int32))
    me.polygons.foreach_set("use_smooth", np.ones(len(Fs), bool))
    uvl = me.uv_layers.new(name="UVMap")
    uvl.uv.foreach_set("vector", np.array(loops_uv, np.float32).ravel())
    base = np.vstack(bases)
    mask = np.vstack(masks)
    matv = np.vstack(matsv)
    nrm = AG.vertex_normals(me).astype(np.float64)
    ed = AG.edge_array(me)
    cav = AG.cavity(V, nrm, ed, 3)
    lo_, hi_ = np.quantile(cav, 0.08), np.quantile(cav, 0.92)
    mask[:, 0] = np.maximum(mask[:, 0], np.clip(cav / max(hi_, 1e-6), 0, 1))
    mask[:, 1] = np.maximum(mask[:, 1], np.clip(cav / min(lo_, -1e-6), 0, 1) * 0.9)
    for nm, arr in (("CrabBase", np.hstack([base, np.ones((len(base), 1))])), ("CrabMask", mask), ("CrabMat", matv)):
        a = me.color_attributes.new(nm, "FLOAT_COLOR", "POINT")
        a.data.foreach_set("color", arr.astype(np.float32).ravel())
    me.color_attributes.active_color = me.color_attributes["CrabBase"]
    vv = me.attributes.new("EyeVeil", "FLOAT", "POINT")
    vv.data.foreach_set("value", np.concatenate(veil).astype(np.float32))
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    for b in bones:
        w = np.concatenate(W[b])
        g = ob.vertex_groups.new(name=b)
        idx = np.nonzero(w > 1e-4)[0]
        if len(idx) == 0:
            continue
        vals = np.round(w[idx], 3)
        for val in np.unique(vals):
            sel = idx[vals == val]
            g.add(sel.tolist(), float(val), "REPLACE")
    return ob


def dead_eye_shape(ob):
    """Forme DeadEyes (convention des mobs) : le voile (EyeVeil = 1) est escamoté dans l'œil en Basis (réduit de
    moitié autour de son centre) et le recouvre à 1."""
    me = ob.data
    veil = np.zeros(len(me.vertices), np.float32)
    me.attributes["EyeVeil"].data.foreach_get("value", veil)
    idx = np.nonzero(veil > 0.5)[0]
    co = np.zeros(len(me.vertices) * 3, np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    full = co.copy()
    shrunk = co.copy()
    for sgn in (1, -1):
        sel = idx[(co[idx, 0] * sgn) > 0]
        if len(sel):
            c = co[sel].mean(0)
            shrunk[sel] = c + (co[sel] - c) * 0.5
    me.vertices.foreach_set("co", shrunk.ravel())
    ob.shape_key_add(name="Basis", from_mix=False)
    sk = ob.shape_key_add(name="DeadEyes", from_mix=False)
    sk.data.foreach_set("co", full.ravel())
    return sk
