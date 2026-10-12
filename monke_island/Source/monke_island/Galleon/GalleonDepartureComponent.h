// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Engine/TimerHandle.h"
#include "Galleon/GalleonDeparture.h"
#include "GalleonDepartureComponent.generated.h"

class AExpeditionGameState;
class AGalleonActor;
class AGameModeBase;
class APawn;
class APlayerController;
class APlayerState;
class UGalleonRules;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FGalleonDepartureChangedSignature);

/**
 *  Departure of the galleon from its wheel (cahier §24.2 step 7, §6.5, §24.5; network review T-268 R-01, T-286), on
 *  the expedition game state: decided by the server, replicated to every machine. Values in DA_GalleonRules.
 *  - A living player aboard holds the wheel's key HelmHoldSeconds (UGalleonCrewComponent sends the key down and up).
 *  - Every living player aboard: the galleon sails after DepartureAllAboardNoticeSeconds.
 *  - Otherwise a vote of the living opens (the holder's vote counted; every living player votes with the vote key):
 *    strictly more than DepartureMajorityFraction of them, somebody aboard, then DepartureVoteNoticeSeconds of notice
 *    for the whole crew (bell, foghorn, message, countdown). The wheel's key cancels a vote or a notice.
 *  - Nobody alive aboard: void; no majority within DepartureVoteSeconds: lapsed; then no new vote for
 *    DepartureVoteCooldownSeconds.
 *  - At the end of the notice whoever is aboard sails with the cargo aboard; whoever is ashore is left behind and dies
 *    (AExpeditionGameState::SailFromHelm, as at the end of the deluge, which still sails on its own).
 *  The pure rules: GalleonDeparture (monke_island.Galleon.Departure). Every request is checked again here: alive, at
 *  the wheel, the phase of the expedition. The server also gives each player controller its crew component.
 */
UCLASS(ClassGroup=(Galleon))
class MONKE_ISLAND_API UGalleonDepartureComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UGalleonDepartureComponent();

	/** The departure of the expedition game state of this world, or nullptr */
	static UGalleonDepartureComponent* Get(const UObject* WorldContextObject);

	/** Whether this pawn is a living player's: a health component, not dead (a pawn still loading or without health
	 *  counts out) */
	static bool IsLivingPawn(const APawn* Pawn);

	/** Replicated state: stage, voters, counts, times */
	const FGalleonDepartureState& GetState() const { return State; }

	/** Replicated hold of the wheel */
	const FGalleonHelmHold& GetHold() const { return Hold; }

	/** Stage under way */
	UFUNCTION(BlueprintPure, Category="Galleon|Departure")
	EGalleonDepartureStage GetStage() const { return State.Stage; }

	/** Whether this player voted for the departure under way */
	UFUNCTION(BlueprintPure, Category="Galleon|Departure")
	bool HasVoted(const APlayerState* Player) const;

	/** Whether the wheel's key does something now: raise the anchor (the clock runs, the day is not over) or cancel a
	 *  vote or a notice */
	UFUNCTION(BlueprintPure, Category="Galleon|Departure")
	bool IsHelmUsable() const;

	/** Rules of the departure: those of the galleon of this world, nullptr without one */
	const UGalleonRules* GetRules() const;

	/** The galleon of this world (cached), nullptr without one */
	AGalleonActor* GetGalleon() const;

	/** Server: Player pressed (true) or released (false) the wheel's key (UGalleonCrewComponent). Pressed at the wheel,
	 *  alive: begins the hold, or cancels the vote or the notice under way; released: the hold ends */
	void HandleHelmKey(APlayerController* Player, bool bPressed);

	/** Server: Player votes for the departure (UGalleonCrewComponent); false when refused (no vote, dead, voted) */
	bool HandleVote(APlayerController* Player);

	/** Server, tests (monke.EndDayAtHelm): the notice of a vote starts now, whoever stands where */
	void DebugStartNotice();

	/** The state in one line (logs, monke.Galleon.Status) */
	FString Describe() const;

	/** Fires on every machine when the stage, the votes, the counts or the hold change */
	UPROPERTY(BlueprintAssignable, Category="Galleon|Departure")
	FGalleonDepartureChangedSignature OnDepartureChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Every machine: the sounds of a new stage, the notice's ticks, the listeners */
	UFUNCTION()
	void OnRep_State(const FGalleonDepartureState& PreviousState);

	/** Every machine: the listeners */
	UFUNCTION()
	void OnRep_Hold();

	/** Server: the day ended otherwise (end of the submersion, the crew wiped): nothing more under way */
	UFUNCTION()
	void HandleDayOver();

private:

	/** Server: the hold of the wheel ran its time: the short notice, a vote, or nothing */
	void CompleteHold();

	/** Server: the hold ends without raising the anchor */
	void ClearHold(const TCHAR* Why);

	/** Server: opens a vote, the vote of By counted (the state is committed by the caller) */
	void EnterVote(APlayerState* By);

	/** Server: the notice starts (committed by the caller) */
	void EnterNotice(bool bAllAboard);

	/** Server: the vote or the notice ends (committed by the caller); a failure delays the next vote */
	void LeaveStage(EGalleonDepartureEnd Why, const APlayerState* By = nullptr);

	/** Server: the notice ran out: the galleon sails with whoever is aboard */
	void Sail();

	/** Server, every DepartureCheckSeconds while a hold, a vote or a notice runs: the holder still at the wheel, the
	 *  majority, the time, somebody aboard */
	void Check();

	/** Server: the checks run while something is under way, not otherwise */
	void UpdateCheckTimer();

	/** Server: counts the living crew, drops the votes of the dead and the gone, updates the replicated counts */
	FGalleonCrewCount CountCrew();

	/** Server: the state changed from PreviousState: replicated now, presented on this machine */
	void CommitState(const FGalleonDepartureState& PreviousState);

	/** Every machine: the bell and the foghorn of a fresh stage, the ticks of a notice */
	void Present(const FGalleonDepartureState& PreviousState);

	/** Every machine, during a notice: a tick at each of its last seconds */
	void TickNotice();

	/** Server: the hold changed: replicated now, the listeners told */
	void CommitHold();

	/** Server: gives a player controller its crew component (keys and requests of the departure) */
	void HandlePostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

	/** The expedition game state that owns this component */
	AExpeditionGameState* GetExpedition() const;

	/** Rules of the galleon, the class defaults without one */
	const UGalleonRules& RulesOrDefault() const;

	/** Server time (s) */
	double GetServerTime() const;

	/** Replicated state of the departure */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FGalleonDepartureState State;

	/** Replicated hold of the wheel */
	UPROPERTY(ReplicatedUsing=OnRep_Hold)
	FGalleonHelmHold Hold;

	/** The galleon of this world */
	mutable TWeakObjectPtr<AGalleonActor> CachedGalleon;

	/** Real time of the next search of the galleon while the world has none (s) */
	mutable double NextGalleonSearch = 0.0;

	/** Server: the hold completes */
	FTimerHandle HoldTimer;

	/** Server: the notice runs out */
	FTimerHandle NoticeTimer;

	/** Server: the checks while something is under way */
	FTimerHandle CheckTimer;

	/** Every machine: the ticks of a notice */
	FTimerHandle TickTimer;

	/** Last second of the notice ticked on this machine */
	int32 LastTickSecond = 0;

	/** Server: logins */
	FDelegateHandle PostLoginHandle;
};
