// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/RequestRateLimiter.h"
#include "Run/RunLoadout.h"
#include "Shop/ShopTypes.h"
#include "Weapons/AmmoTypes.h"
#include "IslanderInventoryComponent.generated.h"

class AFirearmWeapon;
class AInventoryItemActor;
class AInventoryPickup;
class AShopkeeperActor;
class AController;
class APawn;
class UInputAction;
class UInputMappingContext;
class UInventoryItemDefinition;
class UIslanderInventoryComponent;
struct FInventoryCell;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnInventoryChanged);
DECLARE_MULTICAST_DELEGATE(FOnInventoryChangedNative);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnInventoryScreenRequested, UIslanderInventoryComponent*);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnShopRequested, UIslanderInventoryComponent*, AShopkeeperActor*);
DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnShopResult, UIslanderInventoryComponent*, EShopResult, int32 /*Offer*/);
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnInventoryNotice, UIslanderInventoryComponent*, const FText&);

/** One replicated inventory cell (T-166): the item actor representing the stack, its definition and the count */
USTRUCT(BlueprintType)
struct FInventorySlot
{
	GENERATED_BODY()

	/** Actor of the stack (one per cell, whatever the count), null when empty */
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TObjectPtr<AInventoryItemActor> Item;

	/** What lies here (replicated with the cell: the UI never waits for the actor) */
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	TObjectPtr<const UInventoryItemDefinition> Definition;

	/** Identical items in the cell (0 = empty) */
	UPROPERTY(BlueprintReadOnly, Category="Inventory")
	int32 Count = 0;

	bool IsEmpty() const { return Count <= 0 || !Definition; }
};

/**
 *  Inventory of a player (T-157, T-166): a quick bar (keys 1 to 6, SlotCount) then a bag (BagSlotCount), one array;
 *  since the owner's request of 2026-10-07 the bar has 6 slots and there is no bag: E opens the row of the player.
 *  Equipment slots of a player (keys 1 to 4). The server spawns the starting items, owned by the character,
 *  and equips the starting slot (slot 1: Bean's musket). Pressing the key of the active slot puts the item
 *  away (bare hands, punch); pressing another slot equips it. Server-authoritative, the active slot replicates.
 *  Pick up and drop (T-157): F on an AInventoryPickup puts the item in the first free slot, or, when every slot is
 *  full, in place of the item in hand, which falls where the pickup was. G drops the item in hand in front of the
 *  player (the same actor, state kept); G still lets go of a lifted physics object first.
 *  Inventory screen (T-166, Minecraft-like, drawn by UTapaoUISubsystem): E opens it; every move of a cell goes through
 *  a server RPC (move, quick move, gather, drop) checked against the replicated cells: the client only proposes.
 *  The pickup fills a stack of the same item, then the bar, then the bag; full, it replaces the item in hand.
 */
