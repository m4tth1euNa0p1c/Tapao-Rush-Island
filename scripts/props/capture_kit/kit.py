"""Geometry, materials, sockets, collisions and exports of the capture kit (T-337).

MeshBuilder writes faces with their material and per-loop UVs at once: boxes (optionally chamfered) mapped along their
grain axis, cylinders and swept tubes mapped by arc length, all in metres so the tiling CC0 textures keep a real scale
(TILE: metres per UV unit). Static exports follow scripts/props/barque.py (FBX_SCALE_NONE, -Y forward / Z up: Blender +X is
Unreal +X, Blender +Y is Unreal -Y), with UCX_<Mesh>_NN convex boxes and SOCKET_<name> empties parented to the mesh.
"""
from __future__ import annotations

import json
import math
import random
import time
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

TILE = {'M_Capture_Wood': 0.6, 'M_Capture_Iron': 1.0, 'M_Capture_Steel': 0.35}
MATERIAL_TEXTURE = {'M_Capture_Wood': 'Wood', 'M_Capture_Iron': 'Iron', 'M_Capture_Steel': 'Steel'}


def write_json(path, data):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    text = json.dumps(data, indent=1, ensure_ascii=False)
    for attempt in range(8):          # Malwarebytes can lock a file just written: try again
        try:
            path.write_text(text, encoding='utf-8')
            return path
        except PermissionError:
            if attempt == 7:
                raise
            time.sleep(1.0)


def frame_from(x_axis, up_hint):
    """Rotation matrix (3x3) whose X is x_axis and Z is as close as possible to up_hint"""
    x = Vector(x_axis).normalized()
    up = Vector(up_hint)
    z = up - x * up.dot(x)
    if z.length < 1e-6:
        z = Vector((0, 0, 1)) - x * x.z
        if z.length < 1e-6:
            z = Vector((0, 1, 0)) - x * x.y
    z.normalize()
    y = z.cross(x)
    return Matrix((x, y, z)).transposed()


