// monke_island

#include "Weapons/IslanderInventoryComponent.h"
#include "Boats/BoatRiderComponent.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "monke_island.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

UIslanderInventoryComponent::UIslanderInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UIslanderInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UIslanderInventoryComponent, Items);
	DOREPLIFETIME(UIslanderInventoryComponent, ActiveSlot);
}

void UIslanderInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		SpawnStartingItems();
		ApplyActiveSlot(StartingActiveSlot);
	}
	TryBindInput();
}

void UIslanderInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(BindTimer);
	if (GetOwner()->HasAuthority())
	{
		for (AInventoryItemActor* Item : Items)
		{
			if (IsValid(Item))
			{
				Item->Destroy();
			}
		}
		Items.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UIslanderInventoryComponent::SpawnStartingItems()
{
	Items.Init(nullptr, FMath::Max(SlotCount, 1));
	APawn* Pawn = Cast<APawn>(GetOwner());
	for (int32 Slot = 0; Slot < Items.Num() && Slot < StartingItems.Num(); ++Slot)
	{
		const UInventoryItemDefinition* Definition = StartingItems[Slot];
		if (!Definition)
		{
			continue;
		}
		if (!Definition->ItemClass)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Inventory: item %s has no ItemClass, slot %d left empty"), *Definition->GetName(), Slot + 1);
			continue;
		}
		FActorSpawnParameters Params;
		Params.Owner = GetOwner();
		Params.Instigator = Pawn;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AInventoryItemActor* Item = GetWorld()->SpawnActor<AInventoryItemActor>(Definition->ItemClass, GetOwner()->GetActorTransform(), Params);
		if (Item)
		{
			Item->SetDefinition(Definition);
			Items[Slot] = Item;
		}
	}
}

AInventoryItemActor* UIslanderInventoryComponent::GetItem(int32 Slot) const
{
	return Items.IsValidIndex(Slot) ? Items[Slot].Get() : nullptr;
}

float UIslanderInventoryComponent::ModifyIncomingDamage(float Damage, const AActor* DamageCauser) const
{
	AInventoryItemActor* Item = GetEquippedItem();
	return Item ? Item->ModifyIncomingDamage(Damage, DamageCauser) : Damage;
}

void UIslanderInventoryComponent::SelectSlot(int32 Slot)
{
	if (GetOwner()->HasAuthority())
	{
		ServerSelectSlot_Implementation(Slot);
	}
	else
	{
		ServerSelectSlot(Slot);
	}
}

void UIslanderInventoryComponent::ServerSelectSlot_Implementation(int32 Slot)
{
	if (!Items.IsValidIndex(Slot))
	{
		return;
	}
	// seated in a boat (the rower holds the oars): hands stay free, an item can only be put away
	const UBoatRiderComponent* Rider = GetOwner() ? GetOwner()->FindComponentByClass<UBoatRiderComponent>() : nullptr;
	if (Rider && Rider->IsSeated() && Slot != ActiveSlot)
	{
		return;
	}
	// same key again: put the item away
	ApplyActiveSlot(Slot == ActiveSlot ? -1 : Slot);
}

void UIslanderInventoryComponent::ApplyActiveSlot(int32 Slot)
{
	ActiveSlot = GetItem(Slot) ? Slot : -1;
	for (int32 Index = 0; Index < Items.Num(); ++Index)
	{
		if (AInventoryItemActor* Item = Items[Index])
		{
			Item->SetEquipped(Index == ActiveSlot);
		}
	}
}

void UIslanderInventoryComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (bInputBound || !Pawn || SlotActions.Num() == 0)
	{
		return;
	}

	UEnhancedInputComponent* Input = Pawn->IsLocallyControlled() ? Cast<UEnhancedInputComponent>(Pawn->InputComponent) : nullptr;
	const APlayerController* PlayerController = Cast<APlayerController>(Pawn->GetController());
	if (!Input || !PlayerController)
	{
		// the controller and the input component arrive after BeginPlay on clients; remote pawns never get them,
		// so the retries stop after about 15 s
		if ((!Pawn->GetController() || Pawn->IsLocallyControlled()) && ++BindAttempts < 60)
		{
			GetWorld()->GetTimerManager().SetTimer(BindTimer, this, &UIslanderInventoryComponent::TryBindInput, 0.25f, false);
		}
		return;
	}

	for (int32 Slot = 0; Slot < SlotActions.Num(); ++Slot)
	{
		if (SlotActions[Slot])
		{
			Input->BindAction(SlotActions[Slot], ETriggerEvent::Started, this, &UIslanderInventoryComponent::HandleSlotAction, Slot);
		}
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
	{
		if (InventoryMappingContext)
		{
			Subsystem->AddMappingContext(InventoryMappingContext, InventoryMappingPriority);
		}
	}
	bInputBound = true;
}

void UIslanderInventoryComponent::HandleSlotAction(int32 Slot)
{
	SelectSlot(Slot);
}
