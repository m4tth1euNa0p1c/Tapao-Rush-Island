// monke_island

#include "Shop/ShopkeeperActor.h"
#include "Shop/ShopCatalog.h"
#include "Animation/AnimSequence.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunSubsystem.h"
#include "Weapons/AmmoDefinition.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "monke_island.h"

AShopkeeperActor::AShopkeeperActor()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;
	SetReplicatingMovement(true);

	// the bean is 1.05 m: a capsule that blocks the players and the interaction trace (Visibility)
	Capsule = CreateDefaultSubobject<UCapsuleComponent>(TEXT("Capsule"));
	Capsule->InitCapsuleSize(30.0f, 55.0f);
	Capsule->SetCollisionProfileName(UCollisionProfile::BlockAll_ProfileName);
	Capsule->SetCanEverAffectNavigation(false);
	RootComponent = Capsule;

	Body = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(Capsule);
	Body->SetRelativeLocation(FVector(0.0, 0.0, -55.0));
	Body->SetRelativeRotation(FRotator(0.0, -90.0, 0.0));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Body->SetCanEverAffectNavigation(false);

	Outfit = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("Outfit"));
	Outfit->SetupAttachment(Body);
	Outfit->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Outfit->SetCanEverAffectNavigation(false);

	DefaultCatalog = TSoftObjectPtr<UShopCatalog>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Galleon/Shop/DA_ShopCatalog.DA_ShopCatalog")));
	BodyMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Body.SK_Bean_Body")));
	OutfitMesh = TSoftObjectPtr<USkeletalMesh>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/Bean/Meshes/SK_Bean_Outfit_Redcoat.SK_Bean_Outfit_Redcoat")));
	IdleAnimation = TSoftObjectPtr<UAnimSequence>(FSoftObjectPath(TEXT("/Game/MonkeIsland/Characters/Bean/Animations/Library/A_Bean_Idle.A_Bean_Idle")));
}

void AShopkeeperActor::BeginPlay()
{
	Super::BeginPlay();
	if (!Body->GetSkeletalMeshAsset())
	{
		Body->SetSkeletalMeshAsset(BodyMesh.LoadSynchronous());
	}
	if (!Outfit->GetSkeletalMeshAsset())
	{
		Outfit->SetSkeletalMeshAsset(OutfitMesh.LoadSynchronous());
	}
	Outfit->SetLeaderPoseComponent(Body);
	if (UAnimSequence* Idle = IdleAnimation.LoadSynchronous())
	{
		Body->PlayAnimation(Idle, true);
	}
	if (!GetCatalog())
	{
		UE_LOG(Logmonke_island, Warning, TEXT("Shop %s: no catalog (DA_ShopCatalog), nothing for sale"), *GetName());
	}
}

UShopCatalog* AShopkeeperActor::GetCatalog() const
{
	return Catalog ? Catalog.Get() : DefaultCatalog.LoadSynchronous();
}

FText AShopkeeperActor::GetInteractionPrompt_Implementation(APawn* User) const
{
	return Prompt;
}

bool AShopkeeperActor::CanInteract_Implementation(APawn* User) const
{
	const UIslanderHealthComponent* Health = User ? User->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	return User && User->FindComponentByClass<UIslanderInventoryComponent>() && !(Health && Health->IsDead());
}

void AShopkeeperActor::Interact_Implementation(APawn* User)
{
	if (UIslanderInventoryComponent* Inventory = User ? User->FindComponentByClass<UIslanderInventoryComponent>() : nullptr)
	{
		Inventory->ClientOpenShop(this);
	}
}

