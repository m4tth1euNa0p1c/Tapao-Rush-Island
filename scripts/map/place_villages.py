"""Generate the Japanese villages of the island where there is room: the pacification zones of cahier §6.2, where
the NPCs spawn. The map is searched chunk by chunk, each village adapts to the ground and to the objects already
on the map, and the result is remembered.

Usage:
  python scripts/map/place_villages.py raraka [--count 3] [--seed 7] [--reset] [--regen V02]
         [--min-dist 250] [--max-dist 2000] [--spacing 350] [--max-houses 12] [--reclear] [--extras]
         [--apply-clearing] [--no-csv]

How a village is made (a mura, street village, from the kit of scripts/props/village_japonais.py):
  - site: a chunk (CHUNK_M grid) with a wide, flat piece of land, far from the shore and from the real buildings
    (the village of Motutapu stays untouched), not too far from the PlayerStart; candidates are scored by a full
    trial layout;
  - axis: the long axis of the motu (PCA of the land around); an earth road follows its ridge (away from the shore);
  - square on the deeper side of the road: well, notice board, fire watchtower, stone lanterns, and the flag of
    the zone in its centre; a path goes on to the shrine (torii, lanterns);
  - plots along the road on both sides: minka facing the road, fence and lantern at the gate, a kura behind (every
    house in the food village, one out of two elsewhere); each plot slides along the road and back to dodge the
    palm trunks already there, and is dropped when it does not fit (land, shore, slope, real buildings, actors):
    the village grows as big as its site allows;
  - what is in the way is cleared: everything under the buildings (eaves included), the square, the road and the
    path, plus whatever would touch them, by plant (village_clearing.MARGIN_CM: 3 m for a bush or a palm around
    a house, 1 m for a fern...); every other palm and bush stays: the village lives in the grove that was there.
Routine and lights (--extras, also computed for every new village): points of interest where the villagers do
  something with the decor (draw water at the well, read the notice board, pray at the shrine, rest in front of an
  engawa, sweep a yard, fetch goods at a storehouse door, watch from the fire tower, chat around the square) and the
  lights of the village (lamps by the irori and the andon of each house, a glow in each stone lantern), placed in
  the editor as TargetPoints (VillagePOI) and point lights (VillageLight), see docs/features/villages-japonais.md.
Themes (cahier §6.2, "each zone opens a concrete interest"): richesses (Takara-mura, nearest to a lagoon cove),
  bois (Mori-mura, the most wooded site), vivres (Kura-mura, one storehouse per house).

Inputs (docs/reference/map/<ile>/): fields of place_vegetation.py (land, distance to the shore, water side, real
  buildings, terrain surface of the tiles), the vegetation on the map (raw/<ile>_vegetation/DT_*.csv, original
  copies in _orig/), level actors (villages/level_actors.json, exported from the editor, optional), the kit
  art/export/Props/Village/village_kit.json.
Memory: villages/villages.json (tracked): chunk grid with what each chunk can hold, and every village (site,
  axis, theme, items, NPC spawns, zone, clearing shapes). A village in memory is kept as is; a new run only
  fills the chunks without one (--regen <id> rebuilds one, --reset forgets everything, --reclear recomputes the
  clearing shapes of the villages in memory from their items without moving anything).
Outputs: villages/villages.json, villages/preview_villages.png,
  raw/<ile>_villages/DT_<Ile>_Village_<Id>.csv (GameplayTagTableRow, one DataTable = one PCG actor per village),
  --apply-clearing: raw/<ile>_vegetation/DT_*.csv rewritten without the instances under the villages
  (scripts/map/village_clearing.py), villages/clearing_report.json (the DataTables to reimport).
"""
import argparse
import csv
import importlib.util
import json
import math
import zlib
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi
from scipy.spatial import cKDTree

ROOT = Path(__file__).resolve().parents[2]


