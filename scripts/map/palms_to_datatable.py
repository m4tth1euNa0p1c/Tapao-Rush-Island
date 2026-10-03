"""Convertit les emplacements de cocotiers (place_palms.py) en CSV importable comme DataTable UE.

Usage: python scripts/map/palms_to_datatable.py raraka

Le CSV utilise la structure de ligne moteur GameplayTagTableRow (seule structure dispo sans C++) :
  Tag        = chemin du static mesh (FName) -> PCG « Analyser la chaîne » en SoftObjectPath -> sélection du mesh
  DevComment = transformation "x,y,z|pitch,yaw,roll|sx,sy,sz" (cm, degrés) -> PCG « Analyser la chaîne » en $Transform
Sortie : docs/reference/map/raw/<ile>_vegetation/DT_<Ile>_Palms.csv (régénérable, ignoré par git)
Villages générés (scripts/map/place_villages.py) : les cocotiers sous les maisons, la place et la route sont retirés
(copie complète dans raw/<ile>_vegetation/_orig/).
"""
import csv
import importlib.util
import json
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[2]
MESH = "/Game/MonkeIsland/Environment/Vegetation/Palms/Meshes/SM_CoconutPalm_{v}.SM_CoconutPalm_{v}"
UPRIGHT = ("Straight", "Young", "Juvenile", "DeadSnag")  # petite inclinaison aléatoire en plus


def main():
    island = sys.argv[1] if len(sys.argv) > 1 else "raraka"
    src = ROOT / "docs" / "reference" / "map" / island / "vegetation" / "palms_ue.json"
    doc = json.loads(src.read_text(encoding="utf-8"))
    out = ROOT / "docs" / "reference" / "map" / "raw" / f"{island}_vegetation"
    out.mkdir(parents=True, exist_ok=True)
    rng = np.random.default_rng(doc["stats"].get("seed", 0) + 1)
    path = out / f"DT_{island.capitalize()}_Palms.csv"
    n = 0
    with open(path, "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, quoting=csv.QUOTE_MINIMAL)
        w.writerow(["---", "Tag", "DevComment"])
        for v, rows in sorted(doc["variants"].items()):
            tilt = v.startswith(UPRIGHT)
            for x, y, z, cap, s in rows:
                p, r = (rng.normal(0, 1.2), rng.normal(0, 1.2)) if tilt else (0.0, 0.0)
                xf = f"{x:.1f},{y:.1f},{z:.1f}|{p:.2f},{cap:.1f},{r:.2f}|{s:.3f},{s:.3f},{s:.3f}"
                w.writerow([f"P{n:06d}", MESH.format(v=v), xf])
                n += 1
    print(f"{n} lignes -> {path} ({path.stat().st_size / 1e6:.1f} Mo)")
    spec = importlib.util.spec_from_file_location("village_clearing", ROOT / "scripts" / "map" / "village_clearing.py")
    vc = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(vc)
    cleared = vc.refresh(out, [path.name], vc.Clearing.load(island))[path.name]
    if cleared:
        print(f"villages : {cleared} cocotiers retirés sous les villages")


if __name__ == "__main__":
    main()
