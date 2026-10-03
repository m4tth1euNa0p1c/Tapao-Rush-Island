---
name: ue-playtest
description: Session de playtest pilotée via le MCP — lancer le PIE sur une map, parcourir les points clés, capturer, lire les logs (erreurs, warnings, ensures) — et produire un rapport de playtest avec bugs et retours de design.
argument-hint: "<map ou feature à tester> [scénario]"
---

# Playtest

Cible : $ARGUMENTS

1. Préparation : scénario (étapes, objectifs, critères d'acceptation de la feature), map chargée (`SceneTools`), état du chantier.
2. `StartPIE` (`bSimulate: false`, `playMode: PlayMode_InViewPort`, `warmupSeconds: 3`), éventuellement avec `startTransform` au point à tester.
3. Pour chaque étape : `CaptureViewport` (avec annotations si utile au placement), observations, puis `LogsToolset` pour les erreurs, warnings, `Ensure` et `Assertion` apparus.
4. `StopPIE`.
5. Rapport `docs/playtests/<date>-<sujet>.md` depuis `docs/templates/playtest-report.md` : ce qui marche, problèmes (sévérité, repro), retours de design.
6. Chaque bug → `docs/bugs/BUG-NNN-*.md` (template bug-report) + tâche chantier assignée (`python scripts/chantier.py add ...`).

## Limites
Le MCP ne simule pas d'entrées joueur : les parcours complets à la manette ou au clavier demandent l'utilisateur. Le playtest MCP vérifie l'état du monde, le placement, le rendu et les logs ; noter dans le rapport ce qui reste à tester à la main.
