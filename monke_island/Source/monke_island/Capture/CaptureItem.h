// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Core/RequestRateLimiter.h"
#include "Capture/CaptureTypes.h"
#include "CaptureItem.generated.h"

class APlayerController;
class UAnimSequence;
class UBeanPuppetComponent;
class UCaptureRules;
class UInputAction;
class UInputMappingContext;
class UStaticMesh;
class UStaticMeshComponent;

/**
 *  Base of the capture items (T-336): the cage (ACageItem) and the steel handcuffs (AHandcuffsItem). Held in the right
 *  hand of the bean (the mesh placed on the hand bone after the puppet's pose, as the torch), the body's own arms seen
 *  in first person (UBeanAnimDriverComponent::BodyArmsInFirstPersonTag). Their key (DA_CaptureRules.UseKeys, the
 *  interaction key F) is mapped above the interaction only while the item can act (a spot on the beach, a subdued
 *  target in front), so F keeps picking things up elsewhere; the context and its action are built at runtime from the
 *  rules (as the wheel of the galleon). The owner checks every LocalCheckSeconds what it may do and shows it (the HUD
 *  reads GetPrompt through UCaptureSubsystem); the server checks every request again, rate limited.
 *  A gesture (placing the cage, closing the handcuffs) is started by the server and replicated whole: every machine
 *  plays its clip on the bean (UBeanAnimDriverComponent::PlayFullBody) and the owner's movement keys rest meanwhile.
 *  Ticks only while in hand or during a gesture.
 */
UCLASS(Abstract)
class MONKE_ISLAND_API ACaptureItem : public AInventoryItemActor
{
	GENERATED_BODY()

public:

	ACaptureItem();

	/** Capture rules (DA_CaptureRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Capture")
	TObjectPtr<UCaptureRules> Rules;

	/** This machine (the owner): the line the HUD shows for this item; false when it has nothing to say */
	virtual bool GetPrompt(FCaptureHudLine& Out) const { return false; }

	/** The item can act now: in hand, its carrier alive, standing on the ground, out of the water */
	bool IsUsable() const;

	/** Gesture playing (replicated) */
	ECaptureGesture GetGesture() const { return Gesture.Gesture; }

	/** Share of the gesture done on this machine (0 to 1), from the server's clock */
	float GetGestureProgress() const;

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;
	virtual void OnAttachedToCarrier() override;

	/** Forgets the bindings of a former carrier: the next one binds its own (T-288) */
	virtual void OnInputBindingsRemoved() override;

	/** Mesh held in the hand (none: nothing shown), its grip on the hand bone, and whether it shows now */
	virtual UStaticMesh* GetHeldMesh() const { return nullptr; }
	virtual FTransform GetHeldGrip() const { return FTransform::Identity; }
	virtual bool ShouldShowHeldMesh() const { return IsEquipped(); }

	/** The held mesh out of the hand now (set down during a gesture): its world transform; false to keep it in the hand */
	virtual bool GetHeldOverride(FTransform& OutWorld) const { return false; }

	/** Seconds since the start of the gesture playing on this machine (the server's clock), 0 without one */
	float GetGestureElapsed() const;

	/** Owner: the key is mapped now (what the item would do is possible) */
	virtual bool WantsUseKey() const { return false; }

	/** Owner: the key went down / up */
	virtual void OnUsePressed() {}
	virtual void OnUseReleased() {}

	/** Owner: every LocalCheckSeconds, what the item could do now (target, spot) */
	virtual void RefreshLocal() {}

	/** Clip of a gesture on the bean (the carrier's animation library), nullptr when none */
	UAnimSequence* GetGestureClip(ECaptureGesture InGesture) const;

	/** Server: starts a gesture of Seconds for every machine */
	void StartGesture(ECaptureGesture InGesture, float Seconds);

	/** Server: the gesture ends (done or cut) */
	void StopGesture();

	/** Server: one more request of the carrier allowed now (rules: RequestBurst, RequestsPerSecond) */
	bool ConsumeRequest(const TCHAR* What);

	/** Server: the carrier is told why its request is refused */
	void Refuse(ECaptureRefusal Reason, float HealthFraction = 1.0f);

	/** Server: a short message for the carrier */
	void NotifyCarrier(const FText& Text, bool bWarning);

	/** Server world time on this machine */
	double GetServerNow() const;

	/** Every machine: the owner held its key down when the last check ran */
	bool IsUseKeyDown() const { return bUseKeyDown; }

	/** Held mesh in the right hand */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components")
	TObjectPtr<UStaticMeshComponent> HeldMesh;

private:

	/** Every machine: the gesture of the server plays (or stops) */
	UFUNCTION()
	void OnRep_Gesture(const FCaptureGestureRep& Previous);

	/** A short message for the carrier's machine */
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FText& Text, bool bWarning);

	/** Every machine: starts or stops the clip of a gesture on the bean, and the owner's hold of the movement keys */
	void PlayGestureLocally(const FCaptureGestureRep& Rep);

	/** Owner: holds or frees the movement keys during a gesture (a stack: one hold, one release) */
	void SetGestureHold(bool bHold);

	/** Places the held mesh on the hand bone of the puppet, after its pose */
	void UpdateHeldMesh();

	/** Owner: the runtime action and context of the use keys */
	void EnsureInput();

	/** Owner: binds the use action on the carrier's input once */
	void BindInput();

	void HandleUsePressed();
	void HandleUseReleased();

	/** Gesture of the server (replicated whole) */
	UPROPERTY(ReplicatedUsing=OnRep_Gesture)
	FCaptureGestureRep Gesture;

	/** Owner: the action and context built from the rules */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UseAction;

	/** See UseAction */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> UseContext;

	/** Server: the carrier's requests */
	FRequestRateLimiter RequestLimiter;

	/** Puppet of the carrier (the hand bone) */
	TWeakObjectPtr<UBeanPuppetComponent> Puppet;

	/** Owner: the controller whose movement keys rest during a gesture */
	TWeakObjectPtr<APlayerController> HoldController;
	bool bGestureHold = false;

	/** Clip of the gesture playing on this machine */
	TWeakObjectPtr<UAnimSequence> PlayingClip;

	bool bInputBound = false;
	bool bUseKeyDown = false;
	double NextLocalCheck = 0.0;
};
