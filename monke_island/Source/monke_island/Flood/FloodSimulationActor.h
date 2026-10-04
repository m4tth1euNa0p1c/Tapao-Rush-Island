// monke_island

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Flood/FloodGrid.h"
#include "Flood/FloodOutsideSea.h"
#include "Flood/HazardClock.h"
#include "FloodSimulationActor.generated.h"

class AExpeditionGameState;
class UFloodRules;
class UMaterialInterface;
class UProceduralMeshComponent;

/**
 *  Rank of a flood actor among the flood actors of a world (T-141): one simulation per world, the same one kept on every
 *  machine whatever the order of BeginPlay. The actor of the persistent level first, then the smallest actor name, then
 *  the smallest level package name (PIE prefix removed).
 */
struct MONKE_ISLAND_API FFloodActorRank
{
	/** Whether the actor lives in the persistent level of its world */
	bool bPersistentLevel = false;

	/** Name of the actor (stable: placed in a level, replicated by name) */
	FString ActorName;

	/** Package of its level, without the PIE prefix */
	FString LevelName;

	/** Rank of an actor */
	static FFloodActorRank Of(const AActor& Actor);

	/** Whether this actor is kept before Other */
	bool IsBefore(const FFloodActorRank& Other) const;
};

/**
 *  Rising water over the real topography (cahier §5, §6.1 submersion): the sea rises and the water flows inland
 *  through a shallow water simulation laid on the ground around this actor. Low ground floods first,
 *  hollows fill, ridges hold until they overflow, and the island drowns chunk by chunk.
 *  One clock (T-098): in a map with an expedition (AExpeditionGameState with DA_ExpeditionRules) the rise starts at
 *  the flood deadline of the expedition clock (120 s + 120 s per pacified zone) and lasts its SubmersionDuration (60 s);
 *  the end of the submersion (OnSubmersionEnded, then the extraction) is the only end signal. Without expedition the
 *  actor falls back to the day / night clock of DA_FloodRules (StartSeconds, RiseSeconds). The choice is made once
 *  (DA_FloodRules.ClockMode), logged, and the two clocks never drive the water together. The sea never goes down.
 *  The ground is read by traces once the clock is known: the terrain is never modified, any change of the level design
 *  is followed.
 *  Expedition (T-103): the water rises on the fragment played only. At the start the server lays the grid on the
 *  arrival of the boat (UBoatStartSubsystem), pushed towards the island along the bow (DA_FloodRules
 *  ArrivalToGridCenterCm), else on the first pawn, and replicates that centre: every machine reads the same ground.
 *  Chunks that are all under water at the sea level are one flat quad moved with the sea; the other chunks are
 *  rebuilt within a time budget per frame (MeshBudgetMs). The local surface feeds UIslandWaterSubsystem, so wading, swimming and drowning follow the flood.
 *  Outside the grid (T-141) the sea of the fragment takes the same rise, over the land widened by a margin (about 1.5 km),
 *  then fades to none (FFloodOutsideSea, DA_FloodRules "Outside the grid"): a boat or the galleon heading offshore never
 *  drops, and the spawn and the rest of the map stay dry. An apron mesh draws that sea around the grid.
 *  One flood per world (T-141): a second AFloodSimulationActor (one in the map, one in LI_ExpeditionCore) is disabled
 *  with a warning; the kept one is chosen by FFloodActorRank, the same on every machine.
 *  Every machine runs the same deterministic steps from the replicated clock; the server's result
 *  is authoritative for gameplay. Debug (non shipping, day / night fallback only): monke.SetTime <s>, monke.FloodNow.
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

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

	virtual void Tick(float DeltaSeconds) override;

	/** Whether the sea has started rising */
	UFUNCTION(BlueprintPure, Category="Flood")
	bool IsFlooding() const { return bStarted; }

	/** Rise of the sea above its normal level right now (cm) */
	UFUNCTION(BlueprintPure, Category="Flood")
	float GetCurrentRiseCm() const;

	/**
	 *  Water surface of the flood at a location (world Z, cm); false where it is dry, and outside the grid where the sea
	 *  does not rise (far from the fragment, or before the rise)
	 */
	bool GetFloodSurfaceAt(const FVector& Location, float& OutSurfaceZ) const;

	/** Whether this actor was disabled because another flood actor of the world is kept (T-141) */
	UFUNCTION(BlueprintPure, Category="Flood")
	bool IsDuplicate() const { return bDuplicate; }

	/** Sea of the fragment outside the grid (invalid until the ground is read, or without expedition) */
	const FFloodOutsideSea& GetOutsideSea() const { return OutsideSea; }

	/** Clock that drives the rise (Undecided until the game state is known on this machine) */
	EHazardClockSource GetClockSource() const { return ClockSource; }

	/** Whether the final rise reached its end (expedition: end of the submersion; day / night: full rise) */
	UFUNCTION(BlueprintPure, Category="Flood")
	bool IsRiseComplete() const { return bRiseComplete; }

	/** Rise of the sea (cm) after Seconds of flooding on the day / night fallback, from the rules */
	static float RiseAt(const UFloodRules& InRules, float Seconds);

