"""Japanese rural village around the minka: layout, props, pacification zone and NPC spawn markers.

Organisation (traditional mura, street village): eight minka on fenced plots on both sides of a winding earth
road, facades towards the road, storehouses (kura) behind half of them; a small square north of the road with
the shared well, the notice board and the fire watchtower: the centre of the pacification zone, where the royal
flag is planted (cahier §6.2); a path rising north to the shrine (wooden torii, stone lanterns).
No trees, hedges or fields: on the island the villages are generated among the palms and bushes already there
(scripts/map/place_villages.py adapts this kit to the room it finds). This scene is the reference showcase.
NPC spawn points: at each house (doma entrance, engawa), around the well, at the shrine.

Run (background):
  blender --background --factory-startup --python scripts/props/village_japonais.py -- [--render] [--export] [--no-save]
Outputs: art/blender/props/village_japonais.blend, art/_previews/village_*.png,
         art/export/Props/Village/: SM_Minka.fbx and SM_Village_*.fbx (UCX collisions), village_kit.json (bounding
         boxes, ground rectangles, materials for scripts/props/pack_ue_textures.py), village_layout.json (showcase
         positions in cm).
"""
import argparse
import importlib
import json
import math
import random
import sys
from pathlib import Path

import bpy
from mathutils import Matrix, Vector

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import minka_rurale  # noqa: E402
from minka import build, geo  # noqa: E402
from village import props as vprops  # noqa: E402

for _m in (geo, build, vprops):
    importlib.reload(_m)

ROOT = HERE.parents[1]
ART = ROOT / "art"
BLEND = ART / "blender" / "props" / "village_japonais.blend"
PREV = ART / "_previews"
EXPORT = ART / "export" / "Props" / "Village"

ROAD_W = 4.5
NORTH_X = [-58.0, -32.0, 32.0, 58.0]
SOUTH_X = [-46.0, -18.0, 18.0, 46.0]
PLAZA = Vector((0.0, 10.0, 0.0))
ZONE_RADIUS = 75.0


def road_y(x):
    return 4.0 * math.sin(x / 28.0)


def instance(coll, src, name, loc, yaw=0.0):
    e = bpy.data.objects.new(name, None)
    e.instance_type = "COLLECTION"
    e.instance_collection = src
    e.location = loc
    e.rotation_euler = (0, 0, math.radians(yaw))
    coll.objects.link(e)
    return e


def marker(coll, name, loc, kind, size=0.6, **props):
    e = bpy.data.objects.new(name, None)
    e.empty_display_type = "CONE" if kind == "npc" else "CIRCLE" if kind == "zone" else "PLAIN_AXES"
    e.empty_display_size = size
    e.location = loc
    if kind == "npc":
        e.rotation_euler = (math.pi, 0, 0)
    e["marker"] = kind
    for k, v in props.items():
        e[k] = v
    coll.objects.link(e)
    return e


