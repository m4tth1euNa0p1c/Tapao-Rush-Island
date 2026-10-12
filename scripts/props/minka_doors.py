"""Minka of the fragment villages with working doors and a solid shell (T-186, owner request 2026-10-06).

SM_Minka (village_japonais.py) is the whole house in one mesh: its 27 sliding panels are frozen in place and its
67 UCX boxes leave out the roof, the panels, the beams and most of the furniture. This export splits it for
AVillageSpawner (UVillageHouseKit, DA_MinkaKit):
- SM_Minka_Shell: the house without its sliding panels, same frame as SM_Minka (front-left corner of the main body
  at ground level), with a simple collision covering every shape: the 67 boxes of the structure, plus the roof
  (four thatch slabs, the gable prism, the ridge), the wall plates, the transoms over the openings, the ceiling,
  the tie logs and eave purlins, the plinth and the space under the raised floor and the engawa, the outer
  stepping stones and the furniture (irori, kettle, futon, andon, vase, cushions, shelf, bowls, firewood...);
- SM_Minka_<Type>_<W>x<H>: one mesh per panel size (shoji, fusuma, board door), pivot at its bottom-left corner on
  its track, with one UCX box at least 6 cm thick;
- minka_doors.json: in the house frame of Unreal (cm, x = Blender x, y = -Blender y, yaw = -Blender yaw) the
  panels (closed position, yaw, travel, doorway, share of the travel used when the doorway opens), the doorways
  (centre, along-track yaw, half width, height) and the slots of the objects put in the house (relic, items).

Run (background, the maquettes PC or this one):
  blender --background --factory-startup --python scripts/props/minka_doors.py
Outputs: art/export/Props/Village/MinkaDoors/
"""
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import minka_rurale  # noqa: E402
from minka import build, geo  # noqa: E402

ROOT = HERE.parents[1]
OUT = ROOT / "art" / "export" / "Props" / "Village" / "MinkaDoors"
SHELL = "SM_Minka_Shell"
PLINTH_DEPTH = minka_rurale.PLINTH_DEPTH
MIN_PANEL_COLLISION = 0.06       # m: a thin panel still stops a thrown object
PAIR_OPENING_LEAF = "_1"         # of two leaves on two tracks, this one slides behind the other

# Slots of the objects put in the house (Blender frame, m, on the surface they rest on; yaw in Unreal degrees: 0 =
# along the facade, 90 = along the side walls, the long side of an object parallel to the nearest wall). The relic
# stands in the tokonoma, under the scroll, facing the room (+90: its front is local +X). The everyday objects lie
# on free floor away from the furniture, from the passages of the 14 doorways (a leaf opens on the low side of its
# bay) and from the totem of T-143 (3.1, 2.0 in room B, by the irori); two lie on the tansu and the futon.
F = build.P["floor"]
RELIC_SLOT = ((6.28, 6.74, F + 0.10), 90.0)
ITEM_SLOTS = [
    ("room_b", (3.20, 0.50, F), 0.0), ("room_b", (5.00, 0.55, F), 0.0), ("room_b", (2.30, 3.25, F), 0.0),
    ("room_b", (5.00, 3.25, F), 0.0),
    ("room_c", (6.80, 0.45, F), 0.0), ("room_c", (8.60, 1.40, F), 90.0), ("room_c", (8.50, 3.20, F), 0.0),
    ("room_c", (6.80, 3.25, F), 0.0),
    ("room_e", (7.00, 4.20, F), 0.0), ("room_e", (5.90, 5.60, F), 90.0), ("room_e", (8.60, 5.20, F), 90.0),
    ("room_d", (2.40, 4.40, F), 90.0), ("room_d", (2.40, 5.80, F), 90.0), ("room_d", (5.00, 5.60, F), 90.0),
    ("room_d_tansu", (2.60, 6.84, F + 0.95), 0.0), ("room_d_futon", (4.15, 6.70, F + 0.30), 0.0),
    ("doma", (0.40, 2.20, 0.0), 90.0), ("doma", (0.50, 3.80, 0.0), 90.0), ("doma", (1.30, 4.60, 0.0), 90.0),
]

# Furniture without collision in SM_Minka: one box around each group of objects (their world bounds)
FURNITURE_GROUPS = [
    ("Minka_Irori_Frame",), ("Minka_Irori_Tetsubin",), ("Minka_Irori_Hook",), ("Minka_Irori_JizaiKagi",),
    ("Minka_tea_set_01",), ("Minka_Futon", "Minka_Futon_Cover"), ("Minka_Andon_Frame", "Minka_Andon_Paper"),
    ("Minka_ceramic_vase_02", "Minka_Ikebana_Stem", "Minka_Ikebana_Leaves", "Minka_Ikebana_Flowers"),
    ("Minka_Kamado_Pots",), ("Minka_Doma_Shelf",), ("Minka_Bowl0",), ("Minka_Bowl1",), ("Minka_Firewood",),
    ("Minka_wooden_bucket_01",), ("Minka_wooden_broom",),
]


