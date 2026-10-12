"""Shape helpers of the Bean item kit, local copy for the musket pack (T-212).

Copied from the second PC's factory/kits/bean_kit.py (OneDrive maquette-blender, read only) with only what the musket
needs; the helpers and the palette come from this repository's scripts/character/bean_soldier.py, so the musket and the
first person arms are built with the same code as the player bean.
"""
import importlib.util
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

REPO = Path(__file__).resolve().parents[3]
BEAN_PY = REPO / "scripts" / "character" / "bean_soldier.py"

# materials added to the bean's palette (sRGB hex, roughness, metallic), as in bean_kit
EXTRA_PALETTE = {
    "M_Bean_Wood": ("6E3B1E", 0.55, 0.0),
    "M_Bean_WoodDark": ("4A2814", 0.60, 0.0),
    "M_Bean_Steel": ("7D848C", 0.38, 1.0),
    "M_Bean_SteelDark": ("454A50", 0.40, 1.0),
    "M_Bean_Linen": ("E6DBC2", 0.95, 0.0),
    "M_Bean_Flint": ("34303A", 0.22, 0.0),
    "M_Bean_Lead": ("7E848B", 0.42, 0.85),
    "M_Bean_Powder": ("17171A", 0.90, 0.0),
    "M_Bean_Paper": ("EFE6CF", 0.90, 0.0),
}
# densities (kg/m3) for the reported mass, by material name or by the material's baked "look" (looks.py, v2)
DENSITY = {"M_Bean_Wood": 650, "M_Bean_WoodDark": 650, "M_Bean_Steel": 7850, "M_Bean_SteelDark": 7850,
           "M_Bean_Brass": 8500, "M_Bean_Leather": 400, "M_Bean_Linen": 250, "M_Bean_Flint": 2600,
           "M_Bean_Lead": 11340, "M_Bean_Paper": 300,
           "wood": 650, "iron": 7850, "darkiron": 7850, "brass": 8500, "leather": 400, "linen": 250, "flint": 2600,
           "lead": 11340, "paper": 300, "horn": 1300}
TAU = 2 * math.pi
_B = None


def bean():
    """scripts/character/bean_soldier.py as a module (definitions only, its main() is not run)."""
    global _B
    if _B is None:
        spec = importlib.util.spec_from_file_location("bean_soldier_ref", BEAN_PY)
        _B = importlib.util.module_from_spec(spec)
        sys.modules["bean_soldier_ref"] = _B
        spec.loader.exec_module(_B)
        _B.PALETTE.update(EXTRA_PALETTE)
    return _B


def V(*a):
    return Vector(a)


def loft(name, stations, mat, coll, segs=32, expo=2.6, subsurf=1):
    """Swept volume along X: stations = [(x, half width Y, half height Z, centre Z)], super-ellipse sections."""
    B = bean()
    bm = bmesh.new()
    rows = []
    for x, hw, hh, cz in stations:
        ring_verts = []
        for i in range(segs):
            a = TAU * i / segs
            c, s = math.cos(a), math.sin(a)
            ring_verts.append(bm.verts.new((x, hw * math.copysign(abs(c) ** (2 / expo), c),
                                            cz + hh * math.copysign(abs(s) ** (2 / expo), s))))
        rows.append(ring_verts)
    B.grid_faces(bm, rows, True)
    bm.faces.new(rows[0][::-1])
    bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B.obj_from_bm(name, bm, mat, coll, subsurf=subsurf)


def ring(name, center, radius, minor, mat, coll, axis="Z", segs=32, minor_segs=10):
    """Torus (bands, pipes, rings)."""
    B = bean()
    bm = bmesh.new()
    rows = []
    for i in range(segs):
        u = TAU * i / segs
        row = []
        for j in range(minor_segs):
            v = TAU * j / minor_segs
            p = V((radius + minor * math.cos(v)) * math.cos(u), (radius + minor * math.cos(v)) * math.sin(u),
                  minor * math.sin(v))
            if axis == "X":
                p = V(p.z, p.x, p.y)
            elif axis == "Y":
                p = V(p.x, p.z, p.y)
            row.append(bm.verts.new(p + Vector(center)))
        rows.append(row)
    rows.append(rows[0])
    B.grid_faces(bm, rows, True)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B.obj_from_bm(name, bm, mat, coll)


