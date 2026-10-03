"""Place les cocotiers d'une ile reelle d'apres les donnees geospatiales, dans le repere UE du terrain.

Usage: python scripts/map/place_palms.py raraka [--seed 2023] [--natural-density 15]

Entrees (docs/reference/map/<ile>/) : cocoteraies.geojson (inventaire DAG 2023), rivage.geojson (trait de cote),
batiments.geojson, terrain/georef.json + terrain/heightmap_16.png (meme surface que les tuiles du terrain).
Sorties (docs/reference/map/<ile>/vegetation/) :
  palms_ue.json       instances par variante : [x_cm, y_cm, z_cm, cap_deg, echelle] (cap = direction d'inclinaison)
  preview_palms.png   carte de controle (terre, cocoteraies, cocotiers par categorie)

Logique d'emplacement (attributs de l'inventaire des cocoteraies) :
  disposition REGULIERE   plantation en quinconce, rangs alignes sur le grand axe de la parcelle, manquants ~7 %
  disposition IRREGULIERE semis de Poisson (pas de grille)
  recouvrement            densite : 50-75 % -> ~107 pieds/ha ; 25-50 % -> ~70 pieds/ha
  age JEUNE               jeunes cocotiers et juveniles
  etat ARBUSTIVE          sous-bois non entretenu : regeneration (juveniles) entre les troncs
  emergents               part de grands vieux cocotiers (droits, +10 a 20 % de hauteur)
  forme DECOUPEE          clairieres (trouees) dans la parcelle
  lisiere < 12 m du rivage cocotiers penches / en sabre vers l'eau (lumiere, alizes), quelques couches
  exclusions              plage active < 3 m du rivage, batiments < 4 m, sol sous +0,5 m
Hors cocoteraies, sur la terre : cocotiers naturels clairsemes, 30/ha en bande littorale (4-15 m) et
20/ha a l'interieur (lancer de flechettes sur toute la terre), meme logique de lisiere.
"""
import argparse
import importlib.util
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

ROOT = Path(__file__).resolve().parents[2]
EARTH_R = 6378137.0

_spec = importlib.util.spec_from_file_location("build_terrain", ROOT / "scripts" / "map" / "build_terrain.py")
bt = importlib.util.module_from_spec(_spec)
_spec.loader.exec_module(bt)

MATURE = {"Straight_A": 30, "Straight_B": 26, "Leaning_A": 6, "Leaning_B": 10, "SCurve_A": 8, "Young_A": 5,
          "Young_B": 7, "DeadSnag_A": 2}
YOUNG_GROVE = {"Young_A": 35, "Young_B": 40, "Juvenile_A": 15, "Straight_B": 10}
NATURAL = {"Straight_A": 20, "Straight_B": 24, "Leaning_A": 10, "Leaning_B": 12, "SCurve_A": 12, "Young_A": 6,
           "Young_B": 8, "Juvenile_A": 4, "DeadSnag_A": 4}
EMERGENT_SHARE = {"AUCUN": 0.0, "RARE": 0.02, "PEU_COMMUN": 0.06, "DOMINANT": 0.15}