def ue(p):
    """Blender metres (house frame) to Unreal centimetres."""
    return [round(p[0] * 100.0, 1), round(-p[1] * 100.0, 1), round(p[2] * 100.0, 1)]


def world_bounds(objs):
    pts = [o.matrix_world @ Vector(c) for o in objs for c in o.bound_box]
    return (Vector([min(p[i] for p in pts) for i in range(3)]), Vector([max(p[i] for p in pts) for i in range(3)]))


def hull_object(name, points):
    """UCX convex: hull of the points."""
    bm = bmesh.new()
    verts = [bm.verts.new(Vector(p)) for p in points]
    bmesh.ops.convex_hull(bm, input=verts)
    for v in [v for v in bm.verts if not v.link_faces]:
        bm.verts.remove(v)
    me = bpy.data.meshes.new(name)
    bm.to_mesh(me)
    bm.free()
    ob = bpy.data.objects.new(name, me)
    bpy.context.scene.collection.objects.link(ob)
    ob.display_type = "WIRE"
    return ob


def box_points(mn, mx):
    return [(x, y, z) for x in (mn[0], mx[0]) for y in (mn[1], mx[1]) for z in (mn[2], mx[2])]


class ShellCollision:
    """Every convex of SM_Minka_Shell: (label, points)."""

    def __init__(self, mk):
        self.mk = mk
        self.items = []

    def box(self, label, mn, mx):
        self.items.append((label, box_points(mn, mx)))

    def hull(self, label, points):
        self.items.append((label, [tuple(p) for p in points]))

    def structure(self):
        for i, (mn, mx) in enumerate(self.mk.ucx):
            self.box(f"structure_{i:02d}", mn, mx)

    def base(self):
        e = build.P["eng_w"]
        self.box("plinth_body", (0.0, 0.0, -PLINTH_DEPTH), (9.0, 7.2, 0.0))
        self.box("under_floor", (1.8, 0.0, 0.0), (9.0, 7.2, 0.40))
        self.box("under_engawa_front", (1.8, -e - 0.06, -PLINTH_DEPTH), (9.0 + e + 0.06, 0.0, F - 0.04))
        self.box("under_engawa_side", (9.0, 0.0, -PLINTH_DEPTH), (9.0 + e + 0.06, 7.28, F - 0.04))
        # stepping stones up to the engawa (the one of the doma already has its box)
        self.box("step_front", (4.04, -1.93, -0.10), (4.76, -1.37, 0.22))
        self.box("step_side", (9.0 + e + 0.14, 1.44, -0.10), (9.0 + e + 0.70, 2.16, 0.22))
        self.box("edge_beam", (1.74, 0.0, F - 0.16), (1.86, 7.2, F + 0.005))

    def transoms(self):
        """Kamoi and plaster over every opening, from the top of its panels to the wall plate."""
        mk, P = self.mk, build.P
        top = P["keta"][0]
        t = P["wall"]
        n = 0
        for axis, fixed, bays in mk.walls_plan():
            for a, b, kind in bays:
                if kind not in ("door", "shoji", "fusuma", "step"):
                    continue
                z0 = mk.base_z(axis, fixed, a, b)
                half = P["post"] / 2
                a1, b1 = a + half, b - half
                if fixed == 6.3:
                    a1 = a + (0.04 if a > 5.4 + 1e-6 else half)
                    b1 = b - (0.04 if b < 9.0 - 1e-6 else half)
                kz = 2.10 if z0 == 0.0 else mk.kamoi
                if kind == "door":
                    kz, a1 = 2.0, a1 + 0.62
                elif kind == "step":
                    kz = mk.kamoi
                mn, mx = mk._seg_box(axis, fixed, a1, b1, kz, top, t + 0.02)
                self.box(f"transom_{n:02d}", mn, mx)
                n += 1

    def beams(self):
        P = build.P
        k0, k1 = P["keta"]
        e = P["eng_w"]
        ze = P["z_eave"]
        for y in (0.0, 3.6, 7.2):
            self.box(f"keta_y{y:.1f}", (-0.15, y - 0.09, k0), (9.15, y + 0.09, k1))
        for x in (0.0, 1.8, 5.4, 9.0):
            self.box(f"keta_x{x:.1f}", (x - 0.09, -0.15, k0), (x + 0.09, 7.35, k1))
        self.box("keta_toko", (5.4, 6.23, k0), (9.0, 6.37, k0 + 0.14))
        self.box("ceiling", (1.8, 0.0, k0 - 0.02), (9.0, 7.2, k0))
        for y in (1.8, 5.4):
            self.box(f"tie_log_y{y:.1f}", (-0.2, y - 0.13, 3.05), (2.0, y + 0.18, 3.35))
        self.box("tie_log_x", (0.75, -0.2, 3.25), (1.07, 7.4, 3.57))
        self.box("purlin_front", (1.6, -e - 0.08, ze - 0.12), (9.0 + e + 0.08, -e + 0.08, ze + 0.05))
        self.box("purlin_side", (9.0 + e - 0.08, -e, ze - 0.12), (9.0 + e + 0.08, 7.4, ze + 0.05))

    def furniture(self):
        tx, ty = 7.2, 1.85
        for sy in (-1, 1):
            c = (tx, ty + sy * 0.62)
            self.box(f"cushion_{sy:+d}", (c[0] - 0.27, c[1] - 0.29, F), (c[0] + 0.27, c[1] + 0.29, F + 0.075))
        for names in FURNITURE_GROUPS:
            objs = [bpy.data.objects[n] for n in names if n in bpy.data.objects]
            if not objs:
                print(f"[minka_doors] furniture missing: {names}")
                continue
            mn, mx = world_bounds(objs)
            self.box(names[0].replace("Minka_", "furniture_"), mn, mx)

    def roof(self):
        d = self.mk.roof_dims
        P = build.P
        x0, x1, y0, y1, yc = d["x0"], d["x1"], d["y0"], d["y1"], d["yc"]
        xa, xb, ya, yb = d["xa"], d["xb"], d["ya"], d["yb"]
        ze, zh, zr, th = P["z_eave"], P["z_hip"], P["z_ridge"], P["thatch"]
        quads = {
            "roof_front": [(x0, y0, ze), (x1, y0, ze), (xb, ya, zh), (xa, ya, zh)],
            "roof_back": [(x1, y1, ze), (x0, y1, ze), (xa, yb, zh), (xb, yb, zh)],
            "roof_left": [(x0, y1, ze), (x0, y0, ze), (xa, ya, zh), (xa, yb, zh)],
            "roof_right": [(x1, y0, ze), (x1, y1, ze), (xb, yb, zh), (xb, ya, zh)],
        }
        for label, q in quads.items():
            pts = [Vector(p) for p in q]
            normal = (pts[1] - pts[0]).cross(pts[3] - pts[0]).normalized()
            if normal.z < 0:
                normal = -normal
            # the thatch is solidified downwards (build_roof: offset -1)
            self.hull(label, pts + [p - normal * th for p in pts])
        self.hull("roof_gable_prism", [(xa, ya, zh), (xb, ya, zh), (xa, yb, zh), (xb, yb, zh), (xa, yc, zr), (xb, yc, zr)])
        self.box("roof_ridge", (xa - 0.07, yc - 0.24, zr - 0.2), (xb + 0.07, yc + 0.24, zr + 0.31))

    def build_all(self):
        self.structure()
        self.base()
        self.transoms()
        self.beams()
        self.furniture()
        self.roof()
        return self.items


