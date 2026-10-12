// monke_island

#include "Weapons/InventoryPickup.h"
#include "Items/NetRestComponent.h"
#include "Items/ValuableComponent.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"
#include "monke_island.h"

#define LOCTEXT_NAMESPACE "InventoryPickup"

AInventoryPickup::AInventoryPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// players walk over a weapon on the sand without tripping; the crosshair (F) still finds it
	Mesh->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	Mesh->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	Mesh->SetGenerateOverlapEvents(false);
	Mesh->BodyInstance.bUseCCD = true;
	// the network rest hears at once when the body wakes up (T-316)
	Mesh->BodyInstance.bGenerateWakeEvents = true;

	// 30 updates a second while it moves instead of the engine's 100, none at all at rest, glued to the galleon's deck or
	// the barque's hull on every machine (T-316, review R-43)
	NetRest = CreateDefaultSubobject<UNetRestComponent>(TEXT("NetRest"));

	static ConstructorHelpers::FObjectFinder<UStaticMesh> Cube(TEXT("/Engine/BasicShapes/Cube.Cube"));
	FallbackMesh = Cube.Object;
}

void AInventoryPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AInventoryPickup, Definition);
	DOREPLIFETIME(AInventoryPickup, bClaimed);
}

AInventoryPickup* AInventoryPickup::SpawnForItem(UWorld* World, TSubclassOf<AInventoryPickup> PickupClass, AInventoryItemActor* Item,
	const FTransform& Transform, const FVector& Velocity)
{
	if (!World || !Item || !Item->HasAuthority())
	{
		return nullptr;
	}
	UInventoryItemDefinition* ItemDefinition = const_cast<UInventoryItemDefinition*>(Item->GetDefinition());
	AInventoryPickup* Pickup = BeginSpawn(World, PickupClass, ItemDefinition, Transform);
	if (!Pickup)
	{
		return nullptr;
	}
	// the value the item carries, given back before BeginPlay: the pickup keeps the losses and never takes the zone
	// multiplier a second time (T-309, network review R-39); a valuable added by the Blueprint only exists afterwards
	int32 KeptBaseValue = 0;
	int32 KeptValue = 0;
	const bool bHasValue = Item->GetKeptValue(KeptBaseValue, KeptValue);
	UValuableComponent* Valuable = bHasValue ? Pickup->FindComponentByClass<UValuableComponent>() : nullptr;
	if (Valuable)
	{
		Valuable->RestoreValue(KeptBaseValue, KeptValue);
	}
	Pickup->FinishSpawning(Transform);
	if (bHasValue && !Valuable)
	{
		if (UValuableComponent* Added = Pickup->FindComponentByClass<UValuableComponent>())
		{
			Added->RestoreValue(KeptBaseValue, KeptValue);
		}
	}
	Pickup->StoredItem = Item;
	Item->SetOnGround(true);
	if (Pickup->Mesh->IsSimulatingPhysics())
	{
		Pickup->Mesh->SetPhysicsLinearVelocity(Velocity);
	}
	return Pickup;
}

AInventoryPickup* AInventoryPickup::SpawnForDefinition(UWorld* World, TSubclassOf<AInventoryPickup> PickupClass, UInventoryItemDefinition* InDefinition,
	const FTransform& Transform)
{
	AInventoryPickup* Pickup = BeginSpawn(World, PickupClass, InDefinition, Transform);
	if (Pickup)
	{
		Pickup->FinishSpawning(Transform);
	}
	return Pickup;
}

AInventoryPickup* AInventoryPickup::BeginSpawn(UWorld* World, TSubclassOf<AInventoryPickup> PickupClass, UInventoryItemDefinition* InDefinition,
	const FTransform& Transform)
{
	if (!World || !InDefinition)
	{
		return nullptr;
	}
	UClass* Class = PickupClass ? PickupClass.Get() : AInventoryPickup::StaticClass();
	// deferred: the definition is in place before BeginPlay turns the physics on
	AInventoryPickup* Pickup = World->SpawnActorDeferred<AInventoryPickup>(Class, Transform, nullptr, nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Inventory: pickup %s of %s could not be spawned"), *GetNameSafe(Class), *InDefinition->GetName());
		return nullptr;
	}
	Pickup->Definition = InDefinition;
	return Pickup;
}

void AInventoryPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMesh();
}