class MeshBuilder:
    """Accumulates faces (material + loop UVs + smooth flag), builds one mesh object."""

    def __init__(self, name, seed=1):
        self.name = name
        self.verts = []
        self.faces = []
        self.rng = random.Random(seed)

    # -- low level
    def vert(self, co):
        self.verts.append(Vector(co))
        return len(self.verts) - 1

    def face(self, idx, mat, uvs, smooth=False):
        idx = tuple(idx)
        uvs = [tuple(uv) for uv in uvs]
        if len(idx) > 4:            # fan of triangles: no n-gon in the FBX (tangent space exported, Unreal triangulates)
            for k in range(1, len(idx) - 1):
                self.faces.append(((idx[0], idx[k], idx[k + 1]), mat, [uvs[0], uvs[k], uvs[k + 1]], smooth))
            return
        self.faces.append((idx, mat, uvs, smooth))

    def offset(self):
        return Vector((self.rng.random() * 7.0, self.rng.random() * 7.0))

    # -- primitives
    def box(self, center, size, mat, rot=None, chamfer=0.0, grain=None, tile=None):
        """Box of size (local x, y, z) at center, rotated by rot (3x3); chamfer cuts the 12 edges; grain = local axis
        (0, 1, 2) the texture's V follows (the wood grain), default the longest side"""
        R = rot if rot is not None else Matrix.Identity(3)
        h = [s / 2 for s in size]
        c = min(chamfer, min(h) * 0.45)
        tile = tile or TILE.get(mat, 1.0)
        grain = grain if grain is not None else max(range(3), key=lambda i: size[i])
        off = self.offset()
        C = Vector(center)
        locs = {}
        if c <= 0:
            for sx in (-1, 1):
                for sy in (-1, 1):
                    for sz in (-1, 1):
                        locs[(sx, sy, sz)] = Vector((sx * h[0], sy * h[1], sz * h[2]))
            idx = {k: self.vert(C + R @ v) for k, v in locs.items()}
            quads = [((1, -1, -1), (1, 1, -1), (1, 1, 1), (1, -1, 1)), ((-1, 1, -1), (-1, -1, -1), (-1, -1, 1), (-1, 1, 1)),
                     ((1, 1, -1), (-1, 1, -1), (-1, 1, 1), (1, 1, 1)), ((-1, -1, -1), (1, -1, -1), (1, -1, 1), (-1, -1, 1)),
                     ((-1, -1, 1), (1, -1, 1), (1, 1, 1), (-1, 1, 1)), ((-1, 1, -1), (1, 1, -1), (1, -1, -1), (-1, -1, -1))]
            for q in quads:
                pts = [locs[k] for k in q]
                self._box_face([idx[k] for k in q], pts, mat, grain, tile, off)
            return
        # chamfered: three points per corner (one on each face plane)
        P = {}
        for sx in (-1, 1):
            for sy in (-1, 1):
                for sz in (-1, 1):
                    s = (sx, sy, sz)
                    P[(s, 0)] = Vector((sx * h[0], sy * (h[1] - c), sz * (h[2] - c)))
                    P[(s, 1)] = Vector((sx * (h[0] - c), sy * h[1], sz * (h[2] - c)))
                    P[(s, 2)] = Vector((sx * (h[0] - c), sy * (h[1] - c), sz * h[2]))
        idx = {k: self.vert(C + R @ v) for k, v in P.items()}
        polys = []
        for axis in range(3):
            for sign in (-1, 1):
                corners = [s for s in P if s[1] == axis and s[0][axis] == sign]
                polys.append([k for k in corners])
        for a in range(3):
            for b in range(a + 1, 3):
                third = 3 - a - b
                for sa in (-1, 1):
                    for sb in (-1, 1):
                        ring = []
                        for st in (-1, 1):
                            s = [0, 0, 0]
                            s[a], s[b], s[third] = sa, sb, st
                            s = tuple(s)
                            ring += [(s, a), (s, b)]
                        polys.append(ring)
        for s in {k[0] for k in P}:
            polys.append([(s, 0), (s, 1), (s, 2)])
        for poly in polys:
            pts = [P[k] for k in poly]
            centre = sum(pts, Vector()) / len(pts)
            # order around the face centre, counter-clockwise seen from outside (normal = centre direction)
            n = centre.normalized()
            u = (pts[0] - centre).normalized()
            v = n.cross(u)
            order = sorted(range(len(pts)), key=lambda i: math.atan2((pts[i] - centre).dot(v), (pts[i] - centre).dot(u)))
            keys = [poly[i] for i in order]
            self._box_face([idx[k] for k in keys], [P[k] for k in keys], mat, grain, tile, off)

    def _box_face(self, ids, pts, mat, grain, tile, off):
        e1, e2 = pts[1] - pts[0], pts[2] - pts[0]
        n = e1.cross(e2)
        centre = sum(pts, Vector()) / len(pts)
        if n.dot(centre) < 0:          # box local space: the outward normal points away from the centre
            ids, pts = ids[::-1], pts[::-1]
            n = -n
        k = max(range(3), key=lambda i: abs(n[i]))
        plane = [i for i in range(3) if i != k]
        if grain in plane:
            vax = grain
            uax = plane[0] if plane[1] == grain else plane[1]
        else:
            uax, vax = plane
        uvs = [(p[uax] / tile + off.x, p[vax] / tile + off.y) for p in pts]
        self.face(ids, mat, uvs, smooth=False)

    def cylinder(self, p0, p1, r, mat, segs=10, caps=True, tile=None, smooth=True, r1=None):
        p0, p1 = Vector(p0), Vector(p1)
        axis = p1 - p0
        length = axis.length
        R = frame_from(axis, Vector((0, 0, 1)) if abs(axis.normalized().z) < 0.9 else Vector((1, 0, 0)))
        # R: X along the axis
        tile = tile or TILE.get(mat, 1.0)
        off = self.offset()
        r1 = r if r1 is None else r1
        rings = []
        for p, rad in ((p0, r), (p1, r1)):
            ring = []
            for i in range(segs):
                a = 2 * math.pi * i / segs
                ring.append(self.vert(p + R @ Vector((0, math.cos(a) * rad, math.sin(a) * rad))))
            rings.append(ring)
        for i in range(segs):
            j = (i + 1) % segs
            u0, u1 = i / segs, (i + 1) / segs
            circ = 2 * math.pi * max(r, r1) / tile
            self.face([rings[0][i], rings[0][j], rings[1][j], rings[1][i]], mat,
                      [(u0 * circ + off.x, off.y), (u1 * circ + off.x, off.y), (u1 * circ + off.x, length / tile + off.y),
                       (u0 * circ + off.x, length / tile + off.y)], smooth)
        if caps:
            for ring, rad, flip in ((rings[0], r, True), (rings[1], r1, False)):
                ids = ring[::-1] if flip else ring
                uvs = []
                for k in ids:
                    q = R.transposed() @ (self.verts[k] - (p0 if flip else p1))
                    uvs.append((q.y / tile + off.x, q.z / tile + off.y))
                self.face(ids, mat, uvs, False)

    def tube(self, pts, radius, mat, segs=8, closed=False, tile=None, smooth=True, up=None, profile=None, caps=True):
        """Sweep along pts: circle of radius, or profile [(y, z), ...] (closed polygon in the section plane, counter-
        clockwise), with parallel transported frames; closed joins the end to the start"""
        pts = [Vector(p) for p in pts]
        n = len(pts)
        tile = tile or TILE.get(mat, 1.0)
        off = self.offset()
        if profile is None:
            profile = [(math.cos(2 * math.pi * i / segs) * radius, math.sin(2 * math.pi * i / segs) * radius) for i in range(segs)]
        segs = len(profile)
        tangents = []
        for i in range(n):
            if closed:
                t = pts[(i + 1) % n] - pts[i - 1]
            else:
                t = pts[min(i + 1, n - 1)] - pts[max(i - 1, 0)]
            tangents.append(t.normalized())
        ref = Vector(up) if up is not None else (Vector((0, 0, 1)) if abs(tangents[0].z) < 0.9 else Vector((1, 0, 0)))
        frames = []
        F = frame_from(tangents[0], ref)
        frames.append(F)
        for i in range(1, n):
            prev = frames[-1]
            q = Vector(prev.col[0]).rotation_difference(tangents[i])
            frames.append(q.to_matrix() @ prev)
        if closed and n > 2:
            # spread the twist mismatch at the seam along the loop
            last = frames[-1]
            q = Vector(last.col[0]).rotation_difference(tangents[0])
            end = q.to_matrix() @ last
            y0, y1 = Vector(frames[0].col[1]), Vector(end.col[1])
            x0 = Vector(frames[0].col[0])
            twist = math.atan2(y1.cross(y0).dot(x0), y1.dot(y0))
            for i in range(n):
                frames[i] = Matrix.Rotation(twist * i / n, 3, Vector(frames[i].col[0])) @ frames[i]
        rings = []
        lengths = [0.0]
        for i in range(1, n + (1 if closed else 0)):
            lengths.append(lengths[-1] + (pts[i % n] - pts[i - 1]).length)
        for i in range(n):
            ring = [self.vert(pts[i] + frames[i] @ Vector((0, y, z))) for y, z in profile]
            rings.append(ring)
        perim = sum((Vector(profile[(k + 1) % segs]) - Vector(profile[k])).length for k in range(segs))
        acc = [0.0]
        for k in range(segs):
            acc.append(acc[-1] + (Vector(profile[(k + 1) % segs]) - Vector(profile[k])).length)
        last = n if closed else n - 1
        for i in range(last):
            a, b = rings[i], rings[(i + 1) % n]
            v0, v1 = lengths[i] / tile, lengths[i + 1] / tile
            for k in range(segs):
                kk = (k + 1) % segs
                u0, u1 = acc[k] / tile, acc[k + 1] / tile
                self.face([a[k], a[kk], b[kk], b[k]], mat,
                          [(u0 + off.x, v0 + off.y), (u1 + off.x, v0 + off.y), (u1 + off.x, v1 + off.y), (u0 + off.x, v1 + off.y)],
                          smooth)
        if not closed and caps:
            for ring, flip, p, F in ((rings[0], True, pts[0], frames[0]), (rings[-1], False, pts[-1], frames[-1])):
                ids = ring[::-1] if flip else ring
                uvs = []
                for k in ids:
                    q = F.transposed() @ (self.verts[k] - p)
                    uvs.append((q.y / tile + off.x, q.z / tile + off.y))
                self.face(ids, mat, uvs, False)
        return perim

    def torus(self, center, axis, R, r, mat, segs=24, ring_segs=8, up=None, arc=None, tile=None):
        """Ring of major radius R around axis (closed), or an arc (start_deg, end_deg) open tube"""
        axis = Vector(axis).normalized()
        F = frame_from(axis, up if up is not None else (Vector((0, 0, 1)) if abs(axis.z) < 0.9 else Vector((1, 0, 0))))
        a0, a1 = (0.0, 360.0) if arc is None else arc
        closed = arc is None
        count = segs if closed else max(2, int(round(segs * (a1 - a0) / 360.0)) + 1)
        pts = []
        for i in range(count):
            t = math.radians(a0 + (a1 - a0) * i / (count if closed else count - 1))
            pts.append(Vector(center) + F @ Vector((0, math.cos(t) * R, math.sin(t) * R)))
        self.tube(pts, r, mat, segs=ring_segs, closed=closed, tile=tile)

    def transform(self, M, start=0):
        for i in range(start, len(self.verts)):
            self.verts[i] = M @ self.verts[i]

    # -- build
    def build(self, coll, materials):
        names = []
        for _f, mat, _uv, _s in self.faces:
            if mat not in names:
                names.append(mat)
        me = bpy.data.meshes.new(self.name)
        me.from_pydata([tuple(v) for v in self.verts], [], [f[0] for f in self.faces])
        me.update()
        for m in names:
            me.materials.append(materials[m])
        uv = me.uv_layers.new(name='UVMap')
        for poly, (ids, mat, uvs, smooth) in zip(me.polygons, self.faces):
            poly.material_index = names.index(mat)
            poly.use_smooth = smooth
            for li, uvco in zip(poly.loop_indices, uvs):
                uv.data[li].uv = uvco
        me.validate(clean_customdata=False)
        ob = bpy.data.objects.new(self.name, me)
        coll.objects.link(ob)
        return ob


