// monke_island

#include "Boats/BoatSubsystem.h"
#include "Boats/BoatActor.h"
#include "Boats/BoatRiderComponent.h"
#include "Boats/BoatStartSubsystem.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "TimerManager.h"
#include "monke_island.h"

const TCHAR* UBoatSubsystem::DefaultBoatClassPath = TEXT("/Game/MonkeIsland/Boat/Blueprints/BP_Barque.BP_Barque_C");

UBoatSubsystem* UBoatSubsystem::Get(const UObject* WorldContextObject)
{
	const UWorld* World = WorldContextObject ? WorldContextObject->GetWorld() : nullptr;
	return World ? World->GetSubsystem<UBoatSubsystem>() : nullptr;
}

bool UBoatSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	const UWorld* World = Cast<UWorld>(Outer);
	return World && (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::PIE);
}

void UBoatSubsystem::OnWorldBeginPlay(UWorld& InWorld)
{
	Super::OnWorldBeginPlay(InWorld);

	if (InWorld.GetNetMode() != NM_Client)
	{
		InWorld.GetTimerManager().SetTimer(RiderTimer, this, &UBoatSubsystem::EnsureRiders, 0.5f, true, 0.0f);
	}
}

void UBoatSubsystem::Deinitialize()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(RiderTimer);
	}
	Super::Deinitialize();
}

void UBoatSubsystem::EnsureRiders()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		EnsureRider(It->IsValid() ? Cast<ACharacter>((*It)->GetPawn()) : nullptr);
	}

	// a boat waits on the water near the first player when the island has none (ADR-003);
	// wait for the start in the boat (T-093) to decide first, it may spawn the boat at an arrival point
	const UBoatStartSubsystem* Start = UBoatStartSubsystem::Get(this);
	const bool bStartPending = Start && !Start->HasResolved();
	if (bSpawnStarterBoat && !bStarterBoatDone && !bStartPending && GetWorld()->GetFirstPlayerController() && GetWorld()->GetFirstPlayerController()->GetPawn())
	{
		bStarterBoatDone = true;
		// only on an island: a world without sea surface (the main menu L_MainMenu) gets no boat
		bool bHasSea = false;
		for (TActorIterator<AActor> It(GetWorld()); It && !bHasSea; ++It)
		{
			bHasSea = It->ActorHasTag(TEXT("IslandWaterSurface"));
		}
		if (bHasSea && !TActorIterator<ABoatActor>(GetWorld()))
		{
			SpawnBoatNearPlayer();
		}
	}
}

UBoatRiderComponent* UBoatSubsystem::EnsureRider(ACharacter* Character)
{
	if (!Character || !Character->HasAuthority())
	{
		return nullptr;
	}
	if (UBoatRiderComponent* Existing = Character->FindComponentByClass<UBoatRiderComponent>())
	{
		return Existing;
	}
	UBoatRiderComponent* Rider = NewObject<UBoatRiderComponent>(Character, TEXT("BoatRider"));
	Rider->SetIsReplicated(true);
	Character->AddInstanceComponent(Rider);
	Rider->RegisterComponent();
	return Rider;
}

UClass* UBoatSubsystem::LoadBoatClass(const TSoftClassPtr<ABoatActor>& BoatClass)
{
	if (!BoatClass.IsNull())
	{
		if (UClass* Loaded = BoatClass.LoadSynchronous())
		{
			return Loaded;
		}
		UE_LOG(Logmonke_island, Warning, TEXT("Boat class %s not found, trying the default boat"), *BoatClass.ToString());
	}
	UClass* Default = StaticLoadClass(ABoatActor::StaticClass(), nullptr, DefaultBoatClassPath);
	if (!Default)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("%s not found, spawning the bare C++ boat"), DefaultBoatClassPath);
		Default = ABoatActor::StaticClass();
	}
	return Default;
}

ABoatActor* UBoatSubsystem::SpawnBoatNearPlayer()
{
	UWorld* World = GetWorld();
	if (!World || World->GetNetMode() == NM_Client)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("monke.SpawnBoat: server only (host)"));
		return nullptr;
	}
	const APlayerController* PC = World->GetFirstPlayerController();
	const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
	if (!Pawn)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("monke.SpawnBoat: no local player pawn"));
		return nullptr;
	}

	UClass* BoatClass = LoadBoatClass(TSoftClassPtr<ABoatActor>());
	FVector Forward = PC->GetControlRotation().Vector();
	ABoatActor* Boat = ABoatActor::SpawnOnWaterNear(World, Pawn->GetActorLocation(), Forward, BoatClass);
	if (Boat)
	{
		UE_LOG(Logmonke_island, Log, TEXT("monke.SpawnBoat: %s at %s"), *Boat->GetName(), *Boat->GetActorLocation().ToCompactString());
	}
	else
	{
		UE_LOG(Logmonke_island, Warning, TEXT("monke.SpawnBoat: no water deep enough within 40 m"));
	}
	return Boat;
}

#if !UE_BUILD_SHIPPING
static FAutoConsoleCommandWithWorld GSpawnBoatCommand(
	TEXT("monke.SpawnBoat"),
	TEXT("Spawns a rowboat on the water in front of the local player (server / host)."),
	FConsoleCommandWithWorldDelegate::CreateLambda([](UWorld* World)
	{
		if (UBoatSubsystem* Boats = UBoatSubsystem::Get(World))
		{
			Boats->SpawnBoatNearPlayer();
		}
	}));
#endif
