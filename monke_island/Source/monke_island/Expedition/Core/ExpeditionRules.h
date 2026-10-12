// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "ExpeditionRules.generated.h"

/**
 *  Tunable rules of an expedition (DA_ExpeditionRules), cahier §6.1.
 *  Common clock: the flood starts at ExpeditionStartTime + InitialFloodDelay + ZoneBonus x pacified zones,
 *  then the water rises for SubmersionDuration along WaterRise = MaxWaterRiseCm x alpha^WaterRiseExponent,
 *  then keeps rising at PostSubmersionRiseCmPerSecond up to PostSubmersionMaxRiseCm more (the swimmers drown, the boat floats).
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

	/** The clock waits for the players still loading the island at most this long (s; cahier §16, §24.5: 60 s); a
	 *  player who comes later is put on the deck (network review T-268 R-02, T-292) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock", meta=(ClampMin="0", Units="s"))
	float CrewLoadTimeoutSeconds = 60.0f;

	/** Listen server with nobody announced by a travel (a map opened directly, PIE): the clock waits this long for the
	 *  clients still joining (s); solo never waits */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock", meta=(ClampMin="0", Units="s"))
	float CrewJoinGraceSeconds = 3.0f;

	/** Duration of the final rise of the water, from the flood deadline to the resolution (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0", Units="s"))
	float SubmersionDuration = 60.0f;

	/** Height the water gains over the whole submersion, above the low water (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0", Units="cm"))
	float MaxWaterRiseCm = 650.0f;

	/** Shape of the rise: 1 = linear, above 1 = slow start then fast end */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0.01"))
	float WaterRiseExponent = 1.6f;

	/** Once the island is submerged the sea keeps rising at this speed (cm/s): no roof, no marae stays dry, only the boat floats */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0", Units="cm/s"))
	float PostSubmersionRiseCmPerSecond = 10.0f;

	/** Highest extra rise after the submersion (cm), on top of MaxWaterRiseCm; 0 = the sea stops at the end of the submersion */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Flood", meta=(ClampMin="0", Units="cm"))
	float PostSubmersionMaxRiseCm = 1500.0f;

	/** A dead player stays dead until the end of the expedition: no respawn at the player start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game over")
	bool bPermadeath = true;

	/**
	 *  ADR-006: at the end of the submersion every player who is not aboard a galleon dies; the others go on to
	 *  the next day. In a map without galleon nobody is killed: the day ends once the sea reached its highest level.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game over")
	bool bGalleonSavesAtDelugeEnd = true;

	/** Time between the end of a day with survivors and the travel to the next island (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game over", meta=(ClampMin="0", Units="s"))
	float DayEndTravelDelay = 10.0f;

	/**
	 *  "JOUR PERDU" (T-173, ADR-007 §14): time between the death of the last player and the travel to the next island,
	 *  the day lost, the campaign going on (s); the greed question adds its own time, the host may send the galleon on
	 *  sooner. Name kept from the game over it replaced (T-104), so the assets keep their value
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game over", meta=(ClampMin="0", Units="s", DisplayName="Lost Day Travel Delay"))
	float GameOverRestartDelay = 8.0f;

	/** Map opened to travel to the next island, after a day survived or lost; empty = GameDefaultMap of the project
	 *  (L_Boot: new island draw, start in the boat) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Game over", meta=(AllowedClasses="/Script/Engine.World"))
	FSoftObjectPath RestartMap;
};
