// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Run/RunTypes.h"
#include "RunContract.generated.h"

/**
 *  A destination of the run (ADR-007): one contract is played there, its quota and its danger grow with the zone.
 *  Lagons extérieurs, Village abandonné, Marae intérieur, Atoll frappé par une tempête, Île interdite by default.
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunZone
{
	GENERATED_BODY()

	/** Stable identifier (saves, logs) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run")
	FName ZoneId;

	/** Name shown on the arrival card and in the HUD */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run")
	FText DisplayName;

	/** Value the crew must bring back over the days of the contract (crowns) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta=(ClampMin="0"))
	int32 Quota = 1800;

	/** Days the contract of this zone lasts; 0 = DaysPerContract of the rules (the campaign's last stage: 4 days) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta=(ClampMin="0"))
	int32 Days = 0;

	/** Multiplier of the hostile spawns (on top of day and night) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta=(ClampMin="0"))
	float SpawnScale = 1.0f;

	/** Multiplier of the value of the loot found there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta=(ClampMin="0"))
	float LootValueScale = 1.0f;

	/** Multiplier of the time before the deluge: below 1 the galleon leaves earlier */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Run", meta=(ClampMin="0.1"))
	float FloodDelayScale = 1.0f;
};

/** What the end of a day leads to (ADR-007 §9, §14, §16) */
UENUM(BlueprintType)
enum class ERunDayVerdict : uint8
{
	/** Nothing was counted (no run, run over, a choice is pending) */
	Invalid,
	/** Quota not met yet, days left: next day in the same zone */
	NextDay,
	/** Quota met with days left: the crew chooses to leave the zone or to try one more day (greed) */
	QuotaMetChoose,
	/** Quota met: contract done, the galleon sails to the next zone */
	ContractComplete,
	/** Last day without the quota: money lost, back to an easier zone, the upgrades are kept */
	ContractFailed,
	/** The contract of the last zone is done: the run is won */
	Victory,
	/** Last day without the quota, the execution rule on (owner 2026-10-07: the crew is put on a plank and shot by the
	 *  French soldiers): the run is lost */
	Executed
};

/** Values of DA_RunRules the contract calculator needs (plain copy, no UObject) */
struct MONKE_ISLAND_API FRunContractRules
{
	/** Destinations, in order */
	TArray<FRunZone> Zones;

	/** Days a contract lasts */
	int32 DaysPerContract = 3;

	/** Loot value and danger of a day played after the quota was met */
	float GreedLootScale = 1.5f;
	float GreedDangerScale = 1.4f;

	/** Part of the excess over the quota paid into the treasury when a contract is done */
	float ExcessPayoutFraction = 1.0f;

	/** Part of the treasury lost when a contract fails */
	float FailedTreasuryLoss = 0.5f;

	/** Zones the galleon goes back after a failed contract */
	int32 FailedZoneDrop = 1;

	/** A failed contract ends the run: the crew is executed (ERunDayVerdict::Executed, ERunOutcome::Defeat) instead of
	 *  losing money and going back a zone */
	bool bExecuteOnFailedContract = false;

	/** Medical fee per player dead during a day (crowns) */
	int32 MedicalFee = 50;
};

/** State of the run kept by the server between the islands (URunSubsystem) */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunContractState
{
	GENERATED_BODY()

	/** A run was started */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bActive = false;

	/** Zone of the current contract, from 0 */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 ZoneIndex = 0;

	/** Contracts started in this run, from 1 */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 ContractNumber = 1;

	/** Day of the contract being played, from 1 */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 DayInContract = 1;

	/** Days ended in the whole run */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 DaysPlayed = 0;

	/** Quota of the contract (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Quota = 0;

	/** Value brought back during the contract (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 ContractValue = 0;

	/** Money of the crew (crowns), never below zero */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Treasury = 0;

	/** Value brought back in the whole run (crowns), for the record */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 TotalValue = 0;

	/** The quota of the contract is met */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bQuotaMet = false;

	/** The day played is an extra day after the quota (more loot, more danger) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bGreedDay = false;

	/** The crew must choose: leave the zone or one more day */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bAwaitingChoice = false;

	/** Contracts failed in the run */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 FailedContracts = 0;

	/** In progress or won */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	ERunOutcome Outcome = ERunOutcome::InProgress;
};

