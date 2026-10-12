// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Foliage/WindLeavesSim.h"
#include "WindLeavesActor.generated.h"

class UInstancedStaticMeshComponent;
class UMaterialParameterCollection;
class UWindLeavesRules;

/**
 *  Leaves carried by the wind around the local camera: a fixed budget of instances (one instanced
 *  component per leaf mesh) moved in one batch a frame, born upwind, tumbling in the swirls, landing on
 *  the ground or floating on the water. Calm: a few leaves drift by; storm: a flurry.
 *  Cosmetic and local (not replicated); spawned by UWindLeavesSubsystem.
 */
UCLASS(NotPlaceable, Transient)
class MONKE_ISLAND_API AWindLeavesActor : public AActor
{
	GENERATED_BODY()

public:

	AWindLeavesActor();

	/** Sets the rules and builds the instanced components (call once, before or after BeginPlay) */
	void Init(UWindLeavesRules* InRules);

	virtual void Tick(float DeltaSeconds) override;

	/** Leaves alive now */
	int32 GetLiveLeafCount() const;

private:

	/** Wind strength (0 calm .. 1 storm, more in gusts) and the direction it blows toward (unit, horizontal) */
	void ReadWind(float& OutStrength, FVector2D& OutDirection) const;

	/** Gives life to a dead slot near ViewLocation; false when the ground could not be found */
	bool SpawnLeaf(WindLeavesSim::FLeaf& Leaf, const FVector& ViewLocation, const FVector2D& WindDirection, float WaterZ);

	/** Writes every instance transform (hidden leaves collapse at the view location) */
	void PushInstances(const FVector& ViewLocation);

	/** Rules (DA_WindLeavesRules or defaults) */
	UPROPERTY(Transient)
	TObjectPtr<UWindLeavesRules> Rules;

	/** Wind parameters read every frame (MPC_Wind) */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> WindParameters;

	/** One instanced component per leaf mesh */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UInstancedStaticMeshComponent>> LeafComponents;

	/** Leaf slots: slot i always uses mesh i % LeafComponents.Num() */
	TArray<WindLeavesSim::FLeaf> Leaves;

	/** Transforms per component, reused every frame */
	TArray<TArray<FTransform>> InstanceTransforms;

	FRandomStream Random;

	/** Fractional leaves waiting to be born */
	float SpawnBudget = 0.0f;

	/** Clock of the swirls (s) */
	float AirTime = 0.0f;
};
