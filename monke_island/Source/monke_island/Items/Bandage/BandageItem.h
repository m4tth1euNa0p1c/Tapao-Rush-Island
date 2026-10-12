// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/InventoryItemActor.h"
#include "Core/RequestRateLimiter.h"
#include "Items/Bandage/BandageRules.h"
#include "BandageItem.generated.h"

class APawn;
class APlayerController;
class UAnimSequence;
class UBeanFirstPersonArmsComponent;
class UBeanPuppetComponent;
class UInputAction;
class UInputMappingContext;
class UIslanderHealthComponent;
class UStaticMeshComponent;

namespace BandageMath { struct FCarrierFacts; }

/** An application of the server, replicated whole (the serial restarts the same state) */
USTRUCT()
struct FBandageApplyRep
{
	GENERATED_BODY()

	/** Bumped at each change */
	UPROPERTY()
	uint8 Serial = 0;

	/** A bandage is under way */
	UPROPERTY()
	bool bApplying = false;

	/** The heal landed (BandageDone) during the last application */
	UPROPERTY()
	bool bHealed = false;

	/** What cut the last application (None when it went to its end) */
	UPROPERTY()
	EBandageInterrupt Interrupt = EBandageInterrupt::None;

	/** Server world time of the start (s) */
	UPROPERTY()
	double StartServerTime = 0.0;

	/** Length of the application (s) */
	UPROPERTY()
	float Seconds = 0.0f;
};

/**
 *  A stack of healing bandages in the inventory (T-340, owner request 2026-10-11, docs/features/bandages.md). In hand the
 *  bean holds a linen roll in its right mitten; F or a left click (DA_BandageRules.UseKeys, mapped over the interaction
 *  and the punch only while a bandage can be applied) asks the server, which checks the carrier (alive, standing, out of
 *  deep water, not running, hurt), rate limited (SEC-03), and starts the application for everyone: the clip
 *  A_Bean_Bandage_Apply on the whole body, the roll going round the left forearm in first person. A blow, a sprint, a
 *  jump, deep water, putting it away or going down cuts it (nothing used); at the BandageDone moment health goes back to
 *  its maximum (UIslanderHealthComponent::ApplyHeal) and one bandage of the stack is used at the end of the gesture.
 *  The owner's HUD reads GetHudLine (STapaoHUD): the prompt, the gauge of the application, why not.
 *  Ticks only while in hand, during an application or while the wrapped bandage lingers.
 */
UCLASS()
class MONKE_ISLAND_API ABandageItem : public AInventoryItemActor
{
	GENERATED_BODY()

public:

	ABandageItem();

	/** Bandage rules (DA_BandageRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Bandage")
	TObjectPtr<UBandageRules> Rules;

	/** Asks the server to apply a bandage (owning client or server); ignored on any other machine */
	UFUNCTION(BlueprintCallable, Category="Bandage")
	void RequestApply();

	/** A bandage is under way (replicated) */
	UFUNCTION(BlueprintPure, Category="Bandage")
	bool IsApplying() const { return ApplyState.bApplying; }

	/** Share of the application done on this machine (0 to 1), from the server's clock */
	UFUNCTION(BlueprintPure, Category="Bandage")
	float GetApplyProgress() const;

	/** Seconds since the start of the application on this machine, 0 without one */
	float GetApplyElapsed() const;

	/** The last application, as replicated */
	const FBandageApplyRep& GetApplyState() const { return ApplyState; }

	/** Why a bandage may not start now for this carrier (None: it may); the server's check and the owner's preview */
	EBandageRefusal JudgeStart() const;

	/** Bandages in the cell of this item (0 when it is in no inventory) */
	int32 GetStackCount() const;

	/** The line the HUD shows for this item on its owner's machine; false when it has nothing to say */
	bool GetPrompt(FText& OutText, float& OutProgress, bool& bOutWarning) const;

	/** The line of the bandage in the hand of Pawn (the local pawn of the HUD); false without one or nothing to say */
	static bool GetHudLine(const APawn* Pawn, FText& OutText, float& OutProgress, bool& bOutWarning);

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnEquippedChanged() override;
	virtual void OnAttachedToCarrier() override;

