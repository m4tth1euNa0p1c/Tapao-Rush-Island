"""Construit le terrain d'un atoll reel pour Unreal : classes, hauteurs/bathymetrie, masques, meshes glTF tuiles.

Usage: python scripts/map/build_terrain.py <ile> [--step 2] [--tiles 8]

Entrees (docs/reference/map/<ile>/) :
  raw_sat_z15.png (+.json)       mosaique satellite Te Fenua (reference, jamais embarquee)
  atlas_recif_z15.png (+.json)   atlas des recifs IRD 2005, meme grille que la mosaique
  rivage.geojson                 trait de cote officiel (contours fermes des motu)
Sorties :
  docs/reference/map/<ile>/terrain/   apercus, masques, georef.json, heightmap 16 bits
  docs/reference/map/raw/<ile>_terrain/   tuiles SM_<Ile>_Terrain_<x>_<y>.obj + SM_<Ile>_Water.obj (regenerables)

Methode (la couleur absolue varie d'une scene a l'autre, on ne s'y fie pas) :
  - terre = polygones officiels du trait de cote ; platier / pente externe / lagon / pinacles / passe = atlas IRD
  - lagon profond = profondeur lissee a grande echelle (distance au bord, max documente comme estimation)
  - hauts-fonds = contraste local (luminance vs moyenne environnante), nuages rejetes par la chromaticite
Repere UE : origine au centre de l'emprise, X = est, Y = sud, Z = haut, Z = 0 au niveau moyen de la mer.
Format : OBJ (le MCP n'importe que FBX/OBJ) ; voir write_obj pour la conversion d'axes calibree.
"""
import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

Image.MAX_IMAGE_PIXELS = None
ROOT = Path(__file__).resolve().parents[2]
EARTH_R = 6378137.0

# Classes
OCEAN, OUTER_SLOPE, REEF_FLAT, LAND, INNER_SLOPE, LAGOON, PINNACLE, PASS = range(8)
CLASS_NAMES = ["ocean", "pente_externe", "platier", "terre", "pente_interne", "lagon", "pinacle", "passe"]
CLASS_COLORS = np.array([[12, 40, 90], [120, 80, 40], [235, 205, 130], [70, 140, 60],
                         [245, 235, 190], [60, 170, 210], [220, 80, 70], [20, 90, 200]], np.uint8)
# Couleurs de rendu de l'atlas IRD (identifiees par GetFeatureInfo sur Raraka)
ATLAS_PROTOS = [
    ((255, 255, 255), OCEAN),        # hors atlas
    ((93, 193, 217), LAGOON),        # Oceanic atoll / atoll lagoon / deep lagoon
    ((250, 228, 165), INNER_SLOPE),  # atoll lagoon / inner slope
    ((244, 208, 124), REEF_FLAT),    # atoll rim / reef flat (intertidal)
    ((170, 202, 119), REEF_FLAT),    # reef flat (variante verte) ; la terre vient du trait de cote
    ((182, 132, 30), OUTER_SLOPE),   # atoll rim / outer slope (tombant)
]

# Profils de hauteur (m) - estimations documentees dans georef.json
LAND_BASE, LAND_BEACH_RISE, LAND_INLAND_RISE = 0.5, 1.5, 1.5
REEF_FLAT_DEPTH = -0.4
INNER_SLOPE_MAX_DEPTH = -7.5
LAGOON_EDGE_DEPTH, LAGOON_MAX_DEPTH = -8.0, -30.0
PASS_DEPTH = -14.0
PINNACLE_DEPTH = -1.2
OCEAN_MIN_DEPTH = -300.0


def smoothstep(e0, e1, x):
    t = np.clip((x - e0) / (e1 - e0), 0.0, 1.0)
    return t * t * (3 - 2 * t)


def merc(lon, lat):
    return EARTH_R * np.radians(lon), EARTH_R * np.log(np.tan(np.pi / 4 + np.radians(lat) / 2))


def inv_merc_lat(my):
    return np.degrees(2 * np.arctan(np.exp(my / EARTH_R)) - np.pi / 2)


