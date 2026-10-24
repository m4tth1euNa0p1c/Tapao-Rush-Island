#!/usr/bin/env python3
"""Assemble le dépôt `tidemark-unreal` à partir de Tapao-Rush-Island.

Usage : python scripts/tidemark/export_repo.py <chemin du dépôt vierge> [--force]

Copie le plugin, les rôles et skills Tidemark, les docs du produit, les vérifications CI et leurs tests ;
écrit un projet hôte UE 5.8 minimal, CLAUDE.md, README, .gitignore, .gitattributes, .env.example, specs/.
Réécrit les chemins `monke_island/Plugins/Tidemark` -> `Plugins/Tidemark`. Ne touche jamais au dépôt source.
"""
from __future__ import annotations

import argparse
import re
import shutil
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
PRODUIT = ROOT / "docs" / "produit"
BOOT = PRODUIT / "tidemark" / "bootstrap"

OLD_PATH = "monke_island/Plugins/Tidemark"
NEW_PATH = "Plugins/Tidemark"
TEXT_SUFFIXES = {".py", ".md", ".yml", ".yaml", ".json", ".cs", ".h", ".cpp", ".uplugin", ".txt", ".ps1"}

HOST = "TidemarkHost"


def rewrite_paths(text: str) -> str:
	"""Chemins du dépôt Tapao -> chemins du dépôt du plugin."""
	return text.replace(OLD_PATH, NEW_PATH).replace("monke_island/Saved/", "Saved/")


def copy_tree(src: Path, dst: Path, rewrite: bool) -> int:
	count = 0
	for path in src.rglob("*"):
		if path.is_dir():
			continue
		if any(part in {"Binaries", "Intermediate", "Saved", "__pycache__"} for part in path.parts):
			continue
		rel = path.relative_to(src)
		target = dst / rel
		target.parent.mkdir(parents=True, exist_ok=True)
		if rewrite and path.suffix in TEXT_SUFFIXES:
			text = path.read_text(encoding="utf-8")
			target.write_text(rewrite_paths(text), encoding="utf-8")
		else:
			shutil.copy2(path, target)
		count += 1
	return count


def copy_file(src: Path, dst: Path, rewrite: bool = True) -> None:
	dst.parent.mkdir(parents=True, exist_ok=True)
	if rewrite and src.suffix in TEXT_SUFFIXES:
		dst.write_text(rewrite_paths(src.read_text(encoding="utf-8")), encoding="utf-8")
	else:
		shutil.copy2(src, dst)


def write(dst: Path, content: str) -> None:
	dst.parent.mkdir(parents=True, exist_ok=True)
	dst.write_text(content, encoding="utf-8")


UPROJECT = """{
	"FileVersion": 3,
	"EngineAssociation": "5.8",
	"Category": "",
	"Description": "Host project of the Tidemark plugin: compiles, tests and packages the plugin. No gameplay.",
	"Modules": [
		{
			"Name": "TidemarkHost",
			"Type": "Runtime",
			"LoadingPhase": "Default"
		}
	],
	"Plugins": [
		{
			"Name": "Tidemark",
			"Enabled": true
		}
	]
}
"""

TARGET_CS = """using UnrealBuildTool;
using System.Collections.Generic;

public class {name}Target : TargetRules
{{
	public {name}Target(TargetInfo Target) : base(Target)
	{{
		Type = TargetType.{type};
		DefaultBuildSettings = BuildSettingsVersion.V7;
		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_8;
		ExtraModuleNames.Add("TidemarkHost");
	}}
}}
"""

MODULE_BUILD_CS = """using UnrealBuildTool;

public class TidemarkHost : ModuleRules
{
	public TidemarkHost(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine" });
	}
}
"""

MODULE_H = """// Tidemark host project

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/** Empty primary module of the host project: it only exists so that UBT builds the Tidemark plugin. */
class FTidemarkHostModule : public IModuleInterface
{
};
"""

MODULE_CPP = """// Tidemark host project

#include "TidemarkHost.h"

IMPLEMENT_PRIMARY_GAME_MODULE(FTidemarkHostModule, TidemarkHost, "TidemarkHost");
"""

DEFAULT_ENGINE_INI = """[/Script/EngineSettings.GameMapsSettings]
GameDefaultMap=/Engine/Maps/Entry.Entry
EditorStartupMap=/Engine/Maps/Entry.Entry

[/Script/Engine.RendererSettings]
r.DefaultFeature.AutoExposure=False
"""