def _load(name, path):
    spec = importlib.util.spec_from_file_location(name, path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


pv = _load("place_vegetation", ROOT / "scripts" / "map" / "place_vegetation.py")
vc = _load("village_clearing", ROOT / "scripts" / "map" / "village_clearing.py")

KIT = ROOT / "art" / "export" / "Props" / "Village" / "village_kit.json"
UE_MESHES = "/Game/MonkeIsland/Environment/Village/Meshes"
DEFAULT_START_CM = (-653334.7, -1013400.0)
CHUNK_M = 256.0
ROAD_HALF = 2.25            # earth road, 4.5 m
ROAD_MIN_SHORE = 12.0       # the road stays this far from the water (m)
FACADE = 7.85               # road axis -> facade: half road, 1 m verge, fence, front yard, engawa (m)
LOT_S = (15.0, 37.0, 59.0, 81.0)       # plots along the road on each side (m from the square)
PLAZA_R = 11.0
SITE_MIN_SHORE = 24.0       # a site centre is at least this far from the water: motu >= ~50 m wide
BLDG_MIN = 250.0            # site centre -> real buildings (m): Motutapu, a real village, stays apart
MIN_HOUSES = 4
NEAR_M, FAR_PENALTY = 600.0, 0.015   # prefer sites within reach of the start (score per m beyond)
THEMES = {"richesses": ("Takara-mura", "Richesses et crique d'extraction"),
          "bois": ("Mori-mura", "Bois et passage court"),
          "vivres": ("Kura-mura", "Nourriture et abri")}
# asset: (min distance to the water m, max ground difference m, clearing margin m, kind)
SPEC = {"SM_Minka": (10.0, 0.7, 1.5, "building"),
        "SM_Village_Kura": (8.0, 0.5, 1.2, "building"),
        "SM_Village_Shrine": (8.0, 0.5, 1.2, "building"),
        "SM_Village_Torii": (5.0, 0.45, 0.6, "building"),
        "SM_Village_Well": (6.0, 0.45, 0.8, "building"),
        "SM_Village_FireTower": (6.0, 0.45, 0.8, "building"),
        "SM_Village_NoticeBoard": (5.0, 0.45, 0.6, "building"),
        "SM_Village_StoneLantern": (4.0, 0.35, 0.5, "small"),
        "SM_Village_BambooFence": (4.0, 0.25, 0.4, "small")}
# weight of an instance already on the map that a footprint would remove (palm trunk = 1)
WEIGHTS = (("SM_CoconutPalm_Juvenile", 0.05), ("SM_CoconutPalm_Sprout", 0.0), ("SM_CoconutPalm_Fallen", 0.4),
           ("SM_CoconutPalm_", 1.0), ("SM_Pandanus", 0.6), ("SM_MikiMiki", 0.5), ("SM_Tahinu", 0.6),
           ("SM_CoralBlock", 0.3), ("SM_Driftwood", 0.2), ("SM_Coconut_", 0.02), ("SM_Naupaka", 0.06),
           ("SM_Fern", 0.01), ("SM_Grass", 0.0), ("SM_CoconutFrond", 0.0))


# ---------------------------------------------------------------------------------------------- geometry

def rot(yaw):
    a = math.radians(yaw)
    c, s = math.cos(a), math.sin(a)
    return np.array([[c, -s], [s, c]])


def to_world(origin, yaw, pts):
    return np.asarray(origin, float) + np.asarray(pts, float).reshape(-1, 2) @ rot(yaw).T


def to_local(origin, yaw, pts):
    return (np.asarray(pts, float).reshape(-1, 2) - np.asarray(origin, float)) @ rot(yaw)


def facing(f):
    """UE yaw that turns the front of a kit mesh (local +Y in Unreal) towards direction f."""
    return math.degrees(math.atan2(-f[0], f[1]))


def unit(v):
    v = np.asarray(v, float)
    return v / max(np.linalg.norm(v), 1e-9)


def perp(v):
    return np.array([-v[1], v[0]])


def rect_grid(rect, step):
    x0, y0, x1, y1 = rect
    nx = max(2, int(math.ceil((x1 - x0) / step)) + 1)
    ny = max(2, int(math.ceil((y1 - y0) / step)) + 1)
    X, Y = np.meshgrid(np.linspace(x0, x1, nx), np.linspace(y0, y1, ny))
    return np.c_[X.ravel(), Y.ravel()]


def corners(rect):
    x0, y0, x1, y1 = rect
    return np.array([[x0, y0], [x1, y0], [x1, y1], [x0, y1]], float)


def overlap(Pa, Pb, gap):
    """Separating axis test between two convex quads (world corners), with a clearance gap."""
    for P in (Pa, Pb):
        for i in range(4):
            ax = unit(perp(P[(i + 1) % 4] - P[i]))
            a, b = Pa @ ax, Pb @ ax
            if a.max() + gap <= b.min() or b.max() + gap <= a.min():
                return False
    return True


def seg_dist(P, pts):
    """Distance from points P (n,2) to a polyline pts (m,2)."""
    P = np.asarray(P, float).reshape(-1, 2)
    d = np.full(len(P), np.inf)
    for a, b in zip(pts[:-1], pts[1:]):
        ab = b - a
        t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
        d = np.minimum(d, np.linalg.norm(P - (a + t[:, None] * ab), axis=1))
    return d


def clear_rect(asset, rect, origin, yaw):
    """Clearing rectangle of an item: its bounding box (eaves included), no margin; kind building / small."""
    x0, y0, x1, y1 = rect
    cx, cy = to_world(origin, yaw, [((x0 + x1) / 2, (y0 + y1) / 2)])[0]
    return {"type": "rect", "kind": "building" if SPEC[asset][3] == "building" else "small",
            "center": [round(cx * 100, 1), round(cy * 100, 1)],
            "half": [round((x1 - x0) / 2 * 100, 1), round((y1 - y0) / 2 * 100, 1)], "yaw": round(yaw, 2)}


def reclear(v, kit):
    """Clearing shapes of a village in memory, rebuilt from its items, road and square (nothing moves)."""
    shapes = []
    for it in v["items"]:
        o = np.array(it["loc_cm"][:2]) / 100.0
        shapes.append(clear_rect(it["asset"], kit.rect[it["asset"]], o, it["yaw"]))
    c = np.array(v["zone"]["center_cm"][:2]) / 100.0
    shapes.append({"type": "circle", "kind": "plaza", "center": [round(c[0] * 100, 1), round(c[1] * 100, 1)],
                   "radius": round(PLAZA_R * 100, 1)})
    shapes.append({"type": "path", "kind": "road", "points": v["road_cm"], "half_width": ROAD_HALF * 100})
    shrine = next((it for it in v["items"] if it["asset"] == "SM_Village_Shrine"), None)
    torii = next((it for it in v["items"] if it["asset"] == "SM_Village_Torii"), None)
    if shrine and torii:
        sp = np.array(shrine["loc_cm"][:2]) / 100.0
        dv = unit(sp - c)
        a, b = c + dv * (PLAZA_R - 1.0), sp - dv * 3.4
        shapes.append({"type": "path", "kind": "path", "half_width": 160.0,
                       "points": [[round(a[0] * 100, 1), round(a[1] * 100, 1)], [round(b[0] * 100, 1), round(b[1] * 100, 1)]]})
    return shapes


# points of interest by asset: (activity, local point, local point it faces), Unreal local frame (front +Y), meters
POI_KIT = {
    "SM_Village_Well": [("work", (0.0, 1.5), (0.0, 0.0))],
    "SM_Village_NoticeBoard": [("read", (0.0, 1.6), (0.0, 0.0))],
    "SM_Village_Shrine": [("pray", (-0.7, 4.3), (-0.2, 0.0)), ("pray", (0.7, 4.3), (0.2, 0.0))],
    "SM_Village_FireTower": [("watch", (0.0, 2.4), (0.0, 0.0))],
    "SM_Village_Kura": [("work", (0.0, 3.9), (0.0, 0.0))],
    # outside the front fence (4.6 m in front of the facade), on the road verge: sweeping at the gate, sitting
    # in front of the engawa looking at the road (the villagers steer straight, no path through the gate yet)
    "SM_Minka": [("work", (0.35, 5.25), (0.35, 0.0)), ("rest", (8.0, 5.25), (8.0, 12.0))],
}
POI_CLEARANCE = 0.45      # capsule radius + margin when testing that a point of interest is reachable (m)
# lights by asset: (kind, local point, height above the item base, candela, attenuation radius cm, shadows)
LIGHT_KIT = {
    "SM_Minka": [("interior", (3.6, -1.8), 1.25, 25.0, 650.0, True)],     # above the irori (room B), seen through the open front
    "SM_Village_StoneLantern": [("lantern", (0.0, 0.0), 1.26, 6.0, 450.0, False)],
}
# door of each house and gate of its plot (local m, Unreal frame, +Y = front): when a villager is killed, the others
# run home through the gate (outside the fence, which stands 4.6 m in front of the facade) to the doma door, at the
# left end of the facade (the engawa starts at x = 1.8), and hide inside
DOOR_KIT = {"SM_Minka": ((0.9, 0.6), (0.35, 5.7))}


def _quads(it, kit, grow):
    """What blocks a walker: the ground footprints of the item (posts of the torii, body and engawa of a house...),
    not its bounding box (one walks under a torii beam or an eave)."""
    o = np.array(it["loc_cm"][:2]) / 100.0
    return [to_world(o, it["yaw"], corners((x0 - grow, y0 - grow, x1 + grow, y1 + grow)))
            for x0, y0, x1, y1 in kit.ground[it["asset"]]]


def _segment_hits_quad(a, b, Q):
    """True when the segment a-b crosses or enters the convex quad Q (4 corners)."""
    def inside(p):
        sgn = [np.cross(Q[(i + 1) % 4] - Q[i], p - Q[i]) for i in range(4)]
        return all(x >= 0 for x in sgn) or all(x <= 0 for x in sgn)

    def cross(p1, p2, q1, q2):
        d1, d2 = np.cross(p2 - p1, q1 - p1), np.cross(p2 - p1, q2 - p1)
        d3, d4 = np.cross(q2 - q1, p1 - q1), np.cross(q2 - q1, p2 - q1)
        return d1 * d2 < 0 and d3 * d4 < 0
    if inside(a) or inside(b):
        return True
    return any(cross(a, b, Q[i], Q[(i + 1) % 4]) for i in range(4))


def poi_reachable(w, own, v, kit):
    """A villager steers straight (no NavMesh): the point must be reachable from the nearest point of the road
    without crossing a building, a fence or a lantern."""
    road = np.array(v["road_cm"], float) / 100.0
    a = np.asarray(w, float)
    starts = [seg_nearest(road, a), np.array(v["zone"]["center_cm"][:2], float) / 100.0]
    for start in starts:
        if not any(_segment_hits_quad(start, a, Q) for it in v["items"] if it is not own
                   for Q in _quads(it, kit, POI_CLEARANCE)):
            return True
    return False


def seg_nearest(pts, p):
    best, bd = pts[0], 1e18
    for a, b in zip(pts[:-1], pts[1:]):
        ab = b - a
        t = np.clip(((p - a) @ ab) / max(ab @ ab, 1e-9), 0.0, 1.0)
        q = a + t * ab
        d = float(((p - q) ** 2).sum())
        if d < bd:
            best, bd = q, d
    return best


def village_doors(v, G):
    """Doors of the houses of a village (UE cm): where the marker stands, its yaw towards the gate, the gate distance."""
    doors = []
    for it in v["items"]:
        spec = DOOR_KIT.get(it["asset"])
        if not spec:
            continue
        o = np.array(it["loc_cm"][:2]) / 100.0
        door, gate = to_world(o, it["yaw"], [spec[0], spec[1]])
        d = gate - door
        k = len(doors)
        doors.append({"name": f"{v['id']}_Porte{k:02d}", "area": f"maison_{k:02d}",
                      "loc_cm": [round(door[0] * 100, 1), round(door[1] * 100, 1), round(float(G.z([door])[0]) * 100 + 10, 1)],
                      "yaw": round(math.degrees(math.atan2(d[1], d[0])), 1),
                      "gate_cm": round(float(np.linalg.norm(d)) * 100, 1)})
    return doors


def village_extras(v, G, kit):
    """Points of interest and lights of a village, from its items (UE cm, yaw in degrees)."""
    pois, lights = [], []
    count = {}
    for it in v["items"]:
        o = np.array(it["loc_cm"][:2]) / 100.0
        base = it["loc_cm"][2] / 100.0
        for activity, at, look in POI_KIT.get(it["asset"], []):
            w, target = to_world(o, it["yaw"], [at, look])
            if G.shore([w])[0] < 3.0 or not poi_reachable(w, it, v, kit):
                continue
            d = target - w
            k = count[activity] = count.get(activity, 0) + 1
            pois.append({"name": f"{v['id']}_Poi_{activity}_{k:02d}", "activity": activity,
                         "loc_cm": [round(w[0] * 100, 1), round(w[1] * 100, 1), round(float(G.z([w])[0]) * 100 + 10, 1)],
                         "yaw": round(math.degrees(math.atan2(d[1], d[0])), 1)})
        for kind, at, h, cd, radius, shadows in LIGHT_KIT.get(it["asset"], []):
            w = to_world(o, it["yaw"], [at])[0]
            k = count["light_" + kind] = count.get("light_" + kind, 0) + 1
            lights.append({"name": f"{v['id']}_Light_{kind}_{k:02d}", "kind": kind,
                           "loc_cm": [round(w[0] * 100, 1), round(w[1] * 100, 1), round((base + h) * 100, 1)],
                           "candela": cd, "radius_cm": radius, "shadows": shadows})
    # chat around the square: 4 places 2.8 m from its centre, clear of the well, the board and the tower
    c = np.array(v["zone"]["center_cm"][:2]) / 100.0
    others = [np.array(it["loc_cm"][:2]) / 100.0 for it in v["items"] if np.linalg.norm(np.array(it["loc_cm"][:2]) / 100.0 - c) < 15.0]
    k = 0
    for a in np.linspace(0.0, 2.0 * math.pi, 16, endpoint=False):
        w = c + 2.8 * np.array([math.cos(a), math.sin(a)])
        if k >= 4 or any(np.linalg.norm(w - q) < 2.2 for q in others) or any(
                np.linalg.norm(w - np.array(pp["loc_cm"][:2]) / 100.0) < 1.6 for pp in pois if pp["activity"] == "chat"):
            continue
        k += 1
        d = c - w
        pois.append({"name": f"{v['id']}_Poi_chat_{k:02d}", "activity": "chat",
                     "loc_cm": [round(w[0] * 100, 1), round(w[1] * 100, 1), round(float(G.z([w])[0]) * 100 + 10, 1)],
                     "yaw": round(math.degrees(math.atan2(d[1], d[0])), 1)})
    return pois, lights


def clearing_stats(v, objs):
    """Instances of the map the village clears (all plants, and palm trunks)."""
    m = vc.Clearing(v["clear"]).mask(objs.xy * 100.0, objs.cat)
    return int(m.sum()), int((m & (objs.cat == "palm")).sum())


# ---------------------------------------------------------------------------------------------- inputs

class Kit:
    """Bounding boxes and ground rectangles of the kit, converted to the Unreal local frame (y -> -y)."""

    def __init__(self, path):
        d = json.loads(path.read_text(encoding="utf-8"))
        self.rect, self.ground, self.height = {}, {}, {}
        for name, a in d["assets"].items():
            (x0, y0, _), (x1, y1, z1) = a["bbox"]
            self.rect[name] = (x0, -y1, x1, -y0)
            self.ground[name] = [(g[0], -g[3], g[2], -g[1]) for g in a["ground"]]
            self.height[name] = z1


class Ground:
    def __init__(self, A):
        self.A, self.F = A, A.F
        self.shore_img = A.d_shore.astype(np.float32)
        self.shore_smooth = ndi.gaussian_filter(self.shore_img, 2.0)
        self.bldg_img = A.d_bldg.astype(np.float32)
        self.lagoon_img = A.lagoon_side.astype(np.float32)

    def shore(self, P, smooth=False):
        P = np.asarray(P, float).reshape(-1, 2)
        return self.F.sample(self.shore_smooth if smooth else self.shore_img, P[:, 0], P[:, 1])

    def bldg(self, P):
        P = np.asarray(P, float).reshape(-1, 2)
        return self.F.sample(self.bldg_img, P[:, 0], P[:, 1])

    def lagoon(self, P):
        P = np.asarray(P, float).reshape(-1, 2)
        return self.F.sample(self.lagoon_img, P[:, 0], P[:, 1])

    def z(self, P):
        P = np.asarray(P, float).reshape(-1, 2)
        return self.F.mesh_z(P[:, 0], P[:, 1])


class Objects:
    """Instances already on the map (palms + vegetation CSVs, unfiltered copies when they exist), in meters."""

    def __init__(self, island):
        raw = ROOT / "docs" / "reference" / "map" / "raw" / f"{island}_vegetation"
        src = raw / "_orig" if (raw / "_orig").exists() else raw
        xy, w, mesh_ids, self.meshes = [], [], [], []
        cache = {}
        for f in sorted(src.glob("DT_*.csv")):
            with open(f, newline="", encoding="utf-8") as fh:
                r = csv.reader(fh)
                next(r)
                for row in r:
                    m = row[1].rsplit("/", 1)[-1].split(".")[0]
                    if m not in cache:
                        cache[m] = (len(self.meshes), next((v for k, v in WEIGHTS if m.startswith(k)),
                                                          0.03 if "_Soft_" in f.name else 0.3))
                        self.meshes.append(m)
                    i, wt = cache[m]
                    x, y = row[2].split("|", 1)[0].split(",")[:2]
                    xy.append((float(x) / 100.0, float(y) / 100.0))
                    w.append(wt)
                    mesh_ids.append(i)
        self.xy = np.asarray(xy, float)
        self.w = np.asarray(w, np.float32)
        self.mesh = np.asarray(mesh_ids, np.int32)
        self.cat = np.array([vc.category(m) for m in self.meshes])[self.mesh] if len(self.mesh) else np.zeros(0, str)
        self.tree = cKDTree(self.xy)
        self.trunk_tree = cKDTree(self.xy[self.w >= 0.4]) if (self.w >= 0.4).any() else None
        print(f"[villages] {len(self.xy)} instances already on the map ({src.relative_to(ROOT)})")

    def in_rect(self, origin, yaw, rect, margin):
        x0, y0, x1, y1 = rect
        c = to_world(origin, yaw, [((x0 + x1) / 2, (y0 + y1) / 2)])[0]
        r = math.hypot(x1 - x0, y1 - y0) / 2 + margin
        idx = np.asarray(self.tree.query_ball_point(c, r), int)
        if not len(idx):
            return idx
        L = to_local(origin, yaw, self.xy[idx])
        ok = (L[:, 0] >= x0 - margin) & (L[:, 0] <= x1 + margin) & (L[:, 1] >= y0 - margin) & (L[:, 1] <= y1 + margin)
        return idx[ok]

    def in_disc(self, c, r):
        return np.asarray(self.tree.query_ball_point(np.asarray(c, float), r), int)

    def near_path(self, pts, half):
        pts = np.asarray(pts, float)
        lo, hi = pts.min(0) - half, pts.max(0) + half
        idx = np.asarray(self.tree.query_ball_point((lo + hi) / 2, np.linalg.norm(hi - lo) / 2), int)
        if not len(idx):
            return idx
        return idx[seg_dist(self.xy[idx], pts) <= half]


def load_actors(ref):
    path = ref / "villages" / "level_actors.json"
    if not path.exists():
        return [], None
    doc = json.loads(path.read_text(encoding="utf-8"))
    acts, start = [], None
    for a in doc["actors"]:
        x, y = a["loc_cm"][0] / 100.0, a["loc_cm"][1] / 100.0
        if a["class"].startswith("PlayerStart"):
            start = np.array([x, y])
        acts.append((np.array([x, y]), a.get("radius_cm", 300.0) / 100.0, a["label"]))
    return acts, start


# ---------------------------------------------------------------------------------------------- road

class Road:
    def __init__(self, pts, i0):
        self.pts = np.asarray(pts, float)
        seg = np.linalg.norm(np.diff(self.pts, axis=0), axis=1)
        self.s = np.r_[0.0, np.cumsum(seg)]
        self.s -= self.s[i0]

    @property
    def smin(self):
        return float(self.s[0])

    @property
    def smax(self):
        return float(self.s[-1])

    def at(self, s):
        s = float(np.clip(s, self.s[0], self.s[-1]))
        k = int(np.clip(np.searchsorted(self.s, s) - 1, 0, len(self.s) - 2))
        a, b = self.pts[k], self.pts[k + 1]
        t = (s - self.s[k]) / max(self.s[k + 1] - self.s[k], 1e-9)
        T = unit(b - a)
        return a + t * (b - a), T, perp(T)

    def trimmed(self, s0, s1):
        keep = (self.s > s0) & (self.s < s1)
        pts = [self.at(s0)[0]] + list(self.pts[keep]) + [self.at(s1)[0]]
        return np.asarray(pts)


def trace_road(G, c, t, half_len, step=6.0, max_turn=10.0, max_dev=30.0):
    """Earth road from the centre along +-t, following the ridge of the motu (far from the water), turning at
    most max_turn per step and never more than max_dev away from the axis (no hairpin on a wide motu)."""
    def run(sign):
        pts, p, d = [], np.asarray(c, float), sign * t
        base = sign * t
        for _ in range(int(half_len / step)):
            q = p + d * step
            n = perp(d)
            offs = np.linspace(-4.0, 4.0, 9)
            cand = q[None] + offs[:, None] * n[None]
            sc = G.shore(cand, smooth=True) - 0.15 * np.abs(offs)
            target = cand[int(np.argmax(sc))]
            nd = unit(target - p)
            ang = math.degrees(math.atan2(d[0] * nd[1] - d[1] * nd[0], d @ nd))
            ang = max(-max_turn, min(max_turn, ang))
            d = unit(rot(ang) @ d)
            dev = math.degrees(math.atan2(base[0] * d[1] - base[1] * d[0], base @ d))
            if abs(dev) > max_dev:
                d = unit(rot(math.copysign(max_dev, dev)) @ base)
            q = p + d * step
            if G.shore([q])[0] < ROAD_MIN_SHORE:
                break
            pts.append(q)
            p = q
        return pts

    fwd, bwd = run(1.0), run(-1.0)
    return Road(bwd[::-1] + [np.asarray(c, float)] + fwd, len(bwd))


def motu_axis(G, c, radius=150.0):
    F = G.F
    m = F.m_px
    col = (c[0] + F.cx) / m - 0.5
    row = (c[1] + F.cz) / m - 0.5
    r = int(radius / m)
    r0, r1 = int(max(0, row - r)), int(min(F.H, row + r + 1))
    c0, c1 = int(max(0, col - r)), int(min(F.W, col + r + 1))
    win = G.A.land[r0:r1, c0:c1]
    rr, cc = np.nonzero(win)
    if len(rr) < 10:
        return np.array([1.0, 0.0])
    P = np.c_[(cc + c0 + 0.5) * m - F.cx, (rr + r0 + 0.5) * m - F.cz]
    P = P[np.linalg.norm(P - c, axis=1) <= radius]
    w, v = np.linalg.eigh(np.cov((P - P.mean(0)).T))
    return unit(v[:, int(np.argmax(w))])


# ---------------------------------------------------------------------------------------------- layout

class Layout:
    def __init__(self, ctx, center, axis, theme, rng, max_houses):
        self.ctx, self.G, self.kit, self.objs = ctx, ctx["G"], ctx["kit"], ctx["objs"]
        self.c = np.asarray(center, float)
        self.t = unit(axis)
        self.theme, self.rng, self.max_houses = theme, rng, max_houses
        self.items, self.spawns, self.clear = [], [], []
        self.blocks = []            # world quads of the buildings placed (overlap tests)
        self.removed = set()
        self.hard_w = 0.0
        self.counts = {"houses": 0, "kura": 0, "fences": 0, "lanterns": 0, "shrine": 0, "plaza": 0}
        self.road = None
        self.plaza = None
        self.path = None

    # -------------------------------------------------- tests
    def check(self, asset, origin, yaw, extra_free=()):
        """Validity of an item and the weight of what it would clear. Returns (z, idx, weight) or None."""
        min_shore, max_dz, margin, kind = SPEC[asset]
        rect = self.kit.rect[asset]
        S = to_world(origin, yaw, rect_grid(rect, 2.0))
        if self.G.shore(S).min() < min_shore:
            return None
        if self.G.bldg(S).min() < 40.0:
            return None
        for p, r, _ in self.ctx["actors"]:
            if np.linalg.norm(S - p, axis=1).min() < r + 3.0:
                return None
        if self.road is not None and seg_dist(S, self.road.pts).min() < ROAD_HALF + 0.4:
            return None
        if self.path is not None and asset not in ("SM_Village_StoneLantern", "SM_Village_Torii", "SM_Village_Shrine"):
            if seg_dist(S, self.path).min() < 1.8:
                return None
        if self.plaza is not None and "plaza" not in extra_free:
            if np.linalg.norm(S - self.plaza, axis=1).min() < PLAZA_R + 0.5:
                return None
        Q = to_world(origin, yaw, corners(rect))
        gap = 2.0 if asset == "SM_Minka" else 1.0 if kind == "building" else 0.2
        for B in self.blocks:
            if overlap(Q, B, gap):
                return None
        Gp = np.vstack([to_world(origin, yaw, rect_grid(g, 1.0)) for g in self.kit.ground[asset]])
        z = self.G.z(Gp)
        if z.max() - z.min() > max_dz:
            return None
        idx = self.objs.in_rect(origin, yaw, rect, margin)
        idx = np.array([i for i in idx if i not in self.removed], int)
        weight = float(self.objs.w[idx].sum()) if len(idx) else 0.0
        return float(z.max()) - 0.02, idx, weight

    def commit(self, asset, origin, yaw, z, idx, name, block=True):
        self.items.append({"id": name, "asset": asset, "origin": np.asarray(origin, float), "yaw": float(yaw),
                           "z": z})
        self.removed.update(int(i) for i in idx)
        self.hard_w += float(self.objs.w[idx].sum()) if len(idx) else 0.0
        rect = self.kit.rect[asset]
        self.clear.append(clear_rect(asset, rect, origin, yaw))
        if block:
            self.blocks.append(to_world(origin, yaw, corners(rect)))

    def best(self, asset, candidates, name, block=True, extra_free=()):
        best = None
        for origin, yaw, pen in candidates:
            r = self.check(asset, origin, yaw, extra_free)
            if r is None:
                continue
            z, idx, w = r
            cost = w + pen
            if best is None or cost < best[0]:
                best = (cost, origin, yaw, z, idx)
        if best is None:
            return None
        _, origin, yaw, z, idx = best
        self.commit(asset, origin, yaw, z, idx, name, block)
        return origin, yaw

    def ground_point(self, p):
        p = np.asarray(p, float)
        if self.G.shore([p])[0] < 3.0:
            return None
        for B in self.blocks:
            if overlap(B, np.array([p + [-0.4, -0.4], p + [0.4, -0.4], p + [0.4, 0.4], p + [-0.4, 0.4]]), 0.3):
                return None
        return np.array([p[0], p[1], float(self.G.z([p])[0]) + 0.1])

    def spawn(self, name, p, role, area):
        g = self.ground_point(p)
        if g is not None:
            self.spawns.append({"name": name, "loc": g, "role": role, "area": area})

    # -------------------------------------------------- parts
    def depth(self, n):
        """How far the land goes from the centre along direction n (shore distance kept above 6 m)."""
        xs = np.arange(0.0, 90.0, 2.0)
        sh = self.G.shore(self.c[None] + xs[:, None] * n[None])
        bad = np.nonzero(sh < 6.0)[0]
        return float(xs[bad[0]]) if len(bad) else 90.0

    def build(self):
        G = self.G
        self.road = trace_road(G, self.c, self.t, 95.0)
        if self.road.smax - self.road.smin < 40.0:
            return self
        p0, T0, N0 = self.road.at(0.0)
        sp = 1.0 if self.depth(N0) >= self.depth(-N0) else -1.0
        self.sp = sp
        u, v = T0, sp * N0
        P = p0 + v * (ROAD_HALF + 0.5 + PLAZA_R)
        disc = P[None] + np.array([[r * math.cos(a), r * math.sin(a)] for r in (0, 4, 8, PLAZA_R)
                                   for a in np.linspace(0, 2 * math.pi, 12, endpoint=False)])
        zz = G.z(disc)
        if G.shore(disc).min() < 6.0 or zz.max() - zz.min() > 0.9 or G.bldg(disc).min() < 40.0:
            return self
        self.plaza = P
        self.counts["plaza"] = 1
        yaw_v = facing(-v)
        L = lambda a, b: P + a * u + b * v
        self.best("SM_Village_Well", [(L(-4.0, 1.0), yaw_v, 0.0), (L(-5.0, 2.5), yaw_v, 0.5)], "Well",
                  extra_free=("plaza",))
        self.best("SM_Village_NoticeBoard", [(L(5.0, -7.0), yaw_v, 0.0), (L(4.0, -6.0), yaw_v, 0.5)], "NoticeBoard",
                  extra_free=("plaza",))
        self.best("SM_Village_FireTower", [(L(7.0, 4.5), yaw_v, 0.0), (L(6.0, 6.0), yaw_v, 0.5),
                                           (L(-7.0, 4.5), yaw_v, 1.0)], "FireTower", extra_free=("plaza",))
        for k, sx in enumerate((-1, 1)):
            self.best("SM_Village_StoneLantern", [(L(sx * 6.5, -8.5), yaw_v, 0.0)], f"Lantern_Place_{k}",
                      block=False, extra_free=("plaza",))
        self.clear.append({"type": "circle", "kind": "plaza", "center": [round(P[0] * 100, 1), round(P[1] * 100, 1)],
                           "radius": round(PLAZA_R * 100, 1)})
        self.flag = P.copy()
        for a in range(4):
            ang = math.radians(45 + 90 * a)
            self.spawn(f"Place_{a}", P + 4.2 * math.sqrt(2) * (math.cos(ang) * u + math.sin(ang) * v),
                       "elder" if a == 0 else "villager", "place")
        self.shrine(P, u, v)
        self.houses()
        self.finish()
        return self

    def shrine(self, P, u, v):
        yaw = facing(-v)
        for dist in (21.0, 15.0):
            tor = P + v * (PLAZA_R + 2.5)
            shr = P + v * (PLAZA_R + dist)
            if self.check("SM_Village_Torii", tor, yaw, ("plaza",)) is None:
                continue
            if self.check("SM_Village_Shrine", shr, yaw) is None:
                continue
            self.best("SM_Village_Torii", [(tor, yaw, 0.0)], "Torii", extra_free=("plaza",))
            self.best("SM_Village_Shrine", [(shr, yaw, 0.0)], "Shrine")
            front = shr - v * 3.4
            self.path = np.array([P + v * (PLAZA_R - 1.0), front])
            for k, d in enumerate((7.0, 13.0) if dist > 18 else (7.5,)):
                for j, sx in enumerate((-1, 1)):
                    self.best("SM_Village_StoneLantern", [(P + v * (PLAZA_R + d) + sx * 2.3 * u, yaw, 0.0)],
                              f"Lantern_Shrine_{k}{j}", block=False)
            self.clear.append({"type": "path", "kind": "path", "points": [[round(q[0] * 100, 1), round(q[1] * 100, 1)]
                                                                          for q in self.path], "half_width": 160.0})
            self.counts["shrine"] = 1
            for j, sx in enumerate((-1, 1)):
                self.spawn(f"Sanctuaire_{j}", front - v * 1.5 + sx * 1.5 * u, "priest" if j == 0 else "villager",
                           "sanctuaire")
            return
        # no room behind the square: at the end of the road, facing it
        for s in (self.road.smax - 4.0, self.road.smin + 4.0):
            p, T, N = self.road.at(s)
            out = T if s > 0 else -T
            shr = p + out * 6.0
            if self.best("SM_Village_Shrine", [(shr, facing(-out), 0.0)], "Shrine"):
                self.counts["shrine"] = 1
                self.spawn("Sanctuaire_0", shr - out * 4.6, "priest", "sanctuaire")
                return

    def houses(self):
        k = 0
        lots = sorted(((d, sgn * d, side) for d in LOT_S for sgn in (1.0, -1.0) for side in (1.0, -1.0)),
                      key=lambda lot: lot[0])
        kura_every = 1 if self.theme == "vivres" else 2
        for _, s, side in lots:
            if k >= self.max_houses:
                break
            cands = []
            for ds in (0.0, 3.0, -3.0, 6.0, -6.0):
                if not (self.road.smin + 6.0 <= s + ds <= self.road.smax - 6.0):
                    continue
                p, T, N = self.road.at(s + ds)
                f = -side * N
                for e in (0.0, 2.0, 4.0):
                    yaw = facing(f) + float(self.rng.uniform(-2.0, 2.0))
                    A = p + side * N * (FACADE + e)
                    origin = A - rot(yaw) @ np.array([5.1, 0.0])
                    cands.append((origin, yaw, 0.15 * abs(ds) + 0.25 * e))
            got = self.best("SM_Minka", cands, f"Minka_{k:02d}")
            if not got:
                continue
            origin, yaw = got
            hx = lambda a, b: to_world(origin, yaw, [(a, b)])[0]
            self.counts["houses"] += 1
            if k % kura_every == 0:
                kc = [(hx(7.6, -12.5), yaw, 0.0), (hx(2.6, -12.5), yaw, 0.3), (hx(15.4, -4.0), yaw, 0.6),
                      (hx(-5.2, -4.0), yaw, 0.6)]
                if self.best("SM_Village_Kura", kc, f"Kura_{k:02d}"):
                    self.counts["kura"] += 1
            for j, fx in enumerate((-2.4, 3.1, 5.1, 7.1, 9.1, 11.1)):
                if self.best("SM_Village_BambooFence", [(hx(fx, 4.6), yaw, 0.0)], f"Fence_{k:02d}_{j}", block=False):
                    self.counts["fences"] += 1
            if self.best("SM_Village_StoneLantern", [(hx(-0.9, 3.9), yaw, 0.0)], f"Lantern_{k:02d}", block=False):
                self.counts["lanterns"] += 1
            self.spawn(f"Maison{k:02d}_Doma", hx(0.9, 3.0), "villager", f"maison_{k:02d}")
            self.spawn(f"Maison{k:02d}_Engawa", hx(6.0, 3.1), "villager", f"maison_{k:02d}")
            k += 1

    def finish(self):
        ss = []
        for it in self.items:
            d = np.linalg.norm(self.road.pts - it["origin"], axis=1)
            ss.append(self.road.s[int(np.argmin(d))])
        s0 = max(self.road.smin, min(ss + [0.0]) - 10.0)
        s1 = min(self.road.smax, max(ss + [0.0]) + 10.0)
        self.road_pts = self.road.trimmed(s0, s1)
        self.clear.append({"type": "path", "kind": "road", "points": [[round(q[0] * 100, 1), round(q[1] * 100, 1)]
                                                                      for q in self.road_pts],
                           "half_width": ROAD_HALF * 100})
        # what the road and the square clear (not counted twice)
        idx = set(int(i) for i in self.objs.near_path(self.road_pts, ROAD_HALF + 0.4)) | \
            set(int(i) for i in self.objs.in_disc(self.plaza, PLAZA_R + 1.0))
        if self.path is not None:
            idx |= set(int(i) for i in self.objs.near_path(self.path, 2.0))
        new = np.array(sorted(idx - self.removed), int)
        self.removed.update(int(i) for i in new)
        self.hard_w += float(self.objs.w[new].sum()) if len(new) else 0.0
        # zone: the square is its centre, it covers the plots
        dmax = max([np.linalg.norm(it["origin"] - self.plaza) for it in self.items] + [30.0])
        self.radius = float(np.clip(dmax + 12.0, 40.0, 90.0))
        # designated threats (tiki): road ends and gaps between plots, on free ground
        k = 0
        for s in (s0 + 2.0, s1 - 2.0):
            p, T, N = self.road.at(s)
            for off in (0.0, 4.0):
                if k < 6:
                    q = p + N * off
                    before = len(self.spawns)
                    self.spawn(f"Gardien_{k}", q, "guardian", "lisiere")
                    k += len(self.spawns) - before
        for ang in np.linspace(0, 2 * math.pi, 12, endpoint=False):
            if k >= 6:
                break
            q = self.plaza + 0.75 * self.radius * np.array([math.cos(ang), math.sin(ang)])
            if self.G.shore([q])[0] < 6.0:
                continue
            before = len(self.spawns)
            self.spawn(f"Gardien_{k}", q, "guardian", "lisiere")
            k += len(self.spawns) - before

    def score(self):
        if not self.counts["plaza"] or self.counts["houses"] < MIN_HOUSES:
            return -1e9
        return (10.0 * self.counts["houses"] + 3.0 * self.counts["kura"] + 12.0 * self.counts["shrine"] + 8.0 +
                0.2 * self.counts["fences"] - 0.6 * self.hard_w)

    def to_memory(self, vid, name, theme, label, chunk, seed):
        items = []
        for it in self.items:
            o = it["origin"]
            items.append({"id": f"{vid}_{it['id']}", "asset": it["asset"],
                          "loc_cm": [round(o[0] * 100, 1), round(o[1] * 100, 1), round(it["z"] * 100, 1)],
                          "yaw": round(it["yaw"], 2)})
        spawns = [{"name": f"{vid}_{s['name']}", "role": s["role"], "area": s["area"],
                   "loc_cm": [round(v * 100, 1) for v in s["loc"]]} for s in self.spawns]
        fz = float(self.G.z([self.flag])[0])
        hard = self.objs.w[np.array(sorted(self.removed), int)] if self.removed else np.zeros(0)
        return {"id": vid, "name": name, "theme": theme, "theme_label": label, "chunk": chunk, "seed": seed,
                "center_cm": [round(self.plaza[0] * 100, 1), round(self.plaza[1] * 100, 1), round(fz * 100, 1)],
                "axis_deg": round(math.degrees(math.atan2(self.t[1], self.t[0])), 2),
                "zone": {"center_cm": [round(self.plaza[0] * 100, 1), round(self.plaza[1] * 100, 1), round(fz * 100, 1)],
                         "radius_cm": round(self.radius * 100, 1),
                         "flag_cm": [round(self.flag[0] * 100, 1), round(self.flag[1] * 100, 1), round(fz * 100, 1)]},
                "stats": dict(self.counts, items=len(items), spawns=len(spawns), cleared=len(self.removed),
                              cleared_trunks=int((hard >= 0.4).sum()), score=round(self.score(), 1)),
                "items": items, "spawns": spawns,
                "road_cm": [[round(q[0] * 100, 1), round(q[1] * 100, 1)] for q in self.road_pts],
                "clear": self.clear}


# ---------------------------------------------------------------------------------------------- search

def chunk_key(p, F):
    return f"{int((p[0] + F.cx) // CHUNK_M)}_{int((p[1] + F.cz) // CHUNK_M)}"


def search(ctx, args, keep):
    G, F, objs = ctx["G"], ctx["G"].F, ctx["objs"]
    m = F.m_px
    A = G.A
    rows, cols = np.nonzero(A.land & (A.d_shore >= SITE_MIN_SHORE) & (A.d_bldg >= BLDG_MIN))
    sel = (rows % 3 == 0) & (cols % 3 == 0)
    rows, cols = rows[sel], cols[sel]
    P = np.c_[(cols + 0.5) * m - F.cx, (rows + 0.5) * m - F.cz]
    d0 = np.linalg.norm(P - ctx["start"], axis=1)
    ok = (d0 >= args.min_dist) & (d0 <= args.max_dist)
    for v in keep:
        c = np.array(v["center_cm"][:2]) / 100.0
        ok &= np.linalg.norm(P - c, axis=1) >= args.spacing
    for p, r, _ in ctx["actors"]:
        ok &= np.linalg.norm(P - p, axis=1) >= r + 60.0
    P, d0 = P[ok], d0[ok]
    if not len(P):
        return {}
    wide = np.minimum(G.shore(P, smooth=True), 60.0)
    trunks = objs.trunk_tree.query_ball_point(P, 35.0, return_length=True) if objs.trunk_tree is not None else 0
    quick = wide - 0.25 * np.asarray(trunks)
    keys = np.array([chunk_key(p, F) for p in P])
    chunks = {}
    for key in sorted(set(keys)):
        idx = np.nonzero(keys == key)[0]
        top = idx[np.argsort(-quick[idx])][:args.tries]
        best = None
        for i in top:
            seed = int(args.seed * 1000003 + zlib.crc32(key.encode()) % 100003 + int(i)) % (2 ** 31)
            lay = Layout(ctx, P[i], motu_axis(G, P[i]), None, np.random.default_rng(seed), args.max_houses).build()
            sc = lay.score() - FAR_PENALTY * max(0.0, d0[i] - NEAR_M)
            if best is None or sc > best[0]:
                best = (sc, lay, P[i], seed)
        sc, lay, c, seed = best
        chunks[key] = {"score": round(float(sc), 1), "houses": lay.counts["houses"], "shrine": lay.counts["shrine"],
                       "center": c, "seed": seed, "layout": lay}
    return chunks


def pick(chunks, keep, count, spacing):
    chosen = []
    taken = [np.array(v["center_cm"][:2]) / 100.0 for v in keep]
    for key, ch in sorted(chunks.items(), key=lambda kv: -kv[1]["score"]):
        if len(chosen) >= count or ch["score"] < 0:
            break
        c = ch["layout"].plaza
        if any(np.linalg.norm(c - t) < spacing for t in taken):
            continue
        chosen.append(key)
        taken.append(c)
    return chosen


def assign_themes(ctx, chosen, chunks, used):
    free = [t for t in ("richesses", "bois", "vivres") if t not in used]
    out = {}
    if not free:
        free = ["vivres"]
    G, objs = ctx["G"], ctx["objs"]
    info = []
    for key in chosen:
        c = chunks[key]["layout"].plaza
        lagoon = float(G.lagoon([c])[0]) > 0.5
        info.append((key, (0 if lagoon else 1, float(G.shore([c])[0])),
                     len(objs.trunk_tree.query_ball_point(c, 120.0)) if objs.trunk_tree is not None else 0))
    rest = list(info)
    if "richesses" in free and rest:
        k = min(rest, key=lambda x: x[1])
        out[k[0]] = "richesses"
        rest.remove(k)
    if "bois" in free and rest:
        k = max(rest, key=lambda x: x[2])
        out[k[0]] = "bois"
        rest.remove(k)
    for k in rest:
        out[k[0]] = "vivres" if "vivres" in free else free[0]
    return out


# ---------------------------------------------------------------------------------------------- outputs

def write_csv(island, v):
    raw = ROOT / "docs" / "reference" / "map" / "raw" / f"{island}_villages"
    raw.mkdir(parents=True, exist_ok=True)
    path = raw / f"DT_{island.capitalize()}_Village_{v['id']}.csv"
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, quoting=csv.QUOTE_MINIMAL)
        w.writerow(["---", "Tag", "DevComment"])
        for k, it in enumerate(v["items"]):
            x, y, z = it["loc_cm"]
            w.writerow([f"{v['id']}_{k:04d}", f"{UE_MESHES}/{it['asset']}.{it['asset']}",
                        f"{x:.1f},{y:.1f},{z:.1f}|0.00,{it['yaw']:.2f},0.00|1.000,1.000,1.000"])
    return path


