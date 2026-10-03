// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Expedition/Core/ExpeditionTypes.h"
#include "ExpeditionClock.generated.h"

class UExpeditionRules;

/** Clock values read from the rules, so the pure functions run without any asset or world */
struct MONKE_ISLAND_API FExpeditionClockParams
{
	/** Time from the start to the flood deadline without bonus (s) */
	double InitialFloodDelay = 0.0;

	/** Time added by each pacified zone (s) */
	double ZoneBonus = 0.0;

	/** Duration of the final rise (s) */
	double SubmersionDuration = 0.0;

	/** Maximum number of zone bonuses; 0 = no limit */
	int32 ZonesCount = 0;

	/** Height gained over the whole submersion (cm) */
	double MaxWaterRiseCm = 0.0;

	/** Shape of the rise curve */
	double WaterRiseExponent = 1.0;

	/** Copies the clock values of the rules */
	static FExpeditionClockParams FromRules(const UExpeditionRules& Rules);
};

/** Shared state of the expedition clock, replicated as one block by the game state */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FExpeditionClockState
{
	GENERATED_BODY()

	/** Whether the clock runs */
	UPROPERTY(BlueprintReadOnly, Category="Expedition")
	bool bStarted = false;

	/** Server time of the start of the expedition (s) */
	UPROPERTY(BlueprintReadOnly, Category="Expedition")
	double StartTime = 0.0;

	/** Server time the submersion starts (s) */
	UPROPERTY(BlueprintReadOnly, Category="Expedition")
	double FloodStartTime = 0.0;

	/** Zones that already gave their bonus in this expedition, in order */
	UPROPERTY(BlueprintReadOnly, Category="Expedition")
	TArray<FName> BonusedZoneIds;
};

/**
 *  Pure rules of the common clock (cahier §6.1), shared by AExpeditionGameState and the tests.
 *  Times are server world times in seconds.
 */
namespace ExpeditionClock
{
	/** Flood deadline = start + initial delay + bonus x zones (computed from scratch: no drift) */
	MONKE_ISLAND_API double ComputeFloodStartTime(double StartTime, int32 BonusCount, const FExpeditionClockParams& Params);

	/** A fresh clock started at ServerTime, without any bonus */
	MONKE_ISLAND_API FExpeditionClockState Start(double ServerTime, const FExpeditionClockParams& Params);

	/**
	 *  Grants the bonus of a zone once per expedition, only when ServerTime strictly precedes the flood deadline
	 *  (equality = refused, the flood wins). Two zones at the same ServerTime both get their bonus.
	 */
	[[nodiscard]] MONKE_ISLAND_API EZoneBonusResult TryAddZoneBonus(FExpeditionClockState& State, FName ZoneId, double ServerTime, const FExpeditionClockParams& Params);

	/** Server time the submersion ends and the expedition resolves */
	MONKE_ISLAND_API double SubmersionEndTime(const FExpeditionClockState& State, const FExpeditionClockParams& Params);

	/** Phase given by the clock alone at ServerTime */
	MONKE_ISLAND_API EExpeditionPhase PhaseAt(const FExpeditionClockState& State, double ServerTime, const FExpeditionClockParams& Params);

	/** Progress of the submersion in [0, 1]: 0 before the deadline, 1 at its end */
	MONKE_ISLAND_API double SubmersionAlpha(const FExpeditionClockState& State, double ServerTime, const FExpeditionClockParams& Params);

	/** Water height above the low water (cm) = curve(max(0, t - FloodStartTime)); never decreases with time */
	MONKE_ISLAND_API double WaterRiseCm(const FExpeditionClockState& State, double ServerTime, const FExpeditionClockParams& Params);

	/** Time left before the submersion (s), 0 once it started or when the clock does not run */
	MONKE_ISLAND_API double TimeBeforeSubmersion(const FExpeditionClockState& State, double ServerTime);
}
