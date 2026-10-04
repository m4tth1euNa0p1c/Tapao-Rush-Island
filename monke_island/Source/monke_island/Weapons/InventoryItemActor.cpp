// monke_island

#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
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
		if (ActiveContext.Get() == Context)
		{
			return;
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
	// never leave a context that swallows keys behind a dead pawn
	SetInputContextActive(ActiveContext.Get(), 0, false);
	Super::EndPlay(EndPlayReason);
}
