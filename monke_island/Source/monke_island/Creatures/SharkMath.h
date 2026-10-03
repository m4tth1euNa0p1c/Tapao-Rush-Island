// monke_island

#pragma once

#include "CoreMinimal.h"
#include "SharkMath.generated.h"

/** Behaviour states of the shark (cahier §8) */
UENUM(BlueprintType)
enum class ESharkState : uint8
{
	Patrol,
	Detect,
	Chase,
	Attack,
	Return,
	Dead
};

/** Pure rules of the shark, tested without a world */
namespace SharkMath
{
	/** True when the water column (surface - bottom) is deep enough for the shark */
	inline bool IsNavigable(float SurfaceZ, float BottomZ, float MinDepth)
	{
		return SurfaceZ - BottomZ >= MinDepth;
	}

	/** True when a player can be hunted: swimming (immersed), alive and within Radius */
	inline bool IsValidPrey(bool bSwimming, bool bDead, float Distance, float Radius)
	{
		return bSwimming && !bDead && Distance <= Radius;
	}

	/** Yaw after turning toward DesiredYaw at RateDeg per second (degrees, unwound) */
	inline float TurnToward(float CurrentYaw, float DesiredYaw, float RateDeg, float DeltaSeconds)
	{
		const float Delta = FMath::FindDeltaAngleDegrees(CurrentYaw, DesiredYaw);
		const float Step = RateDeg * DeltaSeconds;
		return FRotator::NormalizeAxis(CurrentYaw + FMath::Clamp(Delta, -Step, Step));
	}

	/** True when a bite can land: in range and the interval since the last bite is over */
	inline bool CanBite(float Distance, float Range, double Now, double LastBiteTime, float Interval)
	{
		return Distance <= Range && Now - LastBiteTime >= Interval;
	}

	/**
	 *  Next state while hunting: the chase ends (Return) when the prey is no longer valid (out of the water, dead,
	 *  too far) or the chase lasted too long without a bite.
	 */
	inline ESharkState HuntStep(ESharkState State, bool bPreyValid, float ChaseSeconds, float GiveUpTime)
	{
		if (State != ESharkState::Detect && State != ESharkState::Chase && State != ESharkState::Attack)
		{
			return State;
		}
		if (!bPreyValid || ChaseSeconds > GiveUpTime)
		{
			return ESharkState::Return;
		}
		return State;
	}
}
