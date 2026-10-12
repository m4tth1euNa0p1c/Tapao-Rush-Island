// monke_island

#include "Weapons/IslanderInventoryComponent.h"
#include "Expedition/Core/ExpeditionGameState.h"
#include "Boats/BoatRiderComponent.h"
#include "Interaction/IslanderGrabComponent.h"
#include "Player/IslanderCharacter.h"
#include "Player/IslanderHealthComponent.h"
#include "Run/RunSubsystem.h"
#include "Shop/ShopkeeperActor.h"
#include "Weapons/FirearmWeapon.h"
#include "GameFramework/PlayerState.h"
#include "Weapons/InventoryGrid.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/InventoryPickup.h"
#include "monke_island.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "InputMappingContext.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

FOnInventoryScreenRequested UIslanderInventoryComponent::OnScreenRequested;
FOnShopRequested UIslanderInventoryComponent::OnShopRequested;
FOnShopResult UIslanderInventoryComponent::OnShopResult;
FOnInventoryNotice UIslanderInventoryComponent::OnNotice;

UIslanderInventoryComponent::UIslanderInventoryComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UIslanderInventoryComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UIslanderInventoryComponent, Slots);
	DOREPLIFETIME(UIslanderInventoryComponent, ActiveSlot);
}

void UIslanderInventoryComponent::BeginPlay()
{
	Super::BeginPlay();

	if (GetOwner()->HasAuthority())
	{
		SpawnStartingItems();
		ApplyActiveSlot(StartingActiveSlot);
		// ADR-007: the equipment of the day before comes back once the pawn has its player
		if (APawn* Pawn = Cast<APawn>(GetOwner()))
		{
			Pawn->ReceiveControllerChangedDelegate.AddUniqueDynamic(this, &UIslanderInventoryComponent::HandleControllerChanged);
			RestoreRunLoadout();
		}
	}
	TryBindInput();
}

void UIslanderInventoryComponent::HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController)
{
	RestoreRunLoadout();
}

void UIslanderInventoryComponent::RestoreRunLoadout()
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	const APlayerState* PlayerState = Pawn ? Pawn->GetPlayerState() : nullptr;
	if (bRunLoadoutChecked || !PlayerState || !GetOwner()->HasAuthority())
	{
		return;
	}
	bRunLoadoutChecked = true;
	const URunSubsystem* Run = URunSubsystem::Get(this);
	if (const FRunLoadout* Loadout = Run ? Run->FindLoadout(URunSubsystem::PlayerKey(PlayerState)) : nullptr)
	{
		RestoreLoadout(*Loadout);
	}
}

void UIslanderInventoryComponent::RestoreLoadout(const FRunLoadout& Loadout)
{
	if (!GetOwner()->HasAuthority())
	{
		return;
	}
	for (FInventorySlot& Slot : Slots)
	{
		if (IsValid(Slot.Item))
		{
			Slot.Item->Destroy();
		}
		Slot = FInventorySlot();
	}
	int32 Valued = 0;
	for (const FRunLoadoutItem& Entry : Loadout.Items)
	{
		// a cell a damaged file names twice keeps the first item: the second is never spawned (no orphan item actor)
		if (!Slots.IsValidIndex(Entry.Slot) || !Slots[Entry.Slot].IsEmpty() || Entry.Count <= 0)
		{
			continue;
		}
		const UInventoryItemDefinition* Definition = Entry.Definition.LoadSynchronous();
		if (!Definition || !Definition->ItemClass)
		{
			continue;
		}
		AInventoryItemActor* Item = AInventoryItemActor::SpawnFromDefinition(Definition, GetOwner());
		if (!Item)
		{
			continue;
		}
		if (AFirearmWeapon* Firearm = Cast<AFirearmWeapon>(Item); Firearm && Entry.LoadedRounds >= 0)
		{
			Firearm->SetRounds(Entry.LoadedRounds, Entry.ReserveRounds);
		}
		// a mob weapon keeps the value it had on the island before: the multiplier of the zone it was found in is in it, the
		// zone of this island is never applied on top when it is dropped here (T-314, network review R-39)
		int32 KeptBase = 0;
		int32 Kept = 0;
		if (Entry.GetKeptValue(KeptBase, Kept))
		{
			Item->KeepValue(KeptBase, Kept);
			++Valued;
		}
		Slots[Entry.Slot].Item = Item;
		Slots[Entry.Slot].Definition = Definition;
		Slots[Entry.Slot].Count = FMath::Min(Entry.Count, UInventoryItemDefinition::GetMaxStack(Definition));
	}
	UE_LOG(Logmonke_island, Log, TEXT("Inventory: %s gets back its %d item(s) of the day before (%d with a loot value)"), *GetNameSafe(GetOwner()),
		Loadout.Items.Num(), Valued);
	ApplyActiveSlot(Loadout.ActiveSlot);
}