DEFAULT_GAME_INI = """[/Script/EngineSettings.GeneralProjectSettings]
ProjectName=Tidemark Host
ProjectID=7A1D3E2C4B5F4F0A9C8D1E2F3A4B5C6D
CompanyName=Matthieu Barraque
Description=Host project of the Tidemark plugin
"""

GITIGNORE = """# Unreal generated
Binaries/
Intermediate/
Saved/
DerivedDataCache/
.vs/
*.sln
*.slnx
*.suo
*.VC.db
Plugins/*/Binaries/
Plugins/*/Intermediate/

# Secrets and local data: never committed
.env
.env.*
!.env.example
.envgrid-cache/
*.pem
*.key

# Python
__pycache__/
*.pyc
.venv/

# Claude Code local
.claude/settings.local.json

# Build output
build/
"""

GITATTRIBUTES = """# Unreal binary assets under Git LFS
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap filter=lfs diff=lfs merge=lfs -text
*.png filter=lfs diff=lfs merge=lfs -text
*.jpg filter=lfs diff=lfs merge=lfs -text
*.tga filter=lfs diff=lfs merge=lfs -text
*.tif filter=lfs diff=lfs merge=lfs -text
*.exr filter=lfs diff=lfs merge=lfs -text
*.r16 filter=lfs diff=lfs merge=lfs -text
*.bin filter=lfs diff=lfs merge=lfs -text
*.wav filter=lfs diff=lfs merge=lfs -text
*.fbx filter=lfs diff=lfs merge=lfs -text
*.mp4 filter=lfs diff=lfs merge=lfs -text

# Line endings
*.cs text eol=crlf
*.h text eol=crlf
*.cpp text eol=crlf
*.uproject text eol=crlf
*.uplugin text eol=crlf
*.ini text eol=crlf
*.py text eol=lf
*.md text eol=lf
*.yml text eol=lf
*.json text eol=lf
"""

README = """# Tidemark

**Deterministic sea-level rise for Unreal Engine 5.8.** Bakes any terrain (Landscape, heightmap, meshes) into a grid, simulates the flood on CPU with a seed so every machine sees the same tide, and tells level designers in the editor what floods first — plausible, never a forecast.

- Contrat produit : `docs/CAHIER_DES_CHARGES_TIDEMARK.md` · Conventions : `docs/CONVENTIONS.md` · Backlog : `docs/backlog.md` · Porte S2 : `docs/gate-S2.md`
- Méthode : développement piloté par les specs, `specs/README.md`
- Plugin : `Plugins/Tidemark/` (modules `TidemarkRuntime`, `TidemarkEditor`), format ouvert `.tmk` (`Plugins/Tidemark/Docs/TMK.md`)
- Projet hôte : `TidemarkHost.uproject` (aucun gameplay, sert à compiler et tester)

## Démarrer (Windows, UE 5.8)
```powershell
git lfs install ; git clone <ce dépôt> ; cd tidemark-unreal
copy .env.example .env   # puis renseigner les clés Copernicus, jamais committées
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 build
powershell -NoProfile -ExecutionPolicy Bypass -File scripts/ue.ps1 test -Filter Tidemark
```

## État
Semaines 1 et 2 du plan écrites hors moteur : **non compilées**. Premier objectif de ce dépôt : build et specs vertes (voir `KICKOFF.md`).

> Simulation ludique, non calibrée, impropre à toute évaluation de risque ou à la navigation.
"""

