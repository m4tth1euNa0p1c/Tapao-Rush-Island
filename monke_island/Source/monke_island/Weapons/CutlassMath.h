// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/CutlassRules.h"

/** Pure cutlass logic (no world), shared by the weapon and its tests */
namespace CutlassMath
{
	/** Stamina one use costs */
	inline float StaminaCost(float MaxStamina, float CostFraction)
	{
		return FMath::Max(0.0f, MaxStamina * CostFraction);
	}

	/** The bean has enough stamina for one use (below 11 % it cannot attack) */
	inline bool HasStaminaFor(float Stamina, float MaxStamina, float CostFraction)
	{
		return Stamina >= StaminaCost(MaxStamina, CostFraction) - KINDA_SMALL_NUMBER;
	}

	/** Combo blow after Previous: right cut, backhand cut, thrust, then again; a late click restarts it */
	inline ECutlassMove NextComboMove(ECutlassMove Previous, double SecondsSincePreviousEnd, float ComboWindow)
	{
		if (SecondsSincePreviousEnd > ComboWindow)
		{
			return ECutlassMove::SlashRight;
		}
		switch (Previous)
		{
		case ECutlassMove::SlashRight: return ECutlassMove::SlashLeft;
		case ECutlassMove::SlashLeft: return ECutlassMove::Thrust;
		default: return ECutlassMove::SlashRight;
		}
	}

	/** Damage of a blow with the edge at Sharpness (0-1) */
	inline float BlowDamage(float BaseDamage, float Sharpness, float DullThreshold, float DullMultiplier)
	{
		return BaseDamage * (Sharpness < DullThreshold ? DullMultiplier : 1.0f);
	}

	/** Target at Offset (horizontal, from the bean) is inside the blade's sweep along Forward */
	inline bool IsInSweep(const FVector& Forward, const FVector& Offset, float Reach, float ArcDegrees)
	{
		const FVector Flat(Offset.X, Offset.Y, 0.0f);
		const float Distance = Flat.Size();
		if (Distance > Reach)
		{
			return false;
		}
		if (Distance < 1.0f)
		{
			return true;
		}
		const FVector FlatForward = FVector(Forward.X, Forward.Y, 0.0f).GetSafeNormal();
		const float HalfAngleCos = FMath::Cos(FMath::DegreesToRadians(ArcDegrees * 0.5f));
		return FVector::DotProduct(FlatForward, Flat / Distance) >= HalfAngleCos;
	}

	/** Damage a parry lets through: full when not parrying, hit from behind or out of stamina */
	inline float ParriedDamage(float Damage, bool bParrying, float FacingDot, float FrontDot, float Reduction, bool bHasStamina)
	{
		if (!bParrying || FacingDot < FrontDot || !bHasStamina)
		{
			return Damage;
		}
		return Damage * (1.0f - Reduction);
	}

	/** A blow that cuts a limb off: an attack with bCut of a weapon with an edge (T-161: never a mob's spear or club) */
	inline bool CutsLimbs(bool bEdged, bool bAttackCuts)
	{
		return bEdged && bAttackCuts;
	}

	/** A killing blow shown and heard as a kill (damage number, kill sound): a cut of an edged weapon (the cutlass's
	 *  thrust kills quietly, as before), any killing blow of a weapon without an edge (T-161) */
	inline bool IsKillBlow(bool bKill, bool bEdged, bool bAttackCuts)
	{
		return bKill && (bAttackCuts || !bEdged);
	}

	/** Transform of the weapon mesh: its placement on the grip (UCutlassRules::MeshInGrip), then the grip */
	inline FTransform MeshOnGrip(const FTransform& Grip, const FTransform& MeshInGrip)
	{
		return MeshInGrip * Grip;
	}

	/** The third person mesh is shown: never with the first person one; put away, only on a bean that shows it (the
	 *  cutlass in its scabbard, not a mob's spear or club, T-161) */
	inline bool ShowsThirdPersonMesh(bool bShowFirstPerson, bool bDrawn, bool bShowWhenPutAway)
	{
		return !bShowFirstPerson && (bDrawn || bShowWhenPutAway);
	}

