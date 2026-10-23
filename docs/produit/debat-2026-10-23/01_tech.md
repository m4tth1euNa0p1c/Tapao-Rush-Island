# 01 — Proposition « tech » : Littoral, un solveur côtier déterministe qui se bake sur n'importe quelle map

Auteur : lead programmer. Lecture seule (`S/` = `monke_island/Source/monke_island/`). Fait observé = cité ; le reste est **estimé** ou **supposé**. `.uasset` non lus (LFS).

## 1. Nom et promesse

**Littoral** — *« Bake ton terrain en grille, la mer monte dessus pareil sur toutes les machines, et l'éditeur te dit où tu vas te noyer, cellule par cellule. »*

Alternatives : **Surcote** · **Hoa** · **Tidemark**.

## 2. Le problème que personne ne résout

Vérifié dans le dépôt :
- Une montée des eaux qui suit le relief existe déjà ici, déterministe par construction : `FFloodGrid` (virtual pipes, aucun UObject, `S/Flood/FloodGrid.h:8-13`), simulée **localement sur chaque machine** depuis une horloge répliquée, seul le centre de grille étant répliqué (`FloodSimulationActor.h:61-62`). C'est la bonne architecture réseau et elle est rare.
- Mais elle est **couplée au jeu** (`HazardClock.cpp` inclut `ExpeditionGameState`), **relit le sol par 16 000 rayons par image** (`FloodRules.h:132-140`), reconnaît abris et palmiers **par nom de mesh** (`WindRules.h:120-124`) et n'a **aucune preuve de synchronisation** entre deux PC (audit 01 §6 : zéro test réseau ou multi-machines).

Supposé (non vérifié sur Fab, à confirmer avant toute page produit) : les plugins d'eau vendus pour UE (Fluid Flux, FluidNinja, le plugin Water d'Epic) sont des **simulations GPU orientées rendu**, non déterministes d'un GPU à l'autre, donc inutilisables comme état faisant foi en multijoueur sans répliquer la grille. Aucun ne fournit, à ma connaissance, un « lint » de niveau ni un format de grille lisible hors Unreal.

Le trou : **une submersion qui est un état de jeu (autoritaire, rejouable, interrogeable), pas un effet**. Les studios coop/survie la recodent chacun dans leur coin ; les serious games ne peuvent l'acheter qu'en bureau d'études.

## 3. Clients

**Client n°1 (volume, Fab)** : le studio UE de 2 à 15 personnes qui fait un jeu coop ou survie avec de l'eau qui monte (marée, crue, île qui coule) et qui a besoin que **l'eau soit la même chez l'hôte et les clients** sans payer un programmeur réseau six mois. Il achète un plugin, pas une thèse.

**Client n°2 (valeur, B2B)** : le studio de serious game ou le bureau d'études qui sensibilise une collectivité au risque littoral et veut un scénario **plausible, documenté, rejouable** sur son propre terrain, avec sources et règles listées. Il achète la traçabilité.

La V1 est faite pour le n°1 ; les champs « traçabilité » du format servent le n°2 plus tard.

## 4. L'idée unique

**Le terrain devient une grille bakée, et la grille est le seul monde que le solveur connaît.** Trois conséquences qui, ensemble, n'existent nulle part (supposé) :