class Frame:
    """Georeferencement du terrain : lon/lat -> metres (est, sud) depuis le centre ; hauteur identique au mesh."""

    def __init__(self, ref):
        g = json.loads((ref / "terrain" / "georef.json").read_text(encoding="utf-8"))
        sg = g["source_grid"]
        self.W, self.H = sg["width"], sg["height"]
        self.xmin, self.ymin, self.xmax, self.ymax = sg["merc_bounds"]
        self.merc_px = sg["merc_px"]
        self.m_px = g["ground_m_per_px"]
        self.cx, self.cz = self.W * self.m_px / 2, self.H * self.m_px / 2
        hm = g["heightmap_16"]
        raw = np.asarray(Image.open(ref / "terrain" / hm["file"]), np.float64)
        self.h = hm["min_m"] + raw / 65535.0 * (hm["max_m"] - hm["min_m"])
        s = g["mesh"]["step_px"]
        H2, W2 = self.H // s, self.W // s
        self.hg = self.h[:H2 * s, :W2 * s].reshape(H2, s, W2, s).mean((1, 3))
        self.spacing = s * self.m_px
        self.east0 = (s / 2) * self.m_px - self.cx
        self.south0 = (s / 2) * self.m_px - self.cz
        self.meta = {"merc_bounds": sg["merc_bounds"], "merc_px": self.merc_px}

    def lonlat(self, lon, lat):
        mx = EARTH_R * np.radians(lon)
        my = EARTH_R * np.log(np.tan(np.pi / 4 + np.radians(lat) / 2))
        return ((mx - self.xmin) / self.merc_px * self.m_px - self.cx,
                (self.ymax - my) / self.merc_px * self.m_px - self.cz)

    def mesh_z(self, east, south):
        """Hauteur de la surface triangulee des tuiles (triangles (a,c,b) et (b,c,d), diagonale b-c)."""
        u = (np.asarray(east) - self.east0) / self.spacing
        v = (np.asarray(south) - self.south0) / self.spacing
        j = np.clip(np.floor(u).astype(int), 0, self.hg.shape[1] - 2)
        i = np.clip(np.floor(v).astype(int), 0, self.hg.shape[0] - 2)
        fu, fv = u - j, v - i
        ha, hb = self.hg[i, j], self.hg[i, j + 1]
        hc, hd = self.hg[i + 1, j], self.hg[i + 1, j + 1]
        upper = fu + fv <= 1
        return np.where(upper, ha + fu * (hb - ha) + fv * (hc - ha),
                        hd + (1 - fu) * (hc - hd) + (1 - fv) * (hb - hd))

    def sample(self, img, east, south, order=1):
        """Echantillonne un raster de la grille source (pixel j centre en (j + 0.5) * m_px - cx)."""
        c = (np.asarray(east) + self.cx) / self.m_px - 0.5
        r = (np.asarray(south) + self.cz) / self.m_px - 0.5
        return ndi.map_coordinates(img, [r, c], order=order, mode="nearest")


def rings_of(geom):
    t = geom["type"]
    if t == "Polygon":
        return [geom["coordinates"]]
    if t == "MultiPolygon":
        return geom["coordinates"]
    return []


def inside(poly, pts):
    """Regle pair-impair, vectorisee. poly (n,2), pts (m,2)."""
    x, y = pts[:, 0][:, None], pts[:, 1][:, None]
    x0, y0 = poly[:, 0][None, :], poly[:, 1][None, :]
    x1, y1 = np.roll(poly[:, 0], -1)[None, :], np.roll(poly[:, 1], -1)[None, :]
    cond = (y0 > y) != (y1 > y)
    xi = x0 + (y - y0) * (x1 - x0) / np.where(y1 == y0, 1e-12, y1 - y0)
    return (cond & (x < xi)).sum(1) % 2 == 1


def poisson(rng, bounds, r, accept_batch, k=24):
    """Semis de Poisson (Bridson) dans un rectangle ; accept_batch(pts (n,2)) -> bool (n,)."""
    x0, x1, y0, y1 = bounds
    cell = r / math.sqrt(2)
    gw, gh = int((x1 - x0) / cell) + 1, int((y1 - y0) / cell) + 1
    grid = -np.ones((gh, gw), np.int64)
    pts, active = [], []

    def ok(p):
        gx, gy = int((p[0] - x0) / cell), int((p[1] - y0) / cell)
        for yy in range(max(0, gy - 2), min(gh, gy + 3)):
            for xx in range(max(0, gx - 2), min(gw, gx + 3)):
                jx = grid[yy, xx]
                if jx >= 0 and (pts[jx][0] - p[0]) ** 2 + (pts[jx][1] - p[1]) ** 2 < r * r:
                    return False
        return True

    def add(p):
        pts.append(p)
        active.append(len(pts) - 1)
        grid[int((p[1] - y0) / cell), int((p[0] - x0) / cell)] = len(pts) - 1

    seeds = np.c_[rng.uniform(x0, x1, 400), rng.uniform(y0, y1, 400)]
    for p in seeds[accept_batch(seeds)][:30]:
        if ok(p):
            add(tuple(p))
    while active:
        i = int(rng.integers(len(active)))
        p = pts[active[i]]
        a = rng.uniform(0, 2 * math.pi, k)
        d = rng.uniform(r, 2 * r, k)
        cand = np.c_[p[0] + np.cos(a) * d, p[1] + np.sin(a) * d]
        inb = (cand[:, 0] >= x0) & (cand[:, 0] < x1) & (cand[:, 1] >= y0) & (cand[:, 1] < y1)
        good = np.zeros(k, bool)
        good[inb] = accept_batch(cand[inb])
        for q in cand[good]:
            if ok(q):
                add(tuple(q))
                break
        else:
            active.pop(i)
    return np.array(pts).reshape(-1, 2)


