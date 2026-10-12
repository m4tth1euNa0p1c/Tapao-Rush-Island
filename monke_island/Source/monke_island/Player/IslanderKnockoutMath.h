// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Pure rules of the knockout of an islander across the network (T-287, review T-268 R-22), kept apart for the specs
 *  (monke_island.Player.Knockout). The server alone knocks out and stands up; every machine plays the ragdoll from the
 *  server's start, and stands it up where the server's body stood up.
 */
namespace IslanderKnockoutMath
{
	/** What a machine does with the knockout state the server sent */
	enum class EAction : uint8
	{
		/** Nothing to change */
		None,
		/** The body becomes a ragdoll here */
		Start,
		/** The body stands up here, where the server stood it up */
		Stand
	};

	/** What a client does when the replicated state changes: a ragdoll when the server's body is down and its own is
	 *  not; standing up only on the server's word, and never for a corpse (dead on the server or here) */
	inline EAction Resolve(bool bServerDown, bool bServerDead, bool bLocalRagdoll, bool bLocalDead)
	{
		if (bServerDown || bServerDead)
		{
			return bLocalRagdoll ? EAction::None : EAction::Start;
		}
		return bLocalRagdoll && !bLocalDead ? EAction::Stand : EAction::None;
	}

	/** Where a body thrown from Start at Velocity is Age seconds later under GravityZ (cm/s2, negative: down); the
	 *  drag of the physics asset is left aside (a few tenths of a second at most) */
	inline FVector BallisticLocation(const FVector& Start, const FVector& Velocity, float GravityZ, float Age)
	{
		const float Time = FMath::Max(0.0f, Age);
		return Start + Velocity * Time + FVector(0.0f, 0.0f, 0.5f * GravityZ * Time * Time);
	}

	/** Its velocity then */
	inline FVector BallisticVelocity(const FVector& Velocity, float GravityZ, float Age)
	{
		return Velocity + FVector(0.0f, 0.0f, GravityZ * FMath::Max(0.0f, Age));
	}

	/** Seconds into its flight a client starts a knockout: its age on the server clock, from 0 (just received, or the
	 *  clock estimate a little behind) up to MaxCatchUp; negative when it is older (a client that joins or sees the
	 *  body late): the body then drops where the capsule is */
	inline float CatchUpAge(double ServerNow, double StartServerTime, float MaxCatchUp)
	{
		const double Age = FMath::Max(0.0, ServerNow - StartServerTime);
		return Age <= static_cast<double>(FMath::Max(0.0f, MaxCatchUp)) ? static_cast<float>(Age) : -1.0f;
	}

	/** Capsule centre that stands for a ragdoll whose pelvis is at Pelvis: where the body would stand up on flat
	 *  ground (the server's hits, the mobs and the relevancy find the body there) */
	inline FVector FollowLocation(const FVector& Pelvis, float HalfHeight)
	{
		return Pelvis + FVector(0.0f, 0.0f, HalfHeight);
	}

	/** The capsule moves to Target only once it is MinStep or more away: a body at rest sends nothing */
	inline bool ShouldFollow(const FVector& Capsule, const FVector& Target, float MinStep)
	{
		return FVector::DistSquared(Capsule, Target) >= FMath::Square(FMath::Max(0.0f, MinStep));
	}

	/** Next serial of a knockout (never 0, which is "none yet") */
	inline uint8 NextSerial(uint8 Serial)
	{
		return Serial == MAX_uint8 ? 1 : static_cast<uint8>(Serial + 1);
	}
}
