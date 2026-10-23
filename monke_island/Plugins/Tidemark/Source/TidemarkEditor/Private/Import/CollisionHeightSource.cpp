// Tidemark plugin

#include "TidemarkEditor/Import/CollisionHeightSource.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/ScopedSlowTask.h"
#include "Tidemark/Grid/TidemarkGrid.h"
#include "TidemarkEditor/TidemarkEditorModule.h"

#define LOCTEXT_NAMESPACE "TidemarkCollisionHeightSource"

FString FTidemarkCollisionBakeStats::ToString() const
{
	return FString::Printf(TEXT("%d cells, %d hit, %d missed (%d exhausted), %d retried, %d ignored hits, %lld rays, %.2f s%s"),
		NumCells, NumHit, NumMissed, NumExhausted, NumRetriedCells, NumIgnoredHits, NumRays, Seconds,
		bCancelled ? TEXT(", cancelled") : TEXT(""));
}

FTidemarkCollisionHeightSource::FTidemarkCollisionHeightSource(UWorld* InWorld, const FTidemarkCollisionSettings& InSettings)
	: World(InWorld)
	, Settings(InSettings)
{
	Settings.MaxRetries = FMath::Max(1, Settings.MaxRetries);
	Settings.BatchSize = FMath::Max(256, Settings.BatchSize);
	Settings.RetryStepCm = FMath::Max(0.1f, Settings.RetryStepCm);
	if (Settings.TraceBottomCm >= Settings.TraceTopCm)
	{
		UE_LOG(LogTidemarkEditor, Warning, TEXT("Collision source: TraceBottomCm (%.0f) is not under TraceTopCm (%.0f), nothing will be hit"),
			Settings.TraceBottomCm, Settings.TraceTopCm);
	}
}

bool FTidemarkCollisionHeightSource::ShouldIgnoreHit(const FHitResult& Hit, const FTidemarkCollisionSettings& InSettings)
{
	const UPrimitiveComponent* Component = Hit.GetComponent();
	if (InSettings.bIgnoreInstancedFoliage && Cast<UInstancedStaticMeshComponent>(Component))
	{
		return true;
	}
	if (InSettings.IgnoreActorTags.Num() == 0)
	{
		return false;
	}
	const AActor* Actor = Hit.GetActor();
	for (const FName& Tag : InSettings.IgnoreActorTags)
	{
		if (Tag.IsNone())
		{
			continue;
		}
		if (Actor && Actor->ActorHasTag(Tag))
		{
			return true;
		}
		if (Component && Component->ComponentHasTag(Tag))
		{
			return true;
		}
	}
	return false;
}

