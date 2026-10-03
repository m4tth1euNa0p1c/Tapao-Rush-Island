// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Flood/FloodGrid.h"
#include "FloodSimulationActor.generated.h"

class UFloodRules;
class UMaterialInterface;
class UProceduralMeshComponent;

/**
 *  Rising water over the real topography (cahier §5, submersion): at StartSeconds the sea rises and the water
 *  flows inland through a shallow water simulation laid on the ground around this actor. Low ground floods first,
 *  hollows fill, ridges hold until they overflow, and the island drowns chunk by chunk.
 *  The ground is read by traces at begin play: the terrain is never modified, any change of the level design is
 *  followed. The local surface feeds UIslandWaterSubsystem, so wading, swimming and drowning follow the flood.
 *  Every machine runs the same deterministic steps from the replicated day / night clock; the server's result
 *  is authoritative for gameplay. Debug (non shipping): monke.SetTime <s>, monke.FloodNow.
 */
UCLASS()
class MONKE_ISLAND_API AFloodSimulationActor : public AActor
{
	GENERATED_BODY()

public:

	AFloodSimulationActor();

	/** Rules of the flood (DA_FloodRules) */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Flood")
	TObjectPtr<UFloodRules> Rules;

	virtual void BeginPlay() override;

	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	virtual void Tick(float DeltaSeconds) override;

	/** Whether the sea has started rising */
	UFUNCTION(BlueprintPure, Category="Flood")
	bool IsFlooding() const { return bStarted; }

	/** Rise of the sea above its normal level right now (cm) */
	UFUNCTION(BlueprintPure, Category="Flood")
	float GetCurrentRiseCm() const;

	/** Water surface of the flood at a location (world Z, cm); false outside the grid or where it is dry */
	bool GetFloodSurfaceAt(const FVector& Location, float& OutSurfaceZ) const;

	/** Rise of the sea (cm) after Seconds of flooding, from the rules */
	static float RiseAt(const UFloodRules& InRules, float Seconds);

private:

	/** One procedural mesh per chunk of the grid */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> Chunks;

	/** Flood surface material, loaded at begin play */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** Sea surface actors ignored by the ground traces */
	TArray<TWeakObjectPtr<AActor>> IgnoredSurfaces;

	/** Simulation grid (m) */
	FFloodGrid Grid;

	/** World XY of the corner of the grid (cm) */
	FVector2D Origin = FVector2D::ZeroVector;

	/** Normal sea level (m) */
	float BaseSeaLevel = 0.0f;

	/** Next cell to trace while reading the ground */
	int32 TraceCursor = 0;

	/** Whether the ground has been read */
	bool bGroundReady = false;

	/** Whether the sea has started rising */
	bool bStarted = false;

	/** Simulation steps done since the start of the flood */
	int64 StepsDone = 0;

	/** Seconds until the next round of chunk rebuilds */
	float MeshTimer = 0.0f;

	/** Next chunk to rebuild */
	int32 ChunkCursor = 0;

	/** Chunks per side */
	int32 ChunksX = 0;
	int32 ChunksY = 0;

	/** Elapsed play time on the day / night clock (s) */
	double GetElapsedSeconds() const;

	/** Traces a batch of ground cells */
	void ReadGroundBatch();

	/** Runs the simulation up to the current time */
	void Simulate(double Elapsed);

	/** Rebuilds the surface of one chunk */
	void BuildChunk(int32 ChunkIndex);
};
