// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the giant crabs coming out of the sand (tested by monke_island.Creatures.CrabEmergence) */
namespace GiantCrabEmergenceMath
{
	/** True when the ground is beach sand: between MinAbove and MaxAbove over the sea, and flat enough */
	inline bool IsSand(double GroundZ, double SeaZ, double NormalZ, double MinAbove, double MaxAbove, double MinNormalZ)
	{
		const double Above = GroundZ - SeaZ;
		return Above >= MinAbove && Above <= MaxAbove && NormalZ >= MinNormalZ;
	}

	/** Index of the emergence due at Elapsed seconds into the day (the first not done yet), INDEX_NONE when none */
	inline int32 DueEmergence(double Elapsed, const TArray<float>& Times, int32 Done)
	{
		const int32 Next = FMath::Max(Done, 0);
		return Times.IsValidIndex(Next) && Elapsed >= Times[Next] ? Next : INDEX_NONE;
	}

	/** Times of the day the crabs rise at (s) on a day of the campaign: the first at FirstMinute, the others shifted by
	 *  the same time (never before 0); the rules' times as they are when FirstMinute is 0 (the difficulty by day,
	 *  owner 2026-10-09: the crab out at 4:00 on day 10) */
	inline TArray<float> DayTimes(const TArray<float>& Times, float FirstMinute)
	{
		TArray<float> Out = Times;
		if (FirstMinute > 0.0f && !Out.IsEmpty())
		{
			const float Shift = FirstMinute * 60.0f - Out[0];
			for (float& Time : Out)
			{
				Time = FMath::Max(0.0f, Time + Shift);
			}
		}
		return Out;
	}

	/** Point Index of Count around Center at Distance (ground plane), starting at StartAngle (deg) */
	inline FVector RingPoint(const FVector& Center, double Distance, int32 Index, int32 Count, double StartAngle)
	{
		const double Angle = FMath::DegreesToRadians(StartAngle + 360.0 * Index / FMath::Max(Count, 1));
		return FVector(Center.X + FMath::Cos(Angle) * Distance, Center.Y + FMath::Sin(Angle) * Distance, Center.Z);
	}

	/** Height of the rise at Alpha (0-1): eased out, it slows as it breaks the surface */
	inline double RiseHeight(double Alpha, double Depth)
	{
		const double A = FMath::Clamp(Alpha, 0.0, 1.0);
		return -Depth * (1.0 - A) * (1.0 - A);
	}
}