FRunLoadout UIslanderInventoryComponent::ExportLoadout() const
{
	FRunLoadout Loadout;
	Loadout.ActiveSlot = ActiveSlot;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		const FInventorySlot& Slot = Slots[Index];
		if (Slot.IsEmpty())
		{
			continue;
		}
		FRunLoadoutItem& Entry = Loadout.Items.AddDefaulted_GetRef();
		Entry.Slot = Index;
		Entry.Definition = const_cast<UInventoryItemDefinition*>(Slot.Definition.Get());
		Entry.Count = Slot.Count;
		if (const AFirearmWeapon* Firearm = Cast<AFirearmWeapon>(Slot.Item))
		{
			Entry.LoadedRounds = Firearm->GetLoadedRounds();
			Entry.ReserveRounds = Firearm->GetReserveRounds();
		}
		// the value a mob weapon carries (T-309) goes to the next island with it (T-314)
		int32 KeptBase = 0;
		int32 Kept = 0;
		if (IsValid(Slot.Item) && Slot.Item->GetKeptValue(KeptBase, Kept))
		{
			Entry.SetKeptValue(KeptBase, Kept);
		}
	}
	return Loadout;
}

bool UIslanderInventoryComponent::GiveItem(const UInventoryItemDefinition* Definition)
{
	if (!GetOwner()->HasAuthority() || !Definition || !Definition->ItemClass)
	{
		return false;
	}
	// a stack of the same item with room, else the first empty cell (bar, then bag); never in place of the hand
	int32 Target = INDEX_NONE;
	for (int32 Index = 0; Index < Slots.Num() && Target == INDEX_NONE; ++Index)
	{
		if (Slots[Index].Definition == Definition && Slots[Index].Count < UInventoryItemDefinition::GetMaxStack(Definition))
		{
			Target = Index;
		}
	}
	if (Target == INDEX_NONE)
	{
		Target = FindFreeSlot();
	}
	if (Target == INDEX_NONE)
	{
		return false;
	}
	if (!Slots[Target].IsEmpty())
	{
		++Slots[Target].Count;
		NotifyInventoryChanged();
		return true;
	}
	AInventoryItemActor* Item = AInventoryItemActor::SpawnFromDefinition(Definition, GetOwner());
	if (!Item)
	{
		return false;
	}
	Slots[Target].Item = Item;
	Slots[Target].Definition = Definition;
	Slots[Target].Count = 1;
	// bare hands with a free bar cell: the new item comes to hand
	ApplyActiveSlot(ActiveSlot == INDEX_NONE && Target < GetBarNum() ? Target : ActiveSlot);
	return true;
}

bool UIslanderInventoryComponent::RemoveItem(AInventoryItemActor* Item)
{
	if (!GetOwner()->HasAuthority() || !Item)
	{
		return false;
	}
	const int32 Index = Slots.IndexOfByPredicate([Item](const FInventorySlot& Slot) { return Slot.Item == Item; });
	if (Index == INDEX_NONE)
	{
		return false;
	}
	if (Slots[Index].Count > 1)
	{
		// a stack keeps its actor for the others
		--Slots[Index].Count;
		NotifyInventoryChanged();
		return true;
	}
	Slots[Index] = FInventorySlot();
	ApplyActiveSlot(ActiveSlot == Index ? INDEX_NONE : ActiveSlot);
	Item->Destroy();
	return true;
}

void UIslanderInventoryComponent::RequestBuy(AShopkeeperActor* Shop, int32 Offer)
{
	if (GetOwner()->HasAuthority())
	{
		ServerBuy_Implementation(Shop, Offer);
		return;
	}
	ServerBuy(Shop, Offer);
}

void UIslanderInventoryComponent::ServerBuy_Implementation(AShopkeeperActor* Shop, int32 Offer)
{
	const EShopResult Result = Shop ? Shop->Sell(Cast<APawn>(GetOwner()), Offer) : EShopResult::TooFar;
	ClientShopResult(Result, Offer);
}

void UIslanderInventoryComponent::ClientShopResult_Implementation(EShopResult Result, int32 Offer)
{
	OnShopResult.Broadcast(this, Result, Offer);
}

void UIslanderInventoryComponent::ClientOpenShop_Implementation(AShopkeeperActor* Shop)
{
	OnShopRequested.Broadcast(this, Shop);
}

void UIslanderInventoryComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	GetWorld()->GetTimerManager().ClearTimer(BindTimer);
	if (GetOwner()->HasAuthority())
	{
		for (const FInventorySlot& Slot : Slots)
		{
			if (IsValid(Slot.Item))
			{
				Slot.Item->Destroy();
			}
		}
		Slots.Reset();
	}
	Super::EndPlay(EndPlayReason);
}

void UIslanderInventoryComponent::SpawnStartingItems()
{
	Slots.Init(FInventorySlot(), FMath::Max(SlotCount, 1) + FMath::Max(BagSlotCount, 0));
	const UWorld* World = GetWorld();
	if (bNoStartingItemsInExpedition && World && World->GetGameState<AExpeditionGameState>())
	{
		// an expedition: empty-handed (the equipment of the day before is restored apart, RestoreRunLoadout)
		NotifyInventoryChanged();
		return;
	}
	for (int32 Slot = 0; Slot < Slots.Num() && Slot < StartingItems.Num(); ++Slot)
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
		if (AInventoryItemActor* Item = AInventoryItemActor::SpawnFromDefinition(Definition, GetOwner()))
		{
			Slots[Slot].Item = Item;
			Slots[Slot].Definition = Definition;
			Slots[Slot].Count = 1;
		}
	}
	NotifyInventoryChanged();
}

AInventoryItemActor* UIslanderInventoryComponent::GetItem(int32 Slot) const
{
	return Slots.IsValidIndex(Slot) ? Slots[Slot].Item.Get() : nullptr;
}

int32 UIslanderInventoryComponent::FindSlotOfItem(const AInventoryItemActor* Item) const
{
	return Item ? Slots.IndexOfByPredicate([Item](const FInventorySlot& Slot) { return Slot.Item == Item; }) : INDEX_NONE;
}

// ---------------------------------------------------------------- ammunition (owner 2026-10-09)

void UIslanderInventoryComponent::CollectFirearmsForAmmo(EAmmoKind Kind, TArray<AFirearmWeapon*>& OutWeapons) const
{
	OutWeapons.Reset();
	if (Kind == EAmmoKind::None)
	{
		return;
	}
	// the weapon in hand is served first, then the cells in order
	if (AFirearmWeapon* InHand = Cast<AFirearmWeapon>(GetEquippedItem()); InHand && InHand->GetAmmoKind() == Kind)
	{
		OutWeapons.Add(InHand);
	}
	for (const FInventorySlot& Slot : Slots)
	{
		AFirearmWeapon* Firearm = Cast<AFirearmWeapon>(Slot.Item);
		if (Firearm && Firearm->GetAmmoKind() == Kind && !OutWeapons.Contains(Firearm))
		{
			OutWeapons.Add(Firearm);
		}
	}
}

bool UIslanderInventoryComponent::HasFirearmFor(EAmmoKind Kind) const
{
	TArray<AFirearmWeapon*> Weapons;
	CollectFirearmsForAmmo(Kind, Weapons);
	return !Weapons.IsEmpty();
}

int32 UIslanderInventoryComponent::GetAmmoRoom(EAmmoKind Kind) const
{
	TArray<AFirearmWeapon*> Weapons;
	CollectFirearmsForAmmo(Kind, Weapons);
	int32 Room = 0;
	for (const AFirearmWeapon* Firearm : Weapons)
	{
		Room += Firearm->GetAmmoRoom();
	}
	return Room;
}

int32 UIslanderInventoryComponent::GiveAmmo(EAmmoKind Kind, int32 Count)
{
	if (!GetOwner()->HasAuthority() || Count <= 0)
	{
		return 0;
	}
	TArray<AFirearmWeapon*> Weapons;
	CollectFirearmsForAmmo(Kind, Weapons);
	TArray<int32> Rooms;
	for (const AFirearmWeapon* Firearm : Weapons)
	{
		Rooms.Add(Firearm->GetAmmoRoom());
	}
	TArray<int32> Given;
	const int32 Taken = AmmoMath::Distribute(Rooms, Count, Given);
	for (int32 Index = 0; Index < Weapons.Num(); ++Index)
	{
		if (Given[Index] > 0)
		{
			Weapons[Index]->AddAmmo(Kind, Given[Index]);
		}
	}
	if (Taken > 0)
	{
		// the HUD counters read the weapons; the cells did not change but the listeners refresh on it
		NotifyInventoryChanged();
	}
	return Taken;
}

