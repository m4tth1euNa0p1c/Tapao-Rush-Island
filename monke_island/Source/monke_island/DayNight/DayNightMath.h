// monke_island

#pragma once

#include "CoreMinimal.h"
#include "DayNight/DayNightRules.h"

/** Pure functions of the day / night cycle, shared by the cycle actor and the tests */
namespace DayNightMath
{
	/** Whether the keys loop: sorted, and the last one is exactly one day after the first */
	inline bool IsLooping(const UDayNightRules& Rules)
	{
		const TArray<FDayNightKey>& Keys = Rules.Keys;
		return Rules.bLoop && Keys.Num() >= 2 && Keys.Last().ElapsedSeconds > Keys[0].ElapsedSeconds
			&& FMath::IsNearlyEqual(Keys.Last().SolarHour - Keys[0].SolarHour, 24.0f, 0.01f);
	}

	/** Unwrapped solar hour after Elapsed seconds of play (keeps growing by 24 each loop) */
	inline double SolarHourAt(const UDayNightRules& Rules, double Elapsed)
	{
		const TArray<FDayNightKey>& Keys = Rules.Keys;
		if (Keys.Num() == 0)
		{
			return 12.0;
		}
		if (Keys.Num() == 1 || Elapsed <= Keys[0].ElapsedSeconds)
		{
			return Keys[0].SolarHour;
		}

		double Days = 0.0;
		const double Period = Keys.Last().ElapsedSeconds - Keys[0].ElapsedSeconds;
		if (IsLooping(Rules))
		{
			const double Loops = FMath::FloorToDouble((Elapsed - Keys[0].ElapsedSeconds) / Period);
			Elapsed -= Loops * Period;
			Days = Loops;
		}
		else if (Elapsed >= Keys.Last().ElapsedSeconds)
		{
			return Keys.Last().SolarHour;
		}

		for (int32 Index = 1; Index < Keys.Num(); ++Index)
		{
			const FDayNightKey& A = Keys[Index - 1];
			const FDayNightKey& B = Keys[Index];
			if (Elapsed <= B.ElapsedSeconds)
			{
				const double Span = FMath::Max(B.ElapsedSeconds - A.ElapsedSeconds, UE_SMALL_NUMBER);
				const double Alpha = FMath::Clamp((Elapsed - A.ElapsedSeconds) / Span, 0.0, 1.0);
				return FMath::Lerp<double>(A.SolarHour, B.SolarHour, Alpha) + 24.0 * Days;
			}
		}
		return Keys.Last().SolarHour + 24.0 * Days;
	}

	/** Length of one day of the looping clock (s), 0 when it does not loop */
	inline double DayPeriod(const UDayNightRules& Rules)
	{
		return IsLooping(Rules) ? Rules.Keys.Last().ElapsedSeconds - Rules.Keys[0].ElapsedSeconds : 0.0;
	}

	/** Day number at an elapsed play time: 1 on the first day, +1 each time a night has passed (cycle loop) */
	inline int32 DayNumberAt(const UDayNightRules& Rules, double Elapsed)
	{
		const double Period = DayPeriod(Rules);
		if (Period <= 0.0 || Elapsed <= Rules.Keys[0].ElapsedSeconds)
		{
			return 1;
		}
		return 1 + FMath::FloorToInt32((Elapsed - Rules.Keys[0].ElapsedSeconds) / Period);
	}

	/** Elapsed play time at which day Day (1-based) begins; -1 when the clock does not loop (no days) */
	inline double DayStartElapsed(const UDayNightRules& Rules, int32 Day)
	{
		const double Period = DayPeriod(Rules);
		return Period > 0.0 ? Rules.Keys[0].ElapsedSeconds + Period * FMath::Max(Day - 1, 0) : -1.0;
	}

	/** First elapsed time at or after FromElapsed where the clock shows Hour (0-24); FromElapsed when never */
	inline double ElapsedForHour(const UDayNightRules& Rules, double Hour, double FromElapsed)
	{
		const TArray<FDayNightKey>& Keys = Rules.Keys;
		if (Keys.Num() < 2)
		{
			return FromElapsed;
		}
		const bool bLooping = IsLooping(Rules);
		const double Period = Keys.Last().ElapsedSeconds - Keys[0].ElapsedSeconds;
		const double Start = FMath::Max<double>(FromElapsed, Keys[0].ElapsedSeconds);
		const double FirstLoop = bLooping ? FMath::FloorToDouble((Start - Keys[0].ElapsedSeconds) / Period) : 0.0;

		for (int32 Loop = 0; Loop < (bLooping ? 2 : 1); ++Loop)
		{
			const double Offset = (FirstLoop + Loop) * (bLooping ? Period : 0.0);
			for (int32 Index = 1; Index < Keys.Num(); ++Index)
			{
				const FDayNightKey& A = Keys[Index - 1];
				const FDayNightKey& B = Keys[Index];
				if (B.SolarHour <= A.SolarHour)
				{
					continue;
				}
				// The wanted hour can sit in this segment once per day of unwrapping
				for (double Target = Hour + 24.0 * FMath::FloorToDouble(A.SolarHour / 24.0); Target <= B.SolarHour; Target += 24.0)
				{
					if (Target < A.SolarHour)
					{
						continue;
					}
					const double Alpha = (Target - A.SolarHour) / (B.SolarHour - A.SolarHour);
					const double Elapsed = Offset + FMath::Lerp<double>(A.ElapsedSeconds, B.ElapsedSeconds, Alpha);
					if (Elapsed >= Start - UE_KINDA_SMALL_NUMBER)
					{
						return Elapsed;
					}
				}
			}
		}
		return FromElapsed;
	}

	/**
	 *  Unit vector toward the sun in east / north / up for a local solar hour, a latitude and a day of the year.
	 *  X = east, Y = north, Z = up.
	 */
	inline FVector SunDirectionENU(double SolarHour, double LatitudeDeg, int32 DayOfYear)
	{
		const double Declination = FMath::DegreesToRadians(-23.44) * FMath::Cos(UE_DOUBLE_TWO_PI / 365.0 * (DayOfYear + 10));
		const double HourAngle = FMath::DegreesToRadians(15.0 * (FMath::Fmod(SolarHour, 24.0) - 12.0));
		const double Latitude = FMath::DegreesToRadians(LatitudeDeg);
		const double East = -FMath::Cos(Declination) * FMath::Sin(HourAngle);
		const double North = FMath::Sin(Declination) * FMath::Cos(Latitude) - FMath::Cos(Declination) * FMath::Cos(HourAngle) * FMath::Sin(Latitude);
		const double Up = FMath::Sin(Declination) * FMath::Sin(Latitude) + FMath::Cos(Declination) * FMath::Cos(HourAngle) * FMath::Cos(Latitude);
		return FVector(East, North, Up).GetSafeNormal();
	}

	/** Converts an east / north / up vector to the world frame, given the world yaw of the north */
	inline FVector ENUToWorld(const FVector& ENU, double NorthYawDeg)
	{
		const FVector North = FRotator(0.0, NorthYawDeg, 0.0).Vector();
		const FVector East = FRotator(0.0, NorthYawDeg + 90.0, 0.0).Vector();
		return (East * ENU.X + North * ENU.Y + FVector::UpVector * ENU.Z).GetSafeNormal();
	}

	/** Elevation above the horizon (degrees) of a direction whose Z is up */
	inline double ElevationDeg(const FVector& Direction)
	{
		return FMath::RadiansToDegrees(FMath::Asin(FMath::Clamp(Direction.Z, -1.0, 1.0)));
	}
}
