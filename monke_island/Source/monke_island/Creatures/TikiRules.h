// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TikiRules.generated.h"

/**
 *  Tunable rules of the tiki creatures of an island (DA_TikiRules): villagers who wander in the middle of the
 *  island and gather coconuts, tiki warriors who come out of the bushes at night, and the defender a villager
 *  calls from the nearest bush when a player hits it.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTikiRules : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	/** Villagers kept alive around the director */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	int32 VillagerCount = 4;

	/** Health of a villager */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="1"))
	float VillagerHealth = 60.0f;

	/** Walking speed of a villager (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	float VillagerWalkSpeed = 110.0f;

	/** Running speed of a fleeing villager (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	float VillagerRunSpeed = 380.0f;

	/** Shortest pause between two walks (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float WanderPauseMin = 1.5f;

	/** Longest pause between two walks (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float WanderPauseMax = 4.0f;

	/** A villager notices a coconut up to this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0"))
	float CoconutSearchRadius = 2500.0f;

	/** Time spent picking a coconut up (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float CollectDuration = 2.0f;

	/** Time a hit villager runs away from its attacker (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Villager", meta=(ClampMin="0", Units="s"))
	float FleeDuration = 6.0f;

	/** Coconuts kept on the ground around the director */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coconut", meta=(ClampMin="0"))
	int32 CoconutCount = 10;

	/** Delay between two coconut drops when there are fewer than CoconutCount (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Coconut", meta=(ClampMin="0.1", Units="s"))
	float CoconutDropInterval = 8.0f;

	/** Health of a tiki warrior */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="1"))
	float WarriorHealth = 80.0f;

	/** Walking speed of a tiki warrior (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float WarriorWalkSpeed = 140.0f;

	/** Running speed of a tiki warrior chasing a player (cm/s); the player sprints faster */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float WarriorRunSpeed = 400.0f;

	/** A night warrior notices a player up to this distance (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AggroRadius = 2500.0f;

	/** A warrior gives up a target farther than this (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float LoseRadius = 5000.0f;

	/** Distance from which the warrior starts its blow (cm, centre to centre) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackRange = 160.0f;

	/** Visible preparation before the blow lands (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0", Units="s"))
	float AttackWindup = 0.45f;

	/** Time from the start of a blow to the next one (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0", Units="s"))
	float AttackCooldown = 1.6f;

	/** Damage of a blow */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackDamage = 15.0f;

	/** Reach of the blow in front of the warrior (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackReach = 180.0f;

	/** Radius of the blow sweep (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="1"))
	float AttackRadius = 50.0f;

	/** Horizontal speed given to a struck player (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float AttackKnockback = 400.0f;

	/** Speed of the leap out of the bush (cm/s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Warrior", meta=(ClampMin="0"))
	float EmergeLeapSpeed = 450.0f;

	/** Time a dead creature stays on the ground (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Creature", meta=(ClampMin="0", Units="s"))
	float CorpseLifetime = 8.0f;

	/** Night warriors kept alive while it is night */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0"))
	int32 NightWarriorCount = 4;

	/** Delay between two night spawns (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0.1", Units="s"))
	float NightSpawnInterval = 6.0f;

	/** Closest a night warrior appears from a player (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0"))
	float NightSpawnMinDistance = 1500.0f;

	/** Farthest a night warrior appears from a player (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Night", meta=(ClampMin="0"))
	float NightSpawnMaxDistance = 3500.0f;

	/** A defender comes out of a bush up to this distance from the hit villager (cm); farther, it comes out of the ground next to it */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender", meta=(ClampMin="0"))
	float BushSearchRadius = 3000.0f;

	/** Shortest time between two defenders called against the same player (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender", meta=(ClampMin="0", Units="s"))
	float DefenderCooldown = 5.0f;

	/** Defenders alive at most against the same player */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender", meta=(ClampMin="1"))
	int32 MaxDefendersPerPlayer = 2;

	/** A static mesh whose name contains one of these words is a bush a tiki can hide in */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Defender")
	TArray<FString> BushMeshKeywords = { TEXT("MikiMiki"), TEXT("Naupaka"), TEXT("Pandanus"), TEXT("Tahinu") };

	/** Period of the director decisions (s) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Director", meta=(ClampMin="0.1", Units="s"))
	float ThinkInterval = 1.0f;
};