	/** Blends two keys (rotation by the shortest path) */
	inline FCutlassKey BlendKeys(const FCutlassKey& A, const FCutlassKey& B, float Alpha)
	{
		FCutlassKey Out;
		Out.Time = FMath::Lerp(A.Time, B.Time, Alpha);
		Out.GripLocation = FMath::Lerp(A.GripLocation, B.GripLocation, Alpha);
		Out.GripRotation = FQuat::Slerp(A.GripRotation.Quaternion(), B.GripRotation.Quaternion(), Alpha).Rotator();
		Out.ScabbardAlpha = FMath::Lerp(A.ScabbardAlpha, B.ScabbardAlpha, Alpha);
		return Out;
	}

	/** Blends two first person keys (rotation by the shortest path) */
	inline FCutlassViewKey BlendViewKeys(const FCutlassViewKey& A, const FCutlassViewKey& B, float Alpha)
	{
		FCutlassViewKey Out;
		Out.Time = FMath::Lerp(A.Time, B.Time, Alpha);
		Out.Location = FMath::Lerp(A.Location, B.Location, Alpha);
		Out.Rotation = FQuat::Slerp(A.Rotation.Quaternion(), B.Rotation.Quaternion(), Alpha).Rotator();
		return Out;
	}

	/** Samples a track of keys sorted by time: smoothstep between the two keys around Time, held at the ends; Fallback when empty */
	template <typename TKey, typename TBlend>
	TKey SampleTrack(const TArray<TKey>& Keys, float Time, const TKey& Fallback, TBlend Blend)
	{
		if (Keys.Num() == 0)
		{
			return Fallback;
		}
		if (Time <= Keys[0].Time)
		{
			return Keys[0];
		}
		for (int32 Index = 1; Index < Keys.Num(); ++Index)
		{
			if (Time <= Keys[Index].Time)
			{
				const TKey& A = Keys[Index - 1];
				const TKey& B = Keys[Index];
				const float Span = FMath::Max(B.Time - A.Time, KINDA_SMALL_NUMBER);
				const float Alpha = FMath::SmoothStep(0.0f, 1.0f, (Time - A.Time) / Span);
				return Blend(A, B, Alpha);
			}
		}
		return Keys.Last();
	}

	/** Samples a bean mesh space track at Time (see SampleTrack) */
	inline FCutlassKey SampleKeys(const TArray<FCutlassKey>& Keys, float Time, const FCutlassKey& Fallback)
	{
		return SampleTrack(Keys, Time, Fallback, &BlendKeys);
	}

	/** Samples a first person track at Time (see SampleTrack) */
	inline FCutlassViewKey SampleViewKeys(const TArray<FCutlassViewKey>& Keys, float Time, const FCutlassViewKey& Fallback)
	{
		return SampleTrack(Keys, Time, Fallback, &BlendViewKeys);
	}

	/** Offset of the whole first person set (sabre and arms) in the camera space: breath, run bob, recoil of a blocked hit */
	struct FViewSway
	{
		FVector Location = FVector::ZeroVector;
		FRotator Rotation = FRotator::ZeroRotator;

		FViewSway& operator+=(const FViewSway& Other)
		{
			Location += Other.Location;
			Rotation += Other.Rotation;
			return *this;
		}

		/** Applied after a key: the set rotates about the camera, then shifts */
		FTransform ToTransform() const
		{
			return FTransform(Rotation, Location);
		}
	};

	/** Breathing of the guard at Phase (cycles): the sabre rises and falls by AmplitudeCm, the blade nods by PitchDegrees;
	 *  centred at phase 0, periodic */
	inline FViewSway ViewBreath(float Phase, float AmplitudeCm, float PitchDegrees)
	{
		const float Wave = FMath::Sin(UE_TWO_PI * Phase);
		FViewSway Sway;
		Sway.Location.Z = AmplitudeCm * Wave;
		Sway.Rotation.Pitch = PitchDegrees * Wave;
		return Sway;
	}

	/** Run bob at Phase (stride cycles, two steps each) scaled by Weight (0 still, 1 full speed): a dip of VerticalCm at
	 *  each step, one sway of LateralCm and one roll of RollDegrees per stride. Zero at Weight 0, periodic in Phase */
	inline FViewSway ViewBob(float Phase, float Weight, float VerticalCm, float LateralCm, float RollDegrees)
	{
		const float Cycle = UE_TWO_PI * Phase;
		const float Side = FMath::Sin(Cycle);
		FViewSway Sway;
		Sway.Location.Y = LateralCm * Weight * Side;
		Sway.Location.Z = -VerticalCm * Weight * Side * Side;
		Sway.Rotation.Roll = RollDegrees * Weight * Side;
		return Sway;
	}

