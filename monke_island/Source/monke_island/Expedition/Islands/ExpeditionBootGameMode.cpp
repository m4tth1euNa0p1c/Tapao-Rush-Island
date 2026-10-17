// monke_island

#include "Expedition/Islands/ExpeditionBootGameMode.h"
#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Session/CrewSessionSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/NetConnection.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "Misc/DateTime.h"
#include "TimerManager.h"

namespace
{
	/** Clients connected to this server that have not joined yet: still loading the boot map, no player controller */
	int32 CountClientsStillLoading(const UWorld* World)
	{
		const UNetDriver* Driver = World ? World->GetNetDriver() : nullptr;
		int32 Loading = 0;
		if (Driver)
		{
			for (const UNetConnection* Connection : Driver->ClientConnections)
			{
				if (Connection && Connection->GetConnectionState() == USOCK_Open && !Connection->PlayerController)
				{
					++Loading;
				}
			}
		}
		return Loading;
	}
}

AExpeditionBootGameMode::AExpeditionBootGameMode()
{
	PrimaryActorTick.bCanEverTick = false;
	DefaultPawnClass = nullptr;
	// the lobby (T-315): anyone may join here within MaxPlayers, at the start and between two islands
	bOpenToNewPlayers = true;
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
	// A dedicated server must stay in the boot lobby until someone joins. Without this,
	// its first empty-world tick immediately leaves L_Boot and closes the join window.
	const ENetMode Mode = GetNetMode();
	if (Mode == NM_DedicatedServer && NumPlayers == 0)
	{
		UE_LOG(LogMonkeIslands, Log, TEXT("Boot: dedicated server waiting for first crew member"));
		return;
	}
	const bool bNetworkHost = Mode == NM_ListenServer || Mode == NM_DedicatedServer;
	const float Delay = ComputeTravelDelay(bNetworkHost, ListenJoinGraceSeconds, MaxJoinWaitSeconds,
		static_cast<float>(GetWorld()->GetTimeSeconds() - DrawTime), NumPlayers, ExpectedPlayers);
	if (bNetworkHost)
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
	if (bTravelRequested)
	{
		// The first player on a dedicated host starts the full lobby grace period:
		// the clock must not start while the server is sitting alone in L_Boot.
		if (GetNetMode() == NM_DedicatedServer && !TravelTimer.IsValid())
		{
			DrawTime = GetWorld()->GetTimeSeconds();
		}
		if (TravelTimer.IsValid() || GetNetMode() == NM_DedicatedServer)
		{
			ScheduleTravel();
		}
	}
}

void AExpeditionBootGameMode::TravelToIsland()
{
	// T-088: a client already connected but still loading the boot map (a slower machine coming back from the island)
	// would be dropped by the travel, its connection closing with this world: it gets one more join wait, then the host goes
	const ENetMode Mode = GetNetMode();
	const int32 Loading = (Mode == NM_ListenServer || Mode == NM_DedicatedServer) ? CountClientsStillLoading(GetWorld()) : 0;
	const float Elapsed = static_cast<float>(GetWorld()->GetTimeSeconds() - DrawTime);
	if (Loading > 0 && Elapsed < 2.0f * MaxJoinWaitSeconds)
	{
		UE_LOG(LogMonkeIslands, Log, TEXT("Boot: %d client(s) still loading the boot map after %.0f s, the travel waits for them"), Loading, Elapsed);
		// checked again every second (its join restarts the grace through GenericPlayerInitialization)
		GetWorldTimerManager().SetTimer(TravelTimer, FTimerDelegate::CreateUObject(this, &AExpeditionBootGameMode::TravelToIsland), 1.0f, false);
		return;
	}
	if (GetNumPlayers() < ExpectedPlayers)
	{
		UE_LOG(LogMonkeIslands, Warning, TEXT("Boot: %d of %d expected player(s) after %.0f s, travelling without the others"),
			GetNumPlayers(), ExpectedPlayers, MaxJoinWaitSeconds);
	}
	// T-315: the crew here leaves for the expedition; its island takes only them (and those known from earlier islands)
	if (UCrewSessionSubsystem* Crew = UCrewSessionSubsystem::Get(this); Crew && GameState)
	{
		Crew->LaunchExpedition(*GameState);
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
