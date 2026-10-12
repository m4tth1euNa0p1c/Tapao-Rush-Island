"""Geometrie et bruit numpy pour les artefacts (aucune dependance hors Blender/numpy).

- bruit 3D vectorise (valeur + Worley) pour deplacer des maillages denses sans boucle Python ;
- primitives "Part" (verts, faces, label, aux) : ellipsoide, tube a rayon variable, coquille bivalve,
  coquille spiralee ; un Part porte un label de materiau et une valeur auxiliaire par sommet ;
- fusion par remaillage voxel (union booleenne robuste), attributs par plus proche voisin, normales,
  cavite (courbure grossiere) et enveloppe convexe.
"""
import math

import bmesh
import bpy
import numpy as np
from mathutils import Matrix, Vector, kdtree

TAU = 2 * math.pi


# ----------------------------------------------------------------------------- bruit

def _hash(ix, iy, iz, seed):
    with np.errstate(over="ignore"):
        h = (ix.astype(np.uint32) * np.uint32(374761393) + iy.astype(np.uint32) * np.uint32(668265263)
             + iz.astype(np.uint32) * np.uint32(2147483629) + np.uint32((seed * 1013904223 + 12345) % (2 ** 32)))
        h = (h ^ (h >> np.uint32(13))) * np.uint32(1274126177)
        h = h ^ (h >> np.uint32(16))
        h = h * np.uint32(2246822519)
        h = h ^ (h >> np.uint32(15))
    return (h & np.uint32(0xFFFFFF)).astype(np.float32) / np.float32(16777215.0)


def vnoise(p, seed=0):
    """Bruit de valeur 3D lisse dans [0,1]. p : (n,3)."""
    p = np.asarray(p, dtype=np.float32)
    f = np.floor(p)
    t = p - f
    t = t * t * t * (t * (t * 6 - 15) + 10)
    i = f.astype(np.int64)
    ix, iy, iz = i[:, 0], i[:, 1], i[:, 2]
    tx, ty, tz = t[:, 0], t[:, 1], t[:, 2]
    out = 0
    c = {}
    for dx in (0, 1):
        for dy in (0, 1):
            for dz in (0, 1):
                c[dx, dy, dz] = _hash(ix + dx, iy + dy, iz + dz, seed)
    x00 = c[0, 0, 0] + (c[1, 0, 0] - c[0, 0, 0]) * tx
    x10 = c[0, 1, 0] + (c[1, 1, 0] - c[0, 1, 0]) * tx
    x01 = c[0, 0, 1] + (c[1, 0, 1] - c[0, 0, 1]) * tx
    x11 = c[0, 1, 1] + (c[1, 1, 1] - c[0, 1, 1]) * tx
    y0 = x00 + (x10 - x00) * ty
    y1 = x01 + (x11 - x01) * ty
    return y0 + (y1 - y0) * tz


def fbm(p, octaves=4, lac=2.03, gain=0.5, seed=0):
    """fBm normalise ~[0,1] centre sur 0.5."""
    p = np.asarray(p, dtype=np.float32)
    amp, tot, out = 1.0, 0.0, 0.0
    off = np.array([17.3, 5.1, 31.7], dtype=np.float32)
    for o in range(octaves):
        out = out + amp * vnoise(p, seed + o * 7)
        tot += amp
        amp *= gain
        p = p * lac + off
    return out / tot


def worley(p, seed=0, jitter=1.0, f12=False):
    """Bruit cellulaire : distance F1 en unite de cellule (0 ~ 1) ; f12=True -> (F1, F2)."""
    p = np.asarray(p, dtype=np.float32)
    f = np.floor(p)
    i = f.astype(np.int64)
    best = np.full(len(p), 9.0, dtype=np.float32)
    second = np.full(len(p), 9.0, dtype=np.float32)
    for dx in (-1, 0, 1):
        for dy in (-1, 0, 1):
            for dz in (-1, 0, 1):
                cx, cy, cz = i[:, 0] + dx, i[:, 1] + dy, i[:, 2] + dz
                fx = cx + 0.5 + (_hash(cx, cy, cz, seed + 1) - 0.5) * jitter
                fy = cy + 0.5 + (_hash(cx, cy, cz, seed + 2) - 0.5) * jitter
                fz = cz + 0.5 + (_hash(cx, cy, cz, seed + 3) - 0.5) * jitter
                d = (fx - p[:, 0]) ** 2 + (fy - p[:, 1]) ** 2 + (fz - p[:, 2]) ** 2
                second = np.minimum(second, np.maximum(best, d))
                best = np.minimum(best, d)
    if f12:
        return np.sqrt(best), np.sqrt(second)
    return np.sqrt(best)


