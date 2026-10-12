// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "InputCoreTypes.h"
#include "GalleonCrewComponent.generated.h"

class AGalleonActor;
class APlayerController;
class UEnhancedInputComponent;
class UGalleonRules;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

/**
 *  A player's keys and requests for the departure of the galleon (cahier §24.2 step 7, T-268 R-01, T-286), on its
 *  player controller: added by the server (UGalleonDepartureComponent) and replicated to its owner only.
 *  - Owner: at the wheel, alive and hands free, the wheel's key (HelmKeys: the interaction key F) is taken over by a
 *    mapping above the character's: held, it raises the anchor; pressed during a vote or a notice, it cancels it. The
 *    vote key (VoteKeys: O) votes for the departure while a vote is open and the HUD offers it.
 *  - Server: the requests are reliable (a lost release would raise the anchor) and rate limited
 *    (DepartureRequestMinInterval; a release is never dropped); UGalleonDepartureComponent checks them again: alive,
 *    at the wheel, the phase.
 *  The actions and the mappings are built at run time from DA_GalleonRules: no input asset.
 */
UCLASS(ClassGroup=(Galleon))
class MONKE_ISLAND_API UGalleonCrewComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UGalleonCrewComponent();

	/** Server: the crew component of this controller, added and replicated when missing */
	static UGalleonCrewComponent* Ensure(APlayerController* Controller);

	/** The crew component of the local player of this world, or nullptr */
	static UGalleonCrewComponent* GetLocal(const UObject* WorldContextObject);

	/** Owner: the wheel's key went down (true) or up (false) */
	void RequestHelmKey(bool bPressed);

	/** Owner: votes for the departure under way */
	void RequestVote();

	/** Owner, tests (monke.Galleon.RaiseAnchor): the wheel's key down, then up a little after HelmHoldSeconds */
	void DebugHoldHelm();

	/** Owner: the player stands at the wheel, alive, hands free: the wheel's key is taken over (the HUD tells what it does) */
	bool IsAtUsableHelm() const { return bHelmContextActive; }

	/** Owner: a vote is open and this living player has not voted: the vote key is offered */
	bool IsVoteOffered() const { return bVoteOffered; }

	/** Owner: name of the first wheel's key ("F"), empty before the keys are built */
	const FText& GetHelmKeyName() const { return HelmKeyName; }

	/** Owner: name of the first vote key ("O"), empty before the keys are built */
	const FText& GetVoteKeyName() const { return VoteKeyName; }

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** The wheel's key down or up */
	UFUNCTION(Server, Reliable)
	void ServerHelmKey(bool bPressed);

	/** A vote for the departure */
	UFUNCTION(Server, Reliable)
	void ServerVote();

private:

	/** Owner, every LocalUpdateSeconds: builds the keys once, turns the wheel's mapping on and off, offers the vote */
	void LocalUpdate();

	/** Owner: actions, mappings and their bindings on an input component of the controller's own stack */
	bool EnsureInput(APlayerController* Controller, const UGalleonRules& Rules);

	/** Owner: adds or removes the wheel's mapping */
	void SetHelmContextActive(APlayerController* Controller, bool bActive, int32 Priority);

	/** Owner: the wheel's key */
	void HandleHelmPressed(const FInputActionValue& Value);
	void HandleHelmReleased(const FInputActionValue& Value);

	/** Owner: the vote key */
	void HandleVotePressed(const FInputActionValue& Value);

	/** Owner, tests: the end of DebugHoldHelm */
	void DebugReleaseHelm();

	/** The galleon of this world, looked for at most once a second while missing */
	AGalleonActor* FindGalleon();

	/** The controller that owns this component */
	APlayerController* GetController() const;

	/** Input component pushed on the controller's own stack: the keys work with any pawn */
	UPROPERTY(Transient)
	TObjectPtr<UEnhancedInputComponent> CrewInput;

	/** The wheel's key (consumes it: the interaction does not see it at the wheel) */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> HelmAction;

	/** The vote key */
	UPROPERTY(Transient)
	TObjectPtr<UInputAction> VoteAction;

	/** The wheel's mapping, on only at the wheel */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> HelmContext;

	/** The vote's mapping, always on (its key does nothing without a vote) */
	UPROPERTY(Transient)
	TObjectPtr<UInputMappingContext> VoteContext;

	/** The galleon of this world */
	TWeakObjectPtr<AGalleonActor> Galleon;

	/** Real time of the next search of the galleon (s) */
	double NextGalleonSearch = 0.0;

	/** Owner: the periodic update */
	FTimerHandle UpdateTimer;

	/** Owner, tests: the release of DebugHoldHelm */
	FTimerHandle DebugReleaseTimer;

	/** Owner: the actions and the mappings exist */
	bool bInputBuilt = false;

	/** Owner: the wheel's mapping is on */
	bool bHelmContextActive = false;

	/** Owner: the wheel's key is down (sent to the server) */
	bool bHelmKeyHeld = false;

	/** Owner: the wheel's key was already down when the player reached the wheel: ignored until released */
	bool bIgnoreHeldHelmKey = false;

	/** Owner: keys of the wheel's mapping */
	TArray<FKey> HelmKeys;

	/** Owner: the vote key is offered */
	bool bVoteOffered = false;

	/** Owner: names of the keys for the HUD */
	FText HelmKeyName;
	FText VoteKeyName;

	/** Server: last accepted press of the wheel's key and vote (real time, rate limit) */
	double LastPressTime = -1.0e9;
	double LastVoteTime = -1.0e9;
};
