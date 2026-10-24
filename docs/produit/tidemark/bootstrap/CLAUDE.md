# Tidemark — plugin Unreal Engine 5.8 de montée des eaux déterministe

## Contrat produit
**`docs/CAHIER_DES_CHARGES_TIDEMARK.md`** prime sur tout. Une menace en V1 (l'eau), trois couches (terrain → simulation → lecture), déterminisme CPU seedé et prouvé, zéro donnée géographique embarquée, « plausible, jamais une prévision ». Conventions de code : `docs/CONVENTIONS.md`. Backlog : `docs/backlog.md` (TM-01 → TM-43). Porte S2 : `docs/gate-S2.md` (8 incohérences à corriger en TM-13, 9 risques de build).

## Méthode : développement piloté par les specs (SDD)
Rien ne se code sans spec. Cycle par fonctionnalité, dans `specs/` :
1. **Spec** `specs/SPEC-NNN-<slug>.md` : intention, contrat public (structures, fonctions, fichiers), règles, valeurs tunables (toujours `UTidemarkRules`), critères d'acceptation vérifiables, niveau de preuve exigé.
2. **Plan** : section « Plan » de la spec : découpage en tâches TM-xx, fichiers disjoints, rôle par tâche, dépendances.
3. **Tâches** : une ligne par tâche dans `docs/backlog.md` (ou le chantier si activé), un agent par tâche, fichiers disjoints.
4. **Preuve** : chaque critère a un `It(` dans une Automation Spec `Tidemark.<Feature>.<Cas>` ; une tâche est `fait` seulement avec `compilé` / `testé éditeur` / `testé build` / `testé multi-machines`. « Code écrit » n'est pas « fait ».
Spec vivante : si le code s'écarte de la spec, on met à jour la spec dans le même commit, avec la raison. Les skills `/tidemark*` et les rôles `tidemark-*` suivent ce cycle.

## Arborescence
- `TidemarkHost.uproject`, `Source/TidemarkHost/` : projet hôte minimal (C++ vide) qui ne sert qu'à compiler, tester et packager le plugin. Il n'a aucun gameplay.
- `Plugins/Tidemark/` : le produit. Modules `TidemarkRuntime`, `TidemarkEditor`. **Aucun include du projet hôte ni d'aucun jeu.** Format ouvert `.tmk` (`Plugins/Tidemark/Docs/TMK.md`), outil `Plugins/Tidemark/Tools/tmk.py`.
- `specs/` : specs SDD. `docs/` : cahier, conventions, backlog, portes, benchmark, débat fondateur.
- `scripts/tidemark/` : vérifications CI (`check_no_game_dependency.py`, `check_no_unseeded_random.py`, `check_credits.py`) ; `scripts/envgrid/` : pipeline de données ouvertes (Copernicus, ICESat-2, WorldCover, côte Etalab), script MIT exécuté par l'utilisateur, jamais de donnée dans le plugin.
- `.claude/agents/` : 10 rôles `tidemark-*` ; `.claude/skills/` : 9 skills `tidemark*`.

## Commandes (PC Windows, UE 5.8, éditeur fermé pour un build)
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 build          # UBT TidemarkHostEditor
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 test -Filter Tidemark
python scripts/tidemark/check_no_game_dependency.py ; python scripts/tidemark/check_no_unseeded_random.py ; python scripts/tidemark/check_credits.py
python -m unittest scripts/tests/test_tidemark_checks.py ; python -I Plugins/Tidemark/Tools/tests/test_tmk.py
```
Une session cloud sans UE écrit du code marqué `non compilé` ; le build et les specs se lancent sur le PC et le rapport (`Saved/Automation/Reports/index.json`) est joint au ticket.

## Secrets et données
- Clés Copernicus (CDSE) et toute clé : **dans `.env` (ignoré par Git) ou dans les secrets GitHub, jamais dans un fichier versionné, jamais dans un log.** Modèle : `.env.example`. Le pipeline lit `os.environ`. Un `grep` CI refuse les motifs de clés.
- Sources autorisées : Copernicus DEM (crédit DLR/Airbus + déni), ICESat-2 (domaine public), Sentinel-2, ESA WorldCover (CC BY 4.0), côte Te Fenua (Etalab 2.0, producteur et date). **Interdits** : Google 3D Tiles, imagerie Maxar et dérivés, cocoteraies DAG, raster IRD sans licence, OSM dans une grille redistribuée (ODbL à trancher par un juriste).
- Chaque `.tmk` ou asset livré porte `credits[]` complets ; la CI le vérifie. Lieux habités réels : jamais nommés en V1.

## Git
- Auteur unique : Matthieu Barraque `<matthieu.barraque@outlook.fr>`. **Jamais de `Co-Authored-By`, `Claude-Session` ni « Generated with Claude Code »** dans les commits et PR.
- `main` protégé, reçoit des PR ; branches `tm/TM-xx-<slug>` ; message `tidemark: <quoi> (TM-xx, <preuve>)`.
- Binaires sous Git LFS (`.gitattributes`). Pas de `Binaries/`, `Intermediate/`, `Saved/`, `.env`.

## Conventions C++ (résumé, détail dans `docs/CONVENTIONS.md`)
Style Epic, préfixes `Tidemark`, `TIDEMARKRUNTIME_API`/`TIDEMARKEDITOR_API`, `Category` obligatoire, rien en dur, tick désactivé, `LogTidemark`/`LogTidemarkNet`, specs `Tidemark.<Feature>.<Cas>` en `EditorContext | ProductFilter`. Interdits dans l'état de simulation : `FMath::Rand*`, `FDateTime::Now/UtcNow`, `FPlatformTime::Cycles`, `DeltaTime`. Code et identifiants en anglais, docs en français.