	/** Forgets the bindings of a former carrier: the next one binds its own (T-288) */
	virtual void OnInputBindingsRemoved() override;

	/** Server side of RequestApply */
	UFUNCTION(Server, Reliable)
	void ServerApply();

	/** A short line for the carrier's machine (a refusal) */
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FText& Text, bool bWarning);

	/** Server: a blow on the carrier cuts the bandage */
	UFUNCTION()
	void HandleCarrierHealthChanged(float NewHealth, float Delta);

	UFUNCTION()
	void OnRep_ApplyState(const FBandageApplyRep& Previous);

private:

	/** The facts of the carrier now (every machine; the server's decide) */
	BandageMath::FCarrierFacts GatherFacts() const;

	/** Server: starts an application for everyone */
	void StartApplying();

	/** Server: the BandageDone moment: health back to its maximum */
	void LandHeal();

	/** Server: the gesture ended; a bandage that healed is used (last: may destroy this actor) */
	void FinishApplying();

	/** Server: cuts the application (nothing used when it had not healed yet) */
	void Interrupt(EBandageInterrupt Reason);

	/** Server: unbinds the carrier's health and clears the timers of an application */
	void EndApplicationBookkeeping();

	/** Server: one more request of the carrier allowed now */
	bool ConsumeRequest();

	/** Server: a short line for the carrier */
	void NotifyCarrier(const FText& Text, bool bWarning);

	/** Every machine: plays (or stops) the clip of the state, the owner's hold of the movement keys, the linger */
	void PlayLocally(const FBandageApplyRep& Previous);

	/** Owner: holds or frees the movement keys (one hold, one release) */
	void SetMoveHold(bool bHold);

	/** Places the roll and the wrapped bandage on the bones of the puppet, after its pose */
	void UpdateBodyMeshes();

	/** First person roll and arms of the carrier's own view */
	void UpdateFirstPerson();

	/** Owner: the runtime action and context of the use keys */
	void EnsureInput();

	/** Owner: binds the use action on the carrier's input once */
	void BindInput();

	void HandleUsePressed();

	/** Owner: the key would apply a bandage now */
	bool WantsUseKey() const;

	/** Server world time on this machine */
	double GetServerNow() const;

	/** Roll in the right hand */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> RollMesh;

	/** Bandage wrapped around the left forearm (from WrapStart, and a while after a heal) */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> WrappedMesh;

	/** Roll of the first person view, seen by its carrier only, attached to its first person camera */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Components", meta=(AllowPrivateAccess="true"))
	TObjectPtr<UStaticMeshComponent> FirstPersonRoll;

	/** Application of the server (replicated whole) */
	UPROPERTY(ReplicatedUsing=OnRep_ApplyState)
	FBandageApplyRep ApplyState;

	/** Owner: the action and context built from the rules */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> UseAction;

	/** See UseAction */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> UseContext;

	/** Server: the carrier's requests (RequestBurst, RequestsPerSecond) */
	FRequestRateLimiter RequestLimiter;

	/** Server: the health bound for the blows of the application under way */
	TWeakObjectPtr<UIslanderHealthComponent> BoundHealth;

	FTimerHandle HealTimer;
	FTimerHandle FinishTimer;

	TWeakObjectPtr<UBeanPuppetComponent> Puppet;
	TWeakObjectPtr<UBeanFirstPersonArmsComponent> Arms;

	/** Clip of the application playing on this machine */
	TWeakObjectPtr<UAnimSequence> PlayingClip;

	/** Owner: the controller whose movement keys rest during an application */
	TWeakObjectPtr<APlayerController> HoldController;
	bool bMoveHold = false;

	/** This machine: the wrapped bandage stays on the forearm until this world time */
	double WrappedUntil = -1.0;

	/** Owner: a short line (refusal, cut, healed) and the world time it goes */
	FText NoticeText;
	bool bNoticeWarning = false;
	double NoticeUntil = -1.0;

	/** Owner: the last check (refusal and the crosshair on something to use) */
	EBandageRefusal LocalRefusal = EBandageRefusal::NotInHand;
	bool bInteractableInView = false;
	double NextLocalCheck = 0.0;

	bool bInputBound = false;
	bool bArmsDriven = false;
	bool bMeshesSet = false;
	bool bWarnedNoRules = false;
};
