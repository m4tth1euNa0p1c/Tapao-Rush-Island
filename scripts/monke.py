"""Read-only project diagnostics. Python 3.11+, standard library only.

Commands: status, task T-NNN, doctor, report [path]. Each accepts --json.
Exit codes: 0 usable/success, 1 failed checks, 2 invalid input or unavailable data.
Builds, imports and board mutations remain in the existing project tools.
"""
from __future__ import annotations

import argparse
from collections import Counter
from datetime import datetime, timezone
import json
import os
from pathlib import Path
import re
import shutil
import socket
import subprocess
import sys
import time
import tomllib

ROOT = Path(__file__).resolve().parents[1]
PROJECT = "monke_island/monke_island.uproject"
BOARD = "docs/production/board.md"
REPORT = "monke_island/Saved/Automation/Reports/index.json"
FIELDS = ("id", "title", "agent", "status", "feature", "updated", "note")
STATUSES = {"a-faire", "en-cours", "review", "bloque", "fait"}


def read_board(root: Path) -> list[dict]:
    """Read the chantier format without importing its mutation functions."""
    rows = []
    seen = set()
    for line in (root / BOARD).read_text(encoding="utf-8-sig").splitlines():
        if not re.match(r"^\|\s*T-\d+", line):
            continue
        cells = [cell.strip() for cell in line.strip().strip("|").split("|")]
        if len(cells) != len(FIELDS):
            raise ValueError(f"Ligne chantier invalide : {cells[0]}")
        row = dict(zip(FIELDS, cells))
        if not re.fullmatch(r"T-\d{3,}", row["id"]) or row["id"] in seen:
            raise ValueError(f"ID chantier invalide ou dupliqué : {row['id']}")
        if row["status"] not in STATUSES:
            raise ValueError(f"Statut inconnu : {row['id']} {row['status']}")
        seen.add(row["id"])
        rows.append(row)
    if not rows:
        raise ValueError("Le chantier ne contient aucune tâche lisible")
    return rows


def git(root: Path, *args: str) -> str:
    result = subprocess.run(
        ["git", "--no-optional-locks", *args], cwd=root,
        capture_output=True, encoding="utf-8", errors="replace", timeout=30,
    )
    if result.returncode:
        raise ValueError(result.stderr.strip() or "Git indisponible")
    return result.stdout


def git_changes(root: Path) -> list[dict]:
    # -z preserves Unicode, spaces, tabs and newlines in filenames; renames have two paths.
    parts = git(root, "status", "--porcelain=v1", "-z", "--untracked-files=normal").split("\0")
    changes = []
    index = 0
    while index < len(parts) and parts[index]:
        record = parts[index]
        change = {"status": record[:2], "path": record[3:]}
        index += 1
        if "R" in record[:2] or "C" in record[:2]:
            change["previous_path"] = parts[index]
            index += 1
        changes.append(change)
    return changes


def status(root: Path) -> dict:
    rows = read_board(root)
    changes = git_changes(root)
    source = root / "monke_island/Source/monke_island"
    inventory = Counter(p.suffix for p in source.rglob("*") if p.suffix in {".h", ".cpp"})
    worktrees = [line[9:] for line in git(root, "worktree", "list", "--porcelain").splitlines()
                 if line.startswith("worktree ")]
    return {
        "root": str(root), "head": git(root, "rev-parse", "HEAD").strip(),
        "branch": git(root, "branch", "--show-current").strip() or "DETACHED",
        "tasks": dict(Counter(row["status"] for row in rows)), "total_tasks": len(rows),
        "active": [row for row in rows if row["status"] in {"en-cours", "bloque"}],
        "changes": {"entries": len(changes), "by_status": dict(Counter(c["status"] for c in changes)),
                    "by_area": dict(Counter(c["path"].split("/")[0] for c in changes))},
        "worktrees": worktrees, "source_files": dict(inventory),
        "claude_agents": len(list((root / ".claude/agents").glob("*.md"))),
        "claude_skills": len(list((root / ".claude/skills").glob("*/SKILL.md"))),
        "codex_agents": [p.stem for p in sorted((root / ".codex/agents").glob("*.toml"))],
        "notice": "Statuts déclarés dans le chantier ; aucun test ni build lancé par cette commande.",
    }


