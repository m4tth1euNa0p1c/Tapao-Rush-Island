// monke_island

#include "Camps/CampSpawner.h"
#include "Camps/CampRules.h"
#include "Camps/CampSite.h"
#include "Marae/MaraeRules.h"
#include "Marae/MaraeSite.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Items/LootMath.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "TimerManager.h"

ACampSpawner::ACampSpawner()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = false;
	RootComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("Sprite"));
	CampClass = ACampSite::StaticClass();
	MaraeClass = AMaraeSite::StaticClass();
}

void ACampSpawner::BeginPlay()
{
	Super::BeginPlay();
	if (!HasAuthority())
	{
		return;
	}
	if (!Rules)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s: no DA_CampRules, no camp this game"), *GetName());
		return;
	}
	GetWorldTimerManager().SetTimer(SpawnTimer, this, &ACampSpawner::SpawnCamps, FMath::Max(Rules->SpawnDelay, 0.1f), false);
}

void ACampSpawner::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorldTimerManager().ClearTimer(SpawnTimer);
	Super::EndPlay(EndPlayReason);
}

void ACampSpawner::SpawnCamps()
{
	// the fragment of the expedition played by this world: its own spots, near the players wherever they land
	TArray<FVector> Players;
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		if (const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr)
		{
			Players.Add(Pawn->GetActorLocation());
		}
	}
	// where the players really are (in the boat at the arrival of their fragment) decides; the fragment of the URL
	// only when no player is there yet (a direct PIE on Lvl_Raraka announces FRG_Raraka_01 wherever the boat lands)
	const FCampFragmentSpots* Fragment = nullptr;
	double BestDistSq = TNumericLimits<double>::Max();
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
		if (!Pawn)
		{
			continue;
		}
		for (const FCampFragmentSpots& Entry : Rules->FragmentSpots)
		{
			for (const FVector& Spot : Entry.Spots)
			{
				const double DistSq = FVector::DistSquared2D(Spot, Pawn->GetActorLocation());
				if (DistSq < BestDistSq)
				{
					BestDistSq = DistSq;
					Fragment = &Entry;
				}
			}
		}
	}
	if (!Fragment)
	{
		const UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
		Fragment = Rules->FindFragment(Islands ? Islands->ResolveFragmentIdForWorld(GetWorld()) : NAME_None);
	}
	const FName FragmentId = Fragment ? Fragment->FragmentId : NAME_None;
	const TArray<FVector>& Spots = Fragment ? Fragment->Spots : Rules->Spots;
	const int32 Count = Fragment ? Rules->CountPerFragment : Rules->Count;
	const float MinSpacing = Fragment ? Rules->FragmentMinSpacing : Rules->MinSpacing;
	if (Spots.IsEmpty() || Count <= 0 || !CampClass)
	{
		UE_LOG(Logmonke_island, Log, TEXT("%s: no camp (fragment %s, %d spots)"), *GetName(), *FragmentId.ToString(), Spots.Num());
		return;
	}
	TArray<FVector> Starts;
	if (!Fragment)
	{
		for (TActorIterator<APlayerStart> It(GetWorld()); It; ++It)
		{
			Starts.Add(It->GetActorLocation());
		}
	}
	// quiet zones already in the level (the marae): no camp on them
	TArray<FSphere> Quiet;
	static const FName QuietZoneTag(TEXT("TikiQuietZone"));
	for (TActorIterator<AActor> It(GetWorld()); It; ++It)
	{
		if (It->ActorHasTag(QuietZoneTag))
		{
			if (const USphereComponent* Sphere = It->FindComponentByClass<USphereComponent>())
			{
				Quiet.Emplace(It->GetActorLocation(), Sphere->GetScaledSphereRadius() + Rules->QuietRadius);
			}
		}
	}

	FRandomStream Stream(Rules->Seed != 0 ? Rules->Seed : FMath::Rand());
	TArray<int32> Order;
	for (int32 Index = 0; Index < Spots.Num(); ++Index)
	{
		Order.Add(Index);
	}
	for (int32 Index = Order.Num() - 1; Index > 0; --Index)
	{
		Order.Swap(Index, Stream.RandRange(0, Index));
	}
	// the first camp is the nearest free spot to the players (in a fragment) or to a PlayerStart: they find one soon
	const TArray<FVector>& Anchors = Fragment ? Players : Starts;
	int32 Nearest = INDEX_NONE;
	double NearestDistSq = TNumericLimits<double>::Max();
	for (int32 Index = 0; Index < Spots.Num() && !Anchors.IsEmpty(); ++Index)
	{
		const FVector& Spot = Spots[Index];
		if (!LootMath::IsNearAStart(Spot, Anchors, Rules->MinDistanceFromStart, Fragment ? 0.0f : Rules->MaxDistanceFromStart))
		{
			continue;
		}
		for (const FVector& Start : Anchors)
		{
			const double DistSq = FVector::DistSquared2D(Spot, Start);
			if (DistSq < NearestDistSq)
			{
				NearestDistSq = DistSq;
				Nearest = Index;
			}
		}
	}
	if (Nearest != INDEX_NONE)
	{
		Order.Remove(Nearest);
		Order.Insert(Nearest, 0);
	}

	auto IsQuiet = [&Quiet](const FVector& Spot)
	{
		for (const FSphere& Zone : Quiet)
		{
			if (FVector::DistSquared2D(Spot, Zone.Center) <= FMath::Square(Zone.W))
			{
				return true;
			}
		}
		return false;
	};

	// the marae of the fragment first, on a spot away from the players (found by exploring); the camps keep off it
	TArray<FVector> Taken;
	if (Fragment && MaraeRules && MaraeClass)
	{
		for (int32 Pos = Order.Num() - 1; Pos >= 0 && Marae.Num() < MaraeRules->CountPerFragment; --Pos)
		{
			const FVector& Spot = Spots[Order[Pos]];
			if (Order[Pos] == Nearest || IsQuiet(Spot) || !LootMath::IsFarFromAll(Spot, Players, MaraeRules->MinDistanceFromPlayers)
				|| !LootMath::IsFarFromAll(Spot, Taken, MinSpacing))
			{
				continue;
			}
			const FTransform Transform(FRotator(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f), Spot);
			AMaraeSite* Site = GetWorld()->SpawnActorDeferred<AMaraeSite>(MaraeClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Site)
			{
				continue;
			}
			Site->Rules = MaraeRules;
			Site->FinishSpawning(Transform);
			Marae.Add(Site);
			Taken.Add(Spot);
			Quiet.Emplace(Spot, MaraeRules->QuietRadius + Rules->QuietRadius);
			Order.RemoveAt(Pos);
		}
	}

	const int32 MaraeCount = Taken.Num();
	for (const int32 Index : Order)
	{
		if (Taken.Num() - MaraeCount >= Count)
		{
			break;
		}
		const FVector& Spot = Spots[Index];
		if (!LootMath::IsNearAStart(Spot, Starts, Rules->MinDistanceFromStart, Rules->MaxDistanceFromStart)
			|| !LootMath::IsFarFromAll(Spot, Taken, MinSpacing))
		{
			continue;
		}
		if (IsQuiet(Spot))
		{
			continue;
		}
		const FTransform Transform(FRotator(0.0f, Stream.FRandRange(-180.0f, 180.0f), 0.0f), Spot);
		ACampSite* Camp = GetWorld()->SpawnActorDeferred<ACampSite>(CampClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Camp)
		{
			continue;
		}
		Camp->Rules = Rules;
		Camp->CampSeed = Stream.RandHelper(MAX_int32);
		Camp->FinishSpawning(Transform);
		Camps.Add(Camp);
		Taken.Add(Spot);
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d fishermen camps and %d marae of %d spots (fragment %s)"), *GetName(), Camps.Num(), Marae.Num(), Spots.Num(), *FragmentId.ToString());
}
