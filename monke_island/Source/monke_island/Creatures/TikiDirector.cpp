// monke_island

#include "Creatures/TikiDirector.h"
#include "Creatures/TikiMath.h"
#include "Creatures/TikiRules.h"
#include "Creatures/TikiVillager.h"
#include "Creatures/TikiWarrior.h"
#include "DayNight/DayNightCycleActor.h"
#include "Items/ConsumablePickupActor.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/PlatformTime.h"
#include "TimerManager.h"

namespace
{
	/** Side of a cell of the bush grid (cm) */
	constexpr double BushCellSize = 2000.0;
}

ATikiDirector::ATikiDirector()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;

	Sprite = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	RootComponent = Sprite;
}

ATikiDirector* ATikiDirector::Get(const UObject* WorldContextObject)
{
	UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	if (!World)
	{
		return nullptr;
	}
	for (TActorIterator<ATikiDirector> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}

void ATikiDirector::BeginPlay()
{
	Super::BeginPlay();

	if (!HasAuthority())
	{
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_TikiRules, the tiki stay away"), *GetName());
		return;
	}

	// coconuts already placed in the village area are gathered too
	for (TActorIterator<AConsumablePickupActor> It(GetWorld()); It; ++It)
	{
		if (FVector::Dist2D(It->GetActorLocation(), GetActorLocation()) <= VillageRadius)
		{
			Coconuts.Add(*It);
		}
	}

	GetWorld()->GetTimerManager().SetTimer(ThinkTimer, this, &ATikiDirector::Think, Rules->ThinkInterval, true, 0.5f);

	// the bush grid is built once, early in the day, not on the first night spawn
	FTimerHandle CacheTimer;
	GetWorld()->GetTimerManager().SetTimer(CacheTimer, this, &ATikiDirector::CacheBushes, 2.0f, false);
}

void ATikiDirector::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(ThinkTimer);
	Super::EndPlay(EndPlayReason);
}

bool ATikiDirector::IsNight() const
{
	const ADayNightCycleActor* Cycle = ADayNightCycleActor::Get(this);
	return Cycle && Cycle->GetPhase() == EDayPhase::Night;
}

void ATikiDirector::Think()
{
	const double Now = GetWorld()->GetTimeSeconds();
	TickVillage(Now);
	TickNight(Now);

	Defenders.RemoveAll([](const TWeakObjectPtr<ATikiWarrior>& W) { return !W.IsValid(); });
}

void ATikiDirector::TickVillage(double Now)
{
	Villagers.RemoveAll([](const TWeakObjectPtr<ATikiVillager>& V) { return !V.IsValid() || V->IsDead(); });
	Coconuts.RemoveAll([](const TWeakObjectPtr<AActor>& C) { return !C.IsValid() || C->IsActorBeingDestroyed(); });
	for (auto It = Claims.CreateIterator(); It; ++It)
	{
		if (!It.Key().IsValid() || !It.Value().IsValid())
		{
			It.RemoveCurrent();
		}
	}

	// one villager per think, so they do not all appear on the same frame
	if (VillagerClass && Villagers.Num() < Rules->VillagerCount)
	{
		FVector Point;
		if (FindGroundPointNear(GetActorLocation(), 0.0f, VillageRadius * 0.6f, Point))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			const FTransform Transform(FRotator(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f), Point + FVector(0.0f, 0.0f, 60.0f));
			if (ATikiVillager* Villager = GetWorld()->SpawnActorDeferred<ATikiVillager>(VillagerClass, Transform, this, nullptr, Params.SpawnCollisionHandlingOverride))
			{
				Villager->Rules = Rules;
				Villager->SetDirector(this);
				Villager->SetHome(GetActorLocation(), VillageRadius);
				Villager->FinishSpawning(Transform);
				Villagers.Add(Villager);
				UE_LOG(Logmonke_island, Log, TEXT("%s: villager %s at %s (%d/%d)"), *GetName(), *Villager->GetName(), *Point.ToCompactString(), Villagers.Num(), Rules->VillagerCount);
			}
		}
	}

	if (CoconutClass && Coconuts.Num() < Rules->CoconutCount && Now - LastCoconutDropTime >= Rules->CoconutDropInterval)
	{
		FVector Point;
		if (FindGroundPointNear(GetActorLocation(), 0.0f, VillageRadius, Point))
		{
			FActorSpawnParameters Params;
			Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
			if (AActor* Coconut = GetWorld()->SpawnActor<AActor>(CoconutClass, Point + FVector(0.0f, 0.0f, 12.0f), FRotator(0.0f, FMath::FRandRange(-180.0f, 180.0f), 0.0f), Params))
			{
				Coconuts.Add(Coconut);
				LastCoconutDropTime = Now;
			}
		}
	}
}

