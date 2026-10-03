"""Download CC0 PBR textures for the Tuamotu atoll look (Poly Haven + ambientCG).

Usage: python scripts/fetch_atoll_textures.py [--res 2k]
Output: art/textures/<category>/<asset>/, art/hdri/, art/models/
"""
import argparse
import io
import json
import urllib.request
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent / "art"
UA = {"User-Agent": "monke_island-asset-fetch/1.0"}

# (category, asset id, why)
POLYHAVEN = [
    ("palm", "palm_tree_bark", "tronc de cocotier gris blanchi par le soleil"),
    ("palm", "palm_bark", "tronc de cocotier sombre / humide, base du stipe"),
    ("ground", "coast_sand_01", "sable de plage"),
    ("ground", "damp_beach_sand", "sable mouillé, zone de ressac"),
    ("ground", "shell_floor_01", "haut de plage : sable compact + débris de coquillages"),
    ("ground", "coral_gravel", "gravier corallien (soupe de corail, intérieur du motu)"),
    ("ground", "coral_ground_02", "platier / feo : calcaire corallien poreux"),
    ("ground", "dry_decay_leaves", "litière sèche sous la cocoteraie"),
    ("ground", "forrest_sand_01", "terre de cocoteraie : sable gris + matière organique"),
    ("ground", "aerial_beach_01", "sable immergé à rides (fond du lagon, estran)"),
    ("shrub", "bark_brown_02", "écorce des arbustes (tahinu, miki-miki)"),
]
# ambientCG: white coral sands
AMBIENTCG = [
    ("ground", "Ground092A", "sable corallien blanc sec (plage lagon)"),
    ("ground", "Ground092B", "sable blanc mouillé"),
    ("ground", "Ground101", "sable blanc fin uniforme (hoa / bancs de sable)"),
    ("ground", "Ground033", "sable corallien blanc sec, relief doux (plage, arrière-plage)"),
    ("ground", "Ground072", "litière fibreuse (bourre de coco, brindilles) sous la cocoteraie"),
]
# Poly Haven models (glTF + textures)
MODELS = [
    ("props", "lambis_shell", "coquillage Lambis (araignée de mer), laisse de mer"),
]
HDRI = ["spiaggia_di_mondello", "secluded_beach", "kloofendal_48d_partly_cloudy_puresky"]

PH_MAPS = {"Diffuse": "diff", "nor_gl": "nor_gl", "nor_dx": "nor_dx", "Rough": "rough",
           "Displacement": "disp", "AO": "ao"}


def get(url: str) -> bytes:
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=120) as r:
        return r.read()


def polyhaven(cat: str, asset: str, res: str) -> None:
    files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
    out = ROOT / "textures" / cat / asset
    out.mkdir(parents=True, exist_ok=True)
    for key, suffix in PH_MAPS.items():
        if key not in files:
            continue
        fmt = files[key].get(res) or files[key][sorted(files[key])[0]]
        entry = fmt.get("jpg") or fmt.get("png")
        ext = "jpg" if "jpg" in fmt else "png"
        dest = out / f"{asset}_{suffix}_{res}.{ext}"
        if not dest.exists():
            dest.write_bytes(get(entry["url"]))
    print(f"  ok  {cat}/{asset}")


def ambientcg(cat: str, asset: str, res: str) -> None:
    out = ROOT / "textures" / cat / asset
    if out.exists() and any(out.iterdir()):
        print(f"  skip {cat}/{asset}")
        return
    out.mkdir(parents=True, exist_ok=True)
    data = get(f"https://ambientcg.com/get?file={asset}_{res.upper()}-JPG.zip")
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for n in z.namelist():
            if n.lower().endswith((".jpg", ".png")):
                (out / Path(n).name).write_bytes(z.read(n))
    print(f"  ok  {cat}/{asset}")


def model(cat: str, asset: str, res: str = "1k") -> None:
    files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
    entry = files["gltf"][res]["gltf"]
    out = ROOT / "models" / cat / asset
    out.mkdir(parents=True, exist_ok=True)
    dest = out / Path(entry["url"]).name
    if not dest.exists():
        dest.write_bytes(get(entry["url"]))
    for rel, inc in entry.get("include", {}).items():
        f = out / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        if not f.exists():
            f.write_bytes(get(inc["url"]))
    print(f"  ok  models/{cat}/{asset}")


def hdri(asset: str) -> None:
    files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
    out = ROOT / "hdri"
    out.mkdir(parents=True, exist_ok=True)
    dest = out / f"{asset}_4k.hdr"
    if not dest.exists():
        dest.write_bytes(get(files["hdri"]["4k"]["hdr"]["url"]))
    print(f"  ok  hdri/{asset}")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--res", default="2k")
    res = ap.parse_args().res
    for cat, a, _ in POLYHAVEN:
        polyhaven(cat, a, res)
    for cat, a, _ in AMBIENTCG:
        ambientcg(cat, a, res)
    for a in HDRI:
        hdri(a)
    for cat, a, _ in MODELS:
        model(cat, a)
    lines = ["# Sources (toutes CC0)", ""]
    lines += [f"- `{c}/{a}` — {w} — https://polyhaven.com/a/{a}" for c, a, w in POLYHAVEN]
    lines += [f"- `{c}/{a}` — {w} — https://ambientcg.com/view?id={a}" for c, a, w in AMBIENTCG]
    lines += [f"- `hdri/{a}` — https://polyhaven.com/a/{a}" for a in HDRI]
    lines += [f"- `models/{c}/{a}` — {w} — https://polyhaven.com/a/{a}" for c, a, w in MODELS]
    (ROOT / "SOURCES.md").write_text("\n".join(lines) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
