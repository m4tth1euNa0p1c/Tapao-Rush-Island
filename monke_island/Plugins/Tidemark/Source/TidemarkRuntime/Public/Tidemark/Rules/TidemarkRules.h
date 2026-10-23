// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Tidemark/Solver/TidemarkHash.h"
#include "Tidemark/Solver/TidemarkSchedule.h"
#include "TidemarkRules.generated.h"

/**
 * Default values of UTidemarkRules, the single source for the DataAsset defaults and for the specs that must
 * agree with them (Tidemark.Determinism.VolumeConserved, Tidemark.Schedule.*). Nothing of this is hard-coded elsewhere.
 */
namespace Tidemark::Rules
{
	/** Fixed simulation step (s). Steps are counted from the clock, never from DeltaTime. */
	constexpr float DefaultStepSeconds = 0.05f;
	/** Steps simulated in one frame at most; the rest is caught up on the next frames. */
	constexpr int32 DefaultMaxStepsPerFrame = 4;
	/** Gravity of the virtual pipes (m/s2). */
	constexpr float DefaultGravity = 9.81f;
	/** Flux kept from the previous step (stability of the pipe model). */
	constexpr float DefaultFluxDamping = 0.995f;
	/** A cell that deep under the sea level is open sea: its surface is held at the sea level (cm). */
	constexpr float DefaultOpenSeaDepthCm = 150.0f;
	/** Cells on each edge of the simulation grid. */
	constexpr int32 DefaultSimGridCells = 512;
	/** Edge of a simulation cell (cm): 512 x 6 m = 3 072 m on a side. */
	constexpr float DefaultSimCellSizeCm = 600.0f;
	/** Cells on each edge of the fallback grid (low-end machines, perf budget missed). */
	constexpr int32 DefaultFallbackGridCells = 320;
	/** Edge of a fallback cell (cm): 320 x 8 m = 2 560 m on a side. */
	constexpr float DefaultFallbackCellSizeCm = 800.0f;
	/** Period of the state hash exchanged between host and clients (s). */
	constexpr float DefaultHashIntervalSeconds = 1.0f;
	/** Consecutive hash mismatches before a client asks for a snapshot. */
	constexpr int32 DefaultDivergenceThreshold = 3;
	/** Steps a client catches up at most after a hitch or a late join (10 s at 0.05 s); beyond, it asks for a snapshot. */
	constexpr int32 DefaultMaxCatchUpSteps = 200;
	/** Refresh rate of the render bridge (RT + MPC) (Hz). */
	constexpr float DefaultRenderUpdateHz = 5.0f;
	/** Edge of a hash block, in cells. */
	constexpr int32 DefaultBlockSize = Tidemark::Hash::DefaultBlockSize;
	/** Quantum of the depth before hashing (cm). */
	constexpr float DefaultQuantizeCm = Tidemark::Hash::DefaultQuantizeCm;
	/**
	 * Relative drift of the total volume tolerated in a closed basin over a run (D-1, see FTidemarkSolver).
	 * float32 arithmetic drifts about 4e-6 of the volume over 12 000 steps; 1e-5 leaves a x2 margin and stays under one
	 * hash quantum of one cell (1 mm x 36 m2 = 36 L on a 6 m cell, versus 23 L for 2 304 m3 at 1e-5).
	 */
	constexpr double DefaultVolumeToleranceRelative = 1.0e-5;
	/** Default schedule: the 650 cm rise of the Tapao submersion, over 60 s (1 200 steps at 0.05 s), exponent 1.6. */
	constexpr float DefaultScheduleHeightCm = 650.0f;
	constexpr uint32 DefaultScheduleDurationSteps = 1200;
	constexpr float DefaultScheduleExponent = 1.6f;
	/** The water never goes down. */
	constexpr bool DefaultHoldRise = true;
}

/**
 * Every tunable value of a Tidemark simulation (Tidemark cahier section 6.3, CONVENTIONS.md section 4: no gameplay number
 * hard-coded). One asset per project or per level; the subsystem reads it, the solver receives explicit parameters.
 * Steps and hashes are what host and clients compare, so the same asset must be loaded on every machine.
 */
UCLASS(BlueprintType)
class TIDEMARKRUNTIME_API UTidemarkRules : public UDataAsset
{
	GENERATED_BODY()

public:

	UTidemarkRules();

