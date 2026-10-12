// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Creatures/GiantCrabTypes.h"

/** Pure decisions of the giant crab (T-211, boss T-261), kept apart for the specs (monke_island.Creatures.GiantCrab) */
namespace GiantCrabMath
{
	/** Ground distance between the edges of two round bodies (cm, never below 0) */
	inline float EdgeDistance(const FVector& A, float RadiusA, const FVector& B, float RadiusB)
	{
		return FMath::Max(0.0f, static_cast<float>(FVector::Dist2D(A, B)) - RadiusA - RadiusB);
	}

	/** Signed angle (deg) from the facing Forward to the direction ToTarget on the ground: > 0 to the right (UE: +Y is
	 *  right of +X seen from above, Z up) */
	inline float SignedAngle(const FVector& Forward, const FVector& ToTarget)
	{
		const FVector2D F = FVector2D(Forward).GetSafeNormal();
		const FVector2D T = FVector2D(ToTarget).GetSafeNormal();
		if (F.IsNearlyZero() || T.IsNearlyZero())
		{
			return 0.0f;
		}
		return FMath::RadiansToDegrees(FMath::Atan2(F.X * T.Y - F.Y * T.X, F.X * T.X + F.Y * T.Y));
	}

	/** Turn (deg, > 0 to the right) that puts a target at the nearest flank of the crab: a crab runs sideways */
	inline float SideTurn(const FVector& Forward, const FVector& ToTarget)
	{
		const float Angle = SignedAngle(Forward, ToTarget);
		return Angle >= 0.0f ? Angle - 90.0f : Angle + 90.0f;
	}

	/** True when Direction (from the crab) lies within HalfAngle of its front */
	inline bool IsInFront(const FVector& Forward, const FVector& Direction, float HalfAngleDegrees)
	{
		return FMath::Abs(SignedAngle(Forward, Direction)) <= HalfAngleDegrees;
	}

	/** Index of the candidate to hunt among Distances (cm; < 0 = not a candidate), keeping Current unless another one
	 *  is SwitchMargin closer; INDEX_NONE when there is none */
	inline int32 PickTarget(const TArray<float>& Distances, int32 Current, float SwitchMargin)
	{
		int32 Best = INDEX_NONE;
		for (int32 Index = 0; Index < Distances.Num(); ++Index)
		{
			if (Distances[Index] >= 0.0f && (Best == INDEX_NONE || Distances[Index] < Distances[Best]))
			{
				Best = Index;
			}
		}
		if (Distances.IsValidIndex(Current) && Distances[Current] >= 0.0f && Best != INDEX_NONE
			&& Distances[Best] > Distances[Current] - SwitchMargin)
		{
			return Current;
		}
		return Best;
	}

	/** Blow to strike with (index into Reaches / Weights), among those whose reach covers EdgeDist, drawn with Roll in
	 *  [0, 1); INDEX_NONE when none reaches */
	inline int32 PickAttack(const TArray<float>& Reaches, const TArray<float>& Weights, float EdgeDist, float Roll)
	{
		float Total = 0.0f;
		for (int32 Index = 0; Index < Reaches.Num() && Index < Weights.Num(); ++Index)
		{
			Total += EdgeDist <= Reaches[Index] ? FMath::Max(0.0f, Weights[Index]) : 0.0f;
		}
		if (Total <= 0.0f)
		{
			return INDEX_NONE;
		}
		float Pick = FMath::Clamp(Roll, 0.0f, 0.9999f) * Total;
		for (int32 Index = 0; Index < Reaches.Num() && Index < Weights.Num(); ++Index)
		{
			const float Weight = EdgeDist <= Reaches[Index] ? FMath::Max(0.0f, Weights[Index]) : 0.0f;
			if (Pick < Weight)
			{
				return Index;
			}
			Pick -= Weight;
		}
		return INDEX_NONE;
	}

	/** Records a weak point hit at Now and tells whether FlipHits of them fall within Window (the record is kept) */
	inline bool RecordWeakHit(TArray<double>& HitTimes, double Now, float Window, int32 FlipHits)
	{
		HitTimes.Add(Now);
		HitTimes.RemoveAll([Now, Window](double Time) { return Now - Time > Window; });
		return HitTimes.Num() >= FMath::Max(1, FlipHits);
	}

