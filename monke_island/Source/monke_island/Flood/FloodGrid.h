// monke_island

#pragma once

#include "CoreMinimal.h"

/**
 *  Shallow water on a height grid with the virtual pipe model (Mei, Decaudin, Hu 2007):
 *  each cell exchanges water with its 4 neighbours through pipes whose flux accelerates with the
 *  difference of water surface. Water runs downhill, fills the hollows and only crosses a ridge
 *  once it overflows it, so the flood follows the real topography of the ground.
 *  Fixed cells (open sea, grid border) hold their surface at the sea level and feed the flood.
 *  Units: metres and seconds. No UObject: deterministic and testable.
 */
class MONKE_ISLAND_API FFloodGrid
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

	/** Advances the water by Dt seconds with the sea at SeaLevel (m) */
	void Step(float Dt, float SeaLevel, float Gravity = 9.81f, float FluxDamping = 0.995f);

	/** Water surface of a cell (m) */
	float Surface(int32 CellIndex) const { return Ground[CellIndex] + Depth[CellIndex]; }

	/** Total water volume held by the free (non fixed) cells (m3) */
	double FreeVolume() const;

private:

	int32 SizeX = 0;
	int32 SizeY = 0;
	float CellSize = 1.0f;

	/** Outflow towards left, right, down, up (m3/s) */
	TArray<FVector4f> Flux;
};
