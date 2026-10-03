"""Worktrees de l'usine monke_island : une branche dev/T-NNN et un arbre .worktrees/T-NNN par tâche de code.

Le C++ se développe et se compile dans le worktree (UBT sur la copie du .uproject) ; l'éditeur, les assets
(.uasset/.umap) et les PIE restent dans l'arbre principal, ressource exclusive (FACTORY.md §5).

Usage :
    python scripts/worktree.py new T-053 [--full]   # crée dev/T-053 + .worktrees/T-053 (code seul, ou --full)
    python scripts/worktree.py list                  # worktrees, branches et état (propre / modifié / en avance)
    python scripts/worktree.py build T-053           # compile l'éditeur du worktree (UBT)
    python scripts/worktree.py merge T-053           # fusionne dev/T-053 dans main (--no-ff), arbre principal propre exigé
    python scripts/worktree.py remove T-053          # supprime le worktree (la branche reste, sauf --delete-branch)

Code seul = sparse-checkout de Source/, Config/, le .uproject, docs/, scripts/, .claude/ et CLAUDE.md,
sans téléchargement LFS : quelques Mo au lieu d'environ 3 Go.
"""
import argparse
import os
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
TREES = ROOT / ".worktrees"
UE = Path(r"C:/Program Files/Epic Games/UE_5.8")
SPARSE = ["monke_island/Source", "monke_island/Config", "docs", "scripts", ".claude"]
SPARSE_FILES = ["monke_island/monke_island.uproject", "CLAUDE.md", ".gitignore", ".gitattributes"]


def git(*args, cwd=ROOT, env=None, check=True):
    result = subprocess.run(["git", *args], cwd=cwd, env=env, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if check and result.returncode != 0:
        sys.exit(f"git {' '.join(args)} a échoué :\n{result.stderr.strip()}")
    return result.stdout.strip()


def task_id(value):
    if not re.fullmatch(r"T-\d{3,}", value):
        sys.exit(f"ID de tâche invalide : {value} (attendu T-NNN)")
    return value


def cmd_new(args):
    tid = task_id(args.task)
    path = TREES / tid
    if path.exists():
        sys.exit(f"{path} existe déjà")
    TREES.mkdir(exist_ok=True)
    env = dict(os.environ, GIT_LFS_SKIP_SMUDGE="0" if args.full else "1")
    branch = f"dev/{tid}"
    exists = git("branch", "--list", branch)
    git("worktree", "add", "--no-checkout", str(path), *([branch] if exists else ["-b", branch]), env=env)
    if not args.full:
        git("sparse-checkout", "init", "--no-cone", cwd=path)
        patterns = [f"/{p}/" for p in SPARSE] + [f"/{f}" for f in SPARSE_FILES]
        git("sparse-checkout", "set", "--no-cone", *patterns, cwd=path)
    git("checkout", branch, cwd=path, env=env)
    print(f"{tid} : branche {branch}, arbre {path.relative_to(ROOT)} ({'complet' if args.full else 'code seul'})")
    print(f"Note chantier : env=dev ; worktree=.worktrees/{tid}")


def cmd_list(_):
    lines = git("worktree", "list", "--porcelain").split("\n\n")
    for block in lines:
        info = dict(line.split(" ", 1) if " " in line else (line, "") for line in block.splitlines())
        path = Path(info.get("worktree", ""))
        branch = info.get("branch", "").replace("refs/heads/", "")
        dirty = git("status", "--porcelain", cwd=path, check=False)
        ahead = git("rev-list", "--count", f"main..{branch}", check=False) if branch and branch != "main" else "0"
        state = "modifié" if dirty else "propre"
        print(f"{branch or '(détaché)':<14} {state:<8} +{ahead:<3} {path}")


def cmd_build(args):
    tid = task_id(args.task)
    project = TREES / tid / "monke_island" / "monke_island.uproject"
    if not project.exists():
        sys.exit(f"{project} introuvable : créer le worktree avec `new {tid}`")
    build = UE / "Engine" / "Build" / "BatchFiles" / "Build.bat"
    result = subprocess.run([str(build), "monke_islandEditor", "Win64", "Development", f"-Project={project}", "-WaitMutex"],
                            capture_output=True, text=True, encoding="utf-8", errors="replace")
    errors = [l for l in result.stdout.splitlines() if re.search(r"error [A-Z]+\d+|Result:", l)]
    print("\n".join(errors[-30:]) or result.stdout[-2000:])
    sys.exit(result.returncode)


def cmd_merge(args):
    tid = task_id(args.task)
    if git("status", "--porcelain", cwd=TREES / tid, check=False):
        sys.exit(f"Le worktree {tid} a des modifications non commitées.")
    # other sessions keep editing the main tree: only a file changed on both sides would stop the merge halfway
    changed = set(git("diff", "--name-only", f"main...dev/{tid}").splitlines())
    local = {line[3:] for line in git("status", "--porcelain").splitlines()}
    clash = sorted(changed & local)
    if clash:
        sys.exit("Fichiers modifiés à la fois par la branche et dans l'arbre principal :\n  " + "\n  ".join(clash))
    message = f"merge: {tid} dans main (fin de REC)"
    git("merge", "--no-ff", f"dev/{tid}", "-m", message)
    print(git("log", "--oneline", "-1"))


def cmd_remove(args):
    tid = task_id(args.task)
    git("worktree", "remove", str(TREES / tid))
    if args.delete_branch:
        git("branch", "-d", f"dev/{tid}")
    print(f"{tid} supprimé")


def main():
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    parser = argparse.ArgumentParser(description="Worktrees de l'usine monke_island")
    sub = parser.add_subparsers(dest="cmd", required=True)
    p = sub.add_parser("new"); p.add_argument("task"); p.add_argument("--full", action="store_true"); p.set_defaults(func=cmd_new)
    p = sub.add_parser("list"); p.set_defaults(func=cmd_list)
    p = sub.add_parser("build"); p.add_argument("task"); p.set_defaults(func=cmd_build)
    p = sub.add_parser("merge"); p.add_argument("task"); p.set_defaults(func=cmd_merge)
    p = sub.add_parser("remove"); p.add_argument("task"); p.add_argument("--delete-branch", action="store_true"); p.set_defaults(func=cmd_remove)
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
