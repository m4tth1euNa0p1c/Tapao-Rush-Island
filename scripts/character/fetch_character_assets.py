"""Download the CC0 textures and models used by the tiki creatures (scripts/character/tiki_creatures.py).

Usage: python scripts/character/fetch_character_assets.py [--res 1k]
Output: art/textures/characters/<asset>/, art/models/characters/<asset>/
Credits: the "Personnages" section of art/SOURCES.md is rewritten (the rest of the file is kept).
Every asset is CC0 (Poly Haven, ambientCG): no attribution required, credited anyway.
"""
import argparse
import io
import json
import urllib.request
import zipfile
from pathlib import Path

ART = Path(__file__).resolve().parents[2] / "art"
UA = {"User-Agent": "monke_island-asset-fetch/1.0"}

# (asset id, use)
POLYHAVEN = [
    ("fine_grained_wood", "masques en bois sculpté (Rôdeur des Ronces), hampes polies"),
    ("weathered_planks", "masque en planches du Veilleur des Cendres"),
    ("rough_wood", "manches de massue, chignon du Guerrier Coco, tige du villageois"),
    ("bark_willow", "manches noueux (lance, marteau), lianes"),
    ("hessian_230", "pagne en toile de jute du villageois"),
    ("bamboo_veneer", "sarbacane et carquois en bambou"),
    ("tree_bark_03", "peau d'écorce moussue du Rôdeur des Ronces"),
]
AMBIENTCG = [
    ("Bark014", "bourre de coco (corps des cocos : Guerrier Coco, villageois)"),
    ("Rope001", "cordes et ligatures (teintées jaune, beige, rouge sombre)"),
    ("Lava002", "roche volcanique veinée de lave (Veilleur des Cendres, massue d'obsidienne)"),
    ("Rock035", "roche sombre des facettes du Veilleur des Cendres"),
    ("Rock030", "lame de pierre de la lance du Guerrier Coco"),
    ("Wicker002", "panier tressé du villageois"),
    ("Moss002", "mousse sous les feuilles du Rôdeur des Ronces"),
]
MODELS = [
    ("bananas", "régime de bananes du panier du villageois"),
    ("food_apple_01", "fruits rouges du panier du villageois"),
]
REUSED = [  # already in the repository (scripts/fetch_atoll_textures.py)
    ("textures/ground/coral_ground_02", "relief piqueté de la carapace du Gardien Crabe", "https://polyhaven.com/a/coral_ground_02"),
    ("models/props/lambis_shell", "tête du marteau-coquillage du Gardien Crabe", "https://polyhaven.com/a/lambis_shell"),
]

PH_MAPS = {"Diffuse": "diff", "nor_gl": "nor_gl", "nor_dx": "nor_dx", "Rough": "rough", "AO": "ao"}


def get(url: str) -> bytes:
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=180) as r:
        return r.read()


def polyhaven(asset: str, res: str) -> None:
    files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
    out = ART / "textures" / "characters" / asset
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
    print(f"  ok  textures/characters/{asset}")


def ambientcg(asset: str, res: str) -> None:
    out = ART / "textures" / "characters" / asset
    if out.exists() and any(out.iterdir()):
        print(f"  skip textures/characters/{asset}")
        return
    out.mkdir(parents=True, exist_ok=True)
    data = get(f"https://ambientcg.com/get?file={asset}_{res.upper()}-JPG.zip")
    with zipfile.ZipFile(io.BytesIO(data)) as z:
        for n in z.namelist():
            if n.lower().endswith((".jpg", ".png")):
                (out / Path(n).name).write_bytes(z.read(n))
    print(f"  ok  textures/characters/{asset}")


def model(asset: str, res: str = "1k") -> None:
    files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
    entry = files["gltf"][res]["gltf"]
    out = ART / "models" / "characters" / asset
    out.mkdir(parents=True, exist_ok=True)
    dest = out / Path(entry["url"]).name
    if not dest.exists():
        dest.write_bytes(get(entry["url"]))
    for rel, inc in entry.get("include", {}).items():
        f = out / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        if not f.exists():
            f.write_bytes(get(inc["url"]))
    print(f"  ok  models/characters/{asset}")


def write_credits() -> None:
    path = ART / "SOURCES.md"
    text = path.read_text(encoding="utf-8") if path.exists() else "# Sources (toutes CC0)\n"
    start, end = "<!-- personnages:debut -->", "<!-- personnages:fin -->"
    lines = [start, "", "## Personnages (tiki) — scripts/character/fetch_character_assets.py", ""]
    lines += [f"- `textures/characters/{a}` — {w} — https://polyhaven.com/a/{a}" for a, w in POLYHAVEN]
    lines += [f"- `textures/characters/{a}` — {w} — https://ambientcg.com/view?id={a}" for a, w in AMBIENTCG]
    lines += [f"- `models/characters/{a}` — {w} — https://polyhaven.com/a/{a}" for a, w in MODELS]
    lines += [f"- `{p}` (réutilisé) — {w} — {u}" for p, w, u in REUSED]
    lines += ["", end]
    block = "\n".join(lines)
    if start in text and end in text:
        text = text[:text.index(start)] + block + text[text.index(end) + len(end):]
    else:
        text = text.rstrip("\n") + "\n\n" + block + "\n"
    path.write_text(text, encoding="utf-8")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--res", default="1k")
    res = ap.parse_args().res
    for a, _ in POLYHAVEN:
        polyhaven(a, res)
    for a, _ in AMBIENTCG:
        ambientcg(a, res)
    for a, _ in MODELS:
        model(a)
    write_credits()
    print("credits:", ART / "SOURCES.md")


if __name__ == "__main__":
    main()