void UIslanderInventoryComponent::Notice(const FText& Text)
{
	const APawn* Pawn = Cast<APawn>(GetOwner());
	if (!GetOwner()->HasAuthority() || !Pawn)
	{
		return;
	}
	if (Pawn->IsLocallyControlled())
	{
		// listen host: no RPC to itself
		ClientNotice_Implementation(Text);
	}
	else
	{
		ClientNotice(Text);
	}
}

void UIslanderInventoryComponent::ClientNotice_Implementation(const FText& Text)
{
	UE_LOG(Logmonke_island, Log, TEXT("Inventory: notice for %s: %s"), *GetNameSafe(GetOwner()), *Text.ToString());
	OnNotice.Broadcast(this, Text);
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
		ChooseSlot(Slot);
	}
	else
	{
		ServerSelectSlot(Slot);
	}
}

void UIslanderInventoryComponent::ServerSelectSlot_Implementation(int32 Slot)
{
	// a flood of slot keys equipped and put away items for the whole crew to replicate (security review T-269 M1)
	if (!SlotRequestLimiter.Consume(GetWorld()->GetTimeSeconds(), SlotRequestBurst, SlotRequestsPerSecond))
	{
		UE_LOG(Logmonke_island, Verbose, TEXT("Inventory: %s: slot key refused, more than %.0f requests at once (%d refused so far)"),
			*GetNameSafe(GetOwner()), SlotRequestBurst, SlotRequestLimiter.Refused);
		return;
	}
	ChooseSlot(Slot);
}

void UIslanderInventoryComponent::ChooseSlot(int32 Slot)
{
	// keys 1 to N reach the quick bar only
	if (Slot < 0 || Slot >= GetBarNum())
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
	ActiveSlot = Slot >= 0 && Slot < GetBarNum() && GetItem(Slot) ? Slot : -1;
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (AInventoryItemActor* Item = Slots[Index].Item)
		{
			Item->SetEquipped(Index == ActiveSlot);
		}
	}
	NotifyInventoryChanged();
}

void UIslanderInventoryComponent::TryBindInput()
{
	APawn* Pawn = Cast<APawn>(GetOwner());
	if (bInputBound || !Pawn || (SlotActions.Num() == 0 && !DropItemAction && !InventoryScreenAction))
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
	if (DropItemAction)
	{
		Input->BindAction(DropItemAction, ETriggerEvent::Started, this, &UIslanderInventoryComponent::HandleDropAction);
	}
	if (InventoryScreenAction)
	{
		Input->BindAction(InventoryScreenAction, ETriggerEvent::Started, this, &UIslanderInventoryComponent::HandleInventoryScreenAction);
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

void UIslanderInventoryComponent::HandleInventoryScreenAction()
{
	// the screen is drawn by the UI (UTapaoUISubsystem); the gameplay code does not know it
	OnScreenRequested.Broadcast(this);
}

// ---------------------------------------------------------------- pick up and drop (T-157)

void UIslanderInventoryComponent::NotifyInventoryChanged()
{
	OnInventoryChanged.Broadcast();
	OnInventoryChangedNative.Broadcast();
}

void UIslanderInventoryComponent::OnRep_Slots()
{
	OnInventoryChanged.Broadcast();
	OnInventoryChangedNative.Broadcast();
}

void UIslanderInventoryComponent::OnRep_ActiveSlot()
{
	OnInventoryChanged.Broadcast();
	OnInventoryChangedNative.Broadcast();
}

int32 UIslanderInventoryComponent::FindFreeSlot() const
{
	for (int32 Slot = 0; Slot < Slots.Num(); ++Slot)
	{
		if (Slots[Slot].IsEmpty())
		{
			return Slot;
		}
	}
	return INDEX_NONE;
}

int32 UIslanderInventoryComponent::ChoosePickupSlot(const TArray<bool>& Occupied, int32 InActiveSlot, bool& bOutReplaces)
{
	bOutReplaces = false;
	const int32 Free = Occupied.IndexOfByKey(false);
	if (Free != INDEX_NONE)
	{
		return Free;
	}
	if (Occupied.IsValidIndex(InActiveSlot))
	{
		bOutReplaces = true;
		return InActiveSlot;
	}
	return INDEX_NONE;
}

int32 UIslanderInventoryComponent::ChooseSlotForPickup(bool& bOutReplaces) const
{
	TArray<bool> Occupied;
	Occupied.Reserve(Slots.Num());
	for (const FInventorySlot& Slot : Slots)
	{
		Occupied.Add(!Slot.IsEmpty());
	}
	return ChoosePickupSlot(Occupied, GetItem(ActiveSlot) ? ActiveSlot : INDEX_NONE, bOutReplaces);
}

int32 UIslanderInventoryComponent::ChooseSlotForPickup(const UInventoryItemDefinition* Definition, bool& bOutReplaces) const
{
	bOutReplaces = false;
	const int32 Placement = InventoryGrid::FindPlacement(BuildCells(), Definition, GetBarNum());
	if (Placement != INDEX_NONE)
	{
		return Placement;
	}
	// full: the item in hand gives way (it falls where the new one lay); bare hands refuse
	if (GetItem(ActiveSlot))
	{
		bOutReplaces = true;
		return ActiveSlot;
	}
	return INDEX_NONE;
}

namespace
{
	/** Eyes of the owner: the first person camera of the islander, the pawn view point otherwise */
	FVector GetInventoryOwnerEyes(const AActor* Owner)
	{
		const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Owner);
		if (Islander && Islander->GetFirstPersonCamera())
		{
			return Islander->GetFirstPersonCamera()->GetComponentLocation();
		}
		const APawn* Pawn = Cast<APawn>(Owner);
		return Pawn ? Pawn->GetPawnViewLocation() : Owner->GetActorLocation();
	}
}

bool UIslanderInventoryComponent::CanPickUp(const AInventoryPickup* Pickup) const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !IsValid(Pickup) || Pickup->IsClaimed() || Pickup->IsActorBeingDestroyed() || Slots.Num() == 0)
	{
		return false;
	}
	const UInventoryItemDefinition* Definition = Pickup->GetDefinition();
	if (!Definition || !Definition->ItemClass)
	{
		return false;
	}
	// a dead, ragdolled or rowing player has no free hand
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(Owner); Islander && Islander->IsRagdoll())
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = Owner->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
	{
		return false;
	}
	if (const UBoatRiderComponent* Rider = Owner->FindComponentByClass<UBoatRiderComponent>(); Rider && Rider->IsSeated())
	{
		return false;
	}

	// reach and sight from the real positions, never from a point sent by a client (cahier §4)
	const FVector Eyes = GetInventoryOwnerEyes(Owner);
	const FBox Bounds = Pickup->GetComponentsBoundingBox(true);
	if (!Bounds.IsValid || Bounds.ComputeSquaredDistanceToPoint(Eyes) > FMath::Square(PickupReach))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InventoryPickupSight), false, Owner);
	return !GetWorld()->LineTraceSingleByChannel(Hit, Eyes, Bounds.GetCenter(), ECC_Visibility, Params) || Hit.GetActor() == Pickup;
}

