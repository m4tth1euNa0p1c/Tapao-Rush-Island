// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Run/RunCampaign.h"
#include "Run/RunContract.h"
#include "Run/RunDifficulty.h"
#include "Run/RunTypes.h"
#include "RunRules.generated.h"

class UEconomyRules;

/**
 *  Rules of a run of rounds around the galleon (DA_RunRules, cahier v1.3 §24.6, ADR-005): quota curve, fees, notices,
 *  hold, difficulty. The tax, the contract bonus and the shop stay in DA_EconomyRules (cahier §7), read through Economy.
 *  Starting values to test, never in the code (cahier §1).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API URunRules : public UDataAsset
{
	GENERATED_BODY()

public:

	URunRules();

	/** Destinations of the run, in order (ADR-007 §10): one contract per zone, the quota growing. Since 2026-10-07 the
	 *  three stages of the ten-day campaign (5 500 in days 1-3, 10 500 in days 4-6, 20 500 in days 7-10); since
	 *  2026-10-09 their danger and deluge multipliers are neutral (1): the difficulty curve by day carries them */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract")
	TArray<FRunZone> Zones;

	/** Days the content of the islands opens (temples, forts, every building, crabs), counted from the run's start */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Campaign")
	FRunCampaignUnlocks Campaign;

	/** What each day of the campaign changes in the danger (owner 2026-10-09: "tout devient plus dur"): one row per
	 *  day from day 1, the last row standing for the days after it; nothing on day 0 (test maps). The mob counts,
	 *  their health and damage, the soldiers and the forts, the crab, the deluge and the night; starting values in
	 *  docs/features/campagne-dix-jours.md ("Difficulté par jour"), to tune in play */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty")
	TArray<FRunDayDifficulty> DifficultyByDay;

	/** Solar hour the night falls at on the island (DA_DayNightRules: the sun 6 deg under the horizon at 18 h 31), for
	 *  the NightShare of the days */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0", ClampMax="24"))
	float NightFallHour = 18.5f;

	/** Solar hours an expedition lasts up to the deluge on the day clock (DA_DayNightRules: 11.4 h over 720 s, so the
	 *  540 s of DA_ExpeditionRules.InitialFloodDelay make 8.5 h), before the deluge scale of the day; for NightShare */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	float ExpeditionSolarHours = 8.5f;

	/** Difficulty of a day of the campaign (RunDifficulty::Of: the row of the day, the last one past the table,
	 *  neutral on day 0) */
	FRunDayDifficulty GetDayDifficulty(int32 Day) const;

	/** Days a contract lasts (ADR-007 §9) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="1"))
	int32 DaysPerContract = 3;

	/** Loot value of a day played after the quota was met (greed, ADR-007 §16) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0"))
	float GreedLootScale = 1.5f;

	/** Spawns of a day played after the quota was met (greed) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0"))
	float GreedDangerScale = 1.4f;

	/** Time added to the travel after the day while the crew answers the greed question (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0", Units="s"))
	float GreedChoiceSeconds = 15.0f;

	/** Unanswered greed question when the next day starts: true stays, false leaves the zone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract")
	bool bGreedDefaultStay = false;

	/** Money of the crew at the start of a run, for the first purchases at the shop (crowns) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0"))
	int32 StartingTreasury = 600;

	/** Part of the excess over the quota paid into the treasury when a contract is done */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0", ClampMax="1"))
	float ExcessPayoutFraction = 1.0f;

	/** Part of the treasury lost when a contract fails (ADR-007 §14) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0", ClampMax="1"))
	float FailedTreasuryLoss = 0.5f;

	/** Zones the galleon goes back after a failed contract */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract", meta=(ClampMin="0"))
	int32 FailedZoneDrop = 1;

	/** A failed contract (the quota missed on the last day of the contract) ends the run: the crew is put on a plank on
	 *  the galleon and shot by French soldiers (owner request 2026-10-07; AExecutionScene); off: money lost and back a
	 *  zone (FailedTreasuryLoss, FailedZoneDrop) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Contract")
	bool bExecuteOnFailedContract = true;

	/** Plain copy for FRunContractCalculator (the medical fee is RecruitFee) */
	FRunContractRules MakeContractRules() const;

	/** Tax, contract bonus and shop (DA_EconomyRules); none: 35 %, 100 crowns untaxed */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Economy")
	TObjectPtr<UEconomyRules> Economy;

	/** Quota of round 1 for one player (crowns) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quota", meta=(ClampMin="0"))
	int32 QuotaBase = 250;

	/** Quota added per round (crowns): Base + Step x (n-1) + Accel x (n-1)^2 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quota", meta=(ClampMin="0"))
	int32 QuotaStep = 120;

	/** Quadratic part of the quota curve (crowns) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quota", meta=(ClampMin="0"))
	int32 QuotaAccel = 30;

	/** Quota x (1 + this x (players - 1)), set when the round is launched */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Quota", meta=(ClampMin="0"))
	float PlayerQuotaScale = 0.25f;

	/** Fee per player dead during a round, the abandoned ones included (crowns) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Crew", meta=(ClampMin="0"))
	int32 RecruitFee = 50;

	/** Notice before the galleon leaves when every living player is aboard (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureNoticeAllAboard = 5.0f;

	/** Notice after a majority vote, players ashore warned (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float DepartureNoticeVote = 10.0f;

	/** Holding time of "Raise anchor" at the helm (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Departure", meta=(ClampMin="0", Units="s"))
	float HelmHoldTime = 2.0f;

	/** Preparation shown before the expedition clock starts, once everybody loaded (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition", meta=(ClampMin="0", Units="s"))
	float PreparationTime = 10.0f;

	/** Holding time of the hoist (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Galleon", meta=(ClampMin="0", Units="s"))
	float HoistHoldTime = 3.0f;

	/** Items the hold takes */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Galleon", meta=(ClampMin="1"))
	int32 HoldSlots = 40;

	/** Mass the hold takes (kg) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Galleon", meta=(ClampMin="1", Units="kg"))
	float HoldMassKg = 600.0f;

	/** Difficulty added per round (enemies, guardians, ambushes): 1 + this x (n-1) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	float DifficultyStep = 0.1f;

	/** Highest difficulty factor */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="1"))
	float DifficultyMax = 1.5f;

	/** Plain copy for FRunCalculator, the economy rules merged in */
	FRunRulesSnapshot MakeSnapshot() const;
};
