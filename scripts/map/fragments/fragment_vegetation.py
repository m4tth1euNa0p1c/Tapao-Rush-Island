"""Filter the Raraka vegetation tables onto one fragment (T-089, lot L3).

The fragment maps keep the world frame of Lvl_Raraka (offset 0, ADR-004 / T-083), so the instances of
DT_Raraka_Palms and DT_Raraka_{Hard,Soft}_NN are reused as they are: same meshes (/Game/MonkeIsland/Environment/
Vegetation and /Props, which the runtime village clearing of BP_VillageSpawner relies on), same transforms.

An instance is kept when:
- it lies inside the terrain extent of the fragment (georef.json, terrain_extent_cm);
- the fragment terrain under it is the original terrain: build_fragment.py turned the excluded motu into reef
  flat and lowered the margin, so wherever the fragment heightmap differs from the Raraka heightmap by more than
  --tolerance (default 0.15 m) in the 3x3 pixels around the instance, the instance is dropped. This removes
  every plant and prop of the excluded motu (now under water) and of the lowered floor, and keeps the kept land,
  its beaches and the untouched reef flat (coral blocks) around it.

Palms go to the Hard tables (same collision setting as PCG_Raraka_VegHard). Tables are split in k-d blocks of at
most --max-rows rows (PCG cost grows with the square of the points per actor, see README of Raraka).

    python scripts/map/fragments/fragment_vegetation.py raraka FRG_Raraka_01 [--tolerance 0.15] [--max-rows 20000]

Outputs, in docs/reference/map/raw/raraka_fragments/<FRG>/vegetation/ (regenerable, not versioned):
- DT_<FRG>_{Hard,Soft}_NN.csv (GameplayTagTableRow: Tag = mesh path, DevComment = transform);
- vegetation.json: counts per source table and per group, blocks (rows, bounds in cm), dropped instances.
A copy of vegetation.json goes to docs/reference/map/raraka/fragments/<FRG>/vegetation.json (versioned), with
preview_vegetation.png: kept instances in green, dropped ones (inside the terrain extent) in red, over
preview_relief.png (approximate placement: the relief preview has a caption band).
"""
import argparse
import csv
import json
import sys
from pathlib import Path

import numpy as np
from PIL import Image

ROOT = Path(__file__).resolve().parents[3]


def load_heightmap(path, meta):
    raw = np.asarray(Image.open(path), dtype=np.float64)
    return (meta["min_m"] + raw / 65535.0 * (meta["max_m"] - meta["min_m"])).astype(np.float32)