def cylinder_x(name, x0, x1, radius, center_yz, mat, coll, segs=16, caps=True):
    """Straight cylinder along X (screws, pins, wires): no subdivision, exact radius."""
    B = bean()
    bm = bmesh.new()
    rows = [[bm.verts.new((x, center_yz[0] + radius * math.cos(TAU * i / segs),
                           center_yz[1] + radius * math.sin(TAU * i / segs))) for i in range(segs)] for x in (x0, x1)]
    B.grid_faces(bm, rows, True)
    if caps:
        bm.faces.new(rows[0][::-1])
        bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B.obj_from_bm(name, bm, mat, coll)


def disc(name, center, radius, thickness, normal, mat, coll, segs=20):
    """Flat round head (screw heads, escutcheons) lying on a surface: centre on the surface, normal outwards."""
    B = bean()
    bm = bmesh.new()
    n = Vector(normal).normalized()
    q = Vector((0, 0, 1)).rotation_difference(n)
    rows = []
    for z, r in ((-thickness * 0.5, radius), (thickness * 0.5, radius * 0.92), (thickness * 0.75, radius * 0.6)):
        rows.append([bm.verts.new(Vector(center) + q @ Vector((r * math.cos(TAU * i / segs), r * math.sin(TAU * i / segs), z)))
                     for i in range(segs)])
    B.grid_faces(bm, rows, True)
    bm.faces.new(rows[0][::-1])
    bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B.obj_from_bm(name, bm, mat, coll)


def flint_chip(name, size, coll, seed=3):
    """Faceted flint chip (no subdivision: sharp edges)."""
    B = bean()
    bm = bmesh.new()
    bmesh.ops.create_icosphere(bm, subdivisions=1, radius=0.5)
    k = seed
    for v in bm.verts:
        k = (k * 1103515245 + 12345) % 2 ** 31
        jitter = 0.80 + 0.35 * (k / 2 ** 31)
        v.co = V(v.co.x * size[0] * jitter, v.co.y * size[1] * jitter, v.co.z * size[2] * (0.9 + 0.2 * jitter))
    return B.obj_from_bm(name, bm, "M_Bean_Flint", coll, smooth=False)


def lathe(name, profile, mat, coll, center_yz=(0.0, 0.0), segs=24, smooth=True, transform=None):
    """Surface of revolution about an axis parallel to X through (y, z) = center_yz: profile = [(x, r)] in order; a
    point with r = 0 is a pole (cap). The profile may fold back (a muzzle: outer wall, rim, bore wall, bottom), the
    mesh is one closed manifold with outward normals. transform (Matrix) is applied to the vertices (tilted rods)."""
    B = bean()
    bm = bmesh.new()
    rows = []
    for x, r in profile:
        if r <= 1e-6:
            rows.append(bm.verts.new((x, center_yz[0], center_yz[1])))
        else:
            rows.append([bm.verts.new((x, center_yz[0] + r * math.cos(TAU * i / segs), center_yz[1] + r * math.sin(TAU * i / segs)))
                         for i in range(segs)])
    for a, b in zip(rows, rows[1:]):
        if isinstance(a, list) and isinstance(b, list):
            B.grid_faces(bm, [a, b], True)
        elif isinstance(a, list):
            for i in range(segs):
                bm.faces.new((a[i], a[(i + 1) % segs], b))
        elif isinstance(b, list):
            for i in range(segs):
                bm.faces.new((a, b[(i + 1) % segs], b[i]))
    bmesh.ops.remove_doubles(bm, verts=bm.verts, dist=1e-7)
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    if transform is not None:
        bm.transform(transform)
    return B.obj_from_bm(name, bm, mat, coll, smooth=smooth)