	/** Weight of the run bob: the ground speed over ReferenceSpeed, capped at 1; none off the ground */
	inline float BobTargetWeight(float Speed, float ReferenceSpeed, bool bOnGround)
	{
		if (!bOnGround || ReferenceSpeed <= KINDA_SMALL_NUMBER)
		{
			return 0.0f;
		}
		return FMath::Clamp(Speed / ReferenceSpeed, 0.0f, 1.0f);
	}

	/** Envelope of the recoil of a blocked hit, Elapsed seconds after the block: out and back over Duration, 0 outside */
	inline float BlockRecoilEnvelope(float Elapsed, float Duration)
	{
		if (Duration <= KINDA_SMALL_NUMBER || Elapsed < 0.0f || Elapsed >= Duration)
		{
			return 0.0f;
		}
		return FMath::Sin(UE_PI * Elapsed / Duration);
	}

	/** Depth in front of the camera (X) of the nearest point of the sabre, from HiltCm behind the grip to TipCm ahead of it */
	inline float ViewBladeMinDepth(const FCutlassViewKey& Key, float HiltCm, float TipCm)
	{
		const FVector Axis = Key.Rotation.RotateVector(FVector::ForwardVector);
		const double Hilt = Key.Location.X - Axis.X * HiltCm;
		const double Tip = Key.Location.X + Axis.X * TipCm;
		return static_cast<float>(FMath::Min(Hilt, Tip));
	}

	/** The key pushed straight ahead, if needed, so no point of the sabre is closer than MinDepthCm to the camera (near clip plane) */
	inline FCutlassViewKey KeepAheadOfNearPlane(const FCutlassViewKey& Key, float HiltCm, float TipCm, float MinDepthCm)
	{
		const float Depth = ViewBladeMinDepth(Key, HiltCm, TipCm);
		if (Depth >= MinDepthCm)
		{
			return Key;
		}
		FCutlassViewKey Out = Key;
		Out.Location.X += MinDepthCm - Depth;
		return Out;
	}

	/** Blends two first person arm poses (shoulder, rotation by the shortest path, stretch) */
	inline FBeanArmPose BlendArmPoses(const FBeanArmPose& A, const FBeanArmPose& B, float Alpha)
	{
		return FBeanArmPose(FMath::Lerp(A.Location, B.Location, Alpha),
			FQuat::Slerp(A.Rotation.Quaternion(), B.Rotation.Quaternion(), Alpha).Rotator(),
			FMath::Lerp(A.Stretch, B.Stretch, Alpha));
	}

	/**
	 *  Timeline of a move as played (s from its start). Before ActiveEnd the track plays at its own speed; the recovery
	 *  after it is played faster (RecoveryScale < 1). The move can be cancelled into another blow from AttackCancel, and
	 *  into a parry, a jump or a step from DefensiveCancel (T-152).
	 */
	struct FMoveWindows
	{
		/** Length of the grip track (s) */
		float TrackLength = 0.0f;
		/** Played length, recovery shortened (s) */
		float Duration = 0.0f;
		/** Blade sweep: first active frame (played time = track time up to ActiveEnd) */
		float ActiveStart = 0.0f;
		/** Blade sweep: last active frame */
		float ActiveEnd = 0.0f;
		/** Instant of the arc check (safety net of the blade sweep) */
		float HitTime = 0.0f;
		/** The next blow may start from here */
		float AttackCancel = 0.0f;
		/** A parry, a jump or a step may end the move from here */
		float DefensiveCancel = 0.0f;
		/** Played time / track time in the recovery */
		float RecoveryScale = 1.0f;
		/** An attack (the draw and the sheathe have no active frames and cannot be cancelled) */
		bool bAttack = false;

		/** Track time at a played time (the draw and the sheathe are stretched over their duration) */
		float TrackTime(float Played) const
		{
			if (!bAttack)
			{
				return Duration > KINDA_SMALL_NUMBER ? FMath::Clamp(Played, 0.0f, Duration) * TrackLength / Duration : 0.0f;
			}
			if (Played <= ActiveEnd)
			{
				return FMath::Max(Played, 0.0f);
			}
			return FMath::Min(ActiveEnd + (Played - ActiveEnd) / FMath::Max(RecoveryScale, 0.05f), TrackLength);
		}
	};

