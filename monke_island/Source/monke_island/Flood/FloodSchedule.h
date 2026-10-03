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
}
