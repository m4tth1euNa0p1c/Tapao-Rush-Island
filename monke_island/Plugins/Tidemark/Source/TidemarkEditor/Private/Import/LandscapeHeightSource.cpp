// Tidemark plugin

#include "TidemarkEditor/Import/LandscapeHeightSource.h"
#include "TidemarkEditor/TidemarkEditorModule.h"

#include "Landscape.h"
#include "LandscapeComponent.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeInfo.h"
#include "LandscapeProxy.h"

FTidemarkLandscapeHeightSource::FTidemarkLandscapeHeightSource(ALandscapeProxy* InProxy)
	: Proxy(InProxy)
{
}

bool FTidemarkLandscapeHeightSource::Prepare(FString& OutError)
{
	bPrepared = false;
	RawHeights.Reset();
	Covered.Reset();
	BoundsWorld = FBox2D(ForceInit);

	ALandscapeProxy* LandscapeProxy = Proxy.Get();
	if (!LandscapeProxy)
	{
		OutError = TEXT("no Landscape proxy");
		return false;
	}
	ULandscapeInfo* Info = LandscapeProxy->GetLandscapeInfo();
	if (!Info)
	{
		OutError = FString::Printf(TEXT("Landscape '%s' has no ULandscapeInfo (editor world only)"), *LandscapeProxy->GetName());
		return false;
	}
	if (!Info->GetLandscapeExtent(MinX, MinY, MaxX, MaxY) || MaxX < MinX || MaxY < MinY)
	{
		OutError = FString::Printf(TEXT("Landscape '%s' has no loaded component"), *LandscapeProxy->GetName());
		return false;
	}

	const int64 VertsX = static_cast<int64>(MaxX) - MinX + 1;
	const int64 VertsY = static_cast<int64>(MaxY) - MinY + 1;
	if (VertsX * VertsY > MaxCachedVertices)
	{
		OutError = FString::Printf(TEXT("Landscape '%s' extent %lld x %lld vertices exceeds the cache budget: bake it by regions with BoundsOverride"),
			*LandscapeProxy->GetName(), VertsX, VertsY);
		return false;
	}
	Stride = static_cast<int32>(VertsX);

	// Vertex space <-> world: LandscapeActorToWorld() is the transform of the whole Landscape (section offsets removed),
	// shared by every proxy of the same ULandscapeInfo. Local Z is in Landscape units (height - 32768) / 128 before the scale.
	LandscapeToWorld = LandscapeProxy->LandscapeActorToWorld();
	WorldToLandscape = LandscapeToWorld.Inverse();

	// Read every vertex once. Vertices with no loaded component keep the pre-fill and are excluded by the Covered mask.
	RawHeights.Init(static_cast<uint16>(LandscapeDataAccess::MidValue), static_cast<int32>(VertsX * VertsY));
	{
		FLandscapeEditDataInterface EditData(Info);
		EditData.GetHeightDataFast(MinX, MinY, MaxX, MaxY, RawHeights.GetData(), Stride);
	}

	Covered.Init(false, static_cast<int32>(VertsX * VertsY));
	int32 ComponentCount = 0;
	for (const TPair<FIntPoint, ULandscapeComponent*>& Pair : Info->XYtoComponentMap)
	{
		const ULandscapeComponent* Component = Pair.Value;
		if (!Component)
		{
			continue;
		}
		++ComponentCount;
		const int32 Quads = Component->ComponentSizeQuads;
		const FIntPoint Base = Component->GetSectionBase();
		for (int32 Y = Base.Y; Y <= Base.Y + Quads; ++Y)
		{
			for (int32 X = Base.X; X <= Base.X + Quads; ++X)
			{
				if (X >= MinX && X <= MaxX && Y >= MinY && Y <= MaxY)
				{
					Covered[(Y - MinY) * Stride + (X - MinX)] = true;
				}
			}
		}
	}
	if (ComponentCount == 0)
	{
		OutError = FString::Printf(TEXT("Landscape '%s' has no component in XYtoComponentMap"), *LandscapeProxy->GetName());
		return false;
	}

	// World XY bounds: the four corners of the vertex extent (a rotated Landscape gives a rotated rectangle; its AABB is taken).
	const FVector Corners[4] = {
		LandscapeToWorld.TransformPosition(FVector(MinX, MinY, 0.0)),
		LandscapeToWorld.TransformPosition(FVector(MaxX, MinY, 0.0)),
		LandscapeToWorld.TransformPosition(FVector(MinX, MaxY, 0.0)),
		LandscapeToWorld.TransformPosition(FVector(MaxX, MaxY, 0.0)) };
	for (const FVector& Corner : Corners)
	{
		BoundsWorld += FVector2D(Corner.X, Corner.Y);
	}

	Description = FString::Printf(TEXT("landscape %s (%lld x %lld vertices, %d components)"), *LandscapeProxy->GetName(), VertsX, VertsY, ComponentCount);
	bPrepared = true;
	UE_LOG(LogTidemarkEditor, Log, TEXT("Tidemark landscape source ready: %s, world bounds %s"), *Description, *BoundsWorld.ToString());
	return true;
}

uint16 FTidemarkLandscapeHeightSource::GetRawHeight(int32 X, int32 Y) const
{
	return RawHeights[(Y - MinY) * Stride + (X - MinX)];
}

bool FTidemarkLandscapeHeightSource::IsCovered(int32 X, int32 Y) const
{
	return X >= MinX && X <= MaxX && Y >= MinY && Y <= MaxY && Covered[(Y - MinY) * Stride + (X - MinX)];
}

FBox2D FTidemarkLandscapeHeightSource::GetBoundsWorld() const
{
	return BoundsWorld;
}

bool FTidemarkLandscapeHeightSource::SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const
{
	if (!bPrepared)
	{
		return false;
	}
	// The vertex grid is a plane in Landscape space: the inverse transform of the XY position (any Z) gives the vertex coordinates.
	const FVector Local = WorldToLandscape.TransformPosition(FVector(WorldX, WorldY, 0.0));
	const double LX = Local.X;
	const double LY = Local.Y;
	if (LX < MinX || LY < MinY || LX > MaxX || LY > MaxY)
	{
		return false;
	}
	const int32 X0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(LX)), MinX, MaxX);
	const int32 Y0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(LY)), MinY, MaxY);
	const int32 X1 = FMath::Min(X0 + 1, MaxX);
	const int32 Y1 = FMath::Min(Y0 + 1, MaxY);
	if (!IsCovered(X0, Y0) || !IsCovered(X1, Y0) || !IsCovered(X0, Y1) || !IsCovered(X1, Y1))
	{
		return false;
	}
	const float TX = static_cast<float>(LX - X0);
	const float TY = static_cast<float>(LY - Y0);
	const float H00 = LandscapeDataAccess::GetLocalHeight(GetRawHeight(X0, Y0));
	const float H10 = LandscapeDataAccess::GetLocalHeight(GetRawHeight(X1, Y0));
	const float H01 = LandscapeDataAccess::GetLocalHeight(GetRawHeight(X0, Y1));
	const float H11 = LandscapeDataAccess::GetLocalHeight(GetRawHeight(X1, Y1));
	const float LocalZ = FMath::Lerp(FMath::Lerp(H00, H10, TX), FMath::Lerp(H01, H11, TX), TY);
	// Local height to world Z through the full transform (scale Z, rotation, translation).
	const FVector World = LandscapeToWorld.TransformPosition(FVector(LX, LY, LocalZ));
	OutHeightCm = static_cast<float>(World.Z);
	return true;
}

FString FTidemarkLandscapeHeightSource::GetSourceDescription() const
{
	return Description;
}
