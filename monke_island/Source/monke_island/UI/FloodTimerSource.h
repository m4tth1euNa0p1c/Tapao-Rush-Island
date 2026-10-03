// monke_island

#pragma once

#include "CoreMinimal.h"
#include "UI/FloodTimerMath.h"

class AExpeditionGameState;
class AFloodSimulationActor;
class UWorld;

/** Which clock feeds the flood timer */
enum class EFloodTimerSourceKind : uint8
{
	/** No flood source */
	None,
	/** The common expedition clock (cahier §6.1): AExpeditionGameState, server time */
	Expedition,
	/** A standalone AFloodSimulationActor on its day / night fallback clock (map without expedition game state) */
	FloodActor
};

/**
 *  Reads the clock that really drives the water, without owning any timer:
 *  the expedition game state when its clock runs, else (if allowed) a flood simulation actor of the level that runs on
 *  its day / night fallback. Since T-098 a flood actor in a map with an expedition follows the expedition clock, so
 *  both sources show the same deadline.
 *  Both read times derived from the replicated server time, so the host and the clients show the same values.
 */
class MONKE_ISLAND_API FFloodTimerSource
{
public:

	/** Looks the sources up again; cheap enough for a discovery timer (class-filtered actor iterator) */
	void Resolve(UWorld* World, bool bAllowFloodActor);

	/** Source currently read */
	EFloodTimerSourceKind GetKind() const;

	/** Expedition game state, when found (even before its clock starts) */
	AExpeditionGameState* GetExpedition() const { return Expedition.Get(); }

	/** Whether an expedition clock runs while a flood actor is forced on the day / night clock (two clocks, misconfiguration) */
	bool HasConflictingSources() const;

	/** Times of the current source; inactive when there is none */
	FFloodTimerInput Read() const;

private:

	/** Expedition game state of the world */
	TWeakObjectPtr<AExpeditionGameState> Expedition;

	/** First flood simulation actor of the world */
	TWeakObjectPtr<AFloodSimulationActor> FloodActor;

	/** Whether the flood actor may be shown */
	bool bAllowFloodActor = false;
};
