// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/NetSerialization.h"
#include "NetRestComponent.generated.h"

class UPrimitiveComponent;

/** Network rest of the free objects: dormancy, glue to a moving base (Verbose to follow each object) */
MONKE_ISLAND_API DECLARE_LOG_CATEGORY_EXTERN(LogMonkeNetRest, Log, All);

/** Where a free object lies glued to the moving base that carries it (replicated, T-294 / T-295) */
USTRUCT()
struct FNetRestGlue
{
	GENERATED_BODY()

	/** What carries it: the rolling deck of the galleon, the hull of the barque; none when it is free */
	UPROPERTY()
	TObjectPtr<UPrimitiveComponent> Base;

	/** Its place in the frame of the base (cm) */
	UPROPERTY()
	FVector_NetQuantize100 Location;

	/** Its rotation in the frame of the base */
	UPROPERTY()
	FRotator Rotation = FRotator::ZeroRotator;
};

/**
 *  Network rest of a free physics object (T-312, T-294, T-295; review R-04, R-05, R-28): the server simulates it and
 *  replicates its movement at AwakeNetUpdateFrequency while it moves. Asleep, it stops replicating (dormancy) and wakes
 *  as soon as it moves again, is carried or changes. On a base that moves under it (the rolling deck of the galleon,
 *  the hull of the barque), it never falls asleep (or is woken up if it did): once still on that base for GlueDelay,
 *  the server glues it there, every machine then carries it with its own copy of the base (its movement not sent, the
 *  object looked at GluedNetUpdateFrequency times a second, no byte while nothing changes), until a player takes it.
 *  It keeps simulating all along, so the grab, the cargo counts and the weights find it as before.
 *  The owner's root must be the simulating primitive. Add it to an actor and set bGenerateWakeEvents on that root
 *  for an immediate wake (the server also looks every CheckInterval).
 */
