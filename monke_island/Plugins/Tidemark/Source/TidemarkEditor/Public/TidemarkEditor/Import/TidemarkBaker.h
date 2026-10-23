// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "TidemarkEditor/Import/TidemarkHeightSource.h"

struct FTidemarkGrid;

/**
 * The bake: samples an ITidemarkHeightSource once at the centre of every cell of a new FTidemarkGrid.
 * Pure function of (source, settings): same inputs, same grid, same FTidemarkGrid::ComputeSourceHash.
 *
 * Steps (TidemarkBaker.cpp):
 *  1. bounds = settings.ResolveBounds(source) grown by OpenSeaMarginCm on every side; origin = bounds.Min;
 *     SizeX/SizeY = ceil(extent / CellSizeCm), refused above FTidemarkGrid::MaxCells;
 *  2. every cell is sampled at its centre, in row-major order; a refused sample counts as Missing;
 *  3. a Missing cell takes the lowest sampled value among its 8 neighbours, else MissingElevationCm, and is flagged Forbidden;
 *  4. heights are rounded and clamped to int16;
 *  5. Border on the outer ring, OpenSea where ElevationCm < SeaLevelCm - OpenSeaDepthCm (strict).
 */
namespace Tidemark::Bake
{
	/** Fills Out from Source. Out is reset first; on failure it stays empty and Result.Error says why. */
	TIDEMARKEDITOR_API FTidemarkBakeResult BakeGrid(const ITidemarkHeightSource& Source, const FTidemarkBakeSettings& Settings, FTidemarkGrid& Out);

	/** Grid dimensions a bake would produce for Bounds (already grown by the margin) at CellSizeCm; false when a size is not positive. */
	TIDEMARKEDITOR_API bool ComputeDimensions(const FBox2D& Bounds, float CellSizeCm, int32& OutSizeX, int32& OutSizeY);
}
