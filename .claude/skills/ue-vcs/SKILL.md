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

## Conventions
- Branches : `feat/<feature>`, `fix/<bug>`, `art/<sujet>`, `map/<ile>`.
- Commits : impératif, en français ou en anglais, préfixés par le domaine (`gameplay:`, `map:`, `art:`, `build:`) et l'ID chantier si pertinent (`map: terrain Raraka v1 (T-005)`).
- Jamais de commit des dossiers générés ni des téléchargements bruts (`docs/reference/map/raw/`, `art/textures/` régénérables par script).
- Les `.uasset` et `.umap` sont binaires : verrouiller (`git lfs lock`) avant de modifier un asset partagé quand l'équipe est à plusieurs.

## Avant chaque commit
- Build OK, tests OK, assets sauvegardés (`AssetTools.save_assets`), pas de fichier de plus de 100 Mo hors LFS (`git lfs status`).
