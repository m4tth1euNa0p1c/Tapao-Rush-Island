// Tidemark plugin

#include "TidemarkEditor/Import/TidemarkImportLibrary.h"
#include "TidemarkEditor/Import/LandscapeHeightSource.h"
#include "TidemarkEditor/Import/TidemarkBaker.h"
#include "TidemarkEditor/TidemarkEditorModule.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "Landscape.h"
#include "LandscapeProxy.h"
#include "Misc/PackageName.h"
#include "Tidemark/Grid/TidemarkFormat.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "Tidemark/Rules/TidemarkRules.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

FTidemarkBakeSettings UTidemarkImportLibrary::MakeBakeSettings(const UTidemarkRules* Rules, float SeaLevelCm, float OpenSeaMarginCm)
{
	FTidemarkBakeSettings Settings;
	Settings.CellSizeCm = Rules ? Rules->SimCellSizeCm : Tidemark::Rules::DefaultSimCellSizeCm;
	Settings.OpenSeaDepthCm = Rules ? Rules->OpenSeaDepthCm : Tidemark::Rules::DefaultOpenSeaDepthCm;
	Settings.SeaLevelCm = SeaLevelCm;
	Settings.OpenSeaMarginCm = FMath::Max(OpenSeaMarginCm, 0.0f);
	return Settings;
}

bool UTidemarkImportLibrary::ValidateCredits(const TArray<FTidemarkCredit>& Credits, FString& OutError)
{
	if (Credits.Num() == 0)
	{
		OutError = TEXT("credits are mandatory: at least one entry with producer, license, url and date");
		return false;
	}
	for (int32 Index = 0; Index < Credits.Num(); ++Index)
	{
		if (!Credits[Index].IsComplete())
		{
			OutError = FString::Printf(TEXT("credit %d is incomplete: producer, license, url and date are all required"), Index);
			return false;
		}
	}
	return true;
}

ALandscapeProxy* UTidemarkImportLibrary::FindEditorLandscape()
{
	if (!GEditor)
	{
		return nullptr;
	}
	UWorld* World = GEditor->GetEditorWorldContext().World();
	if (!World)
	{
		return nullptr;
	}
	ALandscapeProxy* FirstProxy = nullptr;
	for (TActorIterator<ALandscapeProxy> It(World); It; ++It)
	{
		if (Cast<ALandscape>(*It))
		{
			return *It;
		}
		if (!FirstProxy)
		{
			FirstProxy = *It;
		}
	}
	return FirstProxy;
}

UTidemarkTerrainAsset* UTidemarkImportLibrary::FindOrCreateAsset(const FString& PackagePath, FString& OutError)
{
	if (!FPackageName::IsValidLongPackageName(PackagePath))
	{
		OutError = FString::Printf(TEXT("'%s' is not a long package name (expected /Game/... or /Tidemark/...)"), *PackagePath);
		return nullptr;
	}
	const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
	if (AssetName.IsEmpty())
	{
		OutError = FString::Printf(TEXT("'%s' has no asset name"), *PackagePath);
		return nullptr;
	}

	UPackage* Package = CreatePackage(*PackagePath);
	if (!Package)
	{
		OutError = FString::Printf(TEXT("cannot create package '%s'"), *PackagePath);
		return nullptr;
	}
	Package->FullyLoad();

	UTidemarkTerrainAsset* Asset = FindObject<UTidemarkTerrainAsset>(Package, *AssetName);
	if (!Asset)
	{
		if (FindObject<UObject>(Package, *AssetName))
		{
			OutError = FString::Printf(TEXT("'%s' exists and is not a Tidemark terrain asset"), *PackagePath);
			return nullptr;
		}
		Asset = NewObject<UTidemarkTerrainAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		if (!Asset)
		{
			OutError = FString::Printf(TEXT("cannot create asset '%s'"), *PackagePath);
			return nullptr;
		}
		FAssetRegistryModule::AssetCreated(Asset);
	}
	return Asset;
}

bool UTidemarkImportLibrary::SaveAssetPackage(UTidemarkTerrainAsset* Asset, FString& OutError)
{
	UPackage* Package = Asset ? Asset->GetOutermost() : nullptr;
	if (!Package)
	{
		OutError = TEXT("asset has no package");
		return false;
	}
	Package->MarkPackageDirty();
	const FString FileName = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
	FSavePackageArgs SaveArgs;
	SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
	SaveArgs.SaveFlags = SAVE_NoError;
	if (!UPackage::SavePackage(Package, Asset, *FileName, SaveArgs))
	{
		OutError = FString::Printf(TEXT("save of '%s' failed (see the log)"), *FileName);
		return false;
	}
	return true;
}

