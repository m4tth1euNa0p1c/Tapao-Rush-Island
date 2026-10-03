"""Pack the materials of the village kit into Unreal texture sets (one set per material, MI of M_Prop_Atlas).

Usage: python scripts/props/pack_ue_textures.py [--kit art/export/Props/Village/village_kit.json]
Input: the "materials" manifest written by Blender (geo.material_manifest): base colour, roughness, metallic,
  normal (OpenGL) and emission, as image files (channel) or constants.
Output (next to the kit): Textures/T_<Name>_D.png (sRGB), T_<Name>_N.png (DirectX normal: green flipped, strength
  applied), T_<Name>_ORM.png (R occlusion, G roughness, B metallic), and ue_materials.json (Blender material ->
  MI name, textures, emissive colour). Constant materials get 8x8 textures; photo sets are capped at 1024 px
  (512 px for the small props) to keep the village light in memory.
"""
import argparse
import json
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
SMALL = ("tea_set", "ceramic_vase", "wooden_bowl", "wooden_bucket", "wooden_broom", "Kakejiku")
FLAT = 8


def linear_to_srgb(c):
    c = np.clip(np.asarray(c, float), 0, 1)
    return np.where(c <= 0.0031308, c * 12.92, 1.055 * c ** (1 / 2.4) - 0.055)


def src_path(p):
    q = Path(p)
    return q if q.is_absolute() else ROOT / q


def load(entry, size, mode="RGB"):
    img = Image.open(src_path(entry["image"]))
    ch = entry.get("channel")
    if ch:
        img = img.convert("RGB").getchannel("RGB".index(ch))
    else:
        img = img.convert(mode)
    if max(img.size) > size:
        k = size / max(img.size)
        img = img.resize((max(1, round(img.width * k)), max(1, round(img.height * k))), Image.LANCZOS)
    return img


def pack(name, m, size, out):
    files = {}
    # base colour
    if isinstance(m.get("base_color"), dict):
        d = load(m["base_color"], size, "RGB")
    else:
        rgb = (linear_to_srgb(m.get("base_color", [0.5, 0.5, 0.5])) * 255 + 0.5).astype(np.uint8)
        d = Image.new("RGB", (FLAT, FLAT), tuple(int(v) for v in rgb))
    files["D"] = f"T_{name}_D.png"
    d.save(out / files["D"])
    # normal (OpenGL -> DirectX, strength)
    if isinstance(m.get("normal"), dict):
        n = np.asarray(load(m["normal"], size, "RGB"), np.float32) / 127.5 - 1.0
        n[..., 1] *= -1.0
        s = float(m["normal"].get("strength", 1.0))
        n[..., :2] *= s
        n /= np.linalg.norm(n, axis=-1, keepdims=True).clip(1e-6)
        nimg = Image.fromarray(((n + 1.0) * 127.5 + 0.5).clip(0, 255).astype(np.uint8), "RGB")
    else:
        nimg = Image.new("RGB", (FLAT, FLAT), (128, 128, 255))
    files["N"] = f"T_{name}_N.png"
    nimg.save(out / files["N"])
    # ORM
    chans = {}
    for key, default in (("ao", 1.0), ("roughness", 0.7), ("metallic", 0.0)):
        v = m.get(key, default)
        chans[key] = load(v, size, "L") if isinstance(v, dict) else float(v)
    imgs = [c for c in chans.values() if isinstance(c, Image.Image)]
    wh = max((c.size for c in imgs), key=lambda s: s[0] * s[1]) if imgs else (FLAT, FLAT)
    planes = []
    for key in ("ao", "roughness", "metallic"):
        c = chans[key]
        if isinstance(c, Image.Image):
            planes.append(c.resize(wh, Image.BILINEAR) if c.size != wh else c)
        else:
            planes.append(Image.new("L", wh, int(round(np.clip(c, 0, 1) * 255))))
    files["ORM"] = f"T_{name}_ORM.png"
    Image.merge("RGB", planes).save(out / files["ORM"])
    return files


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--kit", default=str(ROOT / "art" / "export" / "Props" / "Village" / "village_kit.json"))
    a = ap.parse_args()
    kit_path = Path(a.kit)
    kit = json.loads(kit_path.read_text(encoding="utf-8"))
    out = kit_path.parent / "Textures"
    out.mkdir(exist_ok=True)
    result, seen = {}, {}
    for mat, m in sorted(kit["materials"].items()):
        key = json.dumps(m, sort_keys=True)
        if key in seen:                      # identical material (e.g. "wooden_bowl_01" and its ".001" copy)
            result[mat] = dict(result[seen[key]], alias_of=seen[key])
            continue
        name = mat[2:] if mat.startswith("M_") else mat
        name = name.replace(".", "_")
        size = 512 if any(k in mat for k in SMALL) else 1024
        files = pack(name, m, size, out)
        em = None
        if m.get("emission"):
            em = [round(c * m.get("emission_strength", 1.0), 3) for c in m["emission"]]
        result[mat] = {"mi": f"MI_{name}", "textures": files, "emissive": em}
        seen[key] = mat
        print(f"  {mat:34s} -> MI_{name} ({size if any(isinstance(m.get(k), dict) for k in m) else FLAT} px)")
    (kit_path.parent / "ue_materials.json").write_text(json.dumps(result, indent=1), encoding="utf-8")
    n_tex = len(list(out.glob("T_*.png")))
    mb = sum(f.stat().st_size for f in out.glob("T_*.png")) / 1e6
    print(f"{len(result)} materials, {n_tex} textures ({mb:.1f} MB PNG) -> {out.relative_to(ROOT)}")


if __name__ == "__main__":
    main()
