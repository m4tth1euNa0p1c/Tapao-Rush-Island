// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Village/VillageLayout.h"

/** Pure helpers of the fragment villages (AVillageSpawner), tested by monke_island.Village.Fragments */
namespace VillageMath
{
	/** True when P stands in the area or less than Margin beyond it (same test as scripts/map/village_clearing.py) */
	inline bool IsInArea(const FVillageClearArea& Area, const FVector2D& P, float Margin)
	{
		switch (Area.Shape)
		{
		case EVillageClearShape::Rect:
		{
			const double Angle = FMath::DegreesToRadians(static_cast<double>(Area.Yaw));
			const FVector2D D = P - Area.Center;
			const double LocalX = D.X * FMath::Cos(Angle) + D.Y * FMath::Sin(Angle);
			const double LocalY = -D.X * FMath::Sin(Angle) + D.Y * FMath::Cos(Angle);
			return FMath::Abs(LocalX) <= Area.HalfSize.X + Margin && FMath::Abs(LocalY) <= Area.HalfSize.Y + Margin;
		}
		case EVillageClearShape::Circle:
			return FVector2D::DistSquared(P, Area.Center) <= FMath::Square(static_cast<double>(Area.HalfSize.X + Margin));
		case EVillageClearShape::Path:
		{
			const double Reach = FMath::Square(static_cast<double>(Area.HalfSize.X + Margin));
			for (int32 Index = 0; Index + 1 < Area.Points.Num(); ++Index)
			{
				const FVector2D A = Area.Points[Index];
				const FVector2D AB = Area.Points[Index + 1] - A;
				const double Length = AB.SizeSquared();
				const double T = Length > UE_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(P - A, AB) / Length, 0.0, 1.0) : 0.0;
				if (FVector2D::DistSquared(P, A + AB * T) <= Reach)
				{
					return true;
				}
			}
			return false;
		}
		default:
			return false;
		}
	}

	/** Box holding the area and Pad around it (UE cm) */
	inline FBox2D AreaBounds(const FVillageClearArea& Area, float Pad)
	{
		FBox2D Box(ForceInit);
		switch (Area.Shape)
		{
		case EVillageClearShape::Rect:
		{
			const double Reach = Area.HalfSize.Size() + Pad;
			Box += Area.Center - FVector2D(Reach, Reach);
			Box += Area.Center + FVector2D(Reach, Reach);
			break;
		}
		case EVillageClearShape::Circle:
			Box += Area.Center - FVector2D(Area.HalfSize.X + Pad, Area.HalfSize.X + Pad);
			Box += Area.Center + FVector2D(Area.HalfSize.X + Pad, Area.HalfSize.X + Pad);
			break;
		case EVillageClearShape::Path:
			for (const FVector2D& Point : Area.Points)
			{
				Box += Point - FVector2D(Area.HalfSize.X + Pad, Area.HalfSize.X + Pad);
				Box += Point + FVector2D(Area.HalfSize.X + Pad, Area.HalfSize.X + Pad);
			}
			break;
		default:
			break;
		}
		return Box;
	}

	/** Index of the arrival nearest to one of the players, if one is within MaxDistance (2D), INDEX_NONE otherwise */
	inline int32 PickFragment(const TArray<FVector>& Arrivals, const TArray<FVector>& Players, double MaxDistance)
	{
		int32 Best = INDEX_NONE;
		double BestDistSq = FMath::Square(MaxDistance);
		for (int32 Index = 0; Index < Arrivals.Num(); ++Index)
		{
			for (const FVector& Player : Players)
			{
				const double DistSq = FVector::DistSquared2D(Arrivals[Index], Player);
				if (DistSq <= BestDistSq)
				{
					BestDistSq = DistSq;
					Best = Index;
				}
			}
		}
		return Best;
	}

	/** Point of a door's gate: Distance from the door along Yaw (degrees) */
	inline FVector GateOf(const FVector& Door, float Yaw, float Distance)
	{
		return Door + FRotator(0.0f, Yaw, 0.0f).Vector() * Distance;
	}
}
