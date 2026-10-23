# Tidemark — backlog des 8 semaines

Source : cahier Tidemark §10. Chaque ligne devient un ticket `T-NNN` du chantier (`/tidemark plan <semaine>`), avec le préfixe `[TM][S<n>]`. Rôle = agent de `.claude/agents/`. Preuve = niveau exigé. « Après » = dépendances.

Portes : **S2 go/no-go** (écrit par `tidemark-lead`, ici décidé « go » par le propriétaire le 2026-10-23 : on construit) · **S4 porte multijoueur** (échec = plugin interne, pas de page Fab) · **S5 capture publiable**.

## Semaine 1 — socle et déterminisme (tickets créés le 2026-10-23)
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-01 (T-341) | Squelette du plugin : `Tidemark.uplugin`, `TidemarkRuntime`, `TidemarkEditor`, `Build.cs`, `LogTidemark`, README, CHANGELOG ; compile dans le projet | tidemark-lead | `Plugins/Tidemark/*` | compilé | — |
| TM-02 (T-342) | `FTidemarkSolver` = `FFloodGrid` déplacé tel quel + specs `Tidemark.Solver.*` reprises de `Flood/Tests/FloodGridSpec.cpp` | tidemark-solver-programmer | `Solver/TidemarkSolver.*`, `Tests/TidemarkSolverSpec.cpp` | testé éditeur | TM-01 |
| TM-03 (T-343) | `FTidemarkSeed` (SplitMix64, `Derive`), `FTidemarkHash` (FNV-1a par bloc 32×32), specs | tidemark-solver-programmer | `Solver/TidemarkSeed.*`, `Solver/TidemarkHash.*`, specs | testé éditeur | TM-01 |
| TM-04 (T-344) | Spec `Tidemark.Determinism.SameSeedSameHash` : 2 runs × 600 s, ×10 ; audit `ParallelFor` du solveur | tidemark-qa | `Tests/TidemarkDeterminismSpec.cpp` | testé éditeur | TM-02, TM-03 |
| TM-05 (T-345) | CI : job qui compile le plugin et lance `Tidemark.*` ; grep « zéro include monke_island » ; vérification `credits[]` | tidemark-qa | `.github/workflows/tidemark.yml`, `scripts/tidemark/check_*.py` | CI verte sur une PR | TM-01 |
| TM-06 (T-346) | Banc d'essai documentaire : Fluid Flux, Oceanology, Water d'Epic (déterminisme, API gameplay, prix) | tidemark-release-engineer | `docs/produit/tidemark/benchmark.md` | sourcé | — |

## Semaine 2 — terrain
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-07 | `FTidemarkGrid` (SoA int16 cm, flags), `UTidemarkTerrainAsset` (hash sources, `Credits`), format `.tmk` (lecture/écriture), lecteur Python `Tools/tmk.py` | tidemark-terrain-programmer | `Grid/*`, `Tools/tmk.py` | testé éditeur | TM-01 |
| TM-08 | `ITidemarkHeightSource` + `FromTexture` (R16, PNG, EXR) + `FromLandscape` | tidemark-terrain-programmer | `TidemarkEditor/Import/*` | testé éditeur : Landscape 2 km < 60 s, heightmap < 5 s | TM-07 |
| TM-09 | `FromCollision` : rayons par lots à l'import, canal dédié, « plus haut sol sous le niveau max » | tidemark-terrain-programmer | `TidemarkEditor/Import/CollisionHeightSource.*` | testé éditeur sur `L_Frag_Raraka_01` | TM-07 |
| TM-10 | `UTidemarkRules` (toutes les valeurs), `FSeaLevelSchedule` (termes purs), specs | tidemark-solver-programmer | `Rules/*`, `Solver/TidemarkSchedule.*` | testé éditeur | TM-01 |
| TM-11 | Spec `Tidemark.Import.SameScenarioSameHash` : même scénario sur grille Landscape et heightmap → même hash | tidemark-qa | specs | testé éditeur | TM-08 |
| TM-12 | **Go/no-go S2** : note écrite (benchmark, état des specs, risques) | tidemark-lead | `docs/produit/tidemark/gate-S2.md` | — | TM-04…TM-11 |

