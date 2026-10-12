"""Mesh kit of the pistol pack (T-248): exterior game geometry with one UV atlas per weapon.

Adapted from the second PC's bean-pirate-pistol/scripts/meshkit.py (read only) and extended for texture baking:
every primitive gets its own UV island (parametric strip + polar caps for lathes, tubes and lofts, six box tiles for
the rest), the islands are packed into one atlas at a uniform texel density (shelf packer), then the parts of one bone
are merged into one mesh (materials, UVs and smoothing preserved, rigid vertex group on the bone).

Weapon coordinates: +X forward (muzzle), +Z up, -Y lock side (the right side of the weapon), right grip at the origin.
"""
from __future__ import annotations

import json
import math

import bmesh
import bpy
from mathutils import Matrix, Vector

TAU = 2 * math.pi


def principled(mat):
    return next((n for n in mat.node_tree.nodes if n.type == 'BSDF_PRINCIPLED'), None)


def _strip_uvs(rows, seg, xs, length, circ, radius, caps):
    """Face UVs (island space [0,1]^2) of a lathe/tube: side strip + two polar caps on the right.
    rows = vertex index rows (seg+1 each); returns (face_uvs in face order side..., caps, island size (w, h))."""
    r = max(radius, 1e-4)
    cap_w = 2.2 * r if caps else 0.0
    W = max(length, 1e-5) + 2 * cap_w
    H = max(circ, 2.2 * r if caps else 1e-5)
    face_uvs = []
    for k in range(len(rows) - 1):
        u0, u1 = xs[k] / W, xs[k + 1] / W
        for j in range(seg):
            v0, v1 = (j / seg) * circ / H, ((j + 1) / seg) * circ / H
            face_uvs.append([(u0, v0), (u0, v1), (u1, v1), (u1, v0)])
    if caps:
        for c, (cx, order) in enumerate(((length + cap_w * .5, reversed(range(seg))), (length + cap_w * 1.5, range(seg)))):
            poly = []
            for j in order:
                a = j * TAU / seg
                poly.append(((cx + r * math.cos(a)) / W, (1.1 * r + r * math.sin(a)) / H))
            face_uvs.append(poly)
    return face_uvs, (W, H)


