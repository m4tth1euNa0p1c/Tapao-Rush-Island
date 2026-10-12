// monke_island

#include "Items/ConsumablePickupActor.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/Pawn.h"
#include "Items/ConsumableDefinition.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderHealthComponent.h"
#include "monke_island.h"

#define LOCTEXT_NAMESPACE "Consumable"

AConsumablePickupActor::AConsumablePickupActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	NetDormancy = DORM_Initial;

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	Mesh->SetCollisionProfileName(UCollisionProfile::BlockAllDynamic_ProfileName);
	RootComponent = Mesh;
}

void AConsumablePickupActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	if (Definition && Definition->Mesh && Mesh->GetStaticMesh() != Definition->Mesh)
	{
		Mesh->SetStaticMesh(Definition->Mesh);
	}
}

void AConsumablePickupActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME(AConsumablePickupActor, bConsumed);
}

bool AConsumablePickupActor::CanConsume(bool bAlreadyConsumed, bool bUserDead, float Health, float MaxHealth, bool bUsableAtFullHealth)
{
	if (bAlreadyConsumed || bUserDead)
	{
		return false;
	}
	return bUsableAtFullHealth || Health < MaxHealth;
}

FText AConsumablePickupActor::GetInteractionPrompt_Implementation(APawn* User) const
{
	if (!Definition)
	{
		return FText::GetEmpty();
	}
	return FText::Format(LOCTEXT("Prompt", "{0} (+{1} PV)"), Definition->ActionText,
		FText::AsNumber(FMath::RoundToInt(Definition->HealAmount)));
}

bool AConsumablePickupActor::CanInteract_Implementation(APawn* User) const
{
	if (!Definition || !User)
	{
		return false;
	}
	const UIslanderHealthComponent* Health = User->FindComponentByClass<UIslanderHealthComponent>();
	if (!Health)
	{
		return false;
	}
	return CanConsume(bConsumed, Health->IsDead(), Health->GetHealth(), Health->MaxHealth, Definition->bUsableAtFullHealth);
}

void AConsumablePickupActor::Interact_Implementation(APawn* User)
{
	if (!HasAuthority() || !IIslanderInteractable::Execute_CanInteract(this, User))
	{
		return;
	}
	UIslanderHealthComponent* Health = User->FindComponentByClass<UIslanderHealthComponent>();
	const float Healed = Health->ApplyHeal(Definition->HealAmount, this);
	UE_LOG(Logmonke_island, Log, TEXT("%s consumed %s: +%.0f health"), *GetNameSafe(User), *GetName(), Healed);

	bConsumed = true;
	// initially dormant (no channel): woken first, then its sound sent while it is still shown. A multicast goes only to the
	// clients the actor is relevant for, and an actor hidden without collision is relevant for none (T-316). A static
	// coconut has no UNetRestComponent: that one serves the free physics bodies
	FlushNetDormancy();
	MulticastConsumed();
	OnRep_Consumed();
	SetLifeSpan(FMath::Max(DestroyDelay, 0.01f));
}

void AConsumablePickupActor::OnRep_Consumed()
{
	if (bConsumed)
	{
		// hidden through its mesh, not the actor: still relevant, so bConsumed opens the woken channel and every client hides
		// it now, not only when it is destroyed (T-316)
		Mesh->SetVisibility(false, /*bPropagateToChildren*/ true);
		SetActorEnableCollision(false);
	}
}

void AConsumablePickupActor::MulticastConsumed_Implementation()
{
	if (Definition && Definition->UseSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Definition->UseSound, GetActorLocation());
	}
}

#undef LOCTEXT_NAMESPACE
