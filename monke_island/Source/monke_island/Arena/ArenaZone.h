// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "ArenaZone.generated.h"

class ACharacter;

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
 *  The red ring on the ground is level art (M_Test_ArenaRing).
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

	/** True when Location is inside the zone (ground plane) */
	UFUNCTION(BlueprintPure, Category="Arena")
	bool IsInside(const FVector& Location) const;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Sorts the newcomers (inside = member, outside = ignored) and holds the members (server) */
	void CheckMembers();

	/** Characters that started inside: held in the zone */
	TSet<TWeakObjectPtr<ACharacter>> Members;

	/** Characters first seen outside: never held */
	TSet<TWeakObjectPtr<ACharacter>> Ignored;

	FTimerHandle CheckTimer;
};
