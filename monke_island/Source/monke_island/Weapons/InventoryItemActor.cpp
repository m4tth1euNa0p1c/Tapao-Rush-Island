// monke_island

#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"

AInventoryItemActor::AInventoryItemActor()
{
	bReplicates = true;
	SetReplicatingMovement(false);
	bNetUseOwnerRelevancy = true;
	PrimaryActorTick.bCanEverTick = false;
}

void AInventoryItemActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInventoryItemActor, bEquipped);
	DOREPLIFETIME_CONDITION(AInventoryItemActor, Definition, COND_InitialOnly);
}

AIslanderCharacter* AInventoryItemActor::GetCarrier() const
{
	return Cast<AIslanderCharacter>(GetOwner());
}

void AInventoryItemActor::BeginPlay()
{
	Super::BeginPlay();
	AttachToCarrier();
}

void AInventoryItemActor::OnRep_Owner()
{
	Super::OnRep_Owner();
	// a new carrier: the bindings on the former one's input go (on the former carrier's machine, R-37)
	RemoveInputBindings();
	AttachToCarrier();
}

void AInventoryItemActor::AttachToCarrier()
{
	// attached on every machine (movement is not replicated): follows the carrier, relevant with it
	if (AIslanderCharacter* Carrier = GetCarrier(); Carrier && GetAttachParentActor() != Carrier)
	{
		AttachToActor(Carrier, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
		OnAttachedToCarrier();
	}
}

AInventoryItemActor* AInventoryItemActor::SpawnFromDefinition(const UInventoryItemDefinition* InDefinition, AActor* Carrier)
{
	if (!InDefinition || !InDefinition->ItemClass || !Carrier || !Carrier->HasAuthority())
	{
		return nullptr;
	}
	FActorSpawnParameters Params;
	Params.Owner = Carrier;
	Params.Instigator = Cast<APawn>(Carrier);
	Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AInventoryItemActor* Item = Carrier->GetWorld()->SpawnActor<AInventoryItemActor>(InDefinition->ItemClass, Carrier->GetActorTransform(), Params);
	if (Item)
	{
		Item->SetDefinition(InDefinition);
	}
	return Item;
}

void AInventoryItemActor::SetCarrier(AActor* NewCarrier)
{
	if (!HasAuthority())
	{
		return;
	}
	SetEquipped(false);
	// the listen server's own pawn may have carried it: its bindings go before anyone else holds the item (R-37)
	RemoveInputBindings();
	SetOwner(NewCarrier);
	SetInstigator(Cast<APawn>(NewCarrier));
	SetOnGround(false);
	// clients follow through OnRep_Owner, at once even on an item of low net frequency (the torch)
	AttachToCarrier();
	ForceNetUpdate();
}

void AInventoryItemActor::SetOnGround(bool bInOnGround)
{
	if (!HasAuthority())
	{
		return;
	}
	if (bInOnGround)
	{
		SetEquipped(false);
	}
	bOnGround = bInOnGround;
	// the whole actor (bHidden replicates): a sheathed blade or a slung musket must not stay on the former carrier
	SetActorHiddenInGame(bInOnGround);
	ForceNetUpdate();
}

void AInventoryItemActor::KeepValue(int32 InBaseValue, int32 InValue)
{
	if (!HasAuthority())
	{
		return;
	}
	KeptBaseValue = FMath::Max(0, InBaseValue);
	KeptValue = FMath::Clamp(InValue, 0, KeptBaseValue);
	bHasKeptValue = true;
}

bool AInventoryItemActor::GetKeptValue(int32& OutBaseValue, int32& OutValue) const
{
	OutBaseValue = KeptBaseValue;
	OutValue = KeptValue;
	return bHasKeptValue;
}

bool AInventoryItemActor::IsCarrierLocallyControlled() const
{
	const AIslanderCharacter* Carrier = GetCarrier();
	return Carrier && Carrier->IsLocallyControlled();
}

void AInventoryItemActor::SetEquipped(bool bInEquipped)
{
	if (!HasAuthority() || bEquipped == bInEquipped)
	{
		return;
	}
	bEquipped = bInEquipped;
	// every machine shows or hides it now, whatever the item's net frequency
	ForceNetUpdate();
	OnEquippedChanged();
}

void AInventoryItemActor::OnRep_Equipped()
{
	OnEquippedChanged();
}

void AInventoryItemActor::SetInputContextActive(UInputMappingContext* Context, int32 Priority, bool bActive)
{
	if (!Context)
	{
		return;
	}
	if (bActive)
	{
		// several items share a context (IMC_Musket: musket, pistol, shotgun): the one put away removes it from the
		// player, maybe after the one drawn added it, so a context this item holds is checked and pushed back if gone
		if (ActiveContext.Get() == Context)
		{
			const UEnhancedInputLocalPlayerSubsystem* Held = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(ContextPlayer.Get());
			if (!Held || Held->HasMappingContext(Context))
			{
				return;
			}
			ActiveContext.Reset();
		}
		const AIslanderCharacter* Carrier = GetCarrier();
		const APlayerController* PlayerController = Carrier ? Cast<APlayerController>(Carrier->GetController()) : nullptr;
		ULocalPlayer* LocalPlayer = PlayerController ? PlayerController->GetLocalPlayer() : nullptr;
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(LocalPlayer))
		{
			// a higher priority context consumes the keys it maps (left click no longer punches)
			Subsystem->AddMappingContext(Context, Priority);
			ContextPlayer = LocalPlayer;
			ActiveContext = Context;
		}
		return;
	}

	if (ActiveContext.Get() != Context)
	{
		return;
	}
	if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(ContextPlayer.Get()))
	{
		Subsystem->RemoveMappingContext(Context);
	}
	ContextPlayer.Reset();
	ActiveContext.Reset();
}

void AInventoryItemActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// never leave a context that swallows keys behind a dead pawn, nor bindings to a destroyed item
	SetInputContextActive(ActiveContext.Get(), 0, false);
	RemoveInputBindings();
	Super::EndPlay(EndPlayReason);
}

void AInventoryItemActor::KeepInputBinding(UEnhancedInputComponent* Input, uint32 Handle)
{
	if (Input)
	{
		KeptBindings.Add({Input, Handle});
	}
}

void AInventoryItemActor::RemoveInputBindings()
{
	for (const FKeptInputBinding& Kept : KeptBindings)
	{
		if (UEnhancedInputComponent* Input = Kept.Input.Get())
		{
			Input->RemoveBindingByHandle(Kept.Handle);
		}
	}
	KeptBindings.Reset();
	OnInputBindingsRemoved();
}
