---
name: factory
description: Pilote l'usine agentique monke_island selon le cahier des charges — roster des agents et skills, cellules, et lancement d'une vague d'agents en parallèle sur les tâches prêtes du chantier (fiches complètes, ressources exclusives respectées, isolation par worktree quand git existe). À utiliser pour « lance l'équipe », « lance la vague », « fais tourner les agents », « organisation de l'usine », « qui est prêt ».
argument-hint: "[roster | wave [sprint-NN|lot Lx] [--max N] | plan <lot>]"
---

# Usine agentique

Arguments : `$ARGUMENTS`
Charte : `.claude/factory/FACTORY.md` (lis-la d'abord). Contrat : `docs/CAHIER_DES_CHARGES.md`.

## `roster` (défaut)
1. Liste les agents (`.claude/agents/*.md` : nom + 1re phrase de la description) et les skills (`.claude/skills/*/SKILL.md`), groupés par cellule (FACTORY §3).
2. `python scripts/chantier.py --env` : pour chaque rôle, tâches en cours / bloquées ; rôles disponibles.
3. Signale les écarts : agent présent dans le chantier mais absent du roster, cellule sans lead disponible, ressource exclusive tenue (`owner=` dans les notes).

## `plan <lot>`
Délègue à `/ue-sprint` pour le lot (fiches §13, recette visée), puis répartit les tâches par cellule et par vague selon FACTORY §4–5. Attends l'accord de l'utilisateur avant de créer les tâches.

## `wave [sprint-NN|lot Lx] [--max N]`
1. **Tâches prêtes** : `a-faire`, dépendances (`après T-NNN`) toutes `fait` ou `review`, fiche §13 complète (lot, fichiers autorisés, contrat, recette, preuve). Fiche incomplète → la tâche reste hors vague, signale ce qui manque.
2. **Ressources** (FACTORY §5) : au plus 1 tâche éditeur, 1 tâche Blender, 1 propriétaire par map / Blueprint, fichiers disjoints entre tâches de code. Taille par défaut : 15 (code et docs en worktrees) ; 6 en repli mono-arbre (`--max` pour changer). Pas plus de 3 tâches `en-cours` par rôle.
3. **Montre la vague** à l'utilisateur (ID, rôle, cellule, ressource tenue, isolation) et lance-la après accord — sauf si l'utilisateur a déjà dit « lance ».
4. **Worktrees** : pour chaque tâche de code, de docs ou de scripts, `python scripts/worktree.py new T-NNN` (léger : sans `Content/` ni LFS ; `--full` si la tâche lit des assets). Les tâches éditeur et Blender n'en ont pas : elles travaillent dans l'arbre principal, avec la ressource exclusive et `owner=<asset>` dans la note pour chaque asset qu'elles modifient (pas de `git lfs lock` : dépôt sans remote).
5. **Lancement** : un appel `Agent` par tâche, **tous dans le même message** pour qu'ils tournent en parallèle, `subagent_type` = rôle, **sans** `isolation: "worktree"` : l'isolation de l'outil Agent ferait un checkout complet de `Content/` et du LFS (environ 3 Go par agent). Le prompt de chaque agent contient :
   - `Tâche T-NNN — env=dev — lot Lx — cellule <C>` et la fiche §13 complète ;
   - « Travaille uniquement dans `C:/Users/matth/monk-island/.worktrees/T-NNN` (branche `dev/T-NNN`) : lis et écris tes fichiers avec ce chemin absolu. Compile avec `python scripts/worktree.py build T-NNN` (pas avec `ue.ps1 build` ni Live Coding, qui visent l'arbre principal). Commite tes fichiers sur `dev/T-NNN` avec le préfixe de domaine et l'ID (`gameplay: … (T-NNN)`), jamais avec `git add -A`. » (tâches de code, docs et scripts) ;
   - les fichiers / assets autorisés et la ressource exclusive tenue (`owner=`) ;
   - « Passe T-NNN en-cours au début ; termine en `review` avec `env=rec` et ta preuve, ou en `bloque` avec ce qu'il faut pour débloquer. Ne passe jamais `fait`. »
6. **Suivi** : à chaque retour d'agent, relaie l'essentiel en une ligne, mets à jour la note (`branche=dev/T-NNN`), libère la ressource exclusive, `python scripts/worktree.py list` pour voir les branches en avance, et propose la vague suivante ou `/factory-gate rec` sur les tâches revenues en `review`.

## Règles
- La session principale est le seul lanceur ; les agents ne lancent pas d'agents.
- Ne jamais inventer le résultat d'un agent en cours.
- Aucune tâche hors cahier dans une vague sans accord explicite de l'utilisateur.
