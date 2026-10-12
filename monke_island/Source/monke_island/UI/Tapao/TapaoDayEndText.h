// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Run/RunContract.h"

/** The notice the end of a day shows (T-173, ADR-007 §7, §9, §14, §16) */
enum class ETapaoDayEndKind : uint8
{
	/** JOUR N TERMINÉ: the day survived, the contract goes on (the next day, or the greed question) */
	DayDone,
	/** JOUR N PERDU: the whole crew died; the loot stays on the island, the campaign goes on */
	DayLost,
	/** CONTRAT TERMINÉ: the quota of the stage is met, on its last day or the crew left the zone */
	ContractDone,
	/** CAMPAGNE HONORÉE: the contract of the last stage is met */
	CampaignWon,
	/** CONTRAT ÉCHOUÉ: the quota missed on the milestone day; money lost, back a stage, the campaign goes on */
	ContractFailed,
	/** CONTRAT ÉCHOUÉ and the campaign lost (execution rule): the crew is executed, or was already dead */
	CampaignLost
};

/** What the notice is built from: the replicated run state and DA_RunRules (no UObject: tested by monke_island.UI.DayEnd) */
struct MONKE_ISLAND_API FTapaoDayEndInput
{
	/** The report of the day (URunStateComponent::GetLastDayReport) */
	FRunDayReport Report;

	/** Day of the campaign that ended, from 1 (URunStateComponent::GetDaysPlayed after the day); 0: the report's day */
	int32 CampaignDay = 0;

	/** The greed question is still open (URunStateComponent::IsAwaitingChoice) */
	bool bAwaitingChoice = false;

	/** The run now: its outcome, and whether the day to come is a greed day (the answer "stay") */
	ERunOutcome Outcome = ERunOutcome::InProgress;
	bool bGreedDay = false;

	/** Zone of the day that ended, and the zone of the contract to come */
	FText ZoneName;
	FText NextZoneName;

	/** Campaign day the contract to come ends on (TapaoDayEndText::ContractDeadlineDay), 0 when unknown */
	int32 DeadlineDay = 0;

	/** Treasury after the day (crowns) */
	int32 Treasury = 0;

	/** How the local player died (UDeathSpectatorComponent::DescribeOwnerDeath, T-135), written on a lost day; empty
	 *  when alive or unknown */
	FText DeathCause;

	/** DA_RunRules: part of the treasury a failed contract costs */
	float FailedTreasuryLoss = 0.5f;

	/** DA_RunRules: loot and danger of a greed day, and the answer when nobody answers */
	float GreedLootScale = 1.5f;
	float GreedDangerScale = 1.4f;
	bool bGreedDefaultStay = false;
};

/** The texts of the notice, and what it offers */
struct MONKE_ISLAND_API FTapaoDayEndView
{
	/** What follows the day, the greed answer applied */
	ERunDayVerdict Verdict = ERunDayVerdict::Invalid;

	/** Which notice */
	ETapaoDayEndKind Kind = ETapaoDayEndKind::DayDone;

	/** "BUREAU DES POSSESSIONS LOINTAINES" over a contract notice; empty over a day */
	FText Header;

	/** "JOUR 5 TERMINÉ", "JOUR 5 PERDU", "CONTRAT TERMINÉ"... */
	FText Title;

	/** The zone; on a contract notice the day too ("Villages et marae · jour 3 terminé") */
	FText Subtitle;

	/** "QUOTA : 9 648 / 5 500 couronnes" */
	FText Quota;

	/** The day (value brought back or lost, the dead and their fees), the contract (excess paid, crowns missing,
	 *  penalty), the treasury */
	TArray<FText> Lines;

	/** What follows; in the Bureau's words for a contract */
	FText Outcome;

	/** Under the greed buttons: what one more day changes and the answer when nobody answers (DA_RunRules) */
	FText GreedHint;

	/** The quota of the contract is met: the quota line in gold (its figures say it too) */
	bool bQuotaMet = false;

	/** A lost day, a failed contract, a lost campaign: the title in red (its words say it too) */
	bool bAlarm = false;

	/** The whole crew died this day */
	bool bDayLost = false;

	/** The greed question is open: its two buttons for the host, "the captain decides" for the others */
	bool bAsksGreed = false;

	/** A lost day with a travel to come: the host may send the galleon on now (JOUR SUIVANT) */
	bool bOffersNextDay = false;

	/** No travel follows: the campaign is lost (its end screen comes after) */
	bool bCampaignLost = false;
};

/** Texts of the end of a day, the contract notices and the HUD's contract line (French source, LOCTEXT "TapaoDayEnd") */
namespace TapaoDayEndText
{
	/** The verdict once the greed question was answered (a report may keep QuotaMetChoose): one more day, the contract
	 *  done, or the campaign won */
	MONKE_ISLAND_API ERunDayVerdict EffectiveVerdict(ERunDayVerdict Reported, bool bAwaitingChoice, ERunOutcome Outcome, bool bGreedDay);

	/** The notice of the end of a day */
	MONKE_ISLAND_API FTapaoDayEndView Build(const FTapaoDayEndInput& Input);

	/** "Un jour de plus : butin +50 %, danger +40 %. Sans réponse, l'équipage quitte la région." */
	MONKE_ISLAND_API FText GreedHint(float LootScale, float DangerScale, bool bDefaultStay);

	/** The live line under the notice: the time left to answer the greed question, or to the next day after a lost day;
	 *  empty otherwise (a day survived: the HUD counts the departure) and with a negative time (no travel) */
	MONKE_ISLAND_API FText Countdown(bool bAwaitingChoice, bool bDayLost, int32 SecondsLeft);

	/** Lines of the end screen of a lost campaign (CONTRAT ÉCHOUÉ, Bureau): the contract, the crowns missing, the crew
	 *  declared traitor (handed to the French squad, or posthumously), the day the campaign ends */
	MONKE_ISLAND_API TArray<FText> LostCampaignLines(const FRunDayReport& Report, const FText& ZoneName, int32 CampaignDay);

	/** Campaign day the contract under way ends on (its milestone): days played + 1 + the days left in the contract; 0
	 *  without a run */
	MONKE_ISLAND_API int32 ContractDeadlineDay(int32 DaysPlayed, int32 DayInContract, int32 DaysPerContract);

	/** The HUD's line of the contract (T-283): "Contrat : jour 3", "Contrat : jour 3 · jour de cupidité"; empty without
	 *  a run */
	MONKE_ISLAND_API FText ContractLine(int32 DaysPlayed, int32 DayInContract, int32 DaysPerContract, bool bGreedDay);

	/** A campaign is under way on this map (the run state was filled): a wiped crew loses the day, not the game; false on
	 *  a map without a campaign, where NAUFRAGÉ stays */
	MONKE_ISLAND_API bool IsCampaignUnderWay(int32 Quota, int32 DaysPlayed, ERunDayVerdict LastVerdict);
}
