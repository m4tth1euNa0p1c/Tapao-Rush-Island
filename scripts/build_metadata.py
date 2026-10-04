"""Versionne un build local et archive ses symboles sans modifier les packages.

stamp --version 0.1.0 : ProjectVersion + empreinte des sources/configurations.
archive --package build/package : verifie les exe livres et copie leurs PDB
dans monke_island/Saved/Symbols/<version>, hors du package, sans ecrasement.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess

SECTION = "/Script/EngineSettings.GeneralProjectSettings"


def set_ini_value(text, section, key, value):
    """Conserve les lignes Unreal (+tableaux, commentaires), refuse les ambiguïtés."""
    newline = "\r\n" if "\r\n" in text else "\n"
    lines = text.splitlines()
    headers = [i for i, line in enumerate(lines) if line.strip() == f"[{section}]"]
    if len(headers) > 1:
        raise ValueError("Section de version dupliquee")
    if not headers:
        return text.rstrip() + newline * 2 + f"[{section}]" + newline + f"{key}={value}" + newline
    start = headers[0] + 1
    stop = next((i for i in range(start, len(lines)) if lines[i].lstrip().startswith("[")), len(lines))
    matches = [i for i in range(start, stop) if re.match(rf"\s*{re.escape(key)}\s*=", lines[i])]
    if len(matches) > 1:
        raise ValueError("Cle de version dupliquee")
    if matches:
        lines[matches[0]] = f"{key}={value}"
    else:
        lines.insert(start, f"{key}={value}")
    return newline.join(lines) + newline


def sha256(path):
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def source_digest(root):
    """Empreinte de code/config, pas une empreinte du contenu binaire Unreal."""
    project = root / "monke_island"
    paths = [project / "monke_island.uproject"]
    for folder in ("Source", "Config"):
        paths.extend(p for p in (project / folder).rglob("*") if p.is_file())
    digest = hashlib.sha256()
    for path in sorted(paths):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(bytes.fromhex(sha256(path)))
    return digest.hexdigest()


def git(root, *args):
    return subprocess.check_output(["git", *args], cwd=root, text=True, encoding="utf-8", errors="replace").strip()


def stamp(root, base_version):
    if not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+(?:-[a-zA-Z0-9.-]+)?", base_version):
        raise ValueError("Version attendue : X.Y.Z ou X.Y.Z-dev")
    now = datetime.now(timezone.utc)
    head = git(root, "rev-parse", "HEAD")
    dirty = bool(git(root, "status", "--porcelain", "--untracked-files=all"))
    version = f"{base_version}+{now.strftime('%Y%m%dT%H%M%S%fZ')}.{head[:12]}" + (".dirty" if dirty else "")
    if len(version) > 96:
        raise ValueError("Version trop longue")
    config = root / "monke_island/Config/DefaultGame.ini"
    original = config.read_bytes().decode("utf-8-sig")
    config.write_bytes(set_ini_value(original, SECTION, "ProjectVersion", version).encode("utf-8"))
    metadata = {"schema": 1, "version": version, "git_head": head, "dirty_before_stamp": dirty,
                "stamped_at": now.isoformat(), "source_config_sha256": source_digest(root),
                "scope": "Source, Config, uproject only; not Content or a clean release attestation"}
    output = root / "monke_island/Saved/BuildMetadata/build.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(metadata, indent=2) + "\n", encoding="utf-8")
    return metadata


def archive(root, package, destination=None):
    root, package = root.resolve(), package.resolve()
    metadata = json.loads((root / "monke_island/Saved/BuildMetadata/build.json").read_text(encoding="utf-8"))
    version = metadata["version"]
    if not re.fullmatch(r"[a-zA-Z0-9][a-zA-Z0-9.+_-]{0,95}", version) or ".." in version:
        raise ValueError("Identifiant de build invalide")
    if metadata["source_config_sha256"] != source_digest(root):
        raise ValueError("Sources/config modifiees depuis stamp : ne pas attribuer les PDB a cette version")
    if not package.is_dir() or not any(package.rglob("*.exe")):
        raise ValueError("Package absent ou sans executable")
    if any(package.rglob("*.pdb")):
        raise ValueError("Le package contient des PDB : utiliser -nodebuginfo, sans supprimer un package existant")
    if not any(package.rglob("CrashReportClient*.exe")):
        raise ValueError("CrashReportClient absent du package")
    target = ((destination or root / "monke_island/Saved/Symbols") / version).resolve()
    if target.is_relative_to(package) or package.is_relative_to(target):
        raise ValueError("Archive de symboles imbriquee dans le package")
    if target.exists():
        raise ValueError("Archive deja existante : aucun ecrasement")
    binary_root = root / "monke_island/Binaries/Win64"
    matches = []
    for pdb in sorted(binary_root.glob("monke_island*.pdb")):
        if pdb.name.startswith("UnrealEditor") or ".patch_" in pdb.name:
            continue
        exe = pdb.with_suffix(".exe")
        if not exe.is_file():
            continue
        digest = sha256(exe)
        shipped = [p for p in package.rglob(exe.name) if sha256(p) == digest]
        if shipped:
            matches.append((pdb, exe, shipped[0], digest))
    if not matches:
        raise ValueError("Aucun couple EXE/PDB local correspondant aux executables livres")
    # Un dossier nouveau exclusivement possede par ce run ; aucune suppression.
    target.mkdir(parents=True, exist_ok=False)
    records = []
    for pdb, exe, shipped, digest in matches:
        shutil.copy2(pdb, target / pdb.name)
        if sha256(target / pdb.name) != sha256(pdb):
            raise ValueError("Copie PDB invalide ; archive partielle a examiner")
        records.append({"pdb": pdb.name, "pdb_sha256": sha256(pdb), "exe": exe.name,
                        "exe_sha256": digest, "package_path": shipped.relative_to(package).as_posix()})
    result = {**metadata, "archived_at": datetime.now(timezone.utc).isoformat(), "symbols": records}
    (target / "manifest.json").write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return target


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    commands = parser.add_subparsers(dest="command", required=True)
    stamp_parser = commands.add_parser("stamp")
    stamp_parser.add_argument("--version", default="0.1.0")
    archive_parser = commands.add_parser("archive")
    archive_parser.add_argument("--package", type=Path, required=True)
    archive_parser.add_argument("--destination", type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "stamp":
            print(json.dumps(stamp(args.root, args.version), indent=2))
        else:
            archive(args.root, args.package, args.destination)
            print("Symboles archives et empreintes verifiees hors du package.")
    except (OSError, ValueError, KeyError, subprocess.CalledProcessError) as error:
        # Pas de chemins personnels dans le diagnostic publie par l'outil.
        parser.exit(2, f"Echec metadata/symboles ({type(error).__name__}) : " + (str(error) if isinstance(error, ValueError) else "verifier les fichiers locaux") + "\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
