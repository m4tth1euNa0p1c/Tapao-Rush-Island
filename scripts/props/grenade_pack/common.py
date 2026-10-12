"""Shared helpers of the grenade pack (T-253): bean module, mesh builders with explicit UVs, procedural PBR node
materials, Cycles atlas baking (BaseColor sRGB / Normal DirectX / ORM linear), part joining with rigid weights, weapon
skeleton, convex collision meshes.

Self-contained on purpose (scripts/props/musket_pack is being reworked in T-246 at the same time): only
scripts/character/bean_soldier.py is shared (palette, bean helpers), as the musket kit does.
"""
from __future__ import annotations

import importlib.util
import math
import sys
from pathlib import Path

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector

REPO = Path(__file__).resolve().parents[3]
BEAN_PY = REPO / 'scripts' / 'character' / 'bean_soldier.py'
TAU = 2 * math.pi
_B = None

# stage / helper materials added to the bean palette (sRGB hex, roughness, metallic)
EXTRA_PALETTE = {
    'M_Bean_Linen': ('E6DBC2', 0.95, 0.0),
    'M_Bean_Lead': ('7E848B', 0.42, 0.85),
    'M_Bean_Paper': ('EFE6CF', 0.90, 0.0),
    'M_Bean_Wood': ('6E3B1E', 0.55, 0.0),
    'M_Bean_Steel': ('7D848C', 0.38, 1.0),
}


def bean():
    """scripts/character/bean_soldier.py as a module (definitions only, its main() is not run)."""
    global _B
    if _B is None:
        spec = importlib.util.spec_from_file_location('bean_soldier_ref', BEAN_PY)
        _B = importlib.util.module_from_spec(spec)
        sys.modules['bean_soldier_ref'] = _B
        spec.loader.exec_module(_B)
        _B.PALETTE.update(EXTRA_PALETTE)
    return _B


def V(*a):
    return Vector(a)


# ------------------------------------------------------------------------------------------------ mesh builders

def new_object(name, bm, mat, coll, smooth=True):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    me.materials.append(mat if isinstance(mat, bpy.types.Material) else bean().material(mat))
    for p in me.polygons:
        p.use_smooth = smooth
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    return ob


def _grid(bm, rows, closed, uvs):
    """Quads between consecutive rows; uvs[j][i] = (u, v) of row j vertex i, with uvs[j][n] for the wrap column."""
    uv = bm.loops.layers.uv.verify()
    for j in range(len(rows) - 1):
        n = len(rows[j])
        for i in range(n if closed else n - 1):
            i2 = (i + 1) % n
            try:
                f = bm.faces.new((rows[j][i], rows[j][i2], rows[j + 1][i2], rows[j + 1][i]))
            except ValueError:
                continue
            for loop, (jj, ii) in zip(f.loops, ((j, i), (j, i + 1), (j + 1, i + 1), (j + 1, i))):
                loop[uv].uv = uvs[jj][ii]


def _fan(bm, centre, ring, uv_centre, uv_ring, reverse):
    """uv_centre: one (u, v) for a disc cap, or a list of one per triangle (sphere poles)."""
    uv = bm.loops.layers.uv.verify()
    n = len(ring)
    for i in range(n):
        i2 = (i + 1) % n
        tri = (centre, ring[i], ring[i2])
        tuv = (uv_centre[i] if isinstance(uv_centre, list) else uv_centre, uv_ring[i], uv_ring[(i + 1) if i + 1 < len(uv_ring) else i2])
        if reverse:
            tri, tuv = tri[::-1], tuv[::-1]
        try:
            f = bm.faces.new(tri)
        except ValueError:
            continue
        for loop, t in zip(f.loops, tuv):
            loop[uv].uv = t


def sphere(name, center, radii, mat, coll, seg=48, ring=24, smooth=True):
    """Ellipsoid with equirectangular UVs (u around, v from the bottom to the top)."""
    bm = bmesh.new()
    c = Vector(center)
    rx, ry, rz = radii
    rows, uvs = [], []
    for j in range(1, ring):
        t = math.pi * j / ring
        rows.append([bm.verts.new(c + Vector((rx * math.sin(t) * math.cos(TAU * i / seg), ry * math.sin(t) * math.sin(TAU * i / seg),
                                               rz * math.cos(t)))) for i in range(seg)])
        uvs.append([(i / seg, 1 - j / ring) for i in range(seg + 1)])
    _grid(bm, rows, True, uvs)
    top = bm.verts.new(c + Vector((0, 0, rz)))
    bot = bm.verts.new(c - Vector((0, 0, rz)))
    _fan(bm, top, rows[0], [((i + 0.5) / seg, 1.0) for i in range(seg)], [(i / seg, 1 - 1 / ring) for i in range(seg + 1)], True)
    _fan(bm, bot, rows[-1], [((i + 0.5) / seg, 0.0) for i in range(seg)], [(i / seg, 1 / ring) for i in range(seg + 1)], False)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm, mat, coll, smooth)


