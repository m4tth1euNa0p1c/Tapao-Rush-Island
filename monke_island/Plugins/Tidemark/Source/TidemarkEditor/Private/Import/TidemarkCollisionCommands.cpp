// Tidemark plugin
//
// Editor console command `tidemark.BakeCollision <PackagePath> [CellSizeCm] [TopCm] [BottomCm]`:
// bakes the collision of the current editor world into a UTidemarkTerrainAsset, once, by batches of rays.
// Lives in its own file so that the collision importer (TM-09) never edits the import library of TM-08.

#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "Misc/PackageName.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "Tidemark/Grid/TidemarkTerrainAsset.h"
#include "Tidemark/Rules/TidemarkRules.h"
#include "TidemarkEditor/Import/CollisionHeightSource.h"
#include "TidemarkEditor/TidemarkEditorModule.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"

// The grid baker of TM-08 (Tidemark::Bake::BakeGrid). When its header is not there yet (parallel tickets), a minimal
// local bake samples the source at every cell centre so that the command still works; the asset creation is kept here
// on purpose (NewObject + SavePackage, no dependency on UTidemarkImportLibrary).
#if __has_include("TidemarkEditor/Import/TidemarkImportFactory.h")
#include "TidemarkEditor/Import/TidemarkImportFactory.h"
#define TIDEMARK_HAS_BAKE_GRID 1
#else
#define TIDEMARK_HAS_BAKE_GRID 0
#endif

namespace Tidemark::CollisionCommands
{
	/** Default cell of a collision bake when the command gives none: the simulation cell of the rules. */
	constexpr float DefaultCellSizeCm = Tidemark::Rules::DefaultSimCellSizeCm;

	/** Sea level the open sea flags are computed against in the local bake (cm, world Z). */
	constexpr float DefaultSeaLevelCm = 0.0f;

	/** True when an actor carries one of the ignore tags. */
	static bool IsIgnoredActor(const AActor& Actor, const TArray<FName>& Tags)
	{
		for (const FName& Tag : Tags)
		{
			if (!Tag.IsNone() && Actor.ActorHasTag(Tag))
			{
				return true;
			}
		}
		return false;
	}

	/**
	 * XY union of the collision bounds of every static mesh and instanced mesh of the world, tagged actors excluded.
	 * Only components with query collision count: a visual-only mesh never widens the bake.
	 */
	static FBox2D ComputeMeshBounds(UWorld& World, const TArray<FName>& IgnoreTags, int32& OutActors)
	{
		FBox2D Bounds(ForceInit);
		OutActors = 0;
		for (TActorIterator<AActor> It(&World); It; ++It)
		{
			const AActor* Actor = *It;
			if (!Actor || IsIgnoredActor(*Actor, IgnoreTags))
			{
				continue;
			}
			bool bCounted = false;
			for (const UActorComponent* Component : Actor->GetComponents())
			{
				const UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Component);
				if (!Primitive || !Primitive->IsRegistered() || !Primitive->IsCollisionEnabled())
				{
					continue;
				}
				const bool bStaticMesh = Primitive->IsA<UStaticMeshComponent>() || Primitive->IsA<UInstancedStaticMeshComponent>();
				if (!bStaticMesh)
				{
					continue;
				}
				const FBox Box = Primitive->Bounds.GetBox();
				if (!Box.IsValid)
				{
					continue;
				}
				Bounds += FVector2D(Box.Min.X, Box.Min.Y);
				Bounds += FVector2D(Box.Max.X, Box.Max.Y);
				bCounted = true;
			}
			if (bCounted)
			{
				++OutActors;
			}
		}
		return Bounds;
	}