class Builder:
    """Collects parts (one object each) with their bone, material and UV island."""

    def __init__(self, collection, prefix, materials):
        self.collection, self.prefix, self.materials = collection, prefix, materials
        self.meshes = []
        self.bindings = {}
        self.islands = {}      # object name -> (width_m, height_m) of its UV island (uv in [0,1]^2)

    # ------------------------------------------------------------------ core
    def mesh(self, label, vertices, faces, material='steel', bone='gun_root', smooth=False, face_uvs=None, island=None,
             flat_faces=()):
        name = 'GEO-' + self.prefix + '-' + label
        me = bpy.data.meshes.new(name)
        me.from_pydata(vertices, [], faces)
        me.update()
        ob = bpy.data.objects.new(name, me)
        self.collection.objects.link(ob)
        mats = material if isinstance(material, (list, tuple)) else [material]
        for m in mats:
            me.materials.append(self.materials[m] if isinstance(m, str) else m)
        flat = set(flat_faces)
        for poly in me.polygons:
            poly.use_smooth = smooth and poly.index not in flat
        uv = me.uv_layers.new(name='UVMap')
        if face_uvs is not None:
            for poly, coords in zip(me.polygons, face_uvs):
                for li, co in zip(poly.loop_indices, coords):
                    uv.data[li].uv = co
            self.islands[ob.name] = island
        else:
            self.islands[ob.name] = island or box_project(me)
        self.meshes.append(ob)
        self.bindings[ob.name] = bone
        return ob

    # ------------------------------------------------------------- primitives
    def rings(self, label, profile, center=(0, 0, 0), mat='steel', bone='gun_root', seg=32, caps=True, flat=False):
        """Lathe along X. profile = [(x, radius_y, radius_z[, z_offset[, y_offset]])]; flat = faceted (octagonal)."""
        cx, cy, cz = center
        verts, rows, xs = [], [], []
        mean_r = sum((p[1] + p[2]) * .5 for p in profile) / len(profile)
        along = 0.0
        prev = None
        for p in profile:
            x, ry, rz = p[:3]
            zo = p[3] if len(p) > 3 else 0.0
            yo = p[4] if len(p) > 4 else 0.0
            row = []
            for j in range(seg + 1):
                a = j * TAU / seg + (math.pi / seg if flat else 0.0)
                verts.append((cx + x, cy + yo + ry * math.cos(a), cz + zo + rz * math.sin(a)))
                row.append(len(verts) - 1)
            rows.append(row)
            # strip coordinate: distance along the profile in the (x, radius) plane, so annuli get an area too
            if prev is not None:
                along += math.hypot(x - prev[0], (ry + rz) * .5 - prev[1])
            prev = (x, (ry + rz) * .5)
            xs.append(along)
        length = along or 1e-5
        faces = []
        for k in range(len(profile) - 1):
            for j in range(seg):
                a = rows[k][j]
                faces.append((a, a + 1, a + seg + 2, a + seg + 1))
        nside = len(faces)
        if caps:
            faces.append(tuple(reversed(rows[0][:seg])))
            faces.append(tuple(rows[-1][:seg]))
        face_uvs, island = _strip_uvs(rows, seg, xs, length, TAU * mean_r, mean_r, caps)
        return self.mesh(label, verts, faces, mat, bone, not flat, face_uvs, island,
                         flat_faces=range(nside, nside + 2) if caps else ())

    def tube(self, label, points, radii, mat='steel', bone='gun_root', seg=16, caps=True, up=None, squash=1.0):
        """Round tube along a polyline, radius per point (rods, wires, guards, hooks); squash < 1 flattens the
        section along the second frame axis (t x a: with up = +Y it is the Y thickness of a flat bar)."""
        pts = [Vector(p) for p in points]
        if isinstance(radii, (int, float)):
            radii = [radii] * len(pts)
        verts, rows, xs = [], [], []
        along = 0.0
        prev_a = None
        for i, p in enumerate(pts):
            t = (pts[min(len(pts) - 1, i + 1)] - pts[max(0, i - 1)]).normalized()
            ref = Vector(up) if up else (Vector((0, 0, 1)) if abs(t.z) < .9 else Vector((0, 1, 0)))
            a = t.cross(ref).normalized()
            if prev_a is not None and a.dot(prev_a) < 0:
                a = -a
            b = t.cross(a).normalized()
            prev_a = a
            if i:
                along += (p - pts[i - 1]).length
            row = []
            for j in range(seg + 1):
                ang = j * TAU / seg
                verts.append(p + radii[i] * (a * math.cos(ang) + b * (math.sin(ang) * squash)))
                row.append(len(verts) - 1)
            rows.append(row)
            xs.append(along)
        total = along or 1e-5
        faces = []
        for i in range(len(pts) - 1):
            for j in range(seg):
                a = rows[i][j]
                faces.append((a, a + 1, a + seg + 2, a + seg + 1))
        nside = len(faces)
        if caps:
            faces.append(tuple(reversed(rows[0][:seg])))
            faces.append(tuple(rows[-1][:seg]))
        mean_r = sum(radii) / len(radii)
        face_uvs, island = _strip_uvs(rows, seg, xs, total, TAU * mean_r, mean_r, caps)
        return self.mesh(label, verts, faces, mat, bone, True, face_uvs, island,
                         flat_faces=range(nside, nside + 2) if caps else ())

    def rod(self, label, points, radius=.0015, mat='gold', bone='gun_root', seg=8):
        return self.tube(label, points, radius, mat, bone, seg)

    def box(self, label, center, size, mat='steel', bone='gun_root', bevel=.002, rot=None):
        """Bevelled box; rot = (rx, ry, rz) degrees about its centre."""
        c = Vector(center)
        s = Vector(size) * .5
        R = Matrix.Identity(3)
        if rot:
            R = (Matrix.Rotation(math.radians(rot[2]), 3, 'Z') @ Matrix.Rotation(math.radians(rot[1]), 3, 'Y')
                 @ Matrix.Rotation(math.radians(rot[0]), 3, 'X'))
        verts = [c + R @ Vector((x * s.x, y * s.y, z * s.z)) for x, y, z in
                 [(-1, -1, -1), (-1, -1, 1), (-1, 1, 1), (-1, 1, -1), (1, -1, -1), (1, -1, 1), (1, 1, 1), (1, 1, -1)]]
        faces = [(0, 3, 2, 1), (4, 5, 6, 7), (0, 1, 5, 4), (3, 7, 6, 2), (1, 2, 6, 5), (0, 4, 7, 3)]
        ob = self.mesh(label, verts, faces, mat, bone, False)
        if bevel:
            bm = bmesh.new()
            bm.from_mesh(ob.data)
            bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(bevel, min(size) * .32), segments=2, affect='EDGES')
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
            bm.to_mesh(ob.data)
            bm.free()
            ob.data.update()
            for poly in ob.data.polygons:
                poly.use_smooth = True
            self.islands[ob.name] = box_project(ob.data)
        return ob

    def disc(self, label, center, radius, thickness, normal, mat='brass', bone='gun_root', seg=20, dome=.6):
        """Round head lying on a surface (screw heads, pins, buttons), centre on the surface, normal outwards."""
        n = Vector(normal).normalized()
        q = Vector((0, 0, 1)).rotation_difference(n)
        verts, rows = [], []
        for z, r in ((-thickness * .5, radius), (thickness * .5, radius * .92), (thickness * .5 + thickness * dome, radius * .55)):
            row = []
            for i in range(seg):
                a = TAU * i / seg
                verts.append(Vector(center) + q @ Vector((r * math.cos(a), r * math.sin(a), z)))
                row.append(len(verts) - 1)
            rows.append(row)
        faces = []
        for r0, r1 in zip(rows, rows[1:]):
            for i in range(seg):
                faces.append((r0[i], r0[(i + 1) % seg], r1[(i + 1) % seg], r1[i]))
        faces.append(tuple(reversed(rows[0])))
        faces.append(tuple(rows[-1]))
        return self.mesh(label, verts, faces, mat, bone, True)

    def screw(self, label, pos, radius=.004, normal=(0, -1, 0), mat='edge', bone='gun_root', slot=True, slot_angle=0.0):
        n = Vector(normal).normalized()
        ob = self.disc(label, pos, radius, .0012, n, mat, bone, 16, .5)
        if slot:
            q = Vector((0, 0, 1)).rotation_difference(n)
            side = q @ Vector((math.cos(slot_angle), math.sin(slot_angle), 0))
            top = Vector(pos) + n * .0019
            self.tube(label + '-Slot', [top - side * radius * .7, top + side * radius * .7], radius * .14, 'recess', bone, 6)
        return ob

    def loft(self, label, sections, mat='wood', bone='gun_root', seg=32, expo=2.4, closed=True, frame=None):
        """Swept super-elliptic sections along a 3D centre line: sections = [(centre xyz, half_y, half_z, tilt_deg)].
        tilt rotates the section about Y (grip sections lean back). frame = (a, b) fixes the section axes (a loft
        that goes up then back down, like a bag with an inner wall, must not flip its winding)."""
        pts = [Vector(s[0]) for s in sections]
        verts, rows, xs = [], [], []
        along = 0.0
        mean_c = 0.0
        for i, (c, hy, hz, tilt) in enumerate(sections):
            t = (pts[min(len(pts) - 1, i + 1)] - pts[max(0, i - 1)]).normalized()
            if frame:
                a, b = Vector(frame[0]), Vector(frame[1])
            else:
                a = Vector((0, 1, 0))
                b = t.cross(a).normalized()
            if tilt:
                b = Matrix.Rotation(math.radians(tilt), 3, 'Y') @ b
            if i:
                along += (pts[i] - pts[i - 1]).length
            mean_c += TAU * (hy + hz) * .5
            row = []
            for j in range(seg + 1):
                ang = j * TAU / seg
                cs, sn = math.cos(ang), math.sin(ang)
                y = hy * math.copysign(abs(cs) ** (2 / expo), cs)
                z = hz * math.copysign(abs(sn) ** (2 / expo), sn)
                verts.append(pts[i] + a * y + b * z)
                row.append(len(verts) - 1)
            rows.append(row)
            xs.append(along)
        total = along or 1e-5
        faces = []
        for i in range(len(sections) - 1):
            for j in range(seg):
                n = rows[i][j]
                faces.append((n, n + 1, n + seg + 2, n + seg + 1))
        nside = len(faces)
        if closed:
            faces.append(tuple(reversed(rows[0][:seg])))
            faces.append(tuple(rows[-1][:seg]))
        circ = mean_c / len(sections)
        face_uvs, island = _strip_uvs(rows, seg, xs, total, circ, circ / TAU, closed)
        return self.mesh(label, verts, faces, mat, bone, True, face_uvs, island,
                         flat_faces=range(nside, nside + 2) if closed else ())

    def plate(self, label, outline, y, thickness, mat='steel', bone='gun_root', bevel=.0008):
        """Flat plate in the XZ plane (lock plate, side plate, escutcheon): outline = [(x, z)] counter-clockwise seen
        from -Y (the right side), extruded from y to y + thickness."""
        n = len(outline)
        y0, y1 = (y, y + thickness) if thickness > 0 else (y + thickness, y)
        verts = [(x, y0, z) for x, z in outline] + [(x, y1, z) for x, z in outline]
        faces = [tuple(range(n)), tuple(reversed(range(n, 2 * n)))]
        for i in range(n):
            j = (i + 1) % n
            faces.append((j, i, i + n, j + n))
        ob = self.mesh(label, verts, faces, mat, bone, True, flat_faces=(0, 1))
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        if bevel:
            bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(bevel, abs(thickness) * .4), segments=1, affect='EDGES')
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(ob.data)
        bm.free()
        ob.data.update()
        for poly in ob.data.polygons:
            poly.use_smooth = abs(poly.normal.y) < .95
        self.islands[ob.name] = box_project(ob.data)
        return ob

    def plate_xy(self, label, outline, z, thickness, mat='steel', bone='gun_root', bevel=.0006):
        """Flat plate in the XY plane (breech tang, escutcheon): outline = [(x, y)] counter-clockwise seen from
        above, extruded from z to z + thickness."""
        n = len(outline)
        z0, z1 = (z, z + thickness) if thickness > 0 else (z + thickness, z)
        verts = [(x, y, z0) for x, y in outline] + [(x, y, z1) for x, y in outline]
        faces = [tuple(reversed(range(n))), tuple(range(n, 2 * n))]
        for i in range(n):
            j = (i + 1) % n
            faces.append((i, j, j + n, i + n))
        ob = self.mesh(label, verts, faces, mat, bone, True, flat_faces=(0, 1))
        bm = bmesh.new()
        bm.from_mesh(ob.data)
        bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        if bevel:
            bmesh.ops.bevel(bm, geom=list(bm.edges), offset=min(bevel, abs(thickness) * .4), segments=1, affect='EDGES')
            bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
        bm.to_mesh(ob.data)
        bm.free()
        ob.data.update()
        for poly in ob.data.polygons:
            poly.use_smooth = abs(poly.normal.z) < .95
        self.islands[ob.name] = box_project(ob.data)
        return ob

    def chip(self, label, center, size, mat='flint', bone='gun_root', seed=3):
        """Faceted flint chip."""
        bm = bmesh.new()
        bmesh.ops.create_icosphere(bm, subdivisions=1, radius=.5)
        k = seed
        for v in bm.verts:
            k = (k * 1103515245 + 12345) % 2 ** 31
            jitter = .8 + .35 * (k / 2 ** 31)
            v.co = Vector((v.co.x * size[0] * jitter, v.co.y * size[1] * jitter, v.co.z * size[2] * (.9 + .2 * jitter)))
        bm.verts.ensure_lookup_table()
        verts = [tuple(Vector(center) + v.co) for v in bm.verts]
        faces = [tuple(v.index for v in f.verts) for f in bm.faces]
        bm.free()
        return self.mesh(label, verts, faces, mat, bone, False)

    def ellipsoid(self, label, center, radii, mat='brass', bone='gun_root', seg=24, ring=12):
        verts, rows = [], []
        for i in range(ring + 1):
            phi = math.pi * i / ring
            row = []
            for j in range(seg + 1):
                th = TAU * j / seg
                verts.append((center[0] + radii[0] * math.sin(phi) * math.cos(th),
                              center[1] + radii[1] * math.sin(phi) * math.sin(th), center[2] + radii[2] * math.cos(phi)))
                row.append(len(verts) - 1)
            rows.append(row)
        faces, face_uvs = [], []
        for i in range(ring):
            for j in range(seg):
                a = rows[i][j]
                faces.append((a, a + seg + 1, a + seg + 2, a + 1))
                face_uvs.append([(j / seg, i / ring), (j / seg, (i + 1) / ring), ((j + 1) / seg, (i + 1) / ring), ((j + 1) / seg, i / ring)])
        return self.mesh(label, verts, faces, mat, bone, True, face_uvs,
                         (TAU * (radii[0] + radii[1]) * .5, math.pi * radii[2]))