class Village:
    def __init__(self, root, lib, minka_coll, lib_props, M):
        self.root, self.lib, self.minka, self.P, self.M = root, lib, minka_coll, lib_props, M
        self.c = {k: geo.collection(f"Village_{k}", root) for k in
                  ("Maisons", "Kura", "Place", "Sanctuaire", "Clotures", "Sol", "Marqueurs")}
        self.items = []        # placements for Unreal
        self.spawns = []
        self.rnd = random.Random(11)

    def put(self, kind, coll, loc, yaw=0.0, name=None):
        src = self.minka if kind == "Minka" else self.P[kind]
        n = name or f"{kind}_{len(self.items):03d}"
        e = instance(self.c[coll], src, n, loc, yaw)
        self.items.append({"name": n, "asset": "SM_Minka" if kind == "Minka" else f"SM_Village_{kind}",
                           "loc": [round(loc[0], 3), round(loc[1], 3), round(loc[2], 3)], "yaw": round(yaw, 2)})
        return e

    def spawn(self, name, loc, area, role="villager"):
        marker(self.c["Marqueurs"], name, loc, "npc", area=area, role=role)
        self.spawns.append({"name": name, "loc": [round(loc[0], 3), round(loc[1], 3), round(loc[2], 3)],
                            "area": area, "role": role})

    @staticmethod
    def local(origin, yaw, p):
        c, s = math.cos(math.radians(yaw)), math.sin(math.radians(yaw))
        return Vector((origin.x + c * p[0] - s * p[1], origin.y + s * p[0] + c * p[1], 0.0))

    # ------------------------------------------------------------ houses and plots
    def houses(self):
        k = 0
        for side, xs in ((1, NORTH_X), (-1, SOUTH_X)):
            for x in xs:
                yr = road_y(x)
                jitter = self.rnd.uniform(-3.0, 3.0)
                if side > 0:
                    yaw = jitter
                    origin = Vector((x - 5.1, yr + ROAD_W / 2 + 5.5 + 1.2, 0.0))
                else:
                    yaw = 180.0 + jitter
                    origin = Vector((x + 5.1, yr - ROAD_W / 2 - 5.5 - 1.2, 0.0))
                self.put("Minka", "Maisons", origin, yaw, f"Minka_{k:02d}")
                L = lambda p: self.local(origin, yaw, p)
                if k % 2 == 0:
                    self.put("Kura", "Kura", L((12.2, 11.0)), yaw, f"Kura_{k:02d}")
                # front fence with a gate in front of the doma
                for fx in (-3.0, -1.0, 3.2, 5.2, 7.2, 9.2, 11.2, 13.2):
                    self.put("BambooFence", "Clotures", L((fx + 1.0, -4.6)), yaw)
                self.put("StoneLantern", "Clotures", L((-0.6, -5.2)), yaw)
                self.spawn(f"NPC_Maison{k:02d}_Doma", L((1.25, -1.7)), f"maison_{k:02d}")
                self.spawn(f"NPC_Maison{k:02d}_Engawa", L((5.6, -2.4)), f"maison_{k:02d}")
                k += 1

    # ------------------------------------------------------------ square, shrine
    def square(self):
        P = PLAZA
        self.put("Well", "Place", P + Vector((-2.6, 1.0, 0)), 0.0, "Well")
        self.put("NoticeBoard", "Place", P + Vector((3.8, -5.0, 0)), 0.0, "NoticeBoard")
        self.put("FireTower", "Place", P + Vector((8.5, 5.0, 0)), 0.0, "FireTower")
        for a in range(4):
            ang = math.radians(45 + 90 * a)
            self.spawn(f"NPC_Place_{a}", P + Vector((5.5 * math.cos(ang), 5.5 * math.sin(ang), 0)), "place")
        for sx in (-1, 1):
            self.put("StoneLantern", "Place", P + Vector((sx * 6.5, -7.0, 0)), 0.0)

    def shrine(self):
        self.put("Torii", "Sanctuaire", Vector((0.0, 26.0, 0.0)), 0.0, "Torii")
        for y in (31.0, 38.0):
            for sx in (-1, 1):
                self.put("StoneLantern", "Sanctuaire", Vector((sx * 2.3, y, 0.0)), 0.0)
        self.put("Shrine", "Sanctuaire", Vector((0.0, 47.0, 0.0)), 0.0, "Shrine")
        self.spawn("NPC_Sanctuaire_0", Vector((-2.0, 42.0, 0.0)), "sanctuaire")
        self.spawn("NPC_Sanctuaire_1", Vector((2.0, 42.0, 0.0)), "sanctuaire")

    def zone(self):
        z = marker(self.c["Marqueurs"], "PacificationZone_VillageA", PLAZA, "zone", size=ZONE_RADIUS,
                   radius_m=ZONE_RADIUS, flag="Flag_VillageA")
        z.rotation_euler = (0, 0, 0)
        marker(self.c["Marqueurs"], "Flag_VillageA", PLAZA, "flag", size=1.5)
        # visible flag for the presentation (royal standard placeholder)
        import bmesh
        bm = bmesh.new()
        uvl = geo.uv_layer(bm)
        geo.add_cyl(bm, PLAZA, PLAZA + Vector((0, 0, 4.5)), 0.05, segs=10)
        geo.obj("PRES_FlagPole", bm, self.M["Timber"], self.c["Marqueurs"])
        bm = bmesh.new()
        geo.add_box(bm, PLAZA + Vector((0.05, -0.01, 3.3)), PLAZA + Vector((1.85, 0.01, 4.4)))
        flag = geo.flat_material("M_PRES_RoyalFlag", "B8202A", rough=0.8)
        geo.obj("PRES_FlagCloth", bm, flag, self.c["Marqueurs"])

    def ground(self):
        import bmesh
        bm = bmesh.new()
        uvl = geo.uv_layer(bm)
        geo.add_box(bm, (-110, -80, -0.2), (110, 90, -0.02), uvl, 2.5)
        geo.obj("PRES_Ground", bm, self.M["Grass"], self.c["Sol"])
        # earth road (strip following the curve) and the square / shrine path
        bm = bmesh.new()
        uvl = geo.uv_layer(bm)
        n = 120
        rows = []
        for i in range(n + 1):
            x = -90 + 180 * i / n
            y = road_y(x)
            dy = 4.0 / 28.0 * math.cos(x / 28.0)
            nrm = Vector((-dy, 1.0, 0.0)).normalized()
            rows.append([bm.verts.new(Vector((x, y, 0.0)) + nrm * (s * ROAD_W / 2)) for s in (-1, 1)])
        for i in range(n):
            bm.faces.new((rows[i][0], rows[i + 1][0], rows[i + 1][1], rows[i][1]))
        geo.add_box(bm, (-1.6, 2.0, -0.01), (1.6, 46.0, 0.0), None)
        bmesh.ops.create_circle(bm, cap_ends=True, radius=9.5, segments=40,
                                matrix=Matrix.Translation((PLAZA.x, PLAZA.y, 0.005)))
        bm.normal_update()
        geo.box_uv(bm.faces[:], uvl, 2.0)
        geo.obj("PRES_Road", bm, self.M["Earth"], self.c["Sol"])

    def build(self):
        self.ground()
        self.houses()
        self.square()
        self.shrine()
        self.zone()
        return self


