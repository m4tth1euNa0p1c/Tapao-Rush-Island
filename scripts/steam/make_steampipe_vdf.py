#!/usr/bin/env python3
"""Prepare SteamPipe VDF files for an already-tested Windows Shipping build.

Does NOT authenticate, upload, publish, or toggle Steam branches. T-368.
"""
from __future__ import annotations

import argparse
from pathlib import Path
import re

BLOCKED_PARTS = {
    "saved", "intermediate", "deriveddatacache", "source", ".git", "config.vdf",
}
BLOCKED_SUFFIXES = {".pdb", ".ilk", ".iobj", ".ipch", ".pem", ".key", ".env", ".log"}
BLOCKED_NAMES = {"steam_appid.txt", ".env", "credentials.json", "service-account.json"}


def numeric_id(raw: str, label: str) -> str:
    if not re.fullmatch(r"[1-9][0-9]{3,}", raw):
        raise ValueError(f"{label} must be a real, numeric Steamworks identifier")
    if raw == "480":
        raise ValueError("The public test AppID 480 is not a production AppID")
    return raw


def vdf_value(value: str) -> str:
    if any(c in value for c in ('"', "\r", "\n")):
        raise ValueError("Invalid quote or newline in SteamPipe parameter")
    # SteamPipe uses paths with / on Windows as well.
    return value


def validate_package(root: Path, executable: str) -> None:
    if not root.is_dir():
        raise ValueError(f"Package directory is absent: {root}")
    if not executable or Path(executable).name != executable or not executable.lower().endswith(".exe"):
        raise ValueError("Pass the single Windows Shipping executable filename")
    if not (root / executable).is_file():
        raise ValueError(f"Missing packaged executable: {root / executable}")
    files = [p for p in root.rglob("*") if p.is_file()]
    if not any(p.suffix.lower() in {".pak", ".utoc"} for p in files):
        raise ValueError("No Unreal cooked PAK/IoStore content found in the package")
    for p in files:
        rel = p.relative_to(root)
        pieces = [part.lower() for part in rel.parts]
        if (any(part in BLOCKED_PARTS for part in pieces)
                or p.suffix.lower() in BLOCKED_SUFFIXES
                or p.name.lower() in BLOCKED_NAMES):
            raise ValueError(f"Remove non-redistributable or secret-looking file: {rel}")


def generate(appid: str, depotid: str, root: Path, out_dir: Path, executable: str, version: str) -> tuple[Path, Path]:
    appid, depotid = numeric_id(appid, "AppID"), numeric_id(depotid, "DepotID")
    if appid == depotid:
        raise ValueError("DepotID must be different from AppID")
    root = root.expanduser().resolve()
    out_dir = out_dir.expanduser().resolve()
    validate_package(root, executable)
    # Keep VDF files and build cache OUTSIDE the distributable package.
    if out_dir == root or root in out_dir.parents or out_dir in root.parents:
        raise ValueError("SteamPipe output and game package directories must be separate")
    out_dir.mkdir(parents=True, exist_ok=True)
    cache = out_dir / "cache"
    cache.mkdir(exist_ok=True)
    relative_depot_name = f"depot_{depotid}.vdf"
    app_file = out_dir / f"app_{appid}.vdf"
    depot_file = out_dir / relative_depot_name
    desc = vdf_value(f"TAPAO Windows Shipping {version}")
    app_vdf = (
        '"AppBuild"\n{\n'
        f'\t"AppID" "{appid}"\n'
        f'\t"Desc" "{desc}"\n'
        f'\t"BuildOutput" "{vdf_value(cache.as_posix())}"\n'
        f'\t"ContentRoot" "{vdf_value(root.as_posix())}"\n'
        '\t"Depots"\n\t{\n'
        f'\t\t"{depotid}" "{relative_depot_name}"\n'
        '\t}\n}\n'
    )
    depot_vdf = (
        '"DepotBuild"\n{\n'
        f'\t"DepotID" "{depotid}"\n'
        '\t"FileMapping"\n\t{\n'
        '\t\t"LocalPath" "*"\n'
        '\t\t"DepotPath" "."\n'
        '\t\t"Recursive" "1"\n'
        '\t}\n'
        '}\n'
    )
    app_file.write_text(app_vdf, encoding="utf-8")
    depot_file.write_text(depot_vdf, encoding="utf-8")
    return app_file, depot_file


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--appid", required=True, help="Real Steamworks app ID, never 480")
    parser.add_argument("--depotid", required=True, help="Real Steamworks Windows content depot ID")
    parser.add_argument("--content-root", type=Path, required=True, help="Tested, extracted Windows Shipping build")
    parser.add_argument("--output-dir", type=Path, required=True, help="Directory OUTSIDE the game package")
    parser.add_argument("--executable", default="monke_island.exe")
    parser.add_argument("--version", default="candidate")
    args = parser.parse_args()
    try:
        app, depot = generate(args.appid, args.depotid, args.content_root, args.output_dir, args.executable, args.version)
    except ValueError as exc:
        parser.error(str(exc))
    print(f"Prepared {app} and {depot}. This did NOT upload or publish anything.")


if __name__ == "__main__":
    main()
