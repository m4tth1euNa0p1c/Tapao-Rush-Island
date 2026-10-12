"""
Fort francais : geometrie (lot de pieces multi-materiaux avec UV en metres et couleur de coin `Blend`).

FB etend camp_kit.Batch :
  - attribut de coin FLOAT_COLOR `Blend` (couches des maconneries, voir fort_mat) fixe par `fb.blend` avant chaque
    ajout ;
  - UV « monde module » (world_uv=True) : projection cubique continue dans le repere du module (les blocs voisins
    raccordent leur texture) ; sinon UV locales de camp_kit (planches : fil le long de la piece).
Helpers : prismes de plan, lofts entre polygones (escarpes talutees), balayages de profils avec onglets (cordons,
corniches), murs percés d'ouvertures, escaliers, toits a croupes / a pignons avec tuiles de faitage, dallages,
barreaux, clous, decalage de polygones.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import camp_kit as CK

TAU = 2 * math.pi
Z = Vector((0, 0, 1))


def V(*a):
    return Vector(a)


class FB(CK.Batch):
    def __init__(self, name, uv_size=1.0, world_uv=True):
        super().__init__(name, uv_size)
        self.col = self.bm.loops.layers.float_color.new("Blend")
        self.blend = (0.0, 0.0, 0.0)
        self.base = 0.0              # niveau du sol au pied de la piece (Z local) -> canal A = base / 10 + 0,5
        self.world_uv = world_uv
        self.force_local = False

    def _c4(self):
        b = self.blend
        return (b[0], b[1], b[2], self.base / 10.0 + 0.5)

    def _merge(self, tmp, mat, smooth, world=None):
        mi = self._mi(mat)
        me = bpy.data.meshes.new("_fb_tmp")
        tmp.to_mesh(me)
        tmp.free()
        n0 = len(self.bm.faces)
        self.bm.from_mesh(me)
        bpy.data.meshes.remove(me)
        self.bm.faces.ensure_lookup_table()
        w = self.world_uv if world is None else world
        if self.force_local:
            w = False
        s = self.uv_size
        bl = self._c4()
        for i in range(n0, len(self.bm.faces)):
            f = self.bm.faces[i]
            f.material_index = mi
            f.smooth = smooth
            if w:
                n = f.normal
                ax = max(range(3), key=lambda k: abs(n[k]))
                o = [k for k in range(3) if k != ax]
                if ax == 2:
                    for lp in f.loops:
                        p = lp.vert.co
                        lp[self.uv].uv = (p.x / s, p.y / s)
                else:
                    # face verticale : u horizontal le long de la face, v = z
                    hx = o[0]
                    for lp in f.loops:
                        p = lp.vert.co
                        lp[self.uv].uv = (p[hx] / s, p.z / s)
            for lp in f.loops:
                lp[self.col] = bl

    # ------------------------------------------------------------------ primitives

    def local(self, fn, *a, **kw):
        """Appelle fn avec UV locales (planches, poutres : fil du bois)."""
        old = self.force_local
        self.force_local = True
        try:
            return fn(*a, **kw)
        finally:
            self.force_local = old

    def wood(self, c, size, mat, R=None, bevel=0.006, long_u=True, rng=None, jitter=0.0):
        return self.local(self.box, c, size, mat, R=R, bevel=bevel, long_u=long_u, rng=rng, jitter=jitter)

    def poly_bm(self, verts, faces, mat, smooth=False, world=None, recalc=True):
        tmp = bmesh.new()
        vs = [tmp.verts.new(v) for v in verts]
        for f in faces:
            try:
                tmp.faces.new([vs[i] for i in f])
            except ValueError:
                pass
        if recalc:
            bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
        else:
            tmp.normal_update()
        uv = tmp.loops.layers.uv.verify()
        for f in tmp.faces:
            ax = max(range(3), key=lambda k: abs(f.normal[k]))
            o = [k for k in range(3) if k != ax]
            for lp in f.loops:
                p = lp.vert.co
                lp[uv].uv = (p[o[0]] / self.uv_size, p[o[1]] / self.uv_size)
        self._merge(tmp, mat, smooth, world)

    def loft(self, bottom, top, mat, cap_bottom=True, cap_top=True, closed=True, smooth=False, bevel=0.0,
             world=None):
        """Volume entre deux polygones 3D de meme nombre de sommets (escarpe talutee, tronc de pyramide)."""
        n = len(bottom)
        verts = [Vector(p) for p in bottom] + [Vector(p) for p in top]
        faces = []
        rng_ = range(n) if closed else range(n - 1)
        for i in rng_:
            j = (i + 1) % n
            faces.append((i, j, n + j, n + i))
        if closed:
            if cap_bottom:
                faces.append(tuple(range(n))[::-1])
            if cap_top:
                faces.append(tuple(range(n, 2 * n)))
        tmp = bmesh.new()
        vs = [tmp.verts.new(v) for v in verts]
        for f in faces:
            try:
                tmp.faces.new([vs[i] for i in f])
            except ValueError:
                pass
        bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
        if bevel:
            try:
                bmesh.ops.bevel(tmp, geom=list(tmp.edges), offset=bevel, segments=1, affect="EDGES", profile=0.5,
                                clamp_overlap=True)
            except Exception:
                pass
        uv = tmp.loops.layers.uv.verify()
        for f in tmp.faces:
            ax = max(range(3), key=lambda k: abs(f.normal[k]))
            o = [k for k in range(3) if k != ax]
            for lp in f.loops:
                p = lp.vert.co
                lp[uv].uv = (p[o[0]] / self.uv_size, p[o[1]] / self.uv_size)
        self._merge(tmp, mat, smooth, world)

    def prism(self, poly, z0, z1, mat, bevel=0.0, top_z=None, world=None):
        """Prisme de plan : poly [(x, y)] extrude de z0 a z1 (top_z : liste de z par sommet pour un dessus incline)."""
        bot = [V(p[0], p[1], z0) for p in poly]
        top = [V(p[0], p[1], (top_z[i] if top_z else z1)) for i, p in enumerate(poly)]
        if _area(poly) < 0:
            bot, top = bot[::-1], top[::-1]
        self.loft(bot, top, mat, bevel=bevel, world=world)

    def frame_box(self, M4, size, mat, bevel=0.0, world=None):
        """Boite (sx, sy, sz) dans le repere M4 (centre)."""
        sx, sy, sz = size
        pts = [M4 @ V(x * sx / 2, y * sy / 2, z * sz / 2) for z in (-1, 1) for (x, y) in
               ((-1, -1), (1, -1), (1, 1), (-1, 1))]
        self.loft(pts[:4], pts[4:], mat, bevel=bevel, world=world)

    def sweep(self, path, prof, mat, closed=False, caps=True, smooth=False, world=None, up=Z):
        """Profil 2D [(d, h)] balaye le long d'une polyligne horizontale ; d = vers la droite du chemin
        (exterieur si le chemin tourne dans le sens horaire vu de dessus), h = vers le haut. Onglets aux sommets."""
        path = [Vector(p) for p in path]
        n = len(path)
        rows = []
        for i in range(n):
            if closed:
                a, b = path[i - 1], path[(i + 1) % n]
                d0 = (path[i] - a)
                d1 = (b - path[i])
            else:
                d0 = path[i] - path[max(i - 1, 0)] if i > 0 else path[1] - path[0]
                d1 = path[min(i + 1, n - 1)] - path[i] if i < n - 1 else path[-1] - path[-2]
            d0 = Vector((d0.x, d0.y, 0)).normalized()
            d1 = Vector((d1.x, d1.y, 0)).normalized()
            r0 = Vector((d0.y, -d0.x, 0))
            r1 = Vector((d1.y, -d1.x, 0))
            m = (r0 + r1)
            m = m.normalized() if m.length > 1e-6 else r0
            k = 1.0 / max(0.25, m.dot(r0))
            rows.append([path[i] + m * (d * k) + up * h for d, h in prof])
        tmp = bmesh.new()
        vrows = [[tmp.verts.new(p) for p in r] for r in rows]
        m_ = len(prof)
        segs = n if closed else n - 1
        for i in range(segs):
            j = (i + 1) % n
            for q in range(m_ - 1):
                try:
                    tmp.faces.new((vrows[i][q], vrows[i][q + 1], vrows[j][q + 1], vrows[j][q]))
                except ValueError:
                    pass
        if caps and not closed:
            for r in (vrows[0], vrows[-1]):
                try:
                    tmp.faces.new(r)
                except ValueError:
                    pass
        bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
        uv = tmp.loops.layers.uv.verify()
        for f in tmp.faces:
            ax = max(range(3), key=lambda k: abs(f.normal[k]))
            o = [k for k in range(3) if k != ax]
            for lp in f.loops:
                p = lp.vert.co
                lp[uv].uv = (p[o[0]] / self.uv_size, p[o[1]] / self.uv_size)
        self._merge(tmp, mat, smooth, world)

    def tube(self, pts, radii, mat, **kw):
        """Tube a UV locales (u = tour, v = longueur) : cordes, barreaux, poteaux ronds."""
        return self.local(super().tube, pts, radii, mat, **kw)

    def cyl(self, a, b, r, mat, segs=10, caps=True, smooth=True):
        self.tube([a, b], [r, r], mat, segs=segs, caps=caps, smooth=smooth)

    def sphere(self, c, r, mat, u=10, v=6, scale=(1, 1, 1)):
        tmp = bmesh.new()
        bmesh.ops.create_uvsphere(tmp, u_segments=u, v_segments=v, radius=r)
        for vv in tmp.verts:
            vv.co = Vector((vv.co.x * scale[0], vv.co.y * scale[1], vv.co.z * scale[2])) + Vector(c)
        self.add_bm(tmp, mat, smooth=True)

    def add_bm(self, tmp, mat, smooth=True, uv_size=None):
        uv = tmp.loops.layers.uv.verify()
        s = uv_size or self.uv_size
        for f in tmp.faces:
            ax = max(range(3), key=lambda k: abs(f.normal[k]))
            o = [k for k in range(3) if k != ax]
            for lp in f.loops:
                p = lp.vert.co
                lp[uv].uv = (p[o[0]] / s, p[o[1]] / s)
        self._merge(tmp, mat, smooth)

    def add_mesh_multi(self, bm_src, mats, M4=None, smooth=None, world=False):
        """Fusionne un bmesh multi-materiaux (material_index -> mats[i]) en gardant ses UV."""
        me = bpy.data.meshes.new("_fb_multi")
        bm_src.to_mesh(me)
        bm_src.free()
        if M4 is not None:
            me.transform(M4)
        n0 = len(self.bm.faces)
        remap = [self._mi(m) for m in mats]
        self.bm.from_mesh(me)
        bpy.data.meshes.remove(me)
        self.bm.faces.ensure_lookup_table()
        for i in range(n0, len(self.bm.faces)):
            f = self.bm.faces[i]
            f.material_index = remap[min(f.material_index, len(remap) - 1)]
            if smooth is not None:
                f.smooth = smooth
            for lp in f.loops:
                lp[self.col] = self._c4()

    def mark(self):
        return len(self.bm.faces)

    def paint(self, n0, fn):
        """Recolore (Blend RGB) les faces ajoutees depuis n0 : fn(face) -> (r, g, b) ou None."""
        self.bm.faces.ensure_lookup_table()
        for i in range(n0, len(self.bm.faces)):
            f = self.bm.faces[i]
            c = fn(f)
            if c is not None:
                for lp in f.loops:
                    a = lp[self.col][3]
                    lp[self.col] = (c[0], c[1], c[2], a)

    def set_base(self, n0, fn):
        """Niveau de sol (Z local) des faces ajoutees depuis n0 : fn(face) -> z ou None."""
        self.bm.faces.ensure_lookup_table()
        for i in range(n0, len(self.bm.faces)):
            f = self.bm.faces[i]
            z = fn(f)
            if z is not None:
                for lp in f.loops:
                    c = lp[self.col]
                    lp[self.col] = (c[0], c[1], c[2], z / 10.0 + 0.5)

    def finish(self, coll, sharp=None):
        ob = super().finish(coll)
        if ob.data.uv_layers:
            ob.data.uv_layers[0].name = "UVMap"
        if sharp and hasattr(ob.data, "set_sharp_from_angle"):
            ob.data.set_sharp_from_angle(angle=math.radians(sharp))
        return ob


# ----------------------------------------------------------------------------
# Polygones

def _area(poly):
    a = 0.0
    for i in range(len(poly)):
        x0, y0 = poly[i][0], poly[i][1]
        x1, y1 = poly[(i + 1) % len(poly)][0], poly[(i + 1) % len(poly)][1]
        a += x0 * y1 - x1 * y0
    return a / 2


def inside(poly, p):
    x, y = p[0], p[1]
    c = False
    n = len(poly)
    for i in range(n):
        x0, y0 = poly[i][0], poly[i][1]
        x1, y1 = poly[(i + 1) % n][0], poly[(i + 1) % n][1]
        if (y0 > y) != (y1 > y) and x < (x1 - x0) * (y - y0) / (y1 - y0 + 1e-12) + x0:
            c = not c
    return c


def offset_poly(poly, d, closed=True):
    """Decalage avec onglets ; d > 0 vers l'exterieur d'un polygone anti-horaire (droite du chemin)."""
    pts = [Vector((p[0], p[1], 0)) for p in poly]
    n = len(pts)
    out = []
    for i in range(n):
        if closed:
            a, b = pts[i - 1], pts[(i + 1) % n]
            d0, d1 = (pts[i] - a).normalized(), (b - pts[i]).normalized()
        else:
            d0 = (pts[i] - pts[i - 1]).normalized() if i > 0 else (pts[1] - pts[0]).normalized()
            d1 = (pts[i + 1] - pts[i]).normalized() if i < n - 1 else (pts[-1] - pts[-2]).normalized()
        r0, r1 = Vector((d0.y, -d0.x, 0)), Vector((d1.y, -d1.x, 0))
        m = (r0 + r1)
        m = m.normalized() if m.length > 1e-6 else r0
        k = 1.0 / max(0.2, m.dot(r0))
        q = pts[i] + m * d * k
        out.append((q.x, q.y))
    return out


def seg_frame(a, b):
    """Repere d'un segment horizontal a -> b : (origine, u le long, n a droite = exterieur, longueur)."""
    a, b = Vector((a[0], a[1], 0)), Vector((b[0], b[1], 0))
    u = (b - a)
    L = u.length
    u.normalize()
    n = Vector((u.y, -u.x, 0))
    return a, u, n, L


