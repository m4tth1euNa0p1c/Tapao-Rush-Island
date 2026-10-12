// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the placement of the sacred sites of a fragment (ACampSpawner: reliquary temple, T-180), tested by
 *  monke_island.Marae.Placement */
namespace SitePlacement
{
	/** Middle of the free spots of a fragment (2D mean; Z of the mean too): the heart of its land */
	inline FVector Centroid(const TArray<FVector>& Spots)
	{
		FVector Sum = FVector::ZeroVector;
		for (const FVector& Spot : Spots)
		{
			Sum += Spot;
		}
		return Spots.IsEmpty() ? FVector::ZeroVector : Sum / static_cast<double>(Spots.Num());
	}

	/** Indices of the spots, nearest to Point first (2D); ties keep their order */
	inline TArray<int32> ByDistanceTo(const TArray<FVector>& Spots, const FVector& Point)
	{
		TArray<int32> Indices;
		Indices.Reserve(Spots.Num());
		for (int32 Index = 0; Index < Spots.Num(); ++Index)
		{
			Indices.Add(Index);
		}
		Indices.StableSort([&Spots, &Point](int32 A, int32 B)
		{
			return FVector::DistSquared2D(Spots[A], Point) < FVector::DistSquared2D(Spots[B], Point);
		});
		return Indices;
	}

	/** True when every one of the Expected ground samples was found, all above the beach (SeaZ + MinAboveSea) and
	 *  within MaxDelta of each other: a site of that size stands level on land */
	inline bool IsBuildableGround(const TArray<double>& GroundZ, int32 Expected, double SeaZ, float MinAboveSea, float MaxDelta)
	{
		if (GroundZ.Num() < FMath::Max(Expected, 1))
		{
			return false;
		}
		double Low = TNumericLimits<double>::Max();
		double High = TNumericLimits<double>::Lowest();
		for (const double Z : GroundZ)
		{
			Low = FMath::Min(Low, Z);
			High = FMath::Max(High, Z);
		}
		return Low >= SeaZ + MinAboveSea && High - Low <= MaxDelta;
	}

	/** Yaw that turns the local +Y of a site (the gate of the temple) towards Target */
	inline float YawFacing(const FVector& Site, const FVector& Target)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Target.Y - Site.Y, Target.X - Site.X)) - 90.0f;
	}
}
