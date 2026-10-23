# Tidemark — montée des eaux déterministe pour Unreal Engine 5.8

Tidemark simule une montée du niveau de la mer sur une grille de terrain, **sur CPU, à pas fixe, seedée, journalisée et hashable**. Le même scénario (seed, grille, échéancier, journal) donne le même état sur toutes les machines ; l'éditeur peut lire ce que la simulation fera avant de lancer le jeu (ordre d'inondation, coupure d'une route, instant d'arrivée en un point).

> Simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation.

## Une seule menace en V1 : l'eau
Pas de vent, d'érosion ni de cyclone. Pas de données géographiques embarquées : le plugin vient avec une île de démo fictive.

## Trois couches (dépendances vers le bas seulement)
| Couche | Contenu | Dossier (`TidemarkRuntime`) |
|---|---|---|
| **A — Terrain** | `FTidemarkGrid` (SoA, `int16` cm, flags), `UTidemarkTerrainAsset`, format `.tmk`, importateurs (éditeur) | `Grid/`, `TidemarkEditor/Import/` |
| **B — Simulation** | `FTidemarkSolver` (pas fixe), `FTidemarkSeed`, `FSeaLevelSchedule`, `FTidemarkEdit` + journal, `FTidemarkHash` (par bloc 32×32), `UTidemarkSubsystem`, `UTidemarkRules`, composants réseau | `Solver/`, `Rules/`, `Net/` |
| **C — Lecture** | `UTidemarkQuery` (`TimeToFlood`, `FloodOrder`, `IsPassable`, `SampleDepth`), bake first-wet, lint, `UTidemarkRenderBridge` | `Query/`, `Render/`, `TidemarkEditor/` |

L'état qui fait foi est calculé sur CPU ; le GPU et le rendu lisent. Rien n'est en dur : toutes les valeurs sont dans `UTidemarkRules`.

## Modules
- `TidemarkRuntime` (Runtime) : Core, CoreUObject, Engine ; privé : NetCore, RenderCore, RHI, Niagara. Logs `LogTidemark`, `LogTidemarkNet`.
- `TidemarkEditor` (Editor) : + UnrealEd, Landscape, MeshDescription, Slate, SlateCore, ToolMenus, EditorFramework, Projects. Log `LogTidemarkEditor`.

Le plugin ne dépend d'aucun module de jeu ni d'aucun plugin hors moteur.

## Installation
1. Copier `Tidemark/` dans `<Projet>/Plugins/`.
2. Dans le `.uproject`, ajouter `{ "Name": "Tidemark", "Enabled": true }` sous `Plugins`.
3. Compiler (le plugin est en C++).

## Lancer les specs
Depuis la racine du dépôt (éditeur fermé, verrou pris) :
```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 build
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 test -Filter Tidemark
```
Chemin des specs : `Tidemark.<Feature>.<Cas>` (`Tidemark.Solver.*`, `Tidemark.Determinism.*`, `Tidemark.Replay.*`, `Tidemark.Import.*`, `Tidemark.Lint.*`, `Tidemark.Net.*`), flags `EditorContext | ProductFilter`. Les specs vivent dans `Source/<Module>/Private/Tests/`.

## Console et CLI
- Console : `tidemark.Overlay`, `tidemark.Lint`, `tidemark.Replay <seed>`, `tidemark.Hash`.
- Commandlet : `-run=Tidemark <analyze|explain|lint|replay|schedule>` (JSON sur stdout).

## Licences et provenance
`Docs/LICENSES/` contient les textes des licences des contenus livrés ; `PROVENANCE.json` liste chaque asset de `Content/` (outil, date, licence, SHA-256). Voir `CHANGELOG.md` pour l'historique.
