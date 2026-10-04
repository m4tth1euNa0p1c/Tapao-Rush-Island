"""Marae des Perles (maquette of the second PC, OneDrive\\maquette-blender): copies the FBX of the 7 parts and packs
their CC0 photo textures into Unreal texture sets.

  python scripts/props/marae_textures.py
Input (read only): <OneDrive>/maquettes/marae-*/exports/SM_Marae_*.fbx, <OneDrive>/art/textures/<set>/ (Poly Haven
  and ambientCG, CC0; the same sets the second PC used in factory/kits/marae_kit.py mats()).
Output: art/export/Props/Marae/SM_Marae_*.fbx + item reports, Textures/T_Marae_<Name>_D.png (sRGB, the Blender tint
  and saturation baked in), _N.png (DirectX normal), _ORM.png (R occlusion, G roughness, B metallic 0), and
  marae_materials.json (material slot -> textures and tiling). The FBX UVs are in metres (box projection), the
  Blender mapping scaled them by 1 / tile: the MI gets Tiling = 1 / tile.
"""
import json
import shutil
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[2]
ONEDRIVE = Path.home() / "OneDrive" / "maquette-blender"
MAQ = ONEDRIVE / "maquettes"
TEX = ONEDRIVE / "art" / "textures"
OUT = ROOT / "art" / "export" / "Props" / "Marae"
SIZE = 1024

PARTS = ("marae-pyramide", "marae-autel", "marae-pierres", "marae-torche", "marae-offrandes", "marae-conque",
         "marae-artefact")

# slot -> (texture set, tile (m), tint, saturation): factory/kits/marae_kit.py mats()
MATERIALS = {
    "M_Marae_BasaltMoss": ("props/minka/mossy_rock", 0.9, (1, 1, 1), 1.0),
    "M_Marae_Basalt": ("characters/Rock035", 0.8, (0.85, 0.85, 0.85), 1.0),
    "M_Marae_BasaltLight": ("characters/Rock030", 0.8, (1, 1, 1), 1.0),
    "M_Marae_CoralBlock": ("ground/coral_ground_02", 0.7, (1.08, 1.06, 1.0), 0.25),
    "M_Marae_CoralGravel": ("ground/coral_gravel", 1.2, (1, 1, 1), 0.45),
    "M_Marae_Wood": ("characters/rough_wood", 0.6, (0.85, 0.62, 0.42), 1.0),
    "M_Marae_Rope": ("characters/Rope001", 0.15, (0.95, 0.85, 0.6), 1.0),
    "M_Marae_Sticks": ("characters/bark_willow", 0.3, (1, 1, 1), 1.0),
}


def find(folder, keys):
    for f in sorted(folder.iterdir()):
        low = f.name.lower()
        if any(k in low for k in keys) and low.endswith((".jpg", ".png")):
            return f
    return None


def load(path, mode):
    img = Image.open(path).convert(mode)
    if max(img.size) > SIZE:
        img = img.resize((SIZE, SIZE), Image.LANCZOS)
    return img


def saturate(rgb, sat):
    grey = rgb @ np.array([0.2126, 0.7152, 0.0722])
    return grey[..., None] + (rgb - grey[..., None]) * sat


def pack(name, folder, tint, sat, out=OUT):
    src = TEX / folder
    files = {}
    d = np.asarray(load(find(src, ("_diff", "_color")), "RGB"), float) / 255
    # tint and saturation of the Blender material, applied in linear space
    lin = np.where(d <= 0.04045, d / 12.92, ((d + 0.055) / 1.055) ** 2.4)
    lin = np.clip(saturate(lin, sat) * np.array(tint), 0, 1)
    d = np.where(lin <= 0.0031308, lin * 12.92, 1.055 * lin ** (1 / 2.4) - 0.055)
    files["D"] = f"T_{name}_D.png"
    Image.fromarray((d * 255 + 0.5).astype(np.uint8)).save(out / "Textures" / files["D"])

    nor_dx = find(src, ("nor_dx", "normaldx"))
    if nor_dx:
        n = load(nor_dx, "RGB")
    else:
        n = np.asarray(load(find(src, ("nor_gl", "normalgl")), "RGB")).copy()
        n[..., 1] = 255 - n[..., 1]
        n = Image.fromarray(n)
    files["N"] = f"T_{name}_N.png"
    n.save(out / "Textures" / files["N"])

    rough = np.asarray(load(find(src, ("_rough", "roughness")), "L"))
    ao_path = find(src, ("_ao", "ambientocclusion"))
    ao = np.asarray(load(ao_path, "L")) if ao_path else np.full_like(rough, 255)
    orm = np.stack([ao, rough, np.zeros_like(rough)], axis=-1).astype(np.uint8)
    files["ORM"] = f"T_{name}_ORM.png"
    Image.fromarray(orm).save(out / "Textures" / files["ORM"])
    return files


def main():
    (OUT / "Textures").mkdir(parents=True, exist_ok=True)
    reports = {}
    for part in PARTS:
        for fbx in (MAQ / part / "exports").glob("SM_*.fbx"):
            shutil.copy2(fbx, OUT / fbx.name)
            print("fbx", fbx.name)
        report = MAQ / part / "item-report.json"
        if report.exists():
            data = json.loads(report.read_text(encoding="utf-8"))
            reports[data["export_name"]] = {"sockets": data.get("sockets", {}), "physics": data.get("physics", {}),
                                            "ucx": data.get("ucx", [])}
    materials = {}
    for slot, (folder, tile, tint, sat) in MATERIALS.items():
        name = slot.replace("M_", "", 1)
        materials[slot] = {"textures": pack(name, folder, tint, sat), "tiling": round(1.0 / tile, 4),
                           "source": folder}
        print("textures", slot, materials[slot]["textures"]["D"])
    (OUT / "marae_materials.json").write_text(json.dumps({"materials": materials, "parts": reports}, indent=2),
                                              encoding="utf-8")
    print("->", OUT)


if __name__ == "__main__":
    main()
