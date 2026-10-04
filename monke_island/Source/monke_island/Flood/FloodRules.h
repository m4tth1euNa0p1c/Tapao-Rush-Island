// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Flood/HazardClock.h"
#include "FloodRules.generated.h"

class UMaterialInterface;

/**
 *  Tunable rules of the rising water (DA_FloodRules): when the sea rises, how high and how fast,
 *  and the simulation grid laid on the ground around the flood actor.
 *  The ground is read at runtime by traces, so the flood follows any change of the terrain without baked data
 *  and never deforms it.
 *  When the rise starts (T-098): in a map with an expedition (AExpeditionGameState + DA_ExpeditionRules) the rise starts
 *  at the flood deadline of the expedition clock and lasts SubmersionDuration, with the height and curve of
 *  DA_ExpeditionRules. The "Day / night fallback" values below only apply to a map without expedition.
 */
UCLASS(BlueprintType)
class MONKE_ISLAND_API UFloodRules : public UDataAsset
{
	GENERATED_BODY()

public:

	/**
	 *  Clock of the rise: Auto = the expedition clock when the map has one, else the day / night fallback below;
	 *  ExpeditionOnly = no flood without expedition; DayNightOnly = always the fallback (test maps). Decided once, logged.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Clock")
	EHazardClockMode ClockMode = EHazardClockMode::Auto;

	/** Day / night fallback only: elapsed play time when the sea starts rising (s), 4:50 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise|Day / night fallback", meta=(ClampMin="0", Units="s"))
	float StartSeconds = 290.0f;

	/** Day / night fallback only: rise of the sea above its level (cm), above the highest motu plus a body height */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise|Day / night fallback", meta=(ClampMin="0", Units="cm"))
	float RiseCm = 650.0f;

	/** Day / night fallback only: time for the sea to reach its full rise (s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise|Day / night fallback", meta=(ClampMin="1", Units="s"))
	float RiseSeconds = 45.0f;

	/** Day / night fallback only: shape of the rise, 1 linear, above 1 slow start then surge */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Rise|Day / night fallback", meta=(ClampMin="0.2", ClampMax="5"))
	float RiseExponent = 1.6f;

	/**
	 *  Expedition (T-103): the water rises on the fragment played only, through the local grid below, recentred at the
	 *  start of the expedition on the arrival of the boat. Off: the grid stays around the flood actor (legacy, test maps).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea")
	bool bLocalGridInExpedition = true;

	/** Expedition: cells on each side of the grid laid on the fragment (fragments are up to 2 km long) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea", meta=(ClampMin="16", ClampMax="2048"))
	int32 ExpeditionGridCells = 320;

	/** Expedition: size of a cell of the grid laid on the fragment (cm); 320 x 8 m = 2.56 km */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea", meta=(ClampMin="100", Units="cm"))
	float ExpeditionCellSizeCm = 800.0f;

	/**
	 *  Expedition: distance from the arrival boat to the centre of the grid, along its bow (the bow points to the beach,
	 *  200 to 350 m away): the grid covers the island ahead and the water around the boat
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea", meta=(ClampMin="0", Units="cm"))
	float ArrivalToGridCenterCm = 60000.0f;

	/**
	 *  Expedition: also raise the sea level of the whole map (UIslandWaterSubsystem::SetSeaLevel, UFloodSeaRiseSubsystem).
	 *  Off by default (owner, 2026-10-04): the water rises only where the players are, not at the spawn.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea")
	bool bRaiseGlobalSea = false;

	/**
	 *  Expedition (T-141): past the border of the grid the sea takes the same rise as the open sea of the grid, so a boat
	 *  or the galleon heading offshore never drops; only around the fragment (see the margin and the fade below).
	 *  Off: the water past the grid stays at the level of the map (legacy, the border shows a step).
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea|Outside the grid")
	bool bRaiseOutsideGrid = true;

	/**
	 *  Expedition: the sea outside the grid takes the full rise over the land of the fragment widened by this margin
	 *  (and always over the whole grid) (cm). The spawn and the rest of the map, more than 10 km away, stay dry.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea|Outside the grid", meta=(ClampMin="0", Units="cm", EditCondition="bRaiseOutsideGrid"))
	float OutsideFullRiseMarginCm = 150000.0f;

	/** Expedition: distance past the full rise over which the rise fades smoothly to none (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea|Outside the grid", meta=(ClampMin="0", Units="cm", EditCondition="bRaiseOutsideGrid"))
	float OutsideFadeCm = 50000.0f;

	/** Expedition: width of the band inside the border of the grid blended into the outside sea (cm), a few cells */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea|Outside the grid", meta=(ClampMin="0", Units="cm", EditCondition="bRaiseOutsideGrid"))
	float EdgeBlendCm = 4800.0f;

	/**
	 *  Expedition: size of a cell of the apron, the surface drawn outside the grid over the rising sea (cm); snapped so
	 *  the apron meets the border of the grid exactly
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea|Outside the grid", meta=(ClampMin="1000", Units="cm", EditCondition="bRaiseOutsideGrid"))
	float ApronCellCm = 12800.0f;

	/** Expedition, with bRaiseGlobalSea: smallest move of the global sea level (cm), the surfaces move by steps */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea", meta=(ClampMin="0.1", ClampMax="50", Units="cm"))
	float SeaLevelStepCm = 2.0f;

	/** Expedition: seconds before the submersion over which the frame times are measured as the reference (log) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Expedition sea", meta=(ClampMin="0", Units="s"))
	float FrameStatsLeadSeconds = 10.0f;

	/** Size of a simulation cell (cm), without expedition */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Grid", meta=(ClampMin="100", Units="cm"))
	float CellSizeCm = 600.0f;

	/** Number of cells on each side of the grid, centred on the flood actor (without expedition) */
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

	/** Game thread time given to the chunk rebuilds per frame (ms); the rest waits for the next frame */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render", meta=(ClampMin="0.1", Units="ms"))
	float MeshBudgetMs = 1.0f;

	/**
	 *  A chunk whose cells are all under water within this distance of the sea is drawn as one flat quad moved with
	 *  the sea, instead of a rebuilt mesh (cm): the open sea and the drowned land cost a transform update
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render", meta=(ClampMin="0", Units="cm"))
	float FlatChunkToleranceCm = 3.0f;

	/** Material of the flood surface (the lagoon water) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render")
	TSoftObjectPtr<UMaterialInterface> WaterMaterial;

	/** Below this rise (cm) the apron outside the grid is hidden: the sea of the map shows alone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render", meta=(ClampMin="0", Units="cm"))
	float ApronMinRiseCm = 1.0f;

	/** The apron is sunk this much under the rising sea so its fading edge never flickers against the sea of the map (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Render", meta=(ClampMin="0", Units="cm"))
	float ApronSinkCm = 2.0f;
};
