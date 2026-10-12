// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "TikiBloodSpecies.generated.h"

class UNiagaraSystem;
class USkeleton;

/**
 *  Blood of one species (T-162): its colour and the copies of the blood systems drawn in it. A body bleeds the blood of
 *  the species whose skeleton its mesh uses (the creature, its corpse, its cut pieces, a soldier), so one table covers
 *  every source without knowing their classes.
 */
USTRUCT(BlueprintType)
struct FTikiBloodSpecies
{
	GENERATED_BODY()

	/** Name in the logs and the docs (Bean, Villager, Coco, Crab, Ash) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	FName Name;

	/** Skeletons of the species: a body whose mesh uses one of them bleeds this blood */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TArray<TSoftObjectPtr<USkeleton>> Skeletons;

	/** Colour of the splats and pools (decal parameter BloodColor, linear) and of the BloodColor user parameter of the
	 *  systems that have one; the drops take theirs from the material of their systems */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	FLinearColor Color = FLinearColor(0.30f, 0.005f, 0.004f, 1.0f);

	/** Spray of a hit in this blood (a copy of NS_Blood_MusketImpact with the particle material of the species);
	 *  empty = the system of the rules (DA_TikiRules MusketImpactSystem) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> MusketImpactSystem;

	/** Pulsed jet out of a cut in this blood; empty = SliceSpraySystem of the rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> SliceSpraySystem;

	/** Chunks and splashes in this blood; empty = ChunksSystem of the rules */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TSoftObjectPtr<UNiagaraSystem> ChunksSystem;
};

/**
 *  Blood of each species (DA_BloodSpecies, T-162): loaded by UTikiBloodSubsystem from its SpeciesRulesPath
 *  (DefaultGame.ini). A body of no species listed bleeds the blood of its rules (DA_TikiRules Blood: the dark red of the
 *  humans and beans).
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UTikiBloodSpeciesRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Species and their blood */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood")
	TArray<FTikiBloodSpecies> Species;

	/** A spray without a known body (chunks of a burst) takes the species of the blood spawned within this distance in
	 *  the same frame (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Blood", meta=(ClampMin="0", Units="cm"))
	float SameWoundRadius = 300.0f;
};

/** Pure rules of the species table (monke_island.Tiki.BloodSpecies) */
namespace TikiBloodSpecies
{
	/** Index of the species of a skeleton (by asset path: no load), INDEX_NONE when none lists it */
	inline int32 Find(const TArray<FTikiBloodSpecies>& Species, const FSoftObjectPath& Skeleton)
	{
		if (Skeleton.IsNull())
		{
			return INDEX_NONE;
		}
		for (int32 Index = 0; Index < Species.Num(); ++Index)
		{
			for (const TSoftObjectPtr<USkeleton>& Listed : Species[Index].Skeletons)
			{
				if (Listed.ToSoftObjectPath() == Skeleton)
				{
					return Index;
				}
			}
		}
		return INDEX_NONE;
	}

	/** True when a spawn at Location in Frame belongs to the wound last bled at RecentLocation in RecentFrame */
	inline bool IsSameWound(uint64 Frame, const FVector& Location, uint64 RecentFrame, const FVector& RecentLocation, float Radius)
	{
		return Frame == RecentFrame && FVector::DistSquared(Location, RecentLocation) <= FMath::Square(static_cast<double>(Radius));
	}
}
