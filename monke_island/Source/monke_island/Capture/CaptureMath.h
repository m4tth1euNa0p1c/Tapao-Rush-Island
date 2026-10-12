// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Capture/CaptureTypes.h"

/** Pure rules of the capture (T-336): no world, tested by monke_island.Capture.* */
namespace CaptureMath
{
	// ------------------------------------------------------------------ cuffing

	/** Subdued: at or under the share of health of the rules, or down when that counts */
	inline bool IsSubdued(float HealthFraction, bool bDown, float SubduedFraction, bool bDownCounts)
	{
		if (bDownCounts && bDown)
		{
			return true;
		}
		return FMath::IsFinite(HealthFraction) && HealthFraction <= FMath::Clamp(SubduedFraction, 0.0f, 1.0f) + KINDA_SMALL_NUMBER;
	}

	/** What is known of a cuffing attempt */
	struct FCuffFacts
	{
		/** The player can act (alive, standing on the ground, out of the water) */
		bool bCarrierAble = true;

		/** The handcuffs hold a prisoner already */
		bool bHandcuffsInUse = false;

		/** The player leads a prisoner already */
		bool bLeading = false;

		/** A pawn is aimed at */
		bool bHasTarget = false;

		/** Of a kind that can be cuffed, alive, no player */
		bool bCapturable = false;

		/** Within the reach of the handcuffs and in sight */
		bool bInReach = false;

		float HealthFraction = 1.0f;

		bool bDown = false;

		/** Someone else is cuffing it */
		bool bClaimedByOther = false;
	};

	/** The first reason against the cuffing, None when it may start (or complete) */
	inline ECaptureRefusal JudgeCuff(const FCuffFacts& Facts, float SubduedFraction, bool bDownCounts)
	{
		if (!Facts.bCarrierAble)
		{
			return ECaptureRefusal::CarrierUnable;
		}
		if (Facts.bHandcuffsInUse)
		{
			return ECaptureRefusal::HandcuffsInUse;
		}
		if (Facts.bLeading)
		{
			return ECaptureRefusal::AlreadyLeading;
		}
		if (!Facts.bHasTarget)
		{
			return ECaptureRefusal::NoTarget;
		}
		if (!Facts.bCapturable)
		{
			return ECaptureRefusal::NotCapturable;
		}
		if (!Facts.bInReach)
		{
			return ECaptureRefusal::OutOfReach;
		}
		if (!IsSubdued(Facts.HealthFraction, Facts.bDown, SubduedFraction, bDownCounts))
		{
			return ECaptureRefusal::NotSubdued;
		}
		return Facts.bClaimedByOther ? ECaptureRefusal::Busy : ECaptureRefusal::None;
	}

	/** Angle between the view and the direction to a target (deg, 0 straight ahead); 0 for a target at the eyes */
	inline float AimAngleDeg(const FVector& ViewDirection, const FVector& ToTarget)
	{
		const FVector View = ViewDirection.GetSafeNormal();
		const FVector To = ToTarget.GetSafeNormal();
		if (View.IsNearlyZero() || To.IsNearlyZero())
		{
			return 0.0f;
		}
		return FMath::RadiansToDegrees(FMath::Acos(FMath::Clamp(static_cast<float>(FVector::DotProduct(View, To)), -1.0f, 1.0f)));
	}

	/** Score of a target among several (lower is better): the angle off the view first, the distance then */
	inline float TargetScore(float AimAngle, float Distance, float Reach)
	{
		return AimAngle + 10.0f * FMath::Clamp(Distance / FMath::Max(Reach, 1.0f), 0.0f, 2.0f);
	}

	// ------------------------------------------------------------------ beach