bool FTidemarkCollisionHeightSource::TraceCell(UWorld& InWorld, const FTidemarkCollisionSettings& InSettings, double WorldX, double WorldY,
	float& OutHeightCm, int32& OutIgnoredHits, int64& InOutRays)
{
	OutIgnoredHits = 0;

	FCollisionQueryParams Params(SCENE_QUERY_STAT(TidemarkCollisionBake), InSettings.bTraceComplex);
	Params.bReturnPhysicalMaterial = false;
	const FCollisionObjectQueryParams Objects(InSettings.Channel.GetValue());

	double Top = InSettings.TraceTopCm;
	const FVector End(WorldX, WorldY, InSettings.TraceBottomCm);

	// Highest ground under the max level: first hit from the top that is not foliage nor a tagged actor.
	// An ignored hit resumes the ray just under the impact, the ignored actor being excluded from the next casts.
	for (int32 Attempt = 0; Attempt <= InSettings.MaxRetries; ++Attempt)
	{
		if (Top <= InSettings.TraceBottomCm)
		{
			return false;
		}
		const FVector Start(WorldX, WorldY, Top);
		FHitResult Hit;
		++InOutRays;
		const bool bHit = InSettings.bTraceByObjectType
			? InWorld.LineTraceSingleByObjectType(Hit, Start, End, Objects, Params)
			: InWorld.LineTraceSingleByChannel(Hit, Start, End, InSettings.Channel.GetValue(), Params);
		if (!bHit)
		{
			return false;
		}
		if (ShouldIgnoreHit(Hit, InSettings))
		{
			++OutIgnoredHits;
			if (const AActor* IgnoredActor = Hit.GetActor())
			{
				// A tagged actor (water surface, helper) is excluded for the rest of this cell; a foliage component
				// is excluded alone, its actor may own the ground too (one actor per foliage type).
				if (Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
				{
					Params.AddIgnoredComponent(Hit.GetComponent());
				}
				else
				{
					Params.AddIgnoredActor(IgnoredActor);
				}
			}
			Top = Hit.ImpactPoint.Z - InSettings.RetryStepCm;
			continue;
		}
		OutHeightCm = static_cast<float>(Hit.ImpactPoint.Z);
		return true;
	}
	// MaxRetries ignored hits in a row: the cell is missed, the caller counts it as exhausted.
	return false;
}

bool FTidemarkCollisionHeightSource::Prefetch(int32 InSizeX, int32 InSizeY, const FVector2D& InOrigin, float InCellSizeCm)
{
	SizeX = 0;
	SizeY = 0;
	HeightsCm.Reset();
	Missed.Reset();
	Stats = FTidemarkCollisionBakeStats();

	UWorld* TraceWorld = World.Get();
	if (!TraceWorld)
	{
		UE_LOG(LogTidemarkEditor, Error, TEXT("Collision source: no world to trace"));
		return false;
	}
	if (InSizeX <= 0 || InSizeY <= 0 || InCellSizeCm <= 0.0f)
	{
		UE_LOG(LogTidemarkEditor, Error, TEXT("Collision source: invalid lattice %d x %d, cell %.1f cm"), InSizeX, InSizeY, InCellSizeCm);
		return false;
	}
	const int64 NumCells64 = static_cast<int64>(InSizeX) * static_cast<int64>(InSizeY);
	if (NumCells64 > FTidemarkGrid::MaxCells)
	{
		UE_LOG(LogTidemarkEditor, Error, TEXT("Collision source: lattice %d x %d (%lld cells) exceeds the grid budget of %d cells"),
			InSizeX, InSizeY, NumCells64, FTidemarkGrid::MaxCells);
		return false;
	}

	SizeX = InSizeX;
	SizeY = InSizeY;
	Origin = InOrigin;
	CellSizeCm = InCellSizeCm;
	const int32 NumCells = static_cast<int32>(NumCells64);
	HeightsCm.SetNumZeroed(NumCells);
	Missed.Init(1, NumCells);
	Stats.NumCells = NumCells;

	const int32 NumBatches = FMath::DivideAndRoundUp(NumCells, Settings.BatchSize);
	const double StartSeconds = FPlatformTime::Seconds(); // tidemark-allow-random: editor timing only

	FScopedSlowTask SlowTask(static_cast<float>(NumBatches),
		FText::Format(LOCTEXT("PrefetchTask", "Tidemark: tracing the collision of {0} x {1} cells"), InSizeX, InSizeY));
	SlowTask.MakeDialog(true);

	for (int32 Batch = 0; Batch < NumBatches; ++Batch)
	{
		if (SlowTask.ShouldCancel())
		{
			Stats.bCancelled = true;
			break;
		}
		const int32 Begin = Batch * Settings.BatchSize;
		const int32 End = FMath::Min(NumCells, Begin + Settings.BatchSize);
		SlowTask.EnterProgressFrame(1.0f,
			FText::Format(LOCTEXT("PrefetchBatch", "Tidemark: rays {0} / {1}"), End, NumCells));
		TraceBatch(*TraceWorld, Begin, End);
	}

	// Cells never traced (cancelled) stay missed.
	Stats.NumMissed = NumCells - Stats.NumHit;
	Stats.Seconds = FPlatformTime::Seconds() - StartSeconds; // tidemark-allow-random: editor timing only
	UE_LOG(LogTidemarkEditor, Log, TEXT("Collision source: %s"), *Stats.ToString());
	return !Stats.bCancelled;
}

bool FTidemarkCollisionHeightSource::PrefetchBounds(float InCellSizeCm)
{
	if (!Settings.BoundsWorld.bIsValid || InCellSizeCm <= 0.0f)
	{
		UE_LOG(LogTidemarkEditor, Error, TEXT("Collision source: PrefetchBounds needs valid bounds and a positive cell size"));
		return false;
	}
	const FVector2D Extent = Settings.BoundsWorld.GetSize();
	const int32 CellsX = FMath::Max(1, FMath::CeilToInt(Extent.X / InCellSizeCm));
	const int32 CellsY = FMath::Max(1, FMath::CeilToInt(Extent.Y / InCellSizeCm));
	return Prefetch(CellsX, CellsY, Settings.BoundsWorld.Min, InCellSizeCm);
}

void FTidemarkCollisionHeightSource::TraceBatch(UWorld& InWorld, int32 Begin, int32 End)
{
	for (int32 Index = Begin; Index < End; ++Index)
	{
		const int32 X = Index % SizeX;
		const int32 Y = Index / SizeX;
		const double WX = Origin.X + (X + 0.5) * CellSizeCm;
		const double WY = Origin.Y + (Y + 0.5) * CellSizeCm;

		float HeightCm = 0.0f;
		int32 IgnoredHits = 0;
		const bool bHit = TraceCell(InWorld, Settings, WX, WY, HeightCm, IgnoredHits, Stats.NumRays);
		Stats.NumIgnoredHits += IgnoredHits;
		if (IgnoredHits > 0)
		{
			++Stats.NumRetriedCells;
		}
		if (bHit)
		{
			HeightsCm[Index] = HeightCm;
			Missed[Index] = 0;
			++Stats.NumHit;
		}
		else if (IgnoredHits > Settings.MaxRetries)
		{
			++Stats.NumExhausted;
		}
	}
}

bool FTidemarkCollisionHeightSource::GetCellHeightCm(int32 X, int32 Y, float& OutHeightCm) const
{
	if (!IsPrefetched() || X < 0 || Y < 0 || X >= SizeX || Y >= SizeY)
	{
		return false;
	}
	const int32 Index = Y * SizeX + X;
	if (Missed[Index])
	{
		return false;
	}
	OutHeightCm = HeightsCm[Index];
	return true;
}

bool FTidemarkCollisionHeightSource::IsCellMissed(int32 X, int32 Y) const
{
	if (!IsPrefetched() || X < 0 || Y < 0 || X >= SizeX || Y >= SizeY)
	{
		return false;
	}
	return Missed[Y * SizeX + X] != 0;
}

FBox2D FTidemarkCollisionHeightSource::GetBoundsWorld() const
{
	if (IsPrefetched())
	{
		return FBox2D(Origin, Origin + FVector2D(SizeX * CellSizeCm, SizeY * CellSizeCm));
	}
	return Settings.BoundsWorld;
}

bool FTidemarkCollisionHeightSource::SampleHeightCm(double WorldX, double WorldY, float& OutHeightCm) const
{
	if (!IsPrefetched())
	{
		return false;
	}

	// Continuous cell coordinates (integers on cell centres), clamped between the first and the last centre.
	const double CX = FMath::Clamp((WorldX - Origin.X) / CellSizeCm - 0.5, 0.0, static_cast<double>(SizeX - 1));
	const double CY = FMath::Clamp((WorldY - Origin.Y) / CellSizeCm - 0.5, 0.0, static_cast<double>(SizeY - 1));
	const int32 X0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(CX)), 0, SizeX - 1);
	const int32 Y0 = FMath::Clamp(static_cast<int32>(FMath::FloorToDouble(CY)), 0, SizeY - 1);
	const int32 X1 = FMath::Min(X0 + 1, SizeX - 1);
	const int32 Y1 = FMath::Min(Y0 + 1, SizeY - 1);
	const double TX = CX - static_cast<double>(X0);
	const double TY = CY - static_cast<double>(Y0);

	const int32 Cells[4] = { Y0 * SizeX + X0, Y0 * SizeX + X1, Y1 * SizeX + X0, Y1 * SizeX + X1 };
	const double Weights[4] = { (1.0 - TX) * (1.0 - TY), TX * (1.0 - TY), (1.0 - TX) * TY, TX * TY };

	// Missed neighbours are dropped and the remaining weights renormalised, so that a shoreline cell next to a
	// missed one keeps its own height instead of a hole.
	double Sum = 0.0;
	double TotalWeight = 0.0;
	for (int32 Corner = 0; Corner < 4; ++Corner)
	{
		if (Missed[Cells[Corner]] || Weights[Corner] <= 0.0)
		{
			continue;
		}
		Sum += Weights[Corner] * HeightsCm[Cells[Corner]];
		TotalWeight += Weights[Corner];
	}
	if (TotalWeight <= 0.0)
	{
		return false;
	}
	OutHeightCm = static_cast<float>(Sum / TotalWeight);
	return true;
}

FString FTidemarkCollisionHeightSource::GetSourceDescription() const
{
	const UWorld* TraceWorld = World.Get();
	return FString::Printf(TEXT("Collision rays of %s (%s, top %.0f cm, bottom %.0f cm, %d x %d cells of %.0f cm)"),
		TraceWorld ? *TraceWorld->GetOutermost()->GetName() : TEXT("<no world>"),
		Settings.bTraceByObjectType ? TEXT("object type") : TEXT("trace channel"),
		Settings.TraceTopCm, Settings.TraceBottomCm, SizeX, SizeY, CellSizeCm);
}

#undef LOCTEXT_NAMESPACE
