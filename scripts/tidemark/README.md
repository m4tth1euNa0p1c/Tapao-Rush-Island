# scripts/tidemark — vérifications CI du plugin Tidemark

Trois scripts Python sans dépendance (3.12+), exécutés par `.github/workflows/tidemark.yml` sur chaque PR et push `main`/`ga` touchant le plugin. Exit 0 = OK, exit 1 = au moins une violation, chaque ligne `fichier:ligne: message`. Option `--root <dépôt>` pour cibler une autre racine (fixtures des tests).

| Script | Vérifie | Convention |
|---|---|---|
| `check_no_game_dependency.py` | aucun `monke_island`, `/Game/MonkeIsland`, `Logmonke_island` dans `Plugins/Tidemark/Source` ; aucun `Build.cs` ne liste le module `monke_island` | CONVENTIONS §1 |
| `check_no_unseeded_random.py` | aucun `FMath::Rand/FRand/RandRange/FRandRange/VRand/RandBool`, `FRandomStream(` sans `FTidemarkSeed`/`ToRandomStream`, `FDateTime::Now/UtcNow`, `FPlatformTime::Cycles`, `GetTimeSeconds()` hors dossiers `Tests/`. Exception explicite par ligne : `// tidemark-allow-random: <raison>` (réservée au cosmétique jamais écrit dans l'état de simulation) | CONVENTIONS §4–§5 |
| `check_credits.py` | chaque `manifest.json` (en-tête `.tmk` déplié) a des `credits[]` complets (`producer`, `license`, `url`, `date`) ; chaque `PROVENANCE.json` a des `assets[]` complets (`path`, `tool`, `date`, `license`, `sha256` 64 hex) ; aucune chaîne `Maxar`, `DigitalGlobe`, `cocoteraie`, `3D Tiles` dans `Content/` et `Docs/` (hors `Docs/LICENSES`) | CONVENTIONS §7 |

```bash
python3 scripts/tidemark/check_no_game_dependency.py
python3 scripts/tidemark/check_no_unseeded_random.py
python3 scripts/tidemark/check_credits.py
python3 -m unittest scripts/tests/test_tidemark_checks.py -v   # fixtures temporaires, un cas qui passe et un qui échoue par règle
```

Ces greps ne remplacent pas la compilation ni les specs `Tidemark.*` : la preuve `compilé` / `testé éditeur` reste `scripts/ue.ps1 build` puis `scripts/ue.ps1 test -Filter Tidemark` sur un poste Windows avec UE 5.8, rapport `monke_island/Saved/Automation/Reports/index.json` joint au ticket, tant qu'aucun runner Windows auto-hébergé n'existe (job `build-windows` du workflow, désactivé).
