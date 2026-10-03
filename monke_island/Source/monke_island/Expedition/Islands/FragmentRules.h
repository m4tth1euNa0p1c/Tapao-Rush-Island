// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FragmentRules.generated.h"

/**
 *  Runtime rules of the expedition islands (DA_FragmentRules, ADR-004): thresholds checked on a definition before
 *  it is offered or travelled to, and the maximum delay of an asynchronous load. The cut and size rules of the
 *  offline pipeline stay in its own config (scripts/map/fragments/).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UFragmentRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Maximum delay of the load of the catalog or of a definition before it fails (s, cahier §16) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loading", meta=(ClampMin="1", Units="s"))
	float LoadTimeoutSeconds = 60.0f;

	/** Anchorages a fragment must offer (initial beach, cove, fallback exit: cahier §6.4) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Validation", meta=(ClampMin="0"))
	int32 MinAnchorageCount = 3;

	/** A fragment must keep one anchorage reachable during the whole final submersion (cahier §6.4) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Validation")
	bool bRequireSubmersionAnchorage = true;

	/** Pacifiable zones a fragment must have at least */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Validation", meta=(ClampMin="1"))
	int32 MinZoneCount = 1;
};
