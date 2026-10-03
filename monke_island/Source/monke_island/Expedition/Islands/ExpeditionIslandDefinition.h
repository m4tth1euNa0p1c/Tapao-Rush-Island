// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Expedition/Islands/ExpeditionIslandTypes.h"
#include "ExpeditionIslandDefinition.generated.h"

class UTexture2D;
class UWorld;

/**
 *  One expedition island: a frozen fragment of a real atoll in its own map (ADR-004, DA_Island_<Ile>_<NN>).
 *  Primary asset of type ExpeditionIsland, named by its FragmentId so the id stays stable when the asset is renamed.
 *  Light by design: the map and the preview are soft references, loaded only when needed.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UExpeditionIslandDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Primary asset type of the island definitions (Asset Manager, DefaultGame.ini) */
	static const FPrimaryAssetType PrimaryAssetType;

	/** Primary asset id of a fragment id */
	static FPrimaryAssetId MakePrimaryAssetId(FName InFragmentId);

	virtual FPrimaryAssetId GetPrimaryAssetId() const override;

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

	/** Stable identifier of the frozen fragment (FRG_<Ile>_<NN>, e.g. FRG_Raraka_01) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Identity")
	FName FragmentId;

	/** Fictional name shown at the hub (String Table, never a real toponym) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Identity")
	FText DisplayName;

	/** Map of the fragment (L_Frag_<Ile>_<NN>), reached by ServerTravel */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, AssetRegistrySearchable, Category="Map")
	TSoftObjectPtr<UWorld> Map;

	/** Pacifiable zones of the fragment (one village each) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gameplay", meta=(ClampMin="1"))
	int32 ZoneCount = 3;

	/** Anchorages of the boat (cahier §6.4) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Gameplay")
	TArray<FExpeditionAnchorage> Anchorages;

	/** Land area of the fragment (ha) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Description", meta=(ClampMin="0"))
	float SizeHectares = 0.0f;

	/** Indicative difficulty shown at the hub, 1 (calm) to 5 (hard) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Description", meta=(ClampMin="1", ClampMax="5"))
	int32 IndicativeDifficulty = 1;

	/** Seeds of the placement variants (loot, enemies, events): the terrain never changes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Variants")
	TArray<int32> VariantSeeds;

	/** Credits of the real data the fragment is built from (Te Fenua / DAF Etalab 2.0, IRD atlas) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Credits", meta=(MultiLine="true"))
	FText DataCredits;

	/** Picture shown at the hub, loaded with the Preview bundle */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Description", meta=(AssetBundles="Preview"))
	TSoftObjectPtr<UTexture2D> PreviewImage;
};