def plate(name, outline_xz, y0, y1, mat, coll, bevel=0.0):
    """Flat plate: a polygon outline in the XZ plane (counter clockwise seen from -Y) extruded from y0 to y1."""
    B = bean()
    bm = bmesh.new()
    lo = [bm.verts.new((x, y0, z)) for x, z in outline_xz]
    hi = [bm.verts.new((x, y1, z)) for x, z in outline_xz]
    bm.faces.new(lo)
    bm.faces.new(hi[::-1])
    n = len(lo)
    for i in range(n):
        bm.faces.new((lo[i], hi[i], hi[(i + 1) % n], lo[(i + 1) % n]))
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    ob = B.obj_from_bm(name, bm, mat, coll, smooth=False)
    if bevel:
        mod = ob.modifiers.new("Bevel", "BEVEL")
        mod.width = bevel
        mod.segments = 2
    return ob


def strap(name, pts, half_w, half_t, mat, coll, segs=12, expo=4.0, caps=True, subsurf=1):
    """Sweep of a rounded rectangle along a path lying in the XZ plane: half_w across (along Y), half_t through the
    plate (in-plane normal). Trigger guard bows, butt tangs, sling, buckles."""
    B = bean()
    bm = bmesh.new()
    rows = []
    for k, p in enumerate(pts):
        d = (pts[min(k + 1, len(pts) - 1)] - pts[max(k - 1, 0)]).normalized()
        side = d.cross(Vector((0, 1, 0)))
        if side.length < 1e-5:
            side = d.cross(Vector((1, 0, 0)))
        side.normalize()
        hw = half_w[k] if isinstance(half_w, (list, tuple)) else half_w
        ht = half_t[k] if isinstance(half_t, (list, tuple)) else half_t
        ring = []
        for i in range(segs):
            a = TAU * i / segs
            c, s = math.cos(a), math.sin(a)
            u = hw * math.copysign(abs(c) ** (2 / expo), c)
            v = ht * math.copysign(abs(s) ** (2 / expo), s)
            ring.append(bm.verts.new(p + Vector((0, 1, 0)) * u + side * v))
        rows.append(ring)
    B.grid_faces(bm, rows, True)
    if caps:
        bm.faces.new(rows[0][::-1])
        bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B.obj_from_bm(name, bm, mat, coll, subsurf=subsurf)


def section_loft(name, stations, mat, coll, segs=64, subsurf=1, shape=None):
    """Loft along X of asymmetric super-ellipse sections: stations = [(x, half width, z top, z bottom, expo top,
    expo bottom)]. shape(x, y, z, top) -> z lets the caller carve the section (barrel channel, ramrod groove)."""
    B = bean()
    bm = bmesh.new()
    rows = []
    for x, hw, zt, zb, et, eb in stations:
        zc = 0.5 * (zt + zb)
        ring = []
        for i in range(segs):
            a = TAU * i / segs
            c, s = math.cos(a), math.sin(a)
            e = et if s >= 0 else eb
            y = hw * math.copysign(abs(c) ** (2 / e), c)
            h = (zt - zc) if s >= 0 else (zc - zb)
            z = zc + h * math.copysign(abs(s) ** (2 / e), s)
            if shape is not None:
                z = shape(x, y, z, s >= 0)
            ring.append(bm.verts.new((x, y, z)))
        rows.append(ring)
    B.grid_faces(bm, rows, True)
    bm.faces.new(rows[0][::-1])
    bm.faces.new(rows[-1])
    bmesh.ops.recalc_face_normals(bm, faces=bm.faces)
    return B.obj_from_bm(name, bm, mat, coll, subsurf=subsurf)


def interp(x, keys):
    """Linear interpolation of (x, value) keys, held outside."""
    if x <= keys[0][0]:
        return keys[0][1]
    for (a, va), (b, vb) in zip(keys, keys[1:]):
        if x <= b:
            return va + (vb - va) * (x - a) / max(b - a, 1e-9)
    return keys[-1][1]


