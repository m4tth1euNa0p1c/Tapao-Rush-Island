// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExpeditionRules.generated.h"

/**
 *  Tunable rules of an expedition (DA_ExpeditionRules), cahier §6.1.
 *  Common clock: the flood starts at ExpeditionStartTime + InitialFloodDelay + ZoneBonus x pacified zones,
 *  then the water rises for SubmersionDuration along WaterRise = MaxWaterRiseCm x alpha^WaterRiseExponent.
 *  Other expedition values (cargo, quotas, tax) will join this asset with their lots.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UExpeditionRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Time from the start of the expedition to the flood deadline without any pacified zone (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock", meta=(ClampMin="0", Units="s"))
	float InitialFloodDelay = 120.0f;

	/** Time added to the flood deadline, for the whole team, by the first pacification of a zone (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock", meta=(ClampMin="0", Units="s"))
	float ZoneBonus = 120.0f;

	/** Number of pacifiable zones of the island (prototype setting); 0 = no limit on the bonuses */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock", meta=(ClampMin="0"))
	int32 ZonesCount = 3;

	/** Duration of the final rise of the water, from the flood deadline to the resolution (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0", Units="s"))
	float SubmersionDuration = 60.0f;

	/** Height the water gains over the whole submersion, above the low water (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0", Units="cm"))
	float MaxWaterRiseCm = 650.0f;

	/** Shape of the rise: 1 = linear, above 1 = slow start then fast end */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0.01"))
	float WaterRiseExponent = 1.6f;
};
