"""Tableau de chantier de l'equipe monke_island : statut des taches et de chaque agent.

  python scripts/chantier.py                       tableau de bord (--env : etat editeur/MCP)
  python scripts/chantier.py agent <nom>           taches d'un agent
  python scripts/chantier.py add "<titre>" --agent <nom> [--feature <slug>] [--status a-faire] [--note "..."]
  python scripts/chantier.py set <ID> <statut> [--agent <nom>] [--note "..."]
  python scripts/chantier.py log [-n 20]           dernieres entrees du journal

Statuts : a-faire, en-cours, review, bloque, fait
Fichiers : docs/production/board.md (tableau, gere par ce script) et docs/production/journal.md (append-only)
"""
import argparse
import re
import subprocess
import sys
import urllib.request
from datetime import datetime
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BOARD = ROOT / "docs" / "production" / "board.md"
JOURNAL = ROOT / "docs" / "production" / "journal.md"
AGENTS_DIR = ROOT / ".claude" / "agents"
FEATURES_DIR = ROOT / "docs" / "features"
BUGS_DIR = ROOT / "docs" / "bugs"
MCP_URL = "http://127.0.0.1:8000/mcp"

STATUSES = ["en-cours", "bloque", "review", "a-faire", "fait"]
LABELS = {"en-cours": "EN COURS", "bloque": "BLOQUE", "review": "REVIEW", "a-faire": "A FAIRE", "fait": "FAIT"}
PSEUDO_AGENTS = {"lead", "user"}  # session principale / utilisateur
FIELDS = ["id", "title", "agent", "status", "feature", "updated", "note"]
BOARD_HEADER = """# Tableau de chantier

Géré par `scripts/chantier.py` (skill `/chantier`) — ne pas modifier les colonnes à la main.
Statuts : `a-faire` · `en-cours` · `review` · `bloque` · `fait`

| ID | Tâche | Agent | Statut | Feature | Màj | Note |
|----|-------|-------|--------|---------|-----|------|
"""
JOURNAL_HEADER = "# Journal de chantier\n\nUne ligne par changement de statut (append-only).\n\n"


def now():
    return datetime.now().strftime("%Y-%m-%d %H:%M")


def clean(text):
    return (text or "").replace("|", "/").replace("\n", " ").strip()


def load_rows():
    if not BOARD.exists():
        return []
    rows = []
    for line in BOARD.read_text(encoding="utf-8").splitlines():
        if not re.match(r"^\|\s*T-\d+", line):
            continue
        cells = [c.strip() for c in line.strip().strip("|").split("|")]
        cells += [""] * (len(FIELDS) - len(cells))
        rows.append(dict(zip(FIELDS, cells[: len(FIELDS)])))
    return rows


def save_rows(rows):
    BOARD.parent.mkdir(parents=True, exist_ok=True)
    lines = [f"| {' | '.join(clean(r[f]) for f in FIELDS)} |" for r in rows]
    BOARD.write_text(BOARD_HEADER + "\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")


def journal(task_id, agent, status, note):
    if not JOURNAL.exists():
        JOURNAL.parent.mkdir(parents=True, exist_ok=True)
        JOURNAL.write_text(JOURNAL_HEADER, encoding="utf-8")
    with JOURNAL.open("a", encoding="utf-8") as f:
        f.write(f"- {now()} · {task_id} · {agent} · {status} · {clean(note)}\n")


def known_agents():
    return sorted(p.stem for p in AGENTS_DIR.glob("*.md"))


def check_agent(agent):
    if agent and agent not in known_agents() and agent not in PSEUDO_AGENTS:
        print(f"attention : agent inconnu '{agent}' (agents : {', '.join(known_agents())})", file=sys.stderr)


def next_id(rows):
    numbers = [int(r["id"].split("-")[1]) for r in rows if re.match(r"^T-\d+$", r["id"])]
    return f"T-{(max(numbers) + 1 if numbers else 1):03d}"


def short(text, width):
    return text if len(text) <= width else text[: width - 1] + "…"


def doc_status(path):
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if "**Statut**" in line:
            return line.split(":", 1)[-1].strip()
    return "?"


def cmd_add(args):
    rows = load_rows()
    check_agent(args.agent)
    task_id = next_id(rows)
    rows.append({"id": task_id, "title": args.title, "agent": args.agent, "status": args.status,
                 "feature": args.feature or "", "updated": now(), "note": args.note or ""})
    save_rows(rows)
    journal(task_id, args.agent, args.status, f"créée : {args.title}" + (f" — {args.note}" if args.note else ""))
    print(f"{task_id} ajoutée ({args.agent}, {args.status}) : {args.title}")


def cmd_set(args):
    rows = load_rows()
    row = next((r for r in rows if r["id"].upper() == args.id.upper()), None)
    if not row:
        sys.exit(f"tâche introuvable : {args.id}")
    if args.agent:
        check_agent(args.agent)
        row["agent"] = args.agent
    row["status"] = args.status
    row["updated"] = now()
    if args.note is not None:
        row["note"] = args.note
    save_rows(rows)
    journal(row["id"], row["agent"], args.status, args.note or "")
    print(f"{row['id']} -> {args.status} ({row['agent']}) : {row['title']}")


def print_task(r, indent="    "):
    note = f" — {short(r['note'], 60)}" if r["note"] else ""
    print(f"{indent}[{LABELS.get(r['status'], r['status'])}] {r['id']} {short(r['title'], 58)}  ({r['updated'][5:]}){note}")


