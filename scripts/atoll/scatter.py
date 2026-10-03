"""Natural placement of the vegetation library on the motu (Geometry Nodes instancing) and the
assembly of the "Atoll" scene: terrain, water, foam, far rim, cameras.

Ecological rules (Tuamotu motu, ocean -> lagoon):
- reef flat: coral blocks, rubble, shells; ridge: rubble + miki-miki thicket, a few tahinu/naupaka
- grove: coconut plantation rows (south) / irregular grove (north), juveniles and sprouts under the
  mother palms, ferns in the shade, grass in the gaps, litter + fallen nuts + dead fronds
- lagoon edge: palms leaning toward the lagoon (light), naupaka hedge, tahinu, pandanus
- upper beach: beach morning glory runners, wrack line (shells, rubble, driftwood)
"""
import math

import bpy
import numpy as np
from mathutils import Matrix

from . import materials as M
from . import scene as sc
from . import terrain
from .geo import UP, align_z, norm, rot_z

NEAR_Y = 110.0
FAR_Y = 1300.0


# ---------------------------------------------------------------- GN instancing

def _gn_group(name, coll):
    ng = bpy.data.node_groups.new(name, "GeometryNodeTree")
    ng.interface.new_socket("Geometry", in_out="INPUT", socket_type="NodeSocketGeometry")
    ng.interface.new_socket("Geometry", in_out="OUTPUT", socket_type="NodeSocketGeometry")
    n = ng.nodes
    gi, go = n.new("NodeGroupInput"), n.new("NodeGroupOutput")
    ci = n.new("GeometryNodeCollectionInfo")
    ci.inputs["Collection"].default_value = coll
    ci.inputs["Separate Children"].default_value = True
    ci.inputs["Reset Children"].default_value = True
    iop = n.new("GeometryNodeInstanceOnPoints")
    iop.inputs["Pick Instance"].default_value = True
    attrs = {}
    for an, dt in (("variant", "INT"), ("rot", "FLOAT_VECTOR"), ("scl", "FLOAT_VECTOR")):
        na = n.new("GeometryNodeInputNamedAttribute")
        na.data_type = dt
        na.inputs["Name"].default_value = an
        attrs[an] = na.outputs["Attribute"]
    L = ng.links
    L.new(gi.outputs[0], iop.inputs["Points"])
    L.new(ci.outputs["Instances"], iop.inputs["Instance"])
    L.new(attrs["variant"], iop.inputs["Instance Index"])
    L.new(attrs["rot"], iop.inputs["Rotation"])
    L.new(attrs["scl"], iop.inputs["Scale"])
    L.new(iop.outputs["Instances"], go.inputs[0])
    return ng


def instancer(name, coll, P, idx, rot, scl, parent):
    P = np.asarray(P, np.float32).reshape(-1, 3)
    n = len(P)
    me = bpy.data.meshes.new(name)
    me.vertices.add(n)
    me.vertices.foreach_set("co", P.ravel())
    me.attributes.new("variant", "INT", "POINT").data.foreach_set("value", np.asarray(idx, np.int32))
    me.attributes.new("rot", "FLOAT_VECTOR", "POINT").data.foreach_set("vector", np.asarray(rot, np.float32).ravel())
    s = np.asarray(scl, np.float32)
    if s.ndim == 1:
        s = np.repeat(s[:, None], 3, 1)
    me.attributes.new("scl", "FLOAT_VECTOR", "POINT").data.foreach_set("vector", s.ravel())
    obj = bpy.data.objects.new(name, me)
    parent.objects.link(obj)
    mod = obj.modifiers.new("Scatter", "NODES")
    mod.node_group = _gn_group("GN_" + name, coll)
    return obj


def variant_index(coll):
    names = sorted(o.name for o in coll.objects)
    return {nm: i for i, nm in enumerate(names)}


# ---------------------------------------------------------------- sampling helpers

class Lines:
    """Fast shoreline lookups (tabulated)."""

    def __init__(self, motu):
        self.y = np.arange(-FAR_Y - 200, FAR_Y + 200, 0.5)
        self.t = {k: getattr(motu, k)(self.y) for k in ("xr", "xc", "xl", "xv", "xo")}

    def __call__(self, k, y):
        return np.interp(y, self.y, self.t[k])


