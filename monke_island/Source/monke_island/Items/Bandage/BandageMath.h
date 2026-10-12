// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Items/Bandage/BandageRules.h"

/**
 *  Pure rules of the bandage (T-340): when one may start, what cuts one under way, the share done and the first person
 *  keys. The item gathers the facts of its carrier, these decide; tested without a world (monke_island.Items.Bandage).
 */
namespace BandageMath
{
	/** What the server knows of the carrier and the item when a bandage is asked for, or every frame of one */
	struct FCarrierFacts
	{
		/** The bandage is the item in hand */
		bool bEquipped = false;

		/** The item is in a cell of its carrier's inventory */
		bool bCarried = false;

		bool bAlive = true;
		bool bRagdoll = false;

		/** Walking on the ground (not jumping, falling, flying nor seated) */
		bool bOnGround = true;

		bool bSwimming = false;
		float WaterDepthCm = 0.0f;
		bool bSprinting = false;

		/** Ground speed (cm/s) */
		float Speed2D = 0.0f;

		float Health = 100.0f;
		float MaxHealth = 100.0f;

		/** A bandage is already under way */
		bool bBusy = false;
	};

	/** The water is too deep for a bandage */
	inline bool IsTooDeep(const FCarrierFacts& Facts, const UBandageRules& Rules)
	{
		return Rules.bInterruptInWater && (Facts.bSwimming || Facts.WaterDepthCm > Rules.MaxWaterDepthCm);
	}

	/** Why a bandage may not start now (None: it may) */
	inline EBandageRefusal JudgeStart(const FCarrierFacts& Facts, const UBandageRules& Rules)
	{
		if (!Facts.bCarried)
		{
			return EBandageRefusal::NotCarried;
		}
		if (!Facts.bEquipped)
		{
			return EBandageRefusal::NotInHand;
		}
		if (Facts.bBusy)
		{
			return EBandageRefusal::Busy;
		}
		if (!Facts.bAlive || Facts.bRagdoll || !Facts.bOnGround || IsTooDeep(Facts, Rules))
		{
			return EBandageRefusal::CarrierUnable;
		}
		if (Rules.bInterruptOnSprint && Facts.bSprinting)
		{
			return EBandageRefusal::Sprinting;
		}
		if (!Rules.bUsableAtFullHealth && (Facts.MaxHealth <= 0.0f || Facts.Health >= Facts.MaxHealth))
		{
			return EBandageRefusal::FullHealth;
		}
		return EBandageRefusal::None;
	}

	/** What cuts a bandage under way this frame (None: it goes on); HitDamage = health just lost to a blow (0 without) */
	inline EBandageInterrupt CheckInterrupt(const FCarrierFacts& Facts, const UBandageRules& Rules, float HitDamage = 0.0f)
	{
		if (!Facts.bAlive || Facts.bRagdoll)
		{
			return EBandageInterrupt::Down;
		}
		if (!Facts.bEquipped || !Facts.bCarried)
		{
			return EBandageInterrupt::PutAway;
		}
		if (Rules.bInterruptOnHit && HitDamage > 0.0f && HitDamage >= Rules.MinHitDamage)
		{
			return EBandageInterrupt::Hit;
		}
		if (IsTooDeep(Facts, Rules))
		{
			return EBandageInterrupt::Water;
		}
		if (Rules.bInterruptOnJump && !Facts.bOnGround)
		{
			return EBandageInterrupt::Jump;
		}
		if (Rules.bInterruptOnSprint && (Facts.bSprinting || (Rules.MaxMoveSpeedCmS > 0.0f && Facts.Speed2D > Rules.MaxMoveSpeedCmS)))
		{
			return EBandageInterrupt::Sprint;
		}
		return EBandageInterrupt::None;
	}

	/** Share of an application done at Now (0 to 1) */
	inline float Progress(double Now, double Start, float Seconds)
	{
		if (Seconds <= KINDA_SMALL_NUMBER || !FMath::IsFinite(Now) || !FMath::IsFinite(Start))
		{
			return 0.0f;
		}
		return FMath::Clamp(static_cast<float>((Now - Start) / Seconds), 0.0f, 1.0f);
	}