def rbox_pts(c, s, R=None):
    R = R or Matrix.Identity(3)
    c = Vector(c)
    return [c + R @ Vector((sx * s[0] / 2, sy * s[1] / 2, sz * s[2] / 2))
            for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]


def frame_M4(origin, u, n, w=Z):
    """Matrice 4x4 : X = u, Y = n, Z = w."""
    M = Matrix((u, n, w)).transposed().to_4x4()
    M.translation = origin
    return M


# ----------------------------------------------------------------------------
# Pieces composees

def slab_paving(fb, poly, z, mat, rng, size=(0.42, 0.72), th=0.06, gap=0.02, holes=None, blend=None,
                skip=0.0, angle=0.0, heave=0.008):
    """Dallage de dalles rectangulaires en rangs (lignes paralleles a `angle`) dans un polygone ; centres hors des
    `holes` (fonction p -> bool). Dalles legerement inclinees / soulevees, aretes chanfreinees."""
    if blend is not None:
        old = fb.blend
        fb.blend = blend
    R = Matrix.Rotation(angle, 3, "Z")
    Ri = R.transposed()
    pl = [Ri @ Vector((p[0], p[1], 0)) for p in poly]
    x0, x1 = min(p.x for p in pl), max(p.x for p in pl)
    y0, y1 = min(p.y for p in pl), max(p.y for p in pl)
    y = y0 + gap
    n = 0
    while y < y1:
        h = rng.uniform(*size) * 0.8
        x = x0 + rng.uniform(-0.2, 0.0)
        while x < x1:
            w = rng.uniform(*size)
            c = R @ Vector((x + w / 2, y + h / 2, 0))
            corners_ok = all(inside(poly, R @ Vector((x + dx, y + dy, 0)))
                             for dx in (0.05, w - 0.05) for dy in (0.05, h - 0.05))
            if corners_ok and not (holes and holes(c)) and rng.random() >= skip:
                tilt = Matrix.Rotation(rng.uniform(-0.012, 0.012), 3, "X") @ Matrix.Rotation(
                    rng.uniform(-0.012, 0.012), 3, "Y")
                zz = z + th / 2 + rng.uniform(-heave, heave * 0.5)
                fb.box((c.x, c.y, zz), (w - gap, h - gap, th), mat, R=R @ tilt, bevel=0.012)
                n += 1
            x += w
        y += h
    if blend is not None:
        fb.blend = old
    return n


