// Tidemark plugin

#pragma once

#include "CoreMinimal.h"
#include "TidemarkEditor/Import/TidemarkHeightSource.h"

class ALandscapeProxy;
class ULandscapeInfo;

/**
 * Height source backed by a Landscape: all the proxies sharing the ULandscapeInfo of the actor given (one Landscape
 * split into streaming proxies is read whole, as far as its proxies are loaded).
 *
 * Reading strategy (chosen for safety and speed, see LandscapeHeightSource.cpp):
 *  - Prepare() reads the whole editor heightmap once with FLandscapeEditDataInterface::GetHeightDataFast over the extent of
 *    ULandscapeInfo (one lock per component texture, 2 bytes per vertex: a 2 km Landscape at 1 m is 8 MiB), and remembers
 *    which vertices belong to a loaded component (XYtoComponentMap). Heights are then bilinear in Landscape vertex space and
 *    transformed to world Z with ALandscapeProxy::LandscapeActorToWorld(), so a non-standard ZScale or a rotated Landscape is honoured.
 *  - Alternative, not used: ALandscapeProxy::GetHeightAtLocation(Location, EHeightfieldSource::Editor) per sample. Correct too,
 *    but it builds a FLandscapeComponentDataInterface (texture lock) on every call: minutes for 250 k cells.
 *  - Above MaxCachedVertices (a 16 k x 16 k Landscape), Prepare() refuses: bake with a BoundsOverride per region instead.
 * World Partition: proxies not loaded in the editor are absent from the cache (Missing cells); load the region first.
 */
class TIDEMARKEDITOR_API FTidemarkLandscapeHeightSource final : public ITidemarkHeightSource
{
public:
	/** Largest cached heightmap accepted (vertices): 16 384 x 16 384 x 2 bytes = 512 MiB. */
	static constexpr int64 MaxCachedVertices = 16384LL * 16384LL;

	/** Any proxy of the Landscape (ALandscape or ALandscapeStreamingProxy). Nothing is read until Prepare(). */
	explicit FTidemarkLandscapeHeightSource(ALandscapeProxy* InProxy);

	/** Reads the heightmap once. False with OutError when the proxy, its info or its extent is missing or too large. */
	bool Prepare(FString& OutError);

	/** True after a successful Prepare(). */
	bool IsPrepared() const { return bPrepared; }

	/** Landscape vertex extent read (inclusive). */
	FIntRect GetVertexExtent() const { return FIntRect(MinX, MinY, MaxX, MaxY); }

	//~ Begin ITidemarkHeightSource Interface
	virtual FBox2D GetBoundsWorld() const override;
	virtual bool SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const override;
	virtual FString GetSourceDescription() const override;
	//~ End ITidemarkHeightSource Interface

private:
	/** Cached editor height of vertex (X, Y), Landscape units (uint16, 32768 = 0). */
	uint16 GetRawHeight(int32 X, int32 Y) const;

	/** True when vertex (X, Y) belongs to a loaded component. */
	bool IsCovered(int32 X, int32 Y) const;

	TWeakObjectPtr<ALandscapeProxy> Proxy;
	FTransform LandscapeToWorld = FTransform::Identity;
	FTransform WorldToLandscape = FTransform::Identity;
	int32 MinX = 0;
	int32 MinY = 0;
	int32 MaxX = -1;
	int32 MaxY = -1;
	int32 Stride = 0;
	TArray<uint16> RawHeights;
	TBitArray<> Covered;
	FBox2D BoundsWorld = FBox2D(ForceInit);
	FString Description = TEXT("landscape (not prepared)");
	bool bPrepared = false;
};
