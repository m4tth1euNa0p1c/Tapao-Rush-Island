// monke_island

#include "Items/LootScatterSpawner.h"
#include "Items/LootMath.h"
#include "Items/LootRules.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

namespace
{
	/** Quiet zones of the level (the marae): actors tagged TikiQuietZone with a sphere component, as for the tiki */
	TArray<FSphere> FindLootQuietZones(UWorld* World)
	{
		static const FName QuietZoneTag(TEXT("TikiQuietZone"));
		TArray<FSphere> Zones;
		for (TActorIterator<AActor> It(World); It; ++It)
		{
			if (!It->ActorHasTag(QuietZoneTag))
			{
				continue;
			}
			if (const USphereComponent* Sphere = It->FindComponentByClass<USphereComponent>())
			{
				Zones.Emplace(It->GetActorLocation(), Sphere->GetScaledSphereRadius());
			}
		}
		return Zones;
	}

	bool IsInLootQuietZone(const FVector& Location, const TArray<FSphere>& Zones)
	{
		for (const FSphere& Zone : Zones)
		{
			if (FVector::DistSquared2D(Location, Zone.Center) <= FMath::Square(Zone.W))
			{
				return true;
			}
		}
		return false;
	}

	bool HasKeyword(const FString& Name, const TArray<FString>& Keywords)
	{
		for (const FString& Keyword : Keywords)
		{
			if (!Keyword.IsEmpty() && Name.Contains(Keyword))
			{
				return true;
			}
		}
		return false;
	}
}

ALootScatterSpawner::ALootScatterSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	RootComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
}

void ALootScatterSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_LootRules, no loot this game"), *GetName());
		return;
	}
	GetWorldTimerManager().SetTimer(ScatterTimer, this, &ALootScatterSpawner::Scatter, FMath::Max(Rules->SpawnDelay, 0.1f), false);
}

void ALootScatterSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(ScatterTimer);
	GetWorldTimerManager().ClearTimer(BatchTimer);
	Super::EndPlay(EndPlayReason);
}

void ALootScatterSpawner::Scatter()
{
	TArray<float> Weights;
	for (const FLootEntry& Entry : Rules->Items)
	{
		Weights.Add(Entry.ItemClass ? Entry.Weight : 0.0f);
	}
	if (Rules->Spots.IsEmpty() || Rules->Count <= 0 || LootMath::PickWeighted(Weights, 0.0f) == INDEX_NONE)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: nothing to scatter (%d kinds, %d spots, count %d)"), *GetName(),
			Rules->Items.Num(), Rules->Spots.Num(), Rules->Count);
		return;
	}

	const double StartTime = FPlatformTime::Seconds();
	// around the players where they are (in the boat at the arrival of their fragment), else the PlayerStarts
	TArray<FVector> Starts;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Starts.Add(Pawn->GetActorLocation());
		}
	}
	if (Starts.IsEmpty())
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			Starts.Add(It->GetActorLocation());
		}
	}
	const TArray<FSphere> Quiet = FindLootQuietZones(GetWorld());
	FRandomStream Stream(Rules->Seed != 0 ? Rules->Seed : FMath::Rand());

	// shuffled spots, so the items spread over the whole allowed area
	TArray<int32> Order;
	Order.Reserve(Rules->Spots.Num());
	for (int32 Index = 0; Index < Rules->Spots.Num(); ++Index)
	{
		Order.Add(Index);
	}
	for (int32 Index = Order.Num() - 1; Index > 0; --Index)
	{
		Order.Swap(Index, Stream.RandRange(0, Index));
	}

	// thousands of items: the spacing is checked in a grid of MinSpacing cells (the 9 cells around), not against all
	const double Cell = FMath::Max(static_cast<double>(Rules->MinSpacing), 100.0);
	const double SpacingSq = FMath::Square(static_cast<double>(Rules->MinSpacing));
	TMap<FIntPoint, TArray<FVector, TInlineAllocator<2>>> Grid;
	auto CellOf = [Cell](const FVector& Point) { return FIntPoint(FMath::FloorToInt32(Point.X / Cell), FMath::FloorToInt32(Point.Y / Cell)); };
	auto IsFree = [&Grid, &CellOf, SpacingSq](const FVector& Point)
	{
		const FIntPoint Centre = CellOf(Point);
		for (int32 DX = -1; DX <= 1; ++DX)
		{
			for (int32 DY = -1; DY <= 1; ++DY)
			{
				if (const TArray<FVector, TInlineAllocator<2>>* Near = Grid.Find(Centre + FIntPoint(DX, DY)))
				{
					for (const FVector& Other : *Near)
					{
						if (FVector::DistSquared2D(Point, Other) < SpacingSq)
						{
							return false;
						}
					}
				}
			}
		}
		return true;
	};

	TArray<FVector> Taken;
	int32 Rejected = 0;
	for (const int32 Index : Order)
	{
		if (Taken.Num() >= Rules->Count)
		{
			break;
		}
		const FVector& Spot = Rules->Spots[Index];
		if (!LootMath::IsNearAStart(Spot, Starts, Rules->MinDistanceFromStart, Rules->MaxDistanceFromStart) || !IsFree(Spot))
		{
			continue;
		}
		FVector Ground;
		if (IsInLootQuietZone(Spot, Quiet) || !ProjectToGround(Spot, Ground))
		{
			++Rejected;
			continue;
		}
		const int32 Kind = LootMath::PickWeighted(Weights, Stream.FRand());
		const FRotator Yaw(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f);
		Pending.Emplace(Rules->Items[Kind].ItemClass, FTransform(Yaw, Ground + FVector(0.0f, 0.0f, Rules->DropHeight)));
		Taken.Add(Ground);
		Grid.FindOrAdd(CellOf(Ground)).Add(Ground);
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d common loot items to scatter (%d kinds, %d spots, %d rejected on the level, %.0f ms)"),
		*GetName(), Pending.Num(), Rules->Items.Num(), Rules->Spots.Num(), Rejected, (FPlatformTime::Seconds() - StartTime) * 1000.0);
	GetWorldTimerManager().SetTimer(BatchTimer, this, &ALootScatterSpawner::SpawnBatch, 0.05f, true, 0.0f);
}

