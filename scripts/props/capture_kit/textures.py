"""CC0 textures of the capture kit (T-337): downloaded once, recoloured to the game's palette, packed for Unreal.

Sources (all CC0, credited in art/SOURCES.md, section "Kit de capture"):
  wood   Poly Haven rough_wood       (already in art/textures/characters/rough_wood, scripts/character/fetch_character_assets.py)
  iron   Poly Haven rusty_metal_04   (the wrought iron of the French fort's cages, maquette fort-francais)
  steel  ambientCG Metal054B         (scratched iron / steel, light rust)
Download cache: build/capture_kit/cc0/<asset>/ (build/ is ignored by Git; re-downloaded when missing).

Output (art/export/Props/Capture/Textures/, 1024 x 1024 PNG):
  T_Capture_<Wood|Iron|Steel>_D.png    base colour, sRGB
  T_Capture_<...>_N.png                normal, DirectX (green down) -> Unreal TC_Normalmap without flip
  T_Capture_<...>_ORM.png              linear: R ambient occlusion, G roughness, B metallic
  build/capture_kit/textures_gl/T_Capture_<...>_N_GL.png   the same normal in OpenGL convention, for the Blender previews
                                       and the GLB copies only (not versioned, never imported in Unreal)

Recolouring ("rampe de luminance plus relief des scans", feedback-cc0-assets-online): the luminance of the scan, stretched
between its 2nd and 98th percentiles, drives a ramp of palette colours; the rust of the metals is found by its hue and
kept (toned down) in the palette's rust colours; cracks and pits darken through the AO.
Runs inside Blender (numpy + bpy images, no Pillow), called by build.py --textures. Idempotent.
"""
from __future__ import annotations

import io
import json
import subprocess
import urllib.request
import zipfile
from pathlib import Path

import bpy
import numpy as np

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
CACHE = REPO / 'build' / 'capture_kit' / 'cc0'
OUT = REPO / 'art' / 'export' / 'Props' / 'Capture' / 'Textures'
GL_OUT = REPO / 'build' / 'capture_kit' / 'textures_gl'     # OpenGL normals for Blender / GLB only (not versioned)
SIZE = 1024
UA = {'User-Agent': 'monke_island-capture-kit/1.0'}

SOURCES = {
    'rough_wood': {'site': 'polyhaven', 'url': 'https://polyhaven.com/a/rough_wood', 'author': 'Rob Tuytel (Poly Haven)',
                   'local': REPO / 'art' / 'textures' / 'characters' / 'rough_wood',
                   'maps': {'diff': 'rough_wood_diff_1k.jpg', 'rough': 'rough_wood_rough_1k.jpg',
                            'nor_dx': 'rough_wood_nor_dx_1k.jpg', 'ao': 'rough_wood_ao_1k.jpg'}},
    'rusty_metal_04': {'site': 'polyhaven', 'url': 'https://polyhaven.com/a/rusty_metal_04', 'author': 'Amal Kumar (Poly Haven)',
                       'maps': {'diff': 'rusty_metal_04_diff_1k.jpg', 'rough': 'rusty_metal_04_rough_1k.jpg',
                                'nor_dx': 'rusty_metal_04_nor_dx_1k.jpg', 'ao': 'rusty_metal_04_ao_1k.jpg',
                                'metal': 'rusty_metal_04_metal_1k.jpg'}},
    'Metal054B': {'site': 'ambientcg', 'url': 'https://ambientcg.com/view?id=Metal054B', 'author': 'ambientCG (Lennart Demes)',
                  'maps': {'diff': 'Metal054B_1K-JPG_Color.jpg', 'rough': 'Metal054B_1K-JPG_Roughness.jpg',
                           'nor_dx': 'Metal054B_1K-JPG_NormalDX.jpg', 'metal': 'Metal054B_1K-JPG_Metalness.jpg',
                           'disp': 'Metal054B_1K-JPG_Displacement.jpg'}},
}

# palette (sRGB 0..255): ramps dark -> mid -> light, rust ramps
PALETTE = {
    # teak / coconut palm timber, warm brown, weathered
    'wood': {'ramp': [(52, 31, 19), (112, 70, 40), (170, 122, 76)], 'rough': (0.62, 0.92)},
    # wrought iron, blackened, rust in the pits (the fort's cages: rusty_metal_04 darkened)
    'iron': {'ramp': [(26, 25, 24), (58, 55, 52), (96, 92, 88)], 'rust': [(64, 32, 17), (122, 62, 28)],
             'rust_keep': 0.55, 'rough': (0.38, 0.80), 'metal': (0.85, 0.15)},
    # worn steel of the handcuffs and chain, slightly cool grey, scratched, light rust
    'steel': {'ramp': [(92, 94, 99), (132, 135, 140), (176, 178, 181)], 'rust': [(86, 50, 28), (140, 86, 46)],
              'rust_keep': 0.35, 'rough': (0.32, 0.62), 'metal': (1.0, 0.25), 'soften': 2},
}

