---
name: release-loop
description: Boucle de livraison jusqu'à un exécutable Windows sous contrainte de temps et de tokens — chemin critique séquentiel sur l'éditeur partagé (verrou, build, assets, tests, packaging, smoke test), agents en parallèle seulement hors éditeur, budget par étape, critères d'arrêt et de repli.
argument-hint: "[heure limite, ex. 22:15] [Development|Shipping]"
---

# Boucle de livraison jusqu'à l'exécutable Windows

Cible : $ARGUMENTS (défaut : Development, au plus tôt). Sortie : `build/package/Windows/`, rapport `docs/releases/<version>.md`.

## Principe : un seul chemin critique, des agents autour
L'éditeur, UBT et le cook sont des **ressources exclusives** (FACTORY.md). Le chemin critique est mené par la session principale, une étape après l'autre ; jamais deux builds ou cooks en parallèle.

Les agents ne travaillent **que hors éditeur**, pendant les attentes :
- relecture du code (`ue-code-reviewer`, lecture seule) ;
- rapport de livraison (`ue-integration-lead` ou `ue-producer`, documents) ;
- analyse d'un échec (`ue-build-engineer`, lecture de log).

Un agent ne compile pas, ne ferme pas l'éditeur et ne lance pas de PIE pendant la boucle.

## Budget (temps et tokens)
| Étape | Budget | Dépassement → |
|---|---|---|
| 0. Attente du verrou (pair en fenêtre déclarée) | fin de la fenêtre annoncée | prévenir l'utilisateur, ne jamais fermer l'éditeur d'un autre |
| 1. Verrou + fermeture + `ue.ps1 build` | 15 min | 1re erreur → correction ciblée ; 2e échec → `ue-build-engineer` |
| 2. Scripts d'assets (commandlets) | 5 min | asset manquant → journaliser, continuer si non bloquant |
| 3. `ue.ps1 test -Filter monke_island` | 15 min | échec nouveau → corriger s'il touche le code du jour, sinon noter et continuer |
| 4. Éditeur relancé + PIE éclair (si l'utilisateur est là) | 15 min | sauter si l'heure limite est proche (le dire) |
| 5. `ue.ps1 package -Config Development` en arrière-plan | 45 min | 1re `Error:` du log UAT → correction ou exclusion de l'asset, relance une fois |
| 6. Smoke test de l'exe (menu, L_Boot, île, quitter) + log | 10 min | crash → rapport, l'exe précédent reste la livraison |

**Tokens** :
- les longues commandes tournent **en arrière-plan** avec une attente qui s'arrête d'elle-même (`until … ; do sleep 30; done`) ;
- on lit seulement les lignes `error` / `Error:` / `Result:` des logs, jamais un log entier ;
- un agent reçoit un brief autonome court, avec le modèle le moins cher qui suffit (`sonnet` pour une relecture, `haiku` pour un tri de log) ;
- au plus 2 agents à la fois.

## La boucle
```
tant que pas d'exécutable vérifié :
  étape suivante du tableau → résultat
  OK      → étape suivante
  échec   → correction la plus petite qui débloque (budget de l'étape), puis même étape
  2 échecs de la même étape → agent spécialiste + prévenir l'utilisateur
  heure limite − 30 min → couper les étapes 4 et facultatives, packager ce qui compile
```

## Règles du projet qui restent vraies
- **Verrou** : `python scripts/editor_lock.py acquire|close-editor|release`, avec `MONKE_LOCK_OWNER` positionné.
- **PIE** : jamais de StopPIE sur un PIE qui n'est pas le sien.
- **Commits** : pas de commit sans l'accord explicite de l'utilisateur. Le package se fait sur l'arbre de travail.
- **Preuve** : `compilé` / `testé éditeur` / `testé build`, sans la gonfler (cahier §2). Chantier mis à jour à chaque étape (`/chantier`).
- **Fin** : chemin de l'exe, taille, durée, ce qui est vérifié et ce qui ne l'est pas, rapport dans `docs/releases/`.
