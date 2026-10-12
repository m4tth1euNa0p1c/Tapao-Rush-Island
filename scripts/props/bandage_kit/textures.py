"""CC0 texture of the bandage kit (T-340): the plain weave of a Poly Haven scan recoloured to unbleached linen.

Source (CC0 1.0, credited in art/SOURCES.md, section "Bandages de soin"):
  Poly Haven hessian_230 (already in art/textures/characters/hessian_230, the villager's loincloth): a plain weave whose
  relief reads as a fine linen gauze once tiled at 12 cm (kit.TILE).

Output (art/export/Props/Bandage/Textures/, 1024 x 1024 PNG):
  T_Bandage_Linen_D.png      base colour, sRGB (unbleached linen ramp, ecru)
  T_Bandage_Linen_N.png      normal, DirectX (green down) -> Unreal TC_Normalmap without flip (relief softened)
  T_Bandage_Linen_ORM.png    linear: R ambient occlusion, G roughness, B metallic (0)
  build/bandage_kit/textures_gl/T_Bandage_Linen_N_GL.png  OpenGL normal for the Blender previews and GLB copies only

Recolouring like the capture kit (feedback-cc0-assets-online): the scan's luminance, stretched between its 2nd and 98th
percentiles, drives a ramp of palette colours; the jute's brown hue is dropped; the weave darkens a little through the AO.
Runs inside Blender (numpy + bpy images), called by build.py. Idempotent (reused when present, --force-textures to redo).
"""
from __future__ import annotations

from pathlib import Path

import bpy
import numpy as np

HERE = Path(__file__).resolve().parent
REPO = HERE.parents[2]
OUT = REPO / 'art' / 'export' / 'Props' / 'Bandage' / 'Textures'
GL_OUT = REPO / 'build' / 'bandage_kit' / 'textures_gl'
SIZE = 1024

SOURCE = {'asset': 'hessian_230', 'url': 'https://polyhaven.com/a/hessian_230', 'author': 'Poly Haven',
          'folder': REPO / 'art' / 'textures' / 'characters' / 'hessian_230',
          'maps': {'diff': 'hessian_230_diff_1k.jpg', 'rough': 'hessian_230_rough_1k.jpg',
                   'nor_dx': 'hessian_230_nor_dx_1k.jpg', 'ao': 'hessian_230_ao_1k.jpg'}}

# unbleached linen (sRGB 0..255), dark thread gaps -> light threads; roughness range (cloth, matte)
PALETTE = {'ramp': [(142, 121, 88), (190, 170, 132), (222, 207, 172)], 'rough': (0.78, 0.95), 'normal_strength': 0.6}


def load(path):
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
    for attempt in range(8):          # Malwarebytes may hold a file just written: try again
        try:
            img.save()
            break
        except RuntimeError:
            if attempt == 7:
                raise
            import time
            time.sleep(1.0)
    bpy.data.images.remove(img)
    return path


def stretch(x, lo=2.0, hi=98.0):
    a, b = np.percentile(x, lo), np.percentile(x, hi)
    return np.clip((x - a) / max(b - a, 1e-4), 0.0, 1.0)


def ramp(t, colours):
    stops = np.array(colours, dtype=np.float32) / 255.0
    n = len(stops) - 1
    x = np.clip(t, 0.0, 1.0) * n
    i = np.clip(np.floor(x).astype(np.int32), 0, n - 1)
    f = (x - i)[..., None]
    return stops[i] * (1.0 - f) + stops[i + 1] * f


def make():
    folder = SOURCE['folder']
    missing = [f for f in SOURCE['maps'].values() if not (folder / f).exists()]
    if missing:
        raise RuntimeError(f'hessian_230 maps missing in {folder}: {missing} (scripts/character/fetch_character_assets.py)')
    maps = {k: load(folder / f) for k, f in SOURCE['maps'].items()}
    diff = maps['diff'][..., :3]
    lum = diff[..., 0] * 0.299 + diff[..., 1] * 0.587 + diff[..., 2] * 0.114
    lum = stretch(lum)
    ao = maps['ao'][..., 0]
    base = ramp(lum ** 0.85, PALETTE['ramp'])
    base *= (0.80 + 0.20 * ao ** 0.8)[..., None]
    rough = PALETTE['rough'][0] + (PALETTE['rough'][1] - PALETTE['rough'][0]) * maps['rough'][..., 0]
    orm = np.stack([0.55 + 0.45 * ao, np.clip(rough, 0.0, 1.0), np.zeros_like(ao)], axis=-1)
    # softer relief: blend the DirectX normal toward flat (0.5, 0.5, 1)
    k = PALETTE['normal_strength']
    n = maps['nor_dx'][..., :3] * 2.0 - 1.0
    n[..., :2] *= k
    n /= np.linalg.norm(n, axis=-1, keepdims=True) + 1e-6
    normal_dx = n * 0.5 + 0.5
    normal_gl = normal_dx.copy()
    normal_gl[..., 1] = 1.0 - normal_gl[..., 1]
    name = 'T_Bandage_Linen'
    out = {'D': save(base, OUT / f'{name}_D.png', True), 'N': save(normal_dx, OUT / f'{name}_N.png', False),
           'ORM': save(orm, OUT / f'{name}_ORM.png', False), 'N_GL': save(normal_gl, GL_OUT / f'{name}_N_GL.png', False)}
    stats = {'base_mean_srgb': [round(float(v) * 255, 1) for v in base.reshape(-1, 3).mean(axis=0)],
             'roughness_mean': round(float(orm[..., 1].mean()), 3), 'ao_mean': round(float(orm[..., 0].mean()), 3)}
    return out, stats


def build_all(force=False):
    files = texture_paths('Linen')
    row = {'source': SOURCE['asset'], 'source_url': SOURCE['url'], 'author': SOURCE['author'], 'license': 'CC0 1.0'}
    if not force and all(f.exists() for f in files.values()):
        row['reused'] = True
    else:
        _out, stats = make()
        row['stats'] = stats
        print(f'[bandage textures] Linen: {stats}', flush=True)
    row['files'] = {k: f.relative_to(REPO).as_posix() for k, f in files.items()}
    return {'Linen': row}


def texture_paths(material):
    name = f'T_Bandage_{material}'
    return {'D': OUT / f'{name}_D.png', 'N': OUT / f'{name}_N.png', 'ORM': OUT / f'{name}_ORM.png',
            'N_GL': GL_OUT / f'{name}_N_GL.png'}