#if !TIDEMARK_HAS_BAKE_GRID
	/** Minimal bake used until TM-08 ships Tidemark::Bake::BakeGrid: one sample per cell centre, open sea and border flags. */
	static bool LocalBakeGrid(const ITidemarkHeightSource& Source, const FBox2D& Bounds, float CellSizeCm, float SeaLevelCm,
		float OpenSeaDepthCm, FTidemarkGrid& OutGrid, int32& OutMissed)
	{
		const FVector2D Extent = Bounds.GetSize();
		const int32 SizeX = FMath::Max(1, FMath::CeilToInt(Extent.X / CellSizeCm));
		const int32 SizeY = FMath::Max(1, FMath::CeilToInt(Extent.Y / CellSizeCm));
		if (static_cast<int64>(SizeX) * SizeY > FTidemarkGrid::MaxCells)
		{
			return false;
		}
		OutGrid.Init(SizeX, SizeY, CellSizeCm, Bounds.Min);
		OutMissed = 0;
		const float OpenSeaCm = SeaLevelCm - OpenSeaDepthCm;
		for (int32 Y = 0; Y < SizeY; ++Y)
		{
			for (int32 X = 0; X < SizeX; ++X)
			{
				const FVector2D Centre = OutGrid.CellToWorld(X, Y);
				float HeightCm = 0.0f;
				const int32 Index = OutGrid.Index(X, Y);
				if (Source.SampleHeightCm(Centre.X, Centre.Y, HeightCm))
				{
					OutGrid.ElevationCm[Index] = static_cast<int16>(FMath::Clamp(FMath::RoundToInt(HeightCm), -32768, 32767));
				}
				else
				{
					// No ground under the ray: treated as deep open sea (the historical read used TraceBottomCm).
					OutGrid.ElevationCm[Index] = TNumericLimits<int16>::Min();
					++OutMissed;
				}
				if (static_cast<float>(OutGrid.ElevationCm[Index]) < OpenSeaCm)
				{
					OutGrid.SetFlag(X, Y, ETidemarkCellFlag::OpenSea, true);
				}
				if (X == 0 || Y == 0 || X == SizeX - 1 || Y == SizeY - 1)
				{
					OutGrid.SetFlag(X, Y, ETidemarkCellFlag::Border, true);
				}
			}
		}
		return true;
	}
#endif

	/** Creates (or replaces) the asset at PackagePath with the grid and saves its package. */
	static UTidemarkTerrainAsset* SaveTerrainAsset(const FString& PackagePath, const FTidemarkGrid& Grid, const TArray<FTidemarkCredit>& Credits)
	{
		if (!FPackageName::IsValidLongPackageName(PackagePath))
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: '%s' is not a long package name (expected /Game/... or /Tidemark/...)"), *PackagePath);
			return nullptr;
		}
		const FString AssetName = FPackageName::GetLongPackageAssetName(PackagePath);
		UPackage* Package = CreatePackage(*PackagePath);
		if (!Package)
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: cannot create package '%s'"), *PackagePath);
			return nullptr;
		}
		Package->FullyLoad();

		UTidemarkTerrainAsset* Asset = FindObject<UTidemarkTerrainAsset>(Package, *AssetName);
		const bool bNew = Asset == nullptr;
		if (bNew)
		{
			Asset = NewObject<UTidemarkTerrainAsset>(Package, FName(*AssetName), RF_Public | RF_Standalone);
		}
		Asset->Modify();
		Asset->Grid = Grid;
		Asset->Credits = Credits;
		Asset->RefreshSourceHash();
		if (bNew)
		{
			FAssetRegistryModule::AssetCreated(Asset);
		}
		Package->MarkPackageDirty();

		const FString FileName = FPackageName::LongPackageNameToFilename(PackagePath, FPackageName::GetAssetPackageExtension());
		FSavePackageArgs SaveArgs;
		SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
		SaveArgs.Error = GError;
		if (!UPackage::SavePackage(Package, Asset, *FileName, SaveArgs))
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: SavePackage failed for '%s'"), *FileName);
			return nullptr;
		}
		return Asset;
	}

	static void BakeCollision(const TArray<FString>& Args, UWorld* CommandWorld)
	{
		if (Args.Num() < 1)
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("Usage: tidemark.BakeCollision <PackagePath> [CellSizeCm=%.0f] [TraceTopCm=5000] [TraceBottomCm=-40000]"), DefaultCellSizeCm);
			return;
		}

		UWorld* World = CommandWorld;
		if (!World && GEditor)
		{
			World = GEditor->GetEditorWorldContext().World();
		}
		if (!World)
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: no editor world"));
			return;
		}

		const FString PackagePath = Args[0];
		const float CellSizeCm = Args.Num() > 1 ? FMath::Max(50.0f, FCString::Atof(*Args[1])) : DefaultCellSizeCm;

		FTidemarkCollisionSettings Settings;
		if (Args.Num() > 2)
		{
			Settings.TraceTopCm = FCString::Atof(*Args[2]);
		}
		if (Args.Num() > 3)
		{
			Settings.TraceBottomCm = FCString::Atof(*Args[3]);
		}

		int32 NumActors = 0;
		Settings.BoundsWorld = ComputeMeshBounds(*World, Settings.IgnoreActorTags, NumActors);
		if (!Settings.BoundsWorld.bIsValid)
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: no static mesh with collision in %s"), *World->GetOutermost()->GetName());
			return;
		}
		UE_LOG(LogTidemarkEditor, Log, TEXT("tidemark.BakeCollision: %d actors, bounds (%.0f, %.0f) - (%.0f, %.0f) cm, cell %.0f cm"),
			NumActors, Settings.BoundsWorld.Min.X, Settings.BoundsWorld.Min.Y, Settings.BoundsWorld.Max.X, Settings.BoundsWorld.Max.Y, CellSizeCm);

		const double StartSeconds = FPlatformTime::Seconds(); // tidemark-allow-random: editor timing only

		FTidemarkCollisionHeightSource Source(World, Settings);
		if (!Source.PrefetchBounds(CellSizeCm))
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: prefetch failed or cancelled (%s)"), *Source.GetStats().ToString());
			return;
		}

		FTidemarkGrid Grid;
		int32 NumMissed = Source.GetStats().NumMissed;
