---
name: ue-test
description: Exécute les tests d'automatisation Unreal de monke_island (tout ou un filtre) via le MCP AutomationTestToolset si l'éditeur est ouvert, sinon en headless, et rapporte les résultats.
argument-hint: [filtre, ex monke_island.Combat]
---

# Tests monke_island

Filtre : `$ARGUMENTS` (défaut `monke_island`)

1. `scripts/ue.ps1 status`.
2. **Éditeur ouvert + MCP OK** → `describe_toolset` sur le toolset AutomationTest, puis `call_tool` pour lancer les tests correspondant au filtre et lire les résultats.
3. **Éditeur fermé** → `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 test -Filter <filtre>` (timeout long : 10 min). Rapport JSON : `monke_island/Saved/Automation/Reports/index.json`.
4. Rapporte : total / passés / échoués, et pour chaque échec le nom du test + message exact.
5. Échec → ne modifie pas le test pour le faire passer ; diagnostique le code testé (skill `ue-bugfix`).