UTidemarkTerrainAsset* UTidemarkImportLibrary::SaveGridAsAsset(const FTidemarkGrid& Grid, const TArray<FTidemarkCredit>& Credits, const FString& PackagePath, FString& OutError)
{
	if (!ValidateCredits(Credits, OutError))
	{
		return nullptr;
	}
	if (!Grid.IsValid())
	{
		OutError = TEXT("grid is invalid");
		return nullptr;
	}
	UTidemarkTerrainAsset* Asset = FindOrCreateAsset(PackagePath, OutError);
	if (!Asset)
	{
		return nullptr;
	}
	Asset->Modify();
	Asset->Grid = Grid;
	Asset->Credits = Credits;
	Asset->SchemaVersion = UTidemarkTerrainAsset::CurrentSchemaVersion;
	Asset->RefreshSourceHash();
	if (!SaveAssetPackage(Asset, OutError))
	{
		return nullptr;
	}
	UE_LOG(LogTidemarkEditor, Log, TEXT("Tidemark terrain asset saved: %s (%d x %d, hash %s, %.2f MiB)"),
		*PackagePath, Grid.SizeX, Grid.SizeY, *Asset->SourceHashHex, static_cast<double>(Grid.NumBytes()) / (1024.0 * 1024.0));
	return Asset;
}

UTidemarkTerrainAsset* UTidemarkImportLibrary::BakeFromSource(const ITidemarkHeightSource& Source, const FTidemarkBakeSettings& Settings, const FString& PackagePath, const TArray<FTidemarkCredit>& Credits, FString& OutError, FTidemarkBakeResult& OutResult)
{
	OutResult = FTidemarkBakeResult();
	if (!ValidateCredits(Credits, OutError))
	{
		OutResult.Error = OutError;
		return nullptr;
	}
	FTidemarkGrid Grid;
	OutResult = Tidemark::Bake::BakeGrid(Source, Settings, Grid);
	if (!OutResult.bOk)
	{
		OutError = OutResult.Error;
		return nullptr;
	}
	return SaveGridAsAsset(Grid, Credits, PackagePath, OutError);
}

UTidemarkTerrainAsset* UTidemarkImportLibrary::BakeFromLandscape(ALandscapeProxy* Landscape, const FTidemarkBakeSettings& Settings, const FString& PackagePath, const TArray<FTidemarkCredit>& Credits, FString& OutError, FTidemarkBakeResult& OutResult)
{
	OutResult = FTidemarkBakeResult();
	if (!Landscape)
	{
		OutError = TEXT("no Landscape given");
		return nullptr;
	}
	if (!ValidateCredits(Credits, OutError))
	{
		return nullptr;
	}
	FTidemarkLandscapeHeightSource Source(Landscape);
	if (!Source.Prepare(OutError))
	{
		return nullptr;
	}
	return BakeFromSource(Source, Settings, PackagePath, Credits, OutError, OutResult);
}

UTidemarkTerrainAsset* UTidemarkImportLibrary::BakeFromHeightmapFile(const FString& FilePath, const FTidemarkTextureHeightParams& Params, const FTidemarkBakeSettings& Settings, const FString& PackagePath, const TArray<FTidemarkCredit>& Credits, FString& OutError, FTidemarkBakeResult& OutResult)
{
	OutResult = FTidemarkBakeResult();
	if (!ValidateCredits(Credits, OutError))
	{
		return nullptr;
	}
	FTidemarkTextureHeightSource Source;
	if (!Source.LoadFile(FilePath, Params, OutError))
	{
		return nullptr;
	}
	return BakeFromSource(Source, Settings, PackagePath, Credits, OutError, OutResult);
}

bool UTidemarkImportLibrary::ExportTmk(const UTidemarkTerrainAsset* Asset, const FString& Dir, FString& OutError)
{
	if (!Asset)
	{
		OutError = TEXT("no asset");
		return false;
	}
	if (!ValidateCredits(Asset->Credits, OutError))
	{
		return false;
	}
	return Tidemark::Format::SaveTmk(Asset->Grid, Asset->Credits, Dir, OutError);
}

UTidemarkTerrainAsset* UTidemarkImportLibrary::ImportTmk(const FString& Dir, const FString& PackagePath, FString& OutError)
{
	FTidemarkGrid Grid;
	TArray<FTidemarkCredit> Credits;
	if (!Tidemark::Format::LoadTmk(Dir, Grid, Credits, OutError))
	{
		return nullptr;
	}
	return SaveGridAsAsset(Grid, Credits, PackagePath, OutError);
}

