// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "Engine/EngineTypes.h"
#include "TidemarkEditor/Import/TidemarkHeightSource.h"
#include "CollisionHeightSource.generated.h"

class UWorld;

/**
 * Settings of a collision bake: where to trace, on which channel, what to ignore.
 * Values that used to live in the game rules (TraceTopCm, TraceBottomCm) are explicit here: the bake is an editor
 * operation, run once, never per frame.
 */
USTRUCT(BlueprintType)
struct TIDEMARKEDITOR_API FTidemarkCollisionSettings
{
	GENERATED_BODY()

	/** World XY rectangle (cm) covered by the bake. Empty box: the caller must fill it (union of the level's static meshes). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision")
	FBox2D BoundsWorld = FBox2D(ForceInit);

	/** Height (cm) every ray starts from: the "max level" under which the highest ground is kept. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision", meta = (Units = "cm"))
	float TraceTopCm = 5000.0f;

	/** Lowest point (cm) a ray reaches: a cell with no hit above it is reported as missed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision", meta = (Units = "cm"))
	float TraceBottomCm = -40000.0f;

	/** Collision channel of the bake: used as an object type (default) or as a trace channel, see bTraceByObjectType. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision")
	TEnumAsByte<ECollisionChannel> Channel = ECC_WorldStatic;

	/**
	 * True: Channel names the object type to hit (LineTraceSingleByObjectType, the historical behaviour of the game's
	 * ground read). False: Channel is a trace channel the components must block (LineTraceSingleByChannel), for example
	 * a dedicated ECC_GameTraceChannel reserved by the project.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision")
	bool bTraceByObjectType = true;

	/** Actors (or components) carrying one of these tags never count as ground: water surfaces, helpers, props. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision")
	TArray<FName> IgnoreActorTags = { TEXT("TidemarkIgnore"), TEXT("IslandWaterSurface") };

	/** True: hits on UInstancedStaticMeshComponent (foliage, scattered vegetation) are skipped and the ray resumes below them. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision")
	bool bIgnoreInstancedFoliage = true;

	/** How many ignored hits a ray may skip before the cell is reported as missed (dense vegetation stacks). */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision", meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxRetries = 4;

	/** Rays per batch: one progress step per batch, the editor stays responsive between batches. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision", meta = (ClampMin = "256"))
	int32 BatchSize = 16384;

	/** True: complex (per-triangle) collision, needed for Nanite tiles whose simple collision is a box. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision")
	bool bTraceComplex = true;

	/** Distance (cm) the ray resumes under an ignored hit, so that the same surface is not hit twice. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Tidemark|Collision", meta = (ClampMin = "0.1", Units = "cm"))
	float RetryStepCm = 5.0f;
};

/** Counters of one Prefetch, for the import report. */
struct TIDEMARKEDITOR_API FTidemarkCollisionBakeStats
{
	/** Cells traced. */
	int32 NumCells = 0;

	/** Cells whose first non-ignored hit gave a ground height. */
	int32 NumHit = 0;

	/** Cells with no ground between TraceTopCm and TraceBottomCm, or more ignored hits than MaxRetries. */
	int32 NumMissed = 0;

	/** Ignored hits skipped (foliage instances, tagged actors), summed over every cell. */
	int32 NumIgnoredHits = 0;

	/** Cells whose ray needed at least one retry. */
	int32 NumRetriedCells = 0;

	/** Cells that gave up after MaxRetries ignored hits. */
	int32 NumExhausted = 0;

	/** Rays cast, retries included. */
	int64 NumRays = 0;

	/** Wall-clock duration of the prefetch, seconds (editor timing, never written into the simulation). */
	double Seconds = 0.0;

	/** True when the user cancelled the slow task: the cache holds the cells traced so far, the rest is missed. */
	bool bCancelled = false;

	/** One-line summary for the log. */
	FString ToString() const;
};

/**
 * Height source read from the collision of a level: vertical rays cast once, in batches, at import.
 *
 * Replaces the per-frame ground read of the game's flood actor (16 000 rays per frame while the grid loaded) with a
 * single pre-bake: Prefetch casts every ray of a SizeX x SizeY lattice of cell centres and stores a height and a
 * "missed" flag per cell; SampleHeightCm then reads that cache bilinearly and never traces. The rule is "highest ground
 * under the max level": the first hit from TraceTopCm downwards that is neither an instanced foliage component nor an
 * actor or component carrying one of IgnoreActorTags. An ignored hit resumes the ray RetryStepCm below the impact,
 * up to MaxRetries times, like the historical read; overhangs and caves therefore give the roof, which the import
 * report documents.
 *
 * Synchronous by design in V1: FScopedSlowTask shows one step per batch and allows cancelling. V1.1 note: the same
 * loop can be made asynchronous with UWorld::AsyncLineTraceByChannel / AsyncLineTraceByObjectType (one FTraceHandle per
 * ray, results collected with QueryTraceData on the next tick, or an FTraceDelegate per batch); the cache layout and
 * the retry rule stay identical, only the dispatch changes. Keep the batch order fixed so that the result is the same
 * whichever dispatch is used.
 *
 * The source never looks at mesh names (Tidemark conventions): ignores come from component classes and actor tags only.
 */
class TIDEMARKEDITOR_API FTidemarkCollisionHeightSource : public ITidemarkHeightSource
{
public:
	FTidemarkCollisionHeightSource(UWorld* InWorld, const FTidemarkCollisionSettings& InSettings);
	virtual ~FTidemarkCollisionHeightSource() = default;

