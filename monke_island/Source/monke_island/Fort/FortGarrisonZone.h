// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FortGarrisonZone.generated.h"

class AFortSoldier;

/** A guard post inside a building of the fort (T-332, owner 2026-10-11: "je veux plus de soldats dans les bâtiments"):
 *  found on the floors of the fort level by scripts/editor/fort_building_posts_ue.py (room for a bean, a roof above) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FFortBuildingPost
{
	GENERATED_BODY()

	/** Where the guard stands, on the floor, in the zone's frame (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	FVector Location = FVector::ZeroVector;

	/** Yaw it faces there, in the zone's frame (deg): the door of its room, or the outside from a gallery or a terrace */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(Units="deg"))
	float Yaw = 0.0f;

	/** Building of the post (barracks, guardhouse, governor...): the guards are spread over the buildings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	FName Building;
};

/**
 *  The inside of a French fort and the posts of its garrison (owner request 2026-10-07: the soldiers walk and patrol
 *  in the fort, attack the player, and cannot leave it). Outline = the cordon of the walls (bastions and curtains),
 *  in the actor frame. The soldiers that start inside are its garrison: their paths and goals stay inside, and the
 *  server puts any of them that ends up outside (knocked off a wall, pushed through the gate) back on the last place
 *  inside it stood on. A soldier that spots a player raises the alarm of the fort.
 *  The difficulty of the campaign day (DA_RunRules.DifficultyByDay, owner 2026-10-09) sets the garrison: once the
 *  soldiers of the level stood (GarrisonSettleSeconds), the server completes them at the posts (clones of those
 *  placed) or thins them to SoldiersPerFort, gives every soldier of the fort MobHealthScale times its health, and
 *  holds SoldierHitChanceScale for their shots (GetHitChanceScale). Then it posts the guards of the buildings of the
 *  day (T-332: DA_FortSoldierRules.BuildingGuardsByDay) at its BuildingPosts, the fort held under MaxSoldiersPerFort.
 */
UCLASS()
class MONKE_ISLAND_API AFortGarrisonZone : public AActor
{
	GENERATED_BODY()

public:

	AFortGarrisonZone();

	/** Outline of the fort on the ground plane, actor frame (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	TArray<FVector2D> Outline;

	/** Height band of the fort, actor frame (cm): from the bottom of the court to above the towers */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(Units="cm"))
	float MinZ = -300.0f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(Units="cm"))
	float MaxZ = 1500.0f;

	/** Goals and soldiers put back are kept this far inside the outline (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(ClampMin="0", Units="cm"))
	float InnerMargin = 80.0f;

	/** Posts of the patrol, actor frame (cm): court, gate, ramparts */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(MakeEditWidget))
	TArray<FVector> PatrolPoints;

	/** Guard posts inside the buildings (T-332): once the garrison of the day stands, the server posts there the guards
	 *  of the day (DA_FortSoldierRules.BuildingGuardsByDay), one per post at most; empty = no guard in the buildings */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort")
	TArray<FFortBuildingPost> BuildingPosts;

	/** Seconds between two checks of the garrison (server) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(ClampMin="0.02", Units="s"))
	float CheckInterval = 0.1f;

	/** Seconds after the start the garrison is counted and brought to the day's SoldiersPerFort (server): the soldiers
	 *  placed in the fort level have all begun by then */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(ClampMin="0", Units="s"))
	float GarrisonSettleSeconds = 1.0f;

	/** A soldier added by the day stands this high over its post (cm): half its capsule and a little air */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Fort", meta=(ClampMin="0", Units="cm"))
	float SpawnLiftCm = 95.0f;

	/** Chance a soldier's ball strikes, relative to its rules: the difficulty of the day, read at the start (server) */
	float GetHitChanceScale() const { return HitChanceScale; }

	/** Health of the soldiers of the fort relative to their rules: the difficulty of the day (server) */
	float GetHealthScale() const { return HealthScale; }

	/** True when Location is inside the fort (outline and height band) */
	UFUNCTION(BlueprintPure, Category="Fort")
	bool IsInside(const FVector& Location) const;

	/** Location held inside the outline (InnerMargin in), height kept */
	FVector ClampInside(const FVector& Location) const;

	/** Patrol posts (world) */
	int32 GetPatrolPointCount() const { return PatrolPoints.Num(); }
	FVector GetPatrolPoint(int32 Index) const;

	/** Server: true when a living soldier of the garrison other than Asker walks to or stands at the patrol post Index */
	bool IsPatrolPostTaken(int32 Index, const AFortSoldier* Asker) const;

	/** Guard posts of the buildings (world): where, and the yaw faced there (T-332) */
	int32 GetBuildingPostCount() const { return BuildingPosts.Num(); }
	FVector GetBuildingPostLocation(int32 Index) const;
	float GetBuildingPostYaw(int32 Index) const;

	/** Server: the soldiers of the fort within Radius of From hunt Target (one of them spotted it) */
	void RaiseAlarm(AFortSoldier* Caller, AActor* Target, const FVector& From, float Radius);

	/** The fort around Location, if any */
	static AFortGarrisonZone* FindAt(const UWorld* World, const FVector& Location);

	/** Soldiers of the garrison (server) */
	int32 GetSoldierCount() const { return Soldiers.Num(); }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Server: takes the soldiers that start inside, holds the garrison inside */
	void CheckSoldiers();

	/** Server, once the soldiers of the level stood: the garrison brought to the day's SoldiersPerFort, extra soldiers
	 *  spawned at the posts (clones of those placed, in turn) or the surplus sent away */
	void AdjustGarrison();

	/** Server: a soldier of the day: the class and look of Template, at the patrol post PostIndex, or as the guard of the
	 *  building post BuildingPostIndex when it is one; nullptr when none came */
	AFortSoldier* SpawnSoldier(const AFortSoldier* Template, int32 PostIndex, int32 BuildingPostIndex = INDEX_NONE);

	/** Server, after the garrison of the day: the guards of the buildings of the day (DA_FortSoldierRules, Garrison),
	 *  clones of Templates, spread over the buildings, the fort held under MaxSoldiersPerFort (Garrison: the soldiers
	 *  there already); returns how many came */
	int32 PostBuildingGuards(const TArray<AFortSoldier*>& Templates, int32 Garrison);

	/** Garrison and the last place inside each one stood on */
	TMap<TWeakObjectPtr<AFortSoldier>, FVector> Soldiers;

	/** Soldiers first seen outside: not of this fort */
	TSet<TWeakObjectPtr<AFortSoldier>> Strangers;

	/** Difficulty of the day, read at the start (server): health and aim of the soldiers, soldiers wanted (0 = as placed) */
	float HealthScale = 1.0f;
	float HitChanceScale = 1.0f;
	int32 WantedSoldiers = 0;

	FTimerHandle CheckTimer;
	FTimerHandle SettleTimer;
};