def poisson(rng, bounds, r, accept, k=24, seeds=40):
    """Bridson Poisson-disk sampling with a scalar accept(x, y) predicate."""
    x0, x1, y0, y1 = bounds
    cell = r / math.sqrt(2)
    gw, gh = int((x1 - x0) / cell) + 1, int((y1 - y0) / cell) + 1
    grid = -np.ones((gh, gw), np.int64)
    pts, active = [], []

    def add(p):
        pts.append(p)
        active.append(len(pts) - 1)
        grid[int((p[1] - y0) / cell), int((p[0] - x0) / cell)] = len(pts) - 1

    def ok(p):
        gx, gy = int((p[0] - x0) / cell), int((p[1] - y0) / cell)
        for yy in range(max(0, gy - 2), min(gh, gy + 3)):
            for xx in range(max(0, gx - 2), min(gw, gx + 3)):
                j = grid[yy, xx]
                if j >= 0 and (pts[j][0] - p[0]) ** 2 + (pts[j][1] - p[1]) ** 2 < r * r:
                    return False
        return True

    for _ in range(seeds * 50):
        p = (rng.uniform(x0, x1), rng.uniform(y0, y1))
        if accept(*p) and ok(p):
            add(p)
            if len(pts) >= seeds:
                break
    while active:
        i = int(rng.integers(len(active)))
        p = pts[active[i]]
        for _ in range(k):
            a = rng.uniform(0, 2 * math.pi)
            d = rng.uniform(r, 2 * r)
            q = (p[0] + math.cos(a) * d, p[1] + math.sin(a) * d)
            if x0 <= q[0] < x1 and y0 <= q[1] < y1 and accept(*q) and ok(q):
                add(q)
                break
        else:
            active.pop(i)
    return np.array(pts).reshape(-1, 2)


def nearest_dist(a, b):
    """Distance from each point of a (n,2) to the nearest point of b (m,2)."""
    if len(b) == 0:
        return np.full(len(a), 1e9)
    out = np.empty(len(a))
    for s in range(0, len(a), 4096):
        d = a[s:s + 4096, None, :] - b[None, :, :]
        out[s:s + 4096] = np.sqrt((d ** 2).sum(-1)).min(1)
    return out


def orient(motu, xy, yaw, tilt=0.0, wobble=0.0, rng=None):
    """Euler XYZ per point: yaw about Z, then tilt toward the terrain normal (0..1) + random wobble."""
    n = motu.normal(xy[:, 0], xy[:, 1])
    eul = np.zeros((len(xy), 3))
    for i in range(len(xy)):
        up = norm(UP * (1 - tilt) + n[i] * tilt)
        if wobble and rng is not None:
            up = norm(up + rng.normal(0, wobble, 3))
        R = align_z(up) @ rot_z(yaw[i])
        eul[i] = Matrix(R.tolist()).to_euler("XYZ")
    return eul


def ground_z(motu, xy, sink=0.0):
    return motu.height(xy[:, 0], xy[:, 1]) - sink


# ---------------------------------------------------------------- the scene

