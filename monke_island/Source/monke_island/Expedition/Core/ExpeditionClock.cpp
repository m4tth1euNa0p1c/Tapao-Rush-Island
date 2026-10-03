// monke_island

#include "Expedition/Core/ExpeditionClock.h"
#include "Expedition/Core/ExpeditionRules.h"

FExpeditionClockParams FExpeditionClockParams::FromRules(const UExpeditionRules& Rules)
{
	FExpeditionClockParams Params;
	Params.InitialFloodDelay = FMath::Max(0.0, static_cast<double>(Rules.InitialFloodDelay));
	Params.ZoneBonus = FMath::Max(0.0, static_cast<double>(Rules.ZoneBonus));
	Params.SubmersionDuration = FMath::Max(0.0, static_cast<double>(Rules.SubmersionDuration));
	Params.ZonesCount = FMath::Max(0, Rules.ZonesCount);
	Params.MaxWaterRiseCm = FMath::Max(0.0, static_cast<double>(Rules.MaxWaterRiseCm));
	Params.WaterRiseExponent = FMath::Max(0.01, static_cast<double>(Rules.WaterRiseExponent));
	return Params;
}

namespace ExpeditionClock
{
	double ComputeFloodStartTime(double StartTime, int32 BonusCount, const FExpeditionClockParams& Params)
	{
		return StartTime + Params.InitialFloodDelay + Params.ZoneBonus * FMath::Max(0, BonusCount);
	}

	FExpeditionClockState Start(double ServerTime, const FExpeditionClockParams& Params)
	{
		FExpeditionClockState State;
		State.bStarted = true;
		State.StartTime = ServerTime;
		State.FloodStartTime = ComputeFloodStartTime(ServerTime, 0, Params);
		return State;
	}

	EZoneBonusResult TryAddZoneBonus(FExpeditionClockState& State, FName ZoneId, double ServerTime, const FExpeditionClockParams& Params)
	{
		if (!State.bStarted)
		{
			return EZoneBonusResult::NotStarted;
		}
		if (ZoneId.IsNone())
		{
			return EZoneBonusResult::InvalidZone;
		}
		if (State.BonusedZoneIds.Contains(ZoneId))
		{
			return EZoneBonusResult::AlreadyGranted;
		}
		// Strictly before the deadline: at equality the flood wins (cahier §6.1)
		if (ServerTime >= State.FloodStartTime)
		{
			return EZoneBonusResult::TooLate;
		}
		if (Params.ZonesCount > 0 && State.BonusedZoneIds.Num() >= Params.ZonesCount)
		{
			return EZoneBonusResult::ZoneLimitReached;
		}

		State.BonusedZoneIds.Add(ZoneId);
		State.FloodStartTime = ComputeFloodStartTime(State.StartTime, State.BonusedZoneIds.Num(), Params);
		return EZoneBonusResult::Granted;
	}

	double SubmersionEndTime(const FExpeditionClockState& State, const FExpeditionClockParams& Params)
	{
		return State.FloodStartTime + Params.SubmersionDuration;
	}

	EExpeditionPhase PhaseAt(const FExpeditionClockState& State, double ServerTime, const FExpeditionClockParams& Params)
	{
		if (!State.bStarted)
		{
			return EExpeditionPhase::Preparing;
		}
		if (ServerTime < State.FloodStartTime)
		{
			return EExpeditionPhase::Exploring;
		}
		if (ServerTime < SubmersionEndTime(State, Params))
		{
			return EExpeditionPhase::Submerging;
		}
		return EExpeditionPhase::Resolved;
	}

	double SubmersionAlpha(const FExpeditionClockState& State, double ServerTime, const FExpeditionClockParams& Params)
	{
		if (!State.bStarted)
		{
			return 0.0;
		}
		const double Elapsed = FMath::Max(0.0, ServerTime - State.FloodStartTime);
		if (Params.SubmersionDuration <= 0.0)
		{
			return ServerTime >= State.FloodStartTime ? 1.0 : 0.0;
		}
		return FMath::Clamp(Elapsed / Params.SubmersionDuration, 0.0, 1.0);
	}

	double WaterRiseCm(const FExpeditionClockState& State, double ServerTime, const FExpeditionClockParams& Params)
	{
		const double Alpha = SubmersionAlpha(State, ServerTime, Params);
		if (Alpha <= 0.0)
		{
			return 0.0;
		}
		return Params.MaxWaterRiseCm * FMath::Pow(Alpha, FMath::Max(0.01, Params.WaterRiseExponent));
	}

	double TimeBeforeSubmersion(const FExpeditionClockState& State, double ServerTime)
	{
		return State.bStarted ? FMath::Max(0.0, State.FloodStartTime - ServerTime) : 0.0;
	}
}
