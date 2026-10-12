"""
sails — voiles du galion : 4 voiles carrées (misaine, grand-voile, petit et grand hunier), civadière sous le
beaupré, voile latine d'artimon. Chaque voile est une surface paramétrée S(u, v) (grille NU x NV >= 16 x 12)
gonflée vers l'avant (ventre), bordure en arc, points d'écoute tirés vers les bouts de vergue, double face
EXPLICITE (2 nappes à THICK, normales opposées : pas de solidify), ralingues, rabans, cosses, bande de ris,
pièces rapiécées (sails_parts). Implantation : masts_yards.yard_frame (hullform.YARDS / BOWSPRIT / mast_point),
hauteurs des basses voiles dérivées de hullform.rail_top_z, point d'écoute de la latine de hullform.FITTINGS.

Objet : Galleon_Sails (un maillage multi-matériaux : canvas, linen (pièces), rope, rope_tar).
"""
import math
import random

import bmesh
import bpy
from mathutils import Vector

from galleon import hullform as H
from galleon import materials as GM
from galleon import masts_yards as MY
from galleon import sails_parts as SP

Batch = GM.Batch
NU, NV = 16, 12                    # grille de chaque voile
THICK = 0.02                       # épaisseur de la toile (écart des 2 nappes)
UP = Vector((0, 0, 1))
FWD = Vector((0, -1, 0))
PI = math.pi

# Dimensions propres aux voiles (m). top / bot = retrait de la demi-tête / demi-bordure par rapport au bout de
# vergue (bot < 0 : bordure plus large que la tête = hunier trapèze inversé). belly = ventre, arc = relevé de la
# bordure au milieu, clear = garde entre le milieu de la bordure et la lisse de pavois (+ jambettes).
SPECS = {  # belly = fraction de la hauteur (0,4-0,55) ; basses voiles : bordure légèrement > tête
    # bonnet = rangée de la grille (sur NV) où la bonnette est lacée sous la basse voile (pas de bandes de ris avant
    # 1660 : voir _research/sails.md) ; ligne de laçage + couture au quart inférieur de la voile
    "fore_lower": dict(top=0.40, bot=0.10, belly=0.50, arc=1.0, clear=0.35, bonnet=9),
    "main_lower": dict(top=0.40, bot=0.05, belly=0.45, arc=1.0, clear=0.80, bonnet=9),
    # huniers : coins de bordure à foot_gap AU-DESSUS de la hune (z_top) ; le milieu remonte de `arc` (au-dessus du
    # garde-corps de la hune, 0,52) ; ainsi les étais bas capelés sous la hune (rigging_stays) et les balancines
    # passent sous la voile. Bordure ≈ 0,72 de la basse vergue (bot < 0 : trapèze inversé, planche ≈ 0,75)
    "fore_top": dict(top=0.30, bot=-1.0, belly=0.42, arc=0.60, foot_gap=0.25),
    "main_top": dict(top=0.30, bot=-1.4, belly=0.42, arc=0.65, foot_gap=0.25),
    "sprit": dict(top=0.30, bot=0.50, belly=0.42, arc=0.40, h=2.6),
}
# luff_fade : le ventre de la latine est nul sur l'antenne et plein à cette distance (m) du guindant ; sans cela la
# toile bombait de 0,35 m à 0,4 m derrière l'antenne et traversait les haubans d'artimon (bâbord)
LATEEN = dict(tack=0.55, peak=0.45, belly=1.2, flag_gap=1.2, clew_up=0.55, luff_fade=1.6, shroud_gap=0.08)


def lerp(a, b, t):
    return a + (b - a) * t


def _mizzen_shroud(side):
    """(capelage, cap de mouton le plus intérieur, (y0, y1) emprise, side) des bas-haubans d'artimon du bord `side`
    (rigging_util.hounds_point / hullform.deadeye_positions) ; None si rigging_util est indisponible."""
    try:
        from galleon import rigging_util as RU
        n = RU.N_SHROUDS["mizzen"]
        hp = RU.hounds_point("mizzen", 0, n, side)
        lows = H.deadeye_positions("mizzen", side)
        lo = min(lows, key=lambda p: abs(p.x))
        ys = [p.y for p in lows] + [RU.hounds_point("mizzen", k, n, side).y for k in range(n)]
        return hp, lo, (min(ys) - 0.10, max(ys) + 0.45), side
    except Exception as exc:               # noqa: BLE001
        print("sails: haubans d'artimon inconnus (pas de borne) :", exc)
        return None


