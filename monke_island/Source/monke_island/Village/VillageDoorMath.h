// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Village/VillageHouseKit.h"

/** Pure helpers of the house doors and contents (UVillageDoorsComponent, AVillageSpawner), tested by monke_island.Village.Doors */
namespace VillageDoorMath
{
	/** True when a pawn at Local (house frame, cm) with this radius stands near the doorway: within Depth of the plane
	 *  of the bay, in front or behind, no farther than SideMargin beyond its sides, between below its track and its top */
	inline bool IsNearDoorway(const FVillageHouseDoorway& Doorway, const FVector& Local, float Radius, float Depth, float SideMargin)
	{
		// X along the track, Y across the bay
		const FVector Offset = FRotator(0.0f, Doorway.Yaw, 0.0f).UnrotateVector(Local - Doorway.Center);
		return FMath::Abs(Offset.X) <= Doorway.HalfWidth + SideMargin + Radius
			&& FMath::Abs(Offset.Y) <= Depth + Radius
			&& Offset.Z >= -Depth && Offset.Z <= Doorway.Height + Radius;
	}

	/** True when the segment from Start to End crosses or lies in the box [Min, Max] (slab test) */
	inline bool SegmentHitsBox(const FVector& Start, const FVector& End, const FVector& Min, const FVector& Max)
	{
		double Enter = 0.0;
		double Leave = 1.0;
		const FVector Delta = End - Start;
		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			const double From = Start[Axis];
			const double Step = Delta[Axis];
			if (FMath::IsNearlyZero(Step))
			{
				if (From < Min[Axis] || From > Max[Axis])
				{
					return false;
				}
				continue;
			}
			double A = (Min[Axis] - From) / Step;
			double B = (Max[Axis] - From) / Step;
			if (A > B)
			{
				Swap(A, B);
			}
			Enter = FMath::Max(Enter, A);
			Leave = FMath::Min(Leave, B);
			if (Enter > Leave)
			{
				return false;
			}
		}
		return true;
	}

	/** Seconds ahead a doorway opens for a moving pawn (T-301, review R-08): the share of the slide the panel must have
	 *  done, the look interval, a margin, and the half round trip of a client's pawn the server sees late (capped) */
	inline float LeadSeconds(float HalfRoundTrip, float CheckInterval, float SlideSeconds, float SlideShare, float ExtraSeconds, float MaxPingSeconds)
	{
		return FMath::Clamp(HalfRoundTrip, 0.0f, FMath::Max(MaxPingSeconds, 0.0f)) + FMath::Max(CheckInterval, 0.0f)
			+ FMath::Max(SlideSeconds, 0.0f) * FMath::Clamp(SlideShare, 0.0f, 1.0f) + FMath::Max(ExtraSeconds, 0.0f);
	}

	/** IsNearDoorway now, or anywhere on the way the pawn covers in LeadSeconds at its horizontal LocalVelocity (house
	 *  frame, cm/s), at most MaxLeadDistance ahead: a running pawn finds the door opening, not still shut */
	inline bool IsNearDoorwayAhead(const FVillageHouseDoorway& Doorway, const FVector& Local, const FVector& LocalVelocity, float Radius, float Depth,
		float SideMargin, float InLeadSeconds, float MaxLeadDistance)
	{
		if (IsNearDoorway(Doorway, Local, Radius, Depth, SideMargin))
		{
			return true;
		}
		const FVector Ahead = (FVector(LocalVelocity.X, LocalVelocity.Y, 0.0) * FMath::Max(InLeadSeconds, 0.0f)).GetClampedToMaxSize(FMath::Max(MaxLeadDistance, 0.0f));
		if (Ahead.IsNearlyZero())
		{
			return false;
		}
		// the trigger box of IsNearDoorway, in the doorway frame (X along the track, Y across the bay)
		const FRotator Frame(0.0f, Doorway.Yaw, 0.0f);
		const FVector Start = Frame.UnrotateVector(Local - Doorway.Center);
		const FVector End = Start + Frame.UnrotateVector(Ahead);
		const FVector Min(-(Doorway.HalfWidth + SideMargin + Radius), -(Depth + Radius), -Depth);
		const FVector Max(Doorway.HalfWidth + SideMargin + Radius, Depth + Radius, Doorway.Height + Radius);
		return SegmentHitsBox(Start, End, Min, Max);
	}

	/** Slow start and stop of a slide (smoothstep of Alpha in [0, 1]) */
	inline float Ease(float Alpha)
	{
		const float A = FMath::Clamp(Alpha, 0.0f, 1.0f);
		return A * A * (3.0f - 2.0f * A);
	}

	/** Pivot of a panel in the house frame when it has slid Alpha of its way (0 closed, 1 fully open) */
	inline FVector PanelLocation(const FVillageHousePanel& Panel, float Alpha)
	{
		return Panel.ClosedLocation + Panel.Travel * Ease(Alpha);
	}

	/** Yaw of a panel in the house frame when it has opened Alpha of its way: a hinged leaf turns, a sliding one not */
	inline float PanelYaw(const FVillageHousePanel& Panel, float Alpha)
	{
		return Panel.Yaw + Panel.OpenYaw * Ease(Alpha);
	}

	/** Alpha after DeltaSeconds toward Target, a full slide taking SlideSeconds; never overshoots */
	inline float StepToward(float Alpha, float Target, float DeltaSeconds, float SlideSeconds)
	{
		const float Step = FMath::Max(DeltaSeconds, 0.0f) / FMath::Max(SlideSeconds, KINDA_SMALL_NUMBER);
		return Alpha < Target ? FMath::Min(Alpha + Step, Target) : FMath::Max(Alpha - Step, Target);
	}

	/** A doorway stays open while occupied and CloseDelay after the last time a pawn was near it (< 0: never) */
	inline bool ShouldBeOpen(bool bOccupied, double LastOccupied, double Now, float CloseDelay)
	{
		return bOccupied || (LastOccupied >= 0.0 && Now - LastOccupied < CloseDelay);
	}

	/** Count distinct indices drawn among [0, Num) (as many as there are when Count >= Num) */
	inline TArray<int32> PickDistinct(int32 Num, int32 Count, FRandomStream& Stream)
	{
		TArray<int32> Indices;
		Indices.Reserve(FMath::Max(Num, 0));
		for (int32 Index = 0; Index < Num; ++Index)
		{
			Indices.Add(Index);
		}
		const int32 Taken = FMath::Clamp(Count, 0, Indices.Num());
		// partial Fisher-Yates: the first Taken entries are the draw
		for (int32 Index = 0; Index < Taken; ++Index)
		{
			Indices.Swap(Index, Stream.RandRange(Index, Indices.Num() - 1));
		}
		Indices.SetNum(Taken);
		return Indices;
	}

	/** Actor location that rests the bottom of its bounds on a surface, Clearance above it */
	inline FVector RestOnSurface(const FVector& ActorLocation, double BoundsBottom, double SurfaceZ, float Clearance)
	{
		return ActorLocation + FVector(0.0, 0.0, SurfaceZ + FMath::Max(0.0f, Clearance) - BoundsBottom);
	}

	/** Cell of a uniform 2D grid */
	inline FIntPoint CellOf(const FVector2D& Point, double CellSize)
	{
		const double Size = FMath::Max(CellSize, 1.0);
		return FIntPoint(FMath::FloorToInt32(Point.X / Size), FMath::FloorToInt32(Point.Y / Size));
	}
}