void ATikiDirector::TickNight(double Now)
{
	NightWarriors.RemoveAll([](const TWeakObjectPtr<ATikiWarrior>& W) { return !W.IsValid() || W->IsDead(); });

	if (!IsNight())
	{
		if (!NightWarriors.IsEmpty())
		{
			UE_LOG(Logmonke_island, Log, TEXT("%s: dawn, %d night warriors go back into the ground"), *GetName(), NightWarriors.Num());
		}
		for (const TWeakObjectPtr<ATikiWarrior>& Warrior : NightWarriors)
		{
			Warrior->Burrow();
		}
		NightWarriors.Reset();
		return;
	}

	if (NightWarriors.Num() >= Rules->NightWarriorCount || Now - LastNightSpawnTime < Rules->NightSpawnInterval)
	{
		return;
	}

	// a random player, a point on the ring around it, the bush nearest to that point
	TArray<APawn*> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (APawn* Pawn = It->IsValid() ? (*It)->GetPawn() : nullptr)
		{
			Players.Add(Pawn);
		}
	}
	if (Players.IsEmpty())
	{
		return;
	}
	APawn* Player = Players[FMath::RandRange(0, Players.Num() - 1)];
	const FVector Near = TikiMath::PointOnRing(Player->GetActorLocation(), FMath::FRandRange(0.0f, 2.0f * PI), FMath::FRand(), Rules->NightSpawnMinDistance, Rules->NightSpawnMaxDistance);
	// a failed try (sea, no ground) also waits for the next interval
	LastNightSpawnTime = Now;
	if (ATikiWarrior* Warrior = SpawnWarriorFromBush(Near, Rules->NightSpawnMaxDistance - Rules->NightSpawnMinDistance, nullptr, ETikiWarriorRole::Night))
	{
		NightWarriors.Add(Warrior);
		UE_LOG(Logmonke_island, Log, TEXT("%s: night, %s comes out at %s (%d/%d)"), *GetName(), *Warrior->GetName(), *Warrior->GetActorLocation().ToCompactString(), NightWarriors.Num(), Rules->NightWarriorCount);
	}
}

AActor* ATikiDirector::ClaimCoconutNear(ATikiVillager* Villager, const FVector& Location, float Radius)
{
	AActor* Best = nullptr;
	double BestDistSq = FMath::Square(static_cast<double>(Radius));
	for (const TWeakObjectPtr<AActor>& Coconut : Coconuts)
	{
		if (!Coconut.IsValid() || Claims.Contains(Coconut))
		{
			continue;
		}
		if (const AConsumablePickupActor* Pickup = Cast<AConsumablePickupActor>(Coconut.Get()); Pickup && Pickup->IsConsumed())
		{
			continue;
		}
		const double DistSq = FVector::DistSquared(Coconut->GetActorLocation(), Location);
		if (DistSq <= BestDistSq)
		{
			BestDistSq = DistSq;
			Best = Coconut.Get();
		}
	}
	if (Best)
	{
		Claims.Add(Best, Villager);
	}
	return Best;
}

void ATikiDirector::ReleaseCoconut(AActor* Coconut)
{
	Claims.Remove(Coconut);
}

bool ATikiDirector::CollectCoconut(ATikiVillager* Villager, AActor* Coconut)
{
	Claims.Remove(Coconut);
	if (!IsValid(Coconut) || Coconut->IsActorBeingDestroyed())
	{
		return false;
	}
	if (const AConsumablePickupActor* Pickup = Cast<AConsumablePickupActor>(Coconut); Pickup && Pickup->IsConsumed())
	{
		return false;
	}
	Coconuts.Remove(Coconut);
	Coconut->Destroy();
	UE_LOG(Logmonke_island, Verbose, TEXT("%s gathers a coconut"), *GetNameSafe(Villager));
	return true;
}

