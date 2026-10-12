// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/RequestRateLimiter.h"
#include "IslanderGrabComponent.generated.h"

class ACharacter;
class UPhysicsHandleComponent;
class UPrimitiveComponent;
class UWorld;

/**
 *  R.E.P.O. style physics grab: F on a free physics object makes it float in front of the eyes (T-166: E is the
 *  inventory screen now; a tap of F interacts first, holding F lifts whatever is in view, an item pickup included),
 *  mouse wheel to bring it closer or push it away, left click to throw, F again or G to let go; F aimed at a container
 *  (ILootContainer: the loot crate, T-258) puts the held object in it instead.
 *  The server owns the physics handle (cahier §5: range 200 cm and line of sight re-checked by the server,
 *  the object is released once when its holder dies or leaves). Heavy objects are weak alone and need
 *  several players (shared grab, owner request over the cahier's single carrier, ADR-002 P-06).
 *  The carried mass slows the holder through the movement component.
 */
UCLASS(ClassGroup=(Interaction), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UIslanderGrabComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UIslanderGrabComponent();

	/** Reach from the eyes (cm, cahier §5: 200) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float GrabRange = 200.0f;

	/** Server re-check of a reach (cahier §4, T-289): the latency tolerance added to GrabRange, and how far from its
	 *  target a point sent by a client may lie; the reach is measured to the target itself, never to that point alone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0", Units="cm"))
	float ServerReachTolerance = 50.0f;

	/** Server: interaction requests (use, lift, let go, throw, put in a container) one player may send at once: a player
	 *  mashing F stays under it, the flood of a modified client does not (security review T-269 M1, SEC-03) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab|Network", meta=(ClampMin="1"))
	float RequestBurst = 10.0f;

	/** Server: interaction requests given back each second once RequestBurst is spent */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab|Network", meta=(ClampMin="0.1"))
	float RequestsPerSecond = 10.0f;

	/** Closest and farthest hold distance from the eyes (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float MinHoldDistance = 70.0f;

	/** See MinHoldDistance */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float MaxHoldDistance = 220.0f;

	/** Hold distance change per mouse wheel notch (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float WheelStep = 15.0f;

	/** Mass one player lifts at full strength (kg); heavier objects drag unless more players hold them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0.1"))
	float LiftMassKg = 25.0f;

	/** Actors with this tag are never grabbed, whatever their mass (the boat, the galleon: they are pushed or rowed) */
	static const FName NoGrabTag;

	/** Objects heavier than this cannot be grabbed at all (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float MaxGrabMassKg = 400.0f;

	/** Weakest pull, as a fraction of full strength, on an object far too heavy */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0", ClampMax="1"))
	float MinStrengthFraction = 0.12f;

	/** Physics handle springs at full strength */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float LinearStiffness = 900.0f;

	/** See LinearStiffness */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float LinearDamping = 160.0f;

	/** See LinearStiffness */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float AngularStiffness = 600.0f;

	/** See LinearStiffness */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float AngularDamping = 100.0f;

	/** Radius around the crosshair's point where a small interactable (a pistol on the ground) is still found (cm, 0 = ray only) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0", Units="cm"))
	float InteractionAssistRadius = 15.0f;

	/** Lowest the held object goes when the player looks down (degrees below the horizon): looking at one's feet
	 *  keeps it in front, never under the body */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0", ClampMax="89", Units="deg"))
	float MaxHoldPitchDownDeg = 45.0f;

	/** Farthest the handle target may lead the held object (cm): caps the spring force on what it presses against */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="1"))
	float MaxTargetLead = 80.0f;

	/** Bodies at least this heavy (kg), and anything static, stop the held object instead of being shoved by the
	 *  handle: a boat hull under the holder's feet must not be pushed by its own passenger (lighter objects are bumped) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float HeavyBodyMassKg = 150.0f;

	/** Speed given by a throw at full strength (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0"))
	float ThrowSpeed = 900.0f;

	/** Several players may hold the same object and add their strength (R.E.P.O.); false = one carrier (cahier §5) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab")
	bool bAllowSharedGrab = true;

	/**
	 *  F held this long lifts the physics object in view instead of interacting (T-166); a shorter tap interacts
	 *  (pick up, sit...) on release. 0 = every press interacts at once (no hold).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grab", meta=(ClampMin="0", Units="s"))
	float HoldToGrabSeconds = 0.3f;

	/** Interaction key pressed (local player): lets go at once when holding, otherwise waits for a tap or a hold */
	void PressInteractKey();

	/** Interaction key released (local player): a tap interacts */
	void ReleaseInteractKey();

	/** Grabs what the player looks at (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Grab")
	void RequestGrab();

	/** Interaction key (F): lets go when holding, otherwise uses the interactable in view or lifts the physics object in view */
	UFUNCTION(BlueprintCallable, Category="Grab")
	void RequestInteract();

	/** What the crosshair points at within reach: an interactable actor, or a free physics object to lift (local check) */
	bool FindInteractionTarget(AActor*& OutInteractable, UPrimitiveComponent*& OutPhysics, FVector& OutPoint) const;

	/**
	 *  While holding: the container in view within reach that takes the held object (ILootContainer, e.g. the loot crate,
	 *  T-258), the held object itself left out of the aim. F then puts the object in instead of letting it go
	 *  (local check, the server re-checks)
	 */
	bool FindStoreTarget(AActor*& OutContainer, FVector& OutPoint) const;

	/**
	 *  Server: lifts this free physics object for the owner, with the checks of a grab aimed at it (reach, sight, mass,
	 *  single carrier): what an interactable that is carried asks for, e.g. the loot crate's "Porter la caisse" (T-258).
	 *  True when the object is held
	 */
	bool GrabFromInteraction(UPrimitiveComponent* Target, const FVector& GrabPoint);

	/** Lets go of the held object */
	UFUNCTION(BlueprintCallable, Category="Grab")
	void RequestRelease();

	/** Throws the held object forward */
	UFUNCTION(BlueprintCallable, Category="Grab")
	void RequestThrow();

	/** Moves the held object closer (negative) or farther (positive), in wheel notches */
	UFUNCTION(BlueprintCallable, Category="Grab")
	void AdjustHoldDistance(float Notches);

	/** Object held now (replicated, drives the arms of every player) */
	UFUNCTION(BlueprintPure, Category="Grab")
	UPrimitiveComponent* GetHeldComponent() const { return HeldComponent; }

	/** True while holding something */
	UFUNCTION(BlueprintPure, Category="Grab")
	bool IsHolding() const { return HeldComponent != nullptr; }

	/** Eye location and view direction used to aim and hold */
	void GetEyes(FVector& OutLocation, FVector& OutDirection) const;

	/** Physics object under the crosshair within reach, if any (local check, the server re-checks) */
	UPrimitiveComponent* FindGrabTarget(FVector& OutGrabPoint) const;

	/** Pull strength of one holder, 1 at or under LiftMassKg per holder */
	static float ComputeStrength(float MassKg, int32 Holders, float InLiftMassKg, float MinFraction);

	/** Server check of a reach (cahier §4, T-289): Target within Reach of Eyes (its bounds) and in sight (nothing else
	 *  between the eyes and its middle, AlsoIgnored aside); never measured to a point sent by a client */
	static bool IsActorInReach(const UWorld* World, const AActor* Target, const AActor* Self, const AActor* AlsoIgnored,
		const FVector& Eyes, float Reach);

	/** Where the held object goes: eyes, a hold direction pitched no lower than MaxHoldPitchDownDeg, and the
	 *  view yaw (from the control rotation: the yaw of a straight-down look vector is undefined and made it spin) */
	void GetHoldAim(FVector& OutEyes, FVector& OutDirection, float& OutYaw) const;

	/** Handle target for the held object: stops at static or heavy obstacles (a boat hull, a wall) instead of
	 *  pressing into them, slides along them, never leads the object by more than MaxTargetLead (server) */
	FVector ClampHoldTarget(const FVector& Desired) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION(Server, Reliable)
	void ServerGrab(UPrimitiveComponent* Target, FVector_NetQuantize GrabPoint);

	UFUNCTION(Server, Reliable)
	void ServerRelease();

	UFUNCTION(Server, Reliable)
	void ServerInteract(AActor* Target, FVector_NetQuantize Point);

	/** Puts the held object in the container aimed at (T-258), after the server's checks */
	UFUNCTION(Server, Reliable)
	void ServerStoreHeld(AActor* Container, FVector_NetQuantize Point);

	UFUNCTION(Server, Reliable)
	void ServerThrow();

	UFUNCTION(Server, Unreliable)
	void ServerAdjustHoldDistance(float Notches);

	/** Applies the carried weight on the owning client */
	UFUNCTION()
	void OnRep_HeldMass();

