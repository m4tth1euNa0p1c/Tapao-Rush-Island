// monke_island

#pragma once

#include "CoreMinimal.h"
#include "TikiLimbs.generated.h"

/** What losing a limb does to a living tiki (T-167) */
UENUM(BlueprintType)
enum class ETikiLimbLoss : uint8
{
	/** Nothing more than the stump */
	None,
	/** The head: the creature dies */
	Kills,
	/** An arm: the weapon in its hand falls (DroppedWeaponClass), the blows are weaker (MaimedDamageScale) */
	Arm,
	/** A leg: the creature limps, much slower (LegLossSpeedScale) */
	Leg
};

/** One limb a shot can tear off a tiki (T-167): its first bone and its own health, apart from the body's */
USTRUCT(BlueprintType)
struct FTikiLimb
{
	GENERATED_BODY()

	/** Name, for the logs (Head, Arm_L, Arm_R, Leg_L, Leg_R: the modules of DA_<Mob>_DismemberSet) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
	FName Part;

	/** First bone of the limb: a hit on it or on a bone under it counts for the limb, the tear cuts there */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
	FName Bone;

	/** Damage the limb takes before it is torn off (from the hits on its bones, times the LimbDamageScale of the
	 *  weapon), whether the creature survives or not */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb", meta=(ClampMin="1"))
	float Health = 25.0f;

	/** What losing it does */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Limb")
	ETikiLimbLoss Loss = ETikiLimbLoss::None;
};

/** Pure rules of the limb health (T-167), tested without a world */
namespace TikiLimbs
{
	/** Limb of a bone: the limb whose first bone is this bone or one of its ancestors (Parents by bone index), the
	 *  nearest such ancestor winning; INDEX_NONE for the body (torso, pelvis) or an unknown bone */
	inline int32 LimbOfBone(int32 Bone, const TArray<int32>& LimbBones, const TArray<int32>& Parents)
	{
		for (int32 Current = Bone, Guard = 0; Current != INDEX_NONE && Guard < 256; ++Guard)
		{
			const int32 Limb = LimbBones.Find(Current);
			if (Limb != INDEX_NONE)
			{
				return Limb;
			}
			Current = Parents.IsValidIndex(Current) ? Parents[Current] : INDEX_NONE;
		}
		return INDEX_NONE;
	}

	/** Adds the damage of a hit to its limb (Damage by limb, grown as needed); returns true when the limb crosses its
	 *  health with this hit (it was below, it is at or above now) */
	inline bool AddDamage(TArray<float>& Damage, int32 Limb, float Amount, float Health)
	{
		if (Limb < 0 || Amount <= 0.0f)
		{
			return false;
		}
		if (Damage.Num() <= Limb)
		{
			Damage.SetNumZeroed(Limb + 1);
		}
		const float Before = Damage[Limb];
		Damage[Limb] = Before + Amount;
		return Before < Health && Damage[Limb] >= Health;
	}

	/** Speed left to a creature that lost legs: OneLeg for one, TwoLegs for both or more */
	inline float SpeedScale(int32 LegsLost, float OneLeg, float TwoLegs)
	{
		return LegsLost <= 0 ? 1.0f : (LegsLost == 1 ? OneLeg : TwoLegs);
	}

	/** Limbs a close lethal shot still shreds after those its hits tore: at least 0 */
	inline int32 ShredLeft(int32 Wanted, int32 TornByHits)
	{
		return FMath::Max(0, Wanted - TornByHits);
	}
}
