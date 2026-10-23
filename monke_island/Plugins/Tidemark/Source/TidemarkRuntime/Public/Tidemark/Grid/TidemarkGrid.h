// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "TidemarkGrid.generated.h"

class FTidemarkSolver;

/**
 * Per-cell flags of a Tidemark grid, stored as a bitmask in FTidemarkGrid::Flags.
 * OpenSea and Border cells are held at the sea level by the solver; Forbidden is reserved for the lint (no build, no route).
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class ETidemarkCellFlag : uint8
{
	None = 0 UMETA(Hidden),
	/** Cell below the open sea: its surface follows the sea level and feeds the flood. */
	OpenSea = 1,
	/** Cell on the edge of the grid: held at the sea level like the open sea. */
	Border = 2,
	/** Cell the lint must refuse (no camp, route or refuge). Not read by the solver. */
	Forbidden = 4
};
ENUM_CLASS_FLAGS(ETidemarkCellFlag)

/**
 * Layer A: the baked terrain of a level as a regular grid of square cells, structure of arrays, no UObject.
 *
 * Units are centimetres (Tidemark conventions section 4): ElevationCm is an int16 per cell (-327.68 m to +327.67 m),
 * Flags a uint8 bitmask of ETidemarkCellFlag. Both arrays are row-major: Index(X, Y) = Y * SizeX + X.
 * The value of a cell sits at the centre of the cell: CellToWorld(X, Y) = OriginWorld + (X + 0.5, Y + 0.5) * CellSizeCm.
 *
 * The grid is immutable at runtime: it is produced once at import (Landscape, heightmap or collision), stored in a
 * UTidemarkTerrainAsset or a .tmk folder, and copied into the solver by FillSolver. Its content hash (ComputeSourceHash)
 * is part of what makes a scenario reproducible (seed, grid hash, schedule, journal).
 *
 * V2 reservations, one more layer each, same row-major layout (architecture proposal B.2): Roughness (uint8, Manning n x 1000),
 * VegCover (uint8, 0-255 = 0-100 %), Soil (uint8 enum: sand, reef flat, coral, earth, built, water), WindExposure[8] (uint8 per
 * sector). They are not members in V1 so that the .tmk schema stays at version 1; adding them bumps SchemaVersion.
 */
USTRUCT(BlueprintType)
struct TIDEMARKRUNTIME_API FTidemarkGrid
{
	GENERATED_BODY()

	/** Largest grid accepted (16 MiB at 3 bytes per cell), the budget of the terrain layer. */
	static constexpr int64 MaxBytes = 16LL * 1024 * 1024;

	/** Largest number of cells accepted: MaxBytes / (sizeof(int16) + sizeof(uint8)). */
	static constexpr int32 MaxCells = static_cast<int32>(MaxBytes / 3);

	/** World position (cm) of the lower-left corner of cell (0, 0), in the XY plane of the level. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Grid")
	FVector2D OriginWorld = FVector2D::ZeroVector;

	/** Edge of a square cell, cm. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Tidemark|Grid", meta = (ClampMin = "1.0"))
	float CellSizeCm = 400.0f;

	/** Number of cells along X. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Grid")
	int32 SizeX = 0;

	/** Number of cells along Y. */
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Tidemark|Grid")
	int32 SizeY = 0;

	/** Ground elevation of every cell, cm, row-major (SizeX * SizeY). */
	UPROPERTY(VisibleAnywhere, Category = "Tidemark|Grid")
	TArray<int16> ElevationCm;

	/** ETidemarkCellFlag bitmask of every cell, row-major (SizeX * SizeY). */
	UPROPERTY(VisibleAnywhere, Category = "Tidemark|Grid")
	TArray<uint8> Flags;

	/** Allocates SizeX * SizeY cells, elevation and flags zeroed. Sizes <= 0 give an empty grid. */
	void Init(int32 InSizeX, int32 InSizeY, float InCellSizeCm, const FVector2D& InOriginWorld = FVector2D::ZeroVector);

	/** Empties the grid. */
	void Reset();

	/** Row-major index of cell (X, Y). Not bounds-checked. */
	int32 Index(int32 X, int32 Y) const { return Y * SizeX + X; }

	/** Number of cells. */
	int32 Num() const { return SizeX * SizeY; }

	/** Bytes held by the two layers. */
	int64 NumBytes() const { return static_cast<int64>(ElevationCm.Num()) * sizeof(int16) + Flags.Num(); }

	/** True when both sizes are positive, the cell size is positive and both arrays hold SizeX * SizeY entries. */
	bool IsValid() const;

	/** True when (X, Y) is inside the grid. */
	bool IsInside(int32 X, int32 Y) const { return X >= 0 && Y >= 0 && X < SizeX && Y < SizeY; }

	/** Elevation (cm) of cell (X, Y), coordinates clamped to the grid. Zero on an invalid grid. */
	int16 GetElevationCm(int32 X, int32 Y) const;

	/** Flags of cell (X, Y), coordinates clamped to the grid. None on an invalid grid. */
	ETidemarkCellFlag GetFlags(int32 X, int32 Y) const;

	/** True when cell (X, Y) carries every bit of Flag. */
	bool HasFlag(int32 X, int32 Y, ETidemarkCellFlag Flag) const;

	/** Sets or clears Flag on cell (X, Y). Ignored outside the grid. */
	void SetFlag(int32 X, int32 Y, ETidemarkCellFlag Flag, bool bEnabled);

	/**
	 * Ground elevation (cm) at a world position, bilinear between the four nearest cell centres, clamped to the grid:
	 * outside the grid the nearest edge value is returned. Zero on an invalid grid.
	 */
	float SampleElevationCm(const FVector2D& World) const;

	/** Cell containing a world position (floor), not clamped: test with IsInside. */
	FIntPoint WorldToCell(const FVector2D& World) const;

	/** Continuous cell coordinates of a world position: integer values fall on cell centres. */
	FVector2D WorldToCellContinuous(const FVector2D& World) const;

	/** World position (cm) of the centre of cell (X, Y). */
	FVector2D CellToWorld(int32 X, int32 Y) const;

	/**
	 * FNV-1a 64 of the grid content, in fixed order and explicit little-endian bytes so that the value is identical on every
	 * platform and in Tools/tmk.py: OriginWorld.X (double), OriginWorld.Y (double), CellSizeCm (float), SizeX (int32),
	 * SizeY (int32), then every ElevationCm (int16) and every Flags byte, row-major.
	 */
	uint64 ComputeSourceHash() const;

	/** Hex form (16 lower-case digits) of a source hash. */
	static FString HashToHex(uint64 Hash);

	/**
	 * Copies the grid into a solver: Ground = ElevationCm / 100 (the solver works in metres and seconds), then SetupSea
	 * with SeaLevelCm / 100 and OpenSeaDepthCm / 100, then Fixed is forced on every cell flagged OpenSea or Border
	 * (SetupSea rewrites Fixed from geometry alone, so the flags are applied after it, with the depth of a fixed cell).
	 * Forbidden is not read by the solver.
	 */
	void FillSolver(FTidemarkSolver& Out, float SeaLevelCm, float OpenSeaDepthCm) const;

	bool operator==(const FTidemarkGrid& Other) const;
	bool operator!=(const FTidemarkGrid& Other) const { return !(*this == Other); }
};