bool UIslanderInventoryComponent::PickUp(AInventoryPickup* Pickup)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !CanPickUp(Pickup))
	{
		return false;
	}
	const UInventoryItemDefinition* Definition = Pickup->GetDefinition();
	bool bReplaces = false;
	const int32 Slot = ChooseSlotForPickup(Definition, bReplaces);
	if (Slot == INDEX_NONE)
	{
		return false;
	}

	const FTransform Where = Pickup->GetActorTransform();
	AInventoryItemActor* Item = Pickup->Claim(Owner);
	if (!Item)
	{
		return false;
	}

	if (bReplaces)
	{
		// the stack in hand falls where the picked one lay
		const FInventorySlot Previous = Slots[Slot];
		Slots[Slot] = FInventorySlot();
		const FVector Up(0.0, 0.0, DropUpSpeed);
		if (Previous.Item && !SpawnPickupFor(Previous.Item, Where, Up))
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Inventory: %s could not be dropped, destroyed"), *Previous.Item->GetName());
			Previous.Item->Destroy();
		}
		SpawnDefinitionPickups(Previous.Definition, Previous.Count - 1, Where, Up);
	}

	if (!Slots[Slot].IsEmpty())
	{
		// joins a stack of the same item: the cell keeps its own actor
		++Slots[Slot].Count;
		Item->Destroy();
		Item = Slots[Slot].Item;
	}
	else
	{
		Slots[Slot].Item = Item;
		Slots[Slot].Definition = Definition;
		Slots[Slot].Count = 1;
	}
	UE_LOG(Logmonke_island, Log, TEXT("Inventory: %s picks up %s in slot %d%s"), *Owner->GetName(), *GetNameSafe(Item), Slot + 1,
		bReplaces ? TEXT(" (swap)") : TEXT(""));

	const UBoatRiderComponent* Rider = Owner->FindComponentByClass<UBoatRiderComponent>();
	const bool bSeated = Rider && Rider->IsSeated();
	if (bReplaces || (ActiveSlot == INDEX_NONE && bEquipPickupWhenBareHanded && !bSeated && Slot < GetBarNum()))
	{
		ApplyActiveSlot(Slot);
	}
	else
	{
		ApplyActiveSlot(ActiveSlot);
	}
	return true;
}

