// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Network of the giant crab (T-304, T-305; review T-268 R-23 to R-25), pure and kept apart for the specs
 *  (monke_island.Creatures.GiantCrab.Net).
 *  - A client plays each action of the crab from the server's start (StartServerTime), as far into its clip as the
 *    action is old: a late arrival or a crab back in view does not replay it from the start.
 *  - A distant player sees the crab late (one way of the ping, plus the smoothing of its movement) and its shot reaches
 *    the server one way later still: the server keeps a short history of the weak points and judges the shot against
 *    where they were then, within bounds of the rules (never more than LagCompensationMaxSeconds back).
 */
namespace GiantCrabLag
{
	/** Seconds into its clip a machine starts an action Age seconds old, played at Rate: a looping clip wraps, another
	 *  one holds its end (the last pose of a death); 0 for a fresh action */
	inline float ClipStartTime(float Age, float Rate, float Length, bool bLoop)
	{
		if (Length <= 0.0f || Age <= 0.0f)
		{
			return 0.0f;
		}
		const float Time = Age * FMath::Max(0.05f, Rate);
		return bLoop ? FMath::Fmod(Time, Length) : FMath::Min(Time, Length);
	}

	/** Index of the first moment (sorted times, s after the action began) a machine still plays when it starts an
	 *  action Age seconds old: the ones more than Grace seconds behind are dropped (a crab that died long ago gives no
	 *  death cry); Times.Num() when none is left */
	inline int32 FirstFreshMoment(TConstArrayView<float> Times, float Age, float Grace)
	{
		int32 Index = 0;
		while (Index < Times.Num() && Times[Index] < Age - FMath::Max(0.0f, Grace))
		{
			++Index;
		}
		return Index;
	}

	/** How far back the server judges a shot of a player with this round trip (s): the round trip plus the delay of its
	 *  view, at most MaxRewind; 0 for the host (no round trip) and without compensation (MaxRewind 0) */
	inline float RewindSeconds(float RoundTripSeconds, float ViewDelay, float MaxRewind)
	{
		if (RoundTripSeconds <= 0.0f || MaxRewind <= 0.0f)
		{
			return 0.0f;
		}
		return FMath::Min(RoundTripSeconds + FMath::Max(0.0f, ViewDelay), MaxRewind);
	}

	/** A window [From, To] of the server clock (the claw stuck in the sand after a slam), seen by a player Rewind late:
	 *  a blow that reaches the server at Now counts from its opening until Rewind + Slack after its end. Without
	 *  rewind (the host), exactly the window */
	inline bool IsInLateWindow(double Now, double From, double To, float Rewind, float Slack)
	{
		if (To < From)
		{
			return false;
		}
		const double Late = Rewind > 0.0f ? static_cast<double>(Rewind) + FMath::Max(0.0f, Slack) : 0.0;
		return Now >= From && Now <= To + Late;
	}

	/** True when the segment RayStart-RayEnd passes within Radius of the path of a point (its positions in time
	 *  order: each step between two of them counts, a single position too) */
	inline bool RayNearPath(const FVector& RayStart, const FVector& RayEnd, TConstArrayView<FVector> Path, float Radius)
	{
		const double RadiusSq = FMath::Square(static_cast<double>(FMath::Max(0.0f, Radius)));
		if (Path.Num() == 1)
		{
			return FMath::PointDistToSegmentSquared(Path[0], RayStart, RayEnd) <= RadiusSq;
		}
		for (int32 Index = 1; Index < Path.Num(); ++Index)
		{
			FVector OnRay;
			FVector OnPath;
			FMath::SegmentDistToSegmentSafe(RayStart, RayEnd, Path[Index - 1], Path[Index], OnRay, OnPath);
			if (FVector::DistSquared(OnRay, OnPath) <= RadiusSq)
			{
				return true;
			}
		}
		return false;
	}
}

/** Recent positions of a few points (the crab's weak points, server): a ring of samples, the oldest dropped */
struct FGiantCrabPoseHistory
{
	/** Empties it for InPoints points and InCapacity samples */
	void Reset(int32 InPoints, int32 InCapacity)
	{
		Points = FMath::Max(0, InPoints);
		Capacity = FMath::Max(1, InCapacity);
		Times.SetNumZeroed(Capacity);
		Locations.SetNumZeroed(Capacity * Points);
		Head = 0;
		Count = 0;
	}

	/** Points it was reset for */
	int32 GetPoints() const { return Points; }

	/** Samples kept */
	int32 Num() const { return Count; }

	/** Adds the positions of every point at Time (in the order of the reset; the times only grow) */
	void Add(double Time, TConstArrayView<FVector> InLocations)
	{
		if (Points <= 0 || InLocations.Num() != Points)
		{
			return;
		}
		Times[Head] = Time;
		for (int32 Point = 0; Point < Points; ++Point)
		{
			Locations[Head * Points + Point] = InLocations[Point];
		}
		Head = (Head + 1) % Capacity;
		Count = FMath::Min(Count + 1, Capacity);
	}

	/** Path of a point over [From, To]: its samples in that span, with the one just before and the one just after
	 *  so that the steps across its edges count; a span after the newest sample gives the newest, one before the
	 *  oldest gives the oldest. False when it holds nothing */
	bool GetPath(int32 Point, double From, double To, TArray<FVector>& OutPath) const
	{
		OutPath.Reset();
		if (Count <= 0 || Point < 0 || Point >= Points)
		{
			return false;
		}
		int32 Before = INDEX_NONE;
		for (int32 Step = 0; Step < Count; ++Step)
		{
			const int32 Slot = (Head - Count + Step + Capacity) % Capacity;
			const double Time = Times[Slot];
			if (Time < From)
			{
				Before = Slot;
				continue;
			}
			if (OutPath.IsEmpty() && Before != INDEX_NONE)
			{
				OutPath.Add(Locations[Before * Points + Point]);
			}
			OutPath.Add(Locations[Slot * Points + Point]);
			if (Time > To)
			{
				return true;
			}
		}
		if (OutPath.IsEmpty() && Before != INDEX_NONE)
		{
			OutPath.Add(Locations[Before * Points + Point]);
		}
		return !OutPath.IsEmpty();
	}

private:

	int32 Points = 0;
	int32 Capacity = 1;
	int32 Head = 0;
	int32 Count = 0;
	TArray<double> Times;
	TArray<FVector> Locations;
};