# ----------------------------------------------------------------------------------------------- materials

def material(name, textures):
    """Preview / export material: the kit's PBR textures (GL normal for Blender), ORM split to roughness / metallic"""
    m = bpy.data.materials.get(name)
    if m is not None:
        return m
    m = bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
    paths = textures(MATERIAL_TEXTURE[name])
    tex = nt.nodes.new('ShaderNodeTexImage')
    tex.image = bpy.data.images.load(str(paths['D']), check_existing=True)
    tex.label = 'BaseColor'
    orm = nt.nodes.new('ShaderNodeTexImage')
    orm.image = bpy.data.images.load(str(paths['ORM']), check_existing=True)
    orm.image.colorspace_settings.name = 'Non-Color'
    orm.label = 'ORM'
    nor = nt.nodes.new('ShaderNodeTexImage')
    nor.image = bpy.data.images.load(str(paths['N_GL']), check_existing=True)
    nor.image.colorspace_settings.name = 'Non-Color'
    nor.label = 'Normal'
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    nm = nt.nodes.new('ShaderNodeNormalMap')
    nt.links.new(tex.outputs['Color'], bsdf.inputs['Base Color'])
    nt.links.new(orm.outputs['Color'], sep.inputs['Color'])
    nt.links.new(sep.outputs[1], bsdf.inputs['Roughness'])
    nt.links.new(sep.outputs[2], bsdf.inputs['Metallic'])
    nt.links.new(nor.outputs['Color'], nm.inputs['Color'])
    nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    m['textures'] = {k: str(v) for k, v in paths.items()}
    return m


