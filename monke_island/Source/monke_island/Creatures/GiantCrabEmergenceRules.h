// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GiantCrabEmergenceRules.generated.h"

class AGiantCrab;
class UGiantCrabRules;

/**
 *  Giant crabs coming out of the sand during the campaign (DA_GiantCrabEmergenceRules; owner requests 2026-10-07: "le crabe
 *  sort du sol à partir de 5:30 min, du sable, à 50 m du joueur, dans une zone sable uniquement", "les crabes apparaissent
 *  au 4e jour" = DA_RunRules.Campaign.GiantCrabsFromDay). At each time of EmergeSeconds of a day, a crab rises out of
 *  the sand near a player and charges: its rules (a copy of DA_GiantCrabRules) hunt far enough to reach that player.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UGiantCrabEmergenceRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Crab spawned (BP_GiantCrab) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crab")
	TSubclassOf<AGiantCrab> CrabClass;

	/** Its rules (DA_GiantCrabRules_Sand: a leash long enough to charge the player it rose near) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Crab")
	TObjectPtr<UGiantCrabRules> CrabRules;

	/** Seconds into the day a crab comes out (owner: 5:30 = 330 s); one crab each */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing")
	TArray<float> EmergeSeconds = { 330.0f };

	/** Distance from the player it comes out at (cm), and how far the search may stray from it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta=(ClampMin="0", Units="cm"))
	float DistanceCm = 5000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta=(ClampMin="0", Units="cm"))
	float DistanceJitterCm = 1200.0f;

	/** Sand: ground this high over the sea at least and at most (cm): the beach, not the water nor the inland */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta=(Units="cm"))
	float SandMinAboveSea = 20.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta=(Units="cm"))
	float SandMaxAboveSea = 260.0f;

	/** Flat enough (Z of the ground normal) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta=(ClampMin="0", ClampMax="1"))
	float MinGroundNormalZ = 0.85f;

	/** Places tried around the player at each look */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Place", meta=(ClampMin="1"))
	int32 Samples = 24;

	/** Depth it rises from (cm) and the time it takes (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rise", meta=(ClampMin="0", Units="cm"))
	float DepthCm = 280.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Rise", meta=(ClampMin="0.1", Units="s"))
	float RiseSeconds = 2.5f;

	/** Seconds between two looks at the time of the day */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Timing", meta=(ClampMin="0.1", Units="s"))
	float CheckInterval = 1.0f;
};