# ------------------------------------------------------------------------- UV helpers

def box_project(me):
    """Six tiles by dominant normal (+X -X +Y -Y on a row, +Z -Z below); returns the island size (m)."""
    verts = [v.co for v in me.vertices]
    lo = Vector([min(v[i] for v in verts) for i in range(3)])
    hi = Vector([max(v[i] for v in verts) for i in range(3)])
    size = hi - lo
    w, d, h = max(size.x, 1e-5), max(size.y, 1e-5), max(size.z, 1e-5)
    row1 = [('+X', d), ('-X', d), ('+Y', w), ('-Y', w)]
    island_w = max(sum(t[1] for t in row1), 2 * w)
    island_h = h + d
    uv = me.uv_layers.active or me.uv_layers.new(name='UVMap')
    offsets = {}
    u = 0.0
    for key, tw in row1:
        offsets[key] = u
        u += tw
    for poly in me.polygons:
        n = poly.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        sign = '+' if n[ax] >= 0 else '-'
        key = sign + 'XYZ'[ax]
        if ax == 2:
            u0, v0, tw, th = (0.0 if sign == '+' else w), 0.0, w, d
        else:
            u0, v0, tw, th = offsets[key], d, dict(row1)[key], h
        for li, vi in zip(poly.loop_indices, poly.vertices):
            p = verts[vi] - lo
            if ax == 0:
                a, b = p.y / d, p.z / h
                if sign == '+':
                    a = 1 - a
            elif ax == 1:
                a, b = p.x / w, p.z / h
                if sign == '-':
                    a = 1 - a
            else:
                a, b = p.x / w, p.y / d
                if sign == '-':
                    b = 1 - b
            uv.data[li].uv = ((u0 + a * tw) / island_w, (v0 + b * th) / island_h)
    return (island_w, island_h)


