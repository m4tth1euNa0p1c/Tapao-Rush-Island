// monke_island

#pragma once

#include "CoreMinimal.h"

struct FExpeditionClockParams;
struct FExpeditionClockState;
class UFloodRules;

/**
 *  Pure timing of the rising water (T-098), shared by AFloodSimulationActor and the tests.
 *  Expedition mode: the rise starts at the flood deadline of the expedition clock and lasts SubmersionDuration
 *  (DA_ExpeditionRules); day / night mode (fallback): StartSeconds and RiseSeconds of DA_FloodRules.
 */
namespace FloodSchedule
{
	/**
	 *  Seconds of rising water at ServerTime on the expedition clock, -1 before the rise.
	 *  bSubmersionConfirmed: the expedition reached Submerging (a client waits for the replicated phase, so a zone
	 *  bonus replicated late never makes the water start then stop); the server may also pass ServerTime >= deadline.
	 */
	MONKE_ISLAND_API double ExpeditionFloodSeconds(const FExpeditionClockState& State, double ServerTime, bool bSubmersionConfirmed);

	/** Rise of the sea (cm) after FloodSeconds of the expedition submersion: same curve as AExpeditionGameState::GetWaterRise */
	MONKE_ISLAND_API double ExpeditionRiseCm(const FExpeditionClockState& State, double FloodSeconds, const FExpeditionClockParams& Params);

	/** Seconds of rising water on the day / night clock, -1 before StartSeconds */
	MONKE_ISLAND_API double DayNightFloodSeconds(const UFloodRules& Rules, double Elapsed);

	/** Rise of the sea (cm) after FloodSeconds on the day / night clock (DA_FloodRules curve) */
	MONKE_ISLAND_API double DayNightRiseCm(const UFloodRules& Rules, double FloodSeconds);

	/** The water never goes down: the sea target only ever rises */
	MONKE_ISLAND_API double HoldRise(double PreviousRiseCm, double TargetRiseCm);

	/** Highest rise of an expedition (cm): MaxWaterRiseCm, plus PostSubmersionMaxRiseCm when the sea keeps rising after the submersion */
	MONKE_ISLAND_API double ExpeditionFinalRiseCm(const FExpeditionClockParams& Params);

	/**
	 *  Whether the global sea moves to NewLevelCm: once it is StepCm above the level applied, or at the top of the rise,
	 *  so the water surfaces move a few times per second rather than every frame. Never down.
	 */
	MONKE_ISLAND_API bool ShouldApplySeaLevel(double AppliedLevelCm, double NewLevelCm, double StepCm, bool bTop);
}

/**
 *  Global sea level of an expedition (T-103): the sea level before the rise plus the highest rise reached so far.
 *  Fed every frame with the rise of the replicated expedition clock; the level it returns never goes down.
 */
struct MONKE_ISLAND_API FFloodSeaRise
{
	/** Sea level before the rise (world Z, cm) */
	double BaseSeaLevelCm = 0.0;

	/** Highest rise reached so far (cm) */
	double HeldRiseCm = 0.0;

	/** Feeds the rise of the clock now (cm); returns the sea level to apply, never lower than before */
	double Advance(double TargetRiseCm)
	{
		HeldRiseCm = FloodSchedule::HoldRise(HeldRiseCm, TargetRiseCm);
		return GetSeaLevelCm();
	}

	/** Sea level now (world Z, cm) */
	double GetSeaLevelCm() const { return BaseSeaLevelCm + HeldRiseCm; }
};
