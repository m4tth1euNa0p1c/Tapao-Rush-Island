// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IslanderGrabComponent.generated.h"

class ACharacter;
class UPhysicsHandleComponent;
class UPrimitiveComponent;

/**
 *  R.E.P.O. style physics grab: hold E on a free physics object to make it float in front of the eyes,
 *  mouse wheel to bring it closer or push it away, left click to throw, release E or G to let go.
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

	/** Grabs what the player looks at (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Grab")
	void RequestGrab();

	/** Interaction key (F): lets go when holding, otherwise uses the interactable in view or lifts the physics object in view */
	UFUNCTION(BlueprintCallable, Category="Grab")
	void RequestInteract();

	/** What the crosshair points at within reach: an interactable actor, or a free physics object to lift (local check) */
	bool FindInteractionTarget(AActor*& OutInteractable, UPrimitiveComponent*& OutPhysics, FVector& OutPoint) const;

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

	UFUNCTION(Server, Reliable)
	void ServerThrow();

	UFUNCTION(Server, Unreliable)
	void ServerAdjustHoldDistance(float Notches);

	/** Applies the carried weight on the owning client */
	UFUNCTION()
	void OnRep_HeldMass();

private:

	/** Server: grab after validation */
	void Grab(UPrimitiveComponent* Target, const FVector& GrabPoint);

	/** Server: let go, optionally with a throw */
	void Release(bool bThrow);

	/** Number of players holding this component */
	int32 CountHolders(const UPrimitiveComponent* Component) const;

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
};
