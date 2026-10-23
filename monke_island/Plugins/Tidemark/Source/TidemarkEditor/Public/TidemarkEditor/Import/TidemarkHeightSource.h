// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Math/Box2D.h"
#include "TidemarkHeightSource.generated.h"

/**
 * One source of ground heights the baker samples once at import (Tidemark cahier section 6.2): a Landscape
 * (FTidemarkLandscapeHeightSource), a heightmap file (FTidemarkTextureHeightSource), a set of collision meshes
 * (FTidemarkCollisionHeightSource, batched traces fired once) or a function in memory (FTidemarkFunctionHeightSource, specs).
 *
 * World coordinates are the level's XY plane in centimetres; heights are world Z in centimetres. A source answers
 * false from SampleHeightCm where it has no data (outside its bounds, hole, no component): the baker turns such cells
 * into Forbidden cells. The baker only reads a source, never writes it, and never calls it from several threads at once.
 */
class TIDEMARKEDITOR_API ITidemarkHeightSource
{
public:
	virtual ~ITidemarkHeightSource() = default;

	/** World XY rectangle (cm) covered by the source. The baker samples this rectangle, enlarged by the open sea margin. */
	virtual FBox2D GetBoundsWorld() const = 0;

	/** Ground height (cm, world Z) at a world XY position. False when the source has no data there; OutHeightCm is then untouched. */
	virtual bool SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const = 0;

	/** Short human-readable description used in logs and in the bake result (file name, actor name, number of traces). */
	virtual FString GetSourceDescription() const = 0;

	/**
	 * True for a source whose data is produced asynchronously (collision traces fired in batches): such a source must have
	 * finished its own preparation before it is handed to the baker, which samples synchronously on the calling thread.
	 */
	virtual bool IsAsync() const { return false; }
};

/**
 * Parameters of one bake (ITidemarkHeightSource -> FTidemarkGrid). No value here is a gameplay rule: the import library
 * fills them from UTidemarkRules (cell size, sea level, open sea depth) and from the user (margin, bounds override).
 */
USTRUCT(BlueprintType)
struct TIDEMARKEDITOR_API FTidemarkBakeSettings
{
	GENERATED_BODY()

	/** Edge of a grid cell (cm). Keep it an integer multiple of the simulation cell size (UTidemarkRules::SimCellSizeCm). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Bake", meta = (ClampMin = "1.0", Units = "cm"))
	float CellSizeCm = 600.0f;

	/** Sea level at rest (cm, world Z) the open sea test is made against. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Bake", meta = (Units = "cm"))
	float SeaLevelCm = 0.0f;

	/** A cell lower than SeaLevelCm - OpenSeaDepthCm is flagged OpenSea (cm). Same meaning as UTidemarkRules::OpenSeaDepthCm. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Bake", meta = (ClampMin = "0.0", Units = "cm"))
	float OpenSeaDepthCm = 150.0f;

	/** Band added around the source bounds on every side (cm), so that the island is surrounded by open sea cells. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Bake", meta = (ClampMin = "0.0", Units = "cm"))
	float OpenSeaMarginCm = 0.0f;

	/**
	 * Elevation given to a cell the source could not sample and that has no sampled neighbour (cm). The default sits far
	 * under any sea level so that such a cell is open sea; set it above the sea level to make unknown cells a wall instead.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Bake", meta = (Units = "cm"))
	float MissingElevationCm = -30000.0f;

	/** When valid (bIsValid), replaces the source bounds (cm, world XY) before the margin is added. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Bake")
	FBox2D BoundsOverride = FBox2D(ForceInit);

	/** The rectangle the bake covers before the margin: the override when valid, else the source bounds. */
	FBox2D ResolveBounds(const ITidemarkHeightSource& Source) const
	{
		return BoundsOverride.bIsValid ? BoundsOverride : Source.GetBoundsWorld();
	}
};

/** Outcome of one bake: success flag, error text, wall time and sample statistics. */
USTRUCT(BlueprintType)
struct TIDEMARKEDITOR_API FTidemarkBakeResult
{
	GENERATED_BODY()

	/** True when the grid was produced. */
	UPROPERTY(BlueprintReadOnly, Category = "Tidemark|Bake")
	bool bOk = false;

	/** Why the bake failed; empty on success. */
	UPROPERTY(BlueprintReadOnly, Category = "Tidemark|Bake")
	FString Error;

	/** Wall-clock duration of the bake (s), for the import budget (Landscape 2 km < 60 s, heightmap < 5 s). Editor timing only. */
	UPROPERTY(BlueprintReadOnly, Category = "Tidemark|Bake")
	double Seconds = 0.0;

	/** Cells the source answered. */
	UPROPERTY(BlueprintReadOnly, Category = "Tidemark|Bake")
	int32 Sampled = 0;

	/** Cells the source could not answer: flagged Forbidden, elevation filled from the lowest sampled neighbour or MissingElevationCm. */
	UPROPERTY(BlueprintReadOnly, Category = "Tidemark|Bake")
	int32 Missing = 0;
};

/**
 * Height source backed by a function, for the specs and for synthetic terrains: the function returns false where the
 * terrain has no data. No world, no file.
 */
class TIDEMARKEDITOR_API FTidemarkFunctionHeightSource final : public ITidemarkHeightSource
{
public:
	/** Height function: (WorldX, WorldY, OutHeightCm) -> sampled. */
	using FHeightFunction = TFunction<bool(double, double, float&)>;

	FTidemarkFunctionHeightSource(const FBox2D& InBounds, FHeightFunction InFunction, const FString& InDescription = TEXT("function"))
		: Bounds(InBounds)
		, Function(MoveTemp(InFunction))
		, Description(InDescription)
	{
	}

	//~ Begin ITidemarkHeightSource Interface
	virtual FBox2D GetBoundsWorld() const override { return Bounds; }
	virtual bool SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const override
	{
		return Function ? Function(WorldX, WorldY, OutHeightCm) : false;
	}
	virtual FString GetSourceDescription() const override { return Description; }
	//~ End ITidemarkHeightSource Interface

private:
	FBox2D Bounds;
	FHeightFunction Function;
	FString Description;
};
