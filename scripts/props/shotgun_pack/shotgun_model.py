"""Pump shotgun of the bean, v2 realistic (T-247, owner request 2026-10-09 "armes plus réalistes"): a Winchester 1897
style pump gun at Bean's scale (0.72 m, same length, bones, pivots and sockets as the second PC's bean-pirate-shotgun
v0.1 so every existing clip, the C++ and the import stay valid).

Weapon coordinates: +X to the muzzle, +Z up, -Y the right side (ejection port), origin at the stock wrist (v0.1).
Parts per bone: gun_root (stock, receiver, barrel, magazine tube, guard, hammer), pump (ribbed walnut fore-end, brass
ferrules, action bars), bolt (visible in the ejection port, follows the pump), trigger, shell_load (full shell at the
loading port) and shell_eject (spent shell at the ejection port); the two shells are scaled away by the clips when
they are not in play (v0.1 contract).

Built with the kit of the musket pack (scripts/props/musket_pack/kit.py) and the helpers of the bean
(scripts/character/bean_soldier.py). Materials are the procedural ones of textures.py, baked afterwards.
"""
import math

import bpy
from mathutils import Matrix, Vector

import kit as K

V = K.V
TAU = math.tau

# v0.1 contract (design-contract.json): socket positions in weapon space, metres
SOCKETS = {
    "grip_r": ("gun_root", V(-0.035, 0.0, -0.025)),
    "grip_l": ("pump", V(0.225, 0.0, 0.012)),
    "pump_grip": ("pump", V(0.225, 0.0, 0.012)),
    "muzzle": ("gun_root", V(0.425, 0.0, 0.093)),
    "eject_port": ("gun_root", V(0.056, -0.034, 0.080)),
    "load_port": ("gun_root", V(0.055, 0.0, 0.022)),
    "stock_contact": ("gun_root", V(-0.278, 0.0, 0.026)),
}
BARREL_Z = 0.093
MUZZLE_X = 0.425
MAG_Z = 0.046
PUMP_Z = 0.044
PUMP_TRAVEL = 0.065
BOLT_TRAVEL = 0.0585
# the front bead (brass), top point: the aim clips put it on the first person view axis
SIGHT = V(0.412, 0.0, BARREL_Z + 0.0148 + 0.0045)
# cartridge (Bean scale, v0.1): length 0.046, diameter 0.022, axis +X, brass head at -X, crimp at +X
SHELL_LENGTH = 0.046
SHELL_RADIUS = 0.011

# source materials (procedural, textures.py): name -> (kind, sRGB hex, roughness, metallic)
MATERIALS = {
    "MAT-Shotgun-Walnut": ("wood", "4A2812", 0.45, 0.0),
    "MAT-Shotgun-WalnutPump": ("wood", "4E2B14", 0.47, 0.0),
    "MAT-Shotgun-BluedSteel": ("steel", "20252D", 0.33, 1.0),
    "MAT-Shotgun-EdgeSteel": ("steel_bright", "6F767C", 0.28, 1.0),
    "MAT-Shotgun-Brass": ("brass", "B48A3F", 0.36, 1.0),
    "MAT-Shotgun-Recess": ("dark", "050506", 0.65, 0.4),
    "MAT-Shotgun-ShellPaper": ("paper", "A61E1C", 0.5, 0.0),
    "MAT-Shotgun-ShellSpent": ("paper_spent", "9A2B22", 0.55, 0.0),
    "MAT-Shotgun-ShellBrass": ("brass", "C09A4A", 0.3, 1.0),
    "MAT-Shotgun-Lead": ("lead", "7E848B", 0.42, 0.85),
    "MAT-Ammo-Kraft": ("kraft", "A27E4F", 0.85, 0.0),
    "MAT-Ammo-Label": ("label", "1E2848", 0.7, 0.0),
    "MAT-Ammo-Cream": ("label", "E8DEC4", 0.75, 0.0),
}