def panel_records(mk):
    """Panels and doorways in the Unreal house frame."""
    panels, doorways = [], {}
    for o in mk.panels:
        name = o.name
        doorway = name[:-2] if name.endswith(("_1", "_2")) else name
        opens = 1.0 if (name == doorway or name.endswith(PAIR_OPENING_LEAF)) else 0.0
        rot = math.degrees(o.rotation_euler.z)
        closed = (o["closed_x"], o["closed_y"], o.location.z)
        panels.append({"name": name, "mesh": o.data.name, "type": o["panel_type"], "doorway": doorway,
                       "closed": ue(closed), "yaw": round(-rot, 2),
                       "travel": [round(o["travel_x"] * 100.0, 1), round(-o["travel_y"] * 100.0, 1), 0.0],
                       "open_fraction": opens})
        # bay of the doorway: the union of its closed leaves (the board door of the entrance is on an outer track)
        along_y = abs(rot - 90.0) < 1.0
        w = max(v.co.x for v in o.data.vertices)
        h = max(v.co.z for v in o.data.vertices)
        a = closed[1] if along_y else closed[0]
        across = closed[0] if along_y else closed[1]
        if doorway == "Minka_Itado_Entrance":
            across += 0.11
        rec = doorways.setdefault(doorway, {"name": doorway, "type": o["panel_type"], "along_y": along_y,
                                            "a": a, "b": a + w, "across": [], "z": closed[2], "h": h})
        rec["a"], rec["b"] = min(rec["a"], a), max(rec["b"], a + w)
        rec["across"].append(across)
        rec["z"], rec["h"] = min(rec["z"], closed[2]), max(rec["h"], h)
    out = []
    for rec in doorways.values():
        mid = (rec["a"] + rec["b"]) / 2
        across = sum(rec["across"]) / len(rec["across"])
        centre = (across, mid, rec["z"]) if rec["along_y"] else (mid, across, rec["z"])
        out.append({"name": rec["name"], "type": rec["type"], "centre": ue(centre),
                    "yaw": -90.0 if rec["along_y"] else 0.0, "half_width": round((rec["b"] - rec["a"]) * 50.0, 1),
                    "height": round(rec["h"] * 100.0, 1)})
    index = {d["name"]: i for i, d in enumerate(out)}
    for p in panels:
        p["doorway_index"] = index[p["doorway"]]
    return panels, out