AInventoryPickup* UIslanderInventoryComponent::SpawnPickupFor(AInventoryItemActor* Item, const FTransform& Transform, const FVector& Velocity) const
{
	const UInventoryItemDefinition* Definition = Item ? Item->GetDefinition() : nullptr;
	if (!Definition)
	{
		return nullptr;
	}
	const TSubclassOf<AInventoryPickup> Class = Definition->PickupClass ? Definition->PickupClass : PickupClass;
	return AInventoryPickup::SpawnForItem(GetWorld(), Class, Item, Transform, Velocity);
}

void UIslanderInventoryComponent::SpawnDefinitionPickups(const UInventoryItemDefinition* Definition, int32 Count, const FTransform& Transform,
	const FVector& Velocity) const
{
	if (!Definition || Count <= 0)
	{
		return;
	}
	const TSubclassOf<AInventoryPickup> Class = Definition->PickupClass ? Definition->PickupClass : PickupClass;
	// a stack is bounded by MaxStack (999 at most): each unit is its own pickup, a little apart so they do not overlap
	for (int32 Index = 0; Index < Count; ++Index)
	{
		FTransform Spot = Transform;
		Spot.AddToTranslation(FVector(0.0, 0.0, 8.0 * (Index + 1)));
		AInventoryPickup* Pickup = AInventoryPickup::SpawnForDefinition(GetWorld(), Class, const_cast<UInventoryItemDefinition*>(Definition), Spot);
		if (Pickup && Pickup->GetMesh() && Pickup->GetMesh()->IsSimulatingPhysics())
		{
			Pickup->GetMesh()->SetPhysicsLinearVelocity(Velocity);
		}
	}
}

FTransform UIslanderInventoryComponent::GetDropTransform(FVector& OutVelocity, const TOptional<FVector>& TossVelocity) const
{
	const AActor* Owner = GetOwner();
	const APawn* Pawn = Cast<APawn>(Owner);
	const FRotator View = Pawn ? Pawn->GetBaseAimRotation() : Owner->GetActorRotation();
	const FVector Forward = FRotator(0.0f, View.Yaw, 0.0f).Vector();
	const FVector Body = Owner->GetActorLocation() + FVector(0.0, 0.0, DropHeight);
	FVector Target = Body + Forward * DropForwardDistance;

	// short of a wall: never through it
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(InventoryDrop), false, Owner);
	if (GetWorld()->LineTraceSingleByChannel(Hit, Body, Target, ECC_Visibility, Params))
	{
		Target = Hit.Location - Forward * 15.0;
	}

	const FVector Inherited = Owner->GetVelocity();
	const FVector Toss = TossVelocity.IsSet() ? TossVelocity.GetValue() : Forward * DropSpeed + FVector(0.0, 0.0, DropUpSpeed);
	OutVelocity = (Toss + Inherited).GetClampedToMaxSize(MaxDropSpeed);
	// the long axis of the item across the view, as a weapon let go of
	return FTransform(FRotator(0.0f, View.Yaw + 90.0f, 0.0f), Target);
}

AInventoryPickup* UIslanderInventoryComponent::DropSlot(int32 Slot, int32 Count, const TOptional<FVector>& TossVelocity)
{
	AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority() || !Slots.IsValidIndex(Slot) || Slots[Slot].IsEmpty() || !Slots[Slot].Item)
	{
		return nullptr;
	}
	FInventorySlot& Cell = Slots[Slot];
	Count = FMath::Clamp(Count, 1, Cell.Count);
	FVector Velocity;
	const FTransform Where = GetDropTransform(Velocity, TossVelocity);

	if (Count < Cell.Count)
	{
		// part of a stack: the cell keeps its actor, the units leave as pickups of the definition
		SpawnDefinitionPickups(Cell.Definition, Count, Where, Velocity);
		Cell.Count -= Count;
		UE_LOG(Logmonke_island, Log, TEXT("Inventory: %s drops %d x %s from slot %d"), *Owner->GetName(), Count, *GetNameSafe(Cell.Definition), Slot + 1);
		ApplyActiveSlot(ActiveSlot);
		return nullptr;
	}

	const FInventorySlot Dropped = Cell;
	Cell = FInventorySlot();
	AInventoryPickup* Pickup = SpawnPickupFor(Dropped.Item, Where, Velocity);
	if (!Pickup)
	{
		// no pickup, no loss: the item stays in its slot
		Slots[Slot] = Dropped;
		UE_LOG(Logmonke_island, Warning, TEXT("Inventory: %s could not be dropped (no definition or pickup class)"), *Dropped.Item->GetName());
		return nullptr;
	}
	SpawnDefinitionPickups(Dropped.Definition, Dropped.Count - 1, Where, Velocity);
	UE_LOG(Logmonke_island, Log, TEXT("Inventory: %s drops %s from slot %d"), *Owner->GetName(), *Dropped.Item->GetName(), Slot + 1);
	ApplyActiveSlot(Slot == ActiveSlot ? INDEX_NONE : ActiveSlot);
	return Pickup;
}

