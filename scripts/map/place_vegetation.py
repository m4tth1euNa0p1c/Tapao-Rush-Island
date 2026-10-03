"""Place les arbustes, le couvre-sol et les accessoires de l'atoll d'apres les donnees reelles, dans le repere UE du terrain.

Usage: python scripts/map/place_vegetation.py raraka [--seed 2024] [--density 1.0] [--no-csv]

Regles de pose : docs/reference/assets/catalogue_atoll.md (meme chaine que les cocotiers de place_palms.py).
Entrees (docs/reference/map/<ile>/) : rivage.geojson, batiments.geojson, cocoteraies.geojson, atlas_recif_z15.png,
  terrain/georef.json + terrain/heightmap_16.png (meme surface que les tuiles), vegetation/palms_ue.json (cocotiers poses)
Sorties :
  vegetation/vegetation_ue.json      instances par groupe et par mesh : [x, y, z, pitch, yaw, roll, echelle] (cm, deg),
                                     physique par mesh (Hard / Soft) et liste des blocs (chunks)
  vegetation/preview_vegetation.png  carte de controle de l'atoll ; preview_vegetation_village.png (zoom sur Motutapu)
  raw/<ile>_vegetation/DT_<Ile>_{Hard,Soft}_NN.csv   DataTables GameplayTagTableRow (format de DT_<Ile>_Palms), un bloc
                                     spatial de MAX_CHUNK instances au plus par fichier = un acteur PCG
Physique : Hard = collision (troncs, miki-miki, pandanus, tahinu, cocos, blocs, bois flotte) ;
           Soft = traversable (naupaka, juveniles, fougeres, herbes, debris) : on peut se cacher dans les fourres.

Cote de l'eau : chaque point de terre prend la classe de l'atlas IRD de l'eau la plus proche
(lagon / pente interne -> cote lagon ; platier / tombant / ocean -> cote ocean).
"""
import argparse
import csv
import importlib.util
import json
import math
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


pp = _load("place_palms", ROOT / "scripts" / "map" / "place_palms.py")
bt = pp.bt

MANIFEST = ROOT / "art" / "export" / "ue_manifest.json"
PALM_FOLDER = "/Game/MonkeIsland/Environment/Vegetation/Palms/Meshes"
GROUPS = ("Shrubs", "Thicket", "Trees", "GroundCover", "Props")
UNDER_PALM = ("Straight", "Leaning", "SCurve", "Young", "Sabre")  # cocotiers qui donnent cocos et palmes au sol
MAX_CHUNK = 25000   # instances par acteur PCG : la generation croit comme le carre du nombre de points par acteur
PHYSICS = {"Hard": "H", "Soft": "S"}                          # Hard : collision (troncs, bois, pierre) ; Soft : traversable


def is_soft(name, manifest):
    """Plantes tendres et petits debris : le joueur les traverse (et peut se cacher dans les fourres)."""
    if name.startswith(("SM_Naupaka_", "SM_CoconutPalm_Juvenile", "SM_CoconutPalm_Sprout")):
        return True
    entry = manifest.get(name)
    return entry is not None and entry.get("ucx", 0) == 0


def kd_chunks(xy, max_n):
    """Decoupe spatiale equilibree (k-d, coupe a la mediane sur l'axe le plus long) en blocs de max_n points au plus."""
    todo, out = [np.arange(len(xy))], []
    while todo:
        idx = todo.pop()
        if len(idx) <= max_n:
            out.append(idx)
            continue
        ext = xy[idx].max(axis=0) - xy[idx].min(axis=0)
        order = idx[np.argsort(xy[idx, int(ext[1] > ext[0])], kind="stable")]
        half = len(order) // 2
        todo += [order[:half], order[half:]]
    return sorted(out, key=lambda i: (round(float(xy[i, 1].mean()) / 1e5), float(xy[i, 0].mean())))


def thin(P, r, rng):
    """Semis de Poisson par rejet glouton (grille de hachage, une case = au plus un point)."""
    if len(P) == 0:
        return P
    cell = r / math.sqrt(2)
    grid = {}
    keep = []
    r2 = r * r
    for i in rng.permutation(len(P)):
        x, y = P[i]
        gx, gy = int(math.floor(x / cell)), int(math.floor(y / cell))
        ok = True
        for dx in (-2, -1, 0, 1, 2):
            for dy in (-2, -1, 0, 1, 2):
                j = grid.get((gx + dx, gy + dy))
                if j is not None and (P[j][0] - x) ** 2 + (P[j][1] - y) ** 2 < r2:
                    ok = False
                    break
            if not ok:
                break
        if ok:
            grid[(gx, gy)] = i
            keep.append(i)
    return P[np.array(keep, dtype=np.int64)]