UCLASS(ClassGroup=(Islander), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UIslanderInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UIslanderInventoryComponent();

	/** Number of quick bar slots (keys 1 to N), the first cells of the inventory */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1", ClampMax="9"))
	int32 SlotCount = 6;

	/** Number of bag cells, after the bar (shown by the inventory screen only); 0: the screen shows the bar alone */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0", ClampMax="54"))
	int32 BagSlotCount = 0;

	/** Columns of the bag on the inventory screen (18 cells in 6 columns: 3 rows) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1", ClampMax="12"))
	int32 BagColumns = 6;

	/** Server: most inventory screen requests one player may send per second (two quick clicks pass, a flood does not) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1"))
	float MaxScreenRequestsPerSecond = 20.0f;

	/** Server: slot key requests (1 to 6) one player may send at once: a player mashing the keys stays under it, the
	 *  flood of a modified client does not (security review T-269 M1, SEC-03); the server's own changes are not counted */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1"))
	float SlotRequestBurst = 10.0f;

	/** Server: slot key requests given back each second once SlotRequestBurst is spent */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="0.1"))
	float SlotRequestsPerSecond = 10.0f;

	/** Items given at spawn, by slot (index 0 = key 1); empty entries leave the slot free */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	TArray<TObjectPtr<UInventoryItemDefinition>> StartingItems;

	/** No StartingItems on an expedition map (owner 2026-10-07: the game gives nothing; the merchant and the islands
	 *  do); the test maps keep them. The equipment kept from the day before still comes back (ADR-007) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	bool bNoStartingItemsInExpedition = true;

	/** Slot equipped at spawn (0 = key 1, -1 = bare hands) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="-1"))
	int32 StartingActiveSlot = 0;

	/** Context of the slot keys, pushed for the local player */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> InventoryMappingContext;

	/** Priority of InventoryMappingContext: above the character's contexts (1) so G drops here instead of IA_Drop */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	int32 InventoryMappingPriority = 2;

	/** Inventory screen key (E, IA_InventoryScreen in InventoryMappingContext): opens the screen; the screen closes on it */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> InventoryScreenAction;

	/** One action per slot key (index 0 = key 1) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TArray<TObjectPtr<UInputAction>> SlotActions;

	/**
	 *  Drop key (G, IA_DropItem in InventoryMappingContext): lets go of the lifted object if any, otherwise drops the
	 *  item in hand. InventoryMappingPriority must be above the character's contexts so G is consumed here.
	 */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputAction> DropItemAction;

	/** Pickup spawned for a dropped item whose definition has no PickupClass (AInventoryPickup when empty) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Pickup")
	TSubclassOf<AInventoryPickup> PickupClass;

	/** Server check of the pickup reach, eyes to the closest point of the pickup (cm; 200 of cahier §4 + latency) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Pickup", meta=(ClampMin="0", Units="cm"))
	float PickupReach = 250.0f;

	/** Picking up with bare hands equips the item */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Pickup")
	bool bEquipPickupWhenBareHanded = true;

	/** Dropped item appears this far in front of the body (cm), closer when a wall is there */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(ClampMin="0", Units="cm"))
	float DropForwardDistance = 60.0f;

	/** Height of the drop point above the body centre (cm) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(Units="cm"))
	float DropHeight = 20.0f;

	/** Forward toss speed (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(ClampMin="0", Units="cm/s"))
	float DropSpeed = 220.0f;

	/** Upward toss speed (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(ClampMin="0", Units="cm/s"))
	float DropUpSpeed = 120.0f;

	/** Bound of the toss speed, the player's own velocity included (cm/s) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(ClampMin="0", Units="cm/s"))
	float MaxDropSpeed = 600.0f;

	/** Server: minimum time between two drops of the same player (rate limit of the RPC) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory|Drop", meta=(ClampMin="0", Units="s"))
	float DropCooldown = 0.25f;

	/** Any slot, the active one or an item changed (server and clients, after replication) */
	UPROPERTY(BlueprintAssignable, Category="Inventory")
	FOnInventoryChanged OnInventoryChanged;

	/** Same as OnInventoryChanged, for Slate widgets */
	FOnInventoryChangedNative OnInventoryChangedNative;

	/** The local player pressed the inventory screen key (the TAPAO UI opens or closes its screen) */
	static FOnInventoryScreenRequested OnScreenRequested;

	/** The server asks the local player to open a shop (the TAPAO shop screen listens) */
	static FOnShopRequested OnShopRequested;

	/** The server answered a purchase of the local player */
	static FOnShopResult OnShopResult;

	/** A short message for the local player (ammunition refused, pouch full...): the HUD may show it a few seconds */
	static FOnInventoryNotice OnNotice;

	// ---- ammunition (owner request 2026-10-09: AAmmoPickup, the merchant) ----

	/** Firearms of this inventory taking Kind, the one in hand first then the cells in order (every machine) */
	void CollectFirearmsForAmmo(EAmmoKind Kind, TArray<AFirearmWeapon*>& OutWeapons) const;

	/** A firearm taking Kind is carried */
	UFUNCTION(BlueprintPure, Category="Inventory|Ammo")
	bool HasFirearmFor(EAmmoKind Kind) const;

	/** Rounds of Kind the carried firearms can still take, all together */
	UFUNCTION(BlueprintPure, Category="Inventory|Ammo")
	int32 GetAmmoRoom(EAmmoKind Kind) const;

	/** Server: pours up to Count rounds of Kind into the pouches (the weapon in hand first); the number taken */
	int32 GiveAmmo(EAmmoKind Kind, int32 Count);

	/** Server: sends a short message to the owning player (OnNotice on its machine) */
	void Notice(const FText& Text);

	// ---- shop and run (ADR-007, owner request 2026-10-06) ----

	/** Buys an offer of a shop (owning client or server; the server checks reach, money and room) */
	void RequestBuy(AShopkeeperActor* Shop, int32 Offer);

	/** Server: asks the owning client to open the shop screen */
	UFUNCTION(Client, Reliable)
	void ClientOpenShop(AShopkeeperActor* Shop);

	/** Server: puts a new item in the first free cell (a stack of it first, bar then bag); false when full */
	bool GiveItem(const UInventoryItemDefinition* Definition);

	/** Server: takes an item out of the inventory and destroys it (an empty firearm); false when not carried here */
	bool RemoveItem(AInventoryItemActor* Item);

	/** What this inventory carries, to give it back on the next island (URunSubsystem): a mob weapon with its loot value (T-314) */
	FRunLoadout ExportLoadout() const;

	/**
	 *  Server: replaces what this inventory carries with a kept loadout (the equipment of the day before, ADR-007): the
	 *  firearms get their rounds back, a mob weapon the loot value it had, the zone multiplier never applied again (T-314)
	 */
	void RestoreLoadout(const FRunLoadout& Loadout);

	/** Equips a slot, or puts the item away if it is the active one (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void SelectSlot(int32 Slot);

	/** Active slot (-1 = bare hands) */
	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetActiveSlot() const { return ActiveSlot; }

	/** Item in a slot, null when empty */
	UFUNCTION(BlueprintPure, Category="Inventory")
	AInventoryItemActor* GetItem(int32 Slot) const;

	/** Item in the active slot, null with bare hands */
	UFUNCTION(BlueprintPure, Category="Inventory")
	AInventoryItemActor* GetEquippedItem() const { return GetItem(ActiveSlot); }

	/** Cell holding an item actor, INDEX_NONE when it is not carried here */
	int32 FindSlotOfItem(const AInventoryItemActor* Item) const;

	/** Number of cells, bar and bag (spawned items included) */
	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetSlotNum() const { return Slots.Num(); }

	/** Number of quick bar cells (the first ones) */
	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetBarNum() const { return FMath::Min(SlotCount, Slots.Num()); }

	/** A cell, empty when out of range */
	UFUNCTION(BlueprintPure, Category="Inventory")
	FInventorySlot GetSlot(int32 Slot) const { return Slots.IsValidIndex(Slot) ? Slots[Slot] : FInventorySlot(); }

	/** Items in a cell (0 when empty) */
	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 GetCount(int32 Slot) const { return Slots.IsValidIndex(Slot) ? Slots[Slot].Count : 0; }

	/** Every cell (bar, then bag) */
	const TArray<FInventorySlot>& GetSlots() const { return Slots; }

	// ---- inventory screen (owning client or server; the server checks everything) ----

	/** Moves Count items of a cell onto another (joins, splits or swaps, see InventoryGrid::Move) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void RequestMove(int32 From, int32 To, int32 Count);

	/** Sends a stack between the bar and the bag (shift + click) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void RequestQuickMove(int32 From);

	/** Gathers the identical items into a cell (double click) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void RequestGather(int32 Into);

	/** Drops Count items of a cell on the ground in front of the player (dragged out of the screen) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void RequestDrop(int32 Slot, int32 Count);

	/** Server: moves items between two cells; the number moved, 0 when refused */
	int32 MoveItems(int32 From, int32 To, int32 Count);

	/** Server: quick move of a cell; the number moved */
	int32 QuickMoveItems(int32 From);

	/** Server: gathers into a cell; the number gathered */
	int32 GatherItems(int32 Into);

	/** The cells as the pure rules see them (Source = own index) */
	TArray<FInventoryCell> BuildCells() const;

	/** First empty slot, INDEX_NONE when full */
	UFUNCTION(BlueprintPure, Category="Inventory")
	int32 FindFreeSlot() const;

	/**
	 *  Slot a picked up item goes to: the first free one; when full, the active slot (bOutReplaces, its item drops);
	 *  INDEX_NONE when full with bare hands
	 */
	int32 ChooseSlotForPickup(bool& bOutReplaces) const;

	/** Same, for a given item: a stack of it with room comes first, then the first empty cell (bar, then bag) */
	int32 ChooseSlotForPickup(const UInventoryItemDefinition* Definition, bool& bOutReplaces) const;

	/** Rule behind ChooseSlotForPickup, on the occupancy of each slot */
	static int32 ChoosePickupSlot(const TArray<bool>& Occupied, int32 ActiveSlot, bool& bOutReplaces);

	/** The owner can take this pickup now: item with a class, alive, not ragdolled nor seated, in reach and in sight */
	bool CanPickUp(const AInventoryPickup* Pickup) const;

	/** Server: takes the pickup's item into a slot (see ChooseSlotForPickup); false when refused or already taken */
	bool PickUp(AInventoryPickup* Pickup);

	/**
	 *  Server: drops Count items of a cell (all by default) as pickups in front of the player; the whole stack keeps
	 *  its actor and state, each other unit is a pickup of the definition. Null when the cell is empty.
	 *  TossVelocity (world, cm/s) replaces the forward toss (an empty firearm thrown to the right and down); the
	 *  player's own velocity is added either way.
	 */
	AInventoryPickup* DropSlot(int32 Slot, int32 Count = MAX_int32, const TOptional<FVector>& TossVelocity = TOptional<FVector>());

	/** Drops the item in hand (owning client or server) */
	UFUNCTION(BlueprintCallable, Category="Inventory")
	void DropEquippedItem();

	/** Server: damage that reaches the owner after the item in hand (a parry) */
	float ModifyIncomingDamage(float Damage, const AActor* DamageCauser) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server side of RequestBuy */
	UFUNCTION(Server, Reliable)
	void ServerBuy(AShopkeeperActor* Shop, int32 Offer);

	/** Answer of a purchase to the owning client */
	UFUNCTION(Client, Reliable)
	void ClientShopResult(EShopResult Result, int32 Offer);

	/** A short message of the server for the owning player (see Notice) */
	UFUNCTION(Client, Reliable)
	void ClientNotice(const FText& Text);

	/** Server: the pawn got its controller; its equipment of the day before replaces the default kit */
	UFUNCTION()
	void HandleControllerChanged(APawn* Pawn, AController* OldController, AController* NewController);

	/** Server side of SelectSlot */
	UFUNCTION(Server, Reliable)
	void ServerSelectSlot(int32 Slot);

	/** Server side of DropEquippedItem */
	UFUNCTION(Server, Reliable)
	void ServerDropEquippedItem();

	/** Server side of RequestMove */
	UFUNCTION(Server, Reliable)
	void ServerMove(int32 From, int32 To, int32 Count);

	/** Server side of RequestQuickMove */
	UFUNCTION(Server, Reliable)
	void ServerQuickMove(int32 From);

	/** Server side of RequestGather */
	UFUNCTION(Server, Reliable)
	void ServerGather(int32 Into);

	/** Server side of RequestDrop */
	UFUNCTION(Server, Reliable)
	void ServerDrop(int32 Slot, int32 Count);

	UFUNCTION()
	void OnRep_Slots();

	UFUNCTION()
	void OnRep_ActiveSlot();

