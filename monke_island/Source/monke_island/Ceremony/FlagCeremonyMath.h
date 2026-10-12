// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure helpers of the flag ceremony (AFlagCeremonySite, the villagers around it), tested by monke_island.Ceremony.Flag */
namespace FlagCeremonyMath
{
	/** Share of the raising done at Now, from 0 at StartTime to 1 after Seconds */
	inline float Progress(double StartTime, double Now, float Seconds)
	{
		return FMath::Clamp(static_cast<float>((Now - StartTime) / FMath::Max(Seconds, KINDA_SMALL_NUMBER)), 0.0f, 1.0f);
	}

	/** Height of the bottom of the flag on its pole: hauled at a steady pace on the halyard */
	inline float FlagHeight(float InProgress, float StartHeight, float EndHeight)
	{
		return FMath::Lerp(StartHeight, EndHeight, FMath::Clamp(InProgress, 0.0f, 1.0f));
	}

	/** Seconds left, never below zero */
	inline float SecondsLeft(float InProgress, float Seconds)
	{
		return FMath::Max(0.0f, (1.0f - FMath::Clamp(InProgress, 0.0f, 1.0f)) * Seconds);
	}

	/** True when Location stands in the ceremony area: within Radius on the ground plane, HalfHeight above or below */
	inline bool IsInArea(const FVector& Center, const FVector& Location, float Radius, float HalfHeight)
	{
		return FVector::DistSquared2D(Location, Center) <= FMath::Square(static_cast<double>(Radius))
			&& FMath::Abs(Location.Z - Center.Z) <= HalfHeight;
	}

	/** True when the officiant moved more than Tolerance from where he started (on the ground plane; a jump counts
	 *  once it lifts him more than four times the tolerance, at least 30 cm) */
	inline bool HasMoved(const FVector& Start, const FVector& Now, float Tolerance)
	{
		return FVector::DistSquared2D(Start, Now) > FMath::Square(static_cast<double>(Tolerance))
			|| FMath::Abs(Now.Z - Start.Z) > FMath::Max(30.0f, Tolerance * 4.0f);
	}

	/** Angle of slot Index of Count on a ring, from Phase (degrees) */
	inline float SlotAngle(int32 Index, int32 Count, float PhaseDegrees)
	{
		return PhaseDegrees + 360.0f * static_cast<float>(Index) / static_cast<float>(FMath::Max(Count, 1));
	}

	/** Point of a ring around Center at this angle (degrees, from +X towards +Y), at the height of Center */
	inline FVector RingPoint(const FVector& Center, float AngleDegrees, float Radius)
	{
		const float Radians = FMath::DegreesToRadians(AngleDegrees);
		return Center + FVector(FMath::Cos(Radians), FMath::Sin(Radians), 0.0f) * Radius;
	}

	/** Point pushed out of the disc of Clearance around Cairn, along the line from the cairn (unchanged outside) */
	inline FVector KeepOffCairn(const FVector& Point, const FVector& Cairn, float Clearance)
	{
		FVector Away = Point - Cairn;
		Away.Z = 0.0;
		const double Distance = Away.Size();
		if (Distance >= Clearance)
		{
			return Point;
		}
		const FVector Direction = Distance > KINDA_SMALL_NUMBER ? Away / Distance : FVector::ForwardVector;
		return FVector(Cairn.X, Cairn.Y, Point.Z) + Direction * Clearance;
	}

	/** Angle of a point around Center (degrees, from +X towards +Y) */
	inline float AngleAround(const FVector& Center, const FVector& Point)
	{
		return FMath::RadiansToDegrees(FMath::Atan2(Point.Y - Center.Y, Point.X - Center.X));
	}

	/** Shortest turn from one angle to another (degrees, in [-180, 180]) */
	inline float TurnBetween(float FromDegrees, float ToDegrees)
	{
		return FMath::UnwindDegrees(ToDegrees - FromDegrees);
	}

	/** Way to a slot of the ring around Center that never cuts through it: first to the outer lane on the walker's
	 *  side, then along the lane (one point every StepDegrees at most) round to the slot, then in to the slot. A
	 *  walker already on the lane or inside the ring near its slot goes straight */
	inline TArray<FVector> RingApproach(const FVector& Center, const FVector& From, float SlotDegrees, float SlotRadius, float LaneRadius, float StepDegrees)
	{
		TArray<FVector> Points;
		const FVector Slot = RingPoint(Center, SlotDegrees, SlotRadius);
		const float FromDegrees = AngleAround(Center, From);
		const float Turn = TurnBetween(FromDegrees, SlotDegrees);
		const int32 Steps = FMath::Max(1, FMath::CeilToInt(FMath::Abs(Turn) / FMath::Max(StepDegrees, 5.0f)));
		// a short turn: the straight line passes outside the ring anyway
		if (FMath::Abs(Turn) > StepDegrees)
		{
			for (int32 Step = 0; Step <= Steps; ++Step)
			{
				Points.Add(RingPoint(Center, FromDegrees + Turn * static_cast<float>(Step) / static_cast<float>(Steps), LaneRadius));
			}
		}
		Points.Add(Slot);
		return Points;
	}

	/** "1:05", "0:42" */
	inline FString Countdown(float Seconds)
	{
		const int32 Whole = FMath::CeilToInt(FMath::Max(0.0f, Seconds));
		return FString::Printf(TEXT("%d:%02d"), Whole / 60, Whole % 60);
	}
}
