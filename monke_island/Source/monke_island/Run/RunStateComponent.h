// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Run/RunContract.h"
#include "Run/RunTypes.h"
#include "RunStateComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnRunStateChanged);

/**
 *  Replicated face of the run on the game state (ADR-007): day of the contract, zone, quota, value brought back,
 *  treasury, greed question, outcome. URunSubsystem (server) writes it at the start and the end of each day; widgets
 *  only read it and listen to OnRunStateChanged. A "period" is a contract (one zone); days and periods count from 1.
 *  R-03: every reading of the run by the interface goes through this component, on the host as on a client; a client
 *  never reads URunSubsystem (it keeps no run of its own while it plays on a host).
 */
UCLASS(ClassGroup=(Run), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API URunStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	URunStateComponent();

	/** The component of the game state of this world, or null before it replicates */
	UFUNCTION(BlueprintPure, Category="Run", meta=(WorldContext="WorldContext"))
	static URunStateComponent* Get(const UObject* WorldContext);

	/** Day inside the period, 1 to GetDaysPerPeriod() */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetDayIndex() const { return DayIndex; }

	/** Period, 1 to GetPeriodCount() */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetPeriodIndex() const { return PeriodIndex; }

	/** Days in a period (DA_RunRules) */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetDaysPerPeriod() const { return DaysPerPeriod; }

	/** Periods in a run (DA_RunRules) */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetPeriodCount() const { return PeriodCount; }

	/** The quota is checked today (last day of the period) */
	UFUNCTION(BlueprintPure, Category="Run")
	bool IsQuotaDay() const { return DaysPerPeriod > 0 && DayIndex >= DaysPerPeriod; }

	/** Quota of the contract (crowns) */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetQuota() const { return Quota; }

	/** Value brought back during the contract, against the quota (crowns) */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetHoldValue() const { return HoldValue; }

	/** Common money of the crew (crowns) */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetTreasury() const { return Treasury; }

	/** In progress, defeat or victory */
	UFUNCTION(BlueprintPure, Category="Run")
	ERunOutcome GetOutcome() const { return Outcome; }

	/** Name of the zone of the contract ("Lagons extérieurs") */
	UFUNCTION(BlueprintPure, Category="Run")
	FText GetZoneName() const { return ZoneName; }

	/** The quota of the contract is met */
	UFUNCTION(BlueprintPure, Category="Run")
	bool IsQuotaMet() const { return bQuotaMet; }

	/** The day played is a greed day (more loot, more danger) */
	UFUNCTION(BlueprintPure, Category="Run")
	bool IsGreedDay() const { return bGreedDay; }

	/** The crew must choose: leave the zone or one more day */
	UFUNCTION(BlueprintPure, Category="Run")
	bool IsAwaitingChoice() const { return bAwaitingChoice; }

	/** Days ended in the whole run */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetDaysPlayed() const { return DaysPlayed; }

	/** A run of days is under way on the server (false on a map without a campaign, and before the state replicates) */
	UFUNCTION(BlueprintPure, Category="Run")
	bool IsRunActive() const { return bRunActive; }

	/** Day of the campaign being played, from 1 (days played + 1), 0 without a run: URunSubsystem::GetCampaignDay as every machine sees it */
	UFUNCTION(BlueprintPure, Category="Run")
	int32 GetCampaignDay() const { return bRunActive ? DaysPlayed + 1 : 0; }

	/** GetCampaignDay of the run of this world (0 without an expedition game state) */
	static int32 CampaignDayOf(const UObject* WorldContext);

	/** Summary of the last day that ended ("JOUR 1 TERMINÉ"), Invalid verdict before the first one */
	UFUNCTION(BlueprintPure, Category="Run")
	const FRunDayReport& GetLastDayReport() const { return LastDayReport; }

	/** Server only: copies the run of URunSubsystem and notifies */
	void SetFromContract(const FRunContractState& State, int32 InDaysPerContract, int32 InZoneCount, const FText& InZoneName,
		const FRunDayReport& InLastDayReport);

	/** Server only: sets the whole state at once and notifies */
	void SetState(int32 InDayIndex, int32 InPeriodIndex, int32 InDaysPerPeriod, int32 InPeriodCount, int32 InQuota,
		int32 InHoldValue, int32 InTreasury, ERunOutcome InOutcome);

	/** Called on every machine when any value changed */
	UPROPERTY(BlueprintAssignable, Category="Run")
	FOnRunStateChanged OnRunStateChanged;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:

	UFUNCTION()
	void OnRep_State();

	/** Day inside the period */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 DayIndex = 1;

	/** Period of the run */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 PeriodIndex = 1;

	/** Days in a period */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 DaysPerPeriod = 5;

	/** Periods in a run */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 PeriodCount = 10;

	/** Quota of the period */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 Quota = 0;

	/** Value of the hold */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 HoldValue = 0;

	/** Treasury */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 Treasury = 0;

	/** Outcome of the run */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	ERunOutcome Outcome = ERunOutcome::InProgress;

	/** Zone of the contract */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FText ZoneName;

	/** Quota met */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	bool bQuotaMet = false;

	/** Greed day */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	bool bGreedDay = false;

	/** Greed question pending */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	bool bAwaitingChoice = false;

	/** Days ended in the run */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	int32 DaysPlayed = 0;

	/** A run is under way (FRunContractState::bActive of the server) */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	bool bRunActive = false;

	/** Summary of the last day */
	UPROPERTY(ReplicatedUsing=OnRep_State)
	FRunDayReport LastDayReport;
};
