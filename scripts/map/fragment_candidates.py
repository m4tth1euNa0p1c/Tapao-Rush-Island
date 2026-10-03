"""Repere les fragments candidats de Raraka : groupes de motu isoles par la mer (esquisse, base de T-081).

Usage: python scripts/map/fragment_candidates.py <sortie.png>   (depuis la racine du depot)

Un groupe = motu relies par moins de 40 m d'eau. Classement : fragment naturel (8-100 ha, < 3 km),
grand motu a scinder par un hoa, secteur bati exclu (< 400 m d'un batiment BD TOPO), ilot trop petit.
Spec : docs/features/fragments-tuamotu.md
Remplace par scripts/map/fragments/find_fragments.py (T-082) : garde pour memoire de l'esquisse.
"""
import json, math, sys
import numpy as np
from PIL import Image, ImageDraw
from scipy import ndimage as ndi

OUT = sys.argv[1]
g = json.load(open('docs/reference/map/raraka/terrain/georef.json'))
gj = json.load(open('docs/reference/map/raraka/rivage.geojson'))
lat0 = math.radians(g['latitude_center']); R = 6378137.0
mb = g['source_grid']['merc_bounds']; k = math.cos(lat0)
RES = 5.0
W = int((mb[2] - mb[0]) * k / RES) + 1; H = int((mb[3] - mb[1]) * k / RES) + 1


def px(lon, lat):
    x = R * math.radians(lon); y = R * math.log(math.tan(math.pi / 4 + math.radians(lat) / 2))
    return ((x - mb[0]) * k / RES, (mb[3] - y) * k / RES)


img = Image.new('L', (W, H), 0); d = ImageDraw.Draw(img)
for f in gj['features']:
    for ln in f['geometry']['coordinates']:
        d.polygon([px(*c[:2]) for c in ln], fill=1)
land = np.array(img, bool)
dist = ndi.distance_transform_edt(~land) * RES

bj = json.load(open('docs/reference/map/raraka/batiments.geojson'))
bmask = np.zeros_like(land)
for f in bj['features']:
    c = f['geometry']['coordinates']
    while isinstance(c[0][0], list):
        c = c[0]
    x, y = px(sum(p[0] for p in c) / len(c), sum(p[1] for p in c) / len(c))
    bmask[int(y), int(x)] = True
near_b = ndi.distance_transform_edt(~bmask) * RES < 400

GAP = 40
l2, n2 = ndi.label(dist <= GAP / 2)
objs = ndi.find_objects(l2)
out = np.zeros((H, W, 3), np.uint8); out[:] = (20, 60, 110)
out[dist < 300] = (40, 110, 160)
col = {'ok': (80, 200, 90), 'big': (230, 170, 60), 'small': (150, 150, 150), 'village': (220, 60, 60)}
cands = []; stats = {}
for i, o in enumerate(objs):
    m = (l2 == i + 1) & land; t = m.sum() * RES * RES / 1e6
    w = (o[1].stop - o[1].start) * RES; h = (o[0].stop - o[0].start) * RES
    if (m & near_b).any():
        kind = 'village'
    elif t < 0.08:
        kind = 'small'
    elif t > 1.0 or max(w, h) > 3000:
        kind = 'big'
    else:
        kind = 'ok'
    stats[kind] = stats.get(kind, 0) + 1
    out[m] = col[kind]
    if kind == 'ok':
        ys, xs = np.nonzero(m); cands.append((round(t, 2), int(w), int(h), int(xs.mean()), int(ys.mean())))
cands.sort(reverse=True)
print(stats)
for j, c in enumerate(cands):
    print(f'F{j+1}: {c[0]*100:.0f} ha, emprise {c[1]} x {c[2]} m')
S = 3
im = Image.fromarray(out).resize((W // S, H // S), Image.NEAREST); d = ImageDraw.Draw(im)
for j, c in enumerate(cands):
    d.text((c[3] // S + 10, c[4] // S - 6), f'F{j+1}', fill=(255, 255, 255))
d.text((10, 10), 'vert = fragment naturel (8-100 ha, < 3 km)   orange = grand motu a scinder par un hoa', fill=(255, 255, 255))
d.text((10, 24), 'rouge = secteur de Motutapu exclu   gris = ilot trop petit   bleu clair = platier / bord', fill=(255, 255, 255))
im.save(OUT); print(im.size)
