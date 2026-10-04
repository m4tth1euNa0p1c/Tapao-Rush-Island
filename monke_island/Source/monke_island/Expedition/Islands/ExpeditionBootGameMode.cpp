// monke_island

#include "Expedition/Islands/ExpeditionBootGameMode.h"
#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"

AExpeditionBootGameMode::AExpeditionBootGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = nullptr;
}

void AExpeditionBootGameMode::HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer)
{
	// Deliberately no RestartPlayer: nothing to play on the boot map
}

void AExpeditionBootGameMode::BeginPlay()
{
	Super::BeginPlay();

	UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
	if (!ensureMsgf(Islands, TEXT("%s: no UExpeditionIslandSubsystem on this game instance"), *GetNameSafe(this)))
	{
		Fail(EFragmentError::CatalogNotFound);
		return;
	}
	// T-141: coming back from an expedition, wait for its players (the catalog is cached, the draw is immediate)
	ExpectedPlayers = Islands->TakeTravelPlayerCount();
	UE_LOG(LogMonkeIslands, Log, TEXT("Boot: loading the island catalog (%d player(s) expected)"), ExpectedPlayers);
	Islands->LoadCatalog(FOnIslandCatalogLoaded::CreateUObject(this, &AExpeditionBootGameMode::HandleCatalogLoaded));
}

void AExpeditionBootGameMode::HandleCatalogLoaded(EFragmentError Error, const TArray<UExpeditionIslandDefinition*>& Definitions)
{
	if (bTravelRequested)
	{
		return;
	}
	if (Error != EFragmentError::None)
	{
		Fail(Error);
		return;
	}

	UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
	if (!Islands)
	{
		Fail(EFragmentError::CatalogNotFound);
		return;
	}

	// Clock seed by default: a new island at each launch; the seed is logged to replay a draw
	const int32 Seed = bUseFixedSeed ? FixedSeed : static_cast<int32>(FDateTime::UtcNow().GetTicks() & 0x7fffffff);
	const EFragmentError PickError = Islands->PickRandomIsland(Seed);
	if (PickError != EFragmentError::None)
	{
		Fail(PickError);
		return;
	}

	// Out of the load callback: the travel tears this world down
	bTravelRequested = true;
	DrawTime = GetWorld()->GetTimeSeconds();
	ScheduleTravel();
}

float AExpeditionBootGameMode::ComputeTravelDelay(bool bListenServer, float GraceSeconds, float MaxWaitSeconds, float ElapsedSeconds,
	int32 NumPlayers, int32 ExpectedPlayers)
{
	if (!bListenServer)
	{
		return 0.0f;
	}
	const float Remaining = FMath::Max(0.0f, MaxWaitSeconds - ElapsedSeconds);
	if (NumPlayers < ExpectedPlayers)
	{
		// players of the previous expedition still on their way: wait for them, never past the cap
		return Remaining;
	}
	return FMath::Min(FMath::Max(0.0f, GraceSeconds), Remaining);
}

void AExpeditionBootGameMode::ScheduleTravel()
{
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(TravelTimer);
	const int32 NumPlayers = GetNumPlayers();
	const float Delay = ComputeTravelDelay(GetNetMode() == NM_ListenServer, ListenJoinGraceSeconds, MaxJoinWaitSeconds,
		static_cast<float>(GetWorld()->GetTimeSeconds() - DrawTime), NumPlayers, ExpectedPlayers);
	if (GetNetMode() == NM_ListenServer)
	{
		UE_LOG(LogMonkeIslands, Log, TEXT("Boot: listen server, travel in %.1f s (%d of %d expected player(s) logged in)"), Delay, NumPlayers, ExpectedPlayers);
	}
	if (Delay <= 0.0f)
	{
		Timers.SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &AExpeditionBootGameMode::TravelToIsland));
		return;
	}
	Timers.SetTimer(TravelTimer, FTimerDelegate::CreateUObject(this, &AExpeditionBootGameMode::TravelToIsland), Delay, false);
}

void AExpeditionBootGameMode::GenericPlayerInitialization(AController* NewPlayer)
{
	Super::GenericPlayerInitialization(NewPlayer);
	// A player joined (login or seamless travel) while the host waits: restart the grace so it travels too
	if (bTravelRequested && TravelTimer.IsValid())
	{
		ScheduleTravel();
	}
}

void AExpeditionBootGameMode::TravelToIsland()
{
	if (GetNumPlayers() < ExpectedPlayers)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Boot: %d of %d expected player(s) after %.0f s, travelling without the others"),
			GetNumPlayers(), ExpectedPlayers, MaxJoinWaitSeconds);
	}
	UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
	const EFragmentError Error = Islands ? Islands->TravelToSelectedIsland() : EFragmentError::CatalogNotFound;
	if (Error != EFragmentError::None)
	{
		bTravelRequested = false;
		Fail(Error);
	}
}

void AExpeditionBootGameMode::Fail(EFragmentError Error)
{
	BootError = Error;
	UE_LOG(LogMonkeIslands, Error, TEXT("Boot: no island to travel to (%s): %s"),
		ExpeditionIslands::LexErrorName(Error), *ExpeditionIslands::GetErrorText(Error).ToString());

#if !UE_BUILD_SHIPPING
	if (GEngine)
	{
		GEngine->AddOnScreenDebugMessage(INDEX_NONE, 15.0f, FColor::Red, ExpeditionIslands::GetErrorText(Error).ToString());
	}
#endif

	// Never stuck on the empty boot map: open the fallback map once
	const FString FallbackPackage = FallbackMap.ToSoftObjectPath().GetLongPackageName();
	UExpeditionIslandSubsystem* Islands = UExpeditionIslandSubsystem::Get(this);
	if (FallbackPackage.IsEmpty() || !Islands || bTravelRequested)
	{
		return;
	}
	bTravelRequested = true;
	UE_LOG(LogMonkeIslands, Warning, TEXT("Boot: opening the fallback map %s"), *FallbackPackage);
	if (Islands->TravelToMapURL(FallbackPackage) != EFragmentError::None)
	{
		UE_LOG(LogMonkeIslands, Error, TEXT("Boot: the fallback map %s could not be opened either"), *FallbackPackage);
	}
}
