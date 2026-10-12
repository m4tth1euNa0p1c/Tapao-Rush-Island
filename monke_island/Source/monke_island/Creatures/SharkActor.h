// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Pawn.h"
#include "Creatures/SharkMath.h"
#include "SharkActor.generated.h"

class UIslanderHealthComponent;
class UMobHealthBarComponent;
class USharkRules;
class USphereComponent;
class UStaticMeshComponent;
class UIslandWaterSubsystem;

/**
 *  Shark of the lagoon (cahier §8, lot L5): patrol, detection, chase, attack and return. It hunts swimming
 *  players only and never goes on land: each step is checked against the water column (surface from
 *  UIslandWaterSubsystem, bottom by a trace), so any water deep enough is navigable without a NavMesh, flood
 *  included. Server authoritative and kinematic (no physics); clients get the replicated movement and state and
 *  only animate the tail and the bite.
 *  Debug (non shipping): monke.SpawnShark spawns one in the water near the local player.
 */
UCLASS()
class MONKE_ISLAND_API ASharkActor : public APawn
{
	GENERATED_BODY()

public:

	ASharkActor();

	/** Rules of the shark (DA_SharkRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Shark")
	TObjectPtr<USharkRules> Rules;

	/** Tail sweep amplitude at cruise speed (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shark|Visual", meta=(ClampMin="0"))
	float TailSwingDeg = 18.0f;

	/** Tail beats per second at cruise speed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Shark|Visual", meta=(ClampMin="0"))
	float TailBeatHz = 1.2f;

	/** Current state (replicated) */
	UFUNCTION(BlueprintPure, Category="Shark")
	ESharkState GetState() const { return State; }

	/** Swimmer hunted, nullptr when none (server) */
	UFUNCTION(BlueprintPure, Category="Shark")
	APawn* GetPrey() const { return Prey.Get(); }

	/** Spawns a shark in water deep enough near a location (server); nullptr when there is none within 40 m */
	static ASharkActor* SpawnInWaterNear(UWorld* World, const FVector& Near, TSubclassOf<ASharkActor> SharkClass);

	virtual void Tick(float DeltaSeconds) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	/** Client: the body keeps where it is drawn and glides to the new replicated place (T-312, review R-34) */
	virtual void PostNetReceiveLocationAndRotation() override;

	/** Client: the replicated speed, carried on between two updates (T-312) */
	virtual void PostNetReceiveVelocity(const FVector& NewVelocity) override;

private:

	/** Body collision (pawn), root */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<USphereComponent> Collision;

	/** Body mesh, snout toward +X */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Body;

	/** Tail mesh, pivot at the tail joint, sweeps left and right */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> Tail;

	/** Health, damage and death */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UIslanderHealthComponent> Health;

	/** Health bar above the creature once it is hurt (local players) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UMobHealthBarComponent> HealthBar;

	/** Current state */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	ESharkState State = ESharkState::Patrol;

	/** Swimmer hunted (server) */
	TWeakObjectPtr<APawn> Prey;

	/** Centre of the patrol area: the spawn point */
	FVector Home = FVector::ZeroVector;

	/** Patrol goal (server) */
	FVector PatrolGoal = FVector::ZeroVector;

	/** World time the current state started */
	double StateStartTime = 0.0;

	/** World time of the last bite */
	double LastBiteTime = -1.0e6;

	/** Current speed (cm/s), eased toward the speed of the state */
	float Speed = 0.0f;

	/** Tail phase (radians, every machine) */
	float TailPhase = 0.0f;

	/** Bite lunge left (s, every machine) */
	float LungeTime = 0.0f;

	/** Lunge of the body this frame (cm along its snout, every machine) */
	float Lunge = 0.0f;

	/** Client: where the body is drawn relative to the replicated shark (cm, its own frame) and its yaw offset (deg);
	 *  both fade over ClientSmoothTime */
	FVector SmoothOffset = FVector::ZeroVector;
	float SmoothYaw = 0.0f;

	/** Client: world time of the last replicated place */
	double LastRepTime = -1.0e6;

	/** Rotation of the body in the actor as set up (Blueprint), the glide turns it about the vertical */
	FQuat BodyBaseRotation = FQuat::Identity;

	/** Cached water subsystem */
	TWeakObjectPtr<UIslandWaterSubsystem> Water;

	/** Server decisions and movement */
	void ServerTick(float DeltaSeconds);

	/** Enters a state (server) */
	void SetState(ESharkState NewState);

	/** Living swimmer nearest within Radius, or nullptr */
	APawn* FindPrey(float Radius) const;

	/** True when Pawn is a living, swimming player within Radius */
	bool IsHuntable(const APawn* Pawn, float Radius) const;

	/** Water surface and bottom at a horizontal position; false when there is no bottom under the water */
	bool SampleWater(const FVector& Location, float& OutSurfaceZ, float& OutBottomZ) const;

	/** True when the shark can be at this horizontal position */
	bool IsNavigableAt(const FVector& Location) const;

	/** Turns toward Goal and moves at TargetSpeed if the next step stays in deep water; false when blocked */
	bool SwimToward(const FVector& Goal, float TargetSpeed, float DeltaSeconds, float DesiredDepthUnderSurface);

	/** Draws a new navigable patrol goal around Home */
	void PickPatrolGoal();

	/** Server: bites the prey */
	void Bite(APawn* Victim);

	/** Lunge and sound of the bite on every machine */
	UFUNCTION(NetMulticast, Unreliable)
	void MulticastBite();

	/** Death on every machine: belly up, then sinks */
	UFUNCTION()
	void HandleDeath(AActor* Killer);

	/** State change on clients */
	UFUNCTION()
	void OnRep_State();

	/** Tail sweep and lunge (every machine) */
	void UpdateVisuals(float DeltaSeconds);
};