COLORS = {"SM_Minka": (200, 160, 90), "SM_Village_Kura": (235, 235, 230), "SM_Village_Shrine": (190, 60, 50),
          "SM_Village_Torii": (220, 70, 40), "SM_Village_Well": (90, 140, 200), "SM_Village_FireTower": (120, 90, 60),
          "SM_Village_NoticeBoard": (150, 110, 70), "SM_Village_StoneLantern": (160, 160, 150),
          "SM_Village_BambooFence": (180, 170, 90)}


def preview(path, ctx, villages, chunks, start, kit):
    G, objs = ctx["G"], ctx["objs"]
    F = G.F
    pts = [np.array(v["center_cm"][:2]) / 100.0 for v in villages] + [start]
    lo, hi = np.min(pts, 0) - 700.0, np.max(pts, 0) + 700.0
    span = float((hi - lo).max())
    lo = (lo + hi) / 2 - span / 2
    W = 1200
    px = span / W

    def to_px(p):
        return ((p[0] - lo[0]) / px, (p[1] - lo[1]) / px)

    xs = lo[0] + (np.arange(W) + 0.5) * px
    ys = lo[1] + (np.arange(W) + 0.5) * px
    X, Y = np.meshgrid(xs, ys)
    sh = G.shore(np.c_[X.ravel(), Y.ravel()]).reshape(W, W)
    bl = G.bldg(np.c_[X.ravel(), Y.ravel()]).reshape(W, W)
    img = np.zeros((W, W, 3), np.uint8)
    img[...] = (40, 90, 130)
    img[sh > 0.5] = (205, 195, 160)
    img[(sh > 0.5) & (sh >= SITE_MIN_SHORE)] = (170, 185, 130)
    img[bl < 6.0] = (200, 40, 40)
    over = Image.fromarray(img)
    d = ImageDraw.Draw(over, "RGBA")
    for key, ch in chunks.items():
        i, j = (int(t) for t in key.split("_"))
        x0, y0 = i * CHUNK_M - F.cx, j * CHUNK_M - F.cz
        a, b = to_px((x0, y0)), to_px((x0 + CHUNK_M, y0 + CHUNK_M))
        col = (40, 160, 60, 70) if ch["score"] >= 60 else (230, 200, 40, 70) if ch["score"] >= 0 else (120, 120, 120, 50)
        d.rectangle([a, b], fill=col, outline=(0, 0, 0, 60))
    for v in villages:
        c = np.array(v["zone"]["center_cm"][:2]) / 100.0
        r = v["zone"]["radius_cm"] / 100.0 / px
        cx, cy = to_px(c)
        d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(255, 255, 255, 255), width=3)
        d.text((cx + r + 4, cy - 8), f"{v['id']} {v['name']} ({v['stats']['houses']} maisons)", fill=(255, 255, 255, 255))
    sx, sy = to_px(start)
    d.ellipse([sx - 6, sy - 6, sx + 6, sy + 6], fill=(0, 220, 255, 255))
    d.text((sx + 8, sy - 6), "PlayerStart", fill=(0, 220, 255, 255))
    canvas = Image.new("RGB", (1800, 1800), (30, 30, 30))
    canvas.paste(over, (0, 0))
    removed_all = set()
    for v in villages:
        removed_all |= set(v.get("_removed", []))
    for k, v in enumerate(villages[:3]):
        c = np.array(v["zone"]["center_cm"][:2]) / 100.0
        Z, zs = 600, 0.25              # 150 m window, 4 px / m
        zl = c - Z * zs / 2
        zx = zl[0] + (np.arange(Z) + 0.5) * zs
        zy = zl[1] + (np.arange(Z) + 0.5) * zs
        ZX, ZY = np.meshgrid(zx, zy)
        zsh = G.shore(np.c_[ZX.ravel(), ZY.ravel()]).reshape(Z, Z)
        zim = np.zeros((Z, Z, 3), np.uint8)
        zim[...] = (40, 90, 130)
        zim[zsh > 0.5] = (205, 195, 160)
        zimg = Image.fromarray(zim)
        zd = ImageDraw.Draw(zimg, "RGBA")
        zp = lambda p: ((p[0] - zl[0]) / zs, (p[1] - zl[1]) / zs)
        idx = objs.in_disc(c, Z * zs * 0.75)
        rem = set(int(i) for i in idx[vc.Clearing(v["clear"]).mask(objs.xy[idx] * 100.0, objs.cat[idx])]) if len(idx) else set()
        for i in idx:
            x, y = zp(objs.xy[i])
            if not (0 <= x < Z and 0 <= y < Z):
                continue
            w = objs.w[i]
            if i in rem:
                zd.point((x, y), fill=(255, 60, 160, 255)) if w < 0.4 else zd.ellipse([x - 3, y - 3, x + 3, y + 3], outline=(255, 0, 0, 255))
            elif w >= 0.4:
                zd.ellipse([x - 2, y - 2, x + 2, y + 2], fill=(20, 90, 20, 255))
            else:
                zd.point((x, y), fill=(90, 150, 70, 200))
        road = [zp(np.array(q) / 100.0) for q in v["road_cm"]]
        zd.line(road, fill=(150, 100, 60, 255), width=int(2 * ROAD_HALF / zs))
        pc = zp(c)
        rr = PLAZA_R / zs
        zd.ellipse([pc[0] - rr, pc[1] - rr, pc[0] + rr, pc[1] + rr], fill=(170, 120, 80, 160))
        for it in v["items"]:
            o = np.array(it["loc_cm"][:2]) / 100.0
            Q = to_world(o, it["yaw"], corners(kit.rect[it["asset"]]))
            zd.polygon([zp(q) for q in Q], outline=(0, 0, 0, 255), fill=COLORS[it["asset"]] + (230,))
        for s in v["spawns"]:
            x, y = zp(np.array(s["loc_cm"][:2]) / 100.0)
            col = (220, 30, 30, 255) if s["role"] == "guardian" else (30, 90, 230, 255)
            zd.ellipse([x - 4, y - 4, x + 4, y + 4], fill=col)
        R = v["zone"]["radius_cm"] / 100.0 / zs
        zd.ellipse([pc[0] - R, pc[1] - R, pc[0] + R, pc[1] + R], outline=(255, 255, 255, 255), width=2)
        zd.text((8, 8), f"{v['id']} {v['name']} - {v['theme_label']}", fill=(255, 255, 255, 255))
        st = v["stats"]
        zd.text((8, 24), f"{st['houses']} minka, {st['kura']} kura, sanctuaire {st['shrine']}, {st['items']} objets, "
                         f"{st['spawns']} spawns, {st['cleared']} instances retirees ({st['cleared_trunks']} troncs)",
                fill=(255, 255, 255, 255))
        canvas.paste(zimg, (1200, 600 * k))
    leg = ImageDraw.Draw(canvas)
    leg.text((20, 1220), "Vue d'ensemble : sable (beige), terre large > 24 m du rivage (vert pale), batiments reels (rouge),",
             fill=(230, 230, 230))
    leg.text((20, 1240), "chunks de 256 m : vert = village complet possible, jaune = petit, gris = pas de place ; "
                         "cercle blanc = zone de pacification.", fill=(230, 230, 230))
    leg.text((20, 1260), "Zooms (150 m) : troncs gardes (vert fonce), retires (cercle rouge), buissons retires (rose), "
                         "route (brun), place, spawns PNJ (bleu) et menaces (rouge).", fill=(230, 230, 230))
    canvas.save(path)


