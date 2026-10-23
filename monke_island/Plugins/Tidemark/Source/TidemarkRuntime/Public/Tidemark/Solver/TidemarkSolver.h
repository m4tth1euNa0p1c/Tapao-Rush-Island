// Tidemark plugin

#pragma once

#include "CoreMinimal.h"

/**
 *  Shallow water on a height grid with the virtual pipe model (Mei, Decaudin, Hu 2007):
 *  each cell exchanges water with its 4 neighbours through pipes whose flux accelerates with the
 *  difference of water surface. Water runs downhill, fills the hollows and only crosses a ridge
 *  once it overflows it, so the flood follows the real topography of the ground.
 *  Fixed cells (open sea, grid border) hold their surface at the sea level and feed the flood.
 *  Units: metres and seconds (moved as is from FFloodGrid, Tidemark conventions section 4: no fixed point in V1).
 *  No UObject: deterministic and testable.
 *
 *  Determinism (Tidemark conventions section 5). Step() runs two ParallelFor passes over the rows, and each
 *  pass is safe because every cell reads only the state of the previous pass and writes only its own cell:
 *   - Pass 1 (fluxes) reads Ground and Depth (untouched in this pass) of the cell and its 4 neighbours, reads
 *     and writes Flux of its own cell only. No row depends on another row's work in this pass.
 *   - Pass 2 (volumes) reads Flux (frozen after pass 1, never written here) of the cell and its 4 neighbours,
 *     reads and writes Depth of its own cell only.
 *  There is no parallel reduction: FreeVolume() is a sequential sum in fixed index order. The per-cell float
 *  arithmetic has a fixed operand order, so the result does not depend on the number of workers or on the
 *  scheduling of the rows. StepSeconds comes from the caller (UTidemarkRules), never from DeltaTime.
 *  bForceSingleThread runs both passes on the calling thread (EParallelForFlags::ForceSingleThread): the spec
 *  Tidemark.Determinism.IndependentOfWorkerCount proves the hash is the same either way.
 *
 *  Precision (decision D-1, TM-10): Depth, Ground and Flux stay float32, as moved from FFloodGrid.
 *   - Volume: the fluxes are exact between neighbours (the outflow of a cell is the inflow of the next, same float),
 *     so the only loss is the rounding of each Depth update: about 4e-6 of the volume over 12 000 steps (-10 L for
 *     2 304 m3 in the float32 model of the solver, 0 in float64). That is under one hash quantum of a single cell
 *     (1 mm x 36 m2 = 36 L on a 6 m cell) and invisible in play. The tolerance is UTidemarkRules::VolumeToleranceRelative.
 *   - Memory at 512 x 512 (262 144 cells): Ground 1 MiB + Depth 1 MiB + Fixed 256 KiB + Flux 4 MiB = 6.25 MiB in
 *     float32. Depth and Ground in double would add 2 MiB (8.25 MiB), double the snapshot of Depth sent to a late
 *     client, and change every bit of the arithmetic against FFloodGrid with no spec proving the equivalence.
 *   - Determinism is the same in float32 and float64 (IEEE-754, SSE2, fixed operand order); only the drift differs.
 *  A Kahan compensation per cell (option c) would add 1 MiB and rewrite the update; refused for the same reason.
 *  Revisit in V2 with the fixed point if a client ever shows a cross-build divergence (cahier section 11, risk 3).
 */
class TIDEMARKRUNTIME_API FTidemarkSolver
{
public:

	/** Allocates a SizeX * SizeY grid of square cells */
	void Init(int32 InSizeX, int32 InSizeY, float InCellSizeM);

	int32 GetSizeX() const { return SizeX; }
	int32 GetSizeY() const { return SizeY; }
	float GetCellSize() const { return CellSize; }
	int32 Index(int32 X, int32 Y) const { return Y * SizeX + X; }
	bool IsValid() const { return SizeX > 0 && SizeY > 0; }

	/** Ground height of every cell (m) */
	TArray<float> Ground;

	/** Water depth of every cell (m) */
	TArray<float> Depth;

	/** Cells whose surface is held at the sea level (open sea, border) */
	TArray<uint8> Fixed;

	/** Marks the border and the cells deeper than OpenSeaDepth under the sea level as fixed, and fills the sea */
	void SetupSea(float SeaLevel, float OpenSeaDepth);

	/**
	 * Advances the water by Dt seconds with the sea at SeaLevel (m). Gravity (m/s2) and FluxDamping come from
	 * UTidemarkRules; bForceSingleThread runs both passes on the calling thread (same result, for the determinism specs).
	 */
	void Step(float Dt, float SeaLevel, float Gravity = 9.81f, float FluxDamping = 0.995f, bool bForceSingleThread = false);

	/** Water surface of a cell (m) */
	float Surface(int32 CellIndex) const { return Ground[CellIndex] + Depth[CellIndex]; }

	/** Total water volume held by the free (non fixed) cells (m3). Sequential sum, fixed order. */
	double FreeVolume() const;

private:

	int32 SizeX = 0;
	int32 SizeY = 0;
	float CellSize = 1.0f;

	/** Outflow towards left, right, down, up (m3/s) */
	TArray<FVector4f> Flux;
};
