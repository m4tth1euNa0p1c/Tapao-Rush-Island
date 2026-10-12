// monke_island

#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "Components/BoxComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/DefaultPawn.h"
#include "GameFramework/WorldSettings.h"
#include "Interaction/IslanderInteractable.h"
#include "Items/NetRestComponent.h"
#include "Items/ValuableComponent.h"
#include "Weapons/AmmoPickup.h"
#include "Weapons/InventoryItemActor.h"
#include "Weapons/InventoryItemDefinition.h"
#include "Weapons/InventoryPickup.h"
#include "Weapons/IslanderInventoryComponent.h"
#include "Weapons/MobWeaponPickup.h"

// Run with: ./scripts/ue.ps1 test -Filter monke_island.Weapons.Inventory
BEGIN_DEFINE_SPEC(FInventorySpec, "monke_island.Weapons.Inventory",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)
	UWorld* World = nullptr;
	UInventoryItemDefinition* Definition = nullptr;
	UInventoryItemDefinition* StackDefinition = nullptr;
	ADefaultPawn* PlayerA = nullptr;
	ADefaultPawn* PlayerB = nullptr;
	UIslanderInventoryComponent* InventoryA = nullptr;
	UIslanderInventoryComponent* InventoryB = nullptr;

	/** A pawn with a 4 slot bar and a bag of 2 cells, empty hands (spawned before BeginPlay) */
	ADefaultPawn* MakePlayer(const FVector& Location, UIslanderInventoryComponent*& OutInventory)
	{
		ADefaultPawn* Pawn = World->SpawnActor<ADefaultPawn>(Location, FRotator::ZeroRotator);
		OutInventory = NewObject<UIslanderInventoryComponent>(Pawn, TEXT("Inventory"));
		OutInventory->SlotCount = 4;
		OutInventory->BagSlotCount = 2;
		OutInventory->StartingActiveSlot = -1;
		OutInventory->RegisterComponent();
		return Pawn;
	}

	/** A level pickup of the test definition (or another) between the two players */
	AInventoryPickup* MakePickup(UInventoryItemDefinition* Of = nullptr)
	{
		AInventoryPickup* Pickup = AInventoryPickup::SpawnForDefinition(World, nullptr, Of ? Of : Definition, FTransform(FVector(0.0, 0.0, 60.0)));
		if (Pickup)
		{
			// held still: the rules are under test, not the fall
			Pickup->GetMesh()->SetSimulatePhysics(false);
		}
		return Pickup;
	}

	int32 CountItems(const UIslanderInventoryComponent* Inventory) const
	{
		int32 Count = 0;
		for (int32 Slot = 0; Slot < Inventory->GetSlotNum(); ++Slot)
		{
			Count += Inventory->GetItem(Slot) ? 1 : 0;
		}
		return Count;
	}

	int32 CountLiveActors(UClass* Class) const
	{
		int32 Count = 0;
		for (TActorIterator<AActor> It(World, Class); It; ++It)
		{
			Count += It->IsActorBeingDestroyed() ? 0 : 1;
		}
		return Count;
	}

	AInventoryPickup* FindPickupHolding(const AInventoryItemActor* Item) const
	{
		for (TActorIterator<AInventoryPickup> It(World); It; ++It)
		{
			if (!It->IsActorBeingDestroyed() && It->GetStoredItem() == Item)
			{
				return *It;
			}
		}
		return nullptr;
	}

	/** A mob weapon definition (a spear) whose pickup carries a loot value; the caller removes it from the root */
	static UInventoryItemDefinition* MakeSpearDefinition()
	{
		UInventoryItemDefinition* Spear = NewObject<UInventoryItemDefinition>();
		Spear->AddToRoot();
		Spear->DisplayName = FText::FromString(TEXT("Test spear"));
		Spear->ItemClass = AInventoryItemActor::StaticClass();
		Spear->PickupClass = AMobWeaponPickup::StaticClass();
		return Spear;
	}

	/** A spear lying between the two players, found in a zone at x1.5 (100 -> 150) then knocked about: 110 left */
	AMobWeaponPickup* MakeWornSpear(UInventoryItemDefinition* Spear)
	{
		AMobWeaponPickup* Lying = Cast<AMobWeaponPickup>(AInventoryPickup::SpawnForDefinition(World, AMobWeaponPickup::StaticClass(), Spear,
			FTransform(FVector(0.0, 0.0, 60.0))));
		if (Lying)
		{
			Lying->GetMesh()->SetSimulatePhysics(false);
			Lying->GetValuable()->RestoreValue(150, 150);
			Lying->GetValuable()->ApplyValueLoss(40);
		}
		return Lying;
	}