	/** Ground no steeper than MaxSlopeDeg */
	inline bool IsWalkableSlope(float NormalZ, float MaxSlopeDeg)
	{
		const float Limit = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(MaxSlopeDeg, 0.0f, 89.9f)));
		return NormalZ >= Limit - KINDA_SMALL_NUMBER;
	}

	/** Ground within [MinAbove, MaxAbove] above the sea level (cm) */
	inline bool IsBeachHeight(float FloorZ, float SeaLevel, float MinAbove, float MaxAbove)
	{
		const float Height = FloorZ - SeaLevel;
		return FMath::IsFinite(Height) && Height >= MinAbove && Height <= MaxAbove;
	}

	/** A ground at least MinDepth under the sea level: the sea is there */
	inline bool IsUnderSea(float GroundZ, float SeaLevel, float MinDepth)
	{
		return GroundZ <= SeaLevel - FMath::Max(MinDepth, 0.0f);
	}

	/** Number of probes looking for the sea around a spot */
	inline int32 ShoreProbeCount(int32 Directions, int32 Rings)
	{
		return FMath::Max(1, Directions) * FMath::Max(1, Rings);
	}

	/** Horizontal offset of a probe (cm): rings at 1/Rings .. 1 of MaxDistance, the directions of every other ring turned
	 *  half a step so they cover the gaps */
	inline FVector2D ShoreProbeOffset(int32 Index, int32 Directions, int32 Rings, float MaxDistance)
	{
		const int32 Dirs = FMath::Max(1, Directions);
		const int32 RingCount = FMath::Max(1, Rings);
		const int32 Ring = FMath::Clamp(Index / Dirs, 0, RingCount - 1);
		const int32 Dir = Index % Dirs;
		const float Radius = FMath::Max(MaxDistance, 0.0f) * static_cast<float>(Ring + 1) / static_cast<float>(RingCount);
		const float Step = UE_TWO_PI / static_cast<float>(Dirs);
		const float Angle = Step * (static_cast<float>(Dir) + (Ring % 2 == 1 ? 0.5f : 0.0f));
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	}

	/** What is known of a spot for a cage */
	struct FCageSpotFacts
	{
		/** The player can act (alive, standing on the ground, out of the water) */
		bool bCarrierAble = true;

		/** A floor was found under the spot: still (no physics body), not a pawn */
		bool bFloor = false;

		float FloorZ = 0.0f;

		float FloorNormalZ = 1.0f;

		float SeaLevel = 0.0f;

		/** The sea was found around the spot */
		bool bShoreNear = false;

		/** Another cage stands too close */
		bool bCageNear = false;

		/** Nothing stands where the cage would go */
		bool bClear = true;
	};

	/** The first reason against a cage on this spot, None when it may stand there */
	inline ECaptureRefusal JudgeCageSpot(const FCageSpotFacts& Facts, float MinAboveSea, float MaxAboveSea, float MaxSlopeDeg)
	{
		if (!Facts.bCarrierAble)
		{
			return ECaptureRefusal::CarrierUnable;
		}
		if (!Facts.bFloor || !IsBeachHeight(Facts.FloorZ, Facts.SeaLevel, MinAboveSea, MaxAboveSea) || !Facts.bShoreNear)
		{
			return ECaptureRefusal::NotOnBeach;
		}
		if (!IsWalkableSlope(Facts.FloorNormalZ, MaxSlopeDeg))
		{
			return ECaptureRefusal::TooSteep;
		}
		if (Facts.bCageNear)
		{
			return ECaptureRefusal::CageTooClose;
		}
		return Facts.bClear ? ECaptureRefusal::None : ECaptureRefusal::NoRoom;
	}

	// ------------------------------------------------------------------ leading

	/** Pace of a led prisoner (cm/s): still within FollowDistance; beyond, at least the jailer's pace or the walk, up to
	 *  MaxSpeed as the chain gets taut (and at MaxSpeed beyond its length) */
	inline float FollowSpeed(float Distance, float FollowDistance, float ChainLength, float JailerSpeed, float WalkSpeed, float MaxSpeed)
	{
		if (!FMath::IsFinite(Distance) || Distance <= FollowDistance)
		{
			return 0.0f;
		}
		const float Top = FMath::Max(MaxSpeed, 0.0f);
		const float Base = FMath::Min(FMath::Max(WalkSpeed, FMath::IsFinite(JailerSpeed) ? JailerSpeed : 0.0f), Top);
		const float Pull = FMath::Clamp((Distance - FollowDistance) / FMath::Max(ChainLength - FollowDistance, 1.0f), 0.0f, 1.0f);
		return FMath::Lerp(Base, Top, Pull);
	}

	/** What is known of a jailer and its prisoner */
	struct FLeadFacts
	{
		bool bJailerValid = true;

		bool bJailerDead = false;

		bool bJailerSwimming = false;

		/** Between the jailer and the prisoner (cm) */
		float Distance = 0.0f;
	};

	/** Why the prisoner is let go now, None while it stays on the chain */
	inline ECaptiveRelease JudgeLead(const FLeadFacts& Facts, float ReleaseDistance)
	{
		if (!Facts.bJailerValid)
		{
			return ECaptiveRelease::JailerGone;
		}
		if (Facts.bJailerDead)
		{
			return ECaptiveRelease::JailerDead;
		}
		if (Facts.bJailerSwimming)
		{
			return ECaptiveRelease::JailerInWater;
		}
		return !FMath::IsFinite(Facts.Distance) || Facts.Distance > ReleaseDistance ? ECaptiveRelease::TooFar : ECaptiveRelease::None;
	}

	// ------------------------------------------------------------------ chain

	/** Sag of a chain of ChainLength hung over Distance (cm, parabola: length ~ D + 8 s^2 / 3D), at least MinSag */
	inline float ChainSag(float Distance, float ChainLength, float MinSag)
	{
		const float Span = FMath::Max(Distance, 1.0f);
		const float Slack = FMath::Max(ChainLength - Span, 0.0f);
		return FMath::Max(FMath::Max(MinSag, 0.0f), FMath::Sqrt(3.0f * Span * Slack / 8.0f));
	}

	/** Point of the chain at Alpha (0 at A, 1 at B), sagging by Sag in its middle */
	inline FVector ChainPoint(const FVector& A, const FVector& B, float Alpha, float Sag)
	{
		const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return FMath::Lerp(A, B, T) - FVector(0.0, 0.0, 4.0f * Sag * T * (1.0f - T));
	}

	/** Links drawn for a chain over Distance sagging by Sag, one per Pitch along its length, within [2, MaxLinks] */
	inline int32 ChainLinkCount(float Distance, float Sag, float Pitch, int32 MaxLinks)
	{
		const float Span = FMath::Max(Distance, 1.0f);
		const float Length = Span + 8.0f * Sag * Sag / (3.0f * Span);
		return FMath::Clamp(FMath::CeilToInt32(Length / FMath::Max(Pitch, 0.5f)), 2, FMath::Max(MaxLinks, 2));
	}

	// ------------------------------------------------------------------ animation and gestures

	/** Play rate of a locomotion clip: the speed over the speed it does not slide at, within the limits */
	inline float LocomotionRate(float Speed, float NoSlideSpeed, float MinRate, float MaxRate)
	{
		if (NoSlideSpeed <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}
		return FMath::Clamp(FMath::Max(Speed, 0.0f) / NoSlideSpeed, FMath::Min(MinRate, MaxRate), FMath::Max(MinRate, MaxRate));
	}

	/** Rate a clip of ClipSeconds plays at to last GestureSeconds */
	inline float GesturePlayRate(float ClipSeconds, float GestureSeconds)
	{
		return ClipSeconds > KINDA_SMALL_NUMBER && GestureSeconds > KINDA_SMALL_NUMBER ? ClipSeconds / GestureSeconds : 1.0f;
	}

	/** Share of a gesture or a hold done at Now (0 to 1) */
	inline float Progress(double Now, double Start, float Seconds)
	{
		if (Seconds <= KINDA_SMALL_NUMBER)
		{
			return 1.0f;
		}
		return FMath::Clamp(static_cast<float>((Now - Start) / Seconds), 0.0f, 1.0f);
	}

	// ------------------------------------------------------------------ cage

	/** Seconds left before a cage goes (never negative) */
	inline float SecondsLeft(double ExpireTime, double Now)
	{
		return FMath::Max(0.0f, static_cast<float>(ExpireTime - Now));
	}

	/** "1:42": minutes and seconds left, rounded up */
	inline FString ClockText(float Seconds)
	{
		const int32 Whole = FMath::Max(0, FMath::CeilToInt32(FMath::IsFinite(Seconds) ? Seconds : 0.0f));
		return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
	}

	/** Opening of the door (0 closed, 1 open) after DeltaSeconds toward bOpen, a swing taking SwingSeconds */
	inline float DoorStep(float Current, bool bOpen, float DeltaSeconds, float SwingSeconds)
	{
		const float Step = SwingSeconds > KINDA_SMALL_NUMBER ? FMath::Max(DeltaSeconds, 0.0f) / SwingSeconds : 1.0f;
		return FMath::Clamp(Current + (bOpen ? Step : -Step), 0.0f, 1.0f);
	}

	/** Crowns of a sale (never negative) */
	inline int32 SaleAmount(int32 Price)
	{
		return FMath::Max(0, Price);
	}
}