def tube(name, pts, radii, mat, coll, segs=16, caps=True, smooth=True, uv_offset=0.0):
    """Tube through pts (parallel transported frames) with UVs: u around, v along the length (aspect preserved)."""
    bm = bmesh.new()
    pts = [Vector(p) for p in pts]
    rows, uvs = [], []
    s, side = 0.0, None
    mean_r = max(sum(radii) / len(radii), 1e-4)
    for k, p in enumerate(pts):
        d = (pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]).normalized()
        if side is None:
            side = d.cross(Vector((0, 1, 0)))
            if side.length < 1e-4:
                side = d.cross(Vector((1, 0, 0)))
            side.normalize()
        else:
            side = (side - d * side.dot(d)).normalized()
        up = side.cross(d).normalized()
        if k:
            s += (pts[k] - pts[k - 1]).length
        rows.append([bm.verts.new(p + radii[k] * (math.cos(TAU * i / segs) * side + math.sin(TAU * i / segs) * up)) for i in range(segs)])
        uvs.append([(i / segs, uv_offset + s / (TAU * mean_r)) for i in range(segs + 1)])
    _grid(bm, rows, True, uvs)
    if caps:
        for ring, rev, vc in ((rows[0], True, -0.6), (rows[-1], False, uvs[-1][0][1] + 0.6)):
            centre = bm.verts.new(sum((v.co for v in ring), Vector()) / len(ring))
            _fan(bm, centre, ring, (0.5, vc), [(0.5 + 0.45 * math.cos(TAU * i / segs), vc + 0.45 * math.sin(TAU * i / segs)) for i in range(segs)], rev)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm, mat, coll, smooth)


def lathe(name, profile, center, mat, coll, segs=32, cap_start=False, cap_end=False, smooth=True, axis='Z'):
    """Surface of revolution: profile = [(h, r)] along the axis from center; UVs u around, v along the profile.
    A zero radius at either end of the profile becomes a cap fan (no degenerate ring)."""
    bm = bmesh.new()
    c = Vector(center)
    rows, uvs = [], []
    s = 0.0
    profile = list(profile)
    if profile and profile[0][1] <= 1e-6:
        profile = profile[1:]
        cap_start = True
    if profile and profile[-1][1] <= 1e-6:
        profile = profile[:-1]
        cap_end = True
    mean_r = max(sum(r for _, r in profile) / len(profile), 1e-4)
    for k, (h, r) in enumerate(profile):
        if k:
            s += math.hypot(h - profile[k - 1][0], r - profile[k - 1][1])
        row = []
        for i in range(segs):
            a = TAU * i / segs
            local = Vector((r * math.cos(a), r * math.sin(a), h))
            if axis == 'X':
                local = Vector((local.z, local.x, local.y))
            elif axis == 'Y':
                local = Vector((local.y, local.z, local.x))
            row.append(bm.verts.new(c + local))
        rows.append(row)
        uvs.append([(i / segs, s / (TAU * mean_r)) for i in range(segs + 1)])
    _grid(bm, rows, True, uvs)
    for do, ring, rev, vc in ((cap_start, rows[0], True, -0.6), (cap_end, rows[-1], False, uvs[-1][0][1] + 0.6)):
        if not do:
            continue
        centre = bm.verts.new(sum((v.co for v in ring), Vector()) / len(ring))
        _fan(bm, centre, ring, (0.5, vc), [(0.5 + 0.45 * math.cos(TAU * i / segs), vc + 0.45 * math.sin(TAU * i / segs)) for i in range(segs)], rev)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm, mat, coll, smooth)


def torus(name, center, R, r, mat, coll, axis='Z', segs=32, minor=10, smooth=True):
    bm = bmesh.new()
    c = Vector(center)
    rows, uvs = [], []
    for i in range(segs + 1):
        u = TAU * (i % segs) / segs
        row = []
        for j in range(minor):
            v = TAU * j / minor
            p = Vector(((R + r * math.cos(v)) * math.cos(u), (R + r * math.cos(v)) * math.sin(u), r * math.sin(v)))
            if axis == 'X':
                p = Vector((p.z, p.x, p.y))
            elif axis == 'Y':
                p = Vector((p.x, p.z, p.y))
            row.append(bm.verts.new(p + c) if i < segs else rows[0][j])
        rows.append(row)
        uvs.append([(j / minor * (r / R), i / segs) for j in range(minor + 1)])
    _grid(bm, rows, True, uvs)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return new_object(name, bm, mat, coll, smooth)


def box(name, center, size, mat, coll, bevel=0.0, smooth=False, segments=2):
    """Box (full size) with a real bevel (bmesh, no modifier) and box-projected UVs per dominant axis."""
    bm = bmesh.new()
    bmesh.ops.create_cube(bm, size=1.0)
    for v in bm.verts:
        v.co = Vector((v.co.x * size[0], v.co.y * size[1], v.co.z * size[2]))
    if bevel:
        bmesh.ops.bevel(bm, geom=list(bm.verts) + list(bm.edges), offset=min(bevel, min(size) * 0.45), segments=segments,
                        affect='EDGES', profile=0.5)
    uv = bm.loops.layers.uv.verify()
    bm.normal_update()
    for f in bm.faces:
        n = f.normal
        ax = max(range(3), key=lambda i: abs(n[i]))
        sign = 1 if n[ax] >= 0 else -1
        for loop in f.loops:
            p = loop.vert.co
            u, v_ = ((p.y * sign, p.z), (-p.x * sign, p.z), (p.x, p.y * sign))[ax]
            loop[uv].uv = (u, v_)
    for v in bm.verts:
        v.co += Vector(center)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = new_object(name, bm, mat, coll, smooth)
    if bevel and not smooth:
        # flat sides, smooth bevel strips (faces off the three axes)
        for p in ob.data.polygons:
            p.use_smooth = max(abs(p.normal.x), abs(p.normal.y), abs(p.normal.z)) < 0.99
    return ob


def convex(name, points, coll, mat=None):
    """Convex hull of points (UCX collision mesh for Unreal: named UCX_<RenderMesh>_NN)."""
    bm = bmesh.new()
    for p in points:
        bm.verts.new(p)
    bmesh.ops.convex_hull(bm, input=list(bm.verts))
    bmesh.ops.delete(bm, geom=[v for v in bm.verts if not v.link_faces], context='VERTS')
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    if mat:
        me.materials.append(mat)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    ob.display_type = 'WIRE'
    return ob