def task(root: Path, task_id: str) -> dict:
    rows = read_board(root)
    row = next((row for row in rows if row["id"] == task_id.upper()), None)
    if row is None:
        raise ValueError(f"Tâche inconnue : {task_id}")
    references = sorted(set(re.findall(r"\bT-\d{3,}\b", row["title"] + " " + row["note"])) - {row["id"]})
    related = [r for r in rows if row["feature"] and r["feature"] == row["feature"] and r != row]
    return {"task": row, "referenced_tasks": references,
            "related": [{k: r[k] for k in ("id", "title", "status")} for r in related],
            "instructions": ["AGENTS.md", "CLAUDE.md", "docs/CAHIER_DES_CHARGES.md", ".claude/factory/FACTORY.md"],
            "notice": "Les références T-NNN ne constituent pas un graphe complet de dépendances. Aucune affectation modifiée."}


def doctor(root: Path, engine: Path) -> dict:
    checks = []

    def add(name, state, detail):
        checks.append({"name": name, "state": state, "detail": str(detail)})

    for name in ("git", "python", "powershell", "codex"):
        location = shutil.which(name)
        add(name, "ok" if location else "warning", location or "absent du PATH")
    for relative in (PROJECT, BOARD, "AGENTS.md", "scripts/ue.ps1", "scripts/chantier.py", "scripts/worktree.py"):
        add(relative, "ok" if (root / relative).is_file() else "error", root / relative)
    project = json.loads((root / PROJECT).read_text(encoding="utf-8-sig"))
    add("engine_association", "info", project.get("EngineAssociation", "absente"))
    for relative in ("Engine/Build/BatchFiles/Build.bat", "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"):
        add(relative, "ok" if (engine / relative).is_file() else "warning", engine / relative)
    # Connectivity only: no MCP tool, initialization, editor launch or state mutation.
    try:
        with socket.create_connection(("127.0.0.1", 8000), timeout=1):
            add("unreal_port", "info", "127.0.0.1:8000 ouvert ; protocole MCP non vérifié")
    except OSError:
        add("unreal_port", "warning", "127.0.0.1:8000 fermé ; normal si éditeur arrêté")
    agent_files = sorted((root / ".codex/agents").glob("*.toml"))
    seen = set()
    if not agent_files:
        add("codex_agents", "error", "aucun profil .codex/agents/*.toml")
    for path in agent_files:
        try:
            agent = tomllib.loads(path.read_text(encoding="utf-8-sig"))
            for key in ("name", "description", "developer_instructions"):
                if not isinstance(agent.get(key), str) or not agent[key].strip():
                    raise ValueError(f"{key} absent ou vide")
            if agent["name"] in seen:
                raise ValueError(f"nom dupliqué : {agent['name']}")
            seen.add(agent["name"])
            add(path.relative_to(root).as_posix(), "ok", agent["description"])
        except (ValueError, OSError) as exc:
            add(path.name, "error", exc)
    config = root / ".codex/config.toml"
    if config.exists():
        try:
            tomllib.loads(config.read_text(encoding="utf-8-sig"))
            add("codex_config", "ok", "TOML valide ; chargement effectif dépend de la session Codex")
        except ValueError as exc:
            add("codex_config", "error", exc)
    return {"checks": checks, "ok": not any(c["state"] == "error" for c in checks),
            "notice": "Diagnostic seulement ; warnings à examiner avant build. Aucun processus lancé ou arrêté."}