UE_PS1 = r"""param(
	[Parameter(Position = 0)][ValidateSet("build", "test", "editor", "clean")][string]$Command = "build",
	[string]$Filter = "Tidemark",
	[string]$Engine = "C:/Program Files/Epic Games/UE_5.8"
)
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$Project = Join-Path $Root "TidemarkHost.uproject"
$Ubt = Join-Path $Engine "Engine/Build/BatchFiles/Build.bat"
$EditorCmd = Join-Path $Engine "Engine/Binaries/Win64/UnrealEditor-Cmd.exe"
$EditorExe = Join-Path $Engine "Engine/Binaries/Win64/UnrealEditor.exe"

switch ($Command) {
	"build" {
		& $Ubt TidemarkHostEditor Win64 Development -Project="$Project" -WaitMutex -FromMsBuild
		if ($LASTEXITCODE -ne 0) { throw "Build failed ($LASTEXITCODE)" }
	}
	"test" {
		$Report = Join-Path $Root "Saved/Automation/Reports"
		& $EditorCmd "$Project" -unattended -nopause -nosplash -NullRHI -log `
			-ExecCmds="Automation RunTests $Filter; Quit" -ReportExportPath="$Report" -TestExit="Automation Test Queue Empty"
		Write-Host "Report: $Report/index.json"
	}
	"editor" { & $EditorExe "$Project" }
	"clean" { Remove-Item -Recurse -Force (Join-Path $Root "Binaries"), (Join-Path $Root "Intermediate"), (Join-Path $Root "Plugins/Tidemark/Binaries"), (Join-Path $Root "Plugins/Tidemark/Intermediate") -ErrorAction SilentlyContinue }
}
"""

SPEC_001 = """# SPEC-001 — Cœur du plugin (semaines 1 et 2)

- **Statut** : validée a posteriori (le code existe, **non compilé**) · **Lot** : S1–S2 · **Tickets** : TM-01 → TM-11 · **Rôles** : lead, solver, terrain, qa, release.

## 1. Intention
Poser le socle que tout le reste consomme : un solveur d'eaux peu profondes déterministe, une seed unique, un hash d'état par bloc, un échéancier de niveau de mer pur, une grille de terrain int16 cm avec asset et format ouvert, trois importeurs (heightmap, Landscape, collision), et la CI qui interdit toute dépendance au jeu et tout hasard non seedé.

## 2. Contrat public (tel qu'écrit ; voir les headers de `Plugins/Tidemark/Source/`)
- `FTidemarkSolver` (ex-`FFloodGrid`, mètres et secondes) : `Init`, `SetupSea`, `Step(Dt, SeaLevel, Gravity, FluxDamping, bForceSingleThread)`, `Ground`, `Depth`, `Fixed`, `Surface`, `FreeVolume`.
- `FTidemarkSeed` (`FromValue`, `FromName`, `Derive`, `ToRandomStream`), `FTidemarkRng` (SplitMix64).
- `Tidemark::Hash` : `Fnv1a64`, `HashDepth` (blocs 32×32, quantum 1 mm), `DiffBlocks`.
- `FSeaLevelSchedule` + `Tidemark::Schedule::LevelCm/HeldLevelCm/StepsFromSeconds`.
- `UTidemarkRules` (toutes les valeurs).
- `FTidemarkGrid` (cm, flags OpenSea/Border/Forbidden, `FillSolver`), `UTidemarkTerrainAsset` (hash des sources, crédits), `Tidemark::Format::SaveTmk/LoadTmk`, `Tools/tmk.py`.
- `ITidemarkHeightSource`, `Tidemark::Bake::BakeGrid`, sources texture, Landscape, collision ; `UTidemarkImportLibrary` ; commandes `tidemark.BakeLandscape`, `tidemark.BakeCollision`, `tidemark.ImportTmk`, `tidemark.ExportTmk`.

## 3. Règles
`docs/CONVENTIONS.md` §5 (déterminisme), §7 (licences). Incohérences connues à corriger en TM-13 : `docs/gate-S2.md` §2 (A1–A8).

## 4. Valeurs tunables
Toutes dans `UTidemarkRules` ; défauts dans `Tidemark::Rules::Default*`.

## 5. Critères d'acceptation → specs
108 `It(` dans `Tidemark.Solver.*`, `Seed.*`, `Hash.*`, `Determinism.*`, `Schedule.*`, `Grid.*`, `Format.*`, `Import.Bake.*`, `Import.Collision.*`, `Import.Scenario.*`. **Aucun exécuté.**

## 6. Preuve exigée
`scripts/ue.ps1 build` 0 erreur ; `scripts/ue.ps1 test -Filter Tidemark` : rapport joint ; dérive `VolumeConserved` consignée (D-1).

## 7. Plan
Fait : voir `docs/backlog.md` S1–S2. Reste : le build.

## 8. Écarts et décisions
Voir `docs/backlog.md` « Décisions ouvertes » D-1 à D-5 et `docs/gate-S2.md`.
"""

