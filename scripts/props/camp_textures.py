"""Fishermen camp (maquette campement-pecheurs of the second PC, T-082): copies the FBX of the 11 parts and packs their
CC0 photo textures into Unreal sets, on the scheme of marae_textures.py.

  python scripts/props/camp_textures.py
Output: art/export/Props/Camp/SM_Camp_*.fbx, Textures/T_Camp_<Name>_D/_N/_ORM.png, camp_materials.json (photo sets with
tiling, flat colours of the procedural materials, emissive fire, and per part label / mass / price / usage from
maquette.json "gameplay.stats"). The photo sets are the ones of factory/kits/camp_kit.py mats().
"""
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import marae_textures as MT  # noqa: E402

MAQ = MT.MAQ / "campement-pecheurs"
OUT = MT.ROOT / "art" / "export" / "Props" / "Camp"

# slot -> (texture set, tile (m), tint, saturation): camp_kit.py mats()
PHOTO = {
    "M_Camp_Post": ("characters/rough_wood", 0.55, (0.82, 0.66, 0.50), 1.0),
    "M_Camp_Bark": ("characters/tree_bark_03", 0.45, (0.80, 0.68, 0.55), 1.0),
    "M_Camp_Planks": ("props/minka/weathered_brown_planks", 1.9, (1.05, 0.92, 0.78), 1.0),
    "M_Camp_Thatch": ("props/minka/reed_roof_04", 1.0, (1.10, 0.96, 0.70), 0.9),
    "M_Camp_ThatchEdge": ("props/minka/thatch_roof_angled", 0.5, (1.05, 0.92, 0.66), 1.0),
    "M_Camp_Rope": ("characters/Rope001", 0.08, (0.95, 0.84, 0.62), 1.0),
    "M_Camp_Wicker": ("characters/Wicker002", 0.22, (1.12, 0.98, 0.70), 0.8),
    "M_Camp_Bamboo": ("characters/bamboo_veneer", 0.35, (1.0, 0.92, 0.72), 1.0),
    "M_Camp_CutWood": ("characters/fine_grained_wood", 0.25, (0.95, 0.75, 0.52), 1.0),
    "M_Camp_Stone": ("characters/Rock035", 0.5, (0.82, 0.80, 0.78), 1.0),
    "M_Camp_StoneLight": ("characters/Rock030", 0.5, (1, 1, 1), 1.0),
    "M_Camp_StoneMoss": ("props/minka/mossy_rock", 0.6, (1, 1, 1), 1.0),
    "M_Camp_Pandanus": ("characters/Wicker002", 0.18, (1.05, 1.08, 0.75), 0.7),
    "M_Camp_Linen": ("characters/hessian_230", 0.3, (1.0, 0.95, 0.85), 1.0),
    "M_Camp_Float": ("characters/fine_grained_wood", 0.08, (0.95, 0.72, 0.45), 1.0),
}
# procedural / flat materials -> two colours mixed by a local noise (M_Loot_Speckle), roughness, noise scale
SPECKLE = {
    "M_Camp_Charcoal": ((0.03, 0.03, 0.03), (0.10, 0.09, 0.08), 0.9, 2.0),
    "M_Camp_Ash": ((0.28, 0.27, 0.26), (0.36, 0.35, 0.33), 0.95, 1.0),
    "M_Camp_Fish": ((0.20, 0.27, 0.32), (0.55, 0.62, 0.66), 0.3, 0.6),
    "M_Camp_FishFin": ((0.16, 0.20, 0.24), (0.26, 0.30, 0.34), 0.4, 1.0),
    "M_Camp_FishEye": ((0.02, 0.02, 0.02), (0.03, 0.03, 0.03), 0.08, 1.0),
    "M_Camp_Line": ((0.74, 0.68, 0.54), (0.80, 0.74, 0.60), 0.7, 1.0),
    "M_Camp_NetCord": ((0.26, 0.20, 0.13), (0.34, 0.27, 0.18), 0.85, 1.0),
    "M_Camp_Hook": ((0.35, 0.34, 0.33), (0.45, 0.44, 0.42), 0.35, 1.0),
}
# emissive slots: fire of the camp (marae flame material, embers)
FIRE = {"M_Camp_Flame": "flame", "M_Camp_FlameCore": "flame_core", "M_Camp_Embers": "embers"}


def main():
    (OUT / "Textures").mkdir(parents=True, exist_ok=True)
    spec = json.loads((MAQ / "maquette.json").read_text(encoding="utf-8"))
    report = json.loads((MAQ / "camp-report.json").read_text(encoding="utf-8"))
    for fbx in sorted((MAQ / "exports").glob("SM_Camp_*.fbx")):
        shutil.copy2(fbx, OUT / fbx.name)
    parts = {}
    for name, stats in spec["gameplay"]["stats"].items():
        parts[name] = {**stats, "sockets": report.get(name, {}).get("sockets", []), "info": report.get(name, {}).get("info", {})}
    materials = {}
    for slot, (folder, tile, tint, sat) in PHOTO.items():
        materials[slot] = {"textures": MT.pack(slot.replace("M_", "", 1), folder, tint, sat, OUT),
                           "tiling": round(1.0 / tile, 4), "source": folder}
        print("textures", slot)
    speckle = {slot: {"a": a, "b": b, "roughness": r, "scale": sc} for slot, (a, b, r, sc) in SPECKLE.items()}
    (OUT / "camp_materials.json").write_text(json.dumps({"materials": materials, "speckle": speckle, "fire": FIRE,
                                                         "parts": parts}, indent=2, ensure_ascii=False), encoding="utf-8")
    print(len(parts), "parts ->", OUT)


if __name__ == "__main__":
    main()