def icosphere_points(center, radius, subdivisions=1):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdivisions, radius=radius)
    pts = [Vector(center) + v.co for v in bm.verts]
    bm.free()
    return pts


# ----------------------------------------------------------------------------------------- procedural materials

def _mat(name):
    m = bpy.data.materials.get(name) or bpy.data.materials.new(name)
    m.use_nodes = True
    nt = m.node_tree
    for n in list(nt.nodes):
        nt.nodes.remove(n)
    out = nt.nodes.new('ShaderNodeOutputMaterial')
    bsdf = nt.nodes.new('ShaderNodeBsdfPrincipled')
    nt.links.new(bsdf.outputs['BSDF'], out.inputs['Surface'])
    return m, nt, bsdf


def _node(nt, type_, **props):
    n = nt.nodes.new(type_)
    for k, v in props.items():
        setattr(n, k, v)
    return n


def _ramp(nt, stops):
    """Color ramp with stops [(pos, (r, g, b, a) or float)]."""
    n = nt.nodes.new('ShaderNodeValToRGB')
    el = n.color_ramp.elements
    while len(el) > 1:
        el.remove(el[-1])
    for i, (pos, col) in enumerate(stops):
        e = el[0] if i == 0 else el.new(pos)
        e.position = pos
        e.color = (col, col, col, 1) if isinstance(col, (int, float)) else col
    return n


def _noise(nt, scale, detail=3.0, roughness=0.5, coords=None, dim='3D'):
    n = nt.nodes.new('ShaderNodeTexNoise')
    n.noise_dimensions = dim
    n.inputs['Scale'].default_value = scale
    n.inputs['Detail'].default_value = detail
    n.inputs['Roughness'].default_value = roughness
    if coords is not None:
        nt.links.new(coords, n.inputs['Vector'])
    return n


def _math(nt, op, a, b=None, clamp=False):
    n = nt.nodes.new('ShaderNodeMath')
    n.operation = op
    n.use_clamp = clamp
    for i, x in enumerate((a, b)):
        if x is None:
            continue
        if isinstance(x, (int, float)):
            n.inputs[i].default_value = x
        else:
            nt.links.new(x, n.inputs[i])
    return n


def _mix(nt, a, b, fac, data='RGBA'):
    n = nt.nodes.new('ShaderNodeMix')
    n.data_type = data
    n.blend_type = 'MIX'
    inputs = {'RGBA': ('A', 'B'), 'FLOAT': ('A', 'B')}[data]
    ia = [s for s in n.inputs if s.name == inputs[0] and s.type == ('RGBA' if data == 'RGBA' else 'VALUE')][0]
    ib = [s for s in n.inputs if s.name == inputs[1] and s.type == ('RGBA' if data == 'RGBA' else 'VALUE')][0]
    for sock, x in ((ia, a), (ib, b)):
        if isinstance(x, tuple):
            sock.default_value = x if len(x) == 4 else (*x, 1)
        elif isinstance(x, (int, float)):
            sock.default_value = x
        else:
            nt.links.new(x, sock)
    if isinstance(fac, (int, float)):
        n.inputs['Factor'].default_value = fac
    else:
        nt.links.new(fac, n.inputs['Factor'])
    return n


def _mix_out(n):
    return [s for s in n.outputs if s.enabled][0]


def _coords(nt, space='Object'):
    c = nt.nodes.new('ShaderNodeTexCoord')
    return c.outputs[space]


def _bump(nt, height, strength, distance, bsdf):
    b = nt.nodes.new('ShaderNodeBump')
    b.inputs['Strength'].default_value = strength
    b.inputs['Distance'].default_value = distance
    nt.links.new(height, b.inputs['Height'])
    nt.links.new(b.outputs['Normal'], bsdf.inputs['Normal'])
    return b


def mat_cast_iron(name, base=(0.040, 0.037, 0.035), rust=(0.26, 0.10, 0.035), pit_scale=160.0, rust_amount=0.55, seed=0.0):
    """Pitted, lightly rusted cast iron (a grenado shell, barrel hoops)."""
    m, nt, bsdf = _mat(name)
    co = _coords(nt)
    mp = _node(nt, 'ShaderNodeMapping')
    mp.inputs['Location'].default_value = (seed, seed * 0.7, seed * 0.3)
    nt.links.new(co, mp.inputs['Vector'])
    pits = _noise(nt, pit_scale, 5.0, 0.65, mp.outputs['Vector'])
    pit_mask = _ramp(nt, [(0.30, 1.0), (0.46, 0.0)])            # low noise = pit
    nt.links.new(pits.outputs['Fac'], pit_mask.inputs['Fac'])
    grain = _noise(nt, 900.0, 2.0, 0.5, mp.outputs['Vector'])
    rustn = _noise(nt, 18.0, 4.0, 0.6, mp.outputs['Vector'])
    rust_mask = _ramp(nt, [(0.56 - 0.12 * rust_amount, 0.0), (0.70 - 0.12 * rust_amount, 1.0)])
    nt.links.new(rustn.outputs['Fac'], rust_mask.inputs['Fac'])
    col = _mix(nt, (*base, 1), (*rust, 1), rust_mask.outputs['Color'])
    dark = _mix(nt, _mix_out(col), (base[0] * 0.35, base[1] * 0.35, base[2] * 0.35, 1), pit_mask.outputs['Color'])
    sheen = _mix(nt, _mix_out(dark), (base[0] * 2.2, base[1] * 2.2, base[2] * 2.2, 1), _math(nt, 'MULTIPLY', grain.outputs['Fac'], 0.25).outputs[0])
    nt.links.new(_mix_out(sheen), bsdf.inputs['Base Color'])
    rough = _math(nt, 'ADD', _math(nt, 'MULTIPLY', pit_mask.outputs['Color'], 0.30).outputs[0], 0.52)
    rough2 = _math(nt, 'ADD', rough.outputs[0], _math(nt, 'MULTIPLY', rust_mask.outputs['Color'], 0.25).outputs[0], clamp=True)
    nt.links.new(rough2.outputs[0], bsdf.inputs['Roughness'])
    metal = _math(nt, 'SUBTRACT', 0.92, _math(nt, 'MULTIPLY', rust_mask.outputs['Color'], 0.85).outputs[0], clamp=True)
    nt.links.new(metal.outputs[0], bsdf.inputs['Metallic'])
    h = _math(nt, 'SUBTRACT', _math(nt, 'MULTIPLY', grain.outputs['Fac'], 0.12).outputs[0], _math(nt, 'MULTIPLY', pit_mask.outputs['Color'], 1.0).outputs[0])
    _bump(nt, h.outputs[0], 0.55, 0.0025, bsdf)
    return m