def report(path: Path, prefix: str, min_tests: int, max_age_hours: float,
           since: datetime | None = None) -> dict:
    data = json.loads(path.read_text(encoding="utf-8-sig"))
    if not isinstance(data, dict) or not isinstance(data.get("tests"), list):
        raise ValueError("Rapport Automation invalide : tableau tests absent")
    tests = data["tests"]
    if any(not isinstance(t, dict) or not isinstance(t.get("fullTestPath"), str) for t in tests):
        raise ValueError("Rapport Automation invalide : test sans fullTestPath")
    prefix = prefix.rstrip(".")
    selected = [t for t in tests if t["fullTestPath"] == prefix or t["fullTestPath"].startswith(prefix + ".")]
    problems = []
    age_hours = (time.time() - path.stat().st_mtime) / 3600
    if age_hours < -0.02 or age_hours > max_age_hours:
        problems.append(f"Date fichier hors fenêtre : âge {age_hours:.2f} h, maximum {max_age_hours} h")
    if since and path.stat().st_mtime < since.timestamp():
        problems.append("Rapport antérieur au début d'exécution fourni (--since)")
    if len(selected) < min_tests:
        problems.append(f"Seulement {len(selected)} test(s) sélectionné(s), minimum attendu {min_tests}")
    paths = [t["fullTestPath"] for t in tests]
    if len(set(paths)) != len(paths):
        problems.append("Chemins de tests dupliqués")
    failures = []
    warning_tests = []
    observed_totals = Counter()
    selected_paths = {test["fullTestPath"] for test in selected}
    states = {"Fail": "failed", "NotRun": "notRun", "InProcess": "inProcess"}
    for test in tests:
        state = test.get("state")
        errors = test.get("errors", 0)
        warnings = test.get("warnings", 0)
        entries = test.get("entries", [])
        if (not isinstance(entries, list) or type(errors) is not int or errors < 0
                or type(warnings) is not int or warnings < 0):
            raise ValueError(f"Compteurs/entrées invalides : {test['fullTestPath']}")
        for entry in entries:
            if (not isinstance(entry, dict) or not isinstance(entry.get("event"), dict)
                    or entry["event"].get("type") not in ("Info", "Warning", "Error")):
                raise ValueError(f"Événement Automation illisible : {test['fullTestPath']}")
        if state == "Success":
            observed_totals["succeededWithWarnings" if warnings else "succeeded"] += 1
        elif isinstance(state, str) and state in states:
            observed_totals[states[state]] += 1
        else:
            problems.append(f"État Automation inconnu : {test['fullTestPath']} ({state})")
        if test["fullTestPath"] not in selected_paths:
            continue
        entry_errors = any(e["event"]["type"] == "Error" for e in entries)
        if state != "Success" or errors or entry_errors:
            failures.append({"test": test["fullTestPath"], "state": state, "errors": errors})
        if warnings:
            warning_tests.append(test["fullTestPath"])
    if failures:
        problems.append(f"{len(failures)} test(s) échoué(s), inachevé(s), inconnu(s) ou avec erreurs")
    # UE counts plain successes separately from successes with warnings.
    keys = ("succeeded", "succeededWithWarnings", "failed", "notRun", "inProcess")
    totals = {key: data.get(key) for key in keys}
    if any(type(value) is not int or value < 0 for value in totals.values()):
        problems.append("Compteurs globaux manquants ou invalides")
    elif any(totals[key] != observed_totals[key] for key in keys):
        problems.append("Compteurs globaux incohérents avec les états individuels des tests")
    return {"path": str(path), "report_created_on": data.get("reportCreatedOn"),
            "file_modified_utc": datetime.fromtimestamp(path.stat().st_mtime, timezone.utc).isoformat(),
            "filter": prefix, "selected": len(selected), "total": len(tests), "totals": totals,
            "warnings": warning_tests, "failures": failures, "problems": problems, "ok": not problems,
            "notice": "Rapport existant seulement. Ni relance des tests, ni preuve du commit courant, du PIE, du réseau ou du package."}


def positive_int(value):
    number = int(value)
    if number < 1:
        raise argparse.ArgumentTypeError("doit être >= 1")
    return number


