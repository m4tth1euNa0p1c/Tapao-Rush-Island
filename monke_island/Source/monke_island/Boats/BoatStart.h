// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Math/RandomStream.h"

/**
 *  Pure rules of the start in the boat (T-093), shared by UBoatStartSubsystem and the specs
 *  (monke_island.Boats.Start.*): fragment id from the URL, choice of the arrival point, seat of each player.
 */
namespace BoatStart
{
	/** URL option naming the fragment of the expedition (?Island=FRG_Raraka_01), set by the travel (T-094) */
	inline const TCHAR* IslandOption = TEXT("Island");

	/** URL option forcing the seed of the random choice (?Seed=1234), for a reproducible start */
	inline const TCHAR* SeedOption = TEXT("Seed");

	/**
	 *  Fragment id read from an untrusted URL option: letters, digits, '_' and '-' only, at most MaxLength
	 *  characters; NAME_None when empty or invalid (SEC: never build a name or a path from raw input).
	 */
	inline FName SanitizeFragmentId(const FString& Raw, int32 MaxLength)
	{
		const FString Trimmed = Raw.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Len() > MaxLength)
		{
			return NAME_None;
		}
		for (const TCHAR Char : Trimmed)
		{
			if (!FChar::IsAlnum(Char) && Char != TEXT('_') && Char != TEXT('-'))
			{
				return NAME_None;
			}
		}
		return FName(*Trimmed);
	}

	/** Seed read from an untrusted URL option; false when absent or not a plain integer */
	inline bool ParseSeed(const FString& Raw, int32& OutSeed)
	{
		const FString Trimmed = Raw.TrimStartAndEnd();
		if (Trimmed.IsEmpty() || Trimmed.Len() > 11 || !Trimmed.IsNumeric() || Trimmed.Contains(TEXT(".")))
		{
			return false;
		}
		const int64 Value = FCString::Atoi64(*Trimmed);
		if (Value < MIN_int32 || Value > MAX_int32)
		{
			return false;
		}
		OutSeed = static_cast<int32>(Value);
		return true;
	}

	/**
	 *  Index of the arrival point to use among PointIds (fragment id of each point, in a stable order).
	 *  Wanted set: a point of that fragment (seeded pick when the fragment has several); none of them and
	 *  bRandomWhenUnknown: any point. Wanted unset: any point. INDEX_NONE when there is no point to use.
	 */
	inline int32 ChooseArrivalIndex(TConstArrayView<FName> PointIds, FName Wanted, int32 Seed, bool bRandomWhenUnknown)
	{
		if (PointIds.IsEmpty())
		{
			return INDEX_NONE;
		}
		FRandomStream Stream(Seed);
		if (!Wanted.IsNone())
		{
			TArray<int32, TInlineAllocator<8>> Matching;
			for (int32 Index = 0; Index < PointIds.Num(); ++Index)
			{
				if (PointIds[Index] == Wanted)
				{
					Matching.Add(Index);
				}
			}
			if (!Matching.IsEmpty())
			{
				return Matching[Stream.RandRange(0, Matching.Num() - 1)];
			}
			if (!bRandomWhenUnknown)
			{
				return INDEX_NONE;
			}
		}
		return Stream.RandRange(0, PointIds.Num() - 1);
	}

	/**
	 *  Seat given to the next player boarding: the first free seat of Order (seat indices, the first one is
	 *  the rower's), then any free seat in ascending order; INDEX_NONE when every seat is taken (standing).
	 */
	inline int32 ChooseSeat(TConstArrayView<bool> Occupied, TConstArrayView<int32> Order)
	{
		for (const int32 Seat : Order)
		{
			if (Occupied.IsValidIndex(Seat) && !Occupied[Seat])
			{
				return Seat;
			}
		}
		for (int32 Seat = 0; Seat < Occupied.Num(); ++Seat)
		{
			if (!Occupied[Seat])
			{
				return Seat;
			}
		}
		return INDEX_NONE;
	}

	/** Spot (boat local space) of the StandingIndex-th player standing aboard; the spots are reused in turn */
	inline FVector StandingSpot(TConstArrayView<FVector> Spots, int32 StandingIndex)
	{
		return Spots.IsEmpty() ? FVector(0.0, 0.0, 100.0) : Spots[FMath::Abs(StandingIndex) % Spots.Num()];
	}
}