UCLASS(ClassGroup=(Physics), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UNetRestComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UNetRestComponent();

	/** Network updates per second of the owner while it moves (the engine default is 100) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="1", ClampMax="100"))
	float AwakeNetUpdateFrequency = 30.0f;

	/** Lowest rate while nothing of it changes, with net.UseAdaptiveNetUpdateFrequency 1 (the engine default is 0) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.1", ClampMax="100"))
	float AwakeMinNetUpdateFrequency = 2.0f;

	/** Network updates per second while a player holds it: its holder sees it through the server (review R-16) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="1", ClampMax="100"))
	float HeldNetUpdateFrequency = 60.0f;

	/** Network updates per second while glued to its base (deck, hull): it stays awake there, its movement not sent,
	 *  the server only looks for a change (a dormant object would keep a base its client has since dropped) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.5", ClampMax="100"))
	float GluedNetUpdateFrequency = 2.0f;

	/** At rest (asleep, or glued to what carries it), the owner stops replicating until it moves or changes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network")
	bool bDormantAtRest = true;

	/** Seconds between two looks of the server at the owner (rest, base, a change while dormant) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network", meta=(ClampMin="0.05", Units="s"))
	float CheckInterval = 0.25f;

	/** Still on a base that moves under it this long, the owner is glued to it (s); 0 = never glued */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network|Moving base", meta=(ClampMin="0", Units="s"))
	float GlueDelay = 0.75f;

	/** Still on its base: it moved less than this relative to it between two looks (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network|Moving base", meta=(ClampMin="0", Units="cm"))
	float GlueStillDistance = 1.5f;

	/** Still on its base: it turned less than this relative to it between two looks (degrees) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network|Moving base", meta=(ClampMin="0", Units="Degrees"))
	float GlueStillAngle = 2.0f;

	/** How far below its body the server looks for the base it lies on (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Network|Moving base", meta=(ClampMin="1", Units="cm"))
	float BaseProbeDepth = 25.0f;

	/** Server: glues the owner where it lies on Base (the hull of the barque stowing it); every machine carries it there
	 *  until a player takes it. Nothing when it does not simulate or Base is invalid */
	void GlueTo(UPrimitiveComponent* Base);

	/** Server: free again, simulating from where it lies with the speed of its base, and replicating */
	void Release();

	/** True while glued to a base (every machine) */
	bool IsGlued() const;

	/** Base it is glued to, nullptr when free */
	UPrimitiveComponent* GetGlueBase() const;

	/** Server: the owner replicates again at once and stays awake until it rests again (it was carried, hidden,
	 *  attached or hit while dormant) */
	void WakeNet();

	/** Server, any actor: wakes its network (its UNetRestComponent, else a plain dormancy flush); safe on nullptr */
	static void WakeActorNet(AActor* Actor);

	/** Server: changes the awake rate (the corpses take it from DA_TikiRules); applied at once when awake */
	void SetAwakeNetUpdateFrequency(float Frequency);

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Where the owner lies glued, replicated once per glue (none: free) */
	UPROPERTY(ReplicatedUsing=OnRep_Glue)
	FNetRestGlue Glue;

	/** Carries the owner with its base on this machine, or stops */
	UFUNCTION()
	void OnRep_Glue();

	/** Server: the regular look (rest, base, change while dormant) */
	void CheckRest();

	/** Server: a physics wake of the root; the owner replicates again unless it is glued */
	UFUNCTION()
	void HandleWake(UPrimitiveComponent* WakingComponent, FName BoneName);

	/** The simulating root of the owner, nullptr otherwise */
	UPrimitiveComponent* GetBody() const;

	/** True while a player holds the body (the grab makes it ignore the physics bodies, UIslanderGrabComponent) */
	static bool IsHeld(const UPrimitiveComponent* Body);

	/** Server: the base the body lies on when it is one that moves without simulating (the galleon's deck), the base of
	 *  an object glued there when it lies on one; nullptr otherwise */
	UPrimitiveComponent* FindMovingBase(const UPrimitiveComponent* Body) const;

	/** Server: dormant or awake, the movement replicated or not */
	void ApplyNetState(bool bDormant);

	/** Starts or stops carrying the body with the replicated base on this machine */
	void ApplyGlueLocally();

	/** Carries the body with Base on this machine from now on (nullptr: stops): ticks after the base's owner, and a
	 *  client drops the last replicated movement of the body */
	void SetFollowing(UPrimitiveComponent* Base);

	/** Every machine, every frame while glued: the body where it lies on the base, at the base's speed */
	void FollowBase(float DeltaSeconds);

	/** What may change on a dormant owner without moving it (hidden, collision, attachment, value...) */
	uint32 ComputeSignature() const;

	/** Server: the regular look */
	FTimerHandle CheckTimer;

	/** The owner is dormant (server) */
	bool bNetDormant = false;

	/** Rate the owner replicates at while awake now (server) */
	float AppliedRate = 0.0f;

	/** Server: the awake rate for being held, glued or moving */
	void ApplyAwakeRate(bool bHeld);

	/** Glued and carried on this machine */
	bool bFollowing = false;

	/** Base carried with on this machine, and its owner ticked before this component */
	TWeakObjectPtr<UPrimitiveComponent> FollowedBase;
	TWeakObjectPtr<AActor> FollowedBaseOwner;

	/** Signature when the owner went dormant (server) */
	uint32 DormantSignature = 0;

	/** Base found under the body at the last look, its pose then, and the body's place on it (server) */
	TWeakObjectPtr<UPrimitiveComponent> CandidateBase;
	FTransform CandidateBaseWorld;
	FTransform CandidateLocal;

	/** Time the body has stayed still on CandidateBase, and whether that base moved meanwhile (server) */
	float RestSeconds = 0.0f;
	bool bCandidateBaseMoved = false;

	/** Base that moves without simulating it fell asleep on, and its pose then (server): should the physics leave it
	 *  asleep on the rolling deck, it is woken up once the deck has moved, then glued there */
	TWeakObjectPtr<UPrimitiveComponent> SleepFloor;
	FTransform SleepFloorWorld;

	/** Where the glued body was at the last frame (every machine): its speed with the base */
	FVector LastFollowLocation = FVector::ZeroVector;
	FQuat LastFollowRotation = FQuat::Identity;
	bool bHasLastFollow = false;
};