def kd_split(rows, max_rows):
    """Split rows (list of (x, y, line)) by median until each block has at most max_rows rows."""
    if len(rows) <= max_rows:
        return [rows]
    xs = [r[0] for r in rows]
    ys = [r[1] for r in rows]
    axis = 0 if (max(xs) - min(xs)) >= (max(ys) - min(ys)) else 1
    rows = sorted(rows, key=lambda r: r[axis])
    mid = len(rows) // 2
    return kd_split(rows[:mid], max_rows) + kd_split(rows[mid:], max_rows)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("island")
    ap.add_argument("fragment")
    ap.add_argument("--tolerance", type=float, default=0.15, help="max height change (m) kept under an instance")
    ap.add_argument("--max-rows", type=int, default=20000)
    args = ap.parse_args()

    ref = ROOT / "docs/reference/map" / args.island
    src_geo = json.loads((ref / "terrain/georef.json").read_text(encoding="utf-8"))
    frag_geo = json.loads((ref / "fragments" / args.fragment / "georef.json").read_text(encoding="utf-8"))
    veg_dir = ROOT / "docs/reference/map/raw" / f"{args.island}_vegetation"
    out_dir = ROOT / "docs/reference/map/raw" / f"{args.island}_fragments" / args.fragment / "vegetation"
    out_dir.mkdir(parents=True, exist_ok=True)

    src_h = load_heightmap(ref / "terrain" / src_geo["heightmap_16"]["file"], src_geo["heightmap_16"])
    frag_h = load_heightmap(ROOT / frag_geo["heightmap_16"]["file"], frag_geo["heightmap_16"])
    win = frag_geo["source_window_px"]
    r0, c0 = win["row0"], win["col0"]
    if frag_h.shape != (win["row1"] - r0, win["col1"] - c0):
        sys.exit(f"heightmap shape {frag_h.shape} does not match the source window {win}")
    changed = np.abs(frag_h - src_h[r0:win["row1"], c0:win["col1"]]) > args.tolerance
    # Dilate by one pixel: an instance next to a changed pixel is dropped too
    pad = np.pad(changed, 1, constant_values=True)
    near_changed = np.zeros_like(changed)
    for dr in (0, 1, 2):
        for dc in (0, 1, 2):
            near_changed |= pad[dr:dr + changed.shape[0], dc:dc + changed.shape[1]]

    px = src_geo["ground_m_per_px"]
    half_w, half_h = src_geo["extent_m"][0] / 2.0, src_geo["extent_m"][1] / 2.0
    ex0, ey0, ex1, ey1 = frag_geo["terrain_extent_cm"]

    sources = [("Hard", veg_dir / "DT_Raraka_Palms.csv")]
    sources += [("Hard", p) for p in sorted(veg_dir.glob("DT_Raraka_Hard_*.csv"))]
    sources += [("Soft", p) for p in sorted(veg_dir.glob("DT_Raraka_Soft_*.csv"))]

    kept = {"Hard": [], "Soft": []}
    dropped_xy = []
    report = {"fragment": args.fragment, "tolerance_m": args.tolerance, "sources": {}, "meshes": {},
              "dropped_changed_terrain": 0}
    for physics, path in sources:
        n_in_box = n_kept = 0
        with path.open(encoding="utf-8", newline="") as f:
            for row in csv.DictReader(f):
                x, y, _z = (float(v) for v in row["DevComment"].split("|")[0].split(","))
                if not (ex0 <= x < ex1 and ey0 <= y < ey1):
                    continue
                n_in_box += 1
                col = int((x / 100.0 + half_w) / px) - c0
                rr = int((y / 100.0 + half_h) / px) - r0
                if not (0 <= rr < changed.shape[0] and 0 <= col < changed.shape[1]) or near_changed[rr, col]:
                    report["dropped_changed_terrain"] += 1
                    dropped_xy.append((x, y))
                    continue
                n_kept += 1
                kept[physics].append((x, y, row["Tag"], row["DevComment"]))
                mesh = row["Tag"].rsplit(".", 1)[-1]
                report["meshes"][mesh] = report["meshes"].get(mesh, 0) + 1
        report["sources"][path.name] = {"in_box": n_in_box, "kept": n_kept}

    # Remove the CSV of a previous run before writing the new blocks
    for old in out_dir.glob(f"DT_{args.fragment}_*.csv"):
        old.unlink()
    blocks = []
    for physics in ("Hard", "Soft"):
        for i, block in enumerate(kd_split(kept[physics], args.max_rows)):
            name = f"DT_{args.fragment}_{physics}_{i:02d}"
            with (out_dir / f"{name}.csv").open("w", encoding="utf-8", newline="") as f:
                w = csv.writer(f)
                w.writerow(["---", "Tag", "DevComment"])
                for j, (_x, _y, tag, dev) in enumerate(block):
                    w.writerow([f"{physics[0]}{i:02d}_{j:06d}", tag, dev])
            xs = [b[0] for b in block]
            ys = [b[1] for b in block]
            blocks.append({"name": name, "physics": physics, "rows": len(block),
                           "bounds_cm": [min(xs), min(ys), max(xs), max(ys)],
                           "file": str((out_dir / f"{name}.csv").relative_to(ROOT)).replace("\\", "/")})
    report["blocks"] = blocks
    report["totals"] = {p: len(kept[p]) for p in kept}
    report["changed_terrain_px"] = int(changed.sum())
    text = json.dumps(report, indent=1, ensure_ascii=False)
    (out_dir / "vegetation.json").write_text(text, encoding="utf-8")
    (ref / "fragments" / args.fragment / "vegetation.json").write_text(text, encoding="utf-8")
    relief = ref / "fragments" / args.fragment / "preview_relief.png"
    if relief.exists():
        from PIL import ImageDraw
        img = Image.open(relief).convert("RGB")
        draw = ImageDraw.Draw(img)
        sx, sy = img.size[0] / (ex1 - ex0), img.size[1] / (ey1 - ey0)
        for x, y in dropped_xy:
            draw.point(((x - ex0) * sx, (y - ey0) * sy), fill=(255, 0, 0))
        for p in kept["Hard"] + kept["Soft"]:
            draw.point(((p[0] - ex0) * sx, (p[1] - ey0) * sy), fill=(0, 255, 0))
        img.save(ref / "fragments" / args.fragment / "preview_vegetation.png")
    print(f"{args.fragment}: Hard {report['totals']['Hard']}, Soft {report['totals']['Soft']}, "
          f"dropped on changed terrain {report['dropped_changed_terrain']}, {len(blocks)} blocks -> {out_dir}")


if __name__ == "__main__":
    main()
