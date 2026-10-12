# 38 — Pipelines Python, imports Unreal et outillage
Domaine : D · Auditeur : ingénieur outils et pipelines

## Verdict
Atout réel : 103 scripts (26 580 lignes) documentés, graines explicites, règles en JSON, mémoire gelée à identifiants stables, contrôles de fragments testés « à rebours ». Mais la reproductibilité reste orale : ni requirements, ni README de `scripts/`, ni graphe d'étapes, zéro test Python, cinq canaux manuels vers Unreal. Risque commercial : corriger une donnée en bêta ou passer la main à un tiers dépend des agents actuels, sur une seule machine.

## Constats
- **[Majeur] Aucun manifeste d'environnement ni d'ordre d'exécution**
  - Constat : numpy (51 imports), Pillow (27), scipy (12 fichiers), soundfile, sans version. L'ordre des 22 scripts de `map/` n'existe que dans la prose du README Raraka ; l'arborescence de CLAUDE.md omet 5 des 7 sous-dossiers.
  - Preuve : `.claude/agents/ue-gis-cartographer.md:13` ; poste : Python 3.12.10, numpy 1.26.4, scipy 1.15.3, Pillow 12.1.1 (vu).
  - Recommandation : `requirements.txt` épinglé, `scripts/README.md` (graphe, durées), `pipeline.py` (`--only`, `--dry-run`).
  - Effort : M

