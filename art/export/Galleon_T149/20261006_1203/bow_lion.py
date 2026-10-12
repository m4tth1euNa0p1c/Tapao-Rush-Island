"""
bow_lion — lion de proue doré (rampant, incliné vers l'avant, assis sur la guibre), sculpté : corps massif (hanches,
tronc, poitrail, encolure), pattes avant tendues et pattes arrière repliées avec doigts et griffes, queue ; tête,
crinière en lobes plats « flammes » et couronne dans bow_lion_head. Or du lion un peu plus rugueux (0,36-0,44),
or sombre (M_Galleon_GoldLionDark) dans les creux (gueule, sous la crinière, entre les doigts), bruit de
déplacement 5 mm sur tout le lion. Repère local (s, f, u) : s = bâbord, f = devant le lion (vers −Y et vers le bas),
u = le long de l'échine (vers le haut et l'avant).
"""
import math

import bpy
from mathutils import Matrix, Vector, noise

from galleon import materials as GM
from galleon import bow_util as U
from galleon.bow_util import ellipsoid, cone

TILT = math.radians(33.0)     # inclinaison de l'échine vers l'avant
HEAD_UP = math.radians(37.0)  # relèvement de la tête par rapport à l'échine : museau 4° au-dessus de l'horizontale
NOISE = 0.005                 # amplitude du bruit de déplacement (m)


def lion_mats():
    """(or du lion, or sombre des creux) : variantes de materials.gold_mat (rugosité 0,36-0,44 / 0,45-0,55)."""
    g = GM.gold_mat("M_Galleon_GoldLion")
    d = GM.gold_mat("M_Galleon_GoldLionDark", tone=0.55)
    for m, (r0, r1), dark in ((g, (0.36, 0.44), None), (d, (0.45, 0.55), ("3F2A14", "7A5530"))):
        for n in m.node_tree.nodes:
            if n.type == "MAP_RANGE":
                n.inputs["To Min"].default_value, n.inputs["To Max"].default_value = r0, r1
            if n.type == "VALTORGB" and dark:
                n.color_ramp.elements[0].color = GM.srgb(dark[0])
                n.color_ramp.elements[1].color = GM.srgb(dark[1])
    return g, d


def _u_knee(L, f, k=1.0):
    """Hauteur (u local, non mise à l'échelle) du dessus de la guibre sous le point (0, f·k, 0) du repère du lion."""
    from galleon import bow as B
    y = L.p(0.0, f * k, 0.0).y
    return (Vector((0.0, y, B.knee_top(y))) - L.O).dot(L.U) / k


class Frame:
    def __init__(self, origin, tilt=TILT):
        self.O = Vector(origin)
        self.S = Vector((1.0, 0.0, 0.0))
        self.U = Vector((0.0, -math.sin(tilt), math.cos(tilt)))
        self.F = Vector((0.0, -math.cos(tilt), -math.sin(tilt)))
        self.R = Matrix((self.S, self.F, self.U)).transposed()

    def p(self, s, f, u):
        return self.O + self.S * s + self.F * f + self.U * u

    def d(self, s, f, u):
        return (self.S * s + self.F * f + self.U * u).normalized()

    def rot(self, axis_local):
        """Matrice dont Z est l'axe local donné (s, f, u) exprimé en monde."""
        a = self.S * axis_local[0] + self.F * axis_local[1] + self.U * axis_local[2]
        return U.frame_from(a, ref=self.S if abs(a.normalized().dot(self.S)) < 0.9 else self.F)


def paw(b, L, c, axis, up, gold, dark, k, r=1.0, n_toes=4):
    """Patte : coussin + doigts + griffes (cônes) + creux sombres entre les doigts. c = centre (local), axis = direction
    des doigts (local), up = dessus de la patte (local)."""
    A, Up = L.d(*axis), L.d(*up)
    Sd = A.cross(Up).normalized()
    ca = L.p(*[v * k for v in c])
    ellipsoid(b, ca, [0.105 * r * k, 0.075 * r * k, 0.16 * r * k], gold, R=U.frame_from(A, ref=Sd), u=10, v=6)
    w = 0.062 * r * k
    for j in range(n_toes):
        off = (j - (n_toes - 1) / 2) * w
        ct = ca + A * 0.15 * r * k + Sd * off + Up * 0.01 * k
        ellipsoid(b, ct, [0.034 * r * k, 0.03 * r * k, 0.055 * r * k], gold, R=U.frame_from(A, ref=Sd), u=6, v=4)
        cone(b, ct + A * 0.045 * r * k - Up * 0.012 * k, (A - Up * 0.6).normalized(), 0.014 * r * k, 0.002, 0.05 * r * k, gold, segs=6)
        if j < n_toes - 1:
            ellipsoid(b, ct + Sd * w / 2 - Up * 0.012 * k + A * 0.005, [0.02 * k, 0.03 * k, 0.045 * k], dark, R=U.frame_from(A, ref=Sd), u=5, v=3)


