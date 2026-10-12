// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure geometry of the spell slots of the HUD (STapaoSpellSlot, T-333), tested by monke_island.Emotes.TrumpetCall */
namespace TapaoSpellMath
{
	/** Point of the border of a square of side Size (Y down) seen from its centre at this angle (degrees, clockwise
	 *  from 12 o'clock) */
	inline FVector2f SquareBorderPoint(float Size, float AngleDegrees)
	{
		const float Radians = FMath::DegreesToRadians(AngleDegrees);
		const FVector2f Direction(FMath::Sin(Radians), -FMath::Cos(Radians));
		const float Reach = 0.5f * Size / FMath::Max(FMath::Max(FMath::Abs(Direction.X), FMath::Abs(Direction.Y)), KINDA_SMALL_NUMBER);
		return FVector2f(0.5f * Size, 0.5f * Size) + Direction * Reach;
	}

	/**
	 *  Border of a square of side Size (Y down) still covered by a clock wipe that has Fraction to run, as League of
	 *  Legends draws a cooldown: from the angle (1 - Fraction) of a turn, clockwise, round to 12 o'clock, with the corners
	 *  in between. Empty when Fraction is 0; the whole border, 12 o'clock to 12 o'clock, when it is 1.
	 */
	inline void SquareSweep(float Size, float Fraction, TArray<FVector2f>& OutBorder)
	{
		OutBorder.Reset();
		const float Clamped = FMath::Clamp(Fraction, 0.0f, 1.0f);
		if (Clamped <= 0.0f || Size <= 0.0f)
		{
			return;
		}
		const float From = 360.0f * (1.0f - Clamped);
		OutBorder.Add(SquareBorderPoint(Size, From));
		// the corners of the square sit at 45, 135, 225 and 315 degrees from 12 o'clock
		for (float Corner = 45.0f; Corner < 360.0f; Corner += 90.0f)
		{
			if (Corner > From)
			{
				OutBorder.Add(SquareBorderPoint(Size, Corner));
			}
		}
		OutBorder.Add(SquareBorderPoint(Size, 360.0f));
	}
}