def wall_openings(fb, a, b, z0, z1, thick, openings, mat, inward=1, jamb=None):
    """Mur droit de a vers b (plan, face exterieure sur la ligne a-b, epaisseur vers `inward` x normale gauche),
    perce d'ouvertures [(t0, t1, zb, zt)] (abscisses le long du mur). jamb(t0, t1, zb, zt, M4) : habillage."""
    o, u, n, L = seg_frame(a, b)
    inn = -n * inward          # vers l'interieur
    ops = sorted(openings)
    cuts = [0.0]
    for t0, t1, _zb, _zt in ops:
        cuts += [t0, t1]
    cuts.append(L)

    def piece(t0, t1, za, zb):
        if t1 - t0 < 1e-4 or zb - za < 1e-4:
            return
        p = [o + u * t0, o + u * t1, o + u * t1 + inn * thick, o + u * t0 + inn * thick]
        bot = [V(q.x, q.y, za) for q in p]
        top = [V(q.x, q.y, zb) for q in p]
        fb.loft(bot, top, mat)
    t = 0.0
    for t0, t1, zb, zt in ops:
        piece(t, t0, z0, z1)
        piece(t0, t1, z0, zb)
        piece(t0, t1, zt, z1)
        if jamb:
            jamb(t0, t1, zb, zt, frame_M4(o + u * ((t0 + t1) / 2) + inn * (thick / 2), u, -inn))
        t = t1
    piece(t, L, z0, z1)


