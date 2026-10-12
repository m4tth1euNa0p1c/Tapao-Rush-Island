// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Pure rules of the body of a dead fort soldier at rest (T-323; tested by monke_island.Fort.Corpse), no world: when its
 *  skeleton may stop animating on a machine. Each machine decides alone (a client sees the death a little later, a
 *  player joining late plays the death clip again); the network keeps its own rules (FortSoldierNetMath.h).
 */
namespace FortSoldierCorpse
{
	/** Ticks at rest a body needs at least, whatever the delay asked: its skeleton refreshed once more after the last
	 *  change (a bone hidden, a new detail level), whatever the order of the ticks of a frame */
	constexpr int32 MinRestTicks = 2;

	/** What the body of a dead soldier does on this machine this frame */
	struct FRestFacts
	{
		/** Dead on this machine */
		bool bDead = false;

		/** Its death clip played to its end and fully blended in (or it has none), the upper body layer faded out */
		bool bClipDone = false;

		/** Falling (killed in the air, let go) */
		bool bFalling = false;

		/** Attached to another actor */
		bool bCarried = false;

		/** Speed of the body (cm/s) */
		float Speed = 0.0f;

		/** Its ragdoll simulates with a body awake */
		bool bRagdollAwake = false;

		/** A look of its death still to come (a timer), the smoke or the flame of its last shot */
		bool bLookPending = false;
	};

	/** Whether the death clip is over: past its length and fully blended in (nothing to play without one), the upper
	 *  body layer out */
	inline bool IsClipDone(bool bHasClip, float ClipTime, float ClipLength, float ClipWeight, float UpperWeight)
	{
		const bool bUpperOut = FMath::IsFinite(UpperWeight) && UpperWeight <= UE_KINDA_SMALL_NUMBER;
		if (!bHasClip)
		{
			return bUpperOut;
		}
		return bUpperOut && FMath::IsFinite(ClipTime) && FMath::IsFinite(ClipLength) && ClipTime >= ClipLength
			&& FMath::IsFinite(ClipWeight) && ClipWeight >= 1.0f - UE_KINDA_SMALL_NUMBER;
	}

	/** Whether nothing of the body changes by itself any more: dead, the last pose of its clip, lying still (StillSpeed
	 *  cm/s at most), its ragdoll asleep, no look to come */
	inline bool IsAtRest(const FRestFacts& Facts, float StillSpeed)
	{
		const float Still = FMath::IsFinite(StillSpeed) ? FMath::Max(0.0f, StillSpeed) : 0.0f;
		return Facts.bDead && Facts.bClipDone && !Facts.bFalling && !Facts.bCarried && !Facts.bRagdollAwake && !Facts.bLookPending
			&& FMath::IsFinite(Facts.Speed) && Facts.Speed <= Still;
	}

	/** Time and ticks a body spent at rest since its death or its last wake */
	struct FRestClock
	{
		float Seconds = 0.0f;
		int32 Ticks = 0;

		/** One tick: the rest grows while the body is at rest, and starts over as soon as it is not */
		void Advance(bool bAtRest, float DeltaSeconds)
		{
			if (!bAtRest)
			{
				Reset();
				return;
			}
			Seconds += FMath::IsFinite(DeltaSeconds) ? FMath::Max(0.0f, DeltaSeconds) : 0.0f;
			Ticks = FMath::Min(Ticks + 1, MinRestTicks);
		}

		void Reset()
		{
			Seconds = 0.0f;
			Ticks = 0;
		}
	};

	/** Whether the skeleton may stop animating: allowed, at rest for RestSeconds and MinRestTicks at least; never with a
	 *  delay that is not a number (it animates as before) */
	inline bool ShouldFreeze(bool bAllowed, const FRestClock& Rest, float RestSeconds)
	{
		return bAllowed && FMath::IsFinite(RestSeconds) && Rest.Ticks >= MinRestTicks && Rest.Seconds >= FMath::Max(0.0f, RestSeconds);
	}

	/** Seconds between two looks at a frozen body (the detail level the renderer wants for it), within 0.05..5 */
	inline float WatchDelay(float Seconds)
	{
		return FMath::IsFinite(Seconds) ? FMath::Clamp(Seconds, 0.05f, 5.0f) : 0.25f;
	}

	/** Bounds scale of the body (T-332): standing, the blueprint's (StandingScale, 1 when not a positive number); lying
	 *  on the ground (crawling or dead: about a metre along the ground from its feet) LyingScale when it is set (> 0),
	 *  never below the standing one */
	inline float BodyBoundsScale(bool bLying, float StandingScale, float LyingScale)
	{
		const float Standing = FMath::IsFinite(StandingScale) && StandingScale > 0.0f ? StandingScale : 1.0f;
		return bLying && FMath::IsFinite(LyingScale) && LyingScale > 0.0f ? FMath::Max(Standing, LyingScale) : Standing;
	}
}
