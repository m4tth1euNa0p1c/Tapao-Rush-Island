// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "LootRules.generated.h"

/** One kind of common loot and how often it is drawn */
USTRUCT(BlueprintType)
struct FLootEntry
{
	GENERATED_BODY()

	/** Item spawned (BP_Loot_*: a physics prop with a small value) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TSubclassOf<AActor> ItemClass;

	/** Relative chance of this kind (0 = never) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0"))
	float Weight = 1.0f;
};

/**
 *  Common loot scattered on the ground of the island at the start of each game (ALootScatterSpawner, T-076):
 *  everyday objects of the maquette loot-butin-commun, low value, picked up and carried like any physics prop.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API ULootRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Kinds of loot and their weights */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Loot")
	TArray<FLootEntry> Items;

	/** Items scattered at the start of a game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	int32 Count = 1200;

	/** Two items are never closer than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	float MinSpacing = 2000.0f;

	/** Only spots within this distance of a PlayerStart are used (cm); 0 = the whole island */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	float MaxDistanceFromStart = 0.0f;

	/** No item closer than this to a PlayerStart: the players find it a little further (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	float MinDistanceFromStart = 800.0f;

	/** Ground below this height is the beach the sea covers: no item there (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	float MinGroundHeight = 60.0f;

	/** Steepest ground an item is put on (minimal Z of the ground normal) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0", ClampMax="1"))
	float MinGroundNormalZ = 0.7f;

	/** Spots under a building of these meshes (instanced roofs and floors of the villages) are skipped */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	TArray<FString> BuildingMeshKeywords = { TEXT("SM_Minka"), TEXT("SM_Village_") };

	/** Height above the ground an item appears at: it drops and settles (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	float DropHeight = 8.0f;

	/** Delay after the start of the level before the scatter (s): the level and its vegetation are ready */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0", Units="s"))
	float SpawnDelay = 2.5f;

	/** Items spawned per frame step (the scatter is spread over a few frames, no hitch at the start) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="1"))
	int32 SpawnBatchSize = 80;

	/** Seed of the draw; 0 = a new draw every game */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	int32 Seed = 0;

	/** Value of each item scattered, times a draw between X and Y (the French fort: its common objects from 5 to 120,
	 *  owner request 2026-10-07); (1, 1) = the values of the items */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	FVector2D ValueScaleRange = FVector2D(1.0, 1.0);

	/** A scattered item is never worth more than this (0 = no cap) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="0"))
	int32 MaxValue = 0;

	/** Height above a spot the ground is looked for from (cm): high on the open island (over the trees), just over the
	 *  floor inside buildings so the roof is not taken for the ground (the French fort, T-217) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter", meta=(ClampMin="10", Units="cm"))
	float GroundSearchHeight = 3000.0f;

	/** Candidate spots (UE cm, Z = approximate ground): land above the sea, out of the real village of Motutapu
	 *  (scripts/map/place_loot.py, collectibles/loot_spots.json) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Scatter")
	TArray<FVector> Spots;
};
