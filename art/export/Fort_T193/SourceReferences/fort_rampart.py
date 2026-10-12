"""
Fort francais : remparts (lot 'rempart').

SM_Fort_Curtain_4m (+ _Embrasure, + _Access : garde-corps ouvert sur 2,2 m au debouche des rampes / escaliers),
SM_Fort_Curtain_Corner, SM_Fort_Bastion, SM_Fort_Ramp, SM_Fort_Stair, SM_Fort_Echauguette.

Section commune (params.modules.curtain_4m.section) : escarpe talutee 1/6 de -1,32 a 2,97 (soubassement de 0,66 m en
pierres de lest), cordon en tore de pierre de taille a 2,97, parapet de 1,2 m (merlons 1,32 au-dessus du chemin de
ronde, creneaux a 0,66 en plongee de 8 deg, embrasures a genouillere 0,28), chemin de ronde de beachrock (dessus a
2,97), mur interieur talute 1/10 + garde-corps 0,495 couronne de pierre de taille, gargouille.
"""
import math
import random

from mathutils import Matrix, Vector

import fort_geo as G
import fort_kit as FK
import fort_mat as FM

V = G.V
TAU = 2 * math.pi
ZF, ZC, ZM, WALK, TOP = -1.32, 2.97, 2.91, 2.97, 4.29     # fond de fosse, cordon, dessus du massif, chemin, merlon
BATTER = 1 / 6
PLINTH = 0.66
RUB = (0.0, 0.0, 0.0, 1.0)          # moellons nus
REND = (0.0, 1.0, 0.0, 1.0)         # enduit de chaux (tombe par plaques)
BAL = (1.0, 0.0, 0.0, 1.0)          # pierres de lest
ASH = (0.0, 0.0, 0.0, 1.0)          # pierre de taille
BR = (0.0, 0.0, 1.0, 1.0)           # beachrock


def batter_off(z):
    return (ZC - z) * BATTER


# ----------------------------------------------------------------------------
# Pieces communes

def cordon(fb, M, P, closed=False, z=ZC, piece=0.62, rng=None):
    """Tore de pierre de taille (magistrale) balaye le long de la ligne exterieure P, en blocs de ~0,6 m."""
    rng = rng or random.Random(3)
    prof = [(-0.04, -0.10), (0.05, -0.095), (0.11, -0.065), (0.15, -0.015), (0.15, 0.03), (0.11, 0.065),
            (0.04, 0.085), (-0.04, 0.085)]
    pts = [Vector((p[0], p[1], z)) for p in P]
    if closed:
        pts.append(pts[0])
    acc = [0.0]
    for i in range(1, len(pts)):
        acc.append(acc[-1] + (pts[i] - pts[i - 1]).length)
    total = acc[-1]

    def at(s):
        for i in range(1, len(pts)):
            if s <= acc[i] + 1e-9:
                t = (s - acc[i - 1]) / max(acc[i] - acc[i - 1], 1e-9)
                return pts[i - 1].lerp(pts[i], t)
        return pts[-1]
    fb.blend = ASH
    fb.base = ZF
    s = 0.0
    while s < total - 1e-3:
        ln = min(piece * rng.uniform(0.8, 1.2), total - s)
        if total - (s + ln) < 0.25:
            ln = total - s
        s0, s1 = s + 0.004, s + ln - 0.004
        path = [at(s0)] + [pts[i] for i in range(1, len(pts) - 1) if s0 < acc[i] < s1] + [at(s1)]
        jz = rng.uniform(-0.006, 0.006)
        fb.sweep([p + Vector((0, 0, jz)) for p in path], prof, M["ashlar"], caps=True)
        s += ln
    fb.base = 0.0


def plinth(fb, M, P, z0=ZF, h=PLINTH, proud=0.045):
    """Soubassement de pierres de lest (bande talutee en saillie, glacis chanfreine) le long de P (exterieur a droite)."""
    fb.blend = BAL
    fb.base = z0
    b0 = batter_off(z0)
    b1 = batter_off(z0 + h)
    prof = [(-0.25, z0 - 0.05), (b0 + proud, z0 - 0.05), (b1 + proud, z0 + h), (b1 - 0.02, z0 + h + 0.07),
            (-0.25, z0 + h + 0.07)]
    pts = [Vector((p[0], p[1], 0)) for p in P]
    fb.sweep(pts, [(d, zz) for d, zz in prof], M["rubble"], caps=True)
    fb.base = 0.0