class Sail:
    """Surface paramétrée : S(u, v) (u, v dans [0, 1]), N(u, v) normale orientée vers le ventre, uv(u, v) en m,
    head_points() = points lacés à la vergue, yard = (centre, direction, rayon, longueur) de la vergue.
    bonnet = rangée de la grille où une bonnette est lacée (None : aucune)."""
    bonnet = None

    def N(self, u, v):
        e = 0.01
        du = self.S(min(1.0, u + e), v) - self.S(max(0.0, u - e), v)
        dv = self.S(u, min(1.0 - e, v + e)) - self.S(u, max(0.0, v - e))
        n = du.cross(dv)
        if n.length < 1e-9:
            return self.bulge_dir.copy()
        n.normalize()
        return n if n.dot(self.bulge_dir) >= 0 else -n

    def _flip(self):
        """True si l'ordre (u, v) de la grille donne une normale opposée au ventre (sens des faces)."""
        du = self.S(0.51, 0.5) - self.S(0.49, 0.5)
        dv = self.S(0.5, 0.51) - self.S(0.5, 0.49)
        return du.cross(dv).dot(self.bulge_dir) < 0

    def corners(self):
        """Points d'écoute (clews) : coins bas de la voile."""
        return [(0.0, 1.0), (1.0, 1.0)]


class SquareSail(Sail):
    def __init__(self, name, c0, d, half_top, half_bot, h, belly, arc, bonnet, yard):
        self.name, self.c0, self.d, self.yard = name, c0, d, yard
        self.top, self.bot, self.h, self.belly, self.arc, self.bonnet = half_top, half_bot, h, belly, arc, bonnet
        self.bulge_dir = FWD
        self.flip = self._flip()

    def S(self, u, v, flat=False):
        s = 2 * u - 1
        hw = lerp(self.top, self.bot, v) * (1 - 0.05 * math.sin(PI * v))
        z = -v * self.h + self.arc * v * v * (1 - abs(s) ** 4)
        p = self.c0 + self.d * (s * hw) + UP * z
        if flat:
            return p
        bul = math.sin(PI * v ** 1.15) ** 1.2 * (0.15 + 0.85 * math.sin(PI * u) ** 0.8) + 0.35 * v * v * math.sin(PI * u) ** 1.5
        return p + FWD * (self.belly * bul)

    def uv(self, u, v):
        p = self.S(u, v, flat=True) - self.c0
        return (p.dot(self.d) + self.top + 0.2, p.z + self.h + 0.3)

    def head_points(self):
        return [self.S(i / NU, 0.0) for i in range(NU + 1)]


class LateenSail(Sail):
    """Triangle T (amure, bas de l'antenne) - P (pic) - C (point d'écoute) ; u le long de la bordure T->C,
    v vers le pic (les points de la rangée v = 1 se confondent en P)."""

    def __init__(self, T, P, C, belly, side, yard):
        self.T, self.P, self.C, self.belly, self.yard = T, P, C, belly, yard
        n = (P - T).cross(C - T).normalized()
        self.bulge_dir = n if n.dot(side) >= 0 else -n
        e1 = Vector((C.x - T.x, C.y - T.y, 0.0)).normalized()
        self.e1 = e1
        self.shroud = _mizzen_shroud(1 if self.bulge_dir.x >= 0 else -1)
        self.flip = self._flip()

    def x_limit(self, p):
        """Abscisse maximale (côté ventre) autorisée en p : hauban d'artimon du bord du ventre - shroud_gap, ou None
        hors de l'emprise des haubans (y / z)."""
        if not self.shroud:
            return None
        hp, lo, (y0, y1), side = self.shroud
        if not (y0 <= p.y <= y1 and lo.z + 0.3 <= p.z <= hp.z - 0.05):
            return None
        t = (hp.z - p.z) / (hp.z - lo.z)
        return side * (abs(hp.x) + (abs(lo.x) - abs(hp.x)) * t - LATEEN["shroud_gap"])

    def S(self, u, v, flat=False):
        base = self.T + (self.C - self.T) * u
        p = base + (self.P - base) * v
        if flat:
            return p
        g = math.sin(PI * u) ** 0.8
        if u > 0.5:
            g = 0.12 + 0.88 * g
        bul = g * (math.sin(PI * v ** 1.1) ** 1.1 * 0.8 + 0.3 * (1 - v) ** 2)
        d = (p - (self.T + (self.P - self.T) * v)).length         # distance au guindant (antenne)
        bul *= H.smoothstep(0.0, LATEEN["luff_fade"], d)
        q = p + self.bulge_dir * (self.belly * bul)
        lim = self.x_limit(q)
        if lim is not None and abs(q.x) > abs(lim) and abs(self.bulge_dir.x) > 0.5:
            q -= self.bulge_dir * ((q.x - lim) / self.bulge_dir.x)    # borné sous le hauban (jamais sous le plat)
            if (q - p).dot(self.bulge_dir) < 0:
                q = p
        return q

    def uv(self, u, v):
        p = self.S(u, v, flat=True) - self.T
        return (p.dot(self.e1) + 0.2, p.z + 0.5)

    def head_points(self):
        return [self.S(0.0, j / NV) for j in range(NV)]

    def corners(self):
        return [(1.0, 0.0)]


