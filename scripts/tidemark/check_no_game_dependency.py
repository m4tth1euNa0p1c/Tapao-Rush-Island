#!/usr/bin/env python3
"""Tidemark CI check: the plugin must never depend on the game module.

Fails (exit 1) with ``file:line: message`` if a file under
``monke_island/Plugins/Tidemark/Source`` contains ``monke_island``,
``/Game/MonkeIsland`` or ``Logmonke_island``, or if a ``Build.cs`` of the
plugin lists the ``monke_island`` module. Never runs Unreal.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re
import sys

PLUGIN_SOURCE = Path("monke_island/Plugins/Tidemark/Source")
FORBIDDEN = (
    ("Logmonke_island", "catégorie de log du jeu (utiliser LogTidemark)"),
    ("/Game/MonkeIsland", "asset du jeu référencé (le plugin n'embarque aucun asset de Tapao)"),
    ("monke_island", "référence au module du jeu (zéro dépendance, CONVENTIONS §1)"),
)
BUILD_CS_MODULE = re.compile(r'"monke_island"')
SKIP_SUFFIXES = {".gitkeep", ".png", ".jpg", ".uasset", ".umap", ".tmk"}


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def scan(root: Path) -> list[str]:
    """Returns one ``file:line: message`` string per violation (empty list = OK)."""
    source = root / PLUGIN_SOURCE
    if not source.is_dir():
        return [f"{source}: dossier Source du plugin introuvable"]
    violations: list[str] = []
    for path in sorted(p for p in source.rglob("*") if p.is_file()):
        if path.suffix.lower() in SKIP_SUFFIXES:
            continue
        try:
            text = path.read_text(encoding="utf-8-sig")
        except UnicodeDecodeError:
            continue
        rel = path.relative_to(root).as_posix()
        is_build_cs = path.name.endswith(".Build.cs")
        for number, line in enumerate(text.splitlines(), start=1):
            if is_build_cs and BUILD_CS_MODULE.search(line):
                violations.append(f"{rel}:{number}: Build.cs référence le module monke_island")
                continue
            for needle, why in FORBIDDEN:
                if needle in line:
                    violations.append(f"{rel}:{number}: `{needle}` — {why}")
                    break
    return violations


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=repo_root(), help="racine du dépôt (défaut : ce dépôt)")
    args = parser.parse_args(argv)
    violations = scan(args.root.resolve())
    for v in violations:
        print(v)
    if violations:
        print(f"ÉCHEC check_no_game_dependency : {len(violations)} référence(s) au jeu dans {PLUGIN_SOURCE.as_posix()}")
        return 1
    print(f"OK check_no_game_dependency : aucune référence à monke_island dans {PLUGIN_SOURCE.as_posix()}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