def flat_material(name, rgb, rough=0.6, metal=0.0):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    p = next(n for n in m.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    p.inputs['Base Color'].default_value = (*rgb, 1.0)
    p.inputs['Roughness'].default_value = rough
    p.inputs['Metallic'].default_value = metal
    m.diffuse_color = (*rgb, 1.0)
    return m


# ----------------------------------------------------------------------------------------------- sockets, UCX

def socket(name, loc, parent, coll, rot=(0.0, 0.0, 0.0)):
    e = bpy.data.objects.new(f'SOCKET_{name}', None)
    e.empty_display_type = 'ARROWS'
    e.empty_display_size = 0.08
    e.location = Vector(loc)
    e.rotation_euler = rot
    e.parent = parent
    coll.objects.link(e)
    return e


def ucx_box(name, center, size, coll, rot=None):
    R = rot if rot is not None else Matrix.Identity(3)
    h = [s / 2 for s in size]
    vs = [Vector(center) + R @ Vector((sx * h[0], sy * h[1], sz * h[2])) for sx in (-1, 1) for sy in (-1, 1) for sz in (-1, 1)]
    faces = [(0, 1, 3, 2), (4, 6, 7, 5), (0, 4, 5, 1), (2, 3, 7, 6), (0, 2, 6, 4), (1, 5, 7, 3)]
    me = bpy.data.meshes.new(name)
    me.from_pydata([tuple(v) for v in vs], [], faces)
    me.update()
    me.validate()
    # outward normals
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    centre = sum(vs, Vector()) / 8
    for p in me.polygons:
        if p.normal.dot(p.center - centre) < 0:
            p.flip()
    ob.display_type = 'WIRE'
    ob.hide_render = True
    return ob


def ucx_hull(name, points, coll):
    import bmesh
    bm = bmesh.new()
    verts = [bm.verts.new(Vector(p)) for p in points]
    bmesh.ops.convex_hull(bm, input=verts)
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    ob.display_type = 'WIRE'
    ob.hide_render = True
    return ob


# ----------------------------------------------------------------------------------------------- stats, export

def mesh_stats(ob):
    me = ob.data
    tris = sum(len(p.vertices) - 2 for p in me.polygons)
    xs = [v.co.x for v in me.vertices]
    ys = [v.co.y for v in me.vertices]
    zs = [v.co.z for v in me.vertices]
    return {'triangles': tris, 'vertices': len(me.vertices),
            'min_m': [round(min(xs), 4), round(min(ys), 4), round(min(zs), 4)],
            'max_m': [round(max(xs), 4), round(max(ys), 4), round(max(zs), 4)],
            'size_m': [round(max(xs) - min(xs), 4), round(max(ys) - min(ys), 4), round(max(zs) - min(zs), 4)]}


def select(objs):
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    for o in objs:
        o.hide_set(False)
        o.select_set(True)
    bpy.context.view_layer.objects.active = objs[0]


def export_fbx(objs, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    select(objs)
    for attempt in range(6):
        try:
            bpy.ops.export_scene.fbx(filepath=str(path), use_selection=True, object_types={'MESH', 'EMPTY'},
                                     apply_unit_scale=True, apply_scale_options='FBX_SCALE_NONE', global_scale=1.0,
                                     axis_forward='-Y', axis_up='Z', mesh_smooth_type='FACE', use_mesh_modifiers=True,
                                     add_leaf_bones=False, bake_anim=False, use_tspace=True, path_mode='STRIP',
                                     embed_textures=False)
            return path
        except RuntimeError:
            if attempt == 5:
                raise
            time.sleep(1.0)


def export_glb(objs, path):
    path = Path(path)
    select(objs)
    bpy.ops.export_scene.gltf(filepath=str(path), export_format='GLB', use_selection=True, export_animations=False,
                              export_cameras=False, export_lights=False, export_apply=True, export_extras=False)
    return path


def to_unreal_cm(v):
    """Blender metres -> Unreal centimetres (Y mirrored)"""
    return [round(v[0] * 100, 2), round(-v[1] * 100, 2), round(v[2] * 100, 2)]
