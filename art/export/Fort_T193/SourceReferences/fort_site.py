"""
Fort francais : site (lot 'site').

SM_Fort_Ditch : fosse sec creuse dans le platier fossile (fond -1,32 en sable corallien et gravats, flaques d'eau
saumatre, blocs ecroules), contrescarpe en moellons de corail talutee 1/10 suivant le decalage exact de 6,715 m du trace
bastionne (lignes paralleles aux faces, arcs devant les angles), couronnement de dalles de beachrock et chemin couvert
de 2 m.
REF-Site_* (non exportes : Landscape + eau dans Unreal) : terrain du motu (geologie Tuamotu, params.site.profile) en
deux maillages (1 m pres du fort, 4 m au loin), couches de sol de art/textures/terrain_layers melangees par attributs
(MaskA, MaskB), lagon et ocean (plan d'eau a -1,65, couleur par profondeur, ecume), allees dallees de la cour.
"""
import math
import os
import random

import bmesh
import bpy
from mathutils import Matrix, Vector, noise

import fort_geo as G
import fort_kit as FK
import fort_mat as FM

V = G.V
TAU = 2 * math.pi
MSL = -1.65
LAYERS = os.path.join(FM.ART, "terrain_layers")


# ----------------------------------------------------------------------------
# Geometrie 2D : decalage exact du trace (contrescarpe)

def _seg_dist(p, a, b):
    ax, ay = a
    bx, by = b
    dx, dy = bx - ax, by - ay
    L2 = dx * dx + dy * dy
    t = max(0.0, min(1.0, ((p[0] - ax) * dx + (p[1] - ay) * dy) / L2)) if L2 > 0 else 0.0
    qx, qy = ax + t * dx, ay + t * dy
    return math.hypot(p[0] - qx, p[1] - qy)


def poly_dist(poly, p):
    n = len(poly)
    return min(_seg_dist(p, poly[i], poly[(i + 1) % n]) for i in range(n))


def ray_hit(poly, ang, far=True):
    """Distance (depuis l'origine) de l'intersection d'un rayon avec le polygone (la plus lointaine)."""
    dx, dy = math.cos(ang), math.sin(ang)
    best = None
    n = len(poly)
    for i in range(n):
        ax, ay = poly[i]
        bx, by = poly[(i + 1) % n]
        ex, ey = bx - ax, by - ay
        den = dx * ey - dy * ex
        if abs(den) < 1e-12:
            continue
        t = (ax * ey - ay * ex) / den
        u = (ax * dy - ay * dx) / den
        if t > 0 and -1e-9 <= u <= 1 + 1e-9:
            if best is None or (t > best if far else t < best):
                best = t
    return best


def offset_exact(poly, d, step=0.4, arc_deg=4.0):
    """Courbe a distance d (exterieur) d'un polygone anti-horaire : points candidats (segments decales + arcs aux
    sommets convexes) filtres par la distance, ordonnes par angle polaire (forme etoilee depuis l'origine)."""
    n = len(poly)
    cand = []
    for i in range(n):
        a, b = Vector((*poly[i], 0)), Vector((*poly[(i + 1) % n], 0))
        u = (b - a)
        L = u.length
        u.normalize()
        nrm = Vector((u.y, -u.x, 0))
        k = max(2, int(L / step))
        for j in range(k + 1):
            q = a + u * (L * j / k) + nrm * d
            cand.append((q.x, q.y))
        # arc au sommet b (si convexe)
        c = Vector((*poly[(i + 2) % n], 0))
        u2 = (c - b).normalized()
        n2 = Vector((u2.y, -u2.x, 0))
        cross = u.x * u2.y - u.y * u2.x
        if cross > 0:     # virage a gauche = sommet convexe pour un polygone anti-horaire
            a0 = math.atan2(nrm.y, nrm.x)
            a1 = math.atan2(n2.y, n2.x)
            while a1 < a0:
                a1 += TAU
            m = max(2, int(math.degrees(a1 - a0) / arc_deg))
            for j in range(m + 1):
                aa = a0 + (a1 - a0) * j / m
                cand.append((b.x + d * math.cos(aa), b.y + d * math.sin(aa)))
    keep = [q for q in cand if poly_dist(poly, q) > d - 0.02]
    keep.sort(key=lambda q: math.atan2(q[1], q[0]))
    out = []
    for q in keep:
        if not out or math.hypot(q[0] - out[-1][0], q[1] - out[-1][1]) > 0.15:
            out.append(q)
    return out