def parapet(fb, M, P, layout, thick=1.2, z0=ZM, top=TOP, rng=None, cheek=True):
    """Parapet le long de la polyligne ouverte P (exterieur a droite). layout[i] = ouvertures de l'arete i :
    ("c", t) creneau 0,6 ; ("e", t) embrasure 1,3 / 0,7 ; ("g", t0, t1) vide. Retourne les points UCX."""
    rng = rng or random.Random(5)
    Q = G.offset_poly(P, -thick, closed=False)
    ucx = []
    sill_c = WALK + 0.66
    sill_e = WALK + 0.28
    drop = thick * math.tan(math.radians(8))
    for i in range(len(P) - 1):
        o, u, n, L = G.seg_frame(P[i], P[i + 1])
        n_mark = fb.mark()
        Ai, Bi = Vector((Q[i][0], Q[i][1], 0)), Vector((Q[i + 1][0], Q[i + 1][1], 0))

        def outer(t):
            return o + u * t

        def inner(t):
            if t <= 1e-6:
                return Ai
            if t >= L - 1e-6:
                return Bi
            return o + u * t - n * thick
        ops = []
        for op in sorted(layout[i] if i < len(layout) else [], key=lambda q: q[1]):
            if op[0] == "c":
                ops.append(("c", op[1] - 0.3, op[1] + 0.3, op[1] - 0.3, op[1] + 0.3))
            elif op[0] == "e":
                ops.append(("e", op[1] - 0.65, op[1] + 0.65, op[1] - 0.35, op[1] + 0.35))
            else:
                ops.append(("g", op[1], op[2], op[1], op[2]))
        po, pi = 0.0, 0.0
        segs = ops + [("end", L, L, L, L)]
        for kind, o0, o1, i0, i1 in segs:
            if o0 - po > 0.05 or i0 - pi > 0.05:
                q = [outer(po), outer(o0), inner(i0), inner(pi)]
                fb.blend = REND
                G.block(fb, [(p.x, p.y) for p in q], z0, top, M["rubble"], round_top=0.11, segs=3)
                ucx.append([V(p.x, p.y, z) for p in q for z in (z0, top)])
            if kind in ("c", "e"):
                q = [outer(o0), outer(o1), inner(i1), inner(i0)]
                if kind == "c":
                    fb.blend = REND
                    G.block(fb, [(p.x, p.y) for p in q], z0, sill_c, M["rubble"],
                            top_z=[sill_c - drop, sill_c - drop, sill_c, sill_c])
                    ucx.append([V(p.x, p.y, z) for p in q for z in (z0, sill_c)])
                else:
                    fb.blend = REND
                    G.block(fb, [(p.x, p.y) for p in q], z0, sill_e - 0.08, M["rubble"])
                    # genouillere : dalle de beachrock usee par les affuts
                    fb.blend = BR
                    qq = [outer(o0 - 0.03) - n * 0.0 + n * 0.04, outer(o1 + 0.03) + n * 0.04,
                          inner(i1 + 0.03) - n * 0.03, inner(i0 - 0.03) - n * 0.03]
                    G.block(fb, [(p.x, p.y) for p in qq], sill_e - 0.08, sill_e, M["ashlar"],
                            top_z=[sill_e - 0.025, sill_e - 0.025, sill_e, sill_e])
                    ucx.append([V(p.x, p.y, z) for p in q for z in (z0, top)])     # bloc invisible jusqu'a 1,32
                    if cheek:
                        fb.blend = ASH
                        for a_, b_ in ((outer(o0), inner(i0)), (outer(o1), inner(i1))):
                            d = (b_ - a_)
                            Lc = d.length
                            d.normalize()
                            nn = Vector((d.y, -d.x, 0))
                            c = (a_ + b_) / 2
                            M4 = G.frame_M4(Vector((c.x, c.y, (sill_e + top - 0.12) / 2)), d, nn)
                            fb.frame_box(M4, (Lc + 0.02, 0.09, top - 0.12 - sill_e), M["ashlar"], bevel=0.008)
            po, pi = o1, i1
        fb.set_base(n_mark, lambda f, n=n: ZF if f.normal.dot(n) > 0.3 else WALK)
    return ucx


def quoins(fb, M, C, d1, d2, z0, z1, rng, mat_key="ashlar", depth=0.3, proud=0.022, batter=BATTER, ztop=ZC):
    """Chaine d'angle harpee a l'angle C (plan) : d1 direction de l'arete entrante (vers C), d2 sortante.
    Les blocs suivent le fruit de l'escarpe (decalage vers l'exterieur proportionnel a ztop - z)."""
    d1 = Vector((d1[0], d1[1], 0)).normalized()
    d2 = Vector((d2[0], d2[1], 0)).normalized()
    n1 = Vector((d1.y, -d1.x, 0))
    n2 = Vector((d2.y, -d2.x, 0))
    m = (n1 + n2).normalized()
    m = m / max(0.2, m.dot(n1))
    C = Vector((C[0], C[1], 0))
    fb.blend = ASH
    fb.base = ZF
    z = z0
    k = 0
    while z < z1 - 0.08:
        h = min(rng.uniform(0.28, 0.36), z1 - z)
        for face, (dd, nn) in enumerate(((-d1, n1), (d2, n2))):
            ln = (0.62 if (k + face) % 2 == 0 else 0.34) * rng.uniform(0.92, 1.08)
            layers = []
            for zz in (z + 0.006, z + h - 0.006):
                c = C + m * ((ztop - zz) * batter) + Vector((0, 0, zz))
                ext = proud * 1.2
                layers.append([c - dd * ext + nn * proud, c + dd * ln + nn * proud,
                               c + dd * ln - nn * depth, c - dd * ext - nn * depth])
            fb.loft(layers[0], layers[1], M[mat_key], bevel=0.012)
        z += h
        k += 1
    fb.base = 0.0


def gargoyle(fb, M, c, d, z):
    """Gargouille de pierre (canal en U) sortant de c selon d (plan), sous le garde-corps."""
    d = Vector((d[0], d[1], 0)).normalized()
    s = Vector((-d.y, d.x, 0))
    fb.blend = ASH
    c = Vector((c[0], c[1], z))
    M4 = G.frame_M4(c + d * 0.28, s, d)
    fb.frame_box(M4, (0.22, 0.62, 0.12), M["ashlar"], bevel=0.01)
    for sx in (-1, 1):
        M4 = G.frame_M4(c + d * 0.34 + s * sx * 0.085 + Vector((0, 0, 0.08)), s, d)
        fb.frame_box(M4, (0.05, 0.5, 0.06), M["ashlar"], bevel=0.008)


def guard_wall(fb, M, a, b, z0, rng, thick=0.45, h=0.495, inward=1):
    """Garde-corps (0,495 au-dessus du chemin) de a vers b ; couronnement en pierre de taille."""
    a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
    o, u, n, L = G.seg_frame(a, b)
    off = -n * inward * thick
    q = [a, b, b + off, a + off]
    fb.blend = RUB
    n_mark = fb.mark()
    G.block(fb, [(p.x, p.y) for p in q], z0 - 0.06, WALK + h - 0.07, M["rubble"])
    fb.blend = ASH
    t = 0.0
    while t < L - 0.02:
        ln = min(rng.uniform(0.45, 0.7), L - t)
        c = o + u * (t + ln / 2) + off / 2
        fb.box((c.x, c.y, WALK + h - 0.035), (ln - 0.008, thick + 0.05, 0.07), M["ashlar"],
               R=Matrix.Rotation(math.atan2(u.y, u.x), 3, "Z"), bevel=0.012)
        t += ln
    cour = -n * inward
    fb.set_base(n_mark, lambda f: 0.0 if f.normal.dot(cour) < -0.3 else WALK)
    return [V(p.x, p.y, z) for p in q for z in (z0, WALK + h)]


