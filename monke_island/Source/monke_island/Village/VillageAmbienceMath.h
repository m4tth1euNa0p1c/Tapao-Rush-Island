// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the village ambience, tested without a world */
namespace VillageAmbienceMath
{
	/** Part of its placed intensity a village light keeps for a daylight in [0, 1] (full at night, DayFactor in full day) */
	inline float LightFactor(float Daylight, float DayFactor)
	{
		return FMath::Lerp(1.0f, DayFactor, FMath::Clamp(Daylight, 0.0f, 1.0f));
	}

	/** Span of the wings (Y scale) of a butterfly at time T: from MinSpan (wings up) to 1 (wings open) */
	inline float FlapSpan(float T, float Hz, float Phase, float MinSpan = 0.15f)
	{
		return FMath::Lerp(MinSpan, 1.0f, FMath::Abs(FMath::Sin(2.0f * PI * Hz * T + Phase)));
	}

	/** Brightness (scale) of a firefly at time T: off half of the time, a soft glow the other half */
	inline float Blink(float T, float Hz, float Phase)
	{
		return FMath::Max(0.0f, FMath::Sin(2.0f * PI * Hz * T + Phase));
	}

	/** Random point of the disc of radius Radius around Center, at a height in [HeightMin, HeightMax] above it;
	 *  U, V, W in [0, 1) */
	inline FVector GoalInDisc(const FVector& Center, float Radius, float HeightMin, float HeightMax, float U, float V, float W)
	{
		const float Angle = 2.0f * PI * U;
		const float Distance = Radius * FMath::Sqrt(FMath::Clamp(V, 0.0f, 1.0f));
		return Center + FVector(FMath::Cos(Angle) * Distance, FMath::Sin(Angle) * Distance, FMath::Lerp(HeightMin, HeightMax, W));
	}

	/** One step of a flyer toward its goal at Speed with a wobble (cm/s), never overshooting the goal */
	inline FVector StepToward(const FVector& Location, const FVector& Goal, float Speed, const FVector& Wobble, float DeltaSeconds)
	{
		const FVector Delta = Goal - Location;
		const double Distance = Delta.Size();
		const double Step = static_cast<double>(Speed) * DeltaSeconds;
		const FVector Direct = Distance <= Step ? Delta : Delta / Distance * Step;
		return Location + Direct + Wobble * DeltaSeconds;
	}
}
