// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Storm/WeatherRules.h"

/** Pure rules of the weather (T-203): same result on every machine and in the tests (monke_island.Weather.Transition) */
namespace WeatherMath
{
	/**
	 *  Weight of the turned weather at ServerTime: 0 while clear (StartServerTime < 0: no artifact seized) or before the
	 *  seizure, eased from 0 to 1 over TransitionSeconds, then 1 for good (it never clears back during a day).
	 *  A pure function of the replicated start and the server clock: every machine sees the same weather.
	 */
	inline float TurnWeight(double StartServerTime, double ServerTime, float TransitionSeconds)
	{
		if (StartServerTime < 0.0 || ServerTime < StartServerTime)
		{
			return 0.0f;
		}
		if (TransitionSeconds <= 0.0f)
		{
			return 1.0f;
		}
		return FMath::SmoothStep(0.0f, 1.0f, static_cast<float>((ServerTime - StartServerTime) / TransitionSeconds));
	}

	/** Mean wind strength the turned weather keeps the wind over (0 while clear) */
	inline float WindFloor(const UWeatherRules& Rules, float Weight)
	{
		return FMath::Max(0.0f, Rules.WindStrength) * FMath::Clamp(Weight, 0.0f, 1.0f);
	}

	/** Storm weight the turned weather gives the sky and the light (0-1) */
	inline float SkyFactor(const UWeatherRules& Rules, float Weight)
	{
		return FMath::Clamp(Rules.SkyStormFactor * Weight, 0.0f, 1.0f);
	}

	/**
	 *  Rain seen by a camera (0-1): the turned weather, or the final storm, under the open sky. Nothing under a roof (the
	 *  drops would fall through it) or under water; RainInCalmZones inside a calm zone (the galleon).
	 */
	inline float RainTarget(const UWeatherRules& Rules, float Weight, float FinalStorm, bool bUnderRoof, bool bUnderwater, bool bInCalmZone)
	{
		if (bUnderRoof || bUnderwater)
		{
			return 0.0f;
		}
		const float Final = Rules.bRainInFinalStorm ? FMath::Clamp(FinalStorm, 0.0f, 1.0f) : 0.0f;
		const float Rain = FMath::Max(FMath::Clamp(Weight, 0.0f, 1.0f), Final);
		return bInCalmZone ? Rain * FMath::Clamp(Rules.RainInCalmZones, 0.0f, 1.0f) : Rain;
	}

	/**
	 *  Volume factor (0-1) of the weather's own loops: they follow the turned weather and fade out once the final storm
	 *  sets in (its loops belong to UExpeditionAudioSubsystem, T-234); muted under water, IndoorVolume under a roof.
	 */
	inline float LoopVolume(const UWeatherRules& Rules, float Weight, float FinalStorm, bool bUnderRoof, bool bUnderwater)
	{
		if (bUnderwater)
		{
			return 0.0f;
		}
		const float Own = FMath::Clamp(Weight, 0.0f, 1.0f) * (1.0f - FMath::Clamp(FinalStorm, 0.0f, 1.0f));
		return bUnderRoof ? Own * FMath::Clamp(Rules.IndoorVolume, 0.0f, 1.0f) : Own;
	}

	/** Tilt of the drops from the vertical for a wind strength (degrees), capped at MaxRainTiltDeg */
	inline float RainTiltDeg(const UWeatherRules& Rules, float WindStrength)
	{
		return FMath::Clamp(Rules.RainTiltPerWind * FMath::Max(0.0f, WindStrength), 0.0f, Rules.MaxRainTiltDeg);
	}

	/** Falling direction of the drops (unit): straight down, leaning downwind by TiltDeg */
	inline FVector RainFallDirection(const FVector& Wind, float TiltDeg)
	{
		const FVector Downwind = FVector(Wind.X, Wind.Y, 0.0).GetSafeNormal();
		if (Downwind.IsNearlyZero() || TiltDeg <= 0.0f)
		{
			return FVector::DownVector;
		}
		return (FVector::DownVector + Downwind * FMath::Tan(FMath::DegreesToRadians(TiltDeg))).GetSafeNormal();
	}

	/** Value moved toward Target at the rate of a full 0-1 fade in FadeSeconds */
	inline float Approach(float Current, float Target, float DeltaSeconds, float FadeSeconds)
	{
		const float Step = FadeSeconds > 0.0f ? DeltaSeconds / FadeSeconds : 1.0f;
		return Current < Target ? FMath::Min(Target, Current + Step) : FMath::Max(Target, Current - Step);
	}

	/** True when Class is one of the trigger classes or a child of one (a Blueprint of an artifact) */
	inline bool IsTriggerClass(const UClass* Class, const TArray<const UClass*>& Triggers)
	{
		if (!Class)
		{
			return false;
		}
		for (const UClass* Trigger : Triggers)
		{
			if (Trigger && Class->IsChildOf(Trigger))
			{
				return true;
			}
		}
		return false;
	}

	/** True when a mesh name contains one of the keywords (case insensitive): roofs over the camera */
	inline bool HasKeyword(const FString& Name, const TArray<FString>& Keywords)
	{
		for (const FString& Keyword : Keywords)
		{
			if (!Keyword.IsEmpty() && Name.Contains(Keyword, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}
}