def materials():
    """Creates (or returns) the source materials with their flat Principled values; textures.py adds the procedural
    node trees before the bake."""
    out = {}
    for name, (kind, hexa, rough, metal) in MATERIALS.items():
        mat = bpy.data.materials.get(name)
        if mat is None:
            mat = bpy.data.materials.new(name)
            mat.use_nodes = True
            bsdf = next((n for n in mat.node_tree.nodes if n.type == "BSDF_PRINCIPLED"), None)
            rgba = K.bean().srgb(hexa)
            if bsdf:
                bsdf.inputs["Base Color"].default_value = rgba
                bsdf.inputs["Roughness"].default_value = rough
                bsdf.inputs["Metallic"].default_value = metal
            mat.diffuse_color = rgba
            mat["bake_kind"] = kind
            mat["bake_hex"] = hexa
        out[name] = mat
    return out


def _use(ob, name):
    """Replaces the material of a freshly built part (the bean helpers only know their palette)."""
    mat = bpy.data.materials[name]
    ob.data.materials.clear()
    ob.data.materials.append(mat)
    return ob


def _bool_cut(target, cutter, material):
    """Boolean difference, the new faces take the cutter's material (a dark recess); the cutter is hidden."""
    mod = target.modifiers.new("Cut-" + cutter.name, "BOOLEAN")
    mod.operation = "DIFFERENCE"
    mod.object = cutter
    try:
        mod.solver = "EXACT"
        mod.material_mode = "TRANSFER"
    except (AttributeError, TypeError):
        pass
    cutter.data.materials.clear()
    cutter.data.materials.append(bpy.data.materials[material])
    # the transferred material must be a slot of the target: the kit remaps face materials by slot index
    if bpy.data.materials[material].name not in [m.name for m in target.data.materials if m]:
        target.data.materials.append(bpy.data.materials[material])
    cutter.hide_render = True
    cutter.hide_viewport = True
    cutter.display_type = "WIRE"
    return cutter


# ------------------------------------------------------------------------------------------------ ammunition

def tagged_material(name, tag, origin=(0.0, 0.0, 0.0), band=True):
    """Copy of a source material for one placed shell: the procedural look is in object space, the printed band and
    the scorch depend on X from the shell's own centre (bake_origin), no band for the shells in the box."""
    if not tag:
        return name
    full = f"{name}@{tag}"
    mat = bpy.data.materials.get(full)
    if mat is None:
        src = bpy.data.materials[name]
        mat = src.copy()
        mat.name = full
        for key in ("bake_kind", "bake_hex"):
            mat[key] = src[key]
        mat["procedural"] = False
    mat["bake_origin"] = [float(v) for v in origin]
    mat["bake_band"] = bool(band)
    return full