def resample_polar(poly, n):
    out = []
    for i in range(n):
        a = -math.pi + TAU * i / n
        r = ray_hit(poly, a)
        out.append((r * math.cos(a), r * math.sin(a)))
    return out


def trace():
    return [tuple(p) for p in FK.params()["site"]["trace_cordon_m"]]


def counterscarp(d=None):
    m = FK.mod("ditch")
    d = d or 6.715
    return offset_exact(trace(), d)


# ----------------------------------------------------------------------------
# SM_Fort_Ditch

def build_ditch(coll, p=None):
    p = {**FK.mod("ditch"), **(p or {})}
    M = FM.mats()
    rng = random.Random(501)
    fb = G.FB("Ditch")
    zb = p["bottom_z"]
    hc = p["counterscarp"]["height_m"]
    N = 480
    C = resample_polar(counterscarp(), N)
    F = resample_polar(offset_exact(trace(), 0.715, step=0.3), N)
    # fond : bande entre le pied d'escarpe (F) et la contrescarpe (C), legerement bombee et bosselee
    fb.blend = (0.0, 0.0, 0.0)
    fb.base = 0.0
    off = Vector((rng.uniform(0, 50), rng.uniform(0, 50), 0))
    rows = 4
    for i in range(N):
        j = (i + 1) % N
        ring_i = [Vector(F[i]).lerp(Vector(C[i]), k / rows) for k in range(rows + 1)]
        ring_j = [Vector(F[j]).lerp(Vector(C[j]), k / rows) for k in range(rows + 1)]
        for k in range(rows):
            pts = [ring_i[k], ring_j[k], ring_j[k + 1], ring_i[k + 1]]
            vv = []
            for q in pts:
                z = zb + 0.02 + 0.03 * noise.noise(Vector((q.x * 0.3, q.y * 0.3, 0)) + off)
                vv.append(V(q.x, q.y, z))
            fb.poly_bm(vv, [(0, 1, 2, 3)], M["gravel"], recalc=False)
    # contrescarpe talutee 1/10 + couronnement de beachrock + chemin couvert (2 m)
    n0 = fb.mark()
    fb.blend = (0.0, 0.0, 0.0)
    fb.base = zb
    for i in range(N):
        j = (i + 1) % N
        ci, cj = Vector(C[i]), Vector(C[j])
        di, dj = ci.normalized(), cj.normalized()
        bi, bj = ci - di * (hc * 0.1), cj - dj * (hc * 0.1)
        fb.poly_bm([V(bi.x, bi.y, zb - 0.05), V(bj.x, bj.y, zb - 0.05), V(cj.x, cj.y, -0.1), V(ci.x, ci.y, -0.1)],
                   [(3, 2, 1, 0)], M["rubble"], recalc=False)
    fb.base = 0.0
    fb.blend = (0.0, 0.0, 1.0)
    acc = 0.0
    seg = []
    for i in range(N):
        a, b = Vector(C[i]), Vector(C[(i + 1) % N])
        seg.append((a, b))
    i = 0
    while i < N:
        k = rng.randint(1, 2)
        a = seg[i][0]
        b = seg[min(i + k, N) - 1][1]
        u = (b - a)
        if u.length > 1e-3:
            c = (a + b) / 2 + c_out(a, b) * 0.2
            fb.box((c.x, c.y, -0.06), (u.length - 0.01, 0.5, 0.12), M["ashlar"],
                   R=Matrix.Rotation(math.atan2(u.y, u.x), 3, "Z"), bevel=0.015)
        i += k
    fb.blend = (0.0, 0.0, 0.0)
    W2 = 2.0
    for i in range(N):
        j = (i + 1) % N
        ci, cj = Vector(C[i]), Vector(C[j])
        oi, oj = ci + ci.normalized() * 0.45, cj + cj.normalized() * 0.45
        ei, ej = ci + ci.normalized() * W2, cj + cj.normalized() * W2
        fb.poly_bm([V(oi.x, oi.y, -0.02), V(oj.x, oj.y, -0.02), V(ej.x, ej.y, -0.06), V(ei.x, ei.y, -0.06)],
                   [(0, 1, 2, 3)], M["gravel"], recalc=False)
    # flaques d'eau saumatre et blocs ecroules
    for k in range(14):
        i = rng.randrange(N)
        t = rng.uniform(0.35, 0.75)
        c = Vector(F[i]).lerp(Vector(C[i]), t)
        r = rng.uniform(0.6, 1.8)
        pts = []
        for a in range(14):
            ang = TAU * a / 14
            rr = r * (0.6 + 0.4 * noise.noise(Vector((math.cos(ang) * 2 + k, math.sin(ang) * 2, 0))) + 0.4)
            pts.append(V(c.x + rr * math.cos(ang) * 1.4, c.y + rr * math.sin(ang), zb + 0.045))
        fb.poly_bm(pts, [tuple(range(14))], M["puddle"], recalc=False)
    for k in range(26):
        i = rng.randrange(N)
        t = rng.uniform(0.02, 0.2) if rng.random() < 0.7 else rng.uniform(0.8, 0.97)
        c = Vector(F[i]).lerp(Vector(C[i]), t)
        s = rng.uniform(0.2, 0.55)
        fb.blend = (0.0, 1.0 if rng.random() < 0.4 else 0.0, 0.0)
        fb.box((c.x, c.y, zb + s * 0.3), (s, s * rng.uniform(0.6, 1.0), s * 0.7), M["rubble"],
               R=Matrix.Rotation(rng.uniform(0, TAU), 3, "Z") @ Matrix.Rotation(rng.uniform(-0.3, 0.3), 3, "X"),
               bevel=0.04)
    ob = fb.finish(coll)
    ucx = []
    step = 12
    for i in range(0, N, step):
        j = min(i + step, N) % N
        a, b = Vector(C[i]), Vector(C[j])
        fa, fbb = Vector(F[i]), Vector(F[j])
        ucx.append(("fond", None, [V(q.x, q.y, z) for q in (a, b, fa, fbb) for z in (zb - 0.3, zb + 0.02)]))
        ucx.append(("contrescarpe", None, [V(q.x, q.y, z) for q in (a, b, a + a.normalized() * 0.6,
                                                                     b + b.normalized() * 0.6) for z in (zb, 0.0)]))
    return FK.res([ob], {"Bridge_Counterscarp": ("root", V(0, -26.915, 0))}, ucx, p,
                  info={"counterscarp_points": len(C), "width_bottom_m": 6.0}, no_sharp=True)


