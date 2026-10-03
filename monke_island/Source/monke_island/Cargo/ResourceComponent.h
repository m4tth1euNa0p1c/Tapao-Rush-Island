// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/EngineTypes.h"
#include "Cargo/CargoTypes.h"
#include "ResourceComponent.generated.h"

class UCargoComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FResourceStateChangedSignature, EResourceState, NewState, EResourceState, OldState);

/**
 *  Makes any physics actor a resource (cahier §5): stable id, category, mass, value, quantity and state
 *  Free / Carried / Stowed / Lost. Does not require APhysicsPropActor; the owner should replicate its movement
 *  so the cargo attachment reaches the clients.
 *  Server authoritative: the id and the state are set on the server and replicate; the physics of the root
 *  primitive follows the state on every machine (suspended while stowed, restored afterwards).
 *  The value comes from a UValuableComponent on the same actor when there is one (value lost on impacts).
 */
UCLASS(ClassGroup=(Cargo), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UResourceComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UResourceComponent();

	/** Family of the resource */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource")
	EResourceCategory Category = EResourceCategory::Wood;

	/** Mass counted by the cargo (kg); 0 = mass of the root physics body */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource", meta=(ClampMin="0"))
	float MassKg = 10.0f;

	/** Sale value (pieces) when the actor has no UValuableComponent */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource", meta=(ClampMin="0"))
	int32 Value = 0;

	/** Units delivered to the quota */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Resource", meta=(ClampMin="1"))
	int32 Quantity = 1;

	/** Broadcast on every machine when the state changes */
	UPROPERTY(BlueprintAssignable, Category="Resource")
	FResourceStateChangedSignature OnStateChanged;

	/** Stable id (assigned by the server at BeginPlay) */
	UFUNCTION(BlueprintPure, Category="Resource")
	FResourceId GetResourceId() const { return ResourceId; }

	/** Current state */
	UFUNCTION(BlueprintPure, Category="Resource")
	EResourceState GetState() const { return State; }

	/** Mass counted by the cargo (kg) */
	UFUNCTION(BlueprintPure, Category="Resource")
	float GetEffectiveMassKg() const;

	/** Current sale value (pieces) */
	UFUNCTION(BlueprintPure, Category="Resource")
	int32 GetCurrentValue() const;

	/** Builds the cargo entry of this resource (assigns the id first on the server if needed) */
	FCargoEntry MakeEntry();

	/** Changes the state (server only); Lost is final */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Resource")
	void SetState(EResourceState NewState);

	/** Cargo holding this resource while stowed (server only) */
	UCargoComponent* GetStowedIn() const;

	/** Records the cargo holding this resource (server only, set by UCargoComponent) */
	void SetStowedIn(UCargoComponent* Cargo);

	/** Whether stowed objects keep only query collision (set by the cargo from its rules before stowing) */
	void SetQueryOnlyWhenStowed(bool bQueryOnly) { bQueryOnlyWhenStowed = bQueryOnly; }

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	/** Applies the replicated state on clients */
	UFUNCTION()
	void OnRep_State(EResourceState OldState);

private:

	/** Suspends or restores the root physics to match the state */
	void ApplyStatePhysics(EResourceState OldState);

	/** Stable id */
	UPROPERTY(VisibleInstanceOnly, Replicated, Category="Resource")
	FResourceId ResourceId;

	/** Current state */
	UPROPERTY(VisibleInstanceOnly, ReplicatedUsing=OnRep_State, Category="Resource")
	EResourceState State = EResourceState::Free;

	/** Cargo holding the resource (server) */
	TWeakObjectPtr<UCargoComponent> StowedIn;

	/** Root physics saved while stowed (local to each machine) */
	bool bPhysicsSuspended = false;

	/** Root was simulating before being stowed */
	bool bSavedSimulate = false;

	/** Root collision before being stowed */
	TEnumAsByte<ECollisionEnabled::Type> SavedCollision = ECollisionEnabled::QueryAndPhysics;

	/** Stowed objects keep only query collision */
	bool bQueryOnlyWhenStowed = true;
};
