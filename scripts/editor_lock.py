"""Verrou de l'éditeur Unreal partagé entre les sessions (Claude, Codex) de monke_island.

Une seule session à la fois peut fermer l'éditeur, compiler l'arbre principal, lancer la suite headless,
relancer l'éditeur ou mener une opération longue (gros import, PCG, PIE de test). Le verrou est un fichier
JSON hors git ; il expire seul (durée annoncée) pour qu'une session disparue ne bloque personne.

Usage (depuis la racine du dépôt) :
    python scripts/editor_lock.py status                       # qui tient l'éditeur ?
    python scripts/editor_lock.py acquire --owner monk-island-63 --reason "build T-103" --minutes 15
    python scripts/editor_lock.py release --owner monk-island-63
    python scripts/editor_lock.py check --owner monk-island-63  # code 0 si libre ou à moi, 1 sinon
    python scripts/editor_lock.py close-editor --owner monk-island-63   # ferme proprement l'éditeur, verrou exigé

Le propriétaire par défaut vient de la variable d'environnement MONKE_LOCK_OWNER.
scripts/ue.ps1 (build, rebuild, test, editor, clean, package) appelle `check` puis prend le verrou le temps de
la commande quand il est libre : une commande lancée pendant le verrou d'un autre est refusée.
"""
import argparse
import datetime as dt
import json
import os
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
LOCK = ROOT / ".claude" / "factory" / "editor.lock.json"
LOG = ROOT / ".claude" / "factory" / "editor.lock.log"


def now():
    return dt.datetime.now().astimezone()


def read():
    try:
        data = json.loads(LOCK.read_text(encoding="utf-8"))
    except (FileNotFoundError, json.JSONDecodeError):
        return None
    if dt.datetime.fromisoformat(data["expires"]) <= now():
        return None
    return data


def log(line):
    LOG.parent.mkdir(parents=True, exist_ok=True)
    with LOG.open("a", encoding="utf-8") as f:
        f.write(f"{now().isoformat(timespec='seconds')} {line}\n")


def describe(data):
    left = int((dt.datetime.fromisoformat(data["expires"]) - now()).total_seconds() // 60) + 1
    return f"{data['owner']} — {data['reason']} (depuis {data['acquired'][11:16]}, encore {left} min au plus)"


def owner_of(args):
    owner = args.owner or os.environ.get("MONKE_LOCK_OWNER", "")
    if not owner:
        sys.exit("propriétaire manquant : --owner <session> ou MONKE_LOCK_OWNER")
    return owner


def cmd_status(_):
    data = read()
    print(f"éditeur tenu par {describe(data)}" if data else "éditeur libre")
    return 0


def cmd_check(args):
    data = read()
    owner = args.owner or os.environ.get("MONKE_LOCK_OWNER", "")
    if not data or (owner and data["owner"] == owner):
        return 0
    print(f"REFUSÉ : éditeur tenu par {describe(data)}", file=sys.stderr)
    return 1


def cmd_acquire(args):
    owner = owner_of(args)
    data = read()
    if data and data["owner"] != owner and not args.force:
        print(f"REFUSÉ : éditeur tenu par {describe(data)}", file=sys.stderr)
        return 1
    start = now()
    data = {"owner": owner, "reason": args.reason, "acquired": start.isoformat(timespec="seconds"),
            "expires": (start + dt.timedelta(minutes=args.minutes)).isoformat(timespec="seconds"), "pid": os.getppid()}
    LOCK.parent.mkdir(parents=True, exist_ok=True)
    tmp = LOCK.with_suffix(".tmp")
    tmp.write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
    tmp.replace(LOCK)
    log(f"ACQUIRE {owner} {args.minutes} min : {args.reason}{' (FORCE)' if args.force else ''}")
    print(f"verrou pris : {describe(data)}")
    return 0


def cmd_release(args):
    owner = owner_of(args)
    data = read()
    if data and data["owner"] != owner and not args.force:
        print(f"REFUSÉ : le verrou est à {data['owner']}", file=sys.stderr)
        return 1
    LOCK.unlink(missing_ok=True)
    log(f"RELEASE {owner}")
    print("verrou rendu, éditeur libre")
    return 0


def cmd_close_editor(args):
    owner = owner_of(args)
    data = read()
    if not data or data["owner"] != owner:
        print("REFUSÉ : prends d'abord le verrou (acquire) avant de fermer l'éditeur", file=sys.stderr)
        return 1
    script = ("$p = Get-Process UnrealEditor -ErrorAction SilentlyContinue; if ($p) { $p | ForEach-Object { "
              "$_.CloseMainWindow() | Out-Null }; $p | Wait-Process -Timeout 120 -ErrorAction SilentlyContinue; "
              "if (Get-Process UnrealEditor -ErrorAction SilentlyContinue) { 'toujours ouvert'; exit 1 } else { 'fermé' } } "
              "else { 'pas d''éditeur' }")
    result = subprocess.run(["powershell", "-NoProfile", "-Command", script])
    log(f"CLOSE-EDITOR {owner} -> {result.returncode}")
    return result.returncode


def main():
    for stream in (sys.stdout, sys.stderr):
        if hasattr(stream, "reconfigure"):
            stream.reconfigure(encoding="utf-8", errors="replace")
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    for name in ("check", "acquire", "release", "close-editor"):
        p = sub.add_parser(name)
        p.add_argument("--owner", default="")
        if name == "acquire":
            p.add_argument("--reason", required=True)
            p.add_argument("--minutes", type=int, default=15)
        if name in ("acquire", "release"):
            p.add_argument("--force", action="store_true", help="seulement sur accord du propriétaire ou de l'utilisateur")
    args = parser.parse_args()
    handler = {"status": cmd_status, "check": cmd_check, "acquire": cmd_acquire, "release": cmd_release,
               "close-editor": cmd_close_editor}[args.cmd]
    sys.exit(handler(args))


if __name__ == "__main__":
    main()
