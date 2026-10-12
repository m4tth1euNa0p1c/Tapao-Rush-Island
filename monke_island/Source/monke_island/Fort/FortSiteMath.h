// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the forts built on the atoll (tested by monke_island.Fort.Site) */
namespace FortSiteMath
{
	/** Indices of Count different sites among SiteCount, in the order Stream deals them */
	inline TArray<int32> PickSites(int32 SiteCount, int32 Count, FRandomStream& Stream)
	{
		TArray<int32> Order;
		for (int32 Index = 0; Index < SiteCount; ++Index)
		{
			Order.Add(Index);
		}
		for (int32 Index = Order.Num() - 1; Index > 0; --Index)
		{
			Order.Swap(Index, Stream.RandRange(0, Index));
		}
		Order.SetNum(FMath::Clamp(Count, 0, Order.Num()));
		return Order;
	}

	/** Height under which Quantile (0-1) of the heights lie (nearest rank); Fallback when there is none */
	inline double HeightAtQuantile(TArray<double> Heights, float Quantile, double Fallback)
	{
		if (Heights.IsEmpty())
		{
			return Fallback;
		}
		Heights.Sort();
		const int32 Rank = FMath::Clamp(FMath::CeilToInt32(FMath::Clamp(Quantile, 0.0f, 1.0f) * Heights.Num()) - 1, 0, Heights.Num() - 1);
		return Heights[Rank];
	}

	/** Points the ground is measured at around a site (world, Z of the site): its centre, then 8 per ring */
	inline TArray<FVector> GroundProbes(const FVector& Site, TConstArrayView<float> Rings)
	{
		TArray<FVector> Probes;
		Probes.Add(Site);
		for (const float Ring : Rings)
		{
			for (int32 Index = 0; Index < 8; ++Index)
			{
				const double Angle = UE_TWO_PI * Index / 8.0;
				Probes.Add(Site + FVector(FMath::Cos(Angle) * Ring, FMath::Sin(Angle) * Ring, 0.0));
			}
		}
		return Probes;
	}

	/** Origin of the fort level: on the site, turned to Yaw, the ground measured at TopGroundLocal of its frame */
	inline FTransform LevelTransform(const FVector& Site, float Yaw, double GroundZ, float TopGroundLocal)
	{
		return FTransform(FRotator(0.0f, Yaw, 0.0f), FVector(Site.X, Site.Y, GroundZ - TopGroundLocal));
	}

	/** Name of the streamed level of a fort, the same on the server and every client (a valid package name) */
	inline FString LevelName(const FString& LevelShortName, FName FragmentId, int32 SiteIndex, int32 Draw)
	{
		FString Name = FString::Printf(TEXT("%s_%s_%d_%08X"), *LevelShortName, *FragmentId.ToString(), SiteIndex, static_cast<uint32>(Draw));
		for (TCHAR& Character : Name)
		{
			if (!FChar::IsAlnum(Character) && Character != TEXT('_'))
			{
				Character = TEXT('_');
			}
		}
		return Name;
	}
}