MATERIALS = {'Wood': ('rough_wood', 'wood'), 'Iron': ('rusty_metal_04', 'iron'), 'Steel': ('Metal054B', 'steel')}


# ----------------------------------------------------------------------------------------------- download

def _get(url):
    try:
        with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=180) as r:
            return r.read()
    except Exception:  # noqa: BLE001 - Blender's Python may lack the certificates: curl of Windows instead
        return subprocess.run(['curl', '-s', '-L', '-A', UA['User-Agent'], url], check=True, capture_output=True).stdout


def fetch(asset):
    """Folder holding the 1k maps of asset (downloaded into the cache when missing)."""
    spec = SOURCES[asset]
    local = spec.get('local')
    if local and all((local / f).exists() for f in spec['maps'].values()):
        return local
    folder = CACHE / asset
    if all((folder / f).exists() for f in spec['maps'].values()):
        return folder
    folder.mkdir(parents=True, exist_ok=True)
    if spec['site'] == 'polyhaven':
        files = json.loads(_get(f'https://api.polyhaven.com/files/{asset}'))
        keys = {'diff': 'Diffuse', 'rough': 'Rough', 'nor_dx': 'nor_dx', 'ao': 'AO', 'metal': 'Metal'}
        for kind, name in spec['maps'].items():
            url = files[keys[kind]]['1k']['jpg']['url']
            (folder / name).write_bytes(_get(url))
    else:
        data = _get(f'https://ambientcg.com/get?file={asset}_1K-JPG.zip')
        with zipfile.ZipFile(io.BytesIO(data)) as z:
            for n in z.namelist():
                if n.lower().endswith('.jpg'):
                    (folder / Path(n).name).write_bytes(z.read(n))
    missing = [f for f in spec['maps'].values() if not (folder / f).exists()]
    if missing:
        raise RuntimeError(f'{asset}: missing after download {missing}')
    print(f'[capture textures] downloaded {asset} -> {folder}', flush=True)
    return folder


# ----------------------------------------------------------------------------------------------- image io

def load(path):
    """float32 array (h, w, 4) of the stored values (0..1, no colour conversion), resized to SIZE"""
    img = bpy.data.images.load(str(path), check_existing=False)
    img.colorspace_settings.name = 'Non-Color'
    if img.size[0] != SIZE or img.size[1] != SIZE:
        img.scale(SIZE, SIZE)
    buf = np.empty(SIZE * SIZE * 4, dtype=np.float32)
    img.pixels.foreach_get(buf)
    bpy.data.images.remove(img)
    return buf.reshape(SIZE, SIZE, 4)


def save(arr, path, srgb):
    path.parent.mkdir(parents=True, exist_ok=True)
    img = bpy.data.images.new(path.stem, SIZE, SIZE, alpha=False, float_buffer=False)
    img.colorspace_settings.name = 'sRGB' if srgb else 'Non-Color'
    rgba = np.ones((SIZE, SIZE, 4), dtype=np.float32)
    rgba[..., :3] = np.clip(arr[..., :3], 0.0, 1.0)
    img.pixels.foreach_set(rgba.ravel())
    img.filepath_raw = str(path)
    img.file_format = 'PNG'
    for attempt in range(6):          # Malwarebytes may hold a file just written: try again
        try:
            img.save()
            break
        except RuntimeError:
            if attempt == 5:
                raise
            import time
            time.sleep(1.0)
    bpy.data.images.remove(img)
    return path


def luminance(rgb):
    return rgb[..., 0] * 0.299 + rgb[..., 1] * 0.587 + rgb[..., 2] * 0.114


def stretch(x, lo=2.0, hi=98.0):
    a, b = np.percentile(x, lo), np.percentile(x, hi)
    return np.clip((x - a) / max(b - a, 1e-4), 0.0, 1.0)


def ramp(t, colours):
    """t (h, w) in 0..1 -> rgb through the colour stops (sRGB 0..255), evenly spaced"""
    stops = np.array(colours, dtype=np.float32) / 255.0
    n = len(stops) - 1
    x = np.clip(t, 0.0, 1.0) * n
    i = np.clip(np.floor(x).astype(np.int32), 0, n - 1)
    f = (x - i)[..., None]
    return stops[i] * (1.0 - f) + stops[i + 1] * f


def blur(x, radius=2):
    """Box blur (separable, wrap-around: the scans tile)"""
    out = x.copy()
    for axis in (0, 1):
        acc = np.zeros_like(out)
        for d in range(-radius, radius + 1):
            acc += np.roll(out, d, axis=axis)
        out = acc / (2 * radius + 1)
    return out


# ----------------------------------------------------------------------------------------------- recolour