// ---- Console commands (editor only). Static objects: registered when the module's DLL loads, removed when it unloads.

namespace
{
	/** Placeholder credit of a console bake: the project's own Landscape, to be completed by the user before shipping. */
	TArray<FTidemarkCredit> ConsoleCredits(const FString& What)
	{
		FTidemarkCredit Credit;
		Credit.Producer = What;
		Credit.License = TEXT("Project licence (edit the asset's Credits before shipping)");
		Credit.Url = TEXT("about:blank");
		Credit.Date = TEXT("unknown");
		return { Credit };
	}

	float ArgOr(const TArray<FString>& Args, int32 Index, float Default)
	{
		return Args.IsValidIndex(Index) ? FCString::Atof(*Args[Index]) : Default;
	}

	FAutoConsoleCommand GTidemarkBakeLandscapeCommand(
		TEXT("tidemark.BakeLandscape"),
		TEXT("tidemark.BakeLandscape <PackagePath> [CellSizeCm=600] [SeaLevelCm=0] [MarginCm=0]: bakes the first Landscape of the editor world into a UTidemarkTerrainAsset."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 1)
			{
				UE_LOG(LogTidemarkEditor, Warning, TEXT("usage: tidemark.BakeLandscape <PackagePath> [CellSizeCm] [SeaLevelCm] [MarginCm]"));
				return;
			}
			ALandscapeProxy* Landscape = UTidemarkImportLibrary::FindEditorLandscape();
			if (!Landscape)
			{
				UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeLandscape: no Landscape in the editor world"));
				return;
			}
			FTidemarkBakeSettings Settings = UTidemarkImportLibrary::MakeBakeSettings(nullptr, ArgOr(Args, 2, 0.0f), ArgOr(Args, 3, 0.0f));
			Settings.CellSizeCm = ArgOr(Args, 1, Settings.CellSizeCm);
			FString Error;
			FTidemarkBakeResult Result;
			UTidemarkTerrainAsset* Asset = UTidemarkImportLibrary::BakeFromLandscape(Landscape, Settings, Args[0],
				ConsoleCredits(FString::Printf(TEXT("Landscape %s of this project"), *Landscape->GetName())), Error, Result);
			if (Asset)
			{
				UE_LOG(LogTidemarkEditor, Display, TEXT("tidemark.BakeLandscape: %s baked in %.2f s (%d sampled, %d missing) -> %s"),
					*Landscape->GetName(), Result.Seconds, Result.Sampled, Result.Missing, *Args[0]);
			}
			else
			{
				UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeLandscape failed: %s"), *Error);
			}
		}));

	FAutoConsoleCommand GTidemarkImportTmkCommand(
		TEXT("tidemark.ImportTmk"),
		TEXT("tidemark.ImportTmk <Dir> <PackagePath>: imports a .tmk folder as a UTidemarkTerrainAsset."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogTidemarkEditor, Warning, TEXT("usage: tidemark.ImportTmk <Dir> <PackagePath>"));
				return;
			}
			FString Error;
			if (UTidemarkImportLibrary::ImportTmk(Args[0], Args[1], Error))
			{
				UE_LOG(LogTidemarkEditor, Display, TEXT("tidemark.ImportTmk: %s -> %s"), *Args[0], *Args[1]);
			}
			else
			{
				UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.ImportTmk failed: %s"), *Error);
			}
		}));

	FAutoConsoleCommand GTidemarkExportTmkCommand(
		TEXT("tidemark.ExportTmk"),
		TEXT("tidemark.ExportTmk <PackagePath> <Dir>: writes a UTidemarkTerrainAsset as a .tmk folder."),
		FConsoleCommandWithArgsDelegate::CreateLambda([](const TArray<FString>& Args)
		{
			if (Args.Num() < 2)
			{
				UE_LOG(LogTidemarkEditor, Warning, TEXT("usage: tidemark.ExportTmk <PackagePath> <Dir>"));
				return;
			}
			const UTidemarkTerrainAsset* Asset = LoadObject<UTidemarkTerrainAsset>(nullptr, *Args[0]);
			FString Error;
			if (Asset && UTidemarkImportLibrary::ExportTmk(Asset, Args[1], Error))
			{
				UE_LOG(LogTidemarkEditor, Display, TEXT("tidemark.ExportTmk: %s -> %s"), *Args[0], *Args[1]);
			}
			else
			{
				UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.ExportTmk failed: %s"), Asset ? *Error : TEXT("asset not found"));
			}
		}));
}