def build_shell(coll, prefix, kind="full", center=(0.0, 0.0, 0.0), axis=(1.0, 0.0, 0.0), mat_tag=None, band=True):
    """Exterior of a cartridge: 'full' (red paper hull, star crimp), 'spent' (open scorched mouth, hollow),
    'slug' (lead display token). Local axis +X, visual centre at the origin, then moved to center along axis."""
    B = K.bean()
    materials()
    parts = []
    r, half = SHELL_RADIUS, SHELL_LENGTH * 0.5
    brass, dark = "MAT-Shotgun-ShellBrass", "MAT-Shotgun-Recess"
    paper = tagged_material("MAT-Shotgun-ShellPaper", mat_tag, center, band)
    spent = tagged_material("MAT-Shotgun-ShellSpent", mat_tag, center, band)
    if kind == "slug":
        tok = B.tube(prefix + "_Slug", [V(-0.013, 0, 0), V(-0.011, 0, 0), V(0.006, 0, 0), V(0.012, 0, 0), V(0.016, 0, 0)],
                     [0.0088, 0.0096, 0.0096, 0.0070, 0.0025], "M_Bean_Lead", coll, segs=24)
        parts.append(_use(tok, "MAT-Shotgun-Lead"))
        parts.append(_use(K.ring(prefix + "_SlugBand", (-0.006, 0, 0), 0.0097, 0.0012, "M_Bean_Brass", coll, axis="X", segs=24,
                                 minor_segs=6), brass))
    else:
        # brass head: rim, body, primer, head stamp groove
        head = B.tube(prefix + "_Head", [V(-half, 0, 0), V(-half + 0.0025, 0, 0), V(-half + 0.0032, 0, 0), V(-half + 0.012, 0, 0)],
                      [r, r, r * 0.92, r * 0.92], "M_Bean_Brass", coll, segs=28)
        parts.append(_use(head, brass))
        parts.append(_use(K.disc(prefix + "_Primer", (-half - 0.0004, 0, 0), 0.0032, 0.0012, (-1, 0, 0), "M_Bean_Brass", coll,
                                 segs=16), dark))
        parts.append(_use(K.ring(prefix + "_HeadStamp", (-half - 0.0003, 0, 0), 0.0068, 0.0005, "M_Bean_Brass", coll, axis="X",
                                 segs=24, minor_segs=5), dark))
        if kind == "full":
            hull = B.tube(prefix + "_Hull", [V(-half + 0.011, 0, 0), V(-half + 0.012, 0, 0), V(half - 0.006, 0, 0), V(half - 0.004, 0, 0)],
                          [r * 0.90, r * 0.95, r * 0.95, r * 0.94], "M_Bean_CoatRed", coll, segs=28)
            parts.append(_use(hull, paper))
            # star crimp: a six-fold cone, creases darkened by small rods
            crimp = B.tube(prefix + "_Crimp", [V(half - 0.0045, 0, 0), V(half - 0.002, 0, 0), V(half, 0, 0)],
                           [r * 0.94, r * 0.70, 0.0015], "M_Bean_CoatRed", coll, segs=12)
            parts.append(_use(crimp, paper))
            for i in range(6):
                a = i * TAU / 6
                c, s = math.cos(a), math.sin(a)
                crease = B.tube(prefix + f"_Crease{i}", [V(half - 0.0042, r * 0.92 * c, r * 0.92 * s), V(half - 0.0002, 0.0012 * c, 0.0012 * s)],
                                [0.0005, 0.0004], "M_Bean_Eye", coll, segs=5)
                parts.append(_use(crease, dark))
        else:
            # spent: hull wall with a real open mouth (outer wall, lip, inner wall), scorched toward the mouth
            outer = [V(-half + 0.011, 0, 0), V(-half + 0.012, 0, 0), V(half - 0.004, 0, 0), V(half - 0.001, 0, 0), V(half, 0, 0)]
            wall = B.tube(prefix + "_HullOuter", outer, [r * 0.90, r * 0.96, r * 0.98, r * 1.02, r * 0.99], "M_Bean_CoatRed", coll,
                          segs=28, caps=False)
            parts.append(_use(wall, spent))
            inner = B.tube(prefix + "_HullInner", [V(half, 0, 0), V(half - 0.003, 0, 0), V(-half + 0.014, 0, 0), V(-half + 0.0135, 0, 0)],
                           [r * 0.90, r * 0.84, r * 0.82, 0.0005], "M_Bean_Eye", coll, segs=28, caps=False)
            inner.data.flip_normals()
            parts.append(_use(inner, dark))
            lip = K.ring(prefix + "_Lip", (half, 0, 0), r * 0.945, r * 0.045, "M_Bean_CoatRed", coll, axis="X", segs=28, minor_segs=6)
            parts.append(_use(lip, spent))
            for i in range(5):
                a = i * TAU / 5 + 0.4
                c, s = math.cos(a), math.sin(a)
                tear = B.tube(prefix + f"_Tear{i}", [V(half - 0.006, r * 0.99 * c, r * 0.99 * s), V(half + 0.0005, r * 1.0 * c, r * 1.0 * s)],
                              [0.0005, 0.0004], "M_Bean_Eye", coll, segs=5)
                parts.append(_use(tear, dark))
    rot = Vector((1, 0, 0)).rotation_difference(Vector(axis).normalized()).to_matrix().to_4x4()
    T = Matrix.Translation(Vector(center)) @ rot
    for ob in parts:
        ob.matrix_world = T @ ob.matrix_world
    return parts


