// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionTypes.h"
#include "ExpeditionGameState.generated.h"

class AController;
class AGameModeBase;
class APlayerState;
class UExpeditionRules;
class UGalleonDepartureComponent;
class URunStateComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExpeditionPhaseChangedSignature, EExpeditionPhase, NewPhase, EExpeditionPhase, PreviousPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExpeditionFloodDeadlineChangedSignature, double, NewFloodStartTime, double, PreviousFloodStartTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExpeditionSubmersionEndedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExpeditionTeamWipedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExpeditionDaySurvivedSignature);

/**
 *  Shared state of an expedition (cahier §6.1, §21: BP_ExpeditionGameState).
 *  Server authoritative: the server starts the clock, grants zone bonuses and moves the phases with timers
 *  (no tick); every machine reads the replicated clock and computes the water rise from the server time.
 *  Rules live in UExpeditionRules (DA_ExpeditionRules); the pure computation lives in ExpeditionClock.
 *  Lost day (T-104, T-173, ADR-007 §14): with bPermadeath nobody respawns; once every player is dead the day is lost,
 *  not the game: the run counts it (nothing brought back, the day spent) and the server travels to the next island
 *  after GameOverRestartDelay ("JOUR PERDU"), plus the greed question's time; only a contract failed on its milestone
 *  day ends the campaign (IsExecution, no travel). The pure plan: ExpeditionDayEnd::Plan.
 *  End of the day (ADR-006, T-142): at the end of the submersion the players who are not aboard a galleon die;
 *  with survivors the server waits DayEndTravelDelay then travels to the next island. A player still loading
 *  (no pawn yet) counts as alive, and a player who leaves is counted out on the next frame.
 *  Departure from the galleon's wheel (cahier §24.2 step 7, T-286): UGalleonDepartureComponent (hold, vote, notice),
 *  then SailFromHelm ends the day as the end of the deluge does.
 *  Start of the clock (network review T-268 R-02, T-292): once every player expected from the previous map has a pawn
 *  and has loaded this one, at most CrewLoadTimeoutSeconds after BeginPlay; at once in solo.
 */
UCLASS()
class MONKE_ISLAND_API AExpeditionGameState : public AGameStateBase
{
	GENERATED_BODY()

public:

	AExpeditionGameState();

	/** Returns the expedition game state of the given world, or nullptr */
	static AExpeditionGameState* Get(const UObject* WorldContextObject);

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	/** Run of days and contracts seen by every machine (ADR-007), filled by URunSubsystem on the server */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Run")
	TObjectPtr<URunStateComponent> RunState;

	/** Departure from the galleon's wheel: hold, vote of the crew, notice (T-286), replicated with the game state */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Galleon")
	TObjectPtr<UGalleonDepartureComponent> Departure;

	/** Rules of the expedition (DA_ExpeditionRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition")
	TObjectPtr<UExpeditionRules> Rules;

	/** Starts the clock at BeginPlay without waiting for anybody (prototype maps, tests). Off since the network review
	 *  T-268 R-02 (T-292): a client still loading the island lost the time of its load */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition")
	bool bStartOnBeginPlay = false;

	/** Otherwise the server starts the clock once every expected player (the players of the map left by the travel,
	 *  UExpeditionIslandSubsystem::TakeTravelPlayerCount) has a pawn and has loaded this map, and no client is still
	 *  joining; at most CrewLoadTimeoutSeconds (DA_ExpeditionRules) after BeginPlay, at once in solo. Both off: a game
	 *  mode calls StartExpedition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition")
	bool bStartWhenCrewLoaded = true;

	/** Server: starts the expedition clock now; false when already started or without rules */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Expedition")
	bool StartExpedition();

	/**
	 *  Pure (T-292): whether the clock may start. In solo at once; otherwise once Loaded players (a pawn, this map
	 *  loaded) reach Expected with nobody Loading (logged in without pawn or map, or connected and not logged in yet);
	 *  nobody announced by a travel (Expected 0: a map opened directly, PIE), after JoinGrace seconds for clients still
	 *  joining; whatever happens, once Waited reaches Timeout.
	 */
	static bool ShouldStartClock(bool bSolo, int32 Loaded, int32 Loading, int32 Expected, double Waited, double JoinGrace, double Timeout);