void ALootScatterSpawner::SpawnBatch()
{
	FActorSpawnParameters Params;
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	const int32 Batch = FMath::Min(FMath::Max(Rules->SpawnBatchSize, 1), Pending.Num());
	for (int32 Index = 0; Index < Batch; ++Index)
	{
		const TPair<TSubclassOf<AActor>, FTransform> Item = Pending.Pop(EAllowShrinking::No);
		if (AActor* Spawned = GetWorld()->SpawnActor<AActor>(Item.Key, Item.Value, Params))
		{
			Scattered.Add(Spawned);
		}
	}
	if (Pending.IsEmpty())
	{
		GetWorldTimerManager().ClearTimer(BatchTimer);
		UE_LOG(Logmonke_island, Log, TEXT("%s: %d common loot items scattered"), *GetName(), Scattered.Num());
	}
}

bool ALootScatterSpawner::ProjectToGround(const FVector& Spot, FVector& OutGround) const
{
	const FVector Start(Spot.X, Spot.Y, Spot.Z + 3000.0);
	const FVector End(Spot.X, Spot.Y, Spot.Z - 3000.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(LootGround), false, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);

	// vegetation (instanced) is passed through: the item lies on the ground, possibly under a bush; never under a
	// village building, on a steep surface or on the wet beach
	for (int32 Try = 0; Try < 8; ++Try)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
		{
			return false;
		}
		if (const UInstancedStaticMeshComponent* Instances = Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
		{
			const UStaticMesh* Mesh = Instances->GetStaticMesh();
			if (Mesh && HasKeyword(Mesh->GetName(), Rules->BuildingMeshKeywords))
			{
				return false;
			}
			Params.AddIgnoredComponent(Hit.GetComponent());
			continue;
		}
		if (Hit.ImpactNormal.Z < Rules->MinGroundNormalZ || Hit.ImpactPoint.Z < Rules->MinGroundHeight)
		{
			return false;
		}
		const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
		if (Water && Hit.ImpactPoint.Z < Water->GetWaterSurfaceAt(Hit.ImpactPoint) + 10.0f)
		{
			return false;
		}
		OutGround = Hit.ImpactPoint;
		return true;
	}
	return false;
}