	/** Ground direction the crab walks in: toward its target until EdgeDist reaches Reach * 0.85, while circling it
	 *  sideways by CircleShare (sign CircleSign); zero when it is there */
	inline FVector WalkDirection(const FVector& ToTarget, float EdgeDist, float Reach, float CircleShare, float CircleSign)
	{
		const FVector Toward = FVector(ToTarget.X, ToTarget.Y, 0.0f).GetSafeNormal();
		if (Toward.IsNearlyZero())
		{
			return FVector::ZeroVector;
		}
		const FVector Side = FVector(-Toward.Y, Toward.X, 0.0f) * CircleSign;
		const float Close = EdgeDist > Reach * 0.85f ? 1.0f : 0.0f;
		return (Toward * Close + Side * CircleShare).GetClampedToMaxSize(1.0f);
	}

	/** Pull of a blow on the struck one: back along From -> To on the ground at Speed, plus Up (cm/s) */
	inline FVector Knockback(const FVector& From, const FVector& To, float Speed, float Up)
	{
		return FVector(To.X - From.X, To.Y - From.Y, 0.0f).GetSafeNormal() * Speed + FVector(0.0f, 0.0f, Up);
	}

	/** Velocity of the throw after a grab (cm/s): over the crab's left claw, local (0.6, -0.8, 0) of CrabRotation kept on
	 *  the ground, raised by AngleDegrees; its length is Speed */
	inline FVector ThrowVelocity(const FRotator& CrabRotation, float Speed, float AngleDegrees)
	{
		const FVector Side = CrabRotation.RotateVector(FVector(0.6f, -0.8f, 0.0f)).GetSafeNormal2D();
		const float Pitch = FMath::DegreesToRadians(AngleDegrees);
		return Side * (Speed * FMath::Cos(Pitch)) + FVector(0.0f, 0.0f, Speed * FMath::Sin(Pitch));
	}

	// ---------------------------------------------------------------- boss (T-261)

	/** Phase of a health fraction: the deepest phase whose threshold the fraction has reached. Thresholds are the
	 *  health fractions where the phases begin, in falling order; the first one applies from full health whatever its
	 *  value. 0 without thresholds */
	inline int32 PhaseForHealth(float HealthFraction, TConstArrayView<float> Thresholds)
	{
		int32 Phase = 0;
		for (int32 Index = 1; Index < Thresholds.Num(); ++Index)
		{
			if (HealthFraction <= Thresholds[Index] + UE_KINDA_SMALL_NUMBER)
			{
				Phase = Index;
			}
		}
		return Phase;
	}

	/** Phases never go back (a heal does not calm it down): the deeper of the current one and the one of the health */
	inline int32 NextPhase(int32 Current, int32 ForHealth)
	{
		return FMath::Max(Current, ForHealth);
	}

	/** Point on the ground in the frame of a crab at Location turned to YawDegrees: X ahead of it, Y to its right (cm) */
	inline FVector2D ToLocal2D(const FVector& Location, float YawDegrees, const FVector& Point)
	{
		const FVector Local = FRotator(0.0f, YawDegrees, 0.0f).UnrotateVector(Point - Location);
		return FVector2D(Local.X, Local.Y);
	}

	/** True when Feet stand under the shell of a crab whose ground point (under its centre) is CrabGround, turned to
	 *  CrabYaw: inside the ellipse of Radii (X ahead, Y aside, cm) around Center (crab frame), and no farther than
	 *  MaxHeight above or below its ground (a player on its back is not under it) */
	inline bool IsUnderShell(const FVector& CrabGround, float CrabYaw, const FVector& Feet, const FVector2D& Center, const FVector2D& Radii, float MaxHeight)
	{
		if (Radii.X <= 0.0f || Radii.Y <= 0.0f || FMath::Abs(Feet.Z - CrabGround.Z) > MaxHeight)
		{
			return false;
		}
		const FVector2D Local = ToLocal2D(CrabGround, CrabYaw, Feet) - Center;
		return FMath::Square(Local.X / Radii.X) + FMath::Square(Local.Y / Radii.Y) <= 1.0f;
	}

	/** Seconds a prey has stood under the shell: grows while it is there, back to 0 as soon as it leaves */
	inline float UnderShellTime(float Time, bool bUnder, float DeltaSeconds)
	{
		return bUnder ? Time + FMath::Max(0.0f, DeltaSeconds) : 0.0f;
	}

