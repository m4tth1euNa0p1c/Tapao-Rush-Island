// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WindLeavesSubsystem.generated.h"

class AWindLeavesActor;

/**
 *  Spawns the local leaves carried by the wind (AWindLeavesActor) in game worlds that draw something:
 *  never on a dedicated server. Rules: DA_WindLeavesRules (defaults when missing).
 *  Console: monke.WindLeaves 0 hides them, monke.WindLeaves.Strength forces the wind they feel.
 */
UCLASS()
class MONKE_ISLAND_API UWindLeavesSubsystem : public UWorldSubsystem
{
	GENERATED_BODY()

public:

	/** Rules loaded by default */
	static const TCHAR* DefaultRulesPath;

	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;

	virtual void OnWorldBeginPlay(UWorld& InWorld) override;

	/** The leaves actor of this world, or nullptr */
	AWindLeavesActor* GetLeavesActor() const { return LeavesActor.Get(); }

private:

	/** Spawned leaves (owned by the world) */
	TWeakObjectPtr<AWindLeavesActor> LeavesActor;
};