def stairs(fb, origin, ang, width, n, rise, tread, mat, z0=0.0, nosing=0.02, solid=True, rng=None):
    """Volee droite : pied a origin, monte vers +Y local tourne de `ang`. Renvoie les points du coin UCX."""
    R = Matrix.Rotation(ang, 3, "Z")
    o = Vector(origin)
    for k in range(n):
        zt = z0 + (k + 1) * rise
        y = k * tread
        zb = z0 if solid else zt - rise - 0.06
        c = o + R @ Vector((0, y + tread / 2 - nosing / 2, (zb + zt) / 2 - z0))
        c.z = (zb + zt) / 2
        j = rng.uniform(-0.004, 0.004) if rng else 0.0
        fb.box(c, (width + j, tread + nosing, zt - zb), mat, R=R @ Matrix.Rotation(j, 3, "Z"), bevel=0.012)
    L = n * tread
    pts = [o + R @ Vector((sx * width / 2, y, 0)) for sx in (-1, 1) for y in (0, L)]
    pts = [Vector((p.x, p.y, z0)) for p in pts]
    pts += [o + R @ Vector((sx * width / 2, L, 0)) + Vector((0, 0, z0 + n * rise)) for sx in (-1, 1)]
    pts += [o + R @ Vector((sx * width / 2, 0, 0)) + Vector((0, 0, z0 + rise)) for sx in (-1, 1)]
    return pts