def quad_grid(tmp, uv, rows, flip):
    """Faces d'une grille rows[j][i] = (Vector, (u, v)) ; sommets confondus fusionnés (pic de la latine).
    Renvoie {(j, i): face} (cellule entre les rangées j, j+1 et les colonnes i, i+1)."""
    verts = {}
    vrows = []
    faces = {}
    for row in rows:
        vr = []
        for p, t in row:
            key = (round(p.x, 4), round(p.y, 4), round(p.z, 4))
            if key not in verts:
                verts[key] = tmp.verts.new(p)
            vr.append((verts[key], t))
        vrows.append(vr)
    for j in range(len(vrows) - 1):
        for i in range(len(vrows[j]) - 1):
            quad = [vrows[j][i], vrows[j][i + 1], vrows[j + 1][i + 1], vrows[j + 1][i]]
            if flip:
                quad.reverse()
            vs = []
            for vert, t in quad:
                if all(vert is not q[0] for q in vs):
                    vs.append((vert, t))
            if len(vs) < 3:
                continue
            try:
                f = tmp.faces.new([q[0] for q in vs])
            except ValueError:
                continue
            for lp, (_, t) in zip(f.loops, vs):
                lp[uv].uv = t
            faces[(j, i)] = f
    return faces


def sheet(b, sail, mat, cells=None):
    """Les deux nappes de la voile (avant côté ventre, arrière), lissées, UV en mètres (laizes verticales).
    `cells` = {(sign, i, j): (matériau, décalage UV)} : cellules de la grille portant une pièce rapiécée = les
    MÊMES faces que la nappe (matériau et UV propres, aucune nappe rapportée : ni z-fighting ni bord en escalier)."""
    tmp = bmesh.new()
    uv = tmp.loops.layers.uv.verify()
    special = []
    for sign in (1, -1):
        rows = []
        for j in range(NV + 1):
            v = j / NV
            rows.append([(sail.S(i / NU, v) + sail.N(i / NU, v) * (sign * THICK / 2), sail.uv(i / NU, v)) for i in range(NU + 1)])
        faces = quad_grid(tmp, uv, rows, sail.flip if sign > 0 else not sail.flip)
        for (sg, i, j), (m, shift) in (cells or {}).items():
            f = faces.get((j, i)) if sg == sign else None
            if f is None:
                continue
            for lp in f.loops:
                lp[uv].uv = (lp[uv].uv[0] + shift[0], lp[uv].uv[1] + shift[1])
            special.append((f, m))
    tmp.normal_update()
    tmp.faces.index_update()
    idx = [(f.index, m) for f, m in special]
    n0 = len(b.bm.faces)
    b._merge(tmp, mat, True)
    b.bm.faces.ensure_lookup_table()
    for k, m in idx:
        b.bm.faces[n0 + k].material_index = b._mi(m)


def _rail_clearance_z(y_world):
    """Dessus de la lisse de pavois (+ jambettes) sous un point de bordure à y réel."""
    s = H.station_at(y_world, H.deck_z(y_world) + 1.0)
    return H.rail_top_z(s) + 0.15


