# Cahier des charges — TIDEMARK (nom de travail)

- **Version** : 0.1, 2026-10-23. **Statut** : proposition issue d'un débat contradictoire entre six agents (quatre rédacteurs, deux contradicteurs), arbitrée par la session principale. Pièces du débat : `docs/produit/debat-2026-10-23/`.
- **Ce document n'engage rien** tant que le propriétaire n'a pas tranché les « Questions ouvertes » (§13). Il ne remplace pas `docs/CAHIER_DES_CHARGES.md`, qui reste le contrat du jeu Tapao Rush Island.
- **Convention** : 🔎 fait observé (source) · ⚠ estimé · ✖ supposé. `S/` = `monke_island/Source/monke_island/`.

---

## 0. Ce que le débat a tranché

| Désaccord | Positions en présence | Arbitrage |
|---|---|---|
| Client n°1 | studio UE coop 2–15 (tech, design) · bureau d'études littoral B2B (marché) · dev solo/duo coop (devil) | **Le dev ou petit studio UE (1 à 8 personnes) qui fait un jeu coop ou survie avec une île ou une côte.** Le B2B ne reçoit aucune ligne de code en V1 ; le format ouvert et la trace par cellule le rendent atteignable en V2 |
| Multijoueur | V1 (tech, design, devil) · V2 (marché) | **V1, et c'est la porte de la semaine 4** : sans preuve sur deux PC, le produit n'est qu'un plan d'eau qui monte en Z |
| Packs de lieux réels | pilier (direction initiale) · vitrine unique (tech, design) · jamais (devil, marché) | **Aucun pack vendu en V1.** Une île de démo « inspirée d'un atoll », gratuite, bâtie sur des sources propres, relief marqué estimé, toponymes fictifs. Lieux nommés : V2, seulement là où un LiDAR sous licence ouverte existe, après avis juridique |
| MCP | accroche (direction initiale) · outil gratuit secondaire (tech, design) · supprimé (devil, marché) | **Hors chemin critique.** Un serveur gratuit et mince qui exécute le CLI du plugin, 3 outils, livré en V1.1 si et seulement si la V1 est publiée. L'accroche est visuelle et sans IA |
| Menaces en V1 | une (tous) · six (direction initiale) | **Une seule : l'eau qui monte.** Vent, érosion, passes persistantes, cyclone : V2 |
| Solveur | réécriture en point fixe (tech) · déplacement tel quel (devil, marché) | **Déplacement de `FFloodGrid` sans réécriture**, avec ses specs. Le client livre un seul binaire à ses joueurs ; le déterminisme inter-compilations est un problème de V2, à traiter en point fixe seulement s'il se manifeste |
| Prix | 59 $ (devil) · 99–149 € (design) · 149 $ (marché) · 179 € (tech) | ⚠ **89 $ au lancement**, à tester par les entretiens ; aucun chiffre n'est sourcé, tous sont des paris |
| Entretiens clients | aucun avant S8 (tech, design) · S6 (marché) · S1 (devil) | **8 entretiens de devs UE coop en S1–S2, go/no-go écrit à la fin de S2** |

Convergences sans débat : bake unique du terrain à l'édition (fin des 16 000 rayons par image), seed unique, déterminisme CPU, somme de contrôle par bloc, `credits[]` vérifié par la CI, « le climat s'importe, la géométrie se déduit », `propose_` plutôt que `design_`, purge Maxar/DAG avant tout branchement, une capture publiable par semaine dès la S4.

---

## 1. Nom et promesse

**TIDEMARK** — *« Une montée des eaux qui lit ton terrain, identique sur toutes les machines, et qui te dit dans l'éditeur ce qui coule en premier. »*