def mat_wood(name, light=(0.42, 0.26, 0.12), dark=(0.20, 0.11, 0.05), scale=28.0, axis=(0, 0, 1), rough=0.58, seed=0.0):
    """Wood with the grain along axis (object space): growth rings around that axis (off-centre), so every face
    parallel to the axis shows stripes along it and the end faces show the rings."""
    m, nt, bsdf = _mat(name)
    co = _coords(nt)
    mp = _node(nt, 'ShaderNodeMapping')
    ax = Vector(axis).normalized()
    q = ax.rotation_difference(Vector((1, 0, 0)))
    mp.inputs['Rotation'].default_value = q.to_euler()
    mp.inputs['Location'].default_value = (seed, 0.045 + seed * 0.1, 0.07 + seed * 0.05)   # off-centre rings
    mp.inputs['Scale'].default_value = (1, 1, 1)
    nt.links.new(co, mp.inputs['Vector'])
    warp = _noise(nt, 6.0, 2.0, 0.5, mp.outputs['Vector'])
    wave = nt.nodes.new('ShaderNodeTexWave')
    wave.wave_type = 'RINGS'
    wave.rings_direction = 'X'
    wave.wave_profile = 'SIN'
    wave.inputs['Scale'].default_value = scale
    wave.inputs['Distortion'].default_value = 1.6
    wave.inputs['Detail'].default_value = 2.0
    wave.inputs['Detail Scale'].default_value = 2.0
    vm = nt.nodes.new('ShaderNodeVectorMath')
    vm.operation = 'ADD'
    nt.links.new(mp.outputs['Vector'], vm.inputs[0])
    sc = nt.nodes.new('ShaderNodeVectorMath')
    sc.operation = 'SCALE'
    nt.links.new(warp.outputs['Color'], sc.inputs[0])
    sc.inputs['Scale'].default_value = 0.08
    nt.links.new(sc.outputs['Vector'], vm.inputs[1])
    nt.links.new(vm.outputs['Vector'], wave.inputs['Vector'])
    fine = _noise(nt, 300.0, 2.0, 0.5, mp.outputs['Vector'])
    ramp = _ramp(nt, [(0.25, (*dark, 1)), (0.60, (*light, 1)), (0.85, ((light[0] + dark[0]) / 2, (light[1] + dark[1]) / 2, (light[2] + dark[2]) / 2, 1))])
    nt.links.new(wave.outputs['Fac'], ramp.inputs['Fac'])
    col = _mix(nt, ramp.outputs['Color'], (dark[0] * 0.7, dark[1] * 0.7, dark[2] * 0.7, 1), _math(nt, 'MULTIPLY', fine.outputs['Fac'], 0.35).outputs[0])
    nt.links.new(_mix_out(col), bsdf.inputs['Base Color'])
    r = _math(nt, 'ADD', rough - 0.08, _math(nt, 'MULTIPLY', wave.outputs['Fac'], 0.16).outputs[0], clamp=True)
    nt.links.new(r.outputs[0], bsdf.inputs['Roughness'])
    bsdf.inputs['Metallic'].default_value = 0.0
    h = _math(nt, 'ADD', _math(nt, 'MULTIPLY', wave.outputs['Fac'], 0.6).outputs[0], _math(nt, 'MULTIPLY', fine.outputs['Fac'], 0.4).outputs[0])
    _bump(nt, h.outputs[0], 0.35, 0.0012, bsdf)
    return m