def c_out(a, b):
    m = ((a + b) / 2)
    return m.normalized()


# ----------------------------------------------------------------------------
# Terrain (REF) : profil de motu

class Site:
    def __init__(self):
        P = FK.params()
        self.lay = P["scene_layout"]
        self.C = counterscarp()
        self.F = offset_exact(trace(), 0.715, step=0.5)
        self.rng = random.Random(777)
        r = random.Random(778)
        self.patates = [(r.uniform(-110, 50), r.uniform(-100, -55), r.uniform(2.5, 6.0), r.uniform(-2.6, -2.2))
                        for _ in range(12)]
        self.cache = {}

    def in_C(self, x, y):
        return G.inside(self.C, (x, y))

    def dist_C(self, x, y):
        return poly_dist(self.C, (x, y))

    def shore_y(self, x):
        """Rive du lagon (sud) ; quai droit entre -24 et 24 (fin de quai : la plage reprend en pente douce)."""
        if abs(x) <= 24:
            return -42.0
        return -42.0 + 2.0 * min(1.0, (abs(x) - 24) / 6.0) - 0.05 * max(0.0, abs(x) - 30) + 1.2 * math.sin(x * 0.07)             * min(1.0, (abs(x) - 24) / 10.0)

    def east_end(self, y):
        return 50.0 + 4.0 * math.sin(y * 0.06) + 2.0 * math.sin(y * 0.17)

    def height(self, x, y):
        key = (round(x, 3), round(y, 3))
        if key in self.cache:
            return self.cache[key]
        z = self._height(x, y)
        self.cache[key] = z
        return z

    def _height(self, x, y):
        n1 = noise.noise(Vector((x * 0.05, y * 0.05, 0.3)))
        n2 = noise.noise(Vector((x * 0.21, y * 0.21, 1.7)))
        # fort : terre-plein a 0, fosse a -1,6 (sous le fond du SM_Fort_Ditch), chemin couvert
        if abs(x) < 45 and abs(y) < 42:
            if G.inside(self.F, (x, y)):
                return 0.0
            if self.in_C(x, y):
                return -1.6
            dc = self.dist_C(x, y)
            if dc < 1.6:
                return -1.6
        # motu : sol de base + bourrelet de tempete au nord + plages
        base = 0.28 + 0.14 * n1 + 0.04 * n2
        # aplanissement autour du fort (glacis, esplanade)
        dfort = max(0.0, max(abs(x) - 40.0, abs(y) - 36.0))
        flat = min(1.0, dfort / 18.0)
        flat = flat * flat * (3 - 2 * flat)
        z = base * flat + 0.0 * (1 - flat)
        # nord : glacis -> bourrelet (+0,66 a y 55) -> plage oceane (-1,65 a y 63) -> platier -> crete -> tombant
        if y > 36:
            t = min(1.0, (y - 36) / 19.0)
            z = max(z, 0.66 * t * t * (3 - 2 * t) + 0.08 * n2 * t)
        if y > 55:
            t = min(1.0, (y - 55) / 8.0)
            z = 0.66 + (MSL - 0.66) * (t ** 1.3) + 0.05 * n2
        if y > 63:
            z = -1.8 + 0.06 * n2 + 0.04 * n1
            if y > 86:
                t = min(1.0, (y - 86) / 4.0)
                z = -1.8 + 0.25 * math.sin(math.pi * t) + 0.05 * n2
            if y > 92:
                t = min(1.0, (y - 92) / 18.0)
                z = -1.6 - 16.4 * (t ** 1.6)
        # sud : esplanade (0 -> -0,33 au quai) puis lagon
        ys = self.shore_y(x)
        if y < ys + 6 and y < 36:
            if abs(x) <= 24:
                if y > -38:
                    pass
                elif y > -41.6:
                    z = min(z, -0.33 * (-(y + 38) / 3.6))
                else:
                    z = -2.97
            beach = None
            if abs(x) > 24:
                t = (ys + 6 - y) / 14.0
                beach = 0.35 - 2.2 * max(0.0, t) ** 0.85 + 0.03 * n2
                z = min(z, beach)
            if y < ys - 1.0:
                d = (ys - 1.0 - y)
                zl = -2.3 - min(4.3, d * 0.075) + 0.15 * n1
                if abs(x) <= 24:
                    zl = min(zl, -2.97 - d * 0.06)
                    z = min(z, zl)
                else:
                    z = max(min(z, beach), zl)
                for px, py, pr, pz in self.patates:
                    dd = math.hypot(x - px, y - py) / pr
                    if dd < 1.0:
                        z = max(z, pz + (zl - pz) * dd * dd + 0.2 * n2)
        # est : plage vers la passe (chenal a -9), motu voisin au-dela de x 104
        xe = self.east_end(y)
        if x > xe - 6:
            t = (x - (xe - 6)) / 10.0
            zz = 0.25 - 2.2 * max(0.0, t) ** 0.9
            if x > xe + 4:
                zz = -1.9 - min(7.1, (x - xe - 4) * 0.6)
            if x > 96:
                zz = max(zz, -9.0 + (x - 96) * 1.3)
            if x > 104:
                zz = min(0.45, -1.9 + (x - 104) * 0.3) + 0.06 * n2
            z = min(z, zz) if x < 104 else zz
        # extremites du maillage : le motu ouest et le motu voisin plongent dans l'eau (pas de falaise de bord)
        if x < -150:
            t = min(1.0, (-150 - x) / 18.0)
            z = z * (1 - t) + (-3.5) * t
        if x > 132:
            t = min(1.0, (x - 132) / 14.0)
            z = z * (1 - t) + (-6.0) * t
        if x > 100:
            ext = max(0.0, abs(y - 10.0) - 70.0)
            if ext > 0:
                t = min(1.0, ext / 14.0)
                z = z * (1 - t) + (-6.0) * t
        return z

    def masks(self, x, y, z):
        """(MaskA, MaskB) : A = (sable sec, gravats, feo, terre), B = (litiere, mouille, laisse, -)."""
        n = noise.noise(Vector((x * 0.06, y * 0.06, 4.2)))
        n2 = noise.noise(Vector((x * 0.15, y * 0.15, 7.1)))
        dry = 1.0 if z > MSL + 0.45 else max(0.0, (z - MSL - 0.1) / 0.35)
        rubble, feo, soil, litter, wet, wrack = 0.0, 0.0, 0.0, 0.0, 0.0, 0.0
        if MSL - 0.1 < z < MSL + 0.35:
            wet = 1.0 - abs(z - (MSL + 0.1)) / 0.3
        if MSL + 0.3 < z < MSL + 0.6 and y < 0:
            wrack = max(0.0, 0.8 * n2 + 0.2)
        if y > 52 and z > MSL - 0.2:
            rubble = 0.7 + 0.3 * n
        if y > 62:
            feo = 1.0 if z < MSL + 0.2 else 0.3
            rubble = max(rubble, 0.3 * max(0.0, n2))
        if x > 60 and -60 < y < 60 and z < -0.5:
            feo = max(feo, 0.6)
        # cocoteraie : terre + litiere ; glacis : terre battue melee de sable
        if z > -0.3 and (x < -42 or (36 < y < 54) or x > 104):
            soil = 0.45 + 0.4 * n
            litter = max(0.0, 0.5 + 0.6 * n2) if (x < -45 or y > 40) else 0.0
        if abs(x) < 46 and abs(y) < 40 and z > -0.4:
            soil = max(soil, 0.35 + 0.3 * n)
            rubble = max(rubble, 0.25 * max(0.0, n2))
        if abs(x) < 20 and abs(y) < 16:
            soil = 0.1 + 0.1 * n
            dry = 0.55
            rubble = 0.15 + 0.2 * n2
            wrack = 0.7 + 0.25 * n
        if -42 < y < -36 and abs(x) < 30:
            rubble = max(rubble, 0.3 + 0.3 * n2)
            wrack = max(wrack, 0.6 + 0.3 * n)
        if abs(x) < 46 and abs(y) < 40 and not (abs(x) < 20 and abs(y) < 16):
            wrack = max(wrack, 0.35 + 0.35 * n2)
        c = lambda v: max(0.0, min(1.0, v))                                       # noqa: E731
        return (c(dry), c(rubble), c(feo), c(soil)), (c(litter), c(wet), c(wrack), 1.0)


