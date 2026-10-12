// monke_island

#include "Camps/CampSpawner.h"
#include "Camps/CampRules.h"
#include "Camps/CampSite.h"
#include "Marae/MaraeRules.h"
#include "Marae/MaraeSite.h"
#include "Marae/SitePlacementMath.h"
#include "Run/RunCampaign.h"
#include "Run/RunSubsystem.h"
#include "Water/IslandWaterSubsystem.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Items/LootMath.h"
#include "monke_island.h"
#include "Components/BillboardComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
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
	TempleClass = AMaraeSite::StaticClass();
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
	// the day of the campaign decides what a fragment holds (no run: everything, as authored)
	const URunSubsystem* Run = URunSubsystem::Get(this);
	const int32 Day = Fragment && Run ? Run->GetCampaignDay() : 0;
	const bool bCampsOpen = Day <= 0 || Run->IsUnlocked(ERunUnlock::AllBuildings);
	const int32 Count = !CampClass || !bCampsOpen ? 0 : Fragment ? Rules->CountPerFragment : Rules->Count;
	const float MinSpacing = Fragment ? Rules->FragmentMinSpacing : Rules->MinSpacing;
	if (Spots.IsEmpty() || (Count <= 0 && !Fragment))
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
	TSet<int32> UsedByMarae;
	if (Fragment && MaraeRules && MaraeClass)
	{
		// the guaranteed ones first: the spots nearest to the players within the near band, seen from the arrival
		TArray<int32> NearFirst;
		if (!Players.IsEmpty())
		{
			auto DistSqToPlayers = [&Players, &Spots](int32 Index)
			{
				double Best = TNumericLimits<double>::Max();
				for (const FVector& Player : Players)
				{
					Best = FMath::Min(Best, FVector::DistSquared2D(Spots[Index], Player));
				}
				return Best;
			};
			for (int32 Pos = 0; Pos < Order.Num(); ++Pos)
			{
				const double DistSq = DistSqToPlayers(Order[Pos]);
				if (DistSq >= FMath::Square(static_cast<double>(MaraeRules->NearMinDistance)) && DistSq <= FMath::Square(static_cast<double>(MaraeRules->NearMaxDistance)))
				{
					NearFirst.Add(Order[Pos]);
				}
			}
			NearFirst.Sort([&DistSqToPlayers](int32 A, int32 B) { return DistSqToPlayers(A) < DistSqToPlayers(B); });
		}
		int32 NearPlaced = 0;
		for (int32 Pass = 0; Pass < 2; ++Pass)
		{
		const bool bNearPass = Pass == 0;
		const int32 Candidates = bNearPass ? NearFirst.Num() : Order.Num();
		for (int32 Step = 0; Step < Candidates && Marae.Num() < MaraeRules->CountPerFragment; ++Step)
		{
			if (bNearPass && NearPlaced >= MaraeRules->GuaranteedNearCount)
			{
				break;
			}
			const int32 Pos = bNearPass ? Order.IndexOfByKey(NearFirst[Step]) : Order.Num() - 1 - Step;
			if (!Order.IsValidIndex(Pos))
			{
				continue;
			}
			const FVector& Spot = Spots[Order[Pos]];
			const float MinFromPlayers = bNearPass ? MaraeRules->NearMinDistance : MaraeRules->MinDistanceFromPlayers;
			if ((!bNearPass && Order[Pos] == Nearest) || IsQuiet(Spot) || !LootMath::IsFarFromAll(Spot, Players, MinFromPlayers)
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
			// the spot is used: marked, and dropped from the camp draw below
			UsedByMarae.Add(Order[Pos]);
			NearPlaced += bNearPass ? 1 : 0;
		}
		}
		Order.RemoveAll([&UsedByMarae](int32 Index) { return UsedByMarae.Contains(Index); });
	}

	// the reliquary temple of the fragment, in the heart of its land, away from the marae; the camps keep off it
	if (Fragment && TempleRules && TempleClass)
	{
		const int32 TempleCount = Day > 0 ? RunCampaign::TemplesPerFragment(Run->GetCampaignUnlocks(), Day) : TempleRules->CountPerFragment;
		TArray<TArray<TSubclassOf<AActor>>> Hands;
		if (Day > 0)
		{
			// the artifacts the temple has a pedestal for, dealt without repeat
			TArray<TSubclassOf<AActor>> Pool;
			for (const TSubclassOf<AActor>& Artifact : TempleRules->ArtifactPool)
			{
				if (Artifact && TempleRules->Actors.ContainsByPredicate([&Artifact](const FMaraeActorSpot& Spot) { return Spot.ActorClass == Artifact; }))
				{
					Pool.AddUnique(Artifact);
				}
			}
			for (const TArray<int32>& Hand : RunCampaign::DealArtifacts(Pool.Num(), TempleCount, Run->GetCampaignUnlocks().ArtifactsPerTemple, Stream))
			{
				TArray<TSubclassOf<AActor>>& Classes = Hands.AddDefaulted_GetRef();
				for (const int32 Index : Hand)
				{
					Classes.Add(Pool[Index]);
				}
			}
		}
		SpawnTemples(Spots, Order, Players, Taken, Quiet, Stream, TempleCount, Hands);
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
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d fishermen camps, %d marae and %d reliquary temples of %d spots (fragment %s, campaign day %d%s)"), *GetName(),
		Camps.Num(), Marae.Num(), Temples.Num(), Spots.Num(), *FragmentId.ToString(), Day, Day > 0 && !bCampsOpen ? TEXT(", camps not open yet") : TEXT(""));
}

