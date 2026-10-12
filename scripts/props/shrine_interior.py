"""Village shrine you can walk into (T-189, owner request 2026-10-06 23:05: "modélise l'intérieur du village shrine
et mets-y un artefact légendaire").

SM_Village_Shrine (village/props.py) is a closed block: a hall of 2.2 x 1.95 m on a stone base, its doors painted on
its front. Same footprint, same frame (front towards -Y, pivot on the ground), same materials; this one is hollow:
- stone base and three stone steps (15 cm each), three wooden steps up to the raised floor of the hall (14 cm each);
- hall: board floor, plaster walls on a timber frame, a ceiling at 1.4 m above the floor (beans are 1.05 m), gable
  boards under the thatch, a door 1.1 x 1.28 m in the front;
- inside: an altar at the back where the legendary artifact stands (a slot in shrine_doors.json), a bronze mirror on
  its stand behind it, two candle stands with glowing paper shades, sake flasks, a gohei; outside: a shimenawa rope
  with four shide over the door and an offering box beside the steps;
- two hinged leaves with a lattice (koshi tobira), exported apart (SM_Village_ShrineDoor), opening inwards;
- UCX: every shape, the roof as two slabs and the gables as thin prisms (the hall stays empty for the players).

Run (background):
  blender --background --factory-startup --python scripts/props/shrine_interior.py
Outputs: art/export/Props/Village/ShrineDoors/ SM_Village_Shrine_Shell.fbx, SM_Village_ShrineDoor.fbx,
         shrine_doors.json (Unreal cm, house frame: x, -y, z; yaw = -Blender yaw), shrine_preview.png
"""
import json
import math
import sys
from pathlib import Path

import bmesh
import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import minka_rurale  # noqa: E402
from minka import geo  # noqa: E402
from minka.geo import add_box, add_box_m, add_cyl, add_revolve, uv_layer  # noqa: E402
from village import props as vprops  # noqa: E402

ROOT = HERE.parents[1]
OUT = ROOT / "art" / "export" / "Props" / "Village" / "ShrineDoors"
SHELL = "SM_Village_Shrine_Shell"
DOOR = "SM_Village_ShrineDoor"

FLOOR = 1.02                    # top of the boards of the hall
CEIL = 2.42                     # underside of the ceiling
WALL_T = 0.08
HX, HY0, HY1 = 1.1, -0.99, 1.0  # outer faces of the walls: x = +-HX, front y = HY0, back y = HY1
DOOR_HW, DOOR_TOP = 0.55, FLOOR + 1.28
LEAF_T = 0.04
OPEN_DEG = 95.0                 # leaves turn inwards (Blender +Y)
ALTAR = (-0.5, 0.40, 0.5, 0.90, FLOOR + 0.43)   # x0, y0, x1, y1, top
RELIC = ((0.0, 0.63, ALTAR[4]), 0.0)          # on the altar, its long side along the altar


def ue(p):
    return [round(p[0] * 100.0, 1), round(-p[1] * 100.0, 1), round(p[2] * 100.0, 1)]


def box_points(mn, mx):
    return [(x, y, z) for x in (mn[0], mx[0]) for y in (mn[1], mx[1]) for z in (mn[2], mx[2])]


def hull_object(name, points):
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
    return ob


def add_prism(bm, pts, depth, uvl=None):
    """Thin prism from a convex polygon in an XZ plane at y = pts[0].y, extruded by depth along +Y."""
    verts = [bm.verts.new(Vector(p)) for p in pts]
    back = [bm.verts.new(Vector(p) + Vector((0.0, depth, 0.0))) for p in pts]
    faces = [bm.faces.new(verts[::-1]), bm.faces.new(back)]
    n = len(pts)
    for i in range(n):
        faces.append(bm.faces.new((verts[i], verts[(i + 1) % n], back[(i + 1) % n], back[i])))
    bmesh.ops.recalc_face_normals(bm, faces=faces)
    if uvl is not None:
        geo.box_uv(faces, uvl, 1.0)
    return faces


