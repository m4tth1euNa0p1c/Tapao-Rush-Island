// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "ShoreAmbienceSubsystem.generated.h"

class AShoreAmbienceActor;

/**
 *  Spawns the shore ambience of the local player (AShoreAmbienceActor: waves on the sand, open sea) in game worlds
 *  that play sound: never on a dedicated server. Rules: DA_ShoreAmbienceRules (nothing plays when missing).
 *  Console: monke.ShoreAmbience 0 turns it off (read when a world starts playing).
 */
UCLASS()
class MONKE_ISLAND_API UShoreAmbienceSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Rules loaded by default */
	static const TCHAR* DefaultRulesPath;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

private:

	/** Spawned ambience (owned by the world) */
	TWeakObjectPtr<AShoreAmbienceActor> AmbienceActor;
};
