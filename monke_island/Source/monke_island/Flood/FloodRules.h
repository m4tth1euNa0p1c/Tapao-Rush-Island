// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "FloodRules.generated.h"

class UMaterialInterface;

/**
 *  Tunable rules of the rising water (DA_FloodRules): when the sea rises, how high and how fast,
 *  and the simulation grid laid on the ground around the flood actor.
 *  The ground is read at runtime by traces, so the flood follows any change of the terrain without baked data
 *  and never deforms it. Times are elapsed play seconds on the day / night clock.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UFloodRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/** Elapsed play time when the sea starts rising (s): 4:50 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise", meta=(ClampMin="0", Units="s"))
	float StartSeconds = 290.0f;

	/** Rise of the sea above its level (cm): above the highest motu plus a body height */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise", meta=(ClampMin="0", Units="cm"))
	float RiseCm = 650.0f;

	/** Time for the sea to reach its full rise (s): the player loses footing within about a minute */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise", meta=(ClampMin="1", Units="s"))
	float RiseSeconds = 45.0f;

	/** Shape of the rise: 1 linear, above 1 slow start then surge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise", meta=(ClampMin="0.2", ClampMax="5"))
	float RiseExponent = 1.6f;

	/** Size of a simulation cell (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grid", meta=(ClampMin="100", Units="cm"))
	float CellSizeCm = 600.0f;

	/** Number of cells on each side of the grid, centred on the flood actor */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grid", meta=(ClampMin="16", ClampMax="2048"))
	int32 GridCells = 512;

	/** Cells per side of a render chunk */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grid", meta=(ClampMin="4", ClampMax="128"))
	int32 ChunkCells = 32;

	/** Ground traces per frame while the grid reads the terrain */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grid", meta=(ClampMin="100"))
	int32 TracesPerFrame = 16000;

	/** Height from which the ground traces start (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grid", meta=(Units="cm"))
	float TraceTopCm = 5000.0f;

	/** Lowest point of the ground traces (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grid", meta=(Units="cm"))
	float TraceBottomCm = -40000.0f;

	/** Depth under the sea level from which a cell is open sea, held at the sea level (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="0", Units="cm"))
	float OpenSeaDepthCm = 150.0f;

	/** Simulation time step (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="0.005", ClampMax="0.2", Units="s"))
	float StepSeconds = 0.05f;

	/** Most simulation steps per frame (catches up after a hitch without freezing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="1"))
	int32 MaxStepsPerFrame = 4;

	/** Flux kept from one step to the next (damps the sloshing) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="0.9", ClampMax="1"))
	float FluxDamping = 0.995f;

	/** Gravity (m/s2) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Simulation", meta=(ClampMin="0.1"))
	float Gravity = 9.81f;

	/** Water thinner than this is not drawn (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render", meta=(ClampMin="0", Units="cm"))
	float MinDrawDepthCm = 3.0f;

	/** Seconds between two rebuilds of the flood surface */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render", meta=(ClampMin="0", Units="s"))
	float MeshUpdateInterval = 0.2f;

	/** Most chunks rebuilt per frame */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render", meta=(ClampMin="1"))
	int32 ChunksPerFrame = 24;

	/** Material of the flood surface (the lagoon water) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render")
	TSoftObjectPtr<UMaterialInterface> WaterMaterial;
};
