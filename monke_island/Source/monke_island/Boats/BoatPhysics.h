// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Pure boat physics formulas, shared by ABoatActor and the specs (monke_island.Boats.*).
 *  Unreal units: cm, kg, cm/s; GravityCm = 980 cm/s^2.
 */
namespace BoatPhysics
{
	/** Gravity (cm/s^2), real world 9.8 m/s^2 */
	constexpr float GravityCm = 980.0f;

	/** One newton in Unreal force units (kg.cm/s^2) */
	constexpr float NewtonToUnreal = 100.0f;

	/** One newton-metre in Unreal torque units (kg.cm^2/s^2) */
	constexpr float NewtonMetreToUnreal = 10000.0f;

	/** Upward force of one of PointCount buoyancy points immersed by DepthCm (Unreal force units) */
	inline float PointBuoyancy(float DepthCm, float HullDepthCm, float FloatCapacityKg, int32 PointCount)
	{
		if (DepthCm <= 0.0f || HullDepthCm <= 0.0f || PointCount <= 0)
		{
			return 0.0f;
		}
		const float Fraction = FMath::Clamp(DepthCm / HullDepthCm, 0.0f, 1.0f);
		return FloatCapacityKg * GravityCm * Fraction / static_cast<float>(PointCount);
	}

	/** Immersion of the keel (cm) at which a level hull of TotalMassKg floats */
	inline float EquilibriumImmersion(float TotalMassKg, float HullDepthCm, float FloatCapacityKg)
	{
		if (FloatCapacityKg <= 0.0f)
		{
			return HullDepthCm;
		}
		return FMath::Min(HullDepthCm, HullDepthCm * TotalMassKg / FloatCapacityKg);
	}

	/** Load aboard (kg): players at PersonMassKg each plus the objects */
	inline float LoadKg(int32 PersonCount, float PersonMassKg, float ObjectsKg)
	{
		return FMath::Max(0, PersonCount) * PersonMassKg + FMath::Max(0.0f, ObjectsKg);
	}

	/** Change of the water aboard per second (kg/s): floods while overloaded, drains otherwise */
	inline float BilgeRate(float LoadKg, float MaxLoadKg, float FloodRate, float FloodPerKgOver, float DrainRate, bool bGunwaleUnder, float SwampRate)
	{
		const float Over = LoadKg - MaxLoadKg;
		float Rate = Over > 0.0f ? FloodRate + FloodPerKgOver * Over : -DrainRate;
		if (bGunwaleUnder)
		{
			Rate = FMath::Max(Rate, 0.0f) + SwampRate;
		}
		return Rate;
	}

	/** Stroke force envelope in [0, 1] at phase [0, 1): a smooth pull during the drive, nothing during the recovery */
	inline float StrokeEnvelope(float Phase, float DriveFraction)
	{
		const float P = Phase - FMath::FloorToFloat(Phase);
		if (DriveFraction <= 0.0f || P >= DriveFraction)
		{
			return 0.0f;
		}
		const float S = FMath::Sin(PI * P / DriveFraction);
		return S * S;
	}

	/**
	 *  Transform of a seated rider relative to its seat socket: upright in the hull frame, facing the bow,
	 *  capsule centre CapsuleOffsetCm above the seat, whatever the socket rotation (the FBX sockets come in Z down).
	 */
	inline FTransform SeatedRiderRelative(const FQuat& SocketInHull, float CapsuleOffsetCm)
	{
		return FTransform(SocketInHull.Inverse(), SocketInHull.UnrotateVector(FVector(0.0, 0.0, CapsuleOffsetCm)));
	}

	/** Rotation relative to its oarlock socket that gives an oar the rotation TargetInHull in the hull frame */
	inline FQuat OarRelativeRotation(const FQuat& SocketInHull, const FRotator& TargetInHull)
	{
		return SocketInHull.Inverse() * TargetInHull.Quaternion();
	}

	/**
	 *  A player walking into the hull pushes it: walking (Accel, horizontal) toward the hull point (ToHull, horizontal)
	 *  closer than Reach to the capsule surface. Returns the push direction (unit, horizontal) or zero.
	 */
	inline FVector PushDirection(const FVector& Accel, const FVector& ToHull, float GapCm, float Reach, float MinAlignment)
	{
		const FVector Walk = FVector(Accel.X, Accel.Y, 0.0).GetSafeNormal();
		const FVector Toward = FVector(ToHull.X, ToHull.Y, 0.0).GetSafeNormal();
		if (Walk.IsZero() || GapCm > Reach)
		{
			return FVector::ZeroVector;
		}
		// already touching (gap 0): the hull direction is unreliable, trust the walk
		if (Toward.IsZero() || (Walk | Toward) >= MinAlignment)
		{
			return Walk;
		}
		return FVector::ZeroVector;
	}

	/** Seconds a boat has been stuck: grows while rowing with the keel on the ground without moving, else resets */
	inline float StuckTime(float Current, bool bRowing, bool bGrounded, float Speed, float StuckSpeed, float DeltaSeconds)
	{
		return (bRowing && bGrounded && Speed < StuckSpeed) ? Current + DeltaSeconds : 0.0f;
	}

	/** Index of the deepest probe (the way out), or INDEX_NONE when none is deeper than Here + 5 cm */
	inline int32 DeepestProbe(const TArray<float>& Depths, float Here)
	{
		int32 Best = INDEX_NONE;
		float BestDepth = Here + 5.0f;
		for (int32 Index = 0; Index < Depths.Num(); ++Index)
		{
			if (Depths[Index] > BestDepth)
			{
				BestDepth = Depths[Index];
				Best = Index;
			}
		}
		return Best;
	}

	/** Water drag multiplier of a boat carrying LoadKg: 1 empty, FullLoadScale at MaxLoadKg (and beyond, overloaded) */
	inline float LoadDragScale(float LoadKg, float MaxLoadKg, float FullLoadScale)
	{
		const float Fill = MaxLoadKg > 0.0f ? FMath::Clamp(LoadKg / MaxLoadKg, 0.0f, 1.5f) : 0.0f;
		return 1.0f + (FMath::Max(1.0f, FullLoadScale) - 1.0f) * Fill;
	}

	/** Drive of the left and right oars in [-1, 1] from the rowing input (X turn right, Y forward) */
	inline FVector2D OarDrives(const FVector2D& Input)
	{
		const float Forward = FMath::Clamp(Input.Y, -1.0f, 1.0f);
		const float Turn = FMath::Clamp(Input.X, -1.0f, 1.0f);
		return FVector2D(FMath::Clamp(Forward + Turn, -1.0f, 1.0f), FMath::Clamp(Forward - Turn, -1.0f, 1.0f));
	}
}