def smoothstep(a, b, x):
    t = np.clip((x - a) / (b - a), 0.0, 1.0)
    return t * t * (3 - 2 * t)


# ----------------------------------------------------------------------------- parts

class Part:
    """Maillage elementaire : verts (n,3), faces (liste d'indices), label (int), aux (float par sommet)."""

    def __init__(self, verts, faces, label=0, aux=0.0, name="part"):
        self.v = np.asarray(verts, dtype=np.float64)
        self.f = [tuple(int(i) for i in f) for f in faces]
        self.label = label
        self.aux = np.full(len(self.v), aux, dtype=np.float32) if np.isscalar(aux) else np.asarray(aux, dtype=np.float32)
        self.name = name

    def transform(self, m):
        m = np.asarray(m, dtype=np.float64)
        self.v = self.v @ m[:3, :3].T + m[:3, 3]
        return self

    def to_bmesh(self, bm=None):
        bm = bm or bmesh.new()
        vs = [bm.verts.new(tuple(c)) for c in self.v]
        for f in self.f:
            try:
                bm.faces.new([vs[i] for i in f])
            except ValueError:
                pass
        return bm


def mat4(loc=(0, 0, 0), rot_deg=(0, 0, 0), scale=(1, 1, 1)):
    e = tuple(math.radians(a) for a in rot_deg)
    m = Matrix.LocRotScale(Vector(loc), Matrix.Rotation(e[2], 3, "Z") @ Matrix.Rotation(e[1], 3, "Y")
                           @ Matrix.Rotation(e[0], 3, "X"), Vector(scale))
    return np.array(m)


def orient_z(direction):
    """Matrice 3x3 dont Z = direction."""
    q = Vector(direction).normalized().to_track_quat("Z", "Y")
    return np.array(q.to_matrix())


def _from_bm(bm, label, aux, name):
    bm.verts.ensure_lookup_table()
    v = np.array([tuple(x.co) for x in bm.verts])
    idx = {x: i for i, x in enumerate(bm.verts)}
    f = [tuple(idx[x] for x in face.verts) for face in bm.faces]
    bm.free()
    return Part(v, f, label, aux, name)


def ellipsoid(center, radii, subdiv=4, rot_deg=(0, 0, 0), label=0, lump=0.0, seed=0, name="ellipsoid"):
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=subdiv, radius=1.0)
    p = _from_bm(bm, label, 0.0, name)
    if lump:
        p.v *= (1 + lump * (fbm(p.v * 1.7, 3, seed=seed) - 0.5) * 2)[:, None]
    p.v *= np.array(radii)
    return p.transform(mat4(center, rot_deg))


def clip_z(part, z=0.0):
    """Coupe un Part CONVEXE sous le plan z (face plate de pose)."""
    bm = part.to_bmesh()
    geom = list(bm.verts) + list(bm.edges) + list(bm.faces)
    r = bmesh.ops.bisect_plane(bm, geom=geom, plane_co=(0, 0, z), plane_no=(0, 0, 1), clear_inner=True)
    bnd = [e for e in bm.edges if len(e.link_faces) == 1]
    if bnd:
        bmesh.ops.triangle_fill(bm, edges=bnd, use_beauty=True)
    lab = part.label
    # aux est perdu par la coupe : on le recalcule par plus proche voisin
    kd = kdtree.KDTree(len(part.v))
    for i, c in enumerate(part.v):
        kd.insert(Vector(c), i)
    kd.balance()
    bm.verts.ensure_lookup_table()
    aux = np.array([part.aux[kd.find(v.co)[1]] for v in bm.verts], dtype=np.float32)
    return _from_bm(bm, lab, aux, part.name)


