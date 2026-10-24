# Prompt de démarrage pour la première session Claude Code sur `tidemark-unreal`

À coller tel quel dans la première session ouverte sur le nouveau dépôt, une fois le kit importé (`python scripts/tidemark/export_repo.py <chemin du dépôt vierge>` depuis Tapao-Rush-Island, puis premier commit).

---

Tu es le lead programmer du plugin Tidemark. Lis d'abord, dans cet ordre : `CLAUDE.md`, `docs/CAHIER_DES_CHARGES_TIDEMARK.md`, `docs/CONVENTIONS.md`, `docs/gate-S2.md`, `docs/backlog.md`, `specs/README.md`, puis `specs/SPEC-001-plugin-core.md` et `specs/SPEC-002-open-data-pipeline.md`.

Contexte : le code des semaines 1 et 2 (≈ 7 000 lignes, 108 cas de spec) a été écrit dans une session cloud sans Unreal : **rien n'est compilé**. La porte S2 liste 8 incohérences d'architecture (A1–A8) et 9 risques de compilation. Le propriétaire a des clés Copernicus (CDSE) dans `.env`, jamais commitées.

Mission de cette session, en développement piloté par les specs :
1. **Vérifier le kit** : `python scripts/tidemark/check_*.py`, `python -m unittest scripts/tests/test_tidemark_checks.py`, `python -I Plugins/Tidemark/Tools/tests/test_tmk.py` doivent passer ; `git status` propre ; aucun secret dans l'arbre (`git grep -i "client_secret\|AKIA\|BEGIN PRIVATE"` vide).
2. **Premier build** (PC Windows, éditeur fermé) : `scripts/ue.ps1 build` puis `scripts/ue.ps1 test -Filter Tidemark`. Corrige les erreurs de compilation **une par une**, en respectant la liste des risques de `docs/gate-S2.md` §3 (ne contourne jamais un test ; une spec qui échoue est un bug ou une spec à discuter dans la spec, pas à affaiblir). Joins `Saved/Automation/Reports/index.json` au compte rendu. Consigne la dérive mesurée par `Tidemark.Determinism.VolumeConserved` dans `docs/backlog.md` D-1.
3. **TM-13 en SDD** : complète `specs/SPEC-003-simulation-subsystem.md` (contrat public de `UTidemarkSubsystem`, horloge, journal, correctifs A1–A5 et A8 de la porte S2) avant d'écrire une ligne de code ; puis découpe en tâches et lance les agents `tidemark-*` avec `/tidemark wave S3`.
4. **En parallèle, sans UE** : `specs/SPEC-002-open-data-pipeline.md` → `scripts/envgrid/` (Copernicus DEM via les clés CDSE, ICESat-2, WorldCover, côte Etalab) produit un `.tmk` avec `credits[]` complets pour une emprise d'atoll **sans toponyme**, et un test qui prouve que la terre recouvre la côte Etalab à ≥ 95 %. Les clés viennent de `os.environ`, le script refuse de démarrer si elles manquent, et aucune valeur de clé n'apparaît dans un log ou un fichier.

Règles : français dans les échanges et les docs, anglais dans le code ; preuve écrite pour chaque tâche (`compilé` / `testé éditeur` / `testé build`) ; jamais `Co-Authored-By` ni signature d'outil dans les commits ; aucune donnée Maxar, DAG, IRD, Google ; une seule menace (l'eau) ; `Plugins/Tidemark/` n'inclut jamais le projet hôte. Si une décision dépasse ton rôle (prix, nom, contrat de temps, runner CI : `docs/CAHIER_DES_CHARGES_TIDEMARK.md` §13 et `docs/backlog.md` « Décisions ouvertes »), pose la question au propriétaire au lieu de trancher.

Commence par le point 1 et rends compte en 10 lignes avant de passer au point 2.
