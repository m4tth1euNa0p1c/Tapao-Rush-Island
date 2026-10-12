// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the loot crate (T-258), tested without a world (monke_island.LootCrate.*) */
namespace LootCrateMath
{
	/** True when one more object of ItemKg goes in: fewer than MaxItems inside and the contents within MaxContentKg */
	inline bool CanAccept(int32 Count, float ContentKg, float ItemKg, int32 MaxItems, float MaxContentKg)
	{
		return Count >= 0 && Count < MaxItems && ItemKg >= 0.0f && ContentKg + ItemKg <= MaxContentKg + UE_KINDA_SMALL_NUMBER;
	}

	/** Mass of the crate with what it holds (kg) */
	inline float TotalMassKg(float EmptyKg, float ContentKg)
	{
		return FMath::Max(0.0f, EmptyKg) + FMath::Max(0.0f, ContentKg);
	}

	/** True when a box of ItemSize fits in a box of InnerSize, turned any way along its axes (full sizes, cm) */
	inline bool FitsInside(const FVector& ItemSize, const FVector& InnerSize)
	{
		TArray<double, TInlineAllocator<3>> Item = { FMath::Abs(ItemSize.X), FMath::Abs(ItemSize.Y), FMath::Abs(ItemSize.Z) };
		TArray<double, TInlineAllocator<3>> Inner = { FMath::Abs(InnerSize.X), FMath::Abs(InnerSize.Y), FMath::Abs(InnerSize.Z) };
		Item.Sort();
		Inner.Sort();
		return Item[0] <= Inner[0] && Item[1] <= Inner[1] && Item[2] <= Inner[2];
	}

	/**
	 *  Where the Index-th of Count objects comes out when the crate is emptied, in the frame of the crate turned toward the
	 *  player (+X toward the player, cm): a fan SpreadDeg apart at DistanceCm, a new row RowStepCm further every PerRow
	 */
	inline FVector2D EmptySpot(int32 Index, int32 Count, float DistanceCm, float SpreadDeg, int32 PerRow, float RowStepCm)
	{
		const int32 RowSize = FMath::Max(1, PerRow);
		const int32 Row = FMath::Max(0, Index) / RowSize;
		const int32 InRow = FMath::Clamp(Count - Row * RowSize, 1, RowSize);
		const int32 Column = FMath::Max(0, Index) % RowSize;
		const float Angle = FMath::DegreesToRadians((static_cast<float>(Column) - 0.5f * static_cast<float>(InRow - 1)) * SpreadDeg);
		const float Radius = DistanceCm + static_cast<float>(Row) * RowStepCm;
		return FVector2D(FMath::Cos(Angle), FMath::Sin(Angle)) * Radius;
	}

	/**
	 *  Opening of the lid in [0, 1] at Time seconds of a pulse: opens over OpenSeconds, stays open HoldSeconds, shuts over
	 *  CloseSeconds (smoothed); negative when the pulse is over
	 */
	inline float LidOpenFraction(float Time, float OpenSeconds, float HoldSeconds, float CloseSeconds)
	{
		const float Open = FMath::Max(OpenSeconds, UE_KINDA_SMALL_NUMBER);
		const float Hold = FMath::Max(HoldSeconds, 0.0f);
		const float Close = FMath::Max(CloseSeconds, UE_KINDA_SMALL_NUMBER);
		if (Time < 0.0f || Time >= Open + Hold + Close)
		{
			return -1.0f;
		}
		const float Alpha = Time < Open ? Time / Open : (Time < Open + Hold ? 1.0f : 1.0f - (Time - Open - Hold) / Close);
		return FMath::SmoothStep(0.0f, 1.0f, Alpha);
	}
}