# ----------------------------------------------------------------------------
# Courtine 4 m

def build_curtain(coll, p=None, variant="plain"):
    key = {"plain": "curtain_4m", "emb": "curtain_4m_embrasure", "access": "curtain_4m_access"}[variant]
    base = FK.mod("curtain_4m")
    p = {**base, **FK.mod(key), **(p or {})} if key != "curtain_4m" else {**base, **(p or {})}
    M = FM.mats()
    rng = random.Random({"plain": 11, "emb": 12, "access": 13}[variant])
    fb = G.FB("Curtain")
    hx = 2.0
    yin = 4.2
    yfoot_in = yin + 0.297
    # massif : escarpe talutee (enduite), dessus, mur interieur talute 1/10 (moellons nus)
    sec = [(-0.715, ZF), (0.0, ZC), (0.0, ZM), (yin, ZM), (yfoot_in, 0.0), (yfoot_in, ZF)]
    n0 = fb.mark()
    fb.blend = REND
    fb.loft([V(-hx, y, z) for y, z in sec], [V(hx, y, z) for y, z in sec], M["rubble"])
    fb.paint(n0, lambda f: RUB if f.normal.y > 0.3 else None)
    fb.set_base(n0, lambda f: 0.0 if f.normal.y > 0.3 else ZF)
    plinth(fb, M, [(-hx, 0), (hx, 0)])
    cordon(fb, M, [(-hx, 0), (hx, 0)], rng=rng)
    # parapet
    if variant == "emb":
        lay = [[("e", 2.0)]]
    else:
        lay = [[("c", 1.0), ("c", 3.0)]]
    ucx_par = parapet(fb, M, [(-hx, 0), (hx, 0)], lay, rng=rng)
    # chemin de ronde
    gun = variant == "emb"
    gy0 = 3.75 if variant != "access" else yin
    holes = (lambda c: abs(c.x) < 1.25 and 1.2 < c.y < 3.9) if gun else None
    G.slab_paving(fb, [(-hx, 1.2), (hx, 1.2), (hx, gy0), (-hx, gy0)], ZM, M["ashlar"], rng, holes=holes,
                  blend=BR, angle=0.0)
    ucx = [("massif", None, [V(x, y, z) for x in (-hx, hx) for y, z in sec])]
    ucx += [("parapet", None, pts) for pts in ucx_par]
    # garde-corps (ouvert au debouche des rampes / escaliers pour la variante access)
    if variant == "access":
        for a, b in (((-hx, yin), (-1.1, yin)), ((1.1, yin), (hx, yin))):
            ucx.append(("guard", None, guard_wall(fb, M, a, b, ZM, rng, inward=-1)))
    else:
        ucx.append(("guard", None, guard_wall(fb, M, (-hx, yin), (hx, yin), ZM, rng, inward=-1)))
        gargoyle(fb, M, (1.5, yin - 0.05), (0, 1), WALK - 0.02)
    info = {"variant": variant}
    if gun:
        # plate-forme de tir : madriers goudronnes sur lambourdes, anneaux de brague
        fb.blend = RUB
        for k in range(10):
            x = -1.2 + 0.12 + k * 0.24
            fb.wood((x + rng.uniform(-0.005, 0.005), 2.55, WALK - 0.03), (0.225, 2.6, 0.06), M["oak"], bevel=0.008,
                    long_u=False)
        for sx in (-0.55, 0.55):
            c = V(sx, 1.2 + 0.06, WALK + 0.35)
            ring = [c + V(0.055 * math.cos(TAU * i / 12), 0.03, 0.055 * math.sin(TAU * i / 12) - 0.055)
                    for i in range(12)]
            fb.tube(ring, 0.011, M["iron"], segs=6, closed=True)
            fb.cyl(c + V(0, -0.08, 0), c + V(0, 0.05, 0), 0.02, M["iron"], segs=8)
        info["gun_platform_m"] = [2.4, 2.6]
    ob = fb.finish(coll)
    socks = FK.sockets_of(base["sockets"])
    if variant == "emb":
        socks.update(FK.sockets_of(FK.mod("curtain_4m_embrasure")["sockets"]))
        socks.pop("Crenel_1", None)
        socks.pop("Crenel_2", None)
        socks["Embrasure"] = ("root", V(0, 1.0, WALK + 0.28))
    return FK.res([ob], socks, ucx, p, info=info)


def build_curtain_emb(coll, p=None):
    return build_curtain(coll, p, "emb")


def build_curtain_access(coll, p=None):
    return build_curtain(coll, p, "access")


# ----------------------------------------------------------------------------
# Angle saillant (piece de kit)