def terrain_mat():
    m = bpy.data.materials.get("MAT-Site_Ground")
    if m:
        return m
    m = bpy.data.materials.new("MAT-Site_Ground")
    nb = FM.NB(m)
    P = nb.geo().outputs["Position"]
    px, py, pz = nb.sep(P)

    # anti-repetition : chaque couche est lue a deux echelles (dont une tournee de 37 deg), melangees par un bruit
    rot = nb.node("ShaderNodeVectorRotate", rotation_type="Z_AXIS")
    rot.inputs["Angle"].default_value = 0.65
    nb.link(P, rot.inputs["Vector"])
    blend2 = nb.mr(nb.noise(nb.vscale(P, 0.045), 1.0, 3.0, 0.5), 0.38, 0.62)

    def layer(name, tile):
        f = os.path.join(LAYERS, name + "_ch.png")
        fn = os.path.join(LAYERS, name + "_n.png")
        v1 = nb.vscale(P, 1.0 / tile)
        v2 = nb.vscale(rot.outputs["Vector"], 1.0 / (tile * 2.3))
        c1, c2 = nb.img(f, v1, True), nb.img(f, v2, True)
        n1, n2 = nb.img(fn, v1, False), nb.img(fn, v2, False)
        return (nb.mix(blend2, c1.outputs["Color"], c2.outputs["Color"]),
                nb.mixf(blend2, c1.outputs["Alpha"], c2.outputs["Alpha"]),
                nb.mix(blend2, n1.outputs["Color"], n2.outputs["Color"]))
    base_c, base_h, base_n = layer("sand_wet", 3.0)
    mA = nb.attr("MaskA")
    mB = nb.attr("MaskB")
    spA = nb.node("ShaderNodeSeparateColor")
    nb.link(mA.outputs["Color"], spA.inputs["Color"])
    spB = nb.node("ShaderNodeSeparateColor")
    nb.link(mB.outputs["Color"], spB.inputs["Color"])
    col, nor = base_c, base_n
    for name, tile, fac in (("sand_dry", 3.5, spA.outputs["Red"]), ("rubble", 2.5, spA.outputs["Green"]),
                            ("feo", 3.0, spA.outputs["Blue"]), ("soil", 3.0, mA.outputs["Alpha"]),
                            ("litter", 2.5, spB.outputs["Red"]), ("wrack", 2.0, spB.outputs["Blue"])):
        c, h, n = layer(name, tile)
        f = nb.mr(nb.math("ADD", fac, nb.math("MULTIPLY", nb.math("SUBTRACT", h, 0.5), 0.5)), 0.35, 0.7)
        f = nb.math("MULTIPLY", f, nb.mr(fac, 0.02, 0.2))
        col = nb.mix(f, col, c)
        nor = nb.mix(f, nor, n)
    # mouille + sous l'eau : assombri, bleu-vert selon la profondeur
    wet = spB.outputs["Green"]
    col = nb.mix(nb.math("MULTIPLY", wet, 0.55), col, (0.32, 0.30, 0.26, 1.0), "MULTIPLY")
    depth = nb.math("SUBTRACT", MSL, pz)
    under = nb.mr(depth, 0.0, 0.5)
    tint = nb.mix(nb.mr(depth, 0.5, 6.0), (0.55, 0.85, 0.80, 1.0), (0.10, 0.38, 0.45, 1.0))
    col = nb.mix(nb.math("MULTIPLY", under, 0.85), col, nb.mix(1.0, col, tint, "MULTIPLY"))
    rough = nb.mixf(nb.math("MAXIMUM", wet, under), 0.9, 0.35)
    # variation macro
    mac = nb.noise(nb.vscale(P, 0.02), 1.0, 3.0, 0.5)
    col = nb.mix(1.0, col, nb.mix(nb.mr(mac, 0.35, 0.65), (0.66, 0.64, 0.60), (0.82, 0.80, 0.76)), "MULTIPLY")
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], rough)
    nm = nb.node("ShaderNodeNormalMap")
    nm.inputs["Strength"].default_value = 1.0
    nb.link(nor, nm.inputs["Color"])
    nb.link(nm.outputs["Normal"], nb.bsdf.inputs["Normal"])
    m["ue_recipe"] = "Landscape M_Terrain_Atoll : couches sand_dry / sand_wet / rubble / feo / soil / litter / wrack " \
                     "(height-blend) ; teinte sous l'eau par profondeur (WorldPosition.z < -1,65)."
    m.diffuse_color = (0.75, 0.70, 0.60, 1)
    return m