def quincunx(rng, poly, a, jitter=0.5):
    """Plantation en quinconce alignee sur le grand axe du polygone."""
    c = poly.mean(0)
    seg = np.diff(np.vstack([poly, poly[:1]]), axis=0)
    w = np.linalg.norm(seg, axis=1)
    mids = poly + seg / 2 - c
    cov = (mids * w[:, None]).T @ mids / max(w.sum(), 1e-9)
    evals, evecs = np.linalg.eigh(cov)
    ax = evecs[:, np.argmax(evals)]
    ang = math.atan2(ax[1], ax[0])
    R = np.array([[math.cos(ang), -math.sin(ang)], [math.sin(ang), math.cos(ang)]])
    local = (poly - c) @ R
    lo, hi = local.min(0) - a, local.max(0) + a
    dy = a * math.sqrt(3) / 2
    ph = rng.uniform(0, a, 2)
    pts = []
    for k, yy in enumerate(np.arange(lo[1] + ph[1], hi[1], dy)):
        xs = np.arange(lo[0] + ph[0] + (a / 2 if k % 2 else 0), hi[0], a)
        pts.append(np.c_[xs, np.full(len(xs), yy)])
    P = np.vstack(pts) + rng.normal(0, jitter, (sum(len(p) for p in pts), 2))
    return P @ R.T + c, math.degrees(ang)


def pick(rng, weights, n):
    names = list(weights)
    p = np.array([weights[k] for k in names], float)
    return [names[i] for i in rng.choice(len(names), n, p=p / p.sum())]