def masked_blur(values, mask, sigma):
    """Moyenne locale gaussienne restreinte a un masque (convolution normalisee)."""
    m = mask.astype(np.float32)
    num = ndi.gaussian_filter(values * m, sigma)
    den = ndi.gaussian_filter(m, sigma)
    return num / np.maximum(den, 1e-4), den


def rasterize_coastline(path, meta, shape):
    """Terre = interieur des contours du trait de cote (regle pair-impair pour les etangs interieurs)."""
    h, w = shape
    xmin, _, _, ymax = meta["merc_bounds"]
    px = meta["merc_px"]
    land = np.zeros(shape, bool)
    data = json.loads(Path(path).read_text(encoding="utf-8"))
    rings = 0
    for feat in data["features"]:
        geom = feat["geometry"]
        lines = geom["coordinates"] if geom["type"] == "MultiLineString" else [geom["coordinates"]]
        for line in lines:
            pts = np.asarray(line, dtype=np.float64)
            if len(pts) < 3:
                continue
            mx, my = merc(pts[:, 0], pts[:, 1])
            cols, rows = (mx - xmin) / px, (ymax - my) / px
            c0, c1 = max(int(cols.min()) - 2, 0), min(int(cols.max()) + 3, w)
            r0, r1 = max(int(rows.min()) - 2, 0), min(int(rows.max()) + 3, h)
            if c1 <= c0 or r1 <= r0:
                continue
            img = Image.new("1", (c1 - c0, r1 - r0), 0)
            ImageDraw.Draw(img).polygon(list(zip(cols - c0, rows - r0)), fill=1, outline=1)
            land[r0:r1, c0:c1] ^= np.asarray(img, dtype=bool)
            rings += 1
    return land, rings


def classify_atlas(atlas):
    protos = np.array([p for p, _ in ATLAS_PROTOS], np.float32)
    labels = np.array([c for _, c in ATLAS_PROTOS], np.uint8)
    flat = atlas.reshape(-1, 3)
    cls = np.empty(len(flat), np.uint8)
    for start in range(0, len(flat), 4_000_000):  # par blocs pour la memoire
        block = flat[start:start + 4_000_000]
        d = ((block[:, None, :] - protos[None]) ** 2).sum(-1)
        cls[start:start + len(block)] = labels[d.argmin(1)]
    cls = cls.reshape(atlas.shape[:2])
    r, g, b = atlas[..., 0], atlas[..., 1], atlas[..., 2]
    cls[(r > 190) & (g < 150) & (b < 140)] = PINNACLE   # pinacles (rouge)
    cls[(b > 190) & (r < 60)] = PASS                    # passe (bleu sature)
    return cls


def write_obj(path, positions_cm, uvs, indices, name, material):
    """OBJ pour l'importeur FBX d'UE (le seul format maillage accepte par le MCP, avec le FBX).

    Calibre sur UE 5.8 : l'OBJ est lu en Z haut et en cm, puis UE = (x, -y, z). Les positions sont donc
    ecrites en (est, nord, haut) en cm. L'importeur inverse V (v_UE = 1 - v_OBJ). Normales lisses
    calculees a l'import (groupe de lissage `s 1`).
    """
    tri = indices.reshape(-1, 3) + 1
    with open(path, "w", encoding="ascii", newline="\n") as f:
        f.write(f"# monke_island scripts/map/build_terrain.py\no {name}\nusemtl {material}\ns 1\n")
        np.savetxt(f, positions_cm, fmt="v %.1f %.1f %.1f")
        np.savetxt(f, np.column_stack([uvs[:, 0], 1.0 - uvs[:, 1]]), fmt="vt %.6f %.6f")
        np.savetxt(f, np.column_stack([tri[:, 0], tri[:, 0], tri[:, 1], tri[:, 1], tri[:, 2], tri[:, 2]]),
                   fmt="f %d/%d %d/%d %d/%d")