def rust_mask(rgb):
    """Orange-brown hue of the scan's rust (0..1)"""
    r, g, b = rgb[..., 0], rgb[..., 1], rgb[..., 2]
    warm = np.clip((r - b - 0.05) / 0.22, 0.0, 1.0) * np.clip((r - g + 0.02) / 0.12, 0.0, 1.0)
    return blur(warm, 1)


def make(material):
    asset, key = MATERIALS[material]
    folder = fetch(asset)
    maps = {k: load(folder / f) for k, f in SOURCES[asset]['maps'].items()}
    pal = PALETTE[key]
    diff = maps['diff'][..., :3]
    lum = stretch(luminance(diff))
    rough_src = maps['rough'][..., 0]
    if 'ao' in maps:
        ao = maps['ao'][..., 0]
    elif 'disp' in maps:
        ao = np.clip(0.72 + 0.28 * stretch(maps['disp'][..., 0]), 0.0, 1.0)
    else:
        ao = np.ones_like(lum)
    if key == 'wood':
        base = ramp(lum ** 0.95, pal['ramp'])
        base *= (0.55 + 0.45 * ao ** 0.8)[..., None]
        rough = pal['rough'][0] + (pal['rough'][1] - pal['rough'][0]) * rough_src
        metal = np.zeros_like(lum)
    else:
        rust = rust_mask(diff) * pal['rust_keep']
        if pal.get('soften'):          # the scratches of Metal054B read as cracks at the cuffs' size: blur them a little
            lum = 0.55 * lum + 0.45 * blur(lum, pal['soften'])
        metal_col = ramp(lum, pal['ramp'])
        rust_col = ramp(lum, pal['rust'])
        base = metal_col * (1.0 - rust[..., None]) + rust_col * rust[..., None]
        base *= (0.7 + 0.3 * ao ** 0.7)[..., None]
        rough = pal['rough'][0] + (pal['rough'][1] - pal['rough'][0]) * np.clip(rough_src * 0.85 + rust * 0.5, 0.0, 1.0)
        # the scan's own metal map for the steel (Metal054B); the iron's (rusty_metal_04) is mostly paint: from the rust
        metal_src = maps['metal'][..., 0] if (key == 'steel' and 'metal' in maps) else 1.0 - rust
        metal = np.clip(pal['metal'][0] * metal_src * (1.0 - rust) + pal['metal'][1] * rust, 0.0, 1.0)
    orm = np.stack([ao, np.clip(rough, 0.0, 1.0), metal], axis=-1)
    normal_dx = maps['nor_dx'][..., :3]
    normal_gl = normal_dx.copy()
    normal_gl[..., 1] = 1.0 - normal_gl[..., 1]
    name = f'T_Capture_{material}'
    out = {
        'D': save(base, OUT / f'{name}_D.png', True),
        'N': save(normal_dx, OUT / f'{name}_N.png', False),
        'ORM': save(orm, OUT / f'{name}_ORM.png', False),
        'N_GL': save(normal_gl, GL_OUT / f'{name}_N_GL.png', False),
    }
    stats = {'base_mean_srgb': [round(float(v) * 255, 1) for v in base.reshape(-1, 3).mean(axis=0)],
             'roughness_mean': round(float(orm[..., 1].mean()), 3), 'metallic_mean': round(float(orm[..., 2].mean()), 3),
             'ao_mean': round(float(orm[..., 0].mean()), 3)}
    return {'files': {k: v.relative_to(REPO).as_posix() for k, v in out.items()}, 'source': asset,
            'source_url': SOURCES[asset]['url'], 'author': SOURCES[asset]['author'], 'license': 'CC0 1.0',
            'stats': stats}


def build_all(force=False):
    report = {}
    for material in MATERIALS:
        name = f'T_Capture_{material}'
        files = [OUT / f'{name}_D.png', OUT / f'{name}_N.png', OUT / f'{name}_ORM.png', GL_OUT / f'{name}_N_GL.png']
        if not force and all(f.exists() for f in files):
            asset = MATERIALS[material][0]
            report[material] = {'files': {k: f.relative_to(REPO).as_posix() for k, f in zip(('D', 'N', 'ORM', 'N_GL'), files)},
                                'source': asset, 'source_url': SOURCES[asset]['url'], 'author': SOURCES[asset]['author'],
                                'license': 'CC0 1.0', 'reused': True}
            continue
        report[material] = make(material)
        print(f'[capture textures] {material}: {report[material]["stats"]}', flush=True)
    return report


def texture_paths(material):
    name = f'T_Capture_{material}'
    return {'D': OUT / f'{name}_D.png', 'N': OUT / f'{name}_N.png', 'ORM': OUT / f'{name}_ORM.png',
            'N_GL': GL_OUT / f'{name}_N_GL.png'}

