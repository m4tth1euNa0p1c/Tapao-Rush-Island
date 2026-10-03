// monke_island

#pragma once

#include "CoreMinimal.h"

/** Locomotion clip a creature plays for its speed */
enum class ETikiLocomotion : uint8
{
	Idle,
	Walk,
	Run
};

/** Pure rules of the tiki creatures, tested without a world */
namespace TikiMath
{
	/** Idle under 10 cm/s, walk up to 1.3 x the walking speed, run above */
	inline ETikiLocomotion ChooseLocomotion(float Speed, float WalkSpeed)
	{
		if (Speed < 10.0f)
		{
			return ETikiLocomotion::Idle;
		}
		return Speed <= WalkSpeed * 1.3f ? ETikiLocomotion::Walk : ETikiLocomotion::Run;
	}

	/** True when the blow can start: in range and the cooldown is over */
	inline bool CanStartAttack(float Distance, float Range, double Now, double LastAttackTime, float Cooldown)
	{
		return Distance <= Range && Now - LastAttackTime >= Cooldown;
	}

	/** Index of the point nearest to Origin within MaxDistance, or INDEX_NONE */
	inline int32 NearestIndex(const TArray<FVector>& Points, const FVector& Origin, float MaxDistance)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = FMath::Square(static_cast<double>(MaxDistance));
		for (int32 Index = 0; Index < Points.Num(); ++Index)
		{
			const double DistSq = FVector::DistSquared(Points[Index], Origin);
			if (DistSq <= BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Index;
			}
		}
		return Best;
	}

	/** True when a mesh name contains one of the bush keywords (case insensitive) */
	inline bool IsBushMeshName(const FString& MeshName, const TArray<FString>& Keywords)
	{
		for (const FString& Keyword : Keywords)
		{
			if (!Keyword.IsEmpty() && MeshName.Contains(Keyword, ESearchCase::IgnoreCase))
			{
				return true;
			}
		}
		return false;
	}

	/** Point on the ring [MinRadius, MaxRadius] around Center for an angle and a fraction in [0, 1] (Z kept) */
	inline FVector PointOnRing(const FVector& Center, float AngleRad, float Fraction, float MinRadius, float MaxRadius)
	{
		const float Radius = FMath::Lerp(MinRadius, MaxRadius, FMath::Clamp(Fraction, 0.0f, 1.0f));
		return Center + FVector(FMath::Cos(AngleRad) * Radius, FMath::Sin(AngleRad) * Radius, 0.0f);
	}
}
