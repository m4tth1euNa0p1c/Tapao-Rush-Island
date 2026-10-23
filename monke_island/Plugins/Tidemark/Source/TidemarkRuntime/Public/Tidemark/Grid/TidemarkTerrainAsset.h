// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "TidemarkTerrainAsset.generated.h"

/**
 * Attribution of one data source that fed a grid (Tidemark conventions section 7): every field is mandatory,
 * the CI refuses a .tmk manifest or an asset whose credits are incomplete.
 */
USTRUCT(BlueprintType)
struct TIDEMARKRUNTIME_API FTidemarkCredit
{
	GENERATED_BODY()

	/** Producer of the data (for example a public mapping service or "estimated profiles"). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Credits")
	FString Producer;

	/** Licence of the data, for example "Licence Ouverte Etalab 2.0". */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Credits")
	FString License;

	/** Where the data comes from. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Credits")
	FString Url;

	/** Date of the data or of its retrieval, ISO 8601 (YYYY-MM-DD). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Credits")
	FString Date;

	/** True when every field is non-empty. */
	bool IsComplete() const;

	bool operator==(const FTidemarkCredit& Other) const
	{
		return Producer == Other.Producer && License == Other.License && Url == Other.Url && Date == Other.Date;
	}
};

/**
 * Baked terrain of a level for the Tidemark simulation: the grid, the version of its schema, the hash of the grid
 * when it was imported and the credits of its sources. Loaded through the AssetManager under the primary asset
 * type "TidemarkTerrain". The importer (editor module) fills Grid, SourceHashHex and Credits; nothing here is computed at runtime.
 */
UCLASS(BlueprintType)
class TIDEMARKRUNTIME_API UTidemarkTerrainAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Primary asset type registered with the AssetManager. */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Current schema of the grid and of the .tmk format. */
	static constexpr int32 CurrentSchemaVersion = 1;

	/** The baked grid (elevation and flags, centimetres). */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Terrain")
	FTidemarkGrid Grid;

	/** Schema the grid was baked with; a different CurrentSchemaVersion means a re-import is needed. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Terrain")
	int32 SchemaVersion = CurrentSchemaVersion;

	/** FTidemarkGrid::ComputeSourceHash of the grid when it was imported, 16 hex digits. Empty before the first import. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Terrain")
	FString SourceHashHex;

	/** Attribution of every source that fed the grid. At least one entry, every field filled. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Credits")
	TArray<FTidemarkCredit> Credits;

	/** True when the grid no longer matches the stored hash (edited since the import) or was never hashed. */
	UFUNCTION(BlueprintPure, Category = "Tidemark|Terrain")
	bool IsStale() const;

	/** True when at least one credit exists and every credit is complete. */
	UFUNCTION(BlueprintPure, Category = "Tidemark|Credits")
	bool HasCompleteCredits() const;

	/** Recomputes SourceHashHex from the grid. Called by the importer once the grid is final. */
	void RefreshSourceHash();

	//~ Begin UPrimaryDataAsset Interface
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	//~ End UPrimaryDataAsset Interface
};