# ---------------------------------------------------------------------------------------------- main

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("island")
    ap.add_argument("--count", type=int, default=3, help="villages wanted in total (cahier: 3 zones on island 1)")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--reset", action="store_true", help="forget the villages in memory")
    ap.add_argument("--regen", action="append", default=[], help="rebuild this village id (repeatable)")
    ap.add_argument("--min-dist", type=float, default=250.0, help="from the PlayerStart (m)")
    ap.add_argument("--max-dist", type=float, default=2000.0, help="from the PlayerStart (m)")
    ap.add_argument("--spacing", type=float, default=350.0, help="between two villages (m)")
    ap.add_argument("--max-houses", type=int, default=12)
    ap.add_argument("--tries", type=int, default=3, help="trial layouts per chunk")
    ap.add_argument("--reclear", action="store_true", help="recompute the clearing of the villages in memory")
    ap.add_argument("--extras", action="store_true", help="recompute the points of interest and lights in memory")
    ap.add_argument("--apply-clearing", action="store_true")
    ap.add_argument("--no-csv", action="store_true")
    args = ap.parse_args()

    ref = ROOT / "docs" / "reference" / "map" / args.island
    out = ref / "villages"
    out.mkdir(parents=True, exist_ok=True)
    mem_path = vc.memory_path(args.island)
    memory = json.loads(mem_path.read_text(encoding="utf-8")) if mem_path.exists() and not args.reset else None
    keep = [v for v in (memory or {}).get("villages", []) if v["id"] not in args.regen]

    A = pv.Atoll(ref)
    G = Ground(A)
    kit = Kit(KIT)
    objs = Objects(args.island)
    actors, start = load_actors(ref)
    if start is None:
        start = np.array(DEFAULT_START_CM) / 100.0
    ctx = {"G": G, "kit": kit, "objs": objs, "actors": actors, "start": start}

    need = max(0, args.count - len(keep))
    chunks = search(ctx, args, keep) if need else {}
    chosen = pick(chunks, keep, need, args.spacing)
    themes = assign_themes(ctx, chosen, chunks, {v["theme"] for v in keep})
    ids = {v["id"] for v in keep}
    new = []
    for key in chosen:
        ch = chunks[key]
        theme = themes[key]
        name, label = THEMES[theme]
        lay = Layout(ctx, ch["center"], motu_axis(G, ch["center"]), theme, np.random.default_rng(ch["seed"]),
                     args.max_houses).build()
        if lay.score() < 0:
            continue
        vid = next(f"V{i:02d}" for i in range(1, 100) if f"V{i:02d}" not in ids)
        ids.add(vid)
        v = lay.to_memory(vid, name, theme, label, key, ch["seed"])
        v["_removed"] = sorted(lay.removed)
        new.append(v)
        print(f"[villages] {vid} {name} ({label}) chunk {key}: {v['stats']}")
    villages = sorted(keep + new, key=lambda v: v["id"])
    for v in villages:
        if args.reclear or v in new:
            if args.reclear and v not in new:
                v["clear"] = reclear(v, kit)
            v["stats"]["cleared"], v["stats"]["cleared_trunks"] = clearing_stats(v, objs)
            print(f"[villages] {v['id']} {v['name']}: clears {v['stats']['cleared']} instances "
                  f"({v['stats']['cleared_trunks']} palm trunks)")
        if args.extras or v in new or "pois" not in v or "doors" not in v:
            v["pois"], v["lights"] = village_extras(v, G, kit)
            v["doors"] = village_doors(v, G)
            print(f"[villages] {v['id']} {v['name']}: {len(v['pois'])} points of interest, {len(v['lights'])} lights, "
                  f"{len(v['doors'])} doors")

    chunk_mem = dict((memory or {}).get("chunks", {}))
    for key, ch in chunks.items():
        chunk_mem[key] = {"score": ch["score"], "houses": ch["houses"], "shrine": ch["shrine"],
                          "village": next((v["id"] for v in villages if v["chunk"] == key), None)}
    doc = {"version": 1, "island": args.island, "generator": "scripts/map/place_villages.py",
           "frame": "UE cm: X east, Y south, Z up; yaw in degrees (Unreal); meshes " + UE_MESHES,
           "params": {"count": args.count, "seed": args.seed, "min_dist_m": args.min_dist, "max_dist_m": args.max_dist,
                      "spacing_m": args.spacing, "max_houses": args.max_houses, "chunk_m": CHUNK_M},
           "player_start_cm": [round(float(start[0]) * 100, 1), round(float(start[1]) * 100, 1)],
           "chunks": chunk_mem,
           "villages": [{k: val for k, val in v.items() if not k.startswith("_")} for v in villages]}
    mem_path.write_text(json.dumps(doc, indent=1, ensure_ascii=False), encoding="utf-8")
    print(f"[villages] memory: {mem_path.relative_to(ROOT)} ({len(villages)} villages, {len(chunk_mem)} chunks)")
    if not args.no_csv:
        for v in villages:
            print("[villages] csv:", write_csv(args.island, v).relative_to(ROOT))
    preview(out / "preview_villages.png", ctx, villages, chunks, start, kit)
    print("[villages] preview:", (out / "preview_villages.png").relative_to(ROOT))
    if args.apply_clearing:
        raw = ROOT / "docs" / "reference" / "map" / "raw" / f"{args.island}_vegetation"
        report = vc.reapply(raw, vc.Clearing.load(args.island))
        changed = {k: n for k, n in report.items() if n}
        (out / "clearing_report.json").write_text(json.dumps({"removed": changed}, indent=1), encoding="utf-8")
        print(f"[villages] clearing: {sum(changed.values())} instances removed from {len(changed)} DataTables: {changed}")


if __name__ == "__main__":
    main()
