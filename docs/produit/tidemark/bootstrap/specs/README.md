# Specs — développement piloté par les specs (SDD)

Une spec par fonctionnalité, numérotée, vivante. Le code suit la spec ; si le code doit s'en écarter, la spec change dans le même commit, avec la raison.

## Cycle
| Étape | Livrable | Qui | Sortie |
|---|---|---|---|
| 1. Spec | `SPEC-NNN-<slug>.md`, statut `brouillon` → `validée` | `tidemark-lead` (+ rôle du domaine) | contrat public écrit avant le code |
| 2. Plan | section « Plan » : tâches TM-xx, fichiers disjoints, rôle, dépendances, preuve exigée | `tidemark-lead` | lignes dans `docs/backlog.md` |
| 3. Tâches | un agent par tâche (`/tidemark wave`) | rôles `tidemark-*` | code + specs `Tidemark.*` |
| 4. Preuve | build, specs, rapport joint ; critères cochés dans la spec | `tidemark-qa` | statut `livrée` ; preuve `compilé` / `testé éditeur` / `testé build` / `testé multi-machines` |

## Gabarit (`SPEC-000-template.md`)
```
# SPEC-NNN — <titre>
- Statut : brouillon | validée | livrée | remplacée par SPEC-MMM
- Lot : S<n> · Tickets : TM-xx… · Rôles : …
## 1. Intention (3 lignes : quoi, pour qui, pourquoi)
## 2. Contrat public (structures, fonctions, fichiers, unités, erreurs ; ce que les autres couches peuvent appeler)
## 3. Règles (déterminisme, réseau, licences : ce qui est interdit ici)
## 4. Valeurs tunables (toutes dans UTidemarkRules, avec défaut et unité)
## 5. Critères d'acceptation (numérotés, vérifiables, chacun → un It( nommé)
## 6. Preuve exigée (niveau, commande, artefact à joindre)
## 7. Plan (tâches, fichiers, dépendances)
## 8. Écarts et décisions (datés, avec la raison)
```

## Règles
- Un critère sans `It(` n'est pas un critère. Un `It(` sans critère est un test orphelin : on le rattache ou on le supprime.
- Les valeurs chiffrées vivent dans `UTidemarkRules` et sont citées par nom dans la spec, jamais recopiées en dur dans le code.
- Une spec cite ses sources quand elle s'appuie sur un fait extérieur (API moteur, licence, mesure) et marque « estimé » ce qui ne l'est pas.
- `docs/CAHIER_DES_CHARGES_TIDEMARK.md` prime : une spec qui le contredit demande une décision du propriétaire (§13) et une mise à jour du cahier.

## Index
| Spec | Titre | Statut | Lot |
|---|---|---|---|
| SPEC-001 | Cœur du plugin : solveur, seed, hash, échéancier, grille, format `.tmk`, importeurs | validée a posteriori (code S1–S2, non compilé) | S1–S2 |
| SPEC-002 | Pipeline de données ouvertes (`scripts/envgrid/`, Copernicus, ICESat-2, WorldCover, côte Etalab) | brouillon | V1.1 avancé (parallèle S3) |
| SPEC-003 | Subsystem de simulation, horloge, journal, correctifs A1–A8 | brouillon | S3 |