def main():
    argv = sys.argv[sys.argv.index("--") + 1:] if "--" in sys.argv else []
    ap = argparse.ArgumentParser()
    ap.add_argument("--render", action="store_true")
    ap.add_argument("--export", action="store_true")
    ap.add_argument("--no-save", action="store_true")
    a = ap.parse_args(argv)

    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.name = "Village_Japonais"
    scene.unit_settings.system = "METRIC"
    M = geo.materials()
    vprops.village_materials(M)
    c, n, r = (geo.find(geo.MINKA_TEX / "leafy_grass", k) for k in ("_diff_", "_nor_gl_", "_rough_"))
    M["Grass"] = geo.img_material("M_Village_Ground", c, n, r, tint_hex="4E5A34")

    lib = bpy.data.collections.new("VILLAGE_LIB")
    scene.collection.children.link(lib)
    minka_root = bpy.data.collections.new("LIB_Minka")
    lib.children.link(minka_root)
    mk = build.Minka(minka_root, M).build(minka_rurale.import_model)
    for name in ("Minka_Collisions", "Minka_Presentation"):
        cc = bpy.data.collections.get(name)
        if cc and cc.name in minka_root.children:
            minka_root.children.unlink(cc)
    props = vprops.Props(M, lib)
    props_lib = props.build_all()
    props_lib_ucx = props.ucx
    bpy.context.view_layer.layer_collection.children["VILLAGE_LIB"].exclude = True

    root = geo.collection("VILLAGE_A")
    v = Village(root, lib, minka_root, props_lib, M).build()

    # light and cameras
    minka_rurale.studio(scene).hide_render = True
    world = scene.world
    bg = next(n for n in world.node_tree.nodes if n.type == "BACKGROUND")
    bg.inputs[0].default_value = (0.62, 0.74, 0.9, 1)
    bg.inputs[1].default_value = 0.9
    cams = {
        "vue_aerienne": minka_rurale.camera("CAM_Village_Aerial", (-88.0, -105.0, 62.0), (2.0, 4.0, 0.0), 34),
        "rue": minka_rurale.camera("CAM_Village_Rue", (-40.0, -1.5, 1.7), (10.0, 4.0, 2.0), 30),
        "plan": minka_rurale.camera("CAM_Village_Plan", (0.0, 4.0, 200.0), (0.0, 4.0, 0.0), ortho=175.0),
        "place": minka_rurale.camera("CAM_Village_Place", (-14.0, -14.0, 9.0), (2.0, 14.0, 2.0), 26),
    }
    cams["plan"].rotation_euler = (0, 0, 0)
    scene.camera = cams["vue_aerienne"]
    print(f"[village] {len(v.items)} placements, {len(v.spawns)} NPC spawns, zone radius {ZONE_RADIUS} m")

    if a.render:
        for key, cam in cams.items():
            minka_rurale.render(scene, cam, PREV / f"village_{key}.png", (1920, 1080))
        print("[village] previews in", PREV)

    if a.export:
        EXPORT.mkdir(parents=True, exist_ok=True)
        for old in ("Hedge", "HasaRack", "Paddy", "Pine", "Cedar"):       # removed from the kit
            f = EXPORT / f"SM_Village_{old}.fbx"
            if f.exists():
                f.unlink()
        minka_rurale.EXPORTED.clear()
        kit = {"units": "meters, Blender local frame (front -Y, pivot on the ground); in Unreal (x, -y, z) * 100 cm",
               "assets": {}}
        kit["assets"]["SM_Minka"] = minka_rurale.export_house(mk, EXPORT)
        tris = {"SM_Minka": kit["assets"]["SM_Minka"]["tris"]}
        for name, coll in props_lib.items():
            asset = f"SM_Village_{name}"
            objs = [o for o in coll.all_objects if o.type == "MESH"]
            ucx = geo.ucx_boxes(asset, props_lib_ucx[name])
            _, n = minka_rurale.export_group(objs, asset, extra=ucx, out_dir=EXPORT)
            for o in ucx:
                bpy.data.objects.remove(o)
            vs = [o.matrix_world @ v.co for o in objs for v in o.data.vertices]
            kit["assets"][asset] = {"tris": n, "ucx": len(props_lib_ucx[name]),
                                    "bbox": [[round(min(v[i] for v in vs), 3) for i in range(3)],
                                             [round(max(v[i] for v in vs), 3) for i in range(3)]],
                                    "ground": [[round(b[0][0], 3), round(b[0][1], 3), round(b[1][0], 3),
                                                round(b[1][1], 3)] for b in props_lib_ucx[name]]}
            tris[asset] = n
        kit["materials"] = geo.material_manifest(minka_rurale.EXPORTED.values())
        (EXPORT / "village_kit.json").write_text(json.dumps(kit, indent=1), encoding="utf-8")
        layout = {
            "village": "VillageA",
            "units": "cm and degrees in Blender axes (+Y north); Unreal: (x, -y, z) and yaw -> -yaw",
            "origin": "village square = pacification zone centre is at PLAZA",
            "zone": {"name": "PacificationZone_VillageA", "center": [PLAZA.x * 100, PLAZA.y * 100, 0],
                     "radius_cm": ZONE_RADIUS * 100, "flag": [PLAZA.x * 100, PLAZA.y * 100, 0]},
            "items": [dict(it, loc=[round(c * 100, 1) for c in it["loc"]]) for it in v.items],
            "npc_spawns": [dict(sp, loc=[round(c * 100, 1) for c in sp["loc"]]) for sp in v.spawns],
            "tris": tris,
        }
        (EXPORT / "village_layout.json").write_text(json.dumps(layout, indent=1), encoding="utf-8")
        print("[village] export:", EXPORT, json.dumps(tris), f"{len(kit['materials'])} materials")

    if not a.no_save:
        BLEND.parent.mkdir(parents=True, exist_ok=True)
        bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), compress=True)
        print("[village] saved", BLEND)


if __name__ == "__main__":
    main()
