// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "BoatSubsystem.generated.h"

class ABoatActor;

/**
 *  Boats of a game world: gives every player pawn its UBoatRiderComponent (server, replicated) so the
 *  character classes need no change, and spawns boats on the water (console: monke.SpawnBoat).
 */
UCLASS()
class MONKE_ISLAND_API UBoatSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Boat Blueprint spawned by default (mesh, oars, DA_BoatRules) */
	static const TCHAR* DefaultBoatClassPath;

	/** Returns the subsystem of the given world context, or nullptr */
	static UBoatSubsystem* Get(const UObject* WorldContextObject);

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	virtual void Deinitialize() override;

	/** Spawns a boat on the water in front of the first local player (server); returns it or nullptr */
	ABoatActor* SpawnBoatNearPlayer();

private:

	/** Adds the rider component to the player pawns that miss it (server) */
	void EnsureRiders();

	/** Spawn a boat near the first player at the start when the world has none */
	bool bSpawnStarterBoat = true;

	/** The starter boat check ran */
	bool bStarterBoatDone = false;

	/** Timer of EnsureRiders */
	FTimerHandle RiderTimer;
};
