"""Layout of the Marae des Perles in Lvl_Raraka, from the maquette scene (OneDrive factory/kits/marae_scene.py).

  python scripts/props/marae_layout.py  ->  art/export/Props/Marae/marae_layout.json
Blender (metres, the bean faces -Y) -> UE local (cm): (x, -y, z) * 100, yaw = -degrees(rot_z). The local frame is the
pyramid's (stairs toward local +Y); the whole site is then turned by SITE_YAW and moved to SITE (UE cm), a flat
coral sand clearing on the lagoon beach 145 m west of the PlayerStart, away from the villages, the totems and the
real village of Motutapu (only grass tufts within 8 m in the vegetation tables).
Same seeds as the maquette for the fallen stones and the scatter, so the site matches the reference sheet.
"""
import json
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "art" / "export" / "Props" / "Marae" / "marae_layout.json"
SITE = (-667786.0, -1013264.0, 198.0)   # ground at 203.7 cm (trace), pyramid sunk 5 cm
SITE_YAW = -90.0                        # stairs (local +Y) toward the PlayerStart (east, +X)

TIERS = [(5.2, 4.2), (3.8, 3.0), (2.5, 1.9)]
TIER_H = 0.6
TOP = 1.8
STAIR_Y0 = -4.11

MARAE = "/Game/MonkeIsland/Environment/Marae"
VEG = "/Game/MonkeIsland/Environment/Vegetation"
PROPS = "/Game/MonkeIsland/Environment/Props"
MESH = {
    "SM_Fern_A": f"{VEG}/GroundCover/Meshes/SM_Fern_A", "SM_Fern_B": f"{VEG}/GroundCover/Meshes/SM_Fern_B",
    "SM_Grass_Tuft": f"{VEG}/GroundCover/Meshes/SM_Grass_Tuft", "SM_Grass_Short": f"{VEG}/GroundCover/Meshes/SM_Grass_Short",
    "SM_Grass_Dry": f"{VEG}/GroundCover/Meshes/SM_Grass_Dry",
    "SM_Naupaka_A": f"{VEG}/Shrubs/Meshes/SM_Naupaka_A", "SM_Naupaka_B": f"{VEG}/Shrubs/Meshes/SM_Naupaka_B",
    "SM_Pandanus_Young": f"{VEG}/Shrubs/Meshes/SM_Pandanus_Young", "SM_MikiMiki_A": f"{VEG}/Shrubs/Meshes/SM_MikiMiki_A",
    "SM_CoconutPalm_Leaning_B": f"{VEG}/Palms/Meshes/SM_CoconutPalm_Leaning_B",
    "SM_CoconutPalm_Sabre_A": f"{VEG}/Palms/Meshes/SM_CoconutPalm_Sabre_A",
    "SM_CoconutPalm_Young_A": f"{VEG}/Palms/Meshes/SM_CoconutPalm_Young_A",
    "SM_CoralBlock_A": f"{PROPS}/Coral/Meshes/SM_CoralBlock_A", "SM_CoralBlock_B": f"{PROPS}/Coral/Meshes/SM_CoralBlock_B",
    "SM_CoralBlock_F": f"{PROPS}/Coral/Meshes/SM_CoralBlock_F",
    "SM_CoralRubble_A": f"{PROPS}/Coral/Meshes/SM_CoralRubble_A", "SM_CoralRubble_B": f"{PROPS}/Coral/Meshes/SM_CoralRubble_B",
    "SM_CoralRubble_D": f"{PROPS}/Coral/Meshes/SM_CoralRubble_D", "SM_CoralRubble_F": f"{PROPS}/Coral/Meshes/SM_CoralRubble_F",
    "SM_Coconut_Brown": f"{PROPS}/Coconut/Meshes/SM_Coconut_Brown", "SM_Coconut_Green": f"{PROPS}/Coconut/Meshes/SM_Coconut_Green",
    "SM_CoconutFrond_Dry_A": f"{PROPS}/Coconut/Meshes/SM_CoconutFrond_Dry_A",
    "SM_Marae_Pyramid": f"{MARAE}/Meshes/SM_Marae_Pyramid", "SM_Marae_Altar": f"{MARAE}/Meshes/SM_Marae_Altar",
    "SM_Marae_StoneWall": f"{MARAE}/Meshes/SM_Marae_StoneWall",
}


def ue(x, y, z):
    return [round(x * 100, 1), round(-y * 100, 1), round(z * 100, 1)]


def yaw(rot_z):
    return round(-math.degrees(rot_z), 1)