## Semaine 3 — simulation en jeu
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-13 | `UTidemarkSubsystem` : possède le solveur, pas comptés depuis l'horloge, `SampleWater`, `GetStepIndex`, `GetHash` | tidemark-solver-programmer | `Solver/TidemarkSubsystem.*` | testé éditeur | TM-10 |
| TM-14 | `FTidemarkEdit` + journal local, ops `Dig`/`Fill`/`Raise`, application au pas exact ; spec `Tidemark.Replay.*` | tidemark-solver-programmer | `Solver/TidemarkEdit.*`, specs | testé éditeur | TM-13 |
| TM-15 | Profil : ≤ 1 ms moyen, ≤ 2 ms pic sur 512² (`ue.ps1 profile`, Insights) ; rapport | tidemark-qa | `docs/produit/tidemark/perf-S3.md` | testé build | TM-13 |

## Semaine 4 — réseau (porte)
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-16 | `UTidemarkClockComponent` (état répliqué une fois), `UTidemarkEditJournal` (`FFastArraySerializer`) | tidemark-network-engineer | `Net/*` | testé éditeur hôte+client PIE | TM-14 |
| TM-17 | `UTidemarkSyncComponent` : hash 1 Hz, 3 écarts → instantané delta RLE ; `LogTidemarkNet` ; `tidemark.InjectDivergence` | tidemark-network-engineer | `Net/TidemarkSyncComponent.*` | testé éditeur | TM-16 |
| TM-18 | **Porte S4** : hôte + client sur deux PC, 3 scénarios × 10 min, 50 éditions client, 0 écart ; divergence injectée corrigée < 2 s ; rapport | tidemark-qa, tidemark-network-engineer | `docs/produit/tidemark/gate-S4.md` | testé multi-machines | TM-17 |

## Semaine 5 — lisibilité
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-19 | `FTidemarkFirstWetBake` : simulation hors ligne → texture d'instants ; `UTidemarkQuery` Blueprint (`TimeToFlood`, `FloodOrder`, `IsPassable`, `SampleDepth`), tags | tidemark-editor-programmer | `Query/*`, `TidemarkEditor/Bake/*` | testé éditeur | TM-14 |
| TM-20 | `FTidemarkLint` : 4 règles versionnées, `FLintFinding`, Message Log avec focus ; acteurs d'annotation (camp, route, refuge, spawn) | tidemark-editor-programmer | `TidemarkEditor/Lint/*` | testé éditeur | TM-19 |
| TM-21 | Panneau éditeur : calque first-wet, curseur de niveau de mer, bouton Bake/Lint ; console `tidemark.Overlay` | tidemark-editor-programmer | `TidemarkEditor/UI/*` | testé éditeur + capture 10 s | TM-20 |
| TM-22 | Spec `Tidemark.Lint.VerifiedByReplay` sur 3 maps : 100 % des findings vérifiés ; heightmap de test à 3 bassins | tidemark-qa | specs, `Content/Test/*` | testé éditeur | TM-21 |

## Semaine 6 — rendu et démo
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-23 | `UTidemarkRenderBridge` : RT R16 + MPC à 5 Hz, `MF_Tidemark_WaterDepth`, matériau d'exemple, Niagara Data Interface | tidemark-render-programmer | `Render/*`, `Content/Materials/*` | testé éditeur | TM-13 |
| TM-24 | Map de démo fictive (Landscape, camp, route, pont, refuge) + île de démo « inspirée d'un atoll » (côte Etalab, relief estimé, `credits[]`, toponymes fictifs) | tidemark-release-engineer, tidemark-terrain-programmer | `Content/Demo/*`, `scripts/tidemark/demo_island.py` | testé éditeur ; côte ≥ 95 % conforme | TM-08, TM-23 |
| TM-25 | Package Development, 60 FPS médian 1080p pendant la montée ; rapport | tidemark-qa | `docs/produit/tidemark/perf-S6.md` | testé build | TM-24 |