	/** Zone of a bone of SK_GiantCrab: eye_l / eye_stalk_r (eye), mouth / mandible_l (mouth), abdomen (belly),
	 *  claw_*_forearm (joint), other claw_* (claw), a name holding LegToken (leg); the shell for any other bone (body,
	 *  root, antennae) and for none */
	inline EGiantCrabZone ZoneOfBone(FName Bone, const FString& LegToken = TEXT("leg"))
	{
		if (Bone.IsNone())
		{
			return EGiantCrabZone::Shell;
		}
		const FString Name = Bone.ToString().ToLower();
		if (Name.Contains(TEXT("eye")))
		{
			return EGiantCrabZone::Eye;
		}
		if (Name.Contains(TEXT("mouth")) || Name.Contains(TEXT("mandible")))
		{
			return EGiantCrabZone::Mouth;
		}
		if (Name.Contains(TEXT("abdomen")) || Name.Contains(TEXT("belly")))
		{
			return EGiantCrabZone::Belly;
		}
		if (Name.Contains(TEXT("claw")))
		{
			return Name.Contains(TEXT("forearm")) ? EGiantCrabZone::Joint : EGiantCrabZone::Claw;
		}
		if (!LegToken.IsEmpty() && Name.Contains(LegToken.ToLower()))
		{
			return EGiantCrabZone::Leg;
		}
		return EGiantCrabZone::Shell;
	}

	/** True for the zones whose multiplier comes from the weak points of the rules (eyes, joints, belly, mouth) */
	inline bool IsWeakZone(EGiantCrabZone Zone)
	{
		return Zone == EGiantCrabZone::Eye || Zone == EGiantCrabZone::Joint || Zone == EGiantCrabZone::Belly || Zone == EGiantCrabZone::Mouth;
	}

	/** Place under the root (the capsule) of a shape whose rest place is RestInMesh in the mesh's space, the mesh being
	 *  at MeshInRoot under the root (T-305: the fixed shapes that stop the pawns) */
	inline FTransform RestInRoot(const FTransform& RestInMesh, const FTransform& MeshInRoot)
	{
		return RestInMesh * MeshInRoot;
	}

	// ---------------------------------------------------------------- the characters around it (T-328)

	/** Place under the root of an upright pillar standing at Center on the mesh's ground plane at rest (cm), from Bottom
	 *  to Top above the mesh's sand, the mesh at MeshInRoot under the root; OutHalfHeight: half its height */
	inline FVector PillarInRoot(const FVector2D& Center, float Bottom, float Top, const FTransform& MeshInRoot, float& OutHalfHeight)
	{
		const float Low = FMath::Min(Bottom, Top);
		const float High = FMath::Max(Bottom, Top);
		OutHalfHeight = 0.5f * (High - Low);
		return RestInRoot(FTransform(FVector(Center.X, Center.Y, 0.5f * (Low + High))), MeshInRoot).GetTranslation();
	}

	/** Seconds a character has been pinned in the crab's shapes: grows while it is pressed in at least MinDepth deep (cm),
	 *  back to 0 as soon as it is out; a mere touch is no pin */
	inline float PinnedTime(float Time, float Depth, float MinDepth, float DeltaSeconds)
	{
		return Depth >= FMath::Max(MinDepth, UE_KINDA_SMALL_NUMBER) ? FMath::Max(0.0f, Time) + FMath::Max(0.0f, DeltaSeconds) : 0.0f;
	}

	/** Angle (deg) of the Try-th spot tried around the crab to free a character: straight away from it (0), then turned
	 *  by Step each side in turn (+Step, -Step, +2 Step, -2 Step...) */
	inline float FreeSpotAngle(int32 Try, float StepDegrees)
	{
		if (Try <= 0)
		{
			return 0.0f;
		}
		const int32 Turns = (Try + 1) / 2;
		return (Try % 2 == 1 ? 1.0f : -1.0f) * static_cast<float>(Turns) * FMath::Clamp(StepDegrees, 1.0f, 180.0f);
	}

	/** Spots tried all the way round with turns of StepDegrees, each angle once (half a turn counted once) */
	inline int32 FreeSpotTries(float StepDegrees)
	{
		const float Step = FMath::Clamp(StepDegrees, 1.0f, 180.0f);
		const int32 Turns = FMath::FloorToInt(180.0f / Step + 0.001f);
		return FMath::IsNearlyEqual(static_cast<float>(Turns) * Step, 180.0f, 0.01f) ? 2 * Turns : 2 * Turns + 1;
	}

	/** How far from the crab's centre a freed character goes: Clear (out of its footprint), and always a little farther
	 *  than it already is (Current + MinStep), never toward the crab */
	inline float FreeSpotDistance(float Current, float Clear, float MinStep)
	{
		return FMath::Max(Clear, FMath::Max(0.0f, Current) + FMath::Max(0.0f, MinStep));
	}