def build_curtain_corner(coll, p=None):
    p = {**FK.mod("curtain_corner"), **(p or {})}
    M = FM.mats()
    rng = random.Random(21)
    fb = G.FB("CurtainCorner")
    S = 4.2
    top = [(0, 0), (S, 0), (S, S), (0, S)]
    b = batter_off(ZF)
    bot = [(-b, -b), (S, -b), (S, S), (-b, S)]
    fb.blend = REND
    n0 = fb.mark()
    fb.loft([V(x, y, ZF) for x, y in bot], [V(x, y, ZM) for x, y in top], M["rubble"])
    fb.paint(n0, lambda f: RUB if (f.normal.x > 0.5 or f.normal.y > 0.5) else None)
    fb.set_base(n0, lambda f: ZF)
    P = [(0, S), (0, 0), (S, 0)]
    plinth(fb, M, P)
    cordon(fb, M, P, rng=rng)
    quoins(fb, M, (0, 0), (0, -1), (1, 0), ZF + PLINTH + 0.05, ZC - 0.11, rng)
    ucx_par = parapet(fb, M, P, [[("c", 1.0)], [("c", 3.2)]], rng=rng)
    G.slab_paving(fb, [(1.2, 1.2), (S, 1.2), (S, S), (1.2, S)], ZM, M["ashlar"], rng, blend=BR)
    ob = fb.finish(coll)
    ucx = [("massif", None, [V(x, y, ZF) for x, y in bot] + [V(x, y, ZM) for x, y in top])]
    ucx += [("parapet", None, q) for q in ucx_par]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, socket_rot={"Echauguette": (0, 0, 225)})


# ----------------------------------------------------------------------------
# Bastion

def build_bastion_vault(coll, p=None):
    return build_bastion(coll, p, vault=True)


