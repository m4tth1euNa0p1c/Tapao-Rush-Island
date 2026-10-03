---
name: ue-profile
description: Mesure les performances d'une map monke_island (capture CSV profiler en jeu standalone, résumé frame / game / render / GPU) et produit un rapport avant/après. À utiliser avant et après toute optimisation, ou pour vérifier un budget.
argument-hint: "[/Game/chemin/de/la/map] [frames]"
---

# Profiling

Arguments : $ARGUMENTS (défaut : `/Game/FirstPerson/Lvl_FirstPerson`, 1800 frames)

1. Fermer le PIE éventuel (`IsPIERunning` / `StopPIE`). L'éditeur peut rester ouvert, mais il fausse un peu la mesure : le noter.
2. Capturer :
   ```
   powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 profile -Map <map> -Frames <n>
   ```
   Cela lance le jeu en `-game` (non cooké) avec `-csvCaptureFrames=<n> -csvGpuStats -ExitAfterCsvProfiling`, puis résume le CSV (`scripts/perf_summary.py` : moyenne, p50, p95, p99, max en ms ; les 120 premières frames de chargement sont ignorées).
3. Rapport `docs/perf/<date>-<sujet>.md` depuis `docs/templates/perf-report.md` : contexte, chiffres avant/après, captures, conclusion.
4. Budget par défaut : 16,6 ms (60 fps, 1080p) — à confirmer par l'utilisateur.

## Notes
- Mesures en `-game` non cooké : comparatives uniquement. Pour des chiffres de référence, utiliser un build packagé (`scripts/ue.ps1 package`, puis lancer l'exe avec les mêmes flags CSV).
- Même point de vue, même config, même durée pour comparer.
