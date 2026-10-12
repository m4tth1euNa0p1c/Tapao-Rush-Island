// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure shore probe logic, shared by the ambience actor and the tests */
namespace ShoreAmbienceMath
{
	/** Result of a probe ring: nearest shore crossing and how much water there is around */
	struct FShore
	{
		/** A crossing between land and water was found */
		bool bFound = false;

		/** Distance of the nearest crossing from the listener (cm) */
		float Distance = 0.0f;

		/** Direction index of the nearest crossing */
		int32 Direction = INDEX_NONE;

		/** Share of the probes (centre included) that are water, 0..1 */
		float WaterShare = 0.0f;
	};

	/**
	 *  Wet[Direction * Radii.Num() + RadiusIndex] says whether the probe there is water; bCentreWet is the listener's
	 *  own spot. Along each direction, the first change between land and water (from the centre outward) is a
	 *  crossing, placed half way between the two probes.
	 */
	inline FShore FindShore(bool bCentreWet, const TArray<bool>& Wet, const TArray<float>& Radii, int32 Directions)
	{
		FShore Shore;
		const int32 Rings = Radii.Num();
		if (Rings == 0 || Directions <= 0 || Wet.Num() < Rings * Directions)
		{
			Shore.WaterShare = bCentreWet ? 1.0f : 0.0f;
			return Shore;
		}
		int32 WetCount = bCentreWet ? 1 : 0;
		for (int32 Direction = 0; Direction < Directions; ++Direction)
		{
			bool bPrevious = bCentreWet;
			float PreviousRadius = 0.0f;
			bool bCrossed = false;
			for (int32 Ring = 0; Ring < Rings; ++Ring)
			{
				const bool bWet = Wet[Direction * Rings + Ring];
				WetCount += bWet ? 1 : 0;
				if (!bCrossed && bWet != bPrevious)
				{
					bCrossed = true;
					const float Distance = 0.5f * (PreviousRadius + Radii[Ring]);
					if (!Shore.bFound || Distance < Shore.Distance)
					{
						Shore.bFound = true;
						Shore.Distance = Distance;
						Shore.Direction = Direction;
					}
				}
				bPrevious = bWet;
				PreviousRadius = Radii[Ring];
			}
		}
		Shore.WaterShare = static_cast<float>(WetCount) / static_cast<float>(1 + Rings * Directions);
		return Shore;
	}

	/** Unit direction of probe index Direction out of Directions, around Z (index 0 = +X) */
	inline FVector ProbeDirection(int32 Direction, int32 Directions)
	{
		const float Angle = UE_TWO_PI * static_cast<float>(Direction) / static_cast<float>(FMath::Max(Directions, 1));
		return FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0f);
	}
}
