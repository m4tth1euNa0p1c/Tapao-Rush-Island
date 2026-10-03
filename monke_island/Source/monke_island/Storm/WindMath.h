// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Storm/WindRules.h"

/** One gust of the schedule */
struct FWindGust
{
	/** 0-based index in the schedule */
	int32 Index = INDEX_NONE;

	/** Elapsed play time of its start and end (s) */
	double Start = 0.0;
	double End = 0.0;
};

/** Pure rules of the wind schedule (same result on every machine and in the tests) */
namespace WindMath
{
	/** Start of the gust after the one starting at PreviousStart: duration + minimum calm + seeded random extra */
	inline double NextGustStart(const UWindRules& Rules, int32 PreviousIndex, double PreviousStart)
	{
		FRandomStream Stream(Rules.Seed + 7919 * (PreviousIndex + 1));
		return PreviousStart + Rules.GustDurationSeconds + Rules.MinGapSeconds + Stream.FRand() * Rules.MaxExtraGapSeconds;
	}

	/** Last gust started at or before Elapsed (Index INDEX_NONE before the first one) */
	inline FWindGust LastGustAt(const UWindRules& Rules, double Elapsed)
	{
		FWindGust Gust;
		double Start = Rules.FirstGustSeconds;
		for (int32 Index = 0; Start <= Elapsed && Index < 100000; ++Index)
		{
			Gust.Index = Index;
			Gust.Start = Start;
			Gust.End = Start + Rules.GustDurationSeconds;
			Start = NextGustStart(Rules, Index, Start);
		}
		return Gust;
	}

	/** Level of a gust: +1 every GustsPerLevel gusts */
	inline int32 LevelOf(const UWindRules& Rules, int32 GustIndex)
	{
		return GustIndex <= 0 ? 0 : GustIndex / FMath::Max(Rules.GustsPerLevel, 1);
	}

	/** Peak strength of a gust */
	inline float GustStrength(const UWindRules& Rules, int32 GustIndex)
	{
		return Rules.BaseGustStrength + Rules.StrengthPerLevel * LevelOf(Rules, GustIndex);
	}

	/** Rise and fall of a gust: 0 outside, 1 on its plateau */
	inline float GustEnvelope(const UWindRules& Rules, const FWindGust& Gust, double Elapsed)
	{
		if (Gust.Index == INDEX_NONE || Elapsed < Gust.Start || Elapsed > Gust.End)
		{
			return 0.0f;
		}
		const float Ramp = FMath::Max(Rules.GustRampSeconds, 0.01f);
		const float In = static_cast<float>((Elapsed - Gust.Start) / Ramp);
		const float Out = static_cast<float>((Gust.End - Elapsed) / Ramp);
		return FMath::SmoothStep(0.0f, 1.0f, FMath::Min(In, Out));
	}

	/** Mean strength of the wind (before turbulence): breeze, gust, or the ultra violent wind of the deadline day */
	inline float MeanStrengthAt(const UWindRules& Rules, double Elapsed, double DeadlineStartElapsed, int32* OutGustIndex = nullptr)
	{
		const FWindGust Gust = LastGustAt(Rules, Elapsed);
		if (OutGustIndex)
		{
			*OutGustIndex = Gust.Index;
		}
		const float Gusting = GustStrength(Rules, Gust.Index) * GustEnvelope(Rules, Gust, Elapsed);
		float Strength = FMath::Max(Rules.CalmStrength, Gusting);
		if (DeadlineStartElapsed >= 0.0 && Elapsed >= DeadlineStartElapsed)
		{
			const float Ramp = FMath::SmoothStep(0.0f, 1.0f, static_cast<float>((Elapsed - DeadlineStartElapsed) / Rules.DeadlineRampSeconds));
			Strength = FMath::Max(Strength, Rules.DeadlineStrength * Ramp);
		}
		return Strength;
	}

	/**
	 *  Seconds since the ultra violent wind set in on the expedition clock, -1 before (T-098).
	 *  It sets in LeadSeconds before the flood deadline (never before the start of the expedition).
	 *  bSubmersionConfirmed: the expedition reached Submerging; until a client receives that phase, the part after the
	 *  deadline is held (a zone bonus replicated late must not make the storm set in, then stop).
	 */
	inline double ExpeditionStormSeconds(const FExpeditionClockState& State, double ServerTime, double LeadSeconds, bool bSubmersionConfirmed)
	{
		if (!State.bStarted)
		{
			return -1.0;
		}
		const double StormStart = FMath::Max(State.StartTime, State.FloodStartTime - FMath::Max(0.0, LeadSeconds));
		if (ServerTime < StormStart)
		{
			return -1.0;
		}
		if (ServerTime >= State.FloodStartTime && !bSubmersionConfirmed)
		{
			// Past the deadline but not confirmed yet: hold at the deadline (no storm when it has no lead)
			return State.FloodStartTime > StormStart ? State.FloodStartTime - StormStart : -1.0;
		}
		return ServerTime - StormStart;
	}

	/** Elapsed time at which the ultra violent wind set in, for MeanStrengthAt; -1 when it has not (StormSeconds < 0) */
	inline double DeadlineStartElapsed(double Elapsed, double StormSeconds)
	{
		return StormSeconds >= 0.0 ? FMath::Max(0.0, Elapsed - StormSeconds) : -1.0;
	}

	/** True when a mesh name contains one of the keywords (case insensitive) */
	inline bool HasKeyword(const FString& Name, const TArray<FString>& Keywords)
	{
		for (const FString& Keyword : Keywords)
		{
			if (!Keyword.IsEmpty() && Name.Contains(Keyword, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}