def build_bastion(coll, p=None, vault=False):
    p = {**FK.mod("bastion"), **(FK.mod("bastion_vault") if vault else {}), **(p or {})}
    M = FM.mats()
    rng = random.Random(31)
    fb = G.FB("Bastion")
    tr = p["trace_local_m"]
    FA0, FA1 = tr["flank_a"]
    _, SAL = tr["face_a"]
    FB0, FB1 = tr["flank_b"]
    yin = -4.2
    fo = 0.715
    fin = 0.297 * (ZC - ZF) / ZC       # fruit 1/10 du mur interieur prolonge jusqu'a -1,32
    # contour du massif (anti-horaire) et son pied
    T = [(-4.2, -4.2), (-4.2, -6.2), (0.0, -6.2), tuple(FB0), tuple(SAL), tuple(FA1), (-6.2, 0.0), (-6.2, -4.2)]
    out = lambda a, b, d: [(x + nn.x * d, y + nn.y * d) for (x, y) in (a, b) for nn in [G.seg_frame(a, b)[2]]]  # noqa
    # lignes decalees des aretes exterieures
    e_fb = out((0.0, -6.2), FB0, fo)
    e_b = out(FB0, SAL, fo)
    e_a = out(SAL, FA1, fo)
    e_fa = out(FA1, (-6.2, 0.0), fo)
    e_in1 = out((-6.2, -4.2), (-4.2, -4.2), fin)
    e_in2 = out((-4.2, -4.2), (-4.2, -6.2), fin)
    curtain_n = ((0.0 + fo, 0), (0.0 + fo, 1))          # pied de la courtine est (x = +0,715)
    curtain_w = ((0, fo), (1, fo))                      # pied de la courtine nord (y = +0,715)
    B = [G.line_x(*e_in1, *e_in2),
         G.line_x(*e_in2, (-4.2, -6.2), (0.0, -6.2)),
         G.line_x(*e_fb, *curtain_n),
         G.line_x(*e_fb, *e_b),
         G.line_x(*e_b, *e_a),
         G.line_x(*e_a, *e_fa),
         G.line_x(*e_fa, *curtain_w),
         G.line_x((-6.2, -4.2), (-6.2, 0.0), *e_in1)]
    n0 = fb.mark()
    fb.blend = REND
    if not vault:
        fb.loft([V(x, y, ZF) for x, y in B], [V(x, y, ZM) for x, y in T], M["rubble"])
    else:
        # coque : escarpes, murs de gorge et dessus seulement (vide interieur pour le couloir et la casemate)
        door = p["vault_door"]
        for i in range(len(T)):
            j = (i + 1) % len(T)
            if i in (1, 6):
                continue
            if i == 7:
                _door_wall(fb, M, B[7], B[0], T[7], T[0], door["x_local"], door["height_m"])
                continue
            fb.poly_bm([V(*B[i], ZF), V(*B[j], ZF), V(*T[j], ZM), V(*T[i], ZM)], [(0, 1, 2, 3)], M["rubble"],
                       recalc=False)
        fb.poly_bm([V(x, y, ZM) for x, y in T], [tuple(range(len(T)))], M["rubble"], recalc=False)
    inner_f = lambda f: (f.normal.y < -0.6 and f.calc_center_median().x < -3.0) or \
        (f.normal.x < -0.6 and f.calc_center_median().y < -3.0)                       # noqa: E731
    fb.paint(n0, lambda f: RUB if inner_f(f) else None)
    fb.set_base(n0, lambda f: 0.0 if inner_f(f) else ZF)
    # exterieur a droite du chemin : flanc B -> face B -> saillant -> face A -> flanc A
    Pout = [(0.0, -6.2), tuple(FB0), tuple(SAL), tuple(FA1), (-6.2, 0.0)]
    plinth(fb, M, Pout)
    cordon(fb, M, Pout, rng=rng)
    # chaines d'angle : epaules et saillant
    for i in (1, 2, 3):
        a, c, b = Vector((*Pout[i - 1], 0)), Vector((*Pout[i], 0)), Vector((*Pout[i + 1], 0))
        quoins(fb, M, Pout[i], c - a, b - c, ZF + PLINTH + 0.05, ZC - 0.11, rng)
    # parapet : extremites prolongees de 1,2 m sur l'angle rentrant (raccord avec le parapet des courtines)
    Ppar = [(-1.2, -6.2), tuple(FB0), tuple(SAL), tuple(FA1), (-6.2, -1.2)]
    Ls = [(Vector((*Ppar[i + 1], 0)) - Vector((*Ppar[i], 0))).length for i in range(4)]
    emb = p["embrasures"]

    def crenels(t0, t1):
        span = t1 - t0
        n = max(0, int((span - 1.1) / 2.0))
        return [("c", t0 + span / 2 + (k - (n - 1) / 2) * 2.0) for k in range(n)]
    tip = 1.15
    # abscisses des embrasures sur chaque arete (fractions de params : flanc B / faces depuis le saillant ...)
    emb_t = [[1.2 + 5.0 * (1 - f) for f in emb["flank_b"]],
             [Ls[1] * (1 - f) for f in emb["face_b"]],
             [Ls[2] * (1 - f) for f in emb["face_a"]],
             [5.0 * (1 - f) for f in emb["flank_a"]]]
    lay = []
    for i in range(4):
        L = Ls[i]
        es = sorted(emb_t[i])
        ops = [("e", t) for t in es]
        bounds = [0.0] + [x for t in es for x in (t - 0.65, t + 0.65)] + [L]
        if i == 0:
            bounds[0] = 1.2
        if i == 3:
            bounds[-1] = L - 1.2
        if i == 1:
            ops.append(("g", L - tip, L))
            bounds[-1] = L - tip
        if i == 2:
            ops.append(("g", 0.0, tip))
            bounds[0] = tip
        for k in range(0, len(bounds), 2):
            ops += crenels(bounds[k], bounds[k + 1])
        lay.append(ops)
    ucx_par = parapet(fb, M, Ppar, lay, rng=rng)
    # terre-plein : dalles
    Q = G.offset_poly(Ppar, -1.2, closed=False)
    tp = [(-6.2, -1.2)] + [tuple(q) for q in reversed(Q)] + [(-1.2, -6.2), (-3.75, -6.2), (-3.75, -3.75),
                                                              (-6.2, -3.75)]
    sal_c = Vector((7.85, 7.85, 0))
    G.slab_paving(fb, tp, ZM, M["ashlar"], rng, blend=BR, angle=math.radians(45), size=(0.5, 1.0), skip=0.07,
                  holes=lambda c: (Vector((c.x, c.y, 0)) - sal_c).length < 1.25)
    # sol de sable compacte sous les dalles manquantes (le massif est en beachrock)
    fb.blend = BR
    n0 = fb.mark()
    fb.prism(tp, ZM - 0.02, ZM + 0.012, M["ashlar"])
    # pointe du saillant (sous l'echauguette, parapet interrompu) : meme lit de sable
    oA, uA, nA, LA = G.seg_frame(Ppar[1], Ppar[2])
    oB, uB, nB, LB = G.seg_frame(Ppar[2], Ppar[3])
    pa = oA + uA * (LA - tip - 0.05)
    pb = oB + uB * (tip + 0.05)
    fb.prism([tuple(Q[2]), (pa.x, pa.y), tuple(SAL), (pb.x, pb.y)], ZM - 0.02, ZM + 0.012, M["ashlar"])
    # garde-corps de la gorge
    if vault:
        ucx = _vault_ucx(T, p["vault_door"])
    else:
        ucx = []
    ucx += [] if vault else [("gorge", None, [V(x, y, z) for x, y in ((-6.2, -4.2), (-4.2, -4.2), (-4.2, -6.2), (0, -6.2),
                                                       (0, 0), (-6.2, 0)) for z in (ZF, ZM)]),
           ("demi_A", None, [V(x, y, z) for x, y in ((-6.2, 0), tuple(FA1), tuple(SAL), (0, 0)) for z in (ZF, ZM)]),
           ("demi_B", None, [V(x, y, z) for x, y in ((0, 0), tuple(SAL), tuple(FB0), (0, -6.2)) for z in (ZF, ZM)])]
    ucx.append(("guard_n", None, guard_wall(fb, M, (-6.2, -4.2), (-4.2, -4.2), ZM, rng, inward=-1)))
    ucx.append(("guard_e", None, guard_wall(fb, M, (-4.2, -4.2 + 0.45), (-4.2, -6.2), ZM, rng, inward=-1)))
    gargoyle(fb, M, (-5.2, -4.25), (0, -1), WALK - 0.02)
    gargoyle(fb, M, (-4.25, -5.2), (-1, 0), WALK - 0.02)
    ucx += [("parapet", None, q) for q in ucx_par]
    ob = fb.finish(coll)
    # sockets : embrasures (plates-formes), echauguette, tour, pavillon, boulets
    socks = {}
    rot = {}
    names = (("Gun_FlankB", 0, sorted(emb_t[0])), ("Gun_FaceB", 1, sorted(emb_t[1])),
             ("Gun_FaceA", 2, sorted(emb_t[2])), ("Gun_FlankA", 3, sorted(emb_t[3])))
    for base, i, ts in names:
        o, u, n, L = G.seg_frame(Ppar[i], Ppar[i + 1])
        for k, t in enumerate(ts):
            nm = base if len(ts) == 1 else f"{base}_{k + 1}"
            c = o + u * t - n * 2.35
            socks[nm] = ("root", V(c.x, c.y, WALK))
            rot[nm] = (0, 0, math.degrees(math.atan2(n.y, n.x)) - 90)
    es = p["echauguette_socket"]
    socks["Echauguette"] = ("root", Vector(es["pos"]))
    rot["Echauguette"] = (0, 0, es["rot_z_deg"])
    for k in ("Tower", "Flag", "ShotPile_1", "ShotPile_2"):
        socks[k] = ("root", Vector(p["sockets"][k]))
    info = {"face_m": round(Ls[1], 3), "flank_m": 5.0, "embrasures": 6,
            "salient_deg": round(math.degrees((Vector((*FA1, 0)) - Vector((*SAL, 0))).angle(
                Vector((*FB0, 0)) - Vector((*SAL, 0)))), 1)}
    return FK.res([ob], socks, ucx, p, socket_rot=rot, info=info)