	/** Whether the clock waits for players still loading the island (T-292) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	bool IsAwaitingCrew() const { return CrewWaitEndTime >= 0.0; }

	/** Players loaded, and expected, while the clock waits for the crew */
	int32 GetCrewLoaded() const { return CrewLoaded; }

	/** See GetCrewLoaded */
	int32 GetCrewExpected() const { return CrewExpected; }

	/** Server time the clock starts at the latest while it waits for the crew (s), negative otherwise */
	double GetCrewWaitEndTime() const { return CrewWaitEndTime; }

	/** Departure from the galleon's wheel (T-286) */
	UGalleonDepartureComponent* GetDeparture() const { return Departure; }

	/**
	 *  Server: grants the bonus of a freshly pacified zone, once per (ExpeditionId, ZoneId), only when the current
	 *  server time strictly precedes the flood deadline (read here, never trusted from the caller).
	 *  Called by the zone after its server validation. NoAuthority on a client.
	 */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Expedition")
	EZoneBonusResult AddZoneBonus(FName ZoneId);

	/**
	 *  Water height above the low water at ServerTime (cm): 0 before the flood, then rises over the submersion.
	 *  On a client it stays 0 until the replicated phase reaches Submerging, so a deadline update arriving late
	 *  never makes the water rise then fall back.
	 */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetWaterRise(double ServerTime) const;

	/** Water height above the low water now (cm) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetCurrentWaterRise() const;

	/** Time left before the submersion starts (s) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetTimeBeforeSubmersion() const;

	/** Current phase */
	UFUNCTION(BlueprintPure, Category="Expedition")
	EExpeditionPhase GetPhase() const { return Phase; }

	/** Why the expedition resolved, None while it runs */
	UFUNCTION(BlueprintPure, Category="Expedition")
	EExpeditionResolution GetResolution() const { return Resolution; }

	/** Identifier of this expedition (valid once started) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	FGuid GetExpeditionId() const { return ExpeditionId; }

	/** Fragment played (FRG_<Ile>_<NN>, ADR-004), None outside a fragment map; clients load its definition by this id */
	UFUNCTION(BlueprintPure, Category="Expedition")
	FName GetFragmentId() const { return FragmentId; }

	/** Server time of the start of the expedition (s) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetExpeditionStartTime() const { return ClockState.StartTime; }

	/** Server time the submersion starts (s) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetFloodStartTime() const { return ClockState.FloodStartTime; }

	/** Server time the submersion ends (s) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetSubmersionEndTime() const;

	/** Number of zones that gave their bonus */
	UFUNCTION(BlueprintPure, Category="Expedition")
	int32 GetBonusedZoneCount() const { return ClockState.BonusedZoneIds.Num(); }

	/** Number of zones that can still give a bonus; -1 when the rules set no limit */
	UFUNCTION(BlueprintPure, Category="Expedition")
	int32 GetRemainingZoneCount() const;

	/** Whether this zone already gave its bonus */
	UFUNCTION(BlueprintPure, Category="Expedition")
	bool HasZoneBonus(FName ZoneId) const { return ClockState.BonusedZoneIds.Contains(ZoneId); }

	/** Replicated clock state */
	const FExpeditionClockState& GetClockState() const { return ClockState; }

	/** Whether every player died: the day is lost ("JOUR PERDU"), the next island follows unless the campaign is lost */
	UFUNCTION(BlueprintPure, Category="Expedition")
	bool IsTeamWiped() const { return TeamWipedTime >= 0.0; }

	/** Server time of the travel to the next island after a lost day (s), negative while somebody lives or when the
	 *  campaign is lost */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetRestartTime() const;

	/** Whether the day ended with survivors: the crew travels to the next island */
	UFUNCTION(BlueprintPure, Category="Expedition")
	bool IsDaySurvived() const { return DayEndedTime >= 0.0; }

	/** Whether the expedition is over on this map, lost or survived: a travel follows */
	UFUNCTION(BlueprintPure, Category="Expedition")
	bool IsExpeditionOver() const { return IsTeamWiped() || IsDaySurvived(); }

	/** Whether the day ended on a failed contract with the execution rule: no travel follows, the campaign is lost; the
	 *  survivors are executed (AExecutionScene), a crew wiped that day has no scene (T-173) */
	UFUNCTION(BlueprintPure, Category="Expedition")
	bool IsExecution() const { return bExecution; }

