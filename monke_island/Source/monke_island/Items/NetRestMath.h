// monke_island

#pragma once

#include "CoreMinimal.h"

/** What the network of a free physics object does now (UNetRestComponent, T-312 / T-294) */
enum class ENetRestState : uint8
{
	/** It replicates at its awake rate: it moves, it is held, or something else handles it */
	Awake,
	/** It does not replicate (dormant) until it changes: asleep, or glued to what carries it */
	Dormant
};

/** Pure rules of the network rest of free physics objects, tested by monke_island.Items.NetRest */
namespace NetRestMath
{
	/**
	 *  Network state of a free object. Not simulating: someone else moves it (a crate holds it, a villager carries it, a
	 *  corpse is being laid down) and it stays awake so their changes reach the clients; held by a player: awake; asleep:
	 *  dormant (when allowed). Glued to a moving base: awake, its movement off and its rate low (AwakeRate). It costs no
	 *  bytes there either, and a dormant object is never culled nor sent again: a client that dropped the base and made
	 *  it again (the barque out of relevancy and back), or got the object before its base, would keep it glued to nothing
	 */
	inline ENetRestState Decide(bool bSimulating, bool bHeld, bool bGlued, bool bAwakeBody, bool bDormantAtRest)
	{
		if (!bSimulating || bHeld || bGlued || !bDormantAtRest)
		{
			return ENetRestState::Awake;
		}
		return bAwakeBody ? ENetRestState::Awake : ENetRestState::Dormant;
	}

	/** Network updates per second of a free object while awake: held by a player, glued to its base (its movement is
	 *  not sent, the server only looks for a change), or moving */
	inline float AwakeRate(bool bHeld, bool bGlued, float MovingRate, float HeldRate, float GluedRate)
	{
		return bHeld ? HeldRate : (bGlued ? GluedRate : MovingRate);
	}

	/** True when an object kept its place on its base between two looks: it moved less than MaxDistance (cm) and turned
	 *  less than MaxAngleDeg relative to it */
	inline bool IsStillOnBase(const FTransform& PreviousLocal, const FTransform& Local, float MaxDistance, float MaxAngleDeg)
	{
		if (FVector::DistSquared(PreviousLocal.GetLocation(), Local.GetLocation()) > FMath::Square(static_cast<double>(FMath::Max(MaxDistance, 0.0f))))
		{
			return false;
		}
		const double Angle = FMath::RadiansToDegrees(PreviousLocal.GetRotation().AngularDistance(Local.GetRotation()));
		return Angle <= FMath::Max(MaxAngleDeg, 0.0f);
	}

	/** Time an object has stayed still on the same base: grows by Interval while still, back to 0 otherwise */
	inline float AccumulateRest(float RestSeconds, bool bStill, float Interval)
	{
		return bStill ? RestSeconds + FMath::Max(Interval, 0.0f) : 0.0f;
	}

	/** True when an object still on a base that moves under it (it never falls asleep there) is glued to it: it rested
	 *  GlueDelay at least (0: never glued) */
	inline bool ShouldGlue(float RestSeconds, float GlueDelay, bool bBaseMoved)
	{
		return GlueDelay > 0.0f && bBaseMoved && RestSeconds >= GlueDelay;
	}

	/** True when a base moved between two looks (cm, degrees): a still base lets its objects fall asleep instead */
	inline bool HasBaseMoved(const FTransform& Previous, const FTransform& Current, float MinDistance, float MinAngleDeg)
	{
		return FVector::DistSquared(Previous.GetLocation(), Current.GetLocation()) > FMath::Square(static_cast<double>(FMath::Max(MinDistance, 0.0f)))
			|| FMath::RadiansToDegrees(Previous.GetRotation().AngularDistance(Current.GetRotation())) > FMath::Max(MinAngleDeg, 0.0f);
	}

	/** Velocity of a point glued to a moving base, from where it was a moment ago (cm/s); none without time */
	inline FVector FollowVelocity(const FVector& Previous, const FVector& Current, float DeltaSeconds)
	{
		return DeltaSeconds > KINDA_SMALL_NUMBER ? (Current - Previous) / DeltaSeconds : FVector::ZeroVector;
	}

	/** Angular velocity of a body glued to a moving base, from its rotation a moment ago (rad/s); none without time */
	inline FVector FollowAngularVelocity(const FQuat& Previous, const FQuat& Current, float DeltaSeconds)
	{
		if (DeltaSeconds <= KINDA_SMALL_NUMBER)
		{
			return FVector::ZeroVector;
		}
		FQuat Delta = Current * Previous.Inverse();
		Delta.EnforceShortestArcWith(FQuat::Identity);
		FVector Axis;
		float Angle = 0.0f;
		Delta.ToAxisAndAngle(Axis, Angle);
		return FMath::IsNearlyZero(Angle) ? FVector::ZeroVector : Axis * (Angle / DeltaSeconds);
	}
}
