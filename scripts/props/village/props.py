"""Village props, each built at the origin in its own local frame (meters, +Z up, front towards -Y, pivot on the
ground) so it can be instanced in Blender and exported as one static mesh for Unreal (SM_Village_<Name>).
Materials reuse the minka ones (CC0 photo textures on world-scaled UVs) plus roof tiles and white plaster.
No vegetation and no fields: in Unreal the villages live among the palms and bushes already on the island
(scripts/map/place_villages.py). Bases and posts go 0.3 to 0.6 m below the pivot so a prop set on uneven ground
never floats; `ucx` gives the simple collision boxes exported as UCX_SM_Village_<Name>_NN.
"""
import math
import random

import bmesh
import bpy
from mathutils import Matrix, Vector

import minka.geo as geo
from minka.geo import TAU, add_box, add_box_m, add_cyl, add_revolve, add_rock, uv_layer


def village_materials(M):
    T = geo.MINKA_TEX

    def tex(asset):
        f = T / asset
        return geo.find(f, "_diff_"), geo.find(f, "_nor_gl_"), geo.find(f, "_rough_")

    c, n, r = tex("clay_roof_tiles")
    M["Tiles"] = geo.img_material("M_Village_RoofTiles", geo.derived(c, "tiles_charcoal", (0.42, 0.42, 0.45), sat=0.2),
                                  n, r, tint_hex="3C3C40")
    c, n, r = tex("patterned_clay_plaster")
    M["WhitePlaster"] = geo.img_material("M_Village_WhitePlaster", geo.derived(c, "plaster_white", (1.18, 1.17, 1.14),
                                                                               (0.08, 0.08, 0.08), sat=0.25),
                                         n, r, tint_hex="F0EEE8")
    M["Water"] = geo.flat_material("M_Village_WellWater", "1E2A26", rough=0.05)
    M["Bronze"] = geo.flat_material("M_Village_Bronze", "4A5A48", rough=0.45, metal=0.8)
    M["Paper"] = geo.flat_material("M_Village_NoticePaper", "E8E0CC", rough=0.9)
    return M


