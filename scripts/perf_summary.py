"""Resume un CSV du CSV profiler Unreal (temps de frame, threads, GPU).

Usage: python scripts/perf_summary.py <fichier.csv> [--skip 120]
Les premieres frames (chargement) sont ignorees (--skip).
"""
import argparse
import csv
import statistics
import sys

STATS = ["FrameTime", "GameThreadTime", "RenderThreadTime", "RHIThreadTime", "GPUTime"]


def percentile(values, p):
    ordered = sorted(values)
    return ordered[min(len(ordered) - 1, int(round(p / 100 * (len(ordered) - 1))))]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("csv_path")
    parser.add_argument("--skip", type=int, default=120)
    args = parser.parse_args()

    with open(args.csv_path, newline="", encoding="utf-8", errors="replace") as f:
        rows = list(csv.reader(f))
    if not rows:
        sys.exit("CSV vide")

    header = rows[0]
    columns = {}
    for stat in STATS:
        for i, name in enumerate(header):
            if name.strip().lower() == stat.lower():
                columns[stat] = i
                break

    print(f"{'stat':18s} {'moy':>7s} {'p50':>7s} {'p95':>7s} {'p99':>7s} {'max':>7s}  (ms)")
    for stat, col in columns.items():
        values = []
        for row in rows[1 + args.skip:]:
            try:
                values.append(float(row[col]))
            except (ValueError, IndexError):
                continue  # lignes de metadonnees en fin de fichier
        if not values:
            continue
        print(f"{stat:18s} {statistics.fmean(values):7.2f} {percentile(values, 50):7.2f} "
              f"{percentile(values, 95):7.2f} {percentile(values, 99):7.2f} {max(values):7.2f}")
        if stat == "FrameTime":
            print(f"{'':18s} -> {1000 / statistics.fmean(values):.1f} fps moyen sur {len(values)} frames")

    missing = [s for s in STATS if s not in columns]
    if missing:
        print(f"(colonnes absentes : {', '.join(missing)})")


if __name__ == "__main__":
    main()
