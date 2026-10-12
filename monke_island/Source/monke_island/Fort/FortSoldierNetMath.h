// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Pure network rules of a fort soldier (T-317, network review T-268 R-28; tested by monke_island.Fort.Net), no world:
 *  the update rates of a living and of a dead soldier, when a dead one stops replicating, when its body goes.
 */
namespace FortSoldierNet
{
	/** Update rates given to the engine: a rate within 1..100 per second, a minimum within 0.1..rate */
	struct FRates
	{
		float Frequency = 30.0f;
		float MinFrequency = 2.0f;
	};

	/** Rates of a soldier: the living ones, or the dead rate (never above the living one) once it died */
	inline FRates PickRates(bool bDead, float AliveFrequency, float AliveMinFrequency, float DeadFrequency)
	{
		FRates Rates;
		const float Alive = FMath::Clamp(FMath::IsFinite(AliveFrequency) ? AliveFrequency : 30.0f, 1.0f, 100.0f);
		Rates.Frequency = bDead ? FMath::Clamp(FMath::IsFinite(DeadFrequency) ? DeadFrequency : 1.0f, 1.0f, Alive) : Alive;
		Rates.MinFrequency = FMath::Clamp(FMath::IsFinite(AliveMinFrequency) ? AliveMinFrequency : 2.0f, 0.1f, Rates.Frequency);
		return Rates;
	}

	/** Whether a dead soldier may stop replicating (dormancy): dead for DormancySeconds at least, its body settled (on
	 *  the ground, neither falling nor carried) */
	inline bool ShouldGoDormant(double SecondsSinceDeath, float DormancySeconds, bool bBodySettled)
	{
		return bBodySettled && FMath::IsFinite(SecondsSinceDeath) && SecondsSinceDeath >= FMath::Max(0.0f, DormancySeconds);
	}

	/** Whether the body of a dead soldier may go now: past its lifetime (0 = it stays), no player within ClearanceCm */
	inline bool ShouldRemoveBody(double SecondsSinceDeath, float LifetimeSeconds, double NearestPlayerCm, float ClearanceCm)
	{
		return LifetimeSeconds > 0.0f && FMath::IsFinite(SecondsSinceDeath) && SecondsSinceDeath >= LifetimeSeconds
			&& NearestPlayerCm >= FMath::Max(0.0f, ClearanceCm);
	}

	/** Seconds before the server looks at a dead soldier again: the dormancy or the lifetime still to come, at least
	 *  CheckSeconds when it waits (a body still falling, a player near); a negative value when nothing is left to do */
	inline double NextCheckDelay(double SecondsSinceDeath, bool bDormant, float DormancySeconds, float LifetimeSeconds, float CheckSeconds)
	{
		const double Check = FMath::Max(0.25, static_cast<double>(CheckSeconds));
		if (!bDormant)
		{
			return FMath::Max(Check, static_cast<double>(DormancySeconds) - SecondsSinceDeath);
		}
		if (LifetimeSeconds <= 0.0f)
		{
			return -1.0;
		}
		return FMath::Max(Check, static_cast<double>(LifetimeSeconds) - SecondsSinceDeath);
	}
}