class Shrine:
    def __init__(self, M):
        self.M = M
        self.coll = bpy.data.collections.new("Shrine_Interior")
        bpy.context.scene.collection.children.link(self.coll)
        self.objects = []
        self.ucx = []           # (label, points)

    def part(self, name, mat, build, smooth=False):
        bm = bmesh.new()
        uvl = uv_layer(bm)
        build(bm, uvl)
        ob = geo.obj(f"Shrine_{name}", bm, mat, self.coll, smooth=smooth)
        self.objects.append(ob)
        return ob

    def box(self, label, mn, mx):
        self.ucx.append((label, box_points(mn, mx)))

    def build(self):
        M = self.M
        get = lambda key, fallback="Timber": M.get(key) or M[fallback]

        # ---- base, stone steps, wooden steps
        def base(bm, uvl):
            add_box(bm, (-1.8, -2.1, -0.6), (1.8, 1.8, 0.45), uvl, 0.8)
            for i in range(3):
                add_box(bm, (-0.8, -2.1 - 0.35 * (i + 1), -0.6), (0.8, -2.1 - 0.35 * i, 0.45 - 0.15 * (i + 1)), uvl, 0.8)
        self.part("Base", M["Stone"], base)
        self.box("base", (-1.8, -2.1, -0.6), (1.8, 1.8, 0.45))
        for i in range(3):
            self.box(f"stone_step_{i}", (-0.8, -2.1 - 0.35 * (i + 1), -0.6), (0.8, -2.1 - 0.35 * i, 0.45 - 0.15 * (i + 1)))

        rise = (FLOOR - 0.45) / 4.0

        def steps(bm, uvl):
            for k in range(1, 4):
                y1 = -1.15 - (3 - k) * 0.24
                add_box(bm, (-0.55, y1 - 0.24, 0.45), (0.55, y1, 0.45 + k * rise), uvl, 1.0)
            for sx in (-1, 1):            # stringers
                add_box(bm, (sx * 0.58 - 0.03, -1.87, 0.45), (sx * 0.58 + 0.03, -1.15, FLOOR - 0.04), uvl, 1.0)
        self.part("Steps", get("Cedar"), steps)
        for k in range(1, 4):
            y1 = -1.15 - (3 - k) * 0.24
            self.box(f"wood_step_{k}", (-0.61, y1 - 0.24, 0.45), (0.61, y1, 0.45 + k * rise))

        # ---- raised floor on its skirt, posts, frame
        def floor(bm, uvl):
            add_box(bm, (-1.25, -1.15, FLOOR - 0.07), (1.25, 1.15, FLOOR), uvl, 1.2)
        self.part("Floor", get("FloorWood", "Cedar"), floor)

        def skirt(bm, uvl):
            for (mn, mx) in (((-1.2, -1.12, 0.45), (1.2, -1.08, FLOOR - 0.07)), ((-1.2, 1.08, 0.45), (1.2, 1.12, FLOOR - 0.07)),
                             ((-1.22, -1.1, 0.45), (-1.18, 1.1, FLOOR - 0.07)), ((1.18, -1.1, 0.45), (1.22, 1.1, FLOOR - 0.07))):
                add_box(bm, mn, mx, uvl, 1.0, swap=True)
        self.part("Skirt", M["Timber"], skirt)
        self.box("platform", (-1.25, -1.15, 0.45), (1.25, 1.15, FLOOR))

        def posts(bm, uvl):
            for sx in (-1, 1):
                for sy in (-1, 1):
                    add_box(bm, (sx * HX - 0.07, sy * 1.0 - 0.07, 0.45), (sx * HX + 0.07, sy * 1.0 + 0.07, 2.6), uvl, 1.0,
                            swap=True)
            add_box(bm, (-HX - 0.07, HY0 - 0.05, 2.45), (HX + 0.07, HY1 + 0.05, 2.6), uvl, 1.0)     # wall plates ring
        self.part("Posts", M["Timber"], posts)
        for sx in (-1, 1):
            for sy in (-1, 1):
                self.box(f"post_{sx:+d}{sy:+d}", (sx * HX - 0.07, sy * 1.0 - 0.07, 0.45), (sx * HX + 0.07, sy * 1.0 + 0.07, 2.6))

        # ---- walls: plaster between the posts, the front opened for the door
        walls = [
            ("left", (-HX, HY0, FLOOR), (-HX + WALL_T, HY1, 2.5)),
            ("right", (HX - WALL_T, HY0, FLOOR), (HX, HY1, 2.5)),
            ("back", (-HX, HY1 - WALL_T, FLOOR), (HX, HY1, 2.5)),
            ("front_left", (-HX, HY0, FLOOR), (-DOOR_HW, HY0 + WALL_T, 2.5)),
            ("front_right", (DOOR_HW, HY0, FLOOR), (HX, HY0 + WALL_T, 2.5)),
            ("lintel", (-DOOR_HW, HY0, DOOR_TOP), (DOOR_HW, HY0 + WALL_T, 2.5)),
        ]

        def plaster(bm, uvl):
            for _, mn, mx in walls:
                add_box(bm, mn, mx, uvl, 1.2)
        self.part("Walls", get("Plaster"), plaster)
        for label, mn, mx in walls:
            self.box(f"wall_{label}", mn, mx)

        def frame(bm, uvl):
            # door jambs and lintel trim, a sill, horizontal nuki on the outside of the walls
            for sx in (-1, 1):
                add_box(bm, (sx * DOOR_HW - 0.04, HY0 - 0.02, FLOOR), (sx * DOOR_HW + 0.04, HY0 + WALL_T + 0.02, DOOR_TOP + 0.06), uvl, 1.0,
                        swap=True)
            add_box(bm, (-DOOR_HW - 0.04, HY0 - 0.02, DOOR_TOP), (DOOR_HW + 0.04, HY0 + WALL_T + 0.02, DOOR_TOP + 0.08), uvl, 1.0)
            add_box(bm, (-DOOR_HW, HY0 - 0.02, FLOOR), (DOOR_HW, HY0 + WALL_T + 0.02, FLOOR + 0.025), uvl, 1.0)
            for z in (FLOOR + 0.45, 2.05):
                add_box(bm, (-HX - 0.01, HY0 - 0.015, z), (-DOOR_HW - 0.04, HY0, z + 0.07), uvl, 1.0)
                add_box(bm, (DOOR_HW + 0.04, HY0 - 0.015, z), (HX + 0.01, HY0, z + 0.07), uvl, 1.0)
                for sx in (-1, 1):
                    add_box(bm, (sx * HX - (0.015 if sx > 0 else -0.015), HY0, z), (sx * HX + (0.015 if sx > 0 else -0.015), HY1, z + 0.07),
                            uvl, 1.0, swap=True)
                add_box(bm, (-HX, HY1, z), (HX, HY1 + 0.015, z + 0.07), uvl, 1.0)
        self.part("Frame", M["Timber"], frame)

        def ceiling(bm, uvl):
            add_box(bm, (-HX + WALL_T, HY0 + WALL_T, CEIL), (HX - WALL_T, HY1 - WALL_T, CEIL + 0.03), uvl, 1.2, swap=True)
            for x in (-0.6, 0.0, 0.6):     # ceiling joists
                add_box(bm, (x - 0.03, HY0 + WALL_T, CEIL - 0.05), (x + 0.03, HY1 - WALL_T, CEIL), uvl, 1.0, swap=True)
        self.part("Ceiling", get("Cedar"), ceiling)
        self.box("ceiling", (-HX, HY0, CEIL - 0.05), (HX, HY1, 2.5))

        # ---- roof (as the old shrine): two thatch slabs, ridge, gable boards
        slabs = []

        def roof(bm, uvl):
            for sx in (-1, 1):
                Mx = Matrix.Translation((sx * 0.85, 0, 2.95)) @ Matrix.Rotation(sx * math.radians(30), 4, "Y")
                add_box_m(bm, Mx, (2.0, 3.2, 0.1), uvl, 1.0)
                slabs.append([Mx @ Vector((x * 1.0, y * 1.6, z * 0.05)) for x in (-1, 1) for y in (-1, 1) for z in (-1, 1)])
        self.part("Roof", M["Thatch"], roof)
        for i, pts in enumerate(slabs):
            self.ucx.append((f"roof_{i}", [tuple(p) for p in pts]))

        def ridge(bm, uvl):
            add_box(bm, (-0.08, -1.7, 3.35), (0.08, 1.7, 3.5), uvl, 1.0)
            for y in (-1.2, -0.4, 0.4, 1.2):                 # katsuogi logs across the ridge
                add_cyl(bm, (-0.22, y, 3.56), (0.22, y, 3.56), 0.06, segs=10, uvl=uvl, tile=0.6)
        self.part("Ridge", M["Timber"], ridge)
        self.box("ridge", (-0.24, -1.7, 3.35), (0.24, 1.7, 3.62))

        def gables(bm, uvl):
            for y, d in ((HY0 - 0.02, 0.03), (HY1 - 0.01, 0.03)):
                add_prism(bm, [(-HX, y, 2.5), (HX, y, 2.5), (0.0, y, 3.36)], d, uvl)
        self.part("Gables", M["Timber"], gables)
        for label, y in (("front", HY0 - 0.02), ("back", HY1 - 0.01)):
            pts = [(-HX, y, 2.5), (HX, y, 2.5), (0.0, y, 3.36)]
            self.ucx.append((f"gable_{label}", pts + [(p[0], p[1] + 0.03, p[2]) for p in pts]))

        # ---- inside: altar, mirror, candles, sake flasks, gohei
        x0, y0, x1, y1, top = ALTAR

        def altar(bm, uvl):
            add_box(bm, (x0, y0, top - 0.05), (x1, y1, top), uvl, 1.0)                  # top board
            for sx in (x0 + 0.05, x1 - 0.05):
                for sy in (y0 + 0.05, y1 - 0.05):
                    add_box(bm, (sx - 0.03, sy - 0.03, FLOOR), (sx + 0.03, sy + 0.03, top - 0.05), uvl, 1.0, swap=True)
            add_box(bm, (x0 + 0.02, y0 + 0.02, FLOOR + 0.12), (x1 - 0.02, y1 - 0.02, FLOOR + 0.15), uvl, 1.0)   # low shelf
        self.part("Altar", get("Lacquer", "Cedar"), altar)
        self.box("altar", (x0, y0, FLOOR), (x1, y1, top))

        def mirror_stand(bm, uvl):
            add_box(bm, (-0.16, 0.80, top), (0.16, 0.88, top + 0.05), uvl, 1.0)
            for sx in (-1, 1):
                add_box(bm, (sx * 0.12 - 0.015, 0.83, top), (sx * 0.12 + 0.015, 0.86, top + 0.42), uvl, 1.0, swap=True)
        self.part("MirrorStand", M["Timber"], mirror_stand)

        def mirror(bm, uvl):
            add_cyl(bm, (0.0, 0.845, top + 0.30), (0.0, 0.83, top + 0.30), 0.13, segs=28)
        self.part("Mirror", get("Bronze", "Iron"), mirror, smooth=True)
        self.box("mirror", (-0.16, 0.80, top), (0.16, 0.88, top + 0.45))

        def candles(bm, uvl):
            for sx in (-0.38, 0.38):
                add_cyl(bm, (sx, 0.55, top), (sx, 0.55, top + 0.04), 0.06, segs=12)
                add_cyl(bm, (sx, 0.55, top + 0.04), (sx, 0.55, top + 0.26), 0.015, segs=8)
        self.part("CandleStands", M["Timber"], candles)

        def shades(bm, uvl):
            for sx in (-0.38, 0.38):
                add_cyl(bm, (sx, 0.55, top + 0.26), (sx, 0.55, top + 0.42), 0.07, 0.06, segs=12)
        self.part("CandleShades", get("Lantern", "Washi"), shades)

        def flasks(bm, uvl):
            for sx in (-0.25, 0.25):
                prof = [(0.0, 0.0), (0.045, 0.0), (0.05, 0.08), (0.03, 0.14), (0.015, 0.17), (0.015, 0.2), (0.0, 0.2)]
                add_revolve(bm, prof, (sx, 0.82, top), segs=16)
        self.part("SakeFlasks", get("Ceramic", "Plaster"), flasks, smooth=True)

        def gohei(bm, uvl):
            add_cyl(bm, (0.42, 0.85, top), (0.42, 0.85, top + 0.5), 0.008, segs=6)
            for k in range(4):
                z = top + 0.48 - k * 0.07
                add_box(bm, (0.40 - 0.03 * (k % 2), 0.845, z - 0.06), (0.44 + 0.03 * (k % 2), 0.855, z), uvl, 1.0)
        self.part("Gohei", get("Washi"), gohei)

        # ---- outside: shimenawa and shide over the door, offering box beside the steps
        def shimenawa(bm, uvl):
            n = 12
            for i in range(n):
                t0, t1 = i / n, (i + 1) / n
                xa, xb = -0.75 + 1.5 * t0, -0.75 + 1.5 * t1
                za = DOOR_TOP + 0.2 - 0.12 * math.sin(math.pi * t0)
                zb = DOOR_TOP + 0.2 - 0.12 * math.sin(math.pi * t1)
                add_cyl(bm, (xa, HY0 - 0.06, za), (xb, HY0 - 0.06, zb), 0.035 + 0.01 * math.sin(math.pi * t0), segs=10)
        self.part("Shimenawa", M["Rope"], shimenawa, smooth=True)

        def shide(bm, uvl):
            for x in (-0.45, -0.15, 0.15, 0.45):
                ztop = DOOR_TOP + 0.2 - 0.12 * math.sin(math.pi * (x + 0.75) / 1.5) - 0.03
                for k in range(4):         # zigzag paper
                    off = 0.03 if k % 2 else -0.03
                    add_box(bm, (x + off - 0.025, HY0 - 0.065, ztop - 0.07 * (k + 1)), (x + off + 0.025, HY0 - 0.06, ztop - 0.07 * k),
                            uvl, 1.0)
        self.part("Shide", get("Washi"), shide)

        def offering_box(bm, uvl):
            add_box(bm, (0.9, -1.75, 0.45), (1.5, -1.35, 0.85), uvl, 1.0)
            for k in range(6):           # slatted top
                x = 0.92 + k * 0.1
                add_box(bm, (x, -1.75, 0.85), (x + 0.05, -1.35, 0.88), uvl, 1.0, swap=True)
        self.part("OfferingBox", get("Cedar"), offering_box)
        self.box("offering_box", (0.9, -1.75, 0.45), (1.5, -1.35, 0.88))
        return self

    def door_mesh(self):
        """One leaf, pivot on its hinge at the bottom: x from 0 (hinge) to the middle of the door, y the thickness,
        a timber frame with a vertical lattice and a solid lower panel."""
        M = self.M
        w, h, t = DOOR_HW, DOOR_TOP - FLOOR, LEAF_T
        bm = bmesh.new()
        uvl = uv_layer(bm)
        s = 0.05
        add_box(bm, (0, 0, 0), (s, t, h), uvl, 1.0, swap=True)
        add_box(bm, (w - s, 0, 0), (w, t, h), uvl, 1.0, swap=True)
        add_box(bm, (s, 0, h - s), (w - s, t, h), uvl, 1.0)
        add_box(bm, (s, 0, 0), (w - s, t, 0.42), uvl, 1.0)                  # lower panel
        add_box(bm, (s, 0, 0.42), (w - s, t, 0.47), uvl, 1.0)
        for i in range(1, 7):                                                 # koshi lattice
            x = s + (w - 2 * s) * i / 7
            add_box(bm, (x - 0.009, 0.006, 0.47), (x + 0.009, t - 0.006, h - s), uvl, 1.0, swap=True)
        for z in (0.82, 1.06):
            add_box(bm, (s, 0.008, z - 0.01), (w - s, t - 0.008, z + 0.01), uvl, 1.0)
        add_cyl(bm, (w - 0.09, -0.01, 0.62), (w - 0.09, 0.0, 0.62), 0.025, segs=10)     # ring pull
        me = bpy.data.meshes.new(DOOR)
        bm.normal_update()
        bm.to_mesh(me)
        bm.free()
        me.materials.append(M["Timber"])
        return me