def tube(path, radii, sides=14, label=0, aux=0.0, name="tube", cap_start=True, cap_end=True):
    """Tube lisse suivant une polyligne (k,3) a rayon variable (k), extremites arrondies."""
    path = np.asarray(path, dtype=np.float64)
    radii = np.asarray(radii, dtype=np.float64)
    k = len(path)
    tang = np.gradient(path, axis=0)
    tang /= np.linalg.norm(tang, axis=1)[:, None]
    ref = np.array([0.0, 0.0, 1.0]) if abs(tang[0, 2]) < 0.9 else np.array([1.0, 0.0, 0.0])
    nrm = np.cross(tang[0], ref)
    nrm /= np.linalg.norm(nrm)
    rings = []
    for i in range(k):
        if i:
            nrm = nrm - np.dot(nrm, tang[i]) * tang[i]
            nrm /= np.linalg.norm(nrm)
        b = np.cross(tang[i], nrm)
        a = np.arange(sides) * TAU / sides
        rings.append(path[i] + radii[i] * (np.cos(a)[:, None] * nrm + np.sin(a)[:, None] * b))
    verts, faces = [], []
    for r in rings:
        verts.extend(r.tolist())
    for i in range(k - 1):
        for j in range(sides):
            a, b = i * sides + j, i * sides + (j + 1) % sides
            faces.append((a, b, b + sides, a + sides))
    for cap, (ci, sgn) in ((cap_start, (0, -1)), (cap_end, (k - 1, 1))):
        if not cap:
            continue
        tip = len(verts)
        verts.append((path[ci] + sgn * tang[ci] * radii[ci] * 0.55).tolist())
        for j in range(sides):
            a, b = ci * sides + j, ci * sides + (j + 1) % sides
            faces.append((a, tip, b) if sgn > 0 else (a, b, tip))
            if sgn > 0:
                faces[-1] = (b, a, tip)
    p = Part(verts, faces, label, aux, name)
    return p


def _grid_faces(nu, nv, wrap_u=False):
    f = []
    for i in range(nu - (0 if wrap_u else 1)):
        for j in range(nv - 1):
            a, b = i * nv + j, ((i + 1) % nu) * nv + j
            f.append((a, b, b + 1, a + 1))
    return f


def scallop(radius=0.07, thick=0.005, ribs=13, fan_deg=62, cup=0.35, label=1, name="scallop", seed=0):
    """Coquille bivalve en eventail (valve unique), bord interieur (aux=1) / exterieur (aux=0).
    Charniere a l'origine, ouverture vers +Y, concavite vers +Z."""
    nu, nv = 40, 22
    th = np.linspace(-1, 1, nu) * math.radians(fan_deg)
    rho = np.linspace(0.04, 1.0, nv)
    T, R = np.meshgrid(th, rho, indexing="ij")
    rib = 1 + 0.045 * np.cos(ribs * np.pi * T / math.radians(fan_deg)) * smoothstep(0.1, 0.4, R)
    rr = radius * R * rib
    x = rr * np.sin(T)
    y = rr * np.cos(T)
    z = cup * radius * (R ** 2) * 0.9 + 0.012 * radius * np.cos(ribs * np.pi * T / math.radians(fan_deg)) * R
    rnd = (fbm(np.stack([x, y, z], -1).reshape(-1, 3) * 40, 2, seed=seed).reshape(T.shape) - 0.5)
    rr2 = rr * (1 + 0.30 * rnd * smoothstep(0.5, 1.0, R))        # bord ebreche
    x, y = rr2 * np.sin(T), rr2 * np.cos(T)
    P = np.stack([x, y, z], -1)                                   # (nu, nv, 3)
    du = np.gradient(P, axis=0)
    dv = np.gradient(P, axis=1)
    N = np.cross(du, dv)
    N /= np.linalg.norm(N, axis=-1, keepdims=True) + 1e-12
    N *= np.sign(N[..., 2:3] + 1e-9)
    up = P + N * thick / 2           # face concave (interieur nacre)
    lo = P - N * thick / 2
    verts = np.concatenate([up.reshape(-1, 3), lo.reshape(-1, 3)])
    n0 = nu * nv
    faces = [tuple(i for i in f) for f in _grid_faces(nu, nv)]
    faces += [tuple(n0 + i for i in reversed(f)) for f in _grid_faces(nu, nv)]
    def idx(i, j, o=0):
        return o + i * nv + j
    for i in range(nu - 1):          # bords angulaires/radial
        faces.append((idx(i, nv - 1), idx(i + 1, nv - 1), idx(i + 1, nv - 1, n0), idx(i, nv - 1, n0)))
        faces.append((idx(i, 0), idx(i, 0, n0), idx(i + 1, 0, n0), idx(i + 1, 0)))
    for j in range(nv - 1):
        faces.append((idx(0, j), idx(0, j, n0), idx(0, j + 1, n0), idx(0, j + 1)))
        faces.append((idx(nu - 1, j), idx(nu - 1, j + 1), idx(nu - 1, j + 1, n0), idx(nu - 1, j, n0)))
    aux = np.zeros(len(verts), dtype=np.float32)
    aux[:n0] = 1.0
    return Part(verts, faces, label, aux, name)


