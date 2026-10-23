---
name: tidemark-tapao-bridge
description: Branche Tapao Rush Island sur le plugin Tidemark — remplace Source/monke_island/Flood/ par l'adaptateur UTidemarkTapaoBridge (horloge d'expédition vers échéancier, fournisseur de surface vers UIslandWaterSubsystem), bake la grille de L_Frag_Raraka_01, supprime les rayons par image et la dépendance Flood vers ExpeditionGameState, purge les dérivés Maxar et DAG, puis recette : specs, PIE, captures avant/après. À utiliser pour TM-28, « branche Tapao sur Tidemark », « Flood/ vers le plugin ».
argument-hint: "[plan | bridge | bake | purge | verify]"
---

# Tapao consomme Tidemark

Arguments : `$ARGUMENTS`. Rôle : `tidemark-tapao-integrator` (seul autorisé sur `Source/monke_island/`). Branche `dev/TM-28` en worktree. Cahier §10 S7, §13 Q1–Q2.

## `plan`
1. Inventaire de `Source/monke_island/Flood/` : qui inclut quoi (`grep -rn "Flood/" Source/monke_island --include=*.cpp --include=*.h`), qui appelle `FloodSeaRiseSubsystem`, `HazardClock`, `AFloodSimulationActor` (Audio, UI, Creatures, Storm…).
2. Table « avant → après » : chaque symbole public de `Flood/` → son équivalent (`UTidemarkSubsystem::SampleWater`, `ITidemarkWaterProvider`, `UTidemarkTapaoBridge::GetPhase…`) ou « supprimé, raison ».
3. Assets touchés : `DA_FloodRules` (champs de rayons supprimés), `LI_ExpeditionCore` (acteur d'inondation → acteur de rendu Tidemark), `L_Frag_Raraka_01` (asset de terrain). `owner=` pour chacun.
4. Montre le plan ; ADR-009 en brouillon.

## `bridge`
1. `monke_island.uproject` : plugin `Tidemark` activé ; `monke_island.Build.cs` : `TidemarkRuntime` en dépendance privée.
2. `Flood/TidemarkTapaoBridge.h/.cpp` (`UWorldSubsystem`) : à `BeginPlay` du GameState (serveur) et à la réplication de `FExpeditionClockState` (client), construit `FSeaLevelSchedule` : échéance = `FloodStartTime`, rampe (`MaxWaterRiseCm`, `SubmersionDuration`, `WaterRiseExponent`), puis `PostSubmersionRiseCmPerSecond` jusqu'à `PostSubmersionMaxRiseCm`, `HoldRise` vrai ; seed = `FTidemarkSeed::FromName(ExpeditionId)` ; centre de grille = devant la barque d'arrivée (règle existante) poussé comme configuration répliquée du plugin. Un bonus de zone avant la submersion → nouvel échéancier ; après → ignoré (cahier Tapao §6.1).
3. `UIslandWaterSubsystem::SetSurfaceProvider(bridge)` avec `ITidemarkWaterProvider` ; nage, pataugeage et noyade inchangés.
4. Les consommateurs (`Audio`, `UI`, `Storm`, `Creatures`) lisent la phase via le GameState comme avant, et la hauteur d'eau via `UIslandWaterSubsystem` : ils n'incluent rien de Tidemark.
5. Suppression de `FloodGrid`, `FloodSimulationActor` (ou réduit au rendu via `ATidemarkWaterMeshActor`), `FloodSeaRiseSubsystem`, `HazardClock`, `FloodSchedule`, `FloodOutsideSea` ; `FloodRules` réduit aux réglages propres au jeu (ou supprimé si `UTidemarkRules` suffit, décision écrite).
6. `grep -rn "ExpeditionGameState" Source/monke_island/Flood/` → seulement `TidemarkTapaoBridge.cpp`.

## `bake`
`/tidemark-terrain-import collision L_Frag_Raraka_01` → `TA_Frag_Raraka_01` ; `credits[]` = Te Fenua (côte, Etalab 2.0, date) + « relief estimé » ; aucune référence Maxar/DAG dans l'asset. `L_Test_Flat` idem pour les tests.

## `purge`
1. Liste les assets dérivés de Maxar et DAG (`docs/audit/01_REPO_MAP.md` §7, `70-asset-licensing.md`) : `T_Raraka_TerrainMasks`, masques de végétation, `DT_Raraka_Palms` (22 431 cocotiers DAG), tuiles de relief si elles en dépendent.
2. Pour cette branche : ce qui est cuit (`DefaultGame.ini` `MapsToCook`, `DirectoriesToAlwaysCook`) ne doit plus les référencer. Si `Lvl_Raraka` ne peut pas être assainie, elle sort de la cuisson et c'est écrit dans l'ADR. Pas de renommage pour « régler » une licence.
3. Rapport : liste purgée, liste restante avec raison, qui doit trancher.

## `verify`
1. `python scripts/worktree.py build TM-28` → 0 erreur.
2. `scripts/ue.ps1 test -Filter monke_island.Flood` et `monke_island.Expedition` (specs déplacées ou réécrites contre le pont) → vertes ; `Tidemark.*` vertes.
3. PIE solo sur `L_Frag_Raraka_01` : captures à t = 0, échéance, +20 s, +40 s, +60 s, fin ; comparaison avec les captures d'avant (même map, mêmes instants) : même ordre visuel d'inondation, eau qui ne redescend jamais, extraction automatique à la fin.
4. PIE hôte + client : le client attend la phase `Submerging` répliquée (pas d'eau qui monte puis redescend), hashes égaux.
5. `storm-flood.md` mis à jour, ADR-009 finalisé (écarts : contrat de temps 650 cm/60 s et lisibilité de `TimeToFlood`, question au propriétaire).
Preuve : `testé éditeur` (+ `testé build` si package). Puis `/factory-gate` REC avec revue réseau.
