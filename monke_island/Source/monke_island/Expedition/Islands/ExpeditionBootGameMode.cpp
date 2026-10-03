// monke_island

#include "Expedition/Islands/ExpeditionBootGameMode.h"
#include "Expedition/Islands/ExpeditionIslandDefinition.h"
#include "Expedition/Islands/ExpeditionIslandSubsystem.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
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
	UE_LOG(LogMonkeIslands, Log, TEXT("Boot: loading the island catalog"));
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
	GetWorldTimerManager().SetTimerForNextTick(FTimerDelegate::CreateUObject(this, &AExpeditionBootGameMode::TravelToIsland));
}

void AExpeditionBootGameMode::TravelToIsland()
{
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
