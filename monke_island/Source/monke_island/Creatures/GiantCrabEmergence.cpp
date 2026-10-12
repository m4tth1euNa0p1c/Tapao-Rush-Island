// monke_island

#include "Creatures/GiantCrabEmergence.h"
#include "Creatures/GiantCrab.h"
#include "Creatures/GiantCrabEmergenceMath.h"
#include "Creatures/GiantCrabEmergenceRules.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Fort/FortGarrisonZone.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunCampaign.h"
#include "Run/RunSubsystem.h"
#include "Water/IslandWaterSubsystem.h"
#include "monke_island.h"
#include "Components/CapsuleComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

namespace
{
	/** DA_GiantCrabEmergenceRules when DefaultGame.ini names none */
	const TCHAR* DefaultEmergenceRulesPath = TEXT("/Game/MonkeIsland/Characters/GiantCrab/DA_GiantCrabEmergenceRules.DA_GiantCrabEmergenceRules");

	FAutoConsoleCommandWithWorld EmergeCommand(TEXT("monke.CrabEmerge"), TEXT("A giant crab rises out of the sand near a player now (server)"),
		FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
		{
			if (UGiantCrabEmergenceSubsystem* Emergence = World ? World->GetSubsystem<UGiantCrabEmergenceSubsystem>() : nullptr)
			{
				Emergence->EmergeNow();
			}
		}));

	/** Players alive in the world */
	TArray<const APawn*> AlivePlayers(const UWorld* World)
	{
		TArray<const APawn*> Players;
		for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
		{
			const APawn* Pawn = It->Get() ? It->Get()->GetPawn() : nullptr;
			const UIslanderHealthComponent* Health = Pawn ? Pawn->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
			if (Health && !Health->IsDead())
			{
				Players.Add(Pawn);
			}
		}
		return Players;
	}
}

UGiantCrabEmergenceSubsystem::UGiantCrabEmergenceSubsystem()
	: EmergenceRulesPath(DefaultEmergenceRulesPath)
{
}

bool UGiantCrabEmergenceSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return Super::ShouldCreateSubsystem(Outer) && World && World->IsGameWorld();
}

void UGiantCrabEmergenceSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);
	bServer = InWorld.GetNetMode() != NM_Client;
	if (bServer)
	{
		Rules = Cast<UGiantCrabEmergenceRules>(EmergenceRulesPath.TryLoad());
	}
}

TStatId UGiantCrabEmergenceSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UGiantCrabEmergenceSubsystem, STATGROUP_Tickables);
}

void UGiantCrabEmergenceSubsystem::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	UWorld* World = GetWorld();
	if (!bServer || !Rules || !World)
	{
		return;
	}
	const double Now = World->GetTimeSeconds();
	if (Now >= NextCheck)
	{
		NextCheck = Now + Rules->CheckInterval;
		Check();
	}
	// the crabs rising out of the sand
	for (int32 Index = Rising.Num() - 1; Index >= 0; --Index)
	{
		FRising& Rise = Rising[Index];
		AGiantCrab* Crab = Rise.Crab.Get();
		if (!Crab)
		{
			Rising.RemoveAt(Index);
			continue;
		}
		const double Alpha = (Now - Rise.Start) / FMath::Max(Rules->RiseSeconds, 0.1f);
		Crab->SetActorLocation(Rise.Ground + FVector(0.0, 0.0, GiantCrabEmergenceMath::RiseHeight(Alpha, Rules->DepthCm)), false, nullptr,
			ETeleportType::TeleportPhysics);
		if (Alpha >= 1.0)
		{
			// out of the sand: its collision back on every machine (replicated, R-31)
			Crab->SetRising(false);
			Crab->GetCharacterMovement()->SetMovementMode(MOVE_Walking);
			Rising.RemoveAt(Index);
			UE_LOG(Logmonke_island, Log, TEXT("CrabEmergence: %s is out of the sand"), *Crab->GetName());
		}
	}
}

void UGiantCrabEmergenceSubsystem::Check()
{
	UWorld* World = GetWorld();
	const AExpeditionGameState* Expedition = World->GetGameState<AExpeditionGameState>();
	const URunSubsystem* Run = URunSubsystem::Get(World);
	// the campaign of a run, from the crabs' day, during an expedition under way
	if (!Expedition || !Run || Run->GetCampaignDay() <= 0 || !Run->IsUnlocked(ERunUnlock::GiantCrabs) || Expedition->IsExpeditionOver()
		|| Expedition->GetExpeditionStartTime() <= 0.0)
	{
		return;
	}
	const double Elapsed = Expedition->GetServerWorldTimeSeconds() - Expedition->GetExpeditionStartTime();
	// the hard days bring the crab out earlier (the difficulty curve, owner 2026-10-09: 4:00 on day 10)
	const TArray<float> Times = GiantCrabEmergenceMath::DayTimes(Rules->EmergeSeconds, Run->GetDayDifficulty().CrabEmergenceMinute);
	if (GiantCrabEmergenceMath::DueEmergence(Elapsed, Times, Done) == INDEX_NONE)
	{
		return;
	}
	if (EmergeNow())
	{
		++Done;
	}
}

