// monke_island

#pragma once

#include "CoreMinimal.h"

/** What a soldier of the fort does against the one it fights (FortGarrisonMath::PickMove) */
enum class EFortSoldierMove : uint8
{
	/** Runs toward it (out of range, out of sight) */
	Close,
	/** Aims and fires (loaded, in sight, in range) */
	Fire,
	/** Reloads first (shots left, not loaded, not pressed) */
	Reload,
	/** Cold steel: bayonet or sabre (pressed, or no shot left) */
	Melee
};

/** Pure rules of the garrison of a fort (tested by monke_island.Fort.Garrison), no world */
namespace FortGarrisonMath
{
	/** Even-odd test of Point in a polygon (any orientation, not crossing itself) */
	inline bool IsInside(const TArray<FVector2D>& Polygon, const FVector2D& Point)
	{
		bool bInside = false;
		for (int32 Index = 0, Previous = Polygon.Num() - 1; Index < Polygon.Num(); Previous = Index++)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[Previous];
			if ((A.Y > Point.Y) != (B.Y > Point.Y))
			{
				const double CrossX = A.X + (Point.Y - A.Y) * (B.X - A.X) / (B.Y - A.Y);
				if (Point.X < CrossX)
				{
					bInside = !bInside;
				}
			}
		}
		return bInside;
	}

	/** Point of the polygon's edges nearest to Point; OutEdge is the edge it lies on (from that vertex) */
	inline FVector2D NearestOnEdges(const TArray<FVector2D>& Polygon, const FVector2D& Point, int32* OutEdge = nullptr)
	{
		FVector2D Best = Point;
		double BestDistSq = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Polygon.Num(); ++Index)
		{
			const FVector2D& A = Polygon[Index];
			const FVector2D& B = Polygon[(Index + 1) % Polygon.Num()];
			const FVector2D Edge = B - A;
			const double LengthSq = Edge.SizeSquared();
			const double T = LengthSq > UE_KINDA_SMALL_NUMBER ? FMath::Clamp(FVector2D::DotProduct(Point - A, Edge) / LengthSq, 0.0, 1.0) : 0.0;
			const FVector2D Candidate = A + Edge * T;
			const double DistSq = FVector2D::DistSquared(Candidate, Point);
			if (DistSq < BestDistSq)
			{
				BestDistSq = DistSq;
				Best = Candidate;
				if (OutEdge)
				{
					*OutEdge = Index;
				}
			}
		}
		return Best;
	}

	/** Point held inside the polygon: itself when inside, else the nearest edge point moved Margin inward */
	inline FVector2D ClampInside(const TArray<FVector2D>& Polygon, const FVector2D& Point, double Margin)
	{
		if (Polygon.Num() < 3 || IsInside(Polygon, Point))
		{
			return Point;
		}
		int32 Edge = 0;
		const FVector2D OnEdge = NearestOnEdges(Polygon, Point, &Edge);
		const FVector2D Along = (Polygon[(Edge + 1) % Polygon.Num()] - Polygon[Edge]).GetSafeNormal();
		const FVector2D Normal(-Along.Y, Along.X);
		// the side of the edge that is inside, whatever the orientation of the outline
		for (const double Sign : { 1.0, -1.0 })
		{
			const FVector2D Candidate = OnEdge + Normal * (Sign * FMath::Max(Margin, 1.0));
			if (IsInside(Polygon, Candidate))
			{
				return Candidate;
			}
		}
		return OnEdge;
	}

	/** Chance a ball strikes a target at Distance: NearChance up to NearCm, FarChance from FarCm, linear between */
	inline float HitChance(float Distance, float NearCm, float FarCm, float NearChance, float FarChance)
	{
		if (FarCm <= NearCm)
		{
			return Distance <= NearCm ? NearChance : FarChance;
		}
		const float Alpha = FMath::Clamp((Distance - NearCm) / (FarCm - NearCm), 0.0f, 1.0f);
		return FMath::Clamp(FMath::Lerp(NearChance, FarChance, Alpha), 0.0f, 1.0f);
	}

	/**
	 *  The move against the one it fights, Edge cm away (body to body): cold steel when pressed (closer than MeleeCm) or
	 *  without a shot left; with shots, reload when not loaded, fire when it sees it between MinFireCm and MaxFireCm,
	 *  otherwise close in (a loaded soldier closer than MinFireCm but not pressed fights hand to hand too)
	 */
	inline EFortSoldierMove PickMove(float Edge, bool bSees, int32 ShotsLeft, bool bLoaded, float MeleeCm, float MinFireCm, float MaxFireCm)
	{
		if (ShotsLeft <= 0 || Edge <= MeleeCm)
		{
			return EFortSoldierMove::Melee;
		}
		if (!bLoaded)
		{
			return EFortSoldierMove::Reload;
		}
		if (Edge < MinFireCm)
		{
			return EFortSoldierMove::Melee;
		}
		return bSees && Edge <= MaxFireCm ? EFortSoldierMove::Fire : EFortSoldierMove::Close;
	}

	/** Next patrol point: another one than Current, drawn by Random in [0, 1) (Current when it is the only one) */
	inline int32 NextPatrolPoint(int32 Count, int32 Current, float Random)
	{
		if (Count <= 0)
		{
			return INDEX_NONE;
		}
		if (Count == 1 || !(Current >= 0 && Current < Count))
		{
			return FMath::Clamp(static_cast<int32>(Random * Count), 0, Count - 1);
		}
		const int32 Step = 1 + FMath::Clamp(static_cast<int32>(Random * (Count - 1)), 0, Count - 2);
		return (Current + Step) % Count;
	}

	/** Play rate of a locomotion clip whose feet keep pace at NoSlideSpeed, held between MinRate and MaxRate */
	inline float LocomotionRate(float Speed, float NoSlideSpeed, float MinRate, float MaxRate)
	{
		return FMath::Clamp(Speed / FMath::Max(NoSlideSpeed, 1.0f), MinRate, FMath::Max(MinRate, MaxRate));
	}

	/** Edge distance between two bodies: centre distance on the ground plane less both radii (never below 0) */
	inline float EdgeDistance(const FVector& A, float RadiusA, const FVector& B, float RadiusB)
	{
		return FMath::Max(0.0f, static_cast<float>(FVector::Dist2D(A, B)) - RadiusA - RadiusB);
	}

	/** True when To lies within HalfAngleDegrees of Forward on the ground plane */
	inline bool IsInView(const FVector& Forward, const FVector& To, float HalfAngleDegrees)
	{
		const FVector2D F = FVector2D(Forward.X, Forward.Y).GetSafeNormal();
		const FVector2D T = FVector2D(To.X, To.Y).GetSafeNormal();
		if (T.IsNearlyZero())
		{
			return true;
		}
		return FVector2D::DotProduct(F, T) >= FMath::Cos(FMath::DegreesToRadians(FMath::Clamp(HalfAngleDegrees, 0.0f, 180.0f)));
	}

	/** Soldiers to add to a garrison of Placed to reach the Wanted of the day (negative: the surplus to send away);
	 *  0 when the day wants none in particular (0) or there is nobody placed to clone (the difficulty by day,
	 *  owner 2026-10-09) */
	inline int32 GarrisonChange(int32 Placed, int32 Wanted)
	{
		return Wanted <= 0 || Placed <= 0 ? 0 : Wanted - Placed;
	}

	/** Post of the Index-th soldier added to a garrison of Placed: the posts in turn after those the placed ones
	 *  took; INDEX_NONE without a post */
	inline int32 SpawnPost(int32 Index, int32 Placed, int32 PostCount)
	{
		return PostCount <= 0 ? INDEX_NONE : (FMath::Max(Placed, 0) + FMath::Max(Index, 0)) % PostCount;
	}

	/** Point held inside the disc (Center, Radius) on the ground plane, height kept; a radius of 0 or less leaves it */
	inline FVector ClampIntoDisc(const FVector& Center, float Radius, const FVector& Point)
	{
		const FVector2D Offset(Point.X - Center.X, Point.Y - Center.Y);
		const double Distance = Offset.Size();
		if (Radius <= 0.0f || Distance <= Radius)
		{
			return Point;
		}
		const FVector2D Kept = Offset * (Radius / Distance);
		return FVector(Center.X + Kept.X, Center.Y + Kept.Y, Point.Z);
	}

	// ------------------------------------------------------------------ guards of the buildings (T-332, owner 2026-10-11)

	/** Guards of the buildings a fort posts on Day (DA_FortSoldierRules.BuildingGuardsByDay): the entry of the day (the
	 *  first one is day 1), the last entry for the days after it, never below 0; WithoutRun on day 0 (no campaign: a
	 *  test map) or without any entry */
	inline int32 BuildingGuardsOfDay(TConstArrayView<int32> ByDay, int32 Day, int32 WithoutRun)
	{
		if (Day <= 0 || ByDay.IsEmpty())
		{
			return FMath::Max(WithoutRun, 0);
		}
		return FMath::Max(ByDay[FMath::Min(Day, ByDay.Num()) - 1], 0);
	}

	/** Guards actually posted: the Wanted of the day, one per post at most, the fort never above MaxSoldiers alive once
	 *  they stand (Garrison: the soldiers of the court and the ramparts already there; MaxSoldiers 0 or less: no limit) */
	inline int32 BuildingGuardsToPost(int32 Wanted, int32 Garrison, int32 MaxSoldiers, int32 PostCount)
	{
		int32 Count = FMath::Clamp(Wanted, 0, FMath::Max(PostCount, 0));
		if (MaxSoldiers > 0)
		{
			Count = FMath::Min(Count, FMath::Max(MaxSoldiers - FMath::Max(Garrison, 0), 0));
		}
		return Count;
	}

	/** Posts of Count guards spread over the buildings: the first post of each building in turn (from the building
	 *  FirstBuilding, in the order they first appear), then their second posts, and so on; PostBuildings names the
	 *  building of each post. A building with more posts than the others gets its extra guards last */
	inline TArray<int32> SpreadOverBuildings(const TArray<FName>& PostBuildings, int32 Count, int32 FirstBuilding)
	{
		TArray<FName> Buildings;
		TArray<TArray<int32>> PostsOf;
		for (int32 Index = 0; Index < PostBuildings.Num(); ++Index)
		{
			int32 Building = Buildings.IndexOfByKey(PostBuildings[Index]);
			if (Building == INDEX_NONE)
			{
				Building = Buildings.Add(PostBuildings[Index]);
				PostsOf.AddDefaulted();
			}
			PostsOf[Building].Add(Index);
		}
		TArray<int32> Result;
		const int32 Wanted = FMath::Clamp(Count, 0, PostBuildings.Num());
		if (Buildings.IsEmpty() || Wanted == 0)
		{
			return Result;
		}
		const int32 Start = ((FirstBuilding % Buildings.Num()) + Buildings.Num()) % Buildings.Num();
		// every round takes one more post of each building that still has one: Wanted is reached before the posts run out
		for (int32 Round = 0; Result.Num() < Wanted; ++Round)
		{
			for (int32 Step = 0; Step < Buildings.Num() && Result.Num() < Wanted; ++Step)
			{
				const TArray<int32>& Own = PostsOf[(Start + Step) % Buildings.Num()];
				if (Own.IsValidIndex(Round))
				{
					Result.Add(Own[Round]);
				}
			}
		}
		return Result;
	}

	/** Whether a soldier answers the alarm of its fort for a foe seen Distance away from it (cm): always for the patrol
	 *  of the court and the ramparts; a guard of a building holds it, and comes only for a foe within AlarmRadius (0 or
	 *  less: every alarm) */
	inline bool AnswersAlarm(bool bBuildingGuard, double Distance, float AlarmRadius)
	{
		return !bBuildingGuard || AlarmRadius <= 0.0f || Distance <= AlarmRadius;
	}

	/** Whether a guard of a building may stand at a place Distance from its post (cm) when it stands Current from it:
	 *  inside its hold (HoldRadius, 0 or less: anywhere), or at least no farther out than it is (a guard pushed out walks
	 *  back) */
	inline bool KeepsToHold(double Distance, double Current, float HoldRadius)
	{
		return HoldRadius <= 0.0f || Distance <= HoldRadius || Distance <= Current;
	}
}
