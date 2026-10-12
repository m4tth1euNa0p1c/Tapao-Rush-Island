// monke_island

#include "Weapons/AmmoPickup.h"
#include "Items/NetRestComponent.h"
#include "Weapons/AmmoDefinition.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/CollisionProfile.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "UObject/ConstructorHelpers.h"

#define LOCTEXT_NAMESPACE "AmmoPickup"

namespace
{
	/** "12 balles de mousquet" */
	FText DescribeLot(EAmmoKind Kind, int32 Count)
	{
		const FText Number = FText::AsNumber(Count);
		switch (Kind)
		{
		case EAmmoKind::MusketBall: return FText::Format(LOCTEXT("MusketBall", "{0} balle(s) de mousquet"), Number);
		case EAmmoKind::PistolCharge: return FText::Format(LOCTEXT("PistolCharge", "{0} charge(s) de pistolet"), Number);
		case EAmmoKind::Shell: return FText::Format(LOCTEXT("Shell", "{0} cartouche(s) de fusil à pompe"), Number);
		case EAmmoKind::Shot: return FText::Format(LOCTEXT("Shot", "{0} charge(s) de plombs"), Number);
		case EAmmoKind::RifleCartridge: return FText::Format(LOCTEXT("RifleCartridge", "{0} cartouche(s) de carabine"), Number);
		case EAmmoKind::Grenade: return FText::Format(LOCTEXT("Grenade", "{0} grenade(s)"), Number);
		case EAmmoKind::MachineGunBelt: return FText::Format(LOCTEXT("MachineGunBelt", "{0} cartouche(s) de mitrailleuse"), Number);
		case EAmmoKind::SmgCartridge: return FText::Format(LOCTEXT("SmgCartridge", "{0} cartouche(s) de mitraillette"), Number);
		default: return Number;
		}
	}
}

AAmmoPickup::AAmmoPickup()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
	RootComponent = Mesh;
	Mesh->SetCollisionProfileName(UCollisionProfile::PhysicsActor_ProfileName);
	// players walk over a packet on the sand without tripping; the crosshair (F) still finds it
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

void AAmmoPickup::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(AAmmoPickup, Definition);
	DOREPLIFETIME(AAmmoPickup, Remaining);
	DOREPLIFETIME(AAmmoPickup, bTaken);
}

AAmmoPickup* AAmmoPickup::Spawn(UWorld* World, TSubclassOf<AAmmoPickup> PickupClass, UAmmoDefinition* InDefinition, const FTransform& Transform)
{
	if (!World || !InDefinition)
	{
		return nullptr;
	}
	UClass* Class = PickupClass ? PickupClass.Get() : AAmmoPickup::StaticClass();
	// deferred: the definition is in place before BeginPlay turns the physics on
	AAmmoPickup* Pickup = World->SpawnActorDeferred<AAmmoPickup>(Class, Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Pickup)
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Ammo: packet %s of %s could not be spawned"), *GetNameSafe(Class), *InDefinition->GetName());
		return nullptr;
	}
	Pickup->Definition = InDefinition;
	Pickup->FinishSpawning(Transform);
	return Pickup;
}

void AAmmoPickup::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	ApplyMesh();
}

void AAmmoPickup::BeginPlay()
{
	Super::BeginPlay();
	OnRep_Definition();
	if (HasAuthority() && Definition && Remaining.IsEmpty())
	{
		for (const FAmmoLot& Lot : Definition->Lots)
		{
			Remaining.Add(FMath::Max(Lot.Count, 0));
		}
	}
}

void AAmmoPickup::ApplyMesh()
{
	UStaticMesh* Shown = Definition && Definition->PickupMesh ? Definition->PickupMesh.Get() : FallbackMesh.Get();
	if (Mesh->GetStaticMesh() != Shown)
	{
		Mesh->SetStaticMesh(Shown);
	}
	Mesh->SetRelativeScale3D(Definition && Definition->PickupMesh ? FVector::OneVector : FallbackMeshScale);
}

void AAmmoPickup::OnRep_Definition()
{
	ApplyMesh();
	if (Mesh->GetStaticMesh() && !bTaken)
	{
		Mesh->SetSimulatePhysics(true);
		Mesh->SetMassOverrideInKg(NAME_None, Definition ? Definition->PickupMassKg : 1.0f, true);
	}
}

void AAmmoPickup::OnRep_Taken()
{
	if (bTaken)
	{
		Mesh->SetSimulatePhysics(false);
		// hidden through its mesh, not the actor: an actor hidden without collision is no longer relevant, so a client whose
		// channel slept (dormant at rest) would only see the packet go when it is destroyed (T-316)
		Mesh->SetVisibility(false, /*bPropagateToChildren*/ true);
		SetActorEnableCollision(false);
	}
}

int32 AAmmoPickup::RemainingOfLot(int32 Index) const
{
	if (!Definition || !Definition->Lots.IsValidIndex(Index))
	{
		return 0;
	}
	return Remaining.IsValidIndex(Index) ? Remaining[Index] : FMath::Max(Definition->Lots[Index].Count, 0);
}