Noms alternatifs : **Highwater**, **Littoral**, **Hoa** (terme pa'umotu : à valider culturellement et juridiquement avant tout usage). Vérifier INPI, EUIPO et Fab avant le logo : l'audit 78 a perdu « Crownfall » ainsi.

Trois verbes : **lire** (où et quand ça inonde), **jouer** (la même marée chez l'hôte et les clients), **changer** (creuser, remblayer, rehausser déplace l'eau).

## 2. Le problème que personne ne résout

- 🔎 Dans le dépôt, la montée des eaux est un chronomètre identique sur toutes les îles ; le terrain n'intervient qu'en aval (`S/Expedition/Core/ExpeditionClock.cpp:22-25`). Le solveur d'eaux peu profondes existe, pur, CPU, déterministe, et chaque machine recalcule la même eau à partir d'une horloge répliquée (`S/Flood/FloodGrid.h:4-10`, `FloodSimulationActor.cpp:163-169,401-424`). Mais il relit le sol par 16 000 rayons par image (`FloodRules.h:132-140`), dépend du GameState du jeu (`HazardClock.cpp:4`) et n'a jamais été prouvé sur deux PC (🔎 `docs/production/board.md` : 0 ticket « testé multi-machines »).
- ✖ Les plugins d'eau vendus pour UE (Fluid Flux, Oceanology, Water d'Epic) sont des simulations GPU orientées rendu : résultat différent d'un GPU à l'autre, pas d'état faisant foi, pas de lecture gameplay bon marché. À vérifier en S1 par un banc d'essai.
- 🔎 Trois jeux Steam récents font « l'île coule » avec un plan qui monte uniformément (`docs/audit/2026-10-04-commercial/81-positioning-usp.md`) : aucun n'explique pourquoi l'eau arrive ici d'abord, aucun ne laisse le joueur changer la réponse.

**Le trou** : une submersion qui est **un état de jeu** (autoritaire, rejouable, interrogeable, identique en multijoueur), pas un effet visuel. ✖ Supposé : qu'un studio paie pour ne pas la recoder. C'est ce que les entretiens de S1–S2 doivent établir.

## 3. Clients

**Client n°1 (le seul qui compte en V1)** : le dev solo ou le studio de 1 à 8 personnes sur UE 5.4+ qui fait un jeu coop ou survie (extraction, « friendslop », île, côte, crue) et qui veut une menace d'eau **honnête** : elle suit le relief, elle est la même chez l'hôte et les clients, elle se lit avant d'arriver. Il achète sur Fab, juge en 30 s de vidéo, teste en 15 min, demande remboursement si ça ne marche pas sur son Landscape.

