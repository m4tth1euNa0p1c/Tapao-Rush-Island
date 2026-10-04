// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"

/**
 *  Sea of an expedition outside the flood grid (T-141, REC finding 3). The grid of AFloodSimulationActor covers the
 *  fragment played; past its border the water used to fall back to the level of the map, so a boat or the galleon
 *  heading offshore dropped by metres. Outside the grid the sea now takes the same held rise as the open sea of the
 *  grid, over a box around the fragment (the grid, and the land of the fragment widened by a margin, about 1.5 km),
 *  then fades to no rise over a distance: the spawn and the rest of the map (more than 10 km away) never flood
 *  (owner, 2026-10-04: "the water rises only where I am"). Inside the grid, a band along its border blends the grid
 *  surface into that sea, so neither a step nor a seam shows or is felt.
 *  World XY in cm. No UObject: deterministic, the same on every machine, testable (monke_island.Flood.Outside).
 */
struct MONKE_ISLAND_API FFloodOutsideSea
{
	/** XY box of the grid (cm) */
	FBox2D GridBox = FBox2D(ForceInit);

	/** XY box where the sea outside the grid takes the full rise (cm): the grid plus the land widened by the margin */
	FBox2D FullRiseBox = FBox2D(ForceInit);

	/** Distance past FullRiseBox over which the rise fades to none (cm) */
	double FadeCm = 0.0;

	/** Width of the band inside the grid border blended into the outside sea (cm) */
	double EdgeBlendCm = 0.0;

	/**
	 *  Sea around a grid: full rise over the grid and over LandBox widened by MarginCm (LandBox may be invalid: no land
	 *  read, the grid alone), then a fade to no rise over InFadeCm
	 */
	static FFloodOutsideSea Make(const FBox2D& InGridBox, const FBox2D& LandBox, double MarginCm, double InFadeCm, double InEdgeBlendCm);

	/** Whether the sea is laid (a grid box is known) */
	bool IsValid() const { return GridBox.bIsValid && FullRiseBox.bIsValid; }

	/** Share of the rise the sea takes at a point: 1 over FullRiseBox, smooth fade to 0 over FadeCm, 0 beyond */
	double RiseWeight(const FVector2D& Point) const;

	/** Surface of the sea at a point outside the grid (cm) */
	double SurfaceCm(const FVector2D& Point, double BaseSeaLevelCm, double RiseCm) const
	{
		return BaseSeaLevelCm + RiseCm * RiseWeight(Point);
	}

	/** Farthest distance from the grid at which the sea still rises (cm) */
	double GetReachCm() const;

	/** Inside the grid: weight of the outside sea in the band along the border, 1 on the border, 0 past EdgeBlendCm */
	double EdgeBlend(const FVector2D& Point) const;

	/**
	 *  Water surface at a point (cm), inside or outside the grid. bInGrid: the point is a cell of the grid;
	 *  bGridWet / GridSurfaceCm: that cell holds water at that surface; GroundCm: the ground of the cell.
	 *  False where there is no water to report (a dry cell away from the border, or no rise at all out there).
	 */
	bool Surface(const FVector2D& Point, bool bInGrid, bool bGridWet, double GridSurfaceCm, double GroundCm,
		double BaseSeaLevelCm, double RiseCm, double& OutSurfaceCm) const;
};