def _door_wall(fb, M, b0, b1, t0, t1, xr, hd):
    """Mur de gorge talute (de b0/t0 a b1/t1, normale -Y du bastion NE) perce d'une porte x in xr, z in [0, hd]."""
    def at(x, z):
        v = (z - ZF) / (ZM - ZF)
        y = b0[1] + (t0[1] - b0[1]) * v
        xr_ = b1[0] + (t1[0] - b1[0]) * v
        return V(min(x, xr_), y, z)

    def xr_at(z):
        v = (z - ZF) / (ZM - ZF)
        return b1[0] + (t1[0] - b1[0]) * v
    x0 = t0[0]
    pieces = [((x0, xr[0]), (ZF, ZM)), ((xr[1], None), (ZF, ZM)), ((xr[0], xr[1]), (ZF, 0.0)),
              ((xr[0], xr[1]), (hd, ZM))]
    for (xa, xb), (za, zb) in pieces:
        q = [at(xa, za), at(xb if xb is not None else xr_at(za), za), at(xb if xb is not None else xr_at(zb), zb),
             at(xa, zb)]
        fb.poly_bm(q, [(0, 1, 2, 3)], M["rubble"], recalc=False)
    # tableau de la porte (epaisseur 0,6)
    fb.blend = ASH
    dy = V(0, 0.6, 0)
    a, b_ = at(xr[0], 0.0), at(xr[0], hd)
    fb.poly_bm([a, a + dy, b_ + dy, b_], [(0, 1, 2, 3)], M["ashlar"], recalc=False)
    a, b_ = at(xr[1], 0.0), at(xr[1], hd)
    fb.poly_bm([a, b_, b_ + dy, a + dy], [(0, 1, 2, 3)], M["ashlar"], recalc=False)
    a, b_ = at(xr[0], hd), at(xr[1], hd)
    fb.poly_bm([a, a + dy, b_ + dy, b_], [(0, 1, 2, 3)], M["ashlar"], recalc=False)
    fb.blend = REND


def _vault_ucx(T, door):
    """Collisions de la coque : dalle du terre-plein au-dessus des voutes, murs epais le long des faces, gorge."""
    zt0 = door["ucx_top_from_z"]
    out = []
    for nm, poly in (("terreplein_gorge", [(-6.2, -4.2), (-4.2, -4.2), (-4.2, -6.2), (0, -6.2), (0, 0), (-6.2, 0)]),
                     ("terreplein_A", [(-6.2, 0), tuple(T[5]), tuple(T[4]), (0, 0)]),
                     ("terreplein_B", [(0, 0), tuple(T[4]), tuple(T[3]), (0, -6.2)])):
        out.append((nm, None, [V(x, y, z) for x, y in poly for z in (zt0, ZM)]))
    for i in (2, 3, 4, 5):
        a, b = T[i], T[(i + 1) % len(T)]
        o, u, n, L = G.seg_frame(a, b)
        q = [o, o + u * L, o + u * L - n * 2.0, o - n * 2.0]
        out.append((f"escarpe_{i}", None, [V(pp.x, pp.y, z) for pp in q for z in (ZF, zt0)] +
                    [V(pp.x + n.x * 0.715, pp.y + n.y * 0.715, ZF) for pp in q[:2]]))
    out.append(("gorge_e", None, [V(x, y, z) for x in (-4.2, -3.6) for y in (-6.2, -4.2) for z in (0, zt0)]))
    xr = door["x_local"]
    for nm, (xa, xb), (za, zb) in (("gorge_n1", (-6.2, xr[0]), (0, zt0)), ("gorge_n2", (xr[1], -4.2), (0, zt0)),
                                   ("linteau", xr, (door["height_m"], zt0))):
        out.append((nm, None, [V(x, y, z) for x in (xa, xb) for y in (-4.2, -3.6) for z in (za, zb)]))
    return out


# ----------------------------------------------------------------------------
# Rampe a canons et escalier