def water_mat():
    m = bpy.data.materials.get("MAT-Site_Lagoon")
    if m:
        return m
    m = bpy.data.materials.new("MAT-Site_Lagoon")
    nb = FM.NB(m)
    try:
        m.surface_render_method = "BLENDED"
    except (AttributeError, TypeError):
        pass
    m.use_backface_culling = False
    P = nb.geo().outputs["Position"]
    d = nb.attr("Depth").outputs["Fac"]
    foam = nb.attr("Foam").outputs["Fac"]
    ramp = nb.node("ShaderNodeValToRGB")
    els = ramp.color_ramp.elements
    els[0].position, els[0].color = 0.0, (0.30, 0.78, 0.72, 1)
    els[1].position, els[1].color = 1.0, (0.012, 0.045, 0.12, 1)
    for pos, c in ((0.06, (0.10, 0.58, 0.58, 1)), (0.18, (0.03, 0.36, 0.46, 1)), (0.45, (0.02, 0.14, 0.28, 1))):
        e = els.new(pos)
        e.color = c
    nb.link(nb.mr(d, 0.0, 18.0), ramp.inputs["Fac"])
    col = ramp.outputs["Color"]
    fn = nb.noise(nb.vscale(P, (0.9, 0.9, 1.0)), 3.0, 6.0, 0.6)
    fm = nb.math("MULTIPLY", foam, nb.mr(fn, 0.5, 0.64))
    col = nb.mix(fm, col, (0.92, 0.95, 0.95, 1.0))
    nb.link(col, nb.bsdf.inputs["Base Color"])
    nb._in(nb.bsdf.inputs["Roughness"], nb.mixf(fm, 0.035, 0.6))
    alpha = nb.mr(d, 0.0, 6.0, 0.10, 0.9)
    alpha = nb.math("MAXIMUM", alpha, fm)
    nb.link(alpha, nb.bsdf.inputs["Alpha"])
    nb.set("Specular IOR Level", 0.6)
    w1 = nb.img(FM.TEX["water_n"], nb.vscale(P, 0.08), False)
    w2 = nb.img(FM.TEX["water_n"], nb.vscale(P, 0.023), False)
    nm1 = nb.node("ShaderNodeNormalMap")
    nm1.inputs["Strength"].default_value = 0.35
    nb.link(nb.mix(0.5, w1.outputs["Color"], w2.outputs["Color"]), nm1.inputs["Color"])
    nb.link(nm1.outputs["Normal"], nb.bsdf.inputs["Normal"])
    m["ue_recipe"] = "Water plugin (Lake / Ocean) : absorption turquoise #38C9C1 -> #0E6F8A -> #0B2E5A ; ecume par " \
                     "distance au rivage."
    m.diffuse_color = (0.1, 0.5, 0.55, 1)
    return m