1. **Terrain-agnostique par construction.** Landscape, heightmap, meshes Nanite, ProceduralMesh : tout passe par un *bake* éditeur vers une `FEnvGrid` (altitude, pente, distance à la mer, sol, rugosité, exposition, drapeaux). Le runtime ne trace jamais. Le plugin ne dépend d'aucune représentation du terrain.
2. **Déterminisme prouvé, pas promis.** L'état faisant foi est en **entiers** (millimètres, litres), pas en flottants ; il est hashé chaque seconde ; une expédition se rejoue depuis `(seed, grille, journal d'éditions)`. La CI compile et rejoue 600 s de simulation sur deux binaires et compare le hash.
3. **Le linter lit la même grille que le jeu.** « Ton camp est sous +1,5 m » est la cellule 412×208, la règle `Rule.Submersion.Static@v3` et le niveau qui l'a noyée. Le MCP met ces lignes en phrases, rien d'autre.

Défendable 12 mois parce que le dur est l'exécution : solveur entier stable, bake sur trois types de terrain, rejeu en CI, format spécifié. Un concurrent GPU devrait réécrire son cœur ; un outil d'analyse devrait écrire un solveur runtime multi.

## 5. Périmètre

**V1 (Fab)** : plugin `Littoral` ; bake Landscape + heightmap + meshes ; un aléa, la **submersion** (niveau de mer fonction du temps : marée, surcote, scénario) ; grille de descripteurs ; linter éditeur (inondabilité, ordre de submersion, coupure des chemins entre points nommés) ; réplication (seed, horloge, journal) et hash de contrôle ; pont de rendu ; MCP gratuit à 4 outils ; **un** pack de démonstration « atoll inspiré des Tuamotu » en sources libres.

**V2** : vent et exposition (automate 64²), houle au bord du récif, érosion/ensablement avec « hoa qui s'ouvrent et se ferment », éditions de descripteurs par le gameplay (couper, remblayer), packs supplémentaires, export GeoTIFF, licence B2B avec rapport de traçabilité.

**Jamais** : prévision réelle, rejeu de catastrophes nommées, données personnelles, Google 3D Tiles, imagerie Maxar, cocoteraies DAG, volcans/feux, un éditeur de jeu complet, un solveur GPU faisant foi.

## 6. Architecture technique

### 6.1 Modules (plugin `Littoral`, 3 modules + 1 serveur)

| Module | Type | Dépend de | Rôle |
|---|---|---|---|
| `LittoralCore` | Runtime, **Core seulement** (pas Engine) | — | types, grille, solveurs, règles pures, hash, format. Compilable en Program (`LittoralCli`), exécuté par la CI et le MCP |
| `LittoralRuntime` | Runtime | Core, Engine, NetCore | subsystems, DataAssets, réplication, pont de rendu, requêtes gameplay |
| `LittoralEditor` | Editor | Runtime, UnrealEd, Landscape, MeshDescription | bake, linter, overlay, importeur `.lgrid` |
| `littoral-mcp` | Python, dépôt séparé | `LittoralCli` ou commandlet | 4 outils MCP, aucun calcul propre |

Dépendance vers le bas seulement ; `LittoralCore` n'inclut jamais `Engine/`. Tapao devient un client : `S/Flood/` et `S/Storm/` passent en adaptateurs (`ULittoralTapaoBridge`) sans include de `ExpeditionGameState`.

### 6.2 Classes

**Core (sans UObject)**
- `FEnvGrid` : SoA, origine (double UTM ou monde), pas en cm, `TArray<int16> ElevationCm`, `uint8 Slope`, `uint16 DistToSeaM`, `uint8 Soil`, `uint8 VegCover`, `uint8 Roughness` (Manning ×1000), `uint8 WindExposure[8]`, `uint8 Flags`. Même schéma que la proposition 02 §B.2, adopté tel quel.
- `FShallowWaterSolverQ` : `FFloodGrid` **réécrit en point fixe** : `int32 GroundMm`, `int32 DepthMm`, `int64 FluxL[4]`, `uint8 Fixed`. Même modèle de tuyaux et même double passe parallèle par lignes (`FloodGrid.cpp:34-85` ; chaque cellule ne lit que l'étape précédente, l'ordre des threads n'influe pas). Les flottants marchent sur un même binaire x64 ; rien ne le garantit entre deux compilations ou avec FMA. L'entier le garantit.
- `FLittoralSeed` : seed 64 bits, `Derive(FName)` par SplitMix64 → sous-flux nommés (`Hazard.Surge`, `Hazard.Wind`). Remplace `FMath::Rand()`, `UtcNow`, `Cycles()` (audit 01 §3.4).
- `FSeaLevelSchedule` : fonction pure `LevelMm(StepIndex)` composée de termes (constante, rampe à exposant comme `ExpeditionClock.cpp:101-116`, marée harmonique, surcote gaussienne). Jamais d'état.
- `FHazardRule` (struct de données) + `FHazardEligibility::Evaluate(const FEnvGrid&, const FHazardRule&, FLittoralSeed) -> FExposureMap` : pure, testable.
- `FStateHash` : FNV-1a 64 sur `DepthMm` + `StepIndex`, par bloc 32×32 puis combinée (on sait *où* ça diverge).
- `FDescriptorEdit` : `{FIntRect Cells, ELayer, EOp, int16 Value, FName Cause, uint32 ServerStep}`.
- `FLittoralReplay` : `(seed, hash de grille, éditions)` → rejoue N pas, rend le hash final.

**Runtime**
- `ULittoralGridAsset : UPrimaryDataAsset` : porte `FEnvGrid`, version du schéma, hash SHA-256 des sources, `CreditsJson`. Chargé par l'AssetManager (mécanisme déjà utilisé : `DefaultGame.ini:16-17`).
- `ULittoralRules : UDataAsset` : pas de temps (0,05 s), max pas/image (4), gravité, amortissement, `OpenSeaDepth`, taille de grille de simulation (512² à 6 m par défaut, repli 320² à 8 m), échéancier de niveau, seuils de hash. Rien en dur.
- `ULittoralSimulationSubsystem : UTickableWorldSubsystem` : possède le solveur, compte ses pas **depuis l'horloge serveur répliquée**, pas depuis `DeltaTime` (comme `FloodSimulationActor.cpp:549-558`), applique le journal au pas serveur exact, expose `SampleWater(FVector)`, `GetStepIndex`, `GetHash`.
- `ULittoralClockComponent` (sur le GameState du client) : `FLittoralClockState` répliquée `{StartServerTime, Seed, ScheduleId, bStarted}`. 32 octets, une fois.
- `ULittoralEditJournalComponent` : `FFastArraySerializer` d'`FDescriptorEdit`, serveur → clients, ≤ 1 ko/s **estimé**.
- `ULittoralSyncComponent` : hash toutes les 1 s client → serveur (RPC non fiable, 16 o) ; après 3 écarts consécutifs, instantané compressé (`DepthMm` en delta RLE, ≈ 50 ko **estimé** pour 512²), log `LogLittoralNet`. Un filet, pas le mode normal.
- `ULittoralRenderBridge` : écrit à 5 Hz une `UTextureRenderTarget2D` R16 (profondeur) + un `UMaterialParameterCollection` ; livre `MF_Littoral_WaterDepth` et un Niagara Data Interface. Le rendu de l'eau reste au client (Water plugin ou mesh procédural ; `AFloodSimulationActor` devient rendu de référence, vidé de sa simulation).
- `ILittoralWaterProvider` : ce que `UIslandWaterSubsystem::SetSurfaceProvider` (`IslandWaterSubsystem.h:60-63`) consomme déjà.

**Editor**
- `ILittoralHeightSource` : `SampleHeightCm(x, y)` + `Bounds()`. Trois implémentations : `FLandscapeHeightSource` (lecture directe des `ULandscapeComponent` via `FLandscapeEditDataInterface`), `FHeightmapTextureSource` (R16, `.r16`, `.png`), `FCollisionTraceSource` (meshes Nanite/PMC : traces asynchrones par lots, **une fois à l'édition**, ce que `ReadGroundBatch` refait aujourd'hui à chaque chargement).
- `ULittoralBakeSettings : UDeveloperSettings` + `FLittoralBaker` : sources → `FEnvGrid` ; pente, distance à la mer, exposition par horizon 8 secteurs ; sols et végétation par **Landscape layers, Physical Materials ou tags d'acteurs**, jamais par nom de mesh.
- `FLittoralLinter` : règles nommées et versionnées rendant `FLintFinding {RuleId, Severity, Cells, LevelMm, Message}`. V1 : `Submersible(point, level)`, `FloodOrder(points)`, `EvacuationCut(A, B, level)` (chemin 8-connexe sur cellules sèches), `GridStale` (hash sources ≠ hash asset).
- Console : `littoral.Overlay`, `littoral.Lint`, `littoral.Replay <seed>`.

### 6.3 Format de données ouvert : `.lgrid`

Un dossier (ou zip) : `manifest.json` (schéma v1, origine, CRS, pas, dimensions, couches et types, SHA-256 par fichier, `credits[]` avec producteur, licence, URL, date — exigence Etalab relevée par l'audit 70), une `.bin` little-endian par couche, `edits.jsonl` optionnel. Spécification publique, lecteur Python livré avec le MCP, export GeoTIFF en V2. Import et export par `LittoralEditor`. **Unreal n'est pas nécessaire pour lire ou produire une grille** : le pipeline `scripts/envgrid/` (proposition 02) et le MCP en sont indépendants.

### 6.4 MCP qui n'invente rien

`littoral-mcp` expose quatre outils ; chacun **exécute `LittoralCli`** (ou le commandlet `-run=Littoral` si la map est nécessaire) et renvoie sa sortie JSON, jamais une estimation du modèle de langage :

| Outil | Entrée | Sortie | Calcul |
|---|---|---|---|
| `analyze_level` | chemin `.lgrid` ou map | stats des couches, histogramme d'altitude, zones ≤ seuils | `LittoralCli analyze` |
| `explain` | point (x, y) ou cellule, niveau | règles déclenchées, cellule, voisins, niveau de submersion, ordre | `LittoralCli explain` |
| `lint_level` | map, points nommés, niveaux | liste de `FLintFinding` | `LittoralCli lint` |
| `propose_schedule` | contraintes (durée, pic, type) | 1 à 3 `FSeaLevelSchedule` **validés par rejeu** avec hash | `LittoralCli schedule --validate` |

Chaque réponse porte `rule_id@version`, `grid_hash`, `seed`. Sans donnée, l'outil répond « inconnu », pas une phrase plausible. Le quatrième s'appelle `propose_`, pas `design_` : il énumère, l'humain choisit.

### 6.5 Performance (budgets, à mesurer en S3)

Référence : `docs/conventions/performance.md:16-30` (60 FPS, thread de jeu ≤ 8 ms).
- Solveur 512² à 20 pas/s : ≈ 5,2 M cellules/s ; **estimé** 0,5–1,0 ms/pas sur 6 workers (additions et min entiers : pas plus lent que le flottant).
- Grille de descripteurs 4 m, 1,2 M cellules : ≈ 14 Mo, chargée une fois. Simulation 512² : ≈ 10 Mo.
- Pont de rendu : ≤ 0,3 ms (copie 512² R16 à 5 Hz).
- Réseau : 32 o à l'init, journal ≤ 1 ko/s, hash 16 o/s par client.
- Pas de GPU faisant foi ; un raffinement visuel GPU (écume) reste possible en V2, non autoritaire.

## 7. Démo de 30 secondes

Écran partagé hôte/client. 0–5 s : un Landscape quelconque du marketplace, bouton **Bake**, la grille apparaît en overlay. 5–12 s : **Lint** : « Camp B submersible à +1,2 m », « Route A→B coupée en premier à +0,8 m », cellules surlignées. 12–25 s : Play, « surcote 2 m en 90 s » : l'eau remplit les creux, franchit la crête, coupe la route **dans l'ordre annoncé**, sur les deux écrans, HUD « hash 0x8F3A… = 0x8F3A… ». 25–30 s : même seed, image identique ; dans Claude : « pourquoi le camp B ? » → la cellule, la règle, le niveau.

## 8. Modèle économique (tout estimé)

- MCP et spécification `.lgrid` : gratuits, licence MIT (acquisition, standard de fait).
- Plugin `Littoral` sur Fab : 179 € par siège, sans redevance ; mises à jour V1.x incluses.
- Packs de lieux « inspirés de » : 29–49 € ; un seul en V1 (vitrine), pas de catalogue avant la preuve de vente du plugin.
- Licence B2B (source, rapport de traçabilité, support) : 6–15 k€/an par organisation.
- Plancher : 60 ventes la première année ≈ 10 k€ bruts (≈ 88 % net Fab). Ne couvre pas le temps de dev : le plugin vit si Tapao en est le client et la vitrine.

## 9. Licences, éthique, données (non négociables)

1. Aucun dérivé Maxar, DAG ni raster IRD sans licence dans quoi que ce soit de vendu (audit 70). Sources V1 : Copernicus DEM, ICESat-2, Sentinel-2, WorldCover, OSM (ODbL), côte Te Fenua Etalab (producteur + date).
2. `credits[]` du manifest obligatoire, vérifié par la CI, affiché par le plugin.
3. Chaque sortie porte « plausible, pas une prévision » ; aucun événement réel avec victimes ; aucun nom de propriétaire ni parcelle en V1.
4. Lieux réels : pack nommé par archipel, jamais par village ; jamais 1:1 habité par défaut.
5. Pas de rites ni de légendes réels ; les « signes naturels » sont physiques.
6. Plugin sans dépendance à Tapao, sans plugin payant, licence écrite avant diffusion (ceci n'est pas un avis juridique).

## 10. Plan en 8 semaines

| Sem. | Livrable | Critère de fin vérifiable |
|---|---|---|
| 1 | `LittoralCore` : `FEnvGrid`, `FShallowWaterSolverQ`, `FLittoralSeed`, `FStateHash`, `LittoralCli` | Spec `Littoral.Core.Determinism` : 2 exécutions de 600 s sur 2 builds (Debug et Shipping) → hash identique ; volume conservé à ±0 litre. `compilé`, `testé hors moteur` |
| 2 | Format `.lgrid` + lecteur Python + `scripts/envgrid/` sans Maxar/DAG | Grille FRG_Raraka_01 produite ; terre recouvre la côte Te Fenua à ≥ 95 % ; `credits[]` complet ; CI rejette un manifest sans licence |
| 3 | `LittoralEditor` : bake Landscape et heightmap ; `LittoralRuntime` subsystem + rules | Bake d'un Landscape 2 km en < 60 s **mesuré** ; même scénario, grille Landscape vs heightmap identique → même hash ; coût solveur ≤ 1 ms moyen (Insights) |
| 4 | Bake par traces (Nanite/PMC) ; Tapao branché par `ULittoralTapaoBridge`, traces runtime supprimées | `L_Frag_Raraka_01` inonde comme avant (hash de référence figé) ; `S/Flood/` n'inclut plus `ExpeditionGameState` ; 0 `TracesPerFrame` |
| 5 | Réplication : clock, journal, `ULittoralSyncComponent` | Hôte + client **sur deux PC**, 3 scénarios de 10 min, 0 divergence de hash ; une divergence injectée est corrigée en < 2 s. `testé multi-machines` |
| 6 | Linter 4 règles + overlay + pont de rendu | Sur 3 maps (Landscape marketplace, heightmap, Raraka), chaque `FLintFinding` est vérifié par rejeu : 100 % des « submersible à N » se noient à N |
| 7 | `littoral-mcp` 4 outils | 20 questions-tests : 0 réponse sans `rule_id` et `grid_hash` ; question hors donnée → « inconnu » |
| 8 | Pack démo, doc Fab, vidéo 30 s, package Development | Un dev externe installe le plugin sur son Landscape et obtient un lint en < 30 min (chronométré) ; checklist Fab passée |

## 11. Cinq raisons d'échouer

1. **Le point fixe fait des artefacts** (oscillations, assèchements) : garder l'amortissement et la limite « jamais plus que ce que la cellule contient » (`FloodGrid.cpp:57-61`) ; comparer au solveur flottant sur 50 scénarios en S1 ; tolérance en litres documentée.
2. **Le bake par traces est lent ou faux sur des meshes complexes** (surplombs, grottes) : canal de collision dédié, « plus haut sol sous le niveau max » ; budget mesuré en S3, sinon bake incrémental par tuiles.
3. **Le relief d'atoll est sous le bruit des DEM** (≈ 3 m pour 2–4 m d'erreur) : le pack démo est « inspiré », calé ICESat-2, avec la mention ; le plugin ne dépend pas des données réelles.
4. **Personne n'achète un solveur sans belle eau** : le pont de rendu alimente le Water plugin et des matériaux fournis ; vidéo faite sur un rendu soigné, pas sur l'overlay.
5. **Dispersion avec Tapao** (147 tickets en review, aucun lot validé) : le plugin a son dépôt, Tapao le consomme à S4 ; si S5 échoue (désync), on arrête et on garde le solveur en interne.

## 12. Ce que je rejette de la direction retenue

1. **« Le moteur déduit les aléas plausibles du climat et de l'exposition au vent »** : un heightmap ne contient pas de climat. L'exposition se calcule (horizon), le climat **s'importe** (preset ERA5/IBTrACS par région, DataAsset). Le plugin déduit la géométrie, l'utilisateur déclare le forçage ; tout le reste finit en hallucination.
2. **Six couches à parts égales** : l'« éligibilité » (3) est un pré-calcul éditeur (le linter), la « simulation » (4) est le runtime. Je garde une pile à quatre : bake → grille → solveur → consommateurs (rendu, gameplay, lint). Moins de contrats, moins de doc.
3. **`design_scenario` au MCP** : « l'IA configure, elle ne décide pas » contredit un outil qui *design*. Remplacé par `propose_schedule`, qui rend des paramètres validés par rejeu, numérotés, que l'humain choisit.
4. **Les packs de lieux comme pilier de V1** : à l'échelle d'un atoll, les sources libres donnent une fausse précision (02 §B.8-1) et le travail juridique par pack est réel (audit 70). Un pack vitrine ; le produit, c'est le plugin.
5. **Passes qui s'ouvrent et se ferment, érosion, en V1** : exige une topographie modifiable et persistée, qui n'existe pas (Nanite, `FloodRules.h:15-16`). V2, par éditions de descripteurs, une fois le déterminisme prouvé.