def value_noise(rng, shape, scale):
    g = rng.random((shape[0] // scale + 3, shape[1] // scale + 3))
    return ndi.zoom(g, scale, order=3)[:shape[0], :shape[1]]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("island")
    ap.add_argument("--seed", type=int, default=2023)
    ap.add_argument("--natural-density", type=float, default=20.0, help="cocotiers/ha hors cocoteraies (interieur)")
    ap.add_argument("--shore-density", type=float, default=30.0, help="cocotiers/ha hors cocoteraies, bande 4-15 m")
    args = ap.parse_args()
    ref = ROOT / "docs" / "reference" / "map" / args.island
    out = ref / "vegetation"
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(args.seed)
    F = Frame(ref)
    m = F.m_px

    # --- terre, distance au rivage, direction vers l'eau, batiments ------------------------------------
    land, _ = bt.rasterize_coastline(ref / "rivage.geojson", F.meta, (F.H, F.W))
    d_shore = ndi.distance_transform_edt(land) * m
    ds = ndi.gaussian_filter(d_shore, 1.5)
    g_south, g_east = np.gradient(ds)
    bimg = Image.new("1", (F.W, F.H), 0)
    bd = ImageDraw.Draw(bimg)
    for feat in json.loads((ref / "batiments.geojson").read_text(encoding="utf-8"))["features"]:
        for rings in rings_of(feat["geometry"]):
            ring = np.asarray(rings[0], float)
            e, s = F.lonlat(ring[:, 0], ring[:, 1])
            bd.polygon(list(zip((e + F.cx) / m, (s + F.cz) / m)), fill=1)
    d_bldg = ndi.distance_transform_edt(~np.asarray(bimg, bool)) * m

    groves = []
    gimg = Image.new("L", (F.W, F.H), 0)
    gd = ImageDraw.Draw(gimg)
    for feat in json.loads((ref / "cocoteraies.geojson").read_text(encoding="utf-8"))["features"]:
        for rings in rings_of(feat["geometry"]):
            ring = np.asarray(rings[0], float)
            e, s = F.lonlat(ring[:, 0], ring[:, 1])
            poly = np.c_[e, s]
            groves.append((poly, feat["properties"]))
            gd.polygon(list(zip((e + F.cx) / m, (s + F.cz) / m)), fill=255)
    grove_mask = np.asarray(gimg) > 0

    def valid(P, min_shore=3.0):
        if len(P) == 0:
            return np.zeros(0, bool)
        on_land = F.sample(land.astype(np.float32), P[:, 0], P[:, 1], 1) > 0.5
        dsh = F.sample(d_shore, P[:, 0], P[:, 1])
        db = F.sample(d_bldg, P[:, 0], P[:, 1])
        z = F.mesh_z(P[:, 0], P[:, 1])
        return on_land & (dsh >= min_shore) & (db >= 4.0) & (z >= 0.5)

    clear = value_noise(rng, (F.H // 4, F.W // 4), 12)  # trouees des parcelles decoupees (bruit ~220 m)

    rows = []  # (east, south, variant, cap_deg, scale, category)

    def edge_rules(P, base_var, cat):
        """Lisiere : penche vers l'eau ; ailleurs cap aleatoire."""
        dsh = F.sample(d_shore, P[:, 0], P[:, 1])
        ge = F.sample(g_east, P[:, 0], P[:, 1])
        gs = F.sample(g_south, P[:, 0], P[:, 1])
        to_water = np.degrees(np.arctan2(-gs, -ge))
        for k, (e, s) in enumerate(P):
            v = base_var[k]
            cap = rng.uniform(0, 360)
            d = dsh[k]
            if v not in ("Juvenile_A", "Juvenile_B", "DeadSnag_A"):
                if d < 12 and rng.random() < (0.85 if d < 7 else 0.55):
                    v = (["Sabre_A", "Sabre_B", "Leaning_A", "Leaning_B"][int(rng.choice(4, p=[.35, .15, .3, .2]))]
                         if d < 7 else ["Leaning_A", "Leaning_B", "Sabre_A"][int(rng.choice(3, p=[.45, .4, .15]))])
                    cap = to_water[k] + rng.normal(0, 18)
                elif 7 < d < 22 and rng.random() < 0.006:
                    v = "Fallen_A"
                    cap = to_water[k] + rng.normal(0, 25)
            rows.append((e, s, v, cap % 360, cat))

    stats = {"cocoteraies": len(groves)}
    for gi, (poly, p) in enumerate(groves):
        lo, hi = poly.min(0), poly.max(0)
        cover50 = p.get("recouvrement", "") == "ABONDANT_25-50"
        if p.get("disposition") == "REGULIERE":
            a = 12.5 if cover50 else 10.0
            P, _ = quincunx(rng, poly, a)
            P = P[inside(poly, P)]
            P = P[rng.random(len(P)) > 0.07]
        else:
            r = 10.0 if cover50 else 8.0
            P = poisson(rng, (lo[0], hi[0], lo[1], hi[1]), r, lambda Q: inside(poly, Q))
        if p.get("forme") == "DECOUPEE" and len(P):
            cv = F.sample(clear, (P[:, 0] + F.cx) / 4 - F.cx, (P[:, 1] + F.cz) / 4 - F.cz, 1)
            P = P[cv < 0.74]
        P = P[valid(P)]
        db = F.sample(d_bldg, P[:, 0], P[:, 1])
        P = P[(db > 25) | (rng.random(len(P)) < 0.5)]  # jardins du village : moins dense
        weights = YOUNG_GROVE if p.get("age") == "JEUNE" else dict(MATURE)
        if p.get("etat") == "ARBUSTIVE":
            weights = dict(weights, DeadSnag_A=weights.get("DeadSnag_A", 0) + 2)
        var = pick(rng, weights, len(P))
        emer = EMERGENT_SHARE.get(p.get("emergents"), 0.0)
        n0 = len(rows)
        edge_rules(P, var, "cocoteraie")
        for k in range(n0, len(rows)):
            e, s, v, cap, cat = rows[k]
            sc = rng.uniform(0.88, 1.12)
            if v.startswith("Straight") and rng.random() < emer * 3:
                v, sc = "Straight_A", rng.uniform(1.1, 1.22)
            rows[k] = (e, s, v, cap, sc, cat)
        # regeneration dans le sous-bois (juveniles entre les troncs)
        pj = 0.35 if p.get("etat") == "ARBUSTIVE" else 0.06
        J = poisson(rng, (lo[0], hi[0], lo[1], hi[1]), 4.5, lambda Q: inside(poly, Q))
        J = J[valid(J, 5.0) & (rng.random(len(J)) < pj)]
        if len(P) and len(J):
            dmin = np.sqrt(((J[:, None, :] - P[None, :, :]) ** 2).sum(-1)).min(1)
            J = J[dmin > 2.5]
        for e, s in J:
            rows.append((e, s, "Juvenile_A" if rng.random() < 0.6 else "Juvenile_B", rng.uniform(0, 360),
                         rng.uniform(0.8, 1.25), "regeneration"))

    # --- cocotiers naturels hors cocoteraies (lancer de flechettes sur toute la terre) ----------------
    r_int = math.sqrt(0.7 / (args.natural_density / 1e4))
    r_band = math.sqrt(0.7 / (args.shore_density / 1e4))
    gm = ndi.binary_dilation(grove_mask, iterations=1).astype(np.float32)
    lr, lc = np.nonzero(land)
    n_cand = int(land.sum() * m * m / (r_band * r_band) * 6)
    pick_px = rng.integers(0, len(lr), n_cand)
    C = np.c_[(lc[pick_px] + rng.random(n_cand)) * m - F.cx, (lr[pick_px] + rng.random(n_cand)) * m - F.cz]
    C = C[valid(C, 4.0) & (F.sample(gm, C[:, 0], C[:, 1], 0) < 0.5)]
    dC = F.sample(d_shore, C[:, 0], C[:, 1])
    rC = np.where(dC < 15.0, r_band, r_int)
    cell = r_band / math.sqrt(2)
    reach = int(math.ceil(r_int / cell)) + 1
    grid = {}
    keep = []
    for k in rng.permutation(len(C)):
        px_, py_ = C[k]
        gx, gy = int(px_ // cell), int(py_ // cell)
        ok = True
        for xx in range(gx - reach, gx + reach + 1):
            for yy in range(gy - reach, gy + reach + 1):
                for j in grid.get((xx, yy), ()):
                    rr = max(rC[k], rC[j])
                    if (C[j, 0] - px_) ** 2 + (C[j, 1] - py_) ** 2 < rr * rr:
                        ok = False
                        break
                if not ok:
                    break
            if not ok:
                break
        if ok:
            grid.setdefault((gx, gy), []).append(k)
            keep.append(k)
    N = C[np.array(keep, int)]
    n0 = len(rows)
    edge_rules(N, pick(rng, NATURAL, len(N)), "naturel")
    for k in range(n0, len(rows)):
        e, s, v, cap, cat = rows[k]
        rows[k] = (e, s, v, cap, rng.uniform(0.85, 1.12), cat)

    # --- sortie --------------------------------------------------------------------------------------
    E = np.array([r[0] for r in rows])
    S = np.array([r[1] for r in rows])
    Z = F.mesh_z(E, S) - 0.08
    variants = {}
    for (e, s, v, cap, sc, cat), z in zip(rows, Z):
        variants.setdefault(v, []).append([round(e * 100, 1), round(s * 100, 1), round(float(z) * 100, 1),
                                           round(cap, 1), round(sc, 3)])
    cats = {}
    for r in rows:
        cats[r[5]] = cats.get(r[5], 0) + 1
    grove_ha = sum(abs(0.5 * (np.dot(pl[:, 0], np.roll(pl[:, 1], 1)) - np.dot(pl[:, 1], np.roll(pl[:, 0], 1))))
                   for pl, _ in groves) / 1e4
    stats.update({"total": len(rows), "par_categorie": cats, "par_variante": {k: len(v) for k, v in variants.items()},
                  "cocoteraies_ha": round(grove_ha, 1),
                  "densite_cocoteraies_ha": round(cats.get("cocoteraie", 0) / grove_ha, 1),
                  "seed": args.seed})
    doc = {"island": args.island, "frame": "UE cm : X = est, Y = sud, Z = haut ; cap = direction d'inclinaison (deg, "
                                          "0 = est, 90 = sud)", "stats": stats, "variants": variants}
    (out / "palms_ue.json").write_text(json.dumps(doc, separators=(",", ":")), encoding="utf-8")

    # apercu (1/2 resolution)
    sc_ = 0.5
    img = Image.new("RGB", (int(F.W * sc_), int(F.H * sc_)), (18, 52, 84))
    lm = Image.fromarray((land * 255).astype(np.uint8)).resize(img.size)
    img.paste((214, 200, 160), mask=lm)
    gmask = Image.fromarray((grove_mask * 255).astype(np.uint8)).resize(img.size)
    img.paste((150, 170, 110), mask=gmask)
    dr = ImageDraw.Draw(img)
    colors = {"cocoteraie": (20, 90, 20), "regeneration": (120, 200, 60), "naturel": (40, 110, 160)}
    for (e, s, v, cap, sc, cat) in rows:
        x, y = (e + F.cx) / m * sc_, (s + F.cz) / m * sc_
        col = (220, 110, 20) if v.startswith(("Sabre", "Leaning", "Fallen")) and cat != "regeneration" else colors[cat]
        dr.point((x, y), fill=col)
    img.save(out / "preview_palms.png")
    print(json.dumps(stats, ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()