def build_shell_box(coll, prefix="Ammo_ShellBox"):
    """Open cardboard box of 12 shells (4 x 3, standing crimp up), lid flipped back, label band on the front.
    Bottom centre at the origin."""
    B = K.bean()
    parts = []
    kraft, label, cream, brass = "MAT-Ammo-Kraft", "MAT-Ammo-Label", "MAT-Ammo-Cream", "MAT-Shotgun-Brass"
    pitch = 2 * SHELL_RADIUS + 0.0015
    nx, ny = 4, 3
    wall, floor = 0.0025, 0.003
    inner_x, inner_y = nx * pitch + 0.002, ny * pitch + 0.002
    outer_x, outer_y = inner_x + 2 * wall, inner_y + 2 * wall
    height = SHELL_LENGTH + 0.004

    def box(name, center, size, mat, bevel=0.0008):
        return _use(B.box(name, center, size, "M_Bean_Cream", coll, bevel=bevel), mat)

    parts.append(box(prefix + "_Floor", (0, 0, floor * 0.5), (outer_x, outer_y, floor), kraft))
    for s, name in ((1, "Back"), (-1, "Front")):
        parts.append(box(prefix + "_Wall" + name, (0, s * (outer_y - wall) * 0.5, height * 0.5), (outer_x, wall, height), kraft))
    for s, name in ((1, "Left"), (-1, "Right")):
        parts.append(box(prefix + "_Wall" + name, (s * (outer_x - wall) * 0.5, 0, height * 0.5), (wall, outer_y, height), kraft))
    # lid: hinged on the back top edge, flipped open 115 degrees
    lid = box(prefix + "_Lid", (0, 0, 0), (outer_x, outer_y + 0.004, wall), kraft)
    hinge = Vector((0, outer_y * 0.5, height))
    R = Matrix.Rotation(math.radians(115.0), 4, "X")
    lid.matrix_world = Matrix.Translation(hinge) @ R @ Matrix.Translation(Vector((0, (outer_y + 0.004) * 0.5, wall * 0.5)))
    parts.append(lid)
    # label band on the front and the lid, with a small brass crown-and-anchor
    parts.append(box(prefix + "_LabelFront", (0, -(outer_y * 0.5 + 0.0006), height * 0.55), (outer_x * 0.86, 0.0012, height * 0.42), label, 0.0004))
    parts.append(box(prefix + "_LabelStripe", (0, -(outer_y * 0.5 + 0.0014), height * 0.55), (outer_x * 0.80, 0.0008, 0.004), cream, 0.0002))
    y = -(outer_y * 0.5 + 0.0022)
    z0 = height * 0.55
    anchor = [V(0, y, z0 + 0.012), V(0, y, z0 - 0.010), V(-0.006, y, z0 - 0.004), V(0, y, z0 - 0.011), V(0.006, y, z0 - 0.004)]
    parts.append(_use(B.tube(prefix + "_Anchor", anchor, [0.0009] * 5, "M_Bean_Brass", coll, segs=8), brass))
    parts.append(_use(B.tube(prefix + "_AnchorBar", [V(-0.007, y, z0 + 0.004), V(0.007, y, z0 + 0.004)], [0.0009, 0.0009], "M_Bean_Brass",
                             coll, segs=8), brass))
    for i, (x, z) in enumerate(((-0.006, z0 + 0.016), (0.0, z0 + 0.019), (0.006, z0 + 0.016))):
        parts.append(_use(B.blob(prefix + f"_Crown{i}", (x, y, z), (0.0022, 0.0009, 0.0028), "M_Bean_Brass", coll, seg=10, ring=6), brass))
    # the shells, standing on their brass heads (axis +Z), crimps up
    for ix in range(nx):
        for iy in range(ny):
            cx = (ix - (nx - 1) * 0.5) * pitch
            cy = (iy - (ny - 1) * 0.5) * pitch
            parts += build_shell(coll, prefix + f"_Shell{ix}{iy}", "full", center=(cx, cy, floor + SHELL_LENGTH * 0.5 + 0.0003), axis=(0, 0, 1),
                                 mat_tag="box", band=False)
    return {"parts": parts, "size": (outer_x, outer_y, height)}


# ------------------------------------------------------------------------------------------------ the gun