	/** Spot on the ground plane Distance from the crab's centre, along the direction from the centre through the character
	 *  turned by AngleDegrees (> 0 to the right seen from above); along Fallback (its back) when the character stands on
	 *  its centre */
	inline FVector2D FreeSpot(const FVector2D& CrabCenter, const FVector2D& Character, const FVector2D& Fallback, float Distance, float AngleDegrees)
	{
		FVector2D Away = Character - CrabCenter;
		if (Away.SizeSquared() < 1.0)
		{
			Away = Fallback;
		}
		Away = Away.GetSafeNormal();
		if (Away.IsNearlyZero())
		{
			Away = FVector2D(1.0, 0.0);
		}
		return CrabCenter + Away.GetRotated(AngleDegrees) * FMath::Max(0.0f, Distance);
	}

	/** True when a push out of a shape along Away (its MTD, unit) is level enough to slide the character out on the
	 *  ground: its level part is at least LevelShare of it (a pillar pushes level, a body above pushes down) */
	inline bool IsLevelPush(const FVector& Away, float LevelShare)
	{
		return Away.Size2D() >= FMath::Clamp(LevelShare, 0.0f, 1.0f) && !Away.IsNearlyZero();
	}

	/** True for the zones that move fast at the end of the limbs (legs, claws) */
	inline bool IsLimbZone(EGiantCrabZone Zone)
	{
		return Zone == EGiantCrabZone::Leg || Zone == EGiantCrabZone::Claw;
	}

	/** Damage a blow does: at most Cap (0 = no cap), then the multiplier of the part struck (ZoneScale) and the
	 *  resistance of the phase (PhaseTakenScale); 0 when it is invulnerable or the blow is not positive */
	inline float IncomingDamage(float Damage, float Cap, float ZoneScale, float PhaseTakenScale, bool bInvulnerable)
	{
		if (bInvulnerable || !(Damage > 0.0f))
		{
			return 0.0f;
		}
		const float Base = Cap > 0.0f ? FMath::Min(Damage, Cap) : Damage;
		return Base * FMath::Max(0.0f, ZoneScale) * FMath::Max(0.0f, PhaseTakenScale);
	}

	/** Play rate of a walk clip for a ground speed: Speed over the speed of the clip at rate 1, within [MinRate, MaxRate] */
	inline float LocomotionRate(float Speed, float ClipSpeed, float MinRate, float MaxRate)
	{
		if (ClipSpeed <= 0.0f)
		{
			return 1.0f;
		}
		return FMath::Clamp(Speed / ClipSpeed, MinRate, FMath::Max(MinRate, MaxRate));
	}

	/** A foot touches the sand, from the height of its tip above its lowest point (cm): armed once lifted to Lift,
	 *  a step when it comes back down to Contact; true on the step */
	inline bool FootStep(float Height, float Lift, float Contact, bool& bInOutLifted)
	{
		if (Height >= Lift)
		{
			bInOutLifted = true;
			return false;
		}
		if (bInOutLifted && Height <= Contact)
		{
			bInOutLifted = false;
			return true;
		}
		return false;
	}

	/** Indices of the Times (s into a clip of Length) the clip passes when it moves from Previous to Current: in
	 *  (Previous, Current], across the end of a looping clip when Current < Previous; the clip start counts once the
	 *  clip loops back to it */
	inline void CrossedTimes(TConstArrayView<float> Times, float Previous, float Current, float Length, bool bLoop, TArray<int32>& OutIndices)
	{
		OutIndices.Reset();
		const bool bWrapped = bLoop && Current < Previous;
		for (int32 Index = 0; Index < Times.Num(); ++Index)
		{
			const float Time = Times[Index];
			const bool bHit = bWrapped ? (Time > Previous && Time <= Length) || Time <= Current : Time > Previous && Time <= Current;
			if (bHit)
			{
				OutIndices.Add(Index);
			}
		}
	}

	/** Strength of a camera shake at Distance from its source: 1 within Inner, 0 from Outer, linear between */
	inline float ShakeFalloff(float Distance, float Inner, float Outer)
	{
		if (Distance <= Inner)
		{
			return 1.0f;
		}
		if (Distance >= Outer || Outer <= Inner)
		{
			return 0.0f;
		}
		return 1.0f - (Distance - Inner) / (Outer - Inner);
	}

	/** Where a charge ends: from Start through Target, Overshoot past it, no farther than MaxDistance from Start
	 *  (ground plane, Start's height); Start when Target is on it */
	inline FVector ChargeEnd(const FVector& Start, const FVector& Target, float Overshoot, float MaxDistance)
	{
		const FVector Ground = FVector(Target.X - Start.X, Target.Y - Start.Y, 0.0f);
		const float Distance = static_cast<float>(Ground.Size());
		if (Distance < UE_KINDA_SMALL_NUMBER)
		{
			return Start;
		}
		const float Run = FMath::Min(Distance + FMath::Max(0.0f, Overshoot), FMath::Max(0.0f, MaxDistance));
		return Start + Ground / Distance * Run;
	}
}