class Atoll:
    """Champs raster de l'ile : terre, distance au rivage, cote (lagon/ocean), batiments, cocoteraies, pente."""

    def __init__(self, ref):
        self.F = pp.Frame(ref)
        F = self.F
        m = F.m_px
        self.land, _ = bt.rasterize_coastline(ref / "rivage.geojson", F.meta, (F.H, F.W))
        self.d_shore, inds = ndi.distance_transform_edt(self.land, return_indices=True)
        self.d_shore *= m
        atlas = np.asarray(Image.open(ref / "atlas_recif_z15.png").convert("RGB")).astype(np.float32)
        cls = bt.classify_atlas(atlas)
        lagoonish = np.isin(cls, [bt.LAGOON, bt.INNER_SLOPE, bt.PINNACLE])
        self.lagoon_side = lagoonish[inds[0], inds[1]]          # pour la terre : eau la plus proche cote lagon
        del inds
        self.reef_flat = (cls == bt.REEF_FLAT) & ~self.land
        self.d_from_land = ndi.distance_transform_edt(~self.land) * m  # pour l'eau : distance a la terre

        bimg = Image.new("1", (F.W, F.H), 0)
        bd = ImageDraw.Draw(bimg)
        for feat in json.loads((ref / "batiments.geojson").read_text(encoding="utf-8"))["features"]:
            for rings in pp.rings_of(feat["geometry"]):
                ring = np.asarray(rings[0], float)
                e, s = F.lonlat(ring[:, 0], ring[:, 1])
                bd.polygon(list(zip((e + F.cx) / m, (s + F.cz) / m)), fill=1)
        self.d_bldg = ndi.distance_transform_edt(~np.asarray(bimg, bool)) * m

        gimg = Image.new("1", (F.W, F.H), 0)
        gd = ImageDraw.Draw(gimg)
        for feat in json.loads((ref / "cocoteraies.geojson").read_text(encoding="utf-8"))["features"]:
            for rings in pp.rings_of(feat["geometry"]):
                ring = np.asarray(rings[0], float)
                e, s = F.lonlat(ring[:, 0], ring[:, 1])
                gd.polygon(list(zip((e + F.cx) / m, (s + F.cz) / m)), fill=1)
        self.grove = np.asarray(gimg, bool)

        self.dz_de = np.gradient(F.hg, F.spacing, axis=1)       # pente de la surface des tuiles
        self.dz_ds = np.gradient(F.hg, F.spacing, axis=0)
        smooth = ndi.gaussian_filter(self.d_shore, 1.5)
        self.g_south, self.g_east = np.gradient(smooth)           # vers l'interieur ; l'eau est a l'oppose
        # vegetation reelle (zones sombres du satellite -> litiere brune dans le materiau du terrain)
        masks = Image.open(ref / "terrain" / "T_Raraka_Masks.png")
        self.veg = (np.asarray(masks.getchannel("R").resize((F.W, F.H), Image.BILINEAR), np.float32) / 255.0) * self.land
        self.sand = (np.asarray(masks.getchannel("G").resize((F.W, F.H), Image.BILINEAR), np.float32) / 255.0) * self.land
        # zones de terre (litiere brune plus forte que le sable) : nature dense ; le reste de la terre est du sable
        self.terre = self.land & (ndi.gaussian_filter(self.veg - self.sand, 1.0) > 0.0)

    def at(self, img, P, order=1):
        return self.F.sample(img.astype(np.float32) if img.dtype == bool else img, P[:, 0], P[:, 1], order)

    def candidates(self, rng, mask, r):
        """Points aleatoires dans les pixels du masque, assez nombreux pour saturer un semis de rayon r."""
        F = self.F
        rows, cols = np.nonzero(mask)
        if len(rows) == 0:
            return np.zeros((0, 2))
        per_px = 3.0 * F.m_px ** 2 / (0.87 * r * r)
        if per_px < 1.0:                                         # semis lache : un pixel sur n
            sel = rng.random(len(rows)) < per_px
            rows, cols, k = rows[sel], cols[sel], 1
        else:
            k = int(math.ceil(per_px))
        rr = np.repeat(rows, k) + rng.random(len(rows) * k)
        cc = np.repeat(cols, k) + rng.random(len(rows) * k)
        return np.c_[cc * F.m_px - F.cx, rr * F.m_px - F.cz]

    def slope_at(self, P):
        F = self.F
        u = (P[:, 0] - F.east0) / F.spacing
        v = (P[:, 1] - F.south0) / F.spacing
        coords = [np.clip(v, 0, F.hg.shape[0] - 1), np.clip(u, 0, F.hg.shape[1] - 1)]
        return (ndi.map_coordinates(self.dz_de, coords, order=1, mode="nearest"),
                ndi.map_coordinates(self.dz_ds, coords, order=1, mode="nearest"))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("island")
    ap.add_argument("--seed", type=int, default=2024)
    ap.add_argument("--density", type=float, default=1.0, help="multiplicateur global de densite (1 = catalogue)")
    ap.add_argument("--no-csv", action="store_true")
    args = ap.parse_args()

    ref = ROOT / "docs" / "reference" / "map" / args.island
    out = ref / "vegetation"
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(args.seed)
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    A = Atoll(ref)
    F = A.F
    m = F.m_px
    dens = max(args.density, 1e-3)

    palms_doc = json.loads((ref / "vegetation" / "palms_ue.json").read_text(encoding="utf-8"))
    trunks = np.array([[r[0] / 100.0, r[1] / 100.0] for v, rows in palms_doc["variants"].items()
                       if not v.startswith(("Juvenile", "Sprout")) for r in rows])
    trunk_tree = cKDTree(trunks)
    under = np.array([[r[0] / 100.0, r[1] / 100.0] for v, rows in palms_doc["variants"].items()
                      if v.startswith(UNDER_PALM) for r in rows])

    def radius(r):
        return r / math.sqrt(dens)

    def band_mask(lo, hi, side=None, grove=None):
        """Pre-filtre raster large (a un pixel pres), le filtre exact se fait sur les points."""
        mk = A.land & (A.d_shore >= lo - 1.5 * m) & (A.d_shore <= hi + 1.5 * m)
        if side == "lagon":
            mk &= A.lagoon_side
        elif side == "ocean":
            mk &= ~A.lagoon_side
        if grove is True:
            mk &= A.grove
        elif grove is False:
            mk &= ~A.grove
        return mk

    def keep_band(P, lo, hi, side=None, grove=None, min_bldg=4.0, min_trunk=1.5, min_z=0.3):
        if len(P) == 0:
            return P
        ok = A.at(A.land, P) > 0.5
        d = A.at(A.d_shore, P)
        ok &= (d >= lo) & (d <= hi)
        if side == "lagon":
            ok &= A.at(A.lagoon_side, P) > 0.5
        elif side == "ocean":
            ok &= A.at(A.lagoon_side, P) < 0.5
        if grove is True:
            ok &= A.at(A.grove, P) > 0.5
        elif grove is False:
            ok &= A.at(A.grove, P) < 0.5
        db = A.at(A.d_bldg, P)
        ok &= db >= min_bldg
        ok &= ~((db < 25.0) & (rng.random(len(P)) < 0.5))        # densite divisee par 2 pres du bati
        if min_trunk > 0:
            ok &= trunk_tree.query(P, k=1)[0] >= min_trunk
        ok &= F.mesh_z(P[:, 0], P[:, 1]) >= min_z
        return P[ok]

    def patches(P, scale_m, keep):
        """Repartition en taches (bruit de valeur a l'echelle scale_m) : garde la fraction keep."""
        if len(P) == 0:
            return P
        n = max(int(scale_m / m), 2)
        noise = pp.value_noise(np.random.default_rng(int(rng.integers(1 << 30))), (F.H // n + 4, F.W // n + 4), 6)
        c = np.clip(((P[:, 0] + F.cx) / m / n).astype(int), 0, noise.shape[1] - 1)
        r_ = np.clip(((P[:, 1] + F.cz) / m / n).astype(int), 0, noise.shape[0] - 1)
        return P[noise[r_, c] > np.quantile(noise, 1 - keep)]

    placements = {g: {} for g in GROUPS}
    stats = {}

    def emit(group, P, weights, kind, scale=(0.85, 1.15), sink=(0.05, 0.10), label=None, yaw=None):
        """kind 'upright' (vertical) ou 'ground' (aligne sur la pente) ; yaw aleatoire sauf s'il est impose."""
        if len(P) == 0:
            return
        names = list(weights)
        p = np.array([weights[k] for k in names], float)
        choice = rng.choice(len(names), len(P), p=p / p.sum())
        yaw = rng.uniform(0, 360, len(P)) if yaw is None else np.asarray(yaw, float)
        s = rng.uniform(scale[0], scale[1], len(P))
        z = F.mesh_z(P[:, 0], P[:, 1]) - rng.uniform(sink[0], sink[1], len(P))
        if kind == "ground":
            gx, gy = A.slope_at(P)
            cy, sy = np.cos(np.radians(yaw)), np.sin(np.radians(yaw))
            fwd = gx * cy + gy * sy                                   # pente dans l'axe +X de l'objet
            side = -gx * sy + gy * cy                                 # pente dans l'axe +Y de l'objet
            pitch, roll = np.degrees(np.arctan(fwd)), -np.degrees(np.arctan(side))
        else:
            pitch, roll = rng.normal(0, 1.0, len(P)), rng.normal(0, 1.0, len(P))
        for k, name in enumerate(names):
            sel = choice == k
            if not sel.any():
                continue
            rows = np.c_[P[sel, 0] * 100, P[sel, 1] * 100, z[sel] * 100, pitch[sel], yaw[sel], roll[sel], s[sel]]
            placements[group].setdefault(name, []).extend(np.round(rows, 2).tolist())
        stats[label or names[0]] = stats.get(label or names[0], 0) + int(len(P))

    def scatter(lo, hi, r, side=None, grove=None, **kw):
        P = A.candidates(rng, band_mask(lo, hi, side, grove), radius(r))
        P = keep_band(P, lo, hi, side, grove, **kw)
        return thin(P, radius(r), rng)

    def terre_scatter(r, d_min=8.0, keep=None, patch=60.0, **kw):
        """Semis dans les zones de terre (nature dense) ; keep < 1 laisse des clairieres a l'echelle patch (m)."""
        P = A.candidates(rng, A.terre & (A.d_shore >= d_min - 1.5 * m), radius(r))
        P = keep_band(P, d_min, 1e9, **kw)
        if len(P):
            P = P[A.at(A.terre, P) > 0.5]
        if keep is not None:
            P = patches(P, patch, keep)
        return thin(P, radius(r), rng)

    def outside_terre(P):
        return P[A.at(A.terre, P) < 0.5] if len(P) else P

    # --- Cocotiers adultes en plus dans les zones de terre (troncs durs : collision) ----------------------
    # Places en premier : les couches suivantes evitent leurs troncs, et ils portent cocos et palmes au sol.
    trees = terre_scatter(11.0, min_bldg=10.0, min_trunk=5.0)
    emit("Trees", trees, {"SM_CoconutPalm_Straight_A": 3, "SM_CoconutPalm_Straight_B": 3, "SM_CoconutPalm_SCurve_A": 1,
                          "SM_CoconutPalm_Young_A": 2, "SM_CoconutPalm_Young_B": 2, "SM_CoconutPalm_Leaning_A": 1,
                          "SM_CoconutPalm_Leaning_B": 1, "SM_CoconutPalm_DeadSnag_A": 0.2},
         "upright", scale=(0.85, 1.15), sink=(0.08, 0.08), label="cocotiers (terre)")
    if len(trees):
        trunks = np.vstack([trunks, trees])
        trunk_tree = cKDTree(trunks)
        under = np.vstack([under, trees])

    # --- Passages a travers les haies bloquantes (gameplay) ---------------------------------------------
    # Naupaka et miki-miki ont une collision : une haie continue fermerait la lisiere. On ouvre un couloir
    # perpendiculaire au rivage tous les 20-40 m (de la plage jusqu'a 16 m a l'interieur).
    seeds = thin(A.candidates(rng, band_mask(2.0, 16.0), 26.0), 26.0, rng)
    seeds = keep_band(seeds, 2.0, 16.0, min_bldg=0.0, min_trunk=0.0, min_z=0.0)
    ge, gs = A.at(A.g_east, seeds), A.at(A.g_south, seeds)
    norm = np.hypot(ge, gs) + 1e-9
    inland = np.c_[ge / norm, gs / norm]
    corridor = np.vstack([seeds + inland * t for t in np.arange(-14.0, 16.0, 1.0)])

    def open_passages(P, clearance=2.6):
        if len(P) == 0 or len(corridor) == 0:
            return P
        hit = cKDTree(P).query_ball_point(corridor, clearance)
        drop = np.zeros(len(P), bool)
        for idx in hit:
            drop[idx] = True
        return P[~drop]

    def hedge(P):
        return open_passages(keep_band(P, 0.0, 1e9, min_bldg=10.0, min_trunk=1.5, min_z=0.3))

    # --- Arbustes ----------------------------------------------------------------------------------------
    naupaka = {"SM_Naupaka_A": 4, "SM_Naupaka_B": 3, "SM_Naupaka_C": 3}
    # naupaka : feuilles tendres, traversable (groupe Thicket) ; densite du sable inchangee
    emit("Thicket", hedge(scatter(4.0, 9.0, 2.6, side="lagon")), naupaka, "upright",
         label="naupaka (haie haut de plage, lagon)")
    emit("Thicket", hedge(patches(scatter(9.0, 14.0, 3.4, side="ocean"), 120, 0.4)), naupaka, "upright",
         label="naupaka (derriere le miki-miki, ocean)")
    emit("Shrubs", hedge(scatter(3.0, 9.0, 2.8, side="ocean")),
         {"SM_MikiMiki_A": 4, "SM_MikiMiki_B": 3, "SM_MikiMiki_C": 3}, "upright", label="miki-miki (crete ocean)")
    emit("Shrubs", hedge(scatter(5.0, 12.0, 18.0)), {"SM_Tahinu_A": 1, "SM_Tahinu_B": 1}, "upright",
         label="tahinu (crete, isole)")
    pandanus = scatter(8.0, 40.0, 30.0, grove=False, min_bldg=10.0)
    groups = [pandanus]
    for _ in range(2):                                                # petits groupes de 2 a 3
        extra = pandanus[rng.random(len(pandanus)) < 0.35]
        a = rng.uniform(0, 2 * math.pi, len(extra))
        dd = rng.uniform(4.0, 7.0, len(extra))
        groups.append(keep_band(extra + np.c_[np.cos(a) * dd, np.sin(a) * dd], 8.0, 45.0, grove=False, min_bldg=10.0))
    emit("Shrubs", np.vstack(groups), {"SM_Pandanus_A": 1, "SM_Pandanus_B": 1}, "upright",
         label="pandanus (lisieres, clairieres)")
    emit("Shrubs", scatter(10.0, 80.0, 40.0, min_bldg=10.0), {"SM_Pandanus_Young": 1}, "upright", label="pandanus jeune")
    stats["passages ouverts dans les haies"] = int(len(seeds))

    # --- Interieur vegetalise : densite concentree dans les zones « marron » (masque vegetation) --------
    def veg_scatter(veg_min, d_min, r, power=1.0, **kw):
        mk = A.land & (A.veg >= veg_min) & (A.d_shore >= d_min - 1.5 * m)
        P = A.candidates(rng, mk, radius(r))
        P = keep_band(P, d_min, 1e9, **kw)
        if len(P):
            v = A.at(A.veg, P)
            P = P[(v >= veg_min) & (rng.random(len(P)) < np.clip(v, 0, 1) ** power)]
        return thin(P, radius(r), rng)

    # grands pandanus dans la vegetation naturelle (hors cocoteraies), canopee verte vue du ciel
    emit("Shrubs", veg_scatter(0.35, 15.0, 16.0, grove=False, min_bldg=10.0, min_trunk=3.0),
         {"SM_Pandanus_A": 1, "SM_Pandanus_B": 1}, "upright", label="pandanus (interieur vegetalise)")
    # bosquets de fourre (naupaka + jeunes pandanus), 2-3 pieds par bosquet, hors zones de terre (deja denses)
    centers = outside_terre(veg_scatter(0.4, 14.0, 14.0, min_bldg=10.0, min_trunk=2.5))
    counts = rng.integers(2, 4, len(centers))
    base = np.repeat(centers, counts, axis=0)
    a = rng.uniform(0, 2 * math.pi, len(base))
    dd = rng.uniform(0.5, 3.0, len(base))
    thicket = keep_band(base + np.c_[np.cos(a) * dd, np.sin(a) * dd], 12.0, 1e9, min_bldg=10.0, min_trunk=2.0)
    young = rng.random(len(thicket)) < 3.0 / 11.0
    emit("Shrubs", thicket[young], {"SM_Pandanus_Young": 1}, "upright", label="bosquets de fourre (pandanus jeunes)")
    emit("Thicket", thicket[~young], {"SM_Naupaka_A": 3, "SM_Naupaka_B": 3, "SM_Naupaka_C": 2}, "upright",
         label="bosquets de fourre (naupaka)")
    stats["bosquets de fourre"] = int(len(centers))

    # --- Nature dense dans les zones de terre : fourre continu ou le joueur disparait ----------------------
    # vieux naupaka de 3,3 a 4,8 m de large et 2,1 a 3,7 m de haut, semis de 2,4 m : environ 85 % du sol couvert
    # (simulation), un adulte de 1,80 m y disparait. Cocotiers juveniles dessous. Tous traversables.
    # Village degage (12 m autour du bati), quelques clairieres (10 %).
    emit("Thicket", terre_scatter(2.4, keep=0.9, min_bldg=12.0, min_trunk=1.0), naupaka, "upright",
         scale=(1.1, 1.6), label="fourre de naupaka (terre)")
    emit("Thicket", terre_scatter(5.0, min_bldg=12.0, min_trunk=1.5),
         {"SM_CoconutPalm_Juvenile_A": 3, "SM_CoconutPalm_Juvenile_B": 2}, "upright", scale=(0.9, 1.4),
         label="cocotiers juveniles (terre)")

    # --- Couvre-sol --------------------------------------------------------------------------------------
    ferns = {"SM_Fern_A": 1, "SM_Fern_B": 1}
    emit("GroundCover", outside_terre(veg_scatter(0.3, 10.0, 6.5, power=0.7, min_trunk=0.8, min_bldg=4.0)),
         ferns, "ground", scale=(0.7, 1.3), sink=(0.0, 0.03), label="fougeres (interieur vegetalise)")
    emit("GroundCover", terre_scatter(3.5, min_trunk=0.8, min_bldg=4.0), ferns, "ground", scale=(0.8, 1.4),
         sink=(0.0, 0.03), label="fougeres (terre, sous le fourre)")
    grass = outside_terre(patches(scatter(7.0, 60.0, 5.0, min_trunk=0.5), 40, 0.45))
    if len(grass):
        grass = grass[A.at(A.veg, grass) < 0.5]                    # clairieres et arriere-plage, pas le sous-bois dense
    emit("GroundCover", grass, {"SM_Grass_Short": 2, "SM_Grass_Tuft": 1}, "ground", scale=(0.7, 1.4),
         sink=(0.0, 0.02), label="herbe (clairieres, arriere-plage)")
    emit("GroundCover", patches(scatter(3.0, 7.0, 3.5, min_trunk=0.5), 30, 0.5), {"SM_Grass_Dry": 1}, "ground",
         scale=(0.7, 1.4), sink=(0.0, 0.02), label="herbe seche (haut de plage)")
    emit("GroundCover", patches(scatter(1.5, 6.0, 7.0, min_trunk=0.0, min_bldg=3.0), 50, 0.5),
         {"SM_Ipomoea_A": 1, "SM_Ipomoea_B": 1}, "ground", scale=(0.7, 1.3), sink=(0.0, 0.02),
         label="ipomee (sable du haut de plage)")

    # --- Accessoires sous les cocotiers (cocos, palmes seches, cocos germes) -----------------------------
    def around(points, prob, nmin, nmax, rmin, rmax):
        """Points autour des troncs ; renvoie aussi la direction tronc -> point (deg, 0 = est, 90 = sud)."""
        sel = points[rng.random(len(points)) < prob * min(dens, 3.0)]
        counts = rng.integers(nmin, nmax + 1, len(sel))
        base = np.repeat(sel, counts, axis=0)
        a = rng.uniform(0, 2 * math.pi, len(base))
        d = rng.uniform(rmin, rmax, len(base))
        P = base + np.c_[np.cos(a) * d, np.sin(a) * d]
        P2 = keep_band(P, 0.5, 1e9, min_bldg=2.0, min_trunk=0.0, min_z=0.2)
        kept = np.isin(P.view([("", P.dtype)] * 2).ravel(), P2.view([("", P2.dtype)] * 2).ravel())
        return P[kept], np.degrees(a[kept])

    P, _ = around(under, 0.35, 1, 3, 0.5, 5.0)
    emit("Props", P, {"SM_Coconut_Brown": 7, "SM_Coconut_Green": 2, "SM_Coconut_Yellow": 1}, "ground",
         scale=(0.85, 1.15), sink=(0.01, 0.03), label="cocos au sol")
    # palmes seches : base pres du tronc, la palme s'etend vers +X -> yaw = direction vers l'exterieur
    P, outward = around(under, 0.35, 1, 1, 0.3, 1.5)
    emit("Props", P, {"SM_CoconutFrond_Dry_A": 1, "SM_CoconutFrond_Dry_B": 1}, "ground", scale=(0.8, 1.2),
         sink=(0.0, 0.02), label="palmes seches", yaw=outward + rng.normal(0, 15, len(outward)))
    P, _ = around(under, 0.15, 1, 2, 1.0, 7.0)
    emit("Props", P, {"SM_CoconutPalm_Sprout_A": 1, "SM_CoconutPalm_Sprout_B": 1}, "upright", scale=(0.8, 1.2),
         sink=(0.02, 0.05), label="cocos germes")

    # --- Corail, bois flotte, coquillages ----------------------------------------------------------------
    blocks = {"SM_CoralBlock_A": 30, "SM_CoralBlock_B": 25, "SM_CoralBlock_C": 18, "SM_CoralBlock_D": 12,
              "SM_CoralBlock_E": 9, "SM_CoralBlock_F": 6}
    emit("Props", scatter(0.0, 6.0, 14.0, side="ocean", min_trunk=0.0, min_z=0.0), blocks, "ground",
         scale=(0.7, 1.3), sink=(0.05, 0.15), label="blocs de corail (crete ocean)")
    emit("Props", scatter(0.0, 4.0, 60.0, side="lagon", min_trunk=0.0, min_z=0.0), blocks, "ground",
         scale=(0.6, 1.0), sink=(0.05, 0.15), label="blocs de corail (plage lagon)")
    # platier immerge cote ocean, jusqu'a 120 m du rivage
    reef = A.reef_flat & (A.d_from_land <= 120.0)
    P = A.candidates(rng, reef, radius(40.0))
    if len(P):
        ok = (A.at(reef, P) > 0.5) & (F.mesh_z(P[:, 0], P[:, 1]) > -1.0)
        P = thin(P[ok], radius(40.0), rng)
    emit("Props", P, blocks, "ground", scale=(0.7, 1.4), sink=(0.05, 0.2), label="blocs de corail (platier)")
    rubble = {f"SM_CoralRubble_{c}": 1 for c in "ABCDEFGH"}
    emit("Props", scatter(1.0, 10.0, 6.0, side="ocean", min_trunk=0.3, min_z=0.0), rubble, "ground",
         scale=(0.7, 1.4), sink=(0.0, 0.03), label="gravats de corail (crete de tempete)")
    emit("Props", scatter(0.3, 2.5, 6.0, side="lagon", min_trunk=0.0, min_z=0.0, min_bldg=2.0), rubble, "ground",
         scale=(0.7, 1.4), sink=(0.0, 0.03), label="gravats (laisse de mer, lagon)")
    emit("Props", scatter(1.0, 6.0, 70.0, min_trunk=0.0, min_z=0.0), {"SM_Driftwood_A": 1, "SM_Driftwood_B": 1},
         "ground", scale=(0.8, 1.2), sink=(0.02, 0.06), label="bois flotte")
    emit("Props", scatter(0.3, 3.0, 20.0, min_trunk=0.0, min_z=0.0, min_bldg=2.0), {"SM_LambisShell": 1}, "ground",
         scale=(0.8, 1.2), sink=(0.0, 0.02), label="coquillages (laisse de mer)")

    # --- Sorties -----------------------------------------------------------------------------------------
    names = {n for g in placements.values() for n in g}
    missing = sorted(n for n in names - set(manifest) if not n.startswith("SM_CoconutPalm_"))
    if missing:
        raise SystemExit(f"meshes absents du manifeste : {missing}")

    def mesh_path(n):
        folder = manifest[n]["folder"] if n in manifest else PALM_FOLDER
        return f"{folder}/{n}.{n}"

    totals = {g: sum(len(v) for v in d.values()) for g, d in placements.items()}
    physics = {n: ("Soft" if is_soft(n, manifest) else "Hard") for n in names}
    doc = {"island": args.island,
           "frame": "UE cm : X = est, Y = sud, Z = haut ; instances [x, y, z, pitch, yaw, roll, echelle]",
           "stats": {"seed": args.seed, "density": dens, "par_groupe": totals, "par_regle": stats,
                     "par_physique": {c: int(sum(len(placements[g][n]) for g in placements for n in placements[g]
                                                 if physics[n] == c)) for c in PHYSICS},
                     "total": int(sum(totals.values()))},
           "meshes": {n: mesh_path(n) for n in sorted(names)},
           "physics": physics,
           "groups": placements}

    # blocs spatiaux par physique (un acteur PCG par bloc) : Hard = collision, Soft = traversable
    chunks = []
    raw = ROOT / "docs" / "reference" / "map" / "raw" / f"{args.island}_vegetation"
    if not args.no_csv:
        raw.mkdir(parents=True, exist_ok=True)
        for old in list(raw.glob(f"DT_{args.island.capitalize()}_*.csv")):
            if not old.stem.endswith("_Palms"):
                old.unlink()
    for cls, prefix in PHYSICS.items():
        rows_n, rows_v = [], []
        for g in placements:
            for n, rows in placements[g].items():
                if physics[n] == cls:
                    rows_n += [n] * len(rows)
                    rows_v += rows
        if not rows_v:
            continue
        V = np.asarray(rows_v, float)
        for k, idx in enumerate(kd_chunks(V[:, :2], MAX_CHUNK)):
            name = f"DT_{args.island.capitalize()}_{cls}_{k:02d}"
            lo, hi = V[idx, :2].min(axis=0), V[idx, :2].max(axis=0)
            chunks.append({"name": name, "physics": cls, "count": int(len(idx)),
                           "bbox_cm": [round(float(v), 1) for v in (*lo, *hi)]})
            if args.no_csv:
                continue
            with open(raw / f"{name}.csv", "w", newline="", encoding="utf-8") as f:
                w = csv.writer(f, quoting=csv.QUOTE_MINIMAL)
                w.writerow(["---", "Tag", "DevComment"])
                for j, i in enumerate(sorted(idx, key=lambda i: rows_n[i])):
                    x, y, z, pitch, yaw, roll, s = V[i]
                    xf = f"{x:.1f},{y:.1f},{z:.1f}|{pitch:.2f},{yaw:.1f},{roll:.2f}|{s:.3f},{s:.3f},{s:.3f}"
                    w.writerow([f"{prefix}{k:02d}_{j:06d}", doc["meshes"][rows_n[i]], xf])
    doc["chunks"] = chunks
    if not args.no_csv:   # villages generes (scripts/map/place_villages.py) : rien sous les maisons, la place, la route
        vc = _load("village_clearing", ROOT / "scripts" / "map" / "village_clearing.py")
        cleared = vc.refresh(raw, [f"{c['name']}.csv" for c in chunks], vc.Clearing.load(args.island))
        if any(cleared.values()):
            print(f"villages : {sum(cleared.values())} instances retirees sous les villages "
                  f"({vc.memory_path(args.island).relative_to(ROOT)}), copies completes dans {raw.name}/_orig")
    (out / "vegetation_ue.json").write_text(json.dumps(doc, ensure_ascii=False), encoding="utf-8")
    for cls in PHYSICS:
        sel = [c for c in chunks if c["physics"] == cls]
        if sel:
            print(f"{cls}: {sum(c['count'] for c in sel)} instances en {len(sel)} blocs "
                  f"(max {max(c['count'] for c in sel)}) -> {raw.relative_to(ROOT)}/DT_*_{cls}_NN.csv")

    # apercus : atoll entier et zoom sur le village
    colors = {"Shrubs": (40, 120, 40), "Thicket": (20, 90, 30), "Trees": (110, 80, 40),
              "GroundCover": (150, 190, 70), "Props": (230, 120, 40)}
    for name, (e0, s0, e1, s1, px) in {"preview_vegetation.png": (-F.cx, -F.cz, F.cx, F.cz, 4.0 * m),
                                       "preview_vegetation_village.png": (-7100, -10900, -6000, -9900, 1.0)}.items():
        W_, H_ = int((e1 - e0) / px), int((s1 - s0) / px)
        img = Image.new("RGB", (W_, H_), (30, 90, 160))
        land_small = Image.fromarray((A.land * 255).astype(np.uint8)).resize(
            (int(F.W * m / px), int(F.H * m / px)), Image.NEAREST)
        img.paste(Image.new("RGB", land_small.size, (225, 210, 170)),
                  (int((-F.cx - e0) / px), int((-F.cz - s0) / px)), land_small)
        dr = ImageDraw.Draw(img)
        for tx, ty in trunks:
            if e0 <= tx < e1 and s0 <= ty < s1 and (px < 2 or rng.random() < 0.2):
                dr.point(((tx - e0) / px, (ty - s0) / px), fill=(90, 60, 30))
        for group in ("GroundCover", "Props", "Thicket", "Shrubs", "Trees"):
            for mesh, rows in placements[group].items():
                arr = np.asarray(rows)
                if len(arr) == 0:
                    continue
                size = manifest.get(mesh, {}).get("size_m") or ([1.2, 1.2, 1.0] if group == "Trees" else [1.0, 1.0, 1.0])
                rad = max(0.5, 0.5 * max(size[0], size[1]) / px)       # emprise reelle du mesh
                sel = (arr[:, 0] / 100 >= e0) & (arr[:, 0] / 100 < e1) & (arr[:, 1] / 100 >= s0) & (arr[:, 1] / 100 < s1)
                for x, y in arr[sel, :2] / 100:
                    cxp, cyp = (x - e0) / px, (y - s0) / px
                    if rad <= 0.75:
                        dr.point((cxp, cyp), fill=colors[group])
                    else:
                        dr.ellipse((cxp - rad, cyp - rad, cxp + rad, cyp + rad), fill=colors[group])
        img.save(out / name)

    print(json.dumps(doc["stats"], ensure_ascii=False, indent=1))


if __name__ == "__main__":
    main()