void AInventoryPickup::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Definition();
}

void AInventoryPickup::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// nobody took it: the kept item goes with the pickup (never an orphan hidden actor)
	if (HasAuthority() && IsValid(StoredItem))
	{
		StoredItem->Destroy();
	}
	StoredItem = nullptr;
	Super::EndPlay(EndPlayReason);
}

void AInventoryPickup::ApplyMesh()
{
	UStaticMesh* Shown = Definition && Definition->PickupMesh ? Definition->PickupMesh.Get() : FallbackMesh.Get();
	if (Mesh->GetStaticMesh() != Shown)
	{
		Mesh->SetStaticMesh(Shown);
	}
	Mesh->SetRelativeScale3D(Definition && Definition->PickupMesh ? FVector::OneVector : FallbackMeshScale);
}

void AInventoryPickup::OnRep_Definition()
{
	ApplyMesh();
	if (Mesh->GetStaticMesh() && !bClaimed)
	{
		Mesh->SetSimulatePhysics(true);
		Mesh->SetMassOverrideInKg(NAME_None, Definition ? Definition->PickupMassKg : 3.0f, true);
	}
}

void AInventoryPickup::OnRep_Claimed()
{
	if (bClaimed)
	{
		Mesh->SetSimulatePhysics(false);
		SetActorHiddenInGame(true);
		SetActorEnableCollision(false);
	}
}

AInventoryItemActor* AInventoryPickup::Claim(AActor* NewCarrier)
{
	if (!HasAuthority() || bClaimed || IsActorBeingDestroyed() || !NewCarrier)
	{
		return nullptr;
	}
	AInventoryItemActor* Item = StoredItem;
	if (IsValid(Item))
	{
		Item->SetCarrier(NewCarrier);
	}
	else
	{
		Item = AInventoryItemActor::SpawnFromDefinition(Definition, NewCarrier);
	}
	if (!Item)
	{
		return nullptr;
	}
	// the value goes with the item: a mob weapon dropped again keeps its losses and the multiplier of the zone it was
	// found in (T-309, network review R-39)
	if (const UValuableComponent* Valuable = FindComponentByClass<UValuableComponent>())
	{
		Item->KeepValue(Valuable->BaseValue, Valuable->GetValue());
	}
	// claimed before anything else can run: a second request in the same frame finds bClaimed
	bClaimed = true;
	StoredItem = nullptr;
	OnRep_Claimed();
	// gone at once, no wake needed: the net driver sends the destruction to a client whose channel sleeps (dormant at rest)
	// as to the others, and a pickup glued to the deck or the hull leaves its base with it (T-316)
	Destroy();
	return Item;
}

FText AInventoryPickup::GetInteractionPrompt_Implementation(APawn* User) const
{
	const UIslanderInventoryComponent* Inventory = User ? User->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (!Definition || !Inventory)
	{
		return FText::GetEmpty();
	}
	bool bReplaces = false;
	const int32 Slot = Inventory->ChooseSlotForPickup(bReplaces);
	if (Slot == INDEX_NONE)
	{
		return LOCTEXT("Full", "Inventaire plein : prends en main l'arme à échanger");
	}
	if (bReplaces)
	{
		const AInventoryItemActor* Equipped = Inventory->GetEquippedItem();
		const UInventoryItemDefinition* EquippedDefinition = Equipped ? Equipped->GetDefinition() : nullptr;
		return FText::Format(LOCTEXT("Swap", "Ramasser {0} (lâche {1})"), Definition->DisplayName,
			EquippedDefinition ? EquippedDefinition->DisplayName : LOCTEXT("Equipped", "l'arme en main"));
	}
	return FText::Format(LOCTEXT("PickUp", "Ramasser {0}"), Definition->DisplayName);
}

bool AInventoryPickup::CanInteract_Implementation(APawn* User) const
{
	const UIslanderInventoryComponent* Inventory = User ? User->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	return Inventory && Inventory->CanPickUp(this);
}

void AInventoryPickup::Interact_Implementation(APawn* User)
{
	if (!HasAuthority() || !User)
	{
		return;
	}
	if (UIslanderInventoryComponent* Inventory = User->FindComponentByClass<UIslanderInventoryComponent>())
	{
		Inventory->PickUp(this);
	}
}

#undef LOCTEXT_NAMESPACE
