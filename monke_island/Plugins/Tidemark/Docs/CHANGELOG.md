# Changelog — Tidemark

Format : une entrée par version, de la plus récente à la plus ancienne. Niveau de preuve indiqué comme dans le cahier (`non compilé` / `compilé` / `testé éditeur` / `testé build` / `testé multi-machines`).

## 0.1.0 — squelette
- Plugin `Tidemark` (`FileVersion 3`, `EnabledByDefault false`, `CanContainContent true`).
- Module `TidemarkRuntime` (Runtime, Default) : `LogTidemark`, `LogTidemarkNet`, macro `TIDEMARKRUNTIME_API`.
- Module `TidemarkEditor` (Editor, PostEngineInit) : `LogTidemarkEditor`, macro `TIDEMARKEDITOR_API`.
- Dossiers `Content/`, `Docs/` (README, LICENSES), `Tools/`, `PROVENANCE.json` vide.
- Aucune dépendance au module du jeu ; aucune donnée géographique embarquée.