private:

	/** Server: spawns the starting items */
	void SpawnStartingItems();

	/** Server: gives back the equipment URunSubsystem kept for the owner's player, once (ADR-007) */
	void RestoreRunLoadout();

	/** The equipment of the day before was looked for */
	bool bRunLoadoutChecked = false;

	/** Server: equips Slot (a bar cell with an item, otherwise bare hands) and puts every other item away */
	void ApplyActiveSlot(int32 Slot);

	/** Server: a slot key (the owner's, or the server's own: the boat seat, an emote), with the rules of the keys */
	void ChooseSlot(int32 Slot);

	/** Server: token bucket of the slot key requests of the owning client (SlotRequestBurst, SlotRequestsPerSecond) */
	FRequestRateLimiter SlotRequestLimiter;

	/**
	 *  Server: turns the cells of the pure rules back into slots: kept actors follow their Source, a new stack gets a
	 *  new actor, an actor no cell keeps is destroyed. False (nothing changed) when an actor could not be spawned.
	 */
	bool CommitCells(const TArray<FInventoryCell>& Cells);

	/** Server: one more inventory screen request allowed now (rate limit), false when flooding */
	bool ConsumeScreenRequest();

	/** Server: the owner may rearrange its cells now (alive, not ragdolled) */
	bool CanRearrange() const;

	/** Server: spawns Count pickups of a definition around a transform (the units of a stack beyond its actor) */
	void SpawnDefinitionPickups(const UInventoryItemDefinition* Definition, int32 Count, const FTransform& Transform, const FVector& Velocity) const;

	/** Inventory screen key pressed */
	void HandleInventoryScreenAction();

	/** Local player: binds the slot keys once the pawn has its input component (retried until then) */
	void TryBindInput();

	/** Key of slot N pressed */
	void HandleSlotAction(int32 Slot);

	/** Drop key pressed: lets go of the lifted object first, otherwise drops the item in hand */
	void HandleDropAction();

	/** Server: where a dropped item appears, in front of the owner and short of any wall; the forward toss, or
	 *  TossVelocity when set, plus the owner's own velocity, bounded by MaxDropSpeed */
	FTransform GetDropTransform(FVector& OutVelocity, const TOptional<FVector>& TossVelocity = TOptional<FVector>()) const;

	/** Server: puts an item in a pickup at a transform; false (item left as it was) when no pickup could spawn */
	AInventoryPickup* SpawnPickupFor(AInventoryItemActor* Item, const FTransform& Transform, const FVector& Velocity) const;

	/** Server: tells the server-side listeners (clients hear it from OnRep) */
	void NotifyInventoryChanged();

	/** Cells, bar then bag (an empty cell has no item), set by the server */
	UPROPERTY(ReplicatedUsing=OnRep_Slots)
	TArray<FInventorySlot> Slots;

	/** Active slot (-1 = bare hands) */
	UPROPERTY(ReplicatedUsing=OnRep_ActiveSlot)
	int32 ActiveSlot = -1;

	/** Server: world time of the last drop (rate limit) */
	double LastDropTime = -1.0e9;

	/** Server: requests the inventory screen may still send (token bucket, refilled at MaxScreenRequestsPerSecond) */
	float ScreenRequestTokens = 0.0f;

	/** Server: world time ScreenRequestTokens was last refilled */
	double ScreenRequestTime = -1.0;

	/** Slot keys are bound */
	bool bInputBound = false;

	/** Binding attempts so far */
	int32 BindAttempts = 0;

	FTimerHandle BindTimer;
};