void UIslanderInventoryComponent::DropEquippedItem()
{
	if (GetOwner()->HasAuthority())
	{
		ServerDropEquippedItem_Implementation();
	}
	else
	{
		ServerDropEquippedItem();
	}
}

void UIslanderInventoryComponent::ServerDropEquippedItem_Implementation()
{
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastDropTime < DropCooldown || !GetEquippedItem())
	{
		return;
	}
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner()); Islander && Islander->IsRagdoll())
	{
		return;
	}
	LastDropTime = Now;
	DropSlot(ActiveSlot);
}

void UIslanderInventoryComponent::HandleDropAction()
{
	// G of the cahier lets go of the lifted object first; only free hands drop the item in hand
	if (UIslanderGrabComponent* Grab = GetOwner()->FindComponentByClass<UIslanderGrabComponent>(); Grab && Grab->IsHolding())
	{
		Grab->RequestRelease();
		return;
	}
	DropEquippedItem();
}

// ---------------------------------------------------------------- inventory screen (T-166)

TArray<FInventoryCell> UIslanderInventoryComponent::BuildCells() const
{
	TArray<FInventoryCell> Cells;
	Cells.SetNum(Slots.Num());
	for (int32 Index = 0; Index < Slots.Num(); ++Index)
	{
		if (!Slots[Index].IsEmpty())
		{
			Cells[Index].Definition = Slots[Index].Definition;
			Cells[Index].Count = Slots[Index].Count;
			Cells[Index].Source = Index;
		}
	}
	return Cells;
}

bool UIslanderInventoryComponent::CommitCells(const TArray<FInventoryCell>& Cells)
{
	if (Cells.Num() != Slots.Num() || !InventoryGrid::IsValid(Cells))
	{
		UE_LOG(Logmonke_island, Error, TEXT("Inventory: %s refused an invalid rearrangement"), *GetNameSafe(GetOwner()));
		return false;
	}

	// new stacks need their actor first: if one cannot be spawned, nothing changes
	TArray<FInventorySlot> NewSlots;
	NewSlots.SetNum(Cells.Num());
	TArray<AInventoryItemActor*> Spawned;
	for (int32 Index = 0; Index < Cells.Num(); ++Index)
	{
		const FInventoryCell& Cell = Cells[Index];
		if (Cell.IsEmpty())
		{
			continue;
		}
		NewSlots[Index].Definition = Cell.Definition;
		NewSlots[Index].Count = Cell.Count;
		if (Cell.Source != INDEX_NONE && Slots.IsValidIndex(Cell.Source))
		{
			NewSlots[Index].Item = Slots[Cell.Source].Item;
			continue;
		}
		AInventoryItemActor* Item = AInventoryItemActor::SpawnFromDefinition(Cell.Definition, GetOwner());
		if (!Item)
		{
			UE_LOG(Logmonke_island, Warning, TEXT("Inventory: no actor for a new stack of %s, rearrangement cancelled"), *GetNameSafe(Cell.Definition));
			for (AInventoryItemActor* Made : Spawned)
			{
				Made->Destroy();
			}
			return false;
		}
		Spawned.Add(Item);
		NewSlots[Index].Item = Item;
	}

	// an actor no cell keeps (a stack joined into another) goes
	TSet<const AInventoryItemActor*> Kept;
	for (const FInventorySlot& Slot : NewSlots)
	{
		Kept.Add(Slot.Item);
	}
	for (const FInventorySlot& Slot : Slots)
	{
		if (IsValid(Slot.Item) && !Kept.Contains(Slot.Item))
		{
			Slot.Item->Destroy();
		}
	}

	AInventoryItemActor* HandBefore = GetEquippedItem();
	Slots = MoveTemp(NewSlots);
	// the bar cell in hand stays selected (Minecraft); seated in the boat, a new item never comes to hand
	const UBoatRiderComponent* Rider = GetOwner()->FindComponentByClass<UBoatRiderComponent>();
	const bool bSeated = Rider && Rider->IsSeated();
	ApplyActiveSlot(bSeated && GetItem(ActiveSlot) != HandBefore ? INDEX_NONE : ActiveSlot);
	return true;
}

