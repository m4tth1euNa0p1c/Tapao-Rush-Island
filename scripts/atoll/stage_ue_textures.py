"""Stage the atoll textures under their Unreal names (T_<Name>_D / _N (DirectX) / _R) for import.

Usage: python scripts/atoll/stage_ue_textures.py
Output: art/export/<Category>/Textures/T_*.png
"""
from pathlib import Path

import numpy as np
from PIL import Image

ART = Path(__file__).resolve().parents[2] / "art"
TEX = ART / "textures"
OUT = ART / "export"


def pick(folder, key):
    return sorted(p for p in (TEX / folder).iterdir() if key in p.name)[0]


def plant(stem):
    p = TEX / "plants"
    return {"D": p / f"{stem}_basecolor.png", "N": p / f"{stem}_normal_dx.png", "R": p / f"{stem}_roughness.png"}


def scan(folder):
    return {"D": pick(folder, "_diff_"), "N": pick(folder, "_nor_dx_"), "R": pick(folder, "_rough_")}


JOBS = {
    "Vegetation/Shrubs": {"PandanusLeaf": plant("pandanus"), "NaupakaLeaf": plant("naupaka"),
                          "TahinuLeaf": plant("tahinu"),
                          "MikiMikiTwig": {"D": TEX / "plants" / "mikimiki_twig_basecolor.png",
                                           "N": TEX / "plants" / "mikimiki_twig_normal_dx.png"},
                          "ShrubBark": scan("shrub/bark_brown_02")},
    "Vegetation/GroundCover": {"FernLobe": plant("fern_lobe"), "GrassBlade": plant("grass"),
                               "IpomoeaLeaf": plant("ipomoea")},
    "Vegetation/Palms": {"CoconutEophyll": plant("coconut_eophyll")},
    # coral rock: color graded toward Tuamotu grey-white (same as the terrain "feo" layer), scan normals/roughness
    "Props/Coral": {"CoralRock": dict(scan("ground/coral_ground_02"), D=TEX / "terrain_layers" / "feo_ch.png")},
}


def save(src, dst, kind):
    im = Image.open(src)
    if kind == "R":
        im = im.convert("L")
    elif kind == "D" and im.mode == "RGBA" and "Twig" in dst.name:
        pass  # keep the alpha (opacity mask of leaf cards)
    else:
        im = im.convert("RGB")
    im.save(dst)
    print(f"  {dst.name:28s} {im.size} {im.mode} <- {Path(src).name}")


def main():
    for cat, items in JOBS.items():
        out = OUT / cat / "Textures"
        out.mkdir(parents=True, exist_ok=True)
        for name, maps in items.items():
            for kind, src in maps.items():
                save(src, out / f"T_{name}_{kind}.png", kind)
    # Lambis shell (Poly Haven glTF): OpenGL normal -> DirectX, roughness = G of the ARM map
    lt = ART / "models" / "props" / "lambis_shell" / "textures"
    out = OUT / "Props" / "Beach" / "Textures"
    out.mkdir(parents=True, exist_ok=True)
    save(lt / "lambis_shell_diff_1k.jpg", out / "T_LambisShell_D.png", "D")
    n = np.asarray(Image.open(lt / "lambis_shell_nor_gl_1k.jpg").convert("RGB")).copy()
    n[..., 1] = 255 - n[..., 1]
    Image.fromarray(n).save(out / "T_LambisShell_N.png")
    arm = np.asarray(Image.open(lt / "lambis_shell_arm_1k.jpg").convert("RGB"))
    Image.fromarray(arm[..., 1]).save(out / "T_LambisShell_R.png")
    print("  T_LambisShell_N / _R converted (GL->DX, ARM.G)")


if __name__ == "__main__":
    main()
