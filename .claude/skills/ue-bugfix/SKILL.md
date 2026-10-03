---
name: ue-bugfix
description: Débogage structuré d'un bug ou crash Unreal dans monke_island — reproduire, lire logs/callstack, trouver la cause racine, corriger, ajouter un test de non-régression.
argument-hint: <description du bug ou callstack>
---

# Bugfix

Bug : $ARGUMENTS

1. **Rapport** : crée `docs/bugs/BUG-NNN-<slug>.md` depuis `docs/templates/bug-report.md`.
2. **Preuves** :
   - log éditeur `monke_island/Saved/Logs/monke_island.log` (cherche `Error`, `Warning`, `Ensure`, `Assertion failed`, `Fatal`)
   - crash : `monke_island/Saved/Crashes/*/` (callstack dans le `.log` / `CrashContext.runtime-xml`)
   - éditeur ouvert : MCP `unreal` pour inspecter acteurs/propriétés en jeu
3. **Cause racine** : remonte la callstack jusqu'au code du projet ; vérifie l'hypothèse dans le code avant de corriger. Suspects fréquents : pointeur null après `Cast`/`GetOwner`, UObject collecté (pas de `UPROPERTY`), ordre d'init (`BeginPlay` vs constructeur vs `PostInitializeComponents`), BP qui override une valeur C++.
4. **Test de non-régression** qui échoue avant le fix (agent `ue-qa-tester` ou template AutomationTest).
5. **Fix** minimal → skill `ue-build` → skill `ue-test`.
6. Mets le rapport à jour (cause, correctif, test).