def _grid(name, coll, x0, x1, y0, y1, step, zfun, skip=None, attrs=None, mat=None, fix_edge=None):
    nx = int(round((x1 - x0) / step))
    ny = int(round((y1 - y0) / step))
    verts, idx = [], {}
    for j in range(ny + 1):
        for i in range(nx + 1):
            x, y = x0 + i * step, y0 + j * step
            idx[(i, j)] = len(verts)
            verts.append((x, y, zfun(x, y, i, j, nx, ny)))
    faces = []
    for j in range(ny):
        for i in range(nx):
            if skip and skip(x0 + (i + 0.5) * step, y0 + (j + 0.5) * step):
                continue
            faces.append((idx[(i, j)], idx[(i + 1, j)], idx[(i + 1, j + 1)], idx[(i, j + 1)]))
    me = bpy.data.meshes.new(name)
    me.from_pydata(verts, [], faces)
    me.polygons.foreach_set("use_smooth", [True] * len(me.polygons))
    if attrs:
        for an, fn in attrs.items():
            ca = me.color_attributes.new(an, "FLOAT_COLOR", "POINT")
            vals = []
            for v in verts:
                vals.extend(fn(*v))
            ca.data.foreach_set("color", vals)
    uv = me.uv_layers.new(name="UVMap")
    for lp in me.loops:
        co = verts[lp.vertex_index]
        uv.data[lp.index].uv = (co[0], co[1])
    if mat:
        me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob


