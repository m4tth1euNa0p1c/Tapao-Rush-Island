// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "FortMoat.generated.h"

class AFortGarrisonZone;

/** Pure rule of the moat (tested by monke_island.Fort.Moat) */
namespace FortMoatMath
{
	/** True when feet at FeetZ drown: in the band of the moat along the walls (outside them, MoatWidth at most from
	 *  them) and DrownDepth under the water's surface */
	inline bool IsInMoatWater(bool bInsideWalls, double DistanceToWalls, double FeetZ, double SurfaceZ, float MoatWidth, float DrownDepth)
	{
		return !bInsideWalls && DistanceToWalls <= MoatWidth && FeetZ <= SurfaceZ - DrownDepth;
	}
}

/**
 *  The water of a French fort's moat (owner 2026-10-08: "quand je tombe dans les douves du fort français je meurs dans
 *  l'eau"). Server: a player whose feet stay DrownDepthCm under the surface of the moat's water (the top of Water) in
 *  the band of the moat (outside the walls of Garrison, MoatWidthCm at most from them) for DrownSeconds drowns. The
 *  bridge, the glacis and the court stand above the water; the ground of the atoll around the fort's plateau is past
 *  the band. Placed in the fort level (LI_FortFrench_Land) and by the fort of the test map.
 */
UCLASS()
class MONKE_ISLAND_API AFortMoat : public AActor
{
	GENERATED_BODY()

public:

	AFortMoat();

	/** The walls of the fort: the inner edge of the moat */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moat")
	TObjectPtr<AFortGarrisonZone> Garrison;

	/** The water of the moat: its top is the surface */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moat")
	TObjectPtr<AActor> Water;

	/** Width of the moat from the walls (cm): the counterscarp is 6.7 m out */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moat", meta=(ClampMin="0", Units="cm"))
	float MoatWidthCm = 800.0f;

	/** Depth under the surface the feet drown at (cm) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moat", meta=(ClampMin="0", Units="cm"))
	float DrownDepthCm = 40.0f;

	/** Time in the water before death (s): the fall into it is seen */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moat", meta=(ClampMin="0", Units="s"))
	float DrownSeconds = 0.6f;

	/** Seconds between two looks at the players (server) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Moat", meta=(ClampMin="0.02", Units="s"))
	float CheckInterval = 0.2f;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Server: the players in the water of the moat */
	void Check();

	/** Players in the water and since when (server) */
	TMap<TWeakObjectPtr<APawn>, double> InWater;

	FTimerHandle CheckTimer;
};
