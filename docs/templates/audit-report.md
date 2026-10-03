# Audit <domaine> — <périmètre> — YYYY-MM-DD

- **Domaine** : architecture | mémoire | sécurité | gestion des erreurs
- **Norme** : `docs/conventions/<domaine>.md`
- **Agent** : `ue-…`
- **Tâche** : T-NNN, lot Lx
- **Périmètre** : dossiers ou fichiers, maps, build audité
- **Méthode** : lecture de code, recherches (`grep`), mesures (commandes exactes), tests lancés
- **Niveau de preuve** : lu | compilé | testé éditeur | testé build | testé multi-machines

## Synthèse
| Sévérité | Nombre | Corrigés |
|----------|--------|----------|
| CRITIQUE | | |
| MAJEUR | | |
| MINEUR | | |

Verdict : conforme | conforme avec réserves | non conforme (au moins un CRITIQUE ou un MAJEUR ouvert).

## Constats
| ID | Contrôle | Sévérité | Emplacement | Constat (preuve) | Correction | Statut |
|----|----------|----------|-------------|------------------|------------|--------|
| 1 | SEC-02 | CRITIQUE | `Source/monke_island/Items/CarryComponent.cpp:142` | `ServerRequestGrab` ne vérifie pas la distance (code cité) | vérifier la distance et la ligne de vue côté serveur | ouvert / corrigé / accepté (ADR-NNN) |

## Mesures (mémoire et performance)
Commandes, configuration, chiffres avant et après, fichiers produits (memreport, `.utrace`, CSV).

## Risques acceptés
Ce qui reste ouvert, avec la raison et l'ADR qui l'accepte.

## Suite
Tâches correctives créées au chantier (T-NNN), avec leur propriétaire.
