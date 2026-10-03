// monke_island

#pragma once

#include "CoreMinimal.h"
#include "HazardClock.generated.h"

class AActor;
class AExpeditionGameState;
class UWorld;

/** Which clock the hazards of a level (rising water, final storm) may follow; set in DA_FloodRules / DA_WindRules */
UENUM(BlueprintType)
enum class EHazardClockMode : uint8
{
	/** The expedition clock when the map has one (AExpeditionGameState with rules), else the day / night clock */
	Auto,
	/** The expedition clock only: without an expedition the hazard stays off */
	ExpeditionOnly,
	/** The legacy day / night clock only (test maps without expedition) */
	DayNightOnly
};

/** Clock a hazard actually follows, decided once per actor and never changed afterwards */
UENUM(BlueprintType)
enum class EHazardClockSource : uint8
{
	/** Not decided yet: a client waits for the replicated game state */
	Undecided,
	/** The common expedition clock (cahier §6.1): deadline, zone bonuses, submersion of AExpeditionGameState */
	Expedition,
	/** The legacy day / night clock (fallback when the map has no expedition) */
	DayNight,
	/** No usable clock: the hazard stays off */
	Disabled
};

/**
 *  One clock for the hazards (T-098): the flood and the storm read the expedition clock when the map has one,
 *  and fall back to the day / night clock only when it has none. The decision is taken once, so the two clocks
 *  never drive the same hazard together.
 */
namespace HazardClock
{
	/**
	 *  Pure decision. bGameStateKnown: the game state exists on this machine (a client may receive it late);
	 *  bExpeditionAvailable: it is an expedition game state with rules (its clock may not run yet).
	 */
	MONKE_ISLAND_API EHazardClockSource Resolve(EHazardClockMode Mode, bool bGameStateKnown, bool bExpeditionAvailable);

	/** Whether a game state can drive the hazards: an expedition game state with its rules (DA_ExpeditionRules) */
	MONKE_ISLAND_API bool IsExpeditionAvailable(const AExpeditionGameState* GameState);

	/** Decision for the world of a hazard actor */
	MONKE_ISLAND_API EHazardClockSource ResolveForWorld(EHazardClockMode Mode, const UWorld* World);

	/** Logs the decision of a hazard once it is taken */
	MONKE_ISLAND_API void LogDecision(const AActor* Hazard, const TCHAR* What, EHazardClockMode Mode, EHazardClockSource Source);
}
