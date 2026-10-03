---
name: ue-feature
description: Pipeline complet "équipe dev" pour livrer une feature de gameplay monke_island — spec (game designer) → design technique (architecte) → implémentation C++ → build → intégration éditeur via MCP → tests QA → code review. À utiliser quand l'utilisateur demande une nouvelle feature / mécanique / système.
argument-hint: <description de la feature>
---

# Pipeline feature — monke_island

Tu es le **lead / producer** : tu orchestres les agents, tu ne sautes pas d'étape, et tu tiens la spec `docs/features/<nom>.md` à jour (section 5 Tâches, section 7 Journal).

Feature demandée : $ARGUMENTS

**Contrat** : `docs/CAHIER_DES_CHARGES.md` (adoption : `docs/adr/ADR-001-cahier-des-charges-crownfall.md`). Avant l'étape 1, identifie le lot (L0–L8, E1), les sections du cahier qui décrivent la feature (règles, valeurs Standard, contrats §21, flux §22) et les IDs de recette (§12, §23) qu'elle doit passer. Une demande hors cahier ou contraire au cahier → signale l'écart à l'utilisateur avant de lancer les agents.

## Étapes

1. **Spec** — agent `ue-game-designer` avec la demande brute, le lot et les sections du cahier à reprendre.
   S'il remonte des *Questions ouvertes* qui changent le gameplay → pose-les à l'utilisateur (AskUserQuestion) avant de continuer.

2. **Design technique** — agent `ue-architect` avec le chemin de la spec.
   Montre à l'utilisateur un résumé (classes, frontière C++/BP, tâches) et attends son feu vert si la feature touche plus de ~3 classes ou introduit un ADR.

3. **Implémentation** — pour chaque tâche C++ : agent `ue-gameplay-programmer`.
   Tâches indépendantes (fichiers disjoints) → agents en parallèle. Tâches qui touchent les mêmes fichiers → séquentiel.

4. **Build** — skill `ue-build`. Échec non trivial → agent `ue-build-engineer`.

5. **Intégration éditeur** — si l'éditeur est ouvert et le MCP `unreal` dispo : agent `ue-editor-integrator` (BP dérivés, DataAssets, placement dans le niveau). Sinon, liste les étapes manuelles pour l'utilisateur.

6. **Tests** — agent `ue-qa-tester` (specs d'automatisation par critère d'acceptation + exécution, et recette du cahier concernée ; multijoueur : PIE 2 joueurs listen server minimum dès L1).

7. **Review** — agent `ue-code-reviewer` sur tous les fichiers touchés. Corrige les points CRITIQUE/MAJEUR (retour étape 3-4), puis relance la review si des corrections ont été faites.

8. **Clôture** — statut `done` dans la spec, résumé à l'utilisateur : ce qui a été fait, niveau de preuve (compilé / testé éditeur / testé build / testé multi-machines), recette du cahier passée ou non (IDs), résultat build + tests (chiffres réels), opérations d'éditeur restant à faire.

## Règles
- Ne déclare jamais une étape réussie sans sa preuve (sortie de build, résultats de tests). « Code écrit » n'est pas « Validé » ; aucun `.uasset` n'est créé par simple rédaction de code.
- Valeurs du cahier en DataAsset, logique d'autorité en C++ côté serveur, widgets qui observent sans calculer (§21).
- Petite modif (1 fichier, pas de nouvelle classe) → saute 1-2 et va directement en 3.
