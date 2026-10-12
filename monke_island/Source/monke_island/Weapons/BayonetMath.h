// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/FirearmRules.h"

/**
 *  Pure rules of the bayonet of the bayonet musket (T-335), shared by ABayonetMusketWeapon and the tests
 *  (monke_island.Weapons.Bayonet): the short cone in front of the carrier the point strikes in, the choice between the
 *  thrust and the charge, its cost in stamina, when a thrust may start, the lunge. No world.
 */
namespace BayonetMath
{
	/**
	 *  Score of a target point for a thrust along Forward (Offset from the carrier's centre to the nearest point of the
	 *  target's body): its horizontal distance, made longer the further it is off the axis (the target right in front of
	 *  the point first); -1 when out of the cone: farther than Reach, more than HeightTolerance above or below, or more
	 *  than HalfAngleDegrees off the axis
	 */
	inline float ThrustScore(const FVector& Forward, const FVector& Offset, float Reach, float HalfAngleDegrees, float HeightTolerance)
	{
		if (FMath::Abs(Offset.Z) > FMath::Max(HeightTolerance, 0.0f))
		{
			return -1.0f;
		}
		const FVector Flat(Offset.X, Offset.Y, 0.0f);
		const float Distance = static_cast<float>(Flat.Size());
		if (Distance > Reach)
		{
			return -1.0f;
		}
		if (Distance < 1.0f)
		{
			// against the carrier: struck whatever the angle
			return 0.0f;
		}
		const FVector Axis = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
		if (Axis.IsNearlyZero())
		{
			return -1.0f;
		}
		const float Cos = static_cast<float>(FVector::DotProduct(Axis, Flat / Distance));
		if (Cos < FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f))) - UE_KINDA_SMALL_NUMBER)
		{
			return -1.0f;
		}
		return Distance * (2.0f - Cos);
	}

	/** The carrier charges rather than thrusts: it runs (sprint), or moves at least MinSpeed (0: the sprint only) */
	inline bool IsCharge(bool bSprinting, float Speed, float MinSpeed)
	{
		return bSprinting || (MinSpeed > 0.0f && Speed >= MinSpeed);
	}

	/** Stamina a thrust costs: a share of the carrier's MaxStamina */
	inline float StaminaCost(float MaxStamina, float CostFraction)
	{
		return FMath::Max(0.0f, MaxStamina * CostFraction);
	}

	/** The carrier has the stamina of a thrust (none below that: the thrust is refused) */
	inline bool HasStaminaFor(float Stamina, float MaxStamina, float CostFraction)
	{
		return Stamina >= StaminaCost(MaxStamina, CostFraction) - UE_KINDA_SMALL_NUMBER;
	}

	/** A thrust may start over the gesture playing: none, the inspection, or the reload when thrusts cut it (the ball
	 *  stays in the pouch); never over the draw, a shot, another thrust or the throw of an empty weapon */
	inline bool MayThrust(EFirearmGesture Playing, EFirearmGesture ReloadGesture, bool bThrustCutsReload)
	{
		return Playing == EFirearmGesture::None || Playing == EFirearmGesture::Inspect
			|| (bThrustCutsReload && ReloadGesture != EFirearmGesture::None && Playing == ReloadGesture);
	}

	/** Lunge of a thrust: its own distance, stopped BlockDistance short of the nearest enemy ahead (NearestAhead < 0: none) */
	inline float LungeDistance(float Distance, float NearestAhead, float BlockDistance)
	{
		const float Base = FMath::Max(Distance, 0.0f);
		return NearestAhead < 0.0f ? Base : FMath::Clamp(NearestAhead - FMath::Max(BlockDistance, 0.0f), 0.0f, Base);
	}
}
