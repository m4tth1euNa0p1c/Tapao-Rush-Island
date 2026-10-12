// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaZone.generated.h"

class ACharacter;
class AFortSoldier;

namespace ArenaZoneMath
{
	/** Horizontal clamp of Location into the circle (Center, Radius); Z is kept. Returns true when it moved */
	MONKE_ISLAND_API bool ClampIntoCircle(const FVector& Center, float Radius, FVector& InOutLocation);
}

/**
 *  Logical combat zone (owner request 2026-10-06, test map L_Test_Flat): the hostile mobs that start inside the
 *  circle never leave it, the players and the other actors come and go freely. The tiki move in straight lines
 *  without pathfinding (ATikiCreature::MoveToward), so the nav volumes alone cannot hold them: the server puts any
 *  member that crosses the edge back inside and stops its movement. Mobs that start outside are never pulled in.
 *  The red ring on the ground is level art (M_Test_ArenaRing). Owner request 2026-10-08: French soldiers appear in
 *  the ring at start (SoldierClasses) and every side fights every other one (Creatures/Hostility).
 */
UCLASS()
class MONKE_ISLAND_API AArenaZone : public AActor
{
	GENERATED_BODY()

public:

	AArenaZone();

	/** Radius of the zone (cm), measured on the ground plane from the actor location */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena", meta=(ClampMin="100", Units="cm"))
	float Radius = 2500.0f;

	/** A member is put back this far inside the edge (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena", meta=(ClampMin="0", Units="cm"))
	float InnerMargin = 50.0f;

	/** Seconds between two checks of the members (server) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena", meta=(ClampMin="0.02", Units="s"))
	float CheckInterval = 0.1f;

	/** Characters held in the zone (the hostile tiki by default; the villagers are not warriors) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena")
	TSubclassOf<ACharacter> ConfinedClass;

	/** Other kinds held in the zone as well (the giant crab mini-boss by default, T-211, and the fort soldiers) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena")
	TArray<TSubclassOf<ACharacter>> ExtraConfinedClasses;

	/** Fort soldiers spawned inside the zone when the game starts (server; BP_BeanFR_*, one kind drawn at random for
	 *  each of them); empty: none (nothing is loaded by path) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena|Soldiers")
	TArray<TSubclassOf<AFortSoldier>> SoldierClasses;

	/** How many of them */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena|Soldiers", meta=(ClampMin="0"))
	int32 SoldierCount = 4;

	/** They appear at least this far inside the edge (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena|Soldiers", meta=(ClampMin="0", Units="cm"))
	float SoldierSpawnMargin = 300.0f;

	/** The ground under a spawn point is looked for this far above and below the zone (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Arena|Soldiers", meta=(ClampMin="100", Units="cm"))
	float GroundTraceHeight = 5000.0f;

	/** True when Character is of a kind held in the zone */
	bool IsConfined(const ACharacter* Character) const;

	/** True when Location is inside the zone (ground plane) */
	UFUNCTION(BlueprintPure, Category="Arena")
	bool IsInside(const FVector& Location) const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Sorts the newcomers (inside = member, outside = ignored) and holds the members (server) */
	void CheckMembers();

	/** Spawns the soldiers of SoldierClasses on the ground at random points of the zone (server); returns how many */
	int32 SpawnSoldiers();

	/** Ground point under a random spot of the disc of SpawnRadius around the centre; false when none in a few tries */
	bool FindGroundPoint(float SpawnRadius, FVector& OutPoint) const;

	/** Characters that started inside: held in the zone */
	TSet<TWeakObjectPtr<ACharacter>> Members;

	/** Characters first seen outside: never held */
	TSet<TWeakObjectPtr<ACharacter>> Ignored;

	FTimerHandle CheckTimer;
};