def positive_hours(value):
    number = float(value)
    if not 0 < number < float("inf"):
        raise argparse.ArgumentTypeError("doit être fini et > 0")
    return number


def aware_date(value):
    try:
        date = datetime.fromisoformat(value.replace("Z", "+00:00"))
    except ValueError as exc:
        raise argparse.ArgumentTypeError("date ISO 8601 attendue") from exc
    if date.tzinfo is None:
        raise argparse.ArgumentTypeError("fuseau requis, par exemple 2026-10-04T03:00:00+02:00")
    return date


def print_summary(command: str, result: dict):
    if "error" in result:
        print(f"ERREUR : {result['error']}")
        return
    if command == "status":
        print(f"CHANTIER : {result['total_tasks']} tâches | {result['branch']} @ {result['head'][:8]}")
        print("Statuts : " + " | ".join(f"{v} {k}" for k, v in sorted(result["tasks"].items())))
        print(f"Git : {result['changes']['entries']} entrées modifiées/non suivies ; {len(result['worktrees'])} worktrees")
        print(f"Code : {result['source_files']} ; agents Codex : {len(result['codex_agents'])}")
        for row in result["active"]:
            print(f"  {row['id']} [{row['status']}] {row['title']}")
    elif command == "task":
        for key, value in result["task"].items():
            print(f"{key}: {value}")
        print("Tâches citées : " + ", ".join(result["referenced_tasks"]))
        print("Même feature : " + ", ".join(f"{r['id']} ({r['status']})" for r in result["related"]))
    elif command == "doctor":
        for check in result["checks"]:
            print(f"[{check['state'].upper()}] {check['name']}: {check['detail']}")
    elif command == "report":
        print(f"{'OK' if result['ok'] else 'ÉCHEC'} : {result['selected']}/{result['total']} tests sélectionnés ({result['filter']})")
        print(f"Rapport : {result['path']} | fichier modifié UTC : {result['file_modified_utc']}")
        for problem in result["problems"]:
            print(f"  ERREUR : {problem}")
        for failure in result["failures"]:
            print(f"  {failure['state']}: {failure['test']} (errors={failure['errors']})")
        for warning in result["warnings"]:
            print(f"  AVERTISSEMENT : {warning}")
    print(result["notice"])


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=ROOT, help="racine du dépôt ou du worktree")
    sub = parser.add_subparsers(dest="command", required=True)
    for command in ("status", "task", "doctor", "report"):
        part = sub.add_parser(command)
        part.add_argument("--json", action="store_true", help="sortie structurée")
        if command == "task":
            part.add_argument("id")
        if command == "doctor":
            part.add_argument("--engine", type=Path, default=Path(os.environ.get("UE_ROOT", "C:/Program Files/Epic Games/UE_5.8")))
        if command == "report":
            part.add_argument("path", type=Path, nargs="?", help="index.json ; défaut Saved/Automation/Reports")
            part.add_argument("--filter", default="monke_island", help="préfixe exact de suite")
            part.add_argument("--min-tests", type=positive_int, default=1, help="minimum attendu explicitement")
            part.add_argument("--max-age-hours", type=positive_hours, default=24)
            part.add_argument("--since", type=aware_date, help="début du run, ISO 8601 avec fuseau")
    args = parser.parse_args(argv)
    try:
        root = args.root.resolve()
        if args.command == "status":
            result = status(root)
        elif args.command == "task":
            result = task(root, args.id)
        elif args.command == "doctor":
            result = doctor(root, args.engine)
        else:
            path = args.path or Path(REPORT)
            if not path.is_absolute():
                path = root / path
            result = report(path, args.filter, args.min_tests, args.max_age_hours, args.since)
        code = 0 if result.get("ok", True) else 1
    except (OSError, ValueError, subprocess.SubprocessError) as exc:
        result, code = {"ok": False, "error": str(exc)}, 2
    if args.json:
        print(json.dumps(result, ensure_ascii=False, indent=2))
    else:
        print_summary(args.command, result)
    return code


if __name__ == "__main__":
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    sys.exit(main())
