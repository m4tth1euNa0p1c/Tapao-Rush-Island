---
name: ue-ai-behavior
description: Crée un comportement d'IA (PNJ ou faune) — contrôleur IA C++, tâches et conditions StateTree C++, perception, navigation — et décrit / vérifie l'asset StateTree, puis teste en Simulate.
argument-hint: "<PNJ ou créature> <comportement>"
---

# Comportement IA

Demande : $ARGUMENTS

1. **Spec du comportement** : états (ex. errer → alerté → fuir), transitions, perceptions (vue, ouïe), paramètres tunables, critères observables.
2. **Code** (agent `ue-ai-programmer`) :
   - contrôleur : template `AIController` (StateTreeAIComponent + AIPerception) ;
   - tâches et conditions : template `StateTreeTask`, sur le modèle de `Variant_Shooter/AI/ShooterStateTreeUtility` ;
   - compilation (skill `ue-build`).
3. **Asset StateTree** `ST_<Nom>` : le MCP (`state_tree_toolset...StateTreeTools`) permet de **l'inspecter**. La création et l'édition du graphe se font dans l'éditeur : fournir à l'utilisateur la liste précise des états, tâches, conditions et liaisons, puis vérifier le résultat par inspection MCP.
4. **Navigation** : NavMesh limité aux zones jouables ou navigation invokers (map de ~27 × 19 km).
5. **Test** : `StartPIE` avec `bSimulate: true`, observer (captures, `LogsToolset`), puis Automation Spec sur la logique pure (`ue-qa-tester`).