	/** Windows of an attack whose track lasts TrackLength, its blade active from ActiveStart to ActiveEnd */
	inline FMoveWindows ComputeAttackWindows(float TrackLength, float HitTime, float ActiveStart, float ActiveEnd,
		float RecoveryScale, float AttackCancelDelay, float DefensiveCancelDelay)
	{
		FMoveWindows W;
		W.bAttack = true;
		W.TrackLength = FMath::Max(TrackLength, 0.0f);
		W.HitTime = FMath::Clamp(HitTime, 0.0f, W.TrackLength);
		W.ActiveStart = FMath::Clamp(ActiveStart, 0.0f, W.HitTime);
		W.ActiveEnd = FMath::Clamp(ActiveEnd, W.HitTime, W.TrackLength);
		W.RecoveryScale = FMath::Clamp(RecoveryScale, 0.05f, 1.0f);
		W.Duration = W.ActiveEnd + (W.TrackLength - W.ActiveEnd) * W.RecoveryScale;
		W.AttackCancel = FMath::Min(W.ActiveEnd + FMath::Max(AttackCancelDelay, 0.0f), W.Duration);
		W.DefensiveCancel = FMath::Min(W.ActiveEnd + FMath::Max(DefensiveCancelDelay, 0.0f), W.Duration);
		return W;
	}

	/** Windows of any move with the rules; a move without attack (draw, sheathe) is busy to its end */
	inline FMoveWindows GetMoveWindows(const UCutlassRules& Rules, ECutlassMove Move)
	{
		if (const FCutlassAttack* Attack = Rules.GetAttack(Move))
		{
			return ComputeAttackWindows(Rules.GetMoveDuration(Move), Attack->HitTime, Attack->ActiveStartTime, Attack->ActiveEndTime,
				Rules.RecoveryTimeScale, Rules.AttackCancelDelaySeconds, Rules.DefensiveCancelDelaySeconds);
		}
		FMoveWindows W;
		const TArray<FCutlassKey>* Keys = Rules.GetKeys(Move);
		W.TrackLength = Keys && Keys->Num() > 0 ? Keys->Last().Time : 0.0f;
		W.Duration = Rules.GetMoveDuration(Move);
		W.ActiveStart = W.Duration;
		W.ActiveEnd = W.Duration;
		W.HitTime = W.Duration;
		W.AttackCancel = W.Duration;
		W.DefensiveCancel = W.Duration;
		return W;
	}

	/** What to do with a click kept while a move was busy */
	enum class EBufferedInput : uint8
	{
		/** The move cannot be cancelled yet: keep it */
		Wait,
		/** The cancel window is open: play it now */
		Fire,
		/** Clicked too long before the window opened: forget it */
		Expired
	};

	/** A click at PressTime, kept BufferSeconds, against a cancel window opening at OpenTime */
	inline EBufferedInput EvaluateBufferedInput(double Now, double PressTime, float BufferSeconds, double OpenTime)
	{
		if (OpenTime - PressTime > BufferSeconds + KINDA_SMALL_NUMBER)
		{
			return EBufferedInput::Expired;
		}
		return Now >= OpenTime ? EBufferedInput::Fire : EBufferedInput::Wait;
	}

