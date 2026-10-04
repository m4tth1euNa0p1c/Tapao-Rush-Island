// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the loot scatter, tested without a world */
namespace LootMath
{
	/** Index drawn with the weights (Roll in [0, 1)); INDEX_NONE when every weight is zero */
	inline int32 PickWeighted(const TArray<float>& Weights, float Roll)
	{
		double Total = 0.0;
		for (const float Weight : Weights)
		{
			Total += FMath::Max(Weight, 0.0f);
		}
		if (Total <= 0.0)
		{
			return INDEX_NONE;
		}
		double Target = FMath::Clamp(static_cast<double>(Roll), 0.0, 1.0) * Total;
		int32 Last = INDEX_NONE;
		for (int32 Index = 0; Index < Weights.Num(); ++Index)
		{
			const double Weight = FMath::Max(Weights[Index], 0.0f);
			if (Weight <= 0.0)
			{
				continue;
			}
			Last = Index;
			if (Target < Weight)
			{
				return Index;
			}
			Target -= Weight;
		}
		return Last;
	}

	/** Point at least MinSpacing (2D) from every taken point */
	inline bool IsFarFromAll(const FVector& Point, const TArray<FVector>& Taken, float MinSpacing)
	{
		const double SpacingSq = FMath::Square(static_cast<double>(MinSpacing));
		for (const FVector& Other : Taken)
		{
			if (FVector::DistSquared2D(Point, Other) < SpacingSq)
			{
				return false;
			}
		}
		return true;
	}

	/** Spot within [MinDistance, MaxDistance] (2D) of at least one start; any distance when there is no start or MaxDistance is 0 */
	inline bool IsNearAStart(const FVector& Spot, const TArray<FVector>& Starts, float MinDistance, float MaxDistance)
	{
		if (Starts.IsEmpty())
		{
			return true;
		}
		bool bInRange = MaxDistance <= 0.0f;
		for (const FVector& Start : Starts)
		{
			const double Distance = FVector::Dist2D(Spot, Start);
			if (Distance < MinDistance)
			{
				return false;
			}
			bInRange |= MaxDistance <= 0.0f || Distance <= MaxDistance;
		}
		return bInRange;
	}
}