void ACampSpawner::SpawnTemples(const TArray<FVector>& Spots, TArray<int32>& Order, const TArray<FVector>& Players, TArray<FVector>& Taken,
	TArray<FSphere>& Quiet, FRandomStream& Stream, int32 Count, const TArray<TArray<TSubclassOf<AActor>>>& Hands)
{
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(this);
	const double SeaZ = Water ? Water->GetSeaLevel() : 0.0;
	const FVector Middle = SitePlacement::Centroid(Spots);
	const TArray<int32> Central = TempleRules->bCentral ? SitePlacement::ByDistanceTo(Spots, Middle) : Order;
	const FVector PlayersMiddle = SitePlacement::Centroid(Players);
	auto IsQuiet = [&Quiet](const FVector& Spot)
	{
		return Quiet.ContainsByPredicate([&Spot](const FSphere& Zone) { return FVector::DistSquared2D(Spot, Zone.Center) <= FMath::Square(Zone.W); });
	};
	// first pass: every rule; second pass, only when the first placed nothing: the most central spot still free of the
	// marae, the ground no longer looked at (a temple in every fragment, cahier of the owner)
	for (int32 Pass = 0; Pass < 2 && Temples.Num() < Count; ++Pass)
	{
		const bool bStrict = Pass == 0;
		if (!bStrict && !Temples.IsEmpty())
		{
			break;
		}
		for (int32 Rank = 0; Rank < Central.Num() && Temples.Num() < Count; ++Rank)
		{
			const int32 Index = Central[Rank];
			const FVector& Spot = Spots[Index];
			if (!Order.Contains(Index) || !LootMath::IsFarFromAll(Spot, Taken, bStrict ? TempleRules->MinDistanceFromSites : TempleRules->MinDistanceFromSites * 0.5f))
			{
				continue;
			}
			if (bStrict && (IsQuiet(Spot) || !LootMath::IsFarFromAll(Spot, Players, TempleRules->MinDistanceFromPlayers)))
			{
				continue;
			}
			TArray<double> Ground;
			if (bStrict && TempleRules->GroundCheckRadius > 0.0f
				&& (!SampleGround(Spot, TempleRules->GroundCheckRadius, Ground)
					|| !SitePlacement::IsBuildableGround(Ground, 9, SeaZ, TempleRules->MinGroundAboveSea, TempleRules->MaxGroundDelta)))
			{
				continue;
			}
			const float Yaw = TempleRules->bFacePlayers && !Players.IsEmpty() ? SitePlacement::YawFacing(Spot, PlayersMiddle) : Stream.FRandRange(-180.0f, 180.0f);
			const FTransform Transform(FRotator(0.0f, Yaw, 0.0f), Spot);
			AMaraeSite* Site = GetWorld()->SpawnActorDeferred<AMaraeSite>(TempleClass, Transform, this, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
			if (!Site)
			{
				continue;
			}
			Site->Rules = TempleRules;
			FString Dealt;
			if (Hands.IsValidIndex(Temples.Num()))
			{
				Site->DealArtifacts(Hands[Temples.Num()]);
				for (const TSubclassOf<AActor>& Artifact : Hands[Temples.Num()])
				{
					Dealt += FString::Printf(TEXT(" %s"), *GetNameSafe(Artifact));
				}
			}
			Site->FinishSpawning(Transform);
			Temples.Add(Site);
			if (Hands.IsValidIndex(Temples.Num() - 1))
			{
				UE_LOG(Logmonke_island, Log, TEXT("%s: temple %d holds%s"), *GetName(), Temples.Num(), Dealt.IsEmpty() ? TEXT(" no artifact") : *Dealt);
			}
			Taken.Add(Spot);
			Quiet.Emplace(Spot, TempleRules->QuietRadius + Rules->QuietRadius);
			Order.Remove(Index);
			UE_LOG(Logmonke_island, Log, TEXT("%s: reliquary temple at %s, spot %d of %d by distance to the middle of the land (%.0f m away)%s"),
				*GetName(), *Spot.ToCompactString(), Rank + 1, Central.Num(), FVector::Dist2D(Spot, Middle) / 100.0,
				bStrict ? TEXT("") : TEXT(", ground not checked: no spot passed it"));
		}
	}
}

bool ACampSpawner::SampleGround(const FVector& Spot, float Radius, TArray<double>& OutZ) const
{
	OutZ.Reset();
	FCollisionQueryParams Params(SCENE_QUERY_STAT(TempleGround), false, this);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	for (int32 Sample = 0; Sample < 9; ++Sample)
	{
		const double Angle = 2.0 * UE_DOUBLE_PI * Sample / 8.0;
		const FVector Point = Sample == 0 ? Spot : Spot + FVector(FMath::Cos(Angle), FMath::Sin(Angle), 0.0) * Radius;
		const FVector Start(Point.X, Point.Y, Spot.Z + 3000.0);
		const FVector End(Point.X, Point.Y, Spot.Z - 3000.0);
		FCollisionQueryParams Query = Params;
		bool bFound = false;
		// the vegetation and the village props are instanced: looked through, the ground is under them
		for (int32 Try = 0; Try < 8 && !bFound; ++Try)
		{
			FHitResult Hit;
			if (!GetWorld()->LineTraceSingleByObjectType(Hit, Start, End, Objects, Query))
			{
				break;
			}
			if (Cast<UInstancedStaticMeshComponent>(Hit.GetComponent()))
			{
				Query.AddIgnoredComponent(Hit.GetComponent());
				continue;
			}
			OutZ.Add(Hit.ImpactPoint.Z);
			bFound = true;
		}
		if (!bFound)
		{
			return false;
		}
	}
	return true;
}