def musket_params(p=None):
    d = {
        "butt_x": -0.30, "muzzle_x": 0.62, "breech_x": 0.03, "barrel_z": 0.045,
        "barrel_r": 0.017, "muzzle_r": 0.0155, "bore_r": 0.0095,
        "wrist_hw": 0.020, "wrist_hh": 0.022, "butt_hw": 0.026, "butt_hh": 0.058,
        "grip_l_x": 0.26, "band_x": [0.34, 0.52], "guard_bottom_z": -0.080,
        "ramrod_r": 0.0055, "sling": True,
    }
    d.update(p or {})
    return d


def _evaluated_bmesh(ob, dg):
    ev = ob.evaluated_get(dg)
    me = ev.to_mesh()
    bm = bmesh.new()
    bm.from_mesh(me)
    bm.transform(ob.matrix_world)
    ev.to_mesh_clear()
    return bm


def part_physics(objs):
    """Mass (kg) and centre of mass from the evaluated volumes and the material densities."""
    bpy.context.view_layer.update()
    dg = bpy.context.evaluated_depsgraph_get()
    mass, moment, total = 0.0, Vector(), 0.0
    for ob in objs:
        bm = _evaluated_bmesh(ob, dg)
        vol = abs(bm.calc_volume(signed=True))
        c = sum((v.co for v in bm.verts), Vector()) / max(len(bm.verts), 1)
        bm.free()
        mat = ob.material_slots[0].material if ob.material_slots and ob.material_slots[0].material else None
        key = (mat.get("look") if mat else None) or (mat.name if mat else "")
        m = vol * DENSITY.get(key, 700)
        mass += m
        moment += c * m
        total += vol
    return {"mass_kg": round(mass, 4), "volume_m3": round(total, 7),
            "com": [round(v, 4) for v in (moment / mass if mass else Vector())]}


def join_parts(name, parts_by_bone, coll, bone_names=None):
    """Joins the parts (modifiers applied) into one multi-material mesh; with bone_names, every part is weighted
    100 % on its bone (rigid parts)."""
    B = bean()
    all_parts = [ob for objs in parts_by_bone.values() for ob in objs]
    bpy.context.view_layer.update()
    B.box_uvs(all_parts)
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


def make_skeleton(name, bones, sockets, coll):
    """Weapon armature: rigid bones + non deforming socket_* bones (Unreal sockets and hand targets)."""
    data = bpy.data.armatures.new(name)
    arm = bpy.data.objects.new(name, data)
    coll.objects.link(arm)
    arm.show_in_front = True
    bpy.context.view_layer.objects.active = arm
    for o in bpy.context.view_layer.objects:
        o.select_set(False)
    arm.select_set(True)
    bpy.ops.object.mode_set(mode="EDIT")
    for bname, (head, tail, parent) in bones.items():
        eb = data.edit_bones.new(bname)
        eb.head, eb.tail = head, tail
        eb.roll = 0.0
        if parent:
            eb.parent = data.edit_bones[parent]
    for sname, (parent, pos) in sockets.items():
        eb = data.edit_bones.new("socket_" + sname)
        eb.head, eb.tail = pos, pos + Vector((0.01, 0, 0))
        eb.parent = data.edit_bones[parent]
        eb.use_deform = False
    bpy.ops.object.mode_set(mode="OBJECT")
    return arm


def build_skeletal_musket(builder, coll, params=None):
    """The musket as a skeletal mesh: armature + mesh weighted per part (bean_kit.build_assembly for the musket)."""
    res = builder(coll, params)
    res["physics"] = part_physics([o for objs in res["parts"].values() for o in objs])
    arm = make_skeleton("SKEL-Musket-Template", res["bones"], res["sockets"], coll)
    mesh = join_parts("SK_Bean_Musket", res["parts"], coll, bone_names=list(res["bones"]))
    mesh.parent = arm
    mod = mesh.modifiers.new("Armature", "ARMATURE")
    mod.object = arm
    res.update(object=mesh, armature=arm)
    return res