SPEC_003 = """# SPEC-003 — Subsystem de simulation, horloge, journal (semaine 3)

- **Statut** : brouillon (à compléter par `tidemark-lead` **avant** tout code de TM-13) · **Lot** : S3 · **Tickets** : TM-13, TM-14, TM-15 · **Rôles** : solver, qa.

## 1. Intention
Faire tourner le solveur en jeu : un `UTidemarkSubsystem` à pas fixe compté depuis l'horloge, un journal d'éditions `Dig/Fill/Raise` appliqué au pas exact, le hash exposé, le rejeu, et les correctifs d'architecture A1–A8 de `docs/gate-S2.md`.

## 2. Contrat public (à écrire)
- `UTidemarkSubsystem : UTickableWorldSubsystem` : `Start(const UTidemarkTerrainAsset*, const UTidemarkRules*, FSeaLevelSchedule, FTidemarkSeed, double StartServerTime)`, `GetStepIndex()`, `GetHash()`, `GetBlockHashes()`, `SampleWater(FVector)`, `ApplyEdit(const FTidemarkEdit&)`, `GetJournal()`.
- `FTidemarkEdit { FIntRect Cells; ETidemarkEditOp Op; int16 ValueCm; FName Cause; uint32 ServerStep; }`.
- `Tidemark::Replay(Seed, Grid, Schedule, Edits, Steps) -> FTidemarkStateHash`.
- Correctifs : A1 (`FillSolver` sort de `Grid/` vers `Solver/TidemarkSolverFactory` ou le subsystem), A2 (une seule source de vérité pour `Fixed`), A3 (gravité/amortissement lus des règles seulement), A4 (un seul `bHoldRise` effectif), A5 (unités : cm partout, conversion en un seul point documenté ; mettre `CONVENTIONS.md` §4 à jour), A6 (`SimGridCells` appliqué par le baker ou le subsystem), A7 (tag `IslandWaterSurface` retiré : liste de tags dans les règles), A8 (code mort supprimé).

## 3. Règles
Pas de `DeltaTime` dans le solveur ; éditions au pas exact ; une édition en retard → rejeu depuis le dernier instantané ; aucun hasard non seedé.

## 4. Valeurs tunables
`StepSeconds`, `MaxStepsPerFrame`, `MaxCatchUpSteps`, `SimGridCells`, `SimCellSizeCm`, `HashIntervalSeconds` (existants) ; à ajouter : `SnapshotIntervalSteps`.

## 5. Critères d'acceptation (à numéroter, chacun → un `It(`)
Rejeu identique ; édition appliquée au pas exact ; édition en retard rejouée ; `SampleWater` cohérent avec `Depth` ; coût ≤ 1 ms moyen / 2 ms pic sur 512² (TM-15, mesuré).

## 6. Preuve exigée
`testé éditeur` + profil CSV ou Insights (TM-15).

## 7. Plan
TM-13 (subsystem + A1–A5, A8) → TM-14 (journal, rejeu) → TM-15 (profil). A7 et D-3 peuvent partir sans build.

## 8. Écarts et décisions
—
"""