	/** Fixed simulation step (s). The step count comes from the clock (floor(Elapsed / StepSeconds)), never from DeltaTime. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.005", ClampMax = "0.5", Units = "s"))
	float StepSeconds = Tidemark::Rules::DefaultStepSeconds;

	/** Steps simulated in one frame at most; the remainder is caught up on the next frames. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxStepsPerFrame = Tidemark::Rules::DefaultMaxStepsPerFrame;

	/** Gravity of the virtual pipes (m/s2). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.1", ClampMax = "100"))
	float Gravity = Tidemark::Rules::DefaultGravity;

	/** Flux kept from one step to the next; under 1 for stability of the pipe model. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.5", ClampMax = "1"))
	float FluxDamping = Tidemark::Rules::DefaultFluxDamping;

	/** A cell that deep under the sea level is open sea: its surface is held at the sea level and feeds the flood (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0", Units = "cm"))
	float OpenSeaDepthCm = Tidemark::Rules::DefaultOpenSeaDepthCm;

	/** Cells on each edge of the simulation grid. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "16", ClampMax = "2048"))
	int32 SimGridCells = Tidemark::Rules::DefaultSimGridCells;

	/** Edge of a simulation cell (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid", meta = (ClampMin = "50", Units = "cm"))
	float SimCellSizeCm = Tidemark::Rules::DefaultSimCellSizeCm;

	/** Cells on each edge of the fallback grid used when the perf budget is missed. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid|Fallback", meta = (ClampMin = "16", ClampMax = "2048"))
	int32 FallbackGridCells = Tidemark::Rules::DefaultFallbackGridCells;

	/** Edge of a fallback cell (cm). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grid|Fallback", meta = (ClampMin = "50", Units = "cm"))
	float FallbackCellSizeCm = Tidemark::Rules::DefaultFallbackCellSizeCm;

	/** Period of the state hash sent to the host (s). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sync", meta = (ClampMin = "0.1", ClampMax = "60", Units = "s"))
	float HashIntervalSeconds = Tidemark::Rules::DefaultHashIntervalSeconds;

	/** Consecutive hash mismatches before a client asks for a snapshot. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sync", meta = (ClampMin = "1", ClampMax = "100"))
	int32 DivergenceThreshold = Tidemark::Rules::DefaultDivergenceThreshold;

	/** Steps caught up at most after a hitch or a late join; beyond, the client asks for a snapshot instead of simulating. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Sync", meta = (ClampMin = "1", ClampMax = "100000"))
	int32 MaxCatchUpSteps = Tidemark::Rules::DefaultMaxCatchUpSteps;

	/** Refresh rate of the render bridge: depth render target and material parameter collection (Hz). */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Render", meta = (ClampMin = "0.5", ClampMax = "60", Units = "Hz"))
	float RenderUpdateHz = Tidemark::Rules::DefaultRenderUpdateHz;

	/** Edge of a hash block, in cells: a divergence is located to one block. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hash", meta = (ClampMin = "4", ClampMax = "256"))
	int32 BlockSize = Tidemark::Rules::DefaultBlockSize;

	/** Quantum of the depth before hashing (cm): differences under it are not a divergence. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Hash", meta = (ClampMin = "0.001", ClampMax = "10", Units = "cm"))
	float QuantizeCm = Tidemark::Rules::DefaultQuantizeCm;

	/**
	 * Relative drift of the total volume tolerated in a closed basin over a run (D-1): the solver keeps float32, whose
	 * rounding drifts about 4e-6 of the volume over 12 000 steps, under one hash quantum of one cell. Checked by
	 * Tidemark.Determinism.VolumeConserved; a larger drift means the solver lost or created water.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Simulation", meta = (ClampMin = "0.000001", ClampMax = "0.01"))
	double VolumeToleranceRelative = Tidemark::Rules::DefaultVolumeToleranceRelative;

	/** Sea level schedule run when the level has no scenario of its own. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Schedule")
	FSeaLevelSchedule DefaultSchedule;

	/** The water never goes down, whatever the schedule says: forces FSeaLevelSchedule::bHoldRise on every schedule run. */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Schedule")
	bool bHoldRise = Tidemark::Rules::DefaultHoldRise;

	/** Whether the level of a schedule is held up: its own flag, or the rule that forces it. */
	bool HoldsRise(const FSeaLevelSchedule& Schedule) const { return bHoldRise || Schedule.bHoldRise; }

	/** Steps of the clock at StepSeconds: Tidemark::Schedule::StepsFromSeconds with this asset's step. */
	uint32 StepsFromSeconds(float Seconds) const { return Tidemark::Schedule::StepsFromSeconds(Seconds, StepSeconds); }
};