def build_shotgun(coll, p=None):
    """parts per bone (gun_root, pump, bolt, trigger, shell_load, shell_eject), sockets, bones, params, cutters."""
    B = K.bean()
    materials()
    p = dict(p or {})
    parts = {"gun_root": [], "pump": [], "bolt": [], "trigger": [], "shell_load": [], "shell_eject": []}
    R = parts["gun_root"]
    cutters = []
    wood, pumpwood = "MAT-Shotgun-Walnut", "MAT-Shotgun-WalnutPump"
    steel, edge, brass, dark = "MAT-Shotgun-BluedSteel", "MAT-Shotgun-EdgeSteel", "MAT-Shotgun-Brass", "MAT-Shotgun-Recess"

    def loft(name, stations, mat, segs=36, expo=2.5, subsurf=1):
        return _use(K.loft(name, stations, "M_Bean_Wood", coll, segs=segs, expo=expo, subsurf=subsurf), mat)

    def box(name, center, size, mat, bevel=0.002):
        return _use(B.box(name, center, size, "M_Bean_Steel", coll, bevel=bevel), mat)

    def tube(name, pts, radii, mat, segs=20, caps=True, squash=1.0):
        return _use(B.tube(name, pts, radii, "M_Bean_Steel", coll, segs=segs, caps=caps, squash=squash), mat)

    def ring(name, center, radius, minor, mat, axis="X", segs=32, minor_segs=8):
        return _use(K.ring(name, center, radius, minor, "M_Bean_Steel", coll, axis=axis, segs=segs, minor_segs=minor_segs), mat)

    def disc(name, center, radius, thickness, normal, mat, segs=16):
        return _use(K.disc(name, center, radius, thickness, normal, "M_Bean_Steel", coll, segs=segs), mat)

    def blob(name, center, radii, mat, seg=16, ring_=8):
        return _use(B.blob(name, center, radii, "M_Bean_Steel", coll, seg=seg, ring=ring_), mat)

    # --- stock: figured walnut, straight comb, drop at the heel, steel butt plate (1897) -----------------------
    stations = [
        (-0.295, 0.0255, 0.0590, 0.0180), (-0.288, 0.0290, 0.0650, 0.0205), (-0.272, 0.0310, 0.0660, 0.0245),
        (-0.240, 0.0310, 0.0630, 0.0300), (-0.200, 0.0290, 0.0545, 0.0365), (-0.160, 0.0262, 0.0455, 0.0415),
        (-0.120, 0.0232, 0.0370, 0.0455), (-0.088, 0.0205, 0.0300, 0.0478), (-0.062, 0.0190, 0.0265, 0.0470),
        (-0.044, 0.0192, 0.0268, 0.0478), (-0.028, 0.0205, 0.0300, 0.0500), (-0.018, 0.0210, 0.0320, 0.0520),
    ]
    R.append(loft("Shotgun_Stock", stations, wood, segs=40, expo=2.5, subsurf=1))
    R.append(loft("Shotgun_ButtPlate", [(-0.2995, 0.0265, 0.0610, 0.0180), (-0.2968, 0.0280, 0.0640, 0.0190), (-0.2930, 0.0280, 0.0640, 0.0190)],
                  steel, segs=40, expo=2.5, subsurf=0))
    for z in (0.0180 + 0.040, 0.0180 - 0.040):
        R.append(disc("Shotgun_ButtScrew%+.2f" % z, (-0.2999, 0, z), 0.0030, 0.0012, (-1, 0, 0), edge, segs=14))
    # tangs over and under the wrist, their screws; the wrist is checkered by the texture
    R.append(box("Shotgun_UpperTang", (-0.070, 0, 0.0776), (0.048, 0.011, 0.0035), steel, bevel=0.0012))
    R.append(disc("Shotgun_TangScrew", (-0.078, 0, 0.0795), 0.0032, 0.0012, (0, 0, 1), edge, segs=14))
    R.append(box("Shotgun_LowerTang", (-0.058, 0, 0.0205), (0.052, 0.010, 0.0035), steel, bevel=0.0012))
    R.append(disc("Shotgun_LowerTangScrew", (-0.070, 0, 0.0188), 0.0030, 0.0012, (0, 0, -1), edge, segs=14))

    # --- receiver: tall blued box, rounded edges, open ejection port on the right, loading port underneath --------
    rec = loft("Shotgun_Receiver", [(-0.052, 0.0190, 0.0400, 0.0770), (-0.047, 0.0197, 0.0410, 0.0770), (0.102, 0.0197, 0.0410, 0.0770),
                                    (0.107, 0.0190, 0.0400, 0.0770)], steel, segs=48, expo=5.0, subsurf=0)
    R.append(rec)
    eject_cut = box("Shotgun_CutEject", (0.052, -0.0215, 0.0830), (0.080, 0.016, 0.044), dark, bevel=0.0)
    cutters.append(_bool_cut(rec, eject_cut, dark))
    load_cut = box("Shotgun_CutLoad", (0.055, 0.0, 0.0345), (0.066, 0.020, 0.012), dark, bevel=0.0)
    cutters.append(_bool_cut(rec, load_cut, dark))
    # carrier (shell lifter) seen through the loading port, brass bright
    R.append(box("Shotgun_Carrier", (0.055, 0, 0.0400), (0.060, 0.015, 0.0025), brass, bevel=0.0006))
    # receiver pins and screws
    for s in (1, -1):
        for i, (x, z) in enumerate(((-0.040, 0.101), (-0.040, 0.054), (0.097, 0.054))):
            R.append(disc(f"Shotgun_RecPin{'L' if s > 0 else 'R'}{i}", (x, s * 0.0198, z), 0.0030, 0.0012, (0, s, 0), edge, segs=14))
    # exposed hammer at the rear of the receiver (cocked), knurled spur
    # cocked: its spur stays level with the top of the receiver, under the sight line of the aim clips
    R.append(tube("Shotgun_Hammer", [V(-0.050, 0, 0.094), V(-0.060, 0, 0.102), V(-0.068, 0, 0.109), V(-0.0735, 0, 0.1140)],
                  [0.0052, 0.0046, 0.0042, 0.0046], edge, segs=12))
    R.append(blob("Shotgun_HammerSpur", (-0.0760, 0, 0.1152), (0.0052, 0.0060, 0.0034), steel, seg=14, ring_=7))

    # --- barrel, bore, bead, magazine tube and its cap, barrel band ------------------------------------------------
    R.append(tube("Shotgun_Barrel", [V(0.085, 0, BARREL_Z), V(0.120, 0, BARREL_Z), V(0.260, 0, BARREL_Z), V(0.400, 0, BARREL_Z), V(MUZZLE_X, 0, BARREL_Z)],
                  [0.0178, 0.0172, 0.0158, 0.0149, 0.0148], steel, segs=28))
    R.append(blob("Shotgun_Bore", (MUZZLE_X + 0.0006, 0, BARREL_Z), (0.0018, 0.0108, 0.0108), dark, seg=24, ring_=10))
    R.append(ring("Shotgun_MuzzleCrown", (MUZZLE_X - 0.0008, 0, BARREL_Z), 0.0128, 0.0022, edge, axis="X", segs=28, minor_segs=6))
    R.append(box("Shotgun_BeadBase", (SIGHT.x, 0, BARREL_Z + 0.0148 + 0.0012), (0.0060, 0.0036, 0.0030), steel, bevel=0.0008))
    R.append(blob("Shotgun_Bead", (SIGHT.x, 0, SIGHT.z - 0.0022), (0.0024, 0.0024, 0.0024), brass, seg=16, ring_=8))
    R.append(tube("Shotgun_MagTube", [V(0.095, 0, MAG_Z), V(0.250, 0, MAG_Z), V(0.398, 0, MAG_Z)], [0.0116, 0.0114, 0.0112], steel, segs=24))
    R.append(tube("Shotgun_MagCap", [V(0.398, 0, MAG_Z), V(0.411, 0, MAG_Z)], [0.0124, 0.0124], edge, segs=24))
    R.append(disc("Shotgun_MagCapScrew", (0.4112, 0, MAG_Z), 0.0040, 0.0014, (1, 0, 0), edge, segs=16))
    R.append(ring("Shotgun_BandBarrel", (0.386, 0, BARREL_Z), 0.0158, 0.0024, steel, axis="X", segs=28, minor_segs=7))
    R.append(ring("Shotgun_BandTube", (0.386, 0, MAG_Z), 0.0124, 0.0024, steel, axis="X", segs=24, minor_segs=7))
    R.append(box("Shotgun_BandBridge", (0.386, 0, (BARREL_Z + MAG_Z) * 0.5), (0.0075, 0.0110, BARREL_Z - MAG_Z), steel, bevel=0.0015))

    # --- trigger guard and plate --------------------------------------------------------------------------------
    guard = [V(-0.006, 0, 0.034), V(-0.011, 0, 0.012), V(-0.007, 0, -0.012), V(0.012, 0, -0.0245), V(0.046, 0, -0.0245), V(0.062, 0, -0.013),
             V(0.064, 0, 0.016), V(0.060, 0, 0.034)]
    R.append(tube("Shotgun_Guard", guard, [0.0036] * len(guard), steel, segs=12, squash=0.55))
    R.append(box("Shotgun_TriggerPlate", (0.028, 0, 0.0330), (0.075, 0.016, 0.0035), steel, bevel=0.001))

    # --- pump (fore-end): ribbed walnut on the magazine tube, brass ferrules, action bars into the receiver ---------
    ribs = []
    x = 0.157
    ribs.append((x, 0.0205, 0.0225, PUMP_Z))
    ribs.append((0.1625, 0.0235, 0.0260, PUMP_Z))
    x = 0.166
    while x < 0.284:
        ribs.append((x, 0.0248, 0.0282, PUMP_Z))
        ribs.append((x + 0.0045, 0.0248, 0.0282, PUMP_Z))
        ribs.append((x + 0.0062, 0.0218, 0.0250, PUMP_Z))
        ribs.append((x + 0.0088, 0.0218, 0.0250, PUMP_Z))
        x += 0.0105
    ribs.append((0.288, 0.0235, 0.0262, PUMP_Z))
    ribs.append((0.293, 0.0205, 0.0225, PUMP_Z))
    parts["pump"].append(loft("Shotgun_PumpWood", ribs, pumpwood, segs=32, expo=2.2, subsurf=0))
    for x in (0.1585, 0.2915):
        parts["pump"].append(ring(f"Shotgun_Ferrule{int(x * 1000)}", (x, 0, PUMP_Z), 0.0225, 0.0030, brass, axis="X", segs=32, minor_segs=8))
    for s in (1, -1):
        parts["pump"].append(box(f"Shotgun_ActionBar{'L' if s > 0 else 'R'}", (0.1275, s * 0.0158, MAG_Z), (0.066, 0.0036, 0.0095), edge, bevel=0.0008))

    # --- bolt: seen in the ejection port, slides 0.9 x the pump; extractor claw and flutes ------------------------
    parts["bolt"].append(box("Shotgun_Bolt", (0.052, -0.0162, 0.0825), (0.070, 0.0045, 0.0340), edge, bevel=0.0012))
    parts["bolt"].append(box("Shotgun_Extractor", (0.0845, -0.0188, 0.0825), (0.0055, 0.0018, 0.0110), dark, bevel=0.0003))
    for i in range(3):
        parts["bolt"].append(box(f"Shotgun_BoltFlute{i}", (0.030 + i * 0.016, -0.0188, 0.0825), (0.0016, 0.0010, 0.0240), dark, bevel=0.0002))

    # --- trigger ----------------------------------------------------------------------------------------------
    parts["trigger"].append(tube("Shotgun_Trigger", [V(0.034, 0, 0.026), V(0.029, 0, 0.012), V(0.026, 0, 0.000), V(0.019, 0, -0.011)],
                                 [0.0032, 0.0029, 0.0026, 0.0020], edge, segs=10))

    # --- shells on their bones (hidden by the clips when not in play) --------------------------------------------
    parts["shell_load"] += build_shell(coll, "Shotgun_LoadShell", "full", SOCKETS["load_port"][1], mat_tag="load")
    parts["shell_eject"] += build_shell(coll, "Shotgun_EjectShell", "spent", SOCKETS["eject_port"][1], mat_tag="eject")

    bones = {
        "gun_root": (V(0, 0, 0), V(0.12, 0, 0), None),
        "pump": (V(0.225, 0, 0.035), V(0.260, 0, 0.035), "gun_root"),
        "bolt": (V(0.048, -0.027, 0.081), V(0.083, -0.027, 0.081), "gun_root"),
        "trigger": (V(0.034, 0, 0.018), V(0.069, 0, 0.018), "gun_root"),
        "shell_load": (SOCKETS["load_port"][1], SOCKETS["load_port"][1] + V(0.035, 0, 0), "gun_root"),
        "shell_eject": (SOCKETS["eject_port"][1], SOCKETS["eject_port"][1] + V(0.035, 0, 0), "gun_root"),
    }
    return {"parts": parts, "sockets": dict(SOCKETS), "bones": bones, "params": p, "cutters": cutters,
            "contract": {"forward_axis": "+X", "up_axis": "+Z", "pump_travel_m": PUMP_TRAVEL, "bolt_travel_m": BOLT_TRAVEL,
                         "shell_axis": "+X", "shell_dimensions_m": [SHELL_LENGTH, 2 * SHELL_RADIUS, 2 * SHELL_RADIUS],
                         "sight_m": [*SIGHT], "barrel_z_m": BARREL_Z, "muzzle_x_m": MUZZLE_X}}