def pack_islands(objects, islands, atlas_size, padding_px=6):
    """Shelf packing of the islands (object -> (w_m, h_m)) at one texel density; rewrites the UVs of every object
    into [0,1]^2 of the atlas. Returns the texel density (px/m)."""
    items = []
    for ob in objects:
        w, h = islands[ob.name]
        items.append((ob, max(w, 1e-4), max(h, 1e-4)))
    area = sum(w * h for _, w, h in items)
    pad = padding_px / atlas_size
    # every island landscape, rows sorted by height: homogeneous shelves, little waste
    items = [(ob, w, h, False) if w >= h else (ob, h, w, True) for ob, w, h in items]
    order = sorted(items, key=lambda t: (-t[2], -t[1]))

    def try_pack(scale):
        placed = {}
        x = y = row_h = 0.0
        for ob, w, h, rot in order:
            iw, ih = w * scale, h * scale
            if iw > 1 - 2 * pad:
                return None
            if x + iw + pad > 1:
                x = 0.0
                y += row_h + pad
                row_h = 0.0
            if y + ih + pad > 1:
                return None
            placed[ob.name] = (x + pad * .5, y + pad * .5, iw, ih, rot)
            x += iw + pad
            row_h = max(row_h, ih)
        return placed

    lo, hi = 0.0, math.sqrt(1.0 / area)
    best = None
    for _ in range(48):
        mid = (lo + hi) * .5
        p = try_pack(mid)
        if p:
            best, lo = (mid, p), mid
        else:
            hi = mid
    if best is None:
        raise RuntimeError('UV atlas: nothing fits')
    scale, placed = best
    for ob in objects:
        x, y, iw, ih, rot = placed[ob.name]
        uv = ob.data.uv_layers.active
        for d in uv.data:
            u, v = d.uv
            if rot:
                u, v = v, 1 - u
            d.uv = (x + u * iw, y + v * ih)
    return scale * atlas_size


