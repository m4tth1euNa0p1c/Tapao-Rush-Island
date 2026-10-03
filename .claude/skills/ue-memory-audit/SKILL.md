---
name: ue-memory-audit
description: Audit mémoire de monke_island selon docs/conventions/memory.md — `code [dossier]` cherche les défauts de durée de vie et d'allocation (UPROPERTY manquant, pointeurs bruts, délégués et timers non retirés, chargements synchrones, références dures lourdes) ; `measure [map]` mesure RAM, VRAM et fuites (memreport, LLM, Memory Insights) contre les budgets. À utiliser après une feature, avant la gate PROD des lots L5 et L6, ou pour « ça consomme trop », « fuite », « saccade au chargement ».
argument-hint: "code [dossier] | measure [/Game/chemin/map]"
---

# Audit mémoire

Mode : `$ARGUMENTS`. Par défaut : `code` sur `monke_island/Source/monke_island/`.
Norme : `docs/conventions/memory.md`. Agent : `ue-memory-engineer`.

## `code [dossier]`
1. Recherches ciblées, une par contrôle :

   | Contrôle | Quoi chercher |
   |----------|---------------|
   | MEM-01 | membres `UObject*`, `AActor*`, `UActorComponent*`, `TObjectPtr` sans `UPROPERTY()` sur la ligne précédente (dans les `.h`) |
   | MEM-02 | pointeurs bruts gardés entre deux appels vers des acteurs qui peuvent être détruits |
   | MEM-03 | `CreateLambda`, `BindLambda`, `AddLambda` qui capturent `this` ; `SetTimer` et `AddDynamic` sans retrait dans `EndPlay` ou `Deinitialize` |
   | MEM-04 | `AddToRoot`, UObject `static` |
   | MEM-05 | `TArray` local, `FString::Printf`, `MakeShared` dans `Tick` ou `TickComponent` |
   | MEM-06 | `LoadObject`, `StaticLoadObject`, `LoadSynchronous`, `ConstructorHelpers` hors constructeur |
   | MEM-07 | `TSubclassOf` ou `TObjectPtr` vers le contenu lourd d'un autre domaine (au lieu de `TSoft*`) |
   | MEM-08 | `SetNum`, `Reserve` ou `AddUninitialized` sur une taille venue du réseau ou du disque, sans borne |

2. **Confirmer** chaque cas en lisant le code ; écarter les faux positifs.
3. **Contenu**, si l'éditeur est disponible et libre (`FACTORY.md` §5) : *Size Map* et *Reference Viewer* sur les Blueprints de gameplay. Un Blueprint qui entraîne un domaine entier est un constat MEM-07.
4. **Rapport** dans `docs/audits/<date>-memoire-<sujet>.md` (modèle `docs/templates/audit-report.md`), avec les corrections faites ou les tâches créées.

## `measure [map]`
1. Build packagé (Development ou Test) si possible, sinon `-game` non cooké, à signaler.
2. Lancer le scénario de référence (§12) avec :
   - `-ExecCmds="memreport -full"` au début et après 3 cycles hub → expédition → hub, pour comparer ;
   - `-llm -LLMCSV` pour suivre les catégories dans le temps ;
   - `-trace=default,memory` si une fuite doit être localisée (analyse dans Unreal Insights).
3. Comparer aux budgets de la norme §4 : RAM du processus, VRAM, pool de streaming (avertissements *over budget* dans le log), dérive après cycles (au plus ± 2 %).
4. **Rapport** dans la section Mesures du même modèle, avec les commandes exactes et les fichiers produits (`Saved/Profiling/MemReports/`, `.utrace`, CSV LLM).

## Règles
- Un chiffre pris dans l'éditeur est indicatif et le rapport le dit.
- Aucune option de mesure laissée dans les `.ini` du projet.
- Correction dans le périmètre de la tâche ; ailleurs, tâche corrective au chantier.
