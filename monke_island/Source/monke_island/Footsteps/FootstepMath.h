// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Footsteps/FootstepRules.h"

/** Pure rules of the footsteps (tested by FootstepSpec) */
namespace FootstepMath
{
	/** Surface given by the first keyword found in one of the names, false when none matches */
	inline bool SurfaceFromNames(const TArray<FFootstepKeywords>& Keywords, TConstArrayView<FString> Names, EFootstepSurface& OutSurface)
	{
		for (const FFootstepKeywords& Entry : Keywords)
		{
			for (const FString& Word : Entry.Words)
			{
				for (const FString& Name : Names)
				{
					if (!Word.IsEmpty() && Name.Contains(Word, ESearchCase::IgnoreCase))
					{
						OutSurface = Entry.Surface;
						return true;
					}
				}
			}
		}
		return false;
	}

	/** Terrain: wet sand near the water, dry sand above */
	inline EFootstepSurface TerrainSurface(float HeightAboveWater, float WetSandBand)
	{
		return HeightAboveWater <= WetSandBand ? EFootstepSurface::SandWet : EFootstepSurface::SandDry;
	}

	/** Steps per second at a ground speed: 0 below MinSpeed, then slow -> walk -> sprint, held above */
	inline float StepRate(float Speed, float MinSpeed, float SlowRate, const FVector2D& Walk, const FVector2D& Sprint)
	{
		if (Speed < MinSpeed)
		{
			return 0.0f;
		}
		if (Speed <= Walk.X)
		{
			return FMath::GetMappedRangeValueClamped(FVector2D(MinSpeed, FMath::Max(Walk.X, MinSpeed + 1.0f)), FVector2D(SlowRate, Walk.Y), Speed);
		}
		return FMath::GetMappedRangeValueClamped(FVector2D(Walk.X, FMath::Max(Sprint.X, Walk.X + 1.0f)), FVector2D(Walk.Y, Sprint.Y), Speed);
	}

	/** Step volume from the ground speed, MinSpeed -> sprint */
	inline float StepVolume(float Speed, float MinSpeed, float SprintSpeed, const FVector2D& Range)
	{
		return FMath::GetMappedRangeValueClamped(FVector2D(MinSpeed, FMath::Max(SprintSpeed, MinSpeed + 1.0f)), Range, Speed);
	}

	/** Landing volume from the falling speed, 0 below MinSpeed */
	inline float LandingVolume(float FallSpeed, float MinSpeed, const FVector2D& Range)
	{
		if (FallSpeed < MinSpeed || MinSpeed <= 0.0f)
		{
			return 0.0f;
		}
		return FMath::GetMappedRangeValueClamped(FVector2D(MinSpeed, 2.0f * MinSpeed), Range, FallSpeed);
	}
}