	/** Tests (monke.Execution.Start): the day ends now with the execution of the crew, whatever the run */
	void DebugExecute();

	/** True when a departure may be asked for at the galleon's wheel now: the clock runs (the crew loaded, T-292) and the
	 *  day is not over (cahier §24.2 step 7, T-286) */
	bool CanDepartFromHelm() const;

	/**
	 *  Server, at the end of the notice of a departure (UGalleonDepartureComponent): the day ends as at the end of the
	 *  deluge: whoever is aboard the galleon sails with the cargo aboard, the others are left behind and die (cahier
	 *  §24.2 step 7). False when it cannot (no rules, the day already over).
	 */
	bool SailFromHelm();

	/** Server time of the travel that follows the end of the expedition (s), negative while it runs */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetNextTravelTime() const;

	/** Server: the travel to the next island leaves within Seconds (the greed question was answered, ADR-007; the host
	 *  sends the galleon on after a lost day, T-173); nothing when no travel is due */
	void HurryTravel(float Seconds);

	/** Server, tests (console monke.EndDeluge, not in Shipping): the deluge ends now, as at the end of the submersion */
	void DebugEndDeluge() { ResolveDelugeEnd(); }

	/**
	 *  Whether a team is wiped: somebody died, nobody is alive and nobody is still loading in.
	 *  Pending players (logged in, no pawn yet) keep the game going: the host dying during a client's load is no wipe.
	 */
	static bool IsWipe(int32 Dead, int32 Alive, int32 Pending) { return Dead > 0 && Alive <= 0 && Pending <= 0; }

	/**
	 *  Fires on every machine when the phase changes. The server sees every phase in order; a client may skip one
	 *  (e.g. Exploring -> Resolved after a hitch), so listeners test Phase >= Submerging rather than an exact phase.
	 */
	UPROPERTY(BlueprintAssignable, Category="Expedition")
	FExpeditionPhaseChangedSignature OnPhaseChanged;

	/** Fires on every machine when the flood deadline moves (start, zone bonus) */
	UPROPERTY(BlueprintAssignable, Category="Expedition")
	FExpeditionFloodDeadlineChangedSignature OnFloodDeadlineChanged;

	/** Fires on every machine when the final rise of the water ends (extraction plugs in here later) */
	UPROPERTY(BlueprintAssignable, Category="Expedition")
	FExpeditionSubmersionEndedSignature OnSubmersionEnded;

	/** Fires on every machine when the last player dies (the day is lost) */
	UPROPERTY(BlueprintAssignable, Category="Expedition")
	FExpeditionTeamWipedSignature OnTeamWiped;

	/** Fires on every machine when the day ends with survivors (the travel to the next island follows) */
	UPROPERTY(BlueprintAssignable, Category="Expedition")
	FExpeditionDaySurvivedSignature OnDaySurvived;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Client: broadcasts the deadline change */
	UFUNCTION()
	void OnRep_ClockState(const FExpeditionClockState& PreviousClockState);

	/** Client: broadcasts the phase change, and the end of the submersion */
	UFUNCTION()
	void OnRep_Phase(EExpeditionPhase PreviousPhase);

	/** Client: broadcasts the lost day */
	UFUNCTION()
	void OnRep_TeamWipedTime();

	/** Client: broadcasts the end of the day */
	UFUNCTION()
	void OnRep_DayEndedTime();

	/** Server: a watched player died */
	UFUNCTION()
	void HandlePlayerDeath(AActor* Killer);

private:

	/** Server: moves the phase to what the clock says now, then schedules the next transition */
	void RefreshPhase();

	/** Server: applies one phase change and broadcasts it */
	void SetPhase(EExpeditionPhase NewPhase);

	/** Clock values of the rules, zero when there are no rules */
	FExpeditionClockParams GetParams() const;

	/** Identifier of the expedition, keys the zone bonuses and the future result */
	UPROPERTY(Replicated)
	FGuid ExpeditionId;

	/** Fragment played, chosen by the server (UExpeditionIslandSubsystem) and set at BeginPlay */
	UPROPERTY(Replicated)
	FName FragmentId;

	/** Start, flood deadline and bonused zones */
	UPROPERTY(ReplicatedUsing=OnRep_ClockState)
	FExpeditionClockState ClockState;