## Semaine 7 — livraison et Tapao
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-26 | Docs utilisateur, CHANGELOG, EULA, `LICENSES/`, `PROVENANCE.json`, checklist Fab, extraction dans un dépôt propre | tidemark-release-engineer | `Plugins/Tidemark/Docs/*` | checklist signée | TM-25 |
| TM-27 | Vidéo de 30 s (script §7 du cahier), page Fab en brouillon | tidemark-release-engineer | `docs/produit/tidemark/fab-page.md`, vidéo | fichier vidéo | TM-25 |
| TM-28 | Tapao consomme le plugin sur `dev/TM-28` : `Flood/` remplacé, `ULittoralTapaoBridge` → `UTidemarkTapaoBridge`, 0 include `ExpeditionGameState` dans l'adaptateur, purge Maxar/DAG dans ce qui est cuit | tidemark-tapao-integrator | `Source/monke_island/Flood/*`, `Water/IslandWaterSubsystem.*`, `DefaultGame.ini` | testé éditeur ; specs Tapao vertes | TM-23 |
| TM-29 | 2 testeurs externes installent seuls et obtiennent un lint en < 15 min (chronométré) | tidemark-release-engineer | `docs/produit/tidemark/onboarding-test.md` | rapport | TM-26 |

## Semaine 8 — publication
| ID | Tâche | Rôle | Fichiers | Preuve | Après |
|---|---|---|---|---|---|
| TM-30 | Publication Fab, vidéo sur r/unrealengine et Discord UE, compteur de ventes et retours | tidemark-release-engineer | `docs/produit/tidemark/launch.md` | en ligne | TM-26, TM-27, TM-29 |
| TM-31 | Décision V1.1 (MCP) et V2 écrite à J+30 | tidemark-lead | `docs/produit/tidemark/decision-J30.md` | — | TM-30 |

## V1.1 (seulement si V1 publiée)
| ID | Tâche | Rôle |
|---|---|---|
| TM-40 | Commandlet `-run=Tidemark analyze|explain|lint|replay|schedule` en JSON | tidemark-editor-programmer |
| TM-41 | Serveur MCP Python `tidemark-mcp` : 3 outils qui exécutent le commandlet, réponses avec `rule_id@version`, `grid_hash`, `seed`, « inconnu » sans donnée | tidemark-mcp-developer |
| TM-42 | Script MIT `heightmap_from_open_data.py` (Copernicus, ICESat-2, côte Etalab) avec `CREDITS.json` ; OSM exclu | tidemark-terrain-programmer |
| TM-43 | Export GeoTIFF de `.tmk` | tidemark-terrain-programmer |

## Décisions ouvertes (remontées par la vague 1, 2026-10-23)
| # | Sujet | Constat | Options | Qui tranche |
|---|---|---|---|---|
| D-1 | Conservation du volume en float32 | Modèle numpy du solveur (même ordre d'opérations) : dérive ≈ −10 L sur 12 000 pas pour 2 304 m³ ; 0 en float64. Spec `Tidemark.Determinism.VolumeConserved` gardée à ±1 L : échec attendu au premier build | (a) `Depth` en `double` dans le solveur (mémoire ×2, déterminisme inchangé) ; (b) critère relatif (1e-5) dans `UTidemarkRules` ; (c) accumulateur de compensation (Kahan) par cellule | `tidemark-lead` à TM-10/TM-13 |
| D-2 | `Step(..., bForceSingleThread)` | Demandé par la spec `IndependentOfWorkerCount` (en `#if 0`) | ajouter le paramètre, transmis en `EParallelForFlags::ForceSingleThread` | `tidemark-solver-programmer` à TM-10 |
| D-3 | `Niagara` en dépendance privée de `TidemarkRuntime` | Plugin moteur, pas un module de base : UBT peut refuser pour un plugin livré seul | retirer jusqu'à TM-23, ou déclarer la dépendance de plugin dans le `.uplugin` | `tidemark-lead` au premier build |
| D-4 | `UPROPERTY` sur `uint64` (`FTidemarkSeed`, `FTidemarkStateHash`) | UHT peut refuser | deux `uint32`, ou retirer `BlueprintType` | au premier build |
| D-5 | CI sans compilation | Aucun runner Windows UE 5.8 ; job `build-windows` écrit, `if: false` | runner auto-hébergé sur un PC, ou « build local + rapport joint » | propriétaire |
