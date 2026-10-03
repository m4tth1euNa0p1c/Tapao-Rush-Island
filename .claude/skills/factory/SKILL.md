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
2. **Ressources** (FACTORY §5) : au plus 1 tâche éditeur, 1 tâche Blender, 1 propriétaire par map / Blueprint, fichiers disjoints entre tâches de code. Taille par défaut : 6 en mode mono-arbre, 15 avec git (`--max` pour changer). Pas plus de 3 tâches `en-cours` par rôle.
3. **Montre la vague** à l'utilisateur (ID, rôle, cellule, ressource tenue, isolation) et lance-la après accord — sauf si l'utilisateur a déjà dit « lance ».
4. **Lancement** : un appel `Agent` par tâche, **tous dans le même message** pour qu'ils tournent en parallèle, `subagent_type` = rôle, `isolation: "worktree"` pour les tâches de code quand le dossier est un dépôt git (sinon pas d'isolation, fichiers disjoints obligatoires). Le prompt de chaque agent contient :
   - `Tâche T-NNN — env=dev — lot Lx — cellule <C>` et la fiche §13 complète ;
   - les fichiers / assets autorisés et la ressource exclusive tenue (`owner=`) ;
   - « Passe T-NNN en-cours au début ; termine en `review` avec `env=rec` et ta preuve, ou en `bloque` avec ce qu'il faut pour débloquer. Ne passe jamais `fait`. »
5. **Suivi** : à chaque retour d'agent, relaie l'essentiel en une ligne, mets à jour la note si besoin, libère la ressource exclusive, et propose la vague suivante ou `/factory-gate rec` sur les tâches revenues en `review`.

## Règles
- La session principale est le seul lanceur ; les agents ne lancent pas d'agents.
- Ne jamais inventer le résultat d'un agent en cours.
- Aucune tâche hors cahier dans une vague sans accord explicite de l'utilisateur.
