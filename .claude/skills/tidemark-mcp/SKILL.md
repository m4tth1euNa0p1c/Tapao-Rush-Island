---
name: tidemark-mcp
description: Serveur MCP gratuit du plugin Tidemark (V1.1, après publication de la V1) — installation, configuration dans Claude Code, exécution des 3 outils (analyze_level, explain, lint_level) qui appellent le commandlet du plugin sans rien calculer, tests des 20 questions et vérification que chaque réponse porte rule_id, grid_hash et seed ou répond « unknown ». À utiliser pour « branche le MCP Tidemark », « teste le MCP », « pourquoi cette cellule ».
argument-hint: "[install | run | test | ask <outil> <args>]"
---

# MCP Tidemark (V1.1)

Arguments : `$ARGUMENTS`. Rôle : `tidemark-mcp-developer`. Pré-requis : TM-30 publié, TM-40 (commandlet JSON) livré. Cahier §0 (hors chemin critique), §6.4, §12-2.

## `install`
1. `Plugins/Tidemark/Tools/mcp/` : `pip install -e .` (dépendances : `mcp`, `numpy` ; rien d'autre).
2. Configuration Claude Code (`.mcp.json` du projet utilisateur) :
   ```json
   { "mcpServers": { "tidemark": { "command": "python", "args": ["-m", "tidemark_mcp", "--workdir", "<dossier .tmk et JSON exportés>", "--commandlet", "<chemin UnrealEditor-Cmd> <projet.uproject> -run=Tidemark"] } } }
   ```
3. `--workdir` borne les chemins lisibles ; sans `--commandlet`, le serveur ne lit que les JSON déjà exportés par l'éditeur (mode lecture seule, celui de la doc Fab).

## `run`
`python -m tidemark_mcp …` (stdio). Journal dans `workdir/mcp.log` : chaque appel, durée, taille de sortie, `grid_hash`.

## `test`
`pytest Plugins/Tidemark/Tools/tests/test_mcp.py` : 20 questions, dont 5 hors donnée. Attendu : 0 réponse sans `rule_id@version` et `grid_hash` ; 5 × `{"status": "unknown"}` ; 10 appels identiques → 10 sorties identiques ; dépassement du délai → erreur propre ; chemin hors `workdir` → refus.

## `ask <outil> <args>`
- `analyze_level <fichier.tmk>` → stats par couche, histogramme d'altitude, surfaces sous +100/+150/+240 cm, `credits[]`.
- `explain <fichier.tmk> <x> <y> [--level-cm]` → cellule, altitude, pas d'arrivée, ordre, règles déclenchées, voisins, `grid_hash`, `seed`.
- `lint_level <fichier.tmk> <annotations.json> [--levels]` → `FLintFinding[]` + la mention non désactivable (CONVENTIONS §7).

## Règles
- Le serveur ne contient aucune heuristique ni appel à un modèle de langage : il formule la sortie du commandlet, c'est tout. Si le commandlet échoue, la réponse le dit (`status: "error"`, stderr tronqué), jamais une estimation.
- Pas de `design_scenario`. `propose_schedule` est V2, après décision du propriétaire.
- Licence MIT, hors Fab.
