// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionTypes.h"
#include "ExpeditionGameState.generated.h"

class UExpeditionRules;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExpeditionPhaseChangedSignature, EExpeditionPhase, NewPhase, EExpeditionPhase, PreviousPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FExpeditionFloodDeadlineChangedSignature, double, NewFloodStartTime, double, PreviousFloodStartTime);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExpeditionSubmersionEndedSignature);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FExpeditionTeamWipedSignature);

/**
 *  Shared state of an expedition (cahier §6.1, §21: BP_ExpeditionGameState).
 *  Server authoritative: the server starts the clock, grants zone bonuses and moves the phases with timers
 *  (no tick); every machine reads the replicated clock and computes the water rise from the server time.
 *  Rules live in UExpeditionRules (DA_ExpeditionRules); the pure computation lives in ExpeditionClock.
 *  Game over (T-104): with bPermadeath nobody respawns; once every player is dead the server waits
 *  GameOverRestartDelay then travels back to the restart map (L_Boot: new draw, start in the boat).
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

	/** Rules of the expedition (DA_ExpeditionRules) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition")
	TObjectPtr<UExpeditionRules> Rules;

	/** Starts the clock on BeginPlay (prototype); off when a game mode starts the expedition itself */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition")
	bool bStartOnBeginPlay = true;

	/** Server: starts the expedition clock now; false when already started or without rules */
	UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Expedition")
	bool StartExpedition();

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

	/** Whether every player died: the game is over and the expedition restarts */
	UFUNCTION(BlueprintPure, Category="Expedition")
	bool IsTeamWiped() const { return TeamWipedTime >= 0.0; }

	/** Server time of the return to the restart map (s), negative while somebody lives */
	UFUNCTION(BlueprintPure, Category="Expedition")
	double GetRestartTime() const;

	/** Whether a team is wiped: somebody plays and nobody is alive */
	static bool IsWipe(int32 PlayersWithPawn, int32 PlayersAlive) { return PlayersWithPawn > 0 && PlayersAlive <= 0; }

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

	/** Fires on every machine when the last player dies (game over) */
	UPROPERTY(BlueprintAssignable, Category="Expedition")
	FExpeditionTeamWipedSignature OnTeamWiped;

protected:

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Client: broadcasts the deadline change */
	UFUNCTION()
	void OnRep_ClockState(const FExpeditionClockState& PreviousClockState);

	/** Client: broadcasts the phase change, and the end of the submersion */
	UFUNCTION()
	void OnRep_Phase(EExpeditionPhase PreviousPhase);

	/** Client: broadcasts the game over */
	UFUNCTION()
	void OnRep_TeamWipedTime();

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

	/** Server: declares the game over when nobody is alive */
	void CheckTeamWiped();

	/** Server: travels to the restart map */
	void RestartFromBoot();

	/** Server time the last player died (s), negative while somebody lives */
	UPROPERTY(ReplicatedUsing=OnRep_TeamWipedTime)
	double TeamWipedTime = -1.0;

	/** Return to the restart map after the game over */
	FTimerHandle RestartTimer;

	/** Characters spawned during the expedition */
	FDelegateHandle ActorSpawnedHandle;
};