def build(coll, M, rng, y_hips, z_hips=None, scale=1.0):
    """Lion assis sur la guibre : hanches à (0, y_hips, z_hips). Renvoie l'objet Galleon_Bow_Lion."""
    from galleon import bow as B
    from galleon import bow_lion_head as LH
    if z_hips is None:
        z_hips = B.knee_top(y_hips) + 0.08
    b = GM.Batch("Galleon_Bow_Lion")
    L = Frame((0.0, y_hips, z_hips))
    gold, dark = lion_mats()
    k = scale

    def E(c, radii, axis=None, mat=gold, u=12, v=7):
        R = L.rot(axis) if axis else L.R
        ellipsoid(b, L.p(*[v_ * k for v_ in c]), [r * k for r in radii], mat, R=R, u=u, v=v)

    # corps : hanches, tronc, poitrail massif, encolure (radii dans le repère (s, f, u))
    E((0, -0.02, 0.12), (0.30, 0.30, 0.27), u=14, v=8)
    E((0, 0.05, 0.50), (0.27, 0.28, 0.52), u=14, v=9)
    E((0, 0.14, 0.86), (0.32, 0.30, 0.30), u=14, v=8)
    E((0, 0.17, 1.05), (0.26, 0.27, 0.25), u=14, v=8)
    E((0, 0.11, 0.66), (0.21, 0.21, 0.22), mat=dark, u=10, v=6)        # creux sous le poitrail (aisselles)
    for sg in (1, -1):
        E((sg * 0.27, 0.13, 0.78), (0.13, 0.17, 0.21), axis=(0, 0.4, 1), u=10, v=6)   # épaule
        # pattes arrière repliées : cuisse contre la hanche, jarret en arrière, tibia vers l'avant, patte sur la guibre
        E((sg * 0.26, 0.10, 0.10), (0.15, 0.22, 0.27), axis=(0, 1, 0.45), u=12, v=7)       # cuisse
        E((sg * 0.25, -0.06, -0.12), (0.085, 0.085, 0.10), axis=(0, 0.3, 1), u=9, v=6)     # jarret
        f_paw = 0.30
        u_paw = _u_knee(L, f_paw, k) + 0.05
        shin = [L.p(sg * 0.25 * k, -0.06 * k, -0.12 * k), L.p(sg * 0.21 * k, 0.12 * k, (u_paw - 0.05) * k),
                L.p(sg * 0.16 * k, (f_paw - 0.06) * k, u_paw * k)]
        b.tube(shin, [0.075 * k, 0.07 * k, 0.065 * k], gold, segs=10)
        paw(b, L, (sg * 0.15, f_paw, u_paw), (0, 1, 0), (0, 0, 1), gold, dark, k, r=0.95)
        # pattes avant tendues vers l'avant et le haut
        b.tube([L.p(sg * 0.22 * k, 0.18 * k, 0.78 * k), L.p(sg * 0.25 * k, 0.42 * k, 0.9 * k), L.p(sg * 0.26 * k, 0.66 * k, 0.98 * k)],
               [0.10 * k, 0.085 * k, 0.075 * k], gold, segs=10)
        E((sg * 0.25, 0.44, 0.91), (0.095, 0.095, 0.13), axis=(0, 1, 0.5), u=9, v=6)      # coude
        paw(b, L, (sg * 0.26, 0.76, 0.99), (0, 1, 0.15), (0, -0.15, 1), gold, dark, k)
    LH.build_head(b, L, gold, dark, M, rng, k)
    # queue : sort des hanches, s'enroule sur la cuisse tribord puis court le long du flanc de la guibre
    pts = [L.p(0.08 * k, -0.26 * k, 0.16 * k), L.p(0.26 * k, -0.24 * k, 0.04 * k), L.p(0.34 * k, -0.10 * k, -0.12 * k),
           L.p(0.26 * k, 0.08 * k, (_u_knee(L, 0.08, k) - 0.14) * k), L.p(0.21 * k, 0.26 * k, (_u_knee(L, 0.26, k) - 0.10) * k)]
    b.tube(pts, [0.045 * k, 0.04 * k, 0.035 * k, 0.03 * k, 0.025 * k], gold, segs=8)
    E((0.21, 0.34, _u_knee(L, 0.34, k) - 0.09), (0.045, 0.1, 0.045), axis=(0, 1, 0), u=8, v=5)
    _displace(b, NOISE * k)
    return b.finish(coll)


def _displace(b, amp, scale=9.0):
    """Bruit de déplacement léger (sculpture) sur tous les sommets du lot."""
    b.bm.normal_update()
    for v in b.bm.verts:
        v.co += noise.noise_vector(v.co * scale) * amp
    b.bm.normal_update()