	/** Play rate that stretches a clip of ClipLength over Seconds */
	inline float PlayRate(float ClipLength, float Seconds)
	{
		return ClipLength > KINDA_SMALL_NUMBER && Seconds > KINDA_SMALL_NUMBER ? ClipLength / Seconds : 1.0f;
	}

	/** One arm pose between two (place and turn eased linearly, stretch too) */
	inline FBeanArmPose LerpArm(const FBeanArmPose& A, const FBeanArmPose& B, float Alpha)
	{
		const float T = FMath::Clamp(Alpha, 0.0f, 1.0f);
		const FQuat Rotation = FQuat::Slerp(A.Rotation.Quaternion(), B.Rotation.Quaternion(), T);
		return FBeanArmPose(FMath::Lerp(A.Location, B.Location, T), Rotation.Rotator(), FMath::Lerp(A.Stretch, B.Stretch, T));
	}

	/** Both arms at a time of the clip, from keys sorted by time (held before the first and after the last); false
	 *  without keys */
	inline bool SampleArmKeys(const TArray<FBandageArmKey>& Keys, float ClipTime, FBeanArmPose& OutRight, FBeanArmPose& OutLeft)
	{
		if (Keys.IsEmpty())
		{
			return false;
		}
		if (ClipTime <= Keys[0].TimeSeconds || Keys.Num() == 1)
		{
			OutRight = Keys[0].Right;
			OutLeft = Keys[0].Left;
			return true;
		}
		for (int32 Index = 1; Index < Keys.Num(); ++Index)
		{
			const FBandageArmKey& From = Keys[Index - 1];
			const FBandageArmKey& To = Keys[Index];
			if (ClipTime <= To.TimeSeconds)
			{
				const float Span = To.TimeSeconds - From.TimeSeconds;
				const float Alpha = Span > KINDA_SMALL_NUMBER ? (ClipTime - From.TimeSeconds) / Span : 1.0f;
				OutRight = LerpArm(From.Right, To.Right, Alpha);
				OutLeft = LerpArm(From.Left, To.Left, Alpha);
				return true;
			}
		}
		OutRight = Keys.Last().Right;
		OutLeft = Keys.Last().Left;
		return true;
	}

	/** Point of the first person arm mesh holding the roll, relative to the camera, for an arm pose */
	inline FVector HandOf(const FBeanArmPose& Arm, const FVector& HandPoint)
	{
		return Arm.Location + Arm.Rotation.RotateVector(FVector(HandPoint.X * Arm.Stretch, HandPoint.Y, HandPoint.Z));
	}

	/** Procedural wrap: where the roll is at a time of the clip, relative to the camera (from the held place to the
	 *  forearm before WrapStart, round it until BandageDone, back after) */
	inline FVector WrapRollLocation(const UBandageRules& Rules, float ClipTime)
	{
		const FVector Held = Rules.FirstPersonRollHeld.GetLocation();
		const float Start = FMath::Max(Rules.WrapStartSeconds, 0.01f);
		const float Done = FMath::Max(Rules.HealEventSeconds, Start + 0.01f);
		const FVector Top = Rules.FirstPersonWrapCenter + FVector(0.0, 0.0, Rules.FirstPersonWrapRadiusCm);
		if (ClipTime < Start)
		{
			return FMath::Lerp(Held, Top, FMath::SmoothStep(0.0f, 1.0f, ClipTime / Start));
		}
		if (ClipTime < Done)
		{
			const float Angle = UE_TWO_PI * Rules.FirstPersonWrapTurns * (ClipTime - Start) / (Done - Start);
			return Rules.FirstPersonWrapCenter + FVector(FMath::Sin(Angle) * Rules.FirstPersonWrapRadiusCm, 0.0, FMath::Cos(Angle) * Rules.FirstPersonWrapRadiusCm);
		}
		const float Back = FMath::Max(Rules.ClipSeconds - Done, 0.05f);
		return FMath::Lerp(Top, Held, FMath::SmoothStep(0.0f, 1.0f, (ClipTime - Done) / Back));
	}
}
