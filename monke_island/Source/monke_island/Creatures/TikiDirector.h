// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TikiDirector.generated.h"

class APawn;
enum class ETikiWarriorRole : uint8;
class ATikiVillager;
class ATikiWarrior;
class UBillboardComponent;
class UInstancedStaticMeshComponent;
class UTikiRules;

/**
 *  Tiki life of an island, placed once in the level at the middle of the inhabited land (server only, not
 *  replicated: the creatures replicate themselves).
 *  - Keeps villagers wandering within VillageRadius and coconuts on the ground for them to gather.
 *  - At night (ADayNightCycleActor), brings tiki warriors out of the bushes around the players; at dawn they go
 *    back into the ground.
 *  - When a player hits a villager, a tiki warrior leaps out of the bush nearest to the villager and attacks
 *    that player.
 *  Debug (non shipping): monke.TikiDefender spawns a defender against the local player.
 */
UCLASS()
class MONKE_ISLAND_API ATikiDirector : public AActor
{
	GENERATED_BODY()

public:

	ATikiDirector();

	/** Returns the director of the given world, or nullptr */
	static ATikiDirector* Get(const UObject* WorldContextObject);

	/** Rules of the creatures (DA_TikiRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TObjectPtr<UTikiRules> Rules;

	/** Villager Blueprint (BP_TikiVillager) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TSubclassOf<ATikiVillager> VillagerClass;

	/** Warrior Blueprints, one drawn at random per spawn (BP_TikiCocoWarrior…) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TArray<TSubclassOf<ATikiWarrior>> WarriorClasses;

	/** Coconut dropped for the villagers (BP_CoconutPickup) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki")
	TSubclassOf<AActor> CoconutClass;

	/** Radius of the village area around this actor where villagers wander and coconuts fall (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Tiki", meta=(ClampMin="100"))
	float VillageRadius = 4000.0f;

	/** Server: reserves the free coconut nearest to Location within Radius for a villager; nullptr when none */
	AActor* ClaimCoconutNear(ATikiVillager* Villager, const FVector& Location, float Radius);

	/** Server: frees a reserved coconut */
	void ReleaseCoconut(AActor* Coconut);

	/** Server: the villager picks the coconut up; true when it was still there */
	bool CollectCoconut(ATikiVillager* Villager, AActor* Coconut);

	/** Server: a player hit a villager; a warrior leaps out of the nearest bush against that player */
	ATikiWarrior* CallDefender(ATikiVillager* Villager, APawn* Attacker);

	/** Ground point above the water within [MinRadius, MaxRadius] of Center, drawn at random (a few tries) */
	bool FindGroundPointNear(const FVector& Center, float MinRadius, float MaxRadius, FVector& OutPoint) const;

	/** Ground under a horizontal position, above the water; false over the sea or the lagoon */
	bool ProjectToGround(const FVector& Location, FVector& OutPoint) const;

	/** Bush instance nearest to Location within Radius; false when none */
	bool FindNearestBush(const FVector& Location, float Radius, FVector& OutBush);

	/** Spawns a warrior out of the bush nearest to Near (or out of the ground there) facing Target (server) */
	ATikiWarrior* SpawnWarriorFromBush(const FVector& Near, float BushRadius, APawn* Target, ETikiWarriorRole WarriorRole);

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Editor icon */
	UPROPERTY(VisibleAnywhere, Category="Components")
	TObjectPtr<UBillboardComponent> Sprite;

	/** Villagers alive */
	TArray<TWeakObjectPtr<ATikiVillager>> Villagers;

	/** Night warriors alive */
	TArray<TWeakObjectPtr<ATikiWarrior>> NightWarriors;

	/** Defenders alive */
	TArray<TWeakObjectPtr<ATikiWarrior>> Defenders;

	/** Coconuts on the ground in the village area */
	TArray<TWeakObjectPtr<AActor>> Coconuts;

	/** Coconuts reserved by a villager */
	TMap<TWeakObjectPtr<AActor>, TWeakObjectPtr<ATikiVillager>> Claims;

	/** Last defender call per player */
	TMap<TWeakObjectPtr<APawn>, double> LastDefenderTime;

	/** Bush meshes of the level, found once */
	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> BushComponents;

	/** The bush search ran */
	bool bBushesCached = false;

	/** World time of the last coconut drop and night spawn */
	double LastCoconutDropTime = -1.0e6;
	double LastNightSpawnTime = -1.0e6;

	/** Timer of Think */
	FTimerHandle ThinkTimer;

	/** Periodic decisions (server) */
	void Think();

	/** Keeps the villagers and the coconuts */
	void TickVillage(double Now);

	/** Brings warriors out at night, sends them back at dawn */
	void TickNight(double Now);

	/** Finds the bush meshes among the instanced meshes of the level */
	void CacheBushes();

	/** Warrior class drawn at random, or nullptr */
	TSubclassOf<ATikiWarrior> PickWarriorClass() const;

	/** True when it is night on the island */
	bool IsNight() const;
};
