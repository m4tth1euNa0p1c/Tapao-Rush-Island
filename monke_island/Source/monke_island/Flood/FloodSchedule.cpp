// monke_island

#include "Flood/FloodSchedule.h"
#include "Expedition/Core/ExpeditionClock.h"
#include "Flood/FloodRules.h"

namespace FloodSchedule
{
	double ExpeditionFloodSeconds(const FExpeditionClockState& State, double ServerTime, bool bSubmersionConfirmed)
	{
		if (!State.bStarted || !bSubmersionConfirmed)
		{
			return -1.0;
		}
		// Confirmed by the replicated phase a hair before the synchronised server time reaches the deadline: start at 0
		return FMath::Max(0.0, ServerTime - State.FloodStartTime);
	}

	double ExpeditionRiseCm(const FExpeditionClockState& State, double FloodSeconds, const FExpeditionClockParams& Params)
	{
		if (FloodSeconds < 0.0)
		{
			return 0.0;
		}
		return ExpeditionClock::WaterRiseCm(State, State.FloodStartTime + FloodSeconds, Params);
	}

	double DayNightFloodSeconds(const UFloodRules& Rules, double Elapsed)
	{
		const double FloodTime = Elapsed - Rules.StartSeconds;
		return FloodTime < 0.0 ? -1.0 : FloodTime;
	}

	double DayNightRiseCm(const UFloodRules& Rules, double FloodSeconds)
	{
		if (FloodSeconds < 0.0)
		{
			return 0.0;
		}
		const double Alpha = FMath::Clamp(FloodSeconds / FMath::Max(static_cast<double>(Rules.RiseSeconds), UE_KINDA_SMALL_NUMBER), 0.0, 1.0);
		return Rules.RiseCm * FMath::Pow(Alpha, static_cast<double>(Rules.RiseExponent));
	}

	double HoldRise(double PreviousRiseCm, double TargetRiseCm)
	{
		return FMath::Max(PreviousRiseCm, TargetRiseCm);
	}
}