def main() -> int:
	parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
	parser.add_argument("dest", type=Path)
	parser.add_argument("--force", action="store_true", help="écrit même si le dossier n'est pas vide (ne supprime rien)")
	args = parser.parse_args()
	dest: Path = args.dest.resolve()
	dest.mkdir(parents=True, exist_ok=True)
	if any(p.name != ".git" for p in dest.iterdir()) and not args.force:
		print(f"ÉCHEC : {dest} n'est pas vide (hors .git). Relance avec --force pour écrire par-dessus.", file=sys.stderr)
		return 1

	total = 0
	# Plugin
	total += copy_tree(ROOT / "monke_island" / "Plugins" / "Tidemark", dest / "Plugins" / "Tidemark", rewrite=True)
	# Agents et skills Tidemark
	for agent in (ROOT / ".claude" / "agents").glob("tidemark-*.md"):
		copy_file(agent, dest / ".claude" / "agents" / agent.name)
		total += 1
	for skill in (ROOT / ".claude" / "skills").glob("tidemark*"):
		total += copy_tree(skill, dest / ".claude" / "skills" / skill.name, rewrite=True)
	# Templates de classes (réutilisés par /ue-new-class dans les fiches)
	total += copy_tree(ROOT / ".claude" / "skills" / "ue-new-class", dest / ".claude" / "skills" / "ue-new-class", rewrite=True)
	# Docs produit
	copy_file(PRODUIT / "CAHIER_DES_CHARGES_TIDEMARK.md", dest / "docs" / "CAHIER_DES_CHARGES_TIDEMARK.md")
	for name in ("CONVENTIONS.md", "backlog.md", "gate-S2.md", "benchmark.md"):
		src = PRODUIT / "tidemark" / name
		if src.exists():
			copy_file(src, dest / "docs" / name)
			total += 1
	total += copy_tree(PRODUIT / "debat-2026-10-23", dest / "docs" / "debat-2026-10-23", rewrite=True)
	for name in ("01_REPO_MAP.md", "02_ARCHITECTURE_PROPOSAL.md"):
		copy_file(ROOT / "docs" / "audit" / name, dest / "docs" / "audit-tapao" / name)
		total += 1
	# Scripts et tests
	for script in (ROOT / "scripts" / "tidemark").glob("*.py"):
		if script.name == "export_repo.py":
			continue
		copy_file(script, dest / "scripts" / "tidemark" / script.name)
		total += 1
	copy_file(ROOT / "scripts" / "tidemark" / "README.md", dest / "scripts" / "tidemark" / "README.md")
	copy_file(ROOT / "scripts" / "tests" / "test_tidemark_checks.py", dest / "scripts" / "tests" / "test_tidemark_checks.py")
	copy_file(ROOT / ".github" / "workflows" / "tidemark.yml", dest / ".github" / "workflows" / "tidemark.yml")
	# Bootstrap propre au nouveau dépôt
	copy_file(BOOT / "CLAUDE.md", dest / "CLAUDE.md")
	copy_file(BOOT / "KICKOFF.md", dest / "KICKOFF.md")
	copy_file(BOOT / "env.example", dest / ".env.example")
	total += copy_tree(BOOT / "specs", dest / "specs", rewrite=True)
	write(dest / "specs" / "SPEC-001-plugin-core.md", SPEC_001)
	write(dest / "specs" / "SPEC-003-simulation-subsystem.md", SPEC_003)
	# Projet hôte
	write(dest / f"{HOST}.uproject", UPROJECT)
	write(dest / "Source" / f"{HOST}.Target.cs", TARGET_CS.format(name=HOST, type="Game"))
	write(dest / "Source" / f"{HOST}Editor.Target.cs", TARGET_CS.format(name=HOST + "Editor", type="Editor"))
	write(dest / "Source" / HOST / f"{HOST}.Build.cs", MODULE_BUILD_CS)
	write(dest / "Source" / HOST / f"{HOST}.h", MODULE_H)
	write(dest / "Source" / HOST / f"{HOST}.cpp", MODULE_CPP)
	write(dest / "Config" / "DefaultEngine.ini", DEFAULT_ENGINE_INI)
	write(dest / "Config" / "DefaultGame.ini", DEFAULT_GAME_INI)
	write(dest / "Content" / ".gitkeep", "")
	write(dest / "scripts" / "ue.ps1", UE_PS1)
	write(dest / "scripts" / "envgrid" / "README.md", "# envgrid\n\nPipeline de données ouvertes : voir `specs/SPEC-002-open-data-pipeline.md`. Clés dans `.env`, jamais committées.\n")
	write(dest / ".gitignore", GITIGNORE)
	write(dest / ".gitattributes", GITATTRIBUTES)
	write(dest / "README.md", README)

	# Le workflow CI ne doit plus filtrer sur l'ancien chemin ni ignorer LFS pour rien : déjà réécrit par copy_file.
	# Garde-fou : aucun chemin Tapao restant dans les textes copiés.
	leftovers = []
	for path in dest.rglob("*"):
		if path.is_file() and path.suffix in TEXT_SUFFIXES and ".git" not in path.parts and "debat-2026-10-23" not in path.parts and "audit-tapao" not in path.parts:
			if OLD_PATH in path.read_text(encoding="utf-8", errors="ignore"):
				leftovers.append(str(path.relative_to(dest)))
	if leftovers:
		print("ATTENTION : anciens chemins restants :", *leftovers, sep="\n  ")

	print(f"OK : {total} fichiers copiés vers {dest}")
	print("Suite : cd <dest> ; git add -A ; git commit -m 'tidemark: kit de démarrage (plugin S1-S2 non compilé, specs, CI)' ; git push")
	print("Puis ouvrir Claude Code dans <dest> et coller KICKOFF.md.")
	return 0


if __name__ == "__main__":
	sys.exit(main())