	/** Current phase */
	UPROPERTY(ReplicatedUsing=OnRep_Phase)
	EExpeditionPhase Phase = EExpeditionPhase::Preparing;

	/** Why the expedition resolved (replicated with the phase, read in OnRep_Phase) */
	UPROPERTY(Replicated)
	EExpeditionResolution Resolution = EExpeditionResolution::None;

	/** Next phase transition on the server */
	FTimerHandle PhaseTimer;

	/** Server: turns the respawn of a player character off (bPermadeath) and listens to its death */
	void WatchPlayerCharacter(AActor* Actor);

	/** Server: nobody alive: the day is lost (the campaign too on a failed milestone day), the run counts it */
	void CheckTeamWiped();

	/** Server: remembers the players whose character is dead (the body may disappear before the end) */
	void RecordDeadPlayers();

	/** Server: players dead, alive, and logged in without a character yet */
	void CountPlayers(int32& OutDead, int32& OutAlive, int32& OutPending);

	/** Server: a player left; the team is counted again on the next frame, once its player state is gone */
	void HandleLogout(AGameModeBase* GameMode, AController* Exiting);

	/** Server, end of the submersion (ADR-006): kills the players who are not aboard a galleon, then ends the day */
	void ResolveDelugeEnd();

	/** Server: ends the day for the survivors and schedules the travel to the next island */
	void EndDay();

	/** Server: travels to the restart map */
	void RestartFromBoot();

	/** Server (ADR-007): value of the cargo aboard the galleons of this map (crowns), what the day brought back */
	int32 ComputeDayValue() const;

	/** Server time the last player died (s), negative while somebody lives */
	UPROPERTY(ReplicatedUsing=OnRep_TeamWipedTime)
	double TeamWipedTime = -1.0;

	/** Server time the day ended with survivors (s), negative before */
	UPROPERTY(ReplicatedUsing=OnRep_DayEndedTime)
	double DayEndedTime = -1.0;

	/** Wait between the end of the day and the travel (s): DayEndTravelDelay, longer while the crew chooses (greed) */
	UPROPERTY(Replicated)
	float DayEndTravelWait = -1.0f;

	/** The day ended with the execution of the crew */
	UPROPERTY(Replicated)
	bool bExecution = false;

	/** Server time the clock starts at the latest while it waits for the crew (s), negative otherwise (T-292) */
	UPROPERTY(Replicated)
	double CrewWaitEndTime = -1.0;

	/** Players loaded while the clock waits for the crew */
	UPROPERTY(Replicated)
	uint8 CrewLoaded = 0;

	/** Players expected while the clock waits for the crew */
	UPROPERTY(Replicated)
	uint8 CrewExpected = 0;

	/** Server: players logged in the map left by the travel here, 0 when unknown (a map opened directly) */
	int32 ExpectedCrew = 0;

	/** Server: world time the wait for the crew began (s) */
	double CrewWaitStartTime = 0.0;

	/** Server: the checks of the wait for the crew */
	FTimerHandle CrewLoadTimer;

	/** Server: starts the clock once the crew loaded the island, or at the end of the wait (T-292) */
	void TryStartWhenCrewLoaded();

	/** Server: players with a pawn in this map once loaded, and players on their way (logged in without pawn or with
	 *  the map not loaded, connected but not logged in yet) */
	void CountCrewLoad(int32& OutLoaded, int32& OutLoading) const;

	/** Server: the execution instead of the travel */
	void BeginExecution();

	/** Players who died in this expedition */
	TSet<TWeakObjectPtr<const APlayerState>> DeadPlayers;

	/** Travel to the next island after a day survived or lost */
	FTimerHandle RestartTimer;

	/** End of the day in a map without galleon, once the sea reached its highest level */
	FTimerHandle DayEndTimer;

	/** Players leaving the game */
	FDelegateHandle LogoutHandle;

	/** Characters spawned during the expedition */
	FDelegateHandle ActorSpawnedHandle;

	/** Server: the boat start resolved its arrival; FragmentId becomes the island really played (T-141) */
	void HandleArrivalResolved(FName ArrivalFragmentId, const FTransform& BoatTransform);

	/** Arrival event of the boat start (UBoatStartSubsystem::OnArrivalResolved) */
	FDelegateHandle ArrivalResolvedHandle;
};
