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

	/** Samples a track at Time (smoothstep between keys, held at the ends); Fallback when empty */
	inline FCutlassKey SampleKeys(const TArray<FCutlassKey>& Keys, float Time, const FCutlassKey& Fallback)
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
				const FCutlassKey& A = Keys[Index - 1];
				const FCutlassKey& B = Keys[Index];
				const float Span = FMath::Max(B.Time - A.Time, KINDA_SMALL_NUMBER);
				const float Alpha = FMath::SmoothStep(0.0f, 1.0f, (Time - A.Time) / Span);
				return BlendKeys(A, B, Alpha);
			}
		}
		return Keys.Last();
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