def build_motu_scene(built, sun_az, quick=False):
    rng = np.random.default_rng(2024)
    scn = bpy.data.scenes.new("Atoll")
    sc.setup_render(scn, (1280, 720) if quick else (1920, 1080), 24 if quick else 64)
    sc.world(scn, sun_az)
    sc.sun(scn, 52, math.degrees(sun_az))
    root = scn.collection
    colls = {}
    for nm in ("Terrain", "Water", "Scatter_Palms", "Scatter_Shrubs", "Scatter_Ground", "Scatter_Props", "Far",
               "Cameras"):
        colls[nm] = bpy.data.collections.new(nm)
        root.children.link(colls[nm])

    motu = terrain.Motu(7)
    ln = Lines(motu)
    palms_c, lod_c = built["palms"], built["palms_lod"]
    shrub_c, ground_c, props_c = built["shrubs"], built["ground"], built["props"]
    vp, vs, vg, vpr, vl = (variant_index(c) for c in (palms_c, shrub_c, ground_c, props_c, lod_c))

    def grove(x, y):
        return ln("xo", y) + 1.5 < x < ln("xv", y) + 0.8

    # ---- coconut palms
    P_xy, P_var, P_yaw = [], [], []
    plant = [(x, y) for y in np.arange(-NEAR_Y + 2, -38, 8.5) for x in np.arange(-30, 40, 8.5)]
    for x, y in plant:
        x, y = x + rng.normal(0, 0.7), y + rng.normal(0, 0.7)
        if grove(x, y) and x < ln("xv", y) - 3 and rng.random() > 0.06:
            P_xy.append((x, y))
    plant_xy = np.array(P_xy).reshape(-1, 2)
    wild = poisson(rng, (-60, 60, -36, NEAR_Y), 6.8, lambda x, y: grove(x, y) and x < ln("xv", y) - 3)
    edge = poisson(rng, (-60, 70, -NEAR_Y, NEAR_Y), 5.5,
                   lambda x, y: ln("xv", y) - 4 < x < ln("xv", y) + 1.8)
    oc_edge = poisson(rng, (-60, 60, -NEAR_Y, NEAR_Y), 7.5,
                      lambda x, y: ln("xo", y) + 0.5 < x < ln("xo", y) + 5)
    oc_edge = oc_edge[nearest_dist(oc_edge, np.vstack([plant_xy, wild])) > 4.5]
    edge = edge[nearest_dist(edge, np.vstack([plant_xy, wild])) > 4.0]

    def pick(opts):
        names, w = zip(*opts)
        return names[int(rng.choice(len(names), p=np.array(w) / sum(w)))]

    interior = [("Straight_A", 30), ("Straight_B", 25), ("Leaning_A", 7), ("Leaning_B", 10), ("SCurve_A", 8),
                ("Young_A", 6), ("Young_B", 8), ("DeadSnag_A", 3)]
    for x, y in np.vstack([plant_xy, wild]):
        v = pick(interior)
        P_var.append(v)
        P_yaw.append(rng.uniform(0, 2 * math.pi))
    all_xy = [tuple(p) for p in np.vstack([plant_xy, wild])]
    for x, y in edge:
        all_xy.append((x, y))
        P_var.append(pick([("Sabre_A", 30), ("Sabre_B", 15), ("Leaning_A", 25), ("Leaning_B", 20),
                           ("Straight_B", 10)]))
        P_yaw.append(rng.normal(0, 0.35))  # lean toward the lagoon (+X)
    for x, y in oc_edge:
        all_xy.append((x, y))
        P_var.append(pick([("Leaning_A", 25), ("Leaning_B", 15), ("Straight_A", 40), ("SCurve_A", 20)]))
        P_yaw.append(math.pi + rng.normal(0, 0.4))  # lean toward the ocean
    # fallen palms: one across the lagoon beach, one in the grove
    for (x, y, yaw) in ((ln("xv", 14.0) - 1.0, 14.0, 0.15), (2.0, 64.0, 2.4)):
        all_xy.append((x, y))
        P_var.append("Fallen_A")
        P_yaw.append(yaw)
    P_xy = np.array(all_xy)
    motu.palms_xy = P_xy
    big = np.array([v not in ("DeadSnag_A",) for v in P_var])

    # ---- terrain (litter mask needs the palm positions)
    mb, A, B, ext = terrain.build_terrain(motu)
    me = mb.build("Terrain_Motu", [M.terrain()])
    terrain.add_mask_attrs(me, A, B)
    tobj = bpy.data.objects.new("Terrain_Motu", me)
    colls["Terrain"].objects.link(tobj)

    def mk(name, mbx, mats, coll, shadow=True):
        o = bpy.data.objects.new(name, mbx.build(name, mats))
        coll.objects.link(o)
        o.visible_shadow = shadow
        return o

    mk("Water_Surface", terrain.plane(-6000, 6000, -6000, 6000, 0.0), [M.water_surface()], colls["Water"], False)
    mk("Seabed_Ocean", terrain.plane(-6000, -240, -6000, 6000, -26.0), [M.deep_water()], colls["Water"], False)
    mk("Seabed_Lagoon", terrain.plane(240, 6000, -6000, 6000, -18.0), [M.deep_water()], colls["Water"], False)
    surf = terrain.ribbon(motu, lambda y: ln("xr", y), -FAR_Y, FAR_Y, 1.0, 10.0, -6.0,
                          lambda u: 0.05 + 0.32 * np.exp(-((u - 0.45) / 0.16) ** 2))
    mk("Foam_ReefSurf", surf, [M.foam("M_Foam_Surf", 0.62, 0.9)], colls["Water"], False)
    shore = terrain.ribbon(motu, lambda y: ln("xl", y), -FAR_Y, FAR_Y, 0.5, 1.4, -0.7, lambda u: 0.012 + 0 * u)
    mk("Foam_LagoonShore", shore, [M.foam("M_Foam_Shore", 0.35, 2.5)], colls["Water"], False)

    # ---- far rim of the atoll across the lagoon
    fm_s, fm_f, far_xy = terrain.far_motu(77)
    mk("FarMotu_Sand", fm_s, [M.lineup_ground()], colls["Far"])
    mk("FarMotu_Foliage", fm_f, [M.far_foliage()], colls["Far"])

    # ---- palms instancing (near) + LOD palms (far along the motu, and the far rim)
    Pz = ground_z(motu, P_xy, 0.06)
    eul = orient(motu, P_xy, np.array(P_yaw), 0.0, 0.02, rng)
    instancer("Palms", palms_c, np.c_[P_xy, Pz], [vp["SM_CoconutPalm_" + v] for v in P_var], eul,
              rng.uniform(0.86, 1.12, len(P_xy)), colls["Scatter_Palms"])

    far = []
    for y in np.arange(NEAR_Y, FAR_Y, 9.0):
        for side in (-1, 1):
            yy = side * y + rng.normal(0, 2)
            for x in np.arange(-40, 50, 9.0):
                xx = x + rng.normal(0, 2.5)
                if ln("xo", yy) + 1 < xx < ln("xv", yy) + 1 and rng.random() > 0.2:
                    far.append((xx, yy))
    far = np.array(far)
    fz = ground_z(motu, far, 0.05)
    fyaw = np.where(far[:, 0] > ln("xv", far[:, 1]) - 4, rng.normal(0, 0.4, len(far)), rng.uniform(0, 6.28, len(far)))
    lods = sorted(vl)
    instancer("Palms_LOD", lod_c, np.c_[far, fz], rng.integers(0, len(lods), len(far)),
              np.c_[np.zeros((len(far), 2)), fyaw], rng.uniform(0.85, 1.15, len(far)), colls["Far"])
    instancer("Palms_LOD_FarRim", lod_c, np.c_[far_xy, np.full(len(far_xy), 2.0)],
              rng.integers(0, len(lods), len(far_xy)),
              np.c_[np.zeros((len(far_xy), 2)), rng.uniform(0, 6.28, len(far_xy))],
              rng.uniform(0.85, 1.15, len(far_xy)), colls["Far"])

    # ---- understory: juveniles + sprouts around mother palms
    juv = poisson(rng, (-60, 60, -NEAR_Y, NEAR_Y), 3.2, lambda x, y: grove(x, y))
    juv = juv[(nearest_dist(juv, P_xy) > 2.2) & (rng.random(len(juv)) < 0.33)]
    spr = []
    for (x, y), v in zip(P_xy, P_var):
        if v in ("DeadSnag_A", "Young_A"):
            continue
        for _ in range(int(rng.integers(0, 3))):
            a, d = rng.uniform(0, 6.28), rng.uniform(1.2, 7.0)
            spr.append((x + math.cos(a) * d, y + math.sin(a) * d))
    spr = np.array(spr)
    spr = spr[nearest_dist(spr, P_xy) > 0.9]
    U_xy = np.vstack([juv, spr])
    U_var = (["Juvenile_A" if rng.random() < 0.6 else "Juvenile_B" for _ in juv]
             + ["Sprout_A" if rng.random() < 0.5 else "Sprout_B" for _ in spr])
    instancer("Palms_Understory", palms_c, np.c_[U_xy, ground_z(motu, U_xy, 0.03)],
              [vp["SM_CoconutPalm_" + v] for v in U_var],
              orient(motu, U_xy, rng.uniform(0, 6.28, len(U_xy)), 0.6), rng.uniform(0.8, 1.2, len(U_xy)),
              colls["Scatter_Palms"])

    # ---- shrubs
    S_xy, S_var, S_yaw, S_scl = [], [], [], []

    def add_s(x, y, v, yaw=None, s=(0.8, 1.2)):
        S_xy.append((x, y))
        S_var.append(v)
        S_yaw.append(rng.uniform(0, 6.28) if yaw is None else yaw)
        S_scl.append(rng.uniform(*s))

    for y in np.arange(-NEAR_Y, NEAR_Y, 2.3):  # naupaka hedge on the lagoon beach crest
        y = y + rng.normal(0, 0.6)
        if rng.random() < 0.15:
            continue
        add_s(ln("xv", y) + rng.uniform(-2.8, 1.2), y, "SM_Naupaka_" + "ABC"[int(rng.integers(3))], s=(0.7, 1.25))
    for y in np.arange(-NEAR_Y, NEAR_Y, 2.4):  # miki-miki thicket behind the storm ridge (wind from -X)
        for off in (rng.uniform(0.5, 3.5), rng.uniform(4.0, 8.0)):
            yy = y + rng.normal(0, 0.7)
            if rng.random() < 0.12:
                continue
            add_s(ln("xc", yy) + off, yy, "SM_MikiMiki_" + "ABC"[int(rng.integers(3))], rng.normal(0, 0.25),
                  (0.8, 1.35))
    for y in np.arange(-NEAR_Y, NEAR_Y, 3.6):  # naupaka on the ocean side, mixed in
        yy = y + rng.normal(0, 1)
        add_s(ln("xo", yy) + rng.uniform(-1, 4), yy, "SM_Naupaka_" + "ABC"[int(rng.integers(3))], s=(0.6, 1.0))
    for y in np.arange(-NEAR_Y + 5, NEAR_Y, 15.0):  # tahinu on both crests
        yy = y + rng.normal(0, 3)
        add_s(ln("xv", yy) + rng.uniform(-3.5, 0.5), yy, "SM_Tahinu_" + "AB"[int(rng.integers(2))], s=(0.8, 1.15))
    for y in np.arange(-NEAR_Y + 9, NEAR_Y, 22.0):
        yy = y + rng.normal(0, 4)
        add_s(ln("xo", yy) + rng.uniform(0.5, 4), yy, "SM_Tahinu_" + "AB"[int(rng.integers(2))], s=(0.7, 1.0))
    for _ in range(34):  # pandanus: grove edges and a few inside
        yy = rng.uniform(-NEAR_Y, NEAR_Y)
        r = rng.random()
        x = (ln("xv", yy) - rng.uniform(3, 10)) if r < 0.5 else (ln("xo", yy) + rng.uniform(2, 9)) if r < 0.8 else \
            rng.uniform(ln("xo", yy) + 8, ln("xv", yy) - 8)
        add_s(x, yy, ["SM_Pandanus_A", "SM_Pandanus_B", "SM_Pandanus_Young"][int(rng.integers(3))], s=(0.85, 1.15))
    S_xy = np.array(S_xy)
    keep = nearest_dist(S_xy, P_xy) > 1.3
    S_xy = S_xy[keep]
    S_var = [v for v, k in zip(S_var, keep) if k]
    instancer("Shrubs", shrub_c, np.c_[S_xy, ground_z(motu, S_xy, 0.06)], [vs[v] for v in S_var],
              orient(motu, S_xy, np.array(S_yaw)[keep], 0.25), np.array(S_scl)[keep], colls["Scatter_Shrubs"])

    # ---- ground cover
    def sample(n, xr, accept_p):
        out = []
        tries = 0
        while len(out) < n and tries < n * 40:
            tries += 1
            y = rng.uniform(-NEAR_Y, NEAR_Y)
            x = rng.uniform(*xr(y))
            if rng.random() < accept_p(x, y):
                out.append((x, y))
        return np.array(out).reshape(-1, 2)

    dP = lambda xy: nearest_dist(xy, P_xy)  # noqa: E731
    fern = sample(420, lambda y: (ln("xo", y) + 2, ln("xv", y) - 1), lambda x, y: 1.0)
    fern = fern[rng.random(len(fern)) < 0.25 + 0.7 * np.exp(-dP(fern) ** 2 / 20)]
    grass_g = sample(4200, lambda y: (ln("xo", y) - 1, ln("xv", y) + 1), lambda x, y: 1.0)
    grass_g = grass_g[rng.random(len(grass_g)) < 0.35 + 0.5 * (1 - np.exp(-dP(grass_g) ** 2 / 30))]
    grass_b = sample(1600, lambda y: (ln("xv", y) - 1, ln("xv", y) + 4.5), lambda x, y: 1.0)
    ipo = sample(36, lambda y: (ln("xv", y) + 1.5, ln("xv", y) + 6.0), lambda x, y: 1.0)
    G_xy = np.vstack([fern, grass_g, grass_b, ipo])
    G_var = (["SM_Fern_" + "AB"[int(rng.integers(2))] for _ in fern]
             + ["SM_Grass_" + ("Short", "Tuft")[int(rng.integers(2))] for _ in grass_g]
             + ["SM_Grass_" + ("Tuft", "Dry")[int(rng.integers(2))] for _ in grass_b]
             + ["SM_Ipomoea_" + "AB"[int(rng.integers(2))] for _ in ipo])
    G_scl = np.r_[rng.uniform(0.8, 1.4, len(fern)), rng.uniform(0.7, 1.5, len(grass_g) + len(grass_b)),
                  rng.uniform(0.9, 1.3, len(ipo))]
    instancer("GroundCover", ground_c, np.c_[G_xy, ground_z(motu, G_xy, 0.015)], [vg[v] for v in G_var],
              orient(motu, G_xy, rng.uniform(0, 6.28, len(G_xy)), 1.0), G_scl, colls["Scatter_Ground"])

    # ---- props: nuts and dead fronds under palms, coral, rubble, shells, driftwood
    R_xy, R_var, R_tilt = [], [], []
    for (x, y), v, b in zip(P_xy, P_var, big):
        if not b or v.startswith(("Juvenile", "Sprout")):
            continue
        for _ in range(int(rng.integers(2, 10))):
            a, d = rng.uniform(0, 6.28), rng.uniform(0.4, 4.5)
            R_xy.append((x + math.cos(a) * d, y + math.sin(a) * d))
            R_var.append(str(rng.choice(["SM_Coconut_Brown", "SM_Coconut_Yellow", "SM_Coconut_Green"],
                                        p=[0.72, 0.12, 0.16])))
        for _ in range(int(rng.integers(1, 4))):
            a, d = rng.uniform(0, 6.28), rng.uniform(1.0, 5.0)
            R_xy.append((x + math.cos(a) * d, y + math.sin(a) * d))
            R_var.append("SM_CoconutFrond_Dry_" + "AB"[int(rng.integers(2))])
    for n, xr_ in ((120, lambda y: (ln("xr", y) + 3, ln("xc", y) - 11)), (60, lambda y: (ln("xc", y) - 10, ln("xo", y))),
                   (8, lambda y: (ln("xv", y) + 2, ln("xl", y) - 2))):
        blk = sample(n, xr_, lambda x, y: 1.0)
        for x, y in blk:
            R_xy.append((x, y))
            R_var.append("SM_CoralBlock_" + "ABCDEF"[int(rng.choice(6, p=[0.25, 0.22, 0.15, 0.08, 0.05, 0.25]))])
    rub = np.vstack([sample(7000, lambda y: (ln("xc", y) - 11, ln("xo", y) + 1), lambda x, y: 1.0),
                     sample(2500, lambda y: (ln("xr", y) + 3, ln("xc", y) - 11), lambda x, y: 1.0),
                     sample(1400, lambda y: (ln("xv", y) + 3, ln("xl", y) - 1), lambda x, y: 1.0),
                     sample(300, lambda y: (ln("xo", y), ln("xv", y)), lambda x, y: 1.0)])
    hr = motu.height(rub[:, 0], rub[:, 1])
    on_wrack = (rub[:, 0] > ln("xv", rub[:, 1])) & (np.abs(hr - 1.2) > 0.35)
    rub = rub[~on_wrack | (rng.random(len(rub)) < 0.08)]
    for x, y in rub:
        R_xy.append((x, y))
        R_var.append("SM_CoralRubble_" + "ABCDEFGH"[int(rng.integers(8))])
    if "SM_LambisShell" in vpr:
        sh = np.vstack([sample(30, lambda y: (ln("xv", y) + 3, ln("xl", y) - 1), lambda x, y: 1.0),
                        sample(20, lambda y: (ln("xr", y) + 4, ln("xc", y) - 12), lambda x, y: 1.0)])
        for x, y in sh:
            R_xy.append((x, y))
            R_var.append("SM_LambisShell")
    for _ in range(6):
        y = rng.uniform(-NEAR_Y, NEAR_Y)
        R_xy.append((ln("xv", y) + rng.uniform(3, 6), y) if rng.random() < 0.6 else (ln("xc", y) - rng.uniform(6, 9), y))
        R_var.append("SM_Driftwood_" + "AB"[int(rng.integers(2))])
    R_xy = np.array(R_xy)
    small = np.array([v.startswith(("SM_CoralRubble", "SM_Lambis")) for v in R_var])
    nut = np.array([v.startswith("SM_Coconut") for v in R_var])
    scl = np.where(small, rng.uniform(0.7, 1.6, len(R_xy)), rng.uniform(0.75, 1.3, len(R_xy)))
    scl = np.where(nut, rng.uniform(0.85, 1.12, len(R_xy)), scl)
    scl = np.where([v.startswith("SM_Lambis") for v in R_var], rng.uniform(0.9, 1.3, len(R_xy)), scl)
    instancer("Props", props_c, np.c_[R_xy, ground_z(motu, R_xy, 0.0)], [vpr[v] for v in R_var],
              orient(motu, R_xy, rng.uniform(0, 6.28, len(R_xy)), 0.9), scl, colls["Scatter_Props"])

    # ---- cameras
    def H(x, y):
        return float(motu.height(np.array([x]), np.array([y]))[0])

    def cam(name, x, y, dz, tx, ty, tz, lens):
        return sc.camera(scn, name, (x, y, H(x, y) + dz), (tx, ty, tz), lens, colls["Cameras"])

    yb = -28.0
    cam("CAM_1_LagoonBeach", ln("xl", yb) - 3.5, yb, 1.65, ln("xv", 30.0) + 1.0, 30.0, 4.5, 24)
    cam("CAM_2_Plantation", (ln("xo", -70.0) + ln("xv", -70.0)) / 2 - 2, -78.0, 1.7, 6.0, -20.0, 3.5, 26)
    cam("CAM_3_GroveToLagoon", ln("xv", 10.0) - 13, 10.0, 1.7, ln("xl", 14.0) + 40, 16.0, 1.0, 24)
    cam("CAM_4_OceanRidge", ln("xc", 25.0) + 1.0, 25.0, 1.75, ln("xr", 85.0) + 4, 85.0, 0.0, 24)
    cam("CAM_5_Aerial", 105.0, -150.0, 62.0, -8.0, 5.0, 0.0, 28)
    scn.camera = colls["Cameras"].objects[0]
    _viewport_material(scn)
    return scn


def _viewport_material(scn):
    for scr in bpy.data.screens:
        for area in scr.areas:
            if area.type == "VIEW_3D":
                sp = area.spaces[0]
                sp.shading.type = "MATERIAL"
                sp.shading.use_scene_world = True
                sp.shading.use_scene_lights = True
                sp.overlay.show_floor = False
                sp.clip_end = 6000.0
