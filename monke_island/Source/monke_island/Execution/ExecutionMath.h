// monke_island

#pragma once

#include "CoreMinimal.h"

/** Pure rules of the execution scene (tested by monke_island.Execution.Layout) */
namespace ExecutionMath
{
	/** What the scene shows at Time (s from the end of the day) */
	enum class EPhase : uint8
	{
		/** The end of the day and its title, the crew where it stood */
		Title,
		/** The crew on the plank, the squad on deck, the camera on them */
		Scene,
		/** Black, the end screen next */
		Over
	};

	inline EPhase PhaseAt(float Time, float PlaceAt, float EndAt)
	{
		return Time < PlaceAt ? EPhase::Title : (Time < EndAt ? EPhase::Scene : EPhase::Over);
	}

	/** Feet of the Index-th of the crew on the plank (hull frame): along the plank from its start, on its top */
	inline FVector PlankSpot(const FVector& PlankStart, const FVector& PlankDirection, float FirstSpotCm, float SpacingCm, float PlankLength, int32 Index)
	{
		const FVector Along = PlankDirection.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : PlankDirection.GetSafeNormal2D();
		// never past the end of the board
		const float Distance = FMath::Min(FirstSpotCm + FMath::Max(Index, 0) * SpacingCm, FMath::Max(PlankLength - 25.0f, 0.0f));
		return PlankStart + Along * Distance;
	}

	/** Feet of the Index-th soldier of a line of Count abreast across Facing, centred on Centre */
	inline FVector SquadSpot(const FVector& Centre, const FVector& Facing, float SpacingCm, int32 Index, int32 Count)
	{
		const FVector Ahead = Facing.GetSafeNormal2D().IsNearlyZero() ? FVector::ForwardVector : Facing.GetSafeNormal2D();
		const FVector Side(-Ahead.Y, Ahead.X, 0.0);
		const float Offset = (static_cast<float>(Index) - 0.5f * static_cast<float>(FMath::Max(Count, 1) - 1)) * SpacingCm;
		return Centre + Side * Offset;
	}

	/** Victim of the Index-th soldier: the crew shared out in turn (none without a crew) */
	inline int32 VictimOf(int32 SoldierIndex, int32 VictimCount)
	{
		return VictimCount > 0 ? FMath::Max(SoldierIndex, 0) % VictimCount : INDEX_NONE;
	}

	/** 0 → 1 → 0 opacity of a title shown from Start for Seconds (0.5 s in, 0.8 s out) */
	inline float TitleAlpha(float Time, float Start, float Seconds)
	{
		const float Age = Time - Start;
		if (Age < 0.0f || Age > Seconds)
		{
			return 0.0f;
		}
		return FMath::Clamp(FMath::Min(Age / 0.5f, (Seconds - Age) / 0.8f), 0.0f, 1.0f);
	}
}