**Client n°2 (V2, zéro code en V1)** : studio de serious game ou médiateur scientifique (musée, lycée, association littorale, bureau d'études) qui veut « si la mer monte de 1 m, où passe-t-elle en premier » sur un terrain inventé ou anonymisé, avec la trace « pourquoi cette cellule ». Il est servi plus tard par le format ouvert et le rapport, pas par un développement dédié.

**Tapao Rush Island** : premier utilisateur. Pas « la vitrine » tant qu'il n'a pas un build prouvé à deux PC (🔎 288 tickets `fait`, dont 93 fermés « sans recette dédiée », `board.md`).

## 4. L'idée unique : la « first-wet map »

Pour toute grille de hauteurs, Tidemark calcule **l'ordre et l'instant d'arrivée de l'eau en chaque cellule**, de façon déterministe et seedée, et l'expose trois fois :

1. **au level designer** : un calque dans l'éditeur, un curseur de niveau de mer (+0 → +3 m) qui inonde la map dans l'ordre, et un lint : « Refuge A : 6:20 », « Pont : coupé à 3:40 », « Spawn : sous l'eau à 2:10 » ;
2. **au gameplay**, en Blueprint : `TimeToFlood(point)`, `FloodOrder(point)`, `IsPassable(A, B, t)`, `SampleWater(point)`, et des éditions serveur `Dig`, `Fill`, `Raise` qui déplacent l'eau ;
3. **au réseau** : seed + horloge + journal d'éditions → même eau partout, somme de contrôle par bloc, instantané correctif.

Ce qui est défendable 12 mois n'est pas l'algorithme (tuyaux virtuels, 2007) mais **l'intégration prouvée** : specs de déterminisme en CI, réplication par seed, budget CPU mesuré, trois sources de terrain, API stable. ✖ Un concurrent GPU copiera le rendu avant le déterminisme réseau.

Ce que l'idée **n'est pas** : une reconstitution de lieu réel, une prévision, un moteur d'IA, un éditeur de jeu.

## 5. Périmètre

| V1 (8 semaines) | V1.1 (si V1 publiée) | V2 (si ≥ 150 ventes ou 3 studios utilisateurs, ⚠) | Jamais |
|---|---|---|---|
| Plugin UE `Tidemark`, modules runtime + éditeur, sans dépendance à Tapao | MCP gratuit : `analyze_level`, `explain`, `lint_level`, exécutant le CLI | Rugosité par cellule et végétation : « couper le rivage fait monter l'eau plus vite » (mécanique 5/5 du doc 02) | Prévision, document réglementaire, aide à la décision |
| Entrée : Landscape, heightmap PNG/R16/EXR, meshes de collision (bake par rayons **une fois** à l'import) | Export GeoTIFF de la grille | Surcote et vent seedés par preset de région (forçage importé en DataAsset, jamais déduit) | Packs de lieux habités réels nommés sans LiDAR ouvert ni avis juridique |
| Solveur CPU déterministe (`FFloodGrid` déplacé), seed unique, horloge, journal, checksum, snapshot | Script MIT `heightmap_from_open_data.py` : l'utilisateur assemble Copernicus, ICESat-2, côte Etalab sous sa responsabilité, avec `CREDITS.json` | Érosion et ensablement persistants, passes qui s'ouvrent et se ferment | Cadastre, bâti nominatif, données personnelles |
| Calque first-wet, curseur de niveau de mer, 4 règles de lint | | Générateur d'atoll **fictif** paramétrique (crête, platier, motu, hoa, lagon) | Google 3D Tiles, imagerie Maxar, cocoteraies DAG, raster IRD sans licence |
| Éditions serveur `Dig`, `Fill`, `Raise` (altitude), répliquées en journal | | Pack « inspiré de » nommé par archipel, là où Litto3D Etalab couvre (Société) | Volcans, feux, avalanches |
| Pont de rendu : texture R16 + MPC, matériau d'exemple, Niagara Data Interface | | Rapport PDF sourcé pour le client n°2 | Rejeu de catastrophes réelles avec victimes |
| Map de démo fictive + île de démo « inspirée d'un atoll » gratuite, docs, vidéo 30 s | | | « Crée ton jeu complet » |

## 6. Architecture technique

Trois couches, pas six. Tout vient du dépôt ou de la proposition `docs/audit/02_ARCHITECTURE_PROPOSAL.md`, réduite.

### 6.1 Modules
- `Plugins/Tidemark/` dans son propre dépôt Git ; modules `TidemarkRuntime` (Core, Engine, NetCore) et `TidemarkEditor` (UnrealEd, Landscape en lecture, MeshDescription). Un commandlet `-run=Tidemark` sert de CLI pour la CI, le lint hors éditeur et, plus tard, le MCP. **Aucune dépendance vers `monke_island`** : c'est Tapao qui dépend du plugin (`docs/conventions/architecture.md`, dépendances vers le bas).
- Rejeté : le troisième module « Core sans Engine compilable en Program » et le dépôt Python séparé (proposition tech). Une personne, huit semaines : on sépare quand un utilisateur le demande.

### 6.2 Couche A — Terrain
- `FTidemarkGrid` : SoA, origine (monde, et UTM optionnel), pas en cm, `TArray<int16> ElevationCm`, `uint8 Flags` (mer ouverte, bord, interdit). En V2 : `Roughness`, `VegCover`, `Soil`, `WindExposure[8]` (schéma complet dans le doc 02 §B.2).
- `ITidemarkHeightSource` : `SampleHeightCm(x, y)` + `Bounds()`. Trois implémentations éditeur : `FromLandscape` (lecture des `ULandscapeComponent`), `FromTexture` (R16, PNG, EXR), `FromCollision` (rayons par lots, **à l'import**, canal de collision dédié, « plus haut sol sous le niveau max »).
- `UTidemarkTerrainAsset : UPrimaryDataAsset` : la grille, la version du schéma, le SHA-256 des sources, `Credits` (producteur, licence, URL, date). Chargé par l'AssetManager, mécanisme que le dépôt utilise déjà (🔎 `monke_island/Config/DefaultGame.ini:16-17`).
- Sols et abris : jamais par nom de mesh (🔎 `S/Storm/WindRules.h:120-124`, `S/Hazards/CoconutFallDirector.cpp:53-59`), mais par Landscape layers, Physical Materials ou tags d'acteurs (V2).

### 6.3 Couche B — Simulation
- `FTidemarkSolver` : `FFloodGrid` **déplacé avec ses specs** (`S/Flood/Tests/`), même modèle de tuyaux, même double passe par lignes (🔎 `FloodGrid.cpp:46-87`). Vérifier en S2 qu'aucune réduction `ParallelFor` n'est non ordonnée. Pas fixe 0,05 s, au plus 4 pas de rattrapage par image, comptés depuis l'horloge serveur et non depuis `DeltaTime` (🔎 `FloodSimulationActor.cpp:549-558`).
- `FTidemarkSeed` : 64 bits, `Derive(FName)` par SplitMix64 → sous-flux nommés. Remplace `FMath::Rand()`, `UtcNow`, `Cycles()` (🔎 `docs/audit/01_REPO_MAP.md` §3.4).
- `FSeaLevelSchedule` : fonction pure `LevelCm(StepIndex)` composée de termes (constante, rampe à exposant comme `ExpeditionClock.cpp:101-116`, marée harmonique, surcote gaussienne). Jamais d'état.
- `FTidemarkEdit` : `{FIntRect Cells, EOp (Dig, Fill, Raise), int16 ValueCm, FName Cause, uint32 ServerStep}`.
- `FTidemarkHash` : FNV-1a 64 sur la profondeur + `StepIndex`, **par bloc 32×32** puis combiné : on sait *où* ça diverge.
- `UTidemarkSubsystem : UTickableWorldSubsystem` : possède le solveur, applique le journal au pas serveur exact, expose `SampleWater`, `GetStepIndex`, `GetHash`.
- `UTidemarkRules : UDataAsset` : toutes les valeurs (pas, rattrapage, gravité, amortissement, profondeur de mer ouverte, taille de grille 512² à 6 m par défaut, échéancier, seuils de hash). Rien en dur.
- Réseau : `UTidemarkClockComponent` (sur le GameState : `{StartServerTime, Seed, ScheduleId, bStarted}`, ≈ 32 octets, une fois), `UTidemarkEditJournal` (`FFastArraySerializer`, ⚠ ≤ 1 ko/s), `UTidemarkSyncComponent` (hash client → serveur toutes les 1 s, RPC non fiable, 16 o ; après 3 écarts, instantané delta RLE, ⚠ ≈ 50 ko pour 512²). Un filet, pas le mode normal. **Jamais la grille entière en régime normal, jamais de GPU dans l'état qui fait foi.**

### 6.4 Couche C — Lecture
- `UTidemarkQuery` (BlueprintCallable) : `TimeToFlood`, `FloodOrder`, `IsPassable(A, B, t)` (chemin 8-connexe sur cellules sèches), `SampleDepth`. Tags `Tidemark.Flood.Reached`, `Tidemark.Path.Cut`. **C'est l'API vendue, première ligne de la page Fab.**
- `FTidemarkFirstWetBake` (éditeur) : simulation hors ligne jusqu'au niveau max → texture d'instants d'arrivée. `FTidemarkLint` : règles nommées et versionnées (`Submersible(point, level)`, `FloodOrder(points)`, `PathCut(A, B)`, `GridStale`), `FLintFinding {RuleId, Severity, Cells, LevelCm, Message}`, Message Log avec focus sur l'acteur. Panneau avec curseur de niveau de mer et acteurs d'annotation (camp, route, refuge, spawn).
- `UTidemarkRenderBridge` : `UTextureRenderTarget2D` R16 (profondeur) + `UMaterialParameterCollection`, 5 Hz, le rendu interpole ; `MF_Tidemark_WaterDepth` et un Niagara Data Interface livrés. Le rendu de l'eau reste au client (Water plugin, mesh procédural). `AFloodSimulationActor` de Tapao devient purement visuel.
- Format `.tmk` : en-tête JSON (version, origine, pas, dimensions, hash, `credits[]`) + corps binaire int16 par couche, lisible en Python sans Unreal. `scenario.json` : seed, échéancier, journal. **Un scénario se rejoue depuis ce fichier seul.**

### 6.5 Performance (cibles, mesurées en S3)
Référence 🔎 `docs/conventions/performance.md:16-30` : 60 FPS, thread de jeu ≤ 8 ms.
- Solveur 512² à 20 pas/s : ⚠ 0,5–1,0 ms par pas sur 6 workers, pic ≤ 2 ms.
- Mémoire : grille ≈ 10 Mo, total ≤ 40 Mo.
- Pont de rendu ≤ 0,3 ms. Réseau : 32 o à l'init, journal ≤ 1 ko/s, hash 16 o/s par client.
- Bake Landscape 2 km < 60 s ; import heightmap < 5 s.

### 6.6 Tests et CI
Specs `Tidemark.Solver.*` (reprises), `Tidemark.Determinism` (deux runs de 600 s, même hash, ×10), `Tidemark.Import.*`, `Tidemark.Lint.*`, `Tidemark.Replay`. **La CI compile le plugin et exécute les specs dès la S1**, ce que la CI actuelle ne fait pas (🔎 `.github/workflows/verifications.yml` : aucune compilation C++). Un manifest sans `credits[]` complet fait échouer la CI.

## 7. Démo de 30 secondes

Un Landscape du starter content, sans voix.
- **0–8 s, éditeur.** Clic « Bake first-wet ». Un calque bleu → rouge se peint. Le curseur « Sea level » glisse de +0 à +2,4 m : l'eau entre par les creux, remonte le chenal, coupe la route, cerne le camp. Trois panneaux : *« Refuge A : 6:20 »*, *« Bridge : cut at 3:40 »*, *« Spawn : underwater at 2:10 »*.
- **8–20 s, jeu.** PIE hôte et client côte à côte. L'eau remplit les creux puis franchit la crête, coupe la route **dans l'ordre annoncé**, sur les deux écrans. HUD : `hash 0x8F3A… = 0x8F3A…`, vert.
- **20–28 s.** Le designer rehausse le pont d'un mètre (`Raise`), rebake : *« Bridge : 7:05 »*. Un joueur creuse une tranchée vers le camp : la ligne d'eau saute.
- **28–30 s.** Carton : **« TIDEMARK — reads your terrain. Same tide on every machine. »**

Critère : un dev comprend sans lire ; un joueur partage le clip du bean sur la route coupée.

## 8. Modèle économique (tout ⚠ estimé)

- Fab reverse 88 % (🔎 annonce Epic du lancement de Fab, citée par la proposition 04).
- Plugin : **89 $ au lancement**, licence standard, mises à jour V1.x incluses. Prix à confronter aux 8 entretiens ; fourchette débattue 59–179.
- Scénarios année 1 : bas 40 ventes ≈ 3 k$ ; médian 150 ≈ 12 k$ ; haut 400, avec un jeu connu qui l'utilise, ≈ 31 k$. **Aucun ne paie un salaire.** C'est un produit d'appoint, une preuve de compétence et le refactor dont Tapao a besoin de toute façon.
- Gratuit : MCP (V1.1), spec `.tmk`, lecteur Python, île de démo. Pas de pack payant en V1.
- V2 si les seuils du §5 sont atteints : packs d'archétypes 19–29 $, licence médiation 300–800 € par site, puis seulement le B2B.
- Ce que « fort potentiel » veut dire honnêtement : un outil qui fait vivre un studio d'une ou deux personnes **si** le B2B s'ouvre en année 2 via le format et la trace. Le grand potentiel, s'il existe, est hors Unreal (version web du même moteur pour la médiation et l'urbanisme) : hors de ce cahier, mais c'est la raison de concevoir `.tmk` et `explain` pour survivre sans Unreal.

## 9. Licences, éthique, données (non négociables)

1. **Le plugin V1 ne contient aucune donnée géographique.** Il lit le terrain de l'utilisateur.
2. L'île de démo « inspirée d'un atoll » n'utilise que : côte Te Fenua (Etalab 2.0, producteur **et date** cités), relief **estimé** par profils et déclaré tel, toponymes fictifs. Jamais Maxar, cocoteraies DAG, raster IRD (🔎 `docs/audit/01_REPO_MAP.md` §7, `70-asset-licensing.md`).
3. **Tapao purge ses dérivés Maxar et DAG avant le branchement du plugin**, sinon le plugin hérite de la contamination.
4. Script d'assemblage de données (V1.1) : MIT, hors Fab, exécuté par l'utilisateur sous sa responsabilité ; il écrit les crédits exigés (Copernicus : « © DLR e.V. 2010-2014 and © Airbus Defence and Space GmbH » + déni de responsabilité ; ICESat-2 domaine public ; WorldCover CC BY 4.0). **OSM exclu** des grilles redistribuées tant qu'un juriste n'a pas tranché l'ODbL (base dérivée, partage à l'identique : ⚠ lecture, pas un avis).
5. Toute sortie porte : « simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation ». Aucune fonction ne renvoie une période de retour. Pas de commande « charger un lieu réel » en V1.
6. Pas de rejeu de catastrophes réelles, pas de bâti réel identifiable submergé, pas de rites ni de légendes réels, aucune donnée personnelle (noms de propriétaires, parcelles).
7. `PROVENANCE.json` (outil, date, licence, SHA-256) pour chaque asset livré, y compris ceux produits par des agents. Une heure de juriste PI et données avant la première vente. Ce document n'est pas un avis juridique.
8. Un lieu habité réel nommé n'apparaît qu'en V2, là où un LiDAR sous licence ouverte existe (🔎 Litto3D Etalab : Moorea, Bora Bora, partie de Tahiti ; **aucun produit Tuamotu trouvé**, proposition 04 §0.1), sous contrat avec la collectivité concernée, sans cadastre.

## 10. Plan en 8 semaines

Pré-requis : **une personne à plein temps sur le plugin, Tapao gelé** (hors correctifs) ; dépôt séparé ; verrou de l'éditeur partagé respecté. Niveau de preuve selon `docs/CAHIER_DES_CHARGES.md` §2 et §13.

| Sem. | Livrable | Critère de fin vérifiable | Porte |
|---|---|---|---|
| 1 | Squelette du plugin dans un projet UE 5.8 **neuf** ; `FTidemarkSolver` = `FFloodGrid` déplacé avec ses specs ; `FTidemarkSeed` ; CI qui compile et exécute les specs ; banc d'essai Fluid Flux / Oceanology / Water (déterminisme, API gameplay) ; **4 premiers entretiens** | Specs vertes dans le projet neuf ; `Tidemark.Determinism` : 600 s, même hash ×10 ; tableau comparatif sourcé ; 4 comptes rendus. `compilé`, `testé éditeur` | |
| 2 | Importeurs Landscape, heightmap, collision ; `UTidemarkTerrainAsset` ; `UTidemarkRules` ; **4 entretiens suivants** | Landscape 2 km importé en < 60 s, heightmap < 5 s ; même scénario sur grille Landscape et heightmap → même hash ; 8 comptes rendus | **Go/no-go écrit** : ≥ 4 devs sur 8 décrivent un besoin présent et un prix accepté, sinon arrêt, le solveur reste interne |
| 3 | `UTidemarkSubsystem`, horloge, échéancier, éditions `Dig`/`Fill`/`Raise` serveur ; profil | ≤ 1 ms moyen, ≤ 2 ms pic sur 512² mesurés (`ue.ps1 profile` ou Insights) ; `Tidemark.Replay` : (seed, grille, journal) → même hash final. `testé éditeur` | |
| 4 | Réplication : clock, journal, `UTidemarkSyncComponent`, snapshot | Hôte + client **sur deux PC**, 3 scénarios de 10 min avec 50 éditions côté client, 0 écart de hash ; une divergence injectée corrigée en < 2 s. `testé multi-machines` | **Porte multijoueur** : échec = le plugin reste interne |
| 5 | Bake first-wet, curseur de niveau de mer, 4 règles de lint, annotations, Message Log ; `UTidemarkQuery` Blueprint | Sur 3 maps (Landscape marketplace, heightmap à 3 bassins, île de démo), 100 % des findings vérifiés par rejeu (« submersible à N » se noie à N ; ordre prédit = ordre observé, 10 runs sur 10). Première **capture de 10 s publiable** | Pas de GIF qu'on a envie d'envoyer = on corrige la lisibilité avant de continuer |
| 6 | Pont de rendu, matériau, Niagara DI ; map de démo fictive ; île de démo « inspirée d'un atoll » sur côte Etalab | Package Development ; 60 FPS médian 1080p pendant la montée ; côte de l'île de démo conforme à ≥ 95 % à la côte Etalab ; `credits[]` complet, CI verte. `testé build` | |
| 7 | Docs, vidéo de 30 s, page Fab, EULA, `PROVENANCE.json`, juriste 1 h ; **Tapao consomme le plugin** sur branche séparée (`S/Flood/` remplacé, purge Maxar/DAG faite) | 2 testeurs externes installent seuls et obtiennent un lint en < 15 min, chronométré ; Tapao compile, ses specs passent, 0 include de `ExpeditionGameState` dans ce qui reste de `Flood/` ; checklist licences signée | |
| 8 | Publication Fab ; décision V1.1 (MCP) et V2 écrite à J+30 selon les ventes | Fab en ligne ; vidéo publiée sur r/unrealengine et les Discord UE ; compteur de ventes et de retours ouvert | |

## 11. Les 5 raisons d'échouer, et les parades

1. **Personne n'en veut** (⚠ la raison la plus probable). Les trois jeux « l'île coule » sont sortis sans ce produit. Parade : go/no-go réel en S2 sur 8 entretiens, arrêt écrit ; dans ce cas le travail n'est pas perdu, c'est le refactor de `Flood/` que `docs/audit/02_ARCHITECTURE_PROPOSAL.md` recommande.
2. **Fluid Flux, Oceanology ou Epic sortent un mode déterministe répliqué.** Parade : publier vite, prix bas, et le fossé est netcode + lint + API, pas le solveur. Si Epic le fait, Tidemark redevient interne et on cesse de vendre.
3. **Désynchronisation en production** (flottants, `ParallelFor`, rattrapage, drivers). Parade : spec de déterminisme à chaque commit, hash par bloc, snapshot correctif livré, jamais de GPU dans l'état qui fait foi ; porte de S4 avant tout le reste. Si la divergence inter-compilations apparaît chez un client, point fixe en V2.
4. **Le plugin siphonne le jeu et aucun des deux ne sort.** 🔎 Tapao a 288 tickets `fait` dont 93 sans recette, 0 preuve à deux PC. Parade : huit semaines plafonnées, Tapao gelé, dépôt séparé, aucune V2 sans ventes ; le plugin **est** l'extraction de `Flood/` dont Tapao a besoin.
5. **Une image d'un lieu réel submergé circule avec le nom du produit.** Parade : aucune donnée réelle dans le plugin, démo fictive, île « inspirée » sans toponyme, mention d'exclusion non désactivable, silence public sur « Tuamotu » tant que Tapao n'a ni licences assainies ni contact local.

## 12. Ce que ce cahier rejette de la direction initiale

1. **Six aléas et six couches en V1** : une menace, trois couches. Chaque aléa de plus est un mois sans client ; l'érosion exige une topographie modifiable qui n'existe pas (🔎 `FloodRules.h:15-16`, Nanite non déformable).
2. **Le MCP comme accroche** : un serveur MCP ne se montre pas dans une vidéo et ne se vend pas sur Fab. L'accroche est le curseur et le calque, sans IA. `design_scenario` disparaît ; au plus `propose_schedule` en V2, qui énumère des échéanciers validés par rejeu et laisse l'humain choisir.
3. **Les packs de lieux réels comme pilier et « choisir des lieux précis » comme argument** : le relief vertical d'un atoll n'est pas honorable avec les sources libres (2–4 m d'erreur pour 3 m de relief), le joueur ne distingue pas réel et plausible (🔎 audit 81), et c'est le risque réputationnel n°1. L'envie du propriétaire est servie autrement : l'île de démo garde la **forme réelle** d'un atoll (côte Etalab), le script d'assemblage libre permet à qui veut de charger son emprise, et les lieux nommés arrivent en V2 là où un LiDAR ouvert existe.
4. **Deux publics à égalité** : le B2B tire vers la prévision et le cadastre, ce que les garde-fous interdisent. Il achète en V2 ce que le client n°1 achète en V1, plus un rapport.
5. **« Le moteur déduit le climat et l'exposition au vent »** : un heightmap n'a pas de climat. La géométrie se déduit, le forçage s'importe (preset par région, DataAsset). Tout le reste finit en hallucination.
6. **Commencer le plugin comme produit séparé avant d'avoir décidé** : l'ordre est go/no-go (S2), porte multijoueur (S4), puis seulement le produit. Les deux chantiers à la fois, c'est la seule issue dont tout le monde est sûr qu'elle échoue.

## 13. Questions ouvertes pour le propriétaire

1. **Tapao d'abord ou le plugin d'abord ?** Ce cahier gèle Tapao huit semaines. L'autre option est d'extraire `Flood/` en plugin interne (S1–S4, sans page Fab) et de ne décider la vente qu'après la recette L1–L4 du jeu.
2. **Le contrat de temps de Tapao** : avec 650 cm en 60 s (🔎 `ExpeditionClock.cpp:101-116`), tout le fragment est sous l'eau en une minute et `TimeToFlood` ne varie que de quelques secondes entre cellules. Pour que « lire le risque sur le sol » ait un sens dans Tapao, la montée doit s'étaler (plusieurs minutes) : cela change le cahier v1.4 du jeu.
3. **Les lieux précis** : acceptes-tu qu'ils soient V2, conditionnés à un LiDAR ouvert et à un avis juridique, et que l'île de démo soit « inspirée » sans nom ?
4. **Le MCP** : acceptes-tu qu'il sorte du chemin critique (V1.1, gratuit, 3 outils) ?
5. **Le prix** : 89 $ est un pari ; veux-tu le tester bas (59 $) ou haut (149 $) dans les entretiens ?
6. **Le nom** : Tidemark, Highwater, Littoral, Hoa ? « Hoa » demande une validation culturelle.
7. **Le juriste** : une heure avant la première vente, qui, quand ?
