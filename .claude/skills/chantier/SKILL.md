---
name: chantier
description: Tableau de chantier de l'équipe — statut de toutes les tâches et de chaque agent (en cours, bloqué, en review, fait), agents disponibles, features, bugs ouverts, journal. Permet aussi d'ajouter une tâche ou de changer un statut. À utiliser pour « où en est-on », « statut des agents », « qui fait quoi », « qu'est-ce qui bloque », ou pour mettre à jour une tâche.
argument-hint: "[vide | agent <nom> | add \"<titre>\" --agent <nom> | set <ID> <statut> --note \"...\" | log]"
---

# Chantier

Arguments : `$ARGUMENTS`

1. Exécute `python scripts/chantier.py $ARGUMENTS`. Sans argument, ajoute `--env` pour inclure l'état éditeur / MCP : `python scripts/chantier.py --env`.
2. Restitue la sortie dans un bloc de code, sans la reformuler.
3. Ajoute au plus 3 lignes : ce qui bloque et l'action pour débloquer, puis la prochaine tâche prioritaire pour un agent disponible.

## Commandes
| Commande | Effet |
|----------|-------|
| `python scripts/chantier.py` | Tableau de bord (par agent, disponibles, blocages, features, bugs, journal) |
| `python scripts/chantier.py --env` | Idem + éditeur ouvert / MCP joignable |
| `python scripts/chantier.py agent ue-tech-artist` | Tâches d'un agent |
| `python scripts/chantier.py add "Eau du lagon SLW" --agent ue-tech-artist --feature map-raraka` | Nouvelle tâche (ID auto `T-NNN`) |
| `python scripts/chantier.py set T-008 en-cours --note "master M_Water_Lagoon"` | Change le statut (+ ligne de journal) |
| `python scripts/chantier.py set T-008 bloque --note "attend la bathymétrie (T-004)"` | Blocage, toujours avec ce qu'il faut pour débloquer |
| `python scripts/chantier.py set T-008 fait --agent ue-tech-artist --note "capture OK"` | Terminer (ou réassigner avec `--agent`) |
| `python scripts/chantier.py log -n 30` | Journal |

## Statuts
`a-faire` → `en-cours` → `review` → `fait` ; `bloque` à tout moment, toujours avec une note.

## Règles
- Fichiers : `docs/production/board.md` (géré par le script, ne pas éditer les lignes à la main) et `docs/production/journal.md` (append-only).
- Agents valides : fichiers de `.claude/agents/`, plus `lead` (session principale) et `user`.
- Quand la session principale lance un agent sur une tâche, elle lui passe l'ID `T-NNN` dans le prompt.