def main():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    bpy.context.scene.unit_settings.system = "METRIC"
    M = geo.materials()
    vprops.village_materials(M)
    shrine = Shrine(M).build()
    OUT.mkdir(parents=True, exist_ok=True)
    minka_rurale.EXPORTED.clear()

    ucx = [hull_object(f"UCX_{SHELL}_{i:03d}", pts) for i, (_, pts) in enumerate(shrine.ucx)]
    _, tris = minka_rurale.export_group(shrine.objects, SHELL, extra=ucx, out_dir=OUT)
    for o in ucx:
        bpy.data.objects.remove(o)
    shell_materials = sorted({m.name for o in shrine.objects for m in o.data.materials if m})

    me = shrine.door_mesh()
    h = DOOR_TOP - FLOOR
    door_ucx = hull_object(f"UCX_{DOOR}_00", box_points((0.0, LEAF_T / 2 - 0.03, 0.0), (DOOR_HW, LEAF_T / 2 + 0.03, h)))
    leaf = bpy.data.objects.new(DOOR, me)
    bpy.context.scene.collection.objects.link(leaf)
    _, door_tris = minka_rurale.export_group([leaf], DOOR, extra=[door_ucx], out_dir=OUT)
    bpy.data.objects.remove(leaf)
    bpy.data.objects.remove(door_ucx)

    # leaves: left hinge at -DOOR_HW (leaf towards +x), right hinge at +DOOR_HW (leaf turned 180, towards -x),
    # both in the wall plane; inwards is Blender +Y, so the left leaf turns +OPEN_DEG, the right one -OPEN_DEG
    y_wall = HY0 + WALL_T / 2 - LEAF_T / 2
    leaves = [((-DOOR_HW, y_wall, FLOOR), 0.0, OPEN_DEG), ((DOOR_HW, y_wall + LEAF_T, FLOOR), 180.0, -OPEN_DEG)]
    panels = [{"name": f"Shrine_Door_{i}", "mesh": DOOR, "type": "tobira", "doorway": "Shrine_Door", "doorway_index": 0,
               "closed": ue(p), "yaw": round(-yaw, 2), "open_yaw": round(-turn, 2), "travel": [0.0, 0.0, 0.0],
               "open_fraction": 1.0} for i, (p, yaw, turn) in enumerate(leaves)]
    doorway = {"name": "Shrine_Door", "type": "tobira", "centre": ue((0.0, HY0 + WALL_T / 2, FLOOR)), "yaw": 0.0,
               "half_width": round(DOOR_HW * 100, 1), "height": round(h * 100, 1)}
    report = {
        "task": "T-189",
        "frame": "Unreal cm, frame of SM_Village_Shrine: x = Blender x, y = -Blender y, z; yaw degrees",
        "shell": {"mesh": SHELL, "tris": tris, "ucx": len(shrine.ucx), "convexes": [l for l, _ in shrine.ucx],
                  "materials": shell_materials},
        "panel_meshes": {DOOR: {"tris": door_tris, "materials": [m.name for m in me.materials if m],
                                "size_cm": [round(DOOR_HW * 100, 1), round(LEAF_T * 100, 1), round(h * 100, 1)]}},
        "panels": panels,
        "doorways": [doorway],
        "slots": {"relic": [{"location": ue(RELIC[0]), "yaw": RELIC[1], "room": "altar"}], "items": []},
    }
    (OUT / "shrine_doors.json").write_text(json.dumps(report, indent=1), encoding="utf-8")
    print(f"[shrine] {SHELL}: {tris} tris, {len(shrine.ucx)} UCX; {DOOR}: {door_tris} tris -> {OUT}")
    preview(shrine, me, leaves)


def preview(shrine, me, leaves):
    """Cutaway from the front-left, the roof and the right wall hidden, the leaves open: shrine_preview.png"""
    scene = bpy.context.scene
    for o in shrine.objects:
        o.hide_render = o.name in ("Shrine_Roof", "Shrine_Ridge", "Shrine_Ceiling", "Shrine_Gables")
    for i, (p, yaw, turn) in enumerate(leaves):
        ob = bpy.data.objects.new(f"Leaf_{i}", me)
        scene.collection.objects.link(ob)
        ob.location = p
        ob.rotation_euler = (0.0, 0.0, math.radians(yaw + turn))
    cam = minka_rurale.camera("CAM_Shrine", (-3.4, -4.6, 3.6), (0.0, 0.2, 1.3), 34)
    scene.camera = cam
    minka_rurale.studio(scene)
    minka_rurale.render(scene, cam, OUT / "shrine_preview.png", (1400, 1000))


if __name__ == "__main__":
    main()