def export_panels(mk):
    done = {}
    for o in mk.panels:
        me = o.data
        if me.name in done:
            continue
        xs = [v.co.x for v in me.vertices]
        ys = [v.co.y for v in me.vertices]
        zs = [v.co.z for v in me.vertices]
        ymid = (min(ys) + max(ys)) / 2
        half_t = max((max(ys) - min(ys)) / 2, MIN_PANEL_COLLISION / 2)
        ucx = hull_object(f"UCX_{me.name}_00", box_points((min(xs), ymid - half_t, min(zs)), (max(xs), ymid + half_t, max(zs))))
        tmp = bpy.data.objects.new(me.name, me)
        bpy.context.scene.collection.objects.link(tmp)
        _, tris = minka_rurale.export_group([tmp], me.name, extra=[ucx], out_dir=OUT)
        bpy.data.objects.remove(tmp)
        bpy.data.objects.remove(ucx)
        done[me.name] = {"tris": tris, "materials": [m.name for m in me.materials if m],
                         "size_cm": [round((max(xs) - min(xs)) * 100, 1), round((max(ys) - min(ys)) * 100, 1),
                                     round((max(zs) - min(zs)) * 100, 1)]}
    return done


def export_shell(mk):
    # imported models were moved and turned after their creation: world matrices first
    bpy.context.view_layer.update()
    shell = []
    for c in ("Structure", "Toiture", "Murs", "Planchers", "Engawa", "Shoji", "Fusuma", "Tatamis", "Mobilier"):
        shell += [o for o in bpy.data.collections[f"Minka_{c}"].all_objects if o.type == "MESH" and "panel_type" not in o]
    bm = bmesh.new()
    uvl = geo.uv_layer(bm)
    for x0, y0, x1, y1 in minka_rurale.HOUSE_GROUND:
        geo.add_box(bm, (x0 + 0.02, y0 + 0.02, -PLINTH_DEPTH), (x1 - 0.02, y1 - 0.02, -0.005), uvl, 2.0)
    plinth = geo.obj("Minka_Plinth", bm, mk.M["Earth"], bpy.context.scene.collection)
    convexes = ShellCollision(mk).build_all()
    ucx = [hull_object(f"UCX_{SHELL}_{i:03d}", pts) for i, (_, pts) in enumerate(convexes)]
    materials = sorted({m.name for o in shell + [plinth] for m in o.data.materials if m})
    _, tris = minka_rurale.export_group(shell + [plinth], SHELL, extra=ucx, out_dir=OUT)
    for o in ucx + [plinth]:
        bpy.data.objects.remove(o)
    return {"tris": tris, "ucx": len(convexes), "convexes": [label for label, _ in convexes], "materials": materials}


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    root = geo.collection("Minka_Rurale")
    M = geo.materials()
    mk = build.Minka(root, M).build(minka_rurale.import_model)
    OUT.mkdir(parents=True, exist_ok=True)
    minka_rurale.EXPORTED.clear()
    shell = export_shell(mk)
    meshes = export_panels(mk)
    panels, doorways = panel_records(mk)
    report = {
        "task": "T-186",
        "frame": "Unreal cm, house frame of SM_Minka / SM_Minka_Shell: x = Blender x, y = -Blender y, z; yaw degrees",
        "shell": dict(shell, mesh=SHELL),
        "panel_meshes": meshes,
        "panels": panels,
        "doorways": doorways,
        "slots": {"relic": [{"location": ue(RELIC_SLOT[0]), "yaw": RELIC_SLOT[1], "room": "tokonoma"}],
                  "items": [{"location": ue(p), "yaw": yaw, "room": room} for room, p, yaw in ITEM_SLOTS]},
    }
    (OUT / "minka_doors.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    print(f"[minka_doors] {SHELL}: {shell['tris']} tris, {shell['ucx']} UCX; {len(meshes)} panel meshes, "
          f"{len(panels)} panels, {len(doorways)} doorways -> {OUT}")


if __name__ == "__main__":
    main()
