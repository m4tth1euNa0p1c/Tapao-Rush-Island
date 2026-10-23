#!/usr/bin/env python3
"""Tidemark CI check: no unseeded randomness or wall-clock time in the simulation.

Fails (exit 1) with ``file:line: message`` on ``FMath::Rand``, ``FMath::FRand``,
``RandRange``, ``FRandRange``, ``VRand``, ``RandBool``, ``FRandomStream(`` without
``FTidemarkSeed`` / ``ToRandomStream`` on the same line, ``FDateTime::Now``,
``FDateTime::UtcNow``, ``FPlatformTime::Cycles`` and ``GetTimeSeconds()`` in
``monke_island/Plugins/Tidemark/Source`` outside ``Tests/`` folders.
A line may be exempted explicitly with ``// tidemark-allow-random: <raison>``.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

PLUGIN_SOURCE = Path("monke_island/Plugins/Tidemark/Source")
CODE_SUFFIXES = {".h", ".cpp", ".inl", ".hpp", ".c"}
ALLOW = re.compile(r"//\s*tidemark-allow-random:\s*\S")
SEEDED = re.compile(r"FTidemarkSeed|ToRandomStream")
RULES: tuple[tuple[re.Pattern[str], str], ...] = (
    (re.compile(r"\bFMath::F?Rand\b"), "hasard global non seedé (utiliser FTidemarkSeed)"),
    (re.compile(r"\bF?RandRange\b"), "hasard global non seedé (utiliser FTidemarkSeed)"),
    (re.compile(r"\bVRand\b"), "hasard global non seedé (utiliser FTidemarkSeed)"),
    (re.compile(r"\bRandBool\b"), "hasard global non seedé (utiliser FTidemarkSeed)"),
    (re.compile(r"\bFDateTime::(Utc)?Now\b"), "horloge murale interdite dans la simulation (CONVENTIONS §4)"),
    (re.compile(r"\bFPlatformTime::Cycles"), "compteur de cycles interdit dans la simulation (CONVENTIONS §4)"),
    (re.compile(r"\bGetTimeSeconds\(\)"), "temps monde interdit : la simulation avance par StepIndex"),
)
RANDOM_STREAM = re.compile(r"\bFRandomStream\s*\(")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def in_tests(path: Path, root: Path) -> bool:
    return "Tests" in path.relative_to(root).parts


def check_line(line: str) -> str | None:
    """Returns the violation message for one source line, or None."""
    if ALLOW.search(line):
        return None
    for pattern, why in RULES:
        match = pattern.search(line)
        if match:
            return f"`{match.group(0)}` — {why}"
    if RANDOM_STREAM.search(line) and not SEEDED.search(line):
        return "`FRandomStream(` sans FTidemarkSeed/ToRandomStream — flux non dérivé de la seed"
    return None


def scan(root: Path) -> list[str]:
    source = root / PLUGIN_SOURCE
    if not source.is_dir():
        return [f"{source}: dossier Source du plugin introuvable"]
    violations: list[str] = []
    for path in sorted(p for p in source.rglob("*") if p.is_file() and p.suffix.lower() in CODE_SUFFIXES):
        if in_tests(path, source):
            continue
        try:
            text = path.read_text(encoding="utf-8-sig")
        except UnicodeDecodeError:
            continue
        rel = path.relative_to(root).as_posix()
        for number, line in enumerate(text.splitlines(), start=1):
            message = check_line(line)
            if message:
                violations.append(f"{rel}:{number}: {message}")
    return violations


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=repo_root(), help="racine du dépôt (défaut : ce dépôt)")
    args = parser.parse_args(argv)
    violations = scan(args.root.resolve())
    for v in violations:
        print(v)
    if violations:
        print(f"ÉCHEC check_no_unseeded_random : {len(violations)} source(s) de non-déterminisme hors Tests/")
        return 1
    print(f"OK check_no_unseeded_random : aucun hasard non seedé ni horloge murale dans {PLUGIN_SOURCE.as_posix()} (hors Tests/)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
