#!/usr/bin/env python3
"""Tidemark CI check: complete credits, provenance and no forbidden data.

Under ``monke_island/Plugins/Tidemark/``:
- every ``manifest.json`` (unfolded ``.tmk`` header) needs ``credits[]`` with
  non-empty ``producer``, ``license``, ``url``, ``date`` on each entry;
- every ``PROVENANCE.json`` needs ``assets[]`` with non-empty ``path``, ``tool``,
  ``date``, ``license`` and a 64-hex ``sha256`` on each entry;
- ``Content/`` and ``Docs/`` (except ``Docs/LICENSES``) must not contain the
  strings Maxar, DigitalGlobe, cocoteraie, 3D Tiles (case-insensitive).
Exit 1 with ``file[:line]: message`` on any violation.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path
import re
import sys

PLUGIN = Path("monke_island/Plugins/Tidemark")
CREDIT_FIELDS = ("producer", "license", "url", "date")
ASSET_FIELDS = ("path", "tool", "date", "license", "sha256")
SHA256 = re.compile(r"^[0-9a-f]{64}$")
FORBIDDEN_STRINGS = ("Maxar", "DigitalGlobe", "cocoteraie", "3D Tiles")
FORBIDDEN = re.compile("|".join(re.escape(s) for s in FORBIDDEN_STRINGS).encode(), re.IGNORECASE)
SCAN_DIRS = ("Content", "Docs")
SCAN_EXCLUDE = Path("Docs/LICENSES")


def repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def load_json(path: Path, rel: str, out: list[str]) -> dict | None:
    try:
        data = json.loads(path.read_text(encoding="utf-8-sig"))
    except (OSError, ValueError) as error:
        out.append(f"{rel}: JSON illisible ({error})")
        return None
    if not isinstance(data, dict):
        out.append(f"{rel}: objet JSON attendu à la racine")
        return None
    return data


def check_entries(data: dict, key: str, fields: tuple[str, ...], rel: str, out: list[str]) -> None:
    entries = data.get(key)
    if not isinstance(entries, list):
        out.append(f"{rel}: `{key}[]` absent ou non liste")
        return
    for index, entry in enumerate(entries):
        if not isinstance(entry, dict):
            out.append(f"{rel}: `{key}[{index}]` n'est pas un objet")
            continue
        for field in fields:
            value = entry.get(field)
            if not isinstance(value, str) or not value.strip():
                out.append(f"{rel}: `{key}[{index}].{field}` manquant ou vide")
            elif field == "sha256" and not SHA256.match(value.strip().lower()):
                out.append(f"{rel}: `{key}[{index}].sha256` n'est pas un SHA-256 hexadécimal de 64 caractères")


def check_manifest(path: Path, root: Path, out: list[str]) -> None:
    rel = path.relative_to(root).as_posix()
    data = load_json(path, rel, out)
    if data is None:
        return
    if not data.get("credits"):
        out.append(f"{rel}: `credits[]` absent ou vide (producteur, licence, URL, date obligatoires, CONVENTIONS §7)")
        return
    check_entries(data, "credits", CREDIT_FIELDS, rel, out)


def check_provenance(path: Path, root: Path, out: list[str]) -> None:
    rel = path.relative_to(root).as_posix()
    data = load_json(path, rel, out)
    if data is None:
        return
    check_entries(data, "assets", ASSET_FIELDS, rel, out)


def check_forbidden(plugin: Path, root: Path, out: list[str]) -> None:
    exclude = plugin / SCAN_EXCLUDE
    for name in SCAN_DIRS:
        folder = plugin / name
        if not folder.is_dir():
            continue
        for path in sorted(p for p in folder.rglob("*") if p.is_file()):
            if path.is_relative_to(exclude):
                continue
            rel = path.relative_to(root).as_posix()
            for number, line in enumerate(path.read_bytes().split(b"\n"), start=1):
                match = FORBIDDEN.search(line)
                if match:
                    out.append(f"{rel}:{number}: chaîne interdite `{match.group(0).decode(errors='replace')}` (CONVENTIONS §7)")
                    break


def scan(root: Path) -> tuple[list[str], dict[str, int]]:
    plugin = root / PLUGIN
    violations: list[str] = []
    counts = {"manifests": 0, "provenance": 0}
    if not plugin.is_dir():
        return [f"{plugin}: dossier du plugin introuvable"], counts
    for path in sorted(plugin.rglob("manifest.json")):
        counts["manifests"] += 1
        check_manifest(path, root, violations)
    for path in sorted(plugin.rglob("PROVENANCE.json")):
        counts["provenance"] += 1
        check_provenance(path, root, violations)
    if counts["provenance"] == 0:
        violations.append(f"{PLUGIN.as_posix()}/PROVENANCE.json: fichier absent (CONVENTIONS §7)")
    check_forbidden(plugin, root, violations)
    return violations, counts


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--root", type=Path, default=repo_root(), help="racine du dépôt (défaut : ce dépôt)")
    args = parser.parse_args(argv)
    violations, counts = scan(args.root.resolve())
    for v in violations:
        print(v)
    summary = f"{counts['manifests']} manifest(s), {counts['provenance']} PROVENANCE.json"
    if violations:
        print(f"ÉCHEC check_credits : {len(violations)} problème(s) — {summary}")
        return 1
    print(f"OK check_credits : crédits et provenance complets, aucune chaîne interdite — {summary}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