def build_ramp(coll, p=None):
    p = {**FK.mod("ramp"), **(p or {})}
    M = FM.mats()
    rng = random.Random(41)
    fb = G.FB("Ramp")
    run, land, W, rise = p["run_m"], p["top_landing_m"], p["width_m"], p["rise_m"]
    L = run + land
    hw = W / 2
    sec = [(0.0, 0.0), (0.0, -0.02), (run, rise - 0.06), (L, rise - 0.06), (L, 0.0)]
    fb.blend = REND
    fb.loft([V(-hw, y, z) for y, z in sec], [V(hw + 0.3, y, z) for y, z in sec], M["rubble"])
    # surface : paves de corail sur chant (beachrock) + 2 bandes de roulement en madriers
    fb.blend = BR
    surf = [(0.0, 0.0), (run, rise), (L, rise)]
    for k in range(len(surf) - 1):
        (y0, z0), (y1, z1) = surf[k], surf[k + 1]
        fb.loft([V(-hw, y0, z0 - 0.06), V(hw, y0, z0 - 0.06), V(hw, y1, z1 - 0.06), V(-hw, y1, z1 - 0.06)],
                [V(-hw, y0, z0 - 0.002), V(hw, y0, z0 - 0.002), V(hw, y1, z1 - 0.002), V(-hw, y1, z1 - 0.002)],
                M["ashlar"])
    ang = math.atan2(rise, run)
    for x in (-0.45, 0.45):
        c = V(x, run / 2, rise / 2 + 0.012)
        fb.wood(c, (0.26, run / math.cos(ang) - 0.1, 0.05), M["oak"], R=Matrix.Rotation(ang, 3, "X"), bevel=0.01,
                long_u=False)
    # garde-corps cote cour (+X) suivant la pente, puis au bout du palier
    gh = 0.495
    fb.blend = RUB
    gx0, gx1 = hw, hw + 0.3
    y_start = 2.0
    zs = lambda y: min(rise, y / run * rise)                                          # noqa: E731
    pts = [y_start, run * 0.5, run, L]
    for k in range(len(pts) - 1):
        ya, yb = pts[k], pts[k + 1]
        fb.loft([V(gx0, ya, zs(ya) - 0.3), V(gx1, ya, zs(ya) - 0.3), V(gx1, yb, zs(yb) - 0.3), V(gx0, yb, zs(yb) - 0.3)],
                [V(gx0, ya, zs(ya) + gh), V(gx1, ya, zs(ya) + gh), V(gx1, yb, zs(yb) + gh), V(gx0, yb, zs(yb) + gh)],
                M["rubble"])
    fb.blend = ASH
    for k in range(len(pts) - 1):
        ya, yb = pts[k], pts[k + 1]
        fb.loft([V(gx0 - 0.03, ya, zs(ya) + gh - 0.01), V(gx1 + 0.03, ya, zs(ya) + gh - 0.01),
                 V(gx1 + 0.03, yb, zs(yb) + gh - 0.01), V(gx0 - 0.03, yb, zs(yb) + gh - 0.01)],
                [V(gx0 - 0.03, ya, zs(ya) + gh + 0.06), V(gx1 + 0.03, ya, zs(ya) + gh + 0.06),
                 V(gx1 + 0.03, yb, zs(yb) + gh + 0.06), V(gx0 - 0.03, yb, zs(yb) + gh + 0.06)], M["ashlar"], bevel=0.01)
    fb.blend = RUB
    G.block(fb, [(-hw, L), (gx1, L), (gx1, L - 0.3), (-hw, L - 0.3)], rise - 0.06, rise + gh, M["rubble"])
    fb.blend = ASH
    G.block(fb, [(-hw - 0.02, L + 0.03), (gx1 + 0.03, L + 0.03), (gx1 + 0.03, L - 0.33), (-hw - 0.02, L - 0.33)],
            rise + gh - 0.01, rise + gh + 0.06, M["ashlar"])
    ob = fb.finish(coll)
    ucx = [("ramp", None, [V(x, y, z) for x in (-hw, hw) for y, z in ((0, 0), (run, rise), (run, 0))]),
           ("landing", None, [V(x, y, z) for x in (-hw, hw) for y in (run, L) for z in (0, rise)]),
           ("guard", None, [V(x, y, z) for x in (gx0, gx1) for y in (y_start, run) for z in (0, rise + gh)]),
           ("guard_top", None, [V(x, y, z) for x in (gx0, gx1) for y in (run, L) for z in (0, rise + gh)]),
           ("guard_end", None, [V(x, y, z) for x in (-hw, gx1) for y in (L - 0.3, L) for z in (0, rise + gh)])]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"slope_deg": round(math.degrees(ang), 2)})


def build_stair(coll, p=None):
    p = {**FK.mod("stair"), **(p or {})}
    M = FM.mats()
    rng = random.Random(42)
    fb = G.FB("Stair")
    n, rise, tread, W = p["risers"], p["rise_m"], p["tread_m"], p["width_m"]
    land_after, land = p["landing_after"], p["landing_m"]
    hw = W / 2
    ys = []
    y = 0.0
    for r in range(1, n + 1):
        ys.append(y)
        y += land if r == land_after else tread
    top_land = 1.2
    y_end = ys[-1] + top_land
    # massif en gradins (moellons) + marches de beachrock
    sec = [(0.0, 0.0)]
    for r in range(1, n + 1):
        z = r * rise - 0.05
        sec += [(ys[r - 1], z)]
        y_next = ys[r] if r < n else y_end
        sec += [(y_next, z)]
    sec += [(y_end, 0.0)]
    fb.blend = REND
    fb.loft([V(-hw, yy, zz) for yy, zz in sec], [V(hw, yy, zz) for yy, zz in sec], M["rubble"])
    fb.blend = BR
    for r in range(1, n + 1):
        y0 = ys[r - 1] - 0.025
        y1 = (ys[r] if r < n else y_end)
        j = rng.uniform(-0.004, 0.004)
        fb.box(((0 + j), (y0 + y1) / 2, r * rise - 0.025), (W - 0.01, y1 - y0, 0.05), M["ashlar"],
               R=Matrix.Rotation(j, 3, "Z"), bevel=0.012)
    # garde-corps de fer forge cote vide (+X) : montants, main courante a 0,66
    fb.blend = RUB
    hx = hw - 0.05
    posts = []
    for r in range(2, n + 1, 3):
        posts.append((ys[r - 1] + 0.14, r * rise))
    posts.append((y_end - 0.08, n * rise))
    for yy, zz in posts:
        fb.cyl(V(hx, yy, zz - 0.02), V(hx, yy, zz + 0.66), 0.014, M["iron"], segs=6)
    rail = [V(hx, yy, zz + 0.66) for yy, zz in posts]
    fb.tube(rail, 0.017, M["iron"], segs=8)
    ob = fb.finish(coll)
    pts = []
    ucx = [("flight1", None, [V(x, y_, z) for x in (-hw, hw) for y_, z in ((0, 0), (ys[land_after - 1], 0),
                                                                          (ys[land_after - 1], land_after * rise))]),
           ("landing", None, [V(x, y_, z) for x in (-hw, hw) for y_ in (ys[land_after - 1], ys[land_after])
                              for z in (0, land_after * rise)]),
           ("flight2", None, [V(x, y_, z) for x in (-hw, hw) for y_, z in ((ys[land_after], 0), (ys[-1], 0),
                                                                          (ys[-1], n * rise),
                                                                          (ys[land_after], land_after * rise))]),
           ("top", None, [V(x, y_, z) for x in (-hw, hw) for y_ in (ys[-1], y_end) for z in (0, n * rise)])]
    del pts
    socks = FK.sockets_of(p["sockets"])
    return FK.res([ob], socks, ucx, p, info={"run_m": round(ys[-1], 3), "top_landing_m": top_land})


# ----------------------------------------------------------------------------
# Echauguette