def combine(objects, collection, name):
    """Merge parts into one mesh without operators: materials, UVs and smoothing preserved."""
    vertices, faces, materials, face_materials, smooth, face_uvs = [], [], [], [], [], []
    for ob in objects:
        start = len(vertices)
        vertices.extend(tuple(v.co) for v in ob.data.vertices)
        material_map = {}
        for i, mat in enumerate(ob.data.materials):
            if mat not in materials:
                materials.append(mat)
            material_map[i] = materials.index(mat)
        uv = ob.data.uv_layers.active
        for poly in ob.data.polygons:
            faces.append(tuple(start + i for i in poly.vertices))
            face_materials.append(material_map.get(poly.material_index, 0))
            smooth.append(poly.use_smooth)
            face_uvs.append([tuple(uv.data[i].uv) if uv else (0, 0) for i in poly.loop_indices])
    me = bpy.data.meshes.new(name)
    me.from_pydata(vertices, [], faces)
    me.update()
    for mat in materials:
        me.materials.append(mat)
    uv = me.uv_layers.new(name='UVMap')
    for poly, mi, sm, coords in zip(me.polygons, face_materials, smooth, face_uvs):
        poly.material_index = mi
        poly.use_smooth = sm
        for li, co in zip(poly.loop_indices, coords):
            uv.data[li].uv = co
    result = bpy.data.objects.new(name, me)
    collection.objects.link(result)
    result['parts'] = json.dumps([ob.name for ob in objects])
    for ob in objects:
        old = ob.data
        bpy.data.objects.remove(ob, do_unlink=True)
        if not old.users:
            bpy.data.meshes.remove(old)
    result.name = name
    me.name = name
    return result


def skin(ob, arm, bone):
    vg = ob.vertex_groups.new(name=bone)
    vg.add(list(range(len(ob.data.vertices))), 1.0, 'REPLACE')
    mod = ob.modifiers.new('Armature', 'ARMATURE')
    mod.object = arm
    ob.parent = arm


def triangles(objects):
    return sum(sum(max(0, len(p.vertices) - 2) for p in o.data.polygons) for o in objects)


def bounds(objects, evaluated=True):
    vs = []
    dg = bpy.context.evaluated_depsgraph_get() if evaluated else None
    for ob in objects:
        ev = ob.evaluated_get(dg) if evaluated else ob
        me = ev.to_mesh() if evaluated else ob.data
        vs.extend(ev.matrix_world @ v.co for v in me.vertices)
        if evaluated:
            ev.to_mesh_clear()
    lo = [min(v[i] for v in vs) for i in range(3)]
    hi = [max(v[i] for v in vs) for i in range(3)]
    return {'min': lo, 'max': hi, 'size': [hi[i] - lo[i] for i in range(3)]}
