// monke_island

#pragma once

#include "CoreMinimal.h"
#include "RunDifficulty.generated.h"

/**
 *  What one day of the campaign changes in the danger of the island (owner 2026-10-09: "tout devient plus dur"): one
 *  row per day in DA_RunRules.DifficultyByDay, the last row standing for the days after it. Every value multiplies the
 *  rules of the creatures and the forts (DA_TikiRules, DA_FortSoldierRules, DA_GiantCrabRules, DA_FortSiteRules,
 *  DA_GiantCrabEmergenceRules, DA_ExpeditionRules), or replaces one of their counts when it is set; 1 (or 0 for a
 *  count) leaves them as they are. Day 0 (no campaign: test maps, PIE) plays everything as the rules say.
 *  docs/features/campagne-dix-jours.md, "Difficulté par jour".
 */
USTRUCT(BlueprintType)
struct MONKE_ISLAND_API FRunDayDifficulty
{
	GENERATED_BODY()

	/** Hostile tiki that come out: the bushes by day, the night, the defenders and guardians of the villages (every
	 *  count of DA_TikiRules, on top of the zone and the greed day of the contract and the surplus of the campaign) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	float MobCountScale = 1.0f;

	/** Health of the tiki warriors and of the soldiers that appear this day (their rules' health times this) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.1"))
	float MobHealthScale = 1.0f;

	/** Damage of the mobs, the soldiers and the giant crab (their blows, balls and throws) to the players */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	float MobDamageScale = 1.0f;

	/** Soldiers of each fort: the garrison of the fort level is completed at its posts (clones of those placed) or
	 *  thinned to this; 0 = as placed in the level (7) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	int32 SoldiersPerFort = 0;

	/** Chance a soldier's ball strikes, relative to DA_FortSoldierRules (held at 1 by the soldier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	float SoldierHitChanceScale = 1.0f;

	/** Forts built in the fragment of the day, as many as its sites allow; 0 = DA_FortSiteRules.FortsPerExpedition */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	int32 FortsPerExpedition = 0;

	/** Health of the giant crabs that rise out of the sand (DA_GiantCrabRules.MaxHealth times this) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.1"))
	float CrabHealthScale = 1.0f;

	/** Minute of the day the first crab rises at (4 = 4:00); the other times of DA_GiantCrabEmergenceRules follow,
	 *  shifted by the same amount; 0 = the times of those rules (5:30) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0"))
	float CrabEmergenceMinute = 0.0f;

	/** Time before the deluge, relative to DA_ExpeditionRules.InitialFloodDelay (below 1: the sea rises earlier) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0.1"))
	float FloodDelayScale = 1.0f;

	/** Share of the expedition (up to the deluge) played at night: the day starts at the solar hour that makes the
	 *  night (DA_RunRules.NightFallHour) fall after the rest; 0 = the campaign's rule (the dawn draw of
	 *  DA_DayNightRules, the early night from DA_RunRules.Campaign.EarlyNightFromDay) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Difficulty", meta=(ClampMin="0", ClampMax="1"))
	float NightShare = 0.0f;
};

/** Pure rules of the difficulty by day (tested by monke_island.Run.Campaign, "Difficulty by day") */
namespace RunDifficulty
{
	/** The row with its values held above their floors (an asset edited by hand) */
	inline FRunDayDifficulty Sanitized(const FRunDayDifficulty& Row)
	{
		FRunDayDifficulty Out = Row;
		Out.MobCountScale = FMath::Max(Out.MobCountScale, 0.0f);
		Out.MobHealthScale = FMath::Max(Out.MobHealthScale, 0.1f);
		Out.MobDamageScale = FMath::Max(Out.MobDamageScale, 0.0f);
		Out.SoldiersPerFort = FMath::Max(Out.SoldiersPerFort, 0);
		Out.SoldierHitChanceScale = FMath::Max(Out.SoldierHitChanceScale, 0.0f);
		Out.FortsPerExpedition = FMath::Max(Out.FortsPerExpedition, 0);
		Out.CrabHealthScale = FMath::Max(Out.CrabHealthScale, 0.1f);
		Out.CrabEmergenceMinute = FMath::Max(Out.CrabEmergenceMinute, 0.0f);
		Out.FloodDelayScale = FMath::Max(Out.FloodDelayScale, 0.1f);
		Out.NightShare = FMath::Clamp(Out.NightShare, 0.0f, 1.0f);
		return Out;
	}

	/** Difficulty of Day (from 1): its row, the last one past the end of the table; neutral (everything x1, the rules'
	 *  counts) on day 0 (no campaign: test maps, PIE) and with an empty table */
	inline FRunDayDifficulty Of(const TArray<FRunDayDifficulty>& ByDay, int32 Day)
	{
		if (Day <= 0 || ByDay.IsEmpty())
		{
			return FRunDayDifficulty();
		}
		return Sanitized(ByDay[FMath::Min(Day - 1, ByDay.Num() - 1)]);
	}

	/** A count of the day, or the rules' count when the day sets none (0) */
	inline int32 CountOr(int32 Count, int32 Fallback)
	{
		return Count > 0 ? Count : Fallback;
	}

	/** Maximum health of a creature after the scale of the day (never under 1) */
	inline float ScaledMaxHealth(float MaxHealth, float Scale)
	{
		return FMath::Max(1.0f, MaxHealth * FMath::Max(Scale, 0.0f));
	}

	/** Solar hour (0-24) the expedition starts at so that Share of its ExpeditionHours up to the deluge is night: the
	 *  night falls at NightFallHour, the day before it is the rest; never after the night fall nor before midnight */
	inline float NightShareStartHour(float NightFallHour, float ExpeditionHours, float Share)
	{
		const float DayHours = FMath::Max(ExpeditionHours, 0.0f) * (1.0f - FMath::Clamp(Share, 0.0f, 1.0f));
		return FMath::Clamp(NightFallHour - DayHours, 0.0f, FMath::Max(NightFallHour, 0.0f));
	}

	/** One line for the journal */
	inline FString Describe(const FRunDayDifficulty& Day)
	{
		return FString::Printf(TEXT("mobs x%.2f, health x%.2f, damage x%.2f, soldiers %d, hit chance x%.2f, forts %d, crab health x%.2f, crab at %.2f min, flood x%.2f, night %.0f %%"),
			Day.MobCountScale, Day.MobHealthScale, Day.MobDamageScale, Day.SoldiersPerFort, Day.SoldierHitChanceScale, Day.FortsPerExpedition,
			Day.CrabHealthScale, Day.CrabEmergenceMinute, Day.FloodDelayScale, Day.NightShare * 100.0f);
	}
}