def main():
    rng = random.Random(42)
    items = []

    def add(kind, asset, loc, rot=0.0, scale=1.0, folder="Structure"):
        items.append({"kind": kind, "asset": asset, "loc": loc, "yaw": rot, "scale": scale, "folder": folder})

    # structure: pyramid, altar on the summit
    add("mesh", MESH["SM_Marae_Pyramid"], [0, 0, 0])
    add("mesh", MESH["SM_Marae_Altar"], ue(0, 0.25, TOP))
    # sacred objects: pearl artifact on the altar (rare, 1 500 $), offerings and conch on the summit and at the stairs
    add("bp", f"{MARAE}/Blueprints/BP_Marae_PearlArtifact", ue(0, 0.25, TOP + 0.37), 0.0, 1.0, "Objets")
    add("bp", f"{MARAE}/Blueprints/BP_Marae_Offerings", ue(-0.85, -0.2, TOP), yaw(0.3), 1.0, "Objets")
    add("bp", f"{MARAE}/Blueprints/BP_Marae_Conch", ue(0.8, -0.25, TOP + 0.01), yaw(2.4), 1.0, "Objets")
    add("bp", f"{MARAE}/Blueprints/BP_Marae_Offerings", ue(-0.95, STAIR_Y0 - 0.55, 0.0), yaw(-0.4), 1.0, "Objets")
    add("bp", f"{MARAE}/Blueprints/BP_Marae_Conch", ue(2.25, -2.3, 0.0), yaw(0.9), 1.0, "Objets")
    # torches: both sides of the stairs, back corners (1.35 m) and the first tier front corners (1.0 m)
    for (x, y, z), h in (((-1.05, -3.66, 0), 1.35), ((1.05, -3.66, 0), 1.35), ((-2.45, 1.95, 0), 1.35),
                         ((2.45, 1.95, 0), 1.35), ((-1.45, -1.8, 0.6), 1.0), ((1.45, -1.8, 0.6), 1.0)):
        add("bp", f"{MARAE}/Blueprints/BP_Marae_Torch", ue(x, y, z), yaw(rng.uniform(0, 6.28)), round(h / 1.35, 3),
            "Torches")
    # stones fallen at the foot of the walls: wall chunks of the maquette, lying in the sand
    w0, d0 = TIERS[0]
    for x, y, rot in ((-2.95, 0.6, 1.57), (2.9, -0.9, 1.4), (1.6, 2.45, 0.1), (-1.9, 2.5, -0.2)):
        add("mesh", MESH["SM_Marae_StoneWall"], ue(x, y, -0.12), yaw(rot), 0.7)
    # coral blocks and rubble around (ring), as in _scatter()
    for names, n, ring in ((["SM_CoralBlock_A", "SM_CoralBlock_F", "SM_CoralBlock_B"], 7, (3.3, 5.5)),
                           (["SM_CoralRubble_A", "SM_CoralRubble_B", "SM_CoralRubble_D", "SM_CoralRubble_F"], 60, (2.6, 6.0))):
        for _ in range(n):
            name = rng.choice(names)
            a = rng.uniform(0, math.tau)
            r = rng.uniform(*ring)
            px, py = math.cos(a) * r, math.sin(a) * r * 0.85
            rot, sc = rng.uniform(0, 6.28), rng.uniform(0.6, 1.1)
            if abs(px) < 1.0 and py < -2.0:
                continue
            add("mesh", MESH[name], ue(px, py, 0), yaw(rot), round(sc, 2), "Vegetation")
    # tufts at the foot of the walls and in the tier joints (not on the stairs)
    for _ in range(46):
        tier = rng.choice((0, 0, 0, 1, 2))
        ang = rng.uniform(0, math.tau)
        if tier == 0:
            px = math.cos(ang) * (w0 / 2 + rng.uniform(0.0, 0.5))
            py = math.sin(ang) * (d0 / 2 + rng.uniform(0.0, 0.5))
            z = 0.0
        else:
            w, d = TIERS[tier - 1]
            w2, d2 = TIERS[tier]
            kk = rng.uniform(0.0, 1.0)
            px = math.cos(ang) * ((w + w2) / 4 + kk * 0.1)
            py = math.sin(ang) * ((d + d2) / 4 + kk * 0.1)
            z = tier * TIER_H
        if abs(px) < 0.8 and py < -0.5:
            continue
        name = rng.choice(["SM_Grass_Tuft", "SM_Grass_Short", "SM_Grass_Dry", "SM_Fern_A", "SM_Fern_B"]
                          if tier == 0 else ["SM_Grass_Short", "SM_Grass_Tuft", "SM_Grass_Dry"])
        sc = rng.uniform(0.35, 0.6) if name.startswith("SM_Fern") else rng.uniform(0.6, 1.0)
        add("mesh", MESH[name], ue(px, py, z), yaw(rng.uniform(0, 6.28)), round(sc, 2), "Vegetation")
    # bushes and palms around, as in the maquette
    for name, (x, y), rot, sc in (("SM_Naupaka_A", (-4.6, 1.6), 0.4, 0.75), ("SM_Naupaka_B", (4.4, 2.4), 2.0, 0.7),
                                  ("SM_Pandanus_Young", (-4.2, -2.9), 1.0, 0.8), ("SM_MikiMiki_A", (5.0, -1.0), 3.0, 0.6),
                                  ("SM_CoconutPalm_Leaning_B", (-6.5, 5.5), 0.6, 0.65),
                                  ("SM_CoconutPalm_Sabre_A", (6.8, 4.6), 2.6, 0.6),
                                  ("SM_CoconutPalm_Young_A", (4.8, 6.2), 1.2, 0.55)):
        add("mesh", MESH[name], ue(x, y, 0), yaw(rot), sc, "Vegetation")
    for name, (x, y, z) in (("SM_Coconut_Brown", (5.4, 3.6, 0.05)), ("SM_Coconut_Green", (5.9, 4.1, 0.05)),
                            ("SM_Coconut_Brown", (-5.8, 4.6, 0.05)), ("SM_CoconutFrond_Dry_A", (-5.2, 3.8, 0.02))):
        add("mesh", MESH[name], ue(x, y, z), yaw(rng.uniform(0, 6.28)), 1.0, "Vegetation")

    OUT.write_text(json.dumps({"site": SITE, "site_yaw": SITE_YAW, "items": items}, indent=1), encoding="utf-8")
    print(len(items), "items ->", OUT)


if __name__ == "__main__":
    main()
