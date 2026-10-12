// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Pure helpers of the trumpet call (key A, T-333, UTrumpetEmoteComponent): its timing, its cooldown and the stamina it
 *  gives back. Tested by monke_island.Emotes.TrumpetCall.
 */
namespace TrumpetCallMath
{
	/** Length of the play loop of a call of CallSeconds in all: the equip and the put away take the time of their clips */
	inline float LoopSeconds(float CallSeconds, float EquipSeconds, float UnequipSeconds)
	{
		return FMath::Max(CallSeconds - EquipSeconds - UnequipSeconds, 0.0f);
	}

	/** True once the cooldown counted from the start of the last call (LastStart, server time) is over */
	inline bool IsReady(double Now, double LastStart, float Cooldown)
	{
		return Now >= LastStart + FMath::Max(Cooldown, 0.0f);
	}

	/** Seconds before the next call may start, 0 once ready */
	inline float CooldownLeft(double Now, double LastStart, float Cooldown)
	{
		return static_cast<float>(FMath::Max(0.0, LastStart + FMath::Max(Cooldown, 0.0f) - Now));
	}

	/** Share of the cooldown still to run: 1 as the call starts, 0 once ready (the dark veil of the icon) */
	inline float CooldownFraction(double Now, double LastStart, float Cooldown)
	{
		return Cooldown > 0.0f ? FMath::Clamp(CooldownLeft(Now, LastStart, Cooldown) / Cooldown, 0.0f, 1.0f) : 0.0f;
	}

	/**
	 *  Stamina given back by a call Elapsed seconds after its start, in all: nothing while the trumpet comes out, then
	 *  Total spread evenly over SpreadSeconds from the start of the play loop (the whole loop when SpreadSeconds is 0 or
	 *  longer than it), all of it once that time is over. The caller stops Elapsed at the end of a loop cut short.
	 */
	inline float StaminaGiven(float Elapsed, float EquipSeconds, float InLoopSeconds, float SpreadSeconds, float Total)
	{
		const float Spread = SpreadSeconds > 0.0f ? FMath::Min(SpreadSeconds, InLoopSeconds) : InLoopSeconds;
		if (Total <= 0.0f || Elapsed < EquipSeconds)
		{
			return 0.0f;
		}
		return Spread > 0.0f ? Total * FMath::Clamp((Elapsed - EquipSeconds) / Spread, 0.0f, 1.0f) : Total;
	}
}
