"""Common loot (maquette loot-butin-commun of the second PC): copies the FBX of the 12 items and packs their CC0
textures into Unreal texture sets, on the scheme of marae_textures.py (T-076).

  python scripts/props/loot_textures.py
Output: art/export/Props/Loot/SM_Loot_*.fbx, Textures/T_Loot_<Name>_D/_N/_ORM.png, loot_materials.json (slot ->
  textures + tiling for the photo sets; colours of the procedural ones; per item value, mass and label from
  maquette.json "gameplay.stats"). The photo sets are the ones of factory/kits/loot_kit.py M().
"""
import json
import shutil
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import marae_textures as MT  # noqa: E402

MAQ = MT.MAQ / "loot-butin-commun"
OUT = MT.ROOT / "art" / "export" / "Props" / "Loot"

# slot -> (texture set, tile (m), tint, saturation): factory/kits/loot_kit.py M()
PHOTO = {
    "M_Loot_CoconutHusk": ("characters/Bark014", 0.06, (0.75, 0.55, 0.38), 1.0),
    "M_Loot_CoconutInner": ("characters/fine_grained_wood", 0.08, (0.42, 0.24, 0.13), 0.8),
    "M_Loot_Wicker": ("characters/Wicker002", 0.22, (1.15, 1.02, 0.72), 0.75),
    "M_Loot_Pandanus": ("characters/Wicker002", 0.18, (1.05, 1.10, 0.72), 0.7),
    "M_Loot_Rope": ("characters/Rope001", 0.05, (0.92, 0.80, 0.58), 0.8),
    "M_Loot_Wood": ("characters/fine_grained_wood", 0.12, (0.80, 0.55, 0.34), 1.0),
    "M_Loot_WoodOld": ("characters/rough_wood", 0.10, (0.85, 0.62, 0.42), 1.0),
    "M_Loot_Planks": ("characters/weathered_planks", 0.18, (0.78, 0.58, 0.40), 1.0),
    "M_Loot_Stone": ("characters/Rock030", 0.07, (0.80, 0.92, 0.82), 0.9),
}
# procedural materials of the maquette -> two colours mixed by a noise in the local space, roughness, noise scale
SPECKLE = {
    "M_Loot_Gourd": ((0.28, 0.13, 0.05), (0.62, 0.36, 0.15), 0.38, 0.18),
    "M_Loot_Seed": ((0.20, 0.09, 0.04), (0.42, 0.22, 0.10), 0.18, 1.4),
    "M_Loot_ShellBand": ((0.82, 0.48, 0.30), (0.97, 0.92, 0.84), 0.25, 0.4),
    "M_Loot_ShellCockle": ((0.90, 0.85, 0.78), (0.96, 0.92, 0.86), 0.30, 0.6),
    "M_Loot_DarkCord": ((0.10, 0.07, 0.04), (0.14, 0.09, 0.06), 0.80, 1.0),
}


def main():
    (OUT / "Textures").mkdir(parents=True, exist_ok=True)
    spec = json.loads((MAQ / "maquette.json").read_text(encoding="utf-8"))
    report = json.loads((MAQ / "loot-report.json").read_text(encoding="utf-8"))
    items = {}
    for fbx in sorted((MAQ / "exports").glob("SM_Loot_*.fbx")):
        shutil.copy2(fbx, OUT / fbx.name)
        name = fbx.stem
        stats = spec["gameplay"]["stats"].get(name, {})
        items[name] = {"label": report.get(name, {}).get("label", name), "value": stats.get("value", 1),
                       "mass_kg": stats.get("mass_kg", 0.2), "craft": stats.get("craft", ""),
                       "dims_m": report.get(name, {}).get("dims_m")}
    materials = {}
    for slot, (folder, tile, tint, sat) in PHOTO.items():
        name = slot.replace("M_", "", 1)
        materials[slot] = {"textures": MT.pack(name, folder, tint, sat, OUT), "tiling": round(1.0 / tile, 4),
                           "source": folder}
        print("textures", slot)
    speckle = {slot: {"a": a, "b": b, "roughness": r, "scale": sc} for slot, (a, b, r, sc) in SPECKLE.items()}
    (OUT / "loot_materials.json").write_text(json.dumps({"materials": materials, "speckle": speckle, "items": items},
                                                        indent=2, ensure_ascii=False), encoding="utf-8")
    print(len(items), "items ->", OUT)


if __name__ == "__main__":
    main()