class Props:
    def __init__(self, M, lib):
        self.M = M
        self.lib = lib          # collection holding the prop library (excluded from the view layer)
        self.made = {}
        self.ucx = {}           # name -> [(min, max)] simple collision boxes (meters, local frame)

    def make(self, name, parts, ucx=()):
        """parts: list of (material, builder(bm, uvl)). One object per prop, multi-material."""
        bm = bmesh.new()
        uvl = uv_layer(bm)
        mats = []
        for mat, build in parts:
            if mat not in mats:
                mats.append(mat)
            idx = mats.index(mat)
            n0 = len(bm.faces)
            build(bm, uvl)
            bm.faces.ensure_lookup_table()
            for i in range(n0, len(bm.faces)):
                bm.faces[i].material_index = idx
        coll = bpy.data.collections.new(f"LIB_{name}")
        self.lib.children.link(coll)
        ob = geo.obj(f"SM_Village_{name}", bm, mats, coll)
        self.made[name] = coll
        self.ucx[name] = list(ucx)
        return coll

    # ---------------------------------------------------------------- buildings
    def kura(self):
        M = self.M
        W, D, H = 4.2, 5.4, 4.6

        def walls(bm, uvl):
            add_box(bm, (-W / 2, -D / 2, 0.55), (W / 2, D / 2, H), uvl, 2.0)
        def base(bm, uvl):
            add_box(bm, (-W / 2 - 0.12, -D / 2 - 0.12, -0.6), (W / 2 + 0.12, D / 2 + 0.12, 0.55), uvl, 0.8)
        def boards(bm, uvl):  # dark weatherboards on the lower walls
            for side in (-1, 1):
                add_box(bm, (-W / 2 - 0.03, side * D / 2 - 0.03, 0.55), (W / 2 + 0.03, side * D / 2 + 0.03, 1.9), uvl,
                        1.0, swap=True)
                add_box(bm, (side * W / 2 - 0.03, -D / 2, 0.55), (side * W / 2 + 0.03, D / 2, 1.9), uvl, 1.0, swap=True)
            add_box(bm, (-W / 2 - 0.08, -D / 2 - 0.08, H - 0.2), (W / 2 + 0.08, D / 2 + 0.08, H), uvl, 1.0)
        def door(bm, uvl):
            add_box(bm, (-0.65, -D / 2 - 0.12, 0.55), (0.65, -D / 2 - 0.02, 2.65), uvl, 1.0, swap=True)
            add_box(bm, (-0.8, -D / 2 - 0.16, 2.65), (0.8, -D / 2 + 0.02, 2.8), uvl, 1.0)
        def roof(bm, uvl):
            o, slope = 0.55, math.radians(28)
            rise = (W / 2 + o) * math.tan(slope)
            for side in (-1, 1):
                L = math.hypot(W / 2 + o, rise)
                M2 = (Matrix.Translation((side * (W / 2 + o) / 2, 0, H + rise / 2 + 0.05)) @
                      Matrix.Rotation(side * slope, 4, "Y"))
                add_box_m(bm, M2, (L + 0.05, D + 2 * o, 0.14), uvl, 1.2)
            add_cyl(bm, (0, -D / 2 - o, H + rise + 0.1), (0, D / 2 + o, H + rise + 0.1), 0.14, segs=12, uvl=uvl,
                    tile=0.8)
        return self.make("Kura", [(M["WhitePlaster"], walls), (M["Stone"], base), (M["Timber"], boards),
                                  (M["Cedar"], door), (M["Tiles"], roof)],
                         ucx=[((-W / 2 - 0.12, -D / 2 - 0.16, -0.6), (W / 2 + 0.12, D / 2 + 0.12, H))])

    def well(self):
        M = self.M

        def ring(bm, uvl):
            add_revolve(bm, [(0.0, -0.5), (0.72, -0.5), (0.72, 0.75), (0.58, 0.75), (0.58, 0.2), (0.0, 0.2)], (0, 0, 0),
                        segs=24, uvl=uvl, tile=0.8)
        def water(bm, uvl):
            add_cyl(bm, (0, 0, 0.2), (0, 0, 0.22), 0.58, segs=24)
        def frame(bm, uvl):
            for s in (-1, 1):
                add_box(bm, (s * 0.85 - 0.06, -0.06, -0.5), (s * 0.85 + 0.06, 0.06, 2.2), uvl, 1.0, swap=True)
            add_box(bm, (-1.05, -0.07, 2.2), (1.05, 0.07, 2.34), uvl, 1.0)
            add_cyl(bm, (-0.12, 0, 2.05), (0.12, 0, 2.05), 0.09, segs=12)
            # small gable roof
            for s in (-1, 1):
                M2 = Matrix.Translation((0, s * 0.42, 2.62)) @ Matrix.Rotation(-s * math.radians(35), 4, "X")
                add_box_m(bm, M2, (2.4, 1.0, 0.05), uvl, 1.0)
        def rope(bm, uvl):
            add_cyl(bm, (0, 0, 2.0), (0, 0, 0.9), 0.008, segs=6)
        def bucket(bm, uvl):
            add_revolve(bm, [(0.0, 0.0), (0.14, 0.0), (0.17, 0.26), (0.0, 0.26)], (0, 0, 0.66), segs=16, uvl=uvl)
        return self.make("Well", [(M["Stone"], ring), (M["Water"], water), (M["Timber"], frame), (M["Rope"], rope),
                                  (M["Cedar"], bucket)],
                         ucx=[((-0.72, -0.72, -0.5), (0.72, 0.72, 0.75)),
                              ((-0.91, -0.06, -0.5), (-0.79, 0.06, 2.2)), ((0.79, -0.06, -0.5), (0.91, 0.06, 2.2))])

    def notice_board(self):
        M = self.M

        def posts(bm, uvl):
            for s in (-1, 1):
                add_box(bm, (s * 0.9 - 0.06, -0.06, -0.5), (s * 0.9 + 0.06, 0.06, 2.3), uvl, 1.0, swap=True)
        def board(bm, uvl):
            add_box(bm, (-1.0, -0.04, 1.1), (1.0, 0.04, 2.05), uvl, 1.0)
            for s in (-1, 1):
                M2 = Matrix.Translation((0, s * 0.2, 2.4)) @ Matrix.Rotation(-s * math.radians(30), 4, "X")
                add_box_m(bm, M2, (2.3, 0.5, 0.04), uvl, 1.0)
        def papers(bm, uvl):
            for i, x in enumerate((-0.6, -0.1, 0.45)):
                add_box(bm, (x - 0.2, -0.05, 1.3 + 0.1 * (i % 2)), (x + 0.2, -0.045, 1.85), uvl, 1.0)
        return self.make("NoticeBoard", [(M["Timber"], posts), (M["Cedar"], board), (M["Paper"], papers)],
                         ucx=[((-0.96, -0.07, -0.5), (0.96, 0.07, 2.3))])

    def fire_tower(self):
        M = self.M
        s, Hh = 1.0, 6.5

        def frame(bm, uvl):
            for sx in (-1, 1):
                for sy in (-1, 1):
                    add_cyl(bm, (sx * s * 1.015, sy * s * 1.015, -0.5), (sx * s * 0.8, sy * s * 0.8, Hh), 0.09,
                            segs=10, uvl=uvl,
                            tile=1.2)
            for z in (1.6, 3.4, 5.2):
                for sx in (-1, 1):
                    k = 1 - 0.2 * z / Hh
                    add_box(bm, (sx * s * k - 0.05, -s * k, z - 0.05), (sx * s * k + 0.05, s * k, z + 0.05), uvl, 1.0,
                            swap=True)
                    add_box(bm, (-s * k, sx * s * k - 0.05, z - 0.05), (s * k, sx * s * k + 0.05, z + 0.05), uvl, 1.0)
            # platform, railing, ladder
            add_box(bm, (-1.0, -1.0, Hh - 0.1), (1.0, 1.0, Hh), uvl, 1.0)
            for sx in (-1, 1):
                add_box(bm, (sx * 0.95 - 0.03, -0.95, Hh + 0.75), (sx * 0.95 + 0.03, 0.95, Hh + 0.8), uvl, 1.0,
                        swap=True)
                add_box(bm, (-0.95, sx * 0.95 - 0.03, Hh + 0.75), (0.95, sx * 0.95 + 0.03, Hh + 0.8), uvl, 1.0)
            for sx in (-0.3, 0.3):
                add_cyl(bm, (sx, -1.227, -0.5), (sx, -0.85, Hh), 0.03, segs=8)
            for k in range(1, 22):
                z = Hh * k / 22
                y = -1.2 + 0.35 * k / 22
                add_cyl(bm, (-0.3, y, z), (0.3, y, z), 0.018, segs=6)
            # roof posts and roof
            for sx in (-1, 1):
                for sy in (-1, 1):
                    add_box(bm, (sx * 0.9 - 0.04, sy * 0.9 - 0.04, Hh), (sx * 0.9 + 0.04, sy * 0.9 + 0.04, Hh + 1.9),
                            uvl, 1.0, swap=True)
        def roof(bm, uvl):
            for sy in (-1, 1):
                M2 = Matrix.Translation((0, sy * 0.62, Hh + 2.2)) @ Matrix.Rotation(-sy * math.radians(32), 4, "X")
                add_box_m(bm, M2, (2.6, 1.55, 0.12), uvl, 1.2)
        def bell(bm, uvl):
            add_revolve(bm, [(0.0, 0.0), (0.2, 0.0), (0.17, 0.12), (0.13, 0.35), (0.06, 0.42), (0.0, 0.44)],
                        (0, 0, Hh + 1.0), segs=20)
        return self.make("FireTower", [(M["Timber"], frame), (M["Thatch"], roof), (M["Bronze"], bell)],
                         ucx=[((sx * 0.95 - 0.12, sy * 0.95 - 0.12, -0.5), (sx * 0.95 + 0.12, sy * 0.95 + 0.12, Hh))
                              for sx in (-1, 1) for sy in (-1, 1)])

    def torii(self):
        M = self.M

        def wood(bm, uvl):
            for s in (-1, 1):
                add_cyl(bm, (s * 1.564, 0, -0.5), (s * 1.45, 0, 3.6), 0.16, 0.14, segs=16, uvl=uvl, tile=1.2)
            add_box(bm, (-1.95, -0.1, 2.8), (1.95, 0.1, 3.0), uvl, 1.2)                       # nuki
            pts = 16
            for i in range(pts):       # kasagi, gently curved up at the ends
                x0 = -2.4 + 4.8 * i / pts
                x1 = -2.4 + 4.8 * (i + 1) / pts
                z0 = 3.6 + 0.12 * (abs(x0) / 2.4) ** 2
                z1 = 3.6 + 0.12 * (abs(x1) / 2.4) ** 2
                add_box(bm, (x0, -0.14, min(z0, z1)), (x1, 0.14, max(z0, z1) + 0.22), uvl, 1.2)
            add_box(bm, (-0.12, -0.08, 3.0), (0.12, 0.08, 3.6), uvl, 1.0)                     # gakuzuka
        def rope(bm, uvl):
            add_cyl(bm, (-1.4, -0.16, 2.5), (1.4, -0.16, 2.5), 0.05, segs=10)
        return self.make("Torii", [(M["Timber"], wood), (M["Rope"], rope)],   # posts only: walk under the torii
                         ucx=[((s * 1.5 - 0.17, -0.17, -0.5), (s * 1.5 + 0.17, 0.17, 3.6)) for s in (-1, 1)])

    def shrine(self):
        M = self.M

        def base(bm, uvl):
            add_box(bm, (-1.8, -2.1, -0.6), (1.8, 1.8, 0.45), uvl, 0.8)
            for i in range(3):
                add_box(bm, (-0.8, -2.1 - 0.35 * (i + 1), -0.6), (0.8, -2.1 - 0.35 * i, 0.45 - 0.15 * (i + 1)), uvl,
                        0.8)
        def hall(bm, uvl):
            for sx in (-1, 1):
                for sy in (-1, 1):
                    add_box(bm, (sx * 1.1 - 0.07, sy * 1.0 - 0.07, 0.45), (sx * 1.1 + 0.07, sy * 1.0 + 0.07, 2.6), uvl,
                            1.0, swap=True)
            add_box(bm, (-1.25, -1.15, 0.95), (1.25, 1.15, 1.02), uvl, 1.0)
            add_box(bm, (-1.1, -0.95, 1.02), (1.1, 1.0, 2.5), uvl, 1.2)
        def doors(bm, uvl):
            add_box(bm, (-0.7, -0.98, 1.1), (0.7, -0.95, 2.3), uvl, 1.0)
        def roof(bm, uvl):
            for sx in (-1, 1):
                M2 = Matrix.Translation((sx * 0.85, 0, 2.95)) @ Matrix.Rotation(sx * math.radians(30), 4, "Y")
                add_box_m(bm, M2, (2.0, 3.2, 0.1), uvl, 1.0)
            add_box(bm, (-0.08, -1.7, 3.35), (0.08, 1.7, 3.5), uvl, 1.0)
        return self.make("Shrine", [(M["Stone"], base), (M["Timber"], hall), (M["Washi"], doors),
                                    (M["Thatch"], roof)],
                         ucx=[((-1.8, -2.1, -0.6), (1.8, 1.8, 0.45)), ((-0.8, -3.15, -0.6), (0.8, -2.1, 0.15)),
                              ((-1.25, -1.15, 0.45), (1.25, 1.15, 2.6))])

    def lantern(self):
        M = self.M

        def stone(bm, uvl):
            add_box(bm, (-0.32, -0.32, -0.4), (0.32, 0.32, 0.18), uvl, 0.6)
            add_cyl(bm, (0, 0, 0.18), (0, 0, 1.0), 0.11, 0.09, segs=12, uvl=uvl, tile=0.6)
            add_box(bm, (-0.26, -0.26, 1.0), (0.26, 0.26, 1.1), uvl, 0.6)
            for s in (-1, 1):  # light box posts (openings between them)
                add_box(bm, (s * 0.18 - 0.05, -0.18, 1.1), (s * 0.18 + 0.05, 0.18, 1.42), uvl, 0.6)
            add_revolve(bm, [(0.0, 1.42), (0.42, 1.42), (0.38, 1.5), (0.12, 1.66), (0.0, 1.68)], (0, 0, 0), segs=6,
                        uvl=uvl, tile=0.6)
            add_revolve(bm, [(0.0, 1.66), (0.07, 1.66), (0.09, 1.74), (0.0, 1.84)], (0, 0, 0), segs=12)
        return self.make("StoneLantern", [(M["Stone"], stone)], ucx=[((-0.32, -0.32, -0.4), (0.32, 0.32, 1.7))])

    # ---------------------------------------------------------------- fences
    def fence(self):
        M = self.M
        L = 2.0

        def poles(bm, uvl):
            n = 14
            for i in range(n):
                x = -L / 2 + L * (i + 0.5) / n
                add_cyl(bm, (x, 0, -0.3), (x, 0, 1.35 + 0.05 * math.sin(i * 1.7)), 0.035, segs=8, uvl=uvl,
                        tile=1.0)
        def ties(bm, uvl):
            for z in (0.35, 1.05):
                add_cyl(bm, (-L / 2, -0.045, z), (L / 2, -0.045, z), 0.022, segs=8)
                add_cyl(bm, (-L / 2, 0.045, z), (L / 2, 0.045, z), 0.022, segs=8)
        return self.make("BambooFence", [(M["Bamboo"], poles), (M["Rope"], ties)],
                         ucx=[((-L / 2, -0.07, -0.3), (L / 2, 0.07, 1.4))])

    def build_all(self):
        for f in (self.kura, self.well, self.notice_board, self.fire_tower, self.torii, self.shrine, self.lantern,
                  self.fence):
            f()
        return self.made
