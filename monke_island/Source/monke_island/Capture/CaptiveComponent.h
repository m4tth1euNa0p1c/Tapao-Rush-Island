// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Capture/CaptureTypes.h"
#include "CaptiveComponent.generated.h"

class ACapturePrisoner;
class AHandcuffsItem;
class APawn;
class UBeanPuppetComponent;
class UCaptureRules;
class UInstancedStaticMeshComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnCaptiveStateChanged, ECaptiveState /*NewState*/);
DECLARE_MULTICAST_DELEGATE(FOnCaptiveCageReached);

/**
 *  State of a prisoner (T-336), on the ACapturePrisoner that replaces a cuffed soldier or Veilleur: led by a jailer,
 *  let go, walking into a cage, caged. Server authoritative and replicated (state, jailer): every machine draws the
 *  chain between the handcuffs and the jailer's hand from them (cosmetic, never replicated).
 *  Server: follows its jailer at FollowDistanceCm on the navigation mesh when there is one, straight otherwise, never
 *  into deep water; let go when its jailer dies, swims, leaves, or gets farther than ReleaseDistanceCm (it then waits,
 *  still handcuffed, for anyone to take its chain; DA_CaptureRules.AbandonedEscapeSeconds later it flees). Walking into a
 *  cage, it is moved along the points of the cage, the movement of its character off (no gravity, no collision).
 */
UCLASS(ClassGroup=(Capture))
class MONKE_ISLAND_API UCaptiveComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UCaptiveComponent();

	/** Every machine: the state changed (the prisoner adjusts its collision and its pose) */
	FOnCaptiveStateChanged OnStateChanged;

	/** Server: the walk into a cage reached its end (or ran out of time) */
	FOnCaptiveCageReached OnCageReached;

	/** Where the prisoner is (replicated) */
	UFUNCTION(BlueprintPure, Category="Capture")
	ECaptiveState GetState() const { return State; }

	/** Its jailer while led (replicated), nullptr otherwise */
	UFUNCTION(BlueprintPure, Category="Capture")
	APawn* GetJailer() const { return State == ECaptiveState::Led ? Jailer.Get() : nullptr; }

	/** True while Pawn leads it */
	bool IsLedBy(const APawn* Pawn) const { return Pawn && State == ECaptiveState::Led && Jailer == Pawn; }

	/** Server: the handcuffs on its wrists (they are free again once it is sold, fled or gone) */
	void SetHandcuffs(AHandcuffsItem* InHandcuffs);

	/** Server: the handcuffs on its wrists, nullptr once gone */
	AHandcuffsItem* GetHandcuffs() const;

	/** Server: on the chain of Jailer (the capture, or a chain taken back) */
	void SetLed(APawn* InJailer);

	/** Server: let go where it stands, still handcuffed */
	void Abandon(ECaptiveRelease Reason);

	/** Server: walks to Entry, then to Inside (feet), facing InsideYaw at the end */
	void WalkIntoCage(const FVector& Entry, const FVector& Inside, float InsideYaw);

	/** Server: behind the closed door, still */
	void SetCaged();

	/** Server, every frame: the checks of the chain and the steps of the prisoner */
	void ServerUpdate(float DeltaSeconds);

	/** Every machine with a picture: the links of the chain from Anchor (the handcuffs) to the jailer's hand; none when
	 *  it is not led */
	void UpdateChain(UInstancedStaticMeshComponent* Links, const FVector& Anchor);

	/** Where the chain ends at the jailer on this machine (its hand, or the view of its own player in first person) */
	FVector GetJailerChainEnd(const APawn* InJailer) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Every machine: the state the server set */
	UFUNCTION()
	void OnRep_State();

	/** Sets the state and tells this machine */
	void ApplyState(ECaptiveState NewState);

	/** Server: one step behind the jailer (path or straight), at the pace of the chain */
	void Follow(APawn* InJailer, float DeltaSeconds);

	/** Server: one step of the walk into the cage */
	void StepIntoCage(float DeltaSeconds);

	/** Server: stops the path and the steps */
	void StopMoving();

	/** Server: true when a step toward Direction keeps the feet out of water deeper than the rules allow */
	bool IsDryAhead(const FVector& Direction) const;

	/** The prisoner owning this component, and its rules */
	ACapturePrisoner* GetPrisoner() const;
	const UCaptureRules* GetRules() const;

	/** Where it is (replicated) */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	ECaptiveState State = ECaptiveState::Abandoned;

	/** Its jailer (replicated: every machine draws the chain to it) */
	UPROPERTY(Replicated)
	TObjectPtr<APawn> Jailer;

	/** Server: the handcuffs on its wrists */
	TWeakObjectPtr<AHandcuffsItem> Handcuffs;

	/** Server: path on the navigation mesh under way, else straight steps; when the next path is asked */
	bool bPathing = false;
	double NextRepathTime = 0.0;

	/** Server: the walk into the cage: its points, the one it walks to, when it started, the yaw at the end */
	FVector CagePoints[2] = { FVector::ZeroVector, FVector::ZeroVector };
	int32 CagePoint = 0;
	double CageWalkStart = 0.0;
	float CageYaw = 0.0f;

	/** Links drawn on this machine (the instances are kept and reused) */
	TArray<FTransform> LinkTransforms;

	/** The body (puppet) of the jailer the chain ends at, found once per jailer */
	mutable TWeakObjectPtr<const UBeanPuppetComponent> JailerPuppet;
	mutable TWeakObjectPtr<const APawn> JailerPuppetOwner;
};