	/**
	 * Casts every ray of the lattice: cell (X, Y) is traced at Origin + (X + 0.5, Y + 0.5) * CellSizeCm, in row-major
	 * order, by batches of Settings.BatchSize with a slow task. Returns false when the world is gone, the sizes are not
	 * positive or the lattice exceeds FTidemarkGrid::MaxCells; the cache is then empty and every sample fails.
	 * Calling it again replaces the cache. No ray is cast after it returns.
	 */
	bool Prefetch(int32 InSizeX, int32 InSizeY, const FVector2D& InOrigin, float InCellSizeCm);

	/** Same lattice derived from Settings.BoundsWorld and a cell size: Origin = BoundsWorld.Min, sizes = ceil(extent / cell). */
	bool PrefetchBounds(float InCellSizeCm);

	/** True once Prefetch succeeded. */
	bool IsPrefetched() const { return SizeX > 0 && SizeY > 0; }

	/** Counters of the last Prefetch. */
	const FTidemarkCollisionBakeStats& GetStats() const { return Stats; }

	/** Lattice traced by the last Prefetch. */
	int32 GetSizeX() const { return SizeX; }
	int32 GetSizeY() const { return SizeY; }
	const FVector2D& GetOrigin() const { return Origin; }
	float GetCellSizeCm() const { return CellSizeCm; }

	/** Cached height (cm) of lattice cell (X, Y); false when the cell was missed, or outside the lattice, or nothing was prefetched. */
	bool GetCellHeightCm(int32 X, int32 Y, float& OutHeightCm) const;

	/** True when lattice cell (X, Y) was traced and missed. False outside the lattice. */
	bool IsCellMissed(int32 X, int32 Y) const;

	/** Settings the source was built with. */
	const FTidemarkCollisionSettings& GetSettings() const { return Settings; }

	//~ Begin ITidemarkHeightSource Interface
	/** Settings.BoundsWorld, or the lattice rectangle once prefetched. */
	virtual FBox2D GetBoundsWorld() const override;
	/**
	 * Bilinear read of the cache between the four nearest cell centres, clamped to the lattice; missed neighbours are
	 * dropped from the weights. False when nothing was prefetched or every neighbour was missed.
	 */
	virtual bool SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const override;
	virtual FString GetSourceDescription() const override;
	//~ End ITidemarkHeightSource Interface

	/**
	 * Casts the ray of one world position with the retry rule, on a world. Public for the spec and for tools that need
	 * one height (for example a lint probe); the importer never calls it after Prefetch.
	 * OutHeightCm receives the ground; OutIgnoredHits the number of skipped hits. Returns false when the cell is missed.
	 */
	static bool TraceCell(UWorld& World, const FTidemarkCollisionSettings& InSettings, double WorldX, double WorldY,
		float& OutHeightCm, int32& OutIgnoredHits, int64& InOutRays);

	/** True when a hit must be skipped: instanced foliage (when enabled) or an actor / component carrying an ignore tag. */
	static bool ShouldIgnoreHit(const FHitResult& Hit, const FTidemarkCollisionSettings& InSettings);

private:
	/** Traces one batch of row-major cells [Begin, End). */
	void TraceBatch(UWorld& World, int32 Begin, int32 End);

	/** World the rays are cast in; weak so that a closed level cannot be traced. */
	TWeakObjectPtr<UWorld> World;

	FTidemarkCollisionSettings Settings;

	/** Lattice of the last Prefetch. */
	int32 SizeX = 0;
	int32 SizeY = 0;
	FVector2D Origin = FVector2D::ZeroVector;
	float CellSizeCm = 0.0f;

	/** Cached ground (cm) per cell, row-major; meaningless where Missed is set. */
	TArray<float> HeightsCm;

	/** One byte per cell: 1 when the ray found no ground. */
	TArray<uint8> Missed;

	FTidemarkCollisionBakeStats Stats;
};