def build_echauguette(coll, p=None):
    p = {**FK.mod("echauguette"), **(p or {})}
    M = FM.mats()
    rng = random.Random(51)
    fb = G.FB("Echauguette")
    ri = p["inner_diameter_m"] / 2
    ro = p["outer_diameter_m"] / 2
    hc = p["cornice_above_floor_m"]
    dome = p["dome_rise_m"]
    dw, dh = p["door_m"]
    segs = 24
    fb.blend = ASH
    fb.base = -(WALK - ZF)
    # cul-de-lampe : 4 assises en retrait + culot
    rads = [ro, ro * 0.82, ro * 0.64, ro * 0.46]
    z = 0.0
    for k, r in enumerate(rads):
        h = 0.3
        prof = [(0.0, z - h), (r * 0.92, z - h), (r, z - h + 0.05), (r, z)] if k else \
               [(0.0, z - h), (r * 0.92, z - h), (r + 0.04, z - 0.06), (r + 0.04, z)]
        tmp = _revolve(prof, segs)
        fb.add_bm(tmp, M["ashlar"], smooth=False)
        z -= h
    fb.sphere((0, 0, z - 0.05), 0.12, M["ashlar"], u=12, v=6)
    # plancher (dalle) et fut perce : porte vers +Y, 3 meurtrieres (-Y, +X, -X)
    fb.blend = BR
    fb.add_bm(_revolve([(0.0, -0.02), (ri + 0.05, -0.02), (ri + 0.05, 0.0), (0.0, 0.0)], segs), M["ashlar"], False)
    fb.blend = ASH
    door_a = math.atan2(dw / 2, ri)
    loops = [(-math.pi / 2, 0.12), (0.0, 0.12), (math.pi, 0.12)]
    for i in range(segs):
        a0, a1 = TAU * i / segs, TAU * (i + 1) / segs
        am = (a0 + a1) / 2
        dd = math.atan2(math.sin(am - math.pi / 2), math.cos(am - math.pi / 2))
        spans = [(0.0, hc)]
        if abs(dd) < door_a + 0.02:
            spans = [(dh, hc)]
        for la, _w in loops:
            dl = math.atan2(math.sin(am - la), math.cos(am - la))
            if abs(dl) < TAU / segs * 0.55:
                spans = [(0.0, 0.66), (0.66 + 0.33, hc)]
        for z0, z1 in spans:
            q = [V(ro * math.cos(a0), ro * math.sin(a0), 0), V(ro * math.cos(a1), ro * math.sin(a1), 0),
                 V(ri * math.cos(a1), ri * math.sin(a1), 0), V(ri * math.cos(a0), ri * math.sin(a0), 0)]
            fb.loft([v + V(0, 0, z0) for v in q], [v + V(0, 0, z1) for v in q], M["ashlar"])
    # linteau de porte, corniche, dome, pomme
    fb.add_bm(_revolve([(ro - 0.02, hc), (ro + 0.08, hc + 0.04), (ro + 0.1, hc + 0.12), (ro - 0.02, hc + 0.14),
                        (0.0, hc + 0.14)], segs), M["ashlar"], False)
    prof = [(ro - 0.02, hc + 0.14)]
    for k in range(1, 9):
        a = k / 8 * math.pi / 2
        prof.append(((ro - 0.02) * math.cos(a), hc + 0.14 + dome * math.sin(a)))
    fb.add_bm(_revolve(prof, segs), M["ashlar"], True)
    fb.add_bm(_revolve([(0.0, hc + 0.14 + dome - 0.02), (0.05, hc + 0.14 + dome), (0.05, hc + 0.2 + dome)], 10),
              M["ashlar"], True)
    fb.sphere((0, 0, hc + 0.3 + dome), 0.09, M["ashlar"], u=12, v=7)
    ob = fb.finish(coll)
    ucx = [("fut", None, [V(ro * math.cos(TAU * k / 12), ro * math.sin(TAU * k / 12), z) for k in range(12)
                          for z in (-1.2, hc + 0.14)]),
           ("dome", None, [V(ro * math.cos(TAU * k / 12), ro * math.sin(TAU * k / 12), hc + 0.14) for k in range(12)]
            + [V(0, 0, hc + 0.14 + dome + 0.3)])]
    return FK.res([ob], FK.sockets_of(p["sockets"]), ucx, p, info={"door_m": [dw, dh], "clear_m": hc})


def _revolve(prof, segs):
    import bmesh
    tmp = bmesh.new()
    rings = []
    for r, z in prof:
        if r < 1e-6:
            rings.append([tmp.verts.new((0, 0, z))])
        else:
            rings.append([tmp.verts.new((r * math.cos(TAU * i / segs), r * math.sin(TAU * i / segs), z))
                          for i in range(segs)])
    for k in range(len(rings) - 1):
        a, b = rings[k], rings[k + 1]
        for i in range(segs):
            j = (i + 1) % segs
            try:
                if len(a) == 1 and len(b) == 1:
                    continue
                if len(a) == 1:
                    tmp.faces.new((a[0], b[i], b[j]))
                elif len(b) == 1:
                    tmp.faces.new((a[i], a[j], b[0]))
                else:
                    tmp.faces.new((a[i], a[j], b[j], b[i]))
            except ValueError:
                pass
    bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
    return tmp


MODULES = {
    "curtain_4m": ("SM_Fort_Curtain_4m", lambda c, p=None: build_curtain(c, p, "plain")),
    "curtain_4m_embrasure": ("SM_Fort_Curtain_4m_Embrasure", build_curtain_emb),
    "curtain_4m_access": ("SM_Fort_Curtain_4m_Access", build_curtain_access),
    "curtain_corner": ("SM_Fort_Curtain_Corner", build_curtain_corner),
    "bastion": ("SM_Fort_Bastion", build_bastion),
    "bastion_vault": ("SM_Fort_Bastion_Vault", build_bastion_vault),
    "ramp": ("SM_Fort_Ramp", build_ramp),
    "stair": ("SM_Fort_Stair", build_stair),
    "echauguette": ("SM_Fort_Echauguette", build_echauguette),
}
