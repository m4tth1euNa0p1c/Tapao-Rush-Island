"""Download the CC0 textures and models of the rural minka and of the Japanese village
(scripts/props/minka_rurale.py, scripts/props/village_japonais.py).

Usage: python scripts/props/fetch_minka_assets.py
Output: art/textures/props/minka/<asset>/, art/models/props/<asset>/
Credits: the "Minka" section of art/SOURCES.md is rewritten (the rest of the file is kept). Every asset is CC0
(Poly Haven), credited anyway.
"""
import json
import urllib.request
from pathlib import Path

ART = Path(__file__).resolve().parents[2] / "art"
UA = {"User-Agent": "monke_island-asset-fetch/1.0"}

# (asset id, resolution, use)
TEXTURES = [
    ("reed_roof_04", "2k", "chaume de la toiture (paille dorée)"),
    ("thatch_roof_angled", "1k", "tranche du chaume au bord des avant-toits, faîtage"),
    ("tatami_mat", "2k", "tatamis"),
    ("patterned_clay_plaster", "2k", "enduit terre/chaux ivoire des remplissages"),
    ("clay_plaster", "1k", "enduit du doma, fourneau kamado"),
    ("dirt_floor", "2k", "terre battue du doma"),
    ("hinoki_planks", "2k", "planchers des pièces en bois (hinoki)"),
    ("weathered_brown_planks", "1k", "planches de l'engawa"),
    ("japanese_cedar_planks", "1k", "porte coulissante d'entrée, coffre (sugi)"),
    ("mossy_rock", "1k", "pierres d'appui sous les poteaux"),
    ("rough_linen", "1k", "trame du papier washi, des fusuma, coussins et futon (normale seulement)"),
    ("clay_roof_tiles", "1k", "village : toits de tuiles des entrepôts kura"),
    ("pine_bark", "1k", "village : troncs des pins"),
    ("forest_leaves_02", "1k", "village : haies et feuillage"),
    ("brown_mud_02", "1k", "village : rizières, levées de terre"),
    ("leafy_grass", "1k", "village : sol herbeux de la présentation"),
]
MODELS = [
    ("ceramic_vase_02", "petit vase du tokonoma"),
    ("tea_set_01", "service à thé sur la table basse"),
    ("wooden_bucket_01", "seau du doma"),
    ("wooden_broom", "balai du doma"),
    ("wooden_bowl_01", "bols de la zone de cuisson"),
]
REUSED = [("textures/characters/rough_wood", "bois sombre des poteaux et poutres", "https://polyhaven.com/a/rough_wood"),
          ("textures/characters/bamboo_veneer", "perche jizai-kagi au-dessus de l'irori",
           "https://polyhaven.com/a/bamboo_veneer")]
PH_MAPS = {"Diffuse": "diff", "nor_gl": "nor_gl", "Rough": "rough", "AO": "ao"}


def get(url):
    with urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=180) as r:
        return r.read()


def texture(asset, res):
    files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
    out = ART / "textures" / "props" / "minka" / asset
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
    print(f"  ok  textures/props/minka/{asset}")


def model(asset, res="1k"):
    files = json.loads(get(f"https://api.polyhaven.com/files/{asset}"))
    entry = files["gltf"][res]["gltf"]
    out = ART / "models" / "props" / asset
    out.mkdir(parents=True, exist_ok=True)
    dest = out / Path(entry["url"]).name
    if not dest.exists():
        dest.write_bytes(get(entry["url"]))
    for rel, inc in entry.get("include", {}).items():
        f = out / rel
        f.parent.mkdir(parents=True, exist_ok=True)
        if not f.exists():
            f.write_bytes(get(inc["url"]))
    print(f"  ok  models/props/{asset}")


def write_credits():
    path = ART / "SOURCES.md"
    text = path.read_text(encoding="utf-8") if path.exists() else "# Sources (toutes CC0)\n"
    start, end = "<!-- minka:debut -->", "<!-- minka:fin -->"
    lines = [start, "", "## Minka rurale — scripts/props/fetch_minka_assets.py", ""]
    lines += [f"- `textures/props/minka/{a}` — {w} — https://polyhaven.com/a/{a}" for a, _, w in TEXTURES]
    lines += [f"- `models/props/{a}` — {w} — https://polyhaven.com/a/{a}" for a, w in MODELS]
    lines += [f"- `{p}` (réutilisé) — {w} — {u}" for p, w, u in REUSED]
    lines += ["", end]
    block = "\n".join(lines)
    if start in text and end in text:
        text = text[:text.index(start)] + block + text[text.index(end) + len(end):]
    else:
        text = text.rstrip("\n") + "\n\n" + block + "\n"
    path.write_text(text, encoding="utf-8")


def main():
    for a, res, _ in TEXTURES:
        texture(a, res)
    for a, _ in MODELS:
        model(a)
    write_credits()
    print("credits:", ART / "SOURCES.md")


if __name__ == "__main__":
    main()
