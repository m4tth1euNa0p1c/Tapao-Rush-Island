// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Tidemark/Grid/TidemarkTerrainAsset.h"
#include "TidemarkEditor/Import/TextureHeightSource.h"
#include "TidemarkEditor/Import/TidemarkHeightSource.h"
#include "TidemarkImportLibrary.generated.h"

class ALandscapeProxy;
class UTidemarkRules;
struct FTidemarkGrid;

/**
 * Editor entry points of the terrain import (Tidemark cahier section 6.2; skill tidemark-terrain-import): bake a source
 * into a UTidemarkTerrainAsset saved as a package, export it as a .tmk folder, import a .tmk folder as an asset.
 * Every bake refuses to proceed without at least one complete credit (Tidemark conventions section 7).
 *
 * Console (editor): `tidemark.BakeLandscape <PackagePath> [CellSizeCm] [SeaLevelCm] [MarginCm]` on the first Landscape of
 * the editor world; `tidemark.ImportTmk <Dir> <PackagePath>`; `tidemark.ExportTmk <PackagePath> <Dir>`.
 * Console bakes carry the credit "Project Landscape / Project licence" placeholder: edit the asset before shipping it.
 */
UCLASS()
class TIDEMARKEDITOR_API UTidemarkImportLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	/** Bake settings taken from a rules asset (cell = SimCellSizeCm, open sea depth), or the defaults of UTidemarkRules when null. */
	UFUNCTION(BlueprintCallable, Category = "Tidemark|Import")
	static FTidemarkBakeSettings MakeBakeSettings(const UTidemarkRules* Rules, float SeaLevelCm, float OpenSeaMarginCm);

	/**
	 * Bakes a Landscape (any of its proxies) into a new or existing asset at PackagePath (for example /Game/Tidemark/TA_Island).
	 * Null with OutError when the credits are incomplete, the Landscape cannot be read, the grid is too large or the save fails.
	 */
	UFUNCTION(BlueprintCallable, Category = "Tidemark|Import", meta = (AutoCreateRefTerm = "Credits"))
	static UTidemarkTerrainAsset* BakeFromLandscape(ALandscapeProxy* Landscape, const FTidemarkBakeSettings& Settings, const FString& PackagePath, const TArray<FTidemarkCredit>& Credits, FString& OutError, FTidemarkBakeResult& OutResult);

	/** Bakes a heightmap file (.r16/.raw, 16-bit .png, .exr) placed in the world by Params into an asset at PackagePath. */
	UFUNCTION(BlueprintCallable, Category = "Tidemark|Import", meta = (AutoCreateRefTerm = "Credits"))
	static UTidemarkTerrainAsset* BakeFromHeightmapFile(const FString& FilePath, const FTidemarkTextureHeightParams& Params, const FTidemarkBakeSettings& Settings, const FString& PackagePath, const TArray<FTidemarkCredit>& Credits, FString& OutError, FTidemarkBakeResult& OutResult);

	/** Writes the asset's grid and credits as a .tmk folder at Dir (Tools/tmk.py reads it). */
	UFUNCTION(BlueprintCallable, Category = "Tidemark|Import")
	static bool ExportTmk(const UTidemarkTerrainAsset* Asset, const FString& Dir, FString& OutError);

	/** Reads a .tmk folder into a new or existing asset at PackagePath. Null with OutError on a bad folder, missing credits or a failed save. */
	UFUNCTION(BlueprintCallable, Category = "Tidemark|Import")
	static UTidemarkTerrainAsset* ImportTmk(const FString& Dir, const FString& PackagePath, FString& OutError);

	/**
	 * Bakes any prepared source (C++ only). Creates or reuses the asset at PackagePath, fills Grid, Credits, SchemaVersion,
	 * refreshes SourceHashHex, registers the asset and saves the package.
	 */
	static UTidemarkTerrainAsset* BakeFromSource(const ITidemarkHeightSource& Source, const FTidemarkBakeSettings& Settings, const FString& PackagePath, const TArray<FTidemarkCredit>& Credits, FString& OutError, FTidemarkBakeResult& OutResult);

	/** Stores Grid and Credits into a new or existing asset at PackagePath and saves it (C++ only). */
	static UTidemarkTerrainAsset* SaveGridAsAsset(const FTidemarkGrid& Grid, const TArray<FTidemarkCredit>& Credits, const FString& PackagePath, FString& OutError);

	/** True when Credits has at least one entry and every entry is complete; else OutError says which one is not. */
	static bool ValidateCredits(const TArray<FTidemarkCredit>& Credits, FString& OutError);

	/** Finds the Landscape of the editor world: the ALandscape actor first, else the first ALandscapeProxy. Null when none. */
	static ALandscapeProxy* FindEditorLandscape();

private:
	/** Creates or finds the package and the asset object at PackagePath without saving. */
	static UTidemarkTerrainAsset* FindOrCreateAsset(const FString& PackagePath, FString& OutError);

	/** Saves the package of Asset to its .uasset file. */
	static bool SaveAssetPackage(UTidemarkTerrainAsset* Asset, FString& OutError);
};
