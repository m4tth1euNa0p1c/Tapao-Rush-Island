// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "LootScatterSpawner.generated.h"

class ULootRules;

/**
 *  Scatters the common loot on the ground of the island at the start of each game (T-076). Server only: the items
 *  are replicated physics props (APhysicsPropActor), the spawner itself is not replicated.
 *  A spot is kept when it is near a PlayerStart, far from the other items, out of the quiet zones (tag
 *  TikiQuietZone: the marae) and its ground, found by a trace on the level, stands above the beach.
 */
UCLASS()
class MONKE_ISLAND_API ALootScatterSpawner : public AActor
{
	GENERATED_BODY()

public:

	ALootScatterSpawner();

	/** Kinds of loot, count and candidate spots (DA_LootRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TObjectPtr<ULootRules> Rules;

	/** The spots of the rules are in this spawner's frame, not in the world: they follow it when the site moves (the
	 *  French fort of L_Test_Flat placed as a level instance on the expedition map, T-205) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	bool bSpotsInActorFrame = false;

	/** Items scattered by this spawner (server) */
	UFUNCTION(BlueprintPure, Category="Loot")
	int32 GetScatteredCount() const { return Scattered.Num(); }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Server: draws the spots and spawns the items */
	void Scatter();

	/** Ground under a spot, false on water, under the beach or without solid ground */
	bool ProjectToGround(const FVector& Spot, FVector& OutGround) const;

	/** Items spawned this game */
	TArray<TWeakObjectPtr<AActor>> Scattered;

	/** Items drawn and waiting to be spawned, a batch per step */
	TArray<TPair<TSubclassOf<AActor>, FTransform>> Pending;

	/** Spawns the next batch of Pending */
	void SpawnBatch();

	FTimerHandle BatchTimer;

	FTimerHandle ScatterTimer;
};