def bars(fb, a, b, z0, z1, pitch, r, mat, rails=(), rail_w=0.04, rail_t=0.012, segs=6):
    """Grille de barreaux verticaux de a a b (traverses plates a rails = hauteurs)."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    u = d.normalized()
    nb_ = max(1, int(L / pitch))
    for i in range(nb_ + 1):
        p = a + u * (L * i / nb_)
        fb.cyl(V(p.x, p.y, z0), V(p.x, p.y, z1), r, mat, segs=segs)
    ang = math.atan2(u.y, u.x)
    for z in rails:
        c = (a + b) / 2
        fb.box(V(c.x, c.y, z), (L + 0.02, rail_t, rail_w), mat, R=Matrix.Rotation(ang, 3, "Z"), bevel=0.0)


def studs(fb, pts, r, mat, normal):
    """Clous forges a tete diamant (pyramides basses) sur une face de normale `normal`."""
    n = Vector(normal).normalized()
    u = n.orthogonal().normalized()
    w = n.cross(u)
    for p in pts:
        p = Vector(p)
        base = [p + (u * math.cos(TAU * k / 4 + 0.785) + w * math.sin(TAU * k / 4 + 0.785)) * r * 1.41 for k in range(4)]
        apex = p + n * r * 0.9
        fb.poly_bm(base + [apex], [(0, 1, 4), (1, 2, 4), (2, 3, 4), (3, 0, 4), (3, 2, 1, 0)], mat, world=False)


def hip_roof(fb, cx, cy, L, W, z_eave, pitch_deg, overhang, mat, th=0.07, ridge_mat=None, rng=None, hip=True,
             ang=0.0):
    """Toit a croupes (hip=True) ou a deux pans a pignons (hip=False) sur un rectangle L (X) x W (Y) centre.
    UV : u le long de l'egout, v dans la pente (canal des tuiles). Renvoie (z_faitage, points UCX)."""
    t = math.tan(math.radians(pitch_deg))
    hw, hl = W / 2 + overhang, L / 2 + overhang
    zr = z_eave + (W / 2) * t
    ze = z_eave - overhang * t
    ridge_h = hl - hw if hip else hl
    R = Matrix.Rotation(ang, 4, "Z")
    C = Matrix.Translation((cx, cy, 0)) @ R
    e = [V(-hl, -hw, ze), V(hl, -hw, ze), V(hl, hw, ze), V(-hl, hw, ze)]
    r0, r1 = V(-ridge_h, 0, zr), V(ridge_h, 0, zr)
    planes = [(e[0], e[1], r1, r0), (e[2], e[3], r0, r1)]
    if hip:
        planes += [(e[1], e[2], r1), (e[3], e[0], r0)]
    for pl in planes:
        tmp = bmesh.new()
        top = [tmp.verts.new(C @ p) for p in pl]
        nrm = (pl[1] - pl[0]).cross(pl[-1] - pl[0]).normalized()
        off = C.to_3x3() @ (nrm * -th)
        bot = [tmp.verts.new(C @ p + off) for p in pl]
        tmp.faces.new(top)
        tmp.faces.new(bot[::-1])
        k = len(pl)
        for i in range(k):
            j = (i + 1) % k
            tmp.faces.new((top[j], top[i], bot[i], bot[j]))
        bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
        uv = tmp.loops.layers.uv.verify()
        along = (C.to_3x3() @ (pl[1] - pl[0])).normalized()
        down = (C.to_3x3() @ Vector((pl[0] - pl[-1]))).normalized()
        dn = down - along * down.dot(along)
        dn.normalize()
        for f in tmp.faces:
            for lp in f.loops:
                p = lp.vert.co
                lp[uv].uv = (p.dot(along), -p.dot(dn) / 1.0)
        fb.local(fb._merge, tmp, mat, False)
    # tuiles de faitage / aretiers (demi-ronds)
    rm = ridge_mat or mat
    ridge = [C @ r0, C @ r1]
    if ridge_h > 0.05:
        fb.cyl(ridge[0] + Vector((0, 0, 0.02)), ridge[1] + Vector((0, 0, 0.02)), 0.075, rm, segs=8)
    if hip:
        for a_, b_ in ((r1, e[1]), (r1, e[2]), (r0, e[0]), (r0, e[3])):
            fb.cyl(C @ a_ + Vector((0, 0, 0.02)), C @ b_ + Vector((0, 0, 0.02)), 0.065, rm, segs=8)
    pts = [C @ p for p in e] + [C @ r0, C @ r1]
    return zr, pts