ATikiWarrior* ATikiDirector::CallDefender(ATikiVillager* Villager, APawn* Attacker)
{
	if (!Rules || !Villager || !Attacker)
	{
		return nullptr;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (const double* Last = LastDefenderTime.Find(Attacker); Last && Now - *Last < Rules->DefenderCooldown)
	{
		return nullptr;
	}
	int32 Alive = 0;
	for (const TWeakObjectPtr<ATikiWarrior>& Defender : Defenders)
	{
		if (Defender.IsValid() && !Defender->IsDead() && Defender->GetTarget() == Attacker)
		{
			++Alive;
		}
	}
	if (Alive >= Rules->MaxDefendersPerPlayer)
	{
		return nullptr;
	}

	ATikiWarrior* Warrior = SpawnWarriorFromBush(Villager->GetActorLocation(), Rules->BushSearchRadius, Attacker, ETikiWarriorRole::Defender);
	if (Warrior)
	{
		Defenders.Add(Warrior);
		LastDefenderTime.Add(Attacker, Now);
		UE_LOG(Logmonke_island, Log, TEXT("%s was hit by %s: %s leaps out of a bush"), *Villager->GetName(), *Attacker->GetName(), *Warrior->GetName());
	}
	return Warrior;
}

ATikiWarrior* ATikiDirector::SpawnWarriorFromBush(const FVector& Near, float BushRadius, APawn* Target, ETikiWarriorRole WarriorRole)
{
	const TSubclassOf<ATikiWarrior> WarriorClass = PickWarriorClass();
	if (!WarriorClass || !Rules)
	{
		return nullptr;
	}

	// out of the nearest bush, on the side facing the target; out of the ground when no bush is near
	FVector Bush;
	FVector Ground;
	const FVector Facing = Target ? Target->GetActorLocation() : Near;
	if (FindNearestBush(Near, BushRadius, Bush))
	{
		const FVector ToTarget = (Facing - Bush).GetSafeNormal2D();
		if (!ProjectToGround(Bush + ToTarget * 60.0f, Ground))
		{
			return nullptr;
		}
	}
	else if (!FindGroundPointNear(Near, 100.0f, 400.0f, Ground))
	{
		return nullptr;
	}

	const FVector LeapDirection = (Facing - Ground).GetSafeNormal2D();
	const FTransform Transform(LeapDirection.IsNearlyZero() ? FRotator::ZeroRotator : LeapDirection.Rotation(), Ground + FVector(0.0f, 0.0f, 80.0f));
	ATikiWarrior* Warrior = GetWorld()->SpawnActorDeferred<ATikiWarrior>(WarriorClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn);
	if (!Warrior)
	{
		return nullptr;
	}
	Warrior->Rules = Rules;
	Warrior->SetDirector(this);
	Warrior->InitWarrior(WarriorRole, Target, LeapDirection.IsNearlyZero() ? Warrior->GetActorForwardVector() : LeapDirection);
	Warrior->FinishSpawning(Transform);
	return Warrior;
}

TSubclassOf<ATikiWarrior> ATikiDirector::PickWarriorClass() const
{
	TArray<TSubclassOf<ATikiWarrior>> Valid;
	for (const TSubclassOf<ATikiWarrior>& Class : WarriorClasses)
	{
		if (Class)
		{
			Valid.Add(Class);
		}
	}
	return Valid.IsEmpty() ? nullptr : Valid[FMath::RandRange(0, Valid.Num() - 1)];
}

bool ATikiDirector::ProjectToGround(const FVector& Location, FVector& OutPoint) const
{
	const FVector Start(Location.X, Location.Y, Location.Z + 5000.0);
	const FVector End(Location.X, Location.Y, Location.Z - 5000.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TikiGround), false, this);
	FCollisionObjectQueryParams Objects(ECC_WorldStatic);

	// the vegetation crowns are skipped: only the ground counts
	for (int32 Try = 0; Try < 6; ++Try)
	{
		FHitResult Hit;
		if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Params))
		{
			return false;
		}
		if (Hit.GetComponent() && Hit.GetComponent()->IsA<UInstancedStaticMeshComponent>())
		{
			Params.AddIgnoredComponent(Hit.GetComponent());
			continue;
		}
		const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
		const float WaterZ = Water ? Water->GetWaterSurfaceAt(Hit.ImpactPoint) : 0.0f;
		if (Hit.ImpactPoint.Z < WaterZ + 10.0f)
		{
			return false;
		}
		OutPoint = Hit.ImpactPoint;
		return true;
	}
	return false;
}