def spiral_shell(length=0.08, turns=3.6, growth=0.30, ribs=0.05, label=1, name="spiral_shell", seed=0, sides=18,
                 steps_per_turn=40):
    """Coquille turbinee (turbo/cone). Apex vers +Z, ouverture vers -Z/+X. L'ouverture est une cuvette
    (aux=1 = nacre interieure)."""
    n = int(turns * steps_per_turn)
    t = np.linspace(0.0, turns * TAU, n)          # 0 = ouverture (grand), fin = apex
    s = np.exp(-growth * t / TAU * 1.9)           # facteur d'echelle
    A = 0.62                                      # rayon du centre de spire / rayon du tube
    r = 0.5 * s                                    # rayon du tube (unite relative)
    c = np.stack([A * s * np.cos(t) * 1.0, A * s * np.sin(t) * 1.0, (1 - s) * 1.15], -1)
    rad = np.stack([np.cos(t), np.sin(t), np.zeros_like(t)], -1)
    ax = np.array([0, 0, 1.0])
    verts, faces = [], []
    phis = np.arange(sides) * TAU / sides
    for i in range(n):
        rib = 1 + ribs * np.cos(t[i] * 9 * 0 + 14 * t[i] / TAU * 0 + 7 * t[i])
        for j in range(sides):
            ph = phis[j]
            verts.append(c[i] + r[i] * rib * (np.cos(ph) * rad[i] + np.sin(ph) * ax * 1.12))
    for i in range(n - 1):
        for j in range(sides):
            a, b = i * sides + j, i * sides + (j + 1) % sides
            faces.append((a, b, b + sides, a + sides))
    # apex ferme
    tip = len(verts)
    verts.append(c[-1] + np.array([0, 0, r[-1] * 0.6]))
    for j in range(sides):
        a, b = (n - 1) * sides + j, (n - 1) * sides + (j + 1) % sides
        faces.append((b, a, tip))
    # ouverture : cuvette (aux = 1)
    mouth = len(verts)
    mc = c[0] - np.array([-np.sin(t[0]), np.cos(t[0]), 0]) * r[0] * 0.5          # recule dans le tube
    verts.append(mc)
    for j in range(sides):
        a, b = j, (j + 1) % sides
        faces.append((a, mouth, b))
    v = np.array(verts)
    aux = np.zeros(len(v), dtype=np.float32)
    aux[:sides] = 1.0
    aux[mouth] = 1.0
    p = Part(v, faces, label, aux, name)
    # normalisation : longueur (Z) = length
    z0, z1 = p.v[:, 2].min(), p.v[:, 2].max()
    p.v *= length / (z1 - z0)
    p.v[:, 2] -= p.v[:, 2].min()
    return p


# ----------------------------------------------------------------------------- fusion / maillage

def join_parts(parts):
    bm = bmesh.new()
    labels, aux = [], []
    for p in parts:
        p.to_bmesh(bm)
    bm.verts.ensure_lookup_table()
    return bm


def bm_to_object(bm, name, coll=None, free=True):
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    if free:
        bm.free()
    ob = bpy.data.objects.new(name, me)
    (coll or bpy.context.scene.collection).objects.link(ob)
    return ob


def voxel_union(parts, voxel, name="union", coll=None):
    """Union par remaillage voxel de Parts fermes (voir join_parts). Retourne un objet Blender."""
    bm = bmesh.new()
    for p in parts:
        p.to_bmesh(bm)
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    ob = bm_to_object(bm, name, coll)
    ob.data.remesh_voxel_size = voxel
    ob.data.remesh_voxel_adaptivity = 0.0
    with bpy.context.temp_override(active_object=ob, object=ob, selected_objects=[ob]):
        bpy.ops.object.voxel_remesh()
    return ob


def labels_by_nearest(ob, parts):
    """Transfere label / aux des Parts d'origine vers les sommets de ob (plus proche voisin)."""
    allv = np.concatenate([p.v for p in parts])
    lab = np.concatenate([np.full(len(p.v), p.label, dtype=np.int32) for p in parts])
    aux = np.concatenate([p.aux for p in parts])
    kd = kdtree.KDTree(len(allv))
    for i, c in enumerate(allv):
        kd.insert(c.tolist(), i)
    kd.balance()
    me = ob.data
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    co = co.reshape(-1, 3)
    idx = np.empty(n, dtype=np.int64)
    dist = np.empty(n, dtype=np.float32)
    find = kd.find
    for i in range(n):
        _, j, d = find(co[i].tolist())
        idx[i] = j
        dist[i] = d
    return lab[idx], aux[idx], dist


