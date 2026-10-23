# Tidemark — porte S2 (go/no-go)

- **Version** : 1.0, 2026-10-23. **Auteur** : `tidemark-lead` (TM-12, T-352). **Env** : rec.
- **Décision du propriétaire** : « go, on construit » (2026-10-23). Cette note dit ce que ce go vaut aujourd'hui, ce qu'il manque pour vendre et à quelles conditions la S3 démarre.
- **Niveau de preuve global : `non compilé`.** Tout le C++ du plugin a été écrit en session cloud sans UE : aucun build, aucune spec exécutée. Seuls les scripts Python ont tourné.

## 1. État TM-01 → TM-12

| Ticket | Livrable | Statut | Preuve réelle |
|---|---|---|---|
| TM-01 (T-341) | uplugin, 2 modules, Build.cs, logs, docs | review | non compilé |
| TM-02 (T-342) | `FTidemarkSolver` = `FFloodGrid` déplacé | review | non compilé ; diff avec `FloodGrid.cpp` : identique hors `bForceSingleThread` |
| TM-03 (T-343) | `FTidemarkSeed`, `FTidemarkRng`, `Tidemark::Hash` | review | non compilé ; références calculées en Python |
| TM-04 (T-344) | `Tidemark.Determinism.*`, audit `ParallelFor` | review | non compilé ; audit relu : aucune réduction parallèle, passes lecture/écriture disjointes |
| TM-05 (T-345) | CI greps + unittest, job build `if: false` | review | **exécuté ici** : 3 `check_*.py` OK, 20 tests unittest OK |
| TM-06 (T-346) | banc d'essai | review | sourcé (27 URL, extraits ; fiches Fab non lues) |
| TM-07 (T-347) | `FTidemarkGrid`, `UTidemarkTerrainAsset`, `.tmk`, `tmk.py` | review | non compilé ; **`test_tmk.py` : 11 tests OK ici** |
| TM-08 (T-348) | `ITidemarkHeightSource`, texture, Landscape, baker, import | review | non compilé ; temps d'import non mesurés |
| TM-09 (T-349) | collision par lots, `tidemark.BakeCollision` | review | non compilé ; recette `L_Frag_Raraka_01` non faite |
| TM-10 (T-350) | `UTidemarkRules`, `FSeaLevelSchedule`, D-1, D-2 | review | non compilé |
| TM-11 (T-351) | `Tidemark.Import.Scenario.*` (4 cas) | en-cours | non compilé ; compare texture et fonction, pas Landscape (éditeur requis) |
| TM-12 (T-352) | cette note | review | — |

