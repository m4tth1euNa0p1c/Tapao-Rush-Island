// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WaveSurfaceActor.generated.h"

class UIslandWaterSubsystem;
class UMaterialParameterCollection;
class UProceduralMeshComponent;
class UWaveRules;

/**
 *  The sea surface drawn around the local camera (spawned by UIslandWaterSubsystem, never on a dedicated server):
 *  one grid dense at the camera and wider and wider out to the horizon, kept at the world origin and shifted under
 *  the camera by the material (WaveFollow), displaced by the Gerstner waves of M_Water_Ocean in world space. Feeds MPC_Waves every frame (time on the server clock, the
 *  trains of DA_WaveRules) and hides the flat static sea planes while it is drawn.
 */
UCLASS(NotPlaceable, Transient)
class MONKE_ISLAND_API AWaveSurfaceActor : public AActor
{
	GENERATED_BODY()

public:

	AWaveSurfaceActor();

	/** Builds the grid and starts following the camera */
	void Init(UWaveRules* InRules, UIslandWaterSubsystem* InWater);

	virtual void Tick(float DeltaSeconds) override;

protected:

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:

	/** Writes the time and the wave trains into the collection */
	void UpdateParameters();

	/** Rules */
	UPROPERTY(Transient)
	TObjectPtr<UWaveRules> Rules;

	/** Collection read by the material */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialParameterCollection> Parameters;

	/** The grid */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Surface;

	/** Water subsystem (sea level, clock, trains) */
	TWeakObjectPtr<UIslandWaterSubsystem> Water;

	/** Growth ratio of the grid cells (WaveMath::SurfaceRatio) */
	double GridRatio = 1.0;

	/** Static sea planes hidden while the grid is drawn */
	TArray<TWeakObjectPtr<AActor>> HiddenPlanes;
};