def build_site_terrain(coll, p=None):
    p = {**FK.mod("site_terrain"), **(p or {})}
    S = Site()
    tm = terrain_mat()
    nx0, nx1, ny0, ny1 = -80.0, 84.0, -96.0, 76.0

    def zf_near(x, y, i, j, nx, ny):
        # bord : interpolation lineaire entre sommets du maillage lointain (pas 4) -> pas de fissure
        if i in (0, nx) or j in (0, ny):
            if i in (0, nx) and j % 4:
                ja, jb = j - j % 4, j - j % 4 + 4
                ya, yb = ny0 + ja, ny0 + jb
                t = (j % 4) / 4.0
                return S.height(x, ya) * (1 - t) + S.height(x, yb) * t
            if j in (0, ny) and i % 4:
                ia, ib = i - i % 4, i - i % 4 + 4
                xa, xb = nx0 + ia, nx0 + ib
                t = (i % 4) / 4.0
                return S.height(xa, y) * (1 - t) + S.height(xb, y) * t
        return S.height(x, y)

    def mA(x, y, z):
        return S.masks(x, y, z)[0]

    def mB(x, y, z):
        return S.masks(x, y, z)[1]
    # trou au-dessus de la casemate et de son couloir (bastion SO) : le sol de la cour (Z 0) ne doit pas les traverser
    tv = FK.params()["scene_layout"]["treasure_vault"]["pos"]
    vx0, vx1, vy0, vy1 = tv[0] - 4.0, tv[0] + 7.0, tv[1] - 4.0, tv[1] + 5.0
    near = _grid("REF-Site_Terrain_Near", coll, nx0, nx1, ny0, ny1, 1.0, zf_near, attrs={"MaskA": mA, "MaskB": mB},
                 mat=tm, skip=lambda x, y: vx0 < x < vx1 and vy0 < y < vy1)
    far = _grid("REF-Site_Terrain_Far", coll, -176.0, 180.0, -160.0, 144.0, 4.0, lambda x, y, *_: S.height(x, y),
                skip=lambda x, y: nx0 < x < nx1 and ny0 < y < ny1, attrs={"MaskA": mA, "MaskB": mB}, mat=tm)
    # plan d'eau : profondeur / ecume par sommet
    wm = water_mat()

    def depth(x, y):
        return max(0.0, MSL - S.height(x, y))

    def wz(x, y, *_):
        return MSL

    def dattr(x, y, z):
        d = depth(x, y)
        return (d, d, d, 1.0)

    def fattr(x, y, z):
        d = depth(x, y)
        f = 0.0
        if 0.0 < d < 0.2:
            f = 0.8 * (1.0 - d / 0.2)
        if 86 < y < 95 and x < 60:
            f = max(f, 0.85)
        return (f, f, f, 1.0)

    def wskip(x, y):
        return S.height(x, y) > MSL + 0.35 and S.height(x + 2, y + 2) > MSL + 0.35 and S.height(x - 2, y - 2) > MSL + 0.35
    water = _grid("REF-Lagoon", coll, -176.0, 180.0, -160.0, 144.0, 2.0, wz, skip=wskip,
                  attrs={"Depth": dattr, "Foam": fattr}, mat=wm)
    # ocean lointain (jusqu'a l'horizon)
    bm = bmesh.new()
    ring_o = [(-2500, -2500), (2500, -2500), (2500, 2500), (-2500, 2500)]
    ring_i = [(-176, -160), (180, -160), (180, 144), (-176, 144)]
    vo = [bm.verts.new((x, y, MSL - 0.01)) for x, y in ring_o]
    vi = [bm.verts.new((x, y, MSL - 0.01)) for x, y in ring_i]
    for k in range(4):
        bm.faces.new((vo[k], vo[(k + 1) % 4], vi[(k + 1) % 4], vi[k]))
    me = bpy.data.meshes.new("REF-Ocean")
    bm.to_mesh(me)
    bm.free()
    for an, val in (("Depth", 30.0), ("Foam", 0.0)):
        ca = me.color_attributes.new(an, "FLOAT_COLOR", "POINT")
        ca.data.foreach_set("color", [val, val, val, 1.0] * len(me.vertices))
    me.uv_layers.new(name="UVMap")
    me.materials.append(wm)
    ocean = bpy.data.objects.new("REF-Ocean", me)
    coll.objects.link(ocean)
    # allees dallees de la cour (porte -> citerne -> logis, branches)
    M = FM.mats()
    fb = G.FB("REF-Court_Paths")
    rng = random.Random(601)
    paths = [((-1.1, -15.7), (1.1, -15.7), (1.1, 6.9), (-1.1, 6.9)),
             ((-1.1, -1.1), (13.0, -1.1), (13.0, 0.9), (-1.1, 0.9)),
             ((-13.0, 2.0), (-1.1, 2.0), (-1.1, 0.0), (-13.0, 0.0)),
             ((-7.1, -8.0), (-4.9, -8.0), (-4.9, -1.0), (-7.1, -1.0)),
             ((10.0, -5.7), (12.0, -5.7), (12.0, -1.0), (10.0, -1.0))]
    for poly in paths:
        G.slab_paving(fb, list(poly), -0.04, M["ashlar"], rng, size=(0.4, 0.8), th=0.06, blend=(0.0, 0.0, 1.0),
                      skip=0.08)
    paths_ob = fb.finish(coll)
    paths_ob.name = "REF-Court_Paths"
    for o in (near, far, water, ocean):
        o["ref"] = "terrain du motu (Landscape dans Unreal)"
    ucx = []
    return FK.res([near, far, water, ocean, paths_ob], {}, ucx, p, export_name="REF-site_terrain",
                  info={"near_m": [nx1 - nx0, ny1 - ny0], "far_m": [356, 304]}, keep_separate=True)


MODULES = {
    "ditch": ("SM_Fort_Ditch", build_ditch),
    "site_terrain": (None, build_site_terrain),
}