EShopResult AShopkeeperActor::Sell(APawn* Buyer, int32 Offer)
{
	if (!HasAuthority())
	{
		return EShopResult::TooFar;
	}
	const UShopCatalog* Shop = GetCatalog();
	UIslanderInventoryComponent* Inventory = Buyer ? Buyer->FindComponentByClass<UIslanderInventoryComponent>() : nullptr;
	const UIslanderHealthComponent* Health = Buyer ? Buyer->FindComponentByClass<UIslanderHealthComponent>() : nullptr;
	const FShopOffer* Line = Shop && Shop->Offers.IsValidIndex(Offer) ? &Shop->Offers[Offer] : nullptr;
	// an ammunition line (owner 2026-10-09) gives rounds to the buyer's weapons instead of an item
	const bool bAmmoOffer = Line && Line->Ammo && !Line->Ammo->Lots.IsEmpty();
	const bool bValidOffer = Line && (bAmmoOffer || (Line->Item && Line->Item->ItemClass));
	const bool bInReach = Inventory && !(Health && Health->IsDead())
		&& FVector::Dist(Buyer->GetActorLocation(), GetActorLocation()) <= (Shop ? Shop->BuyReach : 0.0f);

	// rate limit: a flood of RPCs never buys twice in the same instant
	const double Now = GetWorld()->GetTimeSeconds();
	if (Buyer && Shop)
	{
		const double* Last = LastPurchase.Find(Buyer);
		if (Last && Now - *Last < Shop->BuyCooldown)
		{
			return EShopResult::TooFast;
		}
		LastPurchase.Add(Buyer, Now);
	}

	URunSubsystem* Run = URunSubsystem::Get(this);
	const int32 Price = bValidOffer ? FMath::Max(0, Shop->Offers[Offer].Price) : 0;
	const int32 Treasury = Run ? (Run->GetState().bActive ? Run->GetState().Treasury : MAX_int32) : 0;
	bool bHasRoom = false;
	if (bValidOffer && Inventory)
	{
		if (bAmmoOffer)
		{
			// room in the pouch of a weapon taking one of its lots
			for (const FAmmoLot& Lot : Line->Ammo->Lots)
			{
				bHasRoom |= Inventory->GetAmmoRoom(Lot.Kind) > 0;
			}
		}
		else
		{
			bool bReplaces = false;
			const int32 Slot = Inventory->ChooseSlotForPickup(Line->Item, bReplaces);
			bHasRoom = Slot != INDEX_NONE && !bReplaces;
		}
	}
	EShopResult Result = ShopRules::Judge(bValidOffer, bInReach, Price, Treasury, bHasRoom);
	if (Result != EShopResult::Bought)
	{
		UE_LOG(Logmonke_island, Log, TEXT("Shop %s: purchase %d by %s refused (%s)"), *GetName(), Offer, *GetNameSafe(Buyer), *UEnum::GetValueAsString(Result));
		return Result;
	}
	// pay first (a fresh run starts on a map without expedition), then give; refund if the item cannot spawn
	if (!Run || !Run->TrySpend(GetWorld(), Price))
	{
		return EShopResult::NotEnoughMoney;
	}
	if (bAmmoOffer)
	{
		int32 Given = 0;
		for (const FAmmoLot& Lot : Line->Ammo->Lots)
		{
			Given += Inventory->GiveAmmo(Lot.Kind, Lot.Count);
		}
		if (Given <= 0)
		{
			Run->AddMoney(GetWorld(), Price);
			return EShopResult::InventoryFull;
		}
		UE_LOG(Logmonke_island, Log, TEXT("Shop %s: %s bought %s (%d round(s)) for %d, treasury %d"), *GetName(), *GetNameSafe(Buyer),
			*GetNameSafe(Line->Ammo), Given, Price, Run->GetState().Treasury);
		return EShopResult::Bought;
	}
	if (!Inventory->GiveItem(Line->Item))
	{
		Run->AddMoney(GetWorld(), Price);
		return EShopResult::InventoryFull;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Shop %s: %s bought %s for %d, treasury %d"), *GetName(), *GetNameSafe(Buyer),
		*GetNameSafe(Line->Item), Price, Run->GetState().Treasury);
	return EShopResult::Bought;
}
