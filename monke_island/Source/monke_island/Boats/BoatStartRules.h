// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "BoatStartRules.generated.h"

class ABoatActor;

/**
 *  Tunable rules of the start in the boat (DA_BoatStartRules, T-093): at the start of the expedition the
 *  server spawns the boat at the arrival point of the fragment and seats the players in it.
 *  Spec: docs/features/boat-rowing.md, section "Départ en barque". Lengths in cm.
 *  Referenced by UBoatStartSubsystem through Config/DefaultGame.ini (StartRulesAsset).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UBoatStartRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Start the players in the boat when the map has arrival points; off: everyone starts on a PlayerStart */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Start")
	bool bEnabled = true;

	/** Boat spawned at the arrival point (BP_Barque); empty: the default boat class of UBoatSubsystem */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Start")
	TSoftClassPtr<ABoatActor> BoatClass;

	/** ?Island=<id> names a fragment that has no arrival point in this map: start at a random point anyway
	 *  (off: no boat, PlayerStart) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Start")
	bool bRandomArrivalWhenIslandUnknown = true;

	/** Seed of the random arrival point when the URL has no ?Seed=; 0: a new seed each game (logged) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Start")
	int32 ArrivalSeed = 0;

	/** Longest fragment id accepted from the URL (characters) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Start", meta=(ClampMin="1", ClampMax="256"))
	int32 MaxFragmentIdLength = 64;

	/** Height of the hull pivot (waterline) above the water surface at spawn (cm): drops gently to rest */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement")
	float SpawnHeightAboveWaterCm = 5.0f;

	/** Least water under the arrival point (cm): the draught (30 cm) with players aboard plus a margin */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0"))
	float MinWaterDepthCm = 120.0f;

	/** Step of the search for deeper water, backwards from the arrival point (away from the island) (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement", meta=(ClampMin="10"))
	float SafetySearchStepCm = 100.0f;

	/** Farthest the boat may be moved back from the arrival point to find deep water (cm); beyond: no boat */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Placement", meta=(ClampMin="0"))
	float SafetySearchMaxCm = 5000.0f;

	/** Seats given in turn to the players (Seat_N indices); the first one takes the oars; empty: 0, 1, 2... */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boarding")
	TArray<int32> SeatOrder = {0, 1, 2, 3};

	/** Where the players without a seat stand, in the boat's local space (cm), used in turn */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boarding")
	TArray<FVector> StandingSpots = {FVector(-40.0, 0.0, 100.0), FVector(40.0, 0.0, 100.0), FVector(-110.0, 0.0, 100.0), FVector(110.0, 0.0, 100.0)};

	/** Players joining a game in progress also start in the boat (when it is still afloat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boarding")
	bool bBoardLateJoiners = true;

	/** Turn the players' view toward the bow when they board */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boarding")
	bool bFaceBow = true;
};
