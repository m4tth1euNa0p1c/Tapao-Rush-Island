---
name: factory-gate
description: Fait passer une tâche ou un lot d'un environnement de l'usine au suivant — REC (recette QA + code review + revues réseau, sécurité et fiabilité selon la tâche), INT (fusion, build complet, toute la suite de tests, PIE solo et hôte/client), PROD (revue d'architecture, build packagé, recette du lot du cahier, perf et mémoire contre les budgets, sécurité Shipping, release checklist). À utiliser pour « passe en recette », « intègre », « valide le lot », « mets en prod ».
argument-hint: "rec|int|prod <T-NNN…|Lx>"
---

# Gates de l'usine

Arguments : `$ARGUMENTS`
Charte : `.claude/factory/FACTORY.md` §2. Contrat : `docs/CAHIER_DES_CHARGES.md` §11, §12, §15, §23.

## `rec <T-NNN…>` — DEV → REC → prêt pour INT
Entrée : tâche en `review` avec `env=rec` et preuve DEV (compilation au moins). Lance **en parallèle** (même message) :
- `ue-qa-tester` : critères d'acceptation de la spec + IDs de recette du cahier cités dans la tâche ; tests automatisés + PIE (hôte + client dès L1) ;
- `ue-code-reviewer` : fichiers de la tâche ;
- `ue-network-engineer` : seulement si la tâche touche autorité, dégâts, temps, loot, extraction, sauvegarde ou réplication ;
- `ue-security-engineer` (`/ue-security-review code`) : seulement si la tâche ajoute ou modifie un RPC, une lecture ou écriture de fichier (sauvegarde, projet d'île, paramètres), une saisie joueur, un plugin ou une dépendance ;
- `ue-reliability-engineer` (`/ue-error-handling audit`) : seulement si la tâche ajoute une opération faillible (fichier, async, réseau, chargement) ou implémente un flux du §22.
Tout est vert (aucun CRITIQUE/MAJEUR, recette passée, `revue-reseau=OK`, `revue-securite=OK`, `revue-fiabilite=OK` quand requises) → note `env=int`. Sinon → `en-cours`, `env=dev`, écarts listés, et relance du rôle propriétaire.

## `int <T-NNN…>` — REC → INT
Lance `ue-integration-lead` sur les tâches `env=int` (gate INT de son fichier). Une seule gate INT à la fois (ressource : build complet + éditeur). Résultat : `fait` avec `preuve=testé éditeur`, ou retour DEV avec l'écart.

## `prod <Lx>` — INT → PROD
Pré-requis : toutes les tâches du lot `fait`, et `/ue-architecture review` par `ue-architect` sur le code du lot, sans CRITIQUE ni MAJEUR ouvert. Lance `ue-integration-lead` (gate PROD), qui enchaîne :
- `ue-package` ;
- la recette du lot par `ue-qa-tester`, sur le build packagé ;
- pour les lots L5 et L6 : `/ue-optimize` par `ue-performance-engineer` et `/ue-memory-audit measure` par `ue-memory-engineer`, contre les budgets de `docs/conventions/` ;
- avant tout build distribué (L6, L7) : `/ue-security-review shipping` puis `secrets`, par `ue-security-engineer`.

Résultat : « Lot Lx validé » au journal, avec des preuves `testé build` (et `testé multi-machines` pour L7), ou la liste des tâches correctives.

## Règles
- Pas de saut d'environnement, pas de gate déclarée passée sans sa preuve.
- Montre à l'utilisateur le résultat de chaque gate : passée / refusée, chiffres réels (tests N/N, erreurs, FPS mesurés), écarts.
- La gate PROD ne publie rien et ne pousse rien sur un dépôt distant sans demande explicite.
