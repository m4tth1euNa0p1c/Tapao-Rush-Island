---
name: tidemark
description: Pilote la cellule Tidemark (plugin UE de montée des eaux déterministe, répliquée, lisible dans l'éditeur) — statut du backlog TM-01 → TM-43, création des tickets d'une semaine dans le chantier, lancement d'une vague d'agents Tidemark en parallèle, et tenue des portes S2 (go/no-go), S4 (multijoueur) et S5 (capture). À utiliser pour « où en est Tidemark », « lance la semaine 1 », « lance la vague Tidemark », « porte S4 ».
argument-hint: "[status | plan <S1..S8|V1.1> | wave [S<n>] [--max N] | gate <S2|S4|S5>]"
---

# Cellule Tidemark

Arguments : `$ARGUMENTS`. Contrat : `docs/produit/CAHIER_DES_CHARGES_TIDEMARK.md`. Conventions : `docs/produit/tidemark/CONVENTIONS.md`. Backlog : `docs/produit/tidemark/backlog.md`. Charte générale : `.claude/factory/FACTORY.md` (la cellule Tidemark y est décrite au §3).

Rôles : `tidemark-lead`, `tidemark-solver-programmer`, `tidemark-terrain-programmer`, `tidemark-network-engineer`, `tidemark-editor-programmer`, `tidemark-render-programmer`, `tidemark-qa`, `tidemark-release-engineer`, `tidemark-tapao-integrator`, `tidemark-mcp-developer` (V1.1).

## `status` (défaut)
1. `python scripts/chantier.py agent tidemark-*` pour chaque rôle ; tickets `[TM]` par statut.
2. Semaine en cours = la plus petite semaine du backlog dont un ticket n'est pas `fait`. Porte en attente ? (`docs/produit/tidemark/gate-S2.md`, `gate-S4.md` existent-ils, avec verdict ?)
3. Résume en 10 lignes : fait, en cours, bloqué, prochaine porte, ce qui manque pour la franchir.

## `plan <semaine>`
1. Lis la section de la semaine dans `backlog.md`.
2. Pour chaque ligne TM-xx non encore créée dans le chantier : `python scripts/chantier.py add --agent <rôle> --feature tidemark --note "TM-xx — après <dépendances> — fichiers : … — preuve : …" "[TM][S<n>] <titre>"`. Garde la correspondance TM-xx ↔ T-NNN dans la note et dans `docs/produit/tidemark/backlog.md` (colonne ajoutée « Ticket »).
3. Montre la liste à l'utilisateur. Ne crée pas deux fois un ticket.

## `wave [S<n>] [--max N]`
Même mécanique que `/factory wave`, restreinte aux tickets `[TM]` :
1. **Prêts** : `a-faire`, dépendances (`après TM-xx`) toutes `fait` ou `review`, fiche complète (rôle, fichiers, preuve).
2. **Ressources** : fichiers disjoints entre tâches de code ; au plus une tâche qui tient l'éditeur (`owner=editor` : import Landscape, matériaux, PIE) ; les tâches de code en worktree (`python scripts/worktree.py new T-NNN`).
3. **Montre la vague** (ID, rôle, fichiers, ressource) et lance après accord, sauf si l'utilisateur a déjà dit « lance ».
4. **Lancement** : un appel `Agent` par tâche, tous dans le même message, `subagent_type` = rôle Tidemark, prompt = `Tâche T-NNN (TM-xx) — env=dev — cellule Tidemark` + la ligne du backlog + « Lis d'abord `docs/produit/tidemark/CONVENTIONS.md` et ta fiche de rôle. Travaille uniquement dans `<worktree>` sur `dev/T-NNN` ; fichiers autorisés : … ; passe T-NNN `en-cours` au début, termine en `review` + `env=rec` avec ta preuve (ou `bloque`), jamais `fait`. »
5. **Suivi** : à chaque retour, une ligne à l'utilisateur, note du chantier mise à jour, ressource libérée. Puis `/factory-gate` pour REC et INT, comme pour le jeu.

## `gate <S2|S4|S5>`
- **S2** : `tidemark-lead` écrit `gate-S2.md` (specs vertes : `Tidemark.Solver.*`, `Determinism`, `Import` ; benchmark TM-06 ; risques). Verdict : go (décision du propriétaire du 2026-10-23) avec les réserves écrites.
- **S4** : `tidemark-qa` + `tidemark-network-engineer` via `/tidemark-netcheck` ; verdict binaire dans `gate-S4.md`. Rouge = aucune tâche S5 ne part ; on corrige d'abord.
- **S5** : la capture de 10 s de TM-21 est-elle publiable telle quelle ? Si non, la lisibilité passe avant TM-23.

## Règles
- Une seule menace (l'eau), trois couches, zéro include du jeu, rien en dur, déterminisme prouvé : tout écart est renvoyé au `tidemark-lead` et au cahier §12.
- Tapao est gelé pendant les 8 semaines hors correctifs ; seul `tidemark-tapao-integrator` touche `Source/monke_island/`, et seulement en S7.
- Le niveau de preuve est toujours écrit. « Code écrit » n'est pas « fait ».