private:

	/** The expedition ended its submersion: the rise is over, the water stays up (extraction signal) */
	UFUNCTION()
	void HandleSubmersionEnded();

	/** One procedural mesh per chunk of the grid */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UProceduralMeshComponent>> Chunks;

	/** Surface drawn outside the grid over the rising sea of the fragment, scaled in Z by the rise */
	UPROPERTY(Transient)
	TObjectPtr<UProceduralMeshComponent> Apron;

	/** Flood surface material, loaded at begin play */
	UPROPERTY(Transient)
	TObjectPtr<UMaterialInterface> WaterMaterial;

	/** Sea surface actors ignored by the ground traces */
	TArray<TWeakObjectPtr<AActor>> IgnoredSurfaces;

	/** Simulation grid (m) */
	FFloodGrid Grid;

	/** Sea of the fragment outside the grid (T-141) */
	FFloodOutsideSea OutsideSea;

	/** Rise last applied to the apron (cm), negative before the first */
	double ApronRiseCm = -1.0;

	/** Disabled: another flood actor of the world is kept */
	bool bDuplicate = false;

	/** World XY of the corner of the grid (cm) */
	FVector2D Origin = FVector2D::ZeroVector;

	/** Normal sea level (m) */
	float BaseSeaLevel = 0.0f;

	/** Next cell to trace while reading the ground */
	int32 TraceCursor = 0;

	/** Centre of the grid on the fragment played (expedition), chosen by the server */
	UPROPERTY(Replicated)
	FVector GridCenter = FVector::ZeroVector;

	/** Whether the server chose GridCenter */
	UPROPERTY(Replicated)
	bool bHasGridCenter = false;

	/** Whether the grid, its chunks and its surface provider are set up (after the clock is known) */
	bool bGridSetup = false;

	/** Size of a cell of the grid in use (cm) */
	float CellCm = 600.0f;

	/** Per chunk: 1 while it is drawn as one flat quad moved with the sea */
	TArray<uint8> ChunkFlat;

	/** Whether the ground has been read */
	bool bGroundReady = false;

	/** Whether the sea has started rising */
	bool bStarted = false;

	/** Whether the final rise reached its end */
	bool bRiseComplete = false;

	/** Clock that drives the rise, decided once */
	EHazardClockSource ClockSource = EHazardClockSource::Undecided;

	/** Expedition followed in expedition mode */
	TWeakObjectPtr<AExpeditionGameState> Expedition;

	/** Highest rise of the sea reached so far (cm): the sea target never goes down */
	double HeldRiseCm = 0.0;

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

	/** Decides the clock once the game state is known; true when decided */
	bool ResolveClock();

	/** Seconds of rising water now on the chosen clock, -1 before the rise */
	double GetFloodSeconds() const;

	/** Rise of the sea (cm) after FloodSeconds on the chosen clock, before the hold */
	double ComputeRiseCm(double FloodSeconds) const;

	/** Lays a Cells x Cells grid of CellSizeCm cells centred on Center, creates the chunks and feeds UIslandWaterSubsystem */
	void SetupGrid(const FVector& Center, int32 Cells, float CellSizeCm);

	/** Server, expedition: centre of the grid on the fragment played; false while the start is not decided */
	bool ChooseGridCenter(FVector& OutCenter, FString& OutSource) const;

	/** Traces a batch of ground cells */
	void ReadGroundBatch();

	/** Runs the simulation up to FloodSeconds of rising water (< 0: not yet) */
	void Simulate(double FloodSeconds);

	/** Rebuilds the surface of one chunk */
	void BuildChunk(int32 ChunkIndex);

	/** Expedition, once the ground is read: lays the sea outside the grid around the land and builds the apron */
	void SetupOutsideSea();

	/** Builds the apron mesh around the grid, in units of rise (Z 0 to 1) */
	void BuildApron();

	/** Moves the apron with the held rise */
	void UpdateApron();

	/** Keeps one flood actor per world (FFloodActorRank); false when this one is disabled */
	bool ResolveDuplicates();

	/** Stops this actor for good: tick, chunks, apron, surface provider, expedition delegates */
	void DisableAsDuplicate(const AFloodSimulationActor& Kept);
};
