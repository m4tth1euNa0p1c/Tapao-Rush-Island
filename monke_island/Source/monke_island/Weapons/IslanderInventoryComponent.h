// monke_island

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "IslanderInventoryComponent.generated.h"

class AInventoryItemActor;
class UInputAction;
class UInputMappingContext;
class UInventoryItemDefinition;

/**
 *  Equipment slots of a player (keys 1 to 4). The server spawns the starting items, owned by the character,
 *  and equips the starting slot (slot 1: Bean's musket). Pressing the key of the active slot puts the item
 *  away (bare hands, punch); pressing another slot equips it. Server-authoritative, the active slot replicates.
 */
UCLASS(ClassGroup=(Islander), meta=(BlueprintSpawnableComponent))
class MONKE_ISLAND_API UIslanderInventoryComponent : public UActorComponent
{
	GENERATED_BODY()

public:

	UIslanderInventoryComponent();

	/** Number of slots */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="1", ClampMax="9"))
	int32 SlotCount = 4;

	/** Items given at spawn, by slot (index 0 = key 1); empty entries leave the slot free */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory")
	TArray<TObjectPtr<UInventoryItemDefinition>> StartingItems;

	/** Slot equipped at spawn (0 = key 1, -1 = bare hands) */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Inventory", meta=(ClampMin="-1"))
	int32 StartingActiveSlot = 0;

	/** Context of the slot keys, pushed for the local player */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TObjectPtr<UInputMappingContext> InventoryMappingContext;

	/** Priority of InventoryMappingContext */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	int32 InventoryMappingPriority = 1;

	/** One action per slot key (index 0 = key 1) */
	UPROPERTY(EditDefaultsOnly, Category="Input")
	TArray<TObjectPtr<UInputAction>> SlotActions;

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

	/** Server: damage that reaches the owner after the item in hand (a parry) */
	float ModifyIncomingDamage(float Damage, const AActor* DamageCauser) const;

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	/** Server side of SelectSlot */
	UFUNCTION(Server, Reliable)
	void ServerSelectSlot(int32 Slot);

private:

	/** Server: spawns the starting items */
	void SpawnStartingItems();

	/** Server: equips Slot and puts every other item away */
	void ApplyActiveSlot(int32 Slot);

	/** Local player: binds the slot keys once the pawn has its input component (retried until then) */
	void TryBindInput();

	/** Key of slot N pressed */
	void HandleSlotAction(int32 Slot);

	/** Items by slot (null = empty), spawned by the server */
	UPROPERTY(Replicated)
	TArray<TObjectPtr<AInventoryItemActor>> Items;

	/** Active slot (-1 = bare hands) */
	UPROPERTY(Replicated)
	int32 ActiveSlot = -1;

	/** Slot keys are bound */
	bool bInputBound = false;

	/** Binding attempts so far */
	int32 BindAttempts = 0;

	FTimerHandle BindTimer;
};