END_DEFINE_SPEC(FInventorySpec)

void FInventorySpec::Define()
{
	Describe("ChoosePickupSlot", [this]()
	{
		It("takes the first free slot, the active one only when every slot is full", [this]()
		{
			bool bReplaces = true;
			TestEqual(TEXT("empty -> slot 1"), UIslanderInventoryComponent::ChoosePickupSlot({ false, false, false, false }, -1, bReplaces), 0);
			TestFalse(TEXT("no swap"), bReplaces);
			TestEqual(TEXT("hole in slot 3"), UIslanderInventoryComponent::ChoosePickupSlot({ true, true, false, true }, 0, bReplaces), 2);
			TestFalse(TEXT("no swap with a hole"), bReplaces);
			TestEqual(TEXT("full -> slot in hand"), UIslanderInventoryComponent::ChoosePickupSlot({ true, true, true, true }, 1, bReplaces), 1);
			TestTrue(TEXT("swap"), bReplaces);
			TestEqual(TEXT("full, bare hands -> refused"), UIslanderInventoryComponent::ChoosePickupSlot({ true, true, true, true }, -1, bReplaces), INDEX_NONE);
			TestFalse(TEXT("refusal is no swap"), bReplaces);
		});
	});

	Describe("In a world", [this]()
	{
		BeforeEach([this]()
		{
			const UWorld::InitializationValues Init = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(true).ShouldSimulatePhysics(true)
				.CreateNavigation(false).CreateAISystem(false);
			World = UWorld::CreateWorld(EWorldType::Game, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Init);
			GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);

			Definition = NewObject<UInventoryItemDefinition>();
			Definition->AddToRoot();
			Definition->DisplayName = FText::FromString(TEXT("Test item"));
			Definition->ItemClass = AInventoryItemActor::StaticClass();

			StackDefinition = NewObject<UInventoryItemDefinition>();
			StackDefinition->AddToRoot();
			StackDefinition->DisplayName = FText::FromString(TEXT("Test cartridges"));
			StackDefinition->ItemClass = AInventoryItemActor::StaticClass();
			StackDefinition->MaxStack = 5;

			PlayerA = MakePlayer(FVector(-100.0, 0.0, 100.0), InventoryA);
			PlayerB = MakePlayer(FVector(100.0, 0.0, 100.0), InventoryB);
			PlayerB->SetActorRotation(FRotator(0.0f, 180.0f, 0.0f));
			World->InitializeActorsForPlay(FURL());
			World->GetWorldSettings()->NotifyBeginPlay();
		});

		AfterEach([this]()
		{
			World->EndPlay(EEndPlayReason::Quit);
			World->DestroyWorld(false);
			GEngine->DestroyWorldContext(World);
			World = nullptr;
			Definition->RemoveFromRoot();
			Definition = nullptr;
			StackDefinition->RemoveFromRoot();
			StackDefinition = nullptr;
			PlayerA = PlayerB = nullptr;
			InventoryA = InventoryB = nullptr;
		});

		It("puts a picked up item in the first free slot, equipped only with bare hands", [this]()
		{
			TestEqual(TEXT("4 bar cells + 2 bag cells"), InventoryA->GetSlotNum(), 6);
			TestEqual(TEXT("4 bar cells"), InventoryA->GetBarNum(), 4);
			AInventoryPickup* First = MakePickup();
			if (!TestNotNull(TEXT("pickup"), First)) { return; }
			TestTrue(TEXT("in reach and sight"), InventoryA->CanPickUp(First));
			TestTrue(TEXT("picked up"), InventoryA->PickUp(First));
			TestNotNull(TEXT("slot 1 filled"), InventoryA->GetItem(0));
			TestEqual(TEXT("bare hands -> equipped"), InventoryA->GetActiveSlot(), 0);
			TestTrue(TEXT("pickup claimed"), First->IsClaimed());
			TestTrue(TEXT("owned by the player"), InventoryA->GetItem(0)->GetOwner() == PlayerA);

			AInventoryPickup* Second = MakePickup();
			TestTrue(TEXT("second picked up"), InventoryA->PickUp(Second));
			TestNotNull(TEXT("slot 2 filled"), InventoryA->GetItem(1));
			TestEqual(TEXT("item in hand unchanged"), InventoryA->GetActiveSlot(), 0);
		});

		It("refuses a pickup out of reach", [this]()
		{
			AInventoryPickup* Far = MakePickup();
			if (!TestNotNull(TEXT("pickup"), Far)) { return; }
			Far->SetActorLocation(FVector(2000.0, 0.0, 60.0));
			TestFalse(TEXT("too far"), InventoryA->CanPickUp(Far));
			TestFalse(TEXT("refused"), InventoryA->PickUp(Far));
			TestFalse(TEXT("still there"), Far->IsClaimed());
		});

		It("fills the bar, then the bag, then replaces the item in hand; the old one falls as a pickup", [this]()
		{
			for (int32 Index = 0; Index < 6; ++Index)
			{
				InventoryA->PickUp(MakePickup());
			}
			if (!TestEqual(TEXT("full"), CountItems(InventoryA), 6)) { return; }
			TestNotNull(TEXT("the bag took the fifth"), InventoryA->GetItem(4));
			TestNotNull(TEXT("the bag took the sixth"), InventoryA->GetItem(5));
			TestFalse(TEXT("an item of the bag is never in hand"), InventoryA->GetItem(5)->IsEquipped());
			InventoryA->SelectSlot(4);
			TestEqual(TEXT("keys reach the bar only"), InventoryA->GetActiveSlot(), 0);
			InventoryA->SelectSlot(2);
			TestEqual(TEXT("slot 3 in hand"), InventoryA->GetActiveSlot(), 2);
			AInventoryItemActor* Old = InventoryA->GetItem(2);

			AInventoryPickup* Seventh = MakePickup();
			TestTrue(TEXT("swap accepted"), InventoryA->PickUp(Seventh));
			TestEqual(TEXT("still 6 items"), CountItems(InventoryA), 6);
			TestTrue(TEXT("new item in slot 3"), InventoryA->GetItem(2) != nullptr && InventoryA->GetItem(2) != Old);
			TestEqual(TEXT("new item in hand"), InventoryA->GetActiveSlot(), 2);
			TestTrue(TEXT("old item on the ground"), IsValid(Old) && Old->IsOnGround());
			TestNotNull(TEXT("old item in a pickup"), FindPickupHolding(Old));
		});

		It("drops the item in hand as a pickup that keeps the same actor and its state", [this]()
		{
			InventoryA->PickUp(MakePickup());
			AInventoryItemActor* Item = InventoryA->GetItem(0);
			if (!TestNotNull(TEXT("item"), Item)) { return; }

			InventoryA->DropEquippedItem();
			TestNull(TEXT("slot emptied"), InventoryA->GetItem(0));
			TestEqual(TEXT("bare hands"), InventoryA->GetActiveSlot(), -1);
			AInventoryPickup* Dropped = FindPickupHolding(Item);
			if (!TestNotNull(TEXT("pickup keeps the very same item"), Dropped)) { return; }
			TestTrue(TEXT("item hidden on the ground"), Item->IsOnGround() && Item->IsHidden());
			TestFalse(TEXT("not equipped"), Item->IsEquipped());
			TestTrue(TEXT("dropped in front of the player"), Dropped->GetActorLocation().X > PlayerA->GetActorLocation().X);

			Dropped->GetMesh()->SetSimulatePhysics(false);
			Dropped->SetActorLocation(FVector(0.0, 0.0, 60.0));
			TestTrue(TEXT("the other player takes it"), InventoryB->PickUp(Dropped));
			TestTrue(TEXT("same actor, state kept"), InventoryB->GetItem(0) == Item);
			TestTrue(TEXT("new carrier"), Item->GetOwner() == PlayerB);
			TestFalse(TEXT("shown again"), Item->IsOnGround() || Item->IsHidden());
		});

		It("gives one item only when two players press F on the same pickup in the same frame", [this]()
		{
			AInventoryPickup* Contested = MakePickup();
			if (!TestNotNull(TEXT("pickup"), Contested)) { return; }
			TestTrue(TEXT("A can"), IIslanderInteractable::Execute_CanInteract(Contested, PlayerA));
			TestTrue(TEXT("B can"), IIslanderInteractable::Execute_CanInteract(Contested, PlayerB));
			IIslanderInteractable::Execute_Interact(Contested, PlayerA);
			IIslanderInteractable::Execute_Interact(Contested, PlayerB);

			TestEqual(TEXT("one item between the two"), CountItems(InventoryA) + CountItems(InventoryB), 1);
			int32 ItemActors = 0;
			for (TActorIterator<AInventoryItemActor> It(World); It; ++It)
			{
				ItemActors += It->IsActorBeingDestroyed() ? 0 : 1;
			}
			TestEqual(TEXT("one item actor in the world"), ItemActors, 1);
			TestFalse(TEXT("B cannot take it any more"), InventoryB->PickUp(Contested));
		});

		It("moves, swaps and quick moves cells on the server, the same request twice never duplicating", [this]()
		{
			InventoryA->PickUp(MakePickup());
			InventoryA->PickUp(MakePickup());
			AInventoryItemActor* First = InventoryA->GetItem(0);
			AInventoryItemActor* Second = InventoryA->GetItem(1);
			if (!TestNotNull(TEXT("two items"), Second)) { return; }
			TestEqual(TEXT("first in hand"), InventoryA->GetActiveSlot(), 0);

			// two quick clicks: the same move twice
			TestEqual(TEXT("moved to the bag"), InventoryA->MoveItems(0, 4, 1), 1);
			TestEqual(TEXT("second request finds nothing"), InventoryA->MoveItems(0, 4, 1), 0);
			TestTrue(TEXT("same actor in the bag"), InventoryA->GetItem(4) == First);
			TestEqual(TEXT("hand emptied"), InventoryA->GetActiveSlot(), -1);
			TestFalse(TEXT("put away"), First->IsEquipped());

			TestEqual(TEXT("swap"), InventoryA->MoveItems(4, 1, 1), 1);
			TestTrue(TEXT("first in slot 2"), InventoryA->GetItem(1) == First);
			TestTrue(TEXT("second in the bag"), InventoryA->GetItem(4) == Second);
			TestEqual(TEXT("quick move back to the bar"), InventoryA->QuickMoveItems(4), 1);
			TestTrue(TEXT("first free bar cell"), InventoryA->GetItem(0) == Second);
			TestEqual(TEXT("refused out of range"), InventoryA->MoveItems(0, 99, 1), 0);
			TestEqual(TEXT("two items, two actors"), CountItems(InventoryA), 2);
			TestEqual(TEXT("two item actors in the world"), CountLiveActors(AInventoryItemActor::StaticClass()), 2);
		});

		It("stacks identical items in one cell, splits and joins them without duplicating", [this]()
		{
			for (int32 Index = 0; Index < 3; ++Index)
			{
				InventoryA->PickUp(MakePickup(StackDefinition));
			}
			TestEqual(TEXT("one cell of 3"), InventoryA->GetCount(0), 3);
			TestTrue(TEXT("nothing in slot 2"), InventoryA->GetSlot(1).IsEmpty());
			TestEqual(TEXT("one actor for the stack"), CountLiveActors(AInventoryItemActor::StaticClass()), 1);

			// right click: half (2) onto slot 2, then one back
			TestEqual(TEXT("split"), InventoryA->MoveItems(0, 1, 2), 2);
			TestEqual(TEXT("1 left"), InventoryA->GetCount(0), 1);
			TestEqual(TEXT("2 there"), InventoryA->GetCount(1), 2);
			TestEqual(TEXT("a new actor for the new stack"), CountLiveActors(AInventoryItemActor::StaticClass()), 2);
			TestEqual(TEXT("gather"), InventoryA->GatherItems(0), 2);
			TestEqual(TEXT("3 again"), InventoryA->GetCount(0), 3);
			TestEqual(TEXT("the joined stack's actor is gone"), CountLiveActors(AInventoryItemActor::StaticClass()), 1);

			// a part of the stack dropped: pickups of the definition, the cell keeps the rest
			const int32 PickupsBefore = CountLiveActors(AInventoryPickup::StaticClass());
			InventoryA->DropSlot(0, 2);
			TestEqual(TEXT("1 kept"), InventoryA->GetCount(0), 1);
			TestEqual(TEXT("2 pickups on the ground"), CountLiveActors(AInventoryPickup::StaticClass()) - PickupsBefore, 2);
		});

		It("keeps the value of a mob weapon taken then dropped again, never new again (T-309, network review R-39)", [this]()
		{
			UInventoryItemDefinition* Spear = MakeSpearDefinition();
			// found in a zone at x1.5 (100 -> 150), then knocked about: 110 left
			AMobWeaponPickup* Lying = MakeWornSpear(Spear);
			if (!TestNotNull(TEXT("mob weapon on the ground"), Lying))
			{
				Spear->RemoveFromRoot();
				return;
			}

			TestTrue(TEXT("picked up"), InventoryA->PickUp(Lying));
			AInventoryItemActor* Item = InventoryA->GetItem(0);
			int32 KeptBase = 0;
			int32 Kept = 0;
			TestTrue(TEXT("the item carries the value"), Item && Item->GetKeptValue(KeptBase, Kept));
			TestEqual(TEXT("base of its zone"), KeptBase, 150);
			TestEqual(TEXT("value left"), Kept, 110);

			InventoryA->DropEquippedItem();
			const AMobWeaponPickup* Dropped = Cast<AMobWeaponPickup>(FindPickupHolding(Item));
			if (TestNotNull(TEXT("dropped as a mob weapon"), Dropped))
			{
				TestEqual(TEXT("not new again"), Dropped->GetValuable()->GetValue(), 110);
				TestEqual(TEXT("base kept, no multiplier of the zone of the drop"), Dropped->GetValuable()->BaseValue, 150);
			}
			Spear->RemoveFromRoot();
		});

		It("keeps the value of a mob weapon from an island to the next (T-314, network review R-39)", [this]()
		{
			UInventoryItemDefinition* Spear = MakeSpearDefinition();
			AMobWeaponPickup* Lying = MakeWornSpear(Spear);
			if (!TestNotNull(TEXT("mob weapon on the ground"), Lying) || !TestTrue(TEXT("picked up"), InventoryA->PickUp(Lying)))
			{
				Spear->RemoveFromRoot();
				return;
			}

			// end of the day: what A carries is kept for the next island (URunSubsystem::KeepLoadoutOf)
			const FRunLoadout Kept = InventoryA->ExportLoadout();
			const FRunLoadoutItem* Entry = Kept.Items.FindByPredicate([](const FRunLoadoutItem& Item) { return Item.Slot == 0; });
			int32 KeptBase = 0;
			int32 KeptValue = 0;
			TestTrue(TEXT("the loadout keeps the value"), Entry && Entry->GetKeptValue(KeptBase, KeptValue) && KeptBase == 150 && KeptValue == 110);

			// the next island: the new pawn (B stands for it) gets the spear back, then drops it there
			InventoryB->RestoreLoadout(Kept);
			AInventoryItemActor* Restored = InventoryB->GetItem(0);
			if (!TestNotNull(TEXT("restored"), Restored))
			{
				Spear->RemoveFromRoot();
				return;
			}
			TestTrue(TEXT("the restored spear carries the value"), Restored->GetKeptValue(KeptBase, KeptValue) && KeptBase == 150 && KeptValue == 110);
			TestEqual(TEXT("in hand as on the island before"), InventoryB->GetActiveSlot(), 0);
			InventoryB->DropEquippedItem();
			const AMobWeaponPickup* Dropped = Cast<AMobWeaponPickup>(FindPickupHolding(Restored));
			if (TestNotNull(TEXT("dropped as a mob weapon"), Dropped))
			{
				TestEqual(TEXT("not new again on the next island"), Dropped->GetValuable()->GetValue(), 110);
				TestEqual(TEXT("base of the zone it was found in, no multiplier of this one"), Dropped->GetValuable()->BaseValue, 150);
			}
			Spear->RemoveFromRoot();
		});

		It("restores a loadout that names a cell twice with the first item only, never an orphan item (T-314)", [this]()
		{
			// a damaged run file: two entries for the first cell
			FRunLoadout Damaged;
			for (int32 Index = 0; Index < 2; ++Index)
			{
				FRunLoadoutItem& Entry = Damaged.Items.AddDefaulted_GetRef();
				Entry.Slot = 0;
				Entry.Definition = TSoftObjectPtr<UInventoryItemDefinition>(Definition);
			}
			InventoryB->RestoreLoadout(Damaged);
			TestNotNull(TEXT("the cell is filled"), InventoryB->GetItem(0));
			TestEqual(TEXT("one item actor in the world"), CountLiveActors(AInventoryItemActor::StaticClass()), 1);
		});

		It("frees a pickup glued to a moving base when it is lifted, and takes it away whole with F (T-316)", [this]()
		{
			// a deck that moves without simulating (the galleon's), under a weapon lying on it
			AActor* Deck = World->SpawnActor<AActor>(FVector::ZeroVector, FRotator::ZeroRotator);
			UBoxComponent* DeckBody = Deck ? NewObject<UBoxComponent>(Deck, TEXT("DeckBody")) : nullptr;
			if (!TestNotNull(TEXT("deck"), DeckBody))
			{
				return;
			}
			DeckBody->SetMobility(EComponentMobility::Movable);
			DeckBody->InitBoxExtent(FVector(150.0, 150.0, 5.0));
			Deck->SetRootComponent(DeckBody);
			DeckBody->RegisterComponent();
			AInventoryPickup* Lying = AInventoryPickup::SpawnForDefinition(World, nullptr, Definition, FTransform(FVector(0.0, 0.0, 60.0)));
			UNetRestComponent* Rest = Lying ? Lying->GetNetRest() : nullptr;
			if (!TestNotNull(TEXT("the pickup rests on the network"), Rest) || !TestTrue(TEXT("its body simulates"), Lying->GetMesh()->IsSimulatingPhysics()))
			{
				return;
			}

			Rest->GlueTo(DeckBody);
			if (!TestTrue(TEXT("glued to the deck"), Rest->IsGlued() && Rest->GetGlueBase() == DeckBody))
			{
				return;
			}
			TestFalse(TEXT("its movement is no longer sent: every machine carries it"), Lying->IsReplicatingMovement());

			// lifted with E: the grab makes it ignore the physics bodies (UIslanderGrabComponent::SetHeldGhost); the next frame frees it
			Lying->GetMesh()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Ignore);
			Rest->TickComponent(1.0f / 60.0f, LEVELTICK_All, nullptr);
			TestFalse(TEXT("freed once lifted"), Rest->IsGlued());
			TestTrue(TEXT("its movement is sent again"), Lying->IsReplicatingMovement());

			// put down, glued again, then taken with F: the whole pickup goes, the deck stays
			Lying->GetMesh()->SetCollisionResponseToChannel(ECC_PhysicsBody, ECR_Block);
			Rest->GlueTo(DeckBody);
			TestTrue(TEXT("glued again"), Rest->IsGlued());
			TestTrue(TEXT("taken with F"), InventoryA->PickUp(Lying));
			TestTrue(TEXT("the pickup is gone"), !IsValid(Lying) || Lying->IsActorBeingDestroyed());
			TestNotNull(TEXT("the item is carried"), InventoryA->GetItem(0));
			TestFalse(TEXT("the deck stays"), Deck->IsActorBeingDestroyed());
		});
	});

	Describe("Network", [this]()
	{
		It("rests the weapons and the ammunition on the ground like any free object: dormant at rest, glued to a moving base (T-316)", [this]()
		{
			const AInventoryPickup* Weapon = GetDefault<AInventoryPickup>();
			const AMobWeaponPickup* MobWeapon = GetDefault<AMobWeaponPickup>();
			const AAmmoPickup* Ammo = GetDefault<AAmmoPickup>();
			TestNotNull(TEXT("weapon"), Weapon->GetNetRest());
			TestNotNull(TEXT("mob weapon"), MobWeapon->GetNetRest());
			TestNotNull(TEXT("ammunition"), Ammo->GetNetRest());
			// the network rest needs the simulating body as the root, and hears at once when it wakes
			TestTrue(TEXT("weapon body"), Weapon->GetRootComponent() == Weapon->GetMesh() && Weapon->GetMesh()->BodyInstance.bGenerateWakeEvents);
			TestTrue(TEXT("ammunition body"), Ammo->GetRootComponent() == Ammo->GetMesh() && Ammo->GetMesh()->BodyInstance.bGenerateWakeEvents);
		});
	});
}

#endif
