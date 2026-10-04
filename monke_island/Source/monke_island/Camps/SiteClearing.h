// monke_island

#pragma once

#include "CoreMinimal.h"

class AActor;
class UInstancedStaticMeshComponent;
class UPrimitiveComponent;

/**
 *  Ground and vegetation of a site built at runtime (fishermen camp, marae): the instanced bushes, palms and grass
 *  of its zone are moved out of sight (their indices kept, the tiki grid of bushes relies on them) and come back when
 *  the site goes; the ground under a point skips the instanced vegetation.
 */
struct MONKE_ISLAND_API FSiteClearing
{
	/** Moves the vegetation instances within Radius (2D) of Centre out of sight, the components of Ignore excepted */
	void Clear(const AActor* Ignore, const FVector& Centre, double Radius);

	/** Puts every moved instance back */
	void Restore();

	/** Instances moved */
	int32 Num() const { return Moved.Num(); }

	/** Ground under a point (world), or the point itself when no ground is found; the components given are skipped */
	static FVector GroundAt(const AActor* Owner, const FVector& World, TConstArrayView<const UPrimitiveComponent*> Ignored);

private:

	struct FMovedInstance
	{
		TWeakObjectPtr<UInstancedStaticMeshComponent> Component;
		int32 Index = INDEX_NONE;
		FTransform Original;
	};
	TArray<FMovedInstance> Moved;
};
