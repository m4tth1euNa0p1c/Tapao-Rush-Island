// monke_island

#include "TestMap/MonkeTestGameMode.h"
#include "Player/IslanderHealthComponent.h"
#include "Weapons/FirearmWeapon.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"
#include "GameFramework/Pawn.h"
#include "monke_island.h"

void AMonkeTestGameMode::SetPlayerDefaults(APawn* PlayerPawn)
{
	Super::SetPlayerDefaults(PlayerPawn);
	GiveExtraStartingItems(PlayerPawn);
	if (!bGodMode || !PlayerPawn)
	{
		return;
	}
	PlayerPawn->SetCanBeDamaged(false);
	if (UIslanderHealthComponent* Health = PlayerPawn->FindComponentByClass<UIslanderHealthComponent>())
	{
		Health->ResetHealth();
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: god mode for %s (no damage, full health)"), *GetName(), *GetNameSafe(PlayerPawn));
}

void AMonkeTestGameMode::BeginPlay()
{
	Super::BeginPlay();
	if (bInfiniteAmmo)
	{
		GetWorldTimerManager().SetTimer(AmmoTimer, this, &AMonkeTestGameMode::RefillAmmo, FMath::Max(AmmoRefillInterval, 0.05f), true);
		UE_LOG(Logmonke_island, Log, TEXT("%s: infinite ammunition for every firearm of the players"), *GetName());
	}
}

void AMonkeTestGameMode::RefillAmmo()
{
	for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
	{
		const APlayerController* Controller = It->Get();
		const APawn* Pawn = Controller ? Controller->GetPawn() : nullptr;
		const UIslanderInventoryComponent* Inventory = Pawn ? Pawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
		if (!Inventory)
		{
			continue;
		}
		for (int32 Slot = 0; Slot < Inventory->GetSlotNum(); ++Slot)
		{
			AFirearmWeapon* Firearm = Cast<AFirearmWeapon>(Inventory->GetItem(Slot));
			const int32 Room = Firearm ? Firearm->GetAmmoRoom() : 0;
			if (Room > 0)
			{
				Firearm->AddAmmo(Firearm->GetAmmoKind(), Room);
			}
		}
	}
}

void AMonkeTestGameMode::GiveExtraStartingItems(APawn* PlayerPawn) const
{
	UIslanderInventoryComponent* Inventory = PlayerPawn ? PlayerPawn->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (!Inventory || ExtraStartingItems.IsEmpty())
	{
		return;
	}
	int32 Given = 0;
	for (const UInventoryItemDefinition* Definition : ExtraStartingItems)
	{
		// a stack of it first, then the first free cell; a full inventory keeps what it has
		if (Definition && Inventory->GiveItem(Definition))
		{
			++Given;
		}
	}
	UE_LOG(Logmonke_island, Log, TEXT("%s: %d of %d extra starting items given to %s"), *GetName(), Given, ExtraStartingItems.Num(), *GetNameSafe(PlayerPawn));
}