int32 AAmmoPickup::GetRemainingOf(EAmmoKind Kind) const
{
	int32 Count = 0;
	if (Definition)
	{
		for (int32 Index = 0; Index < Definition->Lots.Num(); ++Index)
		{
			Count += Definition->Lots[Index].Kind == Kind ? RemainingOfLot(Index) : 0;
		}
	}
	return Count;
}

FText AAmmoPickup::DescribeContents() const
{
	TArray<FText> Parts;
	if (Definition)
	{
		for (int32 Index = 0; Index < Definition->Lots.Num(); ++Index)
		{
			if (RemainingOfLot(Index) > 0)
			{
				Parts.Add(DescribeLot(Definition->Lots[Index].Kind, RemainingOfLot(Index)));
			}
		}
	}
	return FText::Join(LOCTEXT("Separator", ", "), Parts);
}

FText AAmmoPickup::GetInteractionPrompt_Implementation(APawn* User) const
{
	const UIslanderInventoryComponent* Inventory = User ? User->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (!Definition || !Inventory || bTaken)
	{
		return FText::GetEmpty();
	}
	// the short message of the HUD: what the packet gives, or why the carrier cannot take it
	bool bWeapon = false;
	bool bRoom = false;
	for (int32 Index = 0; Index < Definition->Lots.Num(); ++Index)
	{
		const EAmmoKind Kind = Definition->Lots[Index].Kind;
		if (RemainingOfLot(Index) > 0 && Inventory->HasFirearmFor(Kind))
		{
			bWeapon = true;
			bRoom |= Inventory->GetAmmoRoom(Kind) > 0;
		}
	}
	if (!bWeapon)
	{
		return FText::Format(LOCTEXT("NoWeapon", "{0} : aucune arme pour ces munitions"), Definition->DisplayName);
	}
	if (!bRoom)
	{
		return FText::Format(LOCTEXT("Full", "{0} : poche pleine"), Definition->DisplayName);
	}
	return FText::Format(LOCTEXT("PickUp", "Ramasser {0} ({1})"), Definition->DisplayName, DescribeContents());
}

bool AAmmoPickup::CanInteract_Implementation(APawn* User) const
{
	if (!User || !Definition || bTaken || IsActorBeingDestroyed() || !User->FindComponentByClass<UIslanderInventoryComponent>())
	{
		return false;
	}
	// a dead or knocked out player has no free hand; the refusals keep the prompt (the message) and refuse on F
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(User); Islander && Islander->IsRagdoll())
	{
		return false;
	}
	const UIslanderHealthComponent* Health = User->FindComponentByClass<UIslanderHealthComponent>();
	return !(Health && Health->IsDead());
}

void AAmmoPickup::Interact_Implementation(APawn* User)
{
	if (!HasAuthority() || !IIslanderInteractable::Execute_CanInteract(this, User))
	{
		return;
	}
	GiveTo(User);
}

bool AAmmoPickup::GiveTo(APawn* User)
{
	UIslanderInventoryComponent* Inventory = User ? User->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	if (!Inventory || !Definition)
	{
		return false;
	}
	if (Remaining.Num() != Definition->Lots.Num())
	{
		Remaining.Reset();
		for (const FAmmoLot& Lot : Definition->Lots)
		{
			Remaining.Add(FMath::Max(Lot.Count, 0));
		}
	}
	int32 Given = 0;
	bool bAnyWeapon = false;
	for (int32 Index = 0; Index < Definition->Lots.Num(); ++Index)
	{
		const EAmmoKind Kind = Definition->Lots[Index].Kind;
		const int32 Count = Remaining[Index];
		if (Count <= 0 || !Inventory->HasFirearmFor(Kind))
		{
			continue;
		}
		bAnyWeapon = true;
		const int32 Taken = Inventory->GiveAmmo(Kind, Count);
		Remaining[Index] = Count - Taken;
		Given += Taken;
	}
	if (Given <= 0)
	{
		Inventory->Notice(bAnyWeapon ? LOCTEXT("NoticeFull", "Poche pleine") : LOCTEXT("NoticeNoWeapon", "Aucune arme pour ces munitions"));
		return false;
	}
	bool bEmpty = true;
	for (const int32 Left : Remaining)
	{
		bEmpty &= Left <= 0;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Ammo: %s takes %d round(s) of %s%s"), *User->GetName(), Given, *Definition->GetName(), bEmpty ? TEXT(" (empty)") : TEXT(""));
	// a packet at rest sleeps (dormant): what is left in it, or that it is empty, reaches every client now (T-316)
	UNetRestComponent::WakeActorNet(this);
	if (bEmpty)
	{
		// emptied before anything else can run: a second press in the same frame finds bTaken
		bTaken = true;
		OnRep_Taken();
		SetLifeSpan(FMath::Max(DestroyDelay, 0.01f));
	}
	return true;
}

#undef LOCTEXT_NAMESPACE