def make_sails():
    out = {}
    for name, sp in SPECS.items():
        c, d, L, r = MY.yard_frame(name)
        c0 = c - UP * (1.1 * r + 0.025)                      # têtière juste sous la vergue (sous le quartier octogonal)
        half_top, half_bot = L / 2 - sp["top"], L / 2 - sp["bot"]
        if "h" in sp:
            h = sp["h"]
        elif "foot_gap" in sp:                                # hunier : coins de bordure juste au-dessus de la hune
            h = c0.z - (H.MASTS[H.YARDS[name]["mast"]]["z_top"] + sp["foot_gap"])
        else:                                                 # basse voile : milieu de bordure au-dessus de la lisse
            h = 3.0                                           # (le milieu de bordure avance de 0,35 ventre : point fixe)
            for _ in range(4):
                h = c0.z + sp["arc"] - (_rail_clearance_z(c0.y - 0.35 * sp["belly"] * h) + sp["clear"])
        out[name] = SquareSail(name, c0, d, half_top, half_bot, h, sp["belly"] * h, sp["arc"], sp.get("bonnet"), (c, d, r, L))
    c, d, L, r = MY.yard_frame("lateen")
    perp = Vector((0.0, d.z, -d.y))                           # perpendiculaire à l'antenne, vers le bas / l'arrière
    off = perp * (1.1 * r + 0.025)
    T = c - d * (L / 2 - LATEEN["tack"]) + off
    P = c + d * (L / 2 - LATEEN["peak"]) + off
    y_clew = H.FITTINGS["flagstaff"][1] - LATEEN["flag_gap"]
    z_clew = _rail_clearance_z(y_clew) + LATEEN["clew_up"]
    C = Vector((T.x + 0.10, y_clew, z_clew))
    out["lateen"] = LateenSail(T, P, C, LATEEN["belly"], Vector((1, 0, 0)), (c, d, r, L))
    return out


def patch_mat():
    """Toile de pièce rapiécée : la MÊME recette que `canvas` (laizes, coutures, translucidité), à peine plus
    neutre et moins saturée (écart <= 5 %) ; les UV des pièces sont décalées dans sheet() (laizes discontinues)."""
    name = GM.PREFIX + "CanvasPatch"
    m = bpy.data.materials.get(name)
    if m:
        return m
    m = GM.canvas_mat(name)
    for n in m.node_tree.nodes:
        if n.type == "HUE_SAT":
            n.inputs["Saturation"].default_value = 0.75
        elif n.type == "MIX_RGB" and n.blend_type == "MULTIPLY" and not n.inputs["Color2"].is_linked:
            n.inputs["Color2"].default_value = (1.04, 1.02, 0.96, 1.0)          # teinte (canvas : 1,08/1,02/0,90)
    return m


def build(coll, M=None, rng=None):
    M = M or GM.mats()
    rng = rng or random.Random(11)
    b = Batch("Galleon_Sails")
    m_patch = patch_mat()
    for sail in make_sails().values():
        cells, blocks = SP.patches(sail, m_patch, rng)
        sheet(b, sail, M["canvas"], cells)
        SP.patch_borders(b, sail, blocks, M["rope_tar"])
        SP.boltrope(b, sail, M["rope_tar"])
        SP.robands(b, sail, M["rope"])
        SP.cringles(b, sail, M["rope_tar"])
        if sail.bonnet:
            SP.bonnet_lacing(b, sail, sail.bonnet, M["rope"], seam_mat())
    return [b.finish(coll)]


def seam_mat():
    """Couture de bonnette : même brun que les lignes de laizes du matériau canvas (8D7A68), mat."""
    return GM.flat(GM.PREFIX + "CanvasSeam", "8D7A68", rough=0.95)


def build_test(coll):
    objs = build(coll)
    try:                                   # mâture + beaupré comme repères visuels (hors stats / cadrage)
        from galleon import masts, bow
        for o in masts.build(coll):
            o.name = "REF-" + o.name
        bow.build_bowsprit(coll, GM.mats(), random.Random(3)).name = "REF-Bowsprit"
    except Exception as exc:               # noqa: BLE001
        print("sails.build_test: pas de mâture de référence :", exc)
    return objs
