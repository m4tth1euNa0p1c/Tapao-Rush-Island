# Tidemark — conventions du plugin

Contrat : `docs/produit/CAHIER_DES_CHARGES_TIDEMARK.md` (abrégé « cahier Tidemark »). Ce fichier fixe ce que tous les agents de la cellule Tidemark respectent. Il complète `CLAUDE.md` et `docs/conventions/` ; en cas de conflit sur le plugin, ce fichier prime.

## 1. Emplacement et indépendance
- Le plugin vit dans **`monke_island/Plugins/Tidemark/`** pendant le développement (il compile avec le projet). Il est extrait dans son propre dépôt à la semaine 7 (skill `/tidemark-release`).
- **Zéro dépendance vers le module `monke_island`** : aucun `#include` de `Source/monke_island/`, aucune référence à un asset `/Game/MonkeIsland/`. C'est Tapao qui dépend du plugin, jamais l'inverse. Un test automatisé (`Tidemark.Build.NoGameDependency`, grep en CI) le vérifie.
- Pas de plugin payant, pas de dépendance à un plugin hors moteur. Modules moteur autorisés : Core, CoreUObject, Engine, NetCore, RenderCore, RHI, Niagara (Runtime) ; UnrealEd, Landscape, MeshDescription, Slate, SlateCore, ToolMenus, EditorFramework (Editor).

## 2. Structure
```
Plugins/Tidemark/
  Tidemark.uplugin
  Source/TidemarkRuntime/        (Runtime)
    Public/Tidemark/…            include "Tidemark/X.h"
    Private/…
    Private/Tests/…              specs
  Source/TidemarkEditor/         (Editor)
    Public/TidemarkEditor/…
    Private/…
    Private/Tests/…
  Content/                       matériau d'exemple, MPC, Niagara DI, map de démo
  Docs/                          README utilisateur, CHANGELOG, LICENSES/
  Tools/                         lecteur Python de .tmk, scripts
```
Dossiers logiques dans `TidemarkRuntime` : `Grid/` (couche A), `Solver/` (couche B), `Net/`, `Query/` (couche C), `Render/`, `Rules/`.

## 3. Nommage
- Préfixes Epic (`A/U/F/E/I/T`), macro d'export `TIDEMARKRUNTIME_API` / `TIDEMARKEDITOR_API`.
- Toutes les classes commencent par `Tidemark` : `FTidemarkGrid`, `FTidemarkSolver`, `UTidemarkSubsystem`, `UTidemarkRules`, `UTidemarkTerrainAsset`, `UTidemarkQuery`.
- Fonctions pures dans le namespace `Tidemark::` (`Tidemark::Solver`, `Tidemark::Schedule`, `Tidemark::Hash`).
- Tags : `Tidemark.Flood.Reached`, `Tidemark.Path.Cut`.
- Log : `DECLARE_LOG_CATEGORY_EXTERN(LogTidemark, Log, All)` et `LogTidemarkNet` pour la synchronisation.
- Console : `tidemark.*` (`tidemark.Overlay`, `tidemark.Lint`, `tidemark.Replay <seed>`, `tidemark.Hash`).
- Commandlet : `-run=Tidemark <analyze|explain|lint|replay|schedule>` (sortie JSON sur stdout).

## 4. Unités et types
- Grille et solveur en **centimètres** (`int16` pour l'altitude de grille, `float` dans le solveur : `FFloodGrid` déplacé tel quel, pas de point fixe en V1).
- Temps en pas de simulation (`uint32 StepIndex`), pas fixe `StepSeconds` = 0,05 s par défaut dans `UTidemarkRules`. Jamais de `DeltaTime` dans le solveur.
- Seed : `FTidemarkSeed` 64 bits, sous-flux par `Derive(FName)`. **Interdits** : `FMath::Rand*`, `FDateTime::Now/UtcNow`, `FPlatformTime::Cycles` dans tout code qui touche l'état de simulation.
- Aucun nombre de gameplay en dur : tout dans `UTidemarkRules` (`EditDefaultsOnly`) ou les paramètres explicites des fonctions pures.

## 5. Déterminisme (règle d'or)
- L'état qui fait foi est calculé sur CPU, en ordre d'itération fixe. `ParallelFor` seulement quand chaque cellule ne lit que l'étape précédente (double buffer) ; jamais de réduction parallèle non ordonnée.
- Chaque changement d'état passe par le journal (`FTidemarkEdit`) au pas serveur exact.
- `FTidemarkHash` (FNV-1a 64, par bloc 32×32) est calculable à tout instant et testé.
- Un scénario est reproductible depuis `(seed, hash de grille, échéancier, journal)` : spec `Tidemark.Replay.*`.
- Le GPU n'est jamais autoritaire ; le pont de rendu lit, ne calcule pas.

## 6. Tests et preuves
- Automation Spec par cas, chemin **`Tidemark.<Feature>.<Cas>`**, flags `EditorContext | ProductFilter`, template `.claude/skills/ue-new-class/templates/AutomationTest.cpp.tpl`.
- Specs obligatoires : `Tidemark.Solver.*` (reprises de `Flood/Tests/`), `Tidemark.Determinism.*`, `Tidemark.Replay.*`, `Tidemark.Import.*`, `Tidemark.Lint.*`, `Tidemark.Net.*`.
- Niveaux de preuve, comme le cahier Tapao §2 : `compilé` / `testé éditeur` / `testé build` / `testé multi-machines`. Jamais `fait` sans preuve.
- Commandes : `powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 build` (compile le projet et ses plugins), `… ue.ps1 test -Filter Tidemark`, `… ue.ps1 profile`. Verrou d'éditeur `scripts/editor_lock.py` comme pour le jeu.

## 7. Données et licences
- Le plugin V1 n'embarque **aucune donnée géographique**. L'île de démo « inspirée d'un atoll » n'utilise que la côte Te Fenua (Etalab 2.0, producteur et date) et un relief estimé déclaré tel ; toponymes fictifs.
- Interdits partout : imagerie Maxar et ses dérivés, cocoteraies DAG, raster IRD sans licence, Google 3D Tiles, OSM dans une grille redistribuée (tant que l'ODbL n'est pas tranchée par un juriste).
- `UTidemarkTerrainAsset.Credits` et `.tmk` `credits[]` : producteur, licence, URL, date. La CI refuse un manifest incomplet.
- Mention non désactivable dans les sorties de lint et de rapport : « Simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation. »
- `PROVENANCE.json` pour chaque asset livré dans `Content/` (outil, date, licence, SHA-256).

## 8. Style
- Style Epic (tabs, accolades à la ligne, `/** */` sur chaque `UPROPERTY`/`UFUNCTION`, `Category` obligatoire), `TObjectPtr` + `UPROPERTY()` pour les membres UObject, tick désactivé par défaut.
- Code et identifiants en anglais, commentaires courts en anglais, docs en français.
- Un fichier modifié par un agent est un fichier de sa tâche ; fichiers disjoints entre tâches d'une même vague.
