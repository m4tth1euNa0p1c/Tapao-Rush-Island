// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "IslandCatalog.generated.h"

class UFragmentRules;

/**
 *  Fragments offered at the hub (DA_IslandCatalog, ADR-004; contract board at L4).
 *  Lists primary asset ids of island definitions: nothing heavy is loaded with the catalog.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UIslandCatalog : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Primary asset type of the catalog (Asset Manager, DefaultGame.ini) */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Catalog the game loads: DA_IslandCatalog */
	static FPrimaryAssetId GetDefaultCatalogId();

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Island definitions offered, in display order */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Catalog", meta=(AllowedTypes="ExpeditionIsland"))
	TArray<FPrimaryAssetId> Fragments;

	/** Runtime rules of the islands (DA_FragmentRules); class defaults apply when empty */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Catalog")
	TObjectPtr<UFragmentRules> Rules;
};