def mat_cord(name, color=(0.50, 0.38, 0.20), dark=(0.22, 0.15, 0.07), twist=18.0, charred_at=None, charred_len=0.02, ember=False):
    """Twisted hemp cord (UV twist bands); charred_at = object point where the cord blackens over charred_len (the
    glowing ember colour at the very end when ember)."""
    m, nt, bsdf = _mat(name)
    uv = _coords(nt, 'UV')
    mp = _node(nt, 'ShaderNodeMapping')
    mp.inputs['Rotation'].default_value = (0, 0, math.radians(55))
    nt.links.new(uv, mp.inputs['Vector'])
    wave = nt.nodes.new('ShaderNodeTexWave')
    wave.wave_type = 'BANDS'
    wave.bands_direction = 'X'
    wave.wave_profile = 'SIN'
    wave.inputs['Scale'].default_value = twist
    wave.inputs['Distortion'].default_value = 0.8
    wave.inputs['Detail'].default_value = 1.0
    nt.links.new(mp.outputs['Vector'], wave.inputs['Vector'])
    fibre = _noise(nt, 60.0, 3.0, 0.6, uv)
    col = _mix(nt, (*dark, 1), (*color, 1), wave.outputs['Fac'])
    col2 = _mix(nt, _mix_out(col), (*dark, 1), _math(nt, 'MULTIPLY', fibre.outputs['Fac'], 0.4).outputs[0])
    out_col = _mix_out(col2)
    rough_src = None
    if charred_at is not None:
        ob = _coords(nt, 'Object')
        dist = nt.nodes.new('ShaderNodeVectorMath')
        dist.operation = 'DISTANCE'
        nt.links.new(ob, dist.inputs[0])
        dist.inputs[1].default_value = tuple(charred_at)
        char = _ramp(nt, [(charred_len * 0.35, 1.0), (charred_len, 0.0)])
        nt.links.new(dist.outputs['Value'], char.inputs['Fac'])
        black = _mix(nt, out_col, (0.015, 0.012, 0.010, 1), char.outputs['Color'])
        out_col = _mix_out(black)
        if ember:
            glow = _ramp(nt, [(charred_len * 0.12, 1.0), (charred_len * 0.35, 0.0)])
            nt.links.new(dist.outputs['Value'], glow.inputs['Fac'])
            hot = _mix(nt, out_col, (0.95, 0.28, 0.04, 1), glow.outputs['Color'])
            out_col = _mix_out(hot)
        rough_src = char.outputs['Color']
    nt.links.new(out_col, bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = 0.82
    if rough_src is not None:
        r = _math(nt, 'ADD', 0.78, _math(nt, 'MULTIPLY', rough_src, 0.2).outputs[0], clamp=True)
        nt.links.new(r.outputs[0], bsdf.inputs['Roughness'])
    bsdf.inputs['Metallic'].default_value = 0.0
    h = _math(nt, 'ADD', _math(nt, 'MULTIPLY', wave.outputs['Fac'], 0.8).outputs[0], _math(nt, 'MULTIPLY', fibre.outputs['Fac'], 0.3).outputs[0])
    _bump(nt, h.outputs[0], 0.6, 0.0012, bsdf)
    return m


def mat_leather(name, color=(0.20, 0.10, 0.05), dark=(0.09, 0.045, 0.02), rough=0.62, seed=0.0, scale=260.0):
    m, nt, bsdf = _mat(name)
    co = _coords(nt)
    mp = _node(nt, 'ShaderNodeMapping')
    mp.inputs['Location'].default_value = (seed, seed, seed)
    nt.links.new(co, mp.inputs['Vector'])
    grain = _noise(nt, scale, 4.0, 0.7, mp.outputs['Vector'])
    tone = _noise(nt, 7.0, 3.0, 0.5, mp.outputs['Vector'])
    wear = _ramp(nt, [(0.40, 0.0), (0.62, 1.0)])
    nt.links.new(tone.outputs['Fac'], wear.inputs['Fac'])
    col = _mix(nt, (*color, 1), (*dark, 1), wear.outputs['Color'])
    col2 = _mix(nt, _mix_out(col), (color[0] * 1.5, color[1] * 1.5, color[2] * 1.5, 1), _math(nt, 'MULTIPLY', grain.outputs['Fac'], 0.25).outputs[0])
    nt.links.new(_mix_out(col2), bsdf.inputs['Base Color'])
    r = _math(nt, 'ADD', rough - 0.1, _math(nt, 'MULTIPLY', tone.outputs['Fac'], 0.22).outputs[0], clamp=True)
    nt.links.new(r.outputs[0], bsdf.inputs['Roughness'])
    bsdf.inputs['Metallic'].default_value = 0.0
    h = _math(nt, 'ADD', _math(nt, 'MULTIPLY', grain.outputs['Fac'], 0.7).outputs[0], _math(nt, 'MULTIPLY', tone.outputs['Fac'], 0.3).outputs[0])
    _bump(nt, h.outputs[0], 0.35, 0.0008, bsdf)
    return m


def mat_metal(name, color=(0.62, 0.45, 0.18), rough=0.38, metallic=1.0, seed=0.0, smudge=0.2):
    """Brass / lead / polished steel with smudges."""
    m, nt, bsdf = _mat(name)
    co = _coords(nt)
    mp = _node(nt, 'ShaderNodeMapping')
    mp.inputs['Location'].default_value = (seed, seed, seed)
    nt.links.new(co, mp.inputs['Vector'])
    n = _noise(nt, 40.0, 3.0, 0.55, mp.outputs['Vector'])
    col = _mix(nt, (*color, 1), (color[0] * 0.55, color[1] * 0.55, color[2] * 0.55, 1), _math(nt, 'MULTIPLY', n.outputs['Fac'], smudge * 1.5, clamp=True).outputs[0])
    nt.links.new(_mix_out(col), bsdf.inputs['Base Color'])
    r = _math(nt, 'ADD', rough - smudge * 0.5, _math(nt, 'MULTIPLY', n.outputs['Fac'], smudge).outputs[0], clamp=True)
    nt.links.new(r.outputs[0], bsdf.inputs['Roughness'])
    bsdf.inputs['Metallic'].default_value = metallic
    fine = _noise(nt, 500.0, 2.0, 0.5, mp.outputs['Vector'])
    _bump(nt, fine.outputs['Fac'], 0.08, 0.0005, bsdf)
    return m


def mat_paper(name, color=(0.80, 0.72, 0.56), rough=0.85, seed=0.0):
    m, nt, bsdf = _mat(name)
    co = _coords(nt)
    n = _noise(nt, 120.0, 3.0, 0.6, co)
    n.inputs['Scale'].default_value = 120.0 + seed
    col = _mix(nt, (*color, 1), (color[0] * 0.75, color[1] * 0.72, color[2] * 0.65, 1), _math(nt, 'MULTIPLY', n.outputs['Fac'], 0.5).outputs[0])
    nt.links.new(_mix_out(col), bsdf.inputs['Base Color'])
    bsdf.inputs['Roughness'].default_value = rough
    bsdf.inputs['Metallic'].default_value = 0.0
    _bump(nt, n.outputs['Fac'], 0.15, 0.0005, bsdf)
    return m


# ------------------------------------------------------------------------------------------------ joins, rig, UV

def _evaluated_bmesh(ob, dg):
    ev = ob.evaluated_get(dg)
    me = ev.to_mesh()
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.transform(ob.matrix_world)
    ev.to_mesh_clear()
    return bm


def join_parts(name, parts_by_bone, coll, bone_names=None):
    """Joins the parts into one multi-material mesh keeping UVs; with bone_names every part is weighted 100 % on its
    bone (rigid parts). parts_by_bone = {bone: [objects]}; the parts are removed."""
    all_parts = [ob for objs in parts_by_bone.values() for ob in objs]
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    bm = bmesh.new()
    deform = bm.verts.layers.deform.verify() if bone_names else None
    uv = bm.loops.layers.uv.verify()
    mats = []
    for bone, objs in parts_by_bone.items():
        for ob in objs:
            src = _evaluated_bmesh(ob, dg)
            remap = []
            for slot in ob.material_slots:
                if slot.material not in mats:
                    mats.append(slot.material)
                remap.append(mats.index(slot.material))
            src_uv = src.loops.layers.uv.active
            vmap = {}
            for v in src.verts:
                nv = bm.verts.new(v.co)
                if deform is not None:
                    nv[deform][bone_names.index(bone)] = 1.0
                vmap[v] = nv
            for f in src.faces:
                try:
                    nf = bm.faces.new([vmap[v] for v in f.verts])
                except ValueError:
                    continue
                nf.smooth = f.smooth
                nf.material_index = remap[f.material_index] if remap else 0
                if src_uv is not None:
                    for l_new, l_old in zip(nf.loops, f.loops):
                        l_new[uv].uv = l_old[src_uv].uv
            src.free()
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    for m in mats:
        me.materials.append(m)
    ob = bpy.data.objects.new(name, me)
    coll.objects.link(ob)
    if bone_names:
        for b in bone_names:
            ob.vertex_groups.new(name=b)
    for o in all_parts:
        data = o.data
        bpy.data.objects.remove(o, do_unlink=True)
        if data and data.users == 0:
            bpy.data.meshes.remove(data)
    return ob


def select_only(ob):
    # removed objects leave stale (None) entries in the view layer until it is updated
    bpy.context.view_layer.update()
    for o in bpy.context.view_layer.objects:
        if o is not None:
            o.select_set(False)
    ob.hide_set(False)
    ob.select_set(True)
    bpy.context.view_layer.objects.active = ob


def pack_uvs(ob, margin=0.012):
    """Same texel density for every island, then packed into the unit square (one atlas per object)."""
    select_only(ob)
    bpy.context.scene.tool_settings.use_uv_select_sync = True
    bpy.ops.object.mode_set(mode='EDIT')
    bpy.ops.mesh.select_all(action='SELECT')
    bpy.ops.uv.average_islands_scale()
    bpy.ops.uv.pack_islands(rotate=True, rotate_method='ANY', scale=True, margin_method='FRACTION', margin=margin, shape_method='CONCAVE')
    bpy.ops.object.mode_set(mode='OBJECT')


def make_skeleton(name, bones, sockets, coll):
    """Weapon armature: rigid bones + non deforming socket_* bones (Unreal sockets and hand targets)."""
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    coll.objects.link(arm)
    arm.show_in_front = True
    select_only(arm)
    bpy.ops.object.mode_set(mode='EDIT')
    for bname, (head, tail, parent) in bones.items():
        eb = data.edit_bones.new(bname)
        eb.head, eb.tail = head, tail
        eb.roll = 0.0
        if parent:
            eb.parent = data.edit_bones[parent]
    for sname, (parent, pos) in sockets.items():
        eb = data.edit_bones.new('socket_' + sname)
        eb.head, eb.tail = pos, Vector(pos) + Vector((0.01, 0, 0))
        eb.parent = data.edit_bones[parent]
        eb.use_deform = False
    bpy.ops.object.mode_set(mode='OBJECT')
    return arm


# ------------------------------------------------------------------------------------------------------ baking

def _image(name, size, srgb):
    img = bpy.data.images.get(name)
    if img:
        bpy.data.images.remove(img)
    img = bpy.data.images.new(name, size, size, alpha=False)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    return img


def _targets(ob, img):
    for slot in ob.material_slots:
        nt = slot.material.node_tree
        node = nt.nodes.get('BAKE')
        if node is None:
            node = nt.nodes.new('ShaderNodeTexImage')
            node.name = 'BAKE'
        node.image = img
        nt.nodes.active = node


def _reroute_to_emission(materials, socket_name):
    """Material output <- Emission of the Principled input socket_name (linked tree or constant); returns undo data."""
    undo = []
    for m in materials:
        nt = m.node_tree
        out = next(n for n in nt.nodes if n.type == 'OUTPUT_MATERIAL' and n.is_active_output)
        bsdf = next(n for n in nt.nodes if n.type == 'BSDF_PRINCIPLED')
        old = out.inputs['Surface'].links[0].from_socket if out.inputs['Surface'].links else None
        em = nt.nodes.new('ShaderNodeEmission')
        em.name = 'BAKE_EMIT'
        em.inputs['Strength'].default_value = 1.0
        sock = bsdf.inputs[socket_name]
        if sock.links:
            nt.links.new(sock.links[0].from_socket, em.inputs['Color'])
        else:
            v = sock.default_value
            em.inputs['Color'].default_value = (v, v, v, 1) if isinstance(v, float) else tuple(v)
        nt.links.new(em.outputs['Emission'], out.inputs['Surface'])
        undo.append((nt, out, old, em))
    return undo


def _restore(undo):
    for nt, out, old, em in undo:
        nt.nodes.remove(em)
        if old is not None:
            nt.links.new(old, out.inputs['Surface'])


def _pixels(img):
    buf = np.empty(img.size[0] * img.size[1] * 4, dtype=np.float32)
    img.pixels.foreach_get(buf)
    return buf.reshape(img.size[1], img.size[0], 4)


def _save(img, path):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    img.filepath_raw = str(path)
    img.file_format = 'PNG'
    img.save()
    img.filepath = str(path)
    img.source = 'FILE'
    img.reload()
    return path


def bake_atlas(scene, ob, name, size, out_dir, ao_samples=48, diffuse_samples=1):
    """Bakes the procedural materials of ob into T_<name>_D (sRGB), T_<name>_N (DirectX, green down) and
    T_<name>_ORM (AO, roughness, metallic; linear) in out_dir, then gives ob a single material M_<name> with Image
    Texture nodes labelled BaseColor / Normal / ORM. Returns {label: path}."""
    out_dir = Path(out_dir)
    bpy.context.window.scene = scene
    scene.render.engine = 'CYCLES'
    scene.cycles.device = 'CPU'
    scene.cycles.use_denoising = False
    bake = scene.render.bake
    bake.margin = max(4, size // 256)
    bake.use_clear = True
    bake.use_selected_to_active = False
    bake.target = 'IMAGE_TEXTURES'
    select_only(ob)
    mats = [s.material for s in ob.material_slots]
    paths = {}

    def run(kind, img, samples, **kw):
        _targets(ob, img)
        scene.cycles.samples = samples
        bpy.ops.object.bake(type=kind, **kw)

    d = _image('T_%s_D' % name, size, True)
    undo = _reroute_to_emission(mats, 'Base Color')
    run('EMIT', d, diffuse_samples)
    _restore(undo)
    paths['BaseColor'] = _save(d, out_dir / ('T_%s_D.png' % name))

    n = _image('T_%s_N' % name, size, False)
    bake.normal_space = 'TANGENT'
    bake.normal_r, bake.normal_g, bake.normal_b = 'POS_X', 'NEG_Y', 'POS_Z'
    run('NORMAL', n, 1)
    paths['Normal'] = _save(n, out_dir / ('T_%s_N.png' % name))

    ao = _image('T_%s_AO' % name, size, False)
    run('AO', ao, ao_samples)
    r = _image('T_%s_R' % name, size, False)
    run('ROUGHNESS', r, 1)
    mt = _image('T_%s_M' % name, size, False)
    undo = _reroute_to_emission(mats, 'Metallic')
    run('EMIT', mt, 1)
    _restore(undo)
    orm = _image('T_%s_ORM' % name, size, False)
    px = np.ones((size, size, 4), dtype=np.float32)
    px[..., 0] = _pixels(ao)[..., 0]
    px[..., 1] = _pixels(r)[..., 0]
    px[..., 2] = _pixels(mt)[..., 0]
    orm.pixels.foreach_set(px.ravel())
    paths['ORM'] = _save(orm, out_dir / ('T_%s_ORM.png' % name))
    for img in (ao, r, mt):
        bpy.data.images.remove(img)
    for m in mats:
        node = m.node_tree.nodes.get('BAKE')
        if node:
            m.node_tree.nodes.remove(node)
    baked = baked_material('M_' + name, paths)
    ob.data.materials.clear()
    ob.data.materials.append(baked)
    for p in ob.data.polygons:
        p.material_index = 0
    return paths


def baked_material(name, paths):
    """Principled material on the baked atlas: BaseColor -> Base Color; ORM (Separate Color) G -> Roughness, B ->
    Metallic; Normal (DirectX) -> green flipped for Blender -> Normal Map -> Normal. Labels BaseColor / Normal / ORM."""
    m, nt, bsdf = _mat(name)
    bsdf.location = (300, 0)
    imgs = {}
    for i, (label, srgb) in enumerate((('BaseColor', True), ('Normal', False), ('ORM', False))):
        node = nt.nodes.new('ShaderNodeTexImage')
        node.label = label
        node.name = label
        node.location = (-700, 300 - 330 * i)
        img = bpy.data.images.load(str(paths[label]), check_existing=True)
        img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
        node.image = img
        imgs[label] = node
    nt.links.new(imgs['BaseColor'].outputs['Color'], bsdf.inputs['Base Color'])
    sep = nt.nodes.new('ShaderNodeSeparateColor')
    sep.location = (-380, -360)
    nt.links.new(imgs['ORM'].outputs['Color'], sep.inputs['Color'])
    nt.links.new(sep.outputs['Green'], bsdf.inputs['Roughness'])
    nt.links.new(sep.outputs['Blue'], bsdf.inputs['Metallic'])
    sepn = nt.nodes.new('ShaderNodeSeparateColor')
    sepn.location = (-380, -30)
    nt.links.new(imgs['Normal'].outputs['Color'], sepn.inputs['Color'])
    flip = _math(nt, 'SUBTRACT', 1.0, sepn.outputs['Green'])
    flip.name = 'FlipGreen'
    flip.location = (-200, -60)
    comb = nt.nodes.new('ShaderNodeCombineColor')
    comb.location = (-40, -30)
    nt.links.new(sepn.outputs['Red'], comb.inputs['Red'])
    nt.links.new(flip.outputs[0], comb.inputs['Green'])
    nt.links.new(sepn.outputs['Blue'], comb.inputs['Blue'])
    nm = nt.nodes.new('ShaderNodeNormalMap')
    nm.space = 'TANGENT'
    nm.inputs['Strength'].default_value = 1.0
    nm.location = (120, -80)
    nt.links.new(comb.outputs['Color'], nm.inputs['Color'])
    nt.links.new(nm.outputs['Normal'], bsdf.inputs['Normal'])
    m['baked_textures'] = {k: str(v) for k, v in paths.items()}
    m['normal_convention'] = 'DirectX (green down); the FlipGreen chain is for Blender only'
    return m


def fbx_simplify_normals(materials):
    """For the FBX export: link the DirectX normal image straight into the Normal Map node (what Unreal expects);
    returns an undo function."""
    undo = []
    for m in materials:
        nt = m.node_tree
        nm = next((n for n in nt.nodes if n.type == 'NORMAL_MAP'), None)
        img = nt.nodes.get('Normal')
        if nm is None or img is None or not nm.inputs['Color'].links:
            continue
        old = nm.inputs['Color'].links[0].from_socket
        nt.links.new(img.outputs['Color'], nm.inputs['Color'])
        undo.append((nt, nm, old))

    def restore():
        for nt, nm, old in undo:
            nt.links.new(old, nm.inputs['Color'])
    return restore


def studio_scene(name, look_at, radius, res=(1600, 900)):
    """Product stage: dark floor, key / fill / rim area lights, 3/4 camera on look_at framing a sphere of radius."""
    scene = bpy.data.scenes.new(name)
    scene.unit_settings.system = 'METRIC'
    try:
        scene.render.engine = 'BLENDER_EEVEE'
    except TypeError:
        scene.render.engine = 'BLENDER_EEVEE_NEXT'
    scene.render.resolution_x, scene.render.resolution_y, scene.render.resolution_percentage = res[0], res[1], 100
    scene.render.image_settings.file_format = 'PNG'
    scene.world = bpy.data.worlds.new('World-' + name)
    scene.world.use_nodes = True
    w = next(n for n in scene.world.node_tree.nodes if n.type == 'BACKGROUND')
    w.inputs['Color'].default_value = (.16, .19, .23, 1)
    w.inputs['Strength'].default_value = .5
    try:
        scene.view_settings.view_transform = 'AgX'
    except TypeError:
        pass
    me = bpy.data.meshes.new('REF-StudioFloor')
    me.from_pydata([(-20, -20, 0), (20, -20, 0), (20, 20, 0), (-20, 20, 0)], [], [(0, 1, 2, 3)])
    floor = bpy.data.objects.new('REF-StudioFloor-' + name, me)
    scene.collection.objects.link(floor)
    fm = bpy.data.materials.get('MAT-Studio') or bpy.data.materials.new('MAT-Studio')
    fm.use_nodes = True
    fb = next(n for n in fm.node_tree.nodes if n.type == 'BSDF_PRINCIPLED')
    fb.inputs['Base Color'].default_value = (.22, .21, .19, 1)
    fb.inputs['Roughness'].default_value = .8
    me.materials.append(fm)
    look_at = Vector(look_at)
    for label, offset, power, color, size in (('Key', (1.0, -1.4, 1.9), 90, (1, .9, .78), 1.2), ('Fill', (-1.6, -0.8, 1.0), 45, (.7, .82, 1), 1.6),
                                             ('Rim', (0.3, 1.6, 1.5), 110, (1, .95, .85), 1.0)):
        data = bpy.data.lights.new('LGT-' + label + '-' + name, 'AREA')
        data.energy = power * radius / 0.12
        data.color = color
        data.shape = 'DISK'
        data.size = size * radius / 0.12
        light = bpy.data.objects.new(data.name, data)
        scene.collection.objects.link(light)
        light.location = look_at + Vector(offset) * (radius / 0.12)
        light.rotation_euler = (look_at - light.location).to_track_quat('-Z', 'Y').to_euler()
    data = bpy.data.cameras.new('CAM-' + name)
    cam = bpy.data.objects.new(data.name, data)
    scene.collection.objects.link(cam)
    data.lens = 60.0
    data.sensor_width = 36.0
    data.clip_start = 0.01
    direction = Vector((0.62, -0.72, 0.32)).normalized()
    cam.location = look_at + direction * (radius * 2.0 / math.tan(math.atan(36.0 / (2 * 60.0))) * 0.5 + radius * 0.3)
    cam.rotation_euler = (look_at - cam.location).to_track_quat('-Z', 'Y').to_euler()
    scene.camera = cam
    return scene


def render_scene(scene, path, samples=32):
    bpy.context.window.scene = scene
    try:
        scene.eevee.taa_render_samples = samples
    except AttributeError:
        pass
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    scene.render.filepath = str(path)
    scene.render.image_settings.file_format = 'PNG'
    bpy.ops.render.render(write_still=True, scene=scene.name)


def triangle_count(objs):
    return sum(sum(max(0, len(p.vertices) - 2) for p in o.data.polygons) for o in objs if o.type == 'MESH')


def world_bounds(objs):
    dg = bpy.context.evaluated_depsgraph_get()
    pts = []
    for ob in objs:
        if ob.type != 'MESH':
            continue
        ev = ob.evaluated_get(dg)
        me = ev.to_mesh()
        pts.extend(ev.matrix_world @ v.co for v in me.vertices)
        ev.to_mesh_clear()
    if not pts:
        return None
    lo = [min(p[i] for p in pts) for i in range(3)]
    hi = [max(p[i] for p in pts) for i in range(3)]
    return {'min': lo, 'max': hi, 'size': [hi[i] - lo[i] for i in range(3)]}