def cmd_agent(args):
    rows = [r for r in load_rows() if r["agent"] == args.name]
    if not rows:
        print(f"{args.name} : aucune tâche")
        return
    print(args.name)
    for status in STATUSES:
        for r in rows:
            if r["status"] == status:
                print_task(r)


def cmd_log(args):
    if not JOURNAL.exists():
        print("(journal vide)")
        return
    entries = [l for l in JOURNAL.read_text(encoding="utf-8").splitlines() if l.startswith("- ")]
    for line in entries[-args.n:]:
        print(line[2:])


def env_status():
    try:
        out = subprocess.run(["tasklist", "/FI", "IMAGENAME eq UnrealEditor.exe"], capture_output=True, text=True).stdout
        editor = "UnrealEditor.exe" in out
    except OSError:
        editor = None
    body = (b'{"jsonrpc":"2.0","id":1,"method":"initialize","params":{"protocolVersion":"2025-06-18",'
            b'"capabilities":{},"clientInfo":{"name":"chantier","version":"1"}}}')
    try:
        req = urllib.request.Request(MCP_URL, data=body, headers={
            "Content-Type": "application/json", "Accept": "application/json, text/event-stream"})
        with urllib.request.urlopen(req, timeout=3):
            mcp = True
    except Exception:  # noqa: BLE001 - toute erreur = injoignable
        mcp = False
    return editor, mcp


def cmd_dashboard(args):
    rows = load_rows()
    counts = {s: sum(r["status"] == s for r in rows) for s in STATUSES}
    print(f"CHANTIER monke_island — {now()}")
    print("Tâches : " + " · ".join(f"{counts[s]} {LABELS[s].lower()}" for s in STATUSES))

    if args.env:
        editor, mcp = env_status()
        print(f"Éditeur : {'ouvert' if editor else 'fermé'} · MCP unreal : {'OK' if mcp else 'injoignable'}")

    active = [r for r in rows if r["status"] != "fait"]
    print("\nPAR AGENT")
    busy = sorted({r["agent"] for r in active})
    if not busy:
        print("    (aucune tâche ouverte)")
    for agent in busy:
        print(f"  {agent}")
        for status in STATUSES[:-1]:
            for r in active:
                if r["agent"] == agent and r["status"] == status:
                    print_task(r)
    idle = [a for a in known_agents() if a not in busy]
    if idle:
        print(f"\nDISPONIBLES : {', '.join(idle)}")

    blocked = [r for r in rows if r["status"] == "bloque"]
    if blocked:
        print("\nBLOCAGES")
        for r in blocked:
            print(f"  {r['id']} ({r['agent']}) {r['title']} — {r['note'] or 'aucune note : préciser ce qui bloque'}")

    features = sorted(FEATURES_DIR.glob("*.md")) if FEATURES_DIR.exists() else []
    if features:
        print("\nFEATURES")
        for path in features:
            print(f"  {path.stem:32s} {doc_status(path)}")

    bugs = [p for p in sorted(BUGS_DIR.glob("*.md"))] if BUGS_DIR.exists() else []
    open_bugs = [p for p in bugs if not re.search(r"fixed|verified", doc_status(p), re.I)]
    print(f"\nBUGS OUVERTS : {len(open_bugs)}" + ("".join(f"\n  {p.stem} — {doc_status(p)}" for p in open_bugs)))

    done = [r for r in rows if r["status"] == "fait"]
    if done:
        print(f"\nDERNIERS TERMINÉS")
        for r in sorted(done, key=lambda r: r["updated"])[-5:]:
            print_task(r, indent="  ")

    if JOURNAL.exists():
        entries = [l for l in JOURNAL.read_text(encoding="utf-8").splitlines() if l.startswith("- ")]
        if entries:
            print("\nJOURNAL (6 derniers)")
            for line in entries[-6:]:
                print(f"  {line[2:]}")


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
        sys.stderr.reconfigure(encoding="utf-8")

    parser = argparse.ArgumentParser(description="Tableau de chantier monke_island")
    parser.add_argument("--env", action="store_true", help="affiche l'état éditeur / MCP")
    sub = parser.add_subparsers(dest="command")

    p_add = sub.add_parser("add", help="ajoute une tâche")
    p_add.add_argument("title")
    p_add.add_argument("--agent", required=True)
    p_add.add_argument("--feature")
    p_add.add_argument("--status", choices=STATUSES, default="a-faire")
    p_add.add_argument("--note")
    p_add.set_defaults(func=cmd_add)

    p_set = sub.add_parser("set", help="change le statut d'une tâche")
    p_set.add_argument("id")
    p_set.add_argument("status", choices=STATUSES)
    p_set.add_argument("--agent")
    p_set.add_argument("--note")
    p_set.set_defaults(func=cmd_set)

    p_agent = sub.add_parser("agent", help="tâches d'un agent")
    p_agent.add_argument("name")
    p_agent.set_defaults(func=cmd_agent)

    p_log = sub.add_parser("log", help="journal")
    p_log.add_argument("-n", type=int, default=20)
    p_log.set_defaults(func=cmd_log)

    args = parser.parse_args()
    if args.command is None:
        cmd_dashboard(args)
    else:
        args.func(args)


if __name__ == "__main__":
    main()