bool ATikiDirector::FindGroundPointNear(const FVector& Center, float MinRadius, float MaxRadius, FVector& OutPoint) const
{
	for (int32 Try = 0; Try < 8; ++Try)
	{
		const FVector Candidate = TikiMath::PointOnRing(Center, FMath::FRandRange(0.0f, 2.0f * PI), FMath::Sqrt(FMath::FRand()), MinRadius, MaxRadius);
		if (ProjectToGround(Candidate, OutPoint))
		{
			return true;
		}
	}
	return false;
}

FIntPoint ATikiDirector::BushCellOf(const FVector& Location)
{
	return FIntPoint(FMath::FloorToInt32(Location.X / BushCellSize), FMath::FloorToInt32(Location.Y / BushCellSize));
}

void ATikiDirector::CacheBushes()
{
	if (bBushesCached)
	{
		return;
	}
	bBushesCached = true;
	BushCells.Reset();
	if (!Rules)
	{
		return;
	}
	const double StartTime = FPlatformTime::Seconds();
	int32 Meshes = 0;
	int32 Instances = 0;
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		TInlineComponentArray<UInstancedStaticMeshComponent*> Components(*It);
		for (UInstancedStaticMeshComponent* Component : Components)
		{
			const UStaticMesh* Mesh = Component->GetStaticMesh();
			const int32 Count = Component->GetInstanceCount();
			if (!Mesh || Count <= 0 || !TikiMath::IsBushMeshName(Mesh->GetName(), Rules->BushMeshKeywords))
			{
				continue;
			}
			++Meshes;
			for (int32 Index = 0; Index < Count; ++Index)
			{
				FTransform Transform;
				if (Component->GetInstanceTransform(Index, Transform, true))
				{
					BushCells.FindOrAdd(BushCellOf(Transform.GetLocation())).Add(Transform.GetLocation());
					++Instances;
				}
			}
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d bush meshes, %d bushes in %d cells (%.0f ms)"),
		*GetName(), Meshes, Instances, BushCells.Num(), (FPlatformTime::Seconds() - StartTime) * 1000.0);
}

bool ATikiDirector::FindNearestBush(const FVector& Location, float Radius, FVector& OutBush)
{
	CacheBushes();
	const FIntPoint Center = BushCellOf(Location);
	const int32 Reach = FMath::CeilToInt32(Radius / BushCellSize);
	TArray<FVector> Candidates;
	for (int32 X = Center.X - Reach; X <= Center.X + Reach; ++X)
	{
		for (int32 Y = Center.Y - Reach; Y <= Center.Y + Reach; ++Y)
		{
			if (const TArray<FVector>* Cell = BushCells.Find(FIntPoint(X, Y)))
			{
				Candidates.Append(*Cell);
			}
		}
	}
	const int32 Best = TikiMath::NearestIndex(Candidates, Location, Radius);
	if (Best == INDEX_NONE)
	{
		return false;
	}
	OutBush = Candidates[Best];
	return true;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GTikiDefenderCommand(
	TEXT("monke.TikiDefender"),
	TEXT("A tiki warrior leaps out of the bush nearest to the local player and attacks it (server / host)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		ATikiDirector* Director = ATikiDirector::Get(World);
		APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
		APawn* Pawn = PC ? PC->GetPawn() : nullptr;
		if (!Director || !Pawn || World->GetNetMode() == NM_Client)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("monke.TikiDefender: needs a TikiDirector in the level and a local player on the server"));
			return;
		}
		Director->SpawnWarriorFromBush(Pawn->GetActorLocation(), 3000.0f, Pawn, ETikiWarriorRole::Defender);
	}));
#endif
