---
name: tidemark-determinism
description: Prouve ou diagnostique le déterminisme de la simulation Tidemark — exécution des specs Tidemark.Determinism.* et Tidemark.Replay.*, rejeu d'un scénario depuis (seed, grille, échéancier, journal), comparaison des hashes par bloc 32×32 entre deux runs ou deux machines pour localiser une divergence, audit des sources de non-déterminisme (hasard non seedé, ParallelFor avec réduction, DeltaTime, ordre d'itération, flottants). À utiliser pour « même seed, hash différent », « désync », ou avant chaque porte.
argument-hint: "[prove | replay <seed> [--steps N] | diff <hashA.json> <hashB.json> | audit [dossier]]"
---

# Déterminisme Tidemark

Arguments : `$ARGUMENTS`. Règle d'or : `docs/produit/tidemark/CONVENTIONS.md` §5. Rôles : `tidemark-qa` (preuve), `tidemark-solver-programmer` (correction), `tidemark-network-engineer` (deux machines).

## `prove`
1. `scripts/ue.ps1 test -Filter Tidemark.Determinism` et `-Filter Tidemark.Replay` (éditeur fermé → headless ; ouvert → MCP `AutomationTestToolset`).
2. Specs attendues :
   - `SameSeedSameHash` : 2 runs × 600 s (12 000 pas à 0,05 s), même seed, même grille, même échéancier → hash final identique, ×10 seeds.
   - `DifferentSeedDifferentHash` : garde-fou contre un hash constant.
   - `BlockHashLocatesDivergence` : une cellule modifiée → exactement un bloc 32×32 diffère.
   - `Replay.FromJournal` : simulation en ligne avec 50 éditions à des pas aléatoires (seedés) ; rejeu hors ligne depuis le journal → même hash.
   - `Replay.LateEditTriggersReplay` : une édition datée d'un pas passé provoque un rejeu depuis l'instantané, pas une application tardive.
3. Rapport : nombre passés/échoués, hashes (hex), durée.

## `replay <seed> [--steps N]`
Console `tidemark.Replay <seed> [N]` dans l'éditeur ou le commandlet `-run=Tidemark replay --seed <seed> --grid <asset> --schedule <id> --journal <file> --steps N` (V1.1). Sortie : hash final + hashes par bloc en JSON (`Saved/Tidemark/replay-<seed>.json`).

## `diff <A> <B>`
Compare deux JSON de hashes par bloc (deux runs, ou hôte et client depuis `LogTidemarkNet`). Sortie : liste des blocs qui diffèrent (coordonnées, premier pas divergent si connu), carte ASCII 16×16 des blocs. Un seul bloc → regarder une édition ou un bord ; une ligne entière → `ParallelFor` ; tout → seed, échéancier ou grille différents (comparer `grid_hash` et `ScheduleId` d'abord).

## `audit [dossier]`
Grep et lecture dans `Plugins/Tidemark/Source` (ou le dossier donné) :
| Symptôme | Chercher | Verdict |
|---|---|---|
| Hasard non seedé | `FMath::Rand`, `FRand`, `RandRange`, `VRand`, `FRandomStream(` sans `FTidemarkSeed` | interdit hors `Tests/` |
| Temps réel | `DeltaTime`, `FPlatformTime`, `FDateTime::Now/UtcNow`, `GetTimeSeconds()` dans le solveur ou le journal | interdit : seul `StepIndex` et l'horloge répliquée |
| Réduction parallèle | `ParallelFor` avec accumulation (`+=`, `Min`, `Max`) hors double buffer, `Atomics` | interdit |
| Ordre d'itération | `TMap`/`TSet` itérés pour produire un état, tri non stable | remplacer par tableau ordonné |
| Flottants | `FMath::Sqrt` vs `sqrtf`, `-ffast-math`, FMA : même binaire pour tous les joueurs ? | V1 : un seul binaire ; noter pour V2 (point fixe) |
| Bord de grille | cellules `Fixed`/`Border` lues avant d'être écrites | vérifier l'ordre des passes |
Rapport : chaque occurrence avec `fichier:ligne`, verdict, correctif proposé au rôle propriétaire. Aucune modification dans ce skill.

## Preuve
`testé éditeur` pour les specs ; `testé multi-machines` seulement avec un `diff` entre deux PC réels (voir `/tidemark-netcheck`).