#if TIDEMARK_HAS_BAKE_GRID
		FTidemarkBakeSettings BakeSettings;
		BakeSettings.CellSizeCm = CellSizeCm;
		BakeSettings.SeaLevelCm = DefaultSeaLevelCm;
		BakeSettings.OpenSeaDepthCm = Tidemark::Rules::DefaultOpenSeaDepthCm;
		const FTidemarkBakeResult Result = Tidemark::Bake::BakeGrid(Source, BakeSettings, Grid);
		if (!Result.bSuccess)
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: BakeGrid failed: %s"), *Result.Message);
			return;
		}
#else
		if (!LocalBakeGrid(Source, Source.GetBoundsWorld(), CellSizeCm, DefaultSeaLevelCm, Tidemark::Rules::DefaultOpenSeaDepthCm, Grid, NumMissed))
		{
			UE_LOG(LogTidemarkEditor, Error, TEXT("tidemark.BakeCollision: the grid exceeds the %d-cell budget"), FTidemarkGrid::MaxCells);
			return;
		}
#endif

		// Credit: the level's own meshes. Producer and licence are the project's; the user completes them in the asset.
		FTidemarkCredit Credit;
		Credit.Producer = FString::Printf(TEXT("Collision bake of %s"), *World->GetOutermost()->GetName());
		Credit.License = TEXT("Project content (edit in the asset)");
		Credit.Url = World->GetOutermost()->GetName();
		Credit.Date = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d")); // tidemark-allow-random: editor timing only, credit date

		UTidemarkTerrainAsset* Asset = SaveTerrainAsset(PackagePath, Grid, { Credit });
		const double Seconds = FPlatformTime::Seconds() - StartSeconds; // tidemark-allow-random: editor timing only
		if (!Asset)
		{
			return;
		}
		UE_LOG(LogTidemarkEditor, Log,
			TEXT("tidemark.BakeCollision: %s baked in %.1f s: %d x %d cells of %.0f cm (%.2f MiB), %d missed, hash %s, source: %s"),
			*PackagePath, Seconds, Grid.SizeX, Grid.SizeY, Grid.CellSizeCm, Grid.NumBytes() / (1024.0 * 1024.0), NumMissed,
			*Asset->SourceHashHex, *Source.GetSourceDescription());
	}

	static FAutoConsoleCommandWithWorldAndArgs GBakeCollisionCommand(
		TEXT("tidemark.BakeCollision"),
		TEXT("Bakes the collision of the editor world into a UTidemarkTerrainAsset, once, by batches of rays: tidemark.BakeCollision <PackagePath> [CellSizeCm] [TraceTopCm] [TraceBottomCm]"),
		FConsoleCommandWithWorldAndArgsDelegate::CreateStatic(&BakeCollision));
}