	/** Index of the nearest candidate (horizontal distance) inside the cone of ConeDegrees around Yaw and within Range,
	 *  INDEX_NONE when there is none */
	inline int32 PickAimTarget(const FVector& Origin, float Yaw, TConstArrayView<FVector> Candidates, float ConeDegrees, float Range)
	{
		const FVector Forward = FRotator(0.0f, Yaw, 0.0f).Vector();
		const float HalfCos = FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(ConeDegrees, 0.0f, 360.0f) * 0.5f));
		int32 Best = INDEX_NONE;
		float BestDistance = Range;
		for (int32 Index = 0; Index < Candidates.Num(); ++Index)
		{
			const FVector Flat(Candidates[Index].X - Origin.X, Candidates[Index].Y - Origin.Y, 0.0f);
			const float Distance = Flat.Size();
			if (Distance > BestDistance)
			{
				continue;
			}
			if (Distance > 1.0f && FVector::DotProduct(Forward, Flat / Distance) < HalfCos)
			{
				continue;
			}
			Best = Index;
			BestDistance = Distance;
		}
		return Best;
	}

	/** Yaw turned from Current toward Target by MaxStep degrees at most, the short way (deg) */
	inline float StepYawToward(float Current, float Target, float MaxStep)
	{
		const float Step = FMath::Max(MaxStep, 0.0f);
		const float Delta = FRotator::NormalizeAxis(Target - Current);
		return FRotator::NormalizeAxis(Current + FMath::Clamp(Delta, -Step, Step));
	}

	/** Step forward at the start of a blow: none when an enemy ahead is closer than BlockDistance, never past that
	 *  distance from it (NearestAheadDistance < 0: nobody ahead) */
	inline float LungeDistance(float BaseDistance, float NearestAheadDistance, float BlockDistance)
	{
		if (BaseDistance <= 0.0f)
		{
			return 0.0f;
		}
		if (NearestAheadDistance < 0.0f)
		{
			return BaseDistance;
		}
		return FMath::Clamp(NearestAheadDistance - BlockDistance, 0.0f, BaseDistance);
	}

	/** Point of the blade AlongCm from the grip, in the space of the key (bean mesh space) */
	inline FVector BladePoint(const FCutlassKey& Key, float AlongCm)
	{
		return Key.GripLocation + Key.GripRotation.RotateVector(FVector(AlongCm, 0.0, 0.0));
	}

	/** Count points spread along the blade from StartCm (the guard) to TipCm, transformed by MeshToWorld */
	inline void BladePoints(const FCutlassKey& Key, float StartCm, float TipCm, int32 Count, const FTransform& MeshToWorld, TArray<FVector>& Out)
	{
		Out.Reset();
		const int32 Num = FMath::Max(Count, 2);
		for (int32 Index = 0; Index < Num; ++Index)
		{
			const float Along = FMath::Lerp(StartCm, TipCm, static_cast<float>(Index) / static_cast<float>(Num - 1));
			Out.Add(MeshToWorld.TransformPosition(BladePoint(Key, Along)));
		}
	}

	/** Sub-steps needed to sweep the blade from track time From to To without cutting the corners of its arc */
	inline int32 SubstepCount(float From, float To, float MaxStep)
	{
		return FMath::Clamp(FMath::CeilToInt((To - From) / FMath::Max(MaxStep, 0.001f)), 1, 64);
	}

	/** Closest the swept blade (guard to tip, at each sub-step) comes to Point while the track goes from From to To */
	inline float MinDistanceToBladeSweep(const TArray<FCutlassKey>& Keys, const FCutlassKey& Fallback, float From, float To, float MaxStep,
		float StartCm, float TipCm, const FTransform& MeshToWorld, const FVector& Point)
	{
		float Best = TNumericLimits<float>::Max();
		const int32 Steps = SubstepCount(From, To, MaxStep);
		for (int32 Step = 0; Step <= Steps; ++Step)
		{
			const FCutlassKey Key = SampleKeys(Keys, FMath::Lerp(From, To, static_cast<float>(Step) / Steps), Fallback);
			const FVector Guard = MeshToWorld.TransformPosition(BladePoint(Key, StartCm));
			const FVector Tip = MeshToWorld.TransformPosition(BladePoint(Key, TipCm));
			Best = FMath::Min(Best, static_cast<float>(FMath::PointDistToSegment(Point, Guard, Tip)));
		}
		return Best;
	}

	/**
	 *  Two-bone reach: elbow position so that a chain Shoulder -> elbow (UpperLength) -> end (LowerLength) ends as
	 *  close as possible to Target, bending toward Pole.
	 */
	inline FVector SolveElbow(const FVector& Shoulder, const FVector& Target, float UpperLength, float LowerLength, const FVector& Pole)
	{
		FVector ToTarget = Target - Shoulder;
		const float MaxReach = UpperLength + LowerLength - 0.01f;
		float Distance = ToTarget.Size();
		const FVector Direction = Distance > KINDA_SMALL_NUMBER ? ToTarget / Distance : FVector::ForwardVector;
		Distance = FMath::Clamp(Distance, FMath::Abs(UpperLength - LowerLength) + 0.01f, MaxReach);
		// law of cosines: distance along the shoulder-target line and height of the elbow off it
		const float Along = (UpperLength * UpperLength - LowerLength * LowerLength + Distance * Distance) / (2.0f * Distance);
		const float Height = FMath::Sqrt(FMath::Max(UpperLength * UpperLength - Along * Along, 0.0f));
		FVector Bend = Pole - Direction * FVector::DotProduct(Pole, Direction);
		if (!Bend.Normalize())
		{
			Bend = FVector::UpVector;
		}
		return Shoulder + Direction * Along + Bend * Height;
	}
}