**Specs (`It(`)** : Solver 4 (6 dans `FloodGridSpec` ; les 2 absentes portent sur l'échéancier, couvert par Schedule, et le vent, V2) · Seed 14 · Hash 15 · Determinism 5 · Grid 15 · Format 7 · Schedule 18 · Import.Bake 16 · Import.Collision 10 · Import.Scenario 4. **108 cas, 0 exécuté.**

**Entretiens** : le cahier §10 conditionne le go à « ≥ 4 devs sur 8 ». **Aucun entretien mené.** Le go du propriétaire s'y substitue ; la note le consigne, elle ne le valide pas.

## 2. Revue d'architecture (ARC-01 à ARC-10, CONVENTIONS §1 et §5)

**Dépendances** : `TidemarkEditor` → `TidemarkRuntime` seulement ; zéro include de `monke_island` (script et lecture) ; modules moteur conformes à CONVENTIONS §1 plus `Json`, `JsonUtilities`, `AssetRegistry`, `ImageWrapper`, `ImageCore`, `Projects` (à ajouter à la liste autorisée). Un seul `FRandomStream`, annoté. Aucun `DeltaTime`, aucune horloge murale hors chronométrage de bake.

Incohérences réelles, à corriger **avant** d'écrire le subsystem :

| # | Contrôle | Où | Constat | Correctif | Ticket |
|---|---|---|---|---|---|
| A1 | ARC-01 | `Grid/TidemarkGrid.cpp:4-5`, `:181-209` | La couche A inclut `Solver/TidemarkSolver.h` et `Solver/TidemarkHash.h` : `FillSolver` fait dépendre le terrain de la simulation | `FillSolver` devient `Tidemark::Solver::FromGrid(const FTidemarkGrid&, …)` dans `Solver/` ; `Fnv1a64` dans `Tidemark/Core/` | TM-13 |
| A2 | §5 | `TidemarkGrid.cpp:197-208` | `FillSolver` appelle `SetupSea` (qui recalcule `Fixed` depuis la géométrie et l'`OpenSeaDepth` **de runtime**) puis force `Fixed` depuis les drapeaux bakés : deux sources de vérité ; les cellules fixes dépendent des règles chargées, pas du seul hash de grille | Les drapeaux sont seuls faisant foi : `FromGrid` pose `Fixed = OpenSea\|Border`, `Depth = max(0, Sea − Ground)`, sans `SetupSea` (gardé pour les specs reprises) | TM-13 |
| A3 | ARC-03 | `Solver.h:69` vs `Rules.h:22-24` | `Step(…, 9.81f, 0.995f)` duplique `DefaultGravity`/`DefaultFluxDamping` ; `SolverSpec.cpp:29` et `DeterminismSpec.cpp:196` reposent sur ces défauts cachés | Supprimer les arguments par défaut ; les specs passent `Tidemark::Rules::Default*` | TM-13 |
| A4 | §5 | `Schedule.h:266`, `Rules.h:149-152`, `Schedule.cpp:69` | `bHoldRise` sur le schedule **et** sur les règles ; `HeldLevelCm` ne lit que le schedule, `HoldsRise` combine mais rien ne l'appelle : un rejeu depuis `scenario.json` peut diverger du direct | Un seul drapeau, sur le schedule ; `Rules.bHoldRise` n'initialise que `DefaultSchedule` | TM-13, TM-14 |
| A5 | §4 | `Solver.h:13` (m, s) vs `Grid.h:30`, `Hash.h:137`, `Schedule.h` (cm) | Solveur en mètres, tout le reste en cm ; la conversion ×100 du hash n'existe que dans un helper de spec (`DeterminismSpec.cpp:153-162`) ; CONVENTIONS §4 dit « solveur en cm », faux | Une fonction produit `Tidemark::Hash::HashSolver(const FTidemarkSolver&, Step, Rules)` ; conversion cm→m en un seul point du subsystem ; corriger CONVENTIONS §4 | TM-13 |
| A6 | §6.5 | `Grid.h:48-51` (`MaxCells` 5,6 M), `Rules.h:97` (512), `ImportLibrary.cpp:21-29`, `Baker.cpp:48,85` | Le baker lit `SimCellSizeCm` mais ignore `SimGridCells` : un Landscape de 10 km à 6 m donne 2,8 M cellules, accepté, 10× le budget ; `FallbackGridCells` lu par rien ; `Baker.cpp:48` compare chaque axe à `MaxCells` (sans objet) | Refuser ou avertir au-delà de `SimGridCells` par axe (sauf `BoundsOverride`) ; retirer `Fallback*` tant qu'aucun `Resample` n'existe | TM-13, TM-15 |
| A7 | §1 | `CollisionHeightSource.h:48` | Tag par défaut `IslandWaterSurface` : un nom Tapao dans le plugin | Garder `TidemarkIgnore` seul ; le pont Tapao ajoute le sien | TM-28 |
| A8 | ARC-07 | `HeightSource.h:36`, `CollisionCommands.cpp:26-30, 95-130` | `IsAsync()` jamais lu ; `LocalBakeGrid` mort derrière `#if !TIDEMARK_HAS_BAKE_GRID` (toujours 1) | Supprimer | TM-13 |

ARC-09 : tout a une spec sauf la bibliothèque d'import et les commandes console (éditeur requis) ; accepté en S2, couvert par la recette TM-08/TM-09 sur le PC.

## 3. Risques de compilation (par probabilité)

| P | Doute | Fichier | Parade |
|---|---|---|---|
| Forte | Monde privé en spec : `UWorld::CreateWorld(Editor, false)` + `SpawnActor` + `LineTrace` + `FPhysScene::Flush` ; corps Chaos absents sans tick | `CollisionSpec.cpp:113-134` | `World->Tick(LEVELTICK_All, 0.f)` après spawn, ou `LatentIt` sur le monde éditeur avec une map de test |
| Forte | `FLandscapeEditDataInterface::GetHeightDataFast` : signature mouvante, `LandscapeEdit.h` sous `WITH_EDITOR` | `LandscapeHeightSource.cpp:61-62` | Repli : `ALandscapeProxy::GetHeightAtLocation(…, Editor)` par échantillon |
| Moyenne | `IImageWrapperModule::DecompressImage(…, FImage&)` et `FImage::CopyTo(R32F)` | `TextureHeightSource.cpp:168-198` | Repli : `CreateImageWrapper` + `GetRaw(Gray, 16)` |
| Moyenne | `Niagara` en dépendance privée sans entrée `Plugins[]` du `.uplugin` ; inutilisé | `TidemarkRuntime.Build.cs` | Retirer (D-3) |
| Moyenne | `UPROPERTY uint64` dans des structs `BlueprintType` : accepté sans `BlueprintRead*`, refusé dès qu'on l'ajoute | `Seed.h:26,62`, `Hash.h:97,105` | Pas d'accès Blueprint ; sinon deux `uint32` (D-4) |
| Faible | `ParallelFor(Num, Body, EParallelForFlags)` | `Solver.cpp:135-177` | Surcharge existante |
| Faible | `UPackage::SavePackage(…, FSavePackageArgs)` dupliqué | `ImportLibrary.cpp:126-129`, `CollisionCommands.cpp:177-180` | API stable ; factoriser en S3 |
| Faible | SHA-256 maison | `Format.cpp:19-66` | Aucun risque de build ; spec sur vecteurs FIPS ; intégrité seulement, pas cryptographique |
| Faible | `meta=(Units="Hz")`, `inline const TCHAR*`, `constexpr` dans `USTRUCT` | `Rules.h`, `Format.h`, `Grid.h` | Supportés |

## 4. Décisions

- **D-1 (float32, 1e-5 relatif)** : **validée sous réserve de mesure.** La dérive modélisée (≈ 4e-6) reste sous un quantum de hash d'une cellule ; passer en double changerait chaque bit de l'arithmétique sans spec d'équivalence, contre « déplacé, pas réécrit ». Condition : dérive de `VolumeConserved` lue au premier build < 1e-5, sinon on rouvre (point fixe = V2).
- **D-2** : faite, conforme.
- **D-3 (Niagara)** : **retirer** maintenant ; remettre à TM-23 avec `"Plugins": [{"Name": "Niagara", "Enabled": true}]`. Un plugin livré seul ne doit pas échouer à l'édition de liens pour un module inutilisé.
- **D-4 (`uint64`)** : **garder** sans accès Blueprint ; `UTidemarkQuery` exposera le hash en `FString` hexadécimal. Scinder seulement si UHT refuse.
- **D-5 (CI sans compilation)** — question au propriétaire : runner auto-hébergé sur le PC de dev (job `build-windows`, build nocturne, verrou d'éditeur), ou « build local + rapport `Saved/Automation/Reports/index.json` joint à chaque ticket » ? Ma recommandation : la seconde jusqu'à TM-18, le runner ensuite.

## 5. Verdict S2

**Go acquis par décision du propriétaire.** Techniquement, la S2 livre un socle cohérent sur le papier, mais **rien n'est prouvé** : 108 specs écrites, 0 exécutée, 0 entretien, 0 temps d'import mesuré.

Ce qui manquerait pour vendre aujourd'hui : (1) une preuve d'exécution (build + specs vertes) ; (2) les 8 entretiens, ou l'aveu écrit qu'on s'en passe ; (3) la porte S4 deux PC, sans laquelle « same tide on every machine » est une phrase ; (4) la lecture intégrale des fiches Fab de Fluid Flux et Dynamic Real Water avant toute formulation de prix ; (5) `UTidemarkQuery` et le lint, le produit vendu, qui n'existent pas (S5).

**Conditions d'entrée en S3 (bloquantes)** :
1. Premier build sur le PC (`ue.ps1 build`, éditeur fermé sous verrou) et `ue.ps1 test -Filter Tidemark` **verts**, rapport joint à T-342 → T-351 ; D-3 appliquée avant.
2. Dérive `VolumeConserved` consignée (D-1).
3. Correctifs A1 → A5 dans TM-13, contrat du subsystem écrit **avant** le code, par ce rôle.
4. TM-11 vert.
5. Temps d'import mesurés (Landscape < 60 s, heightmap < 5 s) ou l'écart documenté.

## 6. Ordre de la semaine 3

- **TM-13** `UTidemarkSubsystem` (après TM-10 et les conditions ci-dessus) : solveur, `FromGrid`, pas comptés par `StepsFromSeconds` depuis l'horloge, `HashSolver` produit, `SampleWater`, `GetStepIndex`, `GetHash` ; porte A1–A5, A8. Critique, séquentiel.
- **TM-14** `FTidemarkEdit` + journal + `Tidemark.Replay.*` (après TM-13) ; la struct et ses specs pures peuvent partir dès que le contrat de TM-13 est écrit.
- **TM-15** profil 512² (après TM-13, `testé build`) : tranche A6.
- **En parallèle, sans attendre le build** : D-3, A7, A8, lecture des fiches Fab (benchmark §6), question D-5.

Rappel (cahier §9.5) : « Simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation. »