def get_co(me):
    n = len(me.vertices)
    co = np.empty(n * 3, dtype=np.float32)
    me.vertices.foreach_get("co", co)
    return co.reshape(-1, 3)


def set_co(me, co):
    me.vertices.foreach_set("co", np.asarray(co, dtype=np.float32).ravel())
    me.update()


def vertex_normals(me):
    n = len(me.vertices)
    out = np.empty(n * 3, dtype=np.float32)
    me.vertex_normals.foreach_get("vector", out)
    return out.reshape(-1, 3)


def edge_array(me):
    e = np.empty(len(me.edges) * 2, dtype=np.int32)
    me.edges.foreach_get("vertices", e)
    return e.reshape(-1, 2)


def neighbor_mean(co, edges, iters=1, values=None):
    v = co if values is None else values
    n = len(co)
    for _ in range(iters):
        acc = np.zeros_like(v)
        cnt = np.zeros(n, dtype=np.float32)
        np.add.at(acc, edges[:, 0], v[edges[:, 1]])
        np.add.at(acc, edges[:, 1], v[edges[:, 0]])
        np.add.at(cnt, edges[:, 0], 1)
        np.add.at(cnt, edges[:, 1], 1)
        v = acc / np.maximum(cnt, 1)[:, None] if v.ndim == 2 else acc / np.maximum(cnt, 1)
    return v


def cavity(co, nrm, edges, iters=6):
    """>0 en creux, <0 en bosse (distance signee au barycentre voisin lisse, en m)."""
    sm = neighbor_mean(co, edges, iters)
    return np.einsum("ij,ij->i", sm - co, nrm)


def set_attr(me, name, arr, domain="POINT"):
    arr = np.asarray(arr, dtype=np.float32)
    a = me.attributes.get(name) or me.attributes.new(name, "FLOAT", domain)
    a.data.foreach_set("value", arr)


def _hull_bm(points):
    bm = bmesh.new()
    for c in points:
        bm.verts.new(tuple(c))
    res = bmesh.ops.convex_hull(bm, input=bm.verts)
    loose = {g for g in res["geom_interior"] + res["geom_unused"] if isinstance(g, bmesh.types.BMVert)}
    if loose:
        bmesh.ops.delete(bm, geom=list(loose), context="VERTS")
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=0.0005)
    return bm


def convex_hull_obj(points, name, coll=None, max_verts=None):
    """Enveloppe convexe d'un nuage de points -> objet. Si max_verts : echantillonnage du point le plus eloigne
    (farthest point sampling) sur les sommets de l'enveloppe puis nouvelle enveloppe (convexe par construction)."""
    bm = _hull_bm(points)
    if max_verts and len(bm.verts) > max_verts:
        hv = np.array([tuple(v.co) for v in bm.verts])
        bm.free()
        sel = [int(np.argmax(hv[:, 2]))]
        d = np.linalg.norm(hv - hv[sel[0]], axis=1)
        while len(sel) < max_verts:
            i = int(np.argmax(d))
            sel.append(i)
            d = np.minimum(d, np.linalg.norm(hv - hv[i], axis=1))
        bm = _hull_bm(hv[sel])
    bmesh.ops.recalc_face_normals(bm, faces=list(bm.faces))
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    (coll or bpy.context.scene.collection).objects.link(ob)
    return ob


def surface_probe(parts):
    """Retourne probe(origin, target) -> (point, normal) : premier impact du rayon origin->target sur l'union des Parts."""
    from mathutils.bvhtree import BVHTree
    verts, polys, off = [], [], 0
    for p in parts:
        verts.extend(map(tuple, p.v))
        polys.extend([tuple(i + off for i in f) for f in p.f])
        off += len(p.v)
    tree = BVHTree.FromPolygons(verts, polys)

    def probe(origin, target):
        o = Vector(origin)
        d = (Vector(target) - o).normalized()
        loc, nrm, _i, _dist = tree.ray_cast(o, d)
        if loc is None:
            raise ValueError("rayon sans impact")
        return np.array(loc), np.array(nrm)
    return probe
