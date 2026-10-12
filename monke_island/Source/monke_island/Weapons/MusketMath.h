// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Weapons/MusketRules.h"

/** Pure musket rules, shared by the weapon and the tests */
namespace MusketMath
{
	/** Ammunition of a player: the ball in the barrel and the balls in the pouch */
	struct FAmmo
	{
		bool bLoaded = false;
		int32 PouchBalls = 0;

		int32 Total() const { return PouchBalls + (bLoaded ? 1 : 0); }
	};

	/** Ammunition at spawn: MaxBalls in all, one of them in the barrel when the musket starts loaded */
	inline FAmmo StartingAmmo(int32 MaxBalls, bool bStartLoaded)
	{
		FAmmo Ammo;
		const int32 Max = FMath::Max(MaxBalls, 0);
		Ammo.bLoaded = bStartLoaded && Max > 0;
		Ammo.PouchBalls = Max - (Ammo.bLoaded ? 1 : 0);
		return Ammo;
	}

	/** A shot needs a loaded barrel and an idle musket */
	inline bool CanFire(const FAmmo& Ammo, bool bReloading, bool bRecovering)
	{
		return Ammo.bLoaded && !bReloading && !bRecovering;
	}

	/** A reload needs an empty barrel, a ball in the pouch and an idle musket */
	inline bool CanReload(const FAmmo& Ammo, bool bReloading, bool bRecovering)
	{
		return !Ammo.bLoaded && Ammo.PouchBalls > 0 && !bReloading && !bRecovering;
	}

	/** Shot: the barrel empties */
	inline void Fire(FAmmo& Ammo)
	{
		Ammo.bLoaded = false;
	}

	/** End of a reload: one ball goes from the pouch into the barrel */
	inline bool CompleteReload(FAmmo& Ammo)
	{
		if (Ammo.bLoaded || Ammo.PouchBalls <= 0)
		{
			return false;
		}
		--Ammo.PouchBalls;
		Ammo.bLoaded = true;
		return true;
	}

	/** Play rate that stretches a clip of ClipSeconds over TargetSeconds (T-170: the 6 s GroundReload over
	 *  ReloadSeconds); 1 when either length is not positive */
	inline float StretchRate(float ClipSeconds, float TargetSeconds)
	{
		if (ClipSeconds <= 0.0f || TargetSeconds <= 0.0f)
		{
			return 1.0f;
		}
		return FMath::Clamp(ClipSeconds / TargetSeconds, 0.1f, 20.0f);
	}

	/** Blends the musket of two first person keys (Alpha 0 = A); the mitten holds switch at half way */
	inline FMusketFirstPersonKey BlendKeys(const FMusketFirstPersonKey& A, const FMusketFirstPersonKey& B, float Alpha)
	{
		FMusketFirstPersonKey Out = Alpha < 0.5f ? A : B;
		Out.Time = FMath::Lerp(A.Time, B.Time, Alpha);
		Out.MusketLocation = FMath::Lerp(A.MusketLocation, B.MusketLocation, Alpha);
		// shortest way round: a straight lerp of the angles spins the musket through 360 degrees
		Out.MusketRotation = FQuat::Slerp(A.MusketRotation.Quaternion(), B.MusketRotation.Quaternion(), Alpha).Rotator();
		return Out;
	}

	/** Two keys around a time and the eased weight of the second one */
	struct FKeySpan
	{
		int32 A = INDEX_NONE;
		int32 B = INDEX_NONE;
		float Alpha = 0.0f;
	};

	/** Keys around Time (keys sorted by time; eased between keys, held past the ends); INDEX_NONE for an empty track */
	inline FKeySpan FindKeys(const TArray<FMusketFirstPersonKey>& Keys, float Time)
	{
		FKeySpan Span;
		if (Keys.Num() == 0)
		{
			return Span;
		}
		Span.A = Span.B = Keys.Num() - 1;
		if (Time <= Keys[0].Time)
		{
			Span.A = Span.B = 0;
			return Span;
		}
		for (int32 Index = 1; Index < Keys.Num(); ++Index)
		{
			if (Time <= Keys[Index].Time)
			{
				Span.A = Index - 1;
				Span.B = Index;
				const float Length = FMath::Max(Keys[Index].Time - Keys[Index - 1].Time, KINDA_SMALL_NUMBER);
				Span.Alpha = FMath::SmoothStep(0.0f, 1.0f, (Time - Keys[Index - 1].Time) / Length);
				break;
			}
		}
		return Span;
	}

	/** Pose of a key track at Time (see FindKeys) */
	inline FMusketFirstPersonKey SampleKeys(const TArray<FMusketFirstPersonKey>& Keys, float Time, const FMusketFirstPersonKey& Fallback)
	{
		const FKeySpan Span = FindKeys(Keys, Time);
		return Span.A == INDEX_NONE ? Fallback : BlendKeys(Keys[Span.A], Keys[Span.B], Span.Alpha);
	}

	/** Transform of a per frame track at Time (s), interpolated between frames, held past the ends */
	inline FTransform SampleTrack(const TArray<FTransform>& Track, float Time, float FrameRate, const FTransform& Fallback)
	{
		if (Track.Num() == 0)
		{
			return Fallback;
		}
		const float Frame = FMath::Clamp(Time * FMath::Max(FrameRate, 1.0f), 0.0f, static_cast<float>(Track.Num() - 1));
		const int32 First = FMath::FloorToInt32(Frame);
		const int32 Second = FMath::Min(First + 1, Track.Num() - 1);
		FTransform Out;
		Out.Blend(Track[First], Track[Second], Frame - First);
		return Out;
	}

	/**
	 *  First person arm whose mitten lands on Target (both relative to the camera): it points from Shoulder at the
	 *  target, stretches up to MaxStretch, and slides along that line when the target is still further away.
	 *  HandPoint = centre of the mitten in the arm mesh (pivot at the shoulder, along +X).
	 */
	inline FBeanArmPose SolveArm(const FVector& Shoulder, const FVector& Target, const FVector& HandPoint, float MaxStretch)
	{
		const FVector Direction = (Target - Shoulder).GetSafeNormal(UE_KINDA_SMALL_NUMBER, FVector::ForwardVector);
		const FRotator Rotation = FRotationMatrix::MakeFromXZ(Direction, FVector::UpVector).Rotator();
		const float Reach = FMath::Max(static_cast<float>(HandPoint.X), 1.0f);
		const float Stretch = FMath::Clamp(static_cast<float>(FVector::Dist(Shoulder, Target)) / Reach, 1.0f, FMath::Max(MaxStretch, 1.0f));
		const FVector Hand(HandPoint.X * Stretch, HandPoint.Y, HandPoint.Z);
		return FBeanArmPose(Target - Rotation.RotateVector(Hand), Rotation, Stretch);
	}

	/** A left arm pose written as a right arm one (the arms component mirrors the left arm across the view) */
	inline FBeanArmPose MirrorArm(const FBeanArmPose& Pose)
	{
		return FBeanArmPose(FVector(Pose.Location.X, -Pose.Location.Y, Pose.Location.Z),
			FRotator(Pose.Rotation.Pitch, -Pose.Rotation.Yaw, -Pose.Rotation.Roll), Pose.Stretch);
	}

	/** Shot direction inside a cone of HalfAngleDegrees around Aim */
	inline FVector SpreadDirection(const FVector& Aim, float HalfAngleDegrees, FRandomStream& Stream)
	{
		return HalfAngleDegrees > 0.0f ? Stream.VRandCone(Aim.GetSafeNormal(), FMath::DegreesToRadians(HalfAngleDegrees)) : Aim.GetSafeNormal();
	}
}
