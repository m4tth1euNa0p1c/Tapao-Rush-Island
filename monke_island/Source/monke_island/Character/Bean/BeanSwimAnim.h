// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Pure swimming rules of the bean's animations (T-017), shared by UBeanAnimDriverComponent, UBeanPuppetComponent and
 *  their spec (monke_island.Bean.Swim). Values come from DA_BeanAnimLibrary.
 */
namespace BeanSwimAnim
{
	/** Movement facts the water pose is decided from */
	struct FWaterInput
	{
		/** MOVE_Swimming */
		bool bSwimming = false;

		/** MOVE_Flying: never the water pose */
		bool bFlying = false;

		/** Sea depth above the bottom of the capsule (cm) */
		float DepthAtFeet = 0.0f;

		/** Vertical speed (cm/s, up positive) */
		float VerticalSpeed = 0.0f;
	};

	/**
	 *  The bean plays the water clips while it swims, and while the water at its feet is deep enough whatever its
	 *  movement mode: a 1.05 m bean is afloat before the movement swims (SwimStartDepth is an adult's), and its short
	 *  capsule bobs at that depth between swimming and falling, which must not flicker the pose. Leaving takes
	 *  ExitHysteresis more; a fast rise (a leap out of the water) or a flight leaves at once.
	 */
	inline bool IsInWaterPose(const FWaterInput& In, bool bWasInWaterPose, float MinDepth, float ExitHysteresis, float MaxRiseSpeed)
	{
		if (In.bFlying || In.VerticalSpeed > MaxRiseSpeed)
		{
			return false;
		}
		if (In.bSwimming)
		{
			return true;
		}
		const float Threshold = bWasInWaterPose ? MinDepth - FMath::Max(ExitHysteresis, 0.0f) : MinDepth;
		return In.DepthAtFeet >= Threshold;
	}

	/** Weight of the stroke clip against treading water at a swimming speed: 0 up to TreadMaxSpeed, 1 from
	 *  SwimFullSpeed, linear in between */
	inline float StrokeWeight(float Speed, float TreadMaxSpeed, float SwimFullSpeed)
	{
		if (SwimFullSpeed <= TreadMaxSpeed)
		{
			return Speed > TreadMaxSpeed ? 1.0f : 0.0f;
		}
		return FMath::Clamp((Speed - TreadMaxSpeed) / (SwimFullSpeed - TreadMaxSpeed), 0.0f, 1.0f);
	}

	/** Play rate of the stroke clip at a swimming speed: its own pace at StrokeSpeed, within [MinRate, MaxRate] */
	inline float StrokePlayRate(float Speed, float StrokeSpeed, float MinRate, float MaxRate)
	{
		return FMath::Clamp(Speed / FMath::Max(StrokeSpeed, 1.0f), MinRate, FMath::Max(MinRate, MaxRate));
	}

	/** Height the bean is raised above its dry placement so that its root floats RootDepth under the water surface:
	 *  0 when the water is too shallow (feet on the bottom), at most MaxLift (a diving capsule takes the bean down) */
	inline float FloatLift(float SurfaceZ, float DryRootZ, float RootDepth, float MaxLift)
	{
		return FMath::Clamp(SurfaceZ - RootDepth - DryRootZ, 0.0f, FMath::Max(MaxLift, 0.0f));
	}
}
