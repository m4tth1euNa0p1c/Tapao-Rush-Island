// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Run/RunContract.h"

/** What follows the end of a day on the island (ADR-007 §14, T-104, T-173) */
enum class EDayEndFollowUp : uint8
{
	/** The galleon sails to the next island once the wait is over: the next day of the campaign, after a day survived
	 *  or lost (the whole crew dead: the loot stays on the island, the day is spent, the campaign goes on) */
	NextIsland,
	/** The contract failed on its milestone day with the execution rule: the survivors are executed on the galleon
	 *  (AExecutionScene); no travel, the campaign is lost */
	Execution,
	/** The same failure with nobody left to execute (the whole crew died that day): no scene, no travel, the campaign is
	 *  lost */
	CampaignLost
};

/** The waits the end of a day is planned with (s): DA_ExpeditionRules and DA_RunRules, never in the code */
struct FDayEndDelays
{
	/** DA_ExpeditionRules.DayEndTravelDelay: a day survived, before the travel */
	float SurvivedTravelDelay = 10.0f;

	/** DA_ExpeditionRules.GameOverRestartDelay ("JOUR PERDU"): a day lost (the whole crew dead), before the travel */
	float LostTravelDelay = 8.0f;

	/** DA_RunRules.GreedChoiceSeconds: added while the crew answers the greed question */
	float GreedChoiceSeconds = 15.0f;
};

/** The end of a day, planned */
struct FDayEndPlan
{
	/** What follows */
	EDayEndFollowUp FollowUp = EDayEndFollowUp::NextIsland;

	/** Wait before the travel to the next island (s), negative when no travel follows */
	float TravelWait = -1.0f;

	/** No travel follows: the campaign is lost (execution, or a crew wiped on its milestone day) */
	bool IsCampaignLost() const { return FollowUp != EDayEndFollowUp::NextIsland; }
};

/** Pure rules of the end of a day (tested by monke_island.Expedition.Clock, "Lost day") */
namespace ExpeditionDayEnd
{
	/**
	 *  What follows a day that ended with survivors (bWiped false) or with the whole crew dead (bWiped true), given the
	 *  verdict of the run (Invalid without a run). A lost day is never the end of the game (ADR-007 §14: "jour perdu",
	 *  the galleon sails on); only a contract failed on its milestone day with the execution rule ends the campaign
	 *  (ERunDayVerdict::Executed). The greed question adds its time to the wait, lost day or not.
	 */
	inline FDayEndPlan Plan(bool bWiped, ERunDayVerdict Verdict, const FDayEndDelays& Delays)
	{
		FDayEndPlan Result;
		if (Verdict == ERunDayVerdict::Executed)
		{
			Result.FollowUp = bWiped ? EDayEndFollowUp::CampaignLost : EDayEndFollowUp::Execution;
			Result.TravelWait = -1.0f;
			return Result;
		}
		Result.FollowUp = EDayEndFollowUp::NextIsland;
		Result.TravelWait = FMath::Max(0.0f, bWiped ? Delays.LostTravelDelay : Delays.SurvivedTravelDelay);
		if (Verdict == ERunDayVerdict::QuotaMetChoose)
		{
			// the crew answers the greed question before the galleon sails (the host may answer, and hurry, sooner)
			Result.TravelWait += FMath::Max(0.0f, Delays.GreedChoiceSeconds);
		}
		return Result;
	}
}
