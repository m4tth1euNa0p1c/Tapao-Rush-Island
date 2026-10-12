// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Village/VillageHouseKit.h"

/** Pure helpers of the routine of the villagers inside their house (ATikiVillager), on the waypoints of a kit;
 *  tested by monke_island.Village.HouseNav */
namespace VillageHouseNav
{
	/** True when Local (house frame, cm) lies in the inside of the house grown by Margin (an invalid box: never) */
	inline bool IsInside(const FBox& Interior, const FVector& Local, float Margin)
	{
		return Interior.IsValid && Interior.ExpandBy(FVector(Margin)).IsInsideOrOn(Local);
	}

	/** Index of the node nearest to Local (house frame); a height difference counts four times (the doma lies
	 *  45 cm under the raised floor); INDEX_NONE without nodes */
	inline int32 NearestNode(const TArray<FVillageHouseNode>& Nodes, const FVector& Local)
	{
		int32 Best = INDEX_NONE;
		double BestDistance = TNumericLimits<double>::Max();
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			const FVector Delta = Nodes[Index].Location - Local;
			const double Distance = FVector2D(Delta.X, Delta.Y).SizeSquared() + FMath::Square(Delta.Z * 4.0);
			if (Distance < BestDistance)
			{
				BestDistance = Distance;
				Best = Index;
			}
		}
		return Best;
	}

	/** Neighbours of every node, the links taken both ways, out-of-range links dropped */
	inline TArray<TArray<int32>> Neighbours(const TArray<FVillageHouseNode>& Nodes)
	{
		TArray<TArray<int32>> Out;
		Out.SetNum(Nodes.Num());
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			for (const int32 Link : Nodes[Index].Links)
			{
				if (Link != Index && Nodes.IsValidIndex(Link))
				{
					Out[Index].AddUnique(Link);
					Out[Link].AddUnique(Index);
				}
			}
		}
		return Out;
	}

	/** Shortest way along the links from Start to Goal, both included (Dijkstra on a few nodes); empty when one is
	 *  out of range or Goal cannot be reached */
	inline TArray<int32> FindPath(const TArray<FVillageHouseNode>& Nodes, int32 Start, int32 Goal)
	{
		TArray<int32> Path;
		if (!Nodes.IsValidIndex(Start) || !Nodes.IsValidIndex(Goal))
		{
			return Path;
		}
		const TArray<TArray<int32>> Links = Neighbours(Nodes);
		TArray<double> Cost;
		Cost.Init(TNumericLimits<double>::Max(), Nodes.Num());
		TArray<int32> From;
		From.Init(INDEX_NONE, Nodes.Num());
		TArray<bool> Done;
		Done.Init(false, Nodes.Num());
		Cost[Start] = 0.0;
		for (int32 Step = 0; Step < Nodes.Num(); ++Step)
		{
			int32 Current = INDEX_NONE;
			for (int32 Index = 0; Index < Nodes.Num(); ++Index)
			{
				if (!Done[Index] && Cost[Index] < TNumericLimits<double>::Max() && (Current == INDEX_NONE || Cost[Index] < Cost[Current]))
				{
					Current = Index;
				}
			}
			if (Current == INDEX_NONE || Current == Goal)
			{
				break;
			}
			Done[Current] = true;
			for (const int32 Next : Links[Current])
			{
				const double Through = Cost[Current] + FVector::Dist(Nodes[Current].Location, Nodes[Next].Location);
				if (Through < Cost[Next])
				{
					Cost[Next] = Through;
					From[Next] = Current;
				}
			}
		}
		if (Cost[Goal] == TNumericLimits<double>::Max())
		{
			return Path;
		}
		for (int32 Node = Goal; Node != INDEX_NONE; Node = From[Node])
		{
			Path.Insert(Node, 0);
		}
		return Path;
	}

	/** Node reachable from Start farthest from Threat (house frame): where a villager cornered in its house backs off */
	inline int32 FarthestNode(const TArray<FVillageHouseNode>& Nodes, int32 Start, const FVector& Threat)
	{
		int32 Best = Nodes.IsValidIndex(Start) ? Start : INDEX_NONE;
		double BestDistance = Best != INDEX_NONE ? FVector::DistSquared2D(Nodes[Best].Location, Threat) : -1.0;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			const double Distance = FVector::DistSquared2D(Nodes[Index].Location, Threat);
			if (Distance > BestDistance && !FindPath(Nodes, Start, Index).IsEmpty())
			{
				BestDistance = Distance;
				Best = Index;
			}
		}
		return Best;
	}

	/** Nodes with an activity (points of the routine) */
	inline TArray<int32> ActivityNodes(const TArray<FVillageHouseNode>& Nodes)
	{
		TArray<int32> Out;
		for (int32 Index = 0; Index < Nodes.Num(); ++Index)
		{
			if (!Nodes[Index].Activity.IsNone())
			{
				Out.Add(Index);
			}
		}
		return Out;
	}
}