- **[Majeur] Cinq canaux vers Unreal, aucun pilote commun**
  - Constat : 18 commandlets `-run=pythonscript` (éditeur fermé, alors que l'éditeur partagé reste ouvert), 2 `-ExecutePythonScript`, 2 générateurs de scripts sandbox collés via le contexte LLM, 1 client HTTP direct non réutilisable, 1 test d'automatisation détourné en commande.
  - Preuve : `setup_fragments.py:3-10`, `fragment_villages_ue.py:13`, `VegetationReimportTool.cpp:20`, `ue.ps1:19` (pas de commande `py`).
  - Recommandation : `ue.ps1 py <script>` (garde éditeur, journal, code retour) ; `scripts/lib/mcp.py` partagé.
  - Effort : M

- **[Majeur] Zéro test, zéro CI, contrôles de données hors porte**
  - Constat : 1 `assert` pour 26 580 lignes ; `place_vegetation.py::main` fait 378 lignes. `check_fragments.py` et `build_fragment.py --check` ne sont dans aucune porte (INT : build, `monke_island.*`, PIE).
  - Preuve : `.claude/skills/factory-gate/SKILL.md:24`, `place_vegetation.py:177`.
  - Recommandation : `checks.py` (JSON valides, 20 fragments, ~816 000 instances, dérive JSON ↔ DataAssets) en INT.
  - Effort : M

- **[Majeur] 44 Mo de JSON en git ordinaire, recopié trois fois**
  - Constat : `vegetation_ue.json` échappe à LFS (`.gitattributes` : `*.geojson` seulement) ; 73 JSON = 54,6 Mo. Chaîne : JSON 44 Mo, CSV 125 Mo (+125 `_orig`), DataTables 116 Mo, soit 157 o par instance contre 28 en binaire. Trois tables héritées (50,7 Mo) ne servent plus qu'à des défauts de graphes PCG (supposé). `raw/` ignoré : 2,2 Go dont 908 Mo de copie `avant_pente`.
  - Preuve : blob de 46 113 689 octets ; journal T-068.
  - Recommandation : LFS ou `.json.gz`, binaire à terme, purge après accord.
  - Effort : S

- **[Majeur] Entrées non reproductibles, dont une non redistribuable**
  - Constat : `build_terrain.py` tire hauts-fonds et masque de litière (qui pilote ~348 000 instances de fourré) de l'imagerie Maxar « jamais embarquée » ; maquettes du 2e PC prises sur OneDrive (copie manuelle ou chemin codé en dur) ; 720 Mo de textures retéléchargés d'API vivantes, sans checksum (7 téléchargeurs).
  - Preuve : `build_terrain.py:239-272`, `marae_textures.py:20`, `cutlass_poses.py:14` (chemin absolu).
  - Recommandation : `SOURCES.lock.json` (URL, sha256, licence) ; `sync_maquettes.py` journalisé ; avis juridique (#70/#73).
  - Effort : M

- **[Majeur] Des scripts écrasent ou tronquent ce qu'ils ne possèdent pas**
  - Constat : `fetch_atoll_textures.py` réécrit tout `art/SOURCES.md` (~110 lignes de licences sur 135 perdues) ; `import_loot.py` réimpose 1 200 objets à 20 m, `import_marae.py` des prix de 1 500/120/80 $ (hors DataAsset) ; `fetch_tefenua.py` écrit un GeoJSON tronqué, `fetch_imagery.py` une tuile noire, sortie 0.
  - Preuve : `fetch_atoll_textures.py:126`, `import_loot.py:164-167`, `import_marae.py:360-363`, `fetch_tefenua.py:52-59`, `fetch_imagery.py:60-61`.
  - Recommandation : fusion par balises, n'écrire que ses champs (modèle `set_rowing_crate.py`), sortie non nulle.
  - Effort : S

- **[Majeur] Dette de maintenance, état partagé fragile**
  - Constat : 14 fichiers relisent `georef.json` ; 3 classes de grille ; `smoothstep` et `value_noise` copiés 4 fois ; 6 `importlib`, 12 `sys.path.insert`. `ue_manifest.json` : 3 générateurs, écriture non atomique, 108 chemins `C:/Users/matth/...` commités. Orphelins : `gen_coconut_frond.py`, `gen_leaf_textures.py`. 13 scripts non commités (instantané) ; tout date des 2-4 octobre.
  - Preuve : grep ; `art/export/ue_manifest.json`.
  - Recommandation : `scripts/lib/` (grille, bruit, MCP, manifeste), chemins relatifs.
  - Effort : M

- **[Mineur] Portabilité, outillage qui fuit**
  - Constat : moteur en dur ; `ue.ps1 profile` vise la map template ; addon tiers de 270 Ko sans licence ni crédit, couplé à `uvx blender-mcp` non épinglé ; `ModelContextProtocol` (modules Runtime) non limité à l'Editor, donc supposé livré au build.
  - Preuve : `ue.ps1:27,39`, `worktree.py:28`, `.mcp.json`, `monke_island.uproject`.
  - Recommandation : `UE_ROOT`, versions épinglées, `TargetAllowList: ["Editor"]`.
  - Effort : S

## Manques
- README, requirements, graphe d'étapes exécutable.
- Tests et CI Python, détection de dérive JSON ↔ assets.
- Manifeste d'entrées figé et sauvegarde distante.
- Pilote unique des imports Unreal, utilisable éditeur ouvert.

## Quick wins
1. Corriger `fetch_atoll_textures.py:126` et les codes de sortie des téléchargeurs.
2. `requirements.txt` et `scripts/README.md` (graphe), CLAUDE.md complété.
3. LFS pour `*_ue.json` ; supprimer `avant_pente` après accord.
4. `check_fragments.py raraka` et `build_fragment.py --check` en INT.
5. `TargetAllowList: ["Editor"]` pour `ModelContextProtocol` ; chemins relatifs dans `ue_manifest.json`.

## Questions qui challengent l'équipe
1. Si ce disque meurt (aucun remote git, 3,7 Go de LFS locaux, maquettes sur OneDrive seulement), que reste-t-il de reconstructible, et par qui sans les agents ?
2. Terrain, bathymétrie et fourrés dérivent d'une imagerie Maxar « référence uniquement » : quelle réponse juridique, ou plan de remplacement, avant Steam ?
3. Si deux sessions relancent `place_vegetation.py` puis `place_villages.py --apply-clearing`, quelle version des 40 DataTables est dans le build, et quelle preuve ?
