---
name: ue-vcs
description: Gestion de version du projet Unreal monke_island avec Git + Git LFS — mise en place (gitattributes pour les binaires Unreal), conventions de branches et de commits, vérifications avant commit. Ne fait rien d'irréversible sans accord explicite.
argument-hint: "[setup | status | commit]"
---

# Git + LFS pour Unreal

Action : $ARGUMENTS

## Setup (une fois, avec l'accord de l'utilisateur)
1. Vérifier : `git --version`, `git lfs version` (sinon installer Git LFS).
2. `git init` à la racine `monk-island/` (le `.gitignore` existe déjà : Binaries, Intermediate, Saved, DerivedDataCache, sln, build, caches carto).
3. Copier `.claude/skills/ue-vcs/templates/gitattributes` vers `.gitattributes` (uasset, umap, textures, meshes, audio, vidéo en LFS ; `.uasset`/`.umap` verrouillables).
4. `git lfs install`, premier commit, puis branche de travail.

## Worktrees de l'usine (actif depuis le 2026-10-03)
Le dépôt existe (Git + LFS local, `main`). Outil unique : `python scripts/worktree.py`.
- `new T-NNN [--full]` : `.worktrees/T-NNN` sur `dev/T-NNN`, code + docs sans `Content/` ni LFS (`--full` = checkout complet) ;
- `build T-NNN` : UBT du worktree, éditeur ouvert ou non ;
- `list` : worktrees, branches, avance sur `main`, état ;
- `merge T-NNN` : `--no-ff` dans `main` (gate INT) ;
- `remove T-NNN [--delete-branch]` : après `fait`.

Règles communes à toutes les sessions :
- Sur `main`, ne commiter qu'après un build OK, et seulement ses propres fichiers (`git add <fichiers>`, jamais `git add -A` ni `commit -a` sur le travail des autres sessions).
- Assets : `git lfs lock <.uasset|.umap>` avant de les modifier dans l'arbre principal, `git lfs unlock` après le commit. Une map ou un Blueprint verrouillé par une autre session : ne pas y toucher.
- Pas de `push`, pas de dépôt distant sans demande explicite de l'utilisateur.
- Ne pas utiliser `isolation: "worktree"` de l'outil Agent : il ferait un checkout complet (environ 3 Go par agent).

## Conventions
- Branches : `feat/<feature>`, `fix/<bug>`, `art/<sujet>`, `map/<ile>`.
- Commits : impératif, en français ou en anglais, préfixés par le domaine (`gameplay:`, `map:`, `art:`, `build:`) et l'ID chantier si pertinent (`map: terrain Raraka v1 (T-005)`).
- Jamais de commit des dossiers générés ni des téléchargements bruts (`docs/reference/map/raw/`, `art/textures/` régénérables par script).
- Les `.uasset` et `.umap` sont binaires : verrouiller (`git lfs lock`) avant de modifier un asset partagé quand l'équipe est à plusieurs.

## Avant chaque commit
- Build OK, tests OK, assets sauvegardés (`AssetTools.save_assets`), pas de fichier de plus de 100 Mo hors LFS (`git lfs status`).