bool UIslanderInventoryComponent::ConsumeScreenRequest()
{
	const double Now = GetWorld()->GetTimeSeconds();
	const float Rate = FMath::Max(MaxScreenRequestsPerSecond, 1.0f);
	if (ScreenRequestTime < 0.0)
	{
		ScreenRequestTokens = Rate;
	}
	else
	{
		ScreenRequestTokens = FMath::Min(Rate, ScreenRequestTokens + static_cast<float>(Now - ScreenRequestTime) * Rate);
	}
	ScreenRequestTime = Now;
	if (ScreenRequestTokens < 1.0f)
	{
		return false;
	}
	ScreenRequestTokens -= 1.0f;
	return true;
}

bool UIslanderInventoryComponent::CanRearrange() const
{
	const AActor* Owner = GetOwner();
	if (!Owner || !Owner->HasAuthority())
	{
		return false;
	}
	if (const UIslanderHealthComponent* Health = Owner->FindComponentByClass<UIslanderHealthComponent>(); Health && Health->IsDead())
	{
		return false;
	}
	return true;
}

int32 UIslanderInventoryComponent::MoveItems(int32 From, int32 To, int32 Count)
{
	if (!CanRearrange())
	{
		return 0;
	}
	TArray<FInventoryCell> Cells = BuildCells();
	const int32 Moved = InventoryGrid::Move(Cells, From, To, Count);
	return Moved > 0 && CommitCells(Cells) ? Moved : 0;
}

int32 UIslanderInventoryComponent::QuickMoveItems(int32 From)
{
	if (!CanRearrange())
	{
		return 0;
	}
	TArray<FInventoryCell> Cells = BuildCells();
	const int32 Moved = InventoryGrid::QuickMove(Cells, From, GetBarNum());
	return Moved > 0 && CommitCells(Cells) ? Moved : 0;
}

int32 UIslanderInventoryComponent::GatherItems(int32 Into)
{
	if (!CanRearrange())
	{
		return 0;
	}
	TArray<FInventoryCell> Cells = BuildCells();
	const int32 Gathered = InventoryGrid::Gather(Cells, Into);
	return Gathered > 0 && CommitCells(Cells) ? Gathered : 0;
}

void UIslanderInventoryComponent::RequestMove(int32 From, int32 To, int32 Count)
{
	if (GetOwner()->HasAuthority())
	{
		ServerMove_Implementation(From, To, Count);
	}
	else
	{
		ServerMove(From, To, Count);
	}
}

void UIslanderInventoryComponent::RequestQuickMove(int32 From)
{
	if (GetOwner()->HasAuthority())
	{
		ServerQuickMove_Implementation(From);
	}
	else
	{
		ServerQuickMove(From);
	}
}

void UIslanderInventoryComponent::RequestGather(int32 Into)
{
	if (GetOwner()->HasAuthority())
	{
		ServerGather_Implementation(Into);
	}
	else
	{
		ServerGather(Into);
	}
}

void UIslanderInventoryComponent::RequestDrop(int32 Slot, int32 Count)
{
	if (GetOwner()->HasAuthority())
	{
		ServerDrop_Implementation(Slot, Count);
	}
	else
	{
		ServerDrop(Slot, Count);
	}
}

void UIslanderInventoryComponent::ServerMove_Implementation(int32 From, int32 To, int32 Count)
{
	if (ConsumeScreenRequest())
	{
		MoveItems(From, To, Count);
	}
}

void UIslanderInventoryComponent::ServerQuickMove_Implementation(int32 From)
{
	if (ConsumeScreenRequest())
	{
		QuickMoveItems(From);
	}
}

void UIslanderInventoryComponent::ServerGather_Implementation(int32 Into)
{
	if (ConsumeScreenRequest())
	{
		GatherItems(Into);
	}
}

void UIslanderInventoryComponent::ServerDrop_Implementation(int32 Slot, int32 Count)
{
	if (!ConsumeScreenRequest() || !CanRearrange() || Count <= 0)
	{
		return;
	}
	// the same limits as G: never while ragdolled, never faster than DropCooldown
	if (const AIslanderCharacter* Islander = Cast<AIslanderCharacter>(GetOwner()); Islander && Islander->IsRagdoll())
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	if (Now - LastDropTime < DropCooldown)
	{
		return;
	}
	LastDropTime = Now;
	DropSlot(Slot, Count);
}