bool UGiantCrabEmergenceSubsystem::EmergeNow()
{
	UWorld* World = GetWorld();
	if (!bServer || !Rules || !World || !Rules->CrabClass)
	{
		return false;
	}
	TArray<const APawn*> Players = AlivePlayers(World);
	// a player drawn at random, the others if no sand is around it
	while (!Players.IsEmpty())
	{
		const int32 Pick = FMath::RandRange(0, Players.Num() - 1);
		if (Emerge(Players[Pick]))
		{
			return true;
		}
		Players.RemoveAtSwap(Pick);
	}
	return false;
}

bool UGiantCrabEmergenceSubsystem::Emerge(const AActor* Player)
{
	UWorld* World = GetWorld();
	const UIslandWaterSubsystem* Water = UIslandWaterSubsystem::Get(World);
	const double SeaZ = Water ? Water->GetSeaLevel() : 0.0;
	const FVector Center = Player->GetActorLocation();
	const double StartAngle = FMath::FRandRange(0.0, 360.0);
	FCollisionQueryParams Params(SCENE_QUERY_STAT(CrabEmergence), false, Player);
	const FCollisionObjectQueryParams Objects(ECC_WorldStatic);
	for (int32 Index = 0; Index < Rules->Samples; ++Index)
	{
		const double Distance = Rules->DistanceCm + FMath::FRandRange(-Rules->DistanceJitterCm, Rules->DistanceJitterCm);
		const FVector Point = GiantCrabEmergenceMath::RingPoint(Center, Distance, Index, Rules->Samples, StartAngle);
		FHitResult Hit;
		if (!World->LineTraceSingleByObjectType(Hit, Point + FVector(0.0, 0.0, 5000.0), Point - FVector(0.0, 0.0, 5000.0), Objects, Params))
		{
			continue;
		}
		// open sand: the first thing hit is the ground, not a plant, a hut or a rock of the scatter
		if (Cast<UInstancedStaticMeshComponent>(Hit.GetComponent())
			|| !GiantCrabEmergenceMath::IsSand(Hit.ImpactPoint.Z, SeaZ, Hit.ImpactNormal.Z, Rules->SandMinAboveSea, Rules->SandMaxAboveSea,
				Rules->MinGroundNormalZ)
			|| (Water && Hit.ImpactPoint.Z < Water->GetWaterSurfaceAt(Hit.ImpactPoint) + 10.0f))
		{
			continue;
		}
		// never inside a French fort: its court is sand too, and the crab would rise among the garrison
		if (AFortGarrisonZone::FindAt(World, Hit.ImpactPoint))
		{
			continue;
		}
		const AGiantCrab* Defaults = Rules->CrabClass->GetDefaultObject<AGiantCrab>();
		const float HalfHeight = Defaults && Defaults->GetCapsuleComponent() ? Defaults->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() : 100.0f;
		const FVector Ground = Hit.ImpactPoint + FVector(0.0, 0.0, HalfHeight + 2.0);
		const FRotator Facing(0.0, (Center - Ground).Rotation().Yaw, 0.0);
		const FTransform Transform(Facing, Ground - FVector(0.0, 0.0, Rules->DepthCm));
		AGiantCrab* Crab = World->SpawnActorDeferred<AGiantCrab>(Rules->CrabClass, Transform, nullptr, nullptr,
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Crab)
		{
			return false;
		}
		if (Rules->CrabRules)
		{
			Crab->Rules = Rules->CrabRules;
		}
		Crab->FinishSpawning(Transform);
		// the difficulty of the day (owner 2026-10-09): a sturdier crab, its health set by its BeginPlay just above
		const float HealthScale = URunSubsystem::DifficultyOf(this).CrabHealthScale;
		if (UIslanderHealthComponent* CrabHealth = Crab->GetHealth(); CrabHealth && !FMath::IsNearlyEqual(HealthScale, 1.0f))
		{
			CrabHealth->ScaleMaxHealth(HealthScale);
		}
		// it rises through the sand: no collision on any machine (replicated, R-31), no walking until it is out
		Crab->SetRising(true);
		Crab->GetCharacterMovement()->DisableMovement();
		Rising.Add({ Crab, Ground, World->GetTimeSeconds() });
		UE_LOG(Logmonke_island, Log, TEXT("CrabEmergence: %s rises out of the sand %.0f m from %s (%.1f m over the sea, health x%.2f)"), *Crab->GetName(),
			FVector::Dist2D(Ground, Center) / 100.0, *Player->GetName(), (Hit.ImpactPoint.Z - SeaZ) / 100.0, HealthScale);
		return true;
	}
	UE_LOG(Logmonke_island, Verbose, TEXT("CrabEmergence: no sand around %s"), *Player->GetName());
	return false;
}