def grid_mesh(heights, east0_m, south0_m, spacing_m, uv0, uv_step):
    """Grille de hauteurs -> sommets OBJ en cm (est, nord, haut) + UV + indices.

    Lignes = vers le sud. Triangles (A, C, B) et (B, C, D) : anti-horaires vus du dessus, donc
    faces visibles d'en haut apres import.
    """
    nr, nc = heights.shape
    jj, ii = np.meshgrid(np.arange(nc), np.arange(nr))
    east = east0_m + jj * spacing_m
    south = south0_m + ii * spacing_m
    pos = np.stack([east * 100.0, -south * 100.0, heights * 100.0], -1).reshape(-1, 3)
    uv = np.stack([uv0[0] + jj * uv_step[0], uv0[1] + ii * uv_step[1]], -1).reshape(-1, 2)
    a = (ii[:-1, :-1] * nc + jj[:-1, :-1]).ravel()
    b, c = a + 1, a + nc
    d = c + 1
    idx = np.stack([a, c, b, b, c, d], -1).ravel()
    return pos, uv, idx


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("island")
    parser.add_argument("--step", type=int, default=2, help="pas de la grille de mesh en pixels de mosaique")
    parser.add_argument("--tiles", type=int, default=8, help="nombre de tuiles par axe")
    parser.add_argument("--no-mesh", action="store_true", help="ne regenere que les masques (pas les meshes ni georef)")
    args = parser.parse_args()

    ref = ROOT / "docs" / "reference" / "map" / args.island
    out = ref / "terrain"
    mesh_dir = ROOT / "docs" / "reference" / "map" / "raw" / f"{args.island}_terrain"
    out.mkdir(parents=True, exist_ok=True)
    mesh_dir.mkdir(parents=True, exist_ok=True)
    Island = args.island.capitalize()

    meta = json.loads((ref / "raw_sat_z15.png.json").read_text())
    sat = np.asarray(Image.open(ref / "raw_sat_z15.png").convert("RGB")).astype(np.float32)
    atlas = np.asarray(Image.open(ref / "atlas_recif_z15.png").convert("RGB")).astype(np.float32)
    assert sat.shape == atlas.shape, "mosaique et atlas doivent partager la meme grille"
    H, W = sat.shape[:2]
    xmin, ymin, xmax, ymax = meta["merc_bounds"]
    lat_c = float(inv_merc_lat((ymin + ymax) / 2))
    m_px = meta["merc_px"] * math.cos(math.radians(lat_c))  # metres au sol par pixel
    print(f"grille {W}x{H}, {m_px:.3f} m/px au sol (lat {lat_c:.4f})")

    # --- Classes -------------------------------------------------------------------------
    cls = classify_atlas(atlas)
    land, rings = rasterize_coastline(ref / "rivage.geojson", meta, (H, W))
    print(f"trait de cote : {rings} contours, {land.sum() * m_px * m_px / 1e6:.2f} km2 de terre")
    cls[land] = LAND
    cls[(cls == LAND) & ~land] = REEF_FLAT
    valid_sat = ~(sat >= 250).all(-1)

    # --- Distances (m) -------------------------------------------------------------------
    d_in_land = ndi.distance_transform_edt(land) * m_px
    body = np.isin(cls, [REEF_FLAT, LAND, INNER_SLOPE, LAGOON, PINNACLE, PASS])
    d_out = ndi.distance_transform_edt(~body) * m_px
    rim = np.isin(cls, [LAND, REEF_FLAT])
    d_rim = ndi.distance_transform_edt(~rim) * m_px
    # L'anticrenelage de l'atlas entoure chaque pinacle d'un anneau de couleurs melangees (classees platier ou
    # tombant). Les petites composantes isolees dans le lagon sont des bords de pinacles, pas un vrai rivage :
    # sans correction, la distance au bord repart de zero autour de chacune (cuvettes circulaires).
    non_lagoon = ~np.isin(cls, [LAGOON, PINNACLE])
    labels, count = ndi.label(non_lagoon)
    sizes = ndi.sum(non_lagoon, labels, index=np.arange(1, count + 1))
    keep = np.zeros(count + 1, bool)
    keep[1:] = sizes >= 20000                      # couronne + ocean (une seule grande composante)
    rim_main = keep[labels]
    isolated = non_lagoon & ~rim_main
    cls[isolated & np.isin(cls, [OUTER_SLOPE, OCEAN, REEF_FLAT])] = PINNACLE
    d_lag = ndi.distance_transform_edt(~rim_main) * m_px
    d_pin = ndi.distance_transform_edt(cls != PINNACLE) * m_px

    # --- Hauteurs de base ----------------------------------------------------------------
    h = np.zeros((H, W), np.float32)
    rng = np.random.default_rng(7)
    undulation = ndi.gaussian_filter(rng.standard_normal((H, W)).astype(np.float32), 30)
    undulation /= max(np.abs(undulation).max(), 1e-6)
    h_land = (LAND_BASE + LAND_BEACH_RISE * smoothstep(0, 30, d_in_land)
              + LAND_INLAND_RISE * smoothstep(30, 150, d_in_land)
              + 0.5 * undulation * smoothstep(20, 80, d_in_land))
    h_out = np.maximum(-1.0 - np.minimum(d_out, 40) * 0.35 - np.maximum(d_out - 40, 0) * 1.0, OCEAN_MIN_DEPTH)
    h_inner = -0.6 + (INNER_SLOPE_MAX_DEPTH + 0.6) * smoothstep(0, 400, d_rim)
    h_lag = LAGOON_EDGE_DEPTH + (LAGOON_MAX_DEPTH - LAGOON_EDGE_DEPTH) * smoothstep(0, 1200, d_lag)

    water = ~land
    h_water = np.select(
        [cls == REEF_FLAT, np.isin(cls, [OUTER_SLOPE, OCEAN]), cls == INNER_SLOPE, cls == PASS],
        [np.full_like(h, REEF_FLAT_DEPTH), h_out, h_inner, np.full_like(h, PASS_DEPTH)],
        default=h_lag).astype(np.float32)
    # lisse les marches entre classes (eau seulement, convolution normalisee)
    h_water, _ = masked_blur(h_water, water, 6)
    # pinacles : pitons a flancs raides (pente ~0.6) poses sur le fond du lagon
    pinnacle_cone = PINNACLE_DEPTH - 0.6 * d_pin
    h_water = np.where(np.isin(cls, [LAGOON, PINNACLE, INNER_SLOPE]), np.maximum(h_water, pinnacle_cone), h_water)

    # --- Hauts-fonds par contraste local (lagon + pente interne) -----------------------------
    r, g, b = sat[..., 0], sat[..., 1], sat[..., 2]
    lum = 0.25 * r + 0.5 * g + 0.25 * b
    chroma = (g - r) / (g + r + 1.0)
    zone = np.isin(cls, [LAGOON, INNER_SLOPE, PINNACLE]) & valid_sat
    mu, _ = masked_blur(lum, zone, 25)
    mu_ch, _ = masked_blur(chroma, zone, 25)
    contrast = (lum - mu) / np.maximum(mu, 1.0)
    turquoise = (chroma - mu_ch) > 0.02          # un haut-fond sableux gagne en vert/bleu, un nuage non
    cloud = (contrast > 0.10) & ~turquoise
    shallow = np.clip((contrast - 0.06) / 0.35, 0, 1) * turquoise * zone
    shallow = ndi.gaussian_filter(shallow.astype(np.float32), 1.5)
    h_water = np.where(zone, h_water * (1 - 0.85 * shallow), h_water)
    print(f"hauts-fonds detectes : {(shallow > 0.3).sum() * m_px * m_px / 1e6:.2f} km2 ; "
          f"pixels nuageux rejetes : {(cloud & zone).sum() * m_px * m_px / 1e6:.2f} km2")

    h = np.where(land, np.maximum(h_land, 0.3), np.minimum(h_water, -0.15)).astype(np.float32)

    # --- Masques materiau (R vegetation, G sable, B recif/corail, A humide) -------------------
    mu_land, _ = masked_blur(lum, land & valid_sat, 12)
    c_land = (lum - mu_land) / np.maximum(mu_land, 1.0)
    veg = smoothstep(0.02, -0.15, c_land) * smoothstep(6, 25, d_in_land) * land
    reef = (np.isin(cls, [REEF_FLAT, OUTER_SLOPE, PINNACLE]) & water).astype(np.float32)
    reef = np.maximum(reef, np.clip(shallow * 1.2, 0, 1) * 0.6)
    sand = np.clip(1.0 - veg - reef, 0, 1)
    wet = np.where(water, 1.0, 1.0 - smoothstep(0, 4, d_in_land)).astype(np.float32)
    total = veg + sand + reef + 1e-6
    masks = np.stack([veg / total, sand / total, reef / total, wet], -1)
    # canal par canal : Pillow redimensionne le RGBA en alpha premultiplie, ce qui effacerait les masques
    # partout ou l'humidite (A) vaut 0, c'est-a-dire sur toute la terre seche
    channels = [Image.fromarray((np.clip(masks[..., i], 0, 1) * 255).astype(np.uint8), "L").resize((4096, 4096), Image.BILINEAR)
                for i in range(4)]
    Image.merge("RGBA", channels).save(out / f"T_{Island}_Masks.png")
    if args.no_mesh:
        print("--no-mesh : masques et apercus regeneres, meshes inchanges")
        return

    # --- Apercus -------------------------------------------------------------------------
    small = (W // 4, H // 4)
    Image.fromarray(CLASS_COLORS[cls]).resize(small, Image.NEAREST).save(out / "preview_classes.png")
    gy, gx = np.gradient(h, m_px)
    shade = np.clip(0.55 + 0.45 * (-gx * 0.7 - gy * 0.7) / np.sqrt(1 + gx ** 2 + gy ** 2), 0, 1)
    depth_col = np.stack([smoothstep(-40, 0, h) * 0.4, smoothstep(-40, 0, h) * 0.9, 0.35 + 0.65 * smoothstep(-300, -2, h)], -1)
    land_col = np.stack([0.55 + 0.1 * smoothstep(0, 4, h), 0.5 + 0.2 * smoothstep(0, 4, h), np.full_like(h, 0.3)], -1)
    col = np.where((h > 0)[..., None], land_col, depth_col) * shade[..., None]
    Image.fromarray((np.clip(col, 0, 1) * 255).astype(np.uint8)).resize(small, Image.BILINEAR).save(out / "preview_relief.png")
    Image.fromarray((np.clip(masks[..., :3], 0, 1) * 255).astype(np.uint8)).resize(small, Image.BILINEAR).save(out / "preview_masks.png")

    # --- Heightmap 16 bits (reference / Landscape eventuel) --------------------------------
    hmin, hmax = float(h.min()), float(h.max())
    Image.fromarray(((h - hmin) / (hmax - hmin) * 65535).astype(np.uint16)).save(out / "heightmap_16.png")

    # --- Meshes ----------------------------------------------------------------------------
    s = args.step
    H2, W2 = H // s, W // s
    hg = h[:H2 * s, :W2 * s].reshape(H2, s, W2, s).mean((1, 3))
    spacing = s * m_px
    cx, cz = W * m_px / 2, H * m_px / 2                 # origine UE = centre de l'emprise
    east0 = (s / 2) * m_px - cx                         # centre du premier bloc
    south0 = (s / 2) * m_px - cz

    for old in mesh_dir.glob("*.glb"):
        old.unlink()
    tiles = []
    th, tw = math.ceil((H2 - 1) / args.tiles), math.ceil((W2 - 1) / args.tiles)
    for ty in range(args.tiles):
        for tx in range(args.tiles):
            r0, c0 = ty * th, tx * tw
            r1, c1 = min(r0 + th, H2 - 1), min(c0 + tw, W2 - 1)
            if r1 <= r0 or c1 <= c0:
                continue
            block = hg[r0:r1 + 1, c0:c1 + 1]
            if block.max() < -80.0:   # pleine mer profonde : invisible sous l'eau
                continue
            pos, uv, idx = grid_mesh(block, east0 + c0 * spacing, south0 + r0 * spacing, spacing,
                                     ((c0 * s + s / 2) / W, (r0 * s + s / 2) / H), (s / W, s / H))
            name = f"SM_{Island}_Terrain_{tx}_{ty}"
            write_obj(mesh_dir / f"{name}.obj", pos, uv, idx, name, "M_Terrain")
            tiles.append({"name": name, "file": str((mesh_dir / f"{name}.obj").relative_to(ROOT)).replace("\\", "/"),
                          "vertices": int(len(pos)), "triangles": int(len(idx) // 3),
                          "max_height_m": round(float(block.max()), 2)})
    print(f"{len(tiles)} tuiles ecrites dans {mesh_dir}")

    # Plan d'eau (niveau Z = 0, parametrable dans UE en deplacant l'acteur : montee des eaux, surcote)
    margin = 15000.0
    n = 32
    we, ws = np.meshgrid(np.linspace(-cx - margin, cx + margin, n + 1), np.linspace(-cz - margin, cz + margin, n + 1))
    wpos = np.stack([we * 100.0, -ws * 100.0, np.zeros_like(we)], -1).reshape(-1, 3)
    wj, wi = np.meshgrid(np.arange(n + 1), np.arange(n + 1))
    wuv = np.stack([wj / n, wi / n], -1).reshape(-1, 2)
    a = (wi[:-1, :-1] * (n + 1) + wj[:-1, :-1]).ravel()
    widx = np.stack([a, a + n + 1, a + 1, a + 1, a + n + 1, a + n + 2], -1).ravel()
    write_obj(mesh_dir / f"SM_{Island}_Water.obj", wpos, wuv, widx, f"SM_{Island}_Water", "M_Water")

    # --- Georeferencement ------------------------------------------------------------------
    def to_ue(lon, lat):
        mx, my = merc(np.array([lon]), np.array([lat]))
        col, row = (mx[0] - xmin) / meta["merc_px"], (ymax - my[0]) / meta["merc_px"]
        return {"x_cm": round((col * m_px - cx) * 100, 1), "y_cm": round((row * m_px - cz) * 100, 1)}

    areas = {CLASS_NAMES[k]: round(float((cls == k).sum()) * m_px * m_px / 1e6, 3) for k in range(len(CLASS_NAMES))}
    georef = {
        "island": args.island,
        "source_grid": {"width": W, "height": H, "merc_bounds": meta["merc_bounds"], "merc_px": meta["merc_px"]},
        "ground_m_per_px": m_px,
        "latitude_center": lat_c,
        "ue_frame": "origine = centre de l'emprise ; X = est, Y = sud, Z = haut (cm) ; Z = 0 niveau moyen de la mer",
        "extent_m": [W * m_px, H * m_px],
        "mesh": {"step_px": s, "spacing_m": spacing, "tiles": tiles, "water": f"SM_{Island}_Water"},
        "masks_texture": {"file": f"T_{Island}_Masks.png", "uv0": "UV0 = (colonne/W, ligne/H) sur toute l'emprise",
                          "channels": {"R": "vegetation / litiere", "G": "sable", "B": "recif / debris coralliens",
                                       "A": "humidite (1 = immerge)"}},
        "heights_m": {"land": [LAND_BASE, LAND_BASE + LAND_BEACH_RISE + LAND_INLAND_RISE], "reef_flat": REEF_FLAT_DEPTH,
                      "inner_slope_max": INNER_SLOPE_MAX_DEPTH, "lagoon": [LAGOON_EDGE_DEPTH, LAGOON_MAX_DEPTH],
                      "pass": PASS_DEPTH, "pinnacle": PINNACLE_DEPTH, "ocean_min": OCEAN_MIN_DEPTH,
                      "note": "estimations (pas de bathymetrie mesuree) : a remplacer si un releve SHOM/IRD est obtenu"},
        "heightmap_16": {"file": "heightmap_16.png", "min_m": hmin, "max_m": hmax},
        "class_areas_km2": areas,
        "points_ue": {"village_motutapu": to_ue(-144.9543, -16.0909), "passe_manureva": to_ue(-144.9585, -16.0921)},
        "sources": ["Trait de cote : Te Fenua / DAF (Etalab 2.0)",
                    "Atlas des recifs coralliens de Polynesie francaise, Andrefouet et al., IRD 2005, via Te Fenua",
                    "Hauts-fonds : contraste local de l'imagerie (c) Maxar via Te Fenua (derive, image non embarquee)"],
    }
    (out / "georef.json").write_text(json.dumps(georef, indent=2, ensure_ascii=False), encoding="utf-8")
    print(json.dumps({"areas_km2": areas, "points_ue": georef["points_ue"], "h_range": [hmin, hmax]}, ensure_ascii=False))


if __name__ == "__main__":
    main()