def gable_fill(fb, cx, cy, L, W, z_eave, pitch_deg, thick, mat, ang=0.0, drop=0.1):
    """Pignons triangulaires maconnes aux deux bouts (axe X du batiment), arases sous la couverture."""
    t = math.tan(math.radians(pitch_deg))
    zr = z_eave + (W / 2) * t - drop / math.cos(math.radians(pitch_deg))
    z_eave = z_eave - drop / math.cos(math.radians(pitch_deg))
    R = Matrix.Rotation(ang, 4, "Z")
    C = Matrix.Translation((cx, cy, 0)) @ R
    for sx in (-1, 1):
        x0, x1 = sx * L / 2, sx * (L / 2 - thick)
        tri = [V(0, -W / 2, z_eave), V(0, W / 2, z_eave), V(0, 0, zr)]
        bot = [C @ V(x0, p.y, p.z) for p in tri]
        top = [C @ V(x1, p.y, p.z) for p in tri]
        fb.loft(bot, top, mat)
    return zr


def block(fb, quad, z0, z1, mat, round_top=0.0, segs=3, top_z=None, world=None, round_edges=((0, 1), (2, 3))):
    """Bloc a plan quadrilatere quad = [ext0, ext1, int1, int0] de z0 a z1 (top_z : z par sommet).
    round_top : chanfrein arrondi des aretes hautes listees (chaperon de merlon)."""
    tmp = bmesh.new()
    zt = top_z or [z1] * len(quad)
    b = [tmp.verts.new((q[0], q[1], z0)) for q in quad]
    t = [tmp.verts.new((q[0], q[1], zt[i])) for i, q in enumerate(quad)]
    n = len(quad)
    tmp.faces.new(b[::-1])
    tmp.faces.new(t)
    for i in range(n):
        j = (i + 1) % n
        tmp.faces.new((b[i], b[j], t[j], t[i]))
    bmesh.ops.recalc_face_normals(tmp, faces=tmp.faces)
    if round_top:
        es = []
        for i, j in round_edges:
            e = tmp.edges.get((t[i], t[j]))
            if e is not None:
                es.append(e)
        if es:
            try:
                bmesh.ops.bevel(tmp, geom=es, offset=round_top, segments=segs, affect="EDGES", profile=0.5,
                                clamp_overlap=True)
            except Exception:
                pass
    uv = tmp.loops.layers.uv.verify()
    for f in tmp.faces:
        ax = max(range(3), key=lambda k: abs(f.normal[k]))
        o = [k for k in range(3) if k != ax]
        for lp in f.loops:
            p = lp.vert.co
            lp[uv].uv = (p[o[0]] / fb.uv_size, p[o[1]] / fb.uv_size)
    fb._merge(tmp, mat, False, world)


def line_x(a, b, c, d):
    """Intersection des droites (a, b) et (c, d) dans le plan XY."""
    x1, y1, x2, y2 = a[0], a[1], b[0], b[1]
    x3, y3, x4, y4 = c[0], c[1], d[0], d[1]
    den = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4)
    if abs(den) < 1e-9:
        return None
    t = ((x1 - x3) * (y3 - y4) - (y1 - y3) * (x3 - x4)) / den
    return (x1 + t * (x2 - x1), y1 + t * (y2 - y1))


def beam(fb, a, b, size, mat, bevel=0.006, wood=True):
    """Piece equarrie de section size = (largeur, hauteur) de a a b (fil le long de la piece)."""
    a, b = Vector(a), Vector(b)
    d = b - a
    L = d.length
    q = d.normalized().to_track_quat("Y", "Z").to_matrix()
    c = (a + b) / 2
    if wood:
        fb.wood(c, (size[0], L, size[1]), mat, R=q, bevel=bevel, long_u=False)
    else:
        fb.box(c, (size[0], L, size[1]), mat, R=q, bevel=bevel)
