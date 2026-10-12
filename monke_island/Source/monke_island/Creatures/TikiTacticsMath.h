// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/TikiTacticsTypes.h"

/** Pure rules of the tactics of the tiki warriors (T-275), tested without a world by monke_island.Tiki.Tactics.* */
namespace TikiTactics
{
	/** Role of a warrior class: the first keyword its name contains (case insensitive); Brawler when none matches */
	inline ETikiArchetype ResolveArchetype(const FString& ClassName, const TArray<FTikiArchetypeMatch>& Matches)
	{
		for (const FTikiArchetypeMatch& Match : Matches)
		{
			if (!Match.Keyword.IsEmpty() && Match.Archetype != ETikiArchetype::Auto && ClassName.Contains(Match.Keyword, ESearchCase::IgnoreCase))
			{
				return Match.Archetype;
			}
		}
		return ETikiArchetype::Brawler;
	}

	/** True when Point lies within HalfAngleDeg of the horizontal view of an observer at ViewLocation looking along
	 *  ViewDirection (the foe looks at the stalker); a point on the observer counts as seen */
	inline bool IsInView(const FVector& ViewLocation, const FVector& ViewDirection, const FVector& Point, float HalfAngleDeg)
	{
		const FVector Look = ViewDirection.GetSafeNormal2D();
		const FVector To = (Point - ViewLocation).GetSafeNormal2D();
		if (To.IsNearlyZero() || Look.IsNearlyZero())
		{
			return true;
		}
		return FVector::DotProduct(Look, To) >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDeg, 0.0f, 180.0f)));
	}

	/** Yaw (deg, in [-180, 180)) of the Index-th of Count places around a foe looking along ViewYaw: the first right
	 *  behind it, the others spread evenly from there */
	inline float EncircleSlotYaw(int32 Index, int32 Count, float ViewYaw)
	{
		const int32 Places = FMath::Max(Count, 1);
		const float Yaw = ViewYaw + 180.0f + 360.0f * static_cast<float>(FMath::Clamp(Index, 0, Places - 1)) / static_cast<float>(Places);
		return FRotator::NormalizeAxis(Yaw);
	}

	/** A point of the ring of Radius around Center at Yaw (deg), Z of the centre */
	inline FVector RingPoint(const FVector& Center, float YawDeg, float Radius)
	{
		const float Rad = FMath::DegreesToRadians(YawDeg);
		return Center + FVector(FMath::Cos(Rad), FMath::Sin(Rad), 0.0f) * Radius;
	}

	/** Place of a hunter among those after the same foe: its rank once their ids are sorted, and their count; the
	 *  same order on every think, so the places do not swap */
	inline int32 PackRank(uint32 SelfId, TConstArrayView<uint32> OtherIds)
	{
		int32 Rank = 0;
		for (const uint32 Other : OtherIds)
		{
			Rank += Other < SelfId ? 1 : 0;
		}
		return Rank;
	}

	/** True when one more warrior may commit to a foe that ActiveOthers already attack */
	inline bool HasAttackToken(int32 ActiveOthers, int32 MaxAttackers)
	{
		return ActiveOthers < FMath::Max(MaxAttackers, 1);
	}

	/** True when a wounded warrior with no ally near runs away */
	inline bool ShouldFlee(float HealthFraction, float FleeFraction, int32 AlliesNear)
	{
		return AlliesNear <= 0 && HealthFraction > 0.0f && HealthFraction <= FleeFraction;
	}

	/** Radius a stalker circles its foe at: the farthest while it is looked at, the nearest when not, never inside the
	 *  fear of a lit torch */
	inline float StalkRadius(bool bWatched, float MinRadius, float MaxRadius, float FearRadius)
	{
		const float Lo = FMath::Min(MinRadius, MaxRadius);
		const float Hi = FMath::Max(MinRadius, MaxRadius);
		return FMath::Max(bWatched ? Hi : Lo, FearRadius);
	}

	/** Point a stalker at Self goes to while it circles its foe: on the ring of Radius around the foe, StepDeg further
	 *  around it toward Side (+1: the yaw seen from the foe grows, -1: it decreases) */
	inline FVector OrbitPoint(const FVector& Foe, const FVector& Self, float Radius, float StepDeg, int32 Side)
	{
		FVector From = Self - Foe;
		From.Z = 0.0f;
		const float Yaw = From.IsNearlyZero() ? 0.0f : FMath::RadiansToDegrees(FMath::Atan2(From.Y, From.X));
		FVector Point = RingPoint(Foe, Yaw + StepDeg * static_cast<float>(Side >= 0 ? 1 : -1), Radius);
		Point.Z = Self.Z;
		return Point;
	}

	/** What a blow is to a guard: none (a blast, a burn, a fall), a melee blow, a shot */
	enum class EGuardable : uint8
	{
		None,
		Melee,
		Shot
	};

	/** Kind of a blow from its damage event and causer: a blast is never guarded, a shot is, a point blow is a melee
	 *  blow (sabre, spear, club, torch), and a plain blow only from a pawn (a fist; a burn comes from an item) */
	inline EGuardable ClassifyBlow(bool bPointDamage, bool bRadialDamage, bool bFirearm, bool bCauserIsPawn)
	{
		if (bRadialDamage)
		{
			return EGuardable::None;
		}
		if (bFirearm)
		{
			return EGuardable::Shot;
		}
		if (bPointDamage || bCauserIsPawn)
		{
			return EGuardable::Melee;
		}
		return EGuardable::None;
	}

	/** True when a blow from FromLocation reaches a guard at Location facing Forward within its half arc (deg) */
	inline bool IsInGuardArc(const FVector& Location, const FVector& Forward, const FVector& FromLocation, float HalfArcDeg)
	{
		const FVector To = (FromLocation - Location).GetSafeNormal2D();
		const FVector Facing = Forward.GetSafeNormal2D();
		if (To.IsNearlyZero() || Facing.IsNearlyZero())
		{
			return false;
		}
		return FVector::DotProduct(Facing, To) >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfArcDeg, 0.0f, 180.0f)));
	}

	/** Damage a guard lets through */
	inline float GuardedDamage(float Damage, EGuardable Kind, float MeleeScale, float ShotScale)
	{
		switch (Kind)
		{
		case EGuardable::Melee: return Damage * FMath::Clamp(MeleeScale, 0.0f, 1.0f);
		case EGuardable::Shot: return Damage * FMath::Clamp(ShotScale, 0.0f, 1.0f);
		default: return Damage;
		}
	}

	/** Blows a guard took over a sliding window: it breaks past BreakDamage within the window, or at one blow of
	 *  BreakBlow or more */
	struct FGuardPoise
	{
		/** Blocked damage since WindowStart */
		float Damage = 0.0f;

		/** World time the window started (s) */
		double WindowStart = -1.0e9;

		/** Adds a blocked blow of Blocked damage at Now; true when the guard breaks (the poise is then reset) */
		bool Add(float Blocked, double Now, float Window, float BreakDamage, float BreakBlow)
		{
			if (Now - WindowStart > Window)
			{
				Damage = 0.0f;
				WindowStart = Now;
			}
			Damage += FMath::Max(Blocked, 0.0f);
			if (Blocked >= BreakBlow || Damage >= BreakDamage)
			{
				Reset();
				return true;
			}
			return false;
		}

		void Reset()
		{
			Damage = 0.0f;
			WindowStart = -1.0e9;
		}
	};

	/** Where a bodyguard stands between Ward and Threat: Distance from the ward toward the threat (Z of the ward); on
	 *  the ward when they are at the same place */
	inline FVector InterposePoint(const FVector& Ward, const FVector& Threat, float Distance)
	{
		const FVector Toward = (Threat - Ward).GetSafeNormal2D();
		const float Gap = static_cast<float>(FVector::Dist2D(Threat, Ward));
		return Ward + Toward * FMath::Min(Distance, Gap * 0.5f);
	}

	/** How much an ally needs a bodyguard: 0 when its foe is farther than ThreatRadius from it, or when it is neither a
	 *  watcher (no guard of its own) nor wounded (WoundedFraction of its health or less); else more for a watcher, a
	 *  wounded ally and a closer foe */
	inline float WardUrgency(bool bWatcher, float HealthFraction, float ThreatDistance, float ThreatRadius, float WoundedFraction)
	{
		if (ThreatRadius <= 0.0f || ThreatDistance > ThreatRadius || (!bWatcher && HealthFraction > WoundedFraction))
		{
			return 0.0f;
		}
		const float Closeness = 1.0f - FMath::Clamp(ThreatDistance / ThreatRadius, 0.0f, 1.0f);
		const float Wound = 1.0f - FMath::Clamp(HealthFraction, 0.0f, 1.0f);
		return 0.1f + Closeness + Wound + (bWatcher ? 1.0f : 0.0f);
	}

	/** Direction a crab comes at its foe: toward it, turned by AngleDeg to one Side (+1 toward its right, -1 its left) */
	inline FVector SideApproach(const FVector& ToFoe, int32 Side, float AngleDeg)
	{
		return ToFoe.GetSafeNormal2D().RotateAngleAxis(AngleDeg * static_cast<float>(Side >= 0 ? 1 : -1), FVector::UpVector);
	}

	/** Velocity of a foe thrown by a claw facing Forward: to Side (+1 its right, -1 its left) by SideAngleDeg from its
	 *  front, at Speed, plus UpSpeed */
	inline FVector ThrowVelocity(const FVector& Forward, int32 Side, float SideAngleDeg, float Speed, float UpSpeed)
	{
		// yaw grows clockwise seen from above in Unreal: +angle turns the front toward the right
		const FVector Flat = Forward.GetSafeNormal2D().RotateAngleAxis(SideAngleDeg * static_cast<float>(Side >= 0 ? 1 : -1), FVector::UpVector);
		return Flat * Speed + FVector(0.0f, 0.0f, UpSpeed);
	}

	/** Side a seized foe is thrown to: away from the ally the crab protects (+1 right, -1 left); right without one */
	inline int32 ThrowSide(const FVector& Forward, const FVector& Self, bool bHasWard, const FVector& Ward)
	{
		if (!bHasWard)
		{
			return 1;
		}
		const FVector Right = FVector::CrossProduct(FVector::UpVector, Forward.GetSafeNormal2D());
		return FVector::DotProduct(Ward - Self, Right) > 0.0 ? -1 : 1;
	}

	/** Place behind a bush from a threat: Offset beyond the bush on the line from the threat (Z of the bush) */
	inline FVector CoverPoint(const FVector& Bush, const FVector& Threat, float Offset)
	{
		FVector Away = (Bush - Threat).GetSafeNormal2D();
		if (Away.IsNearlyZero())
		{
			Away = FVector::ForwardVector;
		}
		return Bush + Away * Offset;
	}

	/** Place a watcher wants: on the line from its foe through itself, at its distance clamped to [MinDistance,
	 *  MaxDistance] (Z of Self) */
	inline FVector KeepAwayPoint(const FVector& Self, const FVector& Threat, float MinDistance, float MaxDistance)
	{
		FVector Away = Self - Threat;
		Away.Z = 0.0f;
		const float Distance = static_cast<float>(Away.Size());
		Away = Away.GetSafeNormal(UE_SMALL_NUMBER, FVector::ForwardVector);
		const float Want = FMath::Clamp(Distance, FMath::Min(MinDistance, MaxDistance), FMath::Max(MinDistance, MaxDistance));
		FVector Point = Threat + Away * Want;
		Point.Z = Self.Z;
		return Point;
	}

	/** Aim of a thrown cloud: where the foe will be LeadSeconds ahead at its horizontal speed, the lead at most MaxLead */
	inline FVector LeadAim(const FVector& Foe, const FVector& Velocity, float LeadSeconds, float MaxLead)
	{
		const FVector Flat(Velocity.X, Velocity.Y, 0.0f);
		return Foe + (Flat * FMath::Max(LeadSeconds, 0.0f)).GetClampedToMaxSize(FMath::Max(MaxLead, 0.0f));
	}

	/** Point of a lobbed arc at Alpha in [0, 1]: the straight line from From to To, raised by a parabola whose top is
	 *  ArcHeight at the middle */
	inline FVector ArcPoint(const FVector& From, const FVector& To, float ArcHeight, float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return FMath::Lerp(From, To, T) + FVector(0.0f, 0.0f, 4.0f * ArcHeight * T * (1.0f - T));
	}

	/** Flight time of a lump thrown over Distance at Speed, at least MinSeconds (s) */
	inline float FlightSeconds(float Distance, float Speed, float MinSeconds)
	{
		return FMath::Max(Speed > 1.0f ? Distance / Speed : 0.0f, FMath::Max(MinSeconds, 0.0f));
	}

	/** True when Point is in a cloud or a patch: within Radius of its centre on the ground, and from below its base to
	 *  Height above it */
	inline bool IsInsideBurst(const FVector& Center, float Radius, float Height, const FVector& Point)
	{
		const double Up = Point.Z - Center.Z;
		return FVector::DistSquared2D(Center, Point) <= FMath::Square(static_cast<double>(Radius)) && Up >= -Height * 0.5 && Up <= Height;
	}

	/** Distance a lit torch keeps a stalker at: none unlit, farther raised */
	inline float TorchFearRadius(bool bLit, bool bRaised, float Radius, float RaisedRadius)
	{
		return bLit ? (bRaised ? FMath::Max(Radius, RaisedRadius) : Radius) : 0.0f;
	}

	/** True when a lit torch shows a player that the night would hide: at night, standing, the torch lit, within Range.
	 *  The crouch still hides (T-262) and by day nothing changes */
	inline bool TorchReveals(bool bNight, bool bCrouched, bool bTorchLit, float Distance, float Range)
	{
		return bNight && !bCrouched && bTorchLit && Distance <= Range;
	}

	/** True while the fear of the torch holds a stalker back: not yet TorchCourageSeconds held back, and fewer than
	 *  CouragePack stalkers on the foe */
	inline bool IsTorchFearHolding(double HeldSince, double Now, float CourageSeconds, int32 PackCount, int32 CouragePack)
	{
		return PackCount < FMath::Max(CouragePack, 1) && (HeldSince < 0.0 || Now - HeldSince < CourageSeconds);
	}

	/** Locomotion of a creature facing its foe while it moves, from its velocity in its own frame */
	enum class EStrafeClip : uint8
	{
		Forward,
		Back,
		Left,
		Right
	};

	inline EStrafeClip StrafeClipFor(const FVector& Velocity, const FVector& Forward)
	{
		const FVector Move = Velocity.GetSafeNormal2D();
		const FVector Front = Forward.GetSafeNormal2D();
		if (Move.IsNearlyZero() || Front.IsNearlyZero())
		{
			return EStrafeClip::Forward;
		}
		const float Along = static_cast<float>(FVector::DotProduct(Move, Front));
		const float Side = static_cast<float>(FVector::DotProduct(Move, FVector::CrossProduct(FVector::UpVector, Front)));
		if (Along <= -0.5f)
		{
			return EStrafeClip::Back;
		}
		if (FMath::Abs(Side) >= 0.6f)
		{
			return Side > 0.0f ? EStrafeClip::Right : EStrafeClip::Left;
		}
		return EStrafeClip::Forward;
	}

	/** True when a tactics state moves the warrior facing its foe (side steps, walk backward) */
	inline bool IsStrafeState(ETikiTacticState State)
	{
		switch (State)
		{
		case ETikiTacticState::Stalk:
		case ETikiTacticState::Encircle:
		case ETikiTacticState::Guard:
		case ETikiTacticState::Protect:
		case ETikiTacticState::Keep:
		case ETikiTacticState::Retreat:
			return true;
		default:
			return false;
		}
	}
}