/** Summary of a day (ADR-007 §7: "JOUR 1 TERMINÉ") */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunDayReport
{
	GENERATED_BODY()

	/** What follows */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	ERunDayVerdict Verdict = ERunDayVerdict::Invalid;

	/** Zone and day that ended */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 ZoneIndex = 0;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Day = 1;

	/** Value brought back this day (crowns), 0 when the crew was wiped (the loot stays on the island) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 DayValue = 0;

	/** Players dead this day */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Deaths = 0;

	/** Every player died */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	bool bWiped = false;

	/** Medical fees taken from the treasury (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 MedicalFees = 0;

	/** Contract value and quota after the day (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 ContractValue = 0;

	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Quota = 0;

	/** Paid into the treasury when the contract is done (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Payout = 0;

	/** Lost from the treasury when the contract failed (crowns) */
	UPROPERTY(BlueprintReadOnly, Category="Run")
	int32 Penalty = 0;
};

/**
 *  Pure rules of the days and contracts (ADR-007), with no UObject and no world:
 *   - a contract lasts DaysPerContract days in one zone; the value brought back each day adds up;
 *   - quota met with days left: the crew chooses to leave (contract done) or to stay (greed day: more loot, more danger);
 *   - quota met on the last day: contract done, the excess is paid into the treasury, next zone;
 *   - last day without the quota: contract failed, part of the treasury lost, back FailedZoneDrop zones, new contract;
 *   - a wiped crew brings nothing back that day, and the day is spent; every death costs a medical fee;
 *   - the contract of the last zone done = victory. Nothing ever erases the upgrades.
 */
struct MONKE_ISLAND_API FRunContractCalculator
{
	/** Quota of a zone (crowns), the last zone for an index past the end */
	static int32 QuotaOfZone(const FRunContractRules& Rules, int32 ZoneIndex);

	/** Days the contract of a zone lasts (its own Days, else DaysPerContract), at least 1 */
	static int32 DaysOfZone(const FRunContractRules& Rules, int32 ZoneIndex);

	/** A fresh run: first zone, day 1, empty treasury */
	static FRunContractState StartRun(const FRunContractRules& Rules);

	/** Test tool (monke.Run.Day): the run on a day of the campaign (from 1) as if every earlier contract had been met on
	 *  its last day: the zone whose days hold that day, its day in the contract, nothing brought back yet; the treasury
	 *  is kept. Past the last zone's days: its last day, the campaign day still counted */
	static void JumpToDay(const FRunContractRules& Rules, FRunContractState& State, int32 CampaignDay);

	/** Ends the day being played; Invalid (and nothing changes) without a run in progress or while a choice is pending */
	static FRunDayReport EndDay(const FRunContractRules& Rules, FRunContractState& State, int32 DayValue, int32 Deaths, bool bWiped);

	/** Answers the greed question: stay one more day, or leave (contract done); Invalid when nothing was asked */
	static ERunDayVerdict Choose(const FRunContractRules& Rules, FRunContractState& State, bool bStay, int32* OutPayout = nullptr);

	/** Spawn multiplier of the day played (zone, greed) */
	static float DangerScale(const FRunContractRules& Rules, const FRunContractState& State);

	/** Loot value multiplier of the day played (zone, greed) */
	static float LootValueScale(const FRunContractRules& Rules, const FRunContractState& State);

	/** Multiplier of the time before the deluge in the current zone */
	static float FloodDelayScale(const FRunContractRules& Rules, const FRunContractState& State);

private:

	/** Pays the excess, then moves to the next zone, or wins after the last one */
	static ERunDayVerdict CompleteContract(const FRunContractRules& Rules, FRunContractState& State, int32& OutPayout);

	/** Opens the contract of State.ZoneIndex at day 1 */
	static void OpenContract(const FRunContractRules& Rules, FRunContractState& State);
};