private:

	/** Interactable near AimPoint (InteractionAssistRadius), in reach and sight from Eyes; the closest to AimPoint */
	bool FindAssistedInteractable(const FVector& Eyes, const FVector& AimPoint, AActor*& OutInteractable, FVector& OutPoint) const;

	/** Server: uses Target after validation (CanInteract, reach and sight from the eyes; the ridden actor always in reach) */
	void InteractWith(AActor* Target, const FVector& Point);

	/** Server: grab after validation */
	void Grab(UPrimitiveComponent* Target, const FVector& GrabPoint);

	/** Server: let go, optionally with a throw */
	void Release(bool bThrow);

	/** F while holding: into the container in view when there is one (T-258), else let go */
	void ReleaseOrStore();

	/** Server: puts the held object in a container after validation (reach, sight), every holder letting go first */
	void StoreHeld(AActor* Container, const FVector& Point);

	/** Number of players holding this component */
	int32 CountHolders(const UPrimitiveComponent* Component) const;

	/** Server: one more request of this player allowed now (RequestBurst, RequestsPerSecond); a refusal is logged in
	 *  Verbose and the request dropped */
	bool ConsumeRequest(const TCHAR* What);

	/** Server: token bucket of the requests of this player */
	FRequestRateLimiter RequestLimiter;

	ACharacter* GetCharacter() const;

	/** Applies the carried mass to the movement component */
	void ApplyCarriedWeight();

	/** Object held now */
	UPROPERTY(ReplicatedUsing=OnRep_HeldComponent)
	TObjectPtr<UPrimitiveComponent> HeldComponent;

	/** Ghosts the newly held object, restores the one let go (every machine) */
	UFUNCTION()
	void OnRep_HeldComponent(UPrimitiveComponent* PreviousHeld);

	/** A held object passes through the other physics bodies (a boat hull, loose props): its hand spring would
	 *  otherwise shove them, the boat under the holder's feet first; ClampHoldTarget still stops it at their
	 *  surface. Back to its profile once nobody holds it */
	void SetHeldGhost(UPrimitiveComponent* Component, bool bGhost) const;

	/** Share of the held mass carried by this player (kg), slows the owner */
	UPROPERTY(ReplicatedUsing=OnRep_HeldMass)
	float HeldMass = 0.0f;

	/** Server physics handle */
	UPROPERTY(Transient)
	TObjectPtr<UPhysicsHandleComponent> Handle;

	/** Distance from the eyes the object is held at (cm) */
	float HoldDistance = 140.0f;

	/** Object yaw relative to the view yaw when grabbed */
	FQuat RotationOffset = FQuat::Identity;

	/** F held long enough: lifts what is in view (an interactable alone is used instead) */
	void HandleInteractHold();

	/** The interaction key is down and its tap or hold has not been used yet */
	bool bInteractKeyPending = false;

	FTimerHandle InteractHoldTimer;
};
