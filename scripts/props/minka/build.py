"""The minka itself, following the owner's brief (docs/art/briefs/minka-rurale.md).

Frame: meters, facade towards -Y, right of the facade +X, origin at the front-left corner of the main body at
exterior ground level. Main body 9.00 x 7.20 m on a 1.80 m grid; doma (packed earth) at z = 0, raised floor and
engawa at z = 0.45; openings 2.10 m above their floor; L-shaped engawa 1.20 m wide (front of the raised rooms and
right side); irimoya-style thatched roof (hipped skirt + gable top), separate so it can be hidden.
Rooms: A doma X 0-1.8 (entrance south, kitchen north) - B board room with irori - C front tatami room (8 mats)
- D rear board room (chest, futon) - E rear tatami room (6 mats) - F tokonoma alcove + closed cupboard.
Every sliding panel is its own object, origin at its bottom-left corner on its track, with custom properties
"open" (0..1, drives its location through drivers), "travel_x/y" and its closed position.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

from . import geo
from .geo import TAU, add_box, add_box_m, add_cyl, add_revolve, add_rock, box_uv, slope_uv, uv_layer

P = dict(
    W=9.0, D=7.2, grid=1.8,
    floor=0.45, opening=2.10, eng_w=1.20,
    post=0.20, post_big=0.27, post_eng=0.15, wall=0.09,
    footing=0.12, sill=(0.31, 0.42), keta=(2.85, 3.03),
    eave_out=0.75, z_eave=2.35, z_hip=4.45, z_ridge=6.75, thatch=0.45,
    shoji_t=0.032, fusuma_t=0.025,
)
XS = [0.0, 1.8, 3.6, 5.4, 7.2, 9.0]
YS = [0.0, 1.8, 3.6, 5.4, 7.2]


class Minka:
    def __init__(self, root, M):
        self.M = M
        self.root = root
        self.c = {k: geo.collection(f"Minka_{k}", root) for k in
                  ("Structure", "Toiture", "Murs", "Planchers", "Engawa", "Shoji", "Fusuma", "Tatamis", "Mobilier",
                   "Collisions", "Presentation")}
        self.rnd = random.Random(7)
        self.panels = []
        self.ucx = []
        self.kamoi = P["floor"] + P["opening"]

    # ------------------------------------------------------------ helpers
    def piece(self, name, coll, mat, build, smooth=False, bevel=0.0, origin=None):
        bm = bmesh.new()
        uvl = uv_layer(bm)
        build(bm, uvl)
        return geo.obj(f"Minka_{name}", bm, mat, self.c[coll], smooth=smooth, bevel=bevel, origin=origin)

    def collide(self, mn, mx):
        self.ucx.append((Vector(mn), Vector(mx)))

    # ------------------------------------------------------------ openings plan
    def walls_plan(self):
        """Wall lines: (axis, fixed coord, start, end, base z, kind per bay). kind: wall / shoji / fusuma / door /
        step / open / window. Bays are the 1.80 m grid spans (or the 0.90 m ones of the tokonoma line)."""
        F = P["floor"]
        return [
            # facade (y = 0): doma door bay, then shoji onto the engawa
            ("x", 0.0, [(0.0, 1.8, "door"), (1.8, 3.6, "shoji"), (3.6, 5.4, "shoji"), (5.4, 7.2, "shoji"),
                        (7.2, 9.0, "shoji")]),
            # rear (y = 7.2)
            ("x", 7.2, [(0.0, 1.8, "window"), (1.8, 3.6, "wall"), (3.6, 5.4, "wall"), (5.4, 7.2, "wall"),
                        (7.2, 9.0, "wall")]),
            # left (x = 0): doma side
            ("y", 0.0, [(0.0, 1.8, "wall"), (1.8, 3.6, "window"), (3.6, 5.4, "wall"), (5.4, 7.2, "wall")]),
            # right (x = 9): shoji onto the side engawa, closed at the back
            ("y", 9.0, [(0.0, 1.8, "shoji"), (1.8, 3.6, "shoji"), (3.6, 5.4, "shoji"), (5.4, 7.2, "wall")]),
            # doma / raised floor (x = 1.8)
            ("y", 1.8, [(0.0, 1.8, "wall"), (1.8, 3.6, "step"), (3.6, 5.4, "wall"), (5.4, 7.2, "wall")]),
            # B|C and D|E (x = 5.4)
            ("y", 5.4, [(0.0, 1.8, "wall"), (1.8, 3.6, "fusuma"), (3.6, 5.4, "fusuma"), (5.4, 7.2, "wall")]),
            # B|D and C|E (y = 3.6)
            ("x", 3.6, [(1.8, 3.6, "wall"), (3.6, 5.4, "fusuma"), (5.4, 7.2, "fusuma"), (7.2, 9.0, "wall")]),
            # E | F (y = 6.3): tokonoma open, cupboard closed by fusuma
            ("x", 6.3, [(5.4, 7.2, "open"), (7.2, 9.0, "fusuma")]),
        ]

    def base_z(self, axis, fixed, a, b):
        """Floor level on a wall bay: 0 in the doma, the raised floor elsewhere."""
        mid = (a + b) / 2
        x = fixed if axis == "y" else mid
        if axis == "y" and fixed <= 1.8 + 1e-6:
            return 0.0 if fixed < 1.8 - 1e-6 else P["floor"]
        return 0.0 if x < 1.8 - 1e-6 else P["floor"]

    # ------------------------------------------------------------ structure
    def build_structure(self):
        M = self.M
        F = P["floor"]
        posts, footings, short = [], [], []
        pts = set()
        for x in XS:
            for y in YS:
                on_perim = x in (0.0, 9.0) or y in (0.0, 7.2)
                on_line = (x in (1.8, 5.4)) or (y == 3.6 and x >= 1.8)
                if on_perim or on_line:
                    pts.add((x, y))
        pts |= {(5.4, 6.3), (7.2, 6.3), (9.0, 6.3)}
        self.post_pts = sorted(pts)

        def build_posts(bm, uvl):
            for x, y in self.post_pts:
                s = P["post_big"] if (x, y) == (1.8, 3.6) else P["post"]
                z0 = 0.0 if x < 1.8 - 1e-6 else P["footing"]
                if (x, y) == (7.2, 6.3):  # tokobashira: a natural, slightly irregular log
                    add_cyl(bm, (x, y, P["floor"] + 0.1), (x, y, P["keta"][0]), 0.075, 0.07, segs=10, uvl=uvl,
                            tile=1.2, wobble=0.01, seed=3)
                    continue
                add_box(bm, (x - s / 2, y - s / 2, z0), (x + s / 2, y + s / 2, P["keta"][1]), uvl, 1.2, swap=True)
                self.collide((x - s / 2, y - s / 2, z0), (x + s / 2, y + s / 2, P["keta"][0]))
            # engawa posts
            for x, y in self.engawa_posts():
                s = P["post_eng"]
                add_box(bm, (x - s / 2, y - s / 2, P["footing"]), (x + s / 2, y + s / 2, P["z_eave"] + 0.05), uvl, 1.2,
                        swap=True)
                self.collide((x - s / 2, y - s / 2, 0.0), (x + s / 2, y + s / 2, P["z_eave"]))
        self.piece("Posts", "Structure", M["Timber"], build_posts, bevel=0.012)

        def build_footings(bm, uvl):
            k = 0
            for x, y in list(self.post_pts) + self.engawa_posts():
                if x < 1.8 - 1e-6:
                    continue
                add_rock(bm, (x, y, 0.02), (0.24, 0.22, 0.13), seed=100 + k, uvl=uvl, flat_top=P["footing"])
                k += 1
            # short posts under the floor, every 0.9 m along the room lines, on small stones
            for x in (2.7, 4.5, 6.3, 8.1):
                for y in (0.9, 2.7, 4.5, 6.3):
                    add_rock(bm, (x, y, 0.01), (0.14, 0.13, 0.08), seed=500 + int(x * 10 + y), uvl=uvl, flat_top=0.07)
            for y in (0.9, 2.7, 4.5, 6.3):
                for x in (1.8, 5.4, 9.0):
                    add_rock(bm, (x, y, 0.01), (0.15, 0.14, 0.08), seed=700 + int(x * 10 + y), uvl=uvl, flat_top=0.07)
        self.piece("Footings", "Structure", M["Stone"], build_footings)

        def build_beams(bm, uvl):
            s0, s1 = P["sill"]
            k0, k1 = P["keta"]
            w = 0.16
            # sills under every raised wall line and the floor edge on the doma side
            for (a, b, y) in ((1.8, 9.0, 0.0), (1.8, 9.0, 3.6), (1.8, 9.0, 7.2)):
                add_box(bm, (a - 0.08, y - w / 2, s0), (b + 0.08, y + w / 2, s1), uvl, 1.2)
            for x in (1.8, 5.4, 9.0):
                add_box(bm, (x - w / 2, -0.08, s0), (x + w / 2, 7.28, s1), uvl, 1.2, swap=True)
            # short posts under the floor joists
            for x in (2.7, 4.5, 6.3, 8.1):
                for y in (0.9, 2.7, 4.5, 6.3):
                    add_box(bm, (x - 0.05, y - 0.05, 0.07), (x + 0.05, y + 0.05, s0), uvl, 1.2, swap=True)
            for y in (0.9, 2.7, 4.5, 6.3):
                for x in (1.8, 5.4, 9.0):
                    add_box(bm, (x - 0.05, y - 0.05, 0.07), (x + 0.05, y + 0.05, s0), uvl, 1.2, swap=True)
            # floor joists (every 0.45 m along Y under the board rooms, along X under tatami rooms)
            for y in [0.45 * i for i in range(1, 16)]:
                add_box(bm, (1.8, y - 0.04, s1 - 0.08), (9.0, y + 0.04, s1), uvl, 1.2)
            # wall plates (keta) on every wall line
            for y in (0.0, 3.6, 7.2):
                add_box(bm, (-0.15, y - 0.09, k0), (9.15, y + 0.09, k1), uvl, 1.2)
            for x in (0.0, 1.8, 5.4, 9.0):
                add_box(bm, (x - 0.09, -0.15, k0), (x + 0.09, 7.35, k1), uvl, 1.2, swap=True)
            add_box(bm, (5.4, 6.3 - 0.07, k0), (9.0, 6.3 + 0.07, k0 + 0.14), uvl, 1.2)
        self.piece("Beams", "Structure", M["Timber"], build_beams, bevel=0.01)

        def build_logs(bm, uvl):  # rustic tie beams over the open doma
            for y in (1.8, 5.4):
                add_cyl(bm, (-0.2, y, 3.18), (2.0, y + 0.05, 3.22), 0.13, 0.11, segs=12, uvl=uvl, tile=1.4, wobble=0.04,
                        seed=int(y * 10))
            add_cyl(bm, (0.9, -0.2, 3.42), (0.92, 7.4, 3.40), 0.15, 0.13, segs=12, uvl=uvl, tile=1.4, wobble=0.06, seed=9)
        self.piece("TieLogs", "Structure", M["Timber"], build_logs, smooth=True)

    def engawa_posts(self):
        e = P["eng_w"]
        pts = [(x, -e) for x in (1.8, 3.6, 5.4, 7.2, 9.0)] + [(9.0 + e, -e)]
        pts += [(9.0 + e, y) for y in (1.8, 3.6, 5.4, 7.2)]
        return pts

    # ------------------------------------------------------------ floors
    def build_floors(self):
        M = self.M
        F = P["floor"]

        def doma(bm, uvl):
            add_box(bm, (0.0, 0.0, -0.12), (1.8, 7.2, 0.0), uvl, 2.0)
        self.piece("Floor_Doma", "Planchers", M["Earth"], doma)
        self.collide((0.0, 0.0, -0.12), (1.8, 7.2, 0.0))

        def boards(bm, uvl):  # rooms B and D: planks along X
            add_box(bm, (1.8, 0.0, F - 0.03), (5.4, 3.6, F), uvl, 1.8)
            add_box(bm, (1.8, 3.6, F - 0.03), (5.4, 7.2, F), uvl, 1.8)
        ob = self.piece("Floor_Boards", "Planchers", M["FloorWood"], boards)
        self.collide((1.8, 0.0, F - 0.06), (5.4, 7.2, F))

        def sub(bm, uvl):  # under the tatami (rooms C, E) and the cupboard
            add_box(bm, (5.4, 0.0, F - 0.085), (9.0, 7.2, F - 0.055), uvl, 1.8)
        self.piece("Floor_TatamiBase", "Planchers", M["FloorWood"], sub)
        self.collide((5.4, 0.0, F - 0.1), (9.0, 7.2, F))

        def edge(bm, uvl):  # agari-kamachi: thick edge beam where the raised floor meets the doma
            add_box(bm, (1.8 - 0.06, 0.0, F - 0.16), (1.8 + 0.06, 7.2, F + 0.005), uvl, 1.2, swap=True)
        self.piece("Floor_EdgeBeam", "Planchers", M["Timber"], edge, bevel=0.01)

        # tokonoma: raised alcove, dark front edge, board floor
        def toko(bm, uvl):
            add_box(bm, (5.5, 6.32, F), (7.13, 7.12, F + 0.10), uvl, 1.2)
        self.piece("Tokonoma_Board", "Planchers", M["Cedar"], toko, bevel=0.005)

        def toko_edge(bm, uvl):
            add_box(bm, (5.5, 6.30, F), (7.13, 6.38, F + 0.105), uvl, 1.2)
        self.piece("Tokonoma_Edge", "Planchers", M["Lacquer"], toko_edge, bevel=0.004)
        self.collide((5.5, 6.3, F), (7.13, 7.12, F + 0.1))

        # ceilings over the raised rooms (hidden with the roof)
        def ceiling(bm, uvl):
            add_box(bm, (1.8, 0.0, P["keta"][0] - 0.02), (9.0, 7.2, P["keta"][0]), uvl, 1.8, swap=True)
        self.ceiling = self.piece("Ceilings", "Toiture", M["Cedar"], ceiling)

        # stepping stones: doma -> room B, outside the engawa, doma entrance
        def stones(bm, uvl):
            add_rock(bm, (1.45, 2.7, 0.06), (0.32, 0.42, 0.18), seed=11, uvl=uvl, flat_top=0.22)
            add_rock(bm, (4.4, -1.65, 0.05), (0.36, 0.28, 0.18), seed=12, uvl=uvl, flat_top=0.22)
            add_rock(bm, (9.0 + P["eng_w"] + 0.42, 1.8, 0.05), (0.28, 0.36, 0.18), seed=13, uvl=uvl, flat_top=0.22)
            add_rock(bm, (1.25, -0.55, 0.0), (0.34, 0.30, 0.10), seed=14, uvl=uvl, flat_top=0.04)
        self.piece("SteppingStones", "Planchers", M["Stone"], stones)
        self.collide((1.13, 2.3, 0.0), (1.77, 3.1, 0.22))

    # ------------------------------------------------------------ engawa
    def build_engawa(self):
        M = self.M
        F = P["floor"]
        e = P["eng_w"]

        def deck(bm, uvl):
            # front band (planks along X), side band (planks along Y), corner (along X)
            n = 8
            for i in range(n):
                y0 = -e + i * e / n
                add_box(bm, (1.8, y0 + 0.004, F - 0.035), (9.0 + e, y0 + e / n - 0.004, F), uvl, 1.6)
            for i in range(n):
                x0 = 9.0 + i * e / n
                add_box(bm, (x0 + 0.004, 0.0, F - 0.035), (x0 + e / n - 0.004, 7.2, F), uvl, 1.6, swap=True)
        self.piece("Engawa_Deck", "Engawa", M["Engawa"], deck)
        self.collide((1.8, -e, F - 0.04), (9.0 + e, 0.0, F))
        self.collide((9.0, 0.0, F - 0.04), (9.0 + e, 7.2, F))

        def frame(bm, uvl):
            s0, s1 = P["sill"]
            add_box(bm, (1.8 - 0.05, -e - 0.06, s0), (9.0 + e + 0.06, -e + 0.06, F - 0.035), uvl, 1.4)
            add_box(bm, (9.0 + e - 0.06, -e, s0), (9.0 + e + 0.06, 7.28, F - 0.035), uvl, 1.4, swap=True)
            for x in (1.8, 3.6, 5.4, 7.2, 9.0):
                add_box(bm, (x - 0.05, -e, s1 - 0.1), (x + 0.05, 0.0, F - 0.035), uvl, 1.4, swap=True)
            for y in (1.8, 3.6, 5.4, 7.2):
                add_box(bm, (9.0, y - 0.05, s1 - 0.1), (9.0 + e, y + 0.05, F - 0.035), uvl, 1.4)
            # eave purlin on the engawa posts, ties back to the wall plates
            ze = P["z_eave"]
            add_box(bm, (1.6, -e - 0.08, ze - 0.12), (9.0 + e + 0.08, -e + 0.08, ze + 0.05), uvl, 1.4)
            add_box(bm, (9.0 + e - 0.08, -e, ze - 0.12), (9.0 + e + 0.08, 7.4, ze + 0.05), uvl, 1.4, swap=True)
        self.piece("Engawa_Frame", "Engawa", M["Timber"], frame, bevel=0.008)

    # ------------------------------------------------------------ walls and openings
    def build_walls(self):
        M = self.M
        bm_pl, bm_wood, bm_rail = bmesh.new(), bmesh.new(), bmesh.new()
        uv_pl, uv_wood, uv_rail = uv_layer(bm_pl), uv_layer(bm_wood), uv_layer(bm_rail)
        t = P["wall"]
        top = P["keta"][0]
        for axis, fixed, bays in self.walls_plan():
            for a, b, kind in bays:
                z0 = self.base_z(axis, fixed, a, b)
                if fixed in (0.0,) and axis == "y" or (axis == "x" and fixed in (0.0, 7.2) and a >= 1.8) or (
                        axis == "y" and fixed == 9.0):
                    exterior = True
                else:
                    exterior = axis == "x" and fixed == 7.2 or axis == "y" and fixed == 0.0
                wall_bottom = z0 if z0 == 0.0 else P["sill"][1]
                half = P["post"] / 2
                a1, b1 = a + half, b - half
                if fixed == 6.3:
                    a1 = a + (0.04 if a > 5.4 + 1e-6 else half)
                    b1 = b - (0.04 if b < 9.0 - 1e-6 else half)
                    if kind == "open":
                        # tokonoma: otoshigake beam and plaster above
                        self._seg(bm_wood, uv_wood, axis, fixed, a1, b1, 1.95 + P["floor"] - 0.25, 2.0 + P["floor"] - 0.25,
                                  0.07)
                        self._seg(bm_pl, uv_pl, axis, fixed, a1, b1, 2.0 + P["floor"] - 0.25, top, t * 0.7)
                        continue
                if kind in ("wall", "window"):
                    self._seg(bm_pl, uv_pl, axis, fixed, a1, b1, wall_bottom, top, t)
                    self.collide(*self._seg_box(axis, fixed, a1, b1, wall_bottom, top, t))
                    if exterior:  # aged board skirting outside
                        self._skirt(bm_wood, uv_wood, axis, fixed, a1, b1, wall_bottom, wall_bottom + 0.55, t)
                    if kind == "window":
                        self._window(bm_wood, uv_wood, axis, fixed, (a1 + b1) / 2, z0 + 1.05, t)
                    continue
                # openings: kamoi above, plaster transom, sill rails
                kz = (2.10 if z0 == 0.0 else self.kamoi)
                if kind == "door":
                    kz = 2.0
                    open_a = a1 + 0.62
                    self._seg(bm_pl, uv_pl, axis, fixed, a1, open_a, wall_bottom, top, t)
                    self._skirt(bm_wood, uv_wood, axis, fixed, a1, open_a, wall_bottom, wall_bottom + 0.55, t)
                    self.collide(*self._seg_box(axis, fixed, a1, open_a, wall_bottom, top, t))
                    self._seg(bm_wood, uv_wood, axis, fixed, open_a, b1, kz, kz + 0.12, t + 0.02)
                    self._seg(bm_pl, uv_pl, axis, fixed, open_a, b1, kz + 0.12, top, t)
                    self._seg(bm_rail, uv_rail, axis, fixed, open_a - 0.02, b1, -0.02, 0.03, t + 0.04)
                    # outside track: door slides left past the corner
                    self._seg(bm_wood, uv_wood, axis, fixed - 0.11, -0.2, b1 + 0.02, kz - 0.02, kz + 0.06, 0.05)
                    self.add_door(open_a, b1, kz, fixed)
                    continue
                if kind == "step":
                    kz = self.kamoi
                self._seg(bm_wood, uv_wood, axis, fixed, a1, b1, kz, kz + 0.11, t + 0.02)
                self._seg(bm_pl, uv_pl, axis, fixed, a1, b1, kz + 0.11, top, t)
                rail_z = P["floor"]
                self._seg(bm_rail, uv_rail, axis, fixed, a1, b1, rail_z - 0.03, rail_z + 0.004, t + 0.03)
                if kind in ("shoji", "fusuma", "step"):
                    ptype = {"shoji": "shoji", "fusuma": "fusuma", "step": "itado"}[kind]
                    self.add_pair(ptype, axis, fixed, a1, b1, rail_z, kz)
        geo.obj("Minka_Walls_Plaster", bm_pl, M["Plaster"], self.c["Murs"])
        ob = geo.obj("Minka_Walls_Wood", bm_wood, M["Timber"], self.c["Murs"])
        geo.obj("Minka_Rails", bm_rail, M["Timber"], self.c["Murs"])

    def _seg_box(self, axis, fixed, a, b, z0, z1, t):
        if axis == "x":
            return (a, fixed - t / 2, z0), (b, fixed + t / 2, z1)
        return (fixed - t / 2, a, z0), (fixed + t / 2, b, z1)

    def _seg(self, bm, uvl, axis, fixed, a, b, z0, z1, t):
        if b - a < 1e-3 or z1 - z0 < 1e-3:
            return
        mn, mx = self._seg_box(axis, fixed, a, b, z0, z1, t)
        add_box(bm, mn, mx, uvl, 1.6 if t > 0.06 else 1.2)

    def _skirt(self, bm, uvl, axis, fixed, a, b, z0, z1, t):
        # vertical boards proud of the plaster on both faces
        n = max(1, int((b - a) / 0.18))
        for i in range(n):
            u0 = a + (b - a) * i / n + 0.003
            u1 = a + (b - a) * (i + 1) / n - 0.003
            mn, mx = self._seg_box(axis, fixed, u0, u1, z0, z1, t + 0.024)
            add_box(bm, mn, mx, uvl, 1.0, swap=True)

    def _window(self, bm, uvl, axis, fixed, c, zc, t):
        """Barred window (renji-mado): frame and vertical slats through the plaster."""
        w, h = 0.9, 0.6
        a, b = c - w / 2, c + w / 2
        for (u0, u1, z0, z1) in ((a - 0.05, b + 0.05, zc - h / 2 - 0.05, zc - h / 2),
                                 (a - 0.05, b + 0.05, zc + h / 2, zc + h / 2 + 0.05),
                                 (a - 0.05, a, zc - h / 2, zc + h / 2), (b, b + 0.05, zc - h / 2, zc + h / 2)):
            mn, mx = self._seg_box(axis, fixed, u0, u1, z0, z1, t + 0.04)
            add_box(bm, mn, mx, uvl, 1.0)
        for i in range(1, 9):
            u = a + w * i / 9
            mn, mx = self._seg_box(axis, fixed, u - 0.012, u + 0.012, zc - h / 2, zc + h / 2, t + 0.02)
            add_box(bm, mn, mx, uvl, 1.0, swap=True)
        # dark opening behind the slats (the plaster wall is cut visually by a dark inset)
        mn, mx = self._seg_box(axis, fixed, a, b, zc - h / 2, zc + h / 2, t + 0.006)
        add_box(bm, mn, mx, uvl, 1.0)

    # ------------------------------------------------------------ sliding panels
    def panel_mesh(self, ptype, w, h):
        """One leaf in local space: x along the track from 0 to w, y across (thickness), z up from the rail."""
        key = f"{ptype}_{w:.3f}_{h:.3f}"
        if key in getattr(self, "_panel_cache", {}):
            return self._panel_cache[key]
        self._panel_cache = getattr(self, "_panel_cache", {})
        M = self.M
        bm = bmesh.new()
        uvl = uv_layer(bm)
        mats = []
        if ptype == "shoji":
            t = P["shoji_t"]
            mats = [M["ShojiWood"], M["Washi"]]
            frame = []
            s = 0.035
            frame += add_box(bm, (0, 0, 0), (s, t, h), uvl, 1.0, swap=True)
            frame += add_box(bm, (w - s, 0, 0), (w, t, h), uvl, 1.0, swap=True)
            frame += add_box(bm, (s, 0, h - s), (w - s, t, h), uvl, 1.0)
            frame += add_box(bm, (s, 0, 0), (w - s, t, 0.26), uvl, 1.0)          # koshi (kick board)
            for i in range(1, 4):                                                 # kumiko lattice
                x = s + (w - 2 * s) * i / 4
                frame += add_box(bm, (x - 0.007, 0.004, 0.26), (x + 0.007, t - 0.004, h - s), uvl, 1.0, swap=True)
            nz = 7
            for j in range(1, nz):
                z = 0.26 + (h - s - 0.26) * j / nz
                frame += add_box(bm, (s, 0.004, z - 0.007), (w - s, t - 0.004, z + 0.007), uvl, 1.0)
            paper = add_box(bm, (s, t * 0.25, 0.26), (w - s, t * 0.25 + 0.002, h - s), uvl, 1.0)
            for f in paper:
                f.material_index = 1
        elif ptype == "fusuma":
            t = P["fusuma_t"]
            mats = [M["Lacquer"], M["Fusuma"]]
            s = 0.022
            add_box(bm, (0, 0, 0), (s, t, h), uvl, 1.0)
            add_box(bm, (w - s, 0, 0), (w, t, h), uvl, 1.0)
            add_box(bm, (s, 0, h - s), (w - s, t, h), uvl, 1.0)
            add_box(bm, (s, 0, 0), (w - s, t, s), uvl, 1.0)
            paper = add_box(bm, (s, 0.002, s), (w - s, t - 0.002, h - s), uvl, 1.6)
            for f in paper:
                f.material_index = 1
            for yy in (-0.001, t - 0.002):      # hikite (round pulls) on both faces
                faces = add_cyl(bm, (w - 0.09, yy, 0.85), (w - 0.09, yy + 0.003, 0.85), 0.03, segs=16)
        else:  # itado: wooden board door (doma side of room B, entrance)
            t = 0.034
            mats = [M["Cedar"], M["Timber"]]
            n = max(3, int(w / 0.15))
            for i in range(n):
                add_box(bm, (w * i / n + 0.002, 0, 0), (w * (i + 1) / n - 0.002, t, h), uvl, 1.2, swap=True)
            for zz in (0.25, h * 0.5, h - 0.25):
                fs = add_box(bm, (0.04, t, zz - 0.04), (w - 0.04, t + 0.02, zz + 0.04), uvl, 1.2)
                for f in fs:
                    f.material_index = 1
        me = bpy.data.meshes.new(f"SM_Minka_{ptype.capitalize()}_{int(w * 100)}x{int(h * 100)}")
        bm.normal_update()
        bm.to_mesh(me)
        bm.free()
        for m in mats:
            me.materials.append(m)
        self._panel_cache[key] = me
        return me

    def add_panel(self, ptype, name, origin, axis, w, h, travel, track_off, open_frac=0.0):
        coll = self.c["Fusuma" if ptype == "fusuma" else "Shoji"] if ptype != "itado" else self.c["Shoji"]
        if ptype == "itado":
            coll = self.c["Fusuma"] if name.startswith("Minka_Itado_Step") else self.c["Murs"]
        me = self.panel_mesh(ptype, w, h)
        ob = bpy.data.objects.new(name, me)
        coll.objects.link(ob)
        ox, oy, oz = origin
        if axis == "x":
            ob.location = (ox, oy + track_off, oz)
            d = (travel, 0.0)
        else:
            ob.rotation_euler = (0, 0, math.pi / 2)
            ob.location = (ox + track_off, oy, oz)
            d = (0.0, travel)
        ob["panel_type"] = ptype
        ob["closed_x"], ob["closed_y"] = ob.location.x, ob.location.y
        ob["travel_x"], ob["travel_y"] = d
        ob["open"] = float(open_frac)
        try:
            ui = ob.id_properties_ui("open")
            ui.update(min=0.0, max=1.0, soft_min=0.0, soft_max=1.0, description="0 = fermé, 1 = ouvert")
        except (AttributeError, TypeError):
            pass
        for i, (c, tr) in enumerate((("closed_x", "travel_x"), ("closed_y", "travel_y"))):
            fc = ob.driver_add("location", i)
            drv = fc.driver
            drv.type = "SCRIPTED"
            for vname, prop in (("c", c), ("t", tr), ("o", "open")):
                var = drv.variables.new()
                var.name = vname
                var.type = "SINGLE_PROP"
                var.targets[0].id_type = "OBJECT"
                var.targets[0].id = ob
                var.targets[0].data_path = f'["{prop}"]'
            drv.expression = "c + o * t"
        self.panels.append(ob)
        return ob

    def add_pair(self, ptype, axis, fixed, a, b, z0, kz):
        """Two leaves on two tracks between posts: leaf 1 slides behind leaf 2 when opened (no post crossing)."""
        w = (b - a) / 2 + 0.02
        h = kz - z0
        th = P["shoji_t"] if ptype == "shoji" else P["fusuma_t"] if ptype == "fusuma" else 0.034
        off1, off2 = -th * 0.55, th * 0.55
        tag = f"{'X' if axis == 'x' else 'Y'}{fixed:.1f}_{a:.2f}".replace(".", "")
        base = {"shoji": "Minka_Shoji", "fusuma": "Minka_Fusuma", "itado": "Minka_Itado_Step"}[ptype]
        if axis == "x":
            o1, o2 = (a, fixed, z0), (b - w, fixed, z0)
        else:
            o1, o2 = (fixed, a, z0), (fixed, b - w, z0)
        # fix the rotated leaves (rotation +90 deg about Z maps local x to world +Y)
        travel = (b - w) - a
        open1 = 0.0
        open2 = 0.0
        if ptype == "shoji" and axis == "x" and fixed == 0.0 and a > 5.0:
            open1 = 1.0      # front shoji of the tatami room partly open (brief: reveal the interior)
        if ptype == "shoji" and axis == "y" and a < 1.0:
            open1 = 1.0
        if ptype == "fusuma" and (axis, fixed) == ("x", 3.6) and a > 5.0:
            open2 = 1.0
        self.add_panel(ptype, f"{base}_{tag}_1", o1, axis, w, h, travel, off1 if axis == "x" else -off1, open1)
        self.add_panel(ptype, f"{base}_{tag}_2", o2, axis, w, h, -travel, off2 if axis == "x" else -off2, open2)

    def add_door(self, a, b, kz, fixed):
        """Entrance itado on an outside track, slides left past the corner post (open = fully clear)."""
        w = b - a + 0.04
        h = kz - 0.02
        self.add_panel("itado", "Minka_Itado_Entrance", (a - 0.02, fixed - 0.11, 0.0), "x", w, h, -(w - 0.06), 0.0,
                       0.65)

    # ------------------------------------------------------------ tatami
    def build_tatami(self):
        M = self.M
        F = P["floor"]
        th = 0.055

        def lay(x0, y0, x1, y1, cols, rows, mats_layout, tag):
            ux, uy = (x1 - x0) / cols, (y1 - y0) / rows
            for i, (cx, cy, cw, ch) in enumerate(mats_layout):
                mn = (x0 + cx * ux + 0.003, y0 + cy * uy + 0.003, F - th)
                mx = (x0 + (cx + cw) * ux - 0.003, y0 + (cy + ch) * uy - 0.003, F)
                bm = bmesh.new()
                uvl = uv_layer(bm)
                faces = add_box(bm, mn, mx, uvl, 1.0)
                long_x = (mx[0] - mn[0]) >= (mx[1] - mn[1])
                for f in faces:
                    for loop in f.loops:
                        p = loop.vert.co
                        fu = (p.x - mn[0]) / (mx[0] - mn[0])
                        fv = (p.y - mn[1]) / (mx[1] - mn[1])
                        if abs(f.normal.z) < 0.5:   # sides: thin strip of the border
                            loop[uvl].uv = ((fu if abs(f.normal.y) > 0.5 else fv), (p.z - mn[2]) / th * 0.02)
                        elif long_x:
                            loop[uvl].uv = (fu, fv * 0.5)
                        else:
                            loop[uvl].uv = (fv, fu * 0.5)
                geo.obj(f"Minka_Tatami_{tag}_{i:02d}", bm, M["Tatami"], self.c["Tatamis"])

        h = P["post"] / 2
        # room C: 8 mats in a 4 x 4 unit square (no four corners meeting)
        lay(5.4 + h, 0.0 + h, 9.0 - h, 3.6 - h, 4, 4,
            [(0, 0, 2, 1), (2, 0, 2, 1), (0, 1, 1, 2), (1, 1, 2, 1), (1, 2, 2, 1), (3, 1, 1, 2), (0, 3, 2, 1),
             (2, 3, 2, 1)], "C")
        # room E: 6 mats in a 4 x 3 unit rectangle
        lay(5.4 + h, 3.6 + h, 9.0 - h, 6.3 - 0.04, 4, 3,
            [(0, 0, 1, 2), (1, 0, 2, 1), (3, 0, 1, 2), (1, 1, 2, 1), (0, 2, 2, 1), (2, 2, 2, 1)], "E")
        # cupboard floor: boards
        def cupboard(bm, uvl):
            add_box(bm, (7.24, 6.34, F - 0.055), (8.9, 7.1, F), uvl, 1.2)
        self.piece("Cupboard_Floor", "Planchers", M["Cedar"], cupboard)

    # ------------------------------------------------------------ roof
    def build_roof(self):
        M = self.M
        e = P["eng_w"]
        o = P["eave_out"]
        x0, x1 = -o, 9.0 + e + o
        y0, y1 = -e - o, 7.2 + o
        ze, zh, zr = P["z_eave"], P["z_hip"], P["z_ridge"]
        yc = (y0 + y1) / 2
        run = (zh - ze) / ((zr - ze) / (yc - y0))
        xa, xb = x0 + run, x1 - run
        ya, yb = y0 + run, y1 - run
        self.roof_dims = dict(x0=x0, x1=x1, y0=y0, y1=y1, yc=yc, xa=xa, xb=xb, ya=ya, yb=yb)
        bm = bmesh.new()
        uvl = uv_layer(bm)
        V = {}

        def v(p):
            k = tuple(round(c, 5) for c in p)
            if k not in V:
                V[k] = bm.verts.new(p)
            return V[k]

        quads = [
            [(x0, y0, ze), (x1, y0, ze), (xb, ya, zh), (xa, ya, zh)],      # front skirt
            [(x1, y1, ze), (x0, y1, ze), (xa, yb, zh), (xb, yb, zh)],      # back skirt
            [(x0, y1, ze), (x0, y0, ze), (xa, ya, zh), (xa, yb, zh)],      # left hip
            [(x1, y0, ze), (x1, y1, ze), (xb, yb, zh), (xb, ya, zh)],      # right hip
            [(xa, ya, zh), (xb, ya, zh), (xb, yc, zr), (xa, yc, zr)],      # front gable roof
            [(xb, yb, zh), (xa, yb, zh), (xa, yc, zr), (xb, yc, zr)],      # back gable roof
        ]
        faces = []
        for q in quads:
            faces.append(bm.faces.new([v(p) for p in q]))
        # subdivide for a slightly irregular, softer thatch silhouette
        bmesh.ops.subdivide_edges(bm, edges=bm.edges[:], cuts=6, use_grid_fill=True)
        rnd = random.Random(21)
        for vert in bm.verts:
            if vert.co.z > ze + 0.05 and vert.co.z < zr - 0.05:
                vert.co.z += rnd.uniform(-0.03, 0.03)
        bm.normal_update()
        slope_uv(bm.faces[:], uvl, 1.6)
        roof = geo.obj("Minka_Roof_Thatch", bm, [M["Thatch"], M["ThatchEdge"]], self.c["Toiture"], smooth=True)
        sol = roof.modifiers.new("Thatch", "SOLIDIFY")
        sol.thickness = P["thatch"]
        sol.offset = -1.0
        sol.use_even_offset = True
        sol.material_offset_rim = 1
        self.roof = roof

        # ridge: thick bound roll with bindings
        def ridge(bm2, uvl2):
            add_cyl(bm2, (xa - 0.05, yc, zr + 0.06), (xb + 0.05, yc, zr + 0.06), 0.22, segs=20, uvl=uvl2, tile=1.0)
        self.piece("Roof_Ridge", "Toiture", M["ThatchEdge"], ridge, smooth=True)

        def bindings(bm2, uvl2):
            n = int((xb - xa) / 0.6)
            for i in range(n + 1):
                x = xa + (xb - xa) * i / n
                add_cyl(bm2, (x - 0.025, yc, zr + 0.06), (x + 0.025, yc, zr + 0.06), 0.235, segs=20, uvl=uvl2, tile=0.5)
        self.piece("Roof_RidgeBindings", "Toiture", M["Rope"], bindings, smooth=True)

        # gable boards with a smoke vent (vertical triangles above the hip skirt)
        def gables(bm2, uvl2):
            for xg in (xa + 0.1, xb - 0.1):
                n = 12
                for i in range(n):
                    y_a = ya + (yb - ya) * i / n
                    y_b = ya + (yb - ya) * (i + 1) / n
                    ym = (y_a + y_b) / 2
                    hz = zh + (zr - zh) * (1 - abs(ym - yc) / (yc - ya))
                    add_box(bm2, (xg - 0.02, y_a + 0.004, zh - 0.1), (xg + 0.02, y_b - 0.004, hz - 0.05), uvl2, 1.0,
                            swap=True)
                # smoke vent lattice
                vz = zh + (zr - zh) * 0.45
                for k in range(-3, 4):
                    add_box(bm2, (xg - 0.05, yc + k * 0.12 - 0.015, vz - 0.25), (xg + 0.05, yc + k * 0.12 + 0.015,
                                                                                   vz + 0.25), uvl2, 1.0, swap=True)
        self.piece("Roof_Gables", "Toiture", M["Timber"], gables)

    # ------------------------------------------------------------ furniture
    def build_furniture(self, import_model):
        M = self.M
        F = P["floor"]
        # irori in room B: dark frame, ash pit, charcoal, embers, jizai-kagi, iron kettle
        cx, cy = 3.6, 1.8

        def frame(bm, uvl):
            s, wdt = 0.48, 0.1
            for (a, b, c, d) in ((-s, -s, s, -s + wdt), (-s, s - wdt, s, s), (-s, -s + wdt, -s + wdt, s - wdt),
                                 (s - wdt, -s + wdt, s, s - wdt)):
                add_box(bm, (cx + a, cy + b, F - 0.12), (cx + c, cy + d, F + 0.008), uvl, 1.0)
        self.piece("Irori_Frame", "Mobilier", M["Lacquer"], frame, bevel=0.004)

        def ash(bm, uvl):
            add_box(bm, (cx - 0.38, cy - 0.38, F - 0.2), (cx + 0.38, cy + 0.38, F - 0.11), uvl, 1.0)
        self.piece("Irori_Ash", "Mobilier", M["Ash"], ash)

        def coals(bm, uvl):
            for k in range(7):
                a = TAU * k / 7
                add_rock(bm, (cx + 0.1 * math.cos(a), cy + 0.1 * math.sin(a), F - 0.1), (0.05, 0.04, 0.03), seed=40 + k,
                         uvl=uvl)
        self.piece("Irori_Charcoal", "Mobilier", M["Charcoal"], coals)

        def embers(bm, uvl):
            add_rock(bm, (cx, cy, F - 0.105), (0.06, 0.05, 0.025), seed=50, uvl=uvl)
        self.piece("Irori_Embers", "Mobilier", M["Ember"], embers)

        def jizai(bm, uvl):
            add_cyl(bm, (cx, cy, P["keta"][0] - 0.02), (cx, cy, 0.98 + F), 0.028, segs=10, uvl=uvl, tile=1.0)
        self.piece("Irori_JizaiKagi", "Mobilier", M["Bamboo"], jizai, smooth=True)

        def lever(bm, uvl):
            add_box(bm, (cx - 0.22, cy - 0.03, 1.35 + F), (cx + 0.22, cy + 0.03, 1.45 + F), uvl, 0.6)
            add_cyl(bm, (cx, cy, 0.98 + F), (cx, cy, 0.62 + F), 0.006, segs=8)
        self.piece("Irori_Hook", "Mobilier", M["Iron"], lever)

        def kettle(bm, uvl):
            prof = [(0.0, 0.0), (0.09, 0.005), (0.13, 0.05), (0.135, 0.09), (0.11, 0.14), (0.06, 0.16), (0.0, 0.17)]
            add_revolve(bm, prof, (cx, cy, F + 0.36), segs=28, uvl=uvl)
            add_cyl(bm, (cx + 0.11, cy, F + 0.44), (cx + 0.22, cy, F + 0.50), 0.022, 0.012, segs=10)
            add_revolve(bm, [(0.0, 0.0), (0.03, 0.0), (0.012, 0.03), (0.0, 0.035)], (cx, cy, F + 0.53), segs=16)
            pts = [Vector((cx + 0.12 * math.cos(a), cy, F + 0.50 + 0.12 * math.sin(a))) for a in
                   (math.pi * k / 10 for k in range(11))]
            for i in range(len(pts) - 1):
                add_cyl(bm, pts[i], pts[i + 1], 0.006, segs=6, caps=False)
        self.piece("Irori_Tetsubin", "Mobilier", M["Iron"], kettle, smooth=True)

        # room C: low table, two cushions, tea set
        tx, ty = 7.2, 1.85

        def table(bm, uvl):
            add_box(bm, (tx - 0.45, ty - 0.3, F + 0.29), (tx + 0.45, ty + 0.3, F + 0.33), uvl, 1.0)
            for sx in (-1, 1):
                for sy in (-1, 1):
                    add_box(bm, (tx + sx * 0.38 - 0.03, ty + sy * 0.23 - 0.03, F), (tx + sx * 0.38 + 0.03,
                                                                                     ty + sy * 0.23 + 0.03, F + 0.29),
                            uvl, 1.0, swap=True)
        self.piece("Table", "Mobilier", M["Cedar"], table, bevel=0.006)
        self.collide((tx - 0.45, ty - 0.3, F), (tx + 0.45, ty + 0.3, F + 0.33))

        def cushions(bm, uvl):
            for sy in (-1, 1):
                c = Vector((tx, ty + sy * 0.62, F + 0.04))
                add_box(bm, (c.x - 0.27, c.y - 0.29, F), (c.x + 0.27, c.y + 0.29, F + 0.075), uvl, 0.6)
        ob = self.piece("Cushions", "Mobilier", M["Olive"], cushions, bevel=0.025)
        tea = import_model("tea_set_01", 0.32, (tx, ty, F + 0.33), self.c["Mobilier"])

        # room D: tansu chest and folded futon
        def tansu(bm, uvl):
            add_box(bm, (2.05, 6.62, F), (3.1, 7.05, F + 0.95), uvl, 1.0)
        self.piece("Tansu", "Mobilier", M["Cedar"], tansu, bevel=0.008)
        self.collide((2.05, 6.62, F), (3.1, 7.05, F + 0.95))

        def tansu_iron(bm, uvl):
            for zz in (0.18, 0.42, 0.66, 0.86):
                for xx in (2.3, 2.85):
                    add_box(bm, (xx - 0.07, 6.6, F + zz - 0.012), (xx + 0.07, 6.615, F + zz + 0.012), uvl, 0.5)
                    add_cyl(bm, (xx, 6.6, F + zz - 0.035), (xx, 6.59, F + zz - 0.035), 0.03, segs=10)
        self.piece("Tansu_Fittings", "Mobilier", M["Iron"], tansu_iron)

        def futon(bm, uvl):
            add_box(bm, (3.55, 6.35, F), (4.75, 7.05, F + 0.12), uvl, 0.6)
            add_box(bm, (3.6, 6.4, F + 0.12), (4.7, 7.0, F + 0.22), uvl, 0.6)
        self.piece("Futon", "Mobilier", M["FutonCream"], futon, bevel=0.03)

        def futon_cover(bm, uvl):
            add_box(bm, (3.62, 6.42, F + 0.22), (4.68, 6.98, F + 0.3), uvl, 0.6)
        self.piece("Futon_Cover", "Mobilier", M["FutonIndigo"], futon_cover, bevel=0.03)

        # room E: paper lantern (andon) with a warm light
        def andon_frame(bm, uvl):
            x, y = 8.5, 5.9
            for sx in (-1, 1):
                for sy in (-1, 1):
                    add_box(bm, (x + sx * 0.13 - 0.012, y + sy * 0.13 - 0.012, F), (x + sx * 0.13 + 0.012,
                                                                                     y + sy * 0.13 + 0.012, F + 0.75),
                            uvl, 0.5, swap=True)
            add_box(bm, (x - 0.15, y - 0.15, F + 0.12), (x + 0.15, y + 0.15, F + 0.14), uvl, 0.5)
        self.piece("Andon_Frame", "Mobilier", M["Lacquer"], andon_frame)

        def andon_paper(bm, uvl):
            x, y = 8.5, 5.9
            add_box(bm, (x - 0.125, y - 0.125, F + 0.2), (x + 0.125, y + 0.125, F + 0.72), uvl, 0.5)
        self.piece("Andon_Paper", "Mobilier", M["Lantern"], andon_paper)
        light = bpy.data.lights.new("Minka_AndonLight", "POINT")
        light.energy = 25.0
        light.color = (1.0, 0.78, 0.5)
        light.shadow_soft_size = 0.1
        lo = bpy.data.objects.new("Minka_AndonLight", light)
        lo.location = (8.5, 5.9, F + 0.45)
        self.c["Mobilier"].objects.link(lo)

        # tokonoma: hanging scroll and a small vase with a flower branch
        def scroll(bm, uvl):
            x0, x1, z0, z1 = 6.08, 6.52, F + 0.55, F + 1.95
            faces = add_box(bm, (x0, 7.09, z0), (x1, 7.1, z1), uvl, 1.0)
            for f in faces:
                for loop in f.loops:
                    p = loop.vert.co
                    loop[uvl].uv = ((p.x - x0) / (x1 - x0), (p.z - z0) / (z1 - z0))
        ob = self.piece("Kakejiku", "Mobilier", self.scroll_material(), scroll)

        def roller(bm, uvl):
            add_cyl(bm, (6.02, 7.085, F + 0.54), (6.58, 7.085, F + 0.54), 0.012, segs=10)
            add_cyl(bm, (6.06, 7.09, F + 1.96), (6.54, 7.09, F + 1.96), 0.008, segs=8)
        self.piece("Kakejiku_Rollers", "Mobilier", M["Lacquer"], roller, smooth=True)
        vase = import_model("ceramic_vase_02", 0.2, (6.75, 6.75, F + 0.1), self.c["Mobilier"])

        def branch(bm, uvl):
            base = Vector((6.75, 6.75, F + 0.26))
            add_cyl(bm, base, base + Vector((0.05, -0.02, 0.28)), 0.004, segs=6)
            add_cyl(bm, base + Vector((0.03, -0.01, 0.17)), base + Vector((-0.08, 0.02, 0.24)), 0.003, segs=6)
        self.piece("Ikebana_Stem", "Mobilier", M["Lacquer"], branch)

        def leaves(bm, uvl):
            base = Vector((6.75, 6.75, F + 0.26))
            for k, (dx, dz) in enumerate(((0.05, 0.28), (0.02, 0.2), (-0.08, 0.24), (0.04, 0.12))):
                add_rock(bm, base + Vector((dx, -0.01, dz)), (0.022, 0.006, 0.012), seed=60 + k)
        self.piece("Ikebana_Leaves", "Mobilier", M["Leaf"], leaves)

        def flowers(bm, uvl):
            base = Vector((6.75, 6.75, F + 0.26))
            for k, (dx, dz) in enumerate(((0.05, 0.3), (-0.08, 0.255), (0.01, 0.23))):
                add_rock(bm, base + Vector((dx, -0.02, dz)), (0.016, 0.016, 0.012), seed=70 + k)
        self.piece("Ikebana_Flowers", "Mobilier", M["Flower"], flowers)

        # doma: kamado stove, water jar, shelf, firewood, bucket, broom, bowls
        def kamado(bm, uvl):
            add_box(bm, (0.12, 5.95, 0.0), (1.42, 6.7, 0.68), uvl, 1.2)
        ob = self.piece("Kamado", "Mobilier", M["Clay"], kamado, bevel=0.04)
        self.collide((0.12, 5.95, 0.0), (1.42, 6.7, 0.68))

        def kamado_pots(bm, uvl):
            for x in (0.45, 1.08):
                add_revolve(bm, [(0.0, 0.0), (0.16, 0.0), (0.2, 0.09), (0.19, 0.14), (0.0, 0.14)], (x, 6.32, 0.62),
                            segs=24)
                add_cyl(bm, (x, 6.32, 0.775), (x, 6.32, 0.79), 0.19, segs=24)
        self.piece("Kamado_Pots", "Mobilier", M["Iron"], kamado_pots, smooth=True)

        def kamado_mouths(bm, uvl):
            for x in (0.45, 1.08):
                add_box(bm, (x - 0.12, 5.94, 0.08), (x + 0.12, 5.96, 0.3), uvl, 1.0)
        self.piece("Kamado_Mouths", "Mobilier", M["Charcoal"], kamado_mouths)

        def jar(bm, uvl):
            prof = [(0.0, 0.0), (0.16, 0.0), (0.26, 0.15), (0.27, 0.35), (0.22, 0.52), (0.2, 0.56), (0.0, 0.56)]
            add_revolve(bm, prof, (1.45, 6.95, 0.0), segs=28, uvl=uvl)
        self.piece("WaterJar", "Mobilier", M["Ceramic"], jar, smooth=True)
        self.collide((1.18, 6.68, 0.0), (1.72, 7.12, 0.56))

        def shelf(bm, uvl):
            add_box(bm, (0.1, 7.02, 1.25), (1.4, 7.15, 1.28), uvl, 1.0)
            for x in (0.25, 1.25):
                add_box(bm, (x - 0.02, 7.08, 1.12), (x + 0.02, 7.15, 1.25), uvl, 1.0)
        self.piece("Doma_Shelf", "Mobilier", M["Cedar"], shelf)
        for k, x in enumerate((0.45, 0.8)):
            import_model("wooden_bowl_01", 0.18, (x, 7.08, 1.28), self.c["Mobilier"], name=f"Bowl{k}")

        def firewood(bm, uvl):
            for k in range(9):
                row, col = divmod(k, 3)
                y = 4.95 + col * 0.13 + (0.06 if row % 2 else 0)
                add_cyl(bm, (0.12, y, 0.07 + row * 0.12), (0.72, y, 0.07 + row * 0.12), 0.06, segs=10, uvl=uvl,
                        tile=0.8)
        self.piece("Firewood", "Mobilier", M["Timber"], firewood, smooth=True)
        import_model("wooden_bucket_01", 0.32, (1.5, 0.6, 0.0), self.c["Mobilier"])
        broom = import_model("wooden_broom", 1.1, (0.25, 0.35, 0.0), self.c["Mobilier"], up=True)

    def scroll_material(self):
        """Hanging scroll texture painted with numpy: washi, layered ink mountains, a red seal."""
        import numpy as np
        path = geo.DERIVED / "kakejiku_mountains.png"
        if not path.exists():
            geo.DERIVED.mkdir(parents=True, exist_ok=True)
            W, H = 360, 1160
            img = np.ones((H, W, 3), np.float32) * np.array([236, 228, 208], np.float32) / 255
            y = np.arange(H)[:, None]                      # row 0 = top of the scroll
            x = np.arange(W)[None, :]
            img[:150] = np.array([96, 110, 84]) / 255
            img[H - 170:] = np.array([96, 110, 84]) / 255
            rnd = random.Random(5)
            for layer, (base, amp, shade) in enumerate(((760, 120, 150), (820, 90, 90), (880, 60, 40))):
                cx = W * (0.35 + 0.25 * layer)
                ridge = base - amp * np.exp(-((x - cx) / 90.0) ** 2) - 7 * np.sin(x / 9.0 + layer)
                m = (y >= ridge) & (y < H - 180)
                img[np.broadcast_to(m, (H, W))] = shade / 255
            img[H - 260:H - 230, W - 70:W - 40] = np.array([178, 42, 34]) / 255
            # soften like ink on paper: cheap box blur
            k = 3
            pad = np.pad(img, ((k, k), (k, k), (0, 0)), mode="edge")
            acc = np.zeros_like(img)
            for dy in range(-k, k + 1):
                for dx in range(-k, k + 1):
                    acc += pad[k + dy:k + dy + H, k + dx:k + dx + W]
            img = acc / (2 * k + 1) ** 2
            out = bpy.data.images.new("_kakejiku", W, H, alpha=False)
            rgba = np.concatenate([img[::-1], np.ones((H, W, 1), np.float32)], -1)   # Blender rows go upward
            out.pixels.foreach_set(rgba.ravel())
            out.filepath_raw = str(path)
            out.file_format = "PNG"
            out.save()
            bpy.data.images.remove(out)
        return geo.img_material("M_Minka_Kakejiku", path, rough_value=0.85, tint_hex="DCD2BA")

    # ------------------------------------------------------------ collisions
    def build_collisions(self):
        objs = []
        for i, (mn, mx) in enumerate(self.ucx):
            bm = bmesh.new()
            add_box(bm, mn, mx)
            me = bpy.data.meshes.new(f"UCX_SM_Minka_Structure_{i:02d}")
            bm.to_mesh(me)
            bm.free()
            ob = bpy.data.objects.new(me.name, me)
            self.c["Collisions"].objects.link(ob)
            ob.display_type = "WIRE"
            ob.hide_render = True
            objs.append(ob)
        self.c["Collisions"].hide_render = True
        return objs

    # ------------------------------------------------------------ all
    def build(self, import_model):
        self.build_structure()
        self.build_floors()
        self.build_engawa()
        self.build_walls()
        self.build_tatami()
        self.build_roof()
        self.build_furniture(import_model)
        self.ucx_objects = self.build_collisions()
        return self
