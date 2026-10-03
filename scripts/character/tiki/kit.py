"""Geometry kit for the tiki creatures.

Conventions (as scripts/character/bean_soldier.py): meters, +Z up, the character faces -Y (Blender front view),
its left is +X, feet on z = 0. On a body, theta = 0 faces -Y and grows towards +X (the character's left, the
right of the image in a front view, like the reference sheets).
Every bm_* function appends geometry to a bmesh; obj_from_bm turns it into an object.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Quaternion, Vector

TAU = 2 * math.pi


def V(x, y=None, z=None):
    return Vector(x) if y is None else Vector((x, y, z))


def lerp(a, b, t):
    return a + (b - a) * t


def smoothstep(e0, e1, x):
    t = max(0.0, min(1.0, (x - e0) / max(e1 - e0, 1e-9)))
    return t * t * (3 - 2 * t)


# ---------------------------------------------------------------- bodies and plates

class Body:
    """Revolved egg: radius factor f(z) in [0, 1] times (rx, ry), widest at zw.
    Above zw: f = (1 - t**pt) ** qt, below: f = (1 - t**pb) ** qb, t running from 0 at zw to 1 at the pole.
    q = 1/p gives a superellipse (round pole), q = 1 a pointed, conical pole."""

    def __init__(self, zb, zt, zw, rx, ry, pt=2.0, qt=0.5, pb=2.0, qb=0.5):
        self.zb, self.zt, self.zw, self.rx, self.ry = zb, zt, zw, rx, ry
        self.pt, self.qt, self.pb, self.qb = pt, qt, pb, qb

    def f(self, z):
        if z >= self.zw:
            t = min(1.0, (z - self.zw) / max(self.zt - self.zw, 1e-6))
            return max(0.0, 1 - t ** self.pt) ** self.qt
        t = min(1.0, (self.zw - z) / max(self.zw - self.zb, 1e-6))
        return max(0.0, 1 - t ** self.pb) ** self.qb

    def surf(self, theta, z, off=0.0):
        f = self.f(z)
        return Vector(((self.rx * f + off) * math.sin(theta), -(self.ry * f + off) * math.cos(theta), z))

    def theta_at(self, x, z, off=0.0, back=False):
        rx = self.rx * self.f(z) + off
        th = math.asin(max(-1.0, min(1.0, x / max(rx, 1e-5))))
        return math.pi - th if back else th

    def surf_xz(self, x, z, off=0.0, back=False):
        return self.surf(self.theta_at(x, z, off, back), z, off)

    def normal(self, theta, z, off=0.0):
        e = 1e-3
        p = self.surf(theta, z, off)
        n = (self.surf(theta + e, z, off) - self.surf(theta - e, z, off)).cross(
            self.surf(theta, min(z + e, self.zt), off) - self.surf(theta, max(z - e, self.zb), off))
        radial = Vector((p.x, p.y, 0.0))
        if radial.length > 1e-6 and n.dot(radial) < 0:
            n = -n
        if n.length < 1e-9:
            return Vector((0, 0, 1 if z > self.zw else -1))
        return n.normalized()


class Mask:
    """Plate lying on a body (mask, face). Normalized coords (u, v) in [-1, 1]: u -> +X, v -> +Z."""

    def __init__(self, body, cx, cz, hw, hh, off=0.006, dome=0.01, back=False):
        self.body, self.cx, self.cz, self.hw, self.hh = body, cx, cz, hw, hh
        self.off, self.dome, self.back = off, dome, back

    def dome_at(self, u, v):
        return self.dome * max(0.0, 1.0 - (u * u + v * v)) ** 2

    def pt(self, u, v, extra=0.0):
        x = self.cx + u * self.hw
        z = self.cz + v * self.hh
        return self.body.surf_xz(x, z, self.off + self.dome_at(u, v) + extra, self.back)

    def nrm(self, u, v):
        e = 0.02
        n = (self.pt(u + e, v) - self.pt(u - e, v)).cross(self.pt(u, v + e) - self.pt(u, v - e))
        return (-n if self.back else n).normalized()


# ---------------------------------------------------------------- 2D outlines

def ccw(poly):
    area = sum(poly[i][0] * poly[(i + 1) % len(poly)][1] - poly[(i + 1) % len(poly)][0] * poly[i][1]
               for i in range(len(poly)))
    return poly if area > 0 else poly[::-1]


def chaikin(poly, n=2, closed=True):
    pts = [tuple(p) for p in poly]
    for _ in range(n):
        out = []
        m = len(pts) if closed else len(pts) - 1
        if not closed:
            out.append(pts[0])
        for i in range(m):
            a, b = pts[i], pts[(i + 1) % len(pts)]
            out.append((0.75 * a[0] + 0.25 * b[0], 0.75 * a[1] + 0.25 * b[1]))
            out.append((0.25 * a[0] + 0.75 * b[0], 0.25 * a[1] + 0.75 * b[1]))
        if not closed:
            out.append(pts[-1])
        pts = out
    return pts


def superellipse(a, b, n=4.0, cx=0.0, cy=0.0, k=64):
    out = []
    for i in range(k):
        t = TAU * i / k
        c, s = math.cos(t), math.sin(t)
        out.append((cx + a * math.copysign(abs(c) ** (2 / n), c), cy + b * math.copysign(abs(s) ** (2 / n), s)))
    return out


def ellipse(a, b, cx=0.0, cy=0.0, k=48, rot=0.0):
    out = []
    for i in range(k):
        t = TAU * i / k
        x, y = a * math.cos(t), b * math.sin(t)
        out.append((cx + x * math.cos(rot) - y * math.sin(rot), cy + x * math.sin(rot) + y * math.cos(rot)))
    return out


def mirror_poly(half):
    """half: points with u >= 0 from top (u = 0) to bottom (u = 0); returns the closed symmetric polygon."""
    right = list(half)
    left = [(-u, v) for u, v in reversed(half[1:-1])]
    return ccw(right + left)


def resample(pts, n, closed=True):
    """Even arc-length resampling of a 2D polyline."""
    P = [Vector((p[0], p[1])) for p in pts]
    if closed:
        P.append(P[0])
    seg = [(P[i + 1] - P[i]).length for i in range(len(P) - 1)]
    total = sum(seg)
    out = []
    count = n if closed else n - 1
    for k in range(n):
        d = total * k / count
        i = 0
        while i < len(seg) - 1 and d > seg[i]:
            d -= seg[i]
            i += 1
        t = d / seg[i] if seg[i] > 0 else 0.0
        q = P[i].lerp(P[i + 1], min(1.0, t))
        out.append((q.x, q.y))
    return out


def catmull(pts, n=24):
    """Smooth open curve through 2D/3D points (Catmull-Rom)."""
    P = [Vector(p) for p in pts]
    if len(P) < 3:
        return [P[0].lerp(P[-1], k / (n - 1)) for k in range(n)]
    P = [P[0] + (P[0] - P[1])] + P + [P[-1] + (P[-1] - P[-2])]
    out = []
    segs = len(P) - 3
    for k in range(n):
        s = k / (n - 1) * segs
        i = min(int(s), segs - 1)
        t = s - i
        p0, p1, p2, p3 = P[i], P[i + 1], P[i + 2], P[i + 3]
        out.append(0.5 * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t * t +
                          (-p0 + 3 * p1 - 3 * p2 + p3) * t * t * t))
    return out


# ---------------------------------------------------------------- bmesh builders

def bm_revolve(bm, body, nu=64, nz=56, off=0.0, z0=None, z1=None, close=True, keep=None):
    """Body surface between z0 and z1 (poles closed when they reach zb / zt)."""
    z0 = body.zb if z0 is None else z0
    z1 = body.zt if z1 is None else z1
    rows = []
    for j in range(nz + 1):
        t = j / nz
        z = z0 + (z1 - z0) * (0.5 - 0.5 * math.cos(math.pi * t))
        if close and ((j == 0 and z0 <= body.zb + 1e-6) or (j == nz and z1 >= body.zt - 1e-6)):
            v = bm.verts.new((0, 0, z))
            rows.append([v] * nu)
        else:
            rows.append([bm.verts.new(body.surf(TAU * i / nu, z, off)) for i in range(nu)])
    faces = []
    for j in range(nz):
        for i in range(nu):
            i2 = (i + 1) % nu
            if keep and not keep(TAU * (i + 0.5) / nu, rows[j][i].co.z * 0.5 + rows[j + 1][i].co.z * 0.5):
                continue
            quad = [rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]]
            uniq = []
            for v in quad:
                if v not in uniq:
                    uniq.append(v)
            if len(uniq) >= 3:
                faces.append(bm.faces.new(uniq))
    return faces


def bm_patch(bm, mask, outline, extra=0.0, dome=0.0, nr=8, center=None):
    """Star-shaped outline ((u, v) list, counter-clockwise seen from the front) filled with a polar fan."""
    outline = ccw(outline)
    if center is None:
        center = (sum(p[0] for p in outline) / len(outline), sum(p[1] for p in outline) / len(outline))
    cu, cv = center
    c = bm.verts.new(mask.pt(cu, cv, extra + dome))
    rings = []
    for k in range(1, nr + 1):
        s = k / nr
        rings.append([bm.verts.new(mask.pt(cu + (u - cu) * s, cv + (v - cv) * s, extra + dome * (1 - s * s)))
                      for u, v in outline])
    n = len(outline)
    faces = [bm.faces.new((c, rings[0][i], rings[0][(i + 1) % n])) for i in range(n)]
    for k in range(nr - 1):
        for i in range(n):
            i2 = (i + 1) % n
            faces.append(bm.faces.new((rings[k][i], rings[k + 1][i], rings[k + 1][i2], rings[k][i2])))
    if mask.back:
        bmesh.ops.reverse_faces(bm, faces=faces)
    return faces


def bm_prism(bm, mask, poly, h0, h1):
    """2D polygon ((u, v), any winding) extruded from extra h0 to h1 above the mask."""
    poly = ccw(poly)
    bot = [bm.verts.new(mask.pt(u, v, h0)) for u, v in poly]
    top = [bm.verts.new(mask.pt(u, v, h1)) for u, v in poly]
    faces = [bm.faces.new(top), bm.faces.new(bot[::-1])]
    n = len(poly)
    for i in range(n):
        i2 = (i + 1) % n
        faces.append(bm.faces.new((bot[i], bot[i2], top[i2], top[i])))
    return faces


def frames_along(pts, ups=None):
    """(tangent, side, up) per point; ups pins the 'up' axis, otherwise parallel transport."""
    n = len(pts)
    tangents = []
    for k in range(n):
        d = pts[min(k + 1, n - 1)] - pts[max(k - 1, 0)]
        tangents.append(d.normalized() if d.length > 1e-9 else Vector((0, 0, 1)))
    out = []
    if ups:
        for d, u in zip(tangents, ups):
            up = u - d * u.dot(d)
            if up.length < 1e-6:
                up = d.orthogonal()
            up.normalize()
            out.append((d, d.cross(up).normalized(), up))
        return out
    up = tangents[0].orthogonal().normalized()
    prev = tangents[0]
    for d in tangents:
        q = prev.rotation_difference(d)
        up = q @ up
        up = (up - d * up.dot(d))
        up = up.normalized() if up.length > 1e-6 else d.orthogonal().normalized()
        out.append((d, d.cross(up).normalized(), up))
        prev = d
    return out


def bm_tube(bm, pts, radii, segs=12, caps=True, squash=1.0, ups=None, twist=0.0):
    """Tube through pts with a radius per point; squash < 1 flattens it along its 'up' axis (ups[k])."""
    pts = [Vector(p) for p in pts]
    if isinstance(radii, (int, float)):
        radii = [radii] * len(pts)
    fr = frames_along(pts, ups)
    rows = []
    for k, (p, (d, side, up)) in enumerate(zip(pts, fr)):
        ring = []
        for i in range(segs):
            a = TAU * i / segs + twist
            ring.append(bm.verts.new(p + radii[k] * (math.cos(a) * side + squash * math.sin(a) * up)))
        rows.append(ring)
    faces = []
    for k in range(len(rows) - 1):
        for i in range(segs):
            i2 = (i + 1) % segs
            faces.append(bm.faces.new((rows[k][i], rows[k][i2], rows[k + 1][i2], rows[k + 1][i])))
    if caps:
        for ring, end, rev in ((rows[0], pts[0], True), (rows[-1], pts[-1], False)):
            c = bm.verts.new(end)
            for i in range(segs):
                tri = (c, ring[i], ring[(i + 1) % segs])
                faces.append(bm.faces.new(tri[::-1] if rev else tri))
    return faces


def bm_blob(bm, center, radii, rot=None, seg=24, ring=12, cut_below=None):
    geom = bmesh.ops.create_uvsphere(bm, u_segments=seg, v_segments=ring, radius=1.0)
    R = None
    if rot is not None:
        R = rot.to_matrix() if isinstance(rot, Quaternion) else rot
    c = Vector(center)
    for v in geom["verts"]:
        p = Vector((v.co.x * radii[0], v.co.y * radii[1], v.co.z * radii[2]))
        if R is not None:
            p = R @ p
        p += c
        if cut_below is not None:
            p.z = max(p.z, cut_below)
        v.co = p
    return list({f for v in geom["verts"] for f in v.link_faces})


def bm_box(bm, center, size, rot=None):
    geom = bmesh.ops.create_cube(bm, size=1.0)
    R = rot.to_matrix() if isinstance(rot, Quaternion) else rot
    for v in geom["verts"]:
        p = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
        if R is not None:
            p = R @ p
        v.co = p + Vector(center)
    return list({f for v in geom["verts"] for f in v.link_faces})


def bm_horn(bm, base, d, length, r0, bend=None, bend_amt=0.0, n=10, segs=12, r_tip=0.0015, power=0.9):
    d = Vector(d).normalized()
    pts, radii = [], []
    for k in range(n + 1):
        t = k / n
        p = Vector(base) + d * (length * t)
        if bend is not None:
            p += Vector(bend).normalized() * (bend_amt * length * t * t)
        pts.append(p)
        radii.append(r_tip + (r0 - r_tip) * (1 - t) ** power)
    return bm_tube(bm, pts, radii, segs, caps=True)


def bm_torus(bm, center, axis, R, r, seg=24, ring=8, squash=1.0):
    axis = Vector(axis).normalized()
    a = axis.orthogonal().normalized()
    b = axis.cross(a)
    c0 = Vector(center)
    rows = []
    for i in range(seg):
        phi = TAU * i / seg
        radial = math.cos(phi) * a + math.sin(phi) * b
        c = c0 + R * radial
        rows.append([bm.verts.new(c + r * (math.cos(TAU * j / ring) * radial + squash * math.sin(TAU * j / ring) * axis))
                     for j in range(ring)])
    faces = []
    for i in range(seg):
        for j in range(ring):
            faces.append(bm.faces.new((rows[i][j], rows[(i + 1) % seg][j], rows[(i + 1) % seg][(j + 1) % ring],
                                       rows[i][(j + 1) % ring])))
    return faces


def bm_helix(bm, p0, p1, R, r, turns, segs=8, steps=22, phase=0.0):
    """Rope wrapped around the segment p0-p1 (rope wraps on wrists, ankles, handles)."""
    p0, p1 = Vector(p0), Vector(p1)
    ax = (p1 - p0)
    L = ax.length
    ax.normalize()
    a = ax.orthogonal().normalized()
    b = ax.cross(a)
    n = max(10, int(turns * steps))
    pts = []
    for k in range(n + 1):
        t = k / n
        ang = TAU * turns * t + phase
        pts.append(p0 + ax * (L * t) + R * (math.cos(ang) * a + math.sin(ang) * b))
    return bm_tube(bm, pts, [r] * len(pts), segs, caps=True)


def bm_leaf(bm, base, along, normal, length, width, fold=0.2, droop=0.2, n=6, col=None, col_layer=None,
            shape=0.85, twist=0.0):
    """Leaf blade from base along `along`, facing `normal`; the edges fold back by `fold` of the half width and
    the tip droops back by `droop` * length."""
    along = Vector(along).normalized()
    side = along.cross(Vector(normal)).normalized()
    if side.length < 1e-6:
        side = along.orthogonal().normalized()
    up = side.cross(along).normalized()
    rows = []
    for k in range(n + 1):
        t = k / n
        w = width * 0.5 * math.sin(math.pi * min(1.0, t ** shape)) * (1.0 - 0.1 * t)
        c = Vector(base) + along * (length * t) - up * (droop * length * t * t)
        sd = side * math.cos(twist * t) + up * math.sin(twist * t)
        upt = up * math.cos(twist * t) - side * math.sin(twist * t)
        rows.append([bm.verts.new(c - sd * w - upt * (fold * w)), bm.verts.new(c),
                     bm.verts.new(c + sd * w - upt * (fold * w))])
    faces = []
    for k in range(n):
        faces.append(bm.faces.new((rows[k][0], rows[k][1], rows[k + 1][1], rows[k + 1][0])))
        faces.append(bm.faces.new((rows[k][1], rows[k][2], rows[k + 1][2], rows[k + 1][1])))
    if col_layer is not None and col is not None:
        for f in faces:
            for loop in f.loops:
                loop[col_layer] = col
    return faces


def bm_hull(bm, points, inset=0.0, depth=0.0, seam_mat=1):
    """Convex hull of points (faceted rock); optional inset seams (material index seam_mat) between the facets."""
    tmp = bmesh.new()
    vs = [tmp.verts.new(p) for p in points]
    bmesh.ops.convex_hull(tmp, input=vs)
    for v in [v for v in tmp.verts if not v.link_faces]:
        tmp.verts.remove(v)
    bmesh.ops.dissolve_limit(tmp, angle_limit=math.radians(4), verts=tmp.verts[:], edges=tmp.edges[:])
    if inset > 0:
        faces = tmp.faces[:]
        res = bmesh.ops.inset_individual(tmp, faces=faces, thickness=inset, depth=0.0, use_even_offset=True)
        for f in res["faces"]:
            f.material_index = seam_mat
        for f in faces:
            f.material_index = 0
            if depth:
                n = f.normal.copy()
                for v in f.verts:
                    v.co += n * depth
    me = bpy.data.meshes.new("_hull_tmp")
    tmp.to_mesh(me)
    tmp.free()
    bm.from_mesh(me)
    bpy.data.meshes.remove(me)


def fib_sphere(n, seed=0):
    rnd = random.Random(seed)
    out = []
    ga = math.pi * (3 - math.sqrt(5))
    for i in range(n):
        z = 1 - 2 * (i + 0.5) / n
        r = math.sqrt(max(0.0, 1 - z * z))
        a = ga * i + rnd.uniform(-0.3, 0.3)
        out.append(Vector((r * math.cos(a), r * math.sin(a), z)))
    return out


# ---------------------------------------------------------------- objects

def collection(name, parent=None):
    coll = bpy.data.collections.get(name) or bpy.data.collections.new(name)
    par = parent or bpy.context.scene.collection
    if coll.name not in par.children:
        par.children.link(coll)
    return coll


def obj_from_bm(name, bm, mats, coll, smooth=True, tag="body", solidify=0.0, subsurf=0, bevel=0.0,
                recalc=False, sharp=None):
    """mats: one material or a list (faces keep their material_index)."""
    if recalc:
        bmesh.ops.recalc_face_normals(bm, faces=bm.faces[:])
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in (mats if isinstance(mats, (list, tuple)) else [mats]):
        me.materials.append(m)
    for poly in me.polygons:
        poly.use_smooth = smooth
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    ob["rig"] = tag
    if bevel:
        mod = ob.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
        mod.limit_method = "ANGLE"
    if solidify:
        mod = ob.modifiers.new("Solidify", "SOLIDIFY")
        mod.thickness = solidify
        mod.offset = -1.0
    if subsurf:
        mod = ob.modifiers.new("Subsurf", "SUBSURF")
        mod.levels = mod.render_levels = subsurf
    return ob


def bake_objects(objs):
    """Apply modifiers and transforms so every part sits in armature space as plain mesh data."""
    objs = [o for o in objs if o.type == "MESH"]
    if not objs:
        return
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.parent_clear(type="CLEAR_KEEP_TRANSFORM")
    bpy.ops.object.convert(target="MESH")
    bpy.ops.object.transform_apply(location=True, rotation=True, scale=True)
    bpy.ops.object.select_all(action="DESELECT")


def join(objs, name):
    objs = [o for o in objs if o.type == "MESH"]
    bpy.ops.object.select_all(action="DESELECT")
    for ob in objs:
        ob.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]
    bpy.ops.object.join()
    ob = bpy.context.view_layer.objects.active
    ob.name = ob.data.name = name
    bpy.ops.object.select_all(action="DESELECT")
    return ob


def fixed_texspace(ob):
    """Generated texture coordinates in plain meters (isotropic tiling of the box-projected textures)."""
    me = ob.data
    if hasattr(me, "use_auto_texspace"):
        me.use_auto_texspace = False
        me.texspace_location = (0.0, 0.0, 0.0)
        me.texspace_size = (1.0, 1.0, 1.0)


def tri_count(ob):
    return sum(len(p.vertices) - 2 for p in ob.data.polygons)


def look_rot(direction, up=Vector((0, 0, 1))):
    """Rotation matrix whose +Z points along direction (local +X as close as possible to up x direction)."""
    z = Vector(direction).normalized()
    x = Vector(up).cross(z)
    if x.length < 1e-6:
        x = z.orthogonal()
    x.normalize()
    y = z.cross(x)
    return Matrix((x, y, z)).transposed()
