---
name: tidemark-lint
description: Bake first-wet et linter Tidemark — lance la simulation hors ligne vers la texture d'instants d'arrivée, applique les règles de lint sur les acteurs d'annotation (camp, route, refuge, spawn), affiche le calque et le curseur de niveau de mer, puis vérifie chaque finding par rejeu en PIE. À utiliser pour « lint cette map », « ta route se coupe quand ? », « vérifie que le lint dit vrai », et pour produire la capture de 10 s hebdomadaire.
argument-hint: "<bake <Map> | lint <Map> [--levels 100,150,240] | verify <Map> | capture <Map>>"
---

# First-wet et lint

Arguments : `$ARGUMENTS`. Rôle : `tidemark-editor-programmer` (outil), `tidemark-qa` (vérification). Cahier §4, §6.4.

## `bake <Map>`
1. Verrou éditeur. La map a un `UTidemarkTerrainAsset` (sinon `/tidemark-terrain-import`).
2. Échéancier : `UTidemarkRules.DefaultSchedule` ou celui passé (`--schedule <id>`).
3. `FTidemarkFirstWetBake` : simulation complète avec `FTidemarkSolver` jusqu'au niveau max ou `MaxBakeSteps` ; par cellule `FirstWetStep`, `FloodOrder`. Sauvé dans l'asset dérivé `TA_<Map>_FirstWet` avec `GridHash` et `ScheduleId`.
4. Durée mesurée (objectif < 30 s pour 512²). Calque `tidemark.Overlay 1` : bleu (tôt) → rouge (tard), gris = jamais.

## `lint <Map> [--levels]`
1. Acteurs `ATidemarkAnnotationActor` dans la map : au moins un `Camp` ou `Refuge` et une `Route`, sinon le lint le dit.
2. Règles (versionnées) :
   - `Rule.Submersible@1` : annotation sous l'eau à l'un des niveaux donnés → `Severity Error` si `Refuge`/`Spawn`, `Warning` sinon ; message `« Camp B floods at +1.2 m (step 4120, 3:26) »`.
   - `Rule.FloodOrder@1` : ordre d'inondation des annotations ; signale une `Route` qui se mouille avant le `Camp` qu'elle dessert.
   - `Rule.PathCut@1` : pour chaque `Route` (A→B), premier pas où aucun chemin 8-connexe sec n'existe ; `« Bridge: cut at 3:40 »`.
   - `Rule.GridStale@1` : hash des sources ≠ asset → tout le reste est suspect.
3. Sortie : Message Log « Tidemark » avec focus, et `Saved/Tidemark/lint-<Map>.json` (`FLintFinding[]`, `grid_hash`, `schedule_id`, `plugin_version`, mention non désactivable).

## `verify <Map>`
Pour chaque finding du JSON : PIE avec le même échéancier, `tidemark.Hash` et `SampleDepth` à l'annotation au pas annoncé ± `VerifyToleranceSteps` → mouillé exactement là. 100 % attendus ; tout écart est un bug (`docs/bugs/`), pas une tolérance à élargir. C'est la spec `Tidemark.Lint.VerifiedByReplay`, jouée à la main sur 3 maps : un Landscape du marketplace, la heightmap de test à 3 bassins, l'île de démo.

## `capture <Map>`
10 s : calque visible, curseur de +0 à +2,4 m, deux findings à l'écran. `CaptureViewport` ×10 à 1 s d'intervalle via le MCP + `scripts/mcp_image.py`, ou enregistrement sur la machine du propriétaire. Fichier joint au ticket. Question à se poser : « est-ce qu'on a envie de l'envoyer ? » Sinon, lisibilité d'abord.

## Pièges
- Le curseur doit lire la texture bakée, jamais un seuil sur l'altitude seule : sinon c'est un plan en Z, exactement ce que le produit n'est pas.
- Un bake invalide (grille ou échéancier changés) doit se voir : badge `Stale` dans le panneau.
- Les textes sont en anglais (produit) ; les messages du chantier en français.
